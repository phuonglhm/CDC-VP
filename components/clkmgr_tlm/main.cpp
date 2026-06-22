//Author: trangnm20
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include "clkmgr.h"

using namespace sc_core;

SC_MODULE(testbench) {
    tlm_utils::simple_initiator_socket<testbench> socket;
    Clkmgr* clkmgr_model_ptr = nullptr;  

    SC_CTOR(testbench) : socket("socket") {
        SC_THREAD(run_tests);
    }

    void do_write(uint32_t offset, uint32_t value) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
    }

    uint32_t do_read(uint32_t offset) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        uint32_t value = 0;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return value;
    }

    void run_tests() {
        std::cout << "\n=== Clkmgr Model Test ===" << std::endl;

        // Set life cycle state to TEST so EXTCLK_CTRL writes take effect
        // (mirrors a debug-unlocked chip state)
        clkmgr_model_ptr->set_lc_state(Clkmgr::LcState::Test);

        std::cout << "\n--- Test 1: Enable external clock (high speed) ---"
                  << std::endl;
        uint32_t extclk_ctrl_val = (0x6 << 4) | 0x6;
        do_write(0x8, extclk_ctrl_val);
        uint32_t status = do_read(0xc);
        std::cout << "EXTCLK_STATUS = 0x" << std::hex << status << std::endl;

        std::cout << "\n--- Test 2: Disable external clock ---" << std::endl;
        uint32_t disable_val = (0x6 << 4) | 0x9;
        do_write(0x8, disable_val);
        status = do_read(0xc);
        std::cout << "EXTCLK_STATUS = 0x" << std::hex << status << std::endl;

        std::cout << "\n--- Test 2.5: EXTCLK_CTRL_REGWEN lock ---" << std::endl;
        uint32_t regwen = do_read(0x4);
        std::cout << "EXTCLK_CTRL_REGWEN (before lock) = 0x"
                  << std::hex << regwen << std::endl;

        do_write(0x4, 0x0);
        regwen = do_read(0x4);
        std::cout << "EXTCLK_CTRL_REGWEN (after lock) = 0x"
                  << std::hex << regwen << std::endl;

        std::cout << "Attempting to re-enable external clock (should be blocked)"
                  << std::endl;
        uint32_t blocked_val = (0x6 << 4) | 0x6;
        do_write(0x8, blocked_val);
        status = do_read(0xc);
        std::cout << "EXTCLK_STATUS (should be unchanged, still locked out) = 0x"
                  << std::hex << status << std::endl;

        do_write(0x4, 0x1);
        regwen = do_read(0x4);
        std::cout << "EXTCLK_CTRL_REGWEN (after attempting to write 1) = 0x"
                  << std::hex << regwen
                  << " (should still be 0 — rw0c cannot be unlocked by SW)"
                  << std::endl;

        std::cout << "\n--- Test 2.6: Life cycle PROD blocks switch ---"
                  << std::endl;
        std::cout << "(Note: REGWEN is locked from Test 2.5, so EXTCLK_CTRL "
                  << "is already blocked regardless of life cycle state here. "
                  << "Life cycle gating in isolation is verified separately "
                  << "in the CMake test suite with a fresh model instance.)"
                  << std::endl;

        std::cout << "\n--- Test 3: CLK_ENABLES write/read ---" << std::endl;
        do_write(0x18, 0x5);
        uint32_t enables = do_read(0x18);
        std::cout << "CLK_ENABLES = 0x" << std::hex << enables << std::endl;

        std::cout << "\n--- Test 4: CLK_HINTS shutoff request ---" << std::endl;
        do_write(0x1c, 0x0);
        uint32_t hints_status = do_read(0x20);
        std::cout << "CLK_HINTS_STATUS = 0x" << std::hex << hints_status
                  << std::endl;

        std::cout << "\n=== Tests complete ===" << std::endl;
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Clkmgr    clkmgr_model("clkmgr_model");
    testbench tb("testbench");

    tb.clkmgr_model_ptr = &clkmgr_model;

    tb.socket.bind(clkmgr_model.socket);

    sc_start();
    return 0;
}