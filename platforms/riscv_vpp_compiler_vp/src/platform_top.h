// SPDX-License-Identifier: Apache-2.0
//
// The `riscv_vpp_compiler_vp` SystemC top level.
//
//     ┌──────────────────────────┐
//     │ RISC-V VP++ RV32GCV hart │  one architectural hart: scalar and vector
//     └────────────┬─────────────┘  execution share PC, registers, CSRs and
//                  │ one TLM socket this bus
//     ┌────────────▼─────────────┐
//     │       address decoder    │  counts and classifies every transaction
//     └──────┬────────────┬──────┘
//            │            │
//     ┌──────▼─────┐ ┌────▼─────────────────┐
//     │ program/   │ │ simulator-only       │
//     │ data RAM   │ │ host I/O             │
//     └────────────┘ └──────────────────────┘
//
// Nothing else. Phase 4.5's non-goals are explicit that no NoC, no Sauria, no
// NEO DMA, no core SRAM, no NEO fabric, no second hart, no CLINT, no PLIC, no
// MMU and no cache is instantiated, and `riscv_vpp_compiler_vp_independence`
// re-checks that against the sources, the built binary and the package rather
// than trusting this comment.
//
// ## What the decoder counts, and how far the claim goes
//
// The gate requires instruction fetch, scalar load/store, vector load/store and
// host-I/O MMIO to be observable as TLM transactions. Every one of them arrives
// here, because the backend installs no DMI and no ISS-internal caches (see
// `riscv_vp_plusplus_wrapper.h`), and because this socket refuses
// `get_direct_mem_ptr` and counts the attempt.
//
// Splitting *fetch* from *data* is a different matter and worth being exact
// about. VP++'s combined memory interface carries fetch and data on one socket
// and marks neither, so the payload cannot say which it is; the access type it
// threads through internally exists only to choose a trap cause. What the
// decoder does instead is classify by address: a read inside a loadable
// executable segment of the image is counted as fetch, everything else in RAM
// as data. That is exact for any program that does not read its own text, and
// wrong by exactly the number of such reads for one that does. Both counters
// are labelled accordingly, and nothing downstream treats "fetch" as a fact
// about the transaction rather than about its address.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "elf_image.h"

namespace cdc::cpu {
class riscv_vp_plusplus_cpu;
}

namespace cdc::platforms::riscv_vpp_compiler_vp {

/// The guest has stopped making progress *and* has stopped letting the SystemC
/// kernel run, so neither simulated watchdog can fire.
///
/// This is not hypothetical and it is not exotic. An image whose entry point
/// lands on memory that was never written executes a zero word, takes an
/// illegal-instruction trap, and vectors to `mtvec` — which is still 0, because
/// the startup code that would have set it never ran. Fetching from 0 faults
/// too, and the ISS loops: it retires nothing, so the instruction watchdog
/// never advances, and it never reaches its quantum-sync point, so simulated
/// time never advances either. `sc_start()` cannot return, because a SystemC
/// process that does not yield cannot be preempted.
///
/// The run loop polls between slices and therefore sees none of this. What does
/// see it is the decoder, which is called on every one of those faulting
/// accesses: an unbroken run of refused accesses with no successful one between
/// them is a fault loop, and no working program produces one. Throwing from
/// inside `b_transport` unwinds the ISS thread, which is the only way out.
struct guest_fault_loop : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct platform_config {
    memory_region ram;
    memory_region host_io;
    std::uint32_t hart_id = 0;

    /// Transactions printed by `--trace` before it gives up. A vector example
    /// makes tens of thousands; an unbounded trace turns a diagnostic aid into
    /// a way to fill a disk.
    std::uint64_t trace_limit = 0;
};

struct access_counters {
    std::uint64_t transactions = 0;
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;
    std::uint64_t bytes = 0;

    void record(bool write, std::uint64_t length)
    {
        ++transactions;
        if (write) {
            ++writes;
        } else {
            ++reads;
        }
        bytes += length;
    }
};

/// Program and data RAM.
///
/// A flat `std::vector`, not the TPU_V3 sparse page store: this platform maps
/// one region of a size the user chose, and the sparse model exists to make a
/// 16 MiB-per-core architectural window affordable across eight chips. Here it
/// would only add a dependency on `tpu_v3_common`, which the independence guard
/// then has to reason about.
class ram_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<ram_target> tsock;

    ram_target(sc_core::sc_module_name name, std::uint64_t base,
               std::uint64_t size);

    std::uint64_t base() const noexcept { return base_; }
    std::uint64_t size() const noexcept { return storage_.size(); }

    /// Host-side read that does not go through the bus. Used by
    /// `--dump-signature`, which is a host facility: routing it through the
    /// decoder would add transactions to the counters the run is judged on.
    bool backdoor_read(std::uint64_t address, unsigned char* buffer,
                       std::uint64_t length) const;

private:
    void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& payload);
    bool access(tlm::tlm_generic_payload& payload);

    std::uint64_t base_;
    std::vector<unsigned char> storage_;
};

/// The simulator-only host-I/O target.
///
/// See `compiler_vp/host_io_map.h` for the register map and for why this window
/// is not a TPU_V3 architectural peripheral.
class host_io_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<host_io_target> tsock;

    struct identity {
        std::uint32_t xlen = 32;
        std::uint32_t hart_count = 1;
        std::uint32_t hart_id = 0;
        std::uint32_t vlen_bits = 0;
        std::uint32_t elen_bits = 0;
        std::uint32_t vlenb = 0;
        std::uint32_t ram_base = 0;
        std::uint32_t ram_size = 0;
        std::uint32_t hostio_base = 0;
        std::uint32_t hostio_size = 0;
    };

    host_io_target(sc_core::sc_module_name name, std::uint64_t base,
                   std::uint64_t size, const identity& id);

    /// Wired by the top to the decoder, which owns the access counters the
    /// measurement window is expressed in.
    std::function<void(std::uint32_t)> on_mark;
    std::function<void(std::uint32_t)> on_expect;

    bool exited = false;
    std::uint32_t exit_kind = 0;
    std::uint32_t exit_status = 0xffff'ffff;
    std::uint32_t exit_mcause = 0;
    std::uint32_t exit_mepc = 0;

    /// Bytes the guest wrote to the console, whether or not they ended in a
    /// newline. Reported so an image that produced nothing is distinguishable
    /// from one whose output was lost.
    std::uint64_t console_bytes = 0;

    /// Word accesses inside the window that hit no defined register. They are
    /// answered rather than faulted — the window is 4 KiB and only a few words
    /// of it are registers — but they are counted and reported, because
    /// silently absorbing them would hide a firmware defect that looks like a
    /// working run.
    std::uint64_t undefined_register_accesses = 0;

    /// Filled in by the top from the constructed hart's own `vlenb` CSR. Not
    /// computed from VLEN/8 here: firmware compares this register against the
    /// CSR, and deriving both from the same constant would compare a value
    /// with itself.
    void set_vlenb(std::uint32_t value) { id_.vlenb = value; }

    /// Flush whatever is left in the line buffer. Called at the end of a run:
    /// an image that exits mid-line would otherwise lose the partial line, and
    /// a partial line is often the one naming the failure.
    void flush();

private:
    void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& payload);

    void write_register(std::uint64_t offset, std::uint32_t value);
    std::uint32_t read_register(std::uint64_t offset);

    std::uint64_t base_;
    std::uint64_t size_;
    identity id_;
    std::string line_;
};

/// The address decoder: one target socket in, two initiator sockets out.
class bus_decoder : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<bus_decoder> tsock;
    tlm_utils::simple_initiator_socket<bus_decoder> ram_socket;
    tlm_utils::simple_initiator_socket<bus_decoder> host_io_socket;

    bus_decoder(sc_core::sc_module_name name, const platform_config& config,
                const elf_image& image);

    // Counters. `fetch` and `data` split RAM traffic by whether the address
    // lies in a loadable executable segment; see the header comment for how
    // exact that is.
    access_counters fetch;
    access_counters data;
    access_counters host_io;

    /// Transactions the decoder refused because no region claimed them. VP++
    /// turns the error response into an access fault chosen by access origin
    /// (decision record D13), so these are guest faults, not model defects.
    std::uint64_t unmapped = 0;

    /// Refused accesses since the last successful one. A working program breaks
    /// the run with its very next instruction fetch, so this only grows without
    /// bound in a fault loop; see `guest_fault_loop`.
    std::uint64_t consecutive_unmapped = 0;

    /// How long a run of refusals is allowed to get before the decoder decides
    /// the guest is not coming back. Generous by three orders of magnitude
    /// against anything deliberate — a program probing its address map
    /// interleaves successful fetches of its own probing loop.
    static constexpr std::uint64_t fault_loop_limit = 1024;

    /// Set just before `guest_fault_loop` is thrown.
    ///
    /// The flag exists because the exception type does not survive the trip.
    /// SystemC catches anything escaping a process and rethrows it from
    /// `sc_start()` as an `sc_report` — which is also a `std::exception`, so a
    /// `catch (const guest_fault_loop&)` never matches and a broken guest would
    /// be reported as a model defect. The message is kept here too, so the
    /// diagnostic printed is the one written above rather than SystemC's
    /// "(E549) uncaught exception" wrapper around it.
    bool fault_loop_detected = false;
    std::string fault_loop_message;

    /// DMI requests, all refused. Zero is the expected value: the backend never
    /// calls `dmi_add()`. The counter exists so that if a future change starts
    /// asking, the run says so instead of quietly bypassing every counter here.
    std::uint64_t dmi_requests = 0;

    // ── the measurement window (host-I/O block D) ────────────────────────────

    void mark(std::uint32_t id);
    void expect(std::uint32_t accesses);

    bool window_violated = false;
    std::string window_message;

private:
    void b_transport(tlm::tlm_generic_payload& payload, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& payload);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& payload,
                            tlm::tlm_dmi& dmi);

    const platform_config& config_;
    const elf_image& image_;

    std::uint64_t traced_ = 0;

    std::uint32_t mark_id_ = 0;
    std::uint64_t mark_data_at_open_ = 0;
    std::uint64_t mark_expect_ = 0;
};

/// Everything above, wired together.
class compiler_vp_top : public sc_core::sc_module {
public:
    compiler_vp_top(sc_core::sc_module_name name, const platform_config& config,
                    const elf_image& image);
    ~compiler_vp_top() override;

    cdc::cpu::riscv_vp_plusplus_cpu& cpu() { return *cpu_; }
    const bus_decoder& bus() const { return bus_; }
    const host_io_target& host_io() const { return host_io_; }

    void flush_console() { host_io_.flush(); }

    /// True once the guest wrote the exit trigger.
    bool guest_exited() const noexcept { return host_io_.exited; }

    /// Backdoor read, for `--dump-signature`. Returns false when the range is
    /// not entirely inside RAM.
    bool debug_read(std::uint64_t address, unsigned char* buffer,
                    std::uint64_t length);

    /// The per-region transaction report printed at the end of every run.
    std::string traffic_report() const;

private:
    platform_config config_;
    const elf_image& image_;

    ram_target ram_;
    host_io_target host_io_;
    bus_decoder bus_;
    std::unique_ptr<cdc::cpu::riscv_vp_plusplus_cpu> cpu_;
};

} // namespace cdc::platforms::riscv_vpp_compiler_vp
