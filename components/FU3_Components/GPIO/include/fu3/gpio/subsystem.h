#pragma once
#include "fu3/gpio/gpio.h"
#include "fu3/gpio/io_pad.h"

namespace fu3::gpio {
// GPIO banks remain 32-bit IPs; IO_PAD bonds the low eight bits of each bank.
class Subsystem : public sc_core::sc_module {
  public:
    Gpio gpioa{"gpioa"}, gpiob{"gpiob"}, gpioc{"gpioc"};
    IoPad io_pad{"io_pad"};
    sc_core::sc_in<bool> clk{"clk"}, reset_n{"reset_n"};
    sc_core::sc_vector<sc_core::sc_out<bool>> irq{"irq", 3}, wake_bank{"wake_bank", 3};
    // Board policy: OR of three power_event outputs, no invented WAKE CSR.
    sc_core::sc_out<bool> wake{"wake"};
    // Set bits 8..31 here for core-level use; unbonded pins default to zero.
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<32>>> unbonded_in{"unbonded_in", 3};
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<32>>> input_sync{"input_sync", 3};
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<6>>> pad_config{"pad_config", 96};
    // Non-GPIO functions are identified by package position, avoiding guesses
    // about JTAG / SPI signal order absent from the supplied pin-map table.
    sc_core::sc_vector<sc_core::sc_signal<bool>> peripheral_primary_out{"peripheral_primary_out",
                                                                        80},
        peripheral_alternate_out{"peripheral_alternate_out", 80},
        peripheral_primary_oe{"peripheral_primary_oe", 80},
        peripheral_alternate_oe{"peripheral_alternate_oe", 80};
    sc_core::sc_vector<sc_core::sc_signal<bool>> primary_in{"primary_in", 80},
        alternate_in{"alternate_in", 80};
    SC_HAS_PROCESS(Subsystem);
    explicit Subsystem(sc_core::sc_module_name name);

  private:
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<32>>> raw_{"raw", 3}, out_{"out", 3},
        dir_{"dir", 3};
    sc_core::sc_vector<sc_core::sc_signal<bool>> irqs_{"irqs", 3}, wakes_{"wakes", 3};
    sc_core::sc_vector<sc_core::sc_signal<bool>> p_out_{"p_out", 80}, a_out_{"a_out", 80},
        p_oe_{"p_oe", 80}, a_oe_{"a_oe", 80};
    void route_gpio();
    void publish();
};
} // namespace fu3::gpio
