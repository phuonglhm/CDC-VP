// tb_has_net = tb_fe_core_net with the epilogue and ELEM_WISE of the HAS drawings (has/HAS_IFACE.md):
//   - v4.5 Obp -> has::HasObp (same ports); NCH (0x1A0004) written per tile; waits until HasObp::vectors_out has
//     every vector instead of a fixed IDLE_CYCLES; residual_en is always 0.
//   - prog.bin "FEHP" (tools/fe/fe_has_export_net.py): knobs <4I> after the magic; KIND 5 elem_add -> HasElemwise::add
//     (two stages through the Scratchpad, timed), KIND 6 elem_max -> HasElemwise::maxpool. KIND 3 (host maxpool) is gone.
//   - Knobs come from prog.bin; the HAS_* environment variables (Knobs::from_env) are not used, so net and knobs match.
// Input: fe_work/has/net_{compat,has}. Usage: tb_has_net <net_dir> [max_steps]. Below: the tb_fe_core_net notes.
// Whole-network testbench: like tools/fe/sysc/tb_fe_network.cpp (shared v4.5 SRAM + SauriaDma + Obp) but each
// conv tile can run either on the functional CORE STAND-IN (plain C++) or on the RTL-accurate core
// (rtl_ref_npu_top.h, or RtlRefLaneACoreA + rtl_ref_sa_array + sram/rtl_ref_sram_top.h with per-tile backdoor
// A/B/preload/C as in tools/fe/sysc/tb_fe_core_tile.cpp). Default: stand-in only. Real core via environment:
//   FE_CORE_STEPS="0"        step indices (0-based, in prog.bin) that run on the real core, comma separated,
//                            ranges allowed, e.g. "0,1,5-8". Empty / unset = all stand-in.
//   FE_CORE_GOLD_INPUT=1     core-mode steps read their inputs from the golden (dram_golden.bin) instead of the
//                            running DRAM, so one layer can be checked on its own (layers can be sharded).
// Input: FE_WORK/step6b/net/{prog.bin, dram_init.bin, dram_golden.bin}.
// Usage: tb_fe_core_net <net_dir> [max_steps]

#include <systemc.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "sauria_types.h"
#include "sram/sram_top.h"
#include "control/sauria_dma.h"
#include "has/gvu_obp.h"
#include "has/gvu_elemwise.h"

// Real core, lane_a backend (as tb_fe_core_tile.cpp): separate rtl_ref SRAM / array / core, backdoor loads.
#include "control/native_lane_a_core.h"
#include "sram/rtl_ref_sram_top.h"
#include "systolic_array/rtl_ref_sa_array.h"
// Real core, npu_top backend (default): rtl_ref_npu_top.h (unmodified port of sauria_model's npu_top.h +
// config_regs.h) driven through its host bus. Select with FE_CORE_BACKEND=npu_top (default) | lane_a.
#include "rtl_ref_npu_top.h"
#ifdef FE_METRICS
// Per-tile performance counters for long runs; off by default (no behaviour change).
#include "instrumentation/rtl_ref_perf_counters.h"
#endif
#include "driver/libsauria_cfg.h"
// sauria_weight_order for the core path (step 2 of run_tile_via_npu_top): the SAURIA weight layout differs
// from the logical [k][cin][kh][kw] layout.
#include "driver/libsauria_mem.h"
#include "sauria_targets.h"

using namespace sauria;

static constexpr int W32 = 32;
typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef SauriaDma<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> DmaT;
typedef has::HasObp<W32> ObpT;
static constexpr uint32_t LUT_BASE = 0x00140000, SCALE_BASE = 0x00180000, SHIFT_BASE = 0x00190000;
static constexpr uint32_t NCH_REG = 0x001A0000 + ObpT::REG_NCH;
static constexpr uint32_t NONE = 0xFFFFFFFF;
static constexpr int OBP_DRAIN_LIMIT = 1000;   // max cycles to drain the HasObp pipeline (latency 6 + 2 tb)

// Knobs read from the prog.bin header (FEHP) -- needed when HasObp / HasElemwise are built, before SC_THREAD run.
static has::Knobs read_knobs(const std::string &dir)
{
    std::ifstream f(dir + "/prog.bin", std::ios::binary);
    char magic[4] = {0};
    uint32_t v[4] = {0, 0, 0, 0};
    f.read(magic, 4);
    f.read(reinterpret_cast<char *>(v), sizeof v);
    if (!f || std::string(magic, 4) != "FEHP")
        throw std::runtime_error("tb_has_net: " + dir + "/prog.bin is not FEHP (use tools/fe/fe_has_export_net.py)");
    has::Knobs k;
    k.round_mode = static_cast<int>(v[0]);
    k.req_narrow = static_cast<int>(v[1]);
    k.deq_zp_order = static_cast<int>(v[2]);
    k.scale_fmt = static_cast<int>(v[3]);
    return k;
}

// The skip (residual) tile goes to the scratch that sram/sram_top.h already has: bank_id 5 = "Lane B PSums (96 KB) +
// Scratch (24 KB)", unused on this one-lane path (bank_id 4 is the PRE/bias bank). Keeping it out of bank_id 3 (the
// other ifmap buffer, category "A") frees that buffer for the ifmap prefetch. Offset = SRAMC_CAP_B * Y_DIM *
// sizeof(T_PSUM) = 768*32*4 = 96 KB (Y_DIM = 32, T_PSUM = int32_t fixed in SramT above); the constant is private in
// sram/sram_top.h, so it is recomputed here and that file is not modified.
static constexpr uint32_t SKIP_SCRATCH_BANK = 5;
static constexpr uint32_t SKIP_SCRATCH_OFFSET = 96 * 1024;
static constexpr uint32_t SKIP_SCRATCH_BYTES = 24 * 1024;

// SRAM capacity per buffer. 5056*32 = 158 KB and 5184*32 = 162 KB are the HAS sizes of BOTH ping-pong buffers
// together, but sauria_rtl::Sram (rtl_ref_sram_top.h, FX1_A3_SRAM_CAP_BYTES) computes ROWS = CAP/(DIM*sizeof(T))
// for EACH buffer, so the capacity passed here is one buffer: 79 / 81 KB, as in the RTL (the address is one
// buffer wide; there is no logic that merges the two banks). For tiles <= 81 KB this is a no-op; it keeps a
// plan that needs more than one buffer from passing only because the simulated memory is larger than the RTL's.
static constexpr int kSramaBytes = 79 * 1024;
static constexpr int kSrambBytes = 81 * 1024;
typedef sauria_rtl::Sram<32, 32, int8_t, int8_t, int32_t, kSramaBytes, kSrambBytes, 1536> CoreSramT;
typedef sauria_rtl::SystolicArray<32, 32, int8_t, int8_t, int32_t> CoreArrayT;
typedef RtlRefLaneACoreA<32, 32, int8_t, int8_t, int32_t, 16, 64, 1, 1536, kSramaBytes, kSrambBytes> RtlRefCoreT;
// Same template parameters as tools/fe/sysc/tb_rtl_ref_npu_top_tile.cpp (validated on the real tile shapes).
typedef sauria_rtl::NpuTop<32, 32, int8_t, int8_t, int32_t, kSramaBytes, kSrambBytes, 1536, 16, 64, 1> NpuT;

struct Tensor { uint32_t addr, c, h, w; };
struct Tile { int c0, c1, oy0, oy1, ox0, ox1, iy0, iy1, ix0, ix1, yu; };
struct Step
{
    uint8_t kind;
    uint32_t in{0}, out{0}, skip{NONE};
    uint8_t silu{0};
    uint32_t cin{0}, cout{0}, kh{0}, kw{0}, sh{0}, sw{0};
    uint32_t w_addr{0}, lut_addr{0}, scale_addr{0}, shift_addr{0}, bias_addr{0};
    std::vector<Tile> tiles;
    uint32_t start{0}, end{0}, k{0}, s{0}, p{0}, factor{0};
    std::vector<uint32_t> ins;
    uint32_t in_b{0};        // elem_add: operand B (skip)
    has::AddParams ap;       // elem_add
};

static bool read_file(const std::string &path, std::vector<uint8_t> &buf)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    buf.resize(static_cast<size_t>(f.tellg()));
    f.seekg(0);
    f.read(reinterpret_cast<char *>(buf.data()), buf.size());
    return static_cast<bool>(f);
}

// "0,2,5-8" -> {0,2,5,6,7,8}. Empty -> empty set (no step on the core).
static std::set<int> parse_step_set(const char *s)
{
    std::set<int> out;
    if (!s || !*s) return out;
    if (std::string(s) == "all") { out.insert(-1); return out; }   // -1 = every conv step
    std::string str(s);
    std::stringstream ss(str);
    std::string tok;
    while (std::getline(ss, tok, ','))
    {
        auto dash = tok.find('-');
        if (dash == std::string::npos) { out.insert(std::atoi(tok.c_str())); continue; }
        int lo = std::atoi(tok.substr(0, dash).c_str()), hi = std::atoi(tok.substr(dash + 1).c_str());
        for (int i = lo; i <= hi; i++) out.insert(i);
    }
    return out;
}

struct Cursor
{
    const std::vector<uint8_t> &b;
    size_t pos{0};
    explicit Cursor(const std::vector<uint8_t> &buf) : b(buf) {}
    uint32_t u32() { uint32_t v; std::memcpy(&v, &b.at(pos + 3) - 3, 4); pos += 4; return v; }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    uint8_t u8() { return b.at(pos++); }
};

SC_MODULE(TbFeCoreNet)
{
    sc_in<bool> i_clk;

    // --- Sram ports (only the host/DMA backdoor is used; accelerator ports tied idle) ---
    sc_signal<bool> rstn{"rstn"}, sram_deepsleep{"sram_deepsleep"}, sram_powergate{"sram_powergate"};
    sc_signal<sc_bv<3>> sram_select{"sram_select"};
    sc_signal<uint32_t> s_host_addr{"s_host_addr"};
    sc_signal<bool> s_host_wren{"s_host_wren"}, s_host_rden{"s_host_rden"};
    sc_signal<host_data_t> s_host_wdata{"s_host_wdata"}, s_host_rdata{"s_host_rdata"};
    sc_signal<host_mask_t> s_host_wmask{"s_host_wmask"};
    sc_signal<uint32_t> a_addr_a{"a_addr_a"}, a_addr_b{"a_addr_b"}, b_addr_a{"b_addr_a"}, b_addr_b{"b_addr_b"};
    sc_signal<bool> a_rden_a{"a_rden_a"}, a_rden_b{"a_rden_b"}, b_rden_a{"b_rden_a"}, b_rden_b{"b_rden_b"};
    sc_signal<act_vector_t<W32, int8_t>> a_data_a{"a_data_a"}, a_data_b{"a_data_b"};
    sc_signal<wei_vector_t<W32, int8_t>> b_data_a{"b_data_a"}, b_data_b{"b_data_b"};
    sc_signal<psum_vector_t<W32, int32_t>> c_wdata_a{"c_wdata_a"}, c_wdata_b{"c_wdata_b"}, c_rdata_a{"c_rdata_a"}, c_rdata_b{"c_rdata_b"};
    sc_signal<uint32_t> c_addr_a{"c_addr_a"}, c_addr_b{"c_addr_b"};
    sc_signal<bool> c_wren_a{"c_wren_a"}, c_wren_b{"c_wren_b"}, c_rden_a{"c_rden_a"}, c_rden_b{"c_rden_b"};
    sc_signal<sramc_mask_t<W32>> c_wmask_a{"c_wmask_a"}, c_wmask_b{"c_wmask_b"};

    // --- Obp ports ---
    sc_signal<psum_vector_t<W32, int32_t>> o_in{"o_in"}, o_out{"o_out"};
    sc_signal<uint32_t> o_in_addr{"o_in_addr"}, o_out_addr{"o_out_addr"};
    sc_signal<sramc_mask_t<W32>> o_in_mask{"o_in_mask"}, o_out_mask{"o_out_mask"};
    sc_signal<bool> o_in_valid{"o_in_valid"}, o_out_wren{"o_out_wren"}, o_out_valid{"o_out_valid"};
    sc_signal<act_vector_t<W32, int8_t>> o_residual{"o_residual"};
    sc_signal<bool> bias_en{"bias_en"}, requant_en{"requant_en"}, lut_en{"lut_en"}, residual_en{"residual_en"}, vec_mode{"vec_mode"};
    sc_signal<uint32_t> def_scale{"def_scale"}, def_shift{"def_shift"};
    sc_signal<uint32_t> host_addr{"host_addr"};
    sc_signal<bool> host_wren{"host_wren"}, host_rden{"host_rden"};
    sc_signal<host_data_t> host_wdata{"host_wdata"}, host_rdata{"host_rdata"};
    sc_signal<host_mask_t> host_wmask{"host_wmask"};

    SramT *sram{nullptr};
    DmaT *dma{nullptr};
    ObpT *obp{nullptr};
    has::Knobs knobs;
    has::HasElemwise *ew{nullptr};

    // --- Real core: its own SRAM port (single lane, backdoor as in tb_fe_core_tile.cpp) ---
    sc_signal<bool> core_sram_rstn{"core_sram_rstn"}, core_arr_rstn{"core_arr_rstn"};
    sc_signal<uint32_t> core_srama_addr{"core_srama_addr"}, core_sramb_addr{"core_sramb_addr"};
    sc_signal<bool> core_srama_rden{"core_srama_rden"}, core_sramb_rden{"core_sramb_rden"};
    sc_signal<act_vector_t<W32, int8_t>> core_srama_data{"core_srama_data"};
    sc_signal<wei_vector_t<W32, int8_t>> core_sramb_data{"core_sramb_data"};
    sc_signal<psum_vector_t<W32, int32_t>> core_sramc_wdata{"core_sramc_wdata"}, core_sramc_rdata{"core_sramc_rdata"};
    sc_signal<uint32_t> core_sramc_addr{"core_sramc_addr"};
    sc_signal<bool> core_sramc_wren{"core_sramc_wren"}, core_sramc_rden{"core_sramc_rden"};
    sc_signal<sramc_mask_t<W32>> core_sramc_wmask{"core_sramc_wmask"};
    sc_signal<act_vector_t<W32, int8_t>> core_act_arr{"core_act_arr"};
    sc_signal<wei_vector_t<W32, int8_t>> core_wei_arr{"core_wei_arr"};
    sc_signal<psum_vector_t<W32, int32_t>> core_sa_to_psm_c{"core_sa_to_psm_c"}, core_psm_to_sa_c{"core_psm_to_sa_c"};
    sc_signal<bool> core_arr_softstall_zero{"core_arr_softstall_zero"}, core_arr_pop_en_dbg_zero{"core_arr_pop_en_dbg_zero"};
    sc_signal<float> core_arr_threshold{"core_arr_threshold"};

    sc_signal<bool> core_rstn{"core_rstn"}, core_soft_reset{"core_soft_reset"}, core_start{"core_start"};
    sc_signal<uint32_t> core_mvm_k{"core_mvm_k"}, core_total_contexts{"core_total_contexts"}, core_nsplit{"core_nsplit"};
    sc_signal<uint32_t> core_incntlim{"core_incntlim"}, core_act_reps{"core_act_reps"}, core_wei_reps{"core_wei_reps"}, core_out_ncontexts{"core_out_ncontexts"};
    sc_signal<uint32_t> core_act_incntlim{"core_act_incntlim"}, core_act_incntstep{"core_act_incntstep"};
    sc_signal<uint32_t> core_act_outcntlim{"core_act_outcntlim"}, core_act_outcntstep{"core_act_outcntstep"};
    sc_signal<sc_bv<64>> core_act_dil_pat{"core_act_dil_pat"};
    sc_signal<uint32_t> core_act_xlim{"core_act_xlim"}, core_act_xstep{"core_act_xstep"};
    sc_signal<uint32_t> core_act_ylim{"core_act_ylim"}, core_act_ystep{"core_act_ystep"};
    sc_signal<uint32_t> core_act_chlim{"core_act_chlim"}, core_act_chstep{"core_act_chstep"};
    sc_signal<uint32_t> core_act_til_xlim{"core_act_til_xlim"}, core_act_til_xstep{"core_act_til_xstep"};
    sc_signal<uint32_t> core_act_til_ylim{"core_act_til_ylim"}, core_act_til_ystep{"core_act_til_ystep"};
    sc_signal<uint32_t> core_act_base_addr{"core_act_base_addr"};
    sc_signal<uint32_t> core_wei_incntlim{"core_wei_incntlim"}, core_wei_incntstep{"core_wei_incntstep"};
    sc_signal<uint32_t> core_wei_wlim{"core_wei_wlim"}, core_wei_wstep{"core_wei_wstep"};
    sc_signal<uint32_t> core_wei_klim{"core_wei_klim"}, core_wei_kstep{"core_wei_kstep"};
    sc_signal<uint32_t> core_wei_til_klim{"core_wei_til_klim"}, core_wei_til_kstep{"core_wei_til_kstep"};
    sc_signal<uint32_t> core_wei_cols_active{"core_wei_cols_active"}, core_wei_waligned{"core_wei_waligned"}, core_wei_base_addr{"core_wei_base_addr"};
    sc_signal<uint32_t> core_cxlim{"core_cxlim"}, core_cxstep{"core_cxstep"}, core_cklim{"core_cklim"}, core_ckstep{"core_ckstep"};
    sc_signal<uint32_t> core_out_til_cylim{"core_out_til_cylim"}, core_out_til_cystep{"core_out_til_cystep"};
    sc_signal<uint32_t> core_out_til_cklim{"core_out_til_cklim"}, core_out_til_ckstep{"core_out_til_ckstep"};
    sc_signal<bool> core_out_preload_en{"core_out_preload_en"};
    sc_signal<uint32_t> core_out_base_addr{"core_out_base_addr"};
    sc_signal<sramc_mask_t<W32>> core_rows_active{"core_rows_active"};
    sc_signal<uint32_t> core_inactive_cols{"core_inactive_cols"};
    sc_signal<bool> core_pipeline_en{"core_pipeline_en"}, core_cscan_en{"core_cscan_en"}, core_sa_clear{"core_sa_clear"};
    sc_signal<sc_bv<32>> core_cswitch_arr{"core_cswitch_arr"};
    sc_signal<uint32_t> core_context_id{"core_context_id"};
    sc_signal<bool> core_done{"core_done"}, core_deadlock{"core_deadlock"}, core_active{"core_active"};

    CoreSramT *core_sram{nullptr};
    CoreArrayT *core_array{nullptr};
    RtlRefCoreT *core{nullptr};

    // --- npu_top backend: real core through its own host bus (rtl_ref_npu_top). Separate signals, not shared with
    // the host bus of the v4.5 Sram/Obp above (those are bound to sram/obp). ---
    sc_signal<bool> npu_rstn{"npu_rstn"}, npu_soft_reset{"npu_soft_reset"}, npu_start{"npu_start"};
    sc_signal<bool> npu_done{"npu_done"}, npu_deadlock{"npu_deadlock"};
    sc_signal<uint32_t> npu_mvm_k{"npu_mvm_k"}, npu_total_contexts{"npu_total_contexts"};
    sc_signal<uint32_t> npu_host_addr{"npu_host_addr"};
    sc_signal<bool> npu_host_wren{"npu_host_wren"}, npu_host_rden{"npu_host_rden"};
    sc_signal<host_data_t> npu_host_wdata{"npu_host_wdata"}, npu_host_rdata{"npu_host_rdata"};
    sc_signal<host_mask_t> npu_host_wmask{"npu_host_wmask"};
    sc_signal<float> npu_threshold{"npu_threshold"};
    sc_signal<sc_bv<3>> npu_select{"npu_select"};
    NpuT *npu{nullptr};
#ifdef FE_METRICS
    sauria_rtl::PerfCounters fe_perf_;   // accumulated over the run; each tile records the before/after difference
    std::FILE *fe_mcsv_{nullptr};
#endif
    static const int NPU_SUBWORDS_A = 32 / 4, NPU_SUBWORDS_B = 32 / 4, NPU_SUBWORDS_C = 32 / 4;
    // Backend of the real-core mode: npu_top (default) or lane_a (older path, fallback).
    bool use_npu_top{true};
    int core_max_tiles{-1};
    // FE_CORE_SHAPE_N=N: the first N occurrences (over the whole run) of each tile SHAPE (cin,kh,kw,sy,nch,ht,wt,yu)
    // run on the core, all other tiles on the stand-in (bit-exact with the golden).
    int shape_n{0};
    std::map<std::string, uint64_t> shape_core_count;   // key -> times run on the core
    std::map<std::string, uint64_t> shape_seen_count;   // key -> tiles seen in the selected steps
    uint64_t tiles_dumped{0};
    std::string acc_dump_path;
    long long npu_deadlock_pulses{0};
    int npu_cycles_last{0};

    std::string dir;
    int max_steps{-1};
    std::vector<uint8_t> dram, gold;
    uint64_t obp_rows_written{0};
    int exit_code{1};
    std::set<int> core_steps;
    bool core_gold_input{false};
    int step_first{0};
    std::string snapshot_dir;      // FE_SNAPSHOT_DIR: DRAM snapshot after every layer
    std::string resume_from;       // FE_RESUME_FROM: load a snapshot instead of dram_init
    int core_max_cycles{2000000};

    SC_HAS_PROCESS(TbFeCoreNet);
    TbFeCoreNet(sc_module_name n, const std::string &d, int ms) : sc_module(n), dir(d), max_steps(ms)
    {
        knobs = read_knobs(d);
        ew = new has::HasElemwise(knobs);
        std::printf("[tb_has_net] knobs %s\n", knobs.str().c_str());
        core_steps = parse_step_set(getenv("FE_CORE_STEPS"));
        core_gold_input = getenv("FE_CORE_GOLD_INPUT") != nullptr;
        // FE_STEP_FIRST=K: skip steps < K (per-layer shards). Only meaningful with FE_CORE_GOLD_INPUT, where
        // core steps read every input (skip tensors included) from the golden and so do not depend on earlier
        // steps. Tensors not written in the shard are not compared: each shard has its own verdict.
        if (const char *sf = getenv("FE_STEP_FIRST")) step_first = std::atoi(sf);
        // FE_SNAPSHOT_DIR=<dir>: write a DRAM snapshot after EVERY layer (overwritten, one image) plus
        // last_step.txt. FE_RESUME_FROM=<dir> loads that snapshot instead of dram_init.bin; with
        // FE_STEP_FIRST=<next step> an interrupted run continues where it stopped.
        if (const char *sd = getenv("FE_SNAPSHOT_DIR")) snapshot_dir = sd;
        if (const char *rf = getenv("FE_RESUME_FROM")) resume_from = rf;
        if (const char *mc = getenv("FE_CORE_MAX_CYCLES")) core_max_cycles = std::atoi(mc);
        // Backend of the real-core mode. Default npu_top (real host bus);
        // FE_CORE_BACKEND=lane_a returns to the older RtlRefLaneACoreA path for comparison.
        if (const char *bk = getenv("FE_CORE_BACKEND")) use_npu_top = (std::string(bk) != "lane_a");
        // Small reproducers: FE_CORE_MAX_TILES=N sends only the first N tiles through the real core, the rest
        // through the stand-in. FE_CORE_DUMP_ACC=<file> writes acc[] of every core tile (raw int64) for a
        // comparison with expect_psum.bin.
        if (const char *mt = getenv("FE_CORE_MAX_TILES")) core_max_tiles = std::atoi(mt);
        if (const char *sn = getenv("FE_CORE_SHAPE_N")) shape_n = std::atoi(sn);
        if (const char *da = getenv("FE_CORE_DUMP_ACC")) acc_dump_path = da;

        sram = new SramT("sram");
        sram->i_clk(i_clk); sram->i_rstn(rstn); sram->i_deepsleep(sram_deepsleep); sram->i_powergate(sram_powergate);
        sram->i_select(sram_select);
        sram->i_host_addr(s_host_addr); sram->i_host_wren(s_host_wren); sram->i_host_rden(s_host_rden);
        sram->i_host_wdata(s_host_wdata); sram->i_host_wmask(s_host_wmask); sram->o_host_rdata(s_host_rdata);
        sram->i_srama_addr_a(a_addr_a); sram->i_srama_rden_a(a_rden_a); sram->o_srama_data_a(a_data_a);
        sram->i_srama_addr_b(a_addr_b); sram->i_srama_rden_b(a_rden_b); sram->o_srama_data_b(a_data_b);
        sram->i_sramb_addr_a(b_addr_a); sram->i_sramb_rden_a(b_rden_a); sram->o_sramb_data_a(b_data_a);
        sram->i_sramb_addr_b(b_addr_b); sram->i_sramb_rden_b(b_rden_b); sram->o_sramb_data_b(b_data_b);
        sram->i_sramc_wdata_a(c_wdata_a); sram->i_sramc_addr_a(c_addr_a); sram->i_sramc_wren_a(c_wren_a);
        sram->i_sramc_rden_a(c_rden_a); sram->i_sramc_wmask_a(c_wmask_a); sram->o_sramc_rdata_a(c_rdata_a);
        sram->i_sramc_wdata_b(c_wdata_b); sram->i_sramc_addr_b(c_addr_b); sram->i_sramc_wren_b(c_wren_b);
        sram->i_sramc_rden_b(c_rden_b); sram->i_sramc_wmask_b(c_wmask_b); sram->o_sramc_rdata_b(c_rdata_b);

        dma = new DmaT("dma");
        dma->i_clk(i_clk); dma->i_rstn(rstn);
        dma->set_sram(sram);
        dma->set_dram(&dram);

        obp = new ObpT("obp", knobs);
        obp->i_clk(i_clk); obp->i_rstn(rstn);
        obp->i_data(o_in); obp->i_addr(o_in_addr); obp->i_wmask(o_in_mask); obp->i_valid(o_in_valid); obp->i_residual(o_residual);
        obp->o_sramc_wdata(o_out); obp->o_sramc_addr(o_out_addr); obp->o_sramc_wren(o_out_wren); obp->o_sramc_wmask(o_out_mask);
        obp->o_valid(o_out_valid);
        obp->i_bias_en(bias_en); obp->i_requant_en(requant_en); obp->i_lut_en(lut_en); obp->i_residual_en(residual_en);
        obp->i_vec_channel_mode(vec_mode); obp->i_requant_scale(def_scale); obp->i_requant_shift(def_shift);
        obp->i_host_addr(host_addr); obp->i_host_wren(host_wren); obp->i_host_rden(host_rden);
        obp->i_host_wdata(host_wdata); obp->i_host_wmask(host_wmask); obp->o_host_rdata(host_rdata);

        core_sram = new CoreSramT("core_sram");
        core_sram->i_clk(i_clk); core_sram->i_rstn(core_sram_rstn);
        core_sram->i_deepsleep(sram_deepsleep); core_sram->i_powergate(sram_powergate); core_sram->i_select(sram_select);
        core_sram->i_host_addr(s_host_addr); core_sram->i_host_wren(s_host_wren); core_sram->i_host_rden(s_host_rden);
        core_sram->i_host_wdata(s_host_wdata); core_sram->i_host_wmask(s_host_wmask);
        {
            static sc_signal<host_data_t> unused_core_host_rdata("unused_core_host_rdata");
            core_sram->o_host_rdata(unused_core_host_rdata);
        }
        core_sram->i_srama_addr(core_srama_addr); core_sram->i_srama_rden(core_srama_rden); core_sram->o_srama_data(core_srama_data);
        core_sram->i_sramb_addr(core_sramb_addr); core_sram->i_sramb_rden(core_sramb_rden); core_sram->o_sramb_data(core_sramb_data);
        core_sram->i_sramc_wdata(core_sramc_wdata); core_sram->i_sramc_addr(core_sramc_addr); core_sram->i_sramc_wren(core_sramc_wren);
        core_sram->i_sramc_rden(core_sramc_rden); core_sram->i_sramc_wmask(core_sramc_wmask); core_sram->o_sramc_rdata(core_sramc_rdata);

        core_array = new CoreArrayT("core_array");
        core_array->i_clk(i_clk); core_array->i_rstn(core_arr_rstn);
        core_array->i_threshold(core_arr_threshold);
        core_array->i_act_arr(core_act_arr); core_array->i_wei_arr(core_wei_arr);
        core_array->i_c_arr(core_psm_to_sa_c); core_array->o_c_arr(core_sa_to_psm_c);
        core_array->i_pipeline_en(core_pipeline_en); core_array->i_cscan_en(core_cscan_en);
        core_array->i_cswitch_arr(core_cswitch_arr); core_array->i_sa_clear(core_sa_clear);
        core_array->i_softstall(core_arr_softstall_zero); core_array->i_pop_en_dbg(core_arr_pop_en_dbg_zero);
        core_array->i_context_id(core_context_id);

        core = new RtlRefCoreT("core_NpuTop_std_core");
        core->i_clk(i_clk); core->i_rstn(core_rstn); core->i_soft_reset(core_soft_reset); core->i_start(core_start);
        core->i_mvm_k(core_mvm_k); core->i_total_contexts(core_total_contexts); core->i_nsplit(core_nsplit);
        core->i_incntlim(core_incntlim); core->i_act_reps(core_act_reps); core->i_wei_reps(core_wei_reps);
        core->i_out_ncontexts(core_out_ncontexts);
        core->i_act_incntlim(core_act_incntlim); core->i_act_incntstep(core_act_incntstep);
        core->i_act_outcntlim(core_act_outcntlim); core->i_act_outcntstep(core_act_outcntstep);
        core->i_act_dil_pat(core_act_dil_pat);
        core->i_act_xlim(core_act_xlim); core->i_act_xstep(core_act_xstep);
        core->i_act_ylim(core_act_ylim); core->i_act_ystep(core_act_ystep);
        core->i_act_chlim(core_act_chlim); core->i_act_chstep(core_act_chstep);
        core->i_act_til_xlim(core_act_til_xlim); core->i_act_til_xstep(core_act_til_xstep);
        core->i_act_til_ylim(core_act_til_ylim); core->i_act_til_ystep(core_act_til_ystep);
        core->i_act_base_addr(core_act_base_addr);
        core->i_wei_incntlim(core_wei_incntlim); core->i_wei_incntstep(core_wei_incntstep);
        core->i_wei_wlim(core_wei_wlim); core->i_wei_wstep(core_wei_wstep);
        core->i_wei_klim(core_wei_klim); core->i_wei_kstep(core_wei_kstep);
        core->i_wei_til_klim(core_wei_til_klim); core->i_wei_til_kstep(core_wei_til_kstep);
        core->i_wei_cols_active(core_wei_cols_active); core->i_wei_waligned(core_wei_waligned);
        core->i_wei_base_addr(core_wei_base_addr);
        core->i_cxlim(core_cxlim); core->i_cxstep(core_cxstep); core->i_cklim(core_cklim); core->i_ckstep(core_ckstep);
        core->i_out_til_cylim(core_out_til_cylim); core->i_out_til_cystep(core_out_til_cystep);
        core->i_out_til_cklim(core_out_til_cklim); core->i_out_til_ckstep(core_out_til_ckstep);
        core->i_out_preload_en(core_out_preload_en); core->i_out_base_addr(core_out_base_addr);
        core->i_rows_active(core_rows_active);
#ifdef FX1_A3_PSM_INACTIVE_COLS
        core->i_inactive_cols(core_inactive_cols);
#endif
        core->o_srama_addr(core_srama_addr); core->o_srama_rden(core_srama_rden); core->i_srama_data(core_srama_data);
        core->o_sramb_addr(core_sramb_addr); core->o_sramb_rden(core_sramb_rden); core->i_sramb_data(core_sramb_data);
        core->o_sramc_addr(core_sramc_addr); core->o_sramc_wren(core_sramc_wren); core->o_sramc_rden(core_sramc_rden);
        core->o_sramc_wmask(core_sramc_wmask); core->o_sramc_wdata(core_sramc_wdata); core->i_sramc_rdata(core_sramc_rdata);
        core->o_act_arr(core_act_arr); core->o_wei_arr(core_wei_arr);
        core->i_c_arr(core_sa_to_psm_c); core->o_c_arr(core_psm_to_sa_c);
        core->o_pipeline_en(core_pipeline_en); core->o_cscan_en(core_cscan_en); core->o_cswitch_arr(core_cswitch_arr);
        core->o_sa_clear(core_sa_clear); core->o_context_id(core_context_id);
        core->o_done(core_done); core->o_deadlock(core_deadlock); core->o_active(core_active);

        // --- Real NpuTop (unmodified npu_top.h port). The instance name must contain "NpuTop_std":
        // the tape switch in rtl_ref_ifmap_feeder.h is gated on this name. ---
        npu = new NpuT("NpuTop_std");
#ifdef FE_METRICS
        npu->attach_perf(&fe_perf_);
        {
            const char *mp = getenv("FE_METRICS_CSV");
            // Append ("a"), not "w": a rerun into the same path keeps what an interrupted run wrote (each tile line is complete).
            fe_mcsv_ = std::fopen(mp ? mp : "metrics_tiles.csv", "a");
            if (fe_mcsv_)
            {
                std::fseek(fe_mcsv_, 0, SEEK_END);
                const bool fresh = (std::ftell(fe_mcsv_) == 0);
                // Metadata: one line per start (to check that the FIFO depth did not change between runs).
                // Counting window = ONE run_tile_via_npu_top call (reset, SRAM load, host-bus config, run, read C).
                std::fprintf(fe_mcsv_, "# meta,window=run_tile_via_npu_top_call,fifo=%s,backdoor=%s,launch=%s\n",
#ifdef FX1_A3_FIFO_DEPTH_SPLIT
                             "act5_wei4",
#else
                             "fifo16",
#endif
                             getenv("FE_SRAM_BACKDOOR") ? "on" : "off", fresh ? "first" : "resume");
                if (fresh)
                {
                    std::fprintf(fe_mcsv_, "step,tile,cin,kh,kw,sy,nch,ht,wt,yu,n_ctx,ok,ticks_all,busy,exec,pe_cycles,mac_nz,macs_theory,"
                                           "a_rd_beats,a_rd_bytes,b_rd_beats,b_rd_bytes,c_rd_beats,c_rd_bytes,c_wr_beats,c_wr_bytes");
                    for (int q = 1; q <= 24; q++) std::fprintf(fe_mcsv_, ",st%02d", q);
                    std::fprintf(fe_mcsv_, "\n");
                }
                std::fflush(fe_mcsv_);
            }
        }
#endif
        npu->i_clk(i_clk);
        npu->i_rstn(npu_rstn);
        npu->i_soft_reset(npu_soft_reset);
        npu->i_start(npu_start);
        npu->o_done(npu_done);
        npu->o_deadlock(npu_deadlock);
        npu->i_mvm_k(npu_mvm_k);
        npu->i_host_addr(npu_host_addr);
        npu->i_host_wren(npu_host_wren);
        npu->i_host_rden(npu_host_rden);
        npu->i_host_wdata(npu_host_wdata);
        npu->i_host_wmask(npu_host_wmask);
        npu->o_host_rdata(npu_host_rdata);
        npu->i_threshold(npu_threshold);
        npu->i_select(npu_select);
        npu->i_total_contexts(npu_total_contexts);

        SC_THREAD(run);
        sensitive << i_clk.pos();
        SC_METHOD(obp_writeback);
        sensitive << i_clk.pos();
        dont_initialize();
    }
    ~TbFeCoreNet() { delete core; delete core_array; delete core_sram; delete obp; delete ew; delete dma; delete sram; }

    // OBP output vectors go to PSUM SRAM bank 4 (HAS 6.9: final INT8 written into PSUM SRAM) in C-order
    // [k, h, w], 32 elements per word; the vector id (ctx * nch + x) locates the row segment of the context.
    uint32_t cur_ht{1}, cur_wt{1}, cur_nch{1}, cur_yu{1};
    void obp_writeback()
    {
        if (!o_out_wren.read()) return;
        uint32_t id = o_out_addr.read(), ctx = id / cur_nch, x = id % cur_nch;
        uint32_t oy = ctx / (cur_wt / cur_yu), cx0 = (ctx % (cur_wt / cur_yu)) * cur_yu;
        const auto &v = o_out.read();
        const auto &m = o_out_mask.read();
        for (uint32_t k = 0; k < cur_yu; k++)
            if (m[k])
            {
                uint32_t e = (x * cur_ht + oy) * cur_wt + cx0 + k;
                int32_t val = v[k];
                sram->write_bank_data(4, e * 4, reinterpret_cast<const uint8_t *>(&val), 4);
            }
        obp_rows_written++;
    }

    void host_write(uint32_t addr, const host_data_t &d)
    {
        host_mask_t m;
        m.data.fill(true);
        host_addr.write(addr); host_wdata.write(d); host_wmask.write(m); host_wren.write(true);
        wait();
        host_wren.write(false);
        wait();
    }
    void write_u32(uint32_t addr, uint32_t v) { host_data_t d; d[0] = static_cast<double>(v); host_write(addr, d); }
    uint32_t dram_u32(uint32_t a) { uint32_t v; std::memcpy(&v, &dram[a], 4); return v; }
    int32_t dram_i32(uint32_t a) { return static_cast<int32_t>(dram_u32(a)); }
    int8_t &t8(const Tensor &t, uint32_t c, uint32_t y, uint32_t x) { return reinterpret_cast<int8_t &>(dram[t.addr + (c * t.h + y) * t.w + x]); }
    int8_t t8g(const Tensor &t, uint32_t c, uint32_t y, uint32_t x) { return static_cast<int8_t>(gold[t.addr + (c * t.h + y) * t.w + x]); }

    void wait_dma(uint64_t &cycles)
    {
        while (dma->is_any_read_active() || dma->is_write_active()) { wait(); cycles++; }
    }

    // The ifmap (channel 1) of the NEXT tile is prefetched while the CURRENT tile computes + runs the OBP.
    // wait_dma_excl_ch1: wait for every channel EXCEPT 1 (after issuing the current tile's DMA, and at writeback),
    // since channel 1 may be prefetching for the next tile and is not needed yet.
    void wait_dma_excl_ch1(uint64_t &cycles)
    {
        while (dma->is_write_active() || dma->is_read_active(0) || dma->is_read_active(2) || dma->is_read_active(3))
        { wait(); cycles++; }
    }
    // wait_ch1: wait for channel 1 only -- called right before the bank-A backdoor read; nearly instant when the
    // prefetch issued by the previous tile has finished.
    void wait_ch1(uint64_t &cycles)
    {
        while (dma->is_read_active(1)) { wait(); cycles++; }
    }

    // Same as tools/fe/sysc/tb_fe_core_tile.cpp::apply_layer_cfg.
    void apply_core_cfg(const SauriaLayerDesc &desc, const SauriaTarget &target)
    {
        uint64_t f[F_CFG_COUNT];
        sauria_compute_core_fields(desc, target, f);
        core_incntlim.write((uint32_t)f[F_CFG_INCNTLIM]);
        core_act_reps.write((uint32_t)f[F_CFG_ACT_REPS]);
        core_wei_reps.write((uint32_t)f[F_CFG_WEI_REPS]);
        core_act_incntlim.write((uint32_t)f[F_CFG_INCNTLIM] + 1);
        core_act_incntstep.write((uint32_t)target.Y);
        core_act_outcntlim.write((uint32_t)f[F_CFG_XLIM]);
        core_act_outcntstep.write((uint32_t)target.Y);
        core_act_dil_pat.write(sc_bv<64>((uint64_t)f[F_CFG_DIL_PAT]));
        core_act_xlim.write((uint32_t)f[F_CFG_XLIM]);
        core_act_xstep.write((uint32_t)f[F_CFG_XSTEP]);
        core_act_ylim.write((uint32_t)f[F_CFG_YLIM]);
        core_act_ystep.write((uint32_t)f[F_CFG_YSTEP]);
        core_act_chlim.write((uint32_t)f[F_CFG_CHLIM]);
        core_act_chstep.write((uint32_t)f[F_CFG_CHSTEP]);
        core_act_til_xlim.write((uint32_t)f[F_CFG_TIL_XLIM]);
        core_act_til_xstep.write((uint32_t)f[F_CFG_TIL_XSTEP]);
        core_act_til_ylim.write((uint32_t)f[F_CFG_TIL_YLIM]);
        core_act_til_ystep.write((uint32_t)f[F_CFG_TIL_YSTEP]);
        core_act_base_addr.write(0);
        core_wei_incntlim.write((uint32_t)f[F_CFG_WLIM]);
        core_wei_incntstep.write((uint32_t)target.X);
        core_wei_wlim.write((uint32_t)f[F_CFG_WLIM]);
        core_wei_wstep.write((uint32_t)f[F_CFG_WSTEP]);
        core_wei_klim.write((uint32_t)f[F_CFG_KLIM]);
        core_wei_kstep.write((uint32_t)f[F_CFG_KSTEP]);
        core_wei_til_klim.write((uint32_t)f[F_CFG_TIL_KLIM]);
        core_wei_til_kstep.write((uint32_t)f[F_CFG_TIL_KSTEP]);
        core_wei_cols_active.write((uint32_t)f[F_CFG_COLS_ACTIVE]);
        core_wei_waligned.write((uint32_t)f[F_CFG_WALIGNED]);
        core_wei_base_addr.write(0);
        core_out_ncontexts.write((uint32_t)f[F_CFG_NCONTEXTS]);
        core_total_contexts.write((uint32_t)f[F_CFG_NCONTEXTS]);
        core_cxlim.write((uint32_t)f[F_CFG_CXLIM]);
        core_cxstep.write((uint32_t)f[F_CFG_CXSTEP]);
        core_cklim.write((uint32_t)f[F_CFG_CKLIM]);
        core_ckstep.write((uint32_t)f[F_CFG_CKSTEP]);
        core_out_til_cylim.write((uint32_t)f[F_CFG_TIL_CYLIM]);
        core_out_til_cystep.write((uint32_t)f[F_CFG_TIL_CYSTEP]);
        core_out_til_cklim.write((uint32_t)f[F_CFG_TIL_CKLIM]);
        core_out_til_ckstep.write((uint32_t)f[F_CFG_TIL_CKSTEP]);
        core_out_base_addr.write(0);
        {
            sramc_mask_t<32> ra(false);
            uint64_t bits = f[F_CFG_ROWS_ACTIVE];
            for (int i = 0; i < 32; i++) ra[i] = ((bits >> i) & 1ULL) != 0;
            core_rows_active.write(ra);
        }
        core_mvm_k.write(1);
        core_nsplit.write(32);
        core_out_preload_en.write(true);
#ifdef FX1_A3_PSM_INACTIVE_COLS
        // Inactive PSM columns = X - X_used (see the full note in tools/fe/sysc/tb_fe_core_tile.cpp).
        core_inactive_cols.write((uint32_t)target.X - (uint32_t)desc.X_used);
#endif
    }

    void tick(int n = 1) { for (int i = 0; i < n; i++) wait(); }

    // Run ONE tile on the real core: load A/B/preload through the backdoor into core_sram, configure, run until
    // done (cycle budget or all elements), capture the PSM->SRAM-C writes into acc[] (preload included, same as
    // tb_fe_core_tile.cpp's expect_psum.bin). Element index = addr*32 + lane, C-order [k,h,w] (CKSTEP = w*h).
    // ================= npu_top backend: real core through the host bus =================
    // Same sequence and configuration formulas as tools/fe/sysc/tb_rtl_ref_npu_top_tile.cpp.

    void npu_wr(uint32_t addr, uint32_t val)
    {
        host_data_t d; host_mask_t m;
        d[0] = (double)val; m[0] = true;
        npu_host_addr.write(addr); npu_host_wdata.write(d); npu_host_wmask.write(m);
        npu_host_wren.write(true); npu_host_rden.write(false);
        wait();
        npu_host_wren.write(false);
        wait();
    }

    void npu_wr_rows_active(uint32_t y_used)
    {
        uint64_t bits = (y_used >= 32) ? 0xFFFFFFFFu : ((1u << y_used) - 1u);
        host_data_t d; host_mask_t m;
        for (int b = 0; b < 4; b++) { d[b] = (double)((bits >> (b * 8)) & 0xFF); m[b] = true; }
        npu_host_addr.write(CFG_ACT_OFFSET + 0x00); npu_host_wdata.write(d); npu_host_wmask.write(m);
        npu_host_wren.write(true); npu_host_rden.write(false);
        wait();
        npu_host_wren.write(false);
        wait();
    }

    void npu_wr_cols_active(uint64_t bits64)
    {
        host_data_t d; host_mask_t m;
        d[0] = (double)(uint32_t)(bits64 & 0xFFFFFFFFu); m[0] = true;
        d[1] = (double)(uint32_t)(bits64 >> 32); m[1] = true;
        npu_host_addr.write(WEI_COLS_ACTIVE); npu_host_wdata.write(d); npu_host_wmask.write(m);
        npu_host_wren.write(true); npu_host_rden.write(false);
        wait();
        npu_host_wren.write(false);
        wait();
    }

    void npu_load_i8(uint32_t region_offset, int subwords, int dim_per_row,
                     const uint8_t *data, size_t nbytes)
    {
        size_t nrows = (nbytes + (size_t)dim_per_row - 1) / (size_t)dim_per_row;
        for (size_t row = 0; row < nrows; row++)
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
                    m[lane] = true; any = true;
                }
                if (!any) continue;
                npu_host_addr.write(region_offset + (uint32_t)row * subwords + sw);
                npu_host_wdata.write(d); npu_host_wmask.write(m);
                npu_host_wren.write(true); npu_host_rden.write(false);
                wait();
                npu_host_wren.write(false);
                wait();
            }
    }

    void npu_load_i32(uint32_t region_offset, int subwords, int dim_per_row,
                      const uint8_t *data, size_t nbytes)
    {
        size_t total_elems = nbytes / sizeof(int32_t);
        size_t nrows = (total_elems + (size_t)dim_per_row - 1) / (size_t)dim_per_row;
        for (size_t row = 0; row < nrows; row++)
            for (int sw = 0; sw < subwords; sw++)
            {
                host_data_t d; host_mask_t m; bool any = false;
                for (int lane = 0; lane < 4; lane++)
                {
                    int elem = sw * 4 + lane;
                    if (elem >= dim_per_row) continue;
                    size_t ge = row * (size_t)dim_per_row + (size_t)elem;
                    if (ge >= total_elems) continue;
                    int32_t v; std::memcpy(&v, data + ge * sizeof(int32_t), sizeof(int32_t));
                    d[lane] = (double)v; m[lane] = true; any = true;
                }
                if (!any) continue;
                npu_host_addr.write(region_offset + (uint32_t)row * subwords + sw);
                npu_host_wdata.write(d); npu_host_wmask.write(m);
                npu_host_wren.write(true); npu_host_rden.write(false);
                wait();
                npu_host_wren.write(false);
                wait();
            }
    }

    void npu_apply_cfg(const SauriaLayerDesc &desc, const SauriaTarget &target)
    {
        uint64_t f[F_CFG_COUNT];
        sauria_compute_core_fields(desc, target, f);

        npu_wr(CFG_CON_OFFSET + 0x00, (uint32_t)f[F_CFG_INCNTLIM]);
        npu_wr(CFG_CON_OFFSET + 0x04, (uint32_t)f[F_CFG_ACT_REPS]);
        npu_wr(CFG_CON_OFFSET + 0x08, (uint32_t)f[F_CFG_WEI_REPS]);

        npu_wr_rows_active((uint32_t)desc.Y_used);
        npu_wr(CFG_ACT_OFFSET + 0x04, (uint32_t)f[F_CFG_INCNTLIM] + 1);
        npu_wr(CFG_ACT_OFFSET + 0x08, (uint32_t)target.Y);
        npu_wr(CFG_ACT_OFFSET + 0x0C, (uint32_t)f[F_CFG_XLIM]);
        npu_wr(CFG_ACT_OFFSET + 0x10, (uint32_t)target.Y);
        npu_wr(CFG_ACT_OFFSET + 0x28, (uint32_t)(f[F_CFG_DIL_PAT] & 0xFFFFFFFFu));
        // DIL_PAT is MSB-first over 64 bits: HIGH32 must always be written.
        npu_wr(CFG_ACT_OFFSET + 0x40, (uint32_t)(f[F_CFG_DIL_PAT] >> 32));
        npu_wr(CFG_ACT_OFFSET + 0x44, (uint32_t)desc.s);
        npu_wr(CFG_ACT_OFFSET + 0x14, (uint32_t)f[F_CFG_XLIM]);
        npu_wr(CFG_ACT_OFFSET + 0x18, (uint32_t)f[F_CFG_XSTEP]);
        npu_wr(CFG_ACT_OFFSET + 0x1C, (uint32_t)f[F_CFG_YLIM]);
        npu_wr(CFG_ACT_OFFSET + 0x20, (uint32_t)f[F_CFG_YSTEP]);
        npu_wr(CFG_ACT_OFFSET + 0x24, (uint32_t)f[F_CFG_CHLIM]);
        npu_wr(CFG_ACT_OFFSET + 0x2C, (uint32_t)f[F_CFG_CHSTEP]);
        npu_wr(CFG_ACT_OFFSET + 0x30, (uint32_t)f[F_CFG_TIL_XLIM]);
        npu_wr(CFG_ACT_OFFSET + 0x34, (uint32_t)f[F_CFG_TIL_XSTEP]);
        npu_wr(CFG_ACT_OFFSET + 0x38, (uint32_t)f[F_CFG_TIL_YLIM]);
        npu_wr(CFG_ACT_OFFSET + 0x3C, (uint32_t)f[F_CFG_TIL_YSTEP]);

        npu_wr(CFG_WEI_OFFSET + 0x04, (uint32_t)f[F_CFG_WLIM]);
        npu_wr(CFG_WEI_OFFSET + 0x08, (uint32_t)target.X);
        npu_wr(CFG_WEI_OFFSET + 0x10, (uint32_t)f[F_CFG_WLIM]);
        npu_wr(CFG_WEI_OFFSET + 0x14, (uint32_t)f[F_CFG_WSTEP]);
        npu_wr(CFG_WEI_OFFSET + 0x18, (uint32_t)f[F_CFG_KLIM]);
        npu_wr(CFG_WEI_OFFSET + 0x1C, (uint32_t)f[F_CFG_KSTEP]);
        npu_wr(CFG_WEI_OFFSET + 0x20, (uint32_t)f[F_CFG_TIL_KLIM]);
        npu_wr(CFG_WEI_OFFSET + 0x24, (uint32_t)f[F_CFG_TIL_KSTEP]);
        npu_wr_cols_active(f[F_CFG_COLS_ACTIVE]);
        npu_wr(CFG_WEI_OFFSET + 0x2C, (uint32_t)f[F_CFG_WALIGNED]);

        npu_wr(NCONTEXTS, (uint32_t)f[F_CFG_NCONTEXTS]);
        npu_wr(CFG_OUT_OFFSET + 0x04, (uint32_t)f[F_CFG_CXLIM]);
        npu_wr(CFG_OUT_OFFSET + 0x08, (uint32_t)f[F_CFG_CXSTEP]);
        npu_wr(CFG_OUT_OFFSET + 0x0C, (uint32_t)f[F_CFG_CKLIM]);
        npu_wr(CFG_OUT_OFFSET + 0x10, (uint32_t)f[F_CFG_CKSTEP]);
        npu_wr(TIL_CYLIM, (uint32_t)f[F_CFG_TIL_CYLIM]);
        npu_wr(TIL_CYSTEP, (uint32_t)f[F_CFG_TIL_CYSTEP]);
        npu_wr(TIL_CKLIM, (uint32_t)f[F_CFG_TIL_CKLIM]);
        npu_wr(TIL_CKSTEP, (uint32_t)f[F_CFG_TIL_CKSTEP]);
        npu_wr(INACTIVE_COLS, (uint32_t)f[F_CFG_INACTIVE_COLS]);
        npu_wr(PRELOAD_EN, (uint32_t)f[F_CFG_PRELOAD_EN]);

        npu_wr(CFG_ACT_BASE_ADDR, 0);
        npu_wr(CFG_WEI_BASE_ADDR, 0);
        npu_wr(CFG_OUT_BASE_ADDR, 0);
    }

    // Run ONE tile on the real NpuTop. Same inputs / outputs as run_tile_via_core: takes A/B/preload already laid out,
    // returns acc[] by element index = addr*32 + lane.
    bool run_tile_via_npu_top(const std::vector<int8_t> &a, uint32_t cin, uint32_t ah, uint32_t aw,
                              const std::vector<int8_t> &wgt, uint32_t nch, uint32_t kh, uint32_t kw,
                              const std::vector<int32_t> &pre, uint32_t ht, uint32_t wt, uint32_t sy,
                              uint32_t x_used, uint32_t y_used, std::vector<int64_t> &acc)
    {
        (void)ah; (void)aw;
        const SauriaTarget *t = sauria_find_target("int8_32x32");
        const uint32_t nel = nch * ht * wt;
        // A tile is "wide" when its weights exceed ONE buffer (kSrambBytes = 81 KB, see above). Derived from the
        // geometry (nch*K), no extra FENP field -- the same criterion fe_tile_plan.py uses.
        const uint32_t Kw = cin * kh * kw;
        const bool wide_weight = (uint64_t)nch * Kw > (uint64_t)kSrambBytes;
        // Rows of ONE real buffer (X_DIM*sizeof(T_WEI) = 32 bytes per row) -- the real buffer boundary
        // where i_select is switched while reading.
        const uint32_t rows_per_wei_buffer = (uint32_t)kSrambBytes / 32;

        // --- 1. Reset, select=000 (host nhin buffer 0) ---
        npu_rstn.write(false); npu_soft_reset.write(false); npu_start.write(false);
        npu_host_wren.write(false); npu_host_rden.write(false); npu_threshold.write(0.5f);
        npu_mvm_k.write(1); npu_total_contexts.write(1); npu_select.write(sc_bv<3>("000"));
        tick(5);
        npu_rstn.write(true);
        tick(2);
        npu_wr(CFG_PROFILE_ADDR, (uint32_t)PROFILE_V1_SAURIA);

        // --- 2. Load SRAM-A/B/C ---
        // The only thing `sauria_assemble_dram()` does beyond a plain copy is `sauria_weight_order()` on B
        // (driver/libsauria_mem.h:193-197): the SAURIA weight order differs from the logical [k][cin][kh][kw]
        // layout. A and C are C-order flattens packed at 8/32 bit, i.e. byte-identical to the source buffers.
        // Loading B without the reorder gives systematically wrong psums. assemble_dram is not called here
        // because it re-encodes A + B + C element by element (sauria_enc_elem / sauria_pack_bits,
        // libsauria_mem.h:175-205), about 100 s of host time per tile outside the simulated clock; reordering
        // only B is enough and cheap.
        std::vector<int8_t> wgt_ord;
        {
            std::vector<double> Bd(wgt.size());
            for (size_t i = 0; i < wgt.size(); i++) Bd[i] = (double)wgt[i];
            std::vector<uint64_t> bo = sauria_weight_order(Bd.data(), (int)nch, (int)cin,
                                                           (int)kh, (int)kw,
                                                           (int)cin, (int)nch, false, t->ib_w);
            wgt_ord.resize(bo.size());
            for (size_t i = 0; i < bo.size(); i++) wgt_ord[i] = (int8_t)(uint8_t)(bo[i] & 0xFFu);
        }
        // `load_sram_backdoor` / `write_bank_data` write BOTH buffers identically (rtl_ref_sram_top.h), so the backdoor
        // cannot put two different halves into the two buffers. For a wide tile, B always goes over the host bus
        // (npu_load_i8), whatever FE_SRAM_BACKDOOR says; A and C are unchanged (not split).
#ifdef FX1_A3_SRAM_BACKDOOR_LOAD
        static const bool use_backdoor = getenv("FE_SRAM_BACKDOOR") != nullptr;
        if (use_backdoor)
        {
            npu->load_sram_backdoor(2, 0, reinterpret_cast<const uint8_t *>(a.data()),
                                    (uint32_t)a.size());
            npu->load_sram_backdoor(4, 0, reinterpret_cast<const uint8_t *>(pre.data()),
                                    (uint32_t)(pre.size() * sizeof(int32_t)));
        }
        else
#endif
        {
            npu_load_i8(SRAMA_OFFSET, NPU_SUBWORDS_A, 32,
                        reinterpret_cast<const uint8_t *>(a.data()), a.size());
            npu_load_i32(SRAMC_OFFSET, NPU_SUBWORDS_C, 32,
                         reinterpret_cast<const uint8_t *>(pre.data()), pre.size() * sizeof(int32_t));
        }
        if (!wide_weight)
        {
#ifdef FX1_A3_SRAM_BACKDOOR_LOAD
            if (use_backdoor)
                npu->load_sram_backdoor(0, 0, reinterpret_cast<const uint8_t *>(wgt_ord.data()),
                                        (uint32_t)wgt_ord.size());
            else
#endif
                npu_load_i8(SRAMB_OFFSET, NPU_SUBWORDS_B, 32,
                            reinterpret_cast<const uint8_t *>(wgt_ord.data()), wgt_ord.size());
        }
        else
        {
            // Wide tile: load HALF 1 (buffer 0, select still "000" => host_b_idx = 0), then set only the B bit to "1"
            // (host_b_idx = 1 => buffer 1) and load HALF 2 at the same relative address (SRAMB_OFFSET, row 0) --
            // where wei_feeder's raw address lands after crossing one buffer (addr_b = raw % rows_per_wei_buffer = 0).
            uint32_t half1 = (uint32_t)kSrambBytes;
            npu_load_i8(SRAMB_OFFSET, NPU_SUBWORDS_B, 32,
                        reinterpret_cast<const uint8_t *>(wgt_ord.data()), half1);
            sc_bv<3> sel = npu_select.read(); sel[1] = true; npu_select.write(sel);   // host_b_idx=1
            tick(2);
            npu_load_i8(SRAMB_OFFSET, NPU_SUBWORDS_B, 32,
                        reinterpret_cast<const uint8_t *>(wgt_ord.data()) + half1,
                        (uint32_t)wgt_ord.size() - half1);
            // no need to reset the B bit to 0 -- the next line overwrites all 3 bits with "111" for compute.
        }

        // --- 3. Swap sang phia NPU ---
        npu_select.write(sc_bv<3>("111"));
        tick(2);

        // --- 4-5. Cau hinh qua bus host ---
        SauriaLayerDesc desc{};
        desc.B_w = kw; desc.B_h = kh; desc.d = 1; desc.s = sy;
        desc.c_til = cin; desc.k_til = nch; desc.h_til = ht; desc.w_til = wt;
        desc.X_used = x_used; desc.Y_used = y_used; desc.preload_en = 1;
        npu_apply_cfg(desc, *t);

        // --- 6. total_contexts / mvm_k BEFORE start ---
        {
            uint64_t f[F_CFG_COUNT];
            sauria_compute_core_fields(desc, *t, f);
            npu_total_contexts.write((uint32_t)f[F_CFG_NCONTEXTS]);
        }
        npu_mvm_k.write(1);
        tick();

        // --- 7. Start pulse, 2 cycles ---
        npu_start.write(true);
        tick(2);
        npu_start.write(false);

        // --- 8. Wait for done. Stop on max_cycles, OR deadlock held continuously for >= FE_STALL_CYCLES with no
        // progress (PSM write count unchanged) -- same rule as tb_rtl_ref_npu_top_tile.cpp. `o_deadlock` is a
        // combinational monitor signal, not a latched error. ---
        const int stall_cycles = getenv("FE_STALL_CYCLES") ? atoi(getenv("FE_STALL_CYCLES")) : 5000;
        int cyc = 0, consec_deadlock = 0, cyc_since_progress = 0;
        uint32_t last_wc = npu->get_psm_recorded_write_count();
        bool got_done = false, got_stuck = false;
        bool wei_second_half = false;   // which half the B bit currently points to (followed EVERY cycle)
        static const bool dbg_wei = getenv("FE_DEBUG_WEI_ADDR") != nullptr;   // temporary diagnostics
        uint32_t dbg_wei_min = 0xFFFFFFFFu, dbg_wei_max = 0; int dbg_wei_flips = 0;
        while (cyc < core_max_cycles)
        {
            tick();
            cyc++;
            // Follow the real B address with the B bit of i_select EVERY cycle, not once: the counter can sweep back
            // to 0 for another context / K chunk and must return to half 1. A and C bits are kept (independent per
            // bit, rtl_ref_sram_top.h:beh_process). The exact switching edge depends on SystemC evaluate/update order
            // between SC_METHODs on the same clock edge, so it is validated by the result, not by reasoning.
            if (wide_weight)
            {
                uint32_t raw = npu->dbg_sramb_addr();
                if (dbg_wei) { if (raw < dbg_wei_min) dbg_wei_min = raw; if (raw > dbg_wei_max) dbg_wei_max = raw; }
                bool want_second = raw >= rows_per_wei_buffer;
                if (want_second != wei_second_half)
                {
                    sc_bv<3> sel = npu_select.read();
                    sel[1] = !want_second;   // sel_b=1 => npu_b_idx=0=buffer0 (nua 1); sel_b=0 => buffer1
                    npu_select.write(sel);
                    wei_second_half = want_second;
                    if (dbg_wei) dbg_wei_flips++;
                }
            }
            uint32_t wc = npu->get_psm_recorded_write_count();
            if (wc != last_wc) { cyc_since_progress = 0; last_wc = wc; }
            else cyc_since_progress++;
            if (npu_deadlock.read()) { npu_deadlock_pulses++; consec_deadlock++; }
            else consec_deadlock = 0;
            if (consec_deadlock >= stall_cycles && cyc_since_progress >= stall_cycles) { got_stuck = true; break; }
            if (npu_done.read()) { got_done = true; break; }
        }
        if (dbg_wei && wide_weight)
            std::printf("  [dbg cfg-b] rows_per_buf=%u addr_min=%u addr_max=%u flips=%d nch=%u Kw=%u wgt_ord_bytes=%u\n",
                        rows_per_wei_buffer, dbg_wei_min, dbg_wei_max, dbg_wei_flips, nch, Kw, (unsigned)wgt_ord.size());
        tick(8);

        // --- 9. Read SRAM-C. ORDER MATTERS: read_bank_data() picks the NPU buffer from i_select AT THE CALL
        // (rtl_ref_sram_top.h:209, npu_c_idx = sel[2] ? 0 : 1). The NPU writes psums into mem_c[0] while
        // select = 111; lowering select to 000 BEFORE the backdoor read would read mem_c[1] = the PRELOAD copy
        // (write_bank_data writes both buffers), i.e. acc == per-channel bias.
        // So: backdoor read while select is still 111, then lower it to 000 for the host-bus path.
#ifdef FX1_A3_SRAM_BACKDOOR_LOAD
        if (use_backdoor)
        {
            std::vector<int32_t> cbuf(nel, 0);
            npu->read_sram_backdoor(4, 0, reinterpret_cast<uint8_t *>(cbuf.data()),
                                    (uint32_t)(cbuf.size() * sizeof(int32_t)));
            for (uint32_t e = 0; e < nel; e++) acc[e] = (int64_t)cbuf[e];
            npu_select.write(sc_bv<3>("000"));
            tick(2);
        }
        else
#endif
        {
        npu_select.write(sc_bv<3>("000"));
        tick(2);
        size_t nrows = ((size_t)nel + 31) / 32;
        for (size_t row = 0; row < nrows; row++)
            for (int sw = 0; sw < NPU_SUBWORDS_C; sw++)
            {
                npu_host_addr.write(SRAMC_OFFSET + (uint32_t)row * NPU_SUBWORDS_C + sw);
                npu_host_rden.write(true); npu_host_wren.write(false);
                tick(2);
                host_data_t r = npu_host_rdata.read();
                npu_host_rden.write(false);
                tick();
                for (int lane = 0; lane < 4; lane++)
                {
                    size_t e = row * 32 + (size_t)sw * 4 + (size_t)lane;
                    if (e < (size_t)nel) acc[e] = (int64_t)(int32_t)r[lane];
                }
            }
        }
        npu_cycles_last = cyc;
        // Hold NpuTop in RESET while idle. With rstn = 1 the whole core (32x32 PEs, feeders, PSM) would be clocked
        // on every simulated cycle of the remaining stand-in work although it does nothing (about 4.6x slower
        // host time). Every tile starts with a 5-cycle reset (top of this function), so this changes no result.
        npu_rstn.write(false);
        return got_done && !got_stuck;
    }

    bool run_tile_via_core(const std::vector<int8_t> &a, uint32_t cin, uint32_t ah, uint32_t aw,
                           const std::vector<int8_t> &wgt, uint32_t nch, uint32_t kh, uint32_t kw,
                           const std::vector<int32_t> &pre, uint32_t ht, uint32_t wt, uint32_t sy,
                           uint32_t x_used, uint32_t y_used, std::vector<int64_t> &acc)
    {
        const SauriaTarget *t = sauria_find_target("int8_32x32");
        uint32_t npos = ht * wt, nel = nch * npos;

        core_sram_rstn.write(false); core_arr_rstn.write(false); core_rstn.write(false); core_soft_reset.write(false);
        core_start.write(false);
        core_arr_softstall_zero.write(false); core_arr_pop_en_dbg_zero.write(false); core_arr_threshold.write(0.0f);
        tick(5);
        core_sram_rstn.write(true); core_arr_rstn.write(true); core_rstn.write(true);
        tick(5);

        core_sram->write_bank_data(2, 0, reinterpret_cast<const uint8_t *>(a.data()), a.size());
        core_sram->write_bank_data(0, 0, reinterpret_cast<const uint8_t *>(wgt.data()), wgt.size());
        core_sram->write_bank_data(4, 0, reinterpret_cast<const uint8_t *>(pre.data()), pre.size() * 4);

        SauriaLayerDesc desc{};
        desc.B_w = kw; desc.B_h = kh; desc.d = 1; desc.s = sy;
        desc.c_til = cin; desc.k_til = nch; desc.h_til = ht; desc.w_til = wt;
        desc.X_used = x_used; desc.Y_used = y_used; desc.preload_en = 1;
        apply_core_cfg(desc, *t);

        std::vector<char> seen(nel, 0);
        core_start.write(true);
        tick();
        core_start.write(false);
        int cyc = 0, drain = -1;
        while (cyc < core_max_cycles)
        {
            wait(); cyc++;
            if (core_sramc_wren.read())
            {
                uint32_t addr = core_sramc_addr.read();
                const auto &wd = core_sramc_wdata.read();
                const auto &wm = core_sramc_wmask.read();
                for (int lane = 0; lane < 32; lane++)
                    if (wm[lane])
                    {
                        uint32_t e = addr * 32 + (uint32_t)lane;
                        if (e < nel && !seen[e]) { seen[e] = 1; acc[e] = wd[lane]; }
                    }
            }
            if (drain < 0)
            {
                bool all = true;
                for (uint32_t e = 0; e < nel && all; e++) all = seen[e] != 0;
                if (all) drain = 64;
            }
            else if (--drain <= 0) break;
        }
        bool ok = drain >= 0 || [&]{ for (auto s : seen) if (!s) return false; return true; }();
        return ok;
    }

    void run()
    {
        std::vector<uint8_t> progb;
        if (!read_file(dir + "/prog.bin", progb) || !read_file(dir + "/dram_init.bin", dram) || !read_file(dir + "/dram_golden.bin", gold))
        {
            std::cout << "[tb_fe_core_net] cannot read inputs in " << dir << std::endl;
            sc_stop();
            return;
        }
        // Resume: load the DRAM snapshot of a previous run instead of dram_init (sizes must match).
        if (!resume_from.empty())
        {
            std::vector<uint8_t> snap;
            if (read_file(resume_from + "/dram_snapshot.bin", snap) && snap.size() == dram.size())
            {
                dram.swap(snap);
                std::printf("[RESUME] loading %s/dram_snapshot.bin (%zu bytes)\n",
                            resume_from.c_str(), dram.size());
            }
            else
            {
                std::printf("[RESUME] cannot load %s/dram_snapshot.bin"
                            " (missing or wrong size) -- STOP\n", resume_from.c_str());
                sc_stop();
                return;
            }
        }
        Cursor c(progb);
        if (std::string(reinterpret_cast<const char *>(progb.data()), 4) != "FEHP") { std::cout << "bad magic" << std::endl; sc_stop(); return; }
        c.pos = 4 + 16;   // knob <4I> da doc o read_knobs()
        uint32_t STAGE_A = c.u32(), STAGE_SKIP = c.u32(), STAGE_PRE = c.u32(), STAGE_OUT = c.u32();
        std::vector<Tensor> tensors(c.u32());
        for (auto &t : tensors) { t.addr = c.u32(); t.c = c.u32(); t.h = c.u32(); t.w = c.u32(); }
        std::vector<Step> steps(c.u32());
        for (auto &s : steps)
        {
            s.kind = c.u8();
            if (s.kind == 0)
            {
                s.in = c.u32(); s.out = c.u32(); s.skip = c.u32(); s.silu = c.u8();
                s.cin = c.u32(); s.cout = c.u32(); s.kh = c.u32(); s.kw = c.u32(); s.sh = c.u32(); s.sw = c.u32();
                s.w_addr = c.u32(); s.lut_addr = c.u32(); s.scale_addr = c.u32(); s.shift_addr = c.u32(); s.bias_addr = c.u32();
                s.tiles.resize(c.u32());
                for (auto &t : s.tiles)
                {
                    t.c0 = c.i32(); t.c1 = c.i32(); t.oy0 = c.i32(); t.oy1 = c.i32(); t.ox0 = c.i32(); t.ox1 = c.i32();
                    t.iy0 = c.i32(); t.iy1 = c.i32(); t.ix0 = c.i32(); t.ix1 = c.i32(); t.yu = c.i32();
                }
            }
            else if (s.kind == 1) { s.in = c.u32(); s.out = c.u32(); s.start = c.u32(); s.end = c.u32(); }
            else if (s.kind == 2) { s.out = c.u32(); s.ins.resize(c.u32()); for (auto &i : s.ins) i = c.u32(); }
            else if (s.kind == 4) { s.in = c.u32(); s.out = c.u32(); s.factor = c.u32(); }
            else if (s.kind == 5)
            {
                s.in = c.u32(); s.in_b = c.u32(); s.out = c.u32();
                s.ap.zpA = c.i32(); s.ap.zpB = c.i32(); s.ap.zpO = c.i32();
                s.ap.SA = c.u32(); s.ap.sA = static_cast<int>(c.u32());
                s.ap.SB = c.u32(); s.ap.sB = static_cast<int>(c.u32());
                s.ap.SO = c.u32(); s.ap.sO = static_cast<int>(c.u32());
            }
            else if (s.kind == 6) { s.in = c.u32(); s.out = c.u32(); s.k = c.u32(); s.s = c.u32(); s.p = c.u32(); }
            else { std::printf("[tb_has_net] KIND %u not supported\n", (unsigned)s.kind); sc_stop(); return; }
        }
        if (c.pos != progb.size()) { std::printf("[tb_has_net] prog.bin thua %zu byte\n", progb.size() - c.pos); sc_stop(); return; }

        bias_en.write(false); requant_en.write(true); lut_en.write(false); residual_en.write(false); vec_mode.write(true);
        def_scale.write(1); def_shift.write(0); o_in_valid.write(false); host_wren.write(false); host_rden.write(false);
        sram_deepsleep.write(false); sram_powergate.write(false); s_host_wren.write(false); s_host_rden.write(false);
        rstn.write(false);
        wait(5);
        rstn.write(true);
        wait(2);

        uint64_t dma_cycles = 0, obp_cycles = 0, cfg_cycles = 0, tiles_done = 0, vectors = 0, framing_errors = 0;
        uint64_t mac_ops = 0, tiles_via_core = 0, core_deadlocks = 0, elem_cycles = 0;
        std::set<uint32_t> written;
        int n_steps = max_steps >= 0 ? std::min<int>(max_steps, steps.size()) : static_cast<int>(steps.size());
        for (int si = 0; si < n_steps; si++)
        {
            if (si < step_first) continue;
            const Step &s = steps[si];
            // FE_CORE_MAX_TILES caps the number of tiles sent through the real core (small reproducers).
            bool step_selected = s.kind == 0 && (core_steps.count(si) != 0 || core_steps.count(-1) != 0);   // -1 = "all"
            bool via_core = step_selected &&
                            (shape_n > 0 || core_max_tiles < 0 || (int)tiles_via_core < core_max_tiles);
            if (s.kind == 0)
            {
                const Tensor &src = tensors[s.in], &dst = tensors[s.out];
                if (s.skip != NONE) { std::printf("[tb_has_net] conv with skip -- the HAS program must lower it to elem_add\n"); sc_stop(); return; }
                const Tensor *skp = nullptr;
                uint32_t K = s.cin * s.kh * s.kw;
                lut_en.write(s.silu != 0);
                residual_en.write(false);
                if (s.silu)
                {
                    uint64_t t0 = sc_time_stamp().value();
                    for (uint32_t lane = 0; lane < W32; lane++)
                        for (uint32_t e = 0; e < 256; e += 4)
                        {
                            host_data_t d;
                            for (int q = 0; q < 4; q++) d[q] = static_cast<double>(dram[s.lut_addr + e + q]);
                            host_write(LUT_BASE + lane * 256 + e, d);
                        }
                    cfg_cycles += (sc_time_stamp().value() - t0) / 10000;
                }
                // Prefetch the ifmap of the NEXT tile (same step) into the other act bank while the CURRENT tile
                // computes + runs the OBP (the second act bank is free because skip data lives in scratch).
                // cur_a_bank alternates 2/3 per tile; a_ready = true means this tile was prefetched by the
                // PREVIOUS iteration (only wait for channel 1, no new start_read).
                uint32_t cur_a_bank = 2;
                bool a_ready = false;
                const bool a_prefetch = getenv("FE_DISABLE_A_PREFETCH") == nullptr;
                bool use_gold = via_core && core_gold_input;   // same for every tile of this step
                // host: padded input window (zero = INT8 zero point). Core mode + FE_CORE_GOLD_INPUT: read from the
                // golden (independent of earlier steps: one layer / one shard can be checked alone) instead of the
                // running DRAM (the real chain, which depends on earlier steps).
                auto fill_stage_a = [&](const Tile &t) -> uint32_t
                {
                    uint32_t ah_ = t.iy1 - t.iy0, aw_ = t.ix1 - t.ix0;
                    for (uint32_t ci = 0; ci < s.cin; ci++)
                        for (uint32_t y = 0; y < ah_; y++)
                            for (uint32_t x = 0; x < aw_; x++)
                            {
                                int sy = t.iy0 + static_cast<int>(y), sx = t.ix0 + static_cast<int>(x);
                                int8_t v = (sy >= 0 && sx >= 0 && sy < static_cast<int>(src.h) && sx < static_cast<int>(src.w))
                                               ? (use_gold ? t8g(src, ci, sy, sx) : t8(src, ci, sy, sx))
                                               : 0;
                                dram[STAGE_A + (ci * ah_ + y) * aw_ + x] = static_cast<uint8_t>(v);
                            }
                    return s.cin * ah_ * aw_;
                };
                for (size_t ti = 0; ti < s.tiles.size(); ti++)
                {
                    const Tile &tl = s.tiles[ti];
                    uint32_t nch = tl.c1 - tl.c0, ht = tl.oy1 - tl.oy0, wt = tl.ox1 - tl.ox0, npos = ht * wt;
                    uint32_t ah = tl.iy1 - tl.iy0, aw = tl.ix1 - tl.ix0;
                    uint32_t yu = tl.yu, nel = nch * npos, words = (nel + W32 - 1) / W32;
                    uint32_t n_ctx = ht * (wt / yu);
                    if (!a_ready)
                        dma->start_read(1, STAGE_A, cur_a_bank, 0, fill_stage_a(tl));
                    // else: issued at the end of the PREVIOUS tile iteration, running in the background.
                    std::memset(&dram[STAGE_PRE], 0, words * 128);
                    for (uint32_t x = 0; x < nch; x++)
                    {
                        int32_t b = dram_i32(s.bias_addr + 4 * (tl.c0 + x));
                        for (uint32_t p = 0; p < npos; p++)
                        {
                            uint32_t e = x * npos + p;
                            std::memcpy(&dram[STAGE_PRE + 4 * e], &b, 4);
                            if (skp)
                                dram[STAGE_SKIP + e] = static_cast<uint8_t>(
                                    use_gold ? t8g(*skp, tl.c0 + x, tl.oy0 + p / wt, tl.ox0 + p % wt)
                                             : t8(*skp, tl.c0 + x, tl.oy0 + p / wt, tl.ox0 + p % wt));
                        }
                    }
                    dma->start_read(0, s.w_addr + tl.c0 * K, 0, 0, nch * K);
                    dma->start_read(3, STAGE_PRE, 4, 0, words * 128);
                    // skip -> scratch (bank 5), not bank 3 (the other ifmap buffer).
                    if (skp)
                    {
                        if (nel > SKIP_SCRATCH_BYTES)
                            throw std::runtime_error("skip exceeds the 24 KB scratch -- wrong fe_tile_plan.py constraint");
                        dma->start_read(2, STAGE_SKIP, SKIP_SCRATCH_BANK, SKIP_SCRATCH_OFFSET, nel);
                    }
                    wait();
                    dma_cycles++;
                    wait_dma_excl_ch1(dma_cycles);   // do NOT wait for channel 1 here
                    for (uint32_t x = 0; x < nch; x++)
                    {
                        write_u32(SCALE_BASE + 4 * x, dram_u32(s.scale_addr + 4 * (tl.c0 + x)));
                        write_u32(SHIFT_BASE + 4 * x, dram_u32(s.shift_addr + 4 * (tl.c0 + x)));
                        cfg_cycles += 4;
                    }
                    write_u32(NCH_REG, nch);   // HAS_IFACE section 9, note (1): channel = i_addr % NCH
                    cfg_cycles += 2;
                    wait_ch1(dma_cycles);   // A of THIS tile: nearly instant when already prefetched
                    std::vector<int8_t> a(s.cin * ah * aw), w(nch * K), sk(skp ? nel : 0);
                    std::vector<int32_t> pre(words * W32);
                    sram->read_bank_data(cur_a_bank, 0, reinterpret_cast<uint8_t *>(a.data()), a.size());
                    sram->read_bank_data(0, 0, reinterpret_cast<uint8_t *>(w.data()), w.size());
                    sram->read_bank_data(4, 0, reinterpret_cast<uint8_t *>(pre.data()), pre.size() * 4);
                    if (skp) sram->read_bank_data(SKIP_SCRATCH_BANK, SKIP_SCRATCH_OFFSET, reinterpret_cast<uint8_t *>(sk.data()), sk.size());
                    // Issue the prefetch of the NEXT tile's A (if any in this step) right after this tile's A backdoor
                    // read: STAGE_A (DRAM) and bank cur_a_bank are free now.
                    uint32_t next_a_bank = (cur_a_bank == 2) ? 3 : 2;
                    if (a_prefetch && ti + 1 < s.tiles.size())
                    {
                        dma->start_read(1, STAGE_A, next_a_bank, 0, fill_stage_a(s.tiles[ti + 1]));
                        a_ready = true;
                    }
                    else
                    {
                        a_ready = false;   // step boundary: no prefetch across steps
                    }
                    cur_a_bank = next_a_bank;
                    std::vector<int64_t> acc(nel, 0);
                    // The core-tile cap is checked inside the TILE loop (a step-level check would never trigger).
                    // Shape selection: key = (cin,kh,kw,sy,nch,ht,wt,yu); the first N occurrences of each shape (whole
                    // run) go through the core, the rest through the stand-in. FE_CORE_MAX_TILES stays a hard cap.
                    bool tile_core = via_core;
                    if (via_core && shape_n > 0)
                    {
                        char kb[96];
                        std::snprintf(kb, sizeof kb, "cin=%u kh=%u kw=%u sy=%u nch=%u ht=%u wt=%u yu=%u",
                                      s.cin, s.kh, s.kw, s.sh, nch, ht, wt, yu);
                        const std::string key(kb);
                        shape_seen_count[key]++;
                        uint64_t &used = shape_core_count[key];   // creates the key (=0) if absent
                        tile_core = used < (uint64_t)shape_n && (core_max_tiles < 0 || (int)tiles_via_core < core_max_tiles);
                        if (tile_core) used++;
                    }
                    else if (via_core)
                        tile_core = (core_max_tiles < 0 || (int)tiles_via_core < core_max_tiles);
                    if (tile_core)
                    {
                        tiles_via_core++;
#ifdef FE_METRICS
                        const sauria_rtl::PerfCounters fe_m0_ = fe_perf_;   // snapshot BEFORE the tile
#endif
                        bool ok = use_npu_top
                                    ? run_tile_via_npu_top(a, s.cin, ah, aw, w, nch, s.kh, s.kw, pre, ht, wt, s.sh, nch, yu, acc)
                                    : run_tile_via_core(a, s.cin, ah, aw, w, nch, s.kh, s.kw, pre, ht, wt, s.sh, nch, yu, acc);
#ifdef FE_METRICS
                        if (use_npu_top && fe_mcsv_)
                        {
                            const sauria_rtl::PerfCounters &p1 = fe_perf_;
                            uint64_t d_all = 0, d_busy = 0;
                            for (int q = 0; q < sauria_rtl::PerfCounters::NUM_CTRL_STATES; q++)
                            {
                                const uint64_t d = p1.ctrl_state_cycles[q] - fe_m0_.ctrl_state_cycles[q];
                                d_all += d;
                                if (q != 0) d_busy += d;   // core busy = FSM total minus IDLE
                            }
                            auto U = [](uint64_t v) { return (unsigned long long)v; };
                            std::fprintf(fe_mcsv_, "%d,%zu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
                                         si, (size_t)(&tl - s.tiles.data()), s.cin, s.kh, s.kw, s.sh, nch, ht, wt, yu, n_ctx, ok ? 1 : 0,
                                         U(d_all), U(d_busy),
                                         U(p1.exec_cycles - fe_m0_.exec_cycles), U(p1.total_pe_cycles - fe_m0_.total_pe_cycles),
                                         U(p1.mac_ops - fe_m0_.mac_ops), U((uint64_t)nch * npos * K),
                                         U(p1.srama_read_beats - fe_m0_.srama_read_beats), U(p1.srama_read_bytes - fe_m0_.srama_read_bytes),
                                         U(p1.sramb_read_beats - fe_m0_.sramb_read_beats), U(p1.sramb_read_bytes - fe_m0_.sramb_read_bytes),
                                         U(p1.sramc_read_beats - fe_m0_.sramc_read_beats), U(p1.sramc_read_bytes - fe_m0_.sramc_read_bytes),
                                         U(p1.sramc_write_beats - fe_m0_.sramc_write_beats), U(p1.sramc_write_bytes - fe_m0_.sramc_write_bytes));
                            for (int q = 1; q <= 24; q++)
                                std::fprintf(fe_mcsv_, ",%llu", U(p1.ctrl_state_cycles[q] - fe_m0_.ctrl_state_cycles[q]));
                            std::fprintf(fe_mcsv_, "\n");
                            std::fflush(fe_mcsv_);
                        }
#endif
                        if (!ok) { core_deadlocks++; std::printf("  [core] step %d tile deadlock/timeout (nch=%u ht=%u wt=%u cin=%u)\n", si, nch, ht, wt, s.cin); }
                    }
                    else
                    {
                        // CORE STAND-IN: accumulator = preload + sum of products, C-order.
                        for (uint32_t x = 0; x < nch; x++)
                        {
                            const int8_t *wx = &w[x * K];
                            for (uint32_t p = 0; p < npos; p++)
                            {
                                uint32_t oy = p / wt, ox = p % wt;
                                int64_t sum = 0;
                                for (uint32_t ci = 0; ci < s.cin; ci++)
                                    for (uint32_t ky = 0; ky < s.kh; ky++)
                                    {
                                        const int8_t *arow = &a[(ci * ah + oy * s.sh + ky) * aw + ox * s.sw];
                                        const int8_t *wrow = &wx[(ci * s.kh + ky) * s.kw];
                                        for (uint32_t kx = 0; kx < s.kw; kx++) sum += int32_t(arow[kx]) * int32_t(wrow[kx]);
                                    }
                                acc[x * npos + p] = pre[x * npos + p] + sum;
                            }
                        }
                    }
                    mac_ops += uint64_t(nch) * npos * K;
                    // Dump acc in STAND-IN mode too (same FE_CORE_DUMP_ACC) to compare core vs stand-in inside one
                    // testbench, without external job files.
                    if (!acc_dump_path.empty() && (core_max_tiles < 0 || (int)tiles_dumped < core_max_tiles))
                    {
                        tiles_dumped++;
                        std::FILE *fp = std::fopen(acc_dump_path.c_str(), tiles_dumped == 1 ? "wb" : "ab");
                        if (fp)
                        {
                            std::fwrite(acc.data(), sizeof(int64_t), acc.size(), fp);
                            std::fclose(fp);
                            std::printf("  [dump] tile %llu: %zu elements (via_core=%d nch=%u ht=%u wt=%u cin=%u yu=%u)\n",
                                        (unsigned long long)tiles_dumped, acc.size(), (int)via_core, nch, ht, wt, s.cin, yu);
                        }
                    }
                    cur_ht = ht; cur_wt = wt; cur_nch = nch; cur_yu = yu;
                    for (uint32_t ctx = 0; ctx < n_ctx; ctx++)
                    {
                        uint32_t oy = ctx / (wt / yu), cx0 = (ctx % (wt / yu)) * yu;
                        uint64_t before = obp_rows_written;
                        for (uint32_t x = 0; x < nch; x++)
                        {
                            psum_vector_t<W32, int32_t> v(0);
                            act_vector_t<W32, int8_t> r(0);
                            sramc_mask_t<W32> m(false);
                            for (uint32_t k = 0; k < yu; k++)
                            {
                                uint32_t e = x * npos + oy * wt + cx0 + k;
                                int64_t av = acc[e];
                                if (av > INT32_MAX || av < INT32_MIN) { std::cout << "INT32 overflow" << std::endl; framing_errors++; }
                                v[k] = static_cast<int32_t>(av);
                                r[k] = skp ? sk[e] : 0;
                                m[k] = true;
                            }
                            o_in.write(v); o_residual.write(r); o_in_mask.write(m); o_in_addr.write(ctx * nch + x); o_in_valid.write(true);
                            wait();
                            obp_cycles++;
                        }
                        o_in_valid.write(false);
                        // HAS_IFACE §9 note (2): wait until HasObp has drained (vectors_out == vectors_in) instead of
                        // IDLE_CYCLES. The channel comes from i_addr % NCH, so the next context could enter at once; the
                        // wait only keeps the per-context write-back and the framing count of the original testbench.
                        int drain = 0;
                        while ((obp->vectors_out != obp->vectors_in || obp_rows_written - before != nch) && drain < OBP_DRAIN_LIMIT)
                        {
                            wait();
                            drain++;
                        }
                        obp_cycles += drain;
                        if (obp_rows_written - before != nch) framing_errors++;
                        vectors += nch;
                    }
                    dma->start_write(STAGE_OUT, 4, 0, words * 128);
                    wait();
                    dma_cycles++;
                    // Still no wait on channel 1 here: the NEXT tile's prefetch (if any) keeps running through this
                    // writeback and is only waited for at the top of the next tile.
                    wait_dma_excl_ch1(dma_cycles);
                    for (uint32_t x = 0; x < nch; x++)
                        for (uint32_t p = 0; p < npos; p++)
                        {
                            int32_t val = dram_i32(STAGE_OUT + 4 * (x * npos + p));
                            t8(dst, tl.c0 + x, tl.oy0 + p / wt, tl.ox0 + p % wt) = static_cast<int8_t>(val);
                        }
                    tiles_done++;
                }
            }
            else if (s.kind == 1)
            {
                const Tensor &src = tensors[s.in], &dst = tensors[s.out];
                for (uint32_t ch = s.start; ch < s.end; ch++)
                    std::memcpy(&dram[dst.addr + (ch - s.start) * dst.h * dst.w], &dram[src.addr + ch * src.h * src.w], src.h * src.w);
            }
            else if (s.kind == 2)
            {
                const Tensor &dst = tensors[s.out];
                uint32_t off = 0;
                for (uint32_t id : s.ins)
                {
                    const Tensor &src = tensors[id];
                    std::memcpy(&dram[dst.addr + off], &dram[src.addr], src.c * src.h * src.w);
                    off += src.c * src.h * src.w;
                }
            }
            else if (s.kind == 5)
            {
                // ELEM_WISE ADD (HAS_IFACE section 6.2): 2 stages through the Scratchpad, costs simulated cycles (DMA + pipeline).
                const Tensor &ta = tensors[s.in], &tb = tensors[s.in_b], &dst = tensors[s.out];
                const size_t n = size_t(dst.c) * dst.h * dst.w;
                if (size_t(ta.c) * ta.h * ta.w != n || size_t(tb.c) * tb.h * tb.w != n)
                    throw std::runtime_error("elem_add: A/B/out sizes differ");
                const uint64_t t0 = sc_time_stamp().value();
                ew->add(reinterpret_cast<const int8_t *>(&dram[ta.addr]), reinterpret_cast<const int8_t *>(&dram[tb.addr]),
                        reinterpret_cast<int8_t *>(&dram[dst.addr]), n, s.ap);
                elem_cycles += (sc_time_stamp().value() - t0) / 10000;
            }
            else if (s.kind == 6)
            {
                // ELEM_WISE MAX (SPPF 5x5 s1 p2), pad -128, through the Scratchpad per channel group.
                const Tensor &src = tensors[s.in], &dst = tensors[s.out];
                const uint64_t t0 = sc_time_stamp().value();
                ew->maxpool(reinterpret_cast<const int8_t *>(&dram[src.addr]), reinterpret_cast<int8_t *>(&dram[dst.addr]),
                            int(src.c), int(src.h), int(src.w), int(s.k), int(s.s), int(s.p));
                elem_cycles += (sc_time_stamp().value() - t0) / 10000;
            }
            else if (s.kind == 4)
            {
                const Tensor &src = tensors[s.in], &dst = tensors[s.out];
                for (uint32_t ch = 0; ch < dst.c; ch++)
                    for (uint32_t y = 0; y < dst.h; y++)
                        for (uint32_t x = 0; x < dst.w; x++)
                            t8(dst, ch, y, x) = t8(src, ch, y / s.factor, x / s.factor);
            }
            written.insert(s.out);
            // Verdict right after EVERY layer (not only at the end), so an interrupted run keeps the result of
            // the layers already done: compare this step's output tensor with the golden.
            {
                const Tensor &ot = tensors[s.out];
                uint64_t n = (uint64_t)ot.c * ot.h * ot.w, bad = 0;
                for (uint64_t i = 0; i < n; i++) bad += dram[ot.addr + i] != gold[ot.addr + i];
                std::printf("[STEP] %d/%d kind=%u tensor=%u [%u,%u,%u] elements=%llu bad=%llu %s"
                            " tiles=%llu via_core=%llu sim_cycles=%llu\n",
                            si + 1, n_steps, (unsigned)s.kind, s.out, ot.c, ot.h, ot.w,
                            (unsigned long long)n, (unsigned long long)bad, bad ? "FAIL" : "PASS",
                            (unsigned long long)tiles_done, (unsigned long long)tiles_via_core,
                            (unsigned long long)(sc_time_stamp().value() / 10000));
                std::fflush(stdout);
            }
            // DRAM snapshot after each layer + the last finished step: resumable with FE_STEP_FIRST.
            // One file, overwritten each time. Enable with FE_SNAPSHOT_DIR=<dir>.
            if (!snapshot_dir.empty())
            {
                std::string dpath = snapshot_dir + "/dram_snapshot.bin";
                std::string tmp = dpath + ".tmp";
                std::FILE *fp = std::fopen(tmp.c_str(), "wb");
                if (fp)
                {
                    std::fwrite(dram.data(), 1, dram.size(), fp);
                    std::fclose(fp);
                    std::rename(tmp.c_str(), dpath.c_str());   // atomic rename: never leaves a half-written snapshot
                    std::FILE *mp = std::fopen((snapshot_dir + "/last_step.txt").c_str(), "w");
                    if (mp) { std::fprintf(mp, "%d\n", si); std::fclose(mp); }
                }
                else std::printf("[SNAPSHOT] cannot open %s\n", tmp.c_str());
            }
            if (si % 10 == 9 || si == n_steps - 1)
                std::printf("[tb_fe_core_net] step %d/%d, tiles %llu (via core %llu), sim cycles %llu\n", si + 1, n_steps,
                            (unsigned long long)tiles_done, (unsigned long long)tiles_via_core, (unsigned long long)(sc_time_stamp().value() / 10000));
        }

        uint64_t bad_tensors = 0, bad_elems = 0, elems = 0;
        for (uint32_t id : written)
        {
            const Tensor &t = tensors[id];
            uint64_t n = uint64_t(t.c) * t.h * t.w, bad = 0;
            for (uint64_t i = 0; i < n; i++) bad += dram[t.addr + i] != gold[t.addr + i];
            elems += n;
            if (bad)
            {
                if (bad_tensors < 10) std::printf("  MISMATCH tensor %u [%u,%u,%u]: %llu / %llu\n", id, t.c, t.h, t.w, (unsigned long long)bad, (unsigned long long)n);
                bad_tensors++;
                bad_elems += bad;
            }
        }
        const auto &es = ew->stats();
        std::printf("[tb_has_net] HasObp: vectors in/out %llu/%llu, n %llu sat16 %llu clamp8 %llu ovf64 %llu, cfg_errors %llu, "
                    "lut_lane_mismatch %llu, scratch_param_reads %llu\n",
                    (unsigned long long)obp->vectors_in, (unsigned long long)obp->vectors_out, (unsigned long long)obp->qc.n,
                    (unsigned long long)obp->qc.sat16, (unsigned long long)obp->qc.clamp8, (unsigned long long)obp->qc.ovf64,
                    (unsigned long long)obp->cfg_errors, (unsigned long long)obp->lut_lane_mismatch,
                    (unsigned long long)obp->scratch_param_reads);
        std::printf("[tb_has_net] HasElemwise: add %llu calls / %llu elems, max %llu calls / %llu elems, cycles %llu "
                    "(dma %llu, compute %llu), req sat16 %llu clamp8 %llu, deq sat32 %llu, scratch rd/wr %llu/%llu B peak %u B\n",
                    (unsigned long long)es.add_calls, (unsigned long long)es.add_elems, (unsigned long long)es.max_calls,
                    (unsigned long long)es.max_elems, (unsigned long long)elem_cycles, (unsigned long long)es.cyc_dma,
                    (unsigned long long)es.cyc_compute, (unsigned long long)es.qc_req.sat16, (unsigned long long)es.qc_req.clamp8,
                    (unsigned long long)es.qc_deq.sat32, (unsigned long long)ew->scratch().rd_bytes,
                    (unsigned long long)ew->scratch().wr_bytes, ew->scratch().peak);
        bool pass = bad_tensors == 0 && framing_errors == 0 && core_deadlocks == 0 && !written.empty() &&
                    obp->cfg_errors == 0 && obp->vectors_in == obp->vectors_out;
        std::printf("[tb_fe_core_net] RESULT: %s (steps %d, tensors %zu, elements %llu, bad tensors %llu, bad elements %llu, "
                    "framing errors %llu, core deadlocks %llu)\n", pass ? "PASS" : "FAIL", n_steps, written.size(), (unsigned long long)elems,
                    (unsigned long long)bad_tensors, (unsigned long long)bad_elems, (unsigned long long)framing_errors, (unsigned long long)core_deadlocks);
        std::printf("[tb_fe_core_net] tiles %llu (via core %llu), OBP vectors %llu, MACs (stand-in tiles) %llu, sim cycles %llu "
                    "(DMA wait %llu, OBP %llu, OBP config approx %llu)\n", (unsigned long long)tiles_done, (unsigned long long)tiles_via_core,
                    (unsigned long long)vectors, (unsigned long long)mac_ops, (unsigned long long)(sc_time_stamp().value() / 10000),
                    (unsigned long long)dma_cycles, (unsigned long long)obp_cycles, (unsigned long long)cfg_cycles);
        // Shape coverage: always print shapes_covered / shapes_total + tiles_via_core; warn when a shape never went
        // through the core (e.g. the FE_CORE_MAX_TILES cap ran out before it).
        if (shape_n > 0)
        {
            uint64_t covered = 0, total = shape_seen_count.size(), uncovered = 0;
            for (const auto &kv : shape_seen_count)
            {
                auto it = shape_core_count.find(kv.first);
                if (it != shape_core_count.end() && it->second > 0) covered++;
                else uncovered++;
            }
            std::printf("[COVERAGE] shapes_covered/shapes_total = %llu/%llu  tiles_via_core = %llu  (FE_CORE_SHAPE_N=%d)\n",
                        (unsigned long long)covered, (unsigned long long)total, (unsigned long long)tiles_via_core, shape_n);
            if (uncovered)
            {
                std::printf("[COVERAGE][WARNING] %llu SHAPES NEVER WENT THROUGH THE CORE -- the run does not cover every shape:\n", (unsigned long long)uncovered);
                for (const auto &kv : shape_seen_count)
                {
                    auto it = shape_core_count.find(kv.first);
                    if (it == shape_core_count.end() || it->second == 0)
                        std::printf("[COVERAGE][WARNING]   %s  (%llu tiles seen, 0 on the core)\n", kv.first.c_str(), (unsigned long long)kv.second);
                }
            }
            if (const char *cp = getenv("FE_COVERAGE_CSV"))
            {
                if (std::FILE *cf = std::fopen(cp, "w"))
                {
                    std::fprintf(cf, "shape,tiles_seen,tiles_via_core\n");
                    for (const auto &kv : shape_seen_count)
                    {
                        auto it = shape_core_count.find(kv.first);
                        std::fprintf(cf, "\"%s\",%llu,%llu\n", kv.first.c_str(), (unsigned long long)kv.second,
                                     (unsigned long long)(it == shape_core_count.end() ? 0 : it->second));
                    }
                    std::fclose(cf);
                }
            }
        }
        exit_code = pass ? 0 : 1;
        sc_stop();
    }
};

int sc_main(int argc, char *argv[])
{
    sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
    std::string dir = argc > 1 ? argv[1] : "fe_work/has/net_compat";
    int max_steps = argc > 2 ? std::stoi(argv[2]) : -1;
    sc_clock clk("clk", 10, SC_NS);
    TbFeCoreNet tb("tb", dir, max_steps);
    tb.i_clk(clk);
    sc_start();
    return tb.exit_code;
}
