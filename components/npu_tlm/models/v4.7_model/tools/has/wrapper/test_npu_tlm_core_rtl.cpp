// test_npu_tlm_core_rtl.cpp -- runs a HasNpuTop program through a system TLM wrapper (npu_tlm) the way firmware would,
// with the wrapper's model root set to core_rtl/, and checks every output tensor against the program's golden image.
// It is also a reference for the firmware sequence:
//   1. load dram_init.bin into RAM; declare the program's DRAM window (RICH_WINDOW_BASE / RICH_WINDOW_SIZE)
//   2. for every instruction: write its registers (program offsets in address registers + RAM base), push, wait for
//      STATUS.DONE, clear it; the wrapper's performance counters then hold the instruction's cycles
//   3. host steps of the program ('H' lines, YOLOv8m: two nearest-neighbour upsample2x) are done by the CPU on RAM
//
// Build: together with the wrapper sources (npu_tlm.cpp, bus_router.cpp, memory_tlm.cpp) and the wrapper's test support
// header tlm_probe.h, include path <core_rtl>, <core_rtl>/driver, <core_rtl>/instrumentation; the wrapper must stage a
// DRAM window for extended instructions (RICH_WINDOW_BASE / RICH_WINDOW_SIZE, see npu_tlm_rich_window.patch).
// Run:   test_npu_tlm_core_rtl <insts dir> [count|all]   (insts dir: mmio.txt, dram_init.bin, dram_golden.bin)
//        count = number of instructions from the start of the program (default 2), host steps before the last one
//        included. SAURIA_CORE_RTL_TRACE=1 additionally prints the core_rtl trace line of every instruction.
//        Checked outputs: GEMM_FUSED, ELEM_WISE (ADD, MAX_POOL), FUSED_ATTN, LAYERNORM and host steps. The ViT-B/16 reference
//        cycles were measured with SAURIA_CORE_RTL_C_BCAST=1 (bias broadcast, timing only; results are the same).

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>

#include "has/gvu_rce_params.h"   // parse_ln_block: LAYERNORM output width (int8 or int16)

#include "bus_router.h"
#include "memory_tlm.h"
#include "npu_tlm.h"
#include "npu_tlm_regmap.h"
#include "tlm_probe.h"

namespace
{
    using namespace cdc::components;
    using namespace cdc::components::npu_tlm_reg;

    constexpr std::uint64_t kNpuBase = 0x1020'0000ULL;
    constexpr std::uint64_t kRamBase = 0x8000'0000ULL;
    constexpr std::uint32_t kModelRich = 0x4000'0000u;

    // Instruction registers that carry DRAM addresses (program offsets -> physical addresses for the wrapper).
    bool is_address_register(std::uint32_t a)
    {
        switch (a)
        {
        case 0x40000400: case 0x40000404: case 0x40000408: case 0x4000040C: case 0x40000434:
        case 0x40000444: case 0x40000448: case 0x4000044C:
        case 0x40000490: case 0x40000494: case 0x40000498: case 0x400004E0: case 0x400004E4:
            return true;
        default:
            return false;
        }
    }

    struct Write { std::uint32_t addr, val; };
    struct Step
    {
        char kind = 'I';                 // 'I' instruction, 'H' host upsample2x
        std::vector<Write> writes;       // 'I'
        std::string label;
        std::uint32_t out = 0;           // output tensor (program offset)
        std::uint64_t bytes = 0;         // output bytes compared with the golden image
        std::uint32_t in = 0, c = 0, h = 0, w = 0;   // 'H'
    };

    bool read_file(const std::string &p, std::vector<std::uint8_t> &v)
    {
        std::ifstream f(p, std::ios::binary);
        if (!f) return false;
        v.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return true;
    }

    bool parse_host(const std::string &line, Step &s)
    {
        std::istringstream is(line);
        std::string tok;
        is >> tok >> tok;
        if (tok != "upsample2x") return false;
        s.kind = 'H';
        s.label = line;
        while (is >> tok)
        {
            const size_t e = tok.find('=');
            if (e == std::string::npos) continue;
            const std::string k = tok.substr(0, e);
            const std::uint32_t v = std::uint32_t(std::stoul(tok.substr(e + 1), nullptr, 0));
            if (k == "in") s.in = v; else if (k == "out") s.out = v; else if (k == "c") s.c = v;
            else if (k == "h") s.h = v; else if (k == "w") s.w = v;
        }
        s.bytes = std::uint64_t(s.c) * 4 * s.h * s.w;
        return s.c && s.h && s.w;
    }
} // namespace

int sc_main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <insts dir> [count|all]\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    const bool all = argc > 2 && std::strcmp(argv[2], "all") == 0;
    const int count = all ? -1 : (argc > 2 ? std::atoi(argv[2]) : 2);

    std::vector<std::uint8_t> init, gold;
    if (!read_file(dir + "/dram_init.bin", init) || !read_file(dir + "/dram_golden.bin", gold))
    {
        std::fprintf(stderr, "cannot read %s/dram_{init,golden}.bin\n", dir.c_str());
        return 2;
    }

    // Parse the register stream into steps (one instruction per push to 0x40000310, host steps in between).
    std::vector<Step> steps;
    int n_ins = 0;
    {
        std::ifstream f(dir + "/mmio.txt");
        std::string line, label;
        Step cur;
        // Sticky instruction registers 0x40000400..0x40000468 and one-shot extension registers 0x4000046C..0x400004E7.
        std::uint32_t sticky[27] = {0}, ext[31] = {0};
        while (std::getline(f, line) && (count < 0 || n_ins < count))
        {
            if (line.rfind("# [", 0) == 0) { label = line.substr(2); continue; }
            if (line.empty() || line[0] == '#') continue;
            if (line[0] == 'H')
            {
                Step h;
                if (!parse_host(line, h)) { std::fprintf(stderr, "unsupported host line: %s\n", line.c_str()); return 2; }
                steps.push_back(h);
                continue;
            }
            std::istringstream is(line);
            std::string tag;
            Write w{};
            is >> tag >> std::hex >> w.addr >> w.val;
            if (w.addr >= 0x4000046C && w.addr < 0x400004E8) ext[(w.addr - 0x4000046C) / 4] = w.val;
            else if (w.addr >= 0x40000400 && w.addr <= 0x40000468) sticky[(w.addr - 0x40000400) / 4] = w.val;
            cur.writes.push_back(w);
            if (w.addr == 0x40000310)
            {
                // Output tensor of the instruction, as the delivery testbench computes it.
                const std::uint32_t op = w.val & 0xFF;
                const std::uint32_t pack = sticky[(0x454 - 0x400) / 4];
                const std::uint32_t mode = (pack & 0xFFFF0000u) ? (pack >> 24) & 0xFF : pack;
                std::uint32_t c = 0, h = 0, wd = 0;
                if (op == 0x12) { c = ext[3]; h = ext[4]; wd = ext[5]; }             // X_OUT_C/H/W
                else if (op == 0x15 && mode == 0) { c = 1; h = 1; wd = sticky[(0x450 - 0x400) / 4]; }   // ELEM ADD: LEN
                else if (op == 0x15)                                                 // MAX_POOL
                {
                    const std::uint32_t k = ext[23], pd = ext[24], sd = sticky[(0x424 - 0x400) / 4];
                    c = ext[0];
                    h = (ext[1] + 2 * pd - k) / sd + 1;
                    wd = (ext[2] + 2 * pd - k) / sd + 1;
                }
                else if (op == 0x13)                                                 // FUSED_ATTN: O = [NQ][D]
                {
                    c = 1;
                    h = ext[28] ? ext[28] : sticky[(0x450 - 0x400) / 4];              // X_ROWS, else LEN
                    wd = sticky[(0x458 - 0x400) / 4];                                 // head dimension
                }
                else if (op == 0x14)                                                 // LAYERNORM: Y = [rows][H]
                {
                    c = 1;
                    h = ext[28];                                                     // X_ROWS
                    wd = sticky[(0x450 - 0x400) / 4];                                 // LEN = H
                    has::LnParams lp;
                    if (has::parse_ln_block(init, ext[29], lp) && lp.out_int16) wd *= 2;   // X_PARAM_ADDR block
                }
                else
                {
                    std::fprintf(stderr, "instruction %d: opcode 0x%02x is not checked by this test\n", n_ins, op);
                    return 2;
                }
                cur.label = label;
                cur.out = sticky[(0x408 - 0x400) / 4];
                cur.bytes = std::uint64_t(c) * h * wd;
                steps.push_back(cur);
                cur = Step();
                for (auto &x : ext) x = 0;
                n_ins++;
            }
        }
    }
    if (count >= 0 && n_ins < count)
    {
        std::fprintf(stderr, "only %d instructions in %s/mmio.txt\n", n_ins, dir.c_str());
        return 2;
    }

    const std::uint32_t window = std::uint32_t((init.size() + 4095) & ~std::size_t(4095));
    const std::uint32_t ram_size = std::uint32_t((std::uint64_t(window) + (32u << 20) - 1) & ~std::uint64_t((16u << 20) - 1));

    bus_router bus("bus", 2, 2);
    memory_tlm ram("ram", ram_size);
    npu_tlm npu("npu", sc_core::sc_time(1.25, sc_core::SC_NS));
    cdc::test::tlm_probe probe("probe");
    sc_core::sc_signal<bool> reset_n("reset_n");
    sc_core::sc_signal<bool> irq("irq");
    probe.socket.bind(bus.cpu_port(0));
    npu.master_socket.bind(bus.cpu_port(1));
    bus.add_target(kRamBase, ram.size()).bind(ram.socket);
    bus.add_target(kNpuBase, MMIO_SIZE).bind(npu.target_socket);
    npu.reset_n(reset_n);
    npu.irq_out(irq);
    ram.load(init.data(), init.size(), 0);

    int failures = 0;
    sc_core::sc_spawn([&] {
        auto write_reg = [&](std::uint32_t off, std::uint32_t v) { return probe.write(kNpuBase + WRAPPER_BASE + off, &v, 4); };
        auto read_reg = [&](std::uint32_t off) { std::uint32_t v = 0; probe.read(kNpuBase + WRAPPER_BASE + off, &v, 4); return v; };
        auto read_native = [&](std::uint32_t off) { std::uint32_t v = 0; probe.read(kNpuBase + off, &v, 4); return v; };
        auto write_rich = [&](std::uint32_t model_addr, std::uint32_t v) {
            return probe.write(kNpuBase + RICH_ALIAS_BASE + (model_addr - kModelRich), &v, 4);
        };
        auto compare = [&](std::uint32_t off, std::uint64_t n) {
            std::vector<std::uint8_t> got(n);
            probe.debug(tlm::TLM_READ_COMMAND, kRamBase + off, got.data(), unsigned(n));
            std::uint64_t bad = 0;
            for (std::uint64_t k = 0; k < n; k++)
                if (got[k] != gold[off + k]) bad++;
            return bad;
        };

        reset_n.write(false);
        wait(10, sc_core::SC_NS);
        reset_n.write(true);
        wait(20, sc_core::SC_NS);

        if (write_reg(RICH_WINDOW_BASE, std::uint32_t(kRamBase)) != tlm::TLM_OK_RESPONSE ||
            write_reg(RICH_WINDOW_SIZE, window) != tlm::TLM_OK_RESPONSE)
        {
            std::printf("FAIL: the wrapper has no RICH_WINDOW registers\n");
            failures++;
            sc_core::sc_stop();
            return;
        }
        std::printf("[test] %s: %d instructions, %zu steps, DRAM window 0x%llx + %u B, RAM %u B\n", dir.c_str(), n_ins,
                    steps.size(), (unsigned long long)kRamBase, window, ram_size);

        std::uint64_t total_cycles = 0, total_bad = 0;
        int i = 0;
        for (const Step &s : steps)
        {
            if (s.kind == 'H')
            {
                // Firmware step: nearest-neighbour upsample x2, C-order [c][h][w] int8, on RAM.
                std::vector<std::uint8_t> src(std::size_t(s.c) * s.h * s.w), dst(std::size_t(s.bytes));
                probe.debug(tlm::TLM_READ_COMMAND, kRamBase + s.in, src.data(), unsigned(src.size()));
                for (std::uint32_t c = 0; c < s.c; c++)
                    for (std::uint32_t y = 0; y < 2 * s.h; y++)
                        for (std::uint32_t x = 0; x < 2 * s.w; x++)
                            dst[(std::size_t(c) * 2 * s.h + y) * 2 * s.w + x] = src[(std::size_t(c) * s.h + y / 2) * s.w + x / 2];
                probe.debug(tlm::TLM_WRITE_COMMAND, kRamBase + s.out, dst.data(), unsigned(dst.size()));
                const std::uint64_t bad = compare(s.out, s.bytes);
                total_bad += bad;
                if (bad) failures++;
                std::printf("[test] host %s: %llu bytes, %llu mismatches\n", bad ? "FAIL" : "PASS", (unsigned long long)s.bytes,
                            (unsigned long long)bad);
                continue;
            }
            for (const Write &w : s.writes)
            {
                const std::uint32_t v = is_address_register(w.addr) ? std::uint32_t(kRamBase + w.val) : w.val;
                if (write_rich(w.addr, v) != tlm::TLM_OK_RESPONSE)
                {
                    std::printf("FAIL: instruction %d, write 0x%08x = 0x%x rejected (LAST_ERROR %u)\n", i, w.addr, v,
                                read_reg(LAST_ERROR));
                    failures++;
                    sc_core::sc_stop();
                    return;
                }
            }
            std::uint32_t st = 0;
            for (;;)
            {
                wait(10, sc_core::SC_US);
                st = read_reg(STATUS);
                if (st & (STATUS_DONE | STATUS_ERROR)) break;
            }
            const std::uint64_t cyc = std::uint64_t(read_native(PERF_EXEC_CYCLES)) |
                                      (std::uint64_t(read_native(PERF_EXEC_CYCLES_HI)) << 32);
            write_reg(STATUS, STATUS_DONE | STATUS_ERROR);
            const std::uint64_t bad = compare(s.out, s.bytes);
            const bool ok = !(st & STATUS_ERROR) && bad == 0;
            if (!ok) failures++;
            total_cycles += cyc;
            total_bad += bad;
            std::printf("[test] instruction %d %s: status 0x%x, %llu output bytes, %llu mismatches, PERF_EXEC_CYCLES %llu  # %s\n",
                        i, ok ? "PASS" : "FAIL", st, (unsigned long long)s.bytes, (unsigned long long)bad,
                        (unsigned long long)cyc, s.label.c_str());
            std::fflush(stdout);
            i++;
        }
        std::printf("[test] RESULT: %s (%d instructions, %zu steps, %d failed, %llu mismatches, sum of PERF_EXEC_CYCLES %llu)\n",
                    failures ? "FAIL" : "PASS", n_ins, steps.size(), failures, (unsigned long long)total_bad,
                    (unsigned long long)total_cycles);
        sc_core::sc_stop();
    });

    sc_core::sc_start();
    return failures == 0 ? 0 : 1;
}
