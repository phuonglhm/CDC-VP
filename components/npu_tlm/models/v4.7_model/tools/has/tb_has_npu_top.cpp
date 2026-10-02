// tb_has_npu_top.cpp -- whole-network testbench of has::HasNpuTop, driven only the way a CPU drives it: the testbench
// loads the DRAM image, replays the compiler's register-write stream (<insts dir>/mmio.txt) through the host port,
// polls STATUS / RETIRED, runs the host-only steps ("H" lines), and compares the output tensor of every retired
// instruction with the golden DRAM image. It never touches SRAM, the core or the epilogue directly.
//
//   tb_has_npu_top <insts dir> [--profile P] [--first F] [--count N] [--trace] [--dma v45|has] [--dram-lat N] [options]
//
//   --profile P  recommended (default): configuration of the reference results -- overlapped ping-pong schedule,
//                HAS AXI-128 DMA, epilogue inline on the PSM -> SRAM-C path, banked scratchpad, vector-unit latencies,
//                3-D DMA descriptors, per-layer tile order, attention products on the core.
//                proposals: recommended + the two proposed hardware options (--c-bcast, --halo).
//                legacy: sequential schedule, v4.5 DMA timing, epilogue after the core (regression of older results).
//   Single options (added on top of the profile):
//     --overlap      ping-pong DMA on the core SRAM host half; epilogue / write-back of tile i-1 during core(i)
//     --dma          DMA timing: v45 = 32 B/cycle, no latency; has = AXI-128 (16 B/beat, 8-beat bursts, CH3 low priority)
//     --obp-inline   epilogue on the PSM -> SRAM-C write path (needs --overlap)
//     --sp-banked    scratchpad banks, double-buffered; ELEM_WISE chunks pipelined against the DMA
//     --gvu-lat      epilogue / ELEM_WISE pipeline latencies from the stage counts of the vector-unit drawing
//     --desc3d       3-D DMA descriptors (in-bounds input window, packed int8 write-back; needs --overlap)
//     --s4           per-layer tile walk order with fewer DMA bytes (needs --overlap)
//     --rce-core     FUSED_ATTN products Q.K^T and A.V on the core (otherwise a tile-rule estimate)
//     --c-bcast      proposal: bias preload by a broadcast descriptor, 4 bytes per channel on the bus
//     --halo         proposal: column-wise tile walk, input rows shared with the previous tile copied inside SRAM-A
//     --halo-copy-bpc N  bytes per cycle of that copy (ESTIMATED, default 32; 0 = free)
//     --dram-lat N   extra cycles before the first burst of every DMA transfer (ESTIMATED)
//     --clk-ns T     clock period in ns (default 10); cycle counts are the same for any period
//     --first F      start at instruction F: DRAM starts from dram_golden.bin with the output regions of the window
//                    overwritten (0xA5); register writes of skipped instructions are replayed without their push
//     --count N      run N instructions
//   env FE_METRICS_CSV=<file> (FE_METRICS builds: per-tile core counters), FE_SNAPSHOT_DIR=<dir> (DRAM image after the run),
//       HAS_* knobs of has::Knobs::from_env()
// Output lines "[STEP] ..." and "[tb_fe_core_net] RESULT: ..." are parsed by tools/metrics.
#include <systemc.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "has/has_npu_top.h"

using namespace has;
namespace M = has::mmio;

namespace
{
    bool read_file(const std::string &p, std::vector<uint8_t> &b)
    {
        std::ifstream f(p, std::ios::binary);
        if (!f) return false;
        b.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return true;
    }

    struct Event
    {
        char kind;             // 'W' register write, 'H' host step
        uint32_t addr{0}, val{0};
        std::string text;      // H line
        int instr{-1};         // for the push write: instruction index
    };

    struct Instr
    {
        std::string label;     // "# [i] ..." comment
        uint8_t opcode{0};
        uint32_t out_addr{0};
        uint64_t out_bytes{0};
        uint32_t c{0}, h{0}, w{0};
        uint32_t param{0};     // LAYERNORM parameter block (out_int16 doubles out_bytes)
    };

    struct HostOp { uint32_t in{0}, out{0}, c{0}, h{0}, w{0}; bool ok{false}; };
    HostOp parse_h(const std::string &t)
    {
        HostOp o;
        std::istringstream is(t);
        std::string tok;
        is >> tok >> tok;   // "H", op name
        if (tok != "upsample2x") return o;
        while (is >> tok)
        {
            const size_t e = tok.find('=');
            if (e == std::string::npos) continue;
            const std::string k = tok.substr(0, e);
            const uint32_t v = uint32_t(std::stoul(tok.substr(e + 1), nullptr, 0));
            if (k == "in") o.in = v; else if (k == "out") o.out = v; else if (k == "c") o.c = v; else if (k == "h") o.h = v; else if (k == "w") o.w = v;
        }
        o.ok = o.c && o.h && o.w;
        return o;
    }
}

SC_MODULE(TbHasNpuTop)
{
    sc_in<bool> i_clk;
    sc_signal<bool> rstn{"rstn"}, wren{"wren"}, rden{"rden"}, irq{"irq"}, done{"done"}, deadlock{"deadlock"};
    sc_signal<uint32_t> addr{"addr"};
    sc_signal<sauria::host_data_t> wdata{"wdata"}, rdata{"rdata"};
    sc_signal<sauria::host_mask_t> wmask{"wmask"};
    HasNpuTop *dut{nullptr};

    std::string dir;
    int first{0}, count{-1};
    std::vector<uint8_t> dram, gold;
    size_t program_bytes{0};
    int exit_code{1};

    static bool g_obp_inline;
    static bool g_desc3d;
    static bool g_s4;
    static bool g_halo;
    static int g_halo_bpc;
    static bool g_rce_core;
    static bool g_c_bcast;
    static Knobs g_knobs;
    static double g_clk_ns;   // clock period in ns (--clk-ns); cycle counts do not depend on it
    static uint64_t now_cycles() { return sc_time_stamp().value() / uint64_t(g_clk_ns * 1000.0 + 0.5); }
    SC_HAS_PROCESS(TbHasNpuTop);
    TbHasNpuTop(sc_module_name n, const std::string &d, int f, int c, bool trace, bool overlap, const DmaParams &dp)
        : sc_module(n), dir(d), first(f), count(c)
    {
        dut = new HasNpuTop("has_npu_top", g_knobs);
        dut->i_clk(i_clk); dut->i_rstn(rstn);
        dut->i_host_addr(addr); dut->i_host_wren(wren); dut->i_host_rden(rden);
        dut->i_host_wdata(wdata); dut->i_host_wmask(wmask); dut->o_host_rdata(rdata);
        dut->o_irq(irq); dut->o_done(done); dut->o_deadlock(deadlock);
        dut->trace = trace;
        dut->set_mode(overlap, dp);
        dut->set_clock_period(sc_time(g_clk_ns, SC_NS));
        dut->set_obp_inline(overlap && g_obp_inline);
        dut->desc3d = overlap && g_desc3d;
        dut->tile_order_auto = overlap && g_s4;
        dut->halo_reuse = overlap && g_halo;
        dut->rce_core = g_rce_core;
        dut->c_bias_bcast = overlap && g_c_bcast;
        if (g_halo_bpc >= 0) dut->halo_copy_bpc = uint32_t(g_halo_bpc);
        if (const char *mc = getenv("FE_CORE_MAX_CYCLES")) dut->core_max_cycles = std::atoi(mc);
#ifdef FE_METRICS
        if (const char *mp = getenv("FE_METRICS_CSV")) dut->metrics_csv = std::fopen(mp, "a");
#endif
        SC_THREAD(run);
        sensitive << i_clk.pos();
    }
    ~TbHasNpuTop() { delete dut; }

    // ---- CPU-side MMIO (hold one clock edge) ----
    void mmio_write(uint32_t a, uint32_t v)
    {
        sauria::host_data_t d; sauria::host_mask_t m;
        d[0] = double(v); m.data.fill(true);
        addr.write(a); wdata.write(d); wmask.write(m); wren.write(true); rden.write(false);
        wait();
        wren.write(false);
        wait();
    }
    uint32_t mmio_read(uint32_t a)
    {
        addr.write(a); rden.write(true); wren.write(false);
        wait();
        wait();
        const uint32_t v = uint32_t(rdata.read()[0]);
        rden.write(false);
        wait();
        return v;
    }

    uint64_t compare(uint32_t a, uint64_t n)
    {
        uint64_t bad = 0;
        for (uint64_t i = 0; i < n; i++) bad += dram[a + i] != gold[a + i];
        return bad;
    }

    void run()
    {
        // ---------------- inputs
        std::vector<uint8_t> init;
        if (!read_file(dir + "/dram_golden.bin", gold) || !read_file(dir + "/dram_init.bin", init))
        { std::printf("[tb_has_npu_top] cannot read %s/dram_{init,golden}.bin\n", dir.c_str()); sc_stop(); return; }
        std::vector<Event> ev;
        std::vector<Instr> ins;
        {
            std::ifstream f(dir + "/mmio.txt");
            if (!f) { std::printf("[tb_has_npu_top] cannot read %s/mmio.txt\n", dir.c_str()); sc_stop(); return; }
            std::string line, label;
            uint32_t sticky[64] = {0}, ext[M::X_COUNT] = {0};
            while (std::getline(f, line))
            {
                if (line.rfind("# [", 0) == 0) { label = line.substr(2); continue; }
                if (line.empty() || line[0] == '#') continue;
                if (line[0] == 'H') { Event e; e.kind = 'H'; e.text = line; ev.push_back(e); continue; }
                std::istringstream is(line);
                std::string tag;
                Event e;
                e.kind = 'W';
                is >> tag >> std::hex >> e.addr >> e.val;
                if (e.addr >= M::EXT_BASE && e.addr < M::EXT_END) ext[(e.addr - M::EXT_BASE) / 4] = e.val;
                else if (e.addr >= M::IN_ADDR && e.addr <= M::B_LEN) sticky[(e.addr - M::IN_ADDR) / 4] = e.val;
                if (e.addr == M::PUSH_A)
                {
                    Instr in;
                    in.label = label;
                    in.opcode = uint8_t(e.val);
                    in.out_addr = sticky[(M::OUT_ADDR - M::IN_ADDR) / 4];
                    const uint32_t mode = sticky[(M::MODE_PACK - M::IN_ADDR) / 4];
                    if (in.opcode == 0x12) { in.c = ext[M::X_OUT_C]; in.h = ext[M::X_OUT_H]; in.w = ext[M::X_OUT_W]; }
                    else if (in.opcode == 0x13)   // FUSED_ATTN: O = [NQ][D], NQ = X_ROWS or LEN
                    { in.c = 1; in.h = ext[M::X_ROWS] ? ext[M::X_ROWS] : sticky[(M::LEN - M::IN_ADDR) / 4]; in.w = sticky[(M::R458 - M::IN_ADDR) / 4]; }
                    else if (in.opcode == 0x14)   // LAYERNORM: Y = [rows][H] int8 (w doubled below for int16 outputs)
                    { in.c = 1; in.h = ext[M::X_ROWS]; in.w = sticky[(M::LEN - M::IN_ADDR) / 4]; in.param = ext[M::X_PARAM_ADDR]; }
                    else if (mode == 0) { in.c = 1; in.h = 1; in.w = sticky[(M::LEN - M::IN_ADDR) / 4]; }
                    else
                    {
                        const uint32_t k = ext[M::X_POOL_K], p = ext[M::X_POOL_P], s = sticky[(M::STRIDE - M::IN_ADDR) / 4];
                        in.c = ext[M::X_IN_C];
                        in.h = (ext[M::X_IN_H] + 2 * p - k) / s + 1;
                        in.w = (ext[M::X_IN_W] + 2 * p - k) / s + 1;
                    }
                    in.out_bytes = uint64_t(in.c) * in.h * in.w;
                    e.instr = int(ins.size());
                    ins.push_back(in);
                    for (auto &x : ext) x = 0;
                }
                ev.push_back(e);
            }
        }
        const int n_ins = int(ins.size());
        if (count < 0 || first + count > n_ins) count = n_ins - first;
        const int last = first + count;   // exclusive

        // ---------------- DRAM
        program_bytes = init.size();
        if (first == 0) dram = init;
        else
        {
            dram = gold;
            for (int i = first; i < last; i++) std::memset(&dram[ins[i].out_addr], 0xA5, ins[i].out_bytes);
            int cur = 0;
            for (const Event &e : ev)
            {
                if (e.kind == 'W' && e.instr >= 0) { cur = e.instr + 1; continue; }
                if (e.kind == 'H' && cur >= first && cur < last)
                {
                    const HostOp o = parse_h(e.text);
                    if (o.ok) std::memset(&dram[o.out], 0xA5, size_t(o.c) * 4 * o.h * o.w);
                }
            }
        }
        for (Instr &in : ins)
            if (in.opcode == 0x14)
            {
                LnParams lp;
                if (parse_ln_block(init, in.param, lp) && lp.out_int16) in.out_bytes *= 2;
            }
        dut->set_dram(&dram);

        rstn.write(false); wren.write(false); rden.write(false);
        wait(5);
        rstn.write(true);
        wait(2);
        std::printf("[tb_has_npu_top] %s: %d instructions in stream, window [%d, %d), DRAM %zu B (+ DFC staging from 0x%x)\n",
                    dir.c_str(), n_ins, first, last, program_bytes, dut->staging_base());

        // ---------------- replay
        uint64_t pushed = 0, checked = 0, bad_tensors = 0, bad_elems = 0, elems = 0, host_steps = 0, mmio_errors = 0;
        std::vector<int> pushed_idx;
        const uint64_t t0 = now_cycles();
        auto check_retired = [&](bool block_all)
        {
            for (;;)
            {
                const uint32_t r = mmio_read(M::RETIRED);
                while (checked < r)
                {
                    const Instr &in = ins[size_t(pushed_idx[checked])];
                    const uint64_t bad = compare(in.out_addr, in.out_bytes);
                    elems += in.out_bytes;
                    if (bad) { bad_tensors++; bad_elems += bad; }
                    std::printf("[STEP] %llu/%d kind=%u tensor=%d [%u,%u,%u] elements=%llu bad=%llu %s tiles=%llu via_core=%llu sim_cycles=%llu  # %s\n",
                                (unsigned long long)(checked + 1), count, in.opcode == 0x12 ? 0u : 5u, pushed_idx[checked], in.c, in.h, in.w,
                                (unsigned long long)in.out_bytes, (unsigned long long)bad, bad ? "FAIL" : "PASS",
                                (unsigned long long)dut->st.tiles, (unsigned long long)dut->st.tiles,
                                (unsigned long long)(now_cycles()), in.label.c_str());
                    std::fflush(stdout);
                    checked++;
                }
                if (!block_all || checked == pushed) return;
                wait(64);
            }
        };

        int cur = 0;
        for (const Event &e : ev)
        {
            if (e.kind == 'H')
            {
                if (cur < first || cur >= last) continue;
                // host step: everything pushed so far must have finished (RAW on its input)
                check_retired(true);
                const HostOp o = parse_h(e.text);
                if (!o.ok) { std::printf("[tb_has_npu_top] unknown host line: %s\n", e.text.c_str()); mmio_errors++; continue; }
                for (uint32_t c = 0; c < o.c; c++)
                    for (uint32_t y = 0; y < 2 * o.h; y++)
                        for (uint32_t x = 0; x < 2 * o.w; x++)
                            dram[o.out + (c * 2 * o.h + y) * 2 * o.w + x] = dram[o.in + (c * o.h + y / 2) * o.w + x / 2];
                const uint64_t n = uint64_t(o.c) * 4 * o.h * o.w, bad = compare(o.out, n);
                host_steps++;
                elems += n;
                if (bad) { bad_tensors++; bad_elems += bad; }
                std::printf("[HOST] %s -> bad=%llu %s\n", e.text.c_str(), (unsigned long long)bad, bad ? "FAIL" : "PASS");
                continue;
            }
            if (e.instr >= 0)   // the push of instruction e.instr
            {
                const int i = e.instr;
                cur = i + 1;
                if (i < first) { dut->mmio.discard_ext(); continue; }
                if (i >= last) continue;
                for (;;)   // CPU flow control: wait while the instruction queue is full
                {
                    const uint32_t s = mmio_read(M::STATUS);
                    if ((s & 2u) == 0) break;
                    check_retired(false);
                    wait(256);
                }
                mmio_write(e.addr, e.val);
                pushed++;
                pushed_idx.push_back(i);
                const uint32_t s = mmio_read(M::STATUS);
                if (s & 4u)
                {
                    std::printf("[tb_has_npu_top] STATUS error 0x%08x after push of instruction %d (%s)\n", s, i, ins[i].label.c_str());
                    mmio_errors++;
                    mmio_write(M::STATUS, 0);
                }
                check_retired(false);
                continue;
            }
            if (cur > last) continue;
            mmio_write(e.addr, e.val);   // gather register (skipped instructions too: rebuild sticky state)
        }
        check_retired(true);
        while (mmio_read(M::STATUS) & 1u) wait(64);

        // ---------------- verdict
        const auto &st = dut->st;
        const auto &ob = dut->obp_block();
        const auto &es = dut->elem_block().stats();
        const uint64_t cyc = now_cycles() - t0;
        std::printf("[tb_has_npu_top] DFC: %llu instructions (%llu GEMM_FUSED, %llu ELEM_WISE), %llu tiles, host steps %llu, MMIO errors %llu, "
                    "compat decoder: float-scale %llu, default-tiling %llu, skip-split %llu, errors %llu\n",
                    (unsigned long long)st.instr, (unsigned long long)st.gemm, (unsigned long long)st.elem, (unsigned long long)st.tiles,
                    (unsigned long long)host_steps, (unsigned long long)mmio_errors, (unsigned long long)dut->mmio.n_float_scale,
                    (unsigned long long)dut->mmio.n_default_tiling, (unsigned long long)dut->mmio.n_skip_split, (unsigned long long)dut->mmio.n_errors);
        std::printf("[tb_has_net] HasObp: vectors in/out %llu/%llu, n %llu sat16 %llu clamp8 %llu ovf64 %llu, cfg_errors %llu, lut_lane_mismatch %llu\n",
                    (unsigned long long)ob.vectors_in, (unsigned long long)ob.vectors_out, (unsigned long long)ob.qc.n,
                    (unsigned long long)ob.qc.sat16, (unsigned long long)ob.qc.clamp8, (unsigned long long)ob.qc.ovf64,
                    (unsigned long long)ob.cfg_errors, (unsigned long long)ob.lut_lane_mismatch);
        if (dut->obp_inline())
            std::printf("[tb_has_npu_top] OBP inline (A1): channel groups %llu, lanes outside the tile %llu\n",
                        (unsigned long long)ob.inline_vectors, (unsigned long long)dut->inline_out_of_tile);
        std::printf("[tb_has_npu_top] core passes %llu for %llu tiles (S1 Cin split when >), S4 spatial-outer layers %llu\n",
                    (unsigned long long)st.core_passes, (unsigned long long)st.tiles, (unsigned long long)dut->s4_layers);
        std::printf("[tb_has_npu_top] weight blocks: DMA loads %llu, reused by B flip %llu\n",
                    (unsigned long long)st.weight_loads, (unsigned long long)st.weight_reuse_flips);
        if (st.attn || st.ln || st.rce_param_errors)
            std::printf("[tb_has_npu_top] RCE: FUSED_ATTN %llu, LAYERNORM %llu, rows %llu, cycles %llu (ESTIMATE), ovf16 %llu, param errors %llu\n",
                        (unsigned long long)st.attn, (unsigned long long)st.ln, (unsigned long long)st.rce_rows,
                        (unsigned long long)st.rce_cycles, (unsigned long long)st.rce_ovf16, (unsigned long long)st.rce_param_errors);
        if (st.rce_core_passes)
            std::printf("[tb_has_npu_top] RCE products on the core: passes %llu, core cycles %llu (MEASURED)\n",
                        (unsigned long long)st.rce_core_passes, (unsigned long long)st.rce_core_cycles);
        if (dut->halo_reuse)
            std::printf("[tb_has_npu_top] halo reuse: layers %llu, tiles %llu, rows copied in SRAM-A %llu B (%u B/cycle)\n",
                        (unsigned long long)dut->halo_layers, (unsigned long long)dut->halo_tiles, (unsigned long long)dut->halo_bytes,
                        dut->halo_copy_bpc);
        std::printf("[tb_has_net] HasElemwise: add %llu calls / %llu elems, max %llu calls / %llu elems, cycles %llu, req clamp8 %llu\n",
                    (unsigned long long)es.add_calls, (unsigned long long)es.add_elems, (unsigned long long)es.max_calls,
                    (unsigned long long)es.max_elems, (unsigned long long)st.elem_cycles, (unsigned long long)es.qc_req.clamp8);
        std::printf("[tb_has_npu_top] ELEM_WISE cycles: dma %llu, compute %llu, hidden by ping-pong (A2) %llu\n",
                    (unsigned long long)es.cyc_dma, (unsigned long long)es.cyc_compute, (unsigned long long)es.cyc_overlap_saved);
        const bool pass = bad_tensors == 0 && mmio_errors == 0 && dut->mmio.n_errors == 0 && st.core_deadlocks == 0 &&
                          st.framing_errors == 0 && checked == uint64_t(count) && ob.cfg_errors == 0 && ob.vectors_in == ob.vectors_out;
        std::printf("[tb_fe_core_net] RESULT: %s (steps %d, tensors %llu, elements %llu, bad tensors %llu, bad elements %llu, "
                    "framing errors %llu, core deadlocks %llu)\n", pass ? "PASS" : "FAIL", count,
                    (unsigned long long)(checked + host_steps), (unsigned long long)elems, (unsigned long long)bad_tensors,
                    (unsigned long long)bad_elems, (unsigned long long)st.framing_errors, (unsigned long long)st.core_deadlocks);
        std::printf("[tb_fe_core_net] tiles %llu (via core %llu), OBP vectors %llu, MACs (stand-in tiles) %llu, sim cycles %llu "
                    "(DMA wait %llu, OBP %llu, OBP config approx %llu)\n", (unsigned long long)st.tiles, (unsigned long long)st.tiles,
                    (unsigned long long)st.vectors, (unsigned long long)st.macs, (unsigned long long)cyc,
                    (unsigned long long)st.dma_cycles, (unsigned long long)st.obp_cycles, (unsigned long long)st.cfg_cycles);
        {
            std::istringstream gr(dut->gvu_report());
            for (std::string l; std::getline(gr, l);) std::printf("[GVU] %s\n", l.c_str());
        }
        if (const char *sd = getenv("FE_SNAPSHOT_DIR"))
        {
            const std::string p = std::string(sd) + "/dram_snapshot.bin";
            if (std::FILE *fp = std::fopen(p.c_str(), "wb")) { std::fwrite(dram.data(), 1, program_bytes, fp); std::fclose(fp); }
        }
        exit_code = pass ? 0 : 1;
        sc_stop();
    }
};

bool TbHasNpuTop::g_obp_inline = false;
bool TbHasNpuTop::g_desc3d = false;
bool TbHasNpuTop::g_s4 = false;
bool TbHasNpuTop::g_halo = false;
int TbHasNpuTop::g_halo_bpc = -1;
bool TbHasNpuTop::g_rce_core = false;
bool TbHasNpuTop::g_c_bcast = false;
Knobs TbHasNpuTop::g_knobs;
double TbHasNpuTop::g_clk_ns = 10.0;

int sc_main(int argc, char *argv[])
{
    sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <insts dir> [--profile legacy|recommended|proposals] [--first F] [--count N] [--trace] "
                             "[--dma v45|has] [--dram-lat N] [single options, see the file header]\n", argv[0]);
        return 2;
    }
    int first = 0, count = -1;
    bool sp_banked = false, gvu_lat = false;
    bool trace = false, overlap = false;
    DmaParams dp = DmaParams::v45();
    int dram_lat = 0;
    std::string dma;
    Profile profile = Profile::Recommended;
    for (int i = 2; i < argc; i++)
    {
        const std::string a = argv[i];
        if (a == "--profile" && i + 1 < argc)
        {
            const std::string p = argv[++i];
            if (p == "legacy") profile = Profile::Legacy;
            else if (p == "recommended") profile = Profile::Recommended;
            else if (p == "proposals") profile = Profile::Proposals;
            else { std::fprintf(stderr, "unknown profile %s (legacy | recommended | proposals)\n", p.c_str()); return 2; }
        }
        else if (a == "--first" && i + 1 < argc) first = std::atoi(argv[++i]);
        else if (a == "--count" && i + 1 < argc) count = std::atoi(argv[++i]);
        else if (a == "--trace") trace = true;
        else if (a == "--overlap") overlap = true;
        else if (a == "--obp-inline") TbHasNpuTop::g_obp_inline = true;
        else if (a == "--sp-banked") sp_banked = true;
        else if (a == "--gvu-lat") gvu_lat = true;
        else if (a == "--desc3d") TbHasNpuTop::g_desc3d = true;
        else if (a == "--s4") TbHasNpuTop::g_s4 = true;
        else if (a == "--halo") TbHasNpuTop::g_halo = true;
        else if (a == "--rce-core") TbHasNpuTop::g_rce_core = true;
        else if (a == "--c-bcast") TbHasNpuTop::g_c_bcast = true;
        else if (a == "--halo-copy-bpc" && i + 1 < argc) TbHasNpuTop::g_halo_bpc = std::atoi(argv[++i]);
        else if (a == "--dma" && i + 1 < argc) dma = argv[++i];
        else if (a == "--dram-lat" && i + 1 < argc) dram_lat = std::atoi(argv[++i]);
        else if (a == "--clk-ns" && i + 1 < argc) TbHasNpuTop::g_clk_ns = std::atof(argv[++i]);
    }
    // A profile sets the defaults of the options below; single options on the command line are added on top.
    if (profile != Profile::Legacy)
    {
        overlap = sp_banked = gvu_lat = true;
        TbHasNpuTop::g_obp_inline = TbHasNpuTop::g_desc3d = TbHasNpuTop::g_s4 = TbHasNpuTop::g_rce_core = true;
        if (dma.empty()) dma = "has";
    }
    if (profile == Profile::Proposals) TbHasNpuTop::g_halo = TbHasNpuTop::g_c_bcast = true;
    if (dma.empty()) dma = "v45";
    std::printf("[tb_has_npu_top] profile %s\n", profile_name(profile));
    TbHasNpuTop::g_knobs = Knobs::from_env();
    if (sp_banked) TbHasNpuTop::g_knobs.sp_banked = 1;
    if (gvu_lat) TbHasNpuTop::g_knobs.gvu_latencies();
    std::printf("[tb_has_npu_top] knobs %s\n", TbHasNpuTop::g_knobs.str().c_str());
    sc_clock clk("clk", TbHasNpuTop::g_clk_ns, SC_NS);
    if (dma == "has") dp = DmaParams::has_axi128(uint32_t(dram_lat));
    else dp.dram_latency = uint32_t(dram_lat);
    std::printf("[tb_has_npu_top] OBP %s\n", TbHasNpuTop::g_obp_inline && overlap ? "inline" : "after the core");
    std::printf("[tb_has_npu_top] mode %s, DMA %s (%u B/beat, burst %u, DRAM latency %u, CH3 low priority %d)\n", overlap ? "v1-ovl" : "v1-seq",
                dma.c_str(), dp.bytes_per_beat, dp.burst_beats, dp.dram_latency, int(dp.ch3_low_priority));
    TbHasNpuTop tb("tb", argv[1], first, count, trace, overlap, dp);
    tb.i_clk(clk);
    sc_start();
    return tb.exit_code;
}
