// SPDX-License-Identifier: Apache-2.0
//
// Test bench of the FX1 NPU host tests (test_fx1_npu, and the driver test
// fw/fx1_soc/host/test_npu_driver_host.cpp): the clone in the direct or the
// fabric topology, a sparse DDR stand-in with fault and hold hooks, CPU-like
// initiators, a MAX_POOL fixture with an independent C++ oracle. Header only,
// included by exactly one translation unit of each test executable.
#ifndef FX1_NPU_TESTS_NPU_BENCH_H
#define FX1_NPU_TESTS_NPU_BENCH_H

#define SC_INCLUDE_DYNAMIC_PROCESSES
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/resource.h>

#include <bus/bus_system.h>
#include <fx1/exclusive_monitor.h>
#include <fx1/fx1_memory_map.h>
#include <fx1_npu/fx1_npu.h>
#include <fx1_npu/fx1_npu_regmap.h>

using namespace sc_core;
using namespace fx1::npu::reg;

namespace {

int g_failures = 0;
void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? " ok " : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}
std::string hex(std::uint64_t v)
{
    char b[32];
    std::snprintf(b, sizeof b, "0x%llx", static_cast<unsigned long long>(v));
    return b;
}

constexpr std::uint64_t kNpuBase = FX1_NPU_VP_BASE;
constexpr std::uint64_t kDdrBase = FX1_DDR_BASE;
constexpr std::uint64_t kDdrSize = FX1_DDR_SIZE;

// ---------------------------------------------------------------------------
// DDR stand-in: sparse 4 KiB pages, a latency per transaction, fault hooks.
struct TestMem : sc_module {
    tlm_utils::simple_target_socket<TestMem> socket{"socket"};
    std::uint64_t base, size;
    std::uint64_t bus_offset = 0;  // behind the fabric the target sees offsets: added back here
    std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> pages;
    sc_time latency{20, SC_NS};
    // Fault injection on NPU traffic (any initiator: the tests know who is the master).
    std::uint64_t error_lo = 0, error_hi = 0;   // [lo, hi): reads/writes answered with an error
    std::uint64_t stall_lo = 0, stall_hi = 0;   // reads in [lo, hi) wait for `release`
    sc_event release;
    bool released = false;
    // Write hold: the hold_write-th write transaction from now (1-based) waits
    // for `write_release`; write_held tells the test it is in flight. With
    // fail_held the held write is answered with an error once released.
    unsigned hold_write = 0, writes_seen = 0;
    bool write_held = false, read_held = false, fail_held = false;
    sc_event write_release;
    struct WriteLog {
        sc_time t;
        std::uint64_t addr, len;
    };
    std::vector<WriteLog> writes;
    std::uint64_t max_len = 0, transactions = 0;

    TestMem(sc_module_name n, std::uint64_t b, std::uint64_t s) : sc_module(n), base(b), size(s)
    {
        socket.register_b_transport(this, &TestMem::b_transport);
        socket.register_transport_dbg(this, &TestMem::transport_dbg);
    }
    std::uint8_t* byte(std::uint64_t a)
    {
        auto& p = pages[(a - base) >> 12];
        if (p.empty()) p.assign(4096, 0);
        return &p[(a - base) & 4095];
    }
    void poke(std::uint64_t a, const void* d, std::size_t n)
    {
        const auto* s = static_cast<const std::uint8_t*>(d);
        for (std::size_t i = 0; i < n; ++i) *byte(a + i) = s[i];
    }
    void peek(std::uint64_t a, void* d, std::size_t n)
    {
        auto* o = static_cast<std::uint8_t*>(d);
        for (std::size_t i = 0; i < n; ++i) o[i] = *byte(a + i);
    }
    bool in(std::uint64_t a, std::uint64_t n) const { return a >= base && n <= size && a - base <= size - n; }
    void b_transport(tlm::tlm_generic_payload& tx, sc_time& delay)
    {
        const std::uint64_t a = tx.get_address() + bus_offset, n = tx.get_data_length();
        ++transactions;
        max_len = std::max<std::uint64_t>(max_len, n);
        wait(delay + latency);
        delay = SC_ZERO_TIME;
        if (!in(a, n)) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (error_hi && a < error_hi && a + n > error_lo) {
            tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (tx.is_read() && stall_hi && a < stall_hi && a + n > stall_lo && !released) {
            read_held = true;
            wait(release);
            read_held = false;
        }
        if (tx.is_write() && hold_write && ++writes_seen == hold_write) {
            write_held = true;
            wait(write_release);
            write_held = false;
            hold_write = 0;
            if (fail_held) {
                fail_held = false;
                tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                return;
            }
        }
        if (tx.is_read()) peek(a, tx.get_data_ptr(), n);
        else {
            poke(a, tx.get_data_ptr(), n);
            writes.push_back({sc_time_stamp(), a, n});
        }
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    unsigned transport_dbg(tlm::tlm_generic_payload& tx)
    {
        const std::uint64_t a = tx.get_address() + bus_offset, n = tx.get_data_length();
        if (!in(a, n)) return 0;
        if (tx.is_read()) peek(a, tx.get_data_ptr(), n);
        else poke(a, tx.get_data_ptr(), n);
        return static_cast<unsigned>(n);
    }
    std::uint64_t writes_in(std::uint64_t lo, std::uint64_t hi, sc_time after = SC_ZERO_TIME) const
    {
        std::uint64_t n = 0;
        for (const auto& w : writes)
            if (w.addr < hi && w.addr + w.len > lo && w.t >= after) n += w.len;
        return n;
    }
};

// A peripheral stand-in that records when it was reached (CLINT/UART).
struct Probe : sc_module {
    tlm_utils::simple_target_socket<Probe> socket{"socket"};
    std::vector<sc_time> hits;
    explicit Probe(sc_module_name n) : sc_module(n) { socket.register_b_transport(this, &Probe::b_transport); }
    void b_transport(tlm::tlm_generic_payload& tx, sc_time& delay)
    {
        wait(delay);
        delay = SC_ZERO_TIME;
        hits.push_back(sc_time_stamp());
        if (tx.is_read()) std::memset(tx.get_data_ptr(), 0, tx.get_data_length());
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

// A CPU-like initiator: blocking 32-bit accesses.
struct Cpu : sc_module {
    tlm_utils::simple_initiator_socket<Cpu> socket{"socket"};
    std::uint64_t base;  // added to NPU offsets by npu_* helpers
    Cpu(sc_module_name n, std::uint64_t npu_base) : sc_module(n), base(npu_base) {}
    tlm::tlm_response_status access(bool write, std::uint64_t a, std::uint32_t& v, bool with_be = false,
                                    unsigned len = 4)
    {
        tlm::tlm_generic_payload tx;
        sc_time delay = SC_ZERO_TIME;
        std::uint8_t buf[8] = {};
        std::uint8_t be[8];
        std::memset(be, 0xFF, sizeof be);
        if (write) std::memcpy(buf, &v, 4);
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(a);
        tx.set_data_ptr(buf);
        tx.set_data_length(len);
        tx.set_streaming_width(len);
        tx.set_byte_enable_ptr(with_be ? be : nullptr);
        tx.set_byte_enable_length(with_be ? len : 0);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(tx, delay);
        wait(delay);
        if (!write) std::memcpy(&v, buf, 4);
        return tx.get_response_status();
    }
    tlm::tlm_response_status wr(std::uint32_t off, std::uint32_t v) { return access(true, base + off, v); }
    std::uint32_t rd(std::uint32_t off, tlm::tlm_response_status* st = nullptr)
    {
        std::uint32_t v = 0;
        const auto s = access(false, base + off, v);
        if (st) *st = s;
        return v;
    }
    std::uint32_t bank(std::uint32_t local) { return rd(WRAPPER_BASE + local); }
    tlm::tlm_response_status set_bank(std::uint32_t local, std::uint32_t v) { return wr(WRAPPER_BASE + local, v); }
    tlm::tlm_response_status rich(std::uint32_t model, std::uint32_t v)
    {
        return wr(RICH_ALIAS_BASE + (model - MODEL_RICH_BASE), v);
    }
    std::uint32_t rich_rd(std::uint32_t model) { return rd(RICH_ALIAS_BASE + (model - MODEL_RICH_BASE)); }
};

// ---------------------------------------------------------------------------
// MAX_POOL fixture with an independent oracle (C-order int8 [c][h][w]).
struct Pool {
    std::uint32_t c = 4, h = 8, w = 8, k = 3, p = 1, s = 1;
    std::uint32_t in_off = 0x0000, out_off = 0x1000;
    std::vector<std::int8_t> input, expect;
    std::uint32_t oh() const { return (h + 2 * p - k) / s + 1; }
    std::uint32_t ow() const { return (w + 2 * p - k) / s + 1; }
    std::uint32_t out_bytes() const { return c * oh() * ow(); }
    void make(unsigned seed)
    {
        std::mt19937 rng(seed);
        input.resize(c * h * w);
        for (auto& v : input) v = static_cast<std::int8_t>(static_cast<int>(rng() % 256) - 128);
        expect.assign(out_bytes(), 0);
        for (std::uint32_t ch = 0; ch < c; ++ch)
            for (std::uint32_t y = 0; y < oh(); ++y)
                for (std::uint32_t x = 0; x < ow(); ++x) {
                    int m = -128;
                    for (std::uint32_t dy = 0; dy < k; ++dy)
                        for (std::uint32_t dx = 0; dx < k; ++dx) {
                            const int iy = static_cast<int>(y * s + dy) - static_cast<int>(p);
                            const int ix = static_cast<int>(x * s + dx) - static_cast<int>(p);
                            if (iy < 0 || ix < 0 || iy >= static_cast<int>(h) || ix >= static_cast<int>(w)) continue;
                            m = std::max<int>(m, input[(ch * h + iy) * w + ix]);
                        }
                    expect[(ch * oh() + y) * ow() + x] = static_cast<std::int8_t>(m);
                }
    }
    // The CPU sequence (window already programmed); addresses are physical.
    tlm::tlm_response_status program(Cpu& cpu, std::uint64_t win, bool push = true) const
    {
        cpu.rich(0x4000'0444u, static_cast<std::uint32_t>(win + in_off));
        cpu.rich(0x4000'0408u, static_cast<std::uint32_t>(win + out_off));
        cpu.rich(0x4000'0454u, RICH_ELEM_MAX_POOL);
        cpu.rich(0x4000'0424u, s);
        cpu.rich(0x4000'046Cu, c);
        cpu.rich(0x4000'0470u, h);
        cpu.rich(0x4000'0474u, w);
        cpu.rich(0x4000'04C8u, k);
        cpu.rich(0x4000'04CCu, p);
        return push ? cpu.rich(MODEL_PUSH_A, RICH_OPCODE_ELEM_WISE) : tlm::TLM_OK_RESPONSE;
    }
    void load(TestMem& mem, std::uint64_t win) const
    {
        mem.poke(win + in_off, input.data(), input.size());
        std::vector<std::uint8_t> z(out_bytes(), 0x5A);  // a value the result never has everywhere
        mem.poke(win + out_off, z.data(), z.size());
    }
    bool verify(TestMem& mem, std::uint64_t win, const std::string& what) const
    {
        std::vector<std::int8_t> got(out_bytes());
        mem.peek(win + out_off, got.data(), got.size());
        std::size_t bad = 0;
        for (std::size_t i = 0; i < got.size(); ++i) bad += got[i] != expect[i];
        check(out_bytes() > 0 && bad == 0, what + ": " + std::to_string(out_bytes()) + " output bytes, " +
                                               std::to_string(bad) + " differ from the C++ oracle");
        return bad == 0;
    }
};

// Waits until the rich side is idle (RICH_STATUS.BUSY = 0) or the budget ends.
bool wait_rich_idle(Cpu& cpu, sc_time budget = sc_time(50, SC_MS))
{
    const sc_time end = sc_time_stamp() + budget;
    while (sc_time_stamp() < end) {
        if ((cpu.rich_rd(MODEL_STATUS) & RICH_STATUS_BUSY) == 0) return true;
        wait(200, SC_NS);
    }
    return false;
}
bool wait_bank(Cpu& cpu, std::uint32_t mask, sc_time budget = sc_time(50, SC_MS))
{
    const sc_time end = sc_time_stamp() + budget;
    while (sc_time_stamp() < end) {
        if (cpu.bank(STATUS) & mask) return true;
        wait(200, SC_NS);
    }
    return false;
}
void set_window(Cpu& cpu, std::uint64_t base, std::uint32_t size)
{
    cpu.set_bank(RICH_WINDOW_BASE, static_cast<std::uint32_t>(base));
    cpu.set_bank(RICH_WINDOW_SIZE, size);
}
void enable(Cpu& cpu)
{
    cpu.set_bank(IRQ_ENABLE, IRQ_DONE | IRQ_ERROR);
    cpu.set_bank(CTRL, CTRL_ENABLE | CTRL_IRQ_EN);
}

long peak_rss_kib()
{
    struct rusage u {};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_maxrss;
}
long current_rss_kib()
{
    std::ifstream f("/proc/self/statm");
    long pages = 0, rss = 0;
    f >> pages >> rss;
    return rss * 4;
}

// ---------------------------------------------------------------------------
// Bench: one of the two topologies, with the test body as a thread.
struct Bench : sc_module {
    std::string topology;
    fx1::npu::NpuOptions opt;
    std::unique_ptr<fx1::npu::Fx1Npu> npu;
    std::unique_ptr<TestMem> mem;
    std::unique_ptr<Cpu> cpu0, cpu1;
    std::unique_ptr<bus::BusSystem> fabric;
    std::unique_ptr<fx1::ExclusiveMonitor> monitor;
    std::unique_ptr<fx1::WriteGuard> guard;
    std::unique_ptr<Probe> clint, plic, uart;
    std::unique_ptr<tlm_utils::simple_initiator_socket<Bench>> spare;
    sc_signal<bool> reset_n{"reset_n"};
    sc_signal<bool> irq{"irq"};
    std::function<void(Bench&)> body;
    bool done = false;

    // npu_path: SysBus1Axi as platforms/fx1_soc (make_bus_config), or SysBus0Axi
    // for the contrast case.
    static bus::BusConfig fabric_config(bus::TargetPath npu_path)
    {
        bus::BusConfig c;
        c.initiators = {{"CPU1"}, {"CPU2"}, {"NPU_DMA"}};
        c.targets = {{"MEMCTL_DDR", kDdrBase, kDdrSize, bus::TargetPath::SysBus1Axi, true},
                     {"NPU", FX1_NPU_VP_BASE, FX1_NPU_VP_SIZE, npu_path, true},
                     {"CLINT", FX1_CLINT_BASE, FX1_CLINT_SIZE, bus::TargetPath::Peribus0Apb, true},
                     {"PLIC", FX1_PLIC_BASE, FX1_PLIC_SIZE, bus::TargetPath::Peribus0Apb, true},
                     {"UART", FX1_UART_BASE, FX1_APB_SLOT_SIZE, bus::TargetPath::Peribus0Apb, true}};
        return c;
    }
    bool is_fabric() const { return topology == "fabric" || topology == "fabric_s0"; }

    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name n, const std::string& topo, fx1::npu::NpuOptions o, std::function<void(Bench&)> b)
        : sc_module(n), topology(topo), opt(o), body(std::move(b))
    {
        opt.ram_base = kDdrBase;
        opt.ram_size = kDdrSize;
        npu = std::make_unique<fx1::npu::Fx1Npu>("npu", opt);
        mem = std::make_unique<TestMem>("ddr", kDdrBase, kDdrSize);
        npu->reset_n(reset_n);
        npu->irq_out(irq);
        if (is_fabric()) {
            fabric = std::make_unique<bus::BusSystem>(
                "fabric", fabric_config(topology == "fabric_s0" ? bus::TargetPath::SysBus0Axi : bus::TargetPath::SysBus1Axi));
            monitor = std::make_unique<fx1::ExclusiveMonitor>(2, 64);
            guard = std::make_unique<fx1::WriteGuard>("npu_dma_guard", *monitor, 0x104);
            clint = std::make_unique<Probe>("clint");
            plic = std::make_unique<Probe>("plic");
            uart = std::make_unique<Probe>("uart");
            cpu0 = std::make_unique<Cpu>("cpu0", kNpuBase);
            cpu1 = std::make_unique<Cpu>("cpu1", kNpuBase);
            cpu0->socket.bind(fabric->initiator("CPU1"));
            cpu1->socket.bind(fabric->initiator("CPU2"));
            npu->master_socket.bind(guard->target);
            guard->out.bind(fabric->initiator("NPU_DMA"));
            fabric->target("NPU").bind(npu->target_socket);
            fabric->target("MEMCTL_DDR").bind(mem->socket);
            mem->bus_offset = kDdrBase;
            fabric->target("CLINT").bind(clint->socket);
            fabric->target("PLIC").bind(plic->socket);
            fabric->target("UART").bind(uart->socket);
        } else {
            cpu0 = std::make_unique<Cpu>("cpu0", 0);
            npu->master_socket.bind(mem->socket);
            cpu0->socket.bind(npu->target_socket);
        }
        SC_THREAD(run);
    }
    void run()
    {
        reset_n.write(false);
        wait(20, SC_NS);
        reset_n.write(true);
        wait(50, SC_NS);
        body(*this);
        done = true;
        sc_stop();
    }
};

int run_bench(const std::string& topo, fx1::npu::NpuOptions opt, std::function<void(Bench&)> body,
              sc_time limit = sc_time(10, SC_SEC))
{
    Bench bench("bench", topo, opt, std::move(body));
    sc_start(limit);
    if (!bench.done) check(false, "the test body finished within " + limit.to_string() + " simulated");
    std::printf("[test] npu: %s\n[test] npu: %s\n", bench.npu->banner().c_str(), bench.npu->report().c_str());
    std::printf("[test] RESULT %s (%d failure%s)\n", g_failures ? "FAIL" : "PASS", g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}

constexpr std::uint64_t kLowWin = 0x8090'0000ULL;   // FX1_NPU_ARENA_SMALL
constexpr std::uint64_t kHighWin = 0x8E00'0000ULL;  // FX1_NPU_ARENA_LARGE

} // namespace

#endif
