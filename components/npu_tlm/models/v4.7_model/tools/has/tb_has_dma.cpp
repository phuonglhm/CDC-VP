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
