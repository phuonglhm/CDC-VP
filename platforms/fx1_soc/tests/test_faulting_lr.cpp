// Plan C7 (review G4b-R1): an LR whose load faults reserves nothing.
//
// A real VP++ hart with the real fx1::ExclusiveMonitor, through the FX1 CPU
// port, on a RAM that answers the *first* read of 0x4000 with a slave error.
// The guest:
//
//     li t6, 0x40 ; csrw mtvec, t6 ; lui t0, 0x4 ; li t1, 0x123
//     lr.w t2, (t0)        # faults: mcause 5, the handler skips it
//     sc.w t3, t1, (t0)    # must fail (t3 = 1) and not write
//     lr.w t2, (t0)        # completes: reads the untouched 0x11111111
//     sc.w t4, t1, (t0)    # must succeed (t4 = 0) and write 0x123
//     halt: j halt
//   0x40 handler: csrr s1, mcause ; addi s0, s0, 1 ; mepc += 4 ; mret
//
// Assembled with riscv-none-elf-as -march=rv32ima_zicsr and pasted.
//
//   test_fx1_faulting_lr      Exit 0 when every check holds, 1 otherwise.

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <fx1/cpu_port_adapter.h>
#include <fx1/exclusive_monitor.h>
#include <riscv_vp_plusplus_wrapper.h>

namespace {

int failures = 0;
#define CHECK(cond, what)                                                       \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::cerr << "CHECK failed: " << (what) << " @ line " << __LINE__ << '\n'; \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

constexpr std::uint64_t kWord = 0x4000;
constexpr std::uint64_t kHaltPc = 0x20;
constexpr std::uint32_t kProgram[] = {
    0x04000F93u, 0x305F9073u, 0x000042B7u, 0x12300313u, 0x1002A3AFu, 0x1862AE2Fu,
    0x1002A3AFu, 0x1862AEAFu, 0x0000006Fu, 0, 0, 0, 0, 0, 0, 0,
    0x342024F3u, 0x00140413u, 0x34102573u, 0x00450513u, 0x34151073u, 0x30200073u,
};

struct faulting_ram : sc_core::sc_module {
    tlm_utils::simple_target_socket<faulting_ram> socket{"socket"};
    std::vector<unsigned char> bytes = std::vector<unsigned char>(64 * 1024, 0);
    unsigned faults = 0;

    explicit faulting_ram(sc_core::sc_module_name name) : sc_module(name)
    {
        socket.register_b_transport(this, &faulting_ram::b_transport);
        socket.register_transport_dbg(this, &faulting_ram::transport_dbg);
        std::memcpy(bytes.data(), kProgram, sizeof(kProgram));
        const std::uint32_t initial = 0x11111111u;
        std::memcpy(&bytes[kWord], &initial, 4);
    }
    std::uint32_t word() const
    {
        std::uint32_t v;
        std::memcpy(&v, &bytes[kWord], 4);
        return v;
    }
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay)
    {
        delay += sc_core::sc_time(10, sc_core::SC_NS);
        if (tx.is_read() && tx.get_address() == kWord && faults == 0) {
            ++faults; // the first LR's load
            tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        transport_dbg(tx);
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    unsigned transport_dbg(tlm::tlm_generic_payload& tx)
    {
        const std::uint64_t a = tx.get_address();
        const unsigned n = tx.get_data_length();
        if (a + n > bytes.size()) return 0;
        if (tx.is_write())
            std::memcpy(&bytes[a], tx.get_data_ptr(), n);
        else
            std::memcpy(tx.get_data_ptr(), &bytes[a], n);
        return n;
    }
};

struct harness : sc_core::sc_module {
    fx1::ExclusiveMonitor monitor{1, 64};
    faulting_ram ram{"ram"};
    fx1::CpuPortAdapter port{"cpu_port"};
    cdc::cpu::riscv_vp_plusplus_cpu cpu;

    static cdc::cpu::cpu_config hart0()
    {
        cdc::cpu::cpu_config cfg;
        cfg.hart_id = 0;
        cfg.reset_pc = 0;
        return cfg;
    }
    cdc::cpu::riscv_vp_plusplus_options options()
    {
        cdc::cpu::riscv_vp_plusplus_options opts;
        opts.disable_extensions = "DV";
        opts.exclusive_monitor = &monitor;
        return opts;
    }

    SC_HAS_PROCESS(harness);
    explicit harness(sc_core::sc_module_name name) : sc_module(name), cpu("cpu", hart0(), options())
    {
        cpu.data_bus().bind(port.target);
        port.out.bind(ram.socket);
        SC_THREAD(run);
    }

    void run()
    {
        sc_core::wait(50, sc_core::SC_US);
        CHECK(cpu.get_pc() == kHaltPc, "the program reached its halt loop");
        CHECK(ram.faults == 1, "the first LR's load was answered with a slave error");
        CHECK(cpu.read_gpr(8) == 1 && cpu.read_gpr(9) == 5, "exactly one trap, a load access fault (mcause 5)");
        CHECK(cpu.read_gpr(28) == 1, "the SC after the faulted LR fails");
        CHECK(cpu.read_gpr(7) == 0x11111111u, "... and did not write (the second LR reads the old value)");
        CHECK(cpu.read_gpr(29) == 0, "the SC after a completed LR succeeds");
        CHECK(ram.word() == 0x123u, "... and writes");
        const auto& s = monitor.stats();
        CHECK(s.reservations == 2 && s.sc_failed == 1 && s.sc_succeeded == 1, "monitor: 2 LR, 1 failed SC, 1 SC");
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char*[])
{
    tlm::tlm_global_quantum::instance().set(sc_core::sc_time(100, sc_core::SC_NS));
    harness top("top");
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));
    if (failures) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_fx1_faulting_lr: all checks passed\n";
    return 0;
}
