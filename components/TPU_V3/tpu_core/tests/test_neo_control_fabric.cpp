// SPDX-License-Identifier: Apache-2.0
//
// NEO control fabric tests — the AXI4-Lite half of the Phase 3 gate.
//
// The gate asks for alignment, strobe and error tests, and for a proof that no
// accelerator bulk payload is routed through the control fabric. The second
// one is the interesting requirement: it is satisfied not by inspecting what
// happens to be connected today, but by refusing every payload that is not
// exactly four aligned bytes with full strobes — so a future component that
// tried to move a 64-byte tensor through here would be rejected rather than
// quietly served.
//
// The re-entrancy check is the other one worth reading. D15 says the control
// plane accepts one transaction per initiator at a time. Nothing in the
// current wiring can violate that, so the test builds something that does: a
// target that calls back into the initiator that is still waiting for it.

#include "tpu_v3/core/core_registers.h"
#include "tpu_v3/core/neo_control_fabric.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"

namespace core = cdc::components::tpu_v3::core;
namespace am = cdc::components::tpu_v3::address_map;

using core::control_initiator;
using core::mmio_register_file;
using core::neo_control_fabric;
using core::register_block;

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
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__ << ':' \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

/// A control initiator. Deliberately thin: the point is to be able to issue an
/// illegal transaction as easily as a legal one, so every field of the payload
/// is settable.
class control_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<control_master> socket;

    explicit control_master(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status raw(tlm::tlm_command command,
                                 std::uint64_t address, unsigned char* data,
                                 unsigned int length,
                                 unsigned char* byte_enable = nullptr,
                                 unsigned int byte_enable_length = 0,
                                 unsigned int streaming_width = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_byte_enable_ptr(byte_enable);
        trans.set_byte_enable_length(byte_enable_length);
        trans.set_streaming_width(streaming_width);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);

        CHECK_MSG(trans.get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE,
                  "a path returned without setting a response status, which "
                  "is a defect in the target rather than a condition for the "
                  "caller to handle");
        CHECK_MSG(!trans.is_dmi_allowed(),
                  "DMI is disabled platform-wide (INTERFACE_CONTRACT.md §9)");
        return trans.get_response_status();
    }

    tlm::tlm_response_status write32(std::uint64_t address, std::uint32_t value)
    {
        return raw(tlm::TLM_WRITE_COMMAND, address,
                   reinterpret_cast<unsigned char*>(&value), 4);
    }

    tlm::tlm_response_status read32(std::uint64_t address, std::uint32_t& value)
    {
        return raw(tlm::TLM_READ_COMMAND, address,
                   reinterpret_cast<unsigned char*>(&value), 4);
    }

    unsigned int debug_read32(std::uint64_t address, std::uint32_t& value)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        return socket->transport_dbg(trans);
    }
};

/// Refuses everything it is given, so the fabric's propagation of a target's
/// own refusal can be told apart from a refusal the fabric invented.
class refusing_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<refusing_target> socket;
    std::uint64_t seen = 0;

    explicit refusing_target(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &refusing_target::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        ++seen;
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    }
};

/// Calls back into the initiator that is still waiting for it. Nothing in the
/// real wiring does this; it exists so the one-transaction-per-initiator rule
/// is enforced rather than merely never exercised.
class reentrant_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<reentrant_target> socket;
    control_master* master = nullptr;
    std::uint64_t reentry_address = 0;
    bool attempted = false;
    bool threw = false;

    explicit reentrant_target(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &reentrant_target::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        if (master != nullptr && !attempted) {
            attempted = true;
            try {
                (void)master->write32(reentry_address, 0x1234);
            } catch (const std::runtime_error&) {
                threw = true;
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

constexpr std::uint32_t kBlockCode(register_block block)
{
    return static_cast<std::uint32_t>(block);
}

} // namespace

int sc_main(int, char*[])
{
    // The real core-local control map, taken from `address_map.h` — the test
    // carries no address constant of its own beyond the chip/core indices.
    constexpr cdc::components::tpu_v3::chip_id_t chip = 0;
    constexpr cdc::components::tpu_v3::core_id_t core_id = 0;

    std::vector<core::control_target_spec> specs{
        {am::core_control(chip, core_id), am::core_control_size, "core"},
        {am::sa_control(chip, core_id), am::sa_control_size, "sa"},
        {am::dma_control(chip, core_id), am::dma_control_size, "dma"},
        {am::transform_control(chip, core_id), am::transform_control_size,
         "transform"},
        {am::core_counters(chip, core_id), am::core_counters_size, "counters"},
    };

    neo_control_fabric fabric("control_fabric", specs);

    mmio_register_file core_regs("core_regs", register_block::core,
                                 specs[0].base, specs[0].size);
    mmio_register_file sa_regs("sa_regs", register_block::sa, specs[1].base,
                               specs[1].size);
    mmio_register_file dma_regs("dma_regs", register_block::dma, specs[2].base,
                                specs[2].size);
    mmio_register_file transform_regs("transform_regs",
                                      register_block::transform, specs[3].base,
                                      specs[3].size);
    mmio_register_file counter_regs("counter_regs", register_block::counters,
                                    specs[4].base, specs[4].size);

    fabric.to[0].bind(core_regs.socket);
    fabric.to[1].bind(sa_regs.socket);
    fabric.to[2].bind(dma_regs.socket);
    fabric.to[3].bind(transform_regs.socket);
    fabric.to[4].bind(counter_regs.socket);

    control_master cpu("cpu");
    control_master inbound("inbound");
    cpu.socket.bind(fabric.from[static_cast<unsigned>(control_initiator::cpu)]);
    inbound.socket.bind(
        fabric.from[static_cast<unsigned>(control_initiator::external_inbound)]);

    // ── every window answers, and answers as itself ──────────────────────────
    //
    // Without the identity register a wrong-window decode is invisible: every
    // register file would answer and the driver would program the wrong
    // engine.
    const std::array<std::pair<std::uint64_t, register_block>, 5> windows{{
        {specs[0].base, register_block::core},
        {specs[1].base, register_block::sa},
        {specs[2].base, register_block::dma},
        {specs[3].base, register_block::transform},
        {specs[4].base, register_block::counters},
    }};
    for (const auto& [base, block] : windows) {
        std::uint32_t value = 0;
        CHECK(cpu.read32(base + mmio_register_file::reg_id, value)
              == tlm::TLM_OK_RESPONSE);
        CHECK_MSG(value == (mmio_register_file::id_magic | kBlockCode(block)),
                  std::string("window ") + core::to_string(block)
                      + " answered with the wrong identity");

        CHECK(cpu.read32(base + mmio_register_file::reg_version, value)
              == tlm::TLM_OK_RESPONSE);
        CHECK(value == mmio_register_file::model_version);

        // Round trip through the one writable register, which is what proves
        // the write reached this target rather than merely being accepted
        // upstream.
        const std::uint32_t token = 0xA5A50000u | kBlockCode(block);
        CHECK(cpu.write32(base + mmio_register_file::reg_scratch, token)
              == tlm::TLM_OK_RESPONSE);
        value = 0;
        CHECK(cpu.read32(base + mmio_register_file::reg_scratch, value)
              == tlm::TLM_OK_RESPONSE);
        CHECK(value == token);
    }

    // The engine windows are routed but not built, and say so rather than
    // claiming readiness (decision record D14 for Transform in particular).
    std::uint32_t status = 0;
    CHECK(cpu.read32(specs[1].base + mmio_register_file::reg_status, status)
          == tlm::TLM_OK_RESPONSE);
    CHECK_MSG((status & mmio_register_file::status_implemented) == 0,
              "the SA window must not report itself implemented in Phase 3");
    CHECK(cpu.read32(specs[3].base + mmio_register_file::reg_status, status)
          == tlm::TLM_OK_RESPONSE);
    CHECK_MSG((status & mmio_register_file::status_implemented) == 0,
              "the Transform window must report its unavailable capability, "
              "never fake readiness");
    CHECK(cpu.read32(specs[0].base + mmio_register_file::reg_status, status)
          == tlm::TLM_OK_RESPONSE);
    CHECK((status & mmio_register_file::status_implemented) != 0);

    // ── unimplemented offsets are defined, not erroneous ─────────────────────
    std::uint32_t value = 0xDEAD;
    CHECK(cpu.read32(specs[0].base + 0x40, value) == tlm::TLM_OK_RESPONSE);
    CHECK_MSG(value == 0,
              "an unimplemented register reads as zero with TLM_OK_RESPONSE, "
              "so a firmware register sweep need not know the implementation "
              "status of every offset");
    CHECK(core_regs.unimplemented_reads() == 1);

    const std::uint64_t dropped_before = core_regs.dropped_writes();
    CHECK(cpu.write32(specs[0].base + mmio_register_file::reg_id, 0x1111)
          == tlm::TLM_OK_RESPONSE);
    CHECK(core_regs.dropped_writes() == dropped_before + 1);
    CHECK(cpu.read32(specs[0].base + mmio_register_file::reg_id, value)
          == tlm::TLM_OK_RESPONSE);
    CHECK_MSG(value
                  == (mmio_register_file::id_magic
                      | kBlockCode(register_block::core)),
              "a write to a read-only register must be dropped, not applied");

    // ── the width, alignment and strobe rules ────────────────────────────────
    fabric.reset();
    std::array<unsigned char, 64> bulk{};

    // 1, 2 and 8 bytes: never silently widened, narrowed or split.
    CHECK(cpu.raw(tlm::TLM_READ_COMMAND, specs[0].base, bulk.data(), 1)
          == tlm::TLM_BURST_ERROR_RESPONSE);
    CHECK(cpu.raw(tlm::TLM_READ_COMMAND, specs[0].base, bulk.data(), 2)
          == tlm::TLM_BURST_ERROR_RESPONSE);
    CHECK(cpu.raw(tlm::TLM_READ_COMMAND, specs[0].base, bulk.data(), 8)
          == tlm::TLM_BURST_ERROR_RESPONSE);

    // A 64-byte vector-width payload — the accelerator bulk transfer that must
    // never be routed through the control plane (D15). Refused, and the target
    // never saw it.
    const std::uint64_t forwarded_before = fabric.forwarded(0);
    CHECK_MSG(cpu.raw(tlm::TLM_WRITE_COMMAND, specs[0].base, bulk.data(), 64)
                  == tlm::TLM_BURST_ERROR_RESPONSE,
              "a 64-byte payload must be refused by the control fabric, not "
              "split into sixteen register accesses");
    CHECK(fabric.forwarded(0) == forwarded_before);

    // Misaligned four bytes.
    CHECK(cpu.raw(tlm::TLM_READ_COMMAND, specs[0].base + 2, bulk.data(), 4)
          == tlm::TLM_BURST_ERROR_RESPONSE);

    // A partial strobe: refused, not applied as a read-modify-write, because
    // on a register with write-1-to-clear bits read-modify-write does the
    // wrong thing silently.
    std::array<unsigned char, 4> partial{0xFF, 0xFF, 0x00, 0x00};
    CHECK(cpu.raw(tlm::TLM_WRITE_COMMAND,
                  specs[0].base + mmio_register_file::reg_scratch, bulk.data(),
                  4, partial.data(), 4)
          == tlm::TLM_BURST_ERROR_RESPONSE);
    // An all-ones strobe is legal.
    std::array<unsigned char, 4> full{0xFF, 0xFF, 0xFF, 0xFF};
    CHECK(cpu.raw(tlm::TLM_WRITE_COMMAND,
                  specs[0].base + mmio_register_file::reg_scratch, bulk.data(),
                  4, full.data(), 4)
          == tlm::TLM_OK_RESPONSE);

    // A wrapped streaming transfer.
    CHECK(cpu.raw(tlm::TLM_READ_COMMAND, specs[0].base, bulk.data(), 4, nullptr,
                  0, 2)
          == tlm::TLM_BURST_ERROR_RESPONSE);

    // Neither read nor write.
    CHECK(cpu.raw(tlm::TLM_IGNORE_COMMAND, specs[0].base, bulk.data(), 4)
          == tlm::TLM_COMMAND_ERROR_RESPONSE);

    CHECK(fabric.protocol_errors() == 8);
    CHECK(fabric.decode_errors() == 0);

    // ── decode ───────────────────────────────────────────────────────────────
    // Inside the core aperture but in the reserved hole past the counters.
    CHECK(cpu.read32(am::core_base(chip, core_id) + 0x0105'0000ull, value)
          == tlm::TLM_ADDRESS_ERROR_RESPONSE);
    // Outside the core entirely.
    CHECK(cpu.read32(0x8000'0000ull, value) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
    CHECK(fabric.decode_errors() == 2);

    // ── both initiators reach every target, and are counted apart ────────────
    fabric.reset();
    CHECK(inbound.write32(specs[2].base + mmio_register_file::reg_scratch,
                          0x600D)
          == tlm::TLM_OK_RESPONSE);
    value = 0;
    CHECK(cpu.read32(specs[2].base + mmio_register_file::reg_scratch, value)
          == tlm::TLM_OK_RESPONSE);
    CHECK_MSG(value == 0x600D,
              "a remote master and the local core must reach the same "
              "register through the same address");
    CHECK(fabric.requests(control_initiator::external_inbound) == 1);
    CHECK(fabric.requests(control_initiator::cpu) == 1);
    CHECK(fabric.forwarded(2) == 2);
    CHECK(fabric.peak_in_flight(control_initiator::cpu) == 1);

    // ── debug transport: decode applies, counters do not ─────────────────────
    const std::uint64_t requests_before = fabric.requests(control_initiator::cpu);
    value = 0;
    CHECK(cpu.debug_read32(specs[2].base + mmio_register_file::reg_scratch,
                           value)
          == 4);
    CHECK(value == 0x600D);
    CHECK_MSG(fabric.requests(control_initiator::cpu) == requests_before,
              "a loader is not workload traffic and must not be counted");
    CHECK(cpu.debug_read32(0x8000'0000ull, value) == 0);

    // A debug read must agree with `b_transport`, register for register.
    // `reg_status` is the one a driver checks first, and a debug path that
    // answered zero there would tell a loader the block is unimplemented while
    // the firmware it loaded sees the opposite.
    for (const auto& [base, block] : windows) {
        std::uint32_t normal = 0;
        std::uint32_t debugged = 0xFFFF'FFFFu;
        CHECK(cpu.read32(base + mmio_register_file::reg_status, normal)
              == tlm::TLM_OK_RESPONSE);
        CHECK(cpu.debug_read32(base + mmio_register_file::reg_status, debugged)
              == 4);
        CHECK_MSG(debugged == normal,
                  std::string("the debug path disagrees with b_transport "
                              "about STATUS in the ")
                      + core::to_string(block) + " window");
    }

    // A command that is neither read nor write has no debug meaning, and must
    // not be served as a read that overwrites the caller's buffer.
    {
        std::array<unsigned char, 4> guard_bytes{0xC7, 0xC7, 0xC7, 0xC7};
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_IGNORE_COMMAND);
        trans.set_address(specs[0].base + mmio_register_file::reg_id);
        trans.set_data_ptr(guard_bytes.data());
        trans.set_data_length(4);
        CHECK(cpu.socket->transport_dbg(trans) == 0);
        for (auto byte : guard_bytes) {
            CHECK(byte == 0xC7);
        }
    }

    // ── a target's own refusal is propagated, not invented or swallowed ──────
    std::vector<core::control_target_spec> hostile_specs{
        {am::core_control(chip, core_id), am::core_control_size, "refusing"},
    };
    neo_control_fabric hostile_fabric("hostile_fabric", hostile_specs);
    refusing_target refuser("refuser");
    hostile_fabric.to[0].bind(refuser.socket);

    control_master hostile_master("hostile_master");
    hostile_master.socket.bind(
        hostile_fabric.from[static_cast<unsigned>(control_initiator::cpu)]);
    CHECK(hostile_master.write32(hostile_specs[0].base, 1)
          == tlm::TLM_GENERIC_ERROR_RESPONSE);
    CHECK(refuser.seen == 1);
    CHECK_MSG(hostile_fabric.target_errors() == 1,
              "a refusal by the target is counted apart from one the fabric "
              "raised: the access was legal and the target declined it");
    CHECK(hostile_fabric.protocol_errors() == 0);

    // ── one transaction per initiator, enforced ──────────────────────────────
    std::vector<core::control_target_spec> reentrant_specs{
        {am::core_control(chip, core_id), am::core_control_size, "reentrant"},
    };
    neo_control_fabric reentrant_fabric("reentrant_fabric", reentrant_specs);
    reentrant_target loop("loop");
    reentrant_fabric.to[0].bind(loop.socket);

    control_master loop_master("loop_master");
    loop_master.socket.bind(
        reentrant_fabric.from[static_cast<unsigned>(control_initiator::cpu)]);
    loop.master = &loop_master;
    loop.reentry_address = reentrant_specs[0].base;

    CHECK(loop_master.write32(reentrant_specs[0].base, 0x42)
          == tlm::TLM_OK_RESPONSE);
    CHECK(loop.attempted);
    CHECK_MSG(loop.threw,
              "a second transaction on an initiator that already has one "
              "outstanding must be refused: AXI4-Lite here accepts one at a "
              "time (decision record D15)");
    // And the guard released the port, so the next access still works. Without
    // the RAII release a single violation would make every later access look
    // like a defect.
    CHECK(loop_master.write32(reentrant_specs[0].base, 0x43)
          == tlm::TLM_OK_RESPONSE);

    // ── an overlapping control map is refused during elaboration ─────────────
    bool threw = false;
    try {
        std::vector<core::control_target_spec> overlapping{
            {am::core_control(chip, core_id), 0x0002'0000ull, "a"},
            {am::sa_control(chip, core_id), am::sa_control_size, "b"},
        };
        neo_control_fabric bad("overlapping_fabric", overlapping);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw,
              "there is no correct answer for which of two overlapping "
              "targets should have answered, so the map must be refused");

    std::cout << fabric.report();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_neo_control_fabric: all checks passed\n";
    return 0;
}
