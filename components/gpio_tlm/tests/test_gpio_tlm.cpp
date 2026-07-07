// Unit test for gpio_tlm: register access, direction handling, external pins.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <gpio_tlm.h>

namespace {

class tb : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<tb> socket;
    cdc::components::gpio_tlm* dut = nullptr;

    SC_HAS_PROCESS(tb);
    explicit tb(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket") {
        SC_THREAD(run);
    }

    std::uint32_t read32(std::uint64_t addr) {
        tlm::tlm_generic_payload trans;
        std::uint32_t data = 0;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        socket->b_transport(trans, delay);
        assert(trans.is_response_ok());
        return data;
    }

    void write32(std::uint64_t addr, std::uint32_t data, bool expect_ok = true) {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
        trans.set_data_length(4);
        socket->b_transport(trans, delay);
        assert(trans.is_response_ok() == expect_ok);
    }

    void run() {
        using gpio = cdc::components::gpio_tlm;

        // Reset: all inputs, everything low.
        assert(read32(gpio::kDirOffset) == 0u);
        assert(read32(gpio::kValueOffset) == 0u);

        // External stimulus shows up on input pins.
        dut->set_pin(1, true);
        assert(read32(gpio::kValueOffset) == (1u << 1));
        assert(dut->pin(1));

        // OUT does not affect VALUE while the pin is an input.
        write32(gpio::kOutOffset, 1u << 1);
        dut->set_pin(1, false);
        assert(read32(gpio::kValueOffset) == 0u);

        // Switch pin 1 to output: VALUE now reflects OUT, not ext stimulus.
        write32(gpio::kDirOffset, 1u << 1);
        assert(read32(gpio::kValueOffset) == (1u << 1));
        dut->set_pin(1, true);
        write32(gpio::kOutOffset, 0);
        assert((read32(gpio::kValueOffset) & (1u << 1)) == 0u);

        // VALUE is read-only.
        write32(gpio::kValueOffset, 0xFFFFFFFFu, /*expect_ok=*/false);

        std::cout << "gpio_tlm test PASS\n";
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char*[])
{
    cdc::components::gpio_tlm gpio("gpio");
    tb bench("bench");
    bench.dut = &gpio;
    bench.socket.bind(gpio.socket);
    sc_core::sc_start();
    return 0;
}
