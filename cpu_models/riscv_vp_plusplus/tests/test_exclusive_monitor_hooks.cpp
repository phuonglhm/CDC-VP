// SPDX-License-Identifier: Apache-2.0
//
// The opt-in exclusive-monitor hooks (`riscv_vp_plusplus_options::
// exclusive_monitor`), checked against a scripted monitor that records every
// call and answers `take_reservation` from a fixed script.
//
// What it pins:
//  * the exact hook sequence of a plain store, LR, a successful and a failed
//    SC, and an AMO -- in particular that LR is *not* bracketed and takes no
//    bus lock, and that a failed SC writes nothing;
//  * that the SC result the guest sees is the monitor's answer;
//  * that `reset_cpu()` hands the reset to the monitor (`reset_hart`), so a
//    reservation does not survive a hart reset (plan C7).
//
// The default (no monitor) path is upstream's and is what every other test in
// this directory exercises; nothing here changes it.

#include <cstdint>
#include <cstring>
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/exclusive_monitor_if.h>

#include "riscv_vp_plusplus_wrapper.h"

namespace {

int failures = 0;

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr std::uint64_t kMemorySize = 64 * 1024;
constexpr std::uint64_t kWord = 0x4000;

///   lui t0, 0x4 ; li t1, 5 ; sw t1, 0(t0)
///   lr.w t2, (t0) ; sc.w t3, t1, (t0)     # monitor answers "reserved"
///   lr.w t2, (t0) ; sc.w t4, t1, (t0)     # monitor answers "lost"
///   amoadd.w t5, t1, (t0) ; lr.w t2, (t0)
///   halt: j halt
/// Assembled with riscv-none-elf-as -march=rv32ima_zicsr and pasted.
constexpr std::uint32_t kProgram[] = {
    0x000042B7u, 0x00500313u, 0x0062A023u, 0x1002A3AFu, 0x1862AE2Fu,
    0x1002A3AFu, 0x1862AEAFu, 0x0062AF2Fu, 0x1002A3AFu, 0x0000006Fu,
};
constexpr std::uint64_t kHaltPc = 0x24;

class scripted_monitor : public cdc::cpu::exclusive_monitor_if {
public:
    std::vector<std::string> log;
    std::deque<bool> answers{true, false};

    void load_reserved(unsigned hart, std::uint64_t addr, unsigned size) override
    {
        record("lr", hart, addr, size);
    }
    void drop_reservation(unsigned hart) override { record("drop", hart, 0, 0); }
    void atomic_begin(unsigned hart, std::uint64_t addr, unsigned size) override
    {
        record("begin", hart, addr, size);
    }
    bool take_reservation(unsigned hart, std::uint64_t addr, unsigned size) override
    {
        record("take", hart, addr, size);
        bool answer = false;
        if (!answers.empty()) {
            answer = answers.front();
            answers.pop_front();
        }
        return answer;
    }
    void atomic_end(unsigned hart) override { record("end", hart, 0, 0); }
    bool try_begin_write(unsigned master, std::uint64_t addr, unsigned size,
                         std::uint64_t& ticket) override
    {
        record("write", master, addr, size);
        ticket = ++tickets_;
        return true;
    }
    void end_write(std::uint64_t) override { log.push_back("write_end"); }
    void reset_hart(unsigned hart) override { record("reset", hart, 0, 0); }
    const sc_core::sc_event& changed() const override { return changed_; }

private:
    void record(const char* what, unsigned who, std::uint64_t addr, unsigned size)
    {
        std::ostringstream s;
        s << what << ' ' << who;
        if (size) {
            s << " 0x" << std::hex << addr << std::dec << '/' << size;
        }
        log.push_back(s.str());
    }

    std::uint64_t tickets_ = 0;
    sc_core::sc_event changed_;
};

class memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<memory> socket{"socket"};

    explicit memory(sc_core::sc_module_name name)
        : sc_core::sc_module(name), storage_(kMemorySize, 0)
    {
        socket.register_b_transport(this, &memory::b_transport);
        socket.register_transport_dbg(this, &memory::transport_dbg);
        std::memcpy(storage_.data(), kProgram, sizeof(kProgram));
    }

    std::uint32_t word(std::uint64_t address) const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, &storage_[address], 4);
        return value;
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        delay += sc_core::sc_time(1, sc_core::SC_NS);
        transport_dbg(trans);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        if (address + length > storage_.size()) {
            return 0;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&storage_[address], trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), &storage_[address], length);
        }
        return length;
    }

    std::vector<unsigned char> storage_;
};

class bench : public sc_core::sc_module {
public:
    scripted_monitor monitor;
    memory ram{"ram"};
    std::unique_ptr<cdc::cpu::riscv_vp_plusplus_cpu> cpu;

    SC_HAS_PROCESS(bench);
    explicit bench(sc_core::sc_module_name name) : sc_core::sc_module(name)
    {
        cdc::cpu::cpu_config config;
        config.xlen = 32;
        config.hart_id = 0;
        config.reset_pc = 0;
        cdc::cpu::riscv_vp_plusplus_options options;
        options.exclusive_monitor = &monitor;
        cpu = std::make_unique<cdc::cpu::riscv_vp_plusplus_cpu>("cpu", config, options);
        cpu->data_bus().bind(ram.socket);
        SC_THREAD(run);
    }

    void run()
    {
        sc_core::wait(20, sc_core::SC_US);
        CHECK_MSG(cpu->get_pc() == kHaltPc, "the program did not reach its halt loop");

        const std::vector<std::string> expected = {
            "write 0 0x4000/4", "write_end",                           // sw
            "lr 0 0x4000/4",                                           // lr.w
            "begin 0 0x4000/4", "take 0 0x4000/4",                     // sc.w (reserved)
            "write 0 0x4000/4", "write_end", "end 0",
            "lr 0 0x4000/4",                                           // lr.w
            "begin 0 0x4000/4", "take 0 0x4000/4", "end 0",            // sc.w (lost): no write
            "begin 0 0x4000/4", "write 0 0x4000/4", "write_end", "end 0", // amoadd.w
            "lr 0 0x4000/4",                                           // lr.w
        };
        CHECK_MSG(monitor.log == expected, "hook sequence differs (printed below)");
        if (monitor.log != expected) {
            for (const auto& line : monitor.log) std::cerr << "  got: " << line << '\n';
        }

        CHECK_MSG(cpu->read_gpr(28) == 0, "sc.w with the monitor answering 'reserved' must report success");
        CHECK_MSG(cpu->read_gpr(29) == 1, "sc.w with the monitor answering 'lost' must report failure");
        CHECK_MSG(cpu->read_gpr(30) == 5, "amoadd.w returns the old value");
        CHECK_MSG(ram.word(kWord) == 10, "sw 5, successful sc 5, failed sc (no write), amoadd +5 -> 10");
        CHECK_MSG(!cpu->holds_bus_lock(), "with a monitor, lr.w takes no bus lock");

        const std::size_t before = monitor.log.size();
        cpu->reset_cpu();
        CHECK_MSG(monitor.log.size() == before + 1 && monitor.log.back() == "reset 0",
                  "reset_cpu() must drop the hart's reservation in the monitor");
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char*[])
{
    tlm::tlm_global_quantum::instance().set(sc_core::sc_time(100, sc_core::SC_NS));
    bench top("top");
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));
    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_exclusive_monitor_hooks: all checks passed\n";
    return 0;
}
