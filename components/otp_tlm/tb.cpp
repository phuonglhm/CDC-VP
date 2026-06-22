#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <iostream>
#include <iomanip>
#include <cstring>

#include "otp.h"

using namespace sc_core;

class tb : public sc_module {
public:
    tlm_utils::simple_initiator_socket<tb> socket;
    sc_in<bool> irq_in;

    SC_HAS_PROCESS(tb);

    tb(sc_module_name name)
        : sc_module(name)
        , socket("socket")
        , irq_in("irq_in")
    {
        SC_THREAD(run);
        SC_METHOD(irq_monitor);
        sensitive << irq_in;
        dont_initialize();
    }

private:
    // Register offset
    static constexpr uint32_t REG_CTRL           = 0x00;
    static constexpr uint32_t REG_STATUS         = 0x04;
    static constexpr uint32_t REG_ADDR           = 0x08;
    static constexpr uint32_t REG_WDATA          = 0x0C;
    static constexpr uint32_t REG_RDATA          = 0x10;
    static constexpr uint32_t REG_LOCK           = 0x14;
    static constexpr uint32_t REG_READ_LOCK      = 0x18;
    static constexpr uint32_t REG_ERR_STATUS     = 0x1C;
    static constexpr uint32_t REG_INTR_ENABLE    = 0x20;
    static constexpr uint32_t REG_INTR_STATE     = 0x24;
    static constexpr uint32_t REG_SIZE_WORDS     = 0x28;
    static constexpr uint32_t REG_PARTITION_SIZE = 0x2C;

    // CTRL bits
    static constexpr uint32_t CTRL_READ_START = 1u << 0;
    static constexpr uint32_t CTRL_PROG_START = 1u << 1;
    static constexpr uint32_t CTRL_CLEAR_IRQ  = 1u << 2;

    // STATUS bits
    static constexpr uint32_t STATUS_BUSY  = 1u << 0;
    static constexpr uint32_t STATUS_DONE  = 1u << 1;
    static constexpr uint32_t STATUS_ERROR = 1u << 2;
    static constexpr uint32_t STATUS_IRQ   = 1u << 3;

private:
    void run()
    {
        std::cout << "\n[TB] Start OTP TLM test\n" << std::endl;

        uint32_t size = read32(REG_SIZE_WORDS);
        uint32_t part_size = read32(REG_PARTITION_SIZE);

        std::cout << "[TB] OTP size words      = " << size << std::endl;
        std::cout << "[TB] OTP partition words = " << part_size << std::endl;

        write32(REG_INTR_ENABLE, 1);

        std::cout << "\n[TB] Test 1: program word 0 = 0x12345678" << std::endl;
        otp_program(0, 0x12345678);

        std::cout << "\n[TB] Test 2: read word 0" << std::endl;
        uint32_t data = otp_read(0);

        if (data == 0x12345678) {
            std::cout << "[TB] PASS: read data matched" << std::endl;
        } else {
            std::cout << "[TB] FAIL: read data mismatch" << std::endl;
        }

        clear_irq();

        std::cout << "\n[TB] Test 3: illegal program 1 -> 0" << std::endl;
        otp_program(0, 0x00000000);

        if (has_error()) {
            std::cout << "[TB] PASS: illegal 1 -> 0 detected" << std::endl;
            std::cout << "[TB] ERR_STATUS = " << read32(REG_ERR_STATUS) << std::endl;
        } else {
            std::cout << "[TB] FAIL: illegal 1 -> 0 not detected" << std::endl;
        }

        clear_error();
        clear_irq();

        std::cout << "\n[TB] Test 4: lock partition 0" << std::endl;
        write32(REG_LOCK, 1u << 0);

        std::cout << "[TB] Try program word 1 after lock" << std::endl;
        otp_program(1, 0xAAAAAAAA);

        if (has_error()) {
            std::cout << "[TB] PASS: write lock detected" << std::endl;
            std::cout << "[TB] ERR_STATUS = " << read32(REG_ERR_STATUS) << std::endl;
        } else {
            std::cout << "[TB] FAIL: write lock not detected" << std::endl;
        }

        clear_error();
        clear_irq();

        std::cout << "\n[TB] Test 5: read lock partition 0" << std::endl;
        write32(REG_READ_LOCK, 1u << 0);

        std::cout << "[TB] Try read word 0 after read lock" << std::endl;
        data = otp_read(0);

        if (has_error()) {
            std::cout << "[TB] PASS: read lock detected" << std::endl;
            std::cout << "[TB] ERR_STATUS = " << read32(REG_ERR_STATUS) << std::endl;
        } else {
            std::cout << "[TB] FAIL: read lock not detected" << std::endl;
        }

        clear_error();
        clear_irq();

        std::cout << "\n[TB] Test 6: address out of range" << std::endl;
        otp_read(9999);

        if (has_error()) {
            std::cout << "[TB] PASS: address out of range detected" << std::endl;
            std::cout << "[TB] ERR_STATUS = " << read32(REG_ERR_STATUS) << std::endl;
        } else {
            std::cout << "[TB] FAIL: address out of range not detected" << std::endl;
        }

        std::cout << "\n[TB] OTP TLM test finished\n" << std::endl;

        sc_stop();
    }

    void irq_monitor()
    {
        std::cout << "[TB] IRQ changed to " << irq_in.read()
                  << " at " << sc_time_stamp()
                  << std::endl;
    }

    void write32(uint32_t addr, uint32_t data)
    {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;

        unsigned char buf[4];
        std::memcpy(buf, &data, 4);

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(buf);
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(trans, delay);
        wait(delay);

        if (trans.is_response_error()) {
            std::cout << "[TB] WRITE response error at addr 0x"
                      << std::hex << addr << std::dec
                      << std::endl;
        }
    }

    uint32_t read32(uint32_t addr)
    {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;

        uint32_t data = 0;
        unsigned char buf[4];
        std::memset(buf, 0, 4);

        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(buf);
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        socket->b_transport(trans, delay);
        wait(delay);

        std::memcpy(&data, buf, 4);

        if (trans.is_response_error()) {
            std::cout << "[TB] READ response error at addr 0x"
                      << std::hex << addr << std::dec
                      << std::endl;
        }

        return data;
    }

    void otp_program(uint32_t word_addr, uint32_t data)
    {
        write32(REG_ADDR, word_addr);
        write32(REG_WDATA, data);
        write32(REG_CTRL, CTRL_PROG_START);

        wait_until_not_busy();

        uint32_t status = read32(REG_STATUS);

        std::cout << "[TB] STATUS = 0x"
                  << std::hex << status << std::dec
                  << std::endl;
    }

    uint32_t otp_read(uint32_t word_addr)
    {
        write32(REG_ADDR, word_addr);
        write32(REG_CTRL, CTRL_READ_START);

        wait_until_not_busy();

        uint32_t data = read32(REG_RDATA);
        uint32_t status = read32(REG_STATUS);

        std::cout << "[TB] RDATA  = 0x"
                  << std::hex << data << std::dec
                  << std::endl;

        std::cout << "[TB] STATUS = 0x"
                  << std::hex << status << std::dec
                  << std::endl;

        return data;
    }

    void wait_until_not_busy()
    {
        while (read32(REG_STATUS) & STATUS_BUSY) {
            wait(10, SC_NS);
        }
    }

    bool has_error()
    {
        return (read32(REG_STATUS) & STATUS_ERROR) != 0;
    }

    void clear_error()
    {
        write32(REG_ERR_STATUS, 1);
    }

    void clear_irq()
    {
        write32(REG_INTR_STATE, 1);
        write32(REG_CTRL, CTRL_CLEAR_IRQ);
    }
};

int sc_main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    otp otp0("otp0", 256, 64);
    tb tb0("tb0");

    sc_signal<bool> irq_sig;

    tb0.socket.bind(otp0.socket);
    otp0.irq_out(irq_sig);
    tb0.irq_in(irq_sig);

    sc_start();

    return 0;
}
