// tb_has_dma.cpp -- unit test of has/has_dma.h.
//   D1  parity: SauriaDma (control/sauria_dma.h) and HasDma with v4.5 parameters run the same random transfer script
//       (4 read channels + write channel, random sizes / start times): completion cycle of every transfer, final DRAM and
//       final SRAM contents must be identical
//   D2  HAS parameters (AXI 128-bit, CH3 low priority, DRAM latency 0 / 50 / 100 cycles) on the transfer mix of one
//       typical tile: cycles against the v4.5 model (about 2x, measured)
#include <systemc.h>
#include <cstdio>
#include <random>
#include <vector>
#include "sauria_types.h"
#include "sram/sram_top.h"
#include "control/sauria_dma.h"
#include "has/has_dma.h"

using namespace sauria;
typedef Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> SramT;
typedef SauriaDma<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> DmaT;

// A v4.5 Sram with every datapath port tied off (only the backdoor is used by DMAs).
struct SramBox : sc_module
{
    SramT sram{"sram"};
    sc_signal<bool> f{"f"};
    sc_signal<sc_bv<3>> sel{"sel"};
    sc_signal<uint32_t> ha{"ha"}, z[6];
    sc_signal<host_data_t> hw{"hw"}, hr{"hr"};
    sc_signal<host_mask_t> hm{"hm"};
    sc_signal<act_vector_t<32, int8_t>> ad[2];
    sc_signal<wei_vector_t<32, int8_t>> bd[2];
    sc_signal<psum_vector_t<32, int32_t>> cz[2], cr[2];
    sc_signal<sramc_mask_t<32>> cm[2];
    SramBox(sc_module_name n, sc_signal<bool> &clk, sc_signal<bool> &rstn) : sc_module(n)
    {
        sram.i_clk(clk); sram.i_rstn(rstn); sram.i_deepsleep(f); sram.i_powergate(f); sram.i_select(sel);
        sram.i_host_addr(ha); sram.i_host_wren(f); sram.i_host_rden(f); sram.i_host_wdata(hw); sram.i_host_wmask(hm); sram.o_host_rdata(hr);
        sram.i_srama_addr_a(z[0]); sram.i_srama_rden_a(f); sram.o_srama_data_a(ad[0]);
        sram.i_srama_addr_b(z[1]); sram.i_srama_rden_b(f); sram.o_srama_data_b(ad[1]);
        sram.i_sramb_addr_a(z[2]); sram.i_sramb_rden_a(f); sram.o_sramb_data_a(bd[0]);
        sram.i_sramb_addr_b(z[3]); sram.i_sramb_rden_b(f); sram.o_sramb_data_b(bd[1]);
        sram.i_sramc_wdata_a(cz[0]); sram.i_sramc_addr_a(z[4]); sram.i_sramc_wren_a(f); sram.i_sramc_rden_a(f);
        sram.i_sramc_wmask_a(cm[0]); sram.o_sramc_rdata_a(cr[0]);
        sram.i_sramc_wdata_b(cz[1]); sram.i_sramc_addr_b(z[5]); sram.i_sramc_wren_b(f); sram.i_sramc_rden_b(f);
        sram.i_sramc_wmask_b(cm[1]); sram.o_sramc_rdata_b(cr[1]);
    }
};

struct PortV45
{
    SramT *s;
    void write(int b, uint32_t o, const uint8_t *p, uint32_t n) { s->write_bank_data(b, o, p, n); }
    void read(int b, uint32_t o, uint8_t *p, uint32_t n) { s->read_bank_data(b, o, p, n); }
};
typedef has::HasDma<PortV45> HDmaT;

struct Op { int at; int ch; uint32_t dram, off, size; };   // ch 0..3 read, 4 write

SC_MODULE(Tb)
{
    sc_signal<bool> clk{"clk"}, rstn{"rstn"};
    SramBox *box_ref, *box_new;
    DmaT *ref;
    HDmaT *neu;
    PortV45 port;
    std::vector<uint8_t> dram_ref, dram_new;
    int fails{0};

    SC_CTOR(Tb)
    {
        box_ref = new SramBox("box_ref", clk, rstn);
        box_new = new SramBox("box_new", clk, rstn);
        ref = new DmaT("ref");
        ref->i_clk(clk); ref->i_rstn(rstn); ref->set_sram(&box_ref->sram); ref->set_dram(&dram_ref);
        neu = new HDmaT("neu", has::DmaParams::v45());
        neu->i_clk(clk); neu->i_rstn(rstn);
        port.s = &box_new->sram;
        neu->set_port(&port); neu->set_dram(&dram_new);
        SC_THREAD(clock_gen);
        SC_THREAD(run);
    }

    void clock_gen() { for (;;) { clk.write(false); wait(5, SC_NS); clk.write(true); wait(5, SC_NS); } }
    void tick(int n = 1) { for (int i = 0; i < n; i++) wait(clk.posedge_event()); }

    template <class D> bool active(D *d, int ch) { return ch < 4 ? d->is_read_active(ch) : d->is_write_active(); }
    template <class D> void start(D *d, const Op &o)
    {
        if (o.ch < 4) d->start_read(o.ch, o.dram, o.ch < 2 ? (o.ch == 0 ? 0 : 2) : (o.ch == 2 ? 3 : 4), o.off, o.size);
        else d->start_write(o.dram, 4, o.off, o.size);
    }

    // Runs a script on one DMA; returns the completion cycle of every op (-1 = never).
    template <class D> std::vector<long> play(D *d, const std::vector<Op> &ops, long limit)
    {
        rstn.write(false); tick(3); rstn.write(true); tick(1);
        std::vector<long> done(ops.size(), -1);
        std::vector<int> owner(5, -1);
        size_t next = 0;
        for (long cyc = 0; cyc < limit; cyc++)
        {
            for (int ch = 0; ch < 5; ch++)
                if (owner[ch] >= 0 && !active(d, ch)) { done[size_t(owner[ch])] = cyc; owner[ch] = -1; }
            while (next < ops.size() && ops[next].at <= cyc && owner[ops[next].ch] < 0)
            { start(d, ops[next]); owner[ops[next].ch] = int(next); next++; }
            if (next == ops.size() && owner[0] < 0 && owner[1] < 0 && owner[2] < 0 && owner[3] < 0 && owner[4] < 0) break;
            tick();
        }
        return done;
    }

    void run()
    {
        // ---------------- D1 parity
        std::mt19937 rng(20260925);
        std::vector<Op> ops;
        int t = 0;
        for (int i = 0; i < 400; i++)
        {
            t += int(rng() % 60);
            const int ch = int(rng() % 5);
            const uint32_t size = 1 + rng() % 9000, off = (rng() % 64) * 32;
            ops.push_back({t, ch, uint32_t(rng() % 200000), off, size});
        }
        std::vector<uint8_t> dram0(300000);
        for (auto &b : dram0) b = uint8_t(rng());
        dram_ref = dram0; dram_new = dram0;
        const auto d_ref = play(ref, ops, 5000000);
        const auto d_new = play(neu, ops, 5000000);
        long mism = 0, never = 0;
        for (size_t i = 0; i < ops.size(); i++) { mism += d_ref[i] != d_new[i]; never += d_ref[i] < 0; }
        long dram_diff = 0;
        for (size_t i = 0; i < dram_ref.size() && i < dram_new.size(); i++) dram_diff += dram_ref[i] != dram_new[i];
        dram_diff += long(dram_ref.size() != dram_new.size());
        long sram_diff = 0;
        for (int bank = 0; bank < 5; bank++)
        {
            std::vector<uint8_t> a(16384), b(16384);
            box_ref->sram.read_bank_data(bank, 0, a.data(), uint32_t(a.size()));
            box_new->sram.read_bank_data(bank, 0, b.data(), uint32_t(b.size()));
            for (size_t i = 0; i < a.size(); i++) sram_diff += a[i] != b[i];
        }
        std::printf("[D1] parity with SauriaDma on %zu random transfers: completion-cycle mismatches %ld (never done %ld), "
                    "DRAM bytes differing %ld, SRAM bytes differing %ld, last completion cycle %ld\n",
                    ops.size(), mism, never, dram_diff, sram_diff, d_ref.back());
        if (mism || never || dram_diff || sram_diff) fails++;

        // ---------------- D2 HAS parameters on one tile's transfer mix (a dark3.c2f.b0.1-like layer:
        // A window 96 x 22 x 34, weights 32 x 864, preload 32 x 20 x 32 int32 -> 640 words x 128 B, write-back same)
        const uint32_t A = 96 * 22 * 34, W = 32 * 864, P = 640 * 128;
        const std::vector<Op> tile = {{0, 1, 0, 0, A}, {0, 0, 100000, 0, W}, {0, 3, 150000, 0, P}, {0, 4, 250000, 0, P}};
        const has::DmaParams cases[] = {has::DmaParams::v45(), has::DmaParams::has_axi128(0), has::DmaParams::has_axi128(50),
                                        has::DmaParams::has_axi128(100)};
        const char *names[] = {"v4.5 model (32 B/cycle)", "HAS AXI-128, latency 0", "HAS AXI-128, latency 50", "HAS AXI-128, latency 100"};
        long base = 0;
        for (int k = 0; k < 4; k++)
        {
            neu->prm = cases[k];
            dram_new.assign(400000, 1);
            const auto d = play(neu, tile, 1000000);
            long end = 0;
            for (long c : d) end = std::max(end, c);
            if (k == 0) base = end;
            std::printf("[D2] %-26s reads %u B + write %u B: all done at cycle %6ld (%.2fx v4.5)\n", names[k], A + W + P, P, end,
                        double(end) / double(base));
        }

        // ---------------- D3 4-D DMA Slice / Split / Concat (C, H, W axes + YOLOv8m C2f & SPPF)
        neu->prm = has::DmaParams::v45();
        rstn.write(false); tick(3); rstn.write(true); tick(1);
        auto run_dma_nd = [&](const has::DmaNdDesc &sd, const has::DmaNdDesc &dd) {
            neu->start_read_nd(1, sd, 2, 0);
            tick();
            while (neu->is_read_active(1)) tick();
            neu->start_write_nd(dd, 2, 0);
            tick();
            while (neu->is_write_active()) tick();
        };
        long d3_bad = 0;
        // (a) Split & concat along C (axis 0), H (axis 1), W (axis 2) on a [16][12][20] tensor
        {
            const unsigned C = 16, H = 12, W = 20, N = C * H * W;
            dram_new.assign(200000, 0);
            for (unsigned i = 0; i < N; i++) dram_new[1000 + i] = uint8_t((i * 73 + 19) & 0xFF);
            for (int axis = 0; axis < 3; axis++)
            {
                const unsigned dim = (axis == 0) ? C : (axis == 1 ? H : W);
                const unsigned k1 = dim / 2, k2 = dim - k1;
                const uint32_t part1_addr = 10000, part2_addr = 20000, out_addr = 30000 + uint32_t(axis) * 10000;
                // Slice part 1 [0..k1) and part 2 [k1..dim)
                const auto sd1 = has::DmaNdDesc::tensor_chw(1000, C, H, W).sliced_chw(axis, 0, k1, 1);
                const auto dd1 = has::DmaNdDesc::linear(part1_addr, sd1.total_bytes(), 1);
                run_dma_nd(sd1, dd1);
                const auto sd2 = has::DmaNdDesc::tensor_chw(1000, C, H, W).sliced_chw(axis, k1, k2, 1);
                const auto dd2 = has::DmaNdDesc::linear(part2_addr, sd2.total_bytes(), 1);
                run_dma_nd(sd2, dd2);
                // Concat part 1 and part 2 back into [C][H][W] at out_addr
                const auto cd1 = has::DmaNdDesc::concat_chw(out_addr, C, H, W, axis, 0, k1, 1);
                run_dma_nd(has::DmaNdDesc::linear(part1_addr, cd1.total_bytes(), 1), cd1);
                const auto cd2 = has::DmaNdDesc::concat_chw(out_addr, C, H, W, axis, k1, k2, 1);
                run_dma_nd(has::DmaNdDesc::linear(part2_addr, cd2.total_bytes(), 1), cd2);
                for (unsigned i = 0; i < N; i++)
                    if (dram_new[out_addr + i] != dram_new[1000 + i]) d3_bad++;
            }
        }
        // (b) Strided slice along H and W (step 2) and YOLOv8m SPPF 4-way channel concat (4 x [8][10][10] -> [32][10][10])
        {
            const unsigned C = 8, H = 10, W = 10, P = C * H * W;
            for (unsigned p = 0; p < 4; p++)
                for (unsigned i = 0; i < P; i++)
                    dram_new[80000 + p * P + i] = uint8_t((p * 53 + i * 17 + 3) & 0xFF);
            const uint32_t sppf_out = 90000;
            for (unsigned p = 0; p < 4; p++)
            {
                const auto cd = has::DmaNdDesc::concat_chw(sppf_out, 4 * C, H, W, 0, p * C, C, 1);
                run_dma_nd(has::DmaNdDesc::linear(80000 + p * P, P, 1), cd);
            }
            for (unsigned p = 0; p < 4; p++)
                for (unsigned i = 0; i < P; i++)
                    if (dram_new[sppf_out + p * P + i] != dram_new[80000 + p * P + i]) d3_bad++;
        }
        std::printf("[D3] 4-D DMA Slice/Split/Concat (C/H/W axes + SPPF 4-way concat): mismatches %ld, rd_nd_ops %llu, wr_nd_ops %llu\n",
                    d3_bad, (unsigned long long)neu->st.rd_nd_ops, (unsigned long long)neu->st.wr_nd_ops);
        if (d3_bad) fails++;

        // ---------------- D4 3-D tile window gather & SRAM-C int32->int8 write-back scatter/concat
        long d4_bad = 0;
        {
            const unsigned OC = 12, OH = 8, OW = 16;
            const uint32_t stage_c = 120000, out_base = 140000;
            dram_new.assign(200000, 0);
            // Tile [c0=4..10, y0=2..6, x0=4..12]: nch=8 (with 2 tail channels >6), ht=4, wt=8
            const uint32_t nch = 8, ht = 4, wt = 8, npos = ht * wt, swords = (nch * npos + 31) / 32;
            std::vector<int32_t> sramc_words(swords * 32, 0);
            for (uint32_t x = 0; x < nch; x++)
                for (uint32_t p = 0; p < npos; p++)
                    sramc_words[x * npos + p] = int32_t(int8_t((x * 31 + p * 7) & 0xFF));
            box_new->sram.write_bank_data(4, 0, reinterpret_cast<const uint8_t *>(sramc_words.data()), swords * 128);
            const uint32_t real_c = 6, real_w = 8; // only 6 of the 8 channels are real
            const auto wb_desc = has::DmaNdDesc::tile_writeback_3d(out_base, OC, OH, OW, 4, real_c, 2, ht, 4, real_w);
            neu->start_write_sramc_nd(stage_c, 4, 0, swords * 128, real_c * ht * real_w, wb_desc, nch, ht, wt, real_c, real_w);
            tick();
            while (neu->is_write_active()) tick();
            for (uint32_t c = 0; c < OC; c++)
                for (uint32_t y = 0; y < OH; y++)
                    for (uint32_t x = 0; x < OW; x++)
                    {
                        const uint8_t got = dram_new[out_base + (c * OH + y) * OW + x];
                        uint8_t want = 0;
                        if (c >= 4 && c < 4 + real_c && y >= 2 && y < 2 + ht && x >= 4 && x < 4 + real_w)
                        {
                            const uint32_t xc = c - 4, p = (y - 2) * wt + (x - 4);
                            want = uint8_t((xc * 31 + p * 7) & 0xFF);
                        }
                        if (got != want) d4_bad++;
                    }
        }
        std::printf("[D4] 3-D tile SRAM-C int32->int8 write-back scatter/concat: mismatches %ld\n", d4_bad);
        if (d4_bad) fails++;

        // ---------------- D5 YOLOv8m C2f 3-way split/concat + 300 random 4-D slice/concat descriptors
        long d5_bad = 0;
        {
            // (a) YOLOv8m C2f: split [32, 12, 16] -> branch0 [16, 12, 16], branch1 [16, 12, 16]; concat 3 branches -> [48, 12, 16]
            const unsigned C = 16, H = 12, W = 16, P = C * H * W;
            dram_new.assign(300000, 0);
            for (unsigned i = 0; i < 2 * P; i++) dram_new[2000 + i] = uint8_t((i * 41 + 7) & 0xFF);
            for (unsigned i = 0; i < P; i++) dram_new[50000 + i] = uint8_t((i * 97 + 31) & 0xFF);
            const uint32_t b0_addr = 20000, b1_addr = 30000, c2f_out = 100000;
            run_dma_nd(has::DmaNdDesc::tensor_chw(2000, 2 * C, H, W).sliced_chw(0, 0, C, 1),
                       has::DmaNdDesc::linear(b0_addr, P, 1));
            run_dma_nd(has::DmaNdDesc::tensor_chw(2000, 2 * C, H, W).sliced_chw(0, C, C, 1),
                       has::DmaNdDesc::linear(b1_addr, P, 1));
            run_dma_nd(has::DmaNdDesc::linear(b0_addr, P, 1),
                       has::DmaNdDesc::concat_chw(c2f_out, 3 * C, H, W, 0, 0, C, 1));
            run_dma_nd(has::DmaNdDesc::linear(b1_addr, P, 1),
                       has::DmaNdDesc::concat_chw(c2f_out, 3 * C, H, W, 0, C, C, 1));
            run_dma_nd(has::DmaNdDesc::linear(50000, P, 1),
                       has::DmaNdDesc::concat_chw(c2f_out, 3 * C, H, W, 0, 2 * C, C, 1));
            for (unsigned i = 0; i < 2 * P; i++)
                if (dram_new[c2f_out + i] != dram_new[2000 + i]) d5_bad++;
            for (unsigned i = 0; i < P; i++)
                if (dram_new[c2f_out + 2 * P + i] != dram_new[50000 + i]) d5_bad++;

            // (b) 300 random 4-D slice & concat descriptors across C, H, W axes with random strides (1..4)
            std::mt19937 r5(20261028);
            for (int iter = 0; iter < 300; iter++)
            {
                const uint32_t rC = 2 + r5() % 15, rH = 2 + r5() % 15, rW = 2 + r5() % 20;
                const uint32_t rN = rC * rH * rW;
                const int axis = int(r5() % 3);
                const uint32_t dim = (axis == 0) ? rC : (axis == 1 ? rH : rW);
                const uint32_t start = r5() % dim;
                const uint32_t len = 1 + r5() % (dim - start);
                const uint32_t step = 1 + r5() % 4;
                const uint32_t sliced_dim = (len + step - 1) / step;
                const uint32_t src_base = 1000, slice_base = 60000, cat_base = 120000;
                for (uint32_t i = 0; i < rN; i++) dram_new[src_base + i] = uint8_t((iter * 13 + i * 29 + 5) & 0xFF);
                const auto sd = has::DmaNdDesc::tensor_chw(src_base, rC, rH, rW, 1).sliced_chw(axis, start, len, step);
                const auto dd = has::DmaNdDesc::linear(slice_base, sd.total_bytes(), 1);
                run_dma_nd(sd, dd);
                // Verify sliced tensor matches golden strided slice
                const uint32_t oC = (axis == 0) ? sliced_dim : rC;
                const uint32_t oH = (axis == 1) ? sliced_dim : rH;
                const uint32_t oW = (axis == 2) ? sliced_dim : rW;
                uint32_t idx = 0;
                for (uint32_t c = 0; c < oC; c++)
                    for (uint32_t y = 0; y < oH; y++)
                        for (uint32_t x = 0; x < oW; x++)
                        {
                            const uint32_t sc = (axis == 0) ? (start + c * step) : c;
                            const uint32_t sy = (axis == 1) ? (start + y * step) : y;
                            const uint32_t sx = (axis == 2) ? (start + x * step) : x;
                            const uint8_t want = dram_new[src_base + (sc * rH + sy) * rW + sx];
                            if (dram_new[slice_base + idx] != want) d5_bad++;
                            idx++;
                        }
                // Scatter-concat the sliced tensor back into a zeroed [rC, rH, rW] tensor at offset `start` (when step==1)
                if (step == 1)
                {
                    std::fill(dram_new.begin() + cat_base, dram_new.begin() + cat_base + rN, 0);
                    const auto cd = has::DmaNdDesc::concat_chw(cat_base, rC, rH, rW, axis, start, len, 1);
                    run_dma_nd(has::DmaNdDesc::linear(slice_base, cd.total_bytes(), 1), cd);
                    for (uint32_t c = 0; c < rC; c++)
                        for (uint32_t y = 0; y < rH; y++)
                            for (uint32_t x = 0; x < rW; x++)
                            {
                                const uint32_t coord = (axis == 0) ? c : (axis == 1 ? y : x);
                                const bool in_slice = (coord >= start && coord < start + len);
                                const uint8_t want = in_slice ? dram_new[src_base + (c * rH + y) * rW + x] : 0;
                                if (dram_new[cat_base + (c * rH + y) * rW + x] != want) d5_bad++;
                            }
                }
            }
        }
        std::printf("[D5] YOLOv8m C2f 3-way split/concat + 300 random 4-D slice/concat descriptors: mismatches %ld, total rd_nd_ops %llu, wr_nd_ops %llu, nd_bytes %llu\n",
                    d5_bad, (unsigned long long)neu->st.rd_nd_ops, (unsigned long long)neu->st.wr_nd_ops, (unsigned long long)neu->st.nd_bytes);
        if (d5_bad) fails++;

        std::printf("[tb_has_dma] RESULT: %s\n", fails ? "FAIL" : "PASS");
        sc_stop();
    }
};

int sc_main(int, char *[])
{
    sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
    Tb tb("tb");
    sc_start();
    return tb.fails ? 1 : 0;
}
