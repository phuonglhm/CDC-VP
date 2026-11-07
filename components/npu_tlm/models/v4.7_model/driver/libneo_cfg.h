// libneo_cfg -- C++ encoder of the configuration register file of neo_lite (df_controller_top.sv), driver side, no SystemC.
// Layout: NEO_REGISTER_FILE.md (neo_lite RTL revision 1). The CPU writes N_REGS words at byte 0x10 + 4 * i of the controller window, then the
// interrupt enable (0x08) and START (0x00). The 40 DFC words and the 29 core words reuse driver/libsauria_cfg.h (SAURIA bit layout) unchanged;
// the core's four new output-region bits (transpose, psum stream, cin / oc tile last) are 0 here: the DFC writes the last two per tile.
// Only the words the hardware has are produced here. What the model keeps beyond them travels in has/has_host_neo.h (the "ext" lines).
#ifndef LIBNEO_CFG_H
#define LIBNEO_CFG_H

#include <cstdint>
#include <utility>
#include <vector>
#include "libsauria_cfg.h"

namespace neo
{
    // ---- controller window (neo_addr_pkg.sv CONTROLLER_OFFSET, df_controller_top.sv) ----
    constexpr uint32_t CTRL = 0x00, IEN = 0x08, ISTATUS = 0x0C, REG_BASE = 0x10;
    constexpr unsigned N_DFC = 40, N_CORE = 29, N_GVU = 13, N_REGS = N_DFC + N_CORE + N_GVU;   // 82 (int8 32x32)
    constexpr unsigned IDX_CORE = N_DFC, IDX_GVU = N_DFC + N_CORE;
    inline uint32_t reg_byte(unsigned i) { return REG_BASE + 4u * i; }

    // DFC words (neo_interface.sv READ_ARGS)
    enum Dfc : unsigned {
        XY_LIM = 0, CK_LIM = 1, P_XSTEP = 2, P_YSTEP = 3, P_KSTEP = 4, I_XSTEP = 5, I_YSTEP = 6, I_CSTEP = 7, W_KSTEP = 8, W_CSTEP = 9,
        D_IY_LIM = 10, D_IC_LIM = 11, D_PY_STEP = 12, D_PK_STEP = 13, D_IY_STEP = 14, D_IC_STEP = 15, D_W_STEP = 16, D_I_ETT = 17,
        START_A = 18, START_B = 19, START_C = 20,
        G_TILE_LIM = 21, SB_BTT = 22, SB_MOVE = 23, V0_BTT = 24, V0_STRIDE = 25, V0_REP = 26, V0_MOVE = 27, V1_BTT = 28, V1_MOVE = 29,
        V2_BTT = 30, V2_STRIDE = 31, V2_REP = 32, V2_MOVE = 33, START_SB = 34, START_V0 = 35, START_V1 = 36, START_V2 = 37, CTRL_BITS = 38, OPCODE = 39
    };
    // neo_opcode_t (df_controller_pkg.sv); NONE = an operation the hardware has no opcode for (model only)
    enum Op : unsigned { GEMM_FUSED = 0, SOFTMAX = 1, FUSED_ATTN = 2, LAYERNORM = 3, RESIDUAL = 4, MAXPOOL = 5, AVGPOOL = 6, NONE = 15 };
    // GVU configuration registers (gvu_pkg.sv gvu_cfg_reg_t); cell j of the file is register j + 1
    enum GvuReg : unsigned { G_CONTROL = 0, G_OPCODE = 1, G_LUT = 2, G_GEMM = 3, G_SM0 = 4, G_SM1 = 5, G_SM2 = 6, G_RES0 = 7, G_RES1 = 8, G_RES2 = 9,
                             G_RES3 = 10, G_RES4 = 11, G_POOL0 = 12, G_POOL1 = 13 };
    inline unsigned gvu_cell(GvuReg r) { return IDX_GVU + static_cast<unsigned>(r) - 1u; }
    // gvu_opcode_t numbers (the DFC overwrites cell 0 with these, neo_interface.sv)
    enum GvuOp : unsigned { GVU_GEMM_FUSED = 2, GVU_RESIDUAL = 6, GVU_MAXPOOL = 7, GVU_AVGPOOL = 8 };

    struct Image
    {
        std::vector<uint32_t> reg = std::vector<uint32_t>(N_REGS, 0u);
        uint32_t& dfc(Dfc d) { return reg[d]; }
        uint32_t dfc(Dfc d) const { return reg[d]; }
        uint32_t& gvu(GvuReg r) { return reg[gvu_cell(r)]; }
        uint32_t gvu(GvuReg r) const { return reg[gvu_cell(r)]; }
    };

    inline const sauria::SauriaTarget &target() { static const sauria::SauriaTarget *t = sauria::sauria_find_target("int8_32x32"); return *t; }

    // One GEMM_FUSED layer on a uniform tile grid: the DFC words 0..20 and 38 (libsauria_cfg a[0..21]) + the 29 core words.
    // `desc` is the SAURIA layer descriptor (C_w / C_h / C_c / A_c = full tensor, *_til = tile). dram = {A, B, C} byte addresses.
    // sbank = start of the per-channel requant block (word 34); zp / skip_act fill GEMM_FUSED_REG; the GVU opcode cell is set as the DFC would.
    // loop_order >= 0 replaces the order libsauria_cfg picks by data reuse (0: x, y, c, k; 1: c, k, space; 2: k, space with c_lim = 0); -1 keeps it.
    inline Image encode_gemm(const sauria::SauriaLayerDesc &desc, const uint32_t dram[3], uint32_t sbank, int zp, bool skip_act, int loop_order = -1)
    {
        Image im;
        const std::vector<uint32_t> a = sauria::sauria_encode_controller_args(desc, target(), dram);   // 22 + 29 words
        for (unsigned i = 0; i < 21; i++) im.reg[i] = a[i];
        im.reg[CTRL_BITS] = loop_order >= 0 ? sauria::set_bits(a[21], 17, 16, static_cast<uint32_t>(loop_order)) : a[21];
        for (unsigned i = 0; i < N_CORE; i++) im.reg[IDX_CORE + i] = a[22 + i];
        im.reg[OPCODE] = GEMM_FUSED;
        im.reg[START_SB] = sbank;
        im.gvu(G_OPCODE) = GVU_GEMM_FUSED;
        im.gvu(G_GEMM) = (skip_act ? 1u : 0u) | (static_cast<uint32_t>(zp) & 0xFFu) << 8;
        return im;
    }

    // The 82 CPU writes then interrupt enable and START (the order of tb_neo_top.sv). base = offset of the controller window in the SoC map.
    inline std::vector<std::pair<uint32_t, uint32_t>> write_sequence(const Image &im, uint32_t base = 0)
    {
        std::vector<std::pair<uint32_t, uint32_t>> w;
        for (unsigned i = 0; i < N_REGS; i++) w.emplace_back(base + reg_byte(i), im.reg[i]);
        w.emplace_back(base + IEN, 1u);
        w.emplace_back(base + CTRL, 1u);
        return w;
    }

    // Inverse of write_sequence for the register words only (replay of a trace): returns false for an address outside the file.
    inline bool apply_write(Image &im, uint32_t base, uint32_t addr, uint32_t data)
    {
        if (addr < base + REG_BASE || addr >= base + reg_byte(N_REGS) || (addr - base) % 4u) return false;
        im.reg[(addr - base - REG_BASE) / 4u] = data;
        return true;
    }

    // ==========================================================================================================================
    // Address map of neo_top and bit fields of the configuration words, read from the RTL (neo_lite RTL revision 1, int8_32x32).
    // tools/has/neo_regmap_check.py compares every number between a BEGIN and an END marker below with the RTL, and the core
    // fields with the packed layout of sauria_cfg_layout.h. A change of one side is a change of the other.
    // ==========================================================================================================================
    namespace addr
    {
        // BEGIN rtl_constants
        // neo_addr_pkg.sv. Configuration window of neo_top (AXI-Lite): a block is selected by (address & AXI_<block>_ADDR_MASK) == <block>_OFFSET.
        constexpr uint32_t CONTROLLER_OFFSET = 0x00000000u;
        constexpr uint32_t SAURIA_OFFSET = 0x00010000u;
        constexpr uint32_t GVU_OFFSET = 0x00020000u;
        constexpr uint32_t DMA_OFFSET = 0x00030000u;
        constexpr uint32_t AXI_CONTROLLER_ADDR_MASK = 0x000F0000u;
        constexpr uint32_t AXI_SAURIA_ADDR_MASK = 0x000F0000u;
        constexpr uint32_t AXI_GVU_ADDR_MASK = 0x000F0000u;
        constexpr uint32_t AXI_DMA_ADDR_MASK = 0x000F0000u;
        // Inside the SAURIA block: the four configuration regions of the core, selected by (offset & SAURIA_REG_ADDR_MASK).
        constexpr uint32_t SAURIA_REG_ADDR_MASK = 0x00000F00u;
        constexpr uint32_t CFG_REGS_OFFSET = 0x00000000u;
        constexpr uint32_t CFG_CON_OFFSET = 0x00000200u;
        constexpr uint32_t CFG_ACT_OFFSET = 0x00000400u;
        constexpr uint32_t CFG_WEI_OFFSET = 0x00000600u;
        constexpr uint32_t CFG_OUT_OFFSET = 0x00000800u;
        // neo_interface.sv: where the DFC writes inside the SAURIA and GVU blocks (control and interrupt registers, GVU register 1 onwards).
        constexpr uint32_t SAURIA_CONTROL_OFFSET = 0x00000000u;
        constexpr uint32_t INTERRUPT_EN_OFFSET = 0x00000004u;
        constexpr uint32_t INTERRUPT_STATUS_OFFSET = 0x0000000Cu;
        constexpr uint32_t GVU_CONTROL_OFFSET = 0x00000000u;
        constexpr uint32_t GVU_CFG_OFFSET = 0x00000004u;
        // neo_addr_pkg.sv. Local memories, inside the SAURIA data space (SRAMA / B / C) and the GVU scratchpad space (VBANK0..2, SBANK).
        constexpr uint32_t SRAMA_OFFSET = 0x00000000u;
        constexpr uint32_t SRAMB_OFFSET = 0x00040000u;
        constexpr uint32_t SRAMC_OFFSET = 0x00080000u;
        constexpr uint32_t VBANK0_OFFSET = 0x00000000u;
        constexpr uint32_t VBANK1_OFFSET = 0x00020000u;
        constexpr uint32_t VBANK2_OFFSET = 0x00040000u;
        constexpr uint32_t SBANK_OFFSET = 0x00080000u;
        constexpr uint32_t SAURIA_MEM_ADDR_MASK = 0x000F0000u;
        constexpr uint32_t GVU_MEM_ADDR_MASK = 0x000F0000u;
        constexpr uint32_t GVU_SBANK_ADDR_MASK = 0x00080000u;
        // neo_addr_pkg.sv. The DMA bus: the SAURIA space starts at SAURIA_DMA_OFFSET, the GVU space at GVU_DMA_OFFSET (selected by the _ADDR_MASK).
        constexpr uint32_t SAURIA_DMA_OFFSET = 0x00000000u;
        constexpr uint32_t SAURIA_DMA_ADDR_MASK = 0xFFF00000u;
        constexpr uint32_t GVU_DMA_OFFSET = 0x00100000u;
        constexpr uint32_t GVU_DMA_ADDR_MASK = 0xFFF00000u;
        // neo_dma_controller.sv: the address the DMA puts on the bus to reach a local memory (space + offset).
        constexpr uint32_t BUS_SRAMA = SAURIA_DMA_OFFSET + SRAMA_OFFSET;
        constexpr uint32_t BUS_SRAMB = SAURIA_DMA_OFFSET + SRAMB_OFFSET;
        constexpr uint32_t BUS_SRAMC = SAURIA_DMA_OFFSET + SRAMC_OFFSET;
        constexpr uint32_t BUS_SBANK = GVU_DMA_OFFSET + SBANK_OFFSET;
        constexpr uint32_t BUS_VBANK0 = GVU_DMA_OFFSET + VBANK0_OFFSET;
        constexpr uint32_t BUS_VBANK1 = GVU_DMA_OFFSET + VBANK1_OFFSET;
        constexpr uint32_t BUS_VBANK2 = GVU_DMA_OFFSET + VBANK2_OFFSET;
        // END rtl_constants

        // BEGIN rtl_core_extra
        // The core image (words IDX_CORE.. of the file) in bit positions from its first word. The packed fields are those of sauria_cfg_layout.h;
        // config_regs.sv has six bits more, all 0 here: the SRAM sleep / power-gate bits after THRES, and four output-region bits after PRELOAD_EN.
        constexpr unsigned CORE_IDX_A = 17;
        constexpr unsigned CORE_IDX_W = 17;
        constexpr unsigned CORE_IDX_O = 16;
        constexpr unsigned CORE_WORDS_CON = 2;
        constexpr unsigned CORE_WORDS_ACT = 17;
        constexpr unsigned CORE_WORDS_WEI = 5;
        constexpr unsigned CORE_WORDS_OUT = 5;
        constexpr unsigned CORE_BIT_CON_EXTRA = CORE_IDX_A + 2 * CORE_IDX_O + 2;
        constexpr unsigned CORE_BIT_OUT = 32 * (CORE_WORDS_CON + CORE_WORDS_ACT + CORE_WORDS_WEI);
        constexpr unsigned CORE_BIT_OUT_EXTRA = CORE_BIT_OUT + 9 * CORE_IDX_O + 8 + 1;
        struct CoreExtra { unsigned bit; unsigned width; const char *rtl; };
        static const CoreExtra CORE_EXTRA[] = {
            {CORE_BIT_CON_EXTRA, 1, "sram_deepsleep"},
            {CORE_BIT_CON_EXTRA + 1, 1, "sram_powergate"},
            {CORE_BIT_OUT_EXTRA, 1, "transpose_mode"},
            {CORE_BIT_OUT_EXTRA + 1, 1, "psum_stream_en"},
            {CORE_BIT_OUT_EXTRA + 2, 1, "cin_tile_last"},
            {CORE_BIT_OUT_EXTRA + 3, 1, "oc_tile_last"},
        };
        // END rtl_core_extra

        // ---- address generation. cfg_base = where the configuration window sits in the SoC map; dram_base = where the DRAM sits.
        enum Block : unsigned { BLK_CONTROLLER = 0, BLK_SAURIA = 1, BLK_GVU = 2, BLK_DMA = 3 };
        constexpr uint32_t block_offset(Block b) { return b == BLK_CONTROLLER ? CONTROLLER_OFFSET : b == BLK_SAURIA ? SAURIA_OFFSET : b == BLK_GVU ? GVU_OFFSET : DMA_OFFSET; }
        constexpr uint32_t block_mask(Block b) { return b == BLK_CONTROLLER ? AXI_CONTROLLER_ADDR_MASK : b == BLK_SAURIA ? AXI_SAURIA_ADDR_MASK : b == BLK_GVU ? AXI_GVU_ADDR_MASK : AXI_DMA_ADDR_MASK; }
        // Address of byte `off` of a block.
        constexpr uint32_t cfg_addr(Block b, uint32_t off, uint32_t cfg_base = 0) { return cfg_base + block_offset(b) + off; }
        // The block of an offset in the window, as neo_top.sv selects it (controller, DMA, SAURIA, GVU in that order); false = the error slave answers.
        inline bool block_of(uint32_t cfg_off, Block &b)
        {
            static const Block order[] = {BLK_CONTROLLER, BLK_DMA, BLK_SAURIA, BLK_GVU};
            for (Block c : order)
                if ((cfg_off & block_mask(c)) == block_offset(c)) { b = c; return true; }
            return false;
        }

        // Word i of the 82-word file in the controller window: block root + 0x10 + 4 i (the same as neo::reg_byte(i) when cfg_base = 0).
        constexpr uint32_t dfc_word_addr(unsigned i, uint32_t cfg_base = 0) { return cfg_addr(BLK_CONTROLLER, REG_BASE + 4u * i, cfg_base); }

        // The register the DFC writes for word k of the core image (k = 0..N_CORE-1), in the SAURIA block: region base + 4 * index in the region.
        enum CoreRegion : unsigned { CORE_CON = 0, CORE_ACT = 1, CORE_WEI = 2, CORE_OUT = 3 };
        constexpr uint32_t core_region_offset(CoreRegion r) { return r == CORE_CON ? CFG_CON_OFFSET : r == CORE_ACT ? CFG_ACT_OFFSET : r == CORE_WEI ? CFG_WEI_OFFSET : CFG_OUT_OFFSET; }
        constexpr uint32_t core_reg_addr(CoreRegion r, unsigned i, uint32_t cfg_base = 0) { return cfg_addr(BLK_SAURIA, core_region_offset(r) + 4u * i, cfg_base); }
        constexpr uint32_t core_word_addr(unsigned k, uint32_t cfg_base = 0)
        {
            return k < CORE_WORDS_CON ? core_reg_addr(CORE_CON, k, cfg_base)
                 : k < CORE_WORDS_CON + CORE_WORDS_ACT ? core_reg_addr(CORE_ACT, k - CORE_WORDS_CON, cfg_base)
                 : k < CORE_WORDS_CON + CORE_WORDS_ACT + CORE_WORDS_WEI ? core_reg_addr(CORE_WEI, k - CORE_WORDS_CON - CORE_WORDS_ACT, cfg_base)
                 : core_reg_addr(CORE_OUT, k - CORE_WORDS_CON - CORE_WORDS_ACT - CORE_WORDS_WEI, cfg_base);
        }
        // GVU register `reg` (0 = control, 1..13 = the gvu_cfg_reg_t numbers; cell j of the file is register j + 1) in the GVU block.
        constexpr uint32_t gvu_reg_addr(unsigned reg, uint32_t cfg_base = 0) { return cfg_addr(BLK_GVU, reg == 0 ? GVU_CONTROL_OFFSET : GVU_CFG_OFFSET + 4u * (reg - 1u), cfg_base); }
        constexpr uint32_t gvu_word_addr(unsigned j, uint32_t cfg_base = 0) { return gvu_reg_addr(j + 1u, cfg_base); }

        // Local memories as the DMA bus sees them.
        enum LocalMem : unsigned { MEM_SRAMA = 0, MEM_SRAMB = 1, MEM_SRAMC = 2, MEM_VBANK0 = 3, MEM_VBANK1 = 4, MEM_VBANK2 = 5, MEM_SBANK = 6 };
        constexpr uint32_t bus_base(LocalMem m)
        {
            return m == MEM_SRAMA ? BUS_SRAMA : m == MEM_SRAMB ? BUS_SRAMB : m == MEM_SRAMC ? BUS_SRAMC
                 : m == MEM_VBANK0 ? BUS_VBANK0 : m == MEM_VBANK1 ? BUS_VBANK1 : m == MEM_VBANK2 ? BUS_VBANK2 : BUS_SBANK;
        }
        constexpr uint32_t bus_addr(LocalMem m, uint32_t byte_off) { return bus_base(m) + byte_off; }
        // A DRAM byte address (the pointer words START_A.. hold these).
        constexpr uint32_t dram_addr(uint32_t dram_base, uint32_t byte_off) { return dram_base + byte_off; }

        static_assert(dfc_word_addr(0) == 0x10u && dfc_word_addr(3) == 0x1Cu && dfc_word_addr(81) == 0x10u + 4u * 81u, "controller window words");
        static_assert(dfc_word_addr(3, 0x80000000u) == 0x8000001Cu, "configuration base is a parameter");
        static_assert(core_word_addr(0) == 0x10200u && core_word_addr(1) == 0x10204u && core_word_addr(2) == 0x10400u && core_word_addr(18) == 0x10440u, "core CON / ACT");
        static_assert(core_word_addr(19) == 0x10600u && core_word_addr(24) == 0x10800u && core_word_addr(28) == 0x10810u, "core WEI / OUT");
        static_assert(gvu_reg_addr(0) == 0x20000u && gvu_reg_addr(1) == 0x20004u && gvu_reg_addr(13) == 0x20034u && gvu_word_addr(0) == 0x20004u, "GVU registers");
        static_assert(bus_addr(MEM_SRAMB, 0x10) == 0x40010u && bus_addr(MEM_VBANK1, 0) == 0x120000u && bus_addr(MEM_SBANK, 0) == 0x180000u, "DMA bus");
    } // namespace addr

    // Fields of the configuration words. FS_FILE: word of the 82-word file; FS_GVUREG: GVU register number (0 = control, 1..13);
    // FS_CTLWIN: word of the controller window (0 control, 2 interrupt enable, 3 interrupt status clear). [hi:lo] inside the word.
    // `rtl` is the name the RTL gives the field (the destination register or output port); `symbol` is the Dfc / GvuReg enumerator of the word.
    enum FieldSpace : unsigned { FS_FILE = 0, FS_GVUREG = 1, FS_CTLWIN = 2 };
    struct FieldSpec { FieldSpace space; unsigned word; unsigned hi; unsigned lo; const char *rtl; const char *symbol; };
    // BEGIN rtl_fields
    static const FieldSpec FIELDS[] = {
            {FS_FILE, 0, 31, 16, "sauria_dma_params.tile.y_lim", "XY_LIM"},
            {FS_FILE, 0, 15, 0, "sauria_dma_params.tile.x_lim", "XY_LIM"},
            {FS_FILE, 1, 31, 16, "sauria_dma_params.tile.k_lim", "CK_LIM"},
            {FS_FILE, 1, 15, 0, "sauria_dma_params.tile.c_lim", "CK_LIM"},
            {FS_FILE, 2, 31, 0, "sauria_dma_params.tile.psums.x_step", "P_XSTEP"},
            {FS_FILE, 3, 31, 0, "sauria_dma_params.tile.psums.y_step", "P_YSTEP"},
            {FS_FILE, 4, 31, 0, "sauria_dma_params.tile.psums.k_step", "P_KSTEP"},
            {FS_FILE, 5, 31, 0, "sauria_dma_params.tile.ifmaps.x_step", "I_XSTEP"},
            {FS_FILE, 6, 31, 0, "sauria_dma_params.tile.ifmaps.y_step", "I_YSTEP"},
            {FS_FILE, 7, 31, 0, "sauria_dma_params.tile.ifmaps.c_step", "I_CSTEP"},
            {FS_FILE, 8, 31, 0, "sauria_dma_params.tile.weights.k_step", "W_KSTEP"},
            {FS_FILE, 9, 31, 0, "sauria_dma_params.tile.weights.c_step", "W_CSTEP"},
            {FS_FILE, 10, 31, 0, "sauria_dma_params.dma.ifmaps.y_lim", "D_IY_LIM"},
            {FS_FILE, 11, 31, 0, "sauria_dma_params.dma.ifmaps.c_lim", "D_IC_LIM"},
            {FS_FILE, 12, 31, 0, "sauria_dma_params.dma.psums.y_step", "D_PY_STEP"},
            {FS_FILE, 13, 31, 0, "sauria_dma_params.dma.psums.k_step", "D_PK_STEP"},
            {FS_FILE, 14, 31, 0, "sauria_dma_params.dma.ifmaps.y_step", "D_IY_STEP"},
            {FS_FILE, 15, 31, 0, "sauria_dma_params.dma.ifmaps.c_step", "D_IC_STEP"},
            {FS_FILE, 16, 31, 0, "sauria_dma_params.dma.weights.w_step", "D_W_STEP"},
            {FS_FILE, 17, 31, 0, "sauria_dma_params.dma.ifmaps.ett", "D_I_ETT"},
            {FS_FILE, 18, 31, 0, "start_SRAMA_addr", "START_A"},
            {FS_FILE, 19, 31, 0, "start_SRAMB_addr", "START_B"},
            {FS_FILE, 20, 31, 0, "start_SRAMC_addr", "START_C"},
            {FS_FILE, 21, 31, 0, "gvu_dma_params.tile_lim", "G_TILE_LIM"},
            {FS_FILE, 21, 18, 18, "start_gate", "G_TILE_LIM"},
            {FS_FILE, 22, 31, 0, "gvu_dma_params.sbank.btt", "SB_BTT"},
            {FS_FILE, 23, 31, 0, "gvu_dma_params.sbank.tile_move", "SB_MOVE"},
            {FS_FILE, 24, 31, 0, "gvu_dma_params.vbank0.btt", "V0_BTT"},
            {FS_FILE, 25, 31, 0, "gvu_dma_params.vbank0.stride", "V0_STRIDE"},
            {FS_FILE, 26, 31, 0, "gvu_dma_params.vbank0.rep_lim", "V0_REP"},
            {FS_FILE, 27, 31, 0, "gvu_dma_params.vbank0.tile_move", "V0_MOVE"},
            {FS_FILE, 28, 31, 0, "gvu_dma_params.vbank1.btt", "V1_BTT"},
            {FS_FILE, 29, 31, 0, "gvu_dma_params.vbank1.tile_move", "V1_MOVE"},
            {FS_FILE, 30, 31, 0, "gvu_dma_params.vbank2.btt", "V2_BTT"},
            {FS_FILE, 31, 31, 0, "gvu_dma_params.vbank2.stride", "V2_STRIDE"},
            {FS_FILE, 32, 31, 0, "gvu_dma_params.vbank2.rep_lim", "V2_REP"},
            {FS_FILE, 33, 31, 0, "gvu_dma_params.vbank2.tile_move", "V2_MOVE"},
            {FS_FILE, 34, 31, 0, "start_sbank_addr", "START_SB"},
            {FS_FILE, 35, 31, 0, "start_vbank0_addr", "START_V0"},
            {FS_FILE, 36, 31, 0, "start_vbank1_addr", "START_V1"},
            {FS_FILE, 37, 31, 0, "start_vbank2_addr", "START_V2"},
            {FS_FILE, 38, 31, 31, "WXfer_op", "CTRL_BITS"},
            {FS_FILE, 38, 25, 25, "Ck_eq", "CTRL_BITS"},
            {FS_FILE, 38, 24, 24, "Ch_eq", "CTRL_BITS"},
            {FS_FILE, 38, 23, 23, "Cw_eq", "CTRL_BITS"},
            {FS_FILE, 38, 22, 22, "start", "CTRL_BITS"},
            {FS_FILE, 38, 21, 21, "stand_alone_keep_C", "CTRL_BITS"},
            {FS_FILE, 38, 20, 20, "stand_alone_keep_B", "CTRL_BITS"},
            {FS_FILE, 38, 19, 19, "stand_alone_keep_A", "CTRL_BITS"},
            {FS_FILE, 38, 18, 18, "stand_alone", "CTRL_BITS"},
            {FS_FILE, 38, 18, 18, "start_dma_controller", "CTRL_BITS"},
            {FS_FILE, 38, 17, 16, "loop_order", "CTRL_BITS"},
            {FS_FILE, 39, 3, 0, "opcode", "OPCODE"},
            {FS_FILE, 69, 3, 0, "o_gvu_op", "G_OPCODE"},
            {FS_FILE, 70, 3, 3, "lut_rsqrt_reset", "G_LUT"},
            {FS_FILE, 70, 2, 2, "lut_recip_reset", "G_LUT"},
            {FS_FILE, 70, 1, 1, "lut_exp_reset", "G_LUT"},
            {FS_FILE, 70, 0, 0, "lut_act_reset", "G_LUT"},
            {FS_FILE, 71, 15, 8, "o_gemm_fused_z", "G_GEMM"},
            {FS_FILE, 71, 0, 0, "o_gemm_fused_skip_act", "G_GEMM"},
            {FS_FILE, 72, 31, 16, "o_num_tile", "G_SM0"},
            {FS_FILE, 72, 15, 0, "o_token_len", "G_SM0"},
            {FS_FILE, 73, 31, 0, "o_attn_scale", "G_SM1"},
            {FS_FILE, 74, 15, 8, "o_attn_z", "G_SM2"},
            {FS_FILE, 74, 7, 0, "o_attn_shift", "G_SM2"},
            {FS_FILE, 75, 30, 30, "o_residual_sub_mode", "G_RES0"},
            {FS_FILE, 75, 29, 24, "o_residual_out_shift", "G_RES0"},
            {FS_FILE, 75, 23, 16, "o_residual_out_z", "G_RES0"},
            {FS_FILE, 75, 15, 0, "o_residual_sequence_len", "G_RES0"},
            {FS_FILE, 76, 31, 0, "o_residual_dequant_scale[0]", "G_RES1"},
            {FS_FILE, 77, 31, 0, "o_residual_dequant_scale[1]", "G_RES2"},
            {FS_FILE, 78, 31, 0, "o_residual_bias[31:0]", "G_RES3"},
            {FS_FILE, 79, 31, 0, "o_residual_bias[63:32]", "G_RES4"},
            {FS_FILE, 80, 31, 24, "o_pooling_c", "G_POOL0"},
            {FS_FILE, 80, 23, 16, "o_pooling_oh", "G_POOL0"},
            {FS_FILE, 80, 15, 8, "o_pooling_num_w_tile", "G_POOL0"},
            {FS_FILE, 80, 4, 2, "o_pooling_stride", "G_POOL0"},
            {FS_FILE, 80, 1, 0, "o_pooling_mode", "G_POOL0"},
            {FS_FILE, 81, 25, 16, "o_pooling_c_move", "G_POOL1"},
            {FS_FILE, 81, 9, 0, "o_pooling_h_move", "G_POOL1"},
            {FS_GVUREG, 0, 3, 3, "force_swap", "G_CONTROL"},
            {FS_GVUREG, 0, 2, 2, "done_clear", "G_CONTROL"},
            {FS_GVUREG, 0, 1, 1, "done", "G_CONTROL"},
            {FS_GVUREG, 0, 0, 0, "pipe_en", "G_CONTROL"},
            {FS_GVUREG, 1, 3, 0, "o_gvu_op", "G_OPCODE"},
            {FS_GVUREG, 2, 3, 3, "lut_rsqrt_reset", "G_LUT"},
            {FS_GVUREG, 2, 2, 2, "lut_recip_reset", "G_LUT"},
            {FS_GVUREG, 2, 1, 1, "lut_exp_reset", "G_LUT"},
            {FS_GVUREG, 2, 0, 0, "lut_act_reset", "G_LUT"},
            {FS_GVUREG, 3, 15, 8, "o_gemm_fused_z", "G_GEMM"},
            {FS_GVUREG, 3, 0, 0, "o_gemm_fused_skip_act", "G_GEMM"},
            {FS_GVUREG, 4, 31, 16, "o_num_tile", "G_SM0"},
            {FS_GVUREG, 4, 15, 0, "o_token_len", "G_SM0"},
            {FS_GVUREG, 5, 31, 0, "o_attn_scale", "G_SM1"},
            {FS_GVUREG, 6, 15, 8, "o_attn_z", "G_SM2"},
            {FS_GVUREG, 6, 7, 0, "o_attn_shift", "G_SM2"},
            {FS_GVUREG, 7, 30, 30, "o_residual_sub_mode", "G_RES0"},
            {FS_GVUREG, 7, 29, 24, "o_residual_out_shift", "G_RES0"},
            {FS_GVUREG, 7, 23, 16, "o_residual_out_z", "G_RES0"},
            {FS_GVUREG, 7, 15, 0, "o_residual_sequence_len", "G_RES0"},
            {FS_GVUREG, 8, 31, 0, "o_residual_dequant_scale[0]", "G_RES1"},
            {FS_GVUREG, 9, 31, 0, "o_residual_dequant_scale[1]", "G_RES2"},
            {FS_GVUREG, 10, 31, 0, "o_residual_bias[31:0]", "G_RES3"},
            {FS_GVUREG, 11, 31, 0, "o_residual_bias[63:32]", "G_RES4"},
            {FS_GVUREG, 12, 31, 24, "o_pooling_c", "G_POOL0"},
            {FS_GVUREG, 12, 23, 16, "o_pooling_oh", "G_POOL0"},
            {FS_GVUREG, 12, 15, 8, "o_pooling_num_w_tile", "G_POOL0"},
            {FS_GVUREG, 12, 4, 2, "o_pooling_stride", "G_POOL0"},
            {FS_GVUREG, 12, 1, 0, "o_pooling_mode", "G_POOL0"},
            {FS_GVUREG, 13, 25, 16, "o_pooling_c_move", "G_POOL1"},
            {FS_GVUREG, 13, 9, 0, "o_pooling_h_move", "G_POOL1"},
            {FS_CTLWIN, 0, 1, 1, "done", ""},
            {FS_CTLWIN, 0, 0, 0, "start", ""},
            {FS_CTLWIN, 2, 0, 0, "ien", ""},
            {FS_CTLWIN, 3, 0, 0, "intr", ""},
    };
    // END rtl_fields
    constexpr unsigned N_FIELDS = sizeof(FIELDS) / sizeof(FIELDS[0]);
}

#endif // LIBNEO_CFG_H
