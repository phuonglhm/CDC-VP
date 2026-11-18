// SPDX-License-Identifier: Apache-2.0
//
// FX1 NPU (fx1_npu) tests. One case per process (SystemC elaborates once):
//
//   test_fx1_npu <case> [args]
//
// Topologies:
//   direct  CPU initiators -> NPU target; NPU master -> TestMem (sparse DDR stand-in)
//   fabric  the real FX1 BusSystem: CPU1/CPU2 initiators, NPU a leaf of SYSBUS_1
//           at 0x13000000 (as platforms/fx1_soc places it), DDR stand-in on
//           SYSBUS_1 at 0x80000000 (512 MiB), CLINT/PLIC/UART stand-ins on
//           PERIBUS_0 (behind SYSBUS_0), NPU_DMA through fx1::WriteGuard
//   fabric_s0  the same with the NPU on SYSBUS_0 (the N1 placement before the
//           review): only the contrast case uses it
//
// Unit cases (independent C++ oracle for MAX_POOL; no model data):
//   regs, dbg, gemm, maxpool, window, unsupported, refuse_model, queue,
//   abort, abort_running, native_abort, reset, reset_inflight, dma_error, memory
// Fabric cases:
//   fabric_maxpool, fabric_gemm, fabric_timeout_abort, fabric_s0_timeout_contrast
// Gate replay (program from a drop generator, golden from ref_program.py):
//   gate <dir> <golden.bin> <window base> <direct|fabric>
//   gate_neo <window program> <dir> <golden.bin> <window base> <direct|fabric>
//   gate_neo_reset <window program> <dir> <golden.bin> <window base> <direct|fabric>
//     (first a platform reset while the first doorbell's last copy-back
//     transaction is in flight: no RETIRED, no neo DONE in the new epoch)
//     the program of <dir> as host_trace_replay --emit-neo-mmio --neo-dram-add <base>
//     writes it: the instructions the hardware runs through the neo_lite register
//     file (82 words, IEN, START) and the doorbell (push of 0x17); outputs checked
//     per instruction of <dir> against the oracle, and the whole window
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
    // for `write_release`; write_held tells the test it is in flight.
    unsigned hold_write = 0, writes_seen = 0;
    bool write_held = false, read_held = false;
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

// ============================================================== cases ======
void case_regs(Bench& b)
{
    Cpu& c = *b.cpu0;
    check(c.bank(VP_ID) == VP_ID_VALUE, "VP_ID reads FXNP");
    check(c.bank(VP_ABI) == VP_ABI_VALUE, "VP_ABI reads 1.0");
    check(c.bank(VP_MODEL_HASH) == FX1_NPU_TEST_MODEL_HASH, "VP_MODEL_HASH = manifest SHA-256 prefix " + hex(c.bank(VP_MODEL_HASH)));
    const std::uint32_t f = c.bank(VP_FEATURE);
    check((f & (VP_FEATURE_NATIVE_GEMM | VP_FEATURE_RICH_HAS | VP_FEATURE_ABORT | VP_FEATURE_BOUNDED_PUSH | VP_FEATURE_QUEUE)) ==
                  (VP_FEATURE_NATIVE_GEMM | VP_FEATURE_RICH_HAS | VP_FEATURE_ABORT | VP_FEATURE_BOUNDED_PUSH | VP_FEATURE_QUEUE) &&
              (f & (VP_FEATURE_DMA_TENSOR | VP_FEATURE_ATTN_LN)) == 0,
          "VP_FEATURE " + hex(f));
    check(c.bank(VP_QUEUE_DEPTH) == 16 && c.bank(VP_QUEUE_FREE) == 16, "queue depth 16, all free");
    check(c.bank(VP_STATUS) == VP_STATUS_QUIESCENT, "VP_STATUS quiescent at reset");
    check(c.bank(VP_ERROR) == 0 && c.bank(VP_ACCEPTED) == 0, "no error, nothing accepted");
    check(c.bank(VP_PUSH_WAIT_NS) == 100000, "push wait 100 us at reset");
    check(c.bank(VP_WINDOW_MAX) == (32u << 20), "window max 32 MiB");
    check(c.bank(CORE_ID) == CORE_ID_VALUE, "CORE_ID unchanged (SA42)");
    check(c.bank(STATUS) == STATUS_IDLE, "STATUS idle at reset");
    check(c.set_bank(VP_PUSH_WAIT_NS, 1234) == tlm::TLM_OK_RESPONSE && c.bank(VP_PUSH_WAIT_NS) == 1234, "VP_PUSH_WAIT_NS RW");
    check(c.set_bank(VP_ID, 1) == tlm::TLM_COMMAND_ERROR_RESPONSE && c.bank(VP_ID) == VP_ID_VALUE, "VP_ID RO");
    check(c.set_bank(VP_QUEUE_FREE, 1) == tlm::TLM_COMMAND_ERROR_RESPONSE, "VP_QUEUE_FREE RO");
    check(c.bank(VP_CMD) == 0, "VP_CMD reads 0");
    // Access classes: 32-bit aligned only, no byte enables, unmapped offsets refused.
    std::uint32_t v = 0;
    check(c.access(false, WRAPPER_BASE + VP_ID + 2, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "misaligned access refused");
    check(c.access(false, WRAPPER_BASE + VP_ID, v, false, 2) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "16-bit access refused");
    check(c.access(false, WRAPPER_BASE + VP_ID, v, false, 8) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "64-bit access refused");
    check(c.access(false, WRAPPER_BASE + VP_ID, v, true) == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE, "byte enables refused");
    check(c.access(false, MMIO_SIZE, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "offset beyond 1 MiB refused");
    check(c.access(false, WRAPPER_BASE + 0x1F00, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "unmapped bank offset refused");
    check(c.access(false, RICH_ALIAS_BASE + 0x200, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "unmapped rich offset refused");
    tlm::tlm_response_status st;
    c.rd(NATIVE_CFG_PROFILE, &st);
    check(st == tlm::TLM_OK_RESPONSE, "native profile register readable through the native engine");
    check(c.wr(NATIVE_CFG_PROFILE, PROFILE_V4_LINEAR) == tlm::TLM_OK_RESPONSE && c.rd(NATIVE_CFG_PROFILE) == PROFILE_V4_LINEAR,
          "native profile register RW");
    // SRAM window word through the native host port.
    check(c.wr(NATIVE_SRAMA_OFFSET, 0x04030201u) == tlm::TLM_OK_RESPONSE && c.rd(NATIVE_SRAMA_OFFSET) == 0x04030201u,
          "native SRAM A word RW");
    check(c.rich_rd(MODEL_STATUS) == 0 && c.rich_rd(MODEL_RETIRED) == 0, "RICH_STATUS / RETIRED at reset");
    check(c.rd(PERF_TOTAL_CYCLES) == c.bank(PERF_TOTAL_CYCLES), "counters at native and bank offsets");
}

// transport_dbg from an SC_METHOD: a wait() there would abort the simulation.
struct DbgProbe : sc_module {
    tlm_utils::simple_initiator_socket<DbgProbe> socket{"socket"};
    sc_event go;
    std::vector<std::pair<std::uint32_t, unsigned>> results;
    SC_HAS_PROCESS(DbgProbe);
    explicit DbgProbe(sc_module_name n) : sc_module(n)
    {
        SC_METHOD(sample);
        sensitive << go;
        dont_initialize();
    }
    void sample()
    {
        for (std::uint32_t a : {WRAPPER_BASE + VP_ID, WRAPPER_BASE + STATUS, RICH_ALIAS_BASE + 0x318u, NATIVE_SRAMA_OFFSET,
                                NATIVE_CFG_PROFILE}) {
            std::uint32_t v = 0;
            tlm::tlm_generic_payload tx;
            tx.set_command(tlm::TLM_READ_COMMAND);
            tx.set_address(a);
            tx.set_data_ptr(reinterpret_cast<unsigned char*>(&v));
            tx.set_data_length(4);
            tx.set_streaming_width(4);
            const unsigned n = socket->transport_dbg(tx);
            results.push_back({v, n});
        }
        std::uint32_t v = 1;
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        tx.set_address(WRAPPER_BASE + VP_CMD);
        tx.set_data_ptr(reinterpret_cast<unsigned char*>(&v));
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        results.push_back({0, socket->transport_dbg(tx)});
    }
};

int case_dbg()
{
    fx1::npu::NpuOptions o;
    o.ram_base = kDdrBase;
    o.ram_size = kDdrSize;
    fx1::npu::Fx1Npu npu("npu", o);
    TestMem mem("ddr", kDdrBase, kDdrSize);
    DbgProbe probe("probe");
    sc_signal<bool> rst{"rst"}, irq{"irq"};
    npu.reset_n(rst);
    npu.irq_out(irq);
    npu.master_socket.bind(mem.socket);
    probe.socket.bind(npu.target_socket);
    rst.write(true);
    sc_start(100, SC_NS);
    probe.go.notify(SC_ZERO_TIME);
    sc_start(10, SC_NS);
    check(probe.results.size() == 6, "every debug access returned (none waited)");
    if (probe.results.size() == 6) {
        check(probe.results[0].second == 4 && probe.results[0].first == VP_ID_VALUE, "debug read of VP_ID");
        check(probe.results[1].second == 4, "debug read of STATUS");
        check(probe.results[2].second == 4, "debug read of RICH_STATUS");
        check(probe.results[3].second == 0 && probe.results[4].second == 0, "native region not debug-readable (0 bytes, no wait)");
        check(probe.results[5].second == 0 && npu.stats().aborts == 0, "debug write refused, no side effect (no ABORT)");
    }
    std::printf("[test] RESULT %s (%d failures)\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures ? 1 : 0;
}

// GEMM bank job (C = A x B, 32x32, K) with a host oracle.
bool gemm_job(Bench& b, std::uint64_t a_addr, std::uint64_t w_addr, std::uint64_t c_addr, std::uint32_t K, unsigned seed,
              const std::string& what, bool quiet = false)
{
    Cpu& c = *b.cpu0;
    std::mt19937 rng(seed);
    std::vector<std::int8_t> A(32 * K), W(K * 32);
    for (auto& v : A) v = static_cast<std::int8_t>(static_cast<int>(rng() % 256) - 128);
    for (auto& v : W) v = static_cast<std::int8_t>(static_cast<int>(rng() % 256) - 128);
    b.mem->poke(a_addr, A.data(), A.size());
    b.mem->poke(w_addr, W.data(), W.size());
    c.set_bank(SRC_ADDR, static_cast<std::uint32_t>(a_addr));
    c.set_bank(SRC_SIZE_BYTES, 32 * K);
    c.set_bank(WEIGHTS_ADDR, static_cast<std::uint32_t>(w_addr));
    c.set_bank(WEIGHTS_SIZE_BYTES, 32 * K);
    c.set_bank(DST_ADDR, static_cast<std::uint32_t>(c_addr));
    c.set_bank(DST_SIZE_BYTES, 32 * 32 * 4);
    c.set_bank(WIDTH, 32);
    c.set_bank(HEIGHT, 32);
    c.set_bank(K_DIMENSION, K);
    c.set_bank(FORMAT, FORMAT_INT8_INT8_INT32);
    c.set_bank(OP_MODE, OP_GEMM);
    c.set_bank(STATUS, STATUS_DONE | STATUS_ERROR);
    enable(c);
    c.set_bank(CTRL, CTRL_ENABLE | CTRL_IRQ_EN | CTRL_START);
    const bool finished = wait_bank(c, STATUS_DONE | STATUS_ERROR);
    const std::uint32_t s = c.bank(STATUS);
    std::vector<std::int32_t> got(32 * 32);
    b.mem->peek(c_addr, got.data(), got.size() * 4);
    std::size_t bad = 0;
    for (unsigned r = 0; r < 32; ++r)
        for (unsigned col = 0; col < 32; ++col) {
            std::int64_t acc = 0;
            for (unsigned k = 0; k < K; ++k) acc += A[r * K + k] * W[k * 32 + col];
            bad += got[r * 32 + col] != static_cast<std::int32_t>(acc);
        }
    const bool ok = finished && (s & STATUS_DONE) && !(s & STATUS_ERROR) && bad == 0 && b.irq.read();
    if (!quiet || !ok)
        check(ok, what + ": STATUS " + hex(s) + ", " + std::to_string(bad) + " of 1024 INT32 differ, IRQ " +
                      std::to_string(b.irq.read()));
    c.set_bank(STATUS, STATUS_DONE);
    c.set_bank(IRQ_STATUS, IRQ_DONE);
    return bad == 0;
}

void case_gemm(Bench& b)
{
    gemm_job(b, kDdrBase + 0x0090'0000, kDdrBase + 0x0090'4000, kDdrBase + 0x0090'8000, 64, 1, "GEMM K=64, low DDR");
    gemm_job(b, kDdrBase + kDdrSize - 0x3000, kDdrBase + kDdrSize - 0x2000, kDdrBase + kDdrSize - 0x1000, 96, 2,
             "GEMM K=96, buffers ending at the last DDR byte");
    check(!b.irq.read(), "IRQ low after W1C");
    // Invalid ranges: end beyond DDR, base below DDR, 32-bit overflow.
    Cpu& c = *b.cpu0;
    for (const auto& t : std::vector<std::pair<std::uint64_t, std::string>>{
             {kDdrBase + kDdrSize - 0x100, "source crossing the end of DDR"},
             {kDdrBase - 0x1000, "source below DDR"},
             {0xFFFF'FF00ULL, "source wrapping the 32-bit space"}}) {
        c.set_bank(SRC_ADDR, static_cast<std::uint32_t>(t.first));
        c.set_bank(STATUS, STATUS_DONE | STATUS_ERROR);
        c.set_bank(CTRL, CTRL_ENABLE | CTRL_IRQ_EN | CTRL_START);
        wait_bank(c, STATUS_DONE | STATUS_ERROR);
        check((c.bank(STATUS) & STATUS_ERROR) && c.bank(LAST_ERROR) == static_cast<std::uint32_t>(error_code::invalid_address),
              t.second + ": INVALID_ADDRESS");
    }
    c.set_bank(STATUS, STATUS_ERROR);
    check(c.bank(LAST_ERROR) == 0, "W1C clears LAST_ERROR");
}

void run_pool(Bench& b, std::uint64_t win, std::uint32_t win_size, unsigned seed, const std::string& what)
{
    Cpu& c = *b.cpu0;
    Pool p;
    p.make(seed);
    p.load(*b.mem, win);
    set_window(c, win, win_size);
    enable(c);
    const std::uint32_t retired0 = c.rich_rd(MODEL_RETIRED);
    check(p.program(c, win) == tlm::TLM_OK_RESPONSE, what + ": push accepted on the bus");
    check(wait_rich_idle(c), what + ": rich side idle");
    check(c.rich_rd(MODEL_RETIRED) == retired0 + 1, what + ": RETIRED +1");
    const std::uint32_t s = c.bank(STATUS);
    check((s & STATUS_DONE) && !(s & STATUS_ERROR) && b.irq.read(), what + ": STATUS " + hex(s) + " DONE, IRQ");
    p.verify(*b.mem, win, what);
    c.set_bank(STATUS, STATUS_DONE);
    c.set_bank(IRQ_STATUS, IRQ_DONE);
}

void case_maxpool(Bench& b)
{
    run_pool(b, kLowWin, 0x2000, 7, "MAX_POOL in the small arena");
    run_pool(b, kHighWin, 0x2000, 8, "MAX_POOL in the large arena");
    run_pool(b, kDdrBase + kDdrSize - 0x2000, 0x2000, 9, "MAX_POOL, window ending at the last DDR byte");
    const auto s = b.npu->stats();
    check(s.image_peak <= 0x2000 + (512u << 10), "private image sized by the window (" + std::to_string(s.image_peak) +
                                                     " B), not by the address (" + hex(kHighWin - kDdrBase) + ")");
    check(b.mem->max_len <= 4096, "no DMA transaction above 4 KiB (largest " + std::to_string(b.mem->max_len) + ")");
}

// Window and address checks at acceptance: refused pushes never run, never write.
void case_window(Bench& b)
{
    Cpu& c = *b.cpu0;
    Pool p;
    p.make(3);
    p.load(*b.mem, kLowWin);
    enable(c);
    auto expect_refused = [&](const std::string& what, std::uint32_t vp_bit, std::uint32_t rich_code) {
        const std::uint32_t accepted0 = c.bank(VP_ACCEPTED);
        const sc_time t0 = sc_time_stamp();
        check(p.program(c, kLowWin) == tlm::TLM_OK_RESPONSE, what + ": bus OK (refusal is latched, not a bus error)");
        wait(2, SC_US);
        const std::uint32_t rs = c.rich_rd(MODEL_STATUS);
        check(c.bank(VP_ACCEPTED) == accepted0 && (c.bank(VP_ERROR) & vp_bit) && (rs & RICH_STATUS_ERROR) &&
                  (rs >> 16) == rich_code && (c.bank(STATUS) & STATUS_ERROR) && b.mem->writes_in(kLowWin, kLowWin + 0x2000, t0) == 0,
              what + ": refused (RICH_STATUS " + hex(rs) + ", VP_ERROR " + hex(c.bank(VP_ERROR)) + "), nothing written");
        c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
        c.rich(MODEL_STATUS, 0);
        c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
        c.set_bank(IRQ_STATUS, IRQ_ERROR | IRQ_DONE);
    };
    set_window(c, kLowWin, 0);
    expect_refused("window size 0", VP_ERROR_BAD_ADDR, RICH_ERR_VP_BAD_ADDR);
    set_window(c, kLowWin, (32u << 20) + 4096);
    expect_refused("window above 32 MiB", VP_ERROR_BAD_ADDR, RICH_ERR_VP_BAD_ADDR);
    set_window(c, kDdrBase + kDdrSize - 0x1000, 0x2000);
    expect_refused("window crossing the end of DDR", VP_ERROR_BAD_ADDR, RICH_ERR_VP_BAD_ADDR);
    set_window(c, 0xFFFF'F000u, 0x2000);
    expect_refused("window wrapping 32 bits", VP_ERROR_BAD_ADDR, RICH_ERR_VP_BAD_ADDR);
    // Window fine, output tensor beyond its end.
    set_window(c, kLowWin, 0x1080);  // output [0x1000, 0x1100)
    expect_refused("output beyond the window", VP_ERROR_BAD_ADDR, RICH_ERR_VP_BAD_ADDR);
    // Input address outside the window (CPU gave a foreign buffer).
    set_window(c, kLowWin, 0x2000);
    p.program(c, kLowWin, false);
    c.rich(0x4000'0444u, static_cast<std::uint32_t>(kHighWin));
    {
        const std::uint32_t accepted0 = c.bank(VP_ACCEPTED);
        c.rich(MODEL_PUSH_A, RICH_OPCODE_ELEM_WISE);
        wait(1, SC_US);
        check(c.bank(VP_ACCEPTED) == accepted0 && (c.bank(VP_ERROR) & VP_ERROR_BAD_ADDR), "input outside the window refused");
        c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
        c.rich(MODEL_STATUS, 0);
        c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    }
    // Null (0) for a used pointer.
    p.program(c, kLowWin, false);
    c.rich(0x4000'0444u, 0);
    c.rich(MODEL_PUSH_A, RICH_OPCODE_ELEM_WISE);
    wait(1, SC_US);
    check(c.bank(VP_ERROR) & VP_ERROR_BAD_ADDR, "null input pointer refused");
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.rich(MODEL_STATUS, 0);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    // A good push afterwards runs.
    run_pool(b, kLowWin, 0x2000, 3, "MAX_POOL after the refusals");
}

void case_unsupported(Bench& b)
{
    Cpu& c = *b.cpu0;
    set_window(c, kLowWin, 0x2000);
    enable(c);
    for (const auto& t : std::vector<std::pair<std::uint32_t, std::string>>{{RICH_OPCODE_SET_NSPLIT, "SET_NSPLIT"},
                                                                            {RICH_OPCODE_FUSED_ATTN, "FUSED_ATTN"},
                                                                            {RICH_OPCODE_LAYERNORM, "LAYERNORM"},
                                                                            {0x16u, "DMA_TENSOR"},
                                                                            {0x33u, "unknown opcode"}}) {
        c.rich(MODEL_PUSH_A, t.first);
        wait(1, SC_US);
        const std::uint32_t rs = c.rich_rd(MODEL_STATUS);
        check((c.bank(VP_ERROR) & VP_ERROR_UNSUPPORTED) && (rs >> 16) == RICH_ERR_VP_UNSUPPORTED && c.bank(VP_ACCEPTED) == 0,
              t.second + " refused as unsupported (" + hex(rs) + ")");
        c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
        c.rich(MODEL_STATUS, 0);
    }
    c.rich(MODEL_PUSH_B, RICH_OPCODE_ELEM_WISE);
    wait(1, SC_US);
    check((c.rich_rd(MODEL_STATUS) >> 16) == static_cast<std::uint32_t>(rich_error_code::lane_b), "lane B push: LANE_B");
    c.rich(MODEL_STATUS, 0);
    c.rich(MODEL_LEGACY_HI_A, 0);
    wait(1, SC_US);
    check((c.rich_rd(MODEL_STATUS) >> 16) == static_cast<std::uint32_t>(rich_error_code::legacy_64bit),
          "legacy 64-bit instruction: LEGACY64");
    c.rich(MODEL_STATUS, 0);
    check((c.rich_rd(MODEL_STATUS) & RICH_STATUS_ERROR) == 0, "write to RICH_STATUS clears the sticky error");
    // Element-wise mode outside the ABI (subtract, 2): the model decoder refuses or the clone does.
    Pool p;
    p.make(4);
    p.load(*b.mem, kLowWin);
    p.program(c, kLowWin, false);
    c.rich(0x4000'0454u, 2);
    c.rich(MODEL_PUSH_A, RICH_OPCODE_ELEM_WISE);
    wait(1, SC_US);
    check(c.bank(VP_ACCEPTED) == 0 && (c.rich_rd(MODEL_STATUS) & RICH_STATUS_ERROR), "element-wise mode 2 refused");
    c.rich(MODEL_STATUS, 0);
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    run_pool(b, kLowWin, 0x2000, 4, "MAX_POOL after the unsupported pushes");
}

// The model decoder's own refusals (copy of MmioCompat at acceptance).
void case_refuse_model(Bench& b)
{
    Cpu& c = *b.cpu0;
    set_window(c, kLowWin, 0x4000);
    enable(c);
    // ELEM ADD with different operand lengths: the HAS ADD has no broadcast.
    c.rich(0x4000'0444u, static_cast<std::uint32_t>(kLowWin));
    c.rich(0x4000'0448u, static_cast<std::uint32_t>(kLowWin + 0x1000));
    c.rich(0x4000'0408u, static_cast<std::uint32_t>(kLowWin + 0x2000));
    c.rich(0x4000'0450u, 64);
    c.rich(0x4000'0464u, 64);
    c.rich(0x4000'0468u, 32);
    c.rich(0x4000'0454u, RICH_ELEM_ADD);
    c.rich(MODEL_PUSH_A, RICH_OPCODE_ELEM_WISE);
    wait(1, SC_US);
    check((c.rich_rd(MODEL_STATUS) >> 16) == static_cast<std::uint32_t>(rich_error_code::broadcast) &&
              (c.bank(VP_ERROR) & VP_ERROR_REFUSED) && c.bank(VP_ACCEPTED) == 0 && c.rich_rd(MODEL_RETIRED) == 0,
          "ADD with A_LEN != LEN: BROADCAST, not accepted, RETIRED unchanged");
    c.rich(MODEL_STATUS, 0);
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    // GEMM with an activation table but no LUT address: NEED_LUT.
    c.rich(0x4000'0400u, static_cast<std::uint32_t>(kLowWin));
    c.rich(0x4000'0404u, static_cast<std::uint32_t>(kLowWin + 0x1000));
    c.rich(0x4000'0408u, static_cast<std::uint32_t>(kLowWin + 0x2000));
    c.rich(0x4000'040Cu, static_cast<std::uint32_t>(kLowWin + 0x3000));
    c.rich(0x4000'0410u, 32);
    c.rich(0x4000'0414u, 32);
    c.rich(0x4000'0418u, 32);
    c.rich(0x4000'042Cu, RICH_ACT_SILU);
    c.rich(0x4000'0438u, 0x3F80'0000u);
    c.rich(0x4000'043Cu, 0x3F80'0000u);
    c.rich(0x4000'0440u, 0x3F80'0000u);
    c.rich(MODEL_PUSH_A, RICH_OPCODE_GEMM_FUSED);
    wait(1, SC_US);
    check((c.rich_rd(MODEL_STATUS) >> 16) == static_cast<std::uint32_t>(rich_error_code::need_lut) && c.bank(VP_ACCEPTED) == 0,
          "GEMM SiLU without LUT_ADDR: NEED_LUT");
    c.rich(MODEL_STATUS, 0);
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    c.rich(0x4000'042Cu, RICH_ACT_NONE);
    run_pool(b, kLowWin, 0x4000, 5, "MAX_POOL after the model refusals");
}

// Queue: 16 accepted while the first is held in staging; the 17th waits and
// gets a slot when the first starts; snapshot at acceptance (registers
// changed after the push do not change a queued entry).
void case_queue(Bench& b)
{
    Cpu& c = *b.cpu0;
    enable(c);
    std::vector<Pool> pools(18);
    for (unsigned i = 0; i < pools.size(); ++i) {
        pools[i].make(100 + i);
        pools[i].in_off = 0x2000 * i;
        pools[i].out_off = 0x2000 * i + 0x1000;
        pools[i].load(*b.mem, kLowWin);
    }
    set_window(c, kLowWin, 0x2000 * 18);
    // Hold the staging read of the first entry.
    b.mem->stall_lo = kLowWin;
    b.mem->stall_hi = kLowWin + 0x1000;
    pools[0].program(c, kLowWin);
    wait(1, SC_US);
    for (unsigned i = 1; i <= 16; ++i) pools[i].program(c, kLowWin);
    check(c.bank(VP_QUEUE_FREE) == 0 && (c.rich_rd(MODEL_STATUS) & RICH_STATUS_QUEUE_FULL) && c.bank(VP_ACCEPTED) == 17,
          "17 accepted: 1 staging (held) + 16 queued, QUEUE_FREE 0, QUEUE_FULL");
    // The 18th waits (push_wait 100 us) and is accepted when the hold ends.
    sc_spawn([&] {
        wait(20, SC_US);
        b.mem->released = true;
        b.mem->release.notify();
    });
    const sc_time t0 = sc_time_stamp();
    const auto st = pools[17].program(c, kLowWin);
    const sc_time waited = sc_time_stamp() - t0;
    check(st == tlm::TLM_OK_RESPONSE && waited >= sc_time(19, SC_US) && waited < sc_time(100, SC_US),
          "18th push stalled " + waited.to_string() + " until a slot freed, then accepted");
    // Change registers after the pushes: queued entries keep their snapshot.
    c.rich(0x4000'0444u, static_cast<std::uint32_t>(kHighWin));
    set_window(c, kHighWin, 0x1000);
    check(wait_rich_idle(c), "all 18 retired");
    check(c.rich_rd(MODEL_RETIRED) == 18 && c.bank(VP_ACCEPTED) == 18, "RETIRED 18");
    unsigned good = 0;
    for (unsigned i = 0; i < pools.size(); ++i) good += pools[i].verify(*b.mem, kLowWin, "entry " + std::to_string(i));
    check(good == 18, "every entry produced its own output (sequential stage/copy-back, snapshot at acceptance)");
    check(c.bank(VP_QUEUE_FREE) == 16 && (c.bank(STATUS) & STATUS_DONE), "queue empty, DONE");
}

// R1: queue full and worker stalled -> push times out with a bus error, is
// never enqueued; ABORT once the bus is free; quiescent only when the held
// DMA returned; nothing written for dropped work; a new job then passes.
// fabric: NPU on SYSBUS_1 (platform placement): while the push waits, CPU2
// reaches CLINT, PLIC and UART at once. fabric_s0 (contrast, NPU on SYSBUS_0):
// CPU2 reaches them only after the push returned, which is why the NPU moved.
void timeout_abort(Bench& b)
{
    const bool fabric = b.is_fabric(), on_s0 = b.topology == "fabric_s0";
    Cpu& c = *b.cpu0;
    enable(c);
    std::vector<Pool> pools(18);
    for (unsigned i = 0; i < pools.size(); ++i) {
        pools[i].make(200 + i);
        pools[i].in_off = 0x2000 * i;
        pools[i].out_off = 0x2000 * i + 0x1000;
        pools[i].load(*b.mem, kLowWin);
    }
    set_window(c, kLowWin, 0x2000 * 18);
    c.set_bank(VP_PUSH_WAIT_NS, 5000);
    b.mem->stall_lo = kLowWin;
    b.mem->stall_hi = kLowWin + 0x1000;
    pools[0].program(c, kLowWin);
    wait(1, SC_US);
    for (unsigned i = 1; i <= 16; ++i) pools[i].program(c, kLowWin);
    check(c.bank(VP_QUEUE_FREE) == 0, "queue full, first entry held in staging");
    // While the 18th push waits, the second CPU touches CLINT, PLIC and UART
    // (fabric). Not DDR: the stall injected here holds the MEMCTL_DDR port
    // itself (the first entry's staging read is parked inside TestMem), which
    // a real queue-full stall (a long model run) does not do.
    sc_time clint_at = SC_ZERO_TIME, plic_at = SC_ZERO_TIME, uart_at = SC_ZERO_TIME;
    bool second_done = false;
    tlm::tlm_response_status second_st = tlm::TLM_OK_RESPONSE;
    if (fabric) {
        sc_spawn([&] {
            wait(1, SC_US);
            std::uint32_t v = 0;
            auto note = [&](tlm::tlm_response_status st) {
                if (st != tlm::TLM_OK_RESPONSE) second_st = st;
            };
            note(b.cpu1->access(false, FX1_CLINT_BASE + FX1_CLINT_MTIME_OFF, v));
            clint_at = sc_time_stamp();
            note(b.cpu1->access(false, FX1_PLIC_BASE + FX1_PLIC_PRIORITY_OFF + 4, v));
            plic_at = sc_time_stamp();
            note(b.cpu1->access(true, FX1_UART_BASE, v));
            uart_at = sc_time_stamp();
            second_done = true;
        });
    }
    const sc_time t0 = sc_time_stamp();
    const auto st = pools[17].program(c, kLowWin);
    const sc_time t_ret = sc_time_stamp();
    check(st == tlm::TLM_GENERIC_ERROR_RESPONSE, "push into a full queue: TLM error after the wait limit");
    check(t_ret - t0 >= sc_time(5, SC_US) && t_ret - t0 < sc_time(7, SC_US), "it waited the limit (" + (t_ret - t0).to_string() + ")");
    check((c.bank(VP_ERROR) & VP_ERROR_PUSH_TIMEOUT) && (c.rich_rd(MODEL_STATUS) >> 16) == 1 && c.bank(VP_ACCEPTED) == 17,
          "PUSH_TIMEOUT latched, Q_OVF code, the push was not accepted");
    if (fabric) {
        wait(2, SC_US);
        const std::string at = " (push " + t0.to_string() + " .. " + t_ret.to_string() + "; CLINT " + clint_at.to_string() +
                               ", PLIC " + plic_at.to_string() + ", UART " + uart_at.to_string() + ")";
        check(second_done && second_st == tlm::TLM_OK_RESPONSE && b.clint->hits.size() == 1 && b.plic->hits.size() == 1 &&
                  b.uart->hits.size() == 1,
              "CPU2: CLINT, PLIC and UART accesses completed OK");
        if (on_s0) {
            check(second_done && clint_at >= t_ret && plic_at > clint_at && uart_at > plic_at,
                  "NPU on SYSBUS_0 (contrast): CPU2 reached CLINT, PLIC and UART only after the stalled push returned" + at);
        } else {
            check(second_done && uart_at < t_ret && clint_at < t0 + sc_time(2, SC_US),
                  "NPU on SYSBUS_1: CPU2 reached CLINT, PLIC and UART while the push was still stalled" + at);
        }
    }
    // ABORT now reaches the NPU.
    check(c.set_bank(VP_CMD, VP_CMD_ABORT) == tlm::TLM_OK_RESPONSE, "ABORT written");
    const std::uint32_t vs = c.bank(VP_STATUS);
    check(c.bank(VP_ABORT_DROPPED) == 16 && !(vs & VP_STATUS_QUIESCENT) && (vs & VP_STATUS_ENGINE_BUSY),
          "16 queued entries dropped; not quiescent while the held staging read is in flight (" + hex(vs) + ")");
    const sc_time t_abort = sc_time_stamp();
    wait(3, SC_US);
    b.mem->released = true;
    b.mem->release.notify();
    const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);
    while (!(c.bank(VP_STATUS) & VP_STATUS_QUIESCENT) && sc_time_stamp() < end) wait(100, SC_NS);
    check(c.bank(VP_STATUS) & VP_STATUS_QUIESCENT, "QUIESCENT once the held read returned");
    check(wait_rich_idle(c), "rich side idle");
    wait(50, SC_US);
    check(b.mem->writes_in(kLowWin, kLowWin + 0x2000 * 18, t_abort) == 0 && c.rich_rd(MODEL_RETIRED) == 0,
          "no DDR write for the aborted / dropped / timed-out entries; RETIRED 0");
    const std::uint32_t s = c.bank(STATUS);
    check((s & STATUS_ERROR) && c.bank(LAST_ERROR) == static_cast<std::uint32_t>(error_code::reset_aborted) && b.irq.read(),
          "STATUS ERROR, LAST_ERROR RESET_ABORTED, IRQ (" + hex(s) + ")");
    check(c.bank(VP_ERROR) & VP_ERROR_ABORTED, "VP_ERROR.ABORTED");
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.rich(MODEL_STATUS, 0);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    c.set_bank(IRQ_STATUS, IRQ_ERROR | IRQ_DONE);
    b.mem->stall_hi = 0;
    run_pool(b, kHighWin, 0x2000, 299, "new job after ABORT");
}
void case_abort(Bench& b) { timeout_abort(b); }
void case_fabric_timeout_abort(Bench& b) { timeout_abort(b); }
void case_fabric_s0_timeout_contrast(Bench& b) { timeout_abort(b); }

// ABORT during the model run: the output is not copied back.
void case_abort_running(Bench& b)
{
    Cpu& c = *b.cpu0;
    enable(c);
    Pool p;
    p.c = 64;
    p.h = 32;
    p.w = 32;
    p.k = 5;
    p.p = 2;
    p.in_off = 0;
    p.out_off = 0x10000;
    p.make(11);
    p.load(*b.mem, kLowWin);
    set_window(c, kLowWin, 0x20000);
    p.program(c, kLowWin);
    // Wait until the model runs (staging done: ENGINE_BUSY, no DMA in flight).
    const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);
    while (b.npu->stats().dma_read_bytes < 0x20000 && sc_time_stamp() < end) wait(20, SC_NS);
    c.set_bank(VP_CMD, VP_CMD_ABORT);
    const sc_time t_abort = sc_time_stamp();
    const std::uint32_t vs = c.bank(VP_STATUS);
    check((vs & VP_STATUS_ABORT_ACTIVE) && (vs & VP_STATUS_QUIESCENT),
          "ABORT while the model runs: ABORT_ACTIVE, QUIESCENT at once (no DMA will follow) " + hex(vs));
    check(wait_rich_idle(c), "the aborted instruction finished on the model");
    check(b.mem->writes_in(kLowWin + p.out_off, kLowWin + p.out_off + p.out_bytes(), t_abort) == 0,
          "its output was not copied back");
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.rich(MODEL_STATUS, 0);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    run_pool(b, kLowWin + 0x40000, 0x2000, 12, "next job after ABORT of a running one");
}

// Platform reset while work is queued: dropped, no late write, clean restart.
void case_reset(Bench& b)
{
    Cpu& c = *b.cpu0;
    enable(c);
    std::vector<Pool> pools(4);
    for (unsigned i = 0; i < pools.size(); ++i) {
        pools[i].make(300 + i);
        pools[i].in_off = 0x2000 * i;
        pools[i].out_off = 0x2000 * i + 0x1000;
        pools[i].load(*b.mem, kLowWin);
    }
    set_window(c, kLowWin, 0x8000);
    b.mem->stall_lo = kLowWin;
    b.mem->stall_hi = kLowWin + 0x1000;
    for (auto& p : pools) p.program(c, kLowWin);
    wait(1, SC_US);
    b.reset_n.write(false);
    wait(100, SC_NS);
    const sc_time t_reset = sc_time_stamp();
    b.mem->released = true;
    b.mem->release.notify();
    wait(10, SC_US);
    b.reset_n.write(true);
    wait(1, SC_US);
    {
        const std::uint32_t acc = c.bank(VP_ACCEPTED), qf = c.bank(VP_QUEUE_FREE), ret = c.rich_rd(MODEL_RETIRED),
                            stat = c.bank(STATUS), ws = c.bank(RICH_WINDOW_SIZE), vs = c.bank(VP_STATUS);
        check(acc == 0 && qf == 16 && ret == 0 && stat == STATUS_IDLE && ws == 0 && vs == VP_STATUS_QUIESCENT,
              "registers back at reset values after a platform reset (ACCEPTED " + std::to_string(acc) + ", QUEUE_FREE " +
                  std::to_string(qf) + ", RETIRED " + std::to_string(ret) + ", STATUS " + hex(stat) + ", WINDOW_SIZE " +
                  hex(ws) + ", VP_STATUS " + hex(vs) + ")");
    }
    wait(50, SC_US);
    check(b.mem->writes_in(kLowWin, kLowWin + 0x8000, t_reset) == 0, "no DDR write after the reset");
    b.mem->stall_hi = 0;
    run_pool(b, kLowWin, 0x2000, 310, "job after the reset");
    // GEMM-bank soft reset also clears the rich side.
    c.set_bank(CTRL, CTRL_ENABLE | CTRL_SOFT_RESET);
    wait(2, SC_US);
    check(c.rich_rd(MODEL_RETIRED) == 0 && c.bank(RICH_WINDOW_SIZE) == 0, "CTRL.SOFT_RESET clears the rich side too");
    run_pool(b, kLowWin, 0x2000, 311, "job after the soft reset");
}

void clear_errors(Cpu& c)
{
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.rich(MODEL_STATUS, 0);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    c.set_bank(IRQ_STATUS, IRQ_ERROR | IRQ_DONE);
}
// Until VP_STATUS.NATIVE_BUSY = 0; returns the time it cleared (or SC_ZERO_TIME).
sc_time wait_native_idle(Cpu& c, sc_time budget, sc_time step = sc_time(20, SC_NS))
{
    const sc_time end = sc_time_stamp() + budget;
    while (sc_time_stamp() < end) {
        if (!(c.bank(VP_STATUS) & VP_STATUS_NATIVE_BUSY)) return sc_time_stamp();
        wait(step);
    }
    return SC_ZERO_TIME;
}

// Review N1 R1: ABORT stops a raw native START (native offset 0) as it stops a
// GEMM-bank job: core soft reset, then STATUS ERROR / LAST_ERROR RESET_ABORTED
// / IRQ, VP_ERROR.ABORTED, reported once (never into the next job).
// Priority: an ABORT that reaches the NPU while NATIVE_BUSY = 1 wins over a
// completion (or core timeout) the native worker has not reported yet.
void case_native_abort(Bench& b)
{
    Cpu& c = *b.cpu0;
    const auto aborted_ok = [&](const std::string& what) {
        const std::uint32_t s = c.bank(STATUS), ve = c.bank(VP_ERROR), vs = c.bank(VP_STATUS);
        check((s & STATUS_ERROR) && !(s & STATUS_DONE) && (s & STATUS_IDLE) &&
                  c.bank(LAST_ERROR) == static_cast<std::uint32_t>(error_code::reset_aborted) && b.irq.read() &&
                  (ve & VP_ERROR_ABORTED) && !(vs & VP_STATUS_NATIVE_BUSY) && (vs & VP_STATUS_QUIESCENT),
              what + ": STATUS " + hex(s) + " ERROR, LAST_ERROR RESET_ABORTED, IRQ, VP_ERROR " + hex(ve) + ", VP_STATUS " + hex(vs));
    };
    enable(c);
    // 1. Raw START with the reset configuration: the core runs until it times out.
    check(c.wr(0, 1) == tlm::TLM_OK_RESPONSE, "raw native START accepted");
    wait(1, SC_US);
    std::uint32_t vs = c.bank(VP_STATUS);
    check((vs & VP_STATUS_NATIVE_BUSY) && (vs & VP_STATUS_QUIESCENT) && (c.bank(STATUS) & STATUS_BUSY),
          "raw run active: NATIVE_BUSY, QUIESCENT (it uses SRAM, no DMA), STATUS BUSY (" + hex(vs) + ")");
    Pool blocked;
    blocked.make(40);
    blocked.load(*b.mem, kLowWin);
    set_window(c, kLowWin, 0x2000);
    blocked.program(c, kLowWin);
    check((c.bank(VP_ERROR) & VP_ERROR_BUSY) && c.bank(VP_ACCEPTED) == 0, "rich push during the raw run: refused BUSY");
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.rich(MODEL_STATUS, 0);
    check(c.set_bank(VP_CMD, VP_CMD_ABORT) == tlm::TLM_OK_RESPONSE, "ABORT written");
    const sc_time t_idle = wait_native_idle(c, sc_time(1, SC_US), sc_time(1, SC_NS));
    check(t_idle != SC_ZERO_TIME, "NATIVE_BUSY clears after ABORT (core soft reset, " + t_idle.to_string() + ")");
    aborted_ok("raw run aborted");
    check(c.bank(VP_ABORT_DROPPED) == 0 && b.npu->stats().native_aborts == 1, "nothing dropped from the rich queue; one native abort");
    clear_errors(c);
    check(c.bank(STATUS) == STATUS_IDLE && !b.irq.read() && c.bank(VP_ERROR) == 0, "W1C: STATUS idle, IRQ low");
    // 2. The next jobs run and complete DONE (no stale abort report into them).
    gemm_job(b, kLowWin, kLowWin + 0x4000, kLowWin + 0x8000, 64, 41, "GEMM-bank job after the raw ABORT");
    run_pool(b, kLowWin + 0x10000, 0x2000, 42, "rich job after the raw ABORT");
    check(c.bank(VP_ERROR) == 0, "no VP error after the next jobs");
    // 3. ABORT with nothing running is a no-op for the native side.
    c.set_bank(VP_CMD, VP_CMD_ABORT);
    check(c.bank(VP_ERROR) == 0 && c.bank(STATUS) == STATUS_IDLE && !b.irq.read(), "ABORT while idle: no error, no IRQ");
    // 4. A raw START re-runs the program the GEMM-bank job left in the core and
    //    ends by itself (DONE) at T; measure T. After an ABORT the core was soft
    //    reset: its configuration must be programmed again (a raw START without
    //    that ends in CORE_DEADLOCK), so every run below is primed by a
    //    GEMM-bank job.
    const auto prime = [&] {
        gemm_job(b, kLowWin, kLowWin + 0x4000, kLowWin + 0x8000, 96, 43, "priming GEMM-bank job", true);
    };
    prime();
    sc_time t0 = sc_time_stamp();
    c.wr(0, 1);
    const sc_time t_end = wait_native_idle(c, sc_time(1, SC_MS), sc_time(1250, SC_PS));
    const std::uint32_t natural = c.bank(LAST_ERROR);
    check(t_end != SC_ZERO_TIME && natural != static_cast<std::uint32_t>(error_code::reset_aborted) &&
              !(c.bank(VP_ERROR) & VP_ERROR_ABORTED),
          "raw run left alone ends by itself after " + (t_end - t0).to_string() + " (LAST_ERROR " + std::to_string(natural) + ")");
    const sc_time T = t_end - t0;
    clear_errors(c);
    // 5. ABORT around that end (T is measured with 10 ns reads, so the scan
    //    spans T-80 ns .. T+20 ns in 1.25 ns steps): every run ends in exactly
    //    one of the two states (aborted, or its own end with ABORT a no-op); an
    //    abort is only possible if NATIVE_BUSY was still 1 just before it; the
    //    next raw run is not hit by a stale abort.
    unsigned n_aborted = 0, n_natural = 0, bad = 0;
    for (int d = -64; d <= 16; ++d) {
        prime();
        t0 = sc_time_stamp();
        c.wr(0, 1);
        const sc_time at = t0 + T + sc_time(1250, SC_PS) * d;
        if (at > sc_time_stamp()) wait(at - sc_time_stamp());
        const bool busy_before = (c.bank(VP_STATUS) & VP_STATUS_NATIVE_BUSY) != 0;  // 10 ns read just before the ABORT
        c.set_bank(VP_CMD, VP_CMD_ABORT);
        if (wait_native_idle(c, sc_time(1, SC_MS)) == SC_ZERO_TIME) ++bad;
        const std::uint32_t s = c.bank(STATUS), le = c.bank(LAST_ERROR), ve = c.bank(VP_ERROR);
        const bool was_aborted = le == static_cast<std::uint32_t>(error_code::reset_aborted);
        if (std::getenv("FX1_NPU_TEST_VERBOSE"))
            std::printf("    d=%d busy_before=%d STATUS %s LAST_ERROR %u VP_ERROR %s ended %s\n", d, busy_before, hex(s).c_str(), le,
                        hex(ve).c_str(), (sc_time_stamp() - t0).to_string().c_str());
        if (was_aborted) {
            ++n_aborted;
            if (!((s & STATUS_ERROR) && !(s & STATUS_DONE) && (ve & VP_ERROR_ABORTED)) || !busy_before) ++bad;
        } else {
            ++n_natural;
            if (le != natural || (ve & VP_ERROR_ABORTED) || (s & STATUS_ERROR) != (natural ? STATUS_ERROR : 0u)) ++bad;
        }
        clear_errors(c);
    }
    check(bad == 0 && n_aborted > 0 && n_natural > 0,
          "ABORT at T-80..T+20 ns: " + std::to_string(n_aborted) + " aborted, " + std::to_string(n_natural) +
              " ended by themselves, " + std::to_string(bad) + " inconsistent");
    prime();
    t0 = sc_time_stamp();
    c.wr(0, 1);
    const sc_time t_last = wait_native_idle(c, sc_time(1, SC_MS), sc_time(1250, SC_PS));
    check(c.bank(LAST_ERROR) == natural && !(c.bank(VP_ERROR) & VP_ERROR_ABORTED) && t_last - t0 >= T - sc_time(20, SC_NS),
          "the next raw run is not hit by a stale abort (" + (t_last - t0).to_string() + ")");
    clear_errors(c);
    // 6. Without priming, the raw START after an abort is the firmware's
    //    error: the core reports it (not the wrapper), and nothing hangs.
    c.wr(0, 1);
    wait(200, SC_NS);
    c.set_bank(VP_CMD, VP_CMD_ABORT);
    wait_native_idle(c, sc_time(1, SC_US));
    clear_errors(c);
    c.wr(0, 1);
    const sc_time t_un = wait_native_idle(c, sc_time(1, SC_MS));
    const std::uint32_t le_un = c.bank(LAST_ERROR);
    check(t_un != SC_ZERO_TIME && le_un != static_cast<std::uint32_t>(error_code::reset_aborted) && !(c.bank(VP_ERROR) & VP_ERROR_ABORTED),
          "unprimed raw START after an ABORT ends by itself (LAST_ERROR " + std::to_string(le_un) + "), no stale abort");
    clear_errors(c);
    gemm_job(b, kLowWin, kLowWin + 0x4000, kLowWin + 0x8000, 96, 44, "GEMM-bank job at the end");
}

// Review N1 R2: a platform reset while an NPU DMA transaction is in flight. The
// transaction completes (it is drained, not recalled) but nothing of the old
// entry is published in the new epoch: RETIRED, ACCEPTED, VP_ERROR,
// VP_ERROR_INFO, STATUS, IRQ stay at their reset values, the entry is not
// dispatched to the model (staging case), the rest of its copy-back is not
// written, and the next job passes.
void case_reset_inflight(Bench& b)
{
    Cpu& c = *b.cpu0;
    const auto pulse_reset_while = [&](const std::function<bool()>& in_flight, const std::function<void()>& release) {
        const sc_time end = sc_time_stamp() + sc_time(10, SC_MS);
        while (!in_flight() && sc_time_stamp() < end) wait(5, SC_NS);
        check(in_flight(), "  transaction held in flight");
        b.reset_n.write(false);
        wait(100, SC_NS);
        b.reset_n.write(true);
        wait(100, SC_NS);
        const sc_time t = sc_time_stamp();
        release();
        wait(20, SC_US);
        return t;
    };
    const auto epoch_clean = [&](const std::string& what) {
        const std::uint32_t ret = c.rich_rd(MODEL_RETIRED), acc = c.bank(VP_ACCEPTED), ve = c.bank(VP_ERROR),
                            vi = c.bank(VP_ERROR_INFO), st = c.bank(STATUS), rs = c.rich_rd(MODEL_STATUS),
                            vs = c.bank(VP_STATUS), ctl = c.rich_rd(MODEL_NEO_BASE + NEO_CTRL_OFF),
                            ist = c.rich_rd(MODEL_NEO_BASE + NEO_ISTATUS_OFF);
        check(ret == 0 && acc == 0 && ve == 0 && vi == 0 && st == STATUS_IDLE && rs == 0 && vs == VP_STATUS_QUIESCENT &&
                  !(ctl & 2u) && ist == 0 && !b.irq.read(),
              what + ": new epoch untouched (RETIRED " + std::to_string(ret) + ", ACCEPTED " + std::to_string(acc) +
                  ", VP_ERROR " + hex(ve) + ", VP_ERROR_INFO " + hex(vi) + ", STATUS " + hex(st) + ", RICH_STATUS " + hex(rs) +
                  ", VP_STATUS " + hex(vs) + ", NEO CTRL " + hex(ctl) + ", ISTATUS " + hex(ist) + ")");
    };
    // a) Single-transaction copy-back (256 B), reset while it is in flight.
    {
        enable(c);
        Pool p;
        p.make(17);
        p.load(*b.mem, kLowWin);
        set_window(c, kLowWin, 0x2000);
        b.mem->writes_seen = 0;
        b.mem->hold_write = 1;
        p.program(c, kLowWin);
        pulse_reset_while([&] { return b.mem->write_held; }, [&] { b.mem->write_release.notify(); });
        epoch_clean("reset during the only copy-back transaction");
        run_pool(b, kLowWin, 0x2000, 18, "job after reset a)");
    }
    // b) Multi-transaction copy-back (16 KiB output = 4 transactions): reset
    //    while the first is in flight, then while the last is in flight.
    for (unsigned held : {1u, 4u}) {
        enable(c);
        Pool p;
        p.c = 16;
        p.h = 32;
        p.w = 32;
        p.in_off = 0;
        p.out_off = 0x8000;
        p.make(19 + held);
        p.load(*b.mem, kLowWin);
        set_window(c, kLowWin, 0x10000);
        b.mem->writes_seen = 0;
        b.mem->hold_write = held;
        p.program(c, kLowWin);
        const std::string what = "reset during copy-back transaction " + std::to_string(held) + " of 4";
        const sc_time t = pulse_reset_while([&] { return b.mem->write_held; }, [&] { b.mem->write_release.notify(); });
        epoch_clean(what);
        // The held transaction was issued before the reset: it completes (drained)
        // and is logged when it does, after the release.
        std::size_t after = 0;
        for (const auto& w : b.mem->writes) after += w.t >= t;
        check(after == 1 && b.mem->writes_in(kLowWin, kLowWin + 0x10000, t) == 4096,
              what + ": only the transaction issued before the reset completed (" + std::to_string(after) +
                  " write transactions after the hold), no later one");
        run_pool(b, kLowWin, 0x2000, 30 + held, "job after reset b" + std::to_string(held));
    }
    // c) Reset while the last staging read is in flight: never dispatched.
    //    The input lies above the output, so its read is the last one.
    {
        enable(c);
        Pool p;
        p.out_off = 0;
        p.in_off = 0x1000;
        p.make(23);
        p.load(*b.mem, kLowWin);
        set_window(c, kLowWin, 0x2000);
        const std::uint64_t dispatched0 = b.npu->stats().model_dispatches;
        b.mem->released = false;
        b.mem->stall_lo = kLowWin + p.in_off;
        b.mem->stall_hi = kLowWin + p.in_off + 1;
        p.program(c, kLowWin);
        const sc_time t = pulse_reset_while([&] { return b.mem->read_held; },
                                            [&] {
                                                b.mem->released = true;
                                                b.mem->release.notify();
                                            });
        epoch_clean("reset during the last staging read");
        check(b.npu->stats().model_dispatches == dispatched0, "the entry was never dispatched to the model");
        check(b.mem->writes_in(kLowWin, kLowWin + 0x2000, t) == 0, "nothing written for it");
        b.mem->stall_hi = 0;
        run_pool(b, kLowWin, 0x2000, 24, "job after reset c)");
    }
}

void case_dma_error(Bench& b)
{
    Cpu& c = *b.cpu0;
    enable(c);
    Pool p;
    p.make(21);
    p.load(*b.mem, kLowWin);
    set_window(c, kLowWin, 0x2000);
    b.mem->error_lo = kLowWin;
    b.mem->error_hi = kLowWin + 0x100;
    p.program(c, kLowWin);
    check(wait_rich_idle(c), "rich side idle after a staging read error");
    check((c.bank(VP_ERROR) & VP_ERROR_DMA) && (c.rich_rd(MODEL_STATUS) >> 16) == RICH_ERR_VP_DMA &&
              c.bank(LAST_ERROR) == static_cast<std::uint32_t>(error_code::dma_read) && c.rich_rd(MODEL_RETIRED) == 0,
          "staging read error: VP_ERROR.DMA, DMA_READ, not retired");
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.rich(MODEL_STATUS, 0);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    // Copy-back error.
    b.mem->error_lo = kLowWin + p.out_off;
    b.mem->error_hi = kLowWin + p.out_off + 0x10;
    // Reads of that range also fail: put the output elsewhere for this run.
    Pool q = p;
    q.out_off = 0x1800;
    q.load(*b.mem, kLowWin);
    b.mem->error_lo = kLowWin + 0x1800;
    b.mem->error_hi = kLowWin + 0x1810;
    q.program(c, kLowWin);
    wait_rich_idle(c);
    check((c.bank(VP_ERROR) & VP_ERROR_DMA) && c.rich_rd(MODEL_RETIRED) == 0, "error on the output range: DMA error, not retired");
    c.set_bank(VP_CMD, VP_CMD_CLEAR_ERROR);
    c.rich(MODEL_STATUS, 0);
    c.set_bank(STATUS, STATUS_ERROR | STATUS_DONE);
    b.mem->error_hi = 0;
    run_pool(b, kLowWin, 0x2000, 22, "job after the DMA errors");
}

// Host memory: construct/idle, a small job, a 32 MiB window job, small again, reuse.
void case_memory(Bench& b)
{
    std::printf("[mem] after construction and reset: peak RSS %ld KiB, current %ld KiB, image %zu B\n", peak_rss_kib(),
                current_rss_kib(), b.npu->stats().image_size);
    run_pool(b, kLowWin, 0x2000, 41, "small window (8 KiB) in the low arena");
    std::printf("[mem] after small low job: peak RSS %ld KiB, current %ld KiB, image %zu B\n", peak_rss_kib(), current_rss_kib(),
                b.npu->stats().image_size);
    run_pool(b, kHighWin, 0x2000, 42, "small window (8 KiB) in the high arena");
    const auto s1 = b.npu->stats();
    std::printf("[mem] after small high job: peak RSS %ld KiB, current %ld KiB, image %zu B (capacity %zu)\n", peak_rss_kib(),
                current_rss_kib(), s1.image_size, s1.image_capacity);
    check(s1.image_size <= 0x2000 + (512u << 10), "a high-address window costs its size, not its address");
    // A 32 MiB window (job uses its last 8 KiB): the image grows to the window.
    Cpu& c = *b.cpu0;
    Pool p;
    p.make(43);
    p.in_off = (32u << 20) - 0x2000;
    p.out_off = (32u << 20) - 0x1000;
    p.load(*b.mem, kHighWin);
    set_window(c, kHighWin, 32u << 20);
    p.program(c, kHighWin);
    wait_rich_idle(c, sc_time(200, SC_MS));
    p.verify(*b.mem, kHighWin, "32 MiB window, tensors at its end");
    const auto s2 = b.npu->stats();
    std::printf("[mem] after 32 MiB window job: peak RSS %ld KiB, current %ld KiB, image %zu B (capacity %zu)\n", peak_rss_kib(),
                current_rss_kib(), s2.image_size, s2.image_capacity);
    check(s2.image_size <= (32u << 20) + (512u << 10), "image = window + model staging");
    check(s2.dma_read_bytes < (1u << 20), "only the instruction's regions are staged (" + std::to_string(s2.dma_read_bytes) + " B read)");
    run_pool(b, kLowWin, 0x2000, 44, "small window again (reuse)");
    const auto s3 = b.npu->stats();
    std::printf("[mem] after small job again: peak RSS %ld KiB, current %ld KiB, image %zu B (capacity %zu, peak %zu)\n",
                peak_rss_kib(), current_rss_kib(), s3.image_size, s3.image_capacity, s3.image_peak);
    check(s3.image_size == s2.image_size, "the image is reused, not reallocated");
}

void case_fabric_maxpool(Bench& b)
{
    run_pool(b, kLowWin, 0x2000, 51, "MAX_POOL over the fabric, small arena");
    run_pool(b, kHighWin, 0x2000, 52, "MAX_POOL over the fabric, large arena");
    run_pool(b, kDdrBase + kDdrSize - 0x2000, 0x2000, 53, "MAX_POOL over the fabric, last DDR bytes");
    // Native MMIO while the rich side is idle, from the second CPU.
    tlm::tlm_response_status st;
    b.cpu1->rd(NATIVE_CFG_PROFILE, &st);
    check(st == tlm::TLM_OK_RESPONSE, "native register read over the fabric (CPU2)");
}
void case_fabric_gemm(Bench& b)
{
    gemm_job(b, kLowWin, kLowWin + 0x4000, kLowWin + 0x8000, 64, 61, "GEMM bank over the fabric, low DDR");
    gemm_job(b, kHighWin, kHighWin + 0x4000, kHighWin + 0x8000, 128, 62, "GEMM bank over the fabric, high DDR");
}

// ---------------------------------------------------------- gate replay ----
struct GateWrite {
    std::uint32_t addr, val;
};
struct GateIns {
    std::string label;
    std::vector<GateWrite> writes;
    std::uint32_t out = 0;
    std::uint64_t bytes = 0;
};
bool is_address_register(std::uint32_t a)
{
    switch (a) {
    case 0x40000400: case 0x40000404: case 0x40000408: case 0x4000040C: case 0x40000434:
    case 0x40000444: case 0x40000448: case 0x4000044C:
    case 0x40000490: case 0x40000494: case 0x40000498: case 0x400004E0: case 0x400004E4:
        return true;
    default:
        return false;
    }
}
bool read_file(const std::string& p, std::vector<std::uint8_t>& v)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    v.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}
// Output tensor of each instruction, from its registers (independent of the
// clone): the drop testbench's rule plus GEMM without extension registers
// (M positions x N outputs). An instruction with 0 output bytes FAILS.
bool parse_gate(const std::string& dir, std::vector<GateIns>& out)
{
    std::ifstream f(dir + "/mmio.txt");
    if (!f) return false;
    std::string line, label;
    GateIns cur;
    std::uint32_t sticky[27] = {0}, ext[38] = {0};
    bool ext_set[38] = {false};
    while (std::getline(f, line)) {
        if (line.rfind("# [", 0) == 0) {
            label = line.substr(2);
            continue;
        }
        if (line.empty() || line[0] == '#') continue;
        if (line[0] != 'W') {
            std::printf("unsupported line: %s\n", line.c_str());
            return false;
        }
        std::istringstream is(line);
        std::string tag;
        GateWrite w{};
        is >> tag >> std::hex >> w.addr >> w.val;
        if (w.addr >= 0x4000046C && w.addr < 0x40000504) {
            ext[(w.addr - 0x4000046C) / 4] = w.val;
            ext_set[(w.addr - 0x4000046C) / 4] = true;
        } else if (w.addr >= 0x40000400 && w.addr <= 0x40000468) {
            sticky[(w.addr - 0x40000400) / 4] = w.val;
        }
        cur.writes.push_back(w);
        if (w.addr == 0x40000310) {
            const std::uint32_t op = w.val & 0xFF;
            const std::uint32_t pack = sticky[(0x454 - 0x400) / 4];
            const std::uint32_t mode = (pack & 0xFFFF0000u) ? (pack >> 24) & 0xFF : pack;
            std::uint64_t c = 0, h = 0, wd = 0;
            if (op == 0x12 && ext_set[0]) {
                c = ext[3];
                h = ext[4];
                wd = ext[5];
            } else if (op == 0x12) {  // no extension registers: M x K -> N on a 1 x M image
                c = sticky[(0x418 - 0x400) / 4];
                h = 1;
                wd = sticky[(0x410 - 0x400) / 4];
            } else if (op == 0x15 && mode == 0) {
                c = 1;
                h = 1;
                wd = sticky[(0x450 - 0x400) / 4];
            } else if (op == 0x15) {
                const std::uint32_t k = ext[23], pd = ext[24], sd = sticky[(0x424 - 0x400) / 4] ? sticky[(0x424 - 0x400) / 4] : 1;
                c = ext[0];
                h = (ext[1] + 2 * pd - k) / sd + 1;
                wd = (ext[2] + 2 * pd - k) / sd + 1;
            }
            cur.label = label;
            cur.out = sticky[(0x408 - 0x400) / 4];
            cur.bytes = c * h * wd;
            out.push_back(cur);
            cur = GateIns();
            for (auto& x : ext) x = 0;
            for (auto& x : ext_set) x = false;
        }
    }
    return true;
}

// Writes of a window program (absolute addresses), split at every push.
bool parse_window_program(const std::string& path, std::vector<std::vector<GateWrite>>& out, unsigned& bells)
{
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    std::vector<GateWrite> cur;
    bells = 0;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line[0] != 'W') return false;
        std::istringstream is(line);
        std::string tag;
        GateWrite w{};
        is >> tag >> std::hex >> w.addr >> w.val;
        cur.push_back(w);
        if (w.addr == 0x40000310) {
            bells += (w.val & 0xFF) == 0x17;
            out.push_back(cur);
            cur.clear();
        }
    }
    return cur.empty();
}

int case_gate(const std::string& dir, const std::string& golden_path, std::uint64_t win, const std::string& topo,
              const std::string& window_program = std::string(), bool reset_first = false)
{
    std::vector<std::uint8_t> init, gold;
    std::vector<GateIns> prog;
    if (!read_file(dir + "/dram_init.bin", init) || !read_file(golden_path, gold) || !parse_gate(dir, prog) || prog.empty() ||
        gold.size() != init.size()) {
        std::printf("cannot read the program %s or the golden %s\n", dir.c_str(), golden_path.c_str());
        return 2;
    }
    unsigned bells = 0;
    if (!window_program.empty()) {
        std::vector<std::vector<GateWrite>> wp;
        if (!parse_window_program(window_program, wp, bells) || wp.size() != prog.size()) {
            std::printf("window program %s: unreadable or not %zu instructions\n", window_program.c_str(), prog.size());
            return 2;
        }
        // Rich (v4.5) lines of a mixed program keep image offsets; window words are absolute already.
        for (std::size_t i = 0; i < prog.size(); ++i) {
            prog[i].writes.clear();
            for (auto w : wp[i]) {
                if (is_address_register(w.addr) && !(w.addr == 0x400004E4 && w.val == 0) && w.val < kDdrBase)
                    w.val += static_cast<std::uint32_t>(win);
                prog[i].writes.push_back(w);
            }
        }
        std::printf("[gate] window program %s: %u of %zu instructions through the neo register file + doorbell 0x17\n",
                    window_program.c_str(), bells, prog.size());
    }
    const bool absolute = !window_program.empty();
    const std::uint32_t window = static_cast<std::uint32_t>((init.size() + 4095) & ~std::size_t(4095));
    fx1::npu::NpuOptions o;
    return run_bench(topo, o, [&](Bench& b) {
        Cpu& c = *b.cpu0;
        if (reset_first) {
            // Review N1 R2 on the neo path: instruction #0 through the register
            // file and the doorbell; a platform reset while its last copy-back
            // transaction is in flight; that transaction drains, and the new
            // epoch shows no RETIRED, no neo CONTROL.DONE / ISTATUS, no error.
            const auto& i0 = prog[0];
            const std::uint64_t chunk = 4096, lo = i0.out / chunk * chunk, hi = (std::uint64_t{i0.out} + i0.bytes + chunk - 1) / chunk * chunk;
            b.mem->poke(win, init.data(), init.size());
            set_window(c, win, window);
            enable(c);
            c.rich(MODEL_NEO_BASE + NEO_IEN_OFF, 1);
            b.mem->writes_seen = 0;
            b.mem->hold_write = static_cast<unsigned>((hi - lo) / chunk);
            for (const auto& w : i0.writes) c.rich(w.addr, w.val);
            const sc_time end = sc_time_stamp() + sc_time(1, SC_SEC);
            while (!b.mem->write_held && sc_time_stamp() < end) wait(10, SC_NS);
            check(b.mem->write_held, "#0: copy-back transaction " + std::to_string((hi - lo) / chunk) + " held in flight");
            b.reset_n.write(false);
            wait(100, SC_NS);
            b.reset_n.write(true);
            wait(100, SC_NS);
            const sc_time t = sc_time_stamp();
            b.mem->write_release.notify();
            wait(20, SC_US);
            std::size_t after = 0;
            for (const auto& w : b.mem->writes) after += w.t >= t;
            const std::uint32_t ret = c.rich_rd(MODEL_RETIRED), ctl = c.rich_rd(MODEL_NEO_BASE + NEO_CTRL_OFF),
                                ist = c.rich_rd(MODEL_NEO_BASE + NEO_ISTATUS_OFF), ve = c.bank(VP_ERROR),
                                vi = c.bank(VP_ERROR_INFO), st = c.bank(STATUS), acc = c.bank(VP_ACCEPTED);
            check(ret == 0 && !(ctl & 2u) && ist == 0 && ve == 0 && vi == 0 && st == STATUS_IDLE && acc == 0 && !b.irq.read() &&
                      after == 1,
                  "reset during the doorbell's last copy-back: RETIRED " + std::to_string(ret) + ", neo CTRL " + hex(ctl) +
                      ", ISTATUS " + hex(ist) + ", VP_ERROR " + hex(ve) + ", INFO " + hex(vi) + ", STATUS " + hex(st) +
                      ", ACCEPTED " + std::to_string(acc) + ", " + std::to_string(after) + " transaction drained after the reset");
            b.mem->writes.clear();
            std::printf("[gate] reset epoch checked; the whole program runs again from the initial image\n");
        }
        b.mem->poke(win, init.data(), init.size());
        set_window(c, win, window);
        enable(c);
        std::printf("[gate] %s: %zu instructions, window %s + %u B, %s\n", dir.c_str(), prog.size(), hex(win).c_str(), window,
                    topo.c_str());
        unsigned zero = 0;
        for (std::size_t i = 0; i < prog.size(); ++i) {
            // Single producer: check the credit before the push (QUEUE_FREE).
            while (c.bank(VP_QUEUE_FREE) == 0) wait(1, SC_US);
            for (const auto& w : prog[i].writes) {
                const std::uint32_t v =
                    (!absolute && is_address_register(w.addr)) ? static_cast<std::uint32_t>(win + w.val) : w.val;
                if (c.rich(w.addr, v) != tlm::TLM_OK_RESPONSE) check(false, "write " + hex(w.addr) + " rejected");
            }
            if (prog[i].bytes == 0) ++zero;
        }
        check(wait_rich_idle(c, sc_time(5, SC_SEC)), "program finished");
        const std::uint32_t retired = c.rich_rd(MODEL_RETIRED), rs = c.rich_rd(MODEL_STATUS);
        check(retired == prog.size() && !(rs & RICH_STATUS_ERROR) && c.bank(VP_ERROR) == 0 && (c.bank(STATUS) & STATUS_DONE) &&
                  !(c.bank(STATUS) & STATUS_ERROR) && b.irq.read(),
              "RETIRED " + std::to_string(retired) + "/" + std::to_string(prog.size()) + ", RICH_STATUS " + hex(rs) +
                  ", no error, DONE + IRQ");
        std::vector<std::uint8_t> got(init.size());
        b.mem->peek(win, got.data(), got.size());
        std::uint64_t total_bad = 0;
        for (std::size_t i = 0; i < prog.size(); ++i) {
            const auto& ins = prog[i];
            std::uint64_t bad = 0;
            const bool inside = ins.bytes && std::uint64_t{ins.out} + ins.bytes <= got.size();
            if (inside)
                for (std::uint64_t k = 0; k < ins.bytes; ++k) bad += got[ins.out + k] != gold[ins.out + k];
            check(inside && bad == 0, "#" + std::to_string(i) + " " + ins.label + ": " + std::to_string(ins.bytes) +
                                          " output bytes checked, " + std::to_string(bad) + " differ from the oracle");
        }
        for (std::size_t k = 0; k < got.size(); ++k) total_bad += got[k] != gold[k];
        check(total_bad == 0, "whole window equals the oracle image (" + std::to_string(total_bad) + " bytes differ; catches stray writes)");
        check(zero == 0, "no instruction with 0 checked bytes");
        if (absolute) check(bells > 0, "the neo register file and the doorbell were used (" + std::to_string(bells) + " doorbells)");
        std::printf("[gate] PERF_EXEC_CYCLES %u, DMA %s\n", c.bank(PERF_EXEC_CYCLES), b.npu->report().c_str());
    }, sc_time(20, SC_SEC));
}

} // namespace

int sc_main(int argc, char* argv[])
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: test_fx1_npu <case> [args]\n");
        return 2;
    }
    const std::string name = argv[1];
    std::printf("[test] case %s\n", name.c_str());
    fx1::npu::NpuOptions o;
    static const std::map<std::string, std::pair<std::string, void (*)(Bench&)>> cases{
        {"regs", {"direct", case_regs}},
        {"gemm", {"direct", case_gemm}},
        {"maxpool", {"direct", case_maxpool}},
        {"window", {"direct", case_window}},
        {"unsupported", {"direct", case_unsupported}},
        {"refuse_model", {"direct", case_refuse_model}},
        {"queue", {"direct", case_queue}},
        {"abort", {"direct", case_abort}},
        {"abort_running", {"direct", case_abort_running}},
        {"reset", {"direct", case_reset}},
        {"dma_error", {"direct", case_dma_error}},
        {"memory", {"direct", case_memory}},
        {"fabric_maxpool", {"fabric", case_fabric_maxpool}},
        {"fabric_gemm", {"fabric", case_fabric_gemm}},
        {"fabric_timeout_abort", {"fabric", case_fabric_timeout_abort}},
        {"fabric_s0_timeout_contrast", {"fabric_s0", case_fabric_s0_timeout_contrast}},
        {"native_abort", {"direct", case_native_abort}},
        {"reset_inflight", {"direct", case_reset_inflight}},
    };
    if (name == "dbg") return case_dbg();
    if (name == "gate_neo" || name == "gate_neo_reset") {
        if (argc != 7) {
            std::fprintf(stderr, "usage: test_fx1_npu gate_neo[_reset] <window program> <dir> <golden.bin> <window base> <direct|fabric>\n");
            return 2;
        }
        return case_gate(argv[3], argv[4], std::stoull(argv[5], nullptr, 0), argv[6], argv[2], name == "gate_neo_reset");
    }
    if (name == "gate") {
        if (argc != 6) {
            std::fprintf(stderr, "usage: test_fx1_npu gate <dir> <golden.bin> <window base> <direct|fabric>\n");
            return 2;
        }
        return case_gate(argv[2], argv[3], std::stoull(argv[4], nullptr, 0), argv[5]);
    }
    const auto it = cases.find(name);
    if (it == cases.end()) {
        std::fprintf(stderr, "unknown case %s\n", name.c_str());
        return 2;
    }
    return run_bench(it->second.first, o, it->second.second);
}
