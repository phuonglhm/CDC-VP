#pragma once

// CPU1 = Ibex (boot/control CPU). Nguồn: Common Spec v2.1 (Ibex-Config, MemoryMap,
// Booting, Interrupt Mapping) và các quyết định Q01/Q02 trong T02_De_Xuat_Q01_Q08.md.

// Ibex dùng lại cva6_wrapper (ISS fork cpu_models/fu3_iss) với cấu hình FU3 riêng:
//   cdc::cpu::cva6_wrapper ibex("ibex", fu3::ibex_cpu_config(), fu3::kIbexDisableExtensions,
//                               fu3::ibex_core_options());
//   ibex.data_bus().bind(fabric.initiator("IBEX"));
// Ibex chạy ngay, không giữ reset như CVA6

#include <cstdint>

#include "fu3_cpu/cva6_config.h"   // kDsramBase, kCause* dùng chung

namespace cdc::cpu::fu3 {

// Common v2.1 Ibex-Config #33-#36: aperture Debug Module, debug code trong 4 KiB cuối của IBEX_ISRAM
constexpr std::uint64_t kIbexDmBaseAddr      = 0x1A11'0000;
constexpr std::uint64_t kIbexDmAddrMask      = 0x0000'0FFF;
constexpr std::uint64_t kIbexDmHaltAddr      = 0x0006'0000;
constexpr std::uint64_t kIbexDmExceptionAddr = 0x0006'0008;

// Memory Map (Common v2.1 MemoryMap rows 1, 6, 7)
constexpr std::uint64_t kRomBase           = 0x0000'0000;  // 4 KiB, chỉ Ibex đọc được
constexpr std::uint64_t kRomSize           = 0x0000'1000;
constexpr std::uint64_t kIbexIsramBase     = 0x0005'0000;  // 64 KiB image + 4 KiB debug code
constexpr std::uint64_t kIbexIsramSize     = 0x0001'1000;
constexpr std::uint64_t kIbexDebugCodeBase = kIbexDmHaltAddr;

// Booting #22 / MemoryMap #1, decision Q01-b: boot_addr_i = 0, so the first
// fetch is boot_addr + 0x80 and mtvec resets to BASE 0, MODE vectored (0x1).
constexpr std::uint64_t kIbexResetPc = kRomBase + 0x80;
constexpr std::uint32_t kIbexMtvecReset = 0x0000'0001;

// CVA6 = hart 0 (PLIC target 0), Ibex = hart 1 (Q01-c, VP identifier)
constexpr std::uint32_t kIbexHartId = 1;

// Interrupt Mapping (Ibex): irq_fast_i[i] -> cause 16 + i, priority
// NMI > fast[0..14] > MEI > MSI > MTI. irq_nm_i is tied low (no NMI source).
constexpr unsigned kIbexFastIrqs = 15;
constexpr unsigned kCauseFast0 = 16;

inline cpu_config ibex_cpu_config()
{
    cpu_config cfg;
    cfg.xlen = 32;
    cfg.has_mmu = false;   // Ibex không có MMU, chỉ PMP
    cfg.has_smp = false;
    cfg.num_irq = 1 + kIbexFastIrqs;  // irq_external_i + irq_fast_i[14:0]
    cfg.hart_id = kIbexHartId;
    cfg.reset_pc = kIbexResetPc;
    return cfg;
}

// Ibex-Config #1/#13-#15 (Q01-a, Q01-e): RV32I + M (RV32MFast) + C (Zca + Zcb), B = None, U-mode.
// ISS default IMACF + NUS => bỏ A, F, N, S. Zcb without Zbb: c.sext.b/h and
// c.zext.h are illegal.
inline constexpr const char* kIbexDisableExtensions = "AFNS";

// PMP/ePMP reset profile (Ibex-Config #2-#4, #7-#9, decision Q01-d): 16
// regions, mseccfg RLB = 1, MMWP = 1, MML = 0. Under MML = 0 an unlocked rule
// does not constrain M-mode, so every rule is locked; RLB lets the boot ROM
// rewrite them during staging before it applies the final W^X policy.
//
// VP PROPOSAL, not yet signed off (Q01-d: "chốt bảng reset trước test
// reset/boot"): the spec gives only the regions, not the 16 cfg/addr values.
// Regions follow the Common v2.1 map; no region is RWX; ROM and XIP are RX,
// SRAM staging windows, DSRAM, MMIO and HyperRAM are RW.
namespace pmp {
constexpr std::uint8_t L = 0x80, TOR = 0x08, NAPOT = 0x18, X = 0x04, W = 0x02, R = 0x01;
constexpr std::uint32_t napot(std::uint32_t base, std::uint32_t size) { return (base | (size / 2 - 1)) >> 2; }
constexpr std::uint32_t tor(std::uint32_t address) { return address >> 2; }
}  // namespace pmp
constexpr std::uint32_t kIbexMseccfgReset = 0b110;  // RLB | MMWP

inline cva6_core_options ibex_core_options()
{
    using namespace pmp;
    cva6_core_options o;
    o.fast_irqs = kIbexFastIrqs;
    o.mtvec_vectored_only = true;
    o.mtvec_reset = kIbexMtvecReset;
    o.zcb = true;
    o.pmp_regions = 16;
    o.smepmp = true;
    o.mseccfg_reset = kIbexMseccfgReset;
    o.pmpcfg_reset = {
        L | NAPOT | R | X,  // 0: ROM 0x0000_0000, 4 KiB
        0,                  // 1: TOR base, CVA6_ISRAM 0x0001_0000
        L | TOR | R | W,    // 2: CVA6_ISRAM staging up to 0x0004_1000
        0,                  // 3: TOR base, IBEX_ISRAM 0x0005_0000
        L | TOR | R | W,    // 4: IBEX_ISRAM staging up to 0x0006_1000
        L | NAPOT | R | W,  // 5: DSRAM 0x0100_0000, 256 KiB
        L | NAPOT | R | X,  // 6: QSPI XIP 0x3000_0000, 16 MiB
        L | NAPOT | R | W,  // 7: EDHOC_OSCORE 0x3101_0000, 64 KiB
        0,                  // 8: TOR base, MMIO 0x4000_0000
        L | TOR | R | W,    // 9: AES, PLIC, PERIBUS_0/1 up to 0x5011_0000
        L | NAPOT | R | W,  // 10: HyperRAM 0x6000_0000, 32 MiB
        0, 0, 0, 0, 0,      // 11-15: OFF
    };
    o.pmpaddr_reset = {
        napot(0x0000'0000, 0x1000), tor(0x0001'0000), tor(0x0004'1000), tor(0x0005'0000), tor(0x0006'1000),
        napot(0x0100'0000, 0x4'0000), napot(0x3000'0000, 0x100'0000), napot(0x3101'0000, 0x1'0000),
        tor(0x4000'0000), tor(0x5011'0000), napot(0x6000'0000, 0x200'0000), 0, 0, 0, 0, 0,
    };
    o.cycle_time = sc_core::sc_time(1e9 / static_cast<double>(kCva6ClockHz), sc_core::SC_NS);
    o.bus_timed_memory = true;
    o.split_misaligned = true;  // Ibex LSU splits misaligned accesses into aligned words
    o.debug = true;                       // DbgTriggerEn, Debug Module
    o.debug_halt_address = kIbexDmHaltAddr;
    o.debug_exception_address = kIbexDmExceptionAddr;
    o.debug_module_base = kIbexDmBaseAddr;
    o.debug_module_size = kIbexDmAddrMask + 1;
    o.triggers = 4;                       // DbgHwBreakNum; mcontrol exact execute (Q04-e)
    o.reset_sp.reset();
    return o;
}

}  // namespace cdc::cpu::fu3
