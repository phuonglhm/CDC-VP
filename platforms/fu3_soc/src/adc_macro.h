#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <systemc>

namespace cdc::platforms::fu3_soc {

// The ADC hardmacro behind the FU3 ADC controller (decision Q07-b: VP source
// per logical channel, no analog physics). After each falling SAMPCLK while the
// macro is enabled (reset released, PWD low), D carries the channel's value 12
// ADC clocks later and EOC rises half a clock after D, as the controller's
// contract describes (HDS p11 normal waveform); EOC falls 2.5 clocks after the
// next SAMPCLK rise. A channel without a configured value converts to 0 and is
// reported once (missing stimulus, not a measured value).
class adc_macro : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> sampclk{"sampclk"}, cs{"cs"}, pwd{"pwd"}, macro_reset{"macro_reset"};
    sc_core::sc_in<sc_dt::sc_uint<4>> channel{"channel"};
    sc_core::sc_out<bool> eoc{"eoc"};
    sc_core::sc_out<sc_dt::sc_uint<12>> data{"data"};

    SC_HAS_PROCESS(adc_macro);
    adc_macro(sc_core::sc_module_name name, sc_core::sc_time adc_period)
        : sc_core::sc_module(name), period_(adc_period)
    {
        SC_THREAD(convert);
    }
    // Value a channel converts to (12 bits).
    void set_input(unsigned channel, std::uint16_t value)
    {
        if (channel < 16) {
            value_[channel] = value & 0xfff;
            configured_[channel] = true;
        }
    }
    std::uint64_t conversions() const { return conversions_; }

private:
    const sc_core::sc_time period_;
    std::array<std::uint16_t, 16> value_{};
    std::array<bool, 16> configured_{}, reported_{};
    std::uint64_t conversions_ = 0;

    bool cancelled() const { return macro_reset.read() || pwd.read(); }
    // A wait that ends early on macro reset or power-down. The event OR list is
    // built for every wait: an sc_event_or_expr may be used only once.
    void wait_or_abort(const sc_core::sc_time& t) { wait(t, macro_reset.posedge_event() | pwd.posedge_event()); }
    void convert()
    {
        eoc.write(false);
        data.write(0);
        while (true) {
            wait(sampclk.negedge_event());
            if (cancelled())
                continue;
            const unsigned ch = channel.read().to_uint();
            if (!configured_[ch] && !reported_[ch]) {
                reported_[ch] = true;
                SC_REPORT_WARNING("fu3_soc/adc_stimulus",
                                  ("no stimulus for ADC channel " + std::to_string(ch) + ": converts to 0").c_str());
            }
            wait_or_abort(period_ * 12);
            if (cancelled()) {
                eoc.write(false);
                continue;
            }
            data.write(value_[ch]);
            wait_or_abort(period_ / 2);
            if (cancelled())
                continue;
            ++conversions_;
            eoc.write(true);
            wait(sampclk.posedge_event() | macro_reset.posedge_event() | pwd.posedge_event());
            if (!cancelled())
                wait_or_abort(period_ * 2.5);
            eoc.write(false);
        }
    }
};

} // namespace cdc::platforms::fu3_soc
