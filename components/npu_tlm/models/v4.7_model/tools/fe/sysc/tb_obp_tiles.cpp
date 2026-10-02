// Step 6b: the real, unmodified psm/obp_top.h driven with the
// HAS layout of real YOLOv8m conv tiles, compared against the T2 golden.
//
// Input: FE_WORK/step6b/obp_vectors.bin from tools/fe/fe_step6b_export_obp.py.
// Per context (<= 32 output positions) the testbench sends one 32-wide vector per output channel
// (vec_channel_mode = 1, bias disabled because bias is the PSUM preload already inside the accumulator),
// then keeps i_valid low long enough for the OBP to become idle, which resets its vector counter.
// Per tile it programs scale/shift RAM entries 0..nch-1; per conv it programs the activation LUT into all
// 32 LUT lanes. Only active rows of each vector are compared.
//
// Build/run: tools/fe/sysc/build_tb_obp_tiles.sh

#include <systemc.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "sauria_types.h"
#include "psm/obp_top.h"

using namespace sauria;

static constexpr int OBP_W = 32;
static constexpr uint32_t LUT_BASE = 0x00140000;
static constexpr uint32_t SCALE_BASE = LUT_BASE + 0x00040000;
static constexpr uint32_t SHIFT_BASE = LUT_BASE + 0x00050000;
// 3 pipeline stages to flush + idle cycle that resets vec_channel_cnt; argv[2] overrides (negative control)
static int IDLE_CYCLES = 6;
typedef Obp<OBP_W, 0x00140000, 0x00150000, int32_t, int8_t> ObpDut;

struct Reader
{
    std::ifstream f;
    explicit Reader(const std::string &p) : f(p, std::ios::binary) {}
    uint32_t u32() { uint32_t v = 0; f.read(reinterpret_cast<char *>(&v), 4); return v; }
    uint8_t u8() { uint8_t v = 0; f.read(reinterpret_cast<char *>(&v), 1); return v; }
    void bytes(char *d, size_t n) { f.read(d, n); }
    bool ok() const { return static_cast<bool>(f); }
};

SC_MODULE(TbObpTiles)
{
    sc_in<bool> i_clk;

    sc_signal<bool> rstn{"rstn"};
    sc_signal<psum_vector_t<OBP_W, int32_t>> i_data{"i_data"};
    sc_signal<uint32_t> i_addr{"i_addr"};
    sc_signal<sramc_mask_t<OBP_W>> i_wmask{"i_wmask"};
    sc_signal<bool> i_valid{"i_valid"};
    sc_signal<act_vector_t<OBP_W, int8_t>> i_residual{"i_residual"};
    sc_signal<psum_vector_t<OBP_W, int32_t>> o_wdata{"o_wdata"};
    sc_signal<uint32_t> o_addr{"o_addr"};
    sc_signal<bool> o_wren{"o_wren"};
    sc_signal<sramc_mask_t<OBP_W>> o_wmask{"o_wmask"};
    sc_signal<bool> o_valid{"o_valid"};
    sc_signal<bool> bias_en{"bias_en"}, requant_en{"requant_en"}, lut_en{"lut_en"}, residual_en{"residual_en"};
    sc_signal<bool> vec_mode{"vec_mode"};
    sc_signal<uint32_t> requant_scale{"requant_scale"}, requant_shift{"requant_shift"};
    sc_signal<uint32_t> host_addr{"host_addr"};
    sc_signal<bool> host_wren{"host_wren"}, host_rden{"host_rden"};
    sc_signal<host_data_t> host_wdata{"host_wdata"};
    sc_signal<host_mask_t> host_wmask{"host_wmask"};
    sc_signal<host_data_t> host_rdata{"host_rdata"};

    ObpDut *dut{nullptr};
    std::string path;
    std::deque<std::pair<psum_vector_t<OBP_W, int32_t>, sramc_mask_t<OBP_W>>> captured;
    int exit_code{1};

    SC_HAS_PROCESS(TbObpTiles);
    TbObpTiles(sc_module_name n, const std::string &p) : sc_module(n), path(p)
    {
        dut = new ObpDut("dut");
        dut->i_clk(i_clk);
        dut->i_rstn(rstn);
        dut->i_data(i_data);
        dut->i_addr(i_addr);
        dut->i_wmask(i_wmask);
        dut->i_valid(i_valid);
        dut->i_residual(i_residual);
        dut->o_sramc_wdata(o_wdata);
        dut->o_sramc_addr(o_addr);
        dut->o_sramc_wren(o_wren);
        dut->o_sramc_wmask(o_wmask);
        dut->o_valid(o_valid);
        dut->i_bias_en(bias_en);
        dut->i_requant_en(requant_en);
        dut->i_lut_en(lut_en);
        dut->i_residual_en(residual_en);
        dut->i_vec_channel_mode(vec_mode);
        dut->i_requant_scale(requant_scale);
        dut->i_requant_shift(requant_shift);
        dut->i_host_addr(host_addr);
        dut->i_host_wren(host_wren);
        dut->i_host_rden(host_rden);
        dut->i_host_wdata(host_wdata);
        dut->i_host_wmask(host_wmask);
        dut->o_host_rdata(host_rdata);

        SC_THREAD(run);
        sensitive << i_clk.pos();
        SC_METHOD(monitor);
        sensitive << i_clk.pos();
        dont_initialize();
    }
    ~TbObpTiles() { delete dut; }

    void monitor()
    {
        if (o_valid.read())
            captured.emplace_back(o_wdata.read(), o_wmask.read());
    }

    void host_write(uint32_t addr, const host_data_t &d)
    {
        host_mask_t m;
        m.data.fill(true);
        host_addr.write(addr);
        host_wdata.write(d);
        host_wmask.write(m);
        host_wren.write(true);
        wait();
        host_wren.write(false);
        wait();
    }

    void write_u32(uint32_t addr, uint32_t v)
    {
        host_data_t d;
        d[0] = static_cast<double>(v);
        host_write(addr, d);
    }

    void run()
    {
        bias_en.write(false);
        requant_en.write(true);
        lut_en.write(false);
        residual_en.write(false);
        vec_mode.write(true);
        requant_scale.write(1);
        requant_shift.write(0);
        i_valid.write(false);
        host_wren.write(false);
        host_rden.write(false);
        rstn.write(false);
        wait(5);
        rstn.write(true);
        wait(2);

        Reader r(path);
        char magic[4];
        r.bytes(magic, 4);
        if (!r.ok() || std::string(magic, 4) != "OBPV")
        {
            std::cout << "[tb_obp_tiles] cannot read " << path << std::endl;
            sc_stop();
            return;
        }
        uint32_t n_convs = r.u32();
        uint64_t total_cmp = 0, total_bad = 0, total_vec = 0;
        bool framing_ok = true;
        for (uint32_t ci = 0; ci < n_convs && framing_ok; ci++)
        {
            uint32_t nl = r.u32();
            std::string name(nl, '\0');
            r.bytes(&name[0], nl);
            bool silu = r.u8() != 0;
            bool has_skip = r.u8() != 0;
            int8_t lut[256];
            r.bytes(reinterpret_cast<char *>(lut), 256);
            uint32_t n_tiles = r.u32();

            lut_en.write(silu);
            residual_en.write(has_skip);
            if (silu)
                for (uint32_t lane = 0; lane < OBP_W; lane++)
                    for (uint32_t e = 0; e < 256; e += 4)
                    {
                        host_data_t d;
                        for (int k = 0; k < 4; k++)
                            d[k] = static_cast<double>(static_cast<uint8_t>(lut[e + k]));
                        host_write(LUT_BASE + lane * 256 + e, d);
                    }

            uint64_t cmp = 0, bad = 0, vecs = 0;
            for (uint32_t ti = 0; ti < n_tiles && framing_ok; ti++)
            {
                uint32_t nch = r.u32();
                std::vector<uint32_t> scale(nch), shift(nch);
                r.bytes(reinterpret_cast<char *>(scale.data()), 4 * nch);
                r.bytes(reinterpret_cast<char *>(shift.data()), 4 * nch);
                for (uint32_t x = 0; x < nch; x++)
                {
                    uint32_t src = std::getenv("OBP_NEG_REVERSE") ? nch - 1 - x : x;  // negative control
                    write_u32(SCALE_BASE + 4 * x, scale[src]);
                    write_u32(SHIFT_BASE + 4 * x, shift[src]);
                }
                uint32_t n_ctx = r.u32();
                for (uint32_t cx = 0; cx < n_ctx; cx++)
                {
                    uint32_t nrows = r.u32();
                    std::vector<std::vector<int8_t>> expected(nch, std::vector<int8_t>(nrows));
                    captured.clear();
                    for (uint32_t x = 0; x < nch; x++)
                    {
                        psum_vector_t<OBP_W, int32_t> v(0);
                        act_vector_t<OBP_W, int8_t> res(0);
                        sramc_mask_t<OBP_W> mask(false);
                        std::vector<int32_t> acc(nrows);
                        std::vector<int8_t> sk(nrows);
                        r.bytes(reinterpret_cast<char *>(acc.data()), 4 * nrows);
                        r.bytes(reinterpret_cast<char *>(sk.data()), nrows);
                        r.bytes(reinterpret_cast<char *>(expected[x].data()), nrows);
                        for (uint32_t k = 0; k < nrows; k++)
                        {
                            v[k] = acc[k];
                            res[k] = sk[k];
                            mask[k] = true;
                        }
                        i_data.write(v);
                        i_residual.write(res);
                        i_wmask.write(mask);
                        i_addr.write(ti);
                        i_valid.write(true);
                        wait();
                    }
                    i_valid.write(false);
                    wait(IDLE_CYCLES);
                    if (!r.ok() || captured.size() != nch)
                    {
                        std::cout << "[tb_obp_tiles] " << name << " tile " << ti << " ctx " << cx
                                  << ": captured " << captured.size() << " vectors, expected " << nch << std::endl;
                        framing_ok = false;
                        break;
                    }
                    for (uint32_t x = 0; x < nch; x++)
                    {
                        const auto &out = captured[x].first;
                        for (uint32_t k = 0; k < nrows; k++)
                        {
                            cmp++;
                            if (out[k] != static_cast<int32_t>(expected[x][k]))
                            {
                                if (bad < 5)
                                    std::cout << "  MISMATCH " << name << " tile " << ti << " ctx " << cx << " ch " << x
                                              << " row " << k << ": got " << out[k] << " want " << int(expected[x][k]) << std::endl;
                                bad++;
                            }
                        }
                    }
                    vecs += nch;
                }
            }
            std::printf("[tb_obp_tiles] %-18s tiles %u vectors %llu elements %llu mismatches %llu silu %d skip %d\n",
                        name.c_str(), n_tiles, (unsigned long long)vecs, (unsigned long long)cmp,
                        (unsigned long long)bad, int(silu), int(has_skip));
            total_cmp += cmp;
            total_bad += bad;
            total_vec += vecs;
        }
        bool pass = framing_ok && total_bad == 0 && total_cmp > 0;
        std::printf("[tb_obp_tiles] RESULT: %s (convs %u, vectors %llu, elements %llu, mismatches %llu, sim time %s)\n",
                    pass ? "PASS" : "FAIL", n_convs, (unsigned long long)total_vec, (unsigned long long)total_cmp,
                    (unsigned long long)total_bad, sc_time_stamp().to_string().c_str());
        exit_code = pass ? 0 : 1;
        sc_stop();
    }
};

int sc_main(int argc, char *argv[])
{
    sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
    std::string path = argc > 1 ? argv[1] : "fe_work/step6b/obp_vectors.bin";
    if (argc > 2)
        IDLE_CYCLES = std::stoi(argv[2]);
    sc_clock clk("clk", 10, SC_NS);
    TbObpTiles tb("tb", path);
    tb.i_clk(clk);
    sc_start();
    return tb.exit_code;
}
