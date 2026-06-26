//author: Viet Hoang
//verified: linhtk55-fpt

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include <iostream>

#include "timer.h"

using namespace std;
using namespace sc_core;
using namespace sc_dt;

namespace cdc::components {
SC_MODULE(Testbench) {
    tlm_utils::simple_initiator_socket<Testbench> socket;
    sc_out<bool> prstn;
    sc_out<bool> extin;
    sc_in<bool>  timerint;
 
    void write_reg(uint32_t addr, uint32_t data) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
            cout << "[WARN] Write failed at 0x" << hex << addr << dec << endl;
    }
 
    uint32_t read_reg(uint32_t addr) {
        uint32_t data = 0;
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        if (trans.get_response_status() != tlm::TLM_OK_RESPONSE)
            cout << "[WARN] Read failed at 0x" << hex << addr << dec << endl;
        return data;
    }
 
    // send a transaction with a bad data length
    tlm::tlm_response_status write_reg_bad_len(uint32_t addr, uint32_t data) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(2);          // wrong – should be 4
        trans.set_streaming_width(2);
        trans.set_byte_enable_ptr(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }
 
    // send a transaction to an unmapped address
    tlm::tlm_response_status write_reg_bad_addr(uint32_t addr, uint32_t data) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }
 
    void do_reset() {
        prstn.write(false);
        wait(SC_ZERO_TIME);
        prstn.write(true);
        wait(SC_ZERO_TIME);
    }
 
    void pass(const char* name) { cout << "[PASS] " << name << " @ " << sc_time_stamp() << endl; }
    void fail(const char* name) { cout << "[FAIL] " << name << " @ " << sc_time_stamp() << endl; }
 
    //RUN ALL TESTS
    void run() {
        // bring prstn high so timer leaves reset
        prstn.write(false);
        wait(SC_ZERO_TIME);
        prstn.write(true);
        wait(SC_ZERO_TIME);
        extin.write(false);
        wait(SC_ZERO_TIME);
 
        // ---- TEST 1: basic interrupt firing -------------------------
        cout << "\n=== TEST 1: basic interrupt ===" << endl;
        write_reg(ADDR::RELOAD, 5);
        write_reg(ADDR::CTRL,   OPS::ENABLE | OPS::INTR_EN);
        wait(timerint.posedge_event());
        if (timerint.read())
            pass("basic interrupt fired");
        else
            fail("basic interrupt fired");
        // check VALUE reloaded
        uint32_t val = read_reg(ADDR::VALUE);
        if (val == 5) pass("VALUE reloaded to 5"); else fail("VALUE reloaded to 5");
        // clear interrupt
        write_reg(ADDR::INTSTATUS, 0x1);
        wait(SC_ZERO_TIME);
        if (!timerint.read()) pass("interrupt cleared"); else fail("interrupt cleared");
        // disable timer
        write_reg(ADDR::CTRL, 0);
 
        // ---- TEST 2: INTR_EN = 0, intr_status still sets -----------
        cout << "\n=== TEST 2: INTR_EN=0, intr_status polled ===" << endl;
        do_reset();
        write_reg(ADDR::RELOAD, 3);
        write_reg(ADDR::CTRL, OPS::ENABLE);   // no INTR_EN
        // wait long enough for the counter to expire
        wait(5 * sc_time(20, SC_NS));
        if (!timerint.read()) pass("timerint not asserted without INTR_EN");
        else                   fail("timerint not asserted without INTR_EN");
        uint32_t status = read_reg(ADDR::INTSTATUS);
        if (status) pass("intr_status set without INTR_EN"); else fail("intr_status set without INTR_EN");
        write_reg(ADDR::CTRL, 0);
 
        // ---- TEST 3: disable mid-count ------------------------------
        cout << "\n=== TEST 3: disable mid-count ===" << endl;
        do_reset();
        write_reg(ADDR::RELOAD, 100);
        write_reg(ADDR::CTRL, OPS::ENABLE | OPS::INTR_EN);
        wait(3 * sc_time(20, SC_NS));          // let it count a few ticks
        write_reg(ADDR::CTRL, 0);              // disable
        wait(sc_time(100, SC_NS));
        uint32_t val_before = read_reg(ADDR::VALUE);
        wait(sc_time(100, SC_NS));          // wait some more
        uint32_t val_after = read_reg(ADDR::VALUE);
        if (val_after == val_before) pass("timer stopped after disable");
        else                          fail("timer stopped after disable");
 
        // ---- TEST 4: RELOAD write also sets VALUE -------------------
        cout << "\n=== TEST 4: RELOAD write sets VALUE ===" << endl;
        do_reset();
        write_reg(ADDR::RELOAD, 42);
        uint32_t v = read_reg(ADDR::VALUE);
        if (v == 42) pass("RELOAD write sets VALUE"); else fail("RELOAD write sets VALUE");
 
        // ---- TEST 5: CTRL read back ---------------------------------
        cout << "\n=== TEST 5: CTRL read back ===" << endl;
        do_reset();
        uint32_t ctrl_val = OPS::ENABLE | OPS::INTR_EN;
        write_reg(ADDR::CTRL, ctrl_val);
        uint32_t ctrl_rb = read_reg(ADDR::CTRL);
        if (ctrl_rb == ctrl_val) pass("CTRL read back"); else fail("CTRL read back");
        write_reg(ADDR::CTRL, 0);
 
        // ---- TEST 6: reset mid-count --------------------------------
        cout << "\n=== TEST 6: reset mid-count ===" << endl;
        write_reg(ADDR::RELOAD, 50);
        write_reg(ADDR::CTRL, OPS::ENABLE | OPS::INTR_EN);
        wait(3 * sc_time(20, SC_NS));
        prstn.write(false);
        wait(SC_ZERO_TIME);
        // registers should be cleared
        // (timer_thread is now in reset(), blocked on posedge)
        prstn.write(true);
        wait(SC_ZERO_TIME);
        uint32_t ctrl_after = read_reg(ADDR::CTRL);
        uint32_t val_after2 = read_reg(ADDR::VALUE);
        if (ctrl_after == 0 && val_after2 == 0)
            pass("registers cleared after reset");
        else
            fail("registers cleared after reset");
 
        // ---- TEST 7: EXT_EN – timer pauses when extin LOW ----------
        cout << "\n=== TEST 7: EXT_EN pause ===" << endl;
        do_reset();
        write_reg(ADDR::RELOAD, 20);
        write_reg(ADDR::CTRL, OPS::ENABLE | OPS::EX_EN | OPS::INTR_EN);
        extin.write(false);                    // gate LOW → timer should pause
        wait(5 * sc_time(20, SC_NS));
        uint32_t val_paused = read_reg(ADDR::VALUE);
        // VALUE should still be 20 (reload value, never counted)
        if (val_paused == 20) pass("timer paused with EXT_EN and extin=0");
        else                   fail("timer paused with EXT_EN and extin=0");
        // now let it run
        extin.write(true);
        wait(timerint.posedge_event());
        pass("timer ran after extin=1 with EXT_EN");
        write_reg(ADDR::INTSTATUS, 0x1);
        write_reg(ADDR::CTRL, 0);
 
        // ---- TEST 8: EXT_CLK – count on extin edges ----------------
        cout << "\n=== TEST 8: EXT_CLK ===" << endl;
        do_reset();
        extin.write(false);
        write_reg(ADDR::RELOAD, 3);
        write_reg(ADDR::CTRL, OPS::ENABLE | OPS::EX_CLK | OPS::INTR_EN);
        // manually pulse extin 4 times (3 down-counts + fire at 0)
        for (int i = 0; i < 4; i++) {
            wait(sc_time(100, SC_NS));
            extin.write(true);
            wait(SC_ZERO_TIME);
            extin.write(false);
            wait(SC_ZERO_TIME);
        }
        if (timerint.read()) pass("EXT_CLK interrupt after 4 edges");
        else                  fail("EXT_CLK interrupt after 4 edges");
        write_reg(ADDR::INTSTATUS, 0x1);
        write_reg(ADDR::CTRL, 0);
 
        // ---- TEST 9: multiple interrupts ----------------------------
        cout << "\n=== TEST 9: multiple interrupts ===" << endl;
        do_reset();
        write_reg(ADDR::RELOAD, 2);
        write_reg(ADDR::CTRL, OPS::ENABLE | OPS::INTR_EN);
        int count = 0;
        while (count < 3) {
            wait(timerint.posedge_event());
            count++;
            write_reg(ADDR::INTSTATUS, 0x1);
            wait(SC_ZERO_TIME);
        }
        if (count == 3) pass("3 consecutive interrupts"); else fail("3 consecutive interrupts");
        write_reg(ADDR::CTRL, 0);
 
        // ---- TEST 10: bad length ------------------------------------
        cout << "\n=== TEST 10: bad data length ===" << endl;
        tlm::tlm_response_status r = write_reg_bad_len(ADDR::CTRL, 0x1);
        if (r == tlm::TLM_GENERIC_ERROR_RESPONSE)
            pass("bad length returns TLM_GENERIC_ERROR_RESPONSE");
        else
            fail("bad length returns TLM_GENERIC_ERROR_RESPONSE");
 
        // ---- TEST 11: bad address -----------------------------------
        cout << "\n=== TEST 11: bad address ===" << endl;
        tlm::tlm_response_status r2 = write_reg_bad_addr(0xFF, 0x1);
        if (r2 == tlm::TLM_ADDRESS_ERROR_RESPONSE)
            pass("bad address returns TLM_ADDRESS_ERROR_RESPONSE");
        else
            fail("bad address returns TLM_ADDRESS_ERROR_RESPONSE");
 
        cout << "\n=== All tests done ===" << endl;
        sc_stop();
    }
 
    SC_CTOR(Testbench) : socket("socket") {
        SC_THREAD(run);
    }
};
} // namespace cdc::components

int sc_main(int argc, char* argv[]) {
    cdc::components::Testbench* tb    = new cdc::components::Testbench("tb");
    cdc::components::Timer*     timer = new cdc::components::Timer("timer", 
    1, 2, 3, 4, 5, 6, 7, 8,
    1, 2, 3, 4);
 
    sc_signal<bool> sig_prstn, sig_extin, sig_timerint;
 
    tb->socket.bind(timer->socket);
    tb->prstn.bind(sig_prstn);
    tb->extin.bind(sig_extin);
    tb->timerint.bind(sig_timerint);
    timer->prstn.bind(sig_prstn);
    timer->extin.bind(sig_extin);
    timer->timerint.bind(sig_timerint);
 
    sc_start();
 
    delete tb;
    delete timer;
    return 0;
}
