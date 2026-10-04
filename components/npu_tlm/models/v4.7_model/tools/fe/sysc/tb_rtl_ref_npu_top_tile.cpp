// tb_rtl_ref_npu_top_tile.cpp: drives rtl_ref_npu_top.h (unmodified port of sauria_model's npu_top.h +
// config_regs.h) through its real host bus (i_host_addr/wren/rden/wdata/wmask), in the tb_evaluate.cpp order:
//   load SRAM at select=000 -> switch to select=111 -> write CFG_CON/ACT/WEI/OUT in order
//   -> write i_total_contexts/i_mvm_k BEFORE start -> 2-cycle start pulse -> wait for i_done -> switch back to
//   select=000 -> read SRAM-C over the host bus.
//
// Unlike tools/fe/sysc/tb_fe_core_tile.cpp (RtlRefLaneACoreA with direct sc_signal pokes, no ConfigRegs / host
// bus), this goes through the real NpuTop / ConfigRegs, so it also validates the wiring and register decoding.
//
// Deliberate simplification: SRAM-A/B/preload-C are loaded over the host bus element by element, not through a
// write_bank_data() backdoor, because sram_inst is private in NpuTop (as in sauria_model). Addresses follow
// sram/rtl_ref_sram_top.h's beh_process(): local_addr = phys_addr*SUBWORDS + sub_word,
// sub_word = elem_idx/4, lane = elem_idx%4 (SUBWORDS_A = SUBWORDS_C = Y_DIM/4, SUBWORDS_B = X_DIM/4).
//
// Same jobs.tsv format (name\tgargs\ttile\tdir) and DRAM packer (driver/libsauria_mem.h's sauria_assemble_dram) as
// tb_fe_core_tile.cpp, so the A.bin/B.bin/bias.bin/expect_psum.bin/shape.txt job directories are reused.

#define SC_ALLOW_DEPRECATED_IEEE_API
#include <systemc.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "rtl_ref_npu_top.h"
#include "driver/libsauria_cfg.h"
#include "driver/libsauria_mem.h"
#include "sauria_targets.h"

using namespace sauria;

// Capacity per buffer: 5056*32 / 5184*32 are both ping-pong buffers together and Sram computes ROWS per buffer,
// so each buffer gets 79 / 81 KB as in the HAS (same as tb_fe_core_net.cpp).
static constexpr int kSramaBytes = 79 * 1024;
static constexpr int kSrambBytes = 81 * 1024;
typedef sauria_rtl::NpuTop<32, 32, int8_t, int8_t, int32_t, kSramaBytes, kSrambBytes, 1536, 16, 64, 1> NpuT;

struct Job
{
    std::string name;
    int kw, kh, d, s, cin, w, h, k, x_used, y_used, a_h, a_w;
    std::string dir;
};

static std::vector<char> read_file(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); std::exit(1); }
    return std::vector<char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static std::vector<Job> read_jobs(const std::string &tsv_path)
{
    std::vector<Job> jobs;
    std::ifstream f(tsv_path);
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty()) continue;
        std::istringstream ls(line);
        std::string name, gargs, tile, dir;
        std::getline(ls, name, '\t');
        std::getline(ls, gargs, '\t');
        std::getline(ls, tile, '\t');
        std::getline(ls, dir, '\t');
        std::istringstream ga(gargs);
        Job j; j.name = name; j.dir = dir;
        ga >> j.kw >> j.kh >> j.d >> j.s >> j.cin >> j.w >> j.h >> j.k >> j.x_used >> j.y_used;
        j.a_h = (j.h - 1) * j.s + j.kh;
        j.a_w = (j.w - 1) * j.s + j.kw;
        jobs.push_back(j);
    }
    return jobs;
}

SC_MODULE(TbRtlRefNpuTopTile)
{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn, soft_reset, start, done, deadlock;
    sc_signal<uint32_t> mvm_k, total_contexts;
    sc_signal<uint32_t> host_addr;
    sc_signal<bool> host_wren, host_rden;
    sc_signal<host_data_t> host_wdata;
    sc_signal<host_mask_t> host_wmask;
    sc_signal<host_data_t> host_rdata;
    sc_signal<float> threshold;
    sc_signal<sc_bv<3>> select;

    NpuT *dut{nullptr};

    static const int SUBWORDS_A = 32 / 4;
    static const int SUBWORDS_B = 32 / 4;
    static const int SUBWORDS_C = 32 / 4;

    int max_cycles{300000};

    SC_CTOR(TbRtlRefNpuTopTile)
    {
        // The instance name must contain "NpuTop_std": the tape switch of the port (`rtl_ref_ifmap_feeder.h`,
        // FX1_A3_FEED_TAPE block, verbatim from sauria_model) is gated on it. It produces the same
        // `trace_sysc/feed_tape.csv` as the original tree, for a column-by-column comparison at the same sample points.
        dut = new NpuT("NpuTop_std");
        dut->i_clk(i_clk);
        dut->i_rstn(rstn);
        dut->i_soft_reset(soft_reset);
        dut->i_start(start);
        dut->o_done(done);
        dut->o_deadlock(deadlock);
        dut->i_mvm_k(mvm_k);
        dut->i_host_addr(host_addr);
        dut->i_host_wren(host_wren);
        dut->i_host_rden(host_rden);
        dut->i_host_wdata(host_wdata);
        dut->i_host_wmask(host_wmask);
        dut->o_host_rdata(host_rdata);
        dut->i_threshold(threshold);
        dut->i_select(select);
        dut->i_total_contexts(total_contexts);

        SC_THREAD(run);
        sensitive << i_clk.pos();
    }

    void tick(int n = 1) { for (int i = 0; i < n; i++) wait(); }

    // Single-lane scalar register write (lane 0 only) -- matches tb_unified_smoke.cpp's wr().
    void wr(uint32_t addr, uint32_t val)
    {
        host_data_t d; host_mask_t m;
        d[0] = (double)val; m[0] = true;
        host_addr.write(addr); host_wdata.write(d); host_wmask.write(m);
        host_wren.write(true); host_rden.write(false);
        wait();
        host_wren.write(false);
        wait();
    }

    // ROWS_ACTIVE: 4 bytes packed (bit i of row -> bit (i%8) of byte i/8), per
    // rtl_ref_config_regs.h's decode (matches the core register map, register decode).
    void wr_rows_active(uint32_t y_used)
    {
        uint64_t bits = (y_used >= 32) ? 0xFFFFFFFFu : ((1u << y_used) - 1u);
        host_data_t d; host_mask_t m;
        for (int b = 0; b < 4; b++) { d[b] = (double)((bits >> (b * 8)) & 0xFF); m[b] = true; }
        host_addr.write(CFG_ACT_OFFSET + 0x00); host_wdata.write(d); host_wmask.write(m);
        host_wren.write(true); host_rden.write(false);
        wait();
        host_wren.write(false);
        wait();
    }

    // COLS_ACTIVE: 64-bit value across 2 lanes (core register map).
    void wr_cols_active(uint64_t bits64)
    {
        host_data_t d; host_mask_t m;
        d[0] = (double)(uint32_t)(bits64 & 0xFFFFFFFFu); m[0] = true;
        d[1] = (double)(uint32_t)(bits64 >> 32); m[1] = true;
        host_addr.write(WEI_COLS_ACTIVE); host_wdata.write(d); host_wmask.write(m);
        host_wren.write(true); host_rden.write(false);
        wait();
        host_wren.write(false);
        wait();
    }

    // Generic element-addressed SRAM host-bus load: row = dim_per_row consecutive int8
    // elements (matches sram beh_process: local_addr = row*subwords + sub_word, sub_word
    // selects 4 elements at a time across the 4 host lanes).
    void load_sram_i8(uint32_t region_offset, int subwords, int dim_per_row,
                       const uint8_t *data, size_t nbytes)
    {
        size_t nrows = (nbytes + (size_t)dim_per_row - 1) / (size_t)dim_per_row;
        for (size_t row = 0; row < nrows; row++)
        {
            for (int sw = 0; sw < subwords; sw++)
            {
                host_data_t d; host_mask_t m; bool any = false;
                for (int lane = 0; lane < 4; lane++)
                {
                    int elem = sw * 4 + lane;
                    if (elem >= dim_per_row) continue;
                    size_t byte_idx = row * (size_t)dim_per_row + (size_t)elem;
                    if (byte_idx >= nbytes) continue;
                    d[lane] = (double)(int8_t)data[byte_idx];
                    m[lane] = true;
                    any = true;
                }
                if (!any) continue;
                host_addr.write(region_offset + (uint32_t)row * subwords + sw);
                host_wdata.write(d); host_wmask.write(m);
                host_wren.write(true); host_rden.write(false);
                wait();
                host_wren.write(false);
                wait();
            }
        }
    }

    // Same addressing, but element = int32 (preload C) instead of int8.
    void load_sram_i32(uint32_t region_offset, int subwords, int dim_per_row,
                        const uint8_t *data, size_t nbytes)
    {
        size_t total_elems = nbytes / sizeof(int32_t);
        size_t nrows = (total_elems + (size_t)dim_per_row - 1) / (size_t)dim_per_row;
        for (size_t row = 0; row < nrows; row++)
        {
            for (int sw = 0; sw < subwords; sw++)
            {
                host_data_t d; host_mask_t m; bool any = false;
                for (int lane = 0; lane < 4; lane++)
                {
                    int elem = sw * 4 + lane;
                    if (elem >= dim_per_row) continue;
                    size_t global_elem = row * (size_t)dim_per_row + (size_t)elem;
                    if (global_elem >= total_elems) continue;
                    int32_t v;
                    std::memcpy(&v, data + global_elem * sizeof(int32_t), sizeof(int32_t));
                    d[lane] = (double)v;
                    m[lane] = true;
                    any = true;
                }
                if (!any) continue;
                host_addr.write(region_offset + (uint32_t)row * subwords + sw);
                host_wdata.write(d); host_wmask.write(m);
                host_wren.write(true); host_rden.write(false);
                wait();
                host_wren.write(false);
                wait();
            }
        }
    }

    void apply_layer_cfg(const SauriaLayerDesc &desc, const SauriaTarget &target)
    {
        uint64_t f[F_CFG_COUNT];
        sauria_compute_core_fields(desc, target, f);

        wr(CFG_CON_OFFSET + 0x00, (uint32_t)f[F_CFG_INCNTLIM]);
        wr(CFG_CON_OFFSET + 0x04, (uint32_t)f[F_CFG_ACT_REPS]);
        wr(CFG_CON_OFFSET + 0x08, (uint32_t)f[F_CFG_WEI_REPS]);

        wr_rows_active((uint32_t)desc.Y_used);
        // ACT.INCNTLIM = CON.INCNTLIM + 1 (core bring-up rule, matches tb_fe_core_tile.cpp's
        // apply_layer_cfg() da PASS s0-s11).
        wr(CFG_ACT_OFFSET + 0x04, (uint32_t)f[F_CFG_INCNTLIM] + 1);
        wr(CFG_ACT_OFFSET + 0x08, (uint32_t)target.Y);
        wr(CFG_ACT_OFFSET + 0x0C, (uint32_t)f[F_CFG_XLIM]);
        wr(CFG_ACT_OFFSET + 0x10, (uint32_t)target.Y);
        wr(CFG_ACT_OFFSET + 0x28, (uint32_t)(f[F_CFG_DIL_PAT] & 0xFFFFFFFFu));
        // dil_pat is MSB-first over 64 bits (libsauria_cfg.h: bit set at DILP_W-1-i for i < B_w). With B_w < 32
        // (almost every real kernel) EVERY set bit lies in the HIGH half, so HIGH32 must always be written, not only
        // for kernels > 1x1 (with HIGH32 left at 0, 1x1 tiles deadlock early).
        wr(CFG_ACT_OFFSET + 0x40, (uint32_t)(f[F_CFG_DIL_PAT] >> 32));
        // ACT.STRIDE (0x44) -> o_loc_woffs = rows_active_arr * arange(Y) * stride. Bring-up doc:
        // STRIDE = til_xstep/y_used = (Y_used*s)/Y_used = s directly.
        wr(CFG_ACT_OFFSET + 0x44, (uint32_t)desc.s);
        wr(CFG_ACT_OFFSET + 0x14, (uint32_t)f[F_CFG_XLIM]);
        wr(CFG_ACT_OFFSET + 0x18, (uint32_t)f[F_CFG_XSTEP]);
        wr(CFG_ACT_OFFSET + 0x1C, (uint32_t)f[F_CFG_YLIM]);
        wr(CFG_ACT_OFFSET + 0x20, (uint32_t)f[F_CFG_YSTEP]);
        wr(CFG_ACT_OFFSET + 0x24, (uint32_t)f[F_CFG_CHLIM]);
        wr(CFG_ACT_OFFSET + 0x2C, (uint32_t)f[F_CFG_CHSTEP]);
        wr(CFG_ACT_OFFSET + 0x30, (uint32_t)f[F_CFG_TIL_XLIM]);
        wr(CFG_ACT_OFFSET + 0x34, (uint32_t)f[F_CFG_TIL_XSTEP]);
        wr(CFG_ACT_OFFSET + 0x38, (uint32_t)f[F_CFG_TIL_YLIM]);
        wr(CFG_ACT_OFFSET + 0x3C, (uint32_t)f[F_CFG_TIL_YSTEP]);

        wr(CFG_WEI_OFFSET + 0x04, (uint32_t)f[F_CFG_WLIM]);
        wr(CFG_WEI_OFFSET + 0x08, (uint32_t)target.X);
        wr(CFG_WEI_OFFSET + 0x10, (uint32_t)f[F_CFG_WLIM]);
        wr(CFG_WEI_OFFSET + 0x14, (uint32_t)f[F_CFG_WSTEP]);
        wr(CFG_WEI_OFFSET + 0x18, (uint32_t)f[F_CFG_KLIM]);
        wr(CFG_WEI_OFFSET + 0x1C, (uint32_t)f[F_CFG_KSTEP]);
        wr(CFG_WEI_OFFSET + 0x20, (uint32_t)f[F_CFG_TIL_KLIM]);
        wr(CFG_WEI_OFFSET + 0x24, (uint32_t)f[F_CFG_TIL_KSTEP]);
        wr_cols_active(f[F_CFG_COLS_ACTIVE]);
        wr(CFG_WEI_OFFSET + 0x2C, (uint32_t)f[F_CFG_WALIGNED]);

        wr(NCONTEXTS, (uint32_t)f[F_CFG_NCONTEXTS]);
        wr(CFG_OUT_OFFSET + 0x04, (uint32_t)f[F_CFG_CXLIM]);
        wr(CFG_OUT_OFFSET + 0x08, (uint32_t)f[F_CFG_CXSTEP]);
        wr(CFG_OUT_OFFSET + 0x0C, (uint32_t)f[F_CFG_CKLIM]);
        wr(CFG_OUT_OFFSET + 0x10, (uint32_t)f[F_CFG_CKSTEP]);
        wr(TIL_CYLIM, (uint32_t)f[F_CFG_TIL_CYLIM]);
        wr(TIL_CYSTEP, (uint32_t)f[F_CFG_TIL_CYSTEP]);
        wr(TIL_CKLIM, (uint32_t)f[F_CFG_TIL_CKLIM]);
        wr(TIL_CKSTEP, (uint32_t)f[F_CFG_TIL_CKSTEP]);
        wr(INACTIVE_COLS, (uint32_t)f[F_CFG_INACTIVE_COLS]);
        wr(PRELOAD_EN, (uint32_t)f[F_CFG_PRELOAD_EN]);

        wr(CFG_ACT_BASE_ADDR, 0);
        wr(CFG_WEI_BASE_ADDR, 0);
        wr(CFG_OUT_BASE_ADDR, 0);
    }

    bool run_job(const Job &j)
    {
        const SauriaTarget *t = sauria_find_target("int8_32x32");
        std::vector<char> a8 = read_file(j.dir + "/A.bin");
        std::vector<char> b8 = read_file(j.dir + "/B.bin");
        std::vector<char> bias_raw = read_file(j.dir + "/bias.bin");
        std::vector<char> exp_raw = read_file(j.dir + "/expect_psum.bin");
        const int32_t *bias = reinterpret_cast<const int32_t *>(bias_raw.data());
        const int32_t *expect = reinterpret_cast<const int32_t *>(exp_raw.data());
        size_t n_out = (size_t)j.k * j.h * j.w;

        const bool zero_pre = getenv("FE_ZERO_PRELOAD") != nullptr;
        std::vector<double> A(a8.size()), B(b8.size()), C(n_out);
        for (size_t i = 0; i < a8.size(); i++) A[i] = (double)(int8_t)a8[i];
        for (size_t i = 0; i < b8.size(); i++) B[i] = (double)(int8_t)b8[i];
        // Optional per-element preload (int32, same [k][h][w] order as expect_psum.bin) replaces the per-channel
        // bias -- used when a tile continues a partial sum (Cin split). Absent file => per-channel bias as before.
        std::vector<char> pre_raw;
        {
            std::ifstream pf(j.dir + "/preload_psum.bin", std::ios::binary);
            if (pf)
                pre_raw.assign(std::istreambuf_iterator<char>(pf), std::istreambuf_iterator<char>());
        }
        if (!pre_raw.empty() && pre_raw.size() != n_out * sizeof(int32_t))
        {
            std::fprintf(stderr, "%s/preload_psum.bin: %zu bytes, expected %zu\n", j.dir.c_str(), pre_raw.size(),
                         n_out * sizeof(int32_t));
            std::exit(1);
        }
        const int32_t *pre = reinterpret_cast<const int32_t *>(pre_raw.data());
        for (int c = 0; c < j.k; c++)
            for (int p = 0; p < j.h * j.w; p++)
            {
                const size_t idx = (size_t)c * j.h * j.w + p;
                C[idx] = zero_pre ? 0.0 : (pre_raw.empty() ? (double)bias[c] : (double)pre[idx]);
            }
        SauriaDramLayout L = sauria_assemble_dram(A.data(), j.cin, j.a_h, j.a_w,
                                                 B.data(), j.k, j.cin, j.kh, j.kw,
                                                 C.data(), j.k, j.h, j.w, j.cin, j.k, *t);

        // --- 1. Reset, select=000 (host sees buffer 0) ---
        rstn.write(false); soft_reset.write(false); start.write(false);
        host_wren.write(false); host_rden.write(false); threshold.write(0.5f);
        mvm_k.write(1); total_contexts.write(1); select.write(sc_bv<3>("000"));
        tick(5);
        rstn.write(true);
        tick(2);

        wr(CFG_PROFILE_ADDR, (uint32_t)PROFILE_V1_SAURIA);

        auto load_t0 = std::chrono::steady_clock::now();
        // --- 2. Load SRAM-A (act), SRAM-B (wei), SRAM-C (preload) ---
        // Optional shortcut that bypasses the host bus, off by default (needs both the FX1_A3_SRAM_BACKDOOR_LOAD
        // macro and FE_SRAM_BACKDOOR at run time); the host bus stays the reference path. `L.dram` is already raw
        // row-major bytes with Y_DIM / X_DIM elements per row (the same assumption as load_sram_i8/i32 above), so
        // a direct write_bank_data copy keeps the layout -- it only skips 2 simulated cycles per element.
#ifdef FX1_A3_SRAM_BACKDOOR_LOAD
        static const bool use_backdoor = getenv("FE_SRAM_BACKDOOR") != nullptr;
        if (use_backdoor)
        {
            dut->load_sram_backdoor(2, 0, (const uint8_t *)L.dram.data() + L.A_off,
                                     (uint32_t)(L.B_off - L.A_off));
            dut->load_sram_backdoor(0, 0, (const uint8_t *)L.dram.data() + L.B_off,
                                     (uint32_t)(L.C_off - L.B_off));
            dut->load_sram_backdoor(4, 0, (const uint8_t *)L.dram.data() + L.C_off,
                                     (uint32_t)(L.dram.size() - L.C_off));
        }
        else
#endif
        {
            load_sram_i8(SRAMA_OFFSET, SUBWORDS_A, 32,
                         (const uint8_t *)L.dram.data() + L.A_off, (size_t)(L.B_off - L.A_off));
            load_sram_i8(SRAMB_OFFSET, SUBWORDS_B, 32,
                         (const uint8_t *)L.dram.data() + L.B_off, (size_t)(L.C_off - L.B_off));
            load_sram_i32(SRAMC_OFFSET, SUBWORDS_C, 32,
                          (const uint8_t *)L.dram.data() + L.C_off, L.dram.size() - (size_t)L.C_off);
        }
        double load_ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - load_t0)
                              .count();

        // --- 3. Swap to NPU side ---
        select.write(sc_bv<3>("111"));
        tick(2);

        // --- 4-5. Write CFG_* registers ---
        SauriaLayerDesc desc{};
        desc.B_w = j.kw; desc.B_h = j.kh; desc.d = j.d; desc.s = j.s;
        desc.c_til = j.cin; desc.k_til = j.k; desc.h_til = j.h; desc.w_til = j.w;
        desc.X_used = j.x_used; desc.Y_used = j.y_used; desc.preload_en = 1;
        apply_layer_cfg(desc, *t);

        // --- 6. total_contexts / mvm_k BEFORE start ---
        uint64_t f[F_CFG_COUNT];
        sauria_compute_core_fields(desc, *t, f);
        total_contexts.write((uint32_t)f[F_CFG_NCONTEXTS]);
        mvm_k.write(1);
        tick();

        // --- 7. Start pulse (2 cycles) ---
        bool trace_early = getenv("FE_TRACE_EARLY") != nullptr;
        // Trace BOTH cycles of the start pulse (t=-1, t=0): sauria_model raises cnt_en already at t=0 (anchor on
        // fsm_state), so tracing from t=1 would hide the first 2 cycles. The *_raw columns read the npu_top-level
        // signals directly, BEFORE the feeders gate them with fifo_full_any / stall_any.
        auto trace_row = [&](int t)
        {
            std::printf("  [t=%d] tsim=%llu act_empty=%d wei_full=%d act_start=%d wei_start=%d "
                        "act_valid=%d wei_valid=%d act_done=%d wei_done=%d act_hold=%d "
                        "wei_hold=%d fsm_state=%d act_cnt_en=%d act_cnt_en_raw=%d "
                        "act_cnt_en_seam=%d act_cnt_clear_raw=%d act_feeder_en_raw=%d start_int=%d act_til_done_q=%d "
                        "act_push0=%d act_empty0=%d act_elm0=%u act_nfree0=%u "
                        "act_stall_any=%d act_finalpush=%d\n",
                        t, (unsigned long long)sc_core::sc_time_stamp().value(),
                        (int)dut->dbg_act_fifo_empty(), (int)dut->dbg_wei_fifo_full(),
                        (int)dut->dbg_act_start(), (int)dut->dbg_wei_start(),
                        (int)dut->dbg_act_valid(), (int)dut->dbg_wei_valid(),
                        (int)dut->dbg_act_done(), (int)dut->dbg_wei_done(),
                        (int)dut->dbg_act_hold(), (int)dut->dbg_wei_hold(),
                        dut->dbg_feeders_state(), (int)dut->dbg_act_cnt_en(),
                        (int)dut->dbg_act_cnt_en_raw(), (int)dut->dbg_act_cnt_en_seam(),
                        (int)dut->dbg_act_cnt_clear_raw(),
                        (int)dut->dbg_act_feeder_en_raw(),
                        (int)dut->dbg_start_internal(),
                        (int)dut->dbg_act_til_done_q(), (int)dut->dbg_act_push0(),
                        (int)dut->dbg_act_empty0(), dut->dbg_act_elm0(),
                        dut->dbg_act_nfree0(), (int)dut->dbg_act_stall_any(),
                        (int)dut->dbg_act_finalpush());
        };
        start.write(true);
        for (int i = 0; i < 2; i++) { tick(); if (trace_early) trace_row(i - 1); }
        start.write(false);

        // --- 8. Wait for done ---
        // `o_deadlock` is a purely combinational MONITOR signal (act_empty && wei_full, sauria_model
        // control/main_controller.h:511-514) that toggles with NORMAL backpressure; sauria_model only traces it
        // and never stops on it. Stop rule: max_cycles, OR deadlock held CONTINUOUSLY for >= FE_STALL_CYCLES with
        // no progress (PSM write count unchanged over that window) -- only that means a real hang, not a
        // transient pulse (a normal pulse lasts ~99 cycles). Deadlock pulses are still counted for diagnosis.
        const int stall_cycles = getenv("FE_STALL_CYCLES") ? atoi(getenv("FE_STALL_CYCLES")) : 5000;
        int cyc = 0;
        long long deadlock_pulses = 0;
        int consec_deadlock = 0, cyc_since_progress = 0;
        uint32_t last_write_count = dut->get_psm_recorded_write_count();
        bool got_done = false, got_stuck = false;
        while (cyc < max_cycles)
        {
            tick();
            cyc++;
            if (trace_early && cyc <= 20)
            {
                trace_row(cyc);
            }
            uint32_t wc = dut->get_psm_recorded_write_count();
            if (wc != last_write_count) { cyc_since_progress = 0; last_write_count = wc; }
            else { cyc_since_progress++; }
            if (deadlock.read()) { deadlock_pulses++; consec_deadlock++; }
            else { consec_deadlock = 0; }
            if (consec_deadlock >= stall_cycles && cyc_since_progress >= stall_cycles)
            {
                got_stuck = true;
                break;
            }
            if (done.read()) { got_done = true; break; }
        }
        bool got_deadlock = got_stuck; // "really stuck", not a transient pulse
        // short drain so the last SRAM-C writes land before we swap/read
        tick(8);

        // --- 9. Swap back to host side ---
        select.write(sc_bv<3>("000"));
        tick(2);

        // --- 10. Read SRAM-C, compare ---
        std::vector<int32_t> got(n_out, 0);
        size_t nrows = (n_out + 31) / 32;
        for (size_t row = 0; row < nrows; row++)
        {
            for (int sw = 0; sw < SUBWORDS_C; sw++)
            {
                host_addr.write(SRAMC_OFFSET + (uint32_t)row * SUBWORDS_C + sw);
                host_rden.write(true); host_wren.write(false);
                tick(2);
                host_data_t r = host_rdata.read();
                host_rden.write(false);
                tick();
                for (int lane = 0; lane < 4; lane++)
                {
                    int elem = sw * 4 + lane;
                    size_t e = row * 32 + (size_t)elem;
                    if (e < n_out) got[e] = (int32_t)r[lane];
                }
            }
        }

        size_t mismatch = 0;
        for (size_t e = 0; e < n_out; e++)
            if (got[e] != expect[e]) mismatch++;

        std::printf("%s\t%s\tmismatch=%zu\telems=%zu\tcycles=%d\tdeadlock=%d\tdeadlock_pulses=%lld\ttimeout=%d\tXu=%d\tYu=%d\tload_ms=%.3f\n",
                    j.name.c_str(), (mismatch == 0 && !got_deadlock && got_done) ? "PASS" : "FAIL",
                    mismatch, n_out, cyc, (int)got_deadlock, deadlock_pulses,
                    (int)(!got_done && !got_deadlock), j.x_used, j.y_used, load_ms);
        std::fflush(stdout);
        return mismatch == 0 && !got_deadlock && got_done;
    }

    void run()
    {
        const char *jobs_path = getenv("FE_JOBS_TSV") ? getenv("FE_JOBS_TSV") : "fe_work/core/synth_all.tsv";
        std::vector<Job> jobs = read_jobs(jobs_path);
        int n_pass = 0, n_fail = 0;
        auto t0 = std::chrono::steady_clock::now();
        for (const auto &j : jobs)
        {
            bool ok = run_job(j);
            if (ok) n_pass++; else n_fail++;
        }
        double wall_ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0)
                              .count();
        std::printf("[tb_rtl_ref_npu_top_tile] RESULT: %s (jobs %d, pass %d, fail %d, wall_ms=%.1f)\n",
                    n_fail == 0 ? "PASS" : "FAIL", (int)jobs.size(), n_pass, n_fail, wall_ms);
        sc_stop();
    }
};

int sc_main(int argc, char **argv)
{
    (void)argc; (void)argv;
    sc_clock clk("clk", 10, SC_NS);
    TbRtlRefNpuTopTile tb("tb");
    tb.i_clk(clk);
    sc_start();
    return 0;
}
