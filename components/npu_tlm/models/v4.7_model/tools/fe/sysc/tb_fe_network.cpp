// Step 6b: whole YOLOv8m program through the shared SystemC
// modules, used unmodified via #include: sram/sram_top.h (Sram), control/sauria_dma.h (SauriaDma),
// psm/obp_top.h (Obp). Layout follows the HAS:
//   per conv tile: host builds the padded input window, the PSUM preload (bias) and skip in C-order in DRAM
//   staging buffers -> DMA CH1/CH0/CH3/CH2 read into IFmap bank 2 / Weight bank 0 / PSUM bank 4 / IFmap bank 3
//   -> CORE STAND-IN (functional, not cycle-accurate: accumulator = preload + conv products, computed from
//   the SRAM banks) -> Obp vectors (one per output channel; a context = y_used positions of ONE output row,
//   core mapping of driver/libsauria_cfg.h; idle between contexts)
//   -> OBP outputs written into PSUM bank 4 (C-order [k,h,w]) -> DMA write channel -> host scatters into the tensor.
//   host ops (slice/concat/maxpool/upsample) run on DRAM tensor regions.
// The core stand-in is the only non-shared compute block; it is the place RtlRefLaneACoreA goes later.
//
// Input: FE_WORK/step6b/net/{prog.bin, dram_init.bin, dram_golden.bin} from tools/fe/fe_step6b_export_net.py
// Usage: tb_fe_network <net_dir> [max_steps]

#include <systemc.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "sauria_types.h"
#include "sram/sram_top.h"
#include "control/sauria_dma.h"
#include "psm/obp_top.h"

using namespace sauria;

static constexpr int W32 = 32;
typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef SauriaDma<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> DmaT;
typedef Obp<W32, 0x00140000, 0x00150000, int32_t, int8_t> ObpT;
static constexpr uint32_t LUT_BASE = 0x00140000, SCALE_BASE = 0x00180000, SHIFT_BASE = 0x00190000;
static constexpr uint32_t NONE = 0xFFFFFFFF;
static constexpr int IDLE_CYCLES = 6;

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

struct Cursor
{
    const std::vector<uint8_t> &b;
    size_t pos{0};
    explicit Cursor(const std::vector<uint8_t> &buf) : b(buf) {}
    uint32_t u32() { uint32_t v; std::memcpy(&v, &b.at(pos + 3) - 3, 4); pos += 4; return v; }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    uint8_t u8() { return b.at(pos++); }
};

SC_MODULE(TbFeNetwork)
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
    std::string dir;
    int max_steps{-1};
    std::vector<uint8_t> dram;
    uint64_t obp_rows_written{0};
    int exit_code{1};

    SC_HAS_PROCESS(TbFeNetwork);
    TbFeNetwork(sc_module_name n, const std::string &d, int ms) : sc_module(n), dir(d), max_steps(ms)
    {
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

        obp = new ObpT("obp");
        obp->i_clk(i_clk); obp->i_rstn(rstn);
        obp->i_data(o_in); obp->i_addr(o_in_addr); obp->i_wmask(o_in_mask); obp->i_valid(o_in_valid); obp->i_residual(o_residual);
        obp->o_sramc_wdata(o_out); obp->o_sramc_addr(o_out_addr); obp->o_sramc_wren(o_out_wren); obp->o_sramc_wmask(o_out_mask);
        obp->o_valid(o_out_valid);
        obp->i_bias_en(bias_en); obp->i_requant_en(requant_en); obp->i_lut_en(lut_en); obp->i_residual_en(residual_en);
        obp->i_vec_channel_mode(vec_mode); obp->i_requant_scale(def_scale); obp->i_requant_shift(def_shift);
        obp->i_host_addr(host_addr); obp->i_host_wren(host_wren); obp->i_host_rden(host_rden);
        obp->i_host_wdata(host_wdata); obp->i_host_wmask(host_wmask); obp->o_host_rdata(host_rdata);

        SC_THREAD(run);
        sensitive << i_clk.pos();
        SC_METHOD(obp_writeback);
        sensitive << i_clk.pos();
        dont_initialize();
    }
    ~TbFeNetwork() { delete obp; delete dma; delete sram; }

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

    void wait_dma(uint64_t &cycles)
    {
        while (dma->is_any_read_active() || dma->is_write_active()) { wait(); cycles++; }
    }

    void run()
    {
        std::vector<uint8_t> progb, gold;
        if (!read_file(dir + "/prog.bin", progb) || !read_file(dir + "/dram_init.bin", dram) || !read_file(dir + "/dram_golden.bin", gold))
        {
            std::cout << "[tb_fe_network] cannot read inputs in " << dir << std::endl;
            sc_stop();
            return;
        }
        Cursor c(progb);
        if (std::string(reinterpret_cast<const char *>(progb.data()), 4) != "FENP") { std::cout << "bad magic" << std::endl; sc_stop(); return; }
        c.pos = 4;
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
            else if (s.kind == 3) { s.in = c.u32(); s.out = c.u32(); s.k = c.u32(); s.s = c.u32(); s.p = c.u32(); }
            else if (s.kind == 4) { s.in = c.u32(); s.out = c.u32(); s.factor = c.u32(); }
        }

        bias_en.write(false); requant_en.write(true); lut_en.write(false); residual_en.write(false); vec_mode.write(true);
        def_scale.write(1); def_shift.write(0); o_in_valid.write(false); host_wren.write(false); host_rden.write(false);
        sram_deepsleep.write(false); sram_powergate.write(false); s_host_wren.write(false); s_host_rden.write(false);
        rstn.write(false);
        wait(5);
        rstn.write(true);
        wait(2);

        uint64_t dma_cycles = 0, obp_cycles = 0, cfg_cycles = 0, tiles_done = 0, vectors = 0, framing_errors = 0;
        uint64_t mac_ops = 0;
        std::set<uint32_t> written;
        int n_steps = max_steps >= 0 ? std::min<int>(max_steps, steps.size()) : static_cast<int>(steps.size());
        for (int si = 0; si < n_steps; si++)
        {
            const Step &s = steps[si];
            if (s.kind == 0)
            {
                const Tensor &src = tensors[s.in], &dst = tensors[s.out];
                const Tensor *skp = s.skip != NONE ? &tensors[s.skip] : nullptr;
                uint32_t K = s.cin * s.kh * s.kw;
                lut_en.write(s.silu != 0);
                residual_en.write(skp != nullptr);
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
                for (const Tile &tl : s.tiles)
                {
                    uint32_t nch = tl.c1 - tl.c0, ht = tl.oy1 - tl.oy0, wt = tl.ox1 - tl.ox0, npos = ht * wt;
                    uint32_t ah = tl.iy1 - tl.iy0, aw = tl.ix1 - tl.ix0;
                    uint32_t yu = tl.yu, nel = nch * npos, words = (nel + W32 - 1) / W32;
                    // core mapping: context = yu positions of one output row x nch channels (x_used = nch)
                    uint32_t n_ctx = ht * (wt / yu);
                    // host: padded input window (zero = INT8 zero point)
                    for (uint32_t ci = 0; ci < s.cin; ci++)
                        for (uint32_t y = 0; y < ah; y++)
                            for (uint32_t x = 0; x < aw; x++)
                            {
                                int sy = tl.iy0 + static_cast<int>(y), sx = tl.ix0 + static_cast<int>(x);
                                int8_t v = (sy >= 0 && sx >= 0 && sy < static_cast<int>(src.h) && sx < static_cast<int>(src.w)) ? t8(src, ci, sy, sx) : 0;
                                dram[STAGE_A + (ci * ah + y) * aw + x] = static_cast<uint8_t>(v);
                            }
                    // host: PSUM preload (bias) and skip, both C-order [k, h, w]; element e = (x*ht + oy)*wt + ox
                    std::memset(&dram[STAGE_PRE], 0, words * 128);
                    for (uint32_t x = 0; x < nch; x++)
                    {
                        int32_t b = dram_i32(s.bias_addr + 4 * (tl.c0 + x));
                        for (uint32_t p = 0; p < npos; p++)
                        {
                            uint32_t e = x * npos + p;
                            std::memcpy(&dram[STAGE_PRE + 4 * e], &b, 4);
                            if (skp) dram[STAGE_SKIP + e] = static_cast<uint8_t>(t8(*skp, tl.c0 + x, tl.oy0 + p / wt, tl.ox0 + p % wt));
                        }
                    }
                    dma->start_read(1, STAGE_A, 2, 0, s.cin * ah * aw);
                    dma->start_read(0, s.w_addr + tl.c0 * K, 0, 0, nch * K);
                    dma->start_read(3, STAGE_PRE, 4, 0, words * 128);
                    if (skp) dma->start_read(2, STAGE_SKIP, 3, 0, nel);
                    wait();
                    dma_cycles++;
                    wait_dma(dma_cycles);
                    // OBP per-channel parameters for this tile (HAS: DMA CH3 config load; here host MMIO)
                    for (uint32_t x = 0; x < nch; x++)
                    {
                        write_u32(SCALE_BASE + 4 * x, dram_u32(s.scale_addr + 4 * (tl.c0 + x)));
                        write_u32(SHIFT_BASE + 4 * x, dram_u32(s.shift_addr + 4 * (tl.c0 + x)));
                        cfg_cycles += 4;
                    }
                    // CORE STAND-IN: read SRAM banks, accumulator = preload + sum of products (C-order)
                    std::vector<int8_t> a(s.cin * ah * aw), w(nch * K), sk(skp ? nel : 0);
                    std::vector<int32_t> pre(words * W32);
                    sram->read_bank_data(2, 0, reinterpret_cast<uint8_t *>(a.data()), a.size());
                    sram->read_bank_data(0, 0, reinterpret_cast<uint8_t *>(w.data()), w.size());
                    sram->read_bank_data(4, 0, reinterpret_cast<uint8_t *>(pre.data()), pre.size() * 4);
                    if (skp) sram->read_bank_data(3, 0, reinterpret_cast<uint8_t *>(sk.data()), sk.size());
                    std::vector<int64_t> acc(nel, 0);
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
                    mac_ops += uint64_t(nch) * npos * K;
                    // OBP: per context one vector per channel; i_addr = vector id; idle between contexts
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
                        wait(IDLE_CYCLES);
                        obp_cycles += IDLE_CYCLES;
                        if (obp_rows_written - before != nch) framing_errors++;
                        vectors += nch;
                    }
                    dma->start_write(STAGE_OUT, 4, 0, words * 128);
                    wait();
                    dma_cycles++;
                    wait_dma(dma_cycles);
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
            else if (s.kind == 3)
            {
                const Tensor &src = tensors[s.in], &dst = tensors[s.out];
                for (uint32_t ch = 0; ch < dst.c; ch++)
                    for (uint32_t y = 0; y < dst.h; y++)
                        for (uint32_t x = 0; x < dst.w; x++)
                        {
                            int m = -128;
                            for (uint32_t ky = 0; ky < s.k; ky++)
                                for (uint32_t kx = 0; kx < s.k; kx++)
                                {
                                    int sy = int(y * s.s + ky) - int(s.p), sx = int(x * s.s + kx) - int(s.p);
                                    if (sy >= 0 && sx >= 0 && sy < int(src.h) && sx < int(src.w)) m = std::max<int>(m, t8(src, ch, sy, sx));
                                }
                            t8(dst, ch, y, x) = static_cast<int8_t>(m);
                        }
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
            if (si % 10 == 9 || si == n_steps - 1)
                std::printf("[tb_fe_network] step %d/%d, tiles %llu, sim cycles %llu\n", si + 1, n_steps,
                            (unsigned long long)tiles_done, (unsigned long long)(sc_time_stamp().value() / 10000));
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
        bool pass = bad_tensors == 0 && framing_errors == 0 && !written.empty();
        std::printf("[tb_fe_network] RESULT: %s (steps %d, tensors %zu, elements %llu, bad tensors %llu, bad elements %llu, "
                    "framing errors %llu)\n", pass ? "PASS" : "FAIL", n_steps, written.size(), (unsigned long long)elems,
                    (unsigned long long)bad_tensors, (unsigned long long)bad_elems, (unsigned long long)framing_errors);
        std::printf("[tb_fe_network] tiles %llu, OBP vectors %llu, MACs (stand-in) %llu, sim cycles %llu "
                    "(DMA wait %llu, OBP %llu, OBP config approx %llu)\n", (unsigned long long)tiles_done,
                    (unsigned long long)vectors, (unsigned long long)mac_ops, (unsigned long long)(sc_time_stamp().value() / 10000),
                    (unsigned long long)dma_cycles, (unsigned long long)obp_cycles, (unsigned long long)cfg_cycles);
        exit_code = pass ? 0 : 1;
        sc_stop();
    }
};

int sc_main(int argc, char *argv[])
{
    sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
    std::string dir = argc > 1 ? argv[1] : "fe_work/step6b/net";
    int max_steps = argc > 2 ? std::stoi(argv[2]) : -1;
    sc_clock clk("clk", 10, SC_NS);
    TbFeNetwork tb("tb", dir, max_steps);
    tb.i_clk(clk);
    sc_start();
    return tb.exit_code;
}
