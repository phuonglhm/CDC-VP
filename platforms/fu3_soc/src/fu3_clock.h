#pragma once

#include <systemc>

namespace cdc::platforms::fu3_soc {

// Clock for an IP model that runs on clock edges (GPIO, AES, QSPI, ADC):
// toggles `clk` with the period the platform sets from the CRM, and holds it
// low while `enable` is low (CRM clock gate) or the period is zero (stopped).
// Each running cycle costs two kernel events, so the platform keeps the
// clock of a gated block stopped.
class fu3_clock : public sc_core::sc_module {
public:
    sc_core::sc_out<bool> clk{"clk"};
    sc_core::sc_in<bool> enable{"enable"};

    SC_HAS_PROCESS(fu3_clock);
    explicit fu3_clock(sc_core::sc_module_name name) : sc_core::sc_module(name)
    {
        SC_THREAD(run);
    }
    // New period, used from the next edge; SC_ZERO_TIME stops the clock.
    void set_period(sc_core::sc_time period)
    {
        period_ = period;
        changed_.notify(sc_core::SC_ZERO_TIME);
    }
    sc_core::sc_time period() const { return period_; }

private:
    sc_core::sc_time period_ = sc_core::SC_ZERO_TIME;
    sc_core::sc_event changed_;

    void run()
    {
        while (true) {
            if (!enable.read() || period_ == sc_core::SC_ZERO_TIME) {
                clk.write(false);
                wait(changed_ | enable.value_changed_event());
                continue;
            }
            clk.write(true);
            wait(period_ / 2);
            clk.write(false);
            wait(period_ / 2);
        }
    }
};

} // namespace cdc::platforms::fu3_soc
