// tb_core_rtl_neo.cpp -- the neo window through core_rtl/npu_top.h, driven the way the CDC wrapper (npu_tlm) drives it.
//
//   HAS_HOST=neo tb_core_rtl_neo <program dir> <writes file> [--dram-out <file>] [--dram-base <hex>]
//
// sauria::NpuTop of core_rtl is built as the wrapper builds it; the host bus is driven like npu_tlm::core_host_write (address, data[0] = double(value), mask[0],
// one write per three clocks). The writes file carries model addresses (0x4000xxxx): the v4.5 stream, or the mixed stream of host_trace_replay
// --emit-neo-mmio --neo-dram-add 80000000 (words that hold DRAM addresses are system addresses; route() subtracts --dram-base, default 0x80000000, as the
// wrapper's RAM base). Completion is the one the wrapper uses: DecoderView queue size and state. Output as tb_has_neo_host.
#include <systemc.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "npu_top.h"

using namespace has;
namespace M = has::mmio;

SC_MODULE(TbCoreRtlNeo)
{
    sc_in<bool> i_clk;
    sc_signal<bool> rstn{"rstn"}, soft_rst{"soft_rst"}, start{"start"}, wren{"wren"}, rden{"rden"}, done{"done"}, deadlock{"deadlock"};
    sc_signal<uint32_t> mvm_k{"mvm_k"}, addr{"addr"}, total_ctx{"total_ctx"};
    sc_signal<sauria::host_data_t> wdata{"wdata"}, rdata{"rdata"};
    sc_signal<sauria::host_mask_t> wmask{"wmask"};
    sc_signal<float> threshold{"threshold"};
    sc_signal<sc_bv<3>> select{"select"};
    sauria::NpuTop<> *dut{nullptr};
    std::vector<uint8_t> dram;
    size_t program_bytes{0};
    struct Ev { bool host; uint32_t a, v; std::string text; };   // a register write, or a step of the CPU ("H upsample2x ...")
    std::vector<Ev> writes;
    std::string dram_out;
    uint32_t dram_base{0x80000000u};
    int exit_code{1};

    SC_HAS_PROCESS(TbCoreRtlNeo);
    TbCoreRtlNeo(sc_module_name n) : sc_module(n)
    {
        sauria::PeConfig cfg;   // as npu_tlm.cpp exact_int8_config()
        cfg.arithmetic_type = 0; cfg.mul_type = 0; cfg.add_type = 0; cfg.stages_mul = 1;
        cfg.intermediate_pipeline_stage = true; cfg.zero_gating_mult = false;
        dut = new sauria::NpuTop<>("npu", cfg);
        dut->i_clk(i_clk); dut->i_rstn(rstn); dut->i_soft_reset(soft_rst); dut->i_start(start);
        dut->o_done(done); dut->o_deadlock(deadlock); dut->i_mvm_k(mvm_k);
        dut->i_host_addr(addr); dut->i_host_wren(wren); dut->i_host_rden(rden);
        dut->i_host_wdata(wdata); dut->i_host_wmask(wmask); dut->o_host_rdata(rdata);
        dut->i_threshold(threshold); dut->i_select(select); dut->i_total_contexts(total_ctx);
        SC_THREAD(run);
        sensitive << i_clk.pos();
    }
    ~TbCoreRtlNeo() { delete dut; }

    void core_host_write(uint32_t a, uint32_t v)   // npu_tlm.cpp core_host_write: set, one clock with wren, then low
    {
        sauria::host_data_t d; sauria::host_mask_t m;
        d.data.fill(0.0); m.data.fill(false);
        d[0] = double(v); m[0] = true;
        wait();
        addr.write(a); wdata.write(d); wmask.write(m); rden.write(false); wren.write(true);
        wait();
        wren.write(false);
        wait();
    }
    uint32_t core_host_read(uint32_t a)
    {
        wait();
        addr.write(a); wren.write(false); rden.write(true);
        wait(2);
        rden.write(false);
        wait();
        return uint32_t(rdata.read()[0]);
    }
    bool busy() { return dut->decoder_inst->get_queue_a_size() != 0 || dut->decoder_inst->get_state_a() != 0; }

    void run()
    {
        dut->set_dram(&dram);
        dut->set_dram_base(dram_base);
        rstn.write(false); soft_rst.write(false); start.write(false); wren.write(false); rden.write(false);
        mvm_k.write(1); total_ctx.write(1); threshold.write(0.5f); select.write(sc_bv<3>("000"));
        wait(5);
        rstn.write(true);
        wait(3);
        int max_queue = 0;
        for (const auto &w : writes)
        {
            if (w.host)   // a step of the CPU: everything pushed so far has finished (the wrapper's view: queue and state idle)
            {
                for (int g = 0; busy() && g < 4000000; g++) wait(16);
                unsigned in = 0, out = 0, c = 0, h = 0, wd = 0;
                if (std::sscanf(w.text.c_str(), "H upsample2x in=%x out=%x c=%u h=%u w=%u", &in, &out, &c, &h, &wd) == 5)
                    for (uint32_t ch = 0; ch < c; ch++)
                        for (uint32_t y = 0; y < 2 * h; y++)
                            for (uint32_t x = 0; x < 2 * wd; x++)
                                dram[out + (ch * 2 * h + y) * 2 * wd + x] = dram[in + (ch * h + y / 2) * wd + x / 2];
                continue;
            }
            core_host_write(w.a, w.v);
            max_queue = std::max(max_queue, int(dut->decoder_inst->get_queue_a_size()));
            if (w.a == M::PUSH_A) for (int g = 0; (core_host_read(M::STATUS) & 2u) != 0 && g < 100000; g++) wait(256);
        }
        int guard = 0;
        while (busy() && guard++ < 4000000) wait(16);
        const uint32_t status = core_host_read(M::STATUS), ctrl = dut->rtl->neo.on() ? core_host_read(dut->rtl->neo.BASE) : 0;
        std::printf("[neo_host] retired %u instructions, STATUS 0x%08x, st.instr %llu, st.cycles %llu, st.core_busy %llu, neo accepted %llu / refused %llu\n",
                    core_host_read(M::RETIRED), status, (unsigned long long)dut->rtl->st.instr, (unsigned long long)dut->rtl->st.cycles,
                    (unsigned long long)dut->rtl->st.core_busy, (unsigned long long)dut->rtl->neo.accepted(), (unsigned long long)dut->rtl->neo.refused());
        std::printf("[neo_host] wrapper view: queue peaked at %d, idle at the end %d, neo CONTROL 0x%08x (bit 1 = DONE)\n", max_queue, int(!busy()), ctrl);
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
    if (argc < 3) { std::fprintf(stderr, "usage: %s <program dir> <writes file> [--dram-out f] [--dram-base hex]\n", argv[0]); return 2; }
    sc_clock clk("clk", 10, SC_NS);
    TbCoreRtlNeo tb("tb");
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
            std::istringstream is(line);
            std::string tag; uint32_t a, v;
            if (line.compare(0, 2, "H ") == 0) { tb.writes.push_back({true, 0, 0, line}); continue; }
            if ((is >> tag) && tag == "W" && (is >> std::hex >> a >> v)) tb.writes.push_back({false, a, v, std::string()});
        }
    }
    for (int i = 3; i < argc; i++)
    {
        if (!std::strcmp(argv[i], "--dram-out") && i + 1 < argc) tb.dram_out = argv[++i];
        else if (!std::strcmp(argv[i], "--dram-base") && i + 1 < argc) tb.dram_base = uint32_t(std::strtoul(argv[++i], nullptr, 16));
    }
    sc_start();
    return tb.exit_code;
}
