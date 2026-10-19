#pragma once

//   cdc::cpu::cva6_wrapper cva6("cva6", fu3::cva6_cpu_config(), fu3::kCva6DisableExtensions);
//   cva6.set_reset(true);    // Giữ reset từ đầu

#include <cstdint>

#include <cdc/cpu/cpu_base.h>

#include "cva6_wrapper.h"

namespace cdc::cpu::fu3 {

// Memory Map (Table 10-1)
constexpr std::uint64_t kCva6IsramBase     = 0x0001'0000;  // 64 KiB
constexpr std::uint64_t kCva6IsramSize     = 0x0001'0000;
constexpr std::uint64_t kCva6DebugCodeBase = 0x0001'F000;  // 4 KiB cuối của CVA6_ISRAM
constexpr std::uint64_t kDsramBase         = 0x0100'0000;  // 64 KiB, dùng chung, non-executable
constexpr std::uint64_t kDsramSize         = 0x0001'0000;

// Ibex đặt boot_addr_i của CVA6 = 0x0001_0000 rồi nhả reset.
constexpr std::uint64_t kCva6ResetPc = kCva6IsramBase;

// PLIC target 0 = "hart 0 M-mode" -> CVA6
constexpr std::uint32_t kCva6HartId = 0;

// CPU 400 MHz. Wrapper hiện chạy cố định 1000 lệnh / 10 us => giá trị này chưa được dùng nhưng giữ để nối với CRM/PLL sau này.
constexpr std::uint64_t kCva6ClockHz = 400'000'000;

// Table 11-2: các đường ngắt vào CVA6 -> cause dùng cho set_irq().
constexpr unsigned kCauseSoftware = 3;   // ipi_i      <- SYSCSR software IRQ
constexpr unsigned kCauseTimer    = 7;   // time_irq_i <- TIMER0
constexpr unsigned kCauseExternal = 11;  // irq_i[0]   <- PLIC target 0

inline cpu_config cva6_cpu_config()
{
    cpu_config cfg;
    cfg.xlen = 32;        
    cfg.has_mmu = false;   // #MmuPresent = 0
    cfg.has_smp = false;
    cfg.num_irq = 1;       // chỉ irq_i[0] (M-mode), irq_i[1] tie low
    cfg.hart_id = kCva6HartId;
    cfg.reset_pc = kCva6ResetPc;
    return cfg;
}

// Table 8-1: RVA = RVF = RVD = RVS = RVU = 0. 
// ISS Default: IMACF + NUS, 
// FU3: RVA=0, RVF=0, RVD=0, RVS=0, RVU=0
// Bỏ AFNSU còn RV32IMC, chỉ M-mode do N không có trên CVA6
inline constexpr const char* kCva6DisableExtensions = "AFNSU";

}  // namespace cdc::cpu::fu3
