#pragma once
#include "fu3/gpio/detail/state.h"
#include <systemc>

namespace fu3::gpio {
// GPIO HDS 7.1: default PADDR width 12, decoder uses only [5:2].
// sc_biguint permits any specified address width of at least six bits.
template <unsigned AddressWidth = 12> class ApbGpio : public sc_core::sc_module {
    static_assert(AddressWidth >= 6, "GPIO APB address width must include bits [5:2]");

  public:
    sc_core::sc_in<bool> HCLK{"HCLK"}, HRESETn{"HRESETn"}, PSEL{"PSEL"}, PENABLE{"PENABLE"},
        PWRITE{"PWRITE"};
    sc_core::sc_in<sc_dt::sc_biguint<AddressWidth>> PADDR{"PADDR"};
    sc_core::sc_in<sc_dt::sc_uint<32>> PWDATA{"PWDATA"}, gpio_in{"gpio_in"};
    sc_core::sc_out<sc_dt::sc_uint<32>> PRDATA{"PRDATA"}, gpio_out{"gpio_out"},
        gpio_dir{"gpio_dir"}, gpio_in_sync{"gpio_in_sync"};
    sc_core::sc_out<bool> PREADY{"PREADY"}, PSLVERR{"PSLVERR"}, interrupt{"interrupt"},
        power_event{"power_event"};
    sc_core::sc_vector<sc_core::sc_out<sc_dt::sc_uint<6>>> gpio_padcfg{"gpio_padcfg", 32};
    SC_HAS_PROCESS(ApbGpio);
    explicit ApbGpio(sc_core::sc_module_name name) : sc_module(name) {
        SC_METHOD(tick);
        sensitive << HCLK.pos() << HRESETn.neg();
        SC_METHOD(outputs);
        sensitive << changed_ << PADDR << gpio_in;
    }

  private:
    detail::State state_;
    sc_core::sc_event changed_;
    unsigned index() const {
        return PADDR.read().range(5, 2).to_uint();
    }
    void tick() {
        if (!HRESETn.read())
            state_.reset();
        else if (HCLK.posedge()) {
            const bool access = PSEL.read() && PENABLE.read();
            state_.edge(gpio_in.read().to_uint(), access && !PWRITE.read(), access && PWRITE.read(),
                        index(), PWDATA.read().to_uint());
        }
        changed_.notify(sc_core::SC_ZERO_TIME);
    }
    void outputs() {
        PRDATA.write(state_.read(index()));
        PREADY.write(true);
        PSLVERR.write(false);
        gpio_out.write(state_.read(reg::PADOUT / 4));
        gpio_dir.write(state_.read(reg::PADDIR / 4));
        gpio_in_sync.write(state_.synced());
        interrupt.write(state_.irq());
        power_event.write(state_.wake(gpio_in.read().to_uint()));
        for (unsigned i = 0; i < 32; ++i)
            gpio_padcfg[i].write(state_.padcfg(i));
    }
};
} // namespace fu3::gpio
