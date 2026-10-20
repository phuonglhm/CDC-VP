#pragma once
// Test theo FU3 CRM Specification V1.0: khởi động, giá trị reset/mask, truy cập, clock bus, PLL lock,
// gate/reset output, CPU_DEBUG, 3 yêu cầu reset + ưu tiên, global reset, MRV bị gate, PRSTN, IRQ
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

// Giá trị reset và mask chép độc lập từ spec mục 3.8
struct Expect { std::uint32_t offset, reset, rw; };
constexpr Expect kExpect[] = {
    {0x0000, 0x0001'0000, 0x0},    
    {0x0010, 0x0000'0231, 0x3FFF}, 
    {0x0018, 0x0000'5949, 0x7FFF},
    {0x001C, 0x0, 0x1},            
    {0x0020, 0x0, 0x1},            
    {0x0024, 0x0, 0x3FF},
    {0x0028, 0x0000'1FFF, 0x1FFF}, 
    {0x002C, 0x0000'7FFF, 0x7FFF}, 
    {0x0030, 0x0, 0x1},
    {0x0034, 0x1, 0x1},            
    {0x0038, 0x1, 0x1},            
    {0x003C, 0x1, 0x1},
    {0x0040, 0x0000'00FF, 0xFF},   
    {0x0044, 0x0000'000F, 0xF},    
    {0x0048, 0x0000'3FFF, 0x3FFF},
    {0x004C, 0x0, 0x1},            
    {0x0050, 0x0, 0x1},            
    {0x0054, 0x0, 0x0},
    {0x0058, 0x0, 0x1},
};

// Độ dài trong bench: giữ reset 200 ns, chờ ổn định 1 us
const sc_core::sc_time kHold(200, sc_core::SC_NS), kStartup(1, sc_core::SC_US);

struct Bench : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    Crm dut{"dut", kHold, kStartup};
    sc_core::sc_signal<bool> prst_n{"prst_n", false};
    sc_core::sc_signal<bool> wdt{"wdt"}, cpu_req{"cpu_req"}, sys_req{"sys_req"}, global_req{"global_req"};
    sc_core::sc_signal<bool> cva6_rst_n{"cva6_rst_n"}, sys_rst_n{"sys_rst_n"}, dbg_rst_n{"dbg_rst_n"}, irq{"irq"};
    sc_core::sc_signal<std::uint32_t> gate0{"gate0"}, gate1{"gate1"}, rst0{"rst0"}, rst1{"rst1"}, rst2{"rst2"};
    unsigned falls[3] = {};   // Số sườn xuống của cva6, sys, dbg
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
        dut.cva6_rst_n(cva6_rst_n);
        dut.sys_rst_n(sys_rst_n);
        dut.dbg_rst_n(dbg_rst_n);
        dut.irq(irq);
        dut.clk_gate_0(gate0);
        dut.clk_gate_1(gate1);
        dut.sw_rst_n_0(rst0);
        dut.sw_rst_n_1(rst1);
        dut.sw_rst_n_2(rst2);

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
        sensitive << cva6_rst_n.negedge_event() << sys_rst_n.negedge_event() << dbg_rst_n.negedge_event();
        dont_initialize();
        SC_THREAD(run);
    }

    void count_falls() {
        if (cva6_rst_n.negedge()) 
            ++falls[0];
        if (sys_rst_n.negedge()) 
            ++falls[1];
        if (dbg_rst_n.negedge()) 
            ++falls[2];
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
    void wait_us(double us) { sc_core::wait(us, sc_core::SC_US); }
    void zero_falls() { for (auto& f : falls) f = 0; }
    static std::string hex(std::uint32_t v) {
        char b[16];
        std::snprintf(b, sizeof b, "0x%04X", v);
        return b;
    }

    // Từ PRSTN/global nhả: hệ thống nhả trước, CVA6 sau thời gian ổn định, DBG vẫn giữ
    void check_startup(const std::string& w) {
        settle();
        check(sys_rst_n.read(), w + ": System released first");
        check(!cva6_rst_n.read() && !dut.done(), w + ": CVA6 held, DONE low during stabilization");
        check(rd(FU3_CRM_FCLK_SW_OFFSET) == 0, w + ": FCLK_SW still 0");
        sc_core::wait(kStartup);
        settle();
        check(dut.done(), w + ": DONE set");
        check(cva6_rst_n.read(), w + ": CVA6 released after DONE");
        check(!dbg_rst_n.read(), w + ": Debug reset stays asserted (CPU_DEBUG = 0)");
        for (const auto& e : kExpect) {
            std::uint32_t expect = e.reset;
            if (e.offset == FU3_CRM_FCLK_SW_OFFSET || e.offset == FU3_CRM_SYSTEM_CLK_SW_OFFSET) 
                expect = 1;   // Firmware đã nhả
            if (e.offset == FU3_CRM_SW_RESET_REQ_OFFSET) 
                expect = wdt.read() ? FU3_CRM_REQ_WATCHDOG : 0;   // Mức live
            check(rd(e.offset) == expect, w + ": value at " + hex(e.offset));
        }
        check(gate0.read() == 0x1FFF && gate1.read() == 0x7FFF, w + ": All gates on");
        check(rst0.read() == 0xFF && rst1.read() == 0xF && rst2.read() == 0x3FFF, w + ": all IP resets released");
        check(!irq.read(), w + ": IRQ low");
        check(dut.bus_clock_hz() == FU3_CRM_OSC_HZ, w + ": Bus clock = OSC");
    }

    void test_power_on() {
        settle();
        check(!cva6_rst_n.read() && !sys_rst_n.read() && !dbg_rst_n.read(), "PRSTN low: every reset held");
        std::uint32_t v = 1;
        check(xfer(true, FU3_CRM_CLK_0_OFFSET, v) == tlm::TLM_OK_RESPONSE, "Write during PRSTN OK");
        prst_n = true;
        check_startup("power-on");
        check(rd(FU3_CRM_CLK_0_OFFSET) == 0, "Write during PRSTN ignored");
    }

    void test_masks() {
        for (const auto& e : kExpect) {
            if (e.offset == FU3_CRM_FCLK_SW_OFFSET || e.offset == FU3_CRM_SYSTEM_CLK_SW_OFFSET || e.offset == FU3_CRM_CLK_GATE_0_OFFSET)
                    continue;   // test riêng
            const std::uint32_t keep = rd(e.offset);
            wr(e.offset, 0xFFFF'FFFF);
            std::uint32_t v = rd(e.offset);
            if (e.offset == FU3_CRM_PLL_OFFSET) 
                v &= ~FU3_CRM_PLL_LOCK;
            check(v == ((e.reset & ~e.rw) | e.rw), "All-ones write masks at " + hex(e.offset));
            wr(e.offset, 0);
            v = rd(e.offset);
            if (e.offset == FU3_CRM_PLL_OFFSET) 
                v &= ~FU3_CRM_PLL_LOCK;
            check(v == (e.reset & ~e.rw), "All-zeros write masks at " + hex(e.offset));
            wr(e.offset, keep);
        }
        wr(FU3_CRM_CLK_GATE_0_OFFSET, 0xFFFF'FFFF);
        check(rd(FU3_CRM_CLK_GATE_0_OFFSET) == 0x1FFF, "CLK_GATE_0 masks to [12:0]");
    }

    void test_access() {
        std::uint32_t v = 0x1234;
        check(xfer(false, 0x0FFC, v) == tlm::TLM_OK_RESPONSE && v == 0, "Undefined in-window offset reads 0");
        v = 0xFFFF'FFFF;
        check(xfer(true, 0x0FFC, v) == tlm::TLM_OK_RESPONSE, "Undefined in-window write OK");
        v = 0x1234;
        check(xfer(false, 0x1000, v) == tlm::TLM_GENERIC_ERROR_RESPONSE && v == 0, "Outside the 4 KiB decode: error, data 0");
        v = 0;
        check(xfer(true, 0x1000, v) == tlm::TLM_GENERIC_ERROR_RESPONSE, "Write outside the decode window: error");
        v = 0;
        check(xfer(true, FU3_CRM_CLK_GATE_1_OFFSET, v, 1) == tlm::TLM_BURST_ERROR_RESPONSE, "Byte access refused");
        check(rd(FU3_CRM_CLK_GATE_1_OFFSET) == 0x7FFF, "Refused byte access changes nothing");

        // PSTRB bị bỏ qua: chỉ bật lane 0 vẫn ghi cả word
        const unsigned char be[4] = {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED, TLM_BYTE_DISABLED, TLM_BYTE_DISABLED};
        v = 0x0000'00FF;
        check(xfer(true, FU3_CRM_CLK_GATE_1_OFFSET, v, 4, be) == tlm::TLM_OK_RESPONSE, "Partial PSTRB write OK");
        check(rd(FU3_CRM_CLK_GATE_1_OFFSET) == 0x00FF, "PSTRB ignored: full word written");
        v = 0x7FFF;
        xfer(true, FU3_CRM_CLK_GATE_1_OFFSET, v, 4, nullptr, true);
        check(rd(FU3_CRM_CLK_GATE_1_OFFSET) == 0x7FFF, "Debug write");
        v = 0;
        xfer(false, FU3_CRM_VERSION_OFFSET, v, 4, nullptr, true);
        check(v == 0x0001'0000u, "Debug read");
    }

    void test_clock() {
        wr(FU3_CRM_CLK_OFFSET, 5);
        check(dut.bus_clock_hz() == FU3_CRM_OSC_HZ, "DIV set, both selectors on OSC: undivided");
        wr(FU3_CRM_CLK_0_OFFSET, 1);
        check(dut.bus_clock_hz() == FU3_CRM_OSC_HZ, "Branch selected but final selector on OSC: undivided");
        wr(FU3_CRM_CLK_1_OFFSET, 1);
        check(dut.bus_clock_hz() == 2'000'000u, "Both selectors on the branch, DIV 5: 2 MHz");
        wr(FU3_CRM_CLK_OFFSET, 1);
        check(dut.bus_clock_hz() == 10'000'000u, "DIV 1: 10 MHz");
        wr(FU3_CRM_CLK_OFFSET, 2);
        check(dut.bus_clock_hz() == 5'000'000u, "DIV 2: 5 MHz");
        wr(FU3_CRM_CLK_OFFSET, 0);
        check(dut.bus_clock_hz() == FU3_CRM_OSC_HZ, "DIV 0: bypass");
        wr(FU3_CRM_CLK_OFFSET, 2);
        wr(FU3_CRM_PLL_CTRL_OFFSET, 0x5949 & ~FU3_CRM_PLL_POWERDOWN);
        wr(FU3_CRM_PLL_OFFSET, (5u << 8) | (8u << 2));
        check(dut.bus_clock_hz() == 5'000'000u, "PLL registers do not change the clock (no PLL in the wrapper)");
        wr(FU3_CRM_CLK_1_OFFSET, 0);
        check(dut.bus_clock_hz() == FU3_CRM_OSC_HZ, "Final selector back to OSC");
        wr(FU3_CRM_CLK_0_OFFSET, 0);
        wr(FU3_CRM_CLK_OFFSET, 0);
        wr(FU3_CRM_PLL_OFFSET, 0x231);
        wr(FU3_CRM_PLL_CTRL_OFFSET, 0x5949);
    }

    void test_pll_lock() {
        check(!(rd(FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK), "Powered down: not locked");
        wr(FU3_CRM_PLL_CTRL_OFFSET, 0x5949 & ~FU3_CRM_PLL_POWERDOWN);
        check(rd(FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK, "Counter lock (lock_type 1) when powered up");
        wr(FU3_CRM_PLL_CTRL_OFFSET, 0x5949 & ~FU3_CRM_PLL_POWERDOWN & ~FU3_CRM_PLL_LOCK_TYPE);
        check(!(rd(FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK), "PLL LOCK source: no PLL, not locked");
        wr(FU3_CRM_PLL_CTRL_OFFSET, 0x5949 | FU3_CRM_PLL_LKEN);
        check(rd(FU3_CRM_PLL_OFFSET) & FU3_CRM_PLL_LOCK, "LKEN forces lock high");
        wr(FU3_CRM_PLL_CTRL_OFFSET, 0x5949);
    }

    void test_outputs() {
        wr(FU3_CRM_CLK_GATE_1_OFFSET, 0x7FFF & ~FU3_CRM_GATE1_UART2);
        wr(FU3_CRM_CLK_GATE_0_OFFSET, 0x1FFF & ~FU3_CRM_GATE0_AES_256);
        wr(FU3_CRM_SW_RST_N_0_OFFSET, 0xFF & ~FU3_CRM_RST0_QSPI);
        wr(FU3_CRM_SW_RST_N_1_OFFSET, 0xF & ~FU3_CRM_RST1_TIMER2);
        wr(FU3_CRM_SW_RST_N_2_OFFSET, 0x3FFF & ~FU3_CRM_RST2_PLIC);
        settle();
        check(gate1.read() == (0x7FFFu & ~FU3_CRM_GATE1_UART2), "CLK_GATE_1 output: UART2 off");
        check(gate0.read() == (0x1FFFu & ~FU3_CRM_GATE0_AES_256), "CLK_GATE_0 output: AES off");
        check(rst0.read() == (0xFFu & ~FU3_CRM_RST0_QSPI), "SW_RST_N_0 output: QSPI held");
        check(rst1.read() == (0xFu & ~FU3_CRM_RST1_TIMER2), "SW_RST_N_1 output: TIMER2 held");
        check(rst2.read() == (0x3FFFu & ~FU3_CRM_RST2_PLIC), "SW_RST_N_2 output: PLIC held");
        wr(FU3_CRM_CPU_DEBUG_OFFSET, 1);
        settle();
        check(dbg_rst_n.read(), "CPU_DEBUG = 1 releases the debug subsystem");
        wr(FU3_CRM_FCLK_SW_OFFSET, 0);
        settle();
        check(!cva6_rst_n.read(), "SOftware FCLK_SW = 0 holds CVA6");
        wr(FU3_CRM_FCLK_SW_OFFSET, 1);
        settle();
        check(cva6_rst_n.read(), "Software FCLK_SW = 1 releases CVA6");
        wr(FU3_CRM_CRM_OFFSET, 1);
        settle();
        check(irq.read(), "CRM.int = 1 raises irq");
        wr(FU3_CRM_CRM_OFFSET, 0);
        settle();
        check(!irq.read(), "CRM.int = 0 drops irq");
    }

    // Watchdog và yêu cầu reset CPU: giữ hệ thống + CVA6, nhả hệ thống trước, DONE, rồi nhả CVA6
    void recovery(sc_core::sc_signal<bool>& req, std::uint32_t status, const std::string& w) {
        zero_falls();
        req = true;
        settle();
        check(rd(FU3_CRM_SW_RESET_REQ_OFFSET) == status, w + ": SW_RESET_REQ shows the live level");
        check(!sys_rst_n.read() && !cva6_rst_n.read(), w + ": System and CVA6 held");
        sc_core::wait(kHold);
        settle();
        check(sys_rst_n.read() && !cva6_rst_n.read(), w + ": sSystem released before CVA6");
        sc_core::wait(kStartup);
        settle();
        check(cva6_rst_n.read() && dut.done(), w + ": CVA6 released, DONE");
        check(falls[0] == 1 && falls[1] == 1 && falls[2] == 0, w + ": 1 pulse each on CVA6 and system, debug untouched");
        check(gate1.read() == (0x7FFFu & ~FU3_CRM_GATE1_UART2), w + ": Gates not rewritten");
        zero_falls();
        wait_us(2);
        check(falls[0] == 0 && falls[1] == 0, w + ": Held level does not repeat");
        req = false;
        settle();
        check(rd(FU3_CRM_SW_RESET_REQ_OFFSET) == 0, w + ": Status clears with the level");
    }

    void test_requests() {
        recovery(wdt, FU3_CRM_REQ_WATCHDOG, "Watchdog");
        recovery(cpu_req, FU3_CRM_REQ_SYSRESETREQ, "CPU reset request");

        zero_falls();
        sys_req = true;
        settle();
        check(!sys_rst_n.read() && cva6_rst_n.read(), "System request: only the system domain held");
        sc_core::wait(kHold);
        settle();
        check(sys_rst_n.read(), "System request: released after the hold");
        check(falls[0] == 0 && falls[1] == 1 && falls[2] == 0, "System request: CVA6 and debug untouched");
        sys_req = false;
        settle();

        // watchdog và system cùng lúc -> chỉ xử lý watchdog (có CVA6)
        zero_falls();
        sys_req = true;
        wdt = true;
        wait_us(3);
        check(falls[0] == 1 && falls[1] == 1, "Watchdog wins over system: one recovery with CVA6");
        wdt = false;
        sys_req = false;
        settle();
    }

    void test_mrv_gate() {
        wr(FU3_CRM_CLK_GATE_0_OFFSET, 0x1FFF & ~FU3_CRM_GATE0_MRV);
        zero_falls();
        wdt = true;
        wait_us(2);
        check(falls[0] == 0 && falls[1] == 0, "MRV gated: request not serviced");
        wr(FU3_CRM_CLK_GATE_0_OFFSET, 0x1FFF);
        wait_us(2);
        check(falls[0] == 1 && falls[1] == 1, "MRV re-enabled: pending request serviced");
        wdt = false;
        settle();

        wr(FU3_CRM_CLK_GATE_0_OFFSET, 0x1FFF & ~FU3_CRM_GATE0_MRV);
        zero_falls();
        wdt = true;
        settle();
        wdt = false;
        settle();
        wr(FU3_CRM_CLK_GATE_0_OFFSET, 0x1FFF);
        wait_us(2);
        check(falls[0] == 0 && falls[1] == 0, "Request dropped before service: nothing done (live status)");
    }

    void test_global() {
        wr(FU3_CRM_CPU_DEBUG_OFFSET, 1);
        zero_falls();
        global_req = true;
        settle();
        check(!cva6_rst_n.read() && !sys_rst_n.read() && !dbg_rst_n.read(), "Global: every reset held");
        check(!dut.done(), "Global: DONE cleared");
        std::uint32_t v = 1;
        check(xfer(true, FU3_CRM_CLK_0_OFFSET, v) == tlm::TLM_OK_RESPONSE, "Write during global reset OK");
        wait_us(2);
        check(!sys_rst_n.read(), "Global: held while the request stays high");
        global_req = false;
        check_startup("After global");
    }

    void test_prst() {
        wr(FU3_CRM_CLK_GATE_1_OFFSET, 0);
        prst_n = false;
        settle();
        check(!cva6_rst_n.read() && !sys_rst_n.read(), "PRSTN: resets held");
        zero_falls();
        wdt = true;
        settle();
        prst_n = true;
        check_startup("After PRSTN");
        wait_us(2);
        check(falls[0] == 0 && falls[1] == 0, "Request held across PRSTN is not an edge");
        wdt = false;
        settle();
    }

    void run() {
        test_power_on();
        test_masks();
        test_access();
        test_clock();
        test_pll_lock();
        test_outputs();
        test_requests();
        test_mrv_gate();
        test_global();
        test_prst();
        sc_core::sc_stop();
    }
};
