// FU3 CPU wrapper Debug Mode control (T06), the interface the Debug Module
// (fu3_dbg::DebugHart, T18) and a GDB stub use:
//
//   test_fu3_cpu_debug cva6|ibex
//
// The CPU counts in t0 in a two-instruction loop. Its debug code (at the v2.1
// halt address) reads the Debug Module aperture into s2 and parks; the debug
// exception entry counts in s3. The bench checks halt request, abstract
// register access, resume, single step, the PMP bypass of the DM aperture and
// Debug Mode across reset.

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

constexpr uint32_t kDmBase = 0x1A11'0000;
constexpr uint32_t kDmWord = 0xd00d;
constexpr uint32_t kLoop = 0x10;  // offset of the counting loop (addi, j) from the reset PC

// Ibex: pmpcfg1 with entry 4 (IBEX_ISRAM up to 0x0006_1000) L | TOR | R | X
// instead of the reset R | W, so the debug code can be fetched (RLB = 1).
// li t0,0x9b9d9b8d; csrw pmpcfg1,t0; li t0,0; addi t0,t0,1; j .-4
constexpr uint32_t kIbexProgram[] = {0x9b9da2b7, 0xb8d28293, 0x3a129073, 0x00000293, 0x00128293, 0xffdff06f};
// CVA6: nop; nop; nop; li t0,0; addi t0,t0,1; j .-4
constexpr uint32_t kCva6Program[] = {0x00000013, 0x00000013, 0x00000013, 0x00000293, 0x00128293, 0xffdff06f};
// +0: j +0x10; nop; +8 (exception entry): j +0x20; nop;
// +0x10: lui s1,0x1a110; lw s2,0(s1); j .; nop; +0x20: addi s3,s3,1; j .
constexpr uint32_t kDebugCode[] = {0x0100006f, 0x00000013, 0x0180006f, 0x00000013, 0x1a1104b7,
                                   0x0004a903, 0x0000006f, 0x00000013, 0x00198993, 0x0000006f};

constexpr uint16_t kDcsr = 0x7b0, kDpc = 0x7b1, kDscratch0 = 0x7b2;
constexpr uint16_t kT0 = 0x1000 + 5, kS2 = 0x1000 + 18, kS3 = 0x1000 + 19;

struct memory : sc_core::sc_module {
    tlm_utils::simple_target_socket<memory> socket{"socket"};
    uint32_t code_base, debug_base;
    const uint32_t* program;
    unsigned dm_reads = 0;

    memory(sc_core::sc_module_name n, uint32_t code, uint32_t debug, const uint32_t* prog)
        : sc_module(n), code_base(code), debug_base(debug), program(prog)
    {
        socket.register_b_transport(this, &memory::access);
    }
    void access(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
    {
        wait(delay);
        delay = sc_core::SC_ZERO_TIME;
        const uint32_t a = static_cast<uint32_t>(tx.get_address());
        uint32_t word = 0;
        if (a >= code_base && a < code_base + sizeof kIbexProgram)
            word = program[(a - code_base) / 4];
        else if (a >= debug_base && a < debug_base + sizeof kDebugCode)
            word = kDebugCode[(a - debug_base) / 4];
        else if (a == kDmBase && tx.is_read()) {
            ++dm_reads;
            word = kDmWord;
        }
        wait(10, sc_core::SC_NS);
        if (tx.is_read())
            std::memcpy(tx.get_data_ptr(), &word, 4);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct bench : sc_core::sc_module {
    bool ibex;
    uint32_t reset_pc;
    memory mem;
    cdc::cpu::cva6_wrapper cpu;
    unsigned failures = 0;

    SC_HAS_PROCESS(bench);
    bench(sc_core::sc_module_name n, bool is_ibex)
        : sc_module(n), ibex(is_ibex),
          reset_pc(is_ibex ? uint32_t(fu3::kIbexResetPc) : uint32_t(fu3::kCva6ResetPc)),
          mem("mem", reset_pc, is_ibex ? uint32_t(fu3::kIbexDmHaltAddr) : uint32_t(fu3::kCva6HaltAddress),
              is_ibex ? kIbexProgram : kCva6Program),
          cpu("cpu", is_ibex ? fu3::ibex_cpu_config() : fu3::cva6_cpu_config(),
              is_ibex ? fu3::kIbexDisableExtensions : fu3::kCva6DisableExtensions,
              is_ibex ? fu3::ibex_core_options() : fu3::cva6_core_options_fu3())
    {
        cpu.data_bus().bind(mem.socket);
        if (std::getenv("FU3_TRACE"))
            cpu.set_trace(true);
        SC_THREAD(run);
    }

    void check(bool ok, const char* what, uint32_t observed)
    {
        if (!ok) {
            ++failures;
            std::cerr << "FAIL: " << what << " (observed 0x" << std::hex << observed << std::dec << ")\n";
        }
    }
    uint32_t reg(uint16_t regno)
    {
        uint32_t v = 0xdeadbeef;
        check(cpu.debug_access_register(regno, false, v), "register read while halted", regno);
        return v;
    }
    bool write_reg(uint16_t regno, uint32_t v) { return cpu.debug_access_register(regno, true, v); }
    bool wait_halted(bool want)
    {
        for (unsigned i = 0; i < 400 && cpu.debug_halted() != want; ++i)
            wait(50, sc_core::SC_NS);
        return cpu.debug_halted() == want;
    }
    void halt()
    {
        cpu.set_debug_request(true);
        check(wait_halted(true), "halt request halts the hart", 0);
        cpu.set_debug_request(false);
    }
    void resume()
    {
        cpu.debug_resume();
        // The DRET is applied by the CPU thread later; from now on the DM no
        // longer sees a halted hart and gets no register access.
        uint32_t v = 0;
        check(!cpu.debug_halted() && !cpu.debug_access_register(kT0, false, v), "not halted once resumed", v);
        check(wait_halted(false), "resume leaves Debug Mode", 0);
    }
    void step()
    {
        const uint64_t entries = cpu.debug_entries();
        cpu.debug_resume();
        for (unsigned i = 0; i < 400 && cpu.debug_entries() == entries; ++i)
            wait(50, sc_core::SC_NS);
        check(cpu.debug_entries() == entries + 1 && cpu.debug_halted(), "a step halts again once", 0);
        check(((reg(kDcsr) >> 6) & 7) == 4, "dcsr.cause = step", reg(kDcsr));
    }
    void pulse()
    {
        cpu.set_reset(true);
        wait(5, sc_core::SC_NS);
        cpu.set_reset(false);
    }
    bool in_loop(uint32_t pc) const { return pc == reset_pc + kLoop || pc == reset_pc + kLoop + 4; }

    void run()
    {
        wait(2, sc_core::SC_US);
        uint32_t v = 0;
        check(!cpu.debug_halted() && !cpu.debug_access_register(kT0, false, v),
              "no register access while running", v);

        // Halt request from M-mode, at an instruction of the loop.
        halt();
        const uint32_t dcsr = reg(kDcsr);
        check(cpu.debug_entries() == 1 && ((dcsr >> 6) & 7) == 3 && (dcsr & 3) == 3, "dcsr: haltreq from M", dcsr);
        check(in_loop(reg(kDpc)), "dpc in the loop", reg(kDpc));
        check(reg(kT0) > 0, "t0 counted before the halt", reg(kT0));

        // The debug code ran: the DM aperture read passes PMP in Debug Mode
        // (Ibex: MMWP denies it outside Debug Mode, no rule covers 0x1A11_0000).
        wait(1, sc_core::SC_US);
        check(reg(kS2) == kDmWord && mem.dm_reads == 1, "debug code read the DM aperture", reg(kS2));
        check(reg(kS3) == 0, "no exception in Debug Mode", reg(kS3));

        // Abstract register access: GPR, x0, CSR, FPR (CVA6 only), unknown.
        check(write_reg(kT0, 0x1000) && reg(kT0) == 0x1000, "GPR write", reg(kT0));
        check(write_reg(0x1000, 5) && reg(0x1000) == 0, "x0 stays 0", reg(0x1000));
        check(write_reg(kDscratch0, 0x5a5a) && reg(kDscratch0) == 0x5a5a, "dscratch0 write", reg(kDscratch0));
        uint32_t f1 = 0x3f800000;
        const bool fpr = cpu.debug_access_register(0x1021, true, f1);
        check(fpr == !ibex, "FPR access only with F", fpr);
        if (fpr)
            check(reg(0x1021) == 0x3f800000, "FPR write", reg(0x1021));
        check(!cpu.debug_access_register(0x2000, false, v), "unknown regno refused", 0x2000);

        // Resume at dpc with the written t0; halt again.
        resume();
        wait(1, sc_core::SC_US);
        halt();
        check(reg(kT0) > 0x1000, "resume continues from the written t0", reg(kT0));
        check(cpu.debug_entries() == 2, "two entries", static_cast<uint32_t>(cpu.debug_entries()));

        // dcsr.step: each resume runs one instruction; two steps go once round
        // the loop (one addi).
        const uint32_t dpc0 = reg(kDpc), t0 = reg(kT0);
        check(write_reg(kDcsr, reg(kDcsr) | 4), "dcsr.step write", reg(kDcsr));
        step();
        check(in_loop(reg(kDpc)) && reg(kDpc) != dpc0, "one step moves dpc", reg(kDpc));
        step();
        check(reg(kDpc) == dpc0 && reg(kT0) == t0 + 1, "two steps: one addi", reg(kT0) - t0);
        check(write_reg(kDcsr, reg(kDcsr) & ~4u), "dcsr.step clear", reg(kDcsr));
        resume();
        wait(1, sc_core::SC_US);
        check(!cpu.debug_halted(), "runs after step cleared", 0);

        // Reset while halted, request released: the hart leaves Debug Mode
        // and starts at the reset vector.
        halt();
        pulse();
        check(wait_halted(false), "reset leaves Debug Mode", 0);
        wait(1, sc_core::SC_US);
        halt();
        check(in_loop(reg(kDpc)), "runs from reset after a reset in Debug Mode", reg(kDpc));
        check(((reg(kDcsr) >> 2) & 1) == 0, "reset clears dcsr.step", reg(kDcsr));
        resume();

        // Reset with the request held (debug_req_i is a level): Debug Mode
        // before the first instruction, dpc = reset PC.
        const unsigned reads = mem.dm_reads;
        cpu.set_debug_request(true);
        check(wait_halted(true), "halted before reset", 0);
        pulse();
        wait(1, sc_core::SC_US);
        check(cpu.debug_halted(), "request held across reset halts at once", 0);
        check(reg(kDpc) == reset_pc, "dpc = reset PC", reg(kDpc));
        check(((reg(kDcsr) >> 6) & 7) == 3, "dcsr.cause = haltreq after reset", reg(kDcsr));
        if (ibex) {
            // The reset PMP table (VP proposal, Q01-d) gives IBEX_ISRAM, and so
            // the debug code at 0x0006_0000, R | W only, locked, under MMWP:
            // before the ROM opens it, the debug code cannot be fetched and the
            // hart loops between the debug entries. Open item, see T06 report.
            check(mem.dm_reads == reads + 1, "Ibex: debug code not fetchable under the reset PMP", mem.dm_reads);
        } else {
            check(mem.dm_reads == reads + 2, "CVA6: debug code runs after reset", mem.dm_reads);
        }
        cpu.set_debug_request(false);
        pulse();
        wait(2, sc_core::SC_US);
        check(!cpu.debug_halted(), "released request: runs after reset", 0);

        std::cout << "fu3 cpu debug " << (ibex ? "ibex" : "cva6") << ": " << cpu.debug_entries() << " entries, "
                  << failures << " failures -> " << (failures ? "FAIL" : "PASS") << std::endl;
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int argc, char** argv)
{
    if (argc != 2 || (std::string(argv[1]) != "cva6" && std::string(argv[1]) != "ibex")) {
        std::cerr << "usage: " << argv[0] << " cva6|ibex\n";
        return 2;
    }
    sc_core::sc_report_handler::set_actions(sc_core::SC_INFO, sc_core::SC_DO_NOTHING);
    bench b("bench", std::string(argv[1]) == "ibex");
    sc_core::sc_start(100, sc_core::SC_US);
    return b.failures ? 1 : 0;
}
