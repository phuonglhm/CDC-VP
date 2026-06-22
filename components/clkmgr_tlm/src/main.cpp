#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include "clkmgr.h"

using namespace sc_core;

SC_MODULE(testbench) {
    tlm_utils::simple_initiator_socket<testbench> socket;

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

        std::cout << "\n--- Test 1: Enable external clock (high speed) ---"
                  << std::endl;
        // sel = True (0x6) in bits[3:0], hispeed = True (0x6) in bits[7:4]
        uint32_t extclk_ctrl_val = (0x6 << 4) | 0x6;
        do_write(0x8, extclk_ctrl_val); // REG_EXTCLK_CTRL offset
        uint32_t status = do_read(0xc); // REG_EXTCLK_STATUS offset
        std::cout << "EXTCLK_STATUS = 0x" << std::hex << status << std::endl;

        std::cout << "\n--- Test 2: Disable external clock ---" << std::endl;
        uint32_t disable_val = (0x6 << 4) | 0x9; // sel=False(0x9), hispeed=True
        do_write(0x8, disable_val);
        status = do_read(0xc);
        std::cout << "EXTCLK_STATUS = 0x" << std::hex << status << std::endl;

        std::cout << "\n--- Test 3: CLK_ENABLES write/read ---" << std::endl;
        do_write(0x18, 0x5); // arbitrary pattern: enable bits 0,2 only
        uint32_t enables = do_read(0x18);
        std::cout << "CLK_ENABLES = 0x" << std::hex << enables << std::endl;

        std::cout << "\n--- Test 4: CLK_HINTS shutoff request ---" << std::endl;
        do_write(0x1c, 0x0); // clear all hints -> request shutoff for all
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

    tb.socket.bind(clkmgr_model.socket);

    sc_start();
    return 0;
}