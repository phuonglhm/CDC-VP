// SPDX-License-Identifier: Apache-2.0

#include "noc_soc_top.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <set>
#include <vector>

#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <floo_noc_model/noc_interconnect.h>
#include <floo_noc_model/noc_metrics.hpp>

#include <clint_tlm.h>
#include <plic_tlm.h>
#include <riscv_vp_wrapper.h>

#include <memory_tlm.h>
#include <uart.h>          // UartTLM                    (global)
#include <uart_host_bridge.h>
#include <i2c.h>           // i2c                        (global)
#include <spi_tlm.h>       // cdc::components::spi_tlm
#include <timer.h>         // cdc::components::Timer
#include <wdt_tlm.h>       // cdc::components::wdt_tlm
#include <pwm.h>           // PWM                        (global)
#include <dma_tlm.h>       // cdc::components::dma_tlm
#include <trng_tlm.h>      // cdc::components::trng_tlm
#include <clkmgr.h>        // Clkmgr                     (global)
#include <dmic.h>          // cdc::components::DmicTLM
#include <otp.h>           // otp                        (global)
#include <qspi_tlm.h>      // cdc::components::qspi_tlm
#include <flash_nor_tlm.h>
#include <rtc_tlm.h>       // cdc::components::rtc_tlm
#include <adc_tlm.h>       // cdc::components::adc_tlm
#include <gpio_tlm.h>      // cdc::components::gpio_tlm

#include <soc/noc_dashboard_protocol.h>

namespace cdc::platforms::noc_soc {
namespace {

using node = cdc::components::noc_interconnect::node;
using cpu_backend_t = cdc::cpu::riscv_vp_cpu;

// ── SoC address map (docs/peripheral_memory_map.md) ─────────────────────────
//
// The bases are the SoC-level assignment, unchanged, so firmware defines and
// existing tests read the same here as on VP_FX1_Full_SoC. What differs is only
// where each peripheral physically sits.
constexpr std::uint64_t kMmio = 0x1000;
constexpr std::uint64_t kUart0 = 0x1000'0000, kI2c0  = 0x1001'0000, kSpi0  = 0x1002'0000;
constexpr std::uint64_t kTimer0= 0x1003'0000, kWdt0  = 0x1004'0000, kPwm0  = 0x1005'0000;
constexpr std::uint64_t kDma0  = 0x1006'0000, kTrng0 = 0x1007'0000, kCmu0  = 0x1008'0000;
constexpr std::uint64_t kDmic0 = 0x100A'0000, kOtp0  = 0x100B'0000, kQspi0 = 0x100C'0000;
constexpr std::uint64_t kUart1 = 0x1010'0000, kI2c1  = 0x1011'0000, kSpi1  = 0x1012'0000;
constexpr std::uint64_t kTimer1= 0x1013'0000, kRtc0  = 0x1014'0000, kAdc0  = 0x1015'0000;
constexpr std::uint64_t kGpio0 = 0x1016'0000;
constexpr std::uint64_t kBootromBase = 0x0000'0000, kBootromSize = 0x1'0000;
constexpr std::uint64_t kClintBase = 0x0200'0000, kClintSize = 0x1'0000;
constexpr std::uint64_t kPlicBase  = 0x0C00'0000, kPlicSize  = 0x40'0000;
constexpr std::uint64_t kRamBase = 0x8000'0000, kRamSize = 0x100'0000;  // 16 MiB
/// Final RAM page reserved for the synthetic traffic survey.
///
/// The survey used to write at `kRamBase` and `kRamBase + 0x100`, corrupting
/// live firmware instructions. Survey mode keeps every synthetic RAM write in
/// this page, and firmware mode rejects any ELF PT_LOAD range that claims it.
constexpr std::uint64_t kSurveyScratch = kRamBase + kRamSize - 0x1000;
constexpr std::size_t   kFlashSize = 16u * 1024u * 1024u;
constexpr unsigned      kNumPlic = 31;   // sources 1..31, id 0 reserved

std::uint16_t read_u16(
    const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw std::invalid_argument("firmware ELF header is truncated");
    }
    return static_cast<std::uint16_t>(
        bytes[offset] | (static_cast<std::uint16_t>(bytes[offset + 1]) << 8));
}

std::uint32_t read_u32(
    const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw std::invalid_argument("firmware ELF header is truncated");
    }
    return static_cast<std::uint32_t>(bytes[offset])
        | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8)
        | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16)
        | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::string address_range(std::uint64_t first, std::uint64_t last)
{
    std::ostringstream text;
    text << "[0x" << std::hex << first << ", 0x" << last << ')';
    return text.str();
}

/// Refuse firmware that claims any byte of the page reserved for survey mode.
///
/// `p_memsz`, not only `p_filesz`, is checked so a BSS-only overlap is caught.
/// The check runs during platform construction, before `sc_start()` and before
/// the CPU loader can modify RAM.
void validate_firmware_image(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::invalid_argument("cannot open firmware ELF '" + path + "'");
    }
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};

    if (bytes.size() < 16 || bytes[0] != 0x7F || bytes[1] != 'E'
        || bytes[2] != 'L' || bytes[3] != 'F') {
        throw std::invalid_argument("firmware is not an ELF file: '" + path + "'");
    }
    const unsigned elf_class = bytes[4];
    if (elf_class != 1) {
        throw std::invalid_argument(
            "firmware ELF must be ELF32 for the RV32 CPU");
    }
    if (bytes[5] != 1) {
        throw std::invalid_argument(
            "firmware ELF must use little-endian encoding");
    }

    constexpr std::size_t header_size = 52;
    if (bytes.size() < header_size) {
        throw std::invalid_argument("firmware ELF header is truncated");
    }
    constexpr std::uint16_t riscv_machine = 243;
    if (read_u16(bytes, 18) != riscv_machine) {
        throw std::invalid_argument(
            "firmware ELF machine must be RISC-V");
    }
    const std::uint64_t ph_offset = read_u32(bytes, 28);
    const std::uint16_t ph_entry_size = read_u16(bytes, 42);
    const std::uint16_t ph_count = read_u16(bytes, 44);
    constexpr std::size_t minimum_ph_size = 32;
    if (ph_entry_size < minimum_ph_size) {
        throw std::invalid_argument(
            "firmware ELF program-header entry is too small");
    }
    if (ph_offset > bytes.size()
        || ph_count > (bytes.size() - static_cast<std::size_t>(ph_offset))
                / ph_entry_size) {
        throw std::invalid_argument(
            "firmware ELF program-header table is truncated");
    }

    constexpr std::uint32_t pt_load = 1;
    constexpr std::uint64_t scratch_end = kRamBase + kRamSize;
    for (std::uint16_t index = 0; index < ph_count; ++index) {
        const auto ph = static_cast<std::size_t>(ph_offset)
            + static_cast<std::size_t>(index) * ph_entry_size;
        if (read_u32(bytes, ph) != pt_load) {
            continue;
        }
        const std::uint64_t address = read_u32(bytes, ph + 12);
        const std::uint64_t file_size = read_u32(bytes, ph + 16);
        const std::uint64_t memory_size = read_u32(bytes, ph + 20);
        if (memory_size < file_size) {
            throw std::invalid_argument(
                "firmware PT_LOAD has p_memsz smaller than p_filesz");
        }
        if (memory_size == 0) {
            continue;
        }
        constexpr std::uint64_t rv32_address_space = 1ull << 32;
        if (memory_size > rv32_address_space - address) {
            throw std::invalid_argument(
                "firmware PT_LOAD exceeds the RV32 address space");
        }
        const auto end = address + memory_size;
        if (address < scratch_end && kSurveyScratch < end) {
            throw std::invalid_argument(
                "firmware PT_LOAD " + address_range(address, end)
                + " overlaps reserved survey scratch "
                + address_range(kSurveyScratch, scratch_end));
        }
    }
}

void validate_mode(noc_soc_mode mode, const std::string& firmware)
{
    switch (mode) {
    case noc_soc_mode::survey:
        if (!firmware.empty()) {
            throw std::invalid_argument("survey mode rejects a firmware ELF");
        }
        return;
    case noc_soc_mode::firmware:
        if (firmware.empty()) {
            throw std::invalid_argument(
                "firmware mode requires a firmware ELF");
        }
        validate_firmware_image(firmware);
        return;
    }
    throw std::invalid_argument("invalid noc_soc execution mode");
}

// ── Mesh floorplan ─────────────────────────────────────────────────────────
//
// This is the part a flat bus has no equivalent for. On `bus_router` every
// peripheral is equidistant; here the placement decides the latency, so it is a
// design input rather than an implementation detail.
//
// The layout below puts the CPU port at the origin, RAM one hop away because it
// carries the most traffic, the high-rate peripherals near it, and the
// configure-once blocks in the far corner where their cost does not matter.
//
//        x=0            x=1            x=2            x=3
//  y=3   dma *          cmu0 dmic0     rtc0 adc0      probe *
//                                      gpio0
//  y=2   trng0          timer1         i2c1 spi1      uart1
//  y=1   ram            dma0-regs      wdt0 pwm0      timer0
//  y=0   cpu *          uart0 clint    i2c0 spi0      bootrom plic
//                                                     otp0 qspi0
//
//  * = an AXI manager port. No target may share a node with one: the router
//      defaults to `NoLoopback = 1`, so a flit addressed to the node that
//      injected it is undeliverable and would wedge that port for good. The
//      interconnect refuses such a placement at construction.
//
// CLINT sits next to the CPU because every trap entry touches it. The boot ROM
// and the secure blocks share the far corner of the CPU's row.
constexpr node kCpuNode  {0, 0};   // manager
constexpr node kDmaNode  {0, 3};   // manager
constexpr node kProbeNode{3, 3};   // manager
constexpr node kRamNode  {0, 1};
constexpr node kUart0Node{1, 0};   // uart0, clint
constexpr node kSerial0  {2, 0};   // i2c0, spi0
constexpr node kBootNode {3, 0};   // bootrom, plic, otp0, qspi0
constexpr node kDmaRegs  {1, 1};   // the DMA's own register window
constexpr node kCtrlNode {2, 1};   // wdt0, pwm0
constexpr node kTimer0N  {3, 1};
constexpr node kTrngNode {0, 2};
constexpr node kTimer1N  {1, 2};
constexpr node kSerial1  {2, 2};   // i2c1, spi1
constexpr node kUart1Node{3, 2};
constexpr node kClkNode  {1, 3};   // cmu0, dmic0
constexpr node kIoNode   {2, 3};   // rtc0, adc0, gpio0

constexpr unsigned kTargetCount = 23;  // 20 peripherals, CLINT, PLIC, BOOTROM

/// Every mapped target with its address window and its node, in report order.
///
/// One table with two consumers, deliberately. Survey mode probes these blocks
/// and measures a whole `b_transport`, so it can report the peripheral's own
/// access latency as well as the network's. Firmware mode probes nothing — it
/// owes the zero-synthetic-transaction contract — so the same rows are filled
/// from the passive completion observer, which sees only network cycles.
///
/// Two guards keep this list honest, because it is maintained beside the
/// `add_target` calls rather than generated from them:
///
/// - a `static_assert` that it has exactly `kTargetCount` entries, so adding a
///   target without adding a row fails the build;
/// - `require_metrics_consistent()` rejects a per-block total larger than the
///   global one, which is what overlapping windows would produce.
///
/// Neither catches a row pointing at the wrong node or the wrong base address.
/// The survey does: it probes every block through the real address map, and
/// `[6] PERIPHERAL MAP & LATENCY` then shows the observer's mean beside the
/// probe's own measurement, where a mismatched window shows up as a block that
/// was probed but recorded no traffic.
struct block_info {
    const char* name;
    std::uint64_t base;
    std::uint64_t size;
    node where;
};

constexpr block_info kBlockMap[] = {
    {"ram",     kRamBase,     kRamSize,     kRamNode},
    {"bootrom", kBootromBase, kBootromSize, kBootNode},
    {"clint",   kClintBase,   kClintSize,   kUart0Node},
    {"plic",    kPlicBase,    kPlicSize,    kBootNode},
    {"uart0",   kUart0,  kMmio, kUart0Node},
    {"i2c0",    kI2c0,   kMmio, kSerial0},
    {"spi0",    kSpi0,   kMmio, kSerial0},
    {"timer0",  kTimer0, kMmio, kTimer0N},
    {"wdt0",    kWdt0,   kMmio, kCtrlNode},
    {"pwm0",    kPwm0,   kMmio, kCtrlNode},
    {"dma0",    kDma0,   kMmio, kDmaRegs},
    {"trng0",   kTrng0,  kMmio, kTrngNode},
    {"cmu0",    kCmu0,   kMmio, kClkNode},
    {"dmic0",   kDmic0,  kMmio, kClkNode},
    {"otp0",    kOtp0,   kMmio, kBootNode},
    {"qspi0",   kQspi0,  kMmio, kBootNode},
    {"uart1",   kUart1,  kMmio, kUart1Node},
    {"i2c1",    kI2c1,   kMmio, kSerial1},
    {"spi1",    kSpi1,   kMmio, kSerial1},
    {"timer1",  kTimer1, kMmio, kTimer1N},
    {"rtc0",    kRtc0,   kMmio, kIoNode},
    {"adc0",    kAdc0,   kMmio, kIoNode},
    {"gpio0",   kGpio0,  kMmio, kIoNode},
};

constexpr unsigned kBlockCount =
    static_cast<unsigned>(sizeof(kBlockMap) / sizeof(kBlockMap[0]));

static_assert(kBlockCount == kTargetCount,
              "the reported block map must cover every placed target");

/// Manhattan distance in mesh nodes. Under XY routing this is the hop count.
constexpr unsigned manhattan_hops(node from, node to)
{
    const auto dx = from.x > to.x ? from.x - to.x : to.x - from.x;
    const auto dy = from.y > to.y ? from.y - to.y : to.y - from.y;
    return dx + dy;
}

/// Index into `kBlockMap`, or `kBlockCount` when the address is unmapped.
///
/// An unmapped address is not an error here: the survey deliberately issues one
/// to prove it is refused rather than routed, and that access must not be
/// charged to a neighbouring block.
inline unsigned block_index(std::uint64_t address)
{
    for (unsigned index = 0; index < kBlockCount; ++index) {
        const auto& block = kBlockMap[index];
        if (address >= block.base && address - block.base < block.size) {
            return index;
        }
    }
    return kBlockCount;
}

/// Supplied by CMake so a measurement baseline records what it was built with.
/// A latency number from a Debug build means something different from the same
/// number in Release, and an artifact that does not say which is not evidence.
#ifndef NOC_SOC_BUILD_TYPE
#define NOC_SOC_BUILD_TYPE "unknown"
#endif
constexpr const char* kBuildType = NOC_SOC_BUILD_TYPE;

/// A spin loop, `jal x0, 0`, little endian. Without firmware the CPU would
/// fetch zeroed RAM, trap, and fetch more zeros — a trap storm that says
/// nothing about the interconnect. This gives it a valid instruction to fetch
/// so its traffic is representative.
constexpr std::uint8_t kSpinLoop[] = {0x6F, 0x00, 0x00, 0x00};

// DMA test buffers, well clear of the CPU's instruction stream.
constexpr std::uint64_t kDmaProgram = kRamBase + 0x8'0000;
constexpr std::uint64_t kDmaSrc     = kRamBase + 0x8'0100;
constexpr std::uint64_t kDmaDst     = kRamBase + 0x8'0200;
// 512 bytes, moved as 32 sixteen-byte bursts. An earlier 32-byte transfer
// finished before it could contend with anything: on a 4x4 mesh that is simply
// too little traffic to queue behind, and the measurement duly read zero.
constexpr unsigned      kDmaCopyLen = 512;
constexpr unsigned      kDmaBurstBytes = 16;
constexpr unsigned      kDmaEventDone = 3;
/// A RAM word well away from the DMA's buffers, reached over the same column
/// of the mesh the DMA uses. Reading it during the transfer is what exposes
/// link contention.
constexpr std::uint64_t kDmaProbeAddr = kRamBase + 0x9'0000;

/// A benign off-chip SPI peripheral so the controller's initiator is bound.
class spi_dummy : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<spi_dummy> socket;
    explicit spi_dummy(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket")
    {
        socket.register_b_transport(this, &spi_dummy::b_transport);
    }
private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

/// Idle PDM source, so DMIC's stimulus socket is bound.
class pdm_dummy : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<pdm_dummy> socket;
    explicit pdm_dummy(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket") {}
};

/// One measured access, for the report at the end of the run.
struct probe_result {
    const char* name;
    node where;
    unsigned hops;
    sc_core::sc_time latency;
    /// The interconnect's own share, with the target's access latency removed.
    std::uint64_t network_cycles = 0;
    /// Whether the peripheral accepted the access. A refusal still travels the
    /// network both ways, so the latency is measured either way — it is the
    /// register choice that was wrong, not the interconnect.
    bool accepted = true;
};

/// Walks the whole peripheral map and reports what each access cost.
///
/// On a flat bus every one of these would be identical, which is exactly why
/// the numbers are worth printing here.
class traffic_stub : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<traffic_stub> bus_socket;
    cdc::components::noc_interconnect* noc = nullptr;
    cdc::cpu::cpu_base* cpu = nullptr;
    noc_soc_mode mode = noc_soc_mode::survey;
    /// Requested measurement/firmware execution window.
    double sim_us = 0.0;
    /// Last time the CPU's retire count advanced. Everything after that is the
    /// CPU idling, and must not be charged to its instructions.
    sc_core::sc_time cpu_active_until = sc_core::SC_ZERO_TIME;

    // ── Step 12.7 measurement baseline ──────────────────────────────────────
    //
    // Everything below is passive. It is fed by `noc_interconnect`'s completion
    // observer, which reports each transaction where its latency becomes final,
    // and it never issues traffic, consumes time or touches a payload.
    //
    // The observer exists because `last_latency_cycles()` cannot attribute
    // anything under concurrent traffic: it holds only the most recent
    // completion. PLIC claim and PLIC complete are the same target at the same
    // address, separated only by direction, so polling a global afterwards
    // would attribute one to the other.

    using transaction_metrics = floo::model::transaction_metrics;

    bool baseline_enabled = false;
    noc_timing_mode timing = noc_timing_mode::detailed;
    std::string firmware_path;
    std::string metrics_path;
    transaction_metrics all_stats;
    std::array<transaction_metrics, 3> manager_stats;
    transaction_metrics plic_claim_stats;
    transaction_metrics plic_complete_stats;
    transaction_metrics cpu_ram_quiet_stats;
    transaction_metrics cpu_ram_dma_active_stats;
    transaction_metrics cpu_mmio_stats;
    transaction_metrics dma_manager_stats;
    static constexpr unsigned kTargetRamIndex = 0;
    static constexpr unsigned kTargetPlicIndex = 1;
    static constexpr unsigned kTargetOtherIndex = 2;
    static constexpr unsigned kTargetMetricCount = 3;
    std::array<transaction_metrics, kTargetMetricCount> target_stats;
    std::array<
        std::array<transaction_metrics, kTargetMetricCount>, 3> flow_stats;

    /// Per-block traffic, keyed by `kBlockMap`. Filled by the completion
    /// observer in either mode, so a firmware run reports the blocks its own
    /// software actually touched rather than a synthetic walk of the map.
    std::array<transaction_metrics, kBlockCount> block_stats;

    /// What survey mode's directed register read cost, per block.
    ///
    /// `total` is the whole `b_transport`, so it carries the peripheral's own
    /// access latency; `network_cycles` is the interconnect's share alone. The
    /// completion observer cannot supply the first of those — it reports
    /// network cycles with the target hold-off already excluded — which is why
    /// this is recorded separately instead of derived from `block_stats`.
    struct block_survey_result {
        bool measured = false;
        bool accepted = true;
        std::uint64_t network_cycles = 0;
        sc_core::sc_time total{sc_core::SC_ZERO_TIME};
    };
    std::array<block_survey_result, kBlockCount> block_survey;

    /// Modeled time from the DMA channel's architectural start transition to
    /// the completion interrupt asserting. The start comes from dma_tlm's
    /// passive channel observer; using completion of the CPU's DBGCMD write
    /// would begin after the DMA had already been scheduled and incorrectly
    /// omit the NoC response path.
    sc_core::sc_time dma_start_at{sc_core::SC_ZERO_TIME};
    bool dma_start_pending = false;
    unsigned dma_completion_samples = 0;
    sc_core::sc_time dma_completion_min{sc_core::SC_ZERO_TIME};
    sc_core::sc_time dma_completion_max{sc_core::SC_ZERO_TIME};
    sc_core::sc_time dma_completion_total{sc_core::SC_ZERO_TIME};

    /// PLIC context 0 claim/complete register. A read claims, a write
    /// completes; the address is identical, which is why direction is part of
    /// the classification rather than an afterthought.
    static constexpr std::uint64_t kPlicClaimAddr = kPlicBase + 0x20'0004;
    static constexpr unsigned kCpuPortIndex = 0;
    static constexpr unsigned kDmaPortIndex = 1;
    static constexpr unsigned kProbePortIndex = 2;

    static unsigned target_metric_index(std::uint64_t address)
    {
        if (address >= kRamBase && address < kRamBase + kRamSize) {
            return kTargetRamIndex;
        }
        if (address >= kPlicBase && address < kPlicBase + kPlicSize) {
            return kTargetPlicIndex;
        }
        return kTargetOtherIndex;
    }

    void observe_completion(
        const cdc::components::noc_interconnect::completion& done)
    {
        all_stats.add(done.length, done.latency_cycles);
        const unsigned target = target_metric_index(done.address);
        target_stats[target].add(done.length, done.latency_cycles);
        const unsigned block = block_index(done.address);
        if (block < kBlockCount) {
            block_stats[block].add(done.length, done.latency_cycles);
        }
        if (done.port < manager_stats.size()) {
            manager_stats[done.port].add(done.length, done.latency_cycles);
            flow_stats[done.port][target].add(
                done.length, done.latency_cycles);
        }
        if (done.port == kDmaPortIndex) {
            dma_manager_stats.add(done.length, done.latency_cycles);
            return;
        }
        if (done.port != kCpuPortIndex) {
            return;
        }

        if (done.address == kPlicClaimAddr) {
            if (done.is_write) {
                plic_complete_stats.add(done.length, done.latency_cycles);
            } else {
                plic_claim_stats.add(done.length, done.latency_cycles);
            }
            return;
        }
        if (done.address >= kRamBase && done.address < kRamBase + kRamSize) {
            // "While DMA is active" means the DMA manager had at least one
            // transaction admitted at the moment this one completed. It is a
            // classification, not a claim that the two overlapped for their
            // whole flight; a CPU access is short enough that the distinction
            // rarely matters, and stating the rule is better than implying a
            // stronger one.
            if (noc->outstanding_transactions(kDmaPortIndex) > 0) {
                cpu_ram_dma_active_stats.add(
                    done.length, done.latency_cycles);
            } else {
                cpu_ram_quiet_stats.add(done.length, done.latency_cycles);
            }
            return;
        }
        cpu_mmio_stats.add(done.length, done.latency_cycles);
    }

    void note_dma_channel_start(unsigned channel)
    {
        if (channel != 0) {
            return;
        }
        dma_start_at = sc_core::sc_time_stamp();
        dma_start_pending = true;
    }

    /// Called by the impl when the DMA completion line rises. The signal
    /// lives up there; the accounting lives here, next to everything else the
    /// end-of-run report needs.
    void note_dma_completion_irq()
    {
        if (!dma_start_pending) {
            return;
        }
        dma_start_pending = false;
        const auto elapsed = sc_core::sc_time_stamp() - dma_start_at;
        if (dma_completion_samples == 0 || elapsed < dma_completion_min) {
            dma_completion_min = elapsed;
        }
        if (elapsed > dma_completion_max) {
            dma_completion_max = elapsed;
        }
        dma_completion_total += elapsed;
        ++dma_completion_samples;
    }

    static void print_latency(
        const char* label, const transaction_metrics& stats)
    {
        std::cout << "    " << label << ": n=" << stats.transactions();
        if (stats.transactions() == 0) {
            std::cout << " (not exercised)\n";
            return;
        }
        const auto& latency = stats.latency();
        std::cout << " min=" << latency.min()
                  << " mean=" << latency.mean()
                  << " max=" << latency.max() << " cycles\n";
        std::cout << "      payload=" << stats.payload_bytes()
                  << " bytes p50=" << latency.percentile(50, 100)
                  << " p95=" << latency.percentile(95, 100)
                  << " p99=" << latency.percentile(99, 100)
                  << " cycles\n";
    }

    struct mesh_summary {
        std::uint64_t accepted_flits = 0;
        std::uint64_t accepted_packets = 0;
        std::uint64_t stall_cycles = 0;
        std::uint64_t busy_cycles = 0;
        unsigned input_high_water = 0;
        unsigned output_high_water = 0;
        double peak_link_utilisation = 0.0;
        double peak_link_stall_ratio = 0.0;
        unsigned peak_x = 0;
        unsigned peak_y = 0;
        unsigned peak_port = 0;
    };

    static mesh_summary summarise_mesh(
        const floo::model::mesh_counter_snapshot& mesh,
        std::uint64_t measurement_cycles = 0)
    {
        mesh_summary result{};
        for (std::size_t node_index = 0;
             node_index < mesh.routers.size(); ++node_index) {
            const auto& router = mesh.routers[node_index];
            for (unsigned port = 0;
                 port < floo::model::router_counter_snapshot::num_ports;
                 ++port) {
                const auto& output = router.outputs[port];
                result.accepted_flits += output.accepted_flits;
                result.accepted_packets += output.accepted_packets;
                result.stall_cycles += output.stall_cycles;
                result.busy_cycles += output.busy_cycles;
                result.input_high_water = std::max(
                    result.input_high_water,
                    router.input_buffers[port].high_water);
                result.output_high_water = std::max(
                    result.output_high_water,
                    router.output_buffers[port].high_water);

                // Eject is an endpoint boundary, not an inter-router link.
                if (port == floo::model::to_port(
                                floo::model::direction::eject)) {
                    continue;
                }
                const std::uint64_t denominator = measurement_cycles == 0
                    ? router.counted_cycles : measurement_cycles;
                const double utilisation = denominator == 0
                    ? 0.0
                    : static_cast<double>(output.accepted_flits)
                        / static_cast<double>(denominator);
                const double stall_ratio = output.busy_cycles == 0
                    ? 0.0
                    : static_cast<double>(output.stall_cycles)
                        / static_cast<double>(output.busy_cycles);
                if (utilisation > result.peak_link_utilisation) {
                    result.peak_link_utilisation = utilisation;
                    result.peak_x =
                        static_cast<unsigned>(node_index % mesh.width);
                    result.peak_y =
                        static_cast<unsigned>(node_index / mesh.width);
                    result.peak_port = port;
                }
                result.peak_link_stall_ratio =
                    std::max(result.peak_link_stall_ratio, stall_ratio);
            }
        }
        return result;
    }

    static void print_mesh_summary(
        const char* channel,
        const floo::model::mesh_counter_snapshot& mesh)
    {
        const auto measurement_cycles = static_cast<std::uint64_t>(
            sc_core::sc_time_stamp()
            / sc_core::sc_time(1, sc_core::SC_NS));
        const auto summary = summarise_mesh(mesh, measurement_cycles);
        std::cout << "    " << channel << ": "
                  << summary.accepted_flits << " accepted flits, "
                  << summary.accepted_packets << " packets, "
                  << summary.stall_cycles << " output stall cycles\n"
                  << "      peak directed-link utilisation "
                  << summary.peak_link_utilisation * 100.0 << "% at ("
                  << summary.peak_x << ',' << summary.peak_y << ") "
                  << floo::model::to_string(
                         floo::model::direction_from_port(summary.peak_port))
                  << "; peak stall ratio "
                  << summary.peak_link_stall_ratio * 100.0 << "%\n"
                  << "      FIFO high-water input="
                  << summary.input_high_water << " output="
                  << summary.output_high_water << '\n';
    }

    void require_metrics_consistent() const
    {
        const transaction_metrics* const buckets[] = {
            &all_stats,
            &manager_stats[0],
            &manager_stats[1],
            &manager_stats[2],
            &plic_claim_stats,
            &plic_complete_stats,
            &cpu_ram_quiet_stats,
            &cpu_ram_dma_active_stats,
            &cpu_mmio_stats,
            &dma_manager_stats,
        };
        for (const auto* bucket : buckets) {
            if (!bucket->valid()) {
                throw std::runtime_error(
                    "noc_soc: metric counter overflow or invalid histogram");
            }
        }
        if (noc == nullptr) {
            throw std::logic_error("noc_soc: metrics have no interconnect");
        }
        if (all_stats.transactions() != noc->completed_transactions()
            || all_stats.latency().sum() != noc->total_latency_cycles()) {
            throw std::runtime_error(
                "noc_soc: transaction histogram does not reconcile with NoC "
                "completion totals");
        }
        for (const auto& target : target_stats) {
            if (!target.valid()) {
                throw std::runtime_error(
                    "noc_soc: target metric counter overflow or invalid "
                    "histogram");
            }
        }
        // Per-block counts must not exceed the global total. They are a
        // partition of the mapped traffic, so an over-count means one
        // completion was attributed to two blocks — the failure mode an
        // overlapping window in `kBlockMap` would produce.
        std::uint64_t block_total = 0;
        for (const auto& block : block_stats) {
            if (!block.valid()) {
                throw std::runtime_error(
                    "noc_soc: per-block metric counter overflow or invalid "
                    "histogram");
            }
            block_total += block.transactions();
        }
        if (block_total > all_stats.transactions()) {
            throw std::runtime_error(
                "noc_soc: per-block transaction counts exceed the global "
                "total; the block map windows overlap");
        }
        for (const auto& manager : flow_stats) {
            for (const auto& flow : manager) {
                if (!flow.valid()) {
                    throw std::runtime_error(
                        "noc_soc: flow metric counter overflow or invalid "
                        "histogram");
                }
            }
        }
        std::uint64_t manager_transactions = 0;
        std::uint64_t manager_bytes = 0;
        for (const auto& manager : manager_stats) {
            manager_transactions += manager.transactions();
            manager_bytes += manager.payload_bytes();
        }
        if (manager_transactions != all_stats.transactions()
            || manager_bytes != all_stats.payload_bytes()) {
            throw std::runtime_error(
                "noc_soc: per-manager metrics do not reconcile with global "
                "totals");
        }

        std::uint64_t target_transactions = 0;
        std::uint64_t target_bytes = 0;
        for (const auto& target : target_stats) {
            target_transactions += target.transactions();
            target_bytes += target.payload_bytes();
        }
        if (target_transactions != all_stats.transactions()
            || target_bytes != all_stats.payload_bytes()) {
            throw std::runtime_error(
                "noc_soc: per-target metrics do not reconcile with global "
                "totals");
        }

        std::uint64_t flow_transactions = 0;
        std::uint64_t flow_bytes = 0;
        for (unsigned manager = 0; manager < manager_stats.size(); ++manager) {
            std::uint64_t row_transactions = 0;
            std::uint64_t row_bytes = 0;
            for (unsigned target = 0; target < kTargetMetricCount; ++target) {
                const auto& flow = flow_stats[manager][target];
                row_transactions += flow.transactions();
                row_bytes += flow.payload_bytes();
            }
            if (row_transactions != manager_stats[manager].transactions()
                || row_bytes != manager_stats[manager].payload_bytes()) {
                throw std::runtime_error(
                    "noc_soc: traffic-matrix row does not reconcile with its "
                    "manager total");
            }
            flow_transactions += row_transactions;
            flow_bytes += row_bytes;
        }
        if (flow_transactions != all_stats.transactions()
            || flow_bytes != all_stats.payload_bytes()) {
            throw std::runtime_error(
                "noc_soc: traffic matrix does not reconcile with global "
                "totals");
        }
        for (unsigned target = 0; target < kTargetMetricCount; ++target) {
            std::uint64_t column_transactions = 0;
            std::uint64_t column_bytes = 0;
            for (unsigned manager = 0; manager < manager_stats.size();
                 ++manager) {
                column_transactions +=
                    flow_stats[manager][target].transactions();
                column_bytes += flow_stats[manager][target].payload_bytes();
            }
            if (column_transactions != target_stats[target].transactions()
                || column_bytes != target_stats[target].payload_bytes()) {
                throw std::runtime_error(
                    "noc_soc: traffic-matrix column does not reconcile with "
                    "its target total");
            }
        }
    }

    static std::string json_escape(const std::string& value)
    {
        std::ostringstream escaped;
        escaped << '"';
        for (const unsigned char byte : value) {
            switch (byte) {
            case '"': escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (byte < 0x20) {
                    escaped << "\\u00" << std::hex << std::setw(2)
                            << std::setfill('0')
                            << static_cast<unsigned>(byte) << std::dec
                            << std::setfill(' ');
                } else {
                    escaped << static_cast<char>(byte);
                }
            }
        }
        escaped << '"';
        return escaped.str();
    }

    static std::string generated_utc()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t epoch = std::chrono::system_clock::to_time_t(now);
        std::tm utc{};
#if defined(_POSIX_VERSION)
        gmtime_r(&epoch, &utc);
#else
        const std::tm* converted = std::gmtime(&epoch);
        if (converted == nullptr) {
            return "unavailable";
        }
        utc = *converted;
#endif
        std::ostringstream text;
        text << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
        return text.str();
    }

    static void write_metric(
        std::ostream& out, double value, const char* unit, const char* source,
        std::uint64_t samples = 0, const char* formula = nullptr)
    {
        out << "{\"value\":" << std::setprecision(12) << value
            << ",\"unit\":" << json_escape(unit)
            << ",\"source\":" << json_escape(source);
        if (samples != 0) {
            out << ",\"samples\":" << samples;
        }
        if (formula != nullptr) {
            out << ",\"formula\":" << json_escape(formula);
        }
        out << '}';
    }

    static void write_null_metric(
        std::ostream& out, const char* unit, const char* source,
        std::uint64_t samples = 0)
    {
        out << "{\"value\":null,\"unit\":" << json_escape(unit)
            << ",\"source\":" << json_escape(source);
        if (samples != 0) {
            out << ",\"samples\":" << samples;
        }
        out << '}';
    }

    static void write_transaction_metrics(
        std::ostream& out, const transaction_metrics& metrics,
        const char* name = nullptr)
    {
        out << '{';
        if (name != nullptr) {
            out << "\"name\":" << json_escape(name) << ',';
        }
        out << "\"transactions\":";
        write_metric(
            out, static_cast<double>(metrics.transactions()), "transactions",
            "M", metrics.transactions());
        out << ",\"payload_bytes\":";
        write_metric(
            out, static_cast<double>(metrics.payload_bytes()), "bytes", "M",
            metrics.transactions());
        out << ",\"latency_cycles\":{";
        const auto& latency = metrics.latency();
        const auto write_latency = [&](const char* field, double value) {
            out << json_escape(field) << ':';
            write_metric(
                out, value, "cycles", "M", metrics.transactions());
        };
        if (metrics.transactions() == 0) {
            const char* fields[] = {"min", "mean", "p50", "p95", "p99", "max"};
            for (unsigned index = 0; index < 6; ++index) {
                if (index != 0) out << ',';
                out << json_escape(fields[index]) << ':';
                write_null_metric(out, "cycles", "M");
            }
        } else {
            write_latency("min", static_cast<double>(latency.min()));
            out << ',';
            write_latency("mean", latency.mean());
            out << ',';
            write_latency(
                "p50", static_cast<double>(latency.percentile(50, 100)));
            out << ',';
            write_latency(
                "p95", static_cast<double>(latency.percentile(95, 100)));
            out << ',';
            write_latency(
                "p99", static_cast<double>(latency.percentile(99, 100)));
            out << ',';
            write_latency("max", static_cast<double>(latency.max()));
        }
        out << "}}";
    }

    static void write_mesh(
        std::ostream& out, const char* channel,
        const floo::model::mesh_counter_snapshot& mesh)
    {
        out << "{\"channel\":" << json_escape(channel)
            << ",\"width\":" << mesh.width
            << ",\"height\":" << mesh.height
            << ",\"routers\":[";
        for (std::size_t node = 0; node < mesh.routers.size(); ++node) {
            if (node != 0) out << ',';
            const auto& router = mesh.routers[node];
            out << "{\"x\":" << node % mesh.width
                << ",\"y\":" << node / mesh.width
                << ",\"counted_cycles\":" << router.counted_cycles;
            const auto write_ports = [&](
                const char* field,
                const auto& ports,
                const auto& buffers) {
                out << ',' << json_escape(field) << ":[";
                for (unsigned port = 0;
                     port < floo::model::router_counter_snapshot::num_ports;
                     ++port) {
                    if (port != 0) out << ',';
                    out << "{\"port\":"
                        << json_escape(floo::model::to_string(
                               floo::model::direction_from_port(port)))
                        << ",\"accepted_flits\":"
                        << ports[port].accepted_flits
                        << ",\"accepted_packets\":"
                        << ports[port].accepted_packets
                        << ",\"stall_cycles\":" << ports[port].stall_cycles
                        << ",\"busy_cycles\":" << ports[port].busy_cycles
                        << ",\"occupancy_sum\":"
                        << buffers[port].occupancy_sum
                        << ",\"occupancy_high_water\":"
                        << buffers[port].high_water << '}';
                }
                out << ']';
            };
            write_ports("inputs", router.inputs, router.input_buffers);
            write_ports("outputs", router.outputs, router.output_buffers);
            out << '}';
        }
        out << "]}";
    }

    /// The peripheral floorplan with whatever latency this run can support.
    ///
    /// The hop column needs one reference port, because hops are a property of
    /// a *pair* of nodes. Survey mode issues everything from the probe at
    /// (3,3); firmware traffic is dominated by the CPU at (0,0). Naming the
    /// reference in the file rather than assuming it is what stops the reader
    /// comparing a CPU-side hop count against a probe-side latency — the
    /// mistake the survey's own console note warns about.
    ///
    /// Placement (`x`, `y`, `hops`) is static floorplan arithmetic and is
    /// written as plain integers. Every latency field is `[M]`, and is null
    /// when this run did not measure it: a firmware run leaves the survey
    /// columns null, and a block no software touched leaves the traffic
    /// columns null. Nothing here is estimated to fill a gap.
    void write_peripheral_map(std::ostream& out) const
    {
        const bool firmware = mode == noc_soc_mode::firmware;
        const node reference = firmware ? kCpuNode : kProbeNode;

        out << "\"peripheral_map\":{\"reference\":{\"name\":"
            << (firmware ? "\"cpu\"" : "\"probe\"")
            << ",\"x\":" << reference.x << ",\"y\":" << reference.y
            << "},\"blocks\":[";
        for (unsigned index = 0; index < kBlockCount; ++index) {
            const auto& block = kBlockMap[index];
            const auto& survey = block_survey[index];
            const auto& traffic = block_stats[index];
            if (index != 0) {
                out << ',';
            }
            out << "{\"name\":" << json_escape(block.name)
                << ",\"x\":" << block.where.x
                << ",\"y\":" << block.where.y
                << ",\"hops\":" << manhattan_hops(reference, block.where)
                << ",\"transactions\":";
            write_metric(
                out, static_cast<double>(traffic.transactions()),
                "transactions", "M");
            out << ",\"network_cycles_mean\":";
            if (traffic.transactions() != 0) {
                write_metric(
                    out, traffic.latency().mean(), "cycles", "M",
                    traffic.transactions(),
                    "mean interconnect cycles, target hold-off excluded");
            } else {
                write_null_metric(out, "cycles", "M");
            }
            out << ",\"survey_network_cycles\":";
            if (survey.measured) {
                write_metric(
                    out, static_cast<double>(survey.network_cycles), "cycles",
                    "M", 1, "directed single-register read, network only");
            } else {
                write_null_metric(out, "cycles", "M");
            }
            out << ",\"survey_total_ns\":";
            if (survey.measured) {
                write_metric(
                    out,
                    survey.total / sc_core::sc_time(1, sc_core::SC_NS),
                    "ns", "M", 1,
                    "directed single-register read, network plus the "
                    "peripheral's own access latency");
            } else {
                write_null_metric(out, "ns", "M");
            }
            // Only meaningful alongside a survey measurement: a refusal still
            // travels the network both ways, so its latency is real while its
            // register choice was wrong.
            out << ",\"survey_accepted\":"
                << (survey.measured && !survey.accepted ? "false" : "true")
                << '}';
        }
        out << "]}";
    }

    void write_metrics_json(
        const cdc::components::noc_interconnect::detailed_counters& counters)
    {
        if (metrics_path.empty()) {
            return;
        }
        const std::string temporary = metrics_path + ".tmp";
        std::ofstream out(temporary, std::ios::trunc);
        if (!out) {
            throw std::runtime_error(
                "noc_soc: cannot create metrics file '" + temporary + "'");
        }

        const std::uint64_t modeled_cycles = static_cast<std::uint64_t>(
            sc_core::sc_time_stamp()
            / sc_core::sc_time(1, sc_core::SC_NS));
        const auto request_summary =
            summarise_mesh(counters.request, modeled_cycles);
        const auto response_summary =
            summarise_mesh(counters.response, modeled_cycles);
        const double seconds = sc_core::sc_time_stamp().to_seconds();
        const double throughput = seconds == 0.0
            ? 0.0
            : static_cast<double>(all_stats.transactions()) / seconds / 1e6;
        const double bandwidth = seconds == 0.0
            ? 0.0
            : static_cast<double>(all_stats.payload_bytes()) / seconds / 1e9;
        const bool contention_available =
            cpu_ram_quiet_stats.transactions() != 0
            && cpu_ram_dma_active_stats.transactions() != 0;
        const double contention = contention_available
            ? cpu_ram_dma_active_stats.latency().mean()
                - cpu_ram_quiet_stats.latency().mean()
            : 0.0;

        out << "{\n"
            << "\"schema\":\"floo-noc-metrics-v1\",\n"
            << "\"provenance\":{"
            << "\"generated_utc\":" << json_escape(generated_utc()) << ','
            << "\"git_revision\":null,\"git_dirty\":null,"
            << "\"source_patch_sha256\":null,"
            << "\"platform_binary\":\"unavailable\","
            << "\"platform_sha256\":null,"
            << "\"firmware\":" << json_escape(firmware_path) << ','
            << "\"firmware_sha256\":null,"
            << "\"build_type\":" << json_escape(kBuildType) << ','
            << "\"host_cc\":\"unavailable\","
            << "\"host_cxx\":\"unavailable\","
            << "\"systemc_version\":"
            << json_escape(sc_core::sc_version()) << "},\n"
            << "\"configuration\":{"
            << "\"timing_mode\":\"detailed\","
            << "\"topology\":{\"width\":4,\"height\":4},"
            << "\"routing\":\"XY\","
            << "\"port_order\":[\"North\",\"East\",\"South\",\"West\","
               "\"Eject\"],"
            << "\"clock_period_ns\":1.0,"
            << "\"input_fifo_depth\":2,\"output_fifo_depth\":2,"
            << "\"max_outstanding_per_manager\":"
            << cdc::components::noc_interconnect::
                   default_max_outstanding_per_port
            << ",\"managers\":["
            << "{\"name\":\"cpu\",\"x\":" << kCpuNode.x
            << ",\"y\":" << kCpuNode.y << "},"
            << "{\"name\":\"dma\",\"x\":" << kDmaNode.x
            << ",\"y\":" << kDmaNode.y << "},"
            << "{\"name\":\"probe\",\"x\":" << kProbeNode.x
            << ",\"y\":" << kProbeNode.y << "}]},\n"
            // Survey mode may now write this file too, and it is a synthetic
            // walk of the peripheral map, not a firmware workload. Saying
            // "firmware" there would misdescribe the only traffic in the run.
            << "\"workload\":{\"name\":"
            << (mode == noc_soc_mode::firmware
                    ? "\"FreeRTOS concurrent\",\"kind\":\"firmware\""
                    : "\"synthetic peripheral survey\",\"kind\":\"synthetic\"")
            << ",\"seed\":null},\n"
            << "\"measurement_window\":{"
            << "\"warmup_cycles\":0,\"start_cycle\":0,"
            << "\"end_cycle\":" << modeled_cycles << ','
            << "\"counted_cycles\":" << modeled_cycles
            // Firmware injection was not stopped at this fixed boundary.
            // Instantaneous quiescence is not the D3 stop-and-drain contract.
            << ",\"drained\":false"
            << "},\n"
            << "\"transaction_metrics\":{\"global\":";
        write_transaction_metrics(out, all_stats);
        out << ",\"classes\":[";
        const struct {
            const char* name;
            const transaction_metrics* metrics;
        } classes[] = {
            {"cpu_plic_claim", &plic_claim_stats},
            {"cpu_plic_complete", &plic_complete_stats},
            {"cpu_ram_dma_idle", &cpu_ram_quiet_stats},
            {"cpu_ram_dma_active", &cpu_ram_dma_active_stats},
            {"cpu_other_mmio", &cpu_mmio_stats},
            {"dma_all", &dma_manager_stats},
            {"manager_cpu", &manager_stats[0]},
            {"manager_dma", &manager_stats[1]},
            {"manager_probe", &manager_stats[2]},
            {"target_ram", &target_stats[kTargetRamIndex]},
            {"target_plic", &target_stats[kTargetPlicIndex]},
            {"target_other_mmio", &target_stats[kTargetOtherIndex]},
            {"flow_cpu_ram", &flow_stats[kCpuPortIndex][kTargetRamIndex]},
            {"flow_cpu_plic", &flow_stats[kCpuPortIndex][kTargetPlicIndex]},
            {"flow_cpu_other_mmio",
             &flow_stats[kCpuPortIndex][kTargetOtherIndex]},
            {"flow_dma_ram", &flow_stats[kDmaPortIndex][kTargetRamIndex]},
            {"flow_dma_plic", &flow_stats[kDmaPortIndex][kTargetPlicIndex]},
            {"flow_dma_other_mmio",
             &flow_stats[kDmaPortIndex][kTargetOtherIndex]},
            {"flow_probe_ram", &flow_stats[kProbePortIndex][kTargetRamIndex]},
            {"flow_probe_plic",
             &flow_stats[kProbePortIndex][kTargetPlicIndex]},
            {"flow_probe_other_mmio",
             &flow_stats[kProbePortIndex][kTargetOtherIndex]},
        };
        for (unsigned index = 0; index < sizeof(classes) / sizeof(classes[0]);
             ++index) {
            if (index != 0) out << ',';
            write_transaction_metrics(
                out, *classes[index].metrics, classes[index].name);
        }
        out << "]},\n";
        write_peripheral_map(out);
        out << ",\n\"physical_meshes\":[";
        write_mesh(out, "request", counters.request);
        out << ',';
        write_mesh(out, "response", counters.response);
        out << "],\n\"derived_metrics\":{"
            << "\"transaction_throughput_mtrans_s\":";
        write_metric(
            out, throughput, "Mtrans/s", "D", all_stats.transactions(),
            "completed transactions / measured time");
        out << ",\"payload_bandwidth_gb_s\":";
        write_metric(
            out, bandwidth, "GB/s", "D", all_stats.transactions(),
            "completed payload bytes / measured time");
        out << ",\"request_peak_link_utilisation\":";
        write_metric(
            out, request_summary.peak_link_utilisation * 100.0, "%", "D", 0,
            "accepted flits / counted cycles");
        out << ",\"response_peak_link_utilisation\":";
        write_metric(
            out, response_summary.peak_link_utilisation * 100.0, "%", "D", 0,
            "accepted flits / counted cycles");
        out << ",\"request_peak_stall_ratio\":";
        write_metric(
            out, request_summary.peak_link_stall_ratio * 100.0, "%", "D", 0,
            "stall cycles / busy cycles");
        out << ",\"response_peak_stall_ratio\":";
        write_metric(
            out, response_summary.peak_link_stall_ratio * 100.0, "%", "D", 0,
            "stall cycles / busy cycles");
        out << ",\"ram_contention_delta\":";
        if (contention_available) {
            write_metric(
                out, contention, "cycles", "D",
                cpu_ram_dma_active_stats.transactions(),
                "mean CPU RAM latency with DMA active - idle");
        } else {
            write_null_metric(out, "cycles", "D");
        }
        out << "},\n"
            << "\"availability\":{\"router_counters\":true,"
               "\"area\":false,\"power\":false,\"energy_per_flit\":false},\n"
            << "\"warnings\":["
            << "\"firmware diagnostic starts after reset with zero explicit "
               "warm-up and cannot drain an active CPU; use noc_benchmark for "
               "the D3 selectable measurement contract\","
            << "\"area, power and energy are unavailable pending calibrated "
               "RTL evidence\","
            << "\"binary and firmware hashes are added by the archival runner, "
               "not by this in-process JSON writer\""
            << "]\n}\n";
        out.flush();
        if (!out) {
            std::remove(temporary.c_str());
            throw std::runtime_error(
                "noc_soc: failed while writing metrics file '"
                + temporary + "'");
        }
        out.close();
        if (std::rename(temporary.c_str(), metrics_path.c_str()) != 0) {
            std::remove(temporary.c_str());
            throw std::runtime_error(
                "noc_soc: cannot atomically publish metrics file '"
                + metrics_path + "'");
        }
    }

    void publish_live_metrics()
    {
        if (!baseline_enabled || metrics_path.empty()
            || timing != noc_timing_mode::detailed) {
            throw std::logic_error(
                "live dashboard requires detailed mode and --noc-metrics FILE");
        }
        require_metrics_consistent();
        write_metrics_json(noc->detailed_counter_snapshot());
    }

    void report_baseline()
    {
        require_metrics_consistent();

        std::cout << "\nnoc_soc measurement baseline\n";
        std::cout << "  conditions\n"
                  << "    timing mode:      "
                  << (timing == noc_timing_mode::fast
                          ? "fast (estimated, NOT a measurement)"
                          : "detailed cycle-stepped")
                  << "\n"
                  << "    modeled window:   " << sim_us
                  << " us\n"
                  << "    network clock:    1 ns per cycle\n"
                  << "    floorplan:        4x4 mesh; cpu " << kCpuNode.x << ","
                  << kCpuNode.y << " dma " << kDmaNode.x << "," << kDmaNode.y
                  << " ram " << kRamNode.x << "," << kRamNode.y << " plic "
                  << kBootNode.x << "," << kBootNode.y << "\n"
                  << "    build type:       " << kBuildType << "\n"
                  << "    firmware:         "
                  << (firmware_path.empty() ? "(none)" : firmware_path)
                  << "\n";

        if (timing == noc_timing_mode::fast) {
            std::cout << "  WARNING: fast mode bypasses the cycle-stepped mesh."
                      << " These are no-contention\n"
                      << "           estimates and must not be reported as"
                      << " measured latency or contention.\n";
        }

        std::cout << "  CPU-port network latency, by classified operation\n";
        print_latency("PLIC claim      (read  0x0c200004)", plic_claim_stats);
        print_latency("PLIC complete   (write 0x0c200004)", plic_complete_stats);
        print_latency("RAM, DMA idle                     ", cpu_ram_quiet_stats);
        print_latency("RAM, DMA active                   ",
                      cpu_ram_dma_active_stats);
        print_latency("other CPU MMIO                    ", cpu_mmio_stats);

        if (cpu_ram_quiet_stats.transactions() != 0
            && cpu_ram_dma_active_stats.transactions() != 0) {
            std::cout << "    contention delta (mean RAM, active - idle): "
                      << (cpu_ram_dma_active_stats.latency().mean()
                          - cpu_ram_quiet_stats.latency().mean())
                      << " cycles\n";
        } else {
            std::cout << "    contention delta: not available; one RAM bucket"
                      << " was never exercised\n";
        }

        std::cout << "  DMA manager port\n";
        print_latency("DMA-issued transactions           ", dma_manager_stats);
        if (dma_completion_samples != 0) {
            std::cout << "    channel start to completion interrupt: n="
                      << dma_completion_samples
                      << " min=" << dma_completion_min
                      << " mean="
                      << (dma_completion_total / dma_completion_samples)
                      << " max=" << dma_completion_max << "\n";
        } else {
            std::cout << "    channel start to completion interrupt: not"
                      << " exercised\n";
        }

        std::cout << "  per-manager totals\n";
        static const char* const port_names[] = {"cpu", "dma", "probe"};
        for (unsigned port = 0; port < 3; ++port) {
            std::cout << "    " << port_names[port]
                      << ": peak outstanding "
                      << noc->peak_outstanding_transactions(port) << '\n';
        }
        std::cout << "    all managers: " << noc->completed_transactions()
                  << " completed transactions, " << noc->total_latency_cycles()
                  << " total network cycles\n";

        if (timing != noc_timing_mode::fast) {
            const auto counters = noc->detailed_counter_snapshot();
            std::cout << "  cycle-stepped mesh\n"
                      << "    mesh clocked " << noc->elapsed_cycles()
                      << " cycles\n"
                      << "    clock-gate transitions "
                      << noc->clock_gate_transitions() << '\n'
                      << "    mesh quiescent while wrapper busy "
                      << noc->mesh_quiescent_wrapper_busy_cycles()
                      << " cycles\n";
            std::cout << "  production router counters\n";
            print_mesh_summary("request mesh ", counters.request);
            print_mesh_summary("response mesh", counters.response);
            if (!metrics_path.empty()) {
                write_metrics_json(counters);
                std::cout << "  metrics JSON: " << metrics_path << '\n';
            }
        }
        std::cout << "  These are observations for this fixed window. No"
                  << " threshold is implied.\n";
    }


    /// Latency of the CSR polls issued while the DMA was transferring.
    std::uint64_t busy_polls = 0;
    std::uint64_t busy_cycles_sum = 0;
    std::uint64_t busy_cycles_max = 0;
    std::uint64_t synthetic_transactions = 0;
    std::uint64_t synthetic_ram_writes = 0;
    std::uint64_t synthetic_dma_register_writes = 0;

    SC_HAS_PROCESS(traffic_stub);

    explicit traffic_stub(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , bus_socket("bus_socket")
    {
        SC_THREAD(run);
    }

private:
    static unsigned hops(node from, node to)
    {
        return manhattan_hops(from, to);
    }

    void record_synthetic_access(bool write, std::uint64_t address)
    {
        ++synthetic_transactions;
        if (!write) {
            return;
        }
        if (address >= kRamBase && address < kRamBase + kRamSize) {
            ++synthetic_ram_writes;
        }
        if (address >= kDma0 && address < kDma0 + kMmio) {
            ++synthetic_dma_register_writes;
        }
    }

    sc_core::sc_time access(
        bool write, std::uint64_t address, unsigned char* bytes,
        unsigned length, bool expect_ok = true)
    {
        record_synthetic_access(write, address);
        tlm::tlm_generic_payload trans;
        trans.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(bytes);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        const auto before = sc_core::sc_time_stamp();
        bus_socket->b_transport(trans, delay);
        // Detailed mode has already spent the access time and returns zero.
        // Fast mode returns the same modeled cost as an annotation; this
        // blocking survey thread synchronizes it so both reports use elapsed
        // simulated time. Temporally decoupled CPU traffic keeps the annotation
        // in its quantum keeper instead.
        if (delay > sc_core::SC_ZERO_TIME) {
            wait(delay);
        }
        if (expect_ok && !trans.is_response_ok()) {
            throw std::runtime_error("noc_soc: access failed at address "
                                     + std::to_string(address));
        }
        return sc_core::sc_time_stamp() - before;
    }

    /// ── DMA: the experiment this platform exists for ────────────────────
    ///
    /// Programs a memory-to-memory transfer exactly as `fw/dma_riscv` does, so
    /// the DMA becomes a second bus master pulling data across the mesh while
    /// the CPU is fetching. On a flat bus the two would not interact; here they
    /// share links and the cost shows up in the latency figures.
    ///
    /// Survey mode lets the probe port drive this experiment. Firmware mode
    /// returns before any probe access, so the CPU owns DMA0 exclusively.

    void write32(std::uint64_t address, std::uint32_t value)
    {
        access(true, address, reinterpret_cast<unsigned char*>(&value),
               sizeof(value));
    }

    std::uint32_t read32(std::uint64_t address)
    {
        std::uint32_t value = 0;
        access(false, address, reinterpret_cast<unsigned char*>(&value),
               sizeof(value));
        return value;
    }

    void write8(std::uint64_t address, std::uint8_t value)
    {
        access(true, address, &value, sizeof(value));
    }

    std::uint8_t read8(std::uint64_t address)
    {
        std::uint8_t value = 0;
        access(false, address, &value, sizeof(value));
        return value;
    }

    /// `DMAMOV reg, imm`: opcode 0xBC, register in bits [7:3] of byte 1, then a
    /// little-endian 32-bit immediate.
    std::uint64_t emit_dmamov(
        std::uint64_t pc, std::uint8_t reg, std::uint32_t imm)
    {
        write8(pc + 0, 0xBC);
        write8(pc + 1, static_cast<std::uint8_t>(reg << 3));
        for (unsigned byte = 0; byte < 4; ++byte) {
            write8(pc + 2 + byte,
                   static_cast<std::uint8_t>((imm >> (8 * byte)) & 0xFF));
        }
        return pc + 6;
    }

    /// Returns true if all `kDmaCopyLen` bytes arrived intact.
    bool run_dma_transfer()
    {
        for (unsigned index = 0; index < kDmaCopyLen; ++index) {
            write8(kDmaSrc + index, static_cast<std::uint8_t>(0x40 + index));
            write8(kDmaDst + index, 0);
        }

        // CCR: source and destination incrementing, 4-byte beats, 4-beat
        // bursts, both directions.
        const std::uint32_t ccr = (1u << 0) | (2u << 1) | (3u << 4)
                                | (1u << 14) | (2u << 15) | (3u << 18);
        std::uint64_t pc = kDmaProgram;
        pc = emit_dmamov(pc, 1, ccr);                                  // CCR
        pc = emit_dmamov(pc, 0, static_cast<std::uint32_t>(kDmaSrc));  // SAR
        pc = emit_dmamov(pc, 2, static_cast<std::uint32_t>(kDmaDst));  // DAR
        for (unsigned burst = 0; burst < kDmaCopyLen / kDmaBurstBytes; ++burst) {
            write8(pc++, 0x04);   // DMALD, one 16-byte burst
            write8(pc++, 0x08);   // DMAST
        }
        write8(pc++, 0x34);   // DMASEV
        write8(pc++, static_cast<std::uint8_t>(kDmaEventDone << 3));
        write8(pc++, 0x00);   // DMAEND

        write32(kDma0 + 0x020, 1u << kDmaEventDone);   // INTEN

        // DBGINST0 byte 2 = 0xA0 (DMAGO, secure), byte 3 = channel 0.
        write32(kDma0 + 0xD08, 0xA0u << 16);
        write32(kDma0 + 0xD0C, static_cast<std::uint32_t>(kDmaProgram));
        write32(kDma0 + 0xD04, 0);                     // dispatch

        // Poll CSR0 until the channel stops. Every poll is a real MMIO read
        // over the mesh, issued *while* the DMA is moving data, so these are
        // the accesses that actually see contention. Measuring after the
        // transfer, as an earlier version did, shows nothing.
        unsigned guard = 4000;
        busy_polls = 0;
        busy_cycles_sum = 0;
        busy_cycles_max = 0;
        while ((read32(kDma0 + 0x100) & 0xFu) != 0u && guard-- != 0) {
            // The measured access is a RAM read down the column the DMA is
            // using; the CSR poll above only decides when to stop.
            read32(kDmaProbeAddr);
            if (noc != nullptr) {
                const auto spent = noc->last_latency_cycles();
                ++busy_polls;
                busy_cycles_sum += spent;
                busy_cycles_max = std::max(busy_cycles_max, spent);
            }
        }
        if (guard == 0) {
            std::cout << "  DMA channel did not stop before the guard expired\n";
            return false;
        }

        for (unsigned index = 0; index < kDmaCopyLen; ++index) {
            if (read8(kDmaSrc + index) != read8(kDmaDst + index)) {
                std::cout << "  DMA byte " << index << " did not match\n";
                return false;
            }
        }
        return true;
    }

    /// A register read, which is what firmware does to a control block.
    ///
    /// The width is per peripheral because the IPs disagree: most want exactly
    /// four bytes, `spi_tlm` rejects anything wider than two. The NoC carries
    /// whichever `AxSIZE` the access needs, so a narrow read stays narrow all
    /// the way to the target.
    sc_core::sc_time probe(
        std::uint64_t base, unsigned width, bool& accepted)
    {
        record_synthetic_access(false, base);
        std::uint64_t value = 0;
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(base);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(width);
        trans.set_streaming_width(width);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        const auto before = sc_core::sc_time_stamp();
        bus_socket->b_transport(trans, delay);
        if (delay > sc_core::SC_ZERO_TIME) {
            wait(delay);
        }
        accepted = trans.is_response_ok();
        return sc_core::sc_time_stamp() - before;
    }

    void sample_cpu_window()
    {
        const double window = sim_us > 0.0
            ? sim_us
            : (mode == noc_soc_mode::firmware ? 500.0 : 5.0);
        const auto step = sc_core::sc_time(window / 200.0, sc_core::SC_US);
        std::uint64_t previous = cpu != nullptr ? cpu->get_instret() : 0;
        for (unsigned sample = 0; sample < 200; ++sample) {
            wait(step);
            if (cpu == nullptr) {
                continue;
            }
            const auto now = cpu->get_instret();
            if (now != previous) {
                previous = now;
                cpu_active_until = sc_core::sc_time_stamp();
            }
        }
    }

    void report_synthetic_ownership() const
    {
        std::cout << "\nnoc_soc synthetic traffic:"
                  << "\n  transactions " << synthetic_transactions
                  << "\n  RAM writes " << synthetic_ram_writes
                  << "\n  DMA register writes "
                  << synthetic_dma_register_writes << '\n';
    }

    void report_cpu() const
    {
        if (cpu == nullptr) {
            return;
        }
        const auto retired = cpu->get_instret();
        const auto pc = cpu->get_pc();
        // Charge only the window in which the CPU was actually retiring.
        const auto elapsed = cpu_active_until > sc_core::SC_ZERO_TIME
            ? cpu_active_until
            : sc_core::sc_time_stamp();
        const bool idled = cpu_active_until > sc_core::SC_ZERO_TIME
            && cpu_active_until < sc_core::sc_time_stamp();
        const bool in_ram = pc >= kRamBase && pc < kRamBase + kRamSize;
        const bool in_rom = pc < kBootromBase + kBootromSize;
        const char* region = in_ram ? "RAM" : (in_rom ? "BOOTROM0" : nullptr);

        std::cout << "\n  CPU pc 0x" << std::hex << pc << std::dec
                  << ", retired " << retired << " instructions in "
                  << elapsed << '\n';
        if (region != nullptr && retired != 0) {
            std::cout << "  " << elapsed.to_seconds() * 1e9
                          / static_cast<double>(retired)
                      << " ns per instruction, fetching from " << region;
            if (noc != nullptr
                && noc->selected_timing_mode()
                    == cdc::components::noc_interconnect::timing_mode::fast) {
                std::cout << " through the fast NoC estimate\n";
            } else {
                std::cout << " over the cycle-stepped mesh\n";
            }
            if (idled) {
                std::cout << "  (measured over the " << elapsed
                          << " the CPU was retiring; it then idled until "
                          << sc_core::sc_time_stamp() << ")\n";
            }
            if (!in_ram) {
                // Without firmware the Bremen ISS traps to `mtvec = 0` at
                // startup and then executes the backdoor-loaded boot-ROM loop.
                std::cout << "  (a trap loop, not firmware; use firmware mode "
                             "for a real workload.)\n";
            }
        } else {
            std::cout << "  the CPU is not executing mapped instructions\n";
        }
    }

    void run_firmware_mode()
    {
        std::cout << "\nnoc_soc: firmware mode; synthetic survey disabled\n";
        sample_cpu_window();
        report_synthetic_ownership();
        if (noc != nullptr) {
            const auto count = noc->completed_transactions();
            std::cout << "\n  firmware traffic: " << count
                      << " completed transactions, "
                      << noc->total_latency_cycles();
            if (noc->selected_timing_mode()
                == cdc::components::noc_interconnect::timing_mode::fast) {
                std::cout << " estimated no-contention network cycles\n"
                          << "  cycle-stepped mesh bypassed in fast mode\n";
            } else {
                std::cout << " measured network cycles\n"
                          << "  mesh clocked " << noc->elapsed_cycles()
                          << " cycles; idle gating requires wrapper and mesh "
                             "quiescence\n";
            }
        }
        report_cpu();
        if (baseline_enabled) {
            report_baseline();
        }
        sc_core::sc_stop();
    }

    void run()
    {
        if (mode == noc_soc_mode::firmware) {
            run_firmware_mode();
            return;
        }

        std::cout << "\nnoc_soc: survey mode; synthetic probe owns RAM scratch "
                     "and DMA0\n";
        std::vector<unsigned char> buffer(64, 0);

        // Warm-up, not measured: the mesh holds its reset for a few cycles
        // after time zero and the first access would be charged for it.
        access(true, kSurveyScratch + 0x800, buffer.data(),
               sizeof(std::uint64_t));

        // ── RAM correctness, one beat and a burst ───────────────────────────
        std::uint64_t pattern = 0xCAFE'BABE'DEAD'BEEFull;
        std::memcpy(buffer.data(), &pattern, sizeof(pattern));
        const auto ram_write =
            access(true, kSurveyScratch, buffer.data(), sizeof(pattern));
        std::memset(buffer.data(), 0, buffer.size());
        const auto ram_read =
            access(false, kSurveyScratch, buffer.data(), sizeof(pattern));
        // RAM's row in the reported block map. It is the one memory-like block
        // the survey already reads, so it costs nothing extra to record. The
        // boot ROM is not read at all here, and CLINT and PLIC are deliberately
        // never probed: reading the PLIC claim register *claims* an interrupt,
        // so a latency probe there would change the machine it is measuring.
        {
            const unsigned block = block_index(kSurveyScratch);
            if (block < kBlockCount) {
                block_survey[block] = block_survey_result{
                    true, true,
                    noc != nullptr ? noc->last_latency_cycles() : 0,
                    ram_read};
            }
        }
        std::uint64_t read_back = 0;
        std::memcpy(&read_back, buffer.data(), sizeof(read_back));
        if (read_back != pattern) {
            throw std::runtime_error("noc_soc: RAM read-back mismatch");
        }

        std::uint64_t burst[4] = {1, 2, 3, 4};
        std::memcpy(buffer.data(), burst, sizeof(burst));
        const auto burst_write =
            access(true, kSurveyScratch + 0x100, buffer.data(), sizeof(burst));
        std::memset(buffer.data(), 0, buffer.size());
        const auto burst_read =
            access(false, kSurveyScratch + 0x100, buffer.data(), sizeof(burst));
        std::uint64_t burst_back[4] = {};
        std::memcpy(burst_back, buffer.data(), sizeof(burst_back));
        for (unsigned beat = 0; beat < 4; ++beat) {
            if (burst_back[beat] != burst[beat]) {
                throw std::runtime_error("noc_soc: burst beat mismatch");
            }
        }

        // ── Every peripheral, one register read each ────────────────────────
        // The register offset and access width are per IP, because the models
        // disagree: `spi_tlm` rejects anything wider than two bytes, and
        // `trng_tlm`'s first valid register is at `0x100`, not zero. The NoC
        // carries whichever `AxSIZE` an access needs, so a narrow read stays
        // narrow all the way to the target.
        const struct {
            const char* name; std::uint64_t base; std::uint64_t offset;
            node where; unsigned width;
        } map[] = {
            {"uart0",  kUart0,  0x000, kUart0Node, 4},
            {"i2c0",   kI2c0,   0x000, kSerial0,   4},
            {"spi0",   kSpi0,   0x000, kSerial0,   2},
            {"timer0", kTimer0, 0x000, kTimer0N,   4},
            {"wdt0",   kWdt0,   0x000, kCtrlNode,  4},
            {"pwm0",   kPwm0,   0x000, kCtrlNode,  4},
            {"dma0",   kDma0,   0x000, kDmaRegs,   4},
            {"trng0",  kTrng0,  0x100, kTrngNode,  4},
            {"cmu0",   kCmu0,   0x000, kClkNode,   4},
            {"dmic0",  kDmic0,  0x000, kClkNode,   4},
            {"otp0",   kOtp0,   0x000, kBootNode,  4},
            {"qspi0",  kQspi0,  0x000, kBootNode,  4},
            {"uart1",  kUart1,  0x000, kUart1Node, 4},
            {"i2c1",   kI2c1,   0x000, kSerial1,   4},
            {"spi1",   kSpi1,   0x000, kSerial1,   2},
            {"timer1", kTimer1, 0x000, kTimer1N,   4},
            {"rtc0",   kRtc0,   0x000, kIoNode,    4},
            {"adc0",   kAdc0,   0x000, kIoNode,    4},
            {"gpio0",  kGpio0,  0x000, kIoNode,    4},
        };

        std::vector<probe_result> results;
        results.reserve(sizeof(map) / sizeof(map[0]));
        for (const auto& entry : map) {
            bool accepted = true;
            const auto latency =
                probe(entry.base + entry.offset, entry.width, accepted);
            const std::uint64_t network =
                noc != nullptr ? noc->last_latency_cycles() : 0;
            results.push_back(probe_result{
                entry.name, entry.where, hops(kProbeNode, entry.where),
                latency, network, accepted});

            // Same numbers into the reported block map. `last_latency_cycles()`
            // is safe to read here and only here: the survey is the only
            // issuer in this phase, so the most recent completion is this
            // probe's. Firmware mode has concurrent managers and must use the
            // observer instead.
            const unsigned block = block_index(entry.base + entry.offset);
            if (block < kBlockCount) {
                block_survey[block] = block_survey_result{
                    true, accepted, network, latency};
            }
        }

        // ── An unmapped address must be reported, not routed ────────────────
        std::uint32_t sink = 0;
        record_synthetic_access(false, 0x100D'0000);
        tlm::tlm_generic_payload bad;
        bad.set_command(tlm::TLM_READ_COMMAND);
        bad.set_address(0x100D'0000);  // ISP0, reserved and not bound here
        bad.set_data_ptr(reinterpret_cast<unsigned char*>(&sink));
        bad.set_data_length(sizeof(sink));
        bad.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        bus_socket->b_transport(bad, delay);
        if (bad.get_response_status() != tlm::TLM_ADDRESS_ERROR_RESPONSE) {
            throw std::runtime_error(
                "noc_soc: an unmapped address was not reported");
        }

        // ── DMA: a second bus master pulling data across the mesh ──────────
        //
        // Measure the same peripheral before and during the transfer. The
        // difference is contention: the DMA's reads and writes share links with
        // this port's traffic and with the CPU's instruction fetches. A flat
        // bus cannot produce this number because it has no links to share.
        // Two things have to be right for this to measure contention rather
        // than something else.
        //
        // The baseline must use the *same* target as the busy samples, or the
        // comparison is just hop count. An earlier version baselined against
        // uart1 at one hop and sampled dma0 at four, and the 12-cycle
        // "contention" it reported was pure distance.
        //
        // And the probed path must actually share links with the DMA. Under XY
        // routing the DMA at (0,3) reaching RAM at (0,1) runs down the x=0
        // column; this port at (3,3) reaching RAM crosses row y=3 and then runs
        // down the same column. Probing dma0 at (1,1) instead shares no link
        // with the DMA at all, and duly reported zero.
        bool quiet_ok = true;
        const auto quiet = probe(kDmaProbeAddr, 4, quiet_ok);
        const auto quiet_cycles =
            noc != nullptr ? noc->last_latency_cycles() : 0;

        const bool dma_ok = run_dma_transfer();

        std::cout << "\nnoc_soc: DMA memory-to-memory transfer, "
                  << kDmaCopyLen << " bytes, RAM to RAM\n"
                  << "  result   " << (dma_ok ? "all bytes match" : "FAILED")
                  << '\n'
                  << "  RAM read down the DMA's column, before it starts: " << quiet_cycles
                  << " network cycles (" << quiet << ")\n";
        if (busy_polls != 0) {
            const double mean = static_cast<double>(busy_cycles_sum)
                / static_cast<double>(busy_polls);
            // Signed: the "before" sample is not guaranteed to be the cheaper
            // one, and subtracting unsigned cycle counts printed a 20-digit
            // number the first time the CPU happened to be busier beforehand.
            const double mean_delta = mean - static_cast<double>(quiet_cycles);
            const long long worst_delta =
                static_cast<long long>(busy_cycles_max)
                - static_cast<long long>(quiet_cycles);
            std::cout << "  the same read while the DMA is transferring: "
                      << busy_polls << " samples, mean " << mean
                      << ", worst " << busy_cycles_max << " cycles\n"
                      << "  difference " << mean_delta << " cycles on average, "
                      << worst_delta << " at worst\n";
            if (busy_cycles_max <= quiet_cycles) {
                // Not a broken measurement, but be precise about the cause.
                //
                // Step 10.5 permits bounded concurrent calls on one port, but
                // this survey's probe, CPU and DMA each issue blocking calls
                // from one process. It therefore does not saturate that new
                // capacity. MaxUniqueIds = 1 still constrains downstream
                // response ordering, not the number of wrapper calls admitted.
                std::cout << "  no contention at this load, and it is not a "
                             "measurement artefact.\n"
                             "  Cause: this workload has one blocking issuer "
                             "per upstream port, so it does not\n"
                             "  saturate Step 10.5's bounded same-port "
                             "concurrency. MaxUniqueIds = 1\n"
                             "  constrains downstream response order, not "
                             "outstanding count. This result\n"
                             "  is not proof that a 4x4 FlooNoC cannot "
                             "congest.\n";
            }
        }
        if (!dma_ok) {
            throw std::runtime_error(
                "noc_soc: the DMA transfer did not verify");
        }

        // Sample the retire count as the window passes, and remember when it
        // last advanced. Firmware ends in `wfi`, so it stops retiring long
        // before the window closes; dividing the total instructions by the
        // whole window would make the per-instruction cost look worse the
        // longer you run. It did exactly that: the same firmware reported
        // 29.7 ns at --sim-us 200, 93.3 at 1000 and 279.8 at 3000.
        sample_cpu_window();

        // ── Report ─────────────────────────────────────────────────────────
        std::cout << "\nnoc_soc: RAM at " << kRamNode.x << ',' << kRamNode.y
                  << " (" << hops(kProbeNode, kRamNode)
                  << " hops from the probe port)\n"
                  << "  write 1 beat   " << ram_write << '\n'
                  << "  read  1 beat   " << ram_read << '\n'
                  << "  write 4 beats  " << burst_write << '\n'
                  << "  read  4 beats  " << burst_read << '\n';

        std::cout << "\nnoc_soc: one 32-bit register read per peripheral\n"
                  << "  hops are from the probe port at (3,3), not the CPU;\n"
                     "  the network column is the interconnect alone, total\n"
                     "  also includes the peripheral's own access latency\n"
                  << "  " << std::left << std::setw(8) << "block"
                  << std::setw(8) << "node" << std::setw(6) << "hops"
                  << std::setw(10) << "network" << "total\n";
        for (const auto& result : results) {
            std::cout << "  " << std::left << std::setw(8) << result.name
                      << std::setw(8)
                      << ('(' + std::to_string(result.where.x) + ','
                          + std::to_string(result.where.y) + ')')
                      << std::setw(6) << result.hops
                      << std::setw(10)
                      << (std::to_string(result.network_cycles) + " cyc")
                      << result.latency
                      << (result.accepted ? "" : "   (IP refused the register,"
                                                 " latency still measured)")
                      << '\n';
        }

        report_synthetic_ownership();
        if (noc != nullptr) {
            const auto count = noc->completed_transactions();
            std::cout << "\n  " << count << " transactions, "
                      << noc->total_latency_cycles()
                      << (noc->selected_timing_mode()
                                  == cdc::components::noc_interconnect::
                                      timing_mode::fast
                              ? " estimated no-contention network cycles"
                              : " measured network cycles");
            if (count != 0) {
                std::cout << ", mean "
                          << static_cast<double>(noc->total_latency_cycles())
                              / static_cast<double>(count) << " cycles";
            }
            if (noc->selected_timing_mode()
                == cdc::components::noc_interconnect::timing_mode::fast) {
                std::cout << "\n  cycle-stepped mesh bypassed in fast mode\n";
            } else {
                std::cout << "\n  mesh clocked " << noc->elapsed_cycles()
                          << " cycles; idle gating requires wrapper and mesh "
                             "quiescence\n";
            }
        }

        report_cpu();

        // Compared on the network column, not the total: the total also carries
        // each peripheral's own access latency, which has nothing to do with
        // the floorplan.
        const auto& nearest = results[12];   // uart1 (3,2), 1 hop from (3,3)
        const auto& farthest = results[0];   // uart0 (1,0), 5 hops
        if (farthest.network_cycles <= nearest.network_cycles) {
            throw std::runtime_error(
                "noc_soc: a five-hop peripheral did not cost the network more "
                "than a one-hop one");
        }

        // The survey is the only run that can measure a peripheral's own
        // access latency, because it is the only one allowed to issue a
        // directed read at every block. Publishing the same JSON the firmware
        // path publishes is what lets one dashboard render both.
        if (!metrics_path.empty() && timing != noc_timing_mode::fast) {
            require_metrics_consistent();
            write_metrics_json(noc->detailed_counter_snapshot());
            std::cout << "\n  metrics JSON: " << metrics_path << '\n';
        }

        sc_core::sc_stop();
    }
};

} // namespace

struct noc_soc_top::impl : public sc_core::sc_module {
    SC_HAS_PROCESS(impl);

    cpu_backend_t cpu;
    traffic_stub probe;
    cdc::components::noc_interconnect noc;
    cdc::components::memory_tlm ram;
    cdc::components::memory_tlm bootrom;
    cdc::components::clint_tlm clint;
    cdc::components::plic_tlm plic;

    UartTLM uart0, uart1;
    cdc::components::uart_host_bridge uart0_host;
    i2c i2c0, i2c1;
    cdc::components::spi_tlm spi0, spi1;
    spi_dummy spi0_peri, spi1_peri;
    cdc::components::Timer timer0, timer1;
    cdc::components::wdt_tlm wdt0;
    PWM pwm0;
    cdc::components::dma_tlm dma0;
    cdc::components::trng_tlm trng0;
    Clkmgr cmu0;
    cdc::components::DmicTLM dmic0;
    pdm_dummy pdm_src;
    otp otp0;
    cdc::components::qspi_tlm qspi0;
    cdc::components::flash_nor_tlm flash0;
    cdc::components::rtc_tlm rtc0;
    cdc::components::adc_tlm adc0;
    cdc::components::gpio_tlm gpio0;

    // ── Signals ────────────────────────────────────────────────────────────
    sc_core::sc_buffer<unsigned char> uart0_tx, uart1_tx;
    sc_core::sc_signal<bool> uart0_irq, uart1_irq;
    sc_core::sc_signal<bool> i2c0_irq, i2c1_irq;
    sc_core::sc_signal<bool> spi0_irq, spi1_irq, spi0_rst, spi1_rst;
    sc_core::sc_signal<bool> timer0_irq, timer1_irq;
    sc_core::sc_signal<bool> timer0_rst, timer1_rst, timer0_ext, timer1_ext;
    sc_core::sc_signal<bool> wdt0_irq, wdt0_rst, wdt0_rsto;
    sc_core::sc_signal<bool> pwm0_out;
    sc_core::sc_signal<std::uint32_t> dma0_irq;
    sc_core::sc_signal<bool> dma0_irq_abort, dma0_rst;
    sc_core::sc_signal<bool> trng0_irq, trng0_rst;
    /// `trng_tlm` requires a clock input but never reads it. A real `sc_clock`
    /// would cost an edge pair per period for nothing, so a static level is
    /// bound instead — the same choice VP_FX1_Full_SoC makes.
    sc_core::sc_signal<bool> trng0_clk;
    sc_core::sc_signal<bool> dmic0_irq, dmic0_rst;
    sc_core::sc_signal<bool> otp0_irq;
    sc_core::sc_signal<bool> qspi0_irq, qspi0_rst;
    sc_core::sc_signal<bool> rtc0_irq, rtc0_rst;
    sc_core::sc_signal<bool> adc0_irq, adc0_rst;
    sc_core::sc_vector<sc_core::sc_signal<bool>> cmu_idle;
    sc_core::sc_signal<bool> cmu_byp_req, cmu_byp_ack;
    /// DMA reports a vector; the PLIC wants a level.
    sc_core::sc_signal<bool> dma0_irq_nonzero;
    /// Reserved PLIC sources.
    sc_core::sc_signal<bool> tie_low;
    noc_soc_mode execution_mode;
    noc_timing_mode interconnect_timing;
    std::string firmware_path;
    std::string uart0_control_candidate;

    impl(sc_core::sc_module_name name, const std::string& config_path,
         noc_soc_mode mode, noc_timing_mode timing, std::string firmware,
         double sim_microseconds, bool measure_baseline,
         std::string metrics_output)
        : sc_core::sc_module(name)
        , cpu("cpu")
        , probe("probe")
        // Three AXI managers: the CPU, the DMA, and the latency survey.
        , noc("noc", 4, 4, kTargetCount, /*num_initiators=*/3,
              sc_core::sc_time(1, sc_core::SC_NS),
              cdc::components::noc_interconnect::
                  default_max_outstanding_per_port,
              timing == noc_timing_mode::fast
                  ? cdc::components::noc_interconnect::timing_mode::fast
                  : cdc::components::noc_interconnect::timing_mode::detailed)
        , ram("ram", kRamSize)
        , bootrom("bootrom", kBootromSize, /*read_only=*/true)
        , clint("clint", cpu)
        , plic("plic", cpu, kNumPlic)
        , uart0("uart0"), uart1("uart1")
        , uart0_host("uart0_host")
        , i2c0("i2c0"), i2c1("i2c1")
        , spi0("spi0"), spi1("spi1")
        , spi0_peri("spi0_peri"), spi1_peri("spi1_peri")
        , timer0("timer0", 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4,
                 sc_core::sc_time(10, sc_core::SC_NS))
        , timer1("timer1", 1, 2, 3, 4, 5, 6, 7, 8, 1, 2, 3, 4,
                 sc_core::sc_time(10, sc_core::SC_NS))
        , wdt0("wdt0", sc_core::sc_time(10, sc_core::SC_NS))
        , pwm0("pwm0")
        , dma0("dma0")
        , trng0("trng0")
        , cmu0("cmu0")
        , dmic0("dmic0")
        , pdm_src("pdm_src")
        , otp0("otp0")
        , qspi0("qspi0")
        , flash0("flash0", kFlashSize)
        , rtc0("rtc0")
        , adc0("adc0")
        , gpio0("gpio0")
        , uart0_tx("uart0_tx"), uart1_tx("uart1_tx")
        , trng0_clk("trng0_clk")
        , cmu_idle("cmu_idle", 4)
        , execution_mode(mode)
        , interconnect_timing(timing)
        , firmware_path(std::move(firmware))
    {
        // ── Upstream ports and their placement ──────────────────────────────
        //
        // Two AXI managers sit on this mesh: the CPU port at the origin and the
        // DMA, which is a real manager and gets its own node rather than being
        // tunnelled through the CPU's. On a flat bus that choice is invisible;
        // here it decides how far the DMA's traffic has to travel and whose
        // links it shares.
        noc.place_initiator(0, kCpuNode);
        noc.place_initiator(1, kDmaNode);
        noc.place_initiator(2, kProbeNode);
        cpu.data_bus().bind(noc.target_socket);   // unified instruction+data
        dma0.master_socket.bind(noc.cpu_port(1));
        probe.bus_socket.bind(noc.cpu_port(2));
        probe.noc = &noc;
        probe.cpu = &cpu;
        probe.mode = execution_mode;
        probe.sim_us = sim_microseconds;

        probe.baseline_enabled = measure_baseline;
        probe.timing = timing;
        probe.firmware_path = firmware_path;
        probe.metrics_path = std::move(metrics_output);
        if (measure_baseline) {
            // Passive by contract: the observer only classifies and
            // accumulates. It must not wait, must not throw, and calls nothing
            // on the interconnect but a const accessor.
            noc.set_completion_observer(
                [this](
                    const cdc::components::noc_interconnect::completion& done) {
                    probe.observe_completion(done);
                });
            dma0.set_channel_start_observer(
                [this](unsigned channel) {
                    probe.note_dma_channel_start(channel);
                });
            SC_METHOD(forward_dma_completion_irq);
            sensitive << dma0_irq_nonzero;
            dont_initialize();
        }

        // ── Downstream map. The node is the new argument versus `bus_router`.
        noc.add_target(kRamBase, kRamSize, kRamNode,
                       cdc::components::noc_interconnect::target_kind::memory)
        .bind(ram.socket);
        // RAM and the boot ROM are the only memory-like targets: they hold
        // instructions, and a compressed RISC-V fetch is a 4-byte read at a
        // 2-byte boundary, which AXI widens to a whole beat. Every MMIO target
        // keeps the default `mmio` kind, so a widened read to one is refused
        // rather than silently touching a neighbouring register.
        noc.add_target(kBootromBase, kBootromSize, kBootNode,
                       cdc::components::noc_interconnect::target_kind::memory)
            .bind(bootrom.socket);
        noc.add_target(kClintBase, kClintSize, kUart0Node).bind(clint.socket);
        noc.add_target(kPlicBase, kPlicSize, kBootNode).bind(plic.socket);
        noc.add_target(kUart0, kMmio, kUart0Node).bind(uart0.bus);
        noc.add_target(kI2c0,  kMmio, kSerial0).bind(i2c0.socket);
        noc.add_target(kSpi0,  kMmio, kSerial0).bind(spi0.from_apb_socket);
        noc.add_target(kTimer0,kMmio, kTimer0N).bind(timer0.socket);
        noc.add_target(kWdt0,  kMmio, kCtrlNode).bind(wdt0.target_socket);
        noc.add_target(kPwm0,  kMmio, kCtrlNode).bind(pwm0.socket);
        noc.add_target(kDma0,  kMmio, kDmaRegs).bind(dma0.target_socket);
        noc.add_target(kTrng0, kMmio, kTrngNode).bind(trng0.socket);
        noc.add_target(kCmu0,  kMmio, kClkNode).bind(cmu0.socket);
        noc.add_target(kDmic0, kMmio, kClkNode).bind(dmic0.bus_target_socket);
        noc.add_target(kOtp0,  kMmio, kBootNode).bind(otp0.socket);
        noc.add_target(kQspi0, kMmio, kBootNode).bind(qspi0.from_apb_socket);
        noc.add_target(kUart1, kMmio, kUart1Node).bind(uart1.bus);
        noc.add_target(kI2c1,  kMmio, kSerial1).bind(i2c1.socket);
        noc.add_target(kSpi1,  kMmio, kSerial1).bind(spi1.from_apb_socket);
        noc.add_target(kTimer1,kMmio, kTimer1N).bind(timer1.socket);
        noc.add_target(kRtc0,  kMmio, kIoNode).bind(rtc0.socket);
        noc.add_target(kAdc0,  kMmio, kIoNode).bind(adc0.socket);
        noc.add_target(kGpio0, kMmio, kIoNode).bind(gpio0.socket);

        // ── Per-IP secondary sockets and signal outputs ─────────────────────
        uart0.tx(uart0_tx); uart0.irq(uart0_irq);
        uart1.tx(uart1_tx); uart1.irq(uart1_irq);
        SC_METHOD(monitor_uart0); sensitive << uart0_tx; dont_initialize();
        // Pin-side host input. It idles unless the CLI selects file replay or
        // TCP; injected bytes still traverse UART0, PLIC source 1 and CPU MMIO
        // over FlooNoC rather than entering firmware through a backdoor.
        uart0_host.rx_out(uart0.rx);
        uart0_host.tx_in(uart0_tx);

        i2c0.irq(i2c0_irq); i2c1.irq(i2c1_irq);

        spi0.to_peri_socket.bind(spi0_peri.socket);
        spi0.irq(spi0_irq); spi0.reset_n(spi0_rst);
        spi1.to_peri_socket.bind(spi1_peri.socket);
        spi1.irq(spi1_irq); spi1.reset_n(spi1_rst);

        timer0.irq_out(timer0_irq); timer0.reset_n(timer0_rst);
        timer0.extin(timer0_ext);
        timer1.irq_out(timer1_irq); timer1.reset_n(timer1_rst);
        timer1.extin(timer1_ext);

        wdt0.irq(wdt0_irq); wdt0.reset_n(wdt0_rst); wdt0.reset_o(wdt0_rsto);
        pwm0.pwm_out(pwm0_out);

        dma0.reset_n(dma0_rst); dma0.irq(dma0_irq);
        dma0.irq_abort(dma0_irq_abort);

        trng0.reset_n(trng0_rst); trng0.irq_out(trng0_irq);
        trng0.clk(trng0_clk);

        for (int index = 0; index < 4; ++index) cmu0.idle_i[index](cmu_idle[index]);
        cmu0.io_clk_byp_req_o(cmu_byp_req);
        cmu0.io_clk_byp_ack_i(cmu_byp_ack);

        dmic0.reset_n(dmic0_rst); dmic0.irq_out(dmic0_irq);
        pdm_src.socket.bind(dmic0.pdm_target_socket);

        otp0.irq_out(otp0_irq);

        qspi0.to_flash_socket.bind(flash0.from_qspi_socket);
        qspi0.irq(qspi0_irq); qspi0.reset_n(qspi0_rst);

        rtc0.irq_out(rtc0_irq); rtc0.reset_n(rtc0_rst);
        adc0.irq_out(adc0_irq); adc0.reset_n(adc0_rst);

        // ── PLIC sources, id = index + 1, same assignment as VP_FX1 ─────────
        SC_METHOD(dma_irq_bridge); sensitive << dma0_irq; dont_initialize();
        SC_THREAD(pc_watch);
        SC_METHOD(irq_watch);
        sensitive << uart0_irq << uart1_irq << i2c0_irq << i2c1_irq << spi0_irq
                  << spi1_irq << timer0_irq << timer1_irq << wdt0_irq
                  << trng0_irq << dmic0_irq << otp0_irq << qspi0_irq
                  << rtc0_irq << adc0_irq << dma0_irq_nonzero
                  << dma0_irq_abort;
        dont_initialize();
        plic.irq_in[0](uart0_irq);          // 1  UART0
        plic.irq_in[1](i2c0_irq);           // 2  I2C0
        plic.irq_in[2](spi0_irq);           // 3  SPI0
        plic.irq_in[3](timer0_irq);         // 4  TIMER0
        plic.irq_in[4](wdt0_irq);           // 5  WDT0
        plic.irq_in[5](tie_low);            // 6  PWM0 (no IRQ output)
        plic.irq_in[6](dma0_irq_nonzero);   // 7  DMA0 done
        plic.irq_in[7](dma0_irq_abort);     // 8  DMA0 abort
        plic.irq_in[8](trng0_irq);          // 9  TRNG0
        plic.irq_in[9](tie_low);            // 10 CMU0 (reserved)
        plic.irq_in[10](tie_low);           // 11 PMU0 (not instantiated here)
        plic.irq_in[11](dmic0_irq);         // 12 DMIC0
        plic.irq_in[12](otp0_irq);          // 13 OTP0
        plic.irq_in[13](qspi0_irq);         // 14 QSPI0
        plic.irq_in[14](tie_low);           // 15 ISP0 (reserved)
        plic.irq_in[15](tie_low);           // 16 VPU0 (reserved)
        plic.irq_in[16](tie_low);           // 17 NPU0 (not instantiated here)
        plic.irq_in[17](uart1_irq);         // 18 UART1
        plic.irq_in[18](i2c1_irq);          // 19 I2C1
        plic.irq_in[19](spi1_irq);          // 20 SPI1
        plic.irq_in[20](timer1_irq);        // 21 TIMER1
        plic.irq_in[21](rtc0_irq);          // 22 RTC0
        plic.irq_in[22](adc0_irq);          // 23 ADC0
        for (unsigned source = 23; source < kNumPlic; ++source) {
            plic.irq_in[source](tie_low);   // 24..31 reserved
        }
        tie_low.write(false);

        // ── Static reset levels ────────────────────────────────────────────
        spi0_rst.write(true);   spi1_rst.write(true);
        timer0_rst.write(true); timer1_rst.write(true);
        timer0_ext.write(false); timer1_ext.write(false);
        wdt0_rst.write(true);   dma0_rst.write(true);
        trng0_rst.write(true);  trng0_clk.write(false);
        dmic0_rst.write(true);  qspi0_rst.write(true);
        rtc0_rst.write(true);   adc0_rst.write(true);
        cmu_byp_ack.write(false);
        for (int index = 0; index < 4; ++index) cmu_idle[index].write(false);

        // The spin-loop image goes in during construction, not
        // `start_of_simulation`: the ISS may fetch on the first delta, and a
        // CPU that reaches zeroed memory first takes a trap and never recovers.
        // This is a backdoor load and deliberately does not cross the NoC —
        // the image is not traffic the design would carry, and charging it to
        // the interconnect would corrupt every number reported below.
        if (execution_mode == noc_soc_mode::survey) {
            for (std::uint64_t offset = 0; offset < 0x2000; offset += 4) {
                ram.load(kSpinLoop, sizeof(kSpinLoop), offset);
                bootrom.load(kSpinLoop, sizeof(kSpinLoop), offset);
            }
        }

        if (!config_path.empty()) {
            std::cout << "noc_soc config: " << config_path << '\n';
            std::cout << "noc_soc mode: "
                      << (execution_mode == noc_soc_mode::survey
                              ? "survey"
                              : "firmware")
                      << '\n';
            if (interconnect_timing == noc_timing_mode::fast) {
                std::cout << "interconnect timing: fast approximately-timed "
                             "FlooNoC, 4x4 placement, 1 ns calibration clock\n";
            } else {
                std::cout << "interconnect timing: detailed cycle-stepped "
                             "FlooNoC, 4x4 mesh, 1 ns network clock\n";
            }
            std::cout << "IPs: UARTx2 I2Cx2 SPIx2 TIMERx2 WDT PWM DMA TRNG CMU "
                         "DMIC OTP QSPI(+flash) RTC ADC GPIO | RAM 16 MiB @ "
                         "0x8000_0000 | CLINT + PLIC | RISC-V CPU\n";
        }
    }

    void monitor_uart0()
    {
        static constexpr char marker[] = CDC_NOC_DASHBOARD_REQUEST;
        const char byte = static_cast<char>(uart0_tx.read());

        if (uart0_control_candidate.empty()) {
            if (byte == marker[0]) {
                uart0_control_candidate.push_back(byte);
                return;
            }
            std::cout << byte;
            std::cout.flush();
            return;
        }

        uart0_control_candidate.push_back(byte);
        const std::size_t index = uart0_control_candidate.size() - 1u;
        if (index >= sizeof(marker) - 1u || byte != marker[index]) {
            std::cout << uart0_control_candidate;
            uart0_control_candidate.clear();
            std::cout.flush();
            return;
        }
        if (uart0_control_candidate.size() != sizeof(marker) - 1u) {
            return;
        }

        uart0_control_candidate.clear();
        try {
            probe.publish_live_metrics();
            std::cout << "\nnoc_soc live metrics snapshot: "
                      << probe.metrics_path << '\n';
        } catch (const std::exception& error) {
            std::cout << "\nnoc_soc live dashboard unavailable: "
                      << error.what() << '\n';
        }
        std::cout.flush();
    }

    void dma_irq_bridge() { dma0_irq_nonzero.write(dma0_irq.read() != 0u); }

    /// Reports the first assertion of every interrupt source. Firmware written
    /// for a platform without these peripherals will not have set `mtvec`, and
    /// the first one to fire sends the CPU to address zero.
    void irq_watch()
    {
        const struct { const char* name; bool level; } sources[] = {
            {"uart0", uart0_irq.read()}, {"uart1", uart1_irq.read()},
            {"i2c0", i2c0_irq.read()},   {"i2c1", i2c1_irq.read()},
            {"spi0", spi0_irq.read()},   {"spi1", spi1_irq.read()},
            {"timer0", timer0_irq.read()}, {"timer1", timer1_irq.read()},
            {"wdt0", wdt0_irq.read()},   {"trng0", trng0_irq.read()},
            {"dmic0", dmic0_irq.read()}, {"otp0", otp0_irq.read()},
            {"qspi0", qspi0_irq.read()}, {"rtc0", rtc0_irq.read()},
            {"adc0", adc0_irq.read()},
            {"dma0", dma0_irq_nonzero.read()},
            {"dma0_abort", dma0_irq_abort.read()},
        };
        for (const auto& source : sources) {
            if (source.level && irq_seen.find(source.name) == irq_seen.end()) {
                irq_seen.insert(source.name);
                std::cout << "[IRQ] " << source.name << " asserted at "
                          << sc_core::sc_time_stamp() << '\n';
            }
        }
    }

    /// The DMA completion line lives here; the measurement accounting lives in
    /// the probe, which owns the end-of-run report.
    void forward_dma_completion_irq()
    {
        if (dma0_irq_nonzero.read()) {
            probe.note_dma_completion_irq();
        }
    }

    std::set<std::string> irq_seen;

    /// Reports the moment the CPU first lands on address zero, and the last
    /// address it was executing before that. Without it the ISS's trap warning
    /// gives no way to correlate the fault with what the firmware was doing.
    void pc_watch()
    {
        std::uint64_t previous = 0;
        while (true) {
            wait(sc_core::sc_time(20, sc_core::SC_NS));
            const auto pc = cpu.get_pc();
            if (pc == 0 && previous != 0) {
                std::cout << "[PC] trapped to 0 at " << sc_core::sc_time_stamp()
                          << ", last pc before that 0x" << std::hex << previous
                          << std::dec << '\n';
                return;
            }
            previous = pc;
        }
    }

    /// Firmware, or a spin loop so the CPU fetches something valid.
    ///
    /// This is a backdoor load straight into the memory model. It must not go
    /// through the NoC: the image is not traffic the design would ever carry,
    /// and charging it to the interconnect would corrupt every number the run
    /// reports.
    void start_of_simulation() override
    {
        if (execution_mode == noc_soc_mode::firmware) {
            // The ELF loader writes through the CPU's bus, so it can only run
            // once bindings resolve.
            cpu.load_elf(firmware_path);
            std::cout << "noc_soc firmware: " << firmware_path << '\n';
        }
    }
};

noc_soc_top::noc_soc_top(
    sc_core::sc_module_name name, std::string config_path, noc_soc_mode mode,
    noc_timing_mode timing, std::string firmware, double sim_us,
    bool measure_baseline, std::string metrics_path)
    : sc_core::sc_module(name)
    , impl_([&]() {
        validate_mode(mode, firmware);
        return new impl(
            "impl", config_path, mode, timing, std::move(firmware), sim_us,
            measure_baseline, std::move(metrics_path));
    }())
{
}

noc_soc_top::~noc_soc_top() = default;

void noc_soc_top::set_uart0_socket(
    std::uint16_t port, bool wait_for_client)
{
    impl_->uart0_host.listen_on(port, wait_for_client);
}

void noc_soc_top::set_uart0_rx_file(
    const std::string& path, std::uint64_t start_delay_us)
{
    impl_->uart0_host.replay_file(
        path, sc_core::sc_time(static_cast<double>(start_delay_us),
                              sc_core::SC_US));
}

} // namespace cdc::platforms::noc_soc
