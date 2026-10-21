#include "fu3/gpio/subsystem.h"

namespace fu3::gpio {
Subsystem::Subsystem(sc_core::sc_module_name name) : sc_module(name) {
    Gpio* banks[] = {&gpioa, &gpiob, &gpioc};
    for (unsigned b = 0; b < 3; ++b) {
        auto& g = *banks[b];
        g.clk(clk);
        g.reset_n(reset_n);
        g.gpio_in(raw_[b]);
        g.gpio_out(out_[b]);
        g.gpio_dir(dir_[b]);
        g.gpio_in_sync(input_sync[b]);
        g.interrupt(irqs_[b]);
        g.power_event(wakes_[b]);
        for (unsigned i = 0; i < 32; ++i)
            g.gpio_padcfg[i](pad_config[b * 32 + i]);
    }
    for (unsigned i = 0; i < 80; ++i) {
        io_pad.primary_out[i](p_out_[i]);
        io_pad.alternate_out[i](a_out_[i]);
        io_pad.primary_oe[i](p_oe_[i]);
        io_pad.alternate_oe[i](a_oe_[i]);
        io_pad.primary_in[i](primary_in[i]);
        io_pad.alternate_in[i](alternate_in[i]);
    }
    SC_METHOD(route_gpio);
    for (unsigned b = 0; b < 3; ++b)
        sensitive << out_[b] << dir_[b] << unbonded_in[b];
    for (unsigned i = 0; i < 80; ++i)
        sensitive << primary_in[i] << alternate_in[i] << peripheral_primary_out[i]
                  << peripheral_alternate_out[i] << peripheral_primary_oe[i]
                  << peripheral_alternate_oe[i];
    SC_METHOD(publish);
    for (unsigned b = 0; b < 3; ++b)
        sensitive << irqs_[b] << wakes_[b];
}
void Subsystem::route_gpio() {
    std::uint32_t raw[3];
    for (unsigned b = 0; b < 3; ++b)
        raw[b] = unbonded_in[b].read().to_uint() & 0xffffff00;
    for (unsigned i = 0; i < 80; ++i) {
        bool p = peripheral_primary_out[i].read(), a = peripheral_alternate_out[i].read();
        bool po = peripheral_primary_oe[i].read(), ao = peripheral_alternate_oe[i].read();
        const auto info = pin_info(i + 1);
        if (info.gpio_bank >= 0) {
            const auto b = unsigned(info.gpio_bank), bit = unsigned(info.gpio_bit);
            const bool data = out_[b].read()[bit], oe = dir_[b].read()[bit];
            if (info.gpio_select) {
                a = data;
                ao = oe;
            } else {
                p = data;
                po = oe;
            }
            const bool in = info.gpio_select ? alternate_in[i].read() : primary_in[i].read();
            raw[b] |= std::uint32_t(in) << bit;
        }
        p_out_[i].write(p);
        a_out_[i].write(a);
        p_oe_[i].write(po);
        a_oe_[i].write(ao);
    }
    for (unsigned b = 0; b < 3; ++b)
        raw_[b].write(raw[b]);
}
void Subsystem::publish() {
    bool any = false;
    for (unsigned b = 0; b < 3; ++b) {
        irq[b].write(irqs_[b].read());
        wake_bank[b].write(wakes_[b].read());
        any |= wakes_[b].read();
    }
    wake.write(any);
}
} // namespace fu3::gpio
