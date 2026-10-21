#pragma once

// CPU1 = Ibex (boot/control CPU). Nguồn: HAS v2.0 + FTEL_FU3_Common_Spec v2.0 (Ibex-Config, MemoryMap)

// Ibex dùng lại cva6_wrapper (ISS Bremen) với cấu hình FU3 riêng:
//   cdc::cpu::cva6_wrapper ibex("ibex", fu3::ibex_cpu_config(), fu3::kIbexDisableExtensions);
//   ibex.data_bus().bind(fabric.initiator("IBEX"));  
// Ibex chạy ngay, không giữ reset như CVA6

#include <cstdint>

#include "fu3_cpu/cva6_config.h"   // kDsramBase, kCause* dùng chung

namespace cdc::cpu::fu3 {

// Common v2.0 Ibex-Config #33-#36: aperture Debug Module, debug code trong 4 KiB cuối của IBEX_ISRAM
constexpr std::uint64_t kIbexDmBaseAddr      = 0x1A11'0000;
constexpr std::uint64_t kIbexDmAddrMask      = 0x0000'0FFF;
constexpr std::uint64_t kIbexDmHaltAddr      = 0x0002'F000;
constexpr std::uint64_t kIbexDmExceptionAddr = 0x0002'F008;

// Memory Map (HAS Table 10-1, Common v2.0 MemoryMap)
constexpr std::uint64_t kRomBase           = 0x0000'0000;  // 4 KiB, chỉ Ibex đọc được
constexpr std::uint64_t kRomSize           = 0x0000'1000;
constexpr std::uint64_t kIbexIsramBase     = 0x0002'0000;  // 64 KiB
constexpr std::uint64_t kIbexIsramSize     = 0x0001'0000;
constexpr std::uint64_t kIbexDebugCodeBase = 0x0002'F000;  // 4 KiB cuối của IBEX_ISRAM

// Ibex reset vào ROM.
// Sau khi stage xong, ROM nhảy tới kIbexIsramBase.
constexpr std::uint64_t kIbexResetPc = kRomBase;

// CVA6 = hart 0 (PLIC target 0)
// Ibex = hart 1 
constexpr std::uint32_t kIbexHartId = 1;

inline cpu_config ibex_cpu_config()
{
    cpu_config cfg;
    cfg.xlen = 32;
    cfg.has_mmu = false;   // Ibex không có MMU, chỉ PMP
    cfg.has_smp = false;
    cfg.num_irq = 1;       // Chỉ nối được irq_external_i, irq_fast_i/irq_nm_i ISS chưa hỗ trợ
    cfg.hart_id = kIbexHartId;
    cfg.reset_pc = kIbexResetPc;
    return cfg;
}

// Table 8-3: RV32I + M (RV32MFast) + C (Zca), B = None. Ibex có M và U-mode (PMP).
// ISS Default: IMACF + NUS => bỏ AFNS còn RV32IMC + M/U.
// Zcb chưa có trong ISS Bremen, FW Ibex phải build không dùng Zcb cho tới khi patch ISS.
inline constexpr const char* kIbexDisableExtensions = "AFNS";

}  // namespace cdc::cpu::fu3
