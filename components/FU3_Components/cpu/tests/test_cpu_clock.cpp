// FU3 CPU wrapper clock (T07, decision Q01-g): the core runs on the clock
// period the platform sets from the CRM (FCLK) and changes it at an
// instruction boundary.
//
//   test_fu3_cpu_clock
//
// CVA6 profile with a zero-latency memory, so one loop instruction takes one
// clock period. The loop and the debug code are single-cycle instructions,
// so mcycle equals minstret at any time, across clock changes too. With a
// memory latency, an instruction takes one period plus its fetch latency,
// counted once (Q01-g: no wrapper delay on top of the ISS time).

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <systemc>
#include <tlm_utils/simple_target_socket.h>

#include <fu3_cpu/cva6_config.h>

namespace {

namespace fu3 = cdc::cpu::fu3;

// addi t0,t0,1; j .-4
constexpr uint32_t kLoop[] = {0x00128293, 0xffdff06f};
constexpr uint32_t kParkHere = 0x0000006f;  // j . (debug code at the HaltAddress)

struct memory : sc_core::sc_module {
    tlm_utils::simple_target_socket<memory> socket{"socket"};
    sc_core::sc_time latency = sc_core::SC_ZERO_TIME;   // annotated, loosely timed
    explicit memory(sc_core::sc_module_name n) : sc_module(n)
    {
        socket.register_b_transport(this, &memory::access);
    }
    void access(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
    {
        delay += latency;
        const auto a = tx.get_address();
        uint32_t word = 0;
        if (a >= fu3::kCva6ResetPc && a < fu3::kCva6ResetPc + sizeof kLoop)
            word = kLoop[(a - fu3::kCva6ResetPc) / 4];
        else if (a == fu3::kCva6HaltAddress)
            word = kParkHere;
        if (tx.is_read())
            std::memcpy(tx.get_data_ptr(), &word, 4);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct bench : sc_core::sc_module {
    memory mem{"mem"};
    cdc::cpu::cva6_wrapper cpu{"cpu", fu3::cva6_cpu_config(), fu3::kCva6DisableExtensions,
                               fu3::cva6_core_options_fu3()};
    unsigned failures = 0;

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name n) : sc_module(n)
    {
        cpu.data_bus().bind(mem.socket);
        cpu.set_reset(true);
        SC_THREAD(run);
    }
    void check(bool ok, const char* what, double observed)
    {
        if (!ok) {
            ++failures;
            std::cerr << "FAIL: " << what << " (observed " << observed << ")\n";
        }
    }
    // Instructions retired per period over `window`, within one quantum of skew.
    void expect_rate(sc_core::sc_time period, const char* what)
    {
        const sc_core::sc_time window(100, sc_core::SC_US);
        const auto before = cpu.get_instret();
        wait(window);
        const double executed = static_cast<double>(cpu.get_instret() - before);
        const double expected = window / period;
        const double slack = tlm::tlm_global_quantum::instance().get() / period + 2;
        check(executed > expected - slack && executed < expected + slack, what, executed);
    }
    uint32_t csr(uint16_t regno)
    {
        uint32_t v = 0;
        check(cpu.debug_access_register(regno, false, v), "CSR read while halted", regno);
        return v;
    }
    void counters_agree(const char* what)
    {
        cpu.set_debug_request(true);
        for (unsigned i = 0; i < 100 && !cpu.debug_halted(); ++i)
            wait(100, sc_core::SC_NS);
        cpu.set_debug_request(false);
        const uint32_t mcycle = csr(0xb00), minstret = csr(0xb02);
        check(mcycle == minstret && minstret > 0, what, static_cast<double>(mcycle) - minstret);
        cpu.debug_resume();
        wait(1, sc_core::SC_US);
    }

    void run()
    {
        const sc_core::sc_time osc(50, sc_core::SC_NS), pll(2.5, sc_core::SC_NS);
        cpu.set_clock_period(osc);   // CRM reset: 20 MHz OSC
        cpu.set_reset(false);
        wait(1, sc_core::SC_US);
        expect_rate(osc, "one instruction per 50 ns period");
        counters_agree("mcycle = minstret at 20 MHz");

        cpu.set_clock_period(pll);   // PLL selected: 400 MHz
        expect_rate(pll, "one instruction per 2.5 ns period after the change");
        counters_agree("mcycle = minstret after 20 -> 400 MHz");

        cpu.set_clock_period(sc_core::SC_ZERO_TIME);   // clock stopped
        wait(2, sc_core::SC_US);   // the instruction in flight finishes within a quantum
        const auto stopped = cpu.get_instret();
        wait(50, sc_core::SC_US);
        check(cpu.get_instret() == stopped, "no instruction while the clock is stopped",
              static_cast<double>(cpu.get_instret() - stopped));
        check(cpu.clock_period() == sc_core::SC_ZERO_TIME, "clock_period() reports the stop", 0);

        cpu.set_clock_period(osc);
        expect_rate(osc, "runs again at 50 ns after the restart");
        counters_agree("mcycle = minstret after stop and restart");

        mem.latency = sc_core::sc_time(10, sc_core::SC_NS);
        wait(1, sc_core::SC_US);
        expect_rate(osc + mem.latency, "50 ns period + 10 ns fetch latency, counted once");
        counters_agree("bus latency does not count as cycles");

        std::cout << "fu3 cpu clock: " << cpu.get_instret() << " instructions, " << failures << " failures -> "
                  << (failures ? "FAIL" : "PASS") << std::endl;
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions(sc_core::SC_INFO, sc_core::SC_DO_NOTHING);
    bench b("bench");
    if (std::getenv("FU3_TRACE"))
        b.cpu.set_trace(true);
    sc_core::sc_start(10, sc_core::SC_MS);
    return b.failures ? 1 : 0;
}
