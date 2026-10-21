#include "fu3/io_pad/io_pad.h"
#include <stdexcept>

namespace fu3::io_pad {
Pin pin_info(unsigned p) {
    Pin r;
    r.digital = (p >= 1 && p <= 6) || (p >= 11 && p <= 15) || (p >= 20 && p <= 30) ||
                (p >= 44 && p <= 47) || (p >= 54 && p <= 62) || (p >= 67 && p <= 76);
    r.input_only = p == 1 || p == 5 || p == 11;
    r.reserved = p == 23 || p == 24;
    r.configurable = p == 6 || (p >= 12 && p <= 15) || (p >= 20 && p <= 22) ||
                     (p >= 25 && p <= 29) || p == 46 || p == 47;
    if (p == 6) {
        r.gpio_bank = 0;
        r.gpio_bit = 0;
        r.gpio_select = 1;
    }
    if (p >= 12 && p <= 15) {
        r.gpio_bank = 0;
        r.gpio_bit = int(p) - 11;
        r.gpio_select = 1;
    }
    if (p >= 20 && p <= 22) {
        r.gpio_bank = 0;
        r.gpio_bit = int(p) - 15;
    }
    if (p >= 25 && p <= 30) {
        r.gpio_bank = 1;
        r.gpio_bit = int(p) - 25;
    }
    if (p >= 44 && p <= 45) {
        r.gpio_bank = 1;
        r.gpio_bit = int(p) - 38;
    }
    if (p >= 46 && p <= 47) {
        r.gpio_bank = 2;
        r.gpio_bit = int(p) - 46;
        r.gpio_select = 1;
    }
    if (p >= 54 && p <= 56) {
        r.gpio_bank = 2;
        r.gpio_bit = int(p) - 52;
    }
    if (p >= 59 && p <= 61) {
        r.gpio_bank = 2;
        r.gpio_bit = int(p) - 54;
    }
    return r;
}
IoSel::IoSel(sc_core::sc_module_name name, unsigned first_position)
    : sc_module(name), first_(first_position) {
    if (first_ != 1 && first_ != 41)
        throw std::invalid_argument("IOSEL partition starts at package position 1 or 41");
    SC_METHOD(route);
    sensitive << test_mode;
    for (unsigned i = 0; i < 40; ++i)
        sensitive << select[i] << pad_in[i] << primary_out[i] << alternate_out[i] << primary_oe[i]
                  << alternate_oe[i];
}
void IoSel::route() {
    for (unsigned i = 0; i < 40; ++i) {
        const auto p = first_ + i;
        const auto info = pin_info(p);
        const auto s = info.configurable ? select[i].read().to_uint() : 0;
        bool data = false, oe = false, primary = false, alternate = false;
        if (info.digital && !info.reserved && (!test_mode.read() || p == 5 || p == 11)) {
            if (s == 0) {
                data = primary_out[i].read();
                oe = primary_oe[i].read();
                primary = pad_in[i].read();
            } else if (s == 1) {
                data = alternate_out[i].read();
                oe = alternate_oe[i].read();
                alternate = pad_in[i].read();
            }
        }
        if (info.input_only) {
            data = false;
            oe = false;
        }
        pad_data[i].write(data);
        requested_oe[i].write(oe);
        primary_in[i].write(primary);
        alternate_in[i].write(alternate);
    }
}
IoPad::IoPad(sc_core::sc_module_name name) : sc_module(name) {
    iosel0.test_mode(test_);
    iosel1.test_mode(test_);
    for (unsigned i = 0; i < 80; ++i) {
        auto& part = i < 40 ? iosel0 : iosel1;
        const auto k = i % 40;
        part.select[k](select[i]);
        part.pad_in[k](pad_in[i]);
        part.primary_out[k](primary_out[i]);
        part.alternate_out[k](alternate_out[i]);
        part.primary_oe[k](primary_oe[i]);
        part.alternate_oe[k](alternate_oe[i]);
        part.pad_data[k](pad_data[i]);
        part.requested_oe[k](requested_oe[i]);
        part.primary_in[k](primary_in[i]);
        part.alternate_in[k](alternate_in[i]);
    }
    SC_METHOD(decode_test);
    sensitive << pad_in[4] << pad_in[10];
}
void IoPad::decode_test() {
    const bool mode = pad_in[4].read();
    test_.write(mode);
    test_mode.write(mode);
    test_select.write(mode && pad_in[10].read());
}
} // namespace fu3::io_pad
