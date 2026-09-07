#include "edu_timer_tlm.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

class testbench : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<testbench> socket;
    sc_core::sc_out<bool> reset_n;
    sc_core::sc_in<bool> irq;
    int failures = 0;

    SC_HAS_PROCESS(testbench);
    explicit testbench(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket"), reset_n("reset_n"), irq("irq")
    {
        SC_THREAD(run);
    }

private:
    struct result {
        tlm::tlm_response_status response;
        std::uint32_t data;
        sc_core::sc_time delay;
    };

    result transact(tlm::tlm_command command, std::uint64_t address,
                    std::uint32_t data = 0,
                    const unsigned char* byte_enable = nullptr,
                    unsigned byte_enable_length = 0,
                    unsigned data_length = 4,
                    unsigned streaming_width = 4,
                    sc_core::sc_time initial_delay = sc_core::SC_ZERO_TIME)
    {
        tlm::tlm_generic_payload trans;
        std::array<unsigned char, 4> bytes{};
        std::memcpy(bytes.data(), &data, bytes.size());

        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(bytes.data());
        trans.set_data_length(data_length);
        trans.set_streaming_width(streaming_width);
        trans.set_byte_enable_ptr(const_cast<unsigned char*>(byte_enable));
        trans.set_byte_enable_length(byte_enable_length);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(trans, initial_delay);
        std::memcpy(&data, bytes.data(), bytes.size());
        return {trans.get_response_status(), data, initial_delay};
    }

    std::uint32_t read32(std::uint64_t address)
    {
        const result r = transact(tlm::TLM_READ_COMMAND, address);
        check(r.response == tlm::TLM_OK_RESPONSE, "read response OK");
        return r.data;
    }

    void write32(std::uint64_t address, std::uint32_t value)
    {
        const result r = transact(tlm::TLM_WRITE_COMMAND, address, value);
        check(r.response == tlm::TLM_OK_RESPONSE, "write response OK");
    }

    void check(bool condition, const char* name)
    {
        if (condition) {
            std::cout << "[PASS] " << name << " @ "
                      << sc_core::sc_time_stamp() << '\n';
        } else {
            ++failures;
            std::cout << "[FAIL] " << name << " @ "
                      << sc_core::sc_time_stamp() << '\n';
        }
    }

    void apply_reset()
    {
        reset_n.write(false);
        wait(sc_core::sc_time(1, sc_core::SC_NS));
        reset_n.write(true);
        wait(sc_core::SC_ZERO_TIME);
    }

    void run()
    {
        using timer = tutorial::edu_timer_tlm;

        std::cout << "=== edu_timer_tlm unit test ===\n";
        apply_reset();

        check(read32(timer::kCtrlOffset) == 0, "reset CTRL=0");
        check(read32(timer::kLoadOffset) == 0, "reset LOAD=0");
        check(read32(timer::kValueOffset) == 0, "reset VALUE=0");
        check(read32(timer::kStatusOffset) == 0, "reset STATUS=0");
        check(read32(timer::kIdOffset) == timer::kIdValue, "ID register");

        write32(timer::kLoadOffset, 0xAABB'CCDDu);
        check(read32(timer::kLoadOffset) == 0xAABB'CCDDu,
              "32-bit register read/write");
        check(read32(timer::kValueOffset) == 0xAABB'CCDDu,
              "LOAD write reloads VALUE");

        const unsigned char be_lane_1_3[4] = {0x00, 0xFF, 0x00, 0xFF};
        result r = transact(tlm::TLM_WRITE_COMMAND, timer::kLoadOffset,
                            0x1122'3344u, be_lane_1_3, 4);
        check(r.response == tlm::TLM_OK_RESPONSE, "byte-enable response OK");
        check(read32(timer::kLoadOffset) == 0x11BB'33DDu,
              "byte-enable updates selected lanes only");

        r = transact(tlm::TLM_WRITE_COMMAND, 0x40, 1);
        check(r.response == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "invalid address rejected");
        r = transact(tlm::TLM_WRITE_COMMAND, timer::kCtrlOffset, 1,
                     nullptr, 0, 2, 2);
        check(r.response == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "invalid data length rejected");
        r = transact(tlm::TLM_WRITE_COMMAND, timer::kCtrlOffset, 1,
                     nullptr, 0, 4, 2);
        check(r.response == tlm::TLM_BURST_ERROR_RESPONSE,
              "invalid streaming width rejected");
        r = transact(tlm::TLM_WRITE_COMMAND, timer::kValueOffset, 1);
        check(r.response == tlm::TLM_COMMAND_ERROR_RESPONSE,
              "write to read-only VALUE rejected");
        r = transact(tlm::TLM_WRITE_COMMAND, timer::kIdOffset, 1);
        check(r.response == tlm::TLM_COMMAND_ERROR_RESPONSE,
              "write to read-only ID rejected");
        r = transact(tlm::TLM_IGNORE_COMMAND, timer::kCtrlOffset);
        check(r.response == tlm::TLM_COMMAND_ERROR_RESPONSE,
              "IGNORE command rejected");
        const unsigned char be_dummy[1] = {0xFF};
        r = transact(tlm::TLM_READ_COMMAND, timer::kCtrlOffset, 0, be_dummy, 0);
        check(r.response == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
              "byte-enable pointer with zero length rejected");

        r = transact(tlm::TLM_READ_COMMAND, timer::kIdOffset, 0,
                     nullptr, 0, 4, 4, sc_core::sc_time(5, sc_core::SC_NS));
        check(r.delay == sc_core::sc_time(15, sc_core::SC_NS),
              "b_transport annotates 10 ns access latency");

        {
            tlm::tlm_generic_payload dbg;
            std::array<unsigned char, 4> buffer{};
            dbg.set_command(tlm::TLM_READ_COMMAND);
            dbg.set_address(timer::kIdOffset);
            dbg.set_data_ptr(buffer.data());
            dbg.set_data_length(4);
            dbg.set_streaming_width(4);
            dbg.set_byte_enable_ptr(nullptr);
            dbg.set_byte_enable_length(0);
            dbg.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            const sc_core::sc_time before = sc_core::sc_time_stamp();
            const unsigned int moved = socket->transport_dbg(dbg);
            std::uint32_t seen = 0;
            std::memcpy(&seen, buffer.data(), sizeof(seen));
            check(moved == 4 && seen == timer::kIdValue &&
                      sc_core::sc_time_stamp() == before,
                  "transport_dbg reads ID without consuming time");

            dbg.set_address(0x40);
            dbg.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            check(socket->transport_dbg(dbg) == 0,
                  "transport_dbg reports 0 bytes on invalid address");
        }

        tlm::tlm_generic_payload dmi_trans;
        tlm::tlm_dmi dmi_data;
        dmi_trans.set_command(tlm::TLM_READ_COMMAND);
        dmi_trans.set_address(timer::kValueOffset);
        check(!socket->get_direct_mem_ptr(dmi_trans, dmi_data),
              "DMI denied for side-effect MMIO");

        apply_reset();
        write32(timer::kLoadOffset, 3);
        write32(timer::kCtrlOffset,
                timer::kCtrlEnable | timer::kCtrlIrqEnable);
        const sc_core::sc_time armed_at = sc_core::sc_time_stamp();
        wait(irq.posedge_event());
        check(sc_core::sc_time_stamp() - armed_at ==
                  sc_core::sc_time(60, sc_core::SC_NS),
              "IRQ occurs after three 20 ns ticks");
        check((read32(timer::kStatusOffset) & timer::kStatusPending) != 0,
              "STATUS pending set");
        write32(timer::kStatusOffset, timer::kStatusPending);
        // update_irq() schedules the sole IRQ-driving SC_METHOD in the next
        // delta cycle; the sc_signal value becomes visible after that method.
        wait(irq.negedge_event());
        check(!irq.read(), "W1C clears IRQ level");

        write32(timer::kLoadOffset, 100);
        write32(timer::kCtrlOffset,
                timer::kCtrlEnable | timer::kCtrlIrqEnable);
        wait(sc_core::sc_time(25, sc_core::SC_NS));
        apply_reset();
        check(read32(timer::kCtrlOffset) == 0 &&
              read32(timer::kValueOffset) == 0 && !irq.read(),
              "reset aborts active countdown");

        // Periodic mode: the IRQ acknowledge in the middle of a period must
        // not shorten the next period.
        apply_reset();
        write32(timer::kLoadOffset, 3);
        write32(timer::kCtrlOffset, timer::kCtrlEnable |
                                    timer::kCtrlPeriodic |
                                    timer::kCtrlIrqEnable);
        sc_core::sc_time previous = sc_core::sc_time_stamp();
        bool period_stable = true;
        for (int round = 0; round < 3; ++round) {
            wait(irq.posedge_event());
            const sc_core::sc_time now = sc_core::sc_time_stamp();
            if (now - previous != sc_core::sc_time(60, sc_core::SC_NS)) {
                period_stable = false;
            }
            previous = now;
            // Acknowledge after a short delay rather than in zero time.  A
            // real driver takes time to reach the handler, and an IRQ that
            // rises and falls inside one simulation time is invisible in a
            // VCD waveform -- there is no sub-timestep resolution there.
            wait(sc_core::sc_time(5, sc_core::SC_NS));
            write32(timer::kStatusOffset, timer::kStatusPending);
            wait(irq.negedge_event());
        }
        check(period_stable, "periodic IRQ keeps a stable 60 ns period");

        // One-shot: an unrelated register access mid-countdown must not
        // advance the counter.
        apply_reset();
        write32(timer::kLoadOffset, 5);
        write32(timer::kCtrlOffset,
                timer::kCtrlEnable | timer::kCtrlIrqEnable);
        const sc_core::sc_time one_shot_armed = sc_core::sc_time_stamp();
        wait(sc_core::sc_time(30, sc_core::SC_NS));
        check(read32(timer::kValueOffset) == 4,
              "VALUE advances one tick per 20 ns, not per access");
        wait(irq.posedge_event());
        check(sc_core::sc_time_stamp() - one_shot_armed ==
                  sc_core::sc_time(100, sc_core::SC_NS),
              "register access mid-countdown does not steal a tick");
        write32(timer::kStatusOffset, timer::kStatusPending);
        wait(irq.negedge_event());

        std::cout << "=== edu_timer_tlm failures: " << failures << " ===\n";
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    testbench tb("tb");
    tutorial::edu_timer_tlm dut("dut", sc_core::sc_time(20, sc_core::SC_NS));
    sc_core::sc_signal<bool> reset_n("reset_n");
    sc_core::sc_signal<bool> irq("irq");

    tb.socket.bind(dut.socket);
    tb.reset_n(reset_n);
    tb.irq(irq);
    dut.reset_n(reset_n);
    dut.irq(irq);

    // Optional waveform.  Unlike the platform demo -- whose firmware arms the
    // timer one-shot and therefore produces a single IRQ -- this testbench
    // runs three periodic cycles, so it is the right place to inspect IRQ
    // spacing in a viewer.  Set EDU_TIMER_VCD=<name> to enable.
    //
    // EDU_TIMER_VCD_DELTA=1 additionally records delta-cycle transitions.
    // The tracer samples once per simulation time by default, so a signal
    // that rises and falls within one timestamp leaves no trace at all;
    // delta-cycle mode makes those transitions visible at the cost of a much
    // noisier waveform.
    sc_core::sc_trace_file* trace = nullptr;
    if (const char* vcd = std::getenv("EDU_TIMER_VCD")) {
        trace = sc_core::sc_create_vcd_trace_file(vcd);
        trace->set_time_unit(1, sc_core::SC_NS);
        if (std::getenv("EDU_TIMER_VCD_DELTA") != nullptr) {
            sc_core::sc_trace_delta_cycles(trace, true);
        }
        sc_core::sc_trace(trace, irq, "irq");
        sc_core::sc_trace(trace, reset_n, "reset_n");
        std::cout << "trace -> " << vcd << ".vcd\n";
    }

    sc_core::sc_start();

    if (trace != nullptr) {
        sc_core::sc_close_vcd_trace_file(trace);
    }
    return tb.failures == 0 ? 0 : 1;
}
