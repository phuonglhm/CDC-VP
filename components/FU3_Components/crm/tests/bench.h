#pragma once
// Test theo HAS v2.0 + FU3 CRM Specification V1.1: khởi động (system -> Ibex, CVA6 giữ), giá trị reset/mask, truy cập,
// PLL và clock, gate/reset output, reset hệ thống từ WDT/SYSCSR/debug, global reset, PRSTN, IRQ
// Dùng chung cho behavior.cpp (nối thẳng) và integration.cpp (qua bus FU3, định nghĩa FU3_CRM_WITH_BUS)
#include <fu3_crm/crm.h>

#include <cstdio>
#include <iostream>
#include <string>
#include <tlm_utils/simple_initiator_socket.h>

#ifdef FU3_CRM_WITH_BUS
    #include <fu3_bus/access.h>
    #include <fu3_bus/bus_system.h>
#endif

using namespace fu3_crm;

// Giá trị reset và mask chép độc lập từ CSR_CRM_fu3_v2 (reset của gate/reset lấy theo field)
struct Expect { std::uint32_t offset, reset, rw; };
constexpr Expect kExpect[] = {
    {0x0000, 0x0001'0000, 0x0},
    {0x0010, 0x0000'0231, 0x3FFF},
    {0x0018, 0x0000'5949, 0x7FFF},
    {0x001C, 0x0, 0x1},
    {0x0020, 0x0, 0x1},
    {0x0024, 0x0, 0x3FF},
    {0x0028, 0x0000'01FF, 0x1FF},
    {0x002C, 0x01FF'FFFF, 0x1FF'FFFF},
    {0x0030, 0x0, 0x1},
    {0x0034, 0x0, 0x1},
    {0x0038, 0x1, 0x1},
    {0x003C, 0x1, 0x1},
    {0x0040, 0x1, 0x1},
    {0x0044, 0x0000'00FF, 0xFF},
    {0x0048, 0x01FF'FFFF, 0x1FF'FFFF},
    {0x0054, 0x0, 0x1},
    {0x0058, 0x0, 0x0},
    {0x005C, 0x0, 0x1},
};

// Độ dài trong bench: giữ reset 200 ns, chờ ổn định 1 us
const sc_core::sc_time kHold(200, sc_core::SC_NS), kStartup(1, sc_core::SC_US);

struct Bench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    Crm dut{"dut", kHold, kStartup};
    sc_core::sc_signal<bool> prst_n{"prst_n", false};
    sc_core::sc_signal<bool> wdt{"wdt"}, cpu_req{"cpu_req"}, sys_req{"sys_req"}, global_req{"global_req"};
    sc_core::sc_signal<bool> sys_rst_n{"sys_rst_n"}, ibex_rst_n{"ibex_rst_n"}, cva6_rst_n{"cva6_rst_n"};
    sc_core::sc_signal<bool> dbg_rst_n{"dbg_rst_n"}, irq{"irq"};
    sc_core::sc_signal<std::uint32_t> gate0{"gate0"}, gate1{"gate1"}, rst0{"rst0"}, rst1{"rst1"};
    unsigned falls[4] = {};   // Số sườn xuống của sys, ibex, cva6, dbg
    std::uint64_t base = 0;
    unsigned checks = 0, errors = 0;

#ifdef FU3_CRM_WITH_BUS
    fu3_bus::BusSystem* fabric = nullptr;
#endif

    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name n, bool via_bus) : sc_module(n) {
        dut.prst_n(prst_n);
        dut.wdt_reset_req(wdt);
        dut.cpu_reset_req(cpu_req);
        dut.sw_reset_req(sys_req);
        dut.sw_global_rst_req(global_req);
        dut.sys_rst_n(sys_rst_n);
        dut.ibex_rst_n(ibex_rst_n);
        dut.cva6_rst_n(cva6_rst_n);
        dut.dbg_rst_n(dbg_rst_n);
        dut.irq(irq);
        dut.clk_gate_0(gate0);
        dut.clk_gate_1(gate1);
        dut.sw_rst_n_0(rst0);
        dut.sw_rst_n_1(rst1);

#ifdef FU3_CRM_WITH_BUS
        if (via_bus) {
            auto cfg = fu3_bus::BusConfig::fu3();
            for (auto& t : cfg.targets)
                t.enabled = t.name == "CRM";

            fabric = new fu3_bus::BusSystem("Fabric", cfg);
            cpu.bind(fabric->initiator("IBEX"));
            fabric->target("CRM").bind(dut.socket);
            base = FU3_CRM_BASE;
        } else
#endif

        {
            (void)via_bus;
            cpu.bind(dut.socket);
        }
        SC_METHOD(count_falls);
        sensitive << sys_rst_n.negedge_event() << ibex_rst_n.negedge_event() << cva6_rst_n.negedge_event()
                  << dbg_rst_n.negedge_event();
        dont_initialize();
        SC_THREAD(run);
    }

    void count_falls() {
        if (sys_rst_n.negedge()) ++falls[0];
        if (ibex_rst_n.negedge()) ++falls[1];
        if (cva6_rst_n.negedge()) ++falls[2];
        if (dbg_rst_n.negedge()) ++falls[3];
    }

    void check(bool ok, const std::string& what) {
        ++checks;
        if (!ok) {
            ++errors;
            std::cerr << "FAIL: " << what << '\n';
        }
    }

    tlm::tlm_response_status xfer(bool write, std::uint32_t offset, std::uint32_t& value,
                                  unsigned len = 4, const unsigned char* be = nullptr, bool debug = false) {
        unsigned char data[4] = {};
        for (unsigned i = 0; i < len; ++i) data[i] = (value >> (8 * i)) & 0xFF;
        tlm::tlm_generic_payload tx;

        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(base + offset);
        tx.set_data_ptr(data);
        tx.set_data_length(len);
        tx.set_streaming_width(len);

        if (be) {
            tx.set_byte_enable_ptr(const_cast<unsigned char*>(be));
            tx.set_byte_enable_length(len);
        }

        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

#ifdef FU3_CRM_WITH_BUS
        fu3_bus::ScopedAccess access(tx, len);
#endif

        if (debug) {
            cpu->transport_dbg(tx);
        } else {
            cpu->b_transport(tx, delay);
        }
        if (!write) {
            value = 0;
            for (unsigned i = 0; i < len; ++i) value |= std::uint32_t(data[i]) << (8 * i);
        }
        return tx.get_response_status();
    }
    std::uint32_t rd(std::uint32_t offset) {
        std::uint32_t v = 0;
        check(xfer(false, offset, v) == tlm::TLM_OK_RESPONSE, "Read OK");
        return v;
    }

    void wr(std::uint32_t offset, std::uint32_t v) {
        check(xfer(true, offset, v) == tlm::TLM_OK_RESPONSE, "Write OK");
    }

    void settle() { sc_core::wait(10, sc_core::SC_NS); }
    void zero_falls() { for (auto& f : falls) f = 0; }
    static std::string hex(std::uint32_t v) {
        char b[16];
        std::snprintf(b, sizeof b, "0x%04X", v);
        return b;
    }

    // Hệ thống và DBG nhả trước, Ibex sau thời gian ổn định, CVA6 vẫn giữ
    void check_startup(const std::string& w) {
        settle();
        check(sys_rst_n.read() && dbg_rst_n.read(), w + ": System and DBG released first");
        check(!ibex_rst_n.read() && !cva6_rst_n.read(), w + ": Both CPUs held during stabilization");
        sc_core::wait(kStartup);
        settle();
        check(ibex_rst_n.read(), w + ": CPU1 (Ibex) released after the delay");
        check(!cva6_rst_n.read(), w + ": CPU0 (CVA6) stays held for the Ibex handoff");
    }

    void check_reset_values(const std::string& w) {
        for (const auto& e : kExpect) {
            std::uint32_t expect = e.reset;
            if (e.offset == FU3_CRM_IBEX_SW_OFFSET || e.offset == FU3_CRM_SYSTEM_CLK_SW_OFFSET)
                expect = 1;   // Startup đã nhả
            if (e.offset == FU3_CRM_SW_RESET_REQ_OFFSET)
                expect = wdt.read() ? FU3_CRM_REQ_WATCHDOG : 0;   // Mức live
            check(rd(e.offset) == expect, w + ": value at " + hex(e.offset));
        }
        check(rd(0x004C) == 0 && rd(0x0050) == 0, w + ": Removed CPU_DEBUG / SW_RST_N_2 offsets read 0");
        check(gate0.read() == 0x1FF && gate1.read() == 0x1FF'FFFF, w + ": All gates on");
        check(rst0.read() == 0xFF && rst1.read() == 0x1FF'FFFF, w + ": All IP resets released");
        check(!irq.read(), w + ": IRQ low");
    }

    void test_power_on() {
        settle();
        check(!sys_rst_n.read() && !ibex_rst_n.read() && !cva6_rst_n.read() && !dbg_rst_n.read(),
              "PRSTN low: every reset held");
        std::uint32_t v = 1;
        check(xfer(true, FU3_CRM_CRM_OFFSET, v) == tlm::TLM_OK_RESPONSE, "Write in PRSTN OK");
        check(rd(FU3_CRM_CRM_OFFSET) == 0, "Write ignored in PRSTN");
        prst_n = true;
        check_startup("Power-on");
        check_reset_values("Power-on");
    }

    void test_registers() {
        for (const auto& e : kExpect) {
            if (e.offset == FU3_CRM_IBEX_SW_OFFSET || e.offset == FU3_CRM_FCLK_SW_OFFSET ||
                e.offset == FU3_CRM_SYSTEM_CLK_SW_OFFSET)
                continue;   // Test riêng: ghi các bit này nhả/giữ reset
            const std::uint32_t before = rd(e.offset);
            wr(e.offset, ~0u);
            check(rd(e.offset) == ((before & ~e.rw) | e.rw) || e.offset == FU3_CRM_PLL_OFFSET,
                  "Write ones at " + hex(e.offset));
            wr(e.offset, 0);
            check((rd(e.offset) & e.rw) == 0, "Write zeros at " + hex(e.offset));
            wr(e.offset, e.reset);
        }
        wr(0x004C, ~0u);
        check(rd(0x004C) == 0, "Undefined offset RAZ/WI");
    }

    void test_access() {
        std::uint32_t v = 0xDEAD'BEEF;
        check(xfer(false, 0x1000, v) == tlm::TLM_GENERIC_ERROR_RESPONSE && v == 0, "Outside 4 KiB decode: error, data 0");
        v = 0x1;
        check(xfer(true, 0x1000 + FU3_CRM_CRM_OFFSET, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "Write outside decode: error");
        check(rd(FU3_CRM_CRM_OFFSET) == 0, "Out-of-window write does not alias");
        const unsigned char strb[4] = {0xFF, 0, 0, 0};
        v = 0x0000'0155;
        check(xfer(true, FU3_CRM_CLK_OFFSET, v, 4, strb) == tlm::TLM_OK_RESPONSE, "Partial PSTRB OK");
        check(rd(FU3_CRM_CLK_OFFSET) == 0x155, "PSTRB ignored: full word written");
        wr(FU3_CRM_CLK_OFFSET, 0);
        v = 0x7;
        check(xfer(true, FU3_CRM_CRM_OFFSET, v, 4, nullptr, true) == tlm::TLM_OK_RESPONSE, "Debug write");
        v = 0;
        xfer(false, FU3_CRM_CRM_OFFSET, v, 4, nullptr, true);
        check(v == 1, "Debug read");
        wr(FU3_CRM_CRM_OFFSET, 0);
        if (base == 0) {   // Bus FU3 tự chặn các lỗi này trước khi tới CRM
            check(xfer(false, 0x2, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Misaligned");
            check(xfer(false, 0x0, v, 2) == tlm::TLM_BURST_ERROR_RESPONSE, "Halfword");
            check(xfer(false, 0x10000, v) == tlm::TLM_ADDRESS_ERROR_RESPONSE, "Outside 64 KiB");
        }
    }

    void test_clocks() {
        // Reset: PLL powered down, cả 2 bộ chọn OSC
        check(dut.pll_clock_hz() == 0, "PLL off after reset");
        check(dut.cpu_clock_hz() == FU3_CRM_OSC_HZ && dut.bus_clock_hz() == FU3_CRM_OSC_HZ, "CPU and bus on OSC");
        check(!(rd(FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK), "No lock while powered down");
        wr(FU3_CRM_PLL_CTRL_OFFSET, 0x5949 | FU3_CRM_PLL_LKEN);
        check(rd(FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK, "lken forces lock");
        wr(FU3_CRM_PLL_CTRL_OFFSET, 0x5949 & ~FU3_CRM_PLL_POWERDOWN);
        check(rd(FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK, "Lock after power-up");
        check(dut.pll_clock_hz() == 60'000'000, "Reset dividers: 20 MHz * 12 / 2 / 2 = 60 MHz");
        check(dut.cpu_clock_hz() == FU3_CRM_OSC_HZ, "PLL unused until both selectors pick it");
        wr(FU3_CRM_CLK_0_OFFSET, 1);
        check(dut.cpu_clock_hz() == FU3_CRM_OSC_HZ, "CLK_0 alone has no effect");
        wr(FU3_CRM_CLK_1_OFFSET, 1);
        check(dut.cpu_clock_hz() == 60'000'000 && dut.bus_clock_hz() == 60'000'000, "CPU and bus on PLL");
        wr(FU3_CRM_CLK_OFFSET, 3);
        check(dut.bus_clock_hz() == 10'000'000 && dut.cpu_clock_hz() == 60'000'000, "Divider 3: bus / 6, CPU undivided");
        wr(FU3_CRM_PLL_OFFSET, (2u << 8) | (20u << 2) | 0);
        check(dut.pll_clock_hz() == 200'000'000, "N 2, M 20, OD 1: 200 MHz");
        wr(FU3_CRM_PLL_CTRL_OFFSET, (0x5949 & ~FU3_CRM_PLL_POWERDOWN) | FU3_CRM_PLL_BYPASS);
        check(dut.pll_clock_hz() == 10'000'000, "Bypass: Fin / N");
        wr(FU3_CRM_PLL_CTRL_OFFSET, (0x5949 & ~FU3_CRM_PLL_POWERDOWN & ~FU3_CRM_PLL_OE));
        check(dut.pll_clock_hz() == 0, "oe = 0: no PLL output");
        wr(FU3_CRM_CLK_1_OFFSET, 0);
        check(dut.cpu_clock_hz() == FU3_CRM_OSC_HZ && dut.bus_clock_hz() == FU3_CRM_OSC_HZ, "Back to OSC");
        for (auto off : {FU3_CRM_PLL_OFFSET, FU3_CRM_PLL_CTRL_OFFSET, FU3_CRM_CLK_0_OFFSET, FU3_CRM_CLK_OFFSET})
            for (const auto& e : kExpect)
                if (e.offset == off) wr(off, e.reset);
    }

    void test_outputs() {
        wr(FU3_CRM_CLK_GATE_0_OFFSET, FU3_CRM_GATE0_DBG | FU3_CRM_GATE0_ROM);
        wr(FU3_CRM_CLK_GATE_1_OFFSET, FU3_CRM_PERI_MAILBOX_CVA6 | FU3_CRM_PERI_WDT1);
        wr(FU3_CRM_SW_RST_N_0_OFFSET, FU3_CRM_RST0_HYPERBUS);
        wr(FU3_CRM_SW_RST_N_1_OFFSET, FU3_CRM_PERI_AP_HBCTRL | FU3_CRM_PERI_PLL_CTRL);
        settle();
        check(gate0.read() == (FU3_CRM_GATE0_DBG | FU3_CRM_GATE0_ROM), "clk_gate_0 follows register");
        check(gate1.read() == (FU3_CRM_PERI_MAILBOX_CVA6 | FU3_CRM_PERI_WDT1), "clk_gate_1 follows register");
        check(rst0.read() == FU3_CRM_RST0_HYPERBUS, "sw_rst_n_0 follows register");
        check(rst1.read() == (FU3_CRM_PERI_AP_HBCTRL | FU3_CRM_PERI_PLL_CTRL), "sw_rst_n_1 follows register");
        wr(FU3_CRM_CLK_GATE_0_OFFSET, 0x1FF);
        wr(FU3_CRM_CLK_GATE_1_OFFSET, 0x1FF'FFFF);
        wr(FU3_CRM_SW_RST_N_0_OFFSET, 0xFF);
        wr(FU3_CRM_SW_RST_N_1_OFFSET, 0x1FF'FFFF);

        // Ibex nhả CVA6 bằng FCLK_SW 
        wr(FU3_CRM_FCLK_SW_OFFSET, 1);
        settle();
        check(cva6_rst_n.read(), "FCLK_SW = 1 releases CPU0 (CVA6)");
        wr(FU3_CRM_IBEX_SW_OFFSET, 0);
        settle();
        check(!ibex_rst_n.read() && cva6_rst_n.read(), "IBEX_SW = 0 holds only CPU1 (Ibex)");
        wr(FU3_CRM_IBEX_SW_OFFSET, 1);
        wr(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 0);
        settle();
        check(!sys_rst_n.read() && !ibex_rst_n.read() && !cva6_rst_n.read() && !dbg_rst_n.read(),
              "SYSTEM_CLK_SW = 0 holds system, both CPUs and DBG");
        wr(FU3_CRM_SYSTEM_CLK_SW_OFFSET, 1);
        settle();
        check(sys_rst_n.read() && ibex_rst_n.read() && cva6_rst_n.read() && dbg_rst_n.read(), "SYSTEM_CLK_SW = 1 restores");

        wr(FU3_CRM_CRM_OFFSET, 1);
        settle();
        check(irq.read(), "CRM.int = 1 raises irq");
        wr(FU3_CRM_CRM_OFFSET, 0);
        settle();
        check(!irq.read(), "CRM.int = 0 lowers irq");
    }

    // Reset hệ thống, boot lại (Ibex rồi CVA6 giữ), thanh ghi CRM giữ nguyên
    void system_reset(sc_core::sc_signal<bool>& req, const std::string& w, bool dbg_kept) {
        wr(FU3_CRM_FCLK_SW_OFFSET, 1);
        wr(FU3_CRM_CLK_OFFSET, 0x5);
        settle();
        zero_falls();
        req = true;
        settle();
        check(!sys_rst_n.read() && !ibex_rst_n.read() && !cva6_rst_n.read(), w + ": system and both CPUs held");
        check(dbg_rst_n.read() == dbg_kept, w + (dbg_kept ? ": DBG kept running" : ": DBG reset too"));
        check(rd(FU3_CRM_SW_RESET_REQ_OFFSET) != 0, w + ": request visible in SW_RESET_REQ");
        sc_core::wait(kHold);
        check_startup(w);
        check(rd(FU3_CRM_FCLK_SW_OFFSET) == 0, w + ": FCLK_SW cleared, Ibex must release CVA6 again");
        check(rd(FU3_CRM_CLK_OFFSET) == 0x5, w + ": CRM registers kept");
        check(falls[0] == 1 && falls[1] == 1 && falls[2] == 1 && falls[3] == (dbg_kept ? 0u : 1u), w + ": one pulse");
        req = false;
        settle();
        check(falls[0] == 1, w + ": falling request does nothing");
        wr(FU3_CRM_CLK_OFFSET, 0);
    }

    void test_requests() {
        system_reset(wdt, "Watchdog", false);
        system_reset(sys_req, "SYSCSR reset", false);
        system_reset(cpu_req, "Debug reset", true);

        // Debug + watchdog cùng lúc: một lần reset, DBG cũng reset
        zero_falls();
        cpu_req = true;
        wdt = true;
        settle();
        check(!dbg_rst_n.read(), "Debug + watchdog: DBG reset");
        sc_core::wait(kHold + kStartup);
        settle();
        check(falls[0] == 1 && ibex_rst_n.read(), "Simultaneous requests: one reset");
        cpu_req = false;
        wdt = false;
        settle();
    }

    void test_global() {
        wr(FU3_CRM_CLK_OFFSET, 0x5);
        wr(FU3_CRM_FCLK_SW_OFFSET, 1);
        global_req = true;
        settle();
        check(!sys_rst_n.read() && !ibex_rst_n.read() && !cva6_rst_n.read() && !dbg_rst_n.read(),
              "Global: every reset held");
        check(rd(FU3_CRM_CLK_OFFSET) == 0, "Global: registers back to reset values");
        wr(FU3_CRM_CLK_OFFSET, 0x5);
        check(rd(FU3_CRM_CLK_OFFSET) == 0, "Global: writes ignored");
        check(rd(FU3_CRM_SW_RESET_REQ_OFFSET) == FU3_CRM_REQ_GLOBAL, "Global visible in SW_RESET_REQ");
        sc_core::wait(5, sc_core::SC_US);
        check(!sys_rst_n.read(), "Global level keeps CRM in reset");
        global_req = false;
        check_startup("After global");
        check_reset_values("After global");
    }

    void test_prstn() {
        wdt = true;   // Mức giữ qua PRSTN không là sườn
        settle();
        sc_core::wait(kHold + kStartup);
        prst_n = false;
        settle();
        check(!sys_rst_n.read() && !ibex_rst_n.read() && !dbg_rst_n.read(), "PRSTN: every reset held");
        prst_n = true;
        check_startup("After PRSTN");
        zero_falls();
        sc_core::wait(kHold + kStartup);
        check(falls[0] == 0, "Request held across PRSTN is not an edge");
        wdt = false;
        settle();
    }

    void run() {
        test_power_on();
        test_registers();
        test_access();
        test_clocks();
        test_outputs();
        test_requests();
        test_global();
        test_prstn();
        sc_core::sc_stop();
    }
};
