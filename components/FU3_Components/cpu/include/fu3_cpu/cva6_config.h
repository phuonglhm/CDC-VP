#pragma once

// CPU0 = CVA6 (application CPU). Nguồn: Common Spec v2.1 (CVA6-Config, MemoryMap,
// Interrupt Mapping) và các quyết định Q01/Q02 trong T02_De_Xuat_Q01_Q08.md.

/*  cdc::cpu::cva6_wrapper cva6("cva6", fu3::cva6_cpu_config(), fu3::kCva6DisableExtensions,
                                fu3::cva6_core_options_fu3());
    cva6.set_reset(true);    // Giữ reset từ đầu */

#include <cstdint>

#include <cdc/cpu/cpu_base.h>

#include "cva6_wrapper.h"

namespace cdc::cpu::fu3 {

// Common v2.1 CVA6-Config #45, #46: debug code nằm trong 4 KiB cuối của CVA6_ISRAM
constexpr std::uint64_t kCva6HaltAddress      = 0x0004'0000;
constexpr std::uint64_t kCva6ExceptionAddress = 0x0004'0008;

// Memory Map (Common v2.1 MemoryMap rows 3, 4, 9)
constexpr std::uint64_t kCva6IsramBase     = 0x0001'0000;  // 192 KiB image + 4 KiB debug code
constexpr std::uint64_t kCva6IsramSize     = 0x0003'1000;
constexpr std::uint64_t kCva6DebugCodeBase = kCva6HaltAddress;
constexpr std::uint64_t kDsramBase         = 0x0100'0000;  // 256 KiB, dùng chung, non-executable
constexpr std::uint64_t kDsramSize         = 0x0004'0000;

// CVA6-Config #59: boot_addr_i = 0x0001_0000.
constexpr std::uint64_t kCva6ResetPc = kCva6IsramBase;

// PLIC target 0 = "hart 0 M-mode" -> CVA6 (Q01-c)
constexpr std::uint32_t kCva6HartId = 0;

// CPU 400 MHz (HAS v2.1 §2). The ISS cycle follows it until the CRM drives the
// clock (T07); instruction timing is a VP model, not silicon timing (Q01-g).
constexpr std::uint64_t kCva6ClockHz = 400'000'000;

// Interrupt Mapping: các đường ngắt vào CVA6 -> cause dùng cho set_irq().
constexpr unsigned kCauseSoftware = 3;   // ipi_i      <- SYSCSR software IRQ
constexpr unsigned kCauseTimer    = 7;   // time_irq_i <- TIMER0
constexpr unsigned kCauseExternal = 11;  // irq_i[0]   <- PLIC target 0

inline cpu_config cva6_cpu_config()
{
    cpu_config cfg;
    cfg.xlen = 32;
    cfg.has_mmu = false;   // #MmuPresent = 0
    cfg.has_smp = false;
    cfg.num_irq = 1;       // Chỉ irq_i[0] (M-mode), irq_i[1] tie low
    cfg.hart_id = kCva6HartId;
    cfg.reset_pc = kCva6ResetPc;
    return cfg;
}

// CVA6-Config #2-#12 (Q01-a): RVF = 1, RVD = RVA = RVS = RVU = 0, no MMU or cache.
// ISS default IMACF + NUS -> bỏ A, N, S, U còn RV32IMFC, chỉ M-mode.
// RVB = 1 is required but its subset is not yet decided (Q01-a CHỜ) and the ISS
// has no Zb*: B is not offered and is listed as an open gap.
inline constexpr const char* kCva6DisableExtensions = "ANSU";

// CVA6-Config #42/#43: 8 PMP entries with NAPOT; reset with every entry OFF
// (ASSUMPTION: CVA6 PMP reset value 0, granularity 4 bytes). M-mode only, so
// only locked entries restrict it. mtvec keeps direct/vectored (WARL).
inline cva6_core_options cva6_core_options_fu3()
{
    cva6_core_options o;
    o.pmp_regions = 8;
    o.cycle_time = sc_core::sc_time(1e9 / static_cast<double>(kCva6ClockHz), sc_core::SC_NS);
    o.bus_timed_memory = true;
    o.split_misaligned = false;  // CVA6 traps misaligned loads/stores (cause 4/6); ASSUMPTION per CVA6 docs, no HW split
    o.debug = true;                       // CVA6-Config #44 DebugEn
    o.debug_halt_address = kCva6HaltAddress;
    o.debug_exception_address = kCva6ExceptionAddress;
    o.debug_module_base = 0x1A11'0000;    // DM aperture (one DM for both harts, Q04-c)
    o.debug_module_size = 0x1000;
    o.triggers = 2;                       // #47; execute-only, type/fields provisional (Q04-e)
    o.reset_sp.reset();
    return o;
}

}  // namespace cdc::cpu::fu3
