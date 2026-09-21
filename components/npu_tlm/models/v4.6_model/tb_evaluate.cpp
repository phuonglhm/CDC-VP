//
// SystemC Model for SAURIA NPU Core
// Evaluation Testbench comparing PE parameter profiles:
// 1. Standard FP32 Profile
// 2. Approximate FP32 Profile
// 3. Sparsity Zero-Gating Profile
//

// ---  define MACRO datatype and size---
#ifndef EVAL_X
#define EVAL_X 16
#endif

#ifndef EVAL_Y
#define EVAL_Y 8
#endif

#ifndef NPU_DTYPE_IN
#define NPU_DTYPE_IN int8_t // Active int8 for NPU
#endif

#ifndef NPU_DTYPE_OUT
#define NPU_DTYPE_OUT int32_t
#endif

// PE/SRAM-C accumulator type. Defaults to NPU_DTYPE_OUT (int32_t, matches the
// INT8 path unchanged). The FP16 build overrides this to plain `float` so the
// systolic array accumulates in wide precision and only rounds down to fp16
// once at DRAM write-out (matching the SAURIA "ideal" reference model) instead
// of rounding to fp16 on every MAC cycle.
#ifndef NPU_DTYPE_PSUM
#define NPU_DTYPE_PSUM NPU_DTYPE_OUT
#endif

// FP16 verification tolerance (in ULPs of half precision). Floating-point add is
// not associative: the systolic array accumulates products in array-scan order,
// while the SAURIA "ideal" reference (torch float32) uses a blocked/SIMD reduction
// order. The datapath is otherwise bit-correct (products are correctly-rounded),
// so results differ only by last-bit rounding that grows with the reduction depth
// K (observed max 8 ULP at K=256 on 64x64). A logic bug would be off by hundreds
// of ULP / NaN, far outside this window. INT paths remain EXACT (0-ULP). Override
// with -DNPU_FP16_ULP_TOL=N.
#ifndef NPU_FP16_ULP_TOL
#define NPU_FP16_ULP_TOL 16
#endif

#ifndef A_REGION_BYTES
#define A_REGION_BYTES 4096
#endif

#ifndef B_REGION_BYTES
#define B_REGION_BYTES 2048
#endif

#ifndef C_REGION_BYTES
#define C_REGION_BYTES 2048
#endif

#include "npu_top.h"
#include "sauria_cfg_layout.h"
#include <map>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <iomanip>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>

// ######################### HELPER ###########################
static inline int32_t wrap_add_i32(int32_t a, int32_t b)
{
    uint32_t ua = static_cast<uint32_t>(a);
    uint32_t ub = static_cast<uint32_t>(b);
    uint32_t us = ua + ub;
    return static_cast<int32_t>(us);
}

static inline int32_t read_i32_le(const std::vector<uint8_t> &mem, size_t byte_addr)
{
    uint32_t v = 0;
    v |= static_cast<uint32_t>(mem[byte_addr + 0]) << 0;
    v |= static_cast<uint32_t>(mem[byte_addr + 1]) << 8;
    v |= static_cast<uint32_t>(mem[byte_addr + 2]) << 16;
    v |= static_cast<uint32_t>(mem[byte_addr + 3]) << 24;
    return static_cast<int32_t>(v);
}

static inline void write_i32_le(std::vector<uint8_t> &mem, size_t byte_addr, int32_t value)
{
    uint32_t v = static_cast<uint32_t>(value);

    mem[byte_addr + 0] = static_cast<uint8_t>((v >> 0) & 0xff);
    mem[byte_addr + 1] = static_cast<uint8_t>((v >> 8) & 0xff);
    mem[byte_addr + 2] = static_cast<uint8_t>((v >> 16) & 0xff);
    mem[byte_addr + 3] = static_cast<uint8_t>((v >> 24) & 0xff);
}

static inline uint32_t calc_c_elem_addr(uint32_t context, uint32_t x, uint32_t y, uint32_t ncontexts, uint32_t y_dim)
{
    return x * (ncontexts * y_dim) + context * y_dim + y;
}

using namespace sauria;

class TestbenchEvaluate : public sc_module
{
public:
    // Clock & Reset Ports
    sc_in<bool> i_clk{"i_clk"};
    sc_out<bool> o_rstn{"o_rstn"};
    sc_out<bool> o_soft_reset{"o_soft_reset"};

    sc_out<uint32_t> o_total_contexts{"o_total_contexts"};
    sc_out<uint32_t> o_mvm_k{"o_mvm_k"};
    // NPU Host Control Interfaces
    sc_out<bool> o_start_std{"o_start_std"};
    sc_in<bool> i_done_std{"i_done_std"};
    sc_in<bool> i_deadlock_std{"i_deadlock_std"};

    sc_out<bool> o_start_approx{"o_start_approx"};
    sc_in<bool> i_done_approx{"i_done_approx"};
    sc_in<bool> i_deadlock_approx{"i_deadlock_approx"};

    sc_out<bool> o_start_gated{"o_start_gated"};
    sc_in<bool> i_done_gated{"i_done_gated"};
    sc_in<bool> i_deadlock_gated{"i_deadlock_gated"};

    // NPU Host Memory Ports - STD
    sc_out<uint32_t> o_host_addr_std{"o_host_addr_std"};
    sc_out<bool> o_host_wren_std{"o_host_wren_std"};
    sc_out<bool> o_host_rden_std{"o_host_rden_std"};
    sc_out<host_data_t> o_host_wdata_std{"o_host_wdata_std"};
    sc_out<host_mask_t> o_host_wmask_std{"o_host_wmask_std"};
    sc_in<host_data_t> i_host_rdata_std{"i_host_rdata_std"};

    // NPU Host Memory Ports - APPROX
    sc_out<uint32_t> o_host_addr_approx{"o_host_addr_approx"};
    sc_out<bool> o_host_wren_approx{"o_host_wren_approx"};
    sc_out<bool> o_host_rden_approx{"o_host_rden_approx"};
    sc_out<host_data_t> o_host_wdata_approx{"o_host_wdata_approx"};
    sc_out<host_mask_t> o_host_wmask_approx{"o_host_wmask_approx"};
    sc_in<host_data_t> i_host_rdata_approx{"i_host_rdata_approx"};

    // NPU Host Memory Ports - GATED
    sc_out<uint32_t> o_host_addr_gated{"o_host_addr_gated"};
    sc_out<bool> o_host_wren_gated{"o_host_wren_gated"};
    sc_out<bool> o_host_rden_gated{"o_host_rden_gated"};
    sc_out<host_data_t> o_host_wdata_gated{"o_host_wdata_gated"};
    sc_out<host_mask_t> o_host_wmask_gated{"o_host_wmask_gated"};
    sc_in<host_data_t> i_host_rdata_gated{"i_host_rdata_gated"};

    // Configurations
    sc_out<float> o_threshold{"o_threshold"};
    sc_out<sc_bv<3>> o_select{"o_select"};

    SC_CTOR(TestbenchEvaluate)
    {
        SC_THREAD(test_process);
        sensitive << i_clk.pos();
    }

private:
    // Dynamic address calculator helpers
    const int subwords_a = EVAL_Y / 4;
    const int mask_a = subwords_a - 1;
    const int shift_a = (subwords_a == 16) ? 4 : ((subwords_a == 8) ? 3 : ((subwords_a == 4) ? 2 : ((subwords_a == 2) ? 1 : 0)));

    const int subwords_b = EVAL_X / 4;
    const int mask_b = subwords_b - 1;
    const int shift_b = (subwords_b == 16) ? 4 : ((subwords_b == 8) ? 3 : ((subwords_b == 4) ? 2 : ((subwords_b == 2) ? 1 : 0)));

    const int subwords_c = EVAL_Y / 4;
    const int mask_c = subwords_c - 1;
    const int shift_c = (subwords_c == 16) ? 4 : ((subwords_c == 8) ? 3 : ((subwords_c == 4) ? 2 : ((subwords_c == 2) ? 1 : 0)));

    std::vector<host_data_t> sramc_before_run;

    uint32_t get_srama_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMA_OFFSET | ((phys_addr << shift_a) | (sub_word & mask_a));
    }

    uint32_t get_sramb_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMB_OFFSET | ((phys_addr << shift_b) | (sub_word & mask_b));
    }

    uint32_t get_sramc_addr(uint32_t phys_addr, uint32_t sub_word)
    {
        return SRAMC_OFFSET | ((phys_addr << shift_c) | (sub_word & mask_c));
    }

    bool is_controller_arg_write(uint32_t raw_addr, bool wren)
    {
        if (!wren)
            return false;
        if (raw_addr < 0x40000010)
            return false;
        if (raw_addr >= 0x40000400)
            return false;
        return ((raw_addr - 0x40000010) % 4) == 0;
    }

    uint32_t get_controller_arg_index(uint32_t raw_addr)
    {
        return (raw_addr - 0x40000010) >> 2;
    }

    bool is_controller_start_write(uint32_t raw_addr, uint32_t data_in, bool wren)
    {
        return wren && (raw_addr == 0x40000000) && (data_in == 3);
    }

    enum class StimRegion
    {
        CONTROLLER,
        SAURIA_CORE,
        DMA,
        SAURIA_INTERNAL,
        UNKNOWN
    };

    StimRegion get_stim_region(uint32_t raw_addr)
    {
        if (raw_addr >= 0x40000000 && raw_addr < 0x50000000)
        {
            return StimRegion::CONTROLLER;
        }

        if (raw_addr >= 0x50000000 && raw_addr < 0x50400000)
        {
            return StimRegion::SAURIA_CORE;
        }

        if (raw_addr >= 0x60000000 && raw_addr < 0x70000000)
        {
            return StimRegion::DMA;
        }

        if (raw_addr < 0x00400000)
        {
            return StimRegion::SAURIA_INTERNAL;
        }

        return StimRegion::UNKNOWN;
    }

    std::string stim_region_name(StimRegion r)
    {
        switch (r)
        {
        case StimRegion::CONTROLLER:
            return "CONTROLLER";
        case StimRegion::SAURIA_CORE:
            return "SAURIA_CORE";
        case StimRegion::DMA:
            return "DMA";
        case StimRegion::SAURIA_INTERNAL:
            return "SAURIA_INTERNAL";
        default:
            return "UNKNOWN";
        }
    }

    uint32_t normalize_sauria_addr(uint32_t raw_addr)
    {
        if (raw_addr >= 0x50000000 && raw_addr < 0x50400000)
        {
            return raw_addr - 0x50000000;
        }

        return raw_addr;
    }

    bool is_sauria_core_transaction(uint32_t raw_addr)
    {
        StimRegion r = get_stim_region(raw_addr);

        return (r == StimRegion::SAURIA_CORE ||
                r == StimRegion::SAURIA_INTERNAL);
    }

    bool is_cfg_addr(uint32_t internal_addr)
    {
        return (
            (internal_addr >= CFG_CON_OFFSET && internal_addr < CFG_CON_OFFSET + 0x200) ||
            (internal_addr >= CFG_ACT_OFFSET && internal_addr < CFG_ACT_OFFSET + 0x200) ||
            (internal_addr >= CFG_WEI_OFFSET && internal_addr < CFG_WEI_OFFSET + 0x200) ||
            (internal_addr >= CFG_OUT_OFFSET && internal_addr < CFG_OUT_OFFSET + 0x200));
    }

    void make_stim_packet(uint32_t internal_addr, uint32_t data_in, bool wren, host_data_t &packet, host_mask_t &mask)
    {
        packet.data.fill(0.0f);
        mask.data.fill(false);

        if (!wren)
        {
            return;
        }

        if (is_cfg_addr(internal_addr))
        {
            packet[0] = static_cast<float>(data_in);
            mask[0] = true;
        }
        else
        {
            packet[0] = static_cast<float>((data_in >> 0) & 0xFF);
            packet[1] = static_cast<float>((data_in >> 8) & 0xFF);
            packet[2] = static_cast<float>((data_in >> 16) & 0xFF);
            packet[3] = static_cast<float>((data_in >> 24) & 0xFF);

            mask[0] = true;
            mask[1] = true;
            mask[2] = true;
            mask[3] = true;
        }
    }

    struct DecodedSauriaConfig
    {
        // CONTROL
        uint32_t incntlim = 0;
        uint32_t act_reps = 0;
        uint32_t wei_reps = 0;
        uint32_t thres = 0;

        // ACTIVATION
        uint32_t xlim = 0;
        uint32_t xstep = 0;
        uint32_t ylim = 0;
        uint32_t ystep = 0;
        uint32_t chlim = 0;
        uint32_t chstep = 0;
        uint32_t til_xlim = 0;
        uint32_t til_xstep = 0;
        uint32_t til_ylim = 0;
        uint32_t til_ystep = 0;
        uint64_t dil_pat = 0;
        uint64_t rows_active = 0;

        // WEIGHT
        uint32_t wlim = 0;
        uint32_t wstep = 0;
        uint32_t klim = 0;
        uint32_t kstep = 0;
        uint32_t til_klim = 0;
        uint32_t til_kstep = 0;
        uint32_t cols_active = 0;
        uint32_t waligned = 0;

        // OUTPUT
        uint32_t ncontexts = 0;
        uint32_t cxlim = 0;
        uint32_t cxstep = 0;
        uint32_t cklim = 0;
        uint32_t ckstep = 0;
        uint32_t til_cylim = 0;
        uint32_t til_cystep = 0;
        uint32_t til_cklim = 0;
        uint32_t til_ckstep = 0;
        uint32_t inactive_cols = 0;
        uint32_t preload_en = 0;
    };

    DecodedSauriaConfig decode_sauria_packed_config(const std::vector<uint32_t> &controller_args)
    {
        DecodedSauriaConfig cfg;

        // Packed-config bit widths. Default = int8_8x16 HW (test 2). Override per HW
        // version at build time, e.g. int8_32x32: -DSAURIA_ACT_IDX_W=17 -DSAURIA_WEI_IDX_W=17
        // -DSAURIA_OUT_IDX_W=16  (values from hw_versions IFM_IDX_W/WEI_IDX_W/PSM_IDX_W).
#ifndef SAURIA_ACT_IDX_W
#define SAURIA_ACT_IDX_W 15
#endif
#ifndef SAURIA_WEI_IDX_W
#define SAURIA_WEI_IDX_W 16
#endif
#ifndef SAURIA_OUT_IDX_W
#define SAURIA_OUT_IDX_W 14
#endif
        constexpr uint32_t ACT_IDX_W = SAURIA_ACT_IDX_W;
        constexpr uint32_t WEI_IDX_W = SAURIA_WEI_IDX_W;
        constexpr uint32_t OUT_IDX_W = SAURIA_OUT_IDX_W;
        constexpr uint32_t TH_W = 2;
        constexpr uint32_t DILP_W = 64;
        constexpr uint32_t PARAMS_W = 8;

        // If X_DIM/Y_DIM are not visible here, replace them with constants.
        constexpr uint32_t X = EVAL_X;
        constexpr uint32_t Y = EVAL_Y;

        constexpr uint32_t START_ARG = 22;

        // Decode via the shared bit-layout (sauria_cfg_layout.h) so the tb decoder
        // and the driver-side encoder (libsauria_cfg) iterate ONE field/width table
        // and can never drift. rows_active comes back LSB-first (the layout handles
        // the MSB-first->LSB-first reversal); per-row weight offsets are skipped.
        CfgWidths cw;
        cw.idx_a = ACT_IDX_W; cw.idx_w = WEI_IDX_W; cw.idx_o = OUT_IDX_W;
        cw.X = X; cw.Y = Y; cw.th_w = TH_W; cw.dilp_w = DILP_W; cw.params_w = PARAMS_W;

        uint64_t fld[F_CFG_COUNT];
        cfg_decode(controller_args, START_ARG, cw, fld);

        cfg.incntlim   = (uint32_t)fld[F_CFG_INCNTLIM];
        cfg.act_reps   = (uint32_t)fld[F_CFG_ACT_REPS];
        cfg.wei_reps   = (uint32_t)fld[F_CFG_WEI_REPS];
        cfg.thres      = (uint32_t)fld[F_CFG_THRES];

        cfg.xlim       = (uint32_t)fld[F_CFG_XLIM];
        cfg.xstep      = (uint32_t)fld[F_CFG_XSTEP];
        cfg.ylim       = (uint32_t)fld[F_CFG_YLIM];
        cfg.ystep      = (uint32_t)fld[F_CFG_YSTEP];
        cfg.chlim      = (uint32_t)fld[F_CFG_CHLIM];
        cfg.chstep     = (uint32_t)fld[F_CFG_CHSTEP];
        cfg.til_xlim   = (uint32_t)fld[F_CFG_TIL_XLIM];
        cfg.til_xstep  = (uint32_t)fld[F_CFG_TIL_XSTEP];
        cfg.til_ylim   = (uint32_t)fld[F_CFG_TIL_YLIM];
        cfg.til_ystep  = (uint32_t)fld[F_CFG_TIL_YSTEP];
        cfg.dil_pat    = fld[F_CFG_DIL_PAT];
        cfg.rows_active = fld[F_CFG_ROWS_ACTIVE];

        cfg.wlim       = (uint32_t)fld[F_CFG_WLIM];
        cfg.wstep      = (uint32_t)fld[F_CFG_WSTEP];
        cfg.klim       = (uint32_t)fld[F_CFG_KLIM];
        cfg.kstep      = (uint32_t)fld[F_CFG_KSTEP];
        cfg.til_klim   = (uint32_t)fld[F_CFG_TIL_KLIM];
        cfg.til_kstep  = (uint32_t)fld[F_CFG_TIL_KSTEP];
        cfg.cols_active = (uint32_t)fld[F_CFG_COLS_ACTIVE];
        cfg.waligned   = (uint32_t)fld[F_CFG_WALIGNED];

        cfg.ncontexts  = (uint32_t)fld[F_CFG_NCONTEXTS];
        cfg.cxlim      = (uint32_t)fld[F_CFG_CXLIM];
        cfg.cxstep     = (uint32_t)fld[F_CFG_CXSTEP];
        cfg.cklim      = (uint32_t)fld[F_CFG_CKLIM];
        cfg.ckstep     = (uint32_t)fld[F_CFG_CKSTEP];
        cfg.til_cylim  = (uint32_t)fld[F_CFG_TIL_CYLIM];
        cfg.til_cystep = (uint32_t)fld[F_CFG_TIL_CYSTEP];
        cfg.til_cklim  = (uint32_t)fld[F_CFG_TIL_CKLIM];
        cfg.til_ckstep = (uint32_t)fld[F_CFG_TIL_CKSTEP];
        cfg.inactive_cols = (uint32_t)fld[F_CFG_INACTIVE_COLS];
        cfg.preload_en = (uint32_t)fld[F_CFG_PRELOAD_EN];

        return cfg;
    }

    void print_decoded_sauria_config(const DecodedSauriaConfig &cfg)
    {
        std::cout << "\n=========================================\n";
        std::cout << "DECODED SAURIA PACKED CONFIG\n";
        std::cout << "=========================================\n";

        std::cout << "\nCONTROL\n";
        std::cout << "incntlim       : " << cfg.incntlim << "\n";
        std::cout << "act_reps       : " << cfg.act_reps << "\n";
        std::cout << "wei_reps       : " << cfg.wei_reps << "\n";
        std::cout << "thres          : " << cfg.thres << "\n";

        std::cout << "\nACTIVATION\n";
        std::cout << "xlim           : " << cfg.xlim << "\n";
        std::cout << "xstep          : " << cfg.xstep << "\n";
        std::cout << "ylim           : " << cfg.ylim << "\n";
        std::cout << "ystep          : " << cfg.ystep << "\n";
        std::cout << "chlim          : " << cfg.chlim << "\n";
        std::cout << "chstep         : " << cfg.chstep << "\n";
        std::cout << "til_xlim       : " << cfg.til_xlim << "\n";
        std::cout << "til_xstep      : " << cfg.til_xstep << "\n";
        std::cout << "til_ylim       : " << cfg.til_ylim << "\n";
        std::cout << "til_ystep      : " << cfg.til_ystep << "\n";
        std::cout << "dil_pat        : 0x" << std::hex << cfg.dil_pat << std::dec << "\n";
        std::cout << "rows_active    : 0x" << std::hex << cfg.rows_active << std::dec << "\n";

        std::cout << "\nWEIGHT\n";
        std::cout << "wlim           : " << cfg.wlim << "\n";
        std::cout << "wstep          : " << cfg.wstep << "\n";
        std::cout << "klim           : " << cfg.klim << "\n";
        std::cout << "kstep          : " << cfg.kstep << "\n";
        std::cout << "til_klim       : " << cfg.til_klim << "\n";
        std::cout << "til_kstep      : " << cfg.til_kstep << "\n";
        std::cout << "cols_active    : 0x" << std::hex << cfg.cols_active << std::dec << "\n";
        std::cout << "waligned       : " << cfg.waligned << "\n";

        std::cout << "\nOUTPUT\n";
        std::cout << "ncontexts      : " << cfg.ncontexts << "\n";
        std::cout << "cxlim          : " << cfg.cxlim << "\n";
        std::cout << "cxstep         : " << cfg.cxstep << "\n";
        std::cout << "cklim          : " << cfg.cklim << "\n";
        std::cout << "ckstep         : " << cfg.ckstep << "\n";
        std::cout << "til_cylim      : " << cfg.til_cylim << "\n";
        std::cout << "til_cystep     : " << cfg.til_cystep << "\n";
        std::cout << "til_cklim      : " << cfg.til_cklim << "\n";
        std::cout << "til_ckstep     : " << cfg.til_ckstep << "\n";
        std::cout << "inactive_cols  : " << cfg.inactive_cols << "\n";
        std::cout << "preload_en     : " << cfg.preload_en << "\n";

        std::cout << "=========================================\n\n";
    }

    void write_reg32_all(uint32_t addr, uint32_t value, const std::string &name = "")
    {
        host_data_t data;
        host_mask_t mask;

        data.data.fill(0.0f);
        mask.data.fill(false);

        data[0] = static_cast<float>(value);
        mask[0] = true;

        wait();

        // STD
        o_host_addr_std.write(addr);
        o_host_wdata_std.write(data);
        o_host_wmask_std.write(mask);
        o_host_wren_std.write(true);
        o_host_rden_std.write(false);

        // APPROX
        o_host_addr_approx.write(addr);
        o_host_wdata_approx.write(data);
        o_host_wmask_approx.write(mask);
        o_host_wren_approx.write(true);
        o_host_rden_approx.write(false);

        // GATED
        o_host_addr_gated.write(addr);
        o_host_wdata_gated.write(data);
        o_host_wmask_gated.write(mask);
        o_host_wren_gated.write(true);
        o_host_rden_gated.write(false);

        wait();

        host_mask_t zero_mask;
        zero_mask.data.fill(false);

        o_host_wren_std.write(false);
        o_host_wren_approx.write(false);
        o_host_wren_gated.write(false);

        o_host_wmask_std.write(zero_mask);
        o_host_wmask_approx.write(zero_mask);
        o_host_wmask_gated.write(zero_mask);

        wait();

        if (!name.empty())
        {
            std::cout << "[APPLY CFG] " << name
                      << " addr=0x" << std::hex << addr
                      << " value=0x" << value
                      << std::dec << " (" << value << ")"
                      << std::endl;
        }
    }

    // rows_active must be written BYTE-SPREAD: 8 active-row bits per 32-bit host
    // lane (row i -> lane i/8, bit i%8), matching config_regs decode. Writing the
    // raw 32-bit mask into lane 0 as a float loses precision for masks >= 2^24
    // (e.g. 0xFFFFFFFF -> 0) and only sets lane-0's mask, so rows 8..31 keep their
    // reset default. Packing 4 bytes across 4 lanes (each <=255, float-exact) fixes it.
    void write_rows_active_all(uint32_t addr, uint32_t mask32, const std::string &name = "")
    {
        host_data_t data;
        host_mask_t mask;
        data.data.fill(0.0f);
        mask.data.fill(false);
        for (int k = 0; k < 4; k++)
        {
            data[k] = static_cast<float>((mask32 >> (k * 8)) & 0xFF);
            mask[k] = true;
        }

        wait();

        o_host_addr_std.write(addr);    o_host_wdata_std.write(data);    o_host_wmask_std.write(mask);    o_host_wren_std.write(true);    o_host_rden_std.write(false);
        o_host_addr_approx.write(addr); o_host_wdata_approx.write(data); o_host_wmask_approx.write(mask); o_host_wren_approx.write(true); o_host_rden_approx.write(false);
        o_host_addr_gated.write(addr);  o_host_wdata_gated.write(data);  o_host_wmask_gated.write(mask);  o_host_wren_gated.write(true);  o_host_rden_gated.write(false);

        wait();

        host_mask_t zero_mask;
        zero_mask.data.fill(false);
        o_host_wren_std.write(false);   o_host_wren_approx.write(false);   o_host_wren_gated.write(false);
        o_host_wmask_std.write(zero_mask); o_host_wmask_approx.write(zero_mask); o_host_wmask_gated.write(zero_mask);

        wait();

        if (!name.empty())
        {
            std::cout << "[APPLY CFG] " << name
                      << " addr=0x" << std::hex << addr
                      << " mask=0x" << mask32
                      << std::dec << " (byte-spread)"
                      << std::endl;
        }
    }

    void apply_decoded_config_to_npu(const DecodedSauriaConfig &cfg)
    {
        std::cout << "\n=========================================\n";
        std::cout << "APPLY DECODED SAURIA CONFIG TO SYSTEMC NPU\n";
        std::cout << "=========================================\n";

        // ---------------------------------------------------------
        // CONTROL / REUSE
        // ---------------------------------------------------------
        uint32_t sysc_act_read_count = cfg.incntlim + 1;
        write_reg32_all(CFG_CON_OFFSET + 0x00, sysc_act_read_count, "CON.INCNTLIM");
        write_reg32_all(CFG_CON_OFFSET + 0x04, cfg.act_reps, "CON.ACT_REPS");
        write_reg32_all(CFG_CON_OFFSET + 0x08, cfg.wei_reps, "CON.WEI_REPS");

        // ---------------------------------------------------------
        // ACTIVATION / IFMAP FEEDER
        // SystemC feeder hiện đang dùng:
        // ACT_INCNTLIM, ACT_INCNTSTEP, DIL_PAT, ROWS_ACTIVE
        // ---------------------------------------------------------
        write_rows_active_all(CFG_ACT_OFFSET + 0x00,
                              static_cast<uint32_t>(cfg.rows_active),
                              "ACT.ROWS_ACTIVE");

        write_reg32_all(CFG_ACT_OFFSET + 0x04,
                        sysc_act_read_count,
                        "ACT.INCNTLIM");

        write_reg32_all(CFG_ACT_OFFSET + 0x08,
                        cfg.xstep,
                        "ACT.INCNTSTEP");

        // Hai register này hiện chưa functional đầy đủ, nhưng ghi để log/tracking
        write_reg32_all(CFG_ACT_OFFSET + 0x0C,
                        cfg.xlim,
                        "ACT.OUTCNTLIM");

        write_reg32_all(CFG_ACT_OFFSET + 0x10,
                        cfg.xstep,
                        "ACT.OUTCNTSTEP");

        // Hiện ConfigRegs của bạn mới nhận 32-bit thấp của DIL_PAT.
        // Với test d=1 thì low32 = 0 cũng không sao nếu feeder xem all-zero là allow-all.
        write_reg32_all(CFG_ACT_OFFSET + 0x28,
                        static_cast<uint32_t>(cfg.dil_pat & 0xFFFFFFFFULL),
                        "ACT.DIL_PAT_LOW32");

        // Full SAURIA activation address generator parameters
        write_reg32_all(CFG_ACT_OFFSET + 0x14, cfg.xlim, "ACT.XLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x18, cfg.xstep, "ACT.XSTEP");
        write_reg32_all(CFG_ACT_OFFSET + 0x1C, cfg.ylim, "ACT.YLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x20, cfg.ystep, "ACT.YSTEP");
        write_reg32_all(CFG_ACT_OFFSET + 0x24, cfg.chlim, "ACT.CHLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x2C, cfg.chstep, "ACT.CHSTEP");

        write_reg32_all(CFG_ACT_OFFSET + 0x30, cfg.til_xlim, "ACT.TIL_XLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x34, cfg.til_xstep, "ACT.TIL_XSTEP");
        write_reg32_all(CFG_ACT_OFFSET + 0x38, cfg.til_ylim, "ACT.TIL_YLIM");
        write_reg32_all(CFG_ACT_OFFSET + 0x3C, cfg.til_ystep, "ACT.TIL_YSTEP");
        // ---------------------------------------------------------
        // WEIGHT FEEDER
        // ---------------------------------------------------------
        write_reg32_all(CFG_WEI_OFFSET + 0x04,
                        cfg.wlim,
                        "WEI.INCNTLIM");

        write_reg32_all(CFG_WEI_OFFSET + 0x08,
                        cfg.wstep,
                        "WEI.INCNTSTEP");
        // Full SAURIA weight address-generator parameters
        write_reg32_all(CFG_WEI_OFFSET + 0x10, cfg.wlim, "WEI.WLIM");
        write_reg32_all(CFG_WEI_OFFSET + 0x14, cfg.wstep, "WEI.WSTEP");
        write_reg32_all(CFG_WEI_OFFSET + 0x18, cfg.klim, "WEI.KLIM");
        write_reg32_all(CFG_WEI_OFFSET + 0x1C, cfg.kstep, "WEI.KSTEP");

        write_reg32_all(CFG_WEI_OFFSET + 0x20, cfg.til_klim, "WEI.TIL_KLIM");
        write_reg32_all(CFG_WEI_OFFSET + 0x24, cfg.til_kstep, "WEI.TIL_KSTEP");

        write_reg32_all(CFG_WEI_OFFSET + 0x28, cfg.cols_active, "WEI.COLS_ACTIVE");
        write_reg32_all(CFG_WEI_OFFSET + 0x2C, cfg.waligned, "WEI.WALIGNED");
        // ---------------------------------------------------------
        // PSM / OUTPUT
        // ---------------------------------------------------------
        write_reg32_all(CFG_OUT_OFFSET + 0x04,
                        cfg.cxlim,
                        "OUT.CXLIM");

        write_reg32_all(CFG_OUT_OFFSET + 0x08,
                        cfg.cxstep,
                        "OUT.CXSTEP");

        write_reg32_all(CFG_OUT_OFFSET + 0x0C,
                        cfg.cklim,
                        "OUT.CKLIM");

        write_reg32_all(CFG_OUT_OFFSET + 0x10,
                        cfg.ckstep,
                        "OUT.CKSTEP");

        // Full SAURIA output / PSM schedule parameters
        write_reg32_all(CFG_OUT_OFFSET + 0x00, cfg.ncontexts, "OUT.NCONTEXTS");
        write_reg32_all(CFG_OUT_OFFSET + 0x14, cfg.til_cylim, "OUT.TIL_CYLIM");
        write_reg32_all(CFG_OUT_OFFSET + 0x18, cfg.til_cystep, "OUT.TIL_CYSTEP");
        write_reg32_all(CFG_OUT_OFFSET + 0x1C, cfg.til_cklim, "OUT.TIL_CKLIM");
        write_reg32_all(CFG_OUT_OFFSET + 0x20, cfg.til_ckstep, "OUT.TIL_CKSTEP");
        write_reg32_all(CFG_OUT_OFFSET + 0x24, cfg.inactive_cols, "OUT.INACTIVE_COLS");
        write_reg32_all(CFG_OUT_OFFSET + 0x28, cfg.preload_en, "OUT.PRELOAD_EN");
        // ---------------------------------------------------------
        // BASE ADDRESS
        // Quan trọng: args[18..20] là DRAM base của Sauria controller.
        // TB hiện đã preload dữ liệu trực tiếp vào SRAMA/SRAMB/SRAMC từ offset 0,
        // nên runtime base nội bộ của NPU để 0 trước.
        // ---------------------------------------------------------
        write_reg32_all(CFG_ACT_BASE_ADDR, 0, "ACT.BASE_ADDR");
        write_reg32_all(CFG_WEI_BASE_ADDR, 0, "WEI.BASE_ADDR");
        write_reg32_all(CFG_OUT_BASE_ADDR, 0, "OUT.BASE_ADDR");

        std::cout << "=========================================\n\n";
    }

    std::vector<uint8_t> load_initial_dram_file(const std::string &path)
    {
        std::vector<uint8_t> dram;

        std::ifstream file(path);
        if (!file.is_open())
        {
            std::cerr << "[TB ERROR] Cannot open initial DRAM file: "
                      << path << std::endl;
            return dram;
        }

        std::string token;

        while (file >> token)
        {
            uint32_t value = 0;

            try
            {
                value = static_cast<uint32_t>(std::stoul(token, nullptr, 16));
            }
            catch (...)
            {
                std::cerr << "[TB WARNING] Invalid DRAM token: "
                          << token << std::endl;
                continue;
            }

            dram.push_back(static_cast<uint8_t>(value & 0xFF));
        }

        std::cout << "[TB] Loaded initial_dram.txt: "
                  << dram.size()
                  << " bytes"
                  << std::endl;

        return dram;
    }

    void print_dram_bytes(const std::vector<uint8_t> &dram, uint32_t base, uint32_t nbytes, const std::string &name)
    {
        std::cout << "\n[DRAM DEBUG] " << name
                  << " base=0x" << std::hex << base
                  << std::dec
                  << " nbytes=" << nbytes
                  << std::endl;

        if (base >= dram.size())
        {
            std::cout << "  [OUT OF RANGE] base >= dram.size()" << std::endl;
            return;
        }

        uint32_t end = std::min<uint32_t>(base + nbytes, dram.size());

        for (uint32_t addr = base; addr < end; addr++)
        {
            if ((addr - base) % 16 == 0)
            {
                std::cout << "\n  0x"
                          << std::hex << std::setw(8) << std::setfill('0')
                          << addr
                          << " : "
                          << std::dec;
            }

            std::cout << std::hex
                      << std::setw(2) << std::setfill('0')
                      << static_cast<uint32_t>(dram[addr])
                      << " "
                      << std::dec;
        }

        std::cout << std::setfill(' ') << "\n"
                  << std::endl;
    }

    // Write helper
    void write_mem(int profile, uint32_t addr, const host_data_t &data)
    {
        host_mask_t mask;
        mask.data.fill(true);
        wait();
        if (profile == 0)
        {
            o_host_addr_std.write(addr);
            o_host_wdata_std.write(data);
            o_host_wmask_std.write(mask);
            o_host_wren_std.write(true);
            o_host_rden_std.write(false);
        }
        else if (profile == 1)
        {
            o_host_addr_approx.write(addr);
            o_host_wdata_approx.write(data);
            o_host_wmask_approx.write(mask);
            o_host_wren_approx.write(true);
            o_host_rden_approx.write(false);
        }
        else
        {
            o_host_addr_gated.write(addr);
            o_host_wdata_gated.write(data);
            o_host_wmask_gated.write(mask);
            o_host_wren_gated.write(true);
            o_host_rden_gated.write(false);
        }
        wait();
        o_host_wren_std.write(false);
        o_host_wren_approx.write(false);
        o_host_wren_gated.write(false);
        wait(); // Ensure data resolves
    }

    static std::vector<uint32_t> load_shape_u32(const std::string &path)
    {
        std::ifstream f(path);
        std::vector<uint32_t> shape;

        if (!f.is_open())
        {
            std::cerr << "[TB WARN] Cannot open shape file: " << path << std ::endl;
            return shape;
        }

        int64_t v = 0;
        while (f >> v)
        {
            if (v > 0)
                shape.push_back(static_cast<uint32_t>(v));
        }
        return shape;
    }

    uint32_t offset3_perm(const std::string &order, int a0, int a1, int a2, int d0, int d1, int d2)
    {
        std::map<char, int> idx;
        std::map<char, int> dim;

        idx[order[0]] = a0;
        idx[order[1]] = a1;
        idx[order[2]] = a2;

        dim[order[0]] = d0;
        dim[order[1]] = d1;
        dim[order[2]] = d2;

        uint32_t off = 0;
        uint32_t stride = 1;

        for (int p = 2; p >= 0; p--)
        {
            char c = order[p];
            off += idx[c] * stride;
            stride *= dim[c];
        }

        return off;
    }

    uint32_t offset4_perm(const std::string &order, int a0, int a1, int a2, int a3, int d0, int d1, int d2, int d3)
    {
        std::map<char, int> idx;
        std::map<char, int> dim;

        idx[order[0]] = a0;
        idx[order[1]] = a1;
        idx[order[2]] = a2;
        idx[order[3]] = a3;

        dim[order[0]] = d0;
        dim[order[1]] = d1;
        dim[order[2]] = d2;
        dim[order[3]] = d3;

        uint32_t off = 0;
        uint32_t stride = 1;

        for (int p = 3; p >= 0; p--)
        {
            char c = order[p];
            off += idx[c] * stride;
            stride *= dim[c];
        }

        return off;
    }

    int32_t ref_conv_with_layout(const std::vector<uint8_t> &dram, uint32_t act_base, uint32_t wei_base, int oc, int oy, int ox, const std::string &act_layout, const std::string &wei_layout)
    {
        const int C = 64;
        const int H = 6;
        const int W = 10;
        const int O = 16;
        const int KH = 3;
        const int KW = 3;

        int32_t acc = 0;

        for (int ic = 0; ic < C; ic++)
        {
            for (int ky = 0; ky < KH; ky++)
            {
                for (int kx = 0; kx < KW; kx++)
                {
                    uint32_t act_off = 0;

                    if (act_layout == "CHW")
                    {
                        act_off = (ic * H * W) + ((oy + ky) * W) + (ox + kx);
                    }
                    else if (act_layout == "HWC")
                    {
                        act_off = ((oy + ky) * W * C) + ((ox + kx) * C) + ic;
                    }
                    else if (act_layout == "HCW")
                    {
                        act_off = ((oy + ky) * C * W) + (ic * W) + (ox + kx);
                    }
                    else if (act_layout == "WCH")
                    {
                        act_off = ((ox + kx) * C * H) + (ic * H) + (oy + ky);
                    }
                    else if (act_layout == "WHC")
                    {
                        act_off = ((ox + kx) * H * C) + ((oy + ky) * C) + ic;
                    }
                    else
                    {
                        act_off = (ic * H * W) + ((oy + ky) * W) + (ox + kx);
                    }

                    uint32_t wei_off = 0;

                    // order letters:
                    // O = output channel
                    // I = input channel
                    // Y = kernel y
                    // X = kernel x
                    std::map<char, int> wi;
                    std::map<char, int> wd;

                    wi['O'] = oc;
                    wi['I'] = ic;
                    wi['Y'] = ky;
                    wi['X'] = kx;

                    wd['O'] = O;
                    wd['I'] = C;
                    wd['Y'] = KH;
                    wd['X'] = KW;

                    uint32_t stride = 1;
                    for (int p = 3; p >= 0; p--)
                    {
                        char c = wei_layout[p];
                        wei_off += wi[c] * stride;
                        stride *= wd[c];
                    }

                    int32_t a = read_i8_from_dram(dram, act_base, act_off);
                    int32_t w = read_i8_from_dram(dram, wei_base, wei_off);

                    acc += a * w;
                }
            }
        }

        return acc;
    }

    uint32_t output_index_with_layout(int oc, int oy, int ox, const std::string &out_layout)
    {
        const int O = 16;
        const int OH = 4;
        const int OW = 8;

        std::map<char, int> oi;
        std::map<char, int> od;

        oi['O'] = oc;
        oi['Y'] = oy;
        oi['X'] = ox;

        od['O'] = O;
        od['Y'] = OH;
        od['X'] = OW;

        uint32_t off = 0;
        uint32_t stride = 1;

        for (int p = 2; p >= 0; p--)
        {
            char c = out_layout[p];
            off += oi[c] * stride;
            stride *= od[c];
        }

        return off;
    }

    void brute_force_conv_layout_vs_gold_delta(const std::vector<uint8_t> &initial_dram, const std::vector<int32_t> &gold_delta, uint32_t act_base, uint32_t wei_base)
    {
        std::vector<std::string> act_layouts = {
            "CHW", "HWC", "HCW", "WCH", "WHC"};

        std::vector<std::string> wei_layouts = {
            "OIYX", "OIXY", "OYXI", "OXYI",
            "IOYX", "IOXY", "IYXO", "IXYO",
            "YXIO", "XYIO", "YXO I"};

        // Fix typo-safe list manually
        wei_layouts = {
            "OIYX", "OIXY", "OYIX", "OXYI",
            "IOYX", "IOXY", "IYOX", "IXOY",
            "YXIO", "XYIO", "YIOX", "XIOY"};

        std::vector<std::string> out_layouts = {
            "OYX", "OXY", "YOX", "YXO", "XOY", "XYO"};

        const int O = 16;
        const int OH = 4;
        const int OW = 8;

        struct Result
        {
            std::string act;
            std::string wei;
            std::string out;
            uint32_t exact;
            uint32_t close1k;
        };

        std::vector<Result> results;

        for (const auto &al : act_layouts)
        {
            for (const auto &wl : wei_layouts)
            {
                for (const auto &ol : out_layouts)
                {
                    uint32_t exact = 0;
                    uint32_t close1k = 0;

                    for (int oc = 0; oc < O; oc++)
                    {
                        for (int oy = 0; oy < OH; oy++)
                        {
                            for (int ox = 0; ox < OW; ox++)
                            {
                                int32_t ref =
                                    ref_conv_with_layout(
                                        initial_dram,
                                        act_base,
                                        wei_base,
                                        oc,
                                        oy,
                                        ox,
                                        al,
                                        wl);

                                uint32_t idx =
                                    output_index_with_layout(
                                        oc,
                                        oy,
                                        ox,
                                        ol);

                                int32_t gold =
                                    (idx < gold_delta.size()) ? gold_delta[idx] : 0;

                                if (ref == gold)
                                {
                                    exact++;
                                }

                                int64_t diff =
                                    static_cast<int64_t>(ref) -
                                    static_cast<int64_t>(gold);

                                if (diff < 0)
                                    diff = -diff;

                                if (diff <= 1000)
                                {
                                    close1k++;
                                }
                            }
                        }
                    }

                    results.push_back({al, wl, ol, exact, close1k});
                }
            }
        }

        std::sort(
            results.begin(),
            results.end(),
            [](const Result &a, const Result &b)
            {
                if (a.exact != b.exact)
                    return a.exact > b.exact;
                return a.close1k > b.close1k;
            });

        //     std::cout << "\n=========================================\n";
        //     std::cout << "BRUTE FORCE CONV LAYOUT VS GOLD_DELTA\n";
        //     std::cout << "=========================================\n";

        //     for (size_t i = 0; i < results.size() && i < 20; i++)
        //     {
        //         std::cout << "rank " << i
        //                   << " act=" << results[i].act
        //                   << " wei=" << results[i].wei
        //                   << " out=" << results[i].out
        //                   << " exact=" << results[i].exact
        //                   << " close1k=" << results[i].close1k
        //                   << "\n";
        //     }

        //     std::cout << "=========================================\n\n";
        //
    }

    void preload_int8_region_to_srama(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] ACT -> SRAMA, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t loaded = 0;

        for (uint32_t phys_addr = 0; loaded < nbytes; phys_addr++)
        {
            for (int sw = 0; sw < subwords_a && loaded < nbytes; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && loaded < nbytes; i++)
                {
                    uint32_t dram_idx = dram_base + loaded;

                    if (dram_idx < dram.size())
                    {
                        pkt[i] = static_cast<float>(
                            static_cast<int8_t>(dram[dram_idx]));
                    }

                    loaded++;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_srama_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] ACT loaded bytes: "
                  << loaded << std::endl;
    }

    void preload_int8_region_to_sramb(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] WEI -> SRAMB, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t loaded = 0;

        for (uint32_t phys_addr = 0; loaded < nbytes; phys_addr++)
        {
            for (int sw = 0; sw < subwords_b && loaded < nbytes; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && loaded < nbytes; i++)
                {
                    uint32_t dram_idx = dram_base + loaded;

                    if (dram_idx < dram.size())
                    {
                        pkt[i] = static_cast<float>(
                            static_cast<int8_t>(dram[dram_idx]));
                    }

                    loaded++;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_sramb_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] WEI loaded bytes: "
                  << loaded << std::endl;
    }

    void preload_int32_region_to_sramc(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] PSUM -> SRAMC, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t loaded_bytes = 0;
        uint32_t total_elems = nbytes / 4;
        uint32_t elem_idx = 0;

        for (uint32_t phys_addr = 0; elem_idx < total_elems; phys_addr++)
        {
            for (int sw = 0; sw < subwords_c && elem_idx < total_elems; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && elem_idx < total_elems; i++)
                {
                    uint32_t byte_idx = dram_base + elem_idx * 4;

                    int32_t val = 0;

                    if (byte_idx + 3 < dram.size())
                    {
                        val =
                            (static_cast<uint8_t>(dram[byte_idx + 0]) << 0) |
                            (static_cast<uint8_t>(dram[byte_idx + 1]) << 8) |
                            (static_cast<uint8_t>(dram[byte_idx + 2]) << 16) |
                            (static_cast<uint8_t>(dram[byte_idx + 3]) << 24);
                    }

                    pkt[i] = static_cast<float>(val);

                    elem_idx++;
                    loaded_bytes += 4;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_sramc_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] PSUM loaded bytes: "
                  << loaded_bytes << std::endl;
    }

    // INT16 (2 bytes/elem, signed two's complement) preload variants. Mirror the
    // int8 preloads but read 16-bit signed elements.
    void preload_int16_region_to_srama(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] ACT(INT16) -> SRAMA, base=0x"
                  << std::hex << dram_base << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t n_elems = nbytes / 2;
        uint32_t elem = 0, loaded_bytes = 0;
        for (uint32_t phys_addr = 0; elem < n_elems; phys_addr++)
        {
            for (int sw = 0; sw < subwords_a && elem < n_elems; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0);
                for (int i = 0; i < 4 && elem < n_elems; i++)
                {
                    uint32_t byte_idx = dram_base + elem * 2;
                    int16_t v = static_cast<int16_t>(read_u16_le_from_bytes(dram, byte_idx));
                    pkt[i] = static_cast<double>(v);
                    elem++;
                    loaded_bytes += 2;
                }
                for (int p = 0; p < 3; p++)
                    write_mem(p, get_srama_addr(phys_addr, sw), pkt);
            }
        }
        std::cout << "[TB PRELOAD] ACT(INT16) loaded bytes: " << loaded_bytes << std::endl;
    }

    void preload_int16_region_to_sramb(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] WEI(INT16) -> SRAMB, base=0x"
                  << std::hex << dram_base << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t n_elems = nbytes / 2;
        uint32_t elem = 0, loaded_bytes = 0;
        for (uint32_t phys_addr = 0; elem < n_elems; phys_addr++)
        {
            for (int sw = 0; sw < subwords_b && elem < n_elems; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0);
                for (int i = 0; i < 4 && elem < n_elems; i++)
                {
                    uint32_t byte_idx = dram_base + elem * 2;
                    int16_t v = static_cast<int16_t>(read_u16_le_from_bytes(dram, byte_idx));
                    pkt[i] = static_cast<double>(v);
                    elem++;
                    loaded_bytes += 2;
                }
                for (int p = 0; p < 3; p++)
                    write_mem(p, get_sramb_addr(phys_addr, sw), pkt);
            }
        }
        std::cout << "[TB PRELOAD] WEI(INT16) loaded bytes: " << loaded_bytes << std::endl;
    }

    // INT16 partial-sum preload: OC_W=64 -> 8 bytes/elem, signed two's complement.
    void preload_int64_region_to_sramc(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] PSUM(INT64) -> SRAMC, base=0x"
                  << std::hex << dram_base << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t total_elems = nbytes / 8;
        uint32_t elem = 0, loaded_bytes = 0;
        for (uint32_t phys_addr = 0; elem < total_elems; phys_addr++)
        {
            for (int sw = 0; sw < subwords_c && elem < total_elems; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0);
                for (int i = 0; i < 4 && elem < total_elems; i++)
                {
                    uint32_t byte_idx = dram_base + elem * 8;
                    int64_t v = read_i64_le_from_bytes(dram, byte_idx);
                    pkt[i] = static_cast<double>(v);
                    elem++;
                    loaded_bytes += 8;
                }
                for (int p = 0; p < 3; p++)
                    write_mem(p, get_sramc_addr(phys_addr, sw), pkt);
            }
        }
        std::cout << "[TB PRELOAD] PSUM(INT64) loaded bytes: " << loaded_bytes << std::endl;
    }

    // FP16 (2 bytes/elem) preload variants. Elements are raw IEEE-754 half bit
    // patterns in DRAM (as written by SAURIA's fh.generate_test_files); decode
    // to float here, then let the host_data_t(float) -> T_ACT/T_WEI(fp16_t)
    // assignment in sram_top.h re-quantize (lossless round-trip, since the
    // decoded value is already exactly representable in half precision).
    void preload_fp16_region_to_srama(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] ACT(FP16) -> SRAMA, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t n_elems = nbytes / 2;
        uint32_t elem = 0;
        uint32_t loaded_bytes = 0;

        for (uint32_t phys_addr = 0; elem < n_elems; phys_addr++)
        {
            for (int sw = 0; sw < subwords_a && elem < n_elems; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && elem < n_elems; i++)
                {
                    uint32_t byte_idx = dram_base + elem * 2;
                    uint16_t raw = read_u16_le_from_bytes(dram, byte_idx);
                    pkt[i] = fp16_t::half_to_float(raw);
                    elem++;
                    loaded_bytes += 2;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_srama_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] ACT(FP16) loaded bytes: " << loaded_bytes << std::endl;
    }

    void preload_fp16_region_to_sramb(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] WEI(FP16) -> SRAMB, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t n_elems = nbytes / 2;
        uint32_t elem = 0;
        uint32_t loaded_bytes = 0;

        for (uint32_t phys_addr = 0; elem < n_elems; phys_addr++)
        {
            for (int sw = 0; sw < subwords_b && elem < n_elems; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && elem < n_elems; i++)
                {
                    uint32_t byte_idx = dram_base + elem * 2;
                    uint16_t raw = read_u16_le_from_bytes(dram, byte_idx);
                    pkt[i] = fp16_t::half_to_float(raw);
                    elem++;
                    loaded_bytes += 2;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_sramb_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] WEI(FP16) loaded bytes: " << loaded_bytes << std::endl;
    }

    void preload_fp16_region_to_sramc(const std::vector<uint8_t> &dram, uint32_t dram_base, uint32_t nbytes)
    {
        std::cout << "[TB PRELOAD] PSUM(FP16) -> SRAMC, base=0x"
                  << std::hex << dram_base
                  << std::dec << ", bytes=" << nbytes << std::endl;

        uint32_t n_elems = nbytes / 2;
        uint32_t elem = 0;
        uint32_t loaded_bytes = 0;

        for (uint32_t phys_addr = 0; elem < n_elems; phys_addr++)
        {
            for (int sw = 0; sw < subwords_c && elem < n_elems; sw++)
            {
                host_data_t pkt;
                pkt.data.fill(0.0f);

                for (int i = 0; i < 4 && elem < n_elems; i++)
                {
                    uint32_t byte_idx = dram_base + elem * 2;
                    uint16_t raw = read_u16_le_from_bytes(dram, byte_idx);
                    pkt[i] = fp16_t::half_to_float(raw);
                    elem++;
                    loaded_bytes += 2;
                }

                for (int p = 0; p < 3; p++)
                {
                    write_mem(p, get_sramc_addr(phys_addr, sw), pkt);
                }
            }
        }

        std::cout << "[TB PRELOAD] PSUM(FP16) loaded bytes: " << loaded_bytes << std::endl;
    }

    int32_t ref_conv_chw_oihw(const std::vector<uint8_t> &dram, uint32_t act_base, uint32_t wei_base, int oc, int oy, int ox)
    {
        const int C = 64;
        const int H = 6;
        const int W = 10;
        const int KH = 3;
        const int KW = 3;

        int32_t acc = 0;

        for (int ic = 0; ic < C; ic++)
        {
            for (int ky = 0; ky < KH; ky++)
            {
                for (int kx = 0; kx < KW; kx++)
                {
                    uint32_t act_off =
                        ic * H * W +
                        (oy + ky) * W +
                        (ox + kx);

                    uint32_t wei_off =
                        ((oc * C + ic) * KH + ky) * KW + kx;

                    int32_t a = read_i8_from_dram(dram, act_base, act_off);
                    int32_t w = read_i8_from_dram(dram, wei_base, wei_off);

                    acc += a * w;
                }
            }
        }

        return acc;
    }

    int8_t read_i8_from_dram(const std::vector<uint8_t> &dram, uint32_t base, uint32_t offset)
    {
        uint32_t addr = base + offset;

        if (addr >= dram.size())
        {
            return 0;
        }

        return static_cast<int8_t>(dram[addr]);
    }

    int32_t read_i32_le_from_bytes(const std::vector<uint8_t> &mem, uint32_t byte_addr)
    {
        if (byte_addr + 3 >= mem.size())
        {
            return 0;
        }

        uint32_t v =
            (static_cast<uint32_t>(mem[byte_addr + 0]) << 0) |
            (static_cast<uint32_t>(mem[byte_addr + 1]) << 8) |
            (static_cast<uint32_t>(mem[byte_addr + 2]) << 16) |
            (static_cast<uint32_t>(mem[byte_addr + 3]) << 24);

        return static_cast<int32_t>(v);
    }

    uint32_t read_u32_le_from_bytes(const std::vector<uint8_t> &mem, uint32_t byte_addr)
    {
        if (byte_addr + 3 >= mem.size())
        {
            return 0;
        }

        return (static_cast<uint32_t>(mem[byte_addr + 0]) << 0) |
               (static_cast<uint32_t>(mem[byte_addr + 1]) << 8) |
               (static_cast<uint32_t>(mem[byte_addr + 2]) << 16) |
               (static_cast<uint32_t>(mem[byte_addr + 3]) << 24);
    }

    uint16_t read_u16_le_from_bytes(const std::vector<uint8_t> &mem, uint32_t byte_addr)
    {
        if (byte_addr + 1 >= mem.size())
        {
            return 0;
        }

        return static_cast<uint16_t>(
            (static_cast<uint32_t>(mem[byte_addr + 0]) << 0) |
            (static_cast<uint32_t>(mem[byte_addr + 1]) << 8));
    }

    int64_t read_i64_le_from_bytes(const std::vector<uint8_t> &mem, uint32_t byte_addr)
    {
        if (byte_addr + 7 >= mem.size())
        {
            return 0;
        }

        uint64_t v = 0;
        for (int i = 0; i < 8; i++)
        {
            v |= static_cast<uint64_t>(mem[byte_addr + i]) << (8 * i);
        }
        return static_cast<int64_t>(v);
    }

    std::vector<int32_t> load_gold_delta_output_i32(const std::vector<uint8_t> &initial_dram, const std::vector<uint8_t> &gold_dram, uint32_t out_dram_base, uint32_t n_elems)
    {
        std::vector<int32_t> delta;
        delta.reserve(n_elems);

        for (uint32_t i = 0; i < n_elems; i++)
        {
            uint32_t byte_addr = out_dram_base + i * 4;

            uint32_t init_raw = read_u32_le_from_bytes(initial_dram, byte_addr);
            uint32_t gold_raw = read_u32_le_from_bytes(gold_dram, byte_addr);

            // Two's-complement subtraction. This matches int32 accumulator behavior.
            uint32_t delta_raw = gold_raw - init_raw;

            delta.push_back(static_cast<int32_t>(delta_raw));
        }

        return delta;
    }

    std::vector<int32_t> load_gold_output_i32(const std::vector<uint8_t> &gold_dram, uint32_t out_dram_base, uint32_t n_elems)
    {
        std::vector<int32_t> gold;
        gold.reserve(n_elems);

        for (uint32_t i = 0; i < n_elems; i++)
        {
            uint32_t byte_addr = out_dram_base + i * 4;
            gold.push_back(read_i32_le_from_bytes(gold_dram, byte_addr));
        }

        return gold;
    }

    std::vector<int32_t> dump_sramc_linear_i32(uint32_t n_elems)
    {
        std::vector<int32_t> out;
        out.reserve(n_elems);

        const uint32_t elems_per_sramc_addr = subwords_c * 4;
        uint32_t n_phys_addr =
            (n_elems + elems_per_sramc_addr - 1) / elems_per_sramc_addr;

        for (uint32_t phys_addr = 0; phys_addr < n_phys_addr; phys_addr++)
        {
            for (int sw = 0; sw < subwords_c; sw++)
            {
                uint32_t host_addr = get_sramc_addr(phys_addr, sw);
                host_data_t data = read_mem(0, host_addr);

                for (int lane = 0; lane < 4; lane++)
                {
                    if (out.size() < n_elems)
                    {
                        out.push_back(static_cast<int32_t>(data[lane]));
                    }
                }
            }
        }

        return out;
    }

    std::vector<int32_t> dump_sramc_psm_layout_i32(const DecodedSauriaConfig &cfg)
    {
        std::vector<int32_t> out;

        uint32_t ncontexts = cfg.ncontexts;
        if (ncontexts == 0)
        {
            ncontexts = 1;
        }

        uint32_t vectors_per_context = cfg.cxlim;
        uint32_t vector_step = cfg.cxstep;
        uint32_t context_addr_stride = cfg.cxlim * cfg.cxstep;

        uint32_t expected_elems = cfg.til_cklim;
        if (expected_elems == 0)
        {
            expected_elems = ncontexts * vectors_per_context * EVAL_Y;
        }

        out.reserve(expected_elems);

        std::cout << "\n=========================================\n";
        std::cout << "DUMP SRAMC USING PSM LAYOUT\n";
        std::cout << "=========================================\n";
        std::cout << "ncontexts          : " << ncontexts << "\n";
        std::cout << "vectors/context    : " << vectors_per_context << "\n";
        std::cout << "vector_step        : " << vector_step << "\n";
        std::cout << "context_addr_stride: " << context_addr_stride << "\n";
        std::cout << "expected_elems     : " << expected_elems << "\n";

        for (uint32_t ctx = 0; ctx < ncontexts; ctx++)
        {
            uint32_t ctx_base = ctx * context_addr_stride;

            std::cout << "\n[PSM LAYOUT DUMP] context=" << ctx
                      << " ctx_base=" << ctx_base << "\n";

            for (uint32_t v = 0; v < vectors_per_context; v++)
            {
                uint32_t phys_addr = ctx_base + v * vector_step;

                if (ctx < 4 && v < 4)
                {
                    std::cout << "  vector=" << v
                              << " phys_addr=" << phys_addr
                              << " data=[";
                }

                for (int sw = 0; sw < subwords_c; sw++)
                {
                    uint32_t host_addr = get_sramc_addr(phys_addr, sw);
                    host_data_t data = read_mem(0, host_addr);

                    for (int lane = 0; lane < 4; lane++)
                    {
                        if (out.size() < expected_elems)
                        {
                            int32_t val = static_cast<int32_t>(data[lane]);
                            out.push_back(val);

                            if (ctx < 4 && v < 4)
                            {
                                std::cout << val;
                                if (!(sw == subwords_c - 1 && lane == 3))
                                {
                                    std::cout << ", ";
                                }
                            }
                        }
                    }
                }

                if (ctx < 4 && v < 4)
                {
                    std::cout << "]\n";
                }
            }
        }

        std::cout << "Dumped elements     : " << out.size() << "\n";
        std::cout << "=========================================\n\n";

        return out;
    }

    float read_f32_le_from_bytes(const std::vector<uint8_t> &mem, uint32_t byte_addr)
    {
        if (byte_addr + 3 >= mem.size())
        {
            return 0.0f;
        }

        uint32_t raw =
            (static_cast<uint32_t>(mem[byte_addr + 0]) << 0) |
            (static_cast<uint32_t>(mem[byte_addr + 1]) << 8) |
            (static_cast<uint32_t>(mem[byte_addr + 2]) << 16) |
            (static_cast<uint32_t>(mem[byte_addr + 3]) << 24);

        float f;
        std::memcpy(&f, &raw, sizeof(float));
        return f;
    }

    void compare_sramc_with_gold(const std::vector<int32_t> &sramc_out, const std::vector<int32_t> &gold_out)
    {
        uint32_t n = std::min(sramc_out.size(), gold_out.size());
        uint32_t mismatches = 0;

        std::cout << "\n=========================================\n";
        std::cout << "SRAMC RAW OUTPUT VS GOLD_DRAM COMPARE\n";
        std::cout << "=========================================\n";

        for (uint32_t i = 0; i < n; i++)
        {
            if (sramc_out[i] != gold_out[i])
            {
                if (mismatches < 32)
                {
                    std::cout << "Mismatch[" << i << "] "
                              << "SRAMC=" << sramc_out[i]
                              << " GOLD=" << gold_out[i]
                              << std::endl;
                }
                mismatches++;
            }
        }

        std::cout << "Compared elements : " << n << std::endl;
        std::cout << "Mismatches        : " << mismatches << std::endl;

        if (mismatches == 0)
        {
            std::cout << "RAW COMPARE PASS" << std::endl;
        }
        else
        {
            std::cout << "RAW COMPARE FAIL / layout may need reorder" << std::endl;
        }

        std::cout << "=========================================\n\n";
    }

    std::vector<host_data_t> snapshot_sramc_words(uint32_t n_words, const std::string &tag)
    {
        std::vector<host_data_t> snapshot;

        std::cout << "\n=========================================\n";
        std::cout << "SRAMC SNAPSHOT: " << tag << "\n";
        std::cout << "=========================================\n";

        for (uint32_t i = 0; i < n_words; i++)
        {
            uint32_t addr = get_sramc_addr(i, 0);
            host_data_t data = read_mem(0, addr);
            snapshot.push_back(data);

            if (i < 16)
            {
                std::cout << "SRAMC word " << i
                          << " addr=0x" << std::hex << addr << std::dec
                          << " : ";

                for (int j = 0; j < 4; j++)
                {
                    std::cout << data[j] << " ";
                }

                std::cout << "\n";
            }
        }

        std::cout << "=========================================\n\n";

        return snapshot;
    }

    void compare_sramc_snapshots(const std::vector<host_data_t> &before, const std::vector<host_data_t> &after)
    {
        uint32_t changed_words = 0;

        std::cout << "\n=========================================\n";
        std::cout << "SRAMC BEFORE / AFTER CHANGE CHECK\n";
        std::cout << "=========================================\n";

        uint32_t n = std::min(before.size(), after.size());

        for (uint32_t i = 0; i < n; i++)
        {
            bool changed = false;

            for (int j = 0; j < 4; j++)
            {
                float diff = before[i][j] - after[i][j];
                if (diff < 0)
                    diff = -diff;

                if (diff > 1e-3f)
                {
                    changed = true;
                }
            }

            if (changed)
            {
                changed_words++;

                if (changed_words <= 32)
                {
                    std::cout << "SRAMC word " << i << " CHANGED\n";

                    std::cout << "  before = [";
                    for (int j = 0; j < 4; j++)
                    {
                        std::cout << before[i][j];
                        if (j < 3)
                            std::cout << ", ";
                    }
                    std::cout << "]\n";

                    std::cout << "  after  = [";
                    for (int j = 0; j < 4; j++)
                    {
                        std::cout << after[i][j];
                        if (j < 3)
                            std::cout << ", ";
                    }
                    std::cout << "]\n";
                }
            }
        }

        std::cout << "Changed SRAMC words: " << changed_words << "\n";
        std::cout << "=========================================\n\n";
    }

    // Read helper
    host_data_t read_mem(int profile, uint32_t addr)
    {
        host_data_t zero_data;
        host_mask_t zero_mask;

        zero_data.data.fill(0.0f);
        zero_mask.data.fill(false);

        if (profile == 0)
        {
            o_host_addr_std.write(addr);
            o_host_wren_std.write(false);
            o_host_rden_std.write(true);
            o_host_wdata_std.write(zero_data);
            o_host_wmask_std.write(zero_mask);
        }
        else if (profile == 1)
        {
            o_host_addr_approx.write(addr);
            o_host_wren_approx.write(false);
            o_host_rden_approx.write(true);
            o_host_wdata_approx.write(zero_data);
            o_host_wmask_approx.write(zero_mask);
        }
        else
        {
            o_host_addr_gated.write(addr);
            o_host_wren_gated.write(false);
            o_host_rden_gated.write(true);
            o_host_wdata_gated.write(zero_data);
            o_host_wmask_gated.write(zero_mask);
        }

        // SRAM/host read path is registered.
        // First wait: address/rden sampled.
        // Second wait: rdata for this address becomes valid.
        wait();
        wait();

        host_data_t result;

        if (profile == 0)
        {
            result = i_host_rdata_std.read();
            o_host_rden_std.write(false);
        }
        else if (profile == 1)
        {
            result = i_host_rdata_approx.read();
            o_host_rden_approx.write(false);
        }
        else
        {
            result = i_host_rdata_gated.read();
            o_host_rden_gated.write(false);
        }

        wait();

        return result;
    }

    void analyze_value_match_between_sramc_and_gold(const std::vector<int32_t> &sramc_out, const std::vector<int32_t> &gold_out)
    {
        std::unordered_map<int32_t, std::vector<uint32_t>> gold_pos;

        for (uint32_t i = 0; i < gold_out.size(); i++)
        {
            gold_pos[gold_out[i]].push_back(i);
        }

        uint32_t nonzero_sramc = 0;
        uint32_t matched_values = 0;
        uint32_t printed = 0;

        std::cout << "\n=========================================\n";
        std::cout << "SRAMC VALUE SEARCH IN GOLD_DRAM\n";
        std::cout << "=========================================\n";

        for (uint32_t i = 0; i < sramc_out.size(); i++)
        {
            int32_t v = sramc_out[i];

            // Skip zero because zero is usually too common and not useful
            if (v == 0)
                continue;

            nonzero_sramc++;

            auto it = gold_pos.find(v);

            if (it != gold_pos.end())
            {
                matched_values++;

                if (printed < 32)
                {
                    std::cout << "SRAMC[" << i << "] = " << v
                              << " found in GOLD at index ";

                    for (size_t k = 0; k < it->second.size() && k < 8; k++)
                    {
                        std::cout << it->second[k];
                        if (k + 1 < it->second.size() && k < 7)
                        {
                            std::cout << ", ";
                        }
                    }

                    std::cout << "\n";
                    printed++;
                }
            }
        }

        std::cout << "Non-zero SRAMC values : " << nonzero_sramc << "\n";
        std::cout << "Matched in GOLD       : " << matched_values << "\n";

        if (nonzero_sramc == 0)
        {
            std::cout << "Result: SRAMC output is all zero or not dumped correctly.\n";
        }
        else if (matched_values > 0)
        {
            std::cout << "Result: Some values match. Likely layout/reorder issue.\n";
        }
        else
        {
            std::cout << "Result: No value match. Likely compute/datapath mismatch.\n";
        }

        std::cout << "=========================================\n\n";
    }

    void debug_reference_conv_vs_gold_delta(const std::vector<uint8_t> &initial_dram, const std::vector<int32_t> &gold_delta, uint32_t act_base, uint32_t wei_base)
    {
        std::cout << "\n=========================================\n";
        std::cout << "REFERENCE CONV CHW/OIHW VS GOLD_DELTA\n";
        std::cout << "=========================================\n";

        const int OC = 16;
        const int OH = 4;
        const int OW = 8;

        uint32_t mismatches = 0;
        uint32_t printed = 0;

        for (int oc = 0; oc < OC; oc++)
        {
            for (int oy = 0; oy < OH; oy++)
            {
                for (int ox = 0; ox < OW; ox++)
                {
                    uint32_t idx =
                        (oc * OH * OW) +
                        (oy * OW) +
                        ox;

                    int32_t ref =
                        ref_conv_chw_oihw(
                            initial_dram,
                            act_base,
                            wei_base,
                            oc,
                            oy,
                            ox);

                    int32_t gold =
                        (idx < gold_delta.size()) ? gold_delta[idx] : 0;

                    if (ref != gold)
                    {
                        mismatches++;

                        if (printed < 32)
                        {
                            std::cout << "idx=" << idx
                                      << " oc=" << oc
                                      << " oy=" << oy
                                      << " ox=" << ox
                                      << " ref=" << ref
                                      << " gold_delta=" << gold
                                      << " diff=" << (ref - gold)
                                      << "\n";
                            printed++;
                        }
                    }
                }
            }
        }

        std::cout << "Compared elements : " << (OC * OH * OW) << "\n";
        std::cout << "Mismatches        : " << mismatches << "\n";

        if (mismatches == 0)
        {
            std::cout << "REF CONV PASS: gold_delta matches standard CHW/OIHW convolution.\n";
        }
        else
        {
            std::cout << "REF CONV FAIL: SAURIA data layout is not simple CHW/OIHW, or output order differs.\n";
        }

        std::cout << "=========================================\n\n";
    }

    void debug_gold_delta_preview(const std::vector<int32_t> &delta, uint32_t n = 32)
    {
        std::cout << "\n=========================================\n";
        std::cout << "GOLD DELTA PREVIEW: gold_dram - initial_dram\n";
        std::cout << "=========================================\n";

        for (uint32_t i = 0; i < delta.size() && i < n; i++)
        {
            std::cout << "delta[" << i << "] = " << delta[i] << "\n";
        }

        std::cout << "=========================================\n\n";
    }

    void debug_dram_output_region_sanity(const std::vector<uint8_t> &initial_dram, const std::vector<uint8_t> &gold_dram, uint32_t out_base, uint32_t nbytes)
    {
        std::cout << "\n=========================================\n";
        std::cout << "GOLD_DRAM OUTPUT REGION SANITY CHECK\n";
        std::cout << "=========================================\n";

        std::cout << "OUT base : 0x" << std::hex << out_base << std::dec << "\n";
        std::cout << "nbytes   : " << nbytes << "\n";
        std::cout << "initial_dram size : " << initial_dram.size() << "\n";
        std::cout << "gold_dram size    : " << gold_dram.size() << "\n";

        if (out_base + nbytes > initial_dram.size() ||
            out_base + nbytes > gold_dram.size())
        {
            std::cout << "[ERROR] OUT region out of range.\n";
            std::cout << "=========================================\n\n";
            return;
        }

        uint32_t changed_bytes = 0;

        for (uint32_t i = 0; i < nbytes; i++)
        {
            if (initial_dram[out_base + i] != gold_dram[out_base + i])
            {
                changed_bytes++;
            }
        }

        std::cout << "Changed bytes in OUT region: "
                  << changed_bytes << " / " << nbytes << "\n";

        std::cout << "\nFirst 64 bytes at OUT region\n";
        std::cout << "INITIAL: ";
        for (uint32_t i = 0; i < 64 && i < nbytes; i++)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<uint32_t>(initial_dram[out_base + i])
                      << " ";
        }

        std::cout << "\nGOLD   : ";
        for (uint32_t i = 0; i < 64 && i < nbytes; i++)
        {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<uint32_t>(gold_dram[out_base + i])
                      << " ";
        }
        std::cout << std::dec << std::setfill(' ') << "\n";

        std::cout << "\nInterpret GOLD first 16 elements:\n";
        for (uint32_t i = 0; i < 16; i++)
        {
            uint32_t byte_addr = out_base + i * 4;

            int32_t as_i32 = read_i32_le_from_bytes(gold_dram, byte_addr);
            float as_f32 = read_f32_le_from_bytes(gold_dram, byte_addr);

            std::cout << "gold[" << i << "] "
                      << "bytes=["
                      << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<uint32_t>(gold_dram[byte_addr + 0]) << " "
                      << static_cast<uint32_t>(gold_dram[byte_addr + 1]) << " "
                      << static_cast<uint32_t>(gold_dram[byte_addr + 2]) << " "
                      << static_cast<uint32_t>(gold_dram[byte_addr + 3]) << std::dec
                      << std::setfill(' ')
                      << "] i32=" << as_i32
                      << " f32=" << as_f32
                      << "\n";
        }

        std::cout << "=========================================\n\n";
    }

    void debug_readback_sram_regions()
    {
        std::cout << "\n=========================================\n";
        std::cout << "SRAM READ-BACK DEBUG AFTER PRELOAD\n";
        std::cout << "=========================================\n";

        std::cout << "\n[SRAMA READBACK]\n";
        for (int i = 0; i < 4; i++)
        {
            uint32_t addr = get_srama_addr(i, 0);
            host_data_t data = read_mem(0, addr);

            std::cout << "SRAMA addr=0x" << std::hex << addr << std::dec << " : ";
            for (int j = 0; j < 4; j++)
            {
                std::cout << data[j] << " ";
            }
            std::cout << "\n";
        }

        std::cout << "\n[SRAMB READBACK]\n";
        for (int i = 0; i < 4; i++)
        {
            uint32_t addr = get_sramb_addr(i, 0);
            host_data_t data = read_mem(0, addr);

            std::cout << "SRAMB addr=0x" << std::hex << addr << std::dec << " : ";
            for (int j = 0; j < 4; j++)
            {
                std::cout << data[j] << " ";
            }
            std::cout << "\n";
        }

        std::cout << "\n[SRAMC READBACK]\n";
        for (int i = 0; i < 4; i++)
        {
            uint32_t addr = get_sramc_addr(i, 0);
            host_data_t data = read_mem(0, addr);

            std::cout << "SRAMC addr=0x" << std::hex << addr << std::dec << " : ";
            for (int j = 0; j < 4; j++)
            {
                std::cout << data[j] << " ";
            }
            std::cout << "\n";
        }

        std::cout << "=========================================\n\n";
    }

    void debug_output_sramc_after_run()
    {
        std::cout << "\n=========================================\n";
        std::cout << "SRAMC OUTPUT READBACK AFTER NPU RUN\n";
        std::cout << "=========================================\n";

        for (int i = 0; i < 16; i++)
        {
            uint32_t addr = get_sramc_addr(i, 0);
            host_data_t data = read_mem(0, addr);

            std::cout << "SRAMC addr=0x"
                      << std::hex << addr
                      << std::dec << " : ";

            for (int j = 0; j < 4; j++)
            {
                std::cout << data[j] << " ";
            }

            std::cout << "\n";
        }

        std::cout << "=========================================\n\n";
    }

    uint64_t read_packed_bits(const std::vector<uint32_t> &args, uint32_t start_word_idx, size_t &bitpos, uint32_t width)
    {
        uint64_t value = 0;

        for (uint32_t b = 0; b < width; b++)
        {
            size_t abs_bit = bitpos + b;
            uint32_t word_idx = start_word_idx + static_cast<uint32_t>(abs_bit / 32);
            uint32_t bit_idx = static_cast<uint32_t>(abs_bit % 32);

            uint64_t bit_val = (args[word_idx] >> bit_idx) & 0x1ULL;
            value |= (bit_val << b);
        }

        bitpos += width;
        return value;
    }

    void align_to_next_word(size_t &bitpos)
    {
        if (bitpos % 32 != 0)
        {
            bitpos = ((bitpos / 32) + 1) * 32;
        }
    }

    // -------------------------------------------------------------------------
    // df_controller emulation: external-tile loop + per-tile DMA (multi-tile).
    //
    // SAURIA splits the output into external tiles handled by `df_controller`
    // (OUTSIDE the core). Each external tile == one single-tile core run (the
    // case that already passes, "bài A"). This function reproduces df_controller:
    //   for each tile (enumerated by the pointer generator in loop_order):
    //     1. gather A/B from DRAM -> SRAM (contiguous local) at the tile offset,
    //     2. run the core for `ncontexts` contexts,
    //     3. scatter the tile's SRAM C back to DRAM at the psums tile offset.
    // Then compare the full DRAM C region against gold_dram.
    //
    // Pointer/DMA semantics ported from:
    //   RTL/src/df_controller/sauria_dma_pointer_generator.sv
    //   RTL/src/df_controller/sauria_dma_controller.sv (set_A/B/C_params)
    //   RTL/src/df_controller/sauria_interface.sv (derived dma.psums/weights)
    //   Python/src/config_helper.py get_controller_regs (args[0..21] layout)
    // -------------------------------------------------------------------------
    void run_tiles_and_verify(const std::vector<uint32_t> &controller_args,
                              const DecodedSauriaConfig &cfg,
                              uint32_t baseA,
                              uint32_t baseB,
                              uint32_t baseC)
    {
        // Ensure trace dir exists so per-tile dumps don't fail silently.
        if (std::system("mkdir -p trace_sysc") != 0)
        {
            std::cout << "[TB WARN] could not create trace_sysc/ (dumps may be skipped)\n";
        }

        // Element sizes (df_ctrl_pkg: IA_W/8, IB_W/8, OC_W/8 -> int8/int8/int32).
        const uint32_t A_BYTES = sizeof(NPU_DTYPE_IN);
        const uint32_t B_BYTES = sizeof(NPU_DTYPE_IN);
        const uint32_t C_BYTES = sizeof(NPU_DTYPE_OUT);

        // --- Tiling params from controller args[0..21] (already captured) ---
        const uint32_t x_lim = controller_args[0] & 0xFFFF;
        const uint32_t y_lim = (controller_args[0] >> 16) & 0xFFFF;
        const uint32_t c_lim = controller_args[1] & 0xFFFF;
        const uint32_t k_lim = (controller_args[1] >> 16) & 0xFFFF;

        const uint32_t t_psums_x = controller_args[2];
        const uint32_t t_psums_y = controller_args[3];
        const uint32_t t_psums_k = controller_args[4];
        const uint32_t t_ifm_x = controller_args[5];
        const uint32_t t_ifm_y = controller_args[6];
        const uint32_t t_ifm_c = controller_args[7];
        const uint32_t t_wei_k = controller_args[8];
        const uint32_t t_wei_c = controller_args[9];

        const uint32_t dma_ifm_y_lim = controller_args[10];
        const uint32_t dma_ifm_c_lim = controller_args[11];
        const uint32_t dma_psums_y_step = controller_args[12];
        const uint32_t dma_psums_k_step = controller_args[13];
        const uint32_t dma_ifm_y_step = controller_args[14];
        const uint32_t dma_ifm_c_step = controller_args[15];
        const uint32_t dma_wei_w_step = controller_args[16];
        const uint32_t dma_ifm_ett = controller_args[17];

        const uint32_t loop_order = (controller_args[21] >> 16) & 0x3;
        const bool Cw_eq = ((controller_args[21] >> 23) & 0x1) != 0;
        const bool Ch_eq = ((controller_args[21] >> 24) & 0x1) != 0;
        const bool Ck_eq = ((controller_args[21] >> 25) & 0x1) != 0;
        const bool WXfer = ((controller_args[21] >> 31) & 0x1) != 0;

        // --- Derived DMA params (sauria_interface.sv lines 223-239) ---
        const uint32_t dma_psums_k_lim =
            WXfer ? (dma_wei_w_step - 1) : (t_wei_k - 1);
        const uint32_t dma_psums_y_lim = t_psums_y - dma_psums_y_step;
        const uint32_t dma_wei_w_lim =
            WXfer ? 1u : (t_wei_c - dma_wei_w_step);
        const uint32_t dma_psums_ett =
            (Cw_eq && Ch_eq) ? t_psums_k : (Cw_eq ? t_psums_y : t_psums_x);
        const uint32_t dma_wei_ett = Ck_eq ? t_wei_c : t_wei_k;

        uint32_t ncontexts = cfg.ncontexts ? cfg.ncontexts : 1u;
        uint32_t cxlim = cfg.cxlim;
        // Per-context reduction depth K. decoded incntlim = B_w*B_h*AB_c - 1.
        uint32_t mvm_k = cfg.incntlim + 1;
        bool preload_en = (cfg.preload_en != 0);

        uint64_t rows_mask = cfg.rows_active;
        if (rows_mask == 0)
        {
            rows_mask = (EVAL_Y >= 64) ? ~0ULL : ((1ULL << EVAL_Y) - 1ULL);
        }

        const uint32_t n_tiles =
            (x_lim + 1) * (y_lim + 1) * (c_lim + 1) * (k_lim + 1);

        std::cout << "\n=========================================\n";
        std::cout << "DF_CONTROLLER TILE LOOP (multi-tile)\n";
        std::cout << "=========================================\n";
        std::cout << "loop_order=" << loop_order
                  << " Cw_eq=" << Cw_eq << " Ch_eq=" << Ch_eq
                  << " Ck_eq=" << Ck_eq << " WXfer=" << WXfer << "\n";
        std::cout << "tile lims  x=" << x_lim << " y=" << y_lim
                  << " c=" << c_lim << " k=" << k_lim
                  << "  -> n_tiles=" << n_tiles << "\n";
        std::cout << "ncontexts=" << ncontexts << " cxlim=" << cxlim
                  << " mvm_k=" << mvm_k << " preload_en=" << preload_en << "\n";
        std::cout << "dma ifmaps ett=" << dma_ifm_ett
                  << " y_lim=" << dma_ifm_y_lim << " c_lim=" << dma_ifm_c_lim
                  << " y_step=" << dma_ifm_y_step << " c_step=" << dma_ifm_c_step << "\n";
        std::cout << "dma weights ett=" << dma_wei_ett
                  << " w_lim=" << dma_wei_w_lim << " w_step=" << dma_wei_w_step << "\n";
        std::cout << "dma psums  ett=" << dma_psums_ett
                  << " y_lim=" << dma_psums_y_lim << " k_lim=" << dma_psums_k_lim
                  << " y_step=" << dma_psums_y_step << " k_step=" << dma_psums_k_step << "\n";

        // Working DRAM copies (mutated by C writeback). Init from initial_dram.
        std::vector<uint8_t> dram_std =
            load_initial_dram_file("stimuli/initial_dram.txt");
        std::vector<uint8_t> dram_approx = dram_std;
        std::vector<uint8_t> gold =
            load_initial_dram_file("stimuli/gold_dram.txt");

        auto rdbyte = [](const std::vector<uint8_t> &m, uint32_t a) -> uint8_t
        {
            return (a < m.size()) ? m[a] : (uint8_t)0;
        };
        auto wr_i32 = [](std::vector<uint8_t> &m, uint32_t a, int32_t v)
        {
            if (a + 3 >= m.size())
                return;
            uint32_t u = (uint32_t)v;
            m[a + 0] = (uint8_t)(u >> 0);
            m[a + 1] = (uint8_t)(u >> 8);
            m[a + 2] = (uint8_t)(u >> 16);
            m[a + 3] = (uint8_t)(u >> 24);
        };

        // Pointer-generator state (sauria_dma_pointer_generator.sv).
        uint32_t px = 0, py = 0, pc = 0, pk = 0;
        uint32_t ifx = 0, ify = 0, ifc = 0;
        uint32_t psx = 0, psy = 0, psk = 0;
        uint32_t wec = 0, wek = 0;

        for (uint32_t tile = 0; tile < n_tiles; tile++)
        {
            uint32_t ifmap_off = ifx + ify + ifc;
            uint32_t wei_off = wek + wec;
            uint32_t psums_off = psx + psy + psk;

            std::cout << "\n[TILE " << tile << "/" << n_tiles << "]"
                      << " px=" << px << " py=" << py << " pc=" << pc << " pk=" << pk
                      << " ifmap_off=" << ifmap_off
                      << " wei_off=" << wei_off
                      << " psums_off=" << psums_off << std::endl;

            // ---- GATHER A (contiguous local layout) ----
            std::vector<uint8_t> tileA;
            for (uint32_t z = 0; z <= dma_ifm_c_lim; z++)
            {
                for (uint32_t yy = 0; yy <= dma_ifm_y_lim; yy++)
                {
                    uint32_t src_elem =
                        ifmap_off + yy * dma_ifm_y_step + z * dma_ifm_c_step;
                    uint32_t src = baseA + src_elem * A_BYTES;
                    for (uint32_t e = 0; e < dma_ifm_ett * A_BYTES; e++)
                        tileA.push_back(rdbyte(dram_std, src + e));
                }
            }

            // ---- GATHER B (set_B_params: ylim = Ck_eq ? 0 : w_lim) ----
            std::vector<uint8_t> tileB;
            uint32_t wei_y_lim = Ck_eq ? 0u : dma_wei_w_lim;
            for (uint32_t yy = 0; yy <= wei_y_lim; yy++)
            {
                uint32_t src_elem = wei_off + yy * dma_wei_w_step;
                uint32_t src = baseB + src_elem * B_BYTES;
                for (uint32_t e = 0; e < dma_wei_ett * B_BYTES; e++)
                    tileB.push_back(rdbyte(dram_std, src + e));
            }

            // ---- Per-tile DEBUG dumps (gathered inputs + derived params) ----
            // These let an external (Python) reference recompute the exact tile
            // conv from the SAME data the core received, to localize compute bugs.
            {
                uint32_t A_w = dma_ifm_y_step;                            // = A_w (DMA y_step)
                uint32_t A_h_til = (A_w != 0) ? dma_ifm_c_step / A_w : 0; // A_h*A_w / A_w
                uint32_t d_dil = (A_w != 0) ? cfg.ystep / A_w : 0;        // ystep = A_w*d
                uint32_t s_str = (A_w != 0) ? cfg.til_ystep / A_w : 0;    // til_ystep = A_w*s
                uint32_t c_til_n = (A_w != 0 && A_h_til != 0) ? dma_ifm_ett / (A_w * A_h_til) : 0;
                uint32_t kern_elems = (c_til_n != 0) ? mvm_k / c_til_n : 0; // = B_h*B_w

                std::ofstream fa("trace_sysc/tile" + std::to_string(tile) + "_A.txt");
                for (size_t e = 0; e < tileA.size(); e++)
                    fa << (int)(int8_t)tileA[e] << "\n";
                fa.close();

                std::ofstream fb("trace_sysc/tile" + std::to_string(tile) + "_B.txt");
                for (size_t e = 0; e < tileB.size(); e++)
                    fb << (int)(int8_t)tileB[e] << "\n";
                fb.close();

                std::ofstream fm("trace_sysc/tile" + std::to_string(tile) + "_meta.txt");
                fm << "tile " << tile << "\n";
                fm << "A_w " << A_w << "\n";
                fm << "A_h_til " << A_h_til << "\n";
                fm << "c_til " << c_til_n << "\n";
                fm << "k_til " << cxlim << "\n";
                fm << "kernel_elems " << kern_elems << "\n";
                fm << "stride " << s_str << "\n";
                fm << "dilation " << d_dil << "\n";
                fm << "ncontexts " << ncontexts << "\n";
                fm << "Y_used " << EVAL_Y << "\n";
                fm << "psums_off " << psums_off << "\n";
                fm.close();
            }

            // ---- Preload SRAM (host owns buffer 0) ----
            o_select.write(sc_bv<3>("000"));
            wait(2);
            preload_int8_region_to_srama(tileA, 0, (uint32_t)tileA.size());
            preload_int8_region_to_sramb(tileB, 0, (uint32_t)tileB.size());

            // ---- Run core for this tile (NPU owns buffer 0) ----
            o_total_contexts.write(ncontexts);
            o_mvm_k.write(mvm_k);
            o_select.write(sc_bv<3>("111"));
            wait(2);

            o_start_std.write(true);
            o_start_approx.write(true);
            o_start_gated.write(true);
            wait(2);
            o_start_std.write(false);
            o_start_approx.write(false);
            o_start_gated.write(false);

            bool done = false;
            for (int c = 0; c < 20000; c++)
            {
                wait();
                if (i_done_std.read())
                {
                    done = true;
                    break;
                }
            }
            if (!done)
            {
                std::cout << "[TILE " << tile << "] WARNING: core timeout" << std::endl;
            }

            // ---- Read back SRAM C and scatter to DRAM at psums tile offset ----
            o_select.write(sc_bv<3>("000"));
            wait(2);

            std::ofstream cf("trace_sysc/tile" + std::to_string(tile) + "_compute.csv");
            cf << "x,ctx,y,compute\n";

            for (uint32_t x = 0; x < cxlim; x++)
            {
                for (uint32_t ctx = 0; ctx < ncontexts; ctx++)
                {
                    // SRAM C local element address (same as single-tile path):
                    //   base_elem = x*(ncontexts*Y) + ctx*Y
                    uint32_t base_elem = x * (ncontexts * EVAL_Y) + ctx * EVAL_Y;

                    for (int sw = 0; sw < subwords_c; sw++)
                    {
                        host_data_t vs = read_mem(0, get_sramc_addr(base_elem, sw));
                        host_data_t va = read_mem(1, get_sramc_addr(base_elem, sw));

                        for (int lane = 0; lane < 4; lane++)
                        {
                            uint32_t y = (uint32_t)(sw * 4 + lane);
                            if (y >= (uint32_t)EVAL_Y)
                                continue;
                            if (((rows_mask >> y) & 0x1ULL) == 0)
                                continue;

                            // Conv-output element address in DRAM:
                            //   psums_off + k_local*dma_psums_k_step
                            //             + h_local*dma_psums_y_step + w_local
                            //   (k_local=x=out-channel, h_local=ctx, w_local=y)
                            uint32_t delem = psums_off +
                                             x * dma_psums_k_step +
                                             ctx * dma_psums_y_step + y;
                            uint32_t baddr = baseC + delem * C_BYTES;

                            int32_t cs = (int32_t)vs[lane];
                            int32_t ca = (int32_t)va[lane];

                            cf << x << "," << ctx << "," << y << "," << cs << "\n";

                            if (preload_en)
                            {
                                // Accumulate onto running DRAM value (preload base
                                // for first write; partial-sum accumulation across
                                // reduction (c) tiles).
                                wr_i32(dram_std, baddr,
                                       wrap_add_i32(read_i32_le_from_bytes(dram_std, baddr), cs));
                                wr_i32(dram_approx, baddr,
                                       wrap_add_i32(read_i32_le_from_bytes(dram_approx, baddr), ca));
                            }
                            else
                            {
                                wr_i32(dram_std, baddr, cs);
                                wr_i32(dram_approx, baddr, ca);
                            }
                        }
                    }
                }
            }

            // ---- Advance pointer generator to next tile (loop_order) ----
            if (tile + 1 >= n_tiles)
                break;

            bool o0 = (px == x_lim), o1 = (py == y_lim);
            bool o2 = (pc == c_lim), o3 = (pk == k_lim);

            bool spatial_cond, c_cond, k_cond;
            if (loop_order == 0)
            {
                spatial_cond = true;
                c_cond = o0 && o1;
                k_cond = c_cond && o2;
            }
            else if (loop_order == 1)
            {
                c_cond = true;
                k_cond = o2;
                spatial_cond = k_cond && o3;
            }
            else
            {
                k_cond = true;
                c_cond = o3;
                spatial_cond = c_cond && o2;
            }

            if (spatial_cond)
            {
                if (o0)
                {
                    px = 0;
                    ifx = 0;
                    psx = 0;
                }
                else
                {
                    px++;
                    ifx += t_ifm_x;
                    psx += t_psums_x;
                }
                if (o0)
                {
                    if (o1)
                    {
                        py = 0;
                        ify = 0;
                        psy = 0;
                    }
                    else
                    {
                        py++;
                        ify += t_ifm_y;
                        psy += t_psums_y;
                    }
                }
            }
            if (c_cond)
            {
                if (o2)
                {
                    pc = 0;
                    ifc = 0;
                    wec = 0;
                }
                else
                {
                    pc++;
                    ifc += t_ifm_c;
                    wec += t_wei_c;
                }
            }
            if (k_cond)
            {
                if (o3)
                {
                    pk = 0;
                    psk = 0;
                    wek = 0;
                }
                else
                {
                    pk++;
                    psk += t_psums_k;
                    wek += t_wei_k;
                }
            }
        }

        // ---- Final full-DRAM verification over the C region ----
        uint32_t c_bytes_total =
            (gold.size() > baseC) ? (uint32_t)(gold.size() - baseC) : 0u;
        uint32_t total_c_elements = c_bytes_total / C_BYTES;

        int errors_std = 0;
        int64_t total_approx_error = 0;
        int64_t max_approx_error = 0;

        std::cout << "\n==========================================================================================" << std::endl;
        std::cout << "   VERIFICATION REPORT (multi-tile): NPU FINAL DRAM vs SAURIA gold_dram" << std::endl;
        std::cout << "==========================================================================================" << std::endl;
        std::cout << " Index | Golden Final | NPU Exact Final | NPU Approx Final | Match | APP Error" << std::endl;
        std::cout << "-------+--------------+-----------------+------------------+-------+----------" << std::endl;

        for (uint32_t i = 0; i < total_c_elements; i++)
        {
            uint32_t baddr = baseC + i * C_BYTES;
            int32_t golden_val = read_i32_le_from_bytes(gold, baddr);
            int32_t npu_std = read_i32_le_from_bytes(dram_std, baddr);
            int32_t npu_approx = read_i32_le_from_bytes(dram_approx, baddr);

            bool match_std = (npu_std == golden_val);
            if (!match_std)
                errors_std++;

            int64_t app_error = (int64_t)npu_approx - (int64_t)golden_val;
            int64_t app_error_abs = (app_error < 0) ? -app_error : app_error;
            total_approx_error += app_error_abs;
            if (app_error_abs > max_approx_error)
                max_approx_error = app_error_abs;

            if (!match_std || i < 32 || i + 8 >= total_c_elements)
            {
                std::cout
                    << " " << std::setw(5) << i
                    << " | " << std::setw(12) << golden_val
                    << " | " << std::setw(15) << npu_std
                    << " | " << std::setw(16) << npu_approx
                    << " | " << (match_std ? "PASS " : "FAIL ")
                    << " | " << std::setw(8) << app_error_abs
                    << std::endl;
            }
        }

        std::cout << "==========================================================================================" << std::endl;
        std::cout << "[APPROX METRICS] Total error accumulator: " << total_approx_error << std::endl;
        std::cout << "[APPROX METRICS] Max error per element : " << max_approx_error << std::endl;
        std::cout << "Expected elements : " << total_c_elements << std::endl;
        std::cout << "Mismatches        : " << errors_std << std::endl;

        if (errors_std == 0)
        {
            std::cout << "[RESULT] TEST PASSED: NPU multi-tile DRAM matches SAURIA gold_dram with ZERO errors!" << std::endl;
        }
        else
        {
            std::cout << "[RESULT] TEST FAILED: NPU multi-tile DRAM has "
                      << errors_std << " mismatches with SAURIA gold_dram." << std::endl;
        }
    }

    void test_process()
    {
        // store controller_args
        std::vector<uint32_t> controller_args(256, 0);
        bool controller_started = false;

        DecodedSauriaConfig decoded_cfg_runtime;
        bool decoded_cfg_valid = false;

        uint32_t act_dram_base_runtime = 0;
        uint32_t wei_dram_base_runtime = 0;
        uint32_t out_dram_base_runtime = 0;
        std::cout << "\n=============================================================" << std::endl;
        std::cout << "      SAURIA SystemC NPU Core Multi-Profile Evaluation       " << std::endl;
        std::cout << "=============================================================\n"
                  << std::endl;

        // Reset system
        o_rstn.write(false);
        o_soft_reset.write(false);
        o_start_std.write(false);
        o_start_approx.write(false);
        o_start_gated.write(false);
        o_total_contexts.write(1);

        o_host_wren_std.write(false);
        o_host_rden_std.write(false);
        o_host_wren_approx.write(false);
        o_host_rden_approx.write(false);
        o_host_wren_gated.write(false);
        o_host_rden_gated.write(false);

        // Sparsity threshold. 0.5f is used for the INT8 path (equivalent to
        // "exact zero only", since |nonzero int8| >= 1 > 0.5). Real-valued FP16
        // activations/weights routinely fall inside (0, 0.5], so the same
        // constant would zero out legitimate small products; use 0 there.
#ifdef NPU_FP16
        o_threshold.write(0.0f);
#else
        o_threshold.write(0.5f);
#endif
        o_select.write(sc_bv<3>("000")); // Map physical buffer 0 to Host AXI

        wait(5);
        o_rstn.write(true);
        wait(2);

        std::cout << "[TB] Fetching initial_dram.txt into Memory Pre-load..." << std::endl;
        std::ifstream dram_file("stimuli/initial_dram.txt");
        if (!dram_file.is_open())
        {
            std::cerr << "[ERROR] Can't open initial_dram.txt file!" << std::endl;
            sc_stop();
            return;
        }

        std::string line;
        std::vector<NPU_DTYPE_OUT> dram_data;

        while (std::getline(dram_file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back(); // Remove carriage return if present
            // Skip empty lines
            if (line.empty())
                continue;

            try
            {
                int8_t byte_val = static_cast<int8_t>(std::stoul(line, nullptr, 16));
                dram_data.push_back(byte_val);
            }
            catch (...)
            {
                continue;
            }
        }
        dram_file.close();
        std::cout << "[TB] Completed parsing initial_dram.txt with " << dram_data.size() << " floats loaded." << std::endl;

        std::cout << "[TB] Mapping data to internal SRAM A & B..." << std::endl;
        int byte_ptr = 0;

        // Fetch Tensor A data to SRAM A for all 3 profiles (STD, APPROX, GATED)
        for (int phys_addr = 0; phys_addr < 2048 && byte_ptr < A_REGION_BYTES; phys_addr++)
        {
            for (int sw = 0; sw < subwords_a; sw++)
            {
                host_data_t act_pkt;
                for (int i = 0; i < 4; i++)
                {
                    act_pkt[i] = (byte_ptr < (int)dram_data.size()) ? dram_data[byte_ptr++] : NPU_DTYPE_OUT(0);
                }
                for (int p = 0; p < 3; p++)
                    write_mem(p, get_srama_addr(phys_addr, sw), act_pkt);
            }
        }

        // Fetch Tensor B data to SRAM B for all 3 profiles (STD, APPROX, GATED)
        int b_loaded = 0;
        for (int phys_addr = 0; phys_addr < 2048 && b_loaded < B_REGION_BYTES; phys_addr++)
        {
            for (int sw = 0; sw < subwords_b; sw++)
            {
                host_data_t wei_pkt;
                for (int i = 0; i < 4; i++)
                {
                    wei_pkt[i] = (byte_ptr < (int)dram_data.size()) ? dram_data[byte_ptr++] : NPU_DTYPE_OUT(0);
                    b_loaded++;
                }
                for (int p = 0; p < 3; p++)
                    write_mem(p, get_sramb_addr(phys_addr, sw), wei_pkt);
            }
        }
        std::cout << "[TB] Completed memory pre-load from initial_dram.txt." << std::endl;
        std::cout << "[TB] Input activations and weights programmed to double buffers." << std::endl;

        // Read back inputs to verify
        for (int p = 0; p < 3; p++)
        {
            host_data_t read_act0 = read_mem(p, get_srama_addr(0, 0));
            host_data_t read_act1 = read_mem(p, get_srama_addr(0, 1));
            host_data_t read_wei0 = read_mem(p, get_sramb_addr(0, 0));
            host_data_t read_wei1 = read_mem(p, get_sramb_addr(0, 1));
            std::cout << "[TB] Profile " << p << " Readback SRAM A0: [" << read_act0[0] << ", " << read_act0[1] << ", " << read_act0[2] << ", " << read_act0[3] << "]" << std::endl;
            std::cout << "[TB] Profile " << p << " Readback SRAM A1: [" << read_act1[0] << ", " << read_act1[1] << ", " << read_act1[2] << ", " << read_act1[3] << "]" << std::endl;
            std::cout << "[TB] Profile " << p << " Readback SRAM B0: [" << read_wei0[0] << ", " << read_wei0[1] << ", " << read_wei0[2] << ", " << read_wei0[3] << "]" << std::endl;
            std::cout << "[TB] Profile " << p << " Readback SRAM B1: [" << read_wei1[0] << ", " << read_wei1[1] << ", " << read_wei1[2] << ", " << read_wei1[3] << "]" << std::endl;
        }

        // Swap double buffers to NPU side
        wait();
        o_select.write(sc_bv<3>("111"));
        wait(2);

        std::cout << "[TB] Reading and execute instructions set from GoldenStimuli.txt..." << std::endl;
        std::ifstream stim_file("stimuli/GoldenStimuli.txt");
        if (!stim_file.is_open())
        {
            std::cerr << "[ERROR] Can't open GoldenStimuli.txt file!" << std::endl;
            sc_stop();
            return;
        }

        std::string stim_line;
        int command_count = 0;

        while (std::getline(stim_file, stim_line))
        {
            if (stim_line.empty())
                continue;

            std::stringstream ss(stim_line);
            uint64_t data_in_u64, addr_u64, wren_u64, rden_u64, waitflag_u64, data_out_u64, checkflag_u64;

            ss >> std::hex >> data_in_u64 >> addr_u64 >> wren_u64 >> rden_u64 >> waitflag_u64 >> data_out_u64 >> checkflag_u64;

            uint32_t raw_addr = static_cast<uint32_t>(addr_u64);
            uint32_t data_in = static_cast<uint32_t>(data_in_u64);
            bool wren = (wren_u64 == 1);
            bool rden = (rden_u64 == 1);
            StimRegion region = get_stim_region(raw_addr);

            if (is_controller_arg_write(raw_addr, wren))
            {
                uint32_t arg_idx = get_controller_arg_index(raw_addr);

                if (arg_idx < controller_args.size())
                {
                    controller_args[arg_idx] = data_in;
                }

                std::cout << "[CTRL ARG] idx=" << arg_idx
                          << " offset=0x" << std::hex << (raw_addr - 0x40000000)
                          << " data=0x" << data_in
                          << std::dec << std::endl;
            }

            if (is_controller_start_write(raw_addr, data_in, wren))
            {
                controller_started = true;

                std::cout << "\n=========================================\n";
                std::cout << "SAURIA CONTROLLER START DETECTED\n";
                std::cout << "=========================================\n";

                std::cout << "\nController args [0..21]\n";
                for (int i = 0; i < 22; i++)
                {
                    std::cout << "arg[" << i << "] = 0x"
                              << std::hex << controller_args[i]
                              << std::dec << std::endl;
                }

                std::cout << "\nPacked SAURIA accelerator regs args[22..]\n";
                for (int i = 22; i < 64; i++)
                {
                    if (controller_args[i] != 0)
                    {
                        std::cout << "arg[" << i << "] = 0x"
                                  << std::hex << controller_args[i]
                                  << std::dec << std::endl;
                    }
                }

                decoded_cfg_runtime = decode_sauria_packed_config(controller_args);
                decoded_cfg_valid = true;

                act_dram_base_runtime = controller_args[18];
                wei_dram_base_runtime = controller_args[19];
                out_dram_base_runtime = controller_args[20];

                print_decoded_sauria_config(decoded_cfg_runtime);
                apply_decoded_config_to_npu(decoded_cfg_runtime);
                // ---------------------------------------------------------
                // Derive total_contexts and total_k for matrix-level SystemC run.
                // Must be driven BEFORE o_start_* is asserted.
                // ---------------------------------------------------------
                {
                    std::vector<uint8_t> gold_for_shape =
                        load_initial_dram_file("stimuli/gold_dram.txt");

                    const uint32_t bytes_per_out = sizeof(NPU_DTYPE_OUT);
                    const uint32_t out_base = out_dram_base_runtime;

                    uint32_t file_c_bytes = 0;
                    if (gold_for_shape.size() > out_base)
                    {
                        file_c_bytes =
                            static_cast<uint32_t>(gold_for_shape.size() - out_base);
                    }

                    uint32_t total_c_elements =
                        file_c_bytes / bytes_per_out;

                    uint32_t one_output_tile_elements =
                        decoded_cfg_runtime.ncontexts *
                        decoded_cfg_runtime.cxlim *
                        EVAL_Y;

                    uint32_t output_tiles = 1;
                    if (one_output_tile_elements != 0)
                    {
                        output_tiles = (total_c_elements + one_output_tile_elements - 1) / one_output_tile_elements;
                    }

                    uint32_t total_contexts =
                        decoded_cfg_runtime.ncontexts * output_tiles;

                    if (total_contexts == 0)
                    {
                        total_contexts = decoded_cfg_runtime.ncontexts;
                    }

                    if (total_contexts == 0)
                    {
                        total_contexts = 1;
                    }

                    // total_k comes from dumped MVM matrix shape:
                    // A_Mat_mvm shape = (total_contexts, Y, K)
                    // B_Mat_mvm shape = (total_contexts, K, X)
                    // uint32_t total_k = 0;

                    uint32_t mvm_k = 0;

                    std::vector<uint32_t> a_shape =
                        load_shape_u32("/tmp/sauria_A_Mat_mvm_shape.txt");

                    std::vector<uint32_t> b_shape =
                        load_shape_u32("/tmp/sauria_B_Mat_mvm_shape.txt");

                    if (a_shape.size() >= 3)
                    {
                        mvm_k = a_shape[2];
                    }
                    else if (b_shape.size() >= 3)
                    {
                        mvm_k = b_shape[1];
                    }

                    o_total_contexts.write(total_contexts);
                    o_mvm_k.write(mvm_k);
                    wait();

                    std::cout << "[TB DRIVE BEFORE RUN]"
                              << " file_c_bytes=" << file_c_bytes
                              << " total_c_elements=" << total_c_elements
                              << " one_output_tile=" << one_output_tile_elements
                              << " output_tiles=" << output_tiles
                              << " ncontexts=" << decoded_cfg_runtime.ncontexts
                              << " total_contexts=" << total_contexts
                              << " mvm_k=" << mvm_k
                              << std::endl;
                }
                uint32_t act_dram_base = act_dram_base_runtime;
                uint32_t wei_dram_base = wei_dram_base_runtime;
                uint32_t out_dram_base = out_dram_base_runtime;

                std::cout << "\n=========================================\n";
                std::cout << "DRAM BASE ADDRESSES FROM SAURIA CONTROLLER\n";
                std::cout << "=========================================\n";
                std::cout << "ACT DRAM BASE : 0x" << std::hex << act_dram_base << std::dec << "\n";
                std::cout << "WEI DRAM BASE : 0x" << std::hex << wei_dram_base << std::dec << "\n";
                std::cout << "OUT DRAM BASE : 0x" << std::hex << out_dram_base << std::dec << "\n";
                std::cout << "=========================================\n\n";

                std::string initial_dram_path = "stimuli/initial_dram.txt";
                std::vector<uint8_t> dram = load_initial_dram_file(initial_dram_path);

                print_dram_bytes(dram, act_dram_base, 64, "ACTIVATION REGION");
                print_dram_bytes(dram, wei_dram_base, 64, "WEIGHT REGION");
                print_dram_bytes(dram, out_dram_base, 64, "PSUM / OUTPUT REGION");

                uint32_t act_bytes = 0;
                uint32_t wei_bytes = 0;
                uint32_t psum_bytes = 0;

                // The real DRAM layout is:
                //   [ACT region] [WEI region] [OUT/PSUM region]
                //
                // Do NOT use ACT_CHLIM / WEI_WLIM directly as preload byte length.
                // Those are address-generator limits, not full region sizes.
                //
                // Test 2:
                //   ACT = 27300 - 0     = 27300 bytes
                //   WEI = 41700 - 27300 = 14400 bytes
                //   OUT = 44772 - 41700 = 3072 bytes
                if (wei_dram_base > act_dram_base)
                {
                    act_bytes = wei_dram_base - act_dram_base;
                }
                else
                {
                    act_bytes = decoded_cfg_runtime.chlim;
                }

                if (out_dram_base > wei_dram_base)
                {
                    wei_bytes = out_dram_base - wei_dram_base;
                }
                else
                {
                    wei_bytes = decoded_cfg_runtime.wlim;
                }

                if (dram.size() > out_dram_base)
                {
                    psum_bytes = static_cast<uint32_t>(dram.size() - out_dram_base);
                }
                else
                {
                    psum_bytes = decoded_cfg_runtime.til_cklim * 4;
                }

                std::cout << "[TB PRELOAD SIZE]"
                          << " act_bytes=" << act_bytes
                          << " wei_bytes=" << wei_bytes
                          << " psum_bytes=" << psum_bytes
                          << " dram_size=" << dram.size()
                          << std::endl;

                // ---------------------------------------------------------
                // Double-buffer protocol:
                // select=000:
                //   host writes physical buffer 0
                //   NPU will later read physical buffer 0 after select=111.
                //
                // IMPORTANT:
                //   This MUST be set BEFORE preload.
                // ---------------------------------------------------------
                o_select.write(sc_bv<3>("000"));
                wait(2);

                std::cout << "[TB SELECT] Host preload targets physical buffer 0"
                          << std::endl;

#if defined(NPU_FP16)
                preload_fp16_region_to_srama(dram, act_dram_base, act_bytes);

                preload_fp16_region_to_sramb(dram, wei_dram_base, wei_bytes);

                if (decoded_cfg_runtime.preload_en)
                {
                    preload_fp16_region_to_sramc(dram, out_dram_base, psum_bytes);
                }
#elif defined(NPU_INT16)
                preload_int16_region_to_srama(dram, act_dram_base, act_bytes);

                preload_int16_region_to_sramb(dram, wei_dram_base, wei_bytes);

                if (decoded_cfg_runtime.preload_en)
                {
                    preload_int64_region_to_sramc(dram, out_dram_base, psum_bytes);
                }
#else
                preload_int8_region_to_srama(dram, act_dram_base, act_bytes);

                preload_int8_region_to_sramb(dram, wei_dram_base, wei_bytes);

                if (decoded_cfg_runtime.preload_en)
                {
                    preload_int32_region_to_sramc(dram, out_dram_base, psum_bytes);
                }
#endif

                wait(2);

                sramc_before_run = snapshot_sramc_words(
                    256,
                    "BEFORE NPU RUN - HOST READ BUFFER 0");

                // ---------------------------------------------------------
                // select=111 => NPU accesses physical buffer 0
                // ---------------------------------------------------------
                o_select.write(sc_bv<3>("111"));
                wait(2);

                debug_readback_sram_regions();
                std::cout << "=========================================\n\n";
            }

            // Current NpuTop only accepts SAURIA internal/core address space.
            // Skip CONTROLLER/DMA for now.
            if (!is_sauria_core_transaction(raw_addr))
            {
                if (command_count < 100)
                {
                    std::cout << "[TB SKIP] region=" << stim_region_name(region)
                              << " raw_addr=0x" << std::hex << raw_addr
                              << " data=0x" << data_in
                              << std::dec << std::endl;
                }

                command_count++;

                wait();

                if (waitflag_u64 > 0)
                {
                    wait(static_cast<int>(waitflag_u64));
                }

                continue;
            }

            uint32_t internal_addr = normalize_sauria_addr(raw_addr);

            host_data_t stim_packet;
            host_mask_t stim_mask;

            make_stim_packet(
                internal_addr,
                data_in,
                wren,
                stim_packet,
                stim_mask);

            if (wren && is_cfg_addr(internal_addr))
            {
                std::cout << "[TB CFG WRITE] raw_addr=0x"
                          << std::hex << raw_addr
                          << " internal=0x" << internal_addr
                          << " data=0x" << data_in
                          << std::dec << std::endl;
            }

            // STD
            o_host_addr_std.write(internal_addr);
            o_host_wren_std.write(wren);
            o_host_rden_std.write(rden);
            o_host_wdata_std.write(stim_packet);
            o_host_wmask_std.write(stim_mask);

            // APPROX
            o_host_addr_approx.write(internal_addr);
            o_host_wren_approx.write(wren);
            o_host_rden_approx.write(rden);
            o_host_wdata_approx.write(stim_packet);
            o_host_wmask_approx.write(stim_mask);

            // GATED
            o_host_addr_gated.write(internal_addr);
            o_host_wren_gated.write(wren);
            o_host_rden_gated.write(rden);
            o_host_wdata_gated.write(stim_packet);
            o_host_wmask_gated.write(stim_mask);

            wait();
            command_count++;

            host_mask_t zero_mask;
            zero_mask.data.fill(false);

            o_host_wren_std.write(false);
            o_host_rden_std.write(false);
            o_host_wmask_std.write(zero_mask);

            o_host_wren_approx.write(false);
            o_host_rden_approx.write(false);
            o_host_wmask_approx.write(zero_mask);

            o_host_wren_gated.write(false);
            o_host_rden_gated.write(false);
            o_host_wmask_gated.write(zero_mask);

            wait();

            if (waitflag_u64 > 0)
            {
                wait(static_cast<int>(waitflag_u64));
            }
        }

        stim_file.close();
        std::cout << "[TB] Executed " << command_count << " transactions from GoldenStimuli.txt" << std::endl;

        // ---------------------------------------------------------------------
        // MULTI-TILE PATH: emulate SAURIA df_controller (external-tile loop +
        // per-tile DMA reload) and verify against gold_dram. Each tile is one
        // single-tile core run (the path that already passes). This supersedes
        // the legacy single-shot run + reconstructed-index compare below, which
        // only handled one output tile.
        // ---------------------------------------------------------------------
        const bool USE_DF_CONTROLLER_TILE_EMU = false;
        if (USE_DF_CONTROLLER_TILE_EMU && decoded_cfg_valid)
        {
            run_tiles_and_verify(controller_args,
                                 decoded_cfg_runtime,
                                 act_dram_base_runtime,
                                 wei_dram_base_runtime,
                                 out_dram_base_runtime);
            wait(20);
            sc_stop();
            return;
        }

        std::cout << "[TB] Triggering NPU execution..." << std::endl;
        o_start_std.write(true);
        o_start_approx.write(true);
        o_start_gated.write(true);

        wait(2);

        o_start_std.write(false);
        o_start_approx.write(false);
        o_start_gated.write(false);

        std::cout << "[TB] Waiting for NPU to complete execution..." << std::endl;

        bool completed = false;

        for (int cycle = 0; cycle < 20000; cycle++)
        {
            wait();

            if (i_done_std.read())
            {
                completed = true;
                std::cout << "[TB] STD NPU done at cycle " << cycle << std::endl;
                break;
            }
        }

        if (!completed)
        {
            std::cout << "[TB WARNING] STD NPU execution timeout." << std::endl;
        }

        // ---------------------------------------------------------
        // Switch back so host can read physical buffer 0,
        // which was used by NPU during execution.
        // ---------------------------------------------------------
        o_select.write(sc_bv<3>("000"));
        wait(2);
        // ---------------------------------------------------------
        // FINAL DRAM-LEVEL VERIFICATION
        // Compare:
        //   Golden Final C from gold_dram
        // vs
        //   NPU Exact Final C = initial_C + SRAMC_compute if preload_en
        //                    = SRAMC_compute           if !preload_en
        //
        // Important:
        //   SRAMC stores raw compute result from PSM.
        //   SAURIA gold_dram stores final C after preload accumulation.
        // ---------------------------------------------------------
        std::cout << "[RUNTIME CONFIG]\n";
        std::cout << "  ncontexts = " << decoded_cfg_runtime.ncontexts << "\n";
        std::cout << "  cxlim     = " << decoded_cfg_runtime.cxlim << "\n";
        std::cout << "  cklim     = " << decoded_cfg_runtime.cklim << "\n";
        std::cout << "  ckstep    = " << decoded_cfg_runtime.ckstep << "\n";
        std::cout << "  til_cklim = " << decoded_cfg_runtime.til_cklim << "\n";
        std::cout << "  til_ckstep= " << decoded_cfg_runtime.til_ckstep << "\n";
        std::cout << "  act_reps  = " << decoded_cfg_runtime.act_reps << "\n";
        std::cout << "  wei_reps  = " << decoded_cfg_runtime.wei_reps << "\n";
        std::cout << "  out_base  = " << out_dram_base_runtime << "\n";

        std::cout << "[Verification] Reading initial_dram.txt and gold_dram.txt..." << std::endl;

        std::vector<uint8_t> initial_dram_final =
            load_initial_dram_file("stimuli/initial_dram.txt");

        std::vector<uint8_t> gold_dram_final =
            load_initial_dram_file("stimuli/gold_dram.txt");

        if (!decoded_cfg_valid)
        {
            std::cerr << "[TB ERROR] decoded_cfg_runtime is not valid. "
                      << "Cannot perform final DRAM-level verification."
                      << std::endl;
            sc_stop();
            return;
        }

        // Swap double buffers back to Host AXI side so HOST can read SRAM C.
        wait();
        o_select.write(sc_bv<3>("000"));
        wait(2);

        uint32_t ncontexts = decoded_cfg_runtime.ncontexts;
        if (ncontexts == 0)
        {
            ncontexts = 1;
        }

        uint32_t cxlim = decoded_cfg_runtime.cxlim;

        // Prefer til_cklim as total output element count if available.
        const uint32_t bytes_per_out = sizeof(NPU_DTYPE_OUT); // int32_t = 4
        const uint32_t out_base = out_dram_base_runtime;
        uint32_t file_c_bytes = 0;
        if (gold_dram_final.size() > out_base)
        {
            file_c_bytes = static_cast<uint32_t>(gold_dram_final.size() - out_base);
        }

        uint32_t total_c_elements_from_file = file_c_bytes / bytes_per_out;
        uint32_t total_c_elements = total_c_elements_from_file;
        // FIX: decoded o_cxlim (= Y_used + SRAMC_N) is NOT the output-x count.
        // Derive verification tiling from the actual element count and the SAURIA
        // gold layout (elem = x*(ncontexts*EVAL_Y) + ctx*EVAL_Y + y): treat the
        // whole SRAMC output as one flat tile; the x-count is derived below.
        uint32_t one_output_tile_elements_1 = total_c_elements;
        uint32_t output_tiles = 1;
        // uint32_t total_contexts = decoded_cfg_runtime.ncontexts * output_tiles;

        // Output-x count = number of x-slices in the gold layout. Always derive
        // from total_c_elements (decoded o_cxlim is not this quantity).
        if (ncontexts != 0 && EVAL_Y != 0)
        {
            cxlim = total_c_elements / (ncontexts * EVAL_Y);
        }
        if (cxlim == 0)
        {
            cxlim = 1;
        }

        uint64_t rows_mask = decoded_cfg_runtime.rows_active;
        if (rows_mask == 0)
        {
            // Safety fallback: if config gives zero, treat all rows active.
            rows_mask = (EVAL_Y >= 64) ? ~0ULL : ((1ULL << EVAL_Y) - 1ULL);
        }

        bool preload_en = (decoded_cfg_runtime.preload_en != 0);

        if (out_base + total_c_elements * bytes_per_out > initial_dram_final.size() ||
            out_base + total_c_elements * bytes_per_out > gold_dram_final.size())
        {
            std::cerr << "[TB ERROR] C output region out of range.\n"
                      << "  out_base          = " << out_base << "\n"
                      << "  total_c_elements  = " << total_c_elements << "\n"
                      << "  bytes_per_out     = " << bytes_per_out << "\n"
                      << "  initial_dram size = " << initial_dram_final.size() << "\n"
                      << "  gold_dram size    = " << gold_dram_final.size() << "\n";
            sc_stop();
            return;
        }

        auto abs64 = [](int64_t v) -> int64_t
        {
            return (v < 0) ? -v : v;
        };

        int errors_std = 0;
        int64_t total_approx_error = 0;
        int64_t max_approx_error = 0;

#ifdef NPU_FP16
        // FP16-only accounting: exact (0-ULP) matches, elements accepted only via
        // the ULP tolerance, and the worst-case ULP distance seen.
        int fp16_exact_matches = 0;
        int fp16_tol_matches = 0;
        int32_t fp16_max_ulp = 0;
        // Signed-magnitude half -> monotone ordinal, so |ord(a)-ord(b)| = ULP dist.
        auto fp16_ulp_dist = [](uint16_t a, uint16_t b) -> int32_t
        {
            auto ord = [](uint16_t h) -> int32_t
            {
                return (h & 0x8000) ? -static_cast<int32_t>(h & 0x7FFF)
                                    : static_cast<int32_t>(h);
            };
            int32_t d = ord(a) - ord(b);
            return d < 0 ? -d : d;
        };
#endif

        std::cout << "\n==========================================================================================" << std::endl;
        std::cout << "   VERIFICATION REPORT: NPU FINAL DRAM vs SAURIA gold_dram" << std::endl;
        std::cout << "==========================================================================================" << std::endl;

        std::cout << "Config:" << std::endl;
        std::cout << "  out_base          : " << out_base << std::endl;
        std::cout << "  ncontexts         : " << ncontexts << std::endl;
        std::cout << "  cxlim             : " << cxlim << std::endl;
        std::cout << "  EVAL_Y            : " << EVAL_Y << std::endl;
        std::cout << "  total_c_elements  : " << total_c_elements << std::endl;
        std::cout << "  preload_en        : " << preload_en << std::endl;
        std::cout << "  rows_active       : 0x" << std::hex << rows_mask << std::dec << std::endl;

        std::cout << "------------------------------------------------------------------------------------------" << std::endl;
        std::cout << " Index | Golden Final | Initial C | NPU Compute | NPU Exact Final | NPU Approx Final | Match | APP Error" << std::endl;
        std::cout << "-------+--------------+-----------+-------------+-----------------+------------------+-------+----------" << std::endl;

        // Layout verified against SAURIA gold_dram:
        //   elem_idx = x * (ncontexts * EVAL_Y) + context * EVAL_Y + y
        //
        // PSM writes one vector of EVAL_Y lanes at base element address:
        //   base_elem_addr = calc_c_elem_addr(context, x, 0, ncontexts, EVAL_Y)
        //
        // Host reads SRAMC using the same base element address as PSM write address.
        // Do NOT read SRAMC linearly using phys_addr = global_idx / EVAL_Y.
        uint32_t total_contexts = ncontexts * output_tiles;
        if (total_contexts == 0)
        {
            total_contexts = ncontexts;
        }
        if (total_contexts == 0)
        {
            total_contexts = 1;
        }

        // Yused = valid output rows per vector. Gold packs Yused rows/(x,ctx); SRAM C
        // is physically strided by EVAL_Y. For GeMM Yused==EVAL_Y (no change).
        uint32_t Yv = 0;
        for (uint32_t yy = 0; yy < (uint32_t)EVAL_Y; yy++)
            if ((rows_mask >> yy) & 0x1ULL) Yv++;
        if (Yv == 0) Yv = EVAL_Y;
        if (ncontexts != 0 && Yv != 0)
            cxlim = total_c_elements / (ncontexts * Yv);
        if (cxlim == 0) cxlim = 1;

        uint32_t one_output_tile_elements = ncontexts * cxlim * EVAL_Y; // SRAM (physical) stride
        uint32_t one_output_tile_gold     = ncontexts * cxlim * Yv;     // gold (element) stride

        for (uint32_t out_tile = 0; out_tile < output_tiles; out_tile++)
        {
            for (uint32_t x = 0; x < cxlim; x++)
            {
                for (uint32_t ctx = 0; ctx < ncontexts; ctx++)
                {
                    uint32_t global_context = out_tile * ncontexts + ctx;

                    // Must match SAURIA gold_dram layout and PSM calc_c_addr:
                    //
                    //   C[out_tile][x][local_context][y]
                    //
                    uint32_t base_elem_addr = out_tile * one_output_tile_elements + x * (ncontexts * EVAL_Y) + ctx * EVAL_Y; // SRAM C addr
                    // gold element base with output-tile folding (Cout>Xused): the
                    // ncontexts fold n_tiles(=act_reps) weight tiles x nctx_per_tile
                    // (=Ch). ctx -> (tile, ch); cout = tile*cxlim + x. Single-tile
                    // (n_tiles=1) reduces to x*(ncontexts*Yv)+ctx*Yv (GeMM/strided).
                    uint32_t mt_ntiles = decoded_cfg_runtime.act_reps ? decoded_cfg_runtime.act_reps : 1;
                    uint32_t mt_ncpt   = (mt_ntiles != 0) ? (ncontexts / mt_ntiles) : ncontexts;
                    if (mt_ncpt == 0) mt_ncpt = (ncontexts ? ncontexts : 1);
                    uint32_t mt_tile = ctx / mt_ncpt;
                    uint32_t mt_ch   = ctx % mt_ncpt;
                    uint32_t mt_cout = mt_tile * cxlim + x;
                    uint32_t gold_base = mt_cout * (mt_ncpt * Yv) + mt_ch * Yv; // gold element base
                    for (int sw = 0; sw < subwords_c; sw++)
                    {
                        host_data_t chunk_std =
                            read_mem(0, get_sramc_addr(base_elem_addr, sw));

                        host_data_t chunk_approx =
                            read_mem(1, get_sramc_addr(base_elem_addr, sw));

                        for (int lane = 0; lane < 4; lane++)
                        {
                            uint32_t y = static_cast<uint32_t>(sw * 4 + lane);

                            if (y >= Yv)
                            {
                                continue;
                            }

                            uint32_t global_idx = gold_base + y;
                            if (global_idx >= total_c_elements)
                            {
                                continue;
                            }

                            bool row_active = ((rows_mask >> y) & 0x1ULL) != 0;

                            uint32_t byte_addr =
                                out_base + global_idx * bytes_per_out;

#ifdef NPU_FP16
                            uint16_t initial_bits = read_u16_le_from_bytes(initial_dram_final, byte_addr);
                            uint16_t golden_bits = read_u16_le_from_bytes(gold_dram_final, byte_addr);

                            float initial_val = fp16_t::half_to_float(initial_bits);
                            float golden_val = fp16_t::half_to_float(golden_bits);

                            float compute_std =
                                row_active ? static_cast<float>(chunk_std[lane]) : 0.0f;
                            float compute_approx =
                                row_active ? static_cast<float>(chunk_approx[lane]) : 0.0f;

                            // Match the SAURIA "ideal" reference model: accumulate in
                            // (wide) float, round to fp16 exactly once at DRAM write-out.
                            float npu_final_std_f = preload_en ? (initial_val + compute_std) : compute_std;
                            float npu_final_approx_f = preload_en ? (initial_val + compute_approx) : compute_approx;

                            uint16_t npu_final_std_bits = fp16_t::float_to_half(npu_final_std_f);
                            float npu_final_std = fp16_t::half_to_float(npu_final_std_bits);
                            float npu_final_approx = fp16_t::half_to_float(fp16_t::float_to_half(npu_final_approx_f));

                            // FP add is not associative -> accept within NPU_FP16_ULP_TOL
                            // ULPs of the reference (see macro comment). Track exact
                            // matches and worst-case ULP separately for transparency.
                            int32_t ulp = fp16_ulp_dist(npu_final_std_bits, golden_bits);
                            bool exact = (npu_final_std_bits == golden_bits);
                            bool match_std = (ulp <= (int32_t)NPU_FP16_ULP_TOL);

                            if (exact)
                                fp16_exact_matches++;
                            else if (match_std)
                                fp16_tol_matches++;
                            if (ulp > fp16_max_ulp)
                                fp16_max_ulp = ulp;

                            if (!match_std)
                            {
                                errors_std++;
                            }

                            // Report the ULP distance in the APP-error column for FP16.
                            int64_t app_error_abs = ulp;
#elif defined(NPU_INT16)
                            // INT16: exact integer math. OC_W=64 -> 8-byte output,
                            // int64 accumulate (no overflow for these magnitudes),
                            // read back through the double host bus (exact <= 2^53).
                            int64_t initial_val =
                                read_i64_le_from_bytes(initial_dram_final, byte_addr);

                            int64_t golden_val =
                                read_i64_le_from_bytes(gold_dram_final, byte_addr);

                            int64_t compute_std =
                                row_active ? static_cast<int64_t>(chunk_std[lane]) : 0;

                            int64_t compute_approx =
                                row_active ? static_cast<int64_t>(chunk_approx[lane]) : 0;

                            int64_t npu_final_std =
                                preload_en ? (initial_val + compute_std) : compute_std;

                            int64_t npu_final_approx =
                                preload_en ? (initial_val + compute_approx) : compute_approx;

                            bool match_std = (npu_final_std == golden_val);

                            if (!match_std)
                            {
                                errors_std++;
                            }

                            int64_t app_error_abs = abs64(npu_final_approx - golden_val);
#else
                            int32_t initial_val =
                                read_i32_le_from_bytes(initial_dram_final, byte_addr);

                            int32_t golden_val =
                                read_i32_le_from_bytes(gold_dram_final, byte_addr);

                            int32_t compute_std =
                                row_active ? static_cast<int32_t>(chunk_std[lane]) : 0;

                            int32_t compute_approx =
                                row_active ? static_cast<int32_t>(chunk_approx[lane]) : 0;

                            int32_t npu_final_std =
                                preload_en ? wrap_add_i32(initial_val, compute_std)
                                           : compute_std;

                            int32_t npu_final_approx =
                                preload_en ? wrap_add_i32(initial_val, compute_approx)
                                           : compute_approx;

                            bool match_std = (npu_final_std == golden_val);

                            if (!match_std)
                            {
                                errors_std++;
                            }

                            int64_t app_error =
                                static_cast<int64_t>(npu_final_approx) -
                                static_cast<int64_t>(golden_val);

                            int64_t app_error_abs = abs64(app_error);
#endif

                            total_approx_error += app_error_abs;

                            if (app_error_abs > max_approx_error)
                            {
                                max_approx_error = app_error_abs;
                            }

                            // Print first elements, last elements, and all mismatches.
                            if (!match_std || global_idx < 32 || global_idx + 8 >= total_c_elements)
                            {
                                std::cout
                                    << " " << std::setw(5) << global_idx
                                    << " | " << std::setw(12) << golden_val
                                    << " | " << std::setw(9) << initial_val
                                    << " | " << std::setw(11) << compute_std
                                    << " | " << std::setw(15) << npu_final_std
                                    << " | " << std::setw(16) << npu_final_approx
                                    << " | " << (match_std ? "PASS " : "FAIL ")
                                    << " | " << std::setw(8) << app_error_abs
                                    << std::endl;
                            }
                        }
                    }
                }
            }
        }

        std::cout << "==========================================================================================" << std::endl;
        std::cout << "[APPROX METRICS] Total error accumulator: " << total_approx_error << std::endl;
        std::cout << "[APPROX METRICS] Max error per element : " << max_approx_error << std::endl;
        std::cout << "==========================================================================================" << std::endl;

        std::cout << "Expected elements : " << total_c_elements << std::endl;
#ifdef NPU_FP16
        std::cout << "FP16 exact (0-ULP): " << fp16_exact_matches << std::endl;
        std::cout << "FP16 within tol   : " << fp16_tol_matches
                  << "  (<= " << NPU_FP16_ULP_TOL << " ULP)" << std::endl;
        std::cout << "FP16 max ULP dist : " << fp16_max_ulp << std::endl;
        std::cout << "Mismatches        : " << errors_std
                  << "  (> " << NPU_FP16_ULP_TOL << " ULP)" << std::endl;

        if (errors_std == 0)
        {
            std::cout << "[RESULT] TEST PASSED: NPU FP16 matches SAURIA gold_dram within "
                      << NPU_FP16_ULP_TOL << " ULP (exact=" << fp16_exact_matches
                      << ", tol=" << fp16_tol_matches
                      << ", max=" << fp16_max_ulp << " ULP)!" << std::endl;
        }
        else
        {
            std::cout << "[RESULT] TEST FAILED: NPU FP16 has " << errors_std
                      << " elements beyond " << NPU_FP16_ULP_TOL
                      << " ULP (max=" << fp16_max_ulp << " ULP)." << std::endl;
        }
#else
        std::cout << "Mismatches        : " << errors_std << std::endl;

        if (errors_std == 0)
        {
            std::cout << "[RESULT] TEST PASSED: NPU Exact Final matches SAURIA gold_dram with ZERO errors!" << std::endl;
        }
        else
        {
            std::cout << "[RESULT] TEST FAILED: NPU Exact Final has "
                      << errors_std
                      << " mismatches with SAURIA gold_dram."
                      << std::endl;
        }
#endif
        wait(20);
        sc_stop();
        return;
    }
};

int sc_main(int argc, char *argv[])
{
    sc_clock clk("clk", 10, SC_NS);

    int approx_mul_type = 0;
    int approx_add_type = 0;

    // Ex: ./tb_evaluate 1 2 -> Run with Approx Mult Type 1 and Approx Add Type 2
    if (argc >= 3)
    {
        approx_mul_type = std::stoi(argv[1]);
        approx_add_type = std::stoi(argv[2]);
        std::cout << "[TB] Approximate Multiplier Type: " << approx_mul_type << ", Approximate Adder Type: " << approx_add_type << std::endl;
    }
    else
    {
        std::cout << "[TB] No approximation types provided via command line. Using default (0, 0) for both multiplier and adder." << std::endl;
    }

    // Profile configurations
#ifdef NPU_FP16
    const int npu_arithmetic_type = 1; // FP
#else
    const int npu_arithmetic_type = 0; // int
#endif

    PeConfig cfg_std;
    cfg_std.arithmetic_type = npu_arithmetic_type;
    cfg_std.mul_type = 0;
    cfg_std.add_type = 0;
    cfg_std.stages_mul = 1;
    cfg_std.intermediate_pipeline_stage = true;
    cfg_std.zero_gating_mult = false; // No gating

    PeConfig cfg_approx;
    cfg_approx.arithmetic_type = npu_arithmetic_type;
    cfg_approx.mul_type = approx_mul_type;
    cfg_approx.add_type = approx_add_type;
    cfg_approx.m_approx = 0.85f; // 15% scaling error
    cfg_approx.a_approx = 0.95f; // 5% scaling error
    cfg_approx.stages_mul = 1;
    cfg_approx.intermediate_pipeline_stage = true;
    cfg_approx.zero_gating_mult = false;

    PeConfig cfg_gated;
    cfg_gated.arithmetic_type = npu_arithmetic_type;
    cfg_gated.mul_type = 0;
    cfg_gated.add_type = 0;
    cfg_gated.zero_gating_mult = true; // skip zero

    // NpuTop instances for each profile
    NpuTop<EVAL_X, EVAL_Y, NPU_DTYPE_IN, NPU_DTYPE_IN, NPU_DTYPE_PSUM, A_REGION_BYTES, B_REGION_BYTES, C_REGION_BYTES, 16, EVAL_X + EVAL_Y, 1> npu_std("NpuTop_std", cfg_std);
    NpuTop<EVAL_X, EVAL_Y, NPU_DTYPE_IN, NPU_DTYPE_IN, NPU_DTYPE_PSUM, A_REGION_BYTES, B_REGION_BYTES, C_REGION_BYTES, 16, EVAL_X + EVAL_Y, 1> npu_approx("NpuTop_approx", cfg_approx);
    NpuTop<EVAL_X, EVAL_Y, NPU_DTYPE_IN, NPU_DTYPE_IN, NPU_DTYPE_PSUM, A_REGION_BYTES, B_REGION_BYTES, C_REGION_BYTES, 16, EVAL_X + EVAL_Y, 1> npu_gated("NpuTop_gated", cfg_gated);
    TestbenchEvaluate tb("TestbenchEvaluate_inst");

    // Local signals
    sc_signal<bool> rstn{"rstn"};
    sc_signal<bool> soft_reset{"soft_reset"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<sc_bv<3>> select{"select"};
    sc_signal<uint32_t> sig_total_contexts;
    sc_signal<uint32_t> sig_mvm_k;

    tb.i_clk(clk);
    tb.o_rstn(rstn);
    tb.o_soft_reset(soft_reset);
    tb.o_threshold(threshold);
    tb.o_select(select);
    tb.o_mvm_k(sig_mvm_k);

    npu_std.i_mvm_k(sig_mvm_k);
    npu_approx.i_mvm_k(sig_mvm_k);
    npu_gated.i_mvm_k(sig_mvm_k);
    npu_std.i_clk(clk);
    npu_std.i_rstn(rstn);
    npu_std.i_soft_reset(soft_reset);
    npu_approx.i_clk(clk);
    npu_approx.i_rstn(rstn);
    npu_approx.i_soft_reset(soft_reset);
    npu_gated.i_clk(clk);
    npu_gated.i_rstn(rstn);
    npu_gated.i_soft_reset(soft_reset);

    npu_std.i_total_contexts(sig_total_contexts);
    npu_std.i_threshold(threshold);
    npu_std.i_select(select);

    npu_approx.i_total_contexts(sig_total_contexts);
    npu_approx.i_threshold(threshold);
    npu_approx.i_select(select);

    npu_gated.i_total_contexts(sig_total_contexts);
    npu_gated.i_threshold(threshold);
    npu_gated.i_select(select);

    // Done / start bindings
    sc_signal<bool> start_std, done_std, deadlock_std;
    sc_signal<bool> start_approx, done_approx, deadlock_approx;
    sc_signal<bool> start_gated, done_gated, deadlock_gated;

    tb.o_start_std(start_std);
    tb.i_done_std(done_std);
    tb.i_deadlock_std(deadlock_std);
    tb.o_start_approx(start_approx);
    tb.i_done_approx(done_approx);
    tb.i_deadlock_approx(deadlock_approx);
    tb.o_start_gated(start_gated);
    tb.i_done_gated(done_gated);
    tb.i_deadlock_gated(deadlock_gated);
    tb.o_total_contexts(sig_total_contexts);

    npu_std.i_start(start_std);
    npu_std.o_done(done_std);
    npu_std.o_deadlock(deadlock_std);
    npu_approx.i_start(start_approx);
    npu_approx.o_done(done_approx);
    npu_approx.o_deadlock(deadlock_approx);
    npu_gated.i_start(start_gated);
    npu_gated.o_done(done_gated);
    npu_gated.o_deadlock(deadlock_gated);

    // Memory ports bindings - STD
    sc_signal<uint32_t> addr_std;
    sc_signal<bool> wren_std, rden_std;
    sc_signal<host_data_t> wdata_std, rdata_std;
    sc_signal<host_mask_t> wmask_std;

    tb.o_host_addr_std(addr_std);
    tb.o_host_wren_std(wren_std);
    tb.o_host_rden_std(rden_std);
    tb.o_host_wdata_std(wdata_std);
    tb.o_host_wmask_std(wmask_std);
    tb.i_host_rdata_std(rdata_std);

    npu_std.i_host_addr(addr_std);
    npu_std.i_host_wren(wren_std);
    npu_std.i_host_rden(rden_std);
    npu_std.i_host_wdata(wdata_std);
    npu_std.i_host_wmask(wmask_std);
    npu_std.o_host_rdata(rdata_std);

    // Memory ports bindings - APPROX
    sc_signal<uint32_t> addr_approx;
    sc_signal<bool> wren_approx, rden_approx;
    sc_signal<host_data_t> wdata_approx, rdata_approx;
    sc_signal<host_mask_t> wmask_approx;

    tb.o_host_addr_approx(addr_approx);
    tb.o_host_wren_approx(wren_approx);
    tb.o_host_rden_approx(rden_approx);
    tb.o_host_wdata_approx(wdata_approx);
    tb.o_host_wmask_approx(wmask_approx);
    tb.i_host_rdata_approx(rdata_approx);

    npu_approx.i_host_addr(addr_approx);
    npu_approx.i_host_wren(wren_approx);
    npu_approx.i_host_rden(rden_approx);
    npu_approx.i_host_wdata(wdata_approx);
    npu_approx.i_host_wmask(wmask_approx);
    npu_approx.o_host_rdata(rdata_approx);

    // Memory ports bindings - GATED
    sc_signal<uint32_t> addr_gated;
    sc_signal<bool> wren_gated, rden_gated;
    sc_signal<host_data_t> wdata_gated, rdata_gated;
    sc_signal<host_mask_t> wmask_gated;

    tb.o_host_addr_gated(addr_gated);
    tb.o_host_wren_gated(wren_gated);
    tb.o_host_rden_gated(rden_gated);
    tb.o_host_wdata_gated(wdata_gated);
    tb.o_host_wmask_gated(wmask_gated);
    tb.i_host_rdata_gated(rdata_gated);

    npu_gated.i_host_addr(addr_gated);
    npu_gated.i_host_wren(wren_gated);
    npu_gated.i_host_rden(rden_gated);
    npu_gated.i_host_wdata(wdata_gated);
    npu_gated.i_host_wmask(wmask_gated);
    npu_gated.o_host_rdata(rdata_gated);

    sc_start();
    return 0;
}

