// FU3 CPU wrapper reset behaviour with the FU3 option profiles (T04/T05 review
// R03, R05; started from plan/T04_T05_review_logs/repro):
//
//   test_fu3_cpu_reset cva6|ibex fetch|load|store|wfi|irq
//
// fetch/load/store/wfi: a reset pulse arrives while the CPU waits on the bus
// for that access (or sleeps in WFI). The instruction in flight must not commit
// into the state after reset: execution restarts at the reset vector, so its
// first instruction is fetched a second time.
// The program stores t1 before loading it, so a load that commits after the
// reset shows up as a non-zero t1 in the rerun.
// irq: interrupt inputs driven high across a reset pulse stay visible in mip
// after release (the reset clears the CSR, not the wire).

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <systemc>
#include <tlm_utils/simple_target_socket.h>

#include <fu3_cpu/cva6_config.h>
#include <fu3_cpu/ibex_config.h>

namespace {

namespace fu3 = cdc::cpu::fu3;

constexpr uint32_t kData = 0x0100'0000;  // DSRAM (Ibex PMP reset rule: R/W)

// lui t3,0x1000; sw t1,0x308(t3); addi t0,zero,0x55; lw t1,0x200(t3);
// sw t0,0x300(t3); csrr t2,mip; sw t2,0x304(t3); wfi; j .-4
constexpr uint32_t kProgram[] = {0x01000e37, 0x306e2423, 0x05500293, 0x200e2303, 0x305e2023,
                                 0x344023f3, 0x307e2223, 0x10500073, 0xffdff06f};

struct memory : sc_core::sc_module {
    tlm_utils::simple_target_socket<memory> socket{"socket"};
    uint32_t base;
    unsigned vector_fetches = 0, writes_300 = 0;
    uint32_t last_300 = 0, mip_seen = 0, last_t1 = 0xffffffff;
    sc_core::sc_event mip_stored;
    sc_core::sc_event fetch_second, load_started, store_started, wfi_fetched;

    memory(sc_core::sc_module_name n, uint32_t code_base) : sc_module(n), base(code_base)
    {
        socket.register_b_transport(this, &memory::access);
    }
    void access(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
    {
        wait(delay);
        delay = sc_core::SC_ZERO_TIME;
        const auto a = tx.get_address();
        uint32_t word = 0;
        if (tx.is_read() && a >= base && a < base + sizeof kProgram) {
            if (a == base)
                ++vector_fetches;
            if (a == base + 12)
                fetch_second.notify(sc_core::SC_ZERO_TIME);
            if (a == base + 28)
                wfi_fetched.notify(sc_core::SC_ZERO_TIME);
            word = kProgram[(a - base) / 4];
        } else if (tx.is_read() && a == kData + 0x200) {
            load_started.notify(sc_core::SC_ZERO_TIME);
            word = 0x1234;
        } else if (tx.is_write() && a == kData + 0x300) {
            store_started.notify(sc_core::SC_ZERO_TIME);
        }
        wait(10, sc_core::SC_NS);  // every access takes time: resets land inside it
        if (tx.is_read()) {
            std::memcpy(tx.get_data_ptr(), &word, 4);
        } else if (a == kData + 0x300) {
            ++writes_300;
            std::memcpy(&last_300, tx.get_data_ptr(), 4);
        } else if (a == kData + 0x304) {
            std::memcpy(&mip_seen, tx.get_data_ptr(), 4);
            mip_stored.notify(sc_core::SC_ZERO_TIME);
        } else if (a == kData + 0x308) {
            std::memcpy(&last_t1, tx.get_data_ptr(), 4);
        }
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct bench : sc_core::sc_module {
    bool ibex;
    std::string scenario;
    memory mem;
    cdc::cpu::cva6_wrapper cpu;
    uint32_t expected_mip = 0;

    SC_HAS_PROCESS(bench);
    bench(sc_core::sc_module_name n, bool is_ibex, std::string which)
        : sc_module(n), ibex(is_ibex), scenario(std::move(which)),
          mem("mem", is_ibex ? uint32_t(fu3::kIbexResetPc) : uint32_t(fu3::kCva6ResetPc)),
          cpu("cpu", is_ibex ? fu3::ibex_cpu_config() : fu3::cva6_cpu_config(),
              is_ibex ? fu3::kIbexDisableExtensions : fu3::kCva6DisableExtensions,
              is_ibex ? fu3::ibex_core_options() : fu3::cva6_core_options_fu3())
    {
        cpu.data_bus().bind(mem.socket);
        if (std::getenv("FU3_TRACE"))
            cpu.set_trace(true);
        SC_THREAD(run);
    }
    void pulse()
    {
        cpu.set_reset(true);
        wait(5, sc_core::SC_NS);
        cpu.set_reset(false);
    }
    void run()
    {
        if (scenario == "fetch")
            wait(mem.fetch_second);
        else if (scenario == "load")
            wait(mem.load_started);
        else if (scenario == "store")
            wait(mem.store_started);
        else if (scenario == "wfi") {
            wait(mem.wfi_fetched);
            wait(100, sc_core::SC_NS);  // asleep in WFI
        }
        if (scenario == "irq") {
            // First run completes with no interrupt; then the lines go high and
            // stay high across a reset pulse.
            wait(mem.mip_stored);
            for (unsigned cause : {fu3::kCauseSoftware, fu3::kCauseTimer, fu3::kCauseExternal})
                cpu.set_irq(cause, true);
            expected_mip = 0x888;
            if (ibex) {
                cpu.set_irq(fu3::kCauseFast0, true);
                cpu.set_irq(fu3::kCauseFast0 + 14, true);
                expected_mip |= (1u << 16) | (1u << 30);
            }
        }
        pulse();
        wait(2, sc_core::SC_US);
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv)
{
    const std::string core = argc > 1 ? argv[1] : "", scenario = argc > 2 ? argv[2] : "";
    if ((core != "cva6" && core != "ibex") ||
        (scenario != "fetch" && scenario != "load" && scenario != "store" && scenario != "wfi" && scenario != "irq")) {
        std::cerr << "usage: test_fu3_cpu_reset cva6|ibex fetch|load|store|wfi|irq\n";
        return 2;
    }
    bench b("bench", core == "ibex", scenario);
    sc_core::sc_start();

    // Every scenario reruns from the reset vector with clean registers.
    bool ok = b.mem.last_300 == 0x55 && b.mem.vector_fetches == 2 && b.mem.last_t1 == 0;
    if (scenario == "irq")
        ok = ok && b.mem.mip_seen == b.expected_mip;
    std::cout << "fu3 cpu reset " << core << '/' << scenario << ": reset-vector fetches " << b.mem.vector_fetches
              << ", stores of 0x55 " << b.mem.writes_300 << " (last 0x" << std::hex << b.mem.last_300
              << "), t1 at rerun 0x" << b.mem.last_t1 << ", mip 0x" << b.mem.mip_seen << std::dec << " -> " << (ok ? "PASS" : "FAIL") << '\n';
    return ok ? 0 : 1;
}
