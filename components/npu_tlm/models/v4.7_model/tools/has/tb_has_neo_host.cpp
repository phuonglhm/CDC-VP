// tb_has_neo_host.cpp -- runs a program of host register writes ("W <addr> <value>" lines) on a bare HasNpuTop and reports what it left behind.
//
//   [HAS_HOST=neo|neo_patched] tb_has_neo_host <program dir> <writes file> [--dram-out <file>] [--gvu-off]
//
// The program dir gives dram_init.bin (the DRAM image); the writes file is the v4.5 stream (mmio.txt of the dir) or the mixed stream made by
// host_trace_replay --emit-neo-mmio (instructions the hardware runs as given go through the neo window, NEO_REGISTER_FILE.md, section 2.2). The two runs of
// one program must leave the same DRAM image and the same instruction cycles. Output (one line each, parsed by the gates):
//   [neo_host] retired N instructions, STATUS 0x........ , st.instr, st.cycles, st.core_busy, neo accepted / refused
//   [neo_host] dram <bytes> <fnv1a-64 of the first program bytes>
// "H upsample2x in=.. out=.. c=.. h=.. w=.." lines are steps the CPU does between instructions: the bench waits for the DUT to be idle and does them on the DRAM image.
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

SC_MODULE(TbNeoHost)
{
    sc_in<bool> i_clk;
    sc_signal<bool> rstn{"rstn"}, wren{"wren"}, rden{"rden"}, irq{"irq"}, done{"done"}, deadlock{"deadlock"};
    sc_signal<uint32_t> addr{"addr"};
    sc_signal<sauria::host_data_t> wdata{"wdata"}, rdata{"rdata"};
    sc_signal<sauria::host_mask_t> wmask{"wmask"};
    HasNpuTop *dut{nullptr};
    std::vector<uint8_t> dram;
    size_t program_bytes{0};
    struct Ev { bool host; uint32_t a, v; std::string text; };   // a register write, or a step of the CPU ("H ...")
    std::vector<Ev> writes;
    std::string dram_out;
    int exit_code{1};

    SC_HAS_PROCESS(TbNeoHost);
    TbNeoHost(sc_module_name n) : sc_module(n)
    {
        const Profile prof = Profile::Recommended;   // as core_rtl/npu_top.h (SAURIA_CORE_RTL_PROFILE default)
        dut = new HasNpuTop("has_npu_top", profile_knobs(prof, Knobs::from_env(true)));
        dut->apply_profile(prof);
        dut->i_clk(i_clk); dut->i_rstn(rstn);
        dut->i_host_addr(addr); dut->i_host_wren(wren); dut->i_host_rden(rden);
        dut->i_host_wdata(wdata); dut->i_host_wmask(wmask); dut->o_host_rdata(rdata);
        dut->o_irq(irq); dut->o_done(done); dut->o_deadlock(deadlock);
        dut->set_clock_period(sc_time(10, SC_NS));
        SC_THREAD(run);
        sensitive << i_clk.pos();
    }
    ~TbNeoHost() { delete dut; }

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

    void run()
    {
        dut->set_dram(&dram);
        rstn.write(false); wren.write(false); rden.write(false);
        wait(5);
        rstn.write(true);
        wait(2);
        for (const auto &w : writes)
        {
            if (w.host)   // a step of the CPU: everything pushed so far has finished (RAW on its input)
            {
                for (int guard = 0; (mmio_read(M::STATUS) & 1u) != 0 && guard < 1000000; guard++) wait(64);
                unsigned in = 0, out = 0, c = 0, h = 0, wd = 0;
                if (std::sscanf(w.text.c_str(), "H upsample2x in=%x out=%x c=%u h=%u w=%u", &in, &out, &c, &h, &wd) == 5)
                    for (uint32_t ch = 0; ch < c; ch++)
                        for (uint32_t y = 0; y < 2 * h; y++)
                            for (uint32_t x = 0; x < 2 * wd; x++)
                                dram[out + (ch * 2 * h + y) * 2 * wd + x] = dram[in + (ch * h + y / 2) * wd + x / 2];
                else std::printf("[neo_host] unknown host step: %s\n", w.text.c_str());
                continue;
            }
            if (w.a == M::PUSH_A)   // CPU flow control: wait while the instruction queue is full
                for (int guard = 0; (mmio_read(M::STATUS) & 2u) != 0 && guard < 100000; guard++) wait(256);
            mmio_write(w.a, w.v);
        }
        for (int guard = 0; (mmio_read(M::STATUS) & 1u) != 0 && guard < 1000000; guard++) wait(64);
        const uint32_t status = mmio_read(M::STATUS);
        std::printf("[neo_host] retired %u instructions, STATUS 0x%08x, st.instr %llu, st.cycles %llu, st.core_busy %llu, neo accepted %llu / refused %llu\n",
                    mmio_read(M::RETIRED), status, (unsigned long long)dut->st.instr, (unsigned long long)dut->st.cycles, (unsigned long long)dut->st.core_busy,
                    (unsigned long long)dut->neo.accepted(), (unsigned long long)dut->neo.refused());
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < program_bytes && i < dram.size(); i++) { h ^= dram[i]; h *= 1099511628211ull; }
        std::printf("[neo_host] dram %zu %016llx\n", program_bytes, (unsigned long long)h);
        if (!dram_out.empty())
            if (std::FILE *fp = std::fopen(dram_out.c_str(), "wb")) { std::fwrite(dram.data(), 1, program_bytes, fp); std::fclose(fp); }
        exit_code = 0;
        sc_stop();
    }
};

int sc_main(int argc, char *argv[])
{
    sc_report_handler::set_actions("/IEEE_Std_1666/deprecated", SC_DO_NOTHING);
    if (argc < 3) { std::fprintf(stderr, "usage: %s <program dir> <writes file> [--dram-out f]\n", argv[0]); return 2; }
    sc_clock clk("clk", 10, SC_NS);
    TbNeoHost tb("tb");
    tb.i_clk(clk);
    {
        std::ifstream f(std::string(argv[1]) + "/dram_init.bin", std::ios::binary);
        if (!f) { std::fprintf(stderr, "cannot read %s/dram_init.bin\n", argv[1]); return 2; }
        tb.dram.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        tb.program_bytes = tb.dram.size();
    }
    {
        std::ifstream f(argv[2]);
        if (!f) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
        std::string line;
        while (std::getline(f, line))
        {
            if (line.compare(0, 2, "H ") == 0) { tb.writes.push_back({true, 0, 0, line}); continue; }
            std::istringstream is(line);
            std::string tag; uint32_t a, v;
            if ((is >> tag) && tag == "W" && (is >> std::hex >> a >> v)) tb.writes.push_back({false, a, v, std::string()});
        }
    }
    for (int i = 3; i < argc; i++) if (!std::strcmp(argv[i], "--dram-out") && i + 1 < argc) tb.dram_out = argv[++i];
    sc_start();
    return tb.exit_code;
}
