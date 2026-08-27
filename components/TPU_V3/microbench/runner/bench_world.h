// SPDX-License-Identifier: Apache-2.0
//
// The world outside the standalone NEO-CORE: boot ROM, global RAM and the
// simulator-only host-I/O block the benchmark firmware reports through.
//
// In a header rather than inside the runner so it can be driven directly by a
// test. `INTERFACE_CONTRACT.md` §1 binds every TPU_V3 target, and a testbench
// target that is more permissive than the model it stands in for is a
// testbench that hides the defect it exists to catch — but a contract nothing
// exercises is a comment. `tests/test_bench_world.cpp` is what makes it a rule.

#ifndef CDC_COMPONENTS_TPU_V3_BENCH_WORLD_H
#define CDC_COMPONENTS_TPU_V3_BENCH_WORLD_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"

extern "C" {
#include "bench_map.h"
}

namespace cdc::components::tpu_v3::bench {

namespace am = cdc::components::tpu_v3::address_map;

/// Global RAM behind the standalone external socket. Large enough for the
/// firmware's own 32 KiB plus the staged parameter block and the two 16 KiB
/// end-to-end buffers, with room left over.
inline constexpr std::uint64_t default_global_ram_bytes = 256 * 1024;

/// Boot ROM, global RAM and the simulator-only host-I/O block.
///
/// The host-I/O window sits in the unmapped low-address hole, the same place
/// Phase 2, Phase 4.5 and the Phase 7 pipeline put theirs. It is not a TPU_V3
/// architectural peripheral and must not leak into the SoC firmware ABI.
///
/// It is also where the measured interval is opened and closed. A store to
/// `SIM_MEASURE_BEGIN` arrives here inside the hart's own `b_transport`, so the
/// sample is taken at the logical time of that store — `sc_time_stamp()` plus
/// the delay accumulated so far, which is the same quantity D24 uses at the
/// chip endpoint.
class bench_world : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<bench_world> socket;

    /// Called with the logical time of the marker store.
    std::function<void(double)> on_measure_begin;
    std::function<void(double)> on_measure_end;
    /// Called with the stage number and the logical time it was entered.
    ///
    /// The guest already marks every stage transition for diagnostics; timing
    /// those marks is what turns "the interval took N ns" into an accounting
    /// that can be reconciled against its parts (G3, stage times versus the
    /// total interval).
    std::function<void(std::uint32_t, double)> on_stage;

    bool exited = false;
    std::uint32_t sim[SIM_WORDS] = {};

    /// Bytes this target actually served over `b_transport`, split by
    /// direction and by what was addressed.
    ///
    /// These exist for G3's second conservation clause: DMA bytes must
    /// reconcile with external-memory bytes, and only the memory can say how
    /// many it served. Counting inside the core alone would compare the model
    /// against itself.
    ///
    /// `stage()` and `fetch()` deliberately do not move these. They are the
    /// host loader, not workload traffic, and `INTERFACE_CONTRACT.md` §8 keeps
    /// a loader out of every performance counter.
    struct traffic {
        std::uint64_t read_bytes = 0;
        std::uint64_t write_bytes = 0;
        std::uint64_t read_requests = 0;
        std::uint64_t write_requests = 0;

        std::uint64_t bytes() const noexcept
        {
            return read_bytes + write_bytes;
        }
        std::uint64_t requests() const noexcept
        {
            return read_requests + write_requests;
        }
    };

    /// Boot ROM and global RAM: the memory the core's external port reaches.
    traffic memory;
    /// The simulator-only host-I/O window. Separate because it is not memory
    /// and must never be added to a memory-bandwidth figure — it is the
    /// mechanism the benchmark reports through.
    traffic host_io;

    explicit bench_world(sc_core::sc_module_name name,
                         std::uint64_t ram_bytes = default_global_ram_bytes)
        : sc_core::sc_module(name)
        , socket("socket")
        , rom_(am::boot_rom_size, 0)
        , ram_(ram_bytes, 0)
    {
        socket.register_b_transport(this, &bench_world::b_transport);
        socket.register_transport_dbg(this, &bench_world::transport_dbg);
    }

    void stage(std::uint64_t address, const void* data, std::size_t length)
    {
        std::memcpy(&ram_[address - am::global_ram_base], data, length);
    }

    void fetch(std::uint64_t address, void* into, std::size_t length) const
    {
        std::memcpy(into, &ram_[address - am::global_ram_base], length);
    }

private:
    /// `INTERFACE_CONTRACT.md` §1: what a payload must look like on entry.
    ///
    /// Checked rather than assumed. The traffic this runner actually generates
    /// is well formed, so none of this fires today — but the host-I/O window
    /// used to `memcpy` four bytes unconditionally, which turns a malformed
    /// payload into a buffer overrun instead of an error response. A testbench
    /// target that is more permissive than the model it stands in for is a
    /// testbench that hides the defect it exists to catch.
    tlm::tlm_response_status payload_shape(
        const tlm::tlm_generic_payload& trans) const
    {
        const tlm::tlm_command command = trans.get_command();
        if (command != tlm::TLM_READ_COMMAND
            && command != tlm::TLM_WRITE_COMMAND) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }
        const unsigned int length = trans.get_data_length();
        if (length == 0 || trans.get_data_ptr() == nullptr) {
            return tlm::TLM_BURST_ERROR_RESPONSE;
        }
        const unsigned int streaming = trans.get_streaming_width();
        if (streaming != 0 && streaming < length) {
            return tlm::TLM_BURST_ERROR_RESPONSE;
        }
        // §2: a non-null pointer with a zero-length pattern describes no bytes
        // and cannot be repeated into anything.
        if (trans.get_byte_enable_ptr() != nullptr
            && trans.get_byte_enable_length() == 0) {
            return tlm::TLM_BURST_ERROR_RESPONSE;
        }
        return tlm::TLM_OK_RESPONSE;
    }

    bool access(tlm::tlm_generic_payload& trans, bool debug, double now_ns,
                tlm::tlm_response_status& status)
    {
        status = payload_shape(trans);
        if (status != tlm::TLM_OK_RESPONSE) {
            return false;
        }

        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        const bool write = trans.get_command() == tlm::TLM_WRITE_COMMAND;

        if (address >= SIM_BASE && address < SIM_BASE + sizeof(sim)) {
            // A 32-bit register file, with the §6 MMIO rules: four bytes,
            // naturally aligned, all byte enables on. Never widened, narrowed
            // or split.
            if (length != 4 || (address % 4) != 0
                || address + 4 > SIM_BASE + sizeof(sim)) {
                status = tlm::TLM_BURST_ERROR_RESPONSE;
                return false;
            }
            // §2 asks for all-ones over the four bytes, not for a null
            // pointer. An initiator that spells "every byte" as an explicit
            // all-enabled array is making a legal 32-bit register access, and
            // refusing it would refuse a correct initiator; what must be
            // refused is a strobe that leaves any addressed byte out.
            if (!all_bytes_enabled(trans, 4)) {
                status = tlm::TLM_BURST_ERROR_RESPONSE;
                return false;
            }
            if (!debug) {
                if (write) {
                    ++host_io.write_requests;
                    host_io.write_bytes += 4;
                } else {
                    ++host_io.read_requests;
                    host_io.read_bytes += 4;
                }
            }
            const std::size_t index = (address - SIM_BASE) / 4;
            if (write) {
                std::uint32_t value = 0;
                std::memcpy(&value, trans.get_data_ptr(), 4);
                sim[index] = value;
                if (address == SIM_STAGE_MARK && on_stage) {
                    on_stage(value, now_ns);
                } else if (address == SIM_MEASURE_BEGIN && on_measure_begin) {
                    on_measure_begin(now_ns);
                } else if (address == SIM_MEASURE_END && on_measure_end) {
                    on_measure_end(now_ns);
                } else if (address == SIM_EXIT_KIND) {
                    // Written last by `sim_exit`, so status, cause and mepc
                    // are already in place by the time the run stops.
                    exited = true;
                    sc_core::sc_stop();
                }
            } else {
                std::memcpy(trans.get_data_ptr(), &sim[index], 4);
            }
            return true;
        }

        if (am::contains(am::boot_rom_base, am::boot_rom_size, address,
                         length)) {
            if (!debug) {
                count_memory(write, length);
            }
            const std::uint64_t offset = address - am::boot_rom_base;
            if (write) {
                // The boot ROM refuses ordinary writes; only the host loader
                // reaches it, through debug transport (`ADDRESS_MAP.md` §6).
                if (!debug) {
                    status = tlm::TLM_GENERIC_ERROR_RESPONSE;
                    return false;
                }
                copy_masked(&rom_[offset], trans.get_data_ptr(), trans, length);
            } else {
                copy_masked(trans.get_data_ptr(), &rom_[offset], trans, length);
            }
            return true;
        }

        if (am::contains(am::global_ram_base, ram_.size(), address, length)) {
            if (!debug) {
                count_memory(write, length);
            }
            const std::uint64_t offset = address - am::global_ram_base;
            if (write) {
                copy_masked(&ram_[offset], trans.get_data_ptr(), trans, length);
            } else {
                copy_masked(trans.get_data_ptr(), &ram_[offset], trans, length);
            }
            return true;
        }
        status = tlm::TLM_ADDRESS_ERROR_RESPONSE;
        return false;
    }

    void count_memory(bool write, unsigned int length) noexcept
    {
        if (write) {
            ++memory.write_requests;
            memory.write_bytes += length;
        } else {
            ++memory.read_requests;
            memory.read_bytes += length;
        }
    }

    /// True when every byte of a `length`-byte access is enabled.
    ///
    /// A null pointer means all bytes are enabled — the ordinary case. A
    /// non-null one is read through TLM's repeating-pattern rule, which is why
    /// this cannot be a pointer comparison.
    static bool all_bytes_enabled(const tlm::tlm_generic_payload& trans,
                                  unsigned int length)
    {
        const unsigned char* enables = trans.get_byte_enable_ptr();
        if (enables == nullptr) {
            return true;
        }
        const unsigned int pattern = trans.get_byte_enable_length();
        if (pattern == 0) {
            return false;
        }
        for (unsigned int i = 0; i < length; ++i) {
            if (enables[i % pattern] == TLM_BYTE_DISABLED) {
                return false;
            }
        }
        return true;
    }

    /// Copy honouring TLM's repeating byte-enable pattern (§2). A memory-like
    /// target honours arbitrary patterns, including non-contiguous ones.
    static void copy_masked(unsigned char* destination,
                            const unsigned char* source,
                            const tlm::tlm_generic_payload& trans,
                            unsigned int length)
    {
        const unsigned char* enables = trans.get_byte_enable_ptr();
        if (enables == nullptr) {
            std::memcpy(destination, source, length);
            return;
        }
        const unsigned int pattern = trans.get_byte_enable_length();
        for (unsigned int i = 0; i < length; ++i) {
            if (enables[i % pattern] != TLM_BYTE_DISABLED) {
                destination[i] = source[i];
            }
        }
    }

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const double now_ns
            = (sc_core::sc_time_stamp() + delay).to_seconds() * 1e9;
        tlm::tlm_response_status status = tlm::TLM_OK_RESPONSE;
        const bool served = access(trans, /*debug=*/false, now_ns, status);
        trans.set_response_status(served ? tlm::TLM_OK_RESPONSE : status);
        // §9: DMI is disabled everywhere in TPU_V3, and a testbench target
        // that granted it would let traffic bypass the counters this runner
        // exists to read.
        trans.set_dmi_allowed(false);
        // A fixed annotated cost. It is not a calibrated external-memory model
        // and §7.2 says so; every row repeats it rather than leaving a reader
        // to assume otherwise.
        delay += sc_core::sc_time(1, sc_core::SC_NS);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        // §8: debug transport bypasses timing and counters, not payload
        // validity. A malformed debug request returns zero bytes.
        tlm::tlm_response_status status = tlm::TLM_OK_RESPONSE;
        const bool served = access(trans, /*debug=*/true, 0.0, status);
        // §1's return rules are written against `b_transport` *and*
        // `transport_dbg`: `response_status` is always set and `dmi_allowed`
        // is always cleared. A caller is expected to use the returned byte
        // count, but leaving a computed status on the floor means the payload
        // goes back carrying whatever it arrived with — usually
        // `TLM_INCOMPLETE_RESPONSE`, which §1 says a target must never leave.
        trans.set_response_status(served ? tlm::TLM_OK_RESPONSE : status);
        trans.set_dmi_allowed(false);
        return served ? trans.get_data_length() : 0;
    }

    std::vector<unsigned char> rom_;
    std::vector<unsigned char> ram_;
};


} // namespace cdc::components::tpu_v3::bench

#endif // CDC_COMPONENTS_TPU_V3_BENCH_WORLD_H
