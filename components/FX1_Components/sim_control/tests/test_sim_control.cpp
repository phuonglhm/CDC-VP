#include "fx1/sim_control.h"
#include "fx1/fx1_memory_map.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <string>

namespace {
unsigned checks = 0, failures = 0;
void expect(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> port{"port"};
    fx1::SimControl ctl;
    std::uint32_t value;
    bool after_stop = false;
    SC_HAS_PROCESS(Tb);
    Tb(sc_core::sc_module_name n, std::uint32_t v) : sc_module(n), ctl("ctl"), value(v) {
        port.bind(ctl.socket);
        SC_THREAD(run);
    }
    tlm::tlm_response_status write(std::uint64_t address, std::uint32_t v, unsigned length = 4) {
        unsigned char d[4];
        for (unsigned i = 0; i < 4; ++i) d[i] = static_cast<unsigned char>(v >> (8 * i));
        tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        tx.set_address(address);
        tx.set_data_ptr(d);
        tx.set_data_length(length);
        tx.set_streaming_width(length);
        sc_core::sc_time delay;
        port->b_transport(tx, delay);
        return tx.get_response_status();
    }
    void run() {
        wait(sc_core::sc_time(5, sc_core::SC_US));
        expect(ctl.result() == fx1::SimControl::Result::running, "running before verdict");
        expect(write(4, FX1_SIM_CTRL_PASS) == tlm::TLM_GENERIC_ERROR_RESPONSE, "wrong offset rejected");
        expect(write(0, FX1_SIM_CTRL_PASS, 2) == tlm::TLM_GENERIC_ERROR_RESPONSE, "16-bit write rejected");
        expect(write(0, 0x1234) == tlm::TLM_GENERIC_ERROR_RESPONSE, "unknown value rejected");
        expect(write(FX1_SIM_CTRL_DOORBELL_OFF, 1) == tlm::TLM_GENERIC_ERROR_RESPONSE,
               "doorbell offset rejected unless enabled");
        expect(ctl.result() == fx1::SimControl::Result::running, "rejected writes leave it running");
        expect(write(0, value) == tlm::TLM_OK_RESPONSE, "verdict accepted");
        wait(sc_core::sc_time(1, sc_core::SC_US));
        after_stop = true;  // must never run: sc_stop() ends the simulation
    }
};
// Test interrupt lines (offset FX1_SIM_CTRL_IRQ_OFF).
struct IrqTb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<IrqTb> port{"port"};
    fx1::SimControl ctl{"ctl", false, 2};
    sc_core::sc_vector<sc_core::sc_signal<bool>> line{"line", 2};
    bool done = false;
    SC_HAS_PROCESS(IrqTb);
    explicit IrqTb(sc_core::sc_module_name n) : sc_module(n) {
        port.bind(ctl.socket);
        for (unsigned i = 0; i < 2; ++i) ctl.test_irq[i](line[i]);
        SC_THREAD(run);
    }
    std::uint32_t access(bool write, std::uint32_t v) {
        unsigned char d[4];
        for (unsigned i = 0; i < 4; ++i) d[i] = static_cast<unsigned char>(v >> (8 * i));
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(FX1_SIM_CTRL_IRQ_OFF);
        tx.set_data_ptr(d);
        tx.set_data_length(4);
        tx.set_streaming_width(4);
        sc_core::sc_time delay;
        port->b_transport(tx, delay);
        expect(tx.is_response_ok() && delay == sc_core::sc_time(10, sc_core::SC_NS),
               "irq register access annotates latency");
        wait(delay);
        return d[0] | (d[1] << 8) | (d[2] << 16) | (std::uint32_t(d[3]) << 24);
    }
    void run() {
        wait(sc_core::sc_time(1, sc_core::SC_US));
        expect(!line[0].read() && !line[1].read(), "test lines low at reset");
        access(true, 0xFFFFFFFFu);
        expect(line[0].read() && line[1].read(), "lines raised");
        expect(access(false, 0) == 0x3, "readback masked to the line count");
        access(true, 0x2);
        expect(!line[0].read() && line[1].read(), "line 0 lowered, line 1 kept");
        expect(ctl.result() == fx1::SimControl::Result::running, "irq writes are not verdicts");
        done = true;
    }
};
// Doorbell (offset FX1_SIM_CTRL_DOORBELL_OFF), enabled by the platform.
struct DoorbellTb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<DoorbellTb> port{"port"};
    fx1::SimControl ctl{"ctl", false};
    unsigned wakes = 0;
    bool done = false;
    SC_HAS_PROCESS(DoorbellTb);
    explicit DoorbellTb(sc_core::sc_module_name n) : sc_module(n) {
        port.bind(ctl.socket);
        SC_THREAD(run);
        SC_THREAD(waiter);
    }
    tlm::tlm_response_status access(bool write, std::uint32_t& v, unsigned length = 4) {
        unsigned char d[4];
        for (unsigned i = 0; i < 4; ++i) d[i] = static_cast<unsigned char>(v >> (8 * i));
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(FX1_SIM_CTRL_DOORBELL_OFF);
        tx.set_data_ptr(d);
        tx.set_data_length(length);
        tx.set_streaming_width(length);
        sc_core::sc_time delay;
        port->b_transport(tx, delay);
        v = d[0] | (d[1] << 8) | (d[2] << 16) | (std::uint32_t(d[3]) << 24);
        return tx.get_response_status();
    }
    void waiter() {
        for (;;) {
            wait(ctl.doorbell_event());
            ++wakes;
        }
    }
    void run() {
        std::uint32_t v = 0;
        ctl.enable_doorbell(false);
        expect(access(false, v) == tlm::TLM_OK_RESPONSE && v == 0, "doorbell reads 0 without a bridge");
        ctl.enable_doorbell(true);
        v = 0xFFFFFFFFu;
        expect(access(false, v) == tlm::TLM_OK_RESPONSE && v == FX1_SIM_CTRL_DOORBELL_ATTACHED,
               "doorbell reads ATTACHED only (bits 31..1 zero) with a bridge");
        v = 0;
        expect(access(true, v, 2) == tlm::TLM_GENERIC_ERROR_RESPONSE, "16-bit doorbell access rejected");
        for (std::uint32_t k = 0; k < 3; ++k) {   // three rings in one delta: one wake, three counted
            v = k;
            expect(access(true, v) == tlm::TLM_OK_RESPONSE, "doorbell write accepted");
        }
        wait(sc_core::sc_time(1, sc_core::SC_US));
        expect(ctl.doorbells() == 3 && wakes == 1, "coalesced rings: counted 3, one wake");
        v = 7;
        access(true, v);
        wait(sc_core::sc_time(1, sc_core::SC_US));
        expect(ctl.doorbells() == 4 && wakes == 2, "a later ring wakes again");
        expect(ctl.result() == fx1::SimControl::Result::running, "doorbell writes are not verdicts");
        done = true;
    }
};
} // namespace

int sc_main(int, char**) {
    Tb pass("pass", FX1_SIM_CTRL_PASS);
    Tb fail("fail", (0xBEEFu << 16) | FX1_SIM_CTRL_FAIL);
    IrqTb irq("irq");
    DoorbellTb bell("bell");
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_MS));
    // Both write at the same time stamp; sc_stop() takes effect after the
    // current delta, so both verdicts are recorded.
    expect(pass.ctl.result() == fx1::SimControl::Result::pass, "pass verdict");
    expect(fail.ctl.result() == fx1::SimControl::Result::fail && fail.ctl.fail_code() == 0xBEEF,
           "fail verdict with code");
    expect(pass.ctl.finish_time() == sc_core::sc_time(5, sc_core::SC_US), "finish time recorded");
    expect(sc_core::sc_time_stamp() == sc_core::sc_time(5, sc_core::SC_US), "simulation stopped at verdict");
    expect(!pass.after_stop && !fail.after_stop, "nothing ran after the stop");
    expect(irq.done, "irq line test completed");
    expect(bell.done, "doorbell test completed");
    std::cout << "fx1_sim_control: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
