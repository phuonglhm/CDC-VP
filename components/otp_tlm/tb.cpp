#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <cstring>
#include <iomanip>
#include <iostream>

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
    static constexpr uint32_t REG_INTR_STATE               = 0x000;
    static constexpr uint32_t REG_INTR_ENABLE              = 0x004;
    static constexpr uint32_t REG_INTR_TEST                = 0x008;
    static constexpr uint32_t REG_ALERT_TEST               = 0x00C;
    static constexpr uint32_t REG_STATUS                   = 0x010;
    static constexpr uint32_t REG_PARTITION_STATUS_0       = 0x014;
    static constexpr uint32_t REG_ERR_CODE_0               = 0x018;

    static constexpr uint32_t REG_DIRECT_ACCESS_REGWEN     = 0x078;
    static constexpr uint32_t REG_DIRECT_ACCESS_CMD        = 0x07C;
    static constexpr uint32_t REG_DIRECT_ACCESS_ADDRESS    = 0x080;
    static constexpr uint32_t REG_DIRECT_ACCESS_WDATA_0    = 0x084;
    static constexpr uint32_t REG_DIRECT_ACCESS_WDATA_1    = 0x088;
    static constexpr uint32_t REG_DIRECT_ACCESS_RDATA_0    = 0x08C;
    static constexpr uint32_t REG_DIRECT_ACCESS_RDATA_1    = 0x090;

    static constexpr uint32_t REG_CHECK_TRIGGER            = 0x098;
    static constexpr uint32_t REG_INTEGRITY_CHECK_PERIOD   = 0x0A4;
    static constexpr uint32_t REG_CONSISTENCY_CHECK_PERIOD = 0x0A8;

    static constexpr uint32_t REG_VENDOR_TEST_READ_LOCK    = 0x0AC;

    static constexpr uint32_t REG_MODEL_SIZE_WORDS         = 0x200;
    static constexpr uint32_t REG_MODEL_PARTITION_SIZE     = 0x204;

    static constexpr uint32_t INTR_OTP_OPERATION_DONE      = 1u << 0;
    static constexpr uint32_t INTR_OTP_ERROR               = 1u << 1;

    static constexpr uint32_t DAI_CMD_READ                 = 0x1;
    static constexpr uint32_t DAI_CMD_WRITE                = 0x2;
    static constexpr uint32_t DAI_CMD_DIGEST               = 0x4;

private:
    void run()
    {
        std::cout << "\n[TB] Start OpenTitan-like OTP TLM test\n\n";

        dump_basic_info();

        write32(REG_INTR_ENABLE, INTR_OTP_OPERATION_DONE | INTR_OTP_ERROR);

        std::cout << "\n[TB] Test 1: DAI write word0 = 0x12345678\n";
        dai_write_word(0, 0x12345678);
        expect_no_error();

        std::cout << "\n[TB] Test 2: DAI read word0\n";
        uint32_t rdata = dai_read_word(0);

        if (rdata == 0x12345678) {
            std::cout << "[TB] PASS: read data matched\n";
        } else {
            std::cout << "[TB] FAIL: read data mismatch\n";
        }

        expect_no_error();
        clear_intr();

        std::cout << "\n[TB] Test 3: illegal program 1 -> 0\n";
        dai_write_word(0, 0x00000000);
        expect_error(0x4, "MACRO_WRITE_BLANK_ERROR");

        clear_intr();

        std::cout << "\n[TB] Test 4: read lock partition 0\n";
        write32(REG_VENDOR_TEST_READ_LOCK, 0);
        dai_read_word(0);
        expect_error(0x5, "ACCESS_ERROR by read lock");

        clear_intr();

        std::cout << "\n[TB] Test 5: digest locks partition 1 for write\n";

        // Partition size default = 64 words.
        // Partition 1 starts at word 64.
        // DAI address is byte address, so word64 => 64 * 4 = 0x100.
        uint32_t part1_byte_addr = 64 * 4;

        dai_write_word(part1_byte_addr, 0xAAAAAAAA);
        expect_no_error();
        clear_intr();

        write32(REG_DIRECT_ACCESS_ADDRESS, part1_byte_addr);
        write32(REG_DIRECT_ACCESS_CMD, DAI_CMD_DIGEST);
        wait(10, SC_NS);

        std::cout << "[TB] PARTITION_STATUS_0 = 0x"
                  << std::hex << read32(REG_PARTITION_STATUS_0)
                  << std::dec << "\n";

        clear_intr();

        dai_write_word(part1_byte_addr, 0xFFFFFFFF);
        expect_error(0x5, "ACCESS_ERROR by digest lock");

        clear_intr();

        std::cout << "\n[TB] Test 6: CHECK_TRIGGER pass\n";
        write32(REG_INTEGRITY_CHECK_PERIOD, 0x100);
        write32(REG_CONSISTENCY_CHECK_PERIOD, 0x200);
        write32(REG_CHECK_TRIGGER, 0x3);

        std::cout << "[TB] STATUS = 0x"
                  << std::hex << read32(REG_STATUS)
                  << std::dec << "\n";

        expect_no_error();
        clear_intr();

        std::cout << "\n[TB] Test 7: address out of range\n";
        dai_read_word(9999 * 4);
        expect_error(0x5, "ACCESS_ERROR by address out of range");

        std::cout << "\n[TB] OpenTitan-like OTP TLM test finished\n\n";
        sc_stop();
    }

    void dump_basic_info()
    {
        std::cout << "[TB] MODEL_SIZE_WORDS     = "
                  << read32(REG_MODEL_SIZE_WORDS) << "\n";
        std::cout << "[TB] MODEL_PARTITION_SIZE = "
                  << read32(REG_MODEL_PARTITION_SIZE) << "\n";
        std::cout << "[TB] STATUS               = 0x"
                  << std::hex << read32(REG_STATUS) << std::dec << "\n";
        std::cout << "[TB] DIRECT_ACCESS_REGWEN = 0x"
                  << std::hex << read32(REG_DIRECT_ACCESS_REGWEN)
                  << std::dec << "\n";
    }

    void dai_write_word(uint32_t byte_addr, uint32_t data)
    {
        write32(REG_DIRECT_ACCESS_WDATA_0, data);
        write32(REG_DIRECT_ACCESS_WDATA_1, 0);
        write32(REG_DIRECT_ACCESS_ADDRESS, byte_addr);
        write32(REG_DIRECT_ACCESS_CMD, DAI_CMD_WRITE);

        std::cout << "[TB] STATUS     = 0x"
                  << std::hex << read32(REG_STATUS) << "\n";
        std::cout << "[TB] ERR_CODE_0 = 0x"
                  << read32(REG_ERR_CODE_0) << std::dec << "\n";
    }

    uint32_t dai_read_word(uint32_t byte_addr)
    {
        write32(REG_DIRECT_ACCESS_ADDRESS, byte_addr);
        write32(REG_DIRECT_ACCESS_CMD, DAI_CMD_READ);

        uint32_t data = read32(REG_DIRECT_ACCESS_RDATA_0);

        std::cout << "[TB] RDATA_0    = 0x"
                  << std::hex << data << "\n";
        std::cout << "[TB] STATUS     = 0x"
                  << read32(REG_STATUS) << "\n";
        std::cout << "[TB] ERR_CODE_0 = 0x"
                  << read32(REG_ERR_CODE_0) << std::dec << "\n";

        return data;
    }

    void expect_no_error()
    {
        uint32_t err = read32(REG_ERR_CODE_0);
        uint32_t status = read32(REG_STATUS);

        if (err == 0 && (status & 0xE) == 0) {
            std::cout << "[TB] PASS: no error\n";
        } else {
            std::cout << "[TB] FAIL: unexpected error, STATUS=0x"
                      << std::hex << status
                      << " ERR_CODE_0=0x" << err
                      << std::dec << "\n";
        }
    }

    void expect_error(uint32_t expected, const char* name)
    {
        uint32_t err = read32(REG_ERR_CODE_0);

        if (err == expected) {
            std::cout << "[TB] PASS: " << name
                      << " detected, ERR_CODE_0=0x"
                      << std::hex << err << std::dec << "\n";
        } else {
            std::cout << "[TB] FAIL: expected ERR_CODE_0=0x"
                      << std::hex << expected
                      << ", got 0x" << err << std::dec << "\n";
        }
    }

    void clear_intr()
    {
        write32(REG_INTR_STATE, INTR_OTP_OPERATION_DONE | INTR_OTP_ERROR);
    }

    void irq_monitor()
    {
        std::cout << "[TB] IRQ changed to " << irq_in.read()
                  << " at " << sc_time_stamp() << "\n";
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
            std::cout << "[TB] WRITE response error at 0x"
                      << std::hex << addr << std::dec << "\n";
        }
    }

    uint32_t read32(uint32_t addr)
    {
        tlm::tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;

        uint32_t data = 0;
        unsigned char buf[4] = {0};

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
            std::cout << "[TB] READ response error at 0x"
                      << std::hex << addr << std::dec << "\n";
        }

        return data;
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
