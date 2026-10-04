// tb_has_sram_side.cpp -- unit test of write_host_side()/read_host_side() in sram/rtl_ref_sram_top.h.
// For each of SRAM A, B, C:
//   S1  with i_select = 000, write_host_side() fills the host half: the NPU half (read_bank_data) must stay untouched
//   S2  flip the category's select bit: the core's own read port (o_srama_data / o_sramb_data / o_sramc_rdata, driven by
//       beh_process) returns the pattern, and the host half is now the other (still empty) buffer
//   S3  independent bits: flipping A does not change what the host side of B/C sees
#include <systemc.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "sauria_types.h"
#include "sram/rtl_ref_sram_top.h"

using namespace sauria;
typedef sauria_rtl::Sram<32, 32, int8_t, int8_t, int32_t, 79 * 1024, 81 * 1024, 1536> SramT;

SC_MODULE(Tb)
{
    sc_clock clk{"clk", 10, SC_NS};
    sc_signal<bool> rstn{"rstn"}, f{"f"}, arden{"arden"}, brden{"brden"}, crden{"crden"};
    sc_signal<sc_bv<3>> sel{"sel"};
    sc_signal<uint32_t> ha{"ha"}, aaddr{"aaddr"}, baddr{"baddr"}, caddr{"caddr"};
    sc_signal<host_data_t> hw{"hw"}, hr{"hr"};
    sc_signal<host_mask_t> hm{"hm"};
    sc_signal<act_vector_t<32, int8_t>> adata{"adata"};
    sc_signal<wei_vector_t<32, int8_t>> bdata{"bdata"};
    sc_signal<psum_vector_t<32, int32_t>> cw{"cw"}, cr{"cr"};
    sc_signal<sramc_mask_t<32>> cm{"cm"};
    SramT *s;
    int fails{0};

    SC_CTOR(Tb)
    {
        s = new SramT("sram");
        s->i_clk(clk); s->i_rstn(rstn); s->i_deepsleep(f); s->i_powergate(f); s->i_select(sel);
        s->i_host_addr(ha); s->i_host_wren(f); s->i_host_rden(f); s->i_host_wdata(hw); s->i_host_wmask(hm); s->o_host_rdata(hr);
        s->i_srama_addr(aaddr); s->i_srama_rden(arden); s->o_srama_data(adata);
        s->i_sramb_addr(baddr); s->i_sramb_rden(brden); s->o_sramb_data(bdata);
        s->i_sramc_wdata(cw); s->i_sramc_addr(caddr); s->i_sramc_wren(f); s->i_sramc_rden(crden); s->i_sramc_wmask(cm); s->o_sramc_rdata(cr);
        SC_THREAD(run);
        sensitive << clk.posedge_event();
    }

    void check(bool ok, const char *what) { if (!ok) { fails++; std::printf("  FAIL: %s\n", what); } }

    void run()
    {
        rstn.write(false); sel.write(sc_bv<3>("000")); wait(3); rstn.write(true); wait(2);
        const int bank[3] = {2, 0, 4};          // A, B, C
        const uint32_t row_bytes[3] = {32, 32, 128};
        const char *nm[3] = {"A", "B", "C"};
        for (int k = 0; k < 3; k++)
        {
            std::vector<uint8_t> pat(4 * row_bytes[k]), got(pat.size()), zero(pat.size(), 0);
            for (size_t i = 0; i < pat.size(); i++) pat[i] = uint8_t(17 * i + 3 + 50 * k);
            // S1
            sel.write(sc_bv<3>("000")); wait(2);
            s->write_host_side(bank[k], 0, pat.data(), uint32_t(pat.size()));
            s->read_bank_data(bank[k], 0, got.data(), uint32_t(got.size()));
            check(got == zero, "S1 NPU half untouched");
            s->read_host_side(bank[k], 0, got.data(), uint32_t(got.size()));
            check(got == pat, "S1 host half holds the pattern");
            // S3 (before flipping k): other categories' host halves still empty
            for (int o = 0; o < 3; o++)
                if (o > k)
                {
                    std::vector<uint8_t> g2(4 * row_bytes[o]);
                    s->read_host_side(bank[o], 0, g2.data(), uint32_t(g2.size()));
                    check(g2 == std::vector<uint8_t>(g2.size(), 0), "S3 other category empty");
                }
            // S2: flip only this category's bit -> the written half becomes the NPU half
            sc_bv<3> b("000"); b[k] = true; sel.write(b); wait(2);
            s->read_bank_data(bank[k], 0, got.data(), uint32_t(got.size()));
            check(got == pat, "S2 NPU half after flip = pattern (backdoor)");
            s->read_host_side(bank[k], 0, got.data(), uint32_t(got.size()));
            check(got == zero, "S2 host half after flip = other, empty buffer");
            // core read port, row 2
            bool port_ok = true;
            if (k == 0) { aaddr.write(2); arden.write(true); wait(3); arden.write(false);
                          for (int e = 0; e < 32; e++) port_ok &= uint8_t(adata.read()[e]) == pat[2 * 32 + e]; }
            if (k == 1) { baddr.write(2); brden.write(true); wait(3); brden.write(false);
                          for (int e = 0; e < 32; e++) port_ok &= uint8_t(bdata.read()[e]) == pat[2 * 32 + e]; }
            if (k == 2) { caddr.write(2); crden.write(true); wait(3); crden.write(false);
                          for (int e = 0; e < 32; e++) { int32_t v; std::memcpy(&v, &pat[2 * 128 + 4 * e], 4); port_ok &= cr.read()[e] == v; } }
            check(port_ok, "S2 core read port returns the pattern");
            std::printf("[S] SRAM %s: host-side write/read + select flip + core read port %s\n", nm[k], fails ? "(see FAIL above)" : "ok");
            sel.write(sc_bv<3>("000")); wait(2);
            // clear for the next category's S3 check
            s->write_host_side(bank[k], 0, zero.data(), uint32_t(zero.size()));
        }
        std::printf("[tb_has_sram_side] RESULT: %s\n", fails ? "FAIL" : "PASS");
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
