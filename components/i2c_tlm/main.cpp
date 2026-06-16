#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include "include/i2c.h"

using namespace sc_core;

SC_MODULE(testbench) {
    tlm_utils::simple_initiator_socket<testbench> socket;
    sc_in<bool> irq_in;

    SC_CTOR(testbench) : socket("socket"), irq_in("irq_in") {
        SC_THREAD(run_tests);
    }

    void do_write(uint32_t offset, uint32_t value) {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(offset);
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
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
        trans.set_data_ptr(reinterpret_cast<uint8_t*>(&value));
        trans.set_data_length(4);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return value;
    }

    void run_tests() {
        std::cout << "\n=== FX1 I2C Model Test ===" << std::endl;

        // ── HOST MODE TESTS ───────────────────────────────────

        std::cout << "\n--- Test 1: Enable host mode ---" << std::endl;
        do_write(I2C_CTRL, CTRL_ENABLEHOST);
        uint32_t ctrl = do_read(I2C_CTRL);
        std::cout << "CTRL = 0x" << std::hex << ctrl << std::endl;

        std::cout << "\n--- Test 2: Initial STATUS ---" << std::endl;
        uint32_t status = do_read(I2C_STATUS);
        std::cout << "STATUS = 0x" << std::hex << status << std::endl;

        std::cout << "\n--- Test 3: Enable interrupts ---" << std::endl;
        do_write(I2C_INTR_ENABLE, INTR_CMD_COMPLETE);

        std::cout << "\n--- Test 4: Host write transaction ---" << std::endl;
        std::cout << "Sending: START + ADDR=0x50 (write)" << std::endl;
        do_write(I2C_FDATA, FDATA_START | 0xA0);
        std::cout << "Sending: DATA=0x33" << std::endl;
        do_write(I2C_FDATA, 0x33);
        std::cout << "Sending: STOP" << std::endl;
        do_write(I2C_FDATA, FDATA_STOP);

        std::cout << "\n--- Test 5: STATUS after host transaction ---" << std::endl;
        status = do_read(I2C_STATUS);
        std::cout << "STATUS = 0x" << std::hex << status << std::endl;

        std::cout << "\n--- Test 6: INTR_STATE after host transaction ---" << std::endl;
        uint32_t intr = do_read(I2C_INTR_STATE);
        std::cout << "INTR_STATE = 0x" << std::hex << intr << std::endl;

        std::cout << "\n--- Test 7: HOST_FIFO_STATUS ---" << std::endl;
        uint32_t fifo = do_read(I2C_HOST_FIFO_STATUS);
        std::cout << "HOST_FIFO_STATUS = 0x" << std::hex << fifo << std::endl;

        // Clear interrupt
        do_write(I2C_INTR_STATE, INTR_CMD_COMPLETE);

        // ── TARGET MODE TESTS ─────────────────────────────────

        std::cout << "\n--- Test 8: Enable target mode ---" << std::endl;
        do_write(I2C_CTRL, CTRL_ENABLEHOST | CTRL_ENABLETARGET);
        do_write(I2C_TARGET_ID, 0x3C); // listen on address 0x3C
        std::cout << "TARGET_ID = 0x3C" << std::endl;

        std::cout << "\n--- Test 9: Target write transaction ---" << std::endl;
        std::cout << "Simulating master writing 0xAA, 0xBB to our address"
                  << std::endl;
        // This is called by an external master model
        // We call it directly in the testbench to simulate
        std::vector<uint8_t> write_data = {0xAA, 0xBB};
        std::vector<uint8_t> read_data;
        // Access i2c model directly — see sc_main below
        // tb has a pointer to i2c_model for target testing
        i2c_model_ptr->receive_transaction(0x3C, false, write_data, read_data);

        std::cout << "\n--- Test 10: CPU reads ACQ FIFO ---" << std::endl;
        // START entry
        uint32_t acq = do_read(I2C_ACQDATA);
        std::cout << "ACQDATA (START) = 0x" << std::hex << acq << std::endl;
        // data byte 0xAA
        acq = do_read(I2C_ACQDATA);
        std::cout << "ACQDATA (0xAA) = 0x" << std::hex << acq << std::endl;
        // data byte 0xBB
        acq = do_read(I2C_ACQDATA);
        std::cout << "ACQDATA (0xBB) = 0x" << std::hex << acq << std::endl;
        // STOP entry
        acq = do_read(I2C_ACQDATA);
        std::cout << "ACQDATA (STOP) = 0x" << std::hex << acq << std::endl;

        std::cout << "\n--- Test 11: Target read transaction ---" << std::endl;
        std::cout << "CPU loads TX FIFO with 0xCD, 0xEF" << std::endl;
        do_write(I2C_TXDATA, 0xCD);
        do_write(I2C_TXDATA, 0xEF);

        std::vector<uint8_t> write_data2;
        std::vector<uint8_t> read_data2;
        i2c_model_ptr->receive_transaction(0x3C, true, write_data2, read_data2);

        std::cout << "Master received " << std::dec << read_data2.size()
                  << " bytes:" << std::endl;
        for (uint8_t b : read_data2) {
            std::cout << "  0x" << std::hex << (int)b << std::endl;
        }

        std::cout << "\n--- Test 12: TARGET_FIFO_STATUS ---" << std::endl;
        uint32_t tfifo = do_read(I2C_TARGET_FIFO_STATUS);
        std::cout << "TARGET_FIFO_STATUS = 0x" << std::hex << tfifo << std::endl;

        std::cout << "\n--- Test 13: Final STATUS ---" << std::endl;
        status = do_read(I2C_STATUS);
        std::cout << "STATUS = 0x" << std::hex << status << std::endl;

        std::cout << "\n=== All tests complete ===" << std::endl;
        sc_stop();
    }

    // Pointer to i2c model for direct target calls
    i2c* i2c_model_ptr = nullptr;
};

int sc_main(int argc, char* argv[]) {
    i2c       i2c_model("i2c_model");
    testbench tb("testbench");

    // Give testbench access to i2c model for target mode testing
    tb.i2c_model_ptr = &i2c_model;

    sc_signal<bool> irq_sig;
    i2c_model.irq(irq_sig);
    tb.irq_in(irq_sig);

    tb.socket.bind(i2c_model.socket);

    sc_start();
    return 0;
}
