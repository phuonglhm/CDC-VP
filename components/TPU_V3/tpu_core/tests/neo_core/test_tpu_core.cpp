// SPDX-License-Identifier: Apache-2.0
//
// The NEO-CORE composition gate (Phase 7).
//
// Every part of this core was gated on its own in Phases 2 through 6. What is
// new here is the wiring, so this file tests wiring rather than behaviour: it
// asks where a transaction went, who owned it, and what happens to the parts
// when the core is reset — not whether the DMA copies correctly or whether the
// MXU multiplies correctly, both of which already have their own gates.
//
// The hart is real and it runs. Its reset PC is `GLOBAL_BOOT_ROM`, which is
// outside the core, so the very first instruction fetch has to cross the
// external bridge before anything else can happen. That path is
// architecturally required (D15), and a composition that got it wrong would
// look like a hart that never starts — so the boot ROM here holds two real
// RV32 instructions and the test checks that they were fetched.

#include "tpu_v3/core/tpu_core.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/dma/dma_registers.h"
#include "tpu_v3/sauria/sa_registers.h"
#include "tpu_v3/transform/image_transform_registers.h"

namespace tpu = cdc::components::tpu_v3;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

using core::hart_destination;
using core::local_fabric_timing;
using core::tpu_core;
using core::tpu_core_config;
using sram::neo_requester;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr tpu::chip_id_t kChip = 1;
constexpr tpu::core_id_t kCore = 1;
constexpr std::uint64_t kCapacity = 64 * 1024;
constexpr double kWatchdogMicroseconds = 200.0;

tpu_core_config core_config()
{
    tpu_core_config config;
    config.chip = kChip;
    config.core = kCore;
    config.sram_capacity_bytes = kCapacity;
    // Provisional physical values; the schema has no defaults for them and D15
    // leaves them open until SRAM-macro, frequency and PD inputs exist.
    config.fabric.data_width_bits = 128;
    config.fabric.bank_count = 4;
    config.fabric.mapping = tpu::bank_mapping::low_order_interleaved;
    config.fabric.pipeline_stages = 1;
    config.fabric.max_outstanding_per_requester = 1;
    config.fabric.arbitration = tpu::arbitration_policy::round_robin;
    config.timing = local_fabric_timing::annotated;
    config.cycle = sc_core::sc_time(10, sc_core::SC_NS);
    return config;
}

/// Everything outside the core: boot ROM and global RAM, behind the chip-facing
/// socket. Simple backing storage — this bench is about the core's wiring, and
/// the memory only has to answer.
class outside_world : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<outside_world> socket;

    std::uint64_t fetches = 0;
    std::uint64_t data_accesses = 0;

    explicit outside_world(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , rom_(am::boot_rom_size, 0)
        , ram_(64 * 1024, 0)
    {
        socket.register_b_transport(this, &outside_world::b_transport);
        socket.register_transport_dbg(this, &outside_world::transport_dbg);

        // Five real RV32 instructions at the reset vector. Assembled with the
        // pinned cross toolchain and pasted here, rather than hand-encoded:
        //
        //   lui   t0, 0x1
        //   addi  t0, t0, -2048      # t0 = 0x800, mie.MEIE
        //   csrw  mie, t0
        //   wfi
        //   loop: j loop
        //
        // `mie.MEIE` has to be set for `wfi` to be woken by a machine external
        // interrupt: VP++ implements the instruction as
        // `while (!has_local_pending_enabled_interrupts()) wait(wfi_event);`,
        // which is what the specification asks for. An earlier version of this
        // boot code omitted it and the hart correctly slept through its own
        // interrupt — the observable was wrong, not the core.
        //
        // `mstatus.MIE` is deliberately left clear. The hart wakes but does
        // not take the trap, which is enough to prove the interrupt reached
        // the ISS and avoids needing a trap handler in a wiring test. Firmware
        // with a real handler is Phase 7's later, separate task.
        store32(0x0000'0000, 0x0000'12B7u);
        store32(0x0000'0004, 0x8002'8293u);
        store32(0x0000'0008, 0x3042'9073u);
        store32(0x0000'000C, 0x1050'0073u);
        store32(0x0000'0010, 0x0000'006Fu);
    }

    void store32(std::uint64_t offset, std::uint32_t value)
    {
        std::memcpy(&rom_[offset], &value, 4);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();

        if (am::contains(am::boot_rom_base, am::boot_rom_size, address,
                         length)) {
            ++fetches;
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                std::memcpy(trans.get_data_ptr(),
                            &rom_[address - am::boot_rom_base], length);
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
            } else {
                // The boot ROM refuses ordinary writes; the host loader uses
                // debug transport (`ADDRESS_MAP.md` §6).
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            }
            delay += sc_core::sc_time(1, sc_core::SC_NS);
            return;
        }
        if (am::contains(am::global_ram_base, ram_.size(), address, length)) {
            ++data_accesses;
            const std::uint64_t offset = address - am::global_ram_base;
            if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
                std::memcpy(&ram_[offset], trans.get_data_ptr(), length);
            } else {
                std::memcpy(trans.get_data_ptr(), &ram_[offset], length);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            delay += sc_core::sc_time(1, sc_core::SC_NS);
            return;
        }
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        if (am::contains(am::boot_rom_base, am::boot_rom_size, address,
                         length)) {
            const std::uint64_t offset = address - am::boot_rom_base;
            if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
                std::memcpy(&rom_[offset], trans.get_data_ptr(), length);
            } else {
                std::memcpy(trans.get_data_ptr(), &rom_[offset], length);
            }
            return length;
        }
        return 0;
    }

    std::vector<unsigned char> rom_;
    std::vector<unsigned char> ram_;
};

/// A remote master on the core's inbound side: the sibling core, another chip,
/// or the host loader. It exists to prove the inbound half of the boundary is
/// connected, which nothing else in this bench would exercise.
class remote_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<remote_master> socket;

    explicit remote_master(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status access(tlm::tlm_command command,
                                    std::uint64_t address, unsigned char* data,
                                    unsigned int length)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_streaming_width(length);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }
};

class checks : public sc_core::sc_module {
public:
    checks(sc_core::sc_module_name name, tpu_core& core_under_test,
           remote_master& remote, outside_world& outside)
        : sc_core::sc_module(name)
        , core_(core_under_test)
        , remote_(remote)
        , outside_(outside)
    {
        SC_HAS_PROCESS(checks);
        SC_THREAD(run);
    }

    bool ran() const noexcept { return ran_; }

private:
    void run()
    {
        // Let the hart reach its reset vector before anything is asserted
        // about it.
        wait(sc_core::sc_time(1, sc_core::SC_US));

        hart_fetches_from_boot_rom();
        every_engine_answers_its_window();
        inbound_reaches_local_sram();
        completion_without_irq_leaves_the_hart_parked();
        completion_with_irq_reaches_the_hart();
        unavailable_operation_is_refused_in_place();
        an_injected_error_is_reported_not_absorbed();
        reset_is_hierarchical();
        reset_mid_job_keeps_committed_data();
        reset_near_completion_keeps_its_accounting();
        mxu_start_during_the_reset_pulse_is_refused();

        ran_ = true;
    }

    void hart_fetches_from_boot_rom()
    {
        CHECK_MSG(outside_.fetches > 0,
                  "the hart never fetched from GLOBAL_BOOT_ROM. Its reset PC "
                  "is outside the core, so the first fetch must cross the "
                  "external bridge; a core wired without that path looks "
                  "exactly like a hart that never starts (D15)");
        CHECK_MSG(core_.hart_port().requests(hart_destination::external) > 0,
                  "the fetches did not go through the hart port's external "
                  "leg, so something else is answering them");
        CHECK_MSG(core_.hart_port().requests(hart_destination::local_sram) == 0,
                  "an instruction fetch reached core SRAM; the reset vector is "
                  "in global boot ROM, not in the core");
    }

    /// Each engine owns its own 64 KiB control window, and reaching it proves
    /// three things at once: the control fabric decodes to it, the engine's
    /// target is bound there rather than shadowed by a register file of the
    /// core's, and the identity register really comes from the engine.
    void every_engine_answers_its_window()
    {
        const std::uint64_t dma_base = am::dma_control(kChip, kCore);
        const std::uint64_t sa_base = am::sa_control(kChip, kCore);
        const std::uint64_t transform_base
            = am::transform_control(kChip, kCore);

        CHECK_MSG(read32(dma_base + tpu::dma::reg::id) != 0,
                  "the DMA's ID register read back zero: nothing is bound at "
                  "DMA_CONTROL, or a placeholder register file is answering "
                  "instead of the engine");
        CHECK(read32(sa_base) != 0);
        CHECK(read32(transform_base) != 0);

        // A write to one engine's window must not appear in another's. The
        // windows are 64 KiB apart in one 32 MiB aperture, so a decoder that
        // masked too few bits would alias them.
        const std::uint32_t pattern = 0x1234'5678u;
        write32(dma_base + tpu::dma::reg::src_addr_lo, pattern);
        CHECK_MSG(read32(dma_base + tpu::dma::reg::src_addr_lo) == pattern,
                  "a DMA descriptor register did not hold what was written to "
                  "it through the control plane");
    }

    void inbound_reaches_local_sram()
    {
        const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);
        const auto before
            = core_.fabric().counters(neo_requester::external_inbound)
                  .request_count;

        std::array<unsigned char, 4> out{0xAA, 0xBB, 0xCC, 0xDD};
        CHECK(remote_.access(tlm::TLM_WRITE_COMMAND, sram_base + 0x80,
                             out.data(), 4)
              == tlm::TLM_OK_RESPONSE);

        std::array<unsigned char, 4> in{};
        CHECK(remote_.access(tlm::TLM_READ_COMMAND, sram_base + 0x80,
                             in.data(), 4)
              == tlm::TLM_OK_RESPONSE);
        CHECK(in == out);

        CHECK_MSG(core_.fabric()
                          .counters(neo_requester::external_inbound)
                          .request_count
                      == before + 2,
                  "inbound remote traffic did not arrive as the "
                  "'external_inbound' requester; remote accesses must be "
                  "arbitrated like any other, not bypass the fabric (D15)");
    }

    // ── interrupts ───────────────────────────────────────────────────────────
    //
    // The interesting question is not whether a signal toggles; it is whether
    // the *hart* learns about it. So the observable here is the hart itself:
    // it is parked in `wfi` at its reset vector, and an external interrupt is
    // what wakes it. A woken hart falls through to `j .` and spins, and with
    // the ISS decode cache disabled (P2-5) every one of those iterations is a
    // real fetch across the external bridge. Fetches resuming therefore means
    // `mip.meip` was actually set inside the ISS, which no counter on this
    // side of `set_irq` could have told us.
    //
    // The two scenarios run in this order on purpose. The negative control has
    // to go first, while the hart is still parked: once it is awake, "the hart
    // did not wake" stops being observable and the control would pass for the
    // wrong reason.

    /// Negative control: a completed job with its interrupt disabled must not
    /// reach the hart. `DONE` still sets — the job did finish — and that is
    /// the distinction the enable bit exists to make.
    void completion_without_irq_leaves_the_hart_parked()
    {
        const std::uint64_t dma_base = am::dma_control(kChip, kCore);
        const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

        const auto fetches_before
            = core_.hart_port().requests(hart_destination::external);

        run_dma_transfer(dma_base, am::global_ram_base, sram_base + 0x400, 64,
                         /*enable_irq=*/false);

        CHECK_MSG((read32(dma_base + tpu::dma::reg::status)
                   & tpu::dma::status_bit::done)
                      != 0,
                  "the transfer did not complete; the rest of this scenario "
                  "would be measuring nothing");
        CHECK_MSG(!core_.irq_pending(),
                  "an interrupt was raised for a job whose IRQ_ENABLE is "
                  "clear");

        // Give the hart a generous window to wake in, so "it stayed parked" is
        // a statement about the design rather than about timing luck.
        wait(sc_core::sc_time(2, sc_core::SC_US));
        CHECK_MSG(core_.hart_port().requests(hart_destination::external)
                      == fetches_before,
                  "the hart resumed fetching without an enabled interrupt: it "
                  "either never parked in `wfi`, or something is waking it "
                  "that this test does not know about — either way the "
                  "positive case below would prove nothing");

        write32(dma_base + tpu::dma::reg::status, tpu::dma::status_bit::done);
    }

    /// The positive case: engine → core aggregation → `set_irq` → the hart.
    void completion_with_irq_reaches_the_hart()
    {
        const std::uint64_t dma_base = am::dma_control(kChip, kCore);
        const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

        const auto fetches_before
            = core_.hart_port().requests(hart_destination::external);

        run_dma_transfer(dma_base, am::global_ram_base, sram_base + 0x500, 64,
                         /*enable_irq=*/true);

        CHECK_MSG((read32(dma_base + tpu::dma::reg::status)
                   & tpu::dma::status_bit::done)
                      != 0,
                  "the transfer did not complete");
        CHECK_MSG(core_.irq_pending(),
                  "the DMA completed with its interrupt enabled and the core "
                  "aggregated nothing; the engine's line is not reaching "
                  "`aggregate_irq` (ARCHITECTURE.md §5)");

        wait(sc_core::sc_time(2, sc_core::SC_US));
        CHECK_MSG(core_.hart_port().requests(hart_destination::external)
                      > fetches_before,
                  "the hart never woke, so the aggregated level did not reach "
                  "the ISS. `irq_pending()` alone would have been satisfied by "
                  "a signal that goes nowhere");

        // Level, not edge: it stays asserted until firmware acknowledges.
        CHECK_MSG(core_.irq_pending(),
                  "the interrupt deasserted on its own. A level line that "
                  "clears without an acknowledgement is an edge line with "
                  "extra steps, and a completion lost in a reset window would "
                  "be unrecoverable (ARCHITECTURE.md §5)");

        write32(dma_base + tpu::dma::reg::status, tpu::dma::status_bit::done);
        wait(core_.config().cycle * 4);
        CHECK_MSG(!core_.irq_pending(),
                  "the interrupt survived its W1C acknowledgement");
    }

    /// Programs and runs one DMA job, and waits for it to leave `BUSY`.
    void run_dma_transfer(std::uint64_t dma_base, std::uint64_t source,
                          std::uint64_t destination, std::uint32_t length,
                          bool enable_irq)
    {
        write32(dma_base + tpu::dma::reg::src_addr_lo,
                static_cast<std::uint32_t>(source));
        write32(dma_base + tpu::dma::reg::src_addr_hi,
                static_cast<std::uint32_t>(source >> 32));
        write32(dma_base + tpu::dma::reg::dst_addr_lo,
                static_cast<std::uint32_t>(destination));
        write32(dma_base + tpu::dma::reg::dst_addr_hi,
                static_cast<std::uint32_t>(destination >> 32));
        write32(dma_base + tpu::dma::reg::length, length);
        write32(dma_base + tpu::dma::reg::irq_enable,
                enable_irq ? tpu::dma::irq_enable_bit::completion : 0u);
        write32(dma_base + tpu::dma::reg::control,
                tpu::dma::control_bit::start);

        // Bounded, because a poll loop with no bound turns a stuck engine into
        // a hung suite instead of a failed test.
        for (unsigned i = 0; i < 2000; ++i) {
            if ((read32(dma_base + tpu::dma::reg::status)
                 & tpu::dma::status_bit::busy)
                == 0) {
                return;
            }
            wait(core_.config().cycle);
        }
        CHECK_MSG(false, "the DMA never left BUSY");
    }

    /// The Phase 7 gate line "unavailable ... cases fail predictably".
    ///
    /// The Transform block's own gate already proves Col2Im is refused. What
    /// this adds is that the refusal survives composition: it reaches the
    /// engine through the real control plane, is answered without a single
    /// byte of local-plane traffic, and leaves the core able to run the next
    /// job. An engine that refused by hanging, or by quietly transforming
    /// something, would pass its own gate and fail here.
    void unavailable_operation_is_refused_in_place()
    {
        const std::uint64_t base = am::transform_control(kChip, kCore);

        const std::uint32_t capability
            = read32(base + tpu::transform::reg::capability);
        CHECK_MSG((capability & tpu::transform::capability_bit::im2col) != 0,
                  "the Transform block does not advertise Im2Col");
        CHECK_MSG((capability & tpu::transform::capability_bit::col2im) == 0,
                  "the Transform block advertises Col2Im. D18 keeps that "
                  "capability bit zero until the NPU team supplies a source "
                  "and a golden; advertising it is how a guessed inverse gets "
                  "used");

        const auto traffic_before
            = core_.fabric().counters(neo_requester::transform).request_count;

        write32(base + tpu::transform::reg::operation,
                tpu::transform::operation_value::col2im);
        write32(base + tpu::transform::reg::control,
                tpu::transform::control_bit::start);

        // It completes, and it completes as an error rather than hanging.
        for (unsigned i = 0; i < 200; ++i) {
            if ((read32(base + tpu::transform::reg::status)
                 & tpu::transform::status_bit::busy)
                == 0) {
                break;
            }
            wait(core_.config().cycle);
        }
        const std::uint32_t status = read32(base + tpu::transform::reg::status);
        CHECK_MSG((status & tpu::transform::status_bit::busy) == 0,
                  "an unavailable operation left the engine BUSY");
        CHECK_MSG((status & tpu::transform::status_bit::error) != 0,
                  "an unavailable operation did not report ERROR");
        CHECK_MSG((status & tpu::transform::status_bit::done) == 0,
                  "an unavailable operation reported DONE. A refusal is not a "
                  "completion, and a driver must be able to tell them apart");
        CHECK_MSG(read32(base + tpu::transform::reg::error_cause)
                      == static_cast<std::uint32_t>(
                          tpu::transform::error_cause::unavailable_operation),
                  "the latched cause is not `unavailable_operation`");
        CHECK_MSG(core_.fabric().counters(neo_requester::transform).request_count
                      == traffic_before,
                  "a refused Col2Im moved data on the local plane. D18 is "
                  "explicit that it must cause no SRAM traffic at all");

        // Acknowledge, and leave the engine selectable again.
        write32(base + tpu::transform::reg::status,
                tpu::transform::status_bit::error);
        write32(base + tpu::transform::reg::operation,
                tpu::transform::operation_value::im2col);
    }

    /// The Phase 7 gate line "injected-error cases fail predictably".
    ///
    /// A DMA descriptor aimed at an address nothing decodes. The interesting
    /// property is not that it fails — it is that the failure is *reported*
    /// rather than absorbed: a latched cause, no DONE, and a core that still
    /// works afterwards. An engine that returned zero data on a failed read
    /// would look like a successful transfer of zeros.
    void an_injected_error_is_reported_not_absorbed()
    {
        const std::uint64_t dma_base = am::dma_control(kChip, kCore);
        const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

        // Inside no region at all: above the boot ROM and below global RAM.
        constexpr std::uint64_t unmapped = 0x4000'0000ull;

        write32(dma_base + tpu::dma::reg::src_addr_lo,
                static_cast<std::uint32_t>(unmapped));
        write32(dma_base + tpu::dma::reg::src_addr_hi, 0);
        write32(dma_base + tpu::dma::reg::dst_addr_lo,
                static_cast<std::uint32_t>(sram_base + 0x600));
        write32(dma_base + tpu::dma::reg::dst_addr_hi, 0);
        write32(dma_base + tpu::dma::reg::length, 16);
        write32(dma_base + tpu::dma::reg::irq_enable, 0);
        write32(dma_base + tpu::dma::reg::control,
                tpu::dma::control_bit::start);

        for (unsigned i = 0; i < 500; ++i) {
            if ((read32(dma_base + tpu::dma::reg::status)
                 & tpu::dma::status_bit::busy)
                == 0) {
                break;
            }
            wait(core_.config().cycle);
        }

        const std::uint32_t status = read32(dma_base + tpu::dma::reg::status);
        CHECK_MSG((status & tpu::dma::status_bit::busy) == 0,
                  "the DMA never left BUSY after a failing descriptor");
        CHECK_MSG((status & tpu::dma::status_bit::error) != 0,
                  "a transfer from an unmapped address did not set ERROR");
        CHECK_MSG((status & tpu::dma::status_bit::done) == 0,
                  "a failed transfer also reported DONE");
        CHECK_MSG(read32(dma_base + tpu::dma::reg::error_cause) != 0,
                  "no cause was latched, so firmware cannot tell what went "
                  "wrong");
        CHECK_MSG(read32(dma_base + tpu::dma::reg::bytes_done_lo) == 0,
                  "the DMA reported committed bytes for a transfer whose "
                  "source never answered");

        write32(dma_base + tpu::dma::reg::status, tpu::dma::status_bit::error);

        // The core survives it: a successful transfer still works afterwards.
        run_dma_transfer(dma_base, am::global_ram_base, sram_base + 0x700, 16,
                         /*enable_irq=*/false);
        CHECK_MSG((read32(dma_base + tpu::dma::reg::status)
                   & tpu::dma::status_bit::done)
                      != 0,
                  "the core could not run a transfer after a failed one, so "
                  "the error was not recovered from");
        write32(dma_base + tpu::dma::reg::status, tpu::dma::status_bit::done);
    }

    void reset_is_hierarchical()
    {
        // Put something in every counter that reset is supposed to clear, then
        // check the core cleared all of them in one call. The point is not the
        // zeroes; it is that one `reset()` reached every part, because a core
        // that reset three of its four planes would look fine until the plane
        // it missed carried stale state into the next epoch.
        CHECK(core_.hart_port().requests(hart_destination::external) > 0);

        core_.reset();

        CHECK_MSG(core_.hart_port().requests(hart_destination::external) == 0,
                  "the hart port's counters survived a core reset");
        CHECK_MSG(core_.fabric().counters(neo_requester::cpu).request_count
                      == 0,
                  "the local fabric's counters survived a core reset");
        CHECK_MSG(core_.fabric()
                          .counters(neo_requester::external_inbound)
                          .request_count
                      == 0,
                  "the local fabric's inbound counters survived a core reset");
        CHECK_MSG(!core_.irq_pending(),
                  "an interrupt was still pending after reset; reset must "
                  "deassert the line rather than leave the hart looking at a "
                  "completion that no longer exists");
    }

    /// Reset while a job is actually in flight.
    ///
    /// The existing reset scenario resets an idle core, which cannot see the
    /// question this one asks: every engine promises that bytes already
    /// committed to the destination stay committed and stay reported after a
    /// reset (plan §11.5, D17). That promise is only meaningful if the memory
    /// they were committed to survives the reset too, and an idle-core test
    /// never puts the two claims in the same room.
    void reset_mid_job_keeps_committed_data()
    {
        const std::uint64_t dma_base = am::dma_control(kChip, kCore);
        const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);
        constexpr std::uint64_t destination_offset = 0x900;

        // A known pattern in SRAM first, written through the fabric so it is
        // real backing storage rather than a register somewhere.
        constexpr unsigned kPatternBytes = 32;
        std::array<unsigned char, kPatternBytes> pattern{};
        for (unsigned i = 0; i < kPatternBytes; ++i) {
            pattern[i] = static_cast<unsigned char>(0xA0 + i);
        }
        sram::neo_local_request write{};
        write.requester = neo_requester::cpu;
        write.command = sram::neo_command::write;
        write.address = sram_base + destination_offset;
        write.size = kPatternBytes;
        write.data = pattern.data();
        CHECK_MSG(core_.fabric().dbg_access(write) == kPatternBytes,
                  "could not stage the pattern this scenario checks");

        // Start a transfer long enough that the reset lands while it runs.
        write32(dma_base + tpu::dma::reg::src_addr_lo,
                static_cast<std::uint32_t>(am::global_ram_base));
        write32(dma_base + tpu::dma::reg::src_addr_hi, 0);
        write32(dma_base + tpu::dma::reg::dst_addr_lo,
                static_cast<std::uint32_t>(sram_base + 0xA00));
        write32(dma_base + tpu::dma::reg::dst_addr_hi, 0);
        write32(dma_base + tpu::dma::reg::length, 2048);
        write32(dma_base + tpu::dma::reg::irq_enable, 0);
        write32(dma_base + tpu::dma::reg::control, tpu::dma::control_bit::start);

        // Let it commit something, then reset it mid-flight.
        wait(core_.config().cycle * 3);
        CHECK_MSG((read32(dma_base + tpu::dma::reg::status)
                   & tpu::dma::status_bit::busy)
                      != 0,
                  "the transfer finished before the reset, so this scenario "
                  "degenerated into the idle-core case it exists to go beyond");

        core_.reset();

        // The SRAM keeps what was already in it. This is the claim the reset
        // ordering was getting wrong: `core_sram::reset()` wipes the backing
        // store, and a core reset must not call it.
        std::array<unsigned char, kPatternBytes> read_back{};
        sram::neo_local_request read{};
        read.requester = neo_requester::cpu;
        read.command = sram::neo_command::read;
        read.address = sram_base + destination_offset;
        read.size = kPatternBytes;
        read.data = read_back.data();
        CHECK_MSG(core_.fabric().dbg_access(read) == kPatternBytes,
                  "core SRAM did not answer after a reset");
        CHECK_MSG(core_.sram().debug_bytes_written() >= kPatternBytes,
                  "the debug-write counter was cleared by a core reset. The "
                  "bytes it accounts for are still in SRAM, so clearing it "
                  "makes the report deny the memory it describes");
        CHECK_MSG(core_.sram().read_accesses() == 0
                      && core_.sram().bytes_written() == 0,
                  "core SRAM's workload counters survived the reset. The "
                  "fabric and every requester open a new counter epoch here, "
                  "so storage counters left running would stop reconciling "
                  "with the fabric's -- conservation is a per-epoch property");
        CHECK_MSG(read_back == pattern,
                  "core SRAM lost its contents to a core reset. Every engine "
                  "reports bytes it committed to that memory as still "
                  "committed, so wiping it makes those registers describe data "
                  "that no longer exists");

        // The job itself is abandoned, not completed.
        const std::uint32_t status = read32(dma_base + tpu::dma::reg::status);
        CHECK_MSG((status & tpu::dma::status_bit::busy) == 0,
                  "the DMA was still BUSY after a core reset");
        CHECK_MSG((status & tpu::dma::status_bit::done) == 0,
                  "an abandoned transfer reported DONE. Reset is not "
                  "completion (INTERFACE_CONTRACT.md §4)");

        // And the core still works afterwards.
        run_dma_transfer(dma_base, am::global_ram_base, sram_base + 0xB00, 16,
                         /*enable_irq=*/false);
        CHECK_MSG((read32(dma_base + tpu::dma::reg::status)
                   & tpu::dma::status_bit::done)
                      != 0,
                  "the core could not run a transfer after a mid-job reset");
        write32(dma_base + tpu::dma::reg::status, tpu::dma::status_bit::done);
    }

    /// Reset arriving at every point across a job, including the last cycle.
    ///
    /// The scenario above resets three cycles into a 2048-byte transfer, which
    /// is nowhere near the end and therefore cannot see this: if the reset path
    /// consumes simulated time *before* the components are told to reset, a job
    /// about to finish finishes inside that window, and the handler then takes
    /// its **idle** branch. `neo_dma::reset()` zeroes `BYTES_DONE` when nothing
    /// is busy, so a caller that reset an active job gets the semantics of
    /// resetting an idle one — retained data in SRAM with a committed count of
    /// zero.
    ///
    /// Swept rather than aimed. Hitting the last cycle exactly would be a test
    /// that depends on the engine's timing staying what it is today; walking
    /// the reset across the whole job covers the boundary wherever it sits.
    void reset_near_completion_keeps_its_accounting()
    {
        const std::uint64_t dma_base = am::dma_control(kChip, kCore);
        const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

        unsigned observed_active_with_bytes = 0;

        // Quarter-cycle steps, not whole cycles. Each iteration starts its
        // job at a different phase, so a whole-cycle sweep can step straight
        // over the window where the job is less than one cycle from done --
        // it did, and the negative control passed until the resolution was
        // raised. The failure appears at 21 cycles here.
        for (unsigned quarter = 1; quarter <= 100; ++quarter) {
            write32(dma_base + tpu::dma::reg::status,
                    tpu::dma::status_bit::w1c_mask);
            write32(dma_base + tpu::dma::reg::src_addr_lo,
                    static_cast<std::uint32_t>(am::global_ram_base));
            write32(dma_base + tpu::dma::reg::src_addr_hi, 0);
            write32(dma_base + tpu::dma::reg::dst_addr_lo,
                    static_cast<std::uint32_t>(sram_base + 0xC00));
            write32(dma_base + tpu::dma::reg::dst_addr_hi, 0);
            write32(dma_base + tpu::dma::reg::length, 256);
            write32(dma_base + tpu::dma::reg::irq_enable, 0);
            write32(dma_base + tpu::dma::reg::control,
                    tpu::dma::control_bit::start);

            wait((core_.config().cycle * quarter) / 4);

            const bool active = (read32(dma_base + tpu::dma::reg::status)
                                 & tpu::dma::status_bit::busy)
                != 0;
            const std::uint32_t committed_before
                = read32(dma_base + tpu::dma::reg::bytes_done_lo);

            core_.reset();

            const std::uint32_t committed_after
                = read32(dma_base + tpu::dma::reg::bytes_done_lo);

            if (active && committed_before > 0) {
                ++observed_active_with_bytes;
                CHECK_MSG(committed_after >= committed_before,
                          "resetting an active job at "
                              + std::to_string(quarter)
                              + " quarter-cycles dropped its "
                              + "committed byte count from "
                              + std::to_string(committed_before) + " to "
                              + std::to_string(committed_after)
                              + ". The bytes are still in SRAM, so the count "
                                "now describes less than the memory holds -- "
                                "the reset applied idle semantics to a job "
                                "that was active when reset was requested");
            }
        }

        CHECK_MSG(observed_active_with_bytes > 0,
                  "the sweep never caught the DMA active with bytes already "
                  "committed, so it proved nothing. Widen the range or "
                  "lengthen the transfer");

        write32(dma_base + tpu::dma::reg::status,
                tpu::dma::status_bit::w1c_mask);
    }

    /// A job started inside the reset pulse must be refused, not silently
    /// misconfigured.
    ///
    /// `reset()` returns in zero time but holds `i_rstn` low for one clock
    /// period, so there is a window in which the MXU's registers are live while
    /// the clocked modules behind them are held in reset. A `START` admitted
    /// there would write its `ConfigRegs` values into modules that cannot latch
    /// them and then run the array on a configuration that was silently
    /// dropped — which presents as an arithmetic defect, not a reset-timing
    /// one. Admission is refused instead, with a cause firmware can act on.
    void mxu_start_during_the_reset_pulse_is_refused()
    {
        const std::uint64_t sa_base = am::sa_control(kChip, kCore);
        const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

        core_.reset();   // returns immediately; `i_rstn` stays low for a cycle

        // Program and start inside the pulse.
        write32(sa_base + tpu::sauria::reg::dim_m, 4);
        write32(sa_base + tpu::sauria::reg::dim_n, 4);
        write32(sa_base + tpu::sauria::reg::dim_k, 4);
        write32(sa_base + tpu::sauria::reg::a_addr_lo,
                static_cast<std::uint32_t>(sram_base));
        write32(sa_base + tpu::sauria::reg::b_addr_lo,
                static_cast<std::uint32_t>(sram_base + 0x100));
        write32(sa_base + tpu::sauria::reg::c_addr_lo,
                static_cast<std::uint32_t>(sram_base + 0x200));
        write32(sa_base + tpu::sauria::reg::a_stride, 4);
        write32(sa_base + tpu::sauria::reg::b_stride, 4);
        write32(sa_base + tpu::sauria::reg::c_stride, 16);
        write32(sa_base + tpu::sauria::reg::datatype,
                tpu::sauria::datatype_value::int8_int32);
        write32(sa_base + tpu::sauria::reg::irq_enable, 0);
        write32(sa_base + tpu::sauria::reg::control,
                tpu::sauria::control_bit::start);

        const std::uint32_t status = read32(sa_base + tpu::sauria::reg::status);
        const std::uint32_t cause
            = read32(sa_base + tpu::sauria::reg::error_cause);

        CHECK_MSG((status & tpu::sauria::status_bit::busy) == 0,
                  "the MXU accepted a job while its clocked modules were held "
                  "in reset");
        // The native path, which is public through `matrix_engine()` and does
        // not go through the register file at all. Closing only the MMIO gate
        // left this one open, which is the same mistake as closing one entry
        // to the reset window and calling it shut.
        {
            tpu::sauria::job native{};
            native.m = 4;
            native.n = 4;
            native.k = 4;
            native.a_address = sram_base;
            native.b_address = sram_base + 0x100;
            native.c_address = sram_base + 0x200;
            native.a_stride_bytes = 4;
            native.b_stride_bytes = 4;
            native.c_stride_bytes = 16;
            native.datatype = tpu::sauria::datatype_value::int8_int32;
            CHECK_MSG(core_.matrix_engine().submit(native)
                          == tpu::sauria::submit_status::engine_in_reset,
                      "a native submit() during the reset pulse was accepted. "
                      "`i_rstn` alone cannot gate this: it is an sc_signal and "
                      "still reads high for a delta after reset() returns");
        }

        CHECK_MSG(cause
                      == static_cast<std::uint32_t>(
                          tpu::sauria::error_cause::engine_in_reset),
                  "a START during the reset pulse did not report "
                  "`engine_in_reset`; it read cause "
                      + std::to_string(cause)
                      + ". Without that the job would configure modules that "
                        "cannot latch the write and then run on a "
                        "configuration nobody applied");

        // Once reset deasserts the engine takes work again.
        wait(core_.config().cycle * 4);
        write32(sa_base + tpu::sauria::reg::status,
                tpu::sauria::status_bit::error);
        write32(sa_base + tpu::sauria::reg::control,
                tpu::sauria::control_bit::start);
        CHECK_MSG(read32(sa_base + tpu::sauria::reg::error_cause)
                      != static_cast<std::uint32_t>(
                          tpu::sauria::error_cause::engine_in_reset),
                  "the MXU still refuses work after reset deasserted");

        write32(sa_base + tpu::sauria::reg::control,
                tpu::sauria::control_bit::abort);
        wait(core_.config().cycle * 4);
        write32(sa_base + tpu::sauria::reg::status,
                tpu::sauria::status_bit::w1c_mask);
    }

    std::uint32_t read32(std::uint64_t address)
    {
        std::array<unsigned char, 4> bytes{};
        remote_.access(tlm::TLM_READ_COMMAND, address, bytes.data(), 4);
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data(), 4);
        return value;
    }

    void write32(std::uint64_t address, std::uint32_t value)
    {
        std::array<unsigned char, 4> bytes{};
        std::memcpy(bytes.data(), &value, 4);
        remote_.access(tlm::TLM_WRITE_COMMAND, address, bytes.data(), 4);
    }

    tpu_core& core_;
    remote_master& remote_;
    outside_world& outside_;
    bool ran_ = false;
};

} // namespace

int sc_main(int, char*[])
{
    tpu_core core_under_test("neo_core", core_config());
    outside_world outside("outside");
    remote_master remote("remote");

    core_under_test.external().bind(outside.socket);
    remote.socket.bind(core_under_test.inbound());

    checks scenario("checks", core_under_test, remote, outside);

    // The programming model changed observably this phase — a new
    // `ERROR_CAUSE` value and a new `START` refusal — so a driver reading
    // `VERSION` has to be able to tell it from what came before.
    CHECK_MSG(tpu::sauria::model_version >= 2,
              "the SA programming-model version was not bumped alongside "
              "`engine_in_reset`; firmware cannot distinguish this ABI from "
              "the one where cause 13 did not exist");

    // Identity is a construction-time property, so it is checkable before the
    // simulation starts. chip 1, core 1 is hart 3.
    CHECK_MSG(core_under_test.hart_id() == 3,
              "hart_id must be chip_linear_id * 2 + core_id "
              "(ARCHITECTURE.md §2)");
    CHECK(core_under_test.core_base() == am::core_base(kChip, kCore));

    // "Scalar and RVV remain one hart/memory path; no second vector processor
    // exists." Asserted structurally rather than by observation: VP++ exposes
    // one combined socket, so both accessors have to be the same object. A
    // second vector master would need a second socket, and there is nowhere
    // for one to hide.
    CHECK_MSG(core_under_test.cpu().has_unified_bus(),
              "the hart reports separate instruction and data buses; TPU_V3 "
              "requires one path so scalar and vector traffic cannot diverge");
    CHECK_MSG(&core_under_test.cpu().instr_bus()
                  == &core_under_test.cpu().data_bus(),
              "instruction and data fetch use different sockets, so this is "
              "not the single hart/memory path plan §4.2 freezes");

    // A core is not a NEO-CORE without its engines, and the geometry it
    // reports must be the one it contains: no build, manifest or report may
    // call the 64x64 bring-up array 128x128 (D14).
    CHECK(core_under_test.matrix_engine().identity().rows == 64);
    CHECK(core_under_test.matrix_engine().identity().columns == 64);

    // D16: a full-system core must not be built on a fabric that blocks its
    // requesters, and the core must be able to say which it got.
    CHECK_MSG(!core_under_test.hart_port().blocks_on_arbitration(),
              "this core was built 'annotated' but reports otherwise");

    bool threw = false;
    try {
        auto bad = core_config();
        bad.core = 2;
        tpu_core impossible("three_cores", bad);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw,
              "a core id outside 0..1 must be refused during elaboration; plan "
              "§4.1 freezes two cores per chip and a frozen value has to be "
              "rejected rather than accepted-and-warned");

    sc_core::sc_start(
        sc_core::sc_time(kWatchdogMicroseconds, sc_core::SC_US));

    CHECK_MSG(scenario.ran(),
              "the watchdog expired with work still in flight");

    std::cout << core_under_test.report();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_tpu_core: all checks passed\n";
    return 0;
}
