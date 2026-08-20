// SPDX-License-Identifier: Apache-2.0
//
// The Phase 8 chip composition gate.
//
// Both NEO-COREs were gated on their own in Phase 7, and the chip fabric is
// gated next door. What is new here is everything that only exists once there
// are two harts in one address space, so this file asks the questions a
// single-core bench could not:
//
//   * do the two harts have **different** identities, and does firmware see it?
//   * does a store aimed at the sibling core arrive at the sibling core, and
//     does it stay inside the chip while doing so?
//   * can one core reach the other's private SRAM by accident?
//   * does one core's completion interrupt stay off the other core's hart?
//   * does one lock make `lr`/`sc` and AMO atomic **between** the harts?
//   * does resetting the chip abandon work on both cores at once?
//
// **The harts run a real image and it is the same image on both.** Two
// different programs would let a wiring mistake hide behind the difference; one
// program that branches on `mhartid` cannot, because the only thing that makes
// the two harts behave differently is the identity under test. Each hart writes
// its own id into its own SRAM and into its **sibling's**, so a single pair of
// reads afterwards says whether both booted, whether the cross-core path works
// in both directions, and whether either of them went to the wrong core.
//
// **Both chip-fabric timing modes are run**, as separate CTest cases. They are
// not two flavours of the same run: `annotated` never yields, so nothing
// downstream of a core can ever be entered twice at once and a whole class of
// concurrency defect is unreachable. `arbitrated` blocks, and that is what
// found the one Phase 8 defect a single mode would have shipped — a core's hart
// and its DMA both inside the core's single external socket, which the chip
// fabric correctly refuses because it treats a core as one initiator. The
// annotated mode is the realistic full-system configuration; the arbitrated one
// is the mode that makes contention a behaviour, and Phase 9's real NoC hop will
// block the same way.
//
// Usage: test_tpu_chip [annotated|arbitrated]
// Exit codes: 0 pass, 1 fail.

#include "tpu_v3/chip/tpu_chip.h"

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

namespace tpu = cdc::components::tpu_v3;
namespace chip = cdc::components::tpu_v3::chip;
namespace core = cdc::components::tpu_v3::core;
namespace am = cdc::components::tpu_v3::address_map;

using chip::chip_destination;
using chip::chip_initiator;
using chip::tpu_chip;
using chip::tpu_chip_config;
using core::hart_destination;
using core::local_fabric_timing;

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

/// Chip 1, so `mhartid` is 2 and 3.
///
/// Deliberately not chip 0: its harts are 0 and 1, and 0 is what an
/// uninitialised CSR reads as. A hart-identity check on chip 0 core 0 cannot
/// fail — the same trap the Phase 7 firmware gate had to be moved off.
constexpr tpu::chip_id_t kChip = 1;
constexpr std::uint64_t kCapacity = 64 * 1024;
constexpr unsigned kMachineExternalInterrupt = 11;

/// The boot image both harts run. Assembled with the pinned cross toolchain
/// (`riscv-none-elf-as -march=rv32imafdv_zicsr_zvl512b -mabi=ilp32d`) and
/// pasted, so the gate needs no cross compiler:
///
///   csrr t0, mhartid          ; andi t1, t0, 1      -> own core index
///   lui  t2, 0xc8000          ; slli t3, t1, 25     -> chip 1 core base
///   add  t3, t2, t3           ; sw   t0, 0(t3)      -> own SRAM[0]   = mhartid
///   xori t4, t1, 1            ; slli t5, t4, 25     -> sibling base
///   add  t5, t2, t5           ; sw   t0, 256(t5)    -> sibling SRAM[0x100]
///   lui  t6, 0x1              ; addi t6, t6, -2048  -> mie.MEIE
///   csrw mie, t6              ; wfi
///   halt: j halt
///
/// It parks in `wfi` rather than spinning: a spinning hart fetches across the
/// chip fabric forever and would swamp every contention measurement below with
/// its own traffic.
constexpr std::uint32_t kBootProgram[] = {
    0xF14022F3u, 0x0012F313u, 0xC80003B7u, 0x01931E13u, 0x01C38E33u,
    0x005E2023u, 0x00134E93u, 0x019E9F13u, 0x01E38F33u, 0x105F2023u,
    0x00001FB7u, 0x800F8F93u, 0x304F9073u, 0x10500073u, 0x0000006Fu,
};

/// Where each hart writes its own id, and where it writes the sibling's copy.
constexpr std::uint64_t kOwnMark = 0x0;
constexpr std::uint64_t kSiblingMark = 0x100;

tpu_chip_config chip_config(
    chip::chip_fabric_timing timing = chip::chip_fabric_timing::annotated)
{
    tpu_chip_config config;
    config.chip = kChip;
    config.cycle = sc_core::sc_time(10, sc_core::SC_NS);
    config.timing = timing;
    for (auto& core : config.core) {
        core.sram_capacity_bytes = kCapacity;
        // Provisional physical values; D15 leaves them open until SRAM-macro,
        // frequency and PD inputs exist, and the schema has no defaults.
        core.fabric.data_width_bits = 128;
        core.fabric.bank_count = 4;
        core.fabric.mapping = tpu::bank_mapping::low_order_interleaved;
        core.fabric.pipeline_stages = 1;
        core.fabric.max_outstanding_per_requester = 1;
        core.fabric.arbitration = tpu::arbitration_policy::round_robin;
        core.timing = local_fabric_timing::annotated;
        core.cycle = sc_core::sc_time(10, sc_core::SC_NS);
        core.reset_pc = am::boot_rom_base;
    }
    return config;
}

/// Everything outside the chip: boot ROM and global RAM.
///
/// It also watches for an address inside the chip aperture. Nothing local may
/// ever arrive here — with `NoLoopback = 1` the mesh would refuse it, so a
/// single sighting is a containment defect and not a slow path (D1).
class outside_world : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<outside_world> socket;

    std::uint64_t fetches = 0;
    std::uint64_t data_accesses = 0;
    std::uint64_t saw_chip_local = 0;

    explicit outside_world(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , rom_(am::boot_rom_size, 0)
        , ram_(64 * 1024, 0)
    {
        socket.register_b_transport(this, &outside_world::b_transport);
        socket.register_transport_dbg(this, &outside_world::transport_dbg);

        for (unsigned i = 0; i < std::size(kBootProgram); ++i) {
            std::memcpy(&rom_[i * 4], &kBootProgram[i], 4);
        }
        // A recognisable pattern for the DMA jobs below to move.
        for (std::size_t i = 0; i < ram_.size(); ++i) {
            ram_[i] = static_cast<unsigned char>(0x40 + (i & 0x3F));
        }
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();

        if (address >= am::chip_base(kChip)
            && address < am::chip_base(kChip) + am::chip_aperture_stride) {
            ++saw_chip_local;
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (am::contains(am::boot_rom_base, am::boot_rom_size, address,
                         length)) {
            ++fetches;
            const std::uint64_t offset = address - am::boot_rom_base;
            if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
                std::memcpy(&rom_[offset], trans.get_data_ptr(), length);
            } else {
                std::memcpy(trans.get_data_ptr(), &rom_[offset], length);
            }
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
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

/// The host loader, or a remote chip, on the chip's inbound side.
class host_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<host_master> socket;

    explicit host_master(sc_core::sc_module_name name)
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

    unsigned int debug(tlm::tlm_command command, std::uint64_t address,
                       unsigned char* data, unsigned int length)
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
        return socket->transport_dbg(trans);
    }
};

class checks : public sc_core::sc_module {
public:
    checks(sc_core::sc_module_name name, tpu_chip& chip_under_test,
           host_master& host, outside_world& outside)
        : sc_core::sc_module(name)
        , chip_(chip_under_test)
        , host_(host)
        , outside_(outside)
    {
        SC_HAS_PROCESS(checks);
        SC_THREAD(run);
    }

    bool ran() const noexcept { return ran_; }

private:
    void run()
    {
        // Long enough for both harts to reach their `wfi`.
        wait(sc_core::sc_time(5, sc_core::SC_US));

        the_two_harts_have_different_identities();
        both_harts_booted_through_the_one_external_port();
        each_hart_reached_its_sibling_and_stayed_in_the_chip();
        a_remote_write_lands_in_the_named_core_only();
        debug_access_maps_to_the_named_core();
        one_cores_interrupt_stays_off_the_other_cores_hart();
        both_cores_move_data_concurrently();

        // Printed before the reset clears them. A gate whose numbers are never
        // shown is one nobody can tell apart from a gate that ran against an
        // idle chip.
        report_the_evidence();

        chip_reset_abandons_work_on_both_cores();

        ran_ = true;
    }

    // ── identity ─────────────────────────────────────────────────────────────

    void the_two_harts_have_different_identities()
    {
        CHECK(chip_.hart_id(0) == 2);
        CHECK(chip_.hart_id(1) == 3);

        // What firmware actually reads. `hart_id()` is the model's arithmetic;
        // `mhartid` is the value the ISS was constructed with, and the two
        // agreeing is the claim (D5).
        constexpr unsigned kMhartid = 0xF14;
        CHECK_MSG(chip_.core(0).cpu().read_csr_storage(kMhartid) == 2
                      && chip_.core(1).cpu().read_csr_storage(kMhartid) == 3,
                  "the harts do not report the ids the chip assigned them; "
                  "firmware partitions work by mhartid, so two harts agreeing "
                  "on it is a workload bug that looks like a firmware bug");

        CHECK_MSG(chip_.bus_lock_sharers() == tpu::cores_per_chip,
                  "the two harts are not on one LR/SC and AMO lock. Two "
                  "private locks exclude nobody (decision record D8)");
    }

    void both_harts_booted_through_the_one_external_port()
    {
        CHECK_MSG(outside_.fetches > 0,
                  "nothing fetched from GLOBAL_BOOT_ROM. Both reset vectors "
                  "are outside the chip, so the first fetch of each hart has "
                  "to cross its core's bridge and then the chip fabric");

        for (unsigned i = 0; i < tpu::cores_per_chip; ++i) {
            CHECK_MSG(chip_.core(i).hart_port().requests(
                          hart_destination::external)
                          > 0,
                      "core " + std::to_string(i)
                          + "'s hart never issued an external access, so it "
                            "never started");
        }

        const auto core0_out = chip_.fabric().routed(chip_initiator::core0,
                                                     chip_destination::outside);
        const auto core1_out = chip_.fabric().routed(chip_initiator::core1,
                                                     chip_destination::outside);
        CHECK_MSG(core0_out > 0 && core1_out > 0,
                  "both harts' traffic did not leave through the chip's one "
                  "external port; plan §4.4 gives a chip exactly one "
                  "aggregated NoC manager and this is where that is visible");

        // Each hart wrote its own id into its own SRAM. Read back through the
        // inbound path, which is also the first proof that path decodes.
        CHECK_MSG(read32(am::core_sram_base(kChip, 0) + kOwnMark) == 2,
                  "core 0's hart did not write its id into its own SRAM");
        CHECK_MSG(read32(am::core_sram_base(kChip, 1) + kOwnMark) == 3,
                  "core 1's hart did not write its id into its own SRAM");
    }

    // ── the cross-core path ──────────────────────────────────────────────────

    void each_hart_reached_its_sibling_and_stayed_in_the_chip()
    {
        // Hart 3 wrote into core 0 and hart 2 wrote into core 1: the marks are
        // crossed, which is what says each hart resolved *sibling* correctly
        // rather than writing twice into itself.
        CHECK_MSG(read32(am::core_sram_base(kChip, 0) + kSiblingMark) == 3,
                  "core 0's sibling mark is not hart 3's id; the cross-core "
                  "write went somewhere else");
        CHECK_MSG(read32(am::core_sram_base(kChip, 1) + kSiblingMark) == 2,
                  "core 1's sibling mark is not hart 2's id");

        CHECK_MSG(chip_.fabric().local_bypass() >= 2,
                  "the fabric counted no core-to-core traffic, so the marks "
                  "above arrived by some path this test does not know about");

        CHECK_MSG(outside_.saw_chip_local == 0,
                  "an address inside this chip's aperture was offered to the "
                  "mesh. `noc_interconnect` runs with NoLoopback = 1 and "
                  "refuses a target on a node that hosts an upstream port, so "
                  "this is a deadlock in the real system, not a slow path (D1)");

        // Containment is decided at the core boundary first. A local address
        // arriving on a core's outbound port would mean that decoder is wrong.
        CHECK(chip_.fabric().self_refused() == 0);
        CHECK(chip_.core(0).bridge().outbound_local_refused() == 0);
        CHECK(chip_.core(1).bridge().outbound_local_refused() == 0);
    }

    void a_remote_write_lands_in_the_named_core_only()
    {
        const std::uint64_t probe = 0x200;
        const std::uint32_t before1
            = read32(am::core_sram_base(kChip, 1) + probe);

        write32(am::core_sram_base(kChip, 0) + probe, 0xDEAD'BEEFu);

        CHECK_MSG(read32(am::core_sram_base(kChip, 0) + probe) == 0xDEAD'BEEFu,
                  "a remote write to core 0's SRAM did not land there");
        CHECK_MSG(read32(am::core_sram_base(kChip, 1) + probe) == before1,
                  "writing core 0's SRAM changed core 1's. The two apertures "
                  "are 32 MiB apart; anything else means the decode is using "
                  "an offset where it should use an absolute address");

        // The same address seen from the sibling core rather than from the
        // host: one address per resource, different path.
        CHECK_MSG(chip_.core(0).fabric()
                          .counters(tpu::sram::neo_requester::external_inbound)
                          .request_count
                      > 0,
                  "inbound traffic did not appear in core 0's local-fabric "
                  "counters, so it reached storage without being arbitrated "
                  "(D15 forbids exactly that)");

        // A transfer spanning both cores has no single owner and is refused.
        std::array<unsigned char, 8> bytes{};
        const auto status
            = host_.access(tlm::TLM_WRITE_COMMAND, am::core_base(kChip, 1) - 4,
                           bytes.data(), 8);
        CHECK_MSG(status != tlm::TLM_OK_RESPONSE,
                  "a write straddling both core apertures was accepted; there "
                  "is no correct answer for which core should have taken it");
    }

    void debug_access_maps_to_the_named_core()
    {
        std::array<unsigned char, 4> bytes{};
        CHECK(host_.debug(tlm::TLM_READ_COMMAND,
                          am::core_sram_base(kChip, 0) + kOwnMark,
                          bytes.data(), 4)
              == 4);
        std::uint32_t value0 = 0;
        std::memcpy(&value0, bytes.data(), 4);

        CHECK(host_.debug(tlm::TLM_READ_COMMAND,
                          am::core_sram_base(kChip, 1) + kOwnMark,
                          bytes.data(), 4)
              == 4);
        std::uint32_t value1 = 0;
        std::memcpy(&value1, bytes.data(), 4);

        CHECK_MSG(value0 == 2 && value1 == 3,
                  "a debug read of the two cores' SRAM returned the same "
                  "core's data. Debug is the path a loader and a host tool "
                  "use, and it decodes by the same rules as everything else "
                  "(INTERFACE_CONTRACT.md §8)");

        // Outside the chip is outside the chip on the debug path too.
        CHECK_MSG(host_.debug(tlm::TLM_READ_COMMAND, am::global_ram_base,
                              bytes.data(), 4)
                      == 0,
                  "an inbound debug access to a foreign address was served");
    }

    // ── interrupts ───────────────────────────────────────────────────────────

    /// Each core aggregates its own engines and delivers to its own hart
    /// (`ARCHITECTURE.md` §5). The chip has no interrupt controller, so the
    /// claim to check is **independence**, which needs two cores to state.
    void one_cores_interrupt_stays_off_the_other_cores_hart()
    {
        const auto fetches1_before = chip_.core(1).hart_port().requests(
            hart_destination::external);

        run_dma(0, am::global_ram_base,
                am::core_sram_base(kChip, 0) + 0x400, 64, /*enable_irq=*/true);

        CHECK_MSG(chip_.core(0).irq_pending(),
                  "core 0's DMA completed with its interrupt enabled and core "
                  "0 aggregated nothing");
        CHECK_MSG(!chip_.core(1).irq_pending(),
                  "core 0's completion raised core 1's interrupt line. The "
                  "engines belong to one core each and nothing aggregates "
                  "across the chip");

        wait(sc_core::sc_time(2, sc_core::SC_US));
        CHECK_MSG(chip_.core(1).hart_port().requests(
                      hart_destination::external)
                      == fetches1_before,
                  "core 1's hart woke out of `wfi` on core 0's interrupt. That "
                  "is the failure a signal-level check could not see: "
                  "`irq_pending()` staying false would still let a stray "
                  "`set_irq` reach the other ISS");

        // Acknowledge, so the next scenario starts from a quiet core.
        write32(am::dma_control(kChip, 0) + tpu::dma::reg::status,
                tpu::dma::status_bit::done);
        wait(chip_.core(0).config().cycle * 4);
        CHECK(!chip_.core(0).irq_pending());
    }

    // ── contention ───────────────────────────────────────────────────────────

    /// Both cores pulling from global RAM at the same time. One external port
    /// serves them, so this is the first time anything in TPU_V3 has two
    /// independent bulk movers behind one boundary.
    void both_cores_move_data_concurrently()
    {
        const std::uint64_t dst0 = am::core_sram_base(kChip, 0) + 0x800;
        const std::uint64_t dst1 = am::core_sram_base(kChip, 1) + 0x800;
        constexpr std::uint32_t kLength = 1024;

        const auto conflicts_before
            = chip_.fabric().port_conflicts(chip_destination::outside);

        start_dma(0, am::global_ram_base, dst0, kLength, /*enable_irq=*/false);
        start_dma(1, am::global_ram_base, dst1, kLength, /*enable_irq=*/false);

        wait_for_dma(0);
        wait_for_dma(1);

        CHECK_MSG(chip_.fabric().port_conflicts(chip_destination::outside)
                      > conflicts_before,
                  "nothing ever found the external port busy during the "
                  "concurrent run, so the two transfers did not overlap and "
                  "the correct results below say nothing about contention");

        // Both moved the same source bytes, and each landed in its own SRAM.
        for (std::uint64_t offset = 0; offset < kLength; offset += 256) {
            const std::uint32_t expected = expected_ram_word(offset);
            CHECK_MSG(read32(dst0 + offset) == expected,
                      "core 0's concurrent transfer is wrong at offset "
                          + std::to_string(offset));
            CHECK_MSG(read32(dst1 + offset) == expected,
                      "core 1's concurrent transfer is wrong at offset "
                          + std::to_string(offset));
        }

        CHECK_MSG(chip_.fabric().peak_in_flight(chip_initiator::core0) == 1
                      && chip_.fabric().peak_in_flight(chip_initiator::core1)
                          == 1,
                  "an initiator had two transactions in flight at once; "
                  "Revision 1 allows one, and two under a single identity make "
                  "every counter and every arbitration decision wrong");

        CHECK_MSG(chip_.fabric().routed(chip_initiator::core0,
                                        chip_destination::outside)
                          > 0
                      && chip_.fabric().routed(chip_initiator::core1,
                                               chip_destination::outside)
                          > 0,
                  "one of the two cores never used the external port");

        CHECK_MSG(chip_.fabric().target_errors() == 0,
                  "the fabric saw a target error during the concurrent run");

        // A core's hart and its DMA share one external socket, and the bridge
        // is what arbitrates between them. In `annotated` mode nothing yields,
        // so they never overlap and the counter stays at zero; in `arbitrated`
        // mode they do, and this is the only place it is visible.
        if (chip_.config().timing == chip::chip_fabric_timing::arbitrated) {
            CHECK_MSG(chip_.core(0).bridge().outbound_conflicts() > 0,
                      "core 0's hart and DMA never contended for the core's "
                      "one external socket, so the bridge arbiter added in "
                      "Phase 8 was not exercised by this run");
        }
    }

    // ── reset ────────────────────────────────────────────────────────────────

    void chip_reset_abandons_work_on_both_cores()
    {
        const std::uint64_t dst0 = am::core_sram_base(kChip, 0) + 0xC00;
        const std::uint64_t dst1 = am::core_sram_base(kChip, 1) + 0xC00;

        // Committed bytes, written before the reset, that must survive it.
        write32(am::core_sram_base(kChip, 0) + 0x1000, 0x1234'5678u);
        write32(am::core_sram_base(kChip, 1) + 0x1000, 0x8765'4321u);

        start_dma(0, am::global_ram_base, dst0, 4096, /*enable_irq=*/true);
        start_dma(1, am::global_ram_base, dst1, 4096, /*enable_irq=*/true);

        // Mid-flight, deliberately: a reset that only ever arrives at an idle
        // chip proves nothing about abandoning work.
        wait(chip_.core(0).config().cycle * 4);
        CHECK_MSG(dma_busy(0) || dma_busy(1),
                  "both transfers had already finished, so the reset below is "
                  "not interrupting anything");

        chip_.reset();

        // **Snapshot before anything else touches the chip.**
        //
        // `reset()` returns with the machine running, and both harts restart at
        // their reset vector. In a blocking configuration the very next MMIO
        // read yields, which is all they need to fetch again and move these
        // counters — so a check that reads them after the DMA status would be
        // asserting that no time passed rather than that the reset cleared
        // them. `reset()` consumes no simulated time and nothing between here
        // and it yields, so this snapshot is the state the reset left.
        const auto core0_requests
            = chip_.fabric().requests(chip_initiator::core0);
        const auto core1_requests
            = chip_.fabric().requests(chip_initiator::core1);
        const auto bypass_after = chip_.fabric().local_bypass();

        CHECK_MSG(core0_requests == 0 && core1_requests == 0
                      && bypass_after == 0,
                  "the chip fabric's counters survived the reset: core0 "
                      + std::to_string(core0_requests) + ", core1 "
                      + std::to_string(core1_requests) + ", bypass "
                      + std::to_string(bypass_after));

        CHECK_MSG(!dma_busy(0) && !dma_busy(1),
                  "a DMA was still BUSY after the chip reset");
        CHECK_MSG(!chip_.core(0).irq_pending()
                      && !chip_.core(1).irq_pending(),
                  "an interrupt survived the chip reset");

        // **Core SRAM keeps its contents.** Reset is a control-path signal;
        // every engine here promises that committed bytes stay committed, and
        // wiping the memory would make those promises describe data that is
        // gone (plan §11.5, D17).
        CHECK_MSG(read32(am::core_sram_base(kChip, 0) + 0x1000) == 0x1234'5678u
                      && read32(am::core_sram_base(kChip, 1) + 0x1000)
                          == 0x8765'4321u,
                  "the chip reset wiped core SRAM. Reset abandons work; it "
                  "does not un-commit bytes that were already written");

        // And the chip still works afterwards.
        write32(am::core_sram_base(kChip, 1) + 0x1004, 0x0BAD'F00Du);
        CHECK(read32(am::core_sram_base(kChip, 1) + 0x1004) == 0x0BAD'F00Du);
    }

    void report_the_evidence()
    {
        std::cout << "chip traffic before reset:\n"
                  << "  boot ROM fetches " << outside_.fetches
                  << ", global RAM accesses " << outside_.data_accesses
                  << ", chip-local addresses offered to the mesh "
                  << outside_.saw_chip_local << '\n'
                  << "  core0 -> outside "
                  << chip_.fabric().routed(chip_initiator::core0,
                                           chip_destination::outside)
                  << ", core1 -> outside "
                  << chip_.fabric().routed(chip_initiator::core1,
                                           chip_destination::outside)
                  << ", local bypass " << chip_.fabric().local_bypass()
                  << ", inbound " << chip_.fabric().requests(
                         chip_initiator::external_inbound)
                  << '\n'
                  << "  external port conflicts "
                  << chip_.fabric().port_conflicts(chip_destination::outside)
                  << '\n';
    }

    // ── helpers ──────────────────────────────────────────────────────────────

    /// The bytes `outside_world` seeded global RAM with, as a little-endian
    /// word. Recomputed here rather than read back from the model, so the
    /// comparison has an independent expectation.
    static std::uint32_t expected_ram_word(std::uint64_t offset)
    {
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const auto byte
                = static_cast<std::uint32_t>(0x40 + ((offset + i) & 0x3F));
            value |= byte << (8 * i);
        }
        return value;
    }

    bool dma_busy(unsigned core)
    {
        return (read32(am::dma_control(kChip, core) + tpu::dma::reg::status)
                & tpu::dma::status_bit::busy)
            != 0;
    }

    void start_dma(unsigned core, std::uint64_t source,
                   std::uint64_t destination, std::uint32_t length,
                   bool enable_irq)
    {
        const std::uint64_t base = am::dma_control(kChip, core);
        write32(base + tpu::dma::reg::src_addr_lo,
                static_cast<std::uint32_t>(source));
        write32(base + tpu::dma::reg::src_addr_hi,
                static_cast<std::uint32_t>(source >> 32));
        write32(base + tpu::dma::reg::dst_addr_lo,
                static_cast<std::uint32_t>(destination));
        write32(base + tpu::dma::reg::dst_addr_hi,
                static_cast<std::uint32_t>(destination >> 32));
        write32(base + tpu::dma::reg::length, length);
        write32(base + tpu::dma::reg::irq_enable,
                enable_irq ? tpu::dma::irq_enable_bit::completion : 0u);
        write32(base + tpu::dma::reg::control, tpu::dma::control_bit::start);
    }

    void wait_for_dma(unsigned core)
    {
        // Bounded: a poll loop with no bound turns a stuck engine into a hung
        // suite instead of a failed test.
        for (unsigned i = 0; i < 20000; ++i) {
            if (!dma_busy(core)) {
                return;
            }
            wait(chip_.core(core).config().cycle);
        }
        CHECK_MSG(false, "core " + std::to_string(core)
                             + "'s DMA never left BUSY");
    }

    void run_dma(unsigned core, std::uint64_t source,
                 std::uint64_t destination, std::uint32_t length,
                 bool enable_irq)
    {
        start_dma(core, source, destination, length, enable_irq);
        wait_for_dma(core);
    }

    std::uint32_t read32(std::uint64_t address)
    {
        std::array<unsigned char, 4> bytes{};
        host_.access(tlm::TLM_READ_COMMAND, address, bytes.data(), 4);
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data(), 4);
        return value;
    }

    void write32(std::uint64_t address, std::uint32_t value)
    {
        std::array<unsigned char, 4> bytes{};
        std::memcpy(bytes.data(), &value, 4);
        host_.access(tlm::TLM_WRITE_COMMAND, address, bytes.data(), 4);
    }

    tpu_chip& chip_;
    host_master& host_;
    outside_world& outside_;
    bool ran_ = false;
};

/// Configuration values the frozen architecture refuses.
void check_configuration_refusals()
{
    bool threw = false;
    try {
        auto config = chip_config();
        config.cores = 4;
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "a chip with four cores was accepted; plan §4.1 freezes "
                     "two, and the address map, the hart-id mapping and the "
                     "NoC initiator budget all depend on the frozen value");

    threw = false;
    try {
        auto config = chip_config();
        config.core[1].core = 1;
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw,
              "a per-core identity supplied by the caller was accepted. There "
              "is one source for it; a second one can disagree, and a core "
              "announcing the wrong index collides with its sibling's aperture "
              "and its sibling's mhartid");

    threw = false;
    try {
        auto config = chip_config();
        config.chip = tpu::max_chips;
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "a chip index past the 3-bit FlooNoC manager-id limit was "
                     "accepted (decision record D2)");
}

} // namespace

int sc_main(int argc, char* argv[])
{
    auto timing = chip::chip_fabric_timing::annotated;
    if (argc > 1) {
        const std::string requested = argv[1];
        if (requested == "arbitrated") {
            timing = chip::chip_fabric_timing::arbitrated;
        } else if (requested != "annotated") {
            std::cerr << "usage: test_tpu_chip [annotated|arbitrated]\n";
            return 1;
        }
    }

    check_configuration_refusals();

    tpu_chip chip_under_test("chip", chip_config(timing));
    outside_world outside("outside");
    host_master host("host");

    chip_under_test.external().bind(outside.socket);
    host.socket.bind(chip_under_test.inbound());

    checks scenario("checks", chip_under_test, host, outside);

    std::cout << chip_under_test.report();

    sc_core::sc_start(sc_core::sc_time(4, sc_core::SC_MS));

    CHECK_MSG(scenario.ran(), "the watchdog expired with checks outstanding");

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_tpu_chip: all checks passed\n";
    return 0;
}
