#pragma once
#include <systemc>

namespace fu3::io_pad {
// Package positions use 1-based numbering. Vectors use position-1.
struct Pin {
    bool digital = false, input_only = false, configurable = false, reserved = false;
    int gpio_bank = -1, gpio_bit = -1;
    unsigned gpio_select = 0;
};
Pin pin_info(unsigned position);

// Logical IOSEL partition: 40 package positions, combinational, no bus registers.
class IoSel : public sc_core::sc_module {
  public:
    sc_core::sc_in<bool> test_mode{"test_mode"};
    sc_core::sc_vector<sc_core::sc_in<sc_dt::sc_uint<2>>> select{"select", 40};
    sc_core::sc_vector<sc_core::sc_in<bool>> pad_in{"pad_in", 40}, primary_out{"primary_out", 40},
        alternate_out{"alternate_out", 40}, primary_oe{"primary_oe", 40},
        alternate_oe{"alternate_oe", 40};
    sc_core::sc_vector<sc_core::sc_out<bool>> pad_data{"pad_data", 40},
        requested_oe{"requested_oe", 40}, primary_in{"primary_in", 40},
        alternate_in{"alternate_in", 40};
    SC_HAS_PROCESS(IoSel);
    IoSel(sc_core::sc_module_name name, unsigned first_position);

  private:
    unsigned first_;
    void route();
};

class IoPad : public sc_core::sc_module {
  public:
    sc_core::sc_vector<sc_core::sc_in<sc_dt::sc_uint<2>>> select{"select", 80};
    sc_core::sc_vector<sc_core::sc_in<bool>> pad_in{"pad_in", 80}, primary_out{"primary_out", 80},
        alternate_out{"alternate_out", 80}, primary_oe{"primary_oe", 80},
        alternate_oe{"alternate_oe", 80};
    sc_core::sc_vector<sc_core::sc_out<bool>> pad_data{"pad_data", 80},
        requested_oe{"requested_oe", 80}, primary_in{"primary_in", 80},
        alternate_in{"alternate_in", 80};
    sc_core::sc_out<bool> test_mode{"test_mode"}, test_select{"test_select"};
    IoSel iosel0{"iosel0", 1}, iosel1{"iosel1", 41};
    SC_HAS_PROCESS(IoPad);
    explicit IoPad(sc_core::sc_module_name name);

  private:
    sc_core::sc_signal<bool> test_{"test"};
    void decode_test();
};
} // namespace fu3::io_pad
