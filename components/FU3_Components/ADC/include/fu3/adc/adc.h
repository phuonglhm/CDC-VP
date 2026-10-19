#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <array>
#include <cstdint>

namespace fu3::adc {
namespace reg {
inline constexpr std::uint32_t VERSION = 0x00, CTRL = 0x04, CFG = 0x08,
    STATUS = 0x0c, DATA = 0x10, TIMING = 0x14, SAMPLE_CFG = 0x18;
inline constexpr std::uint32_t sample(unsigned n) { return 0x1c + 4 * n; }
inline constexpr std::uint32_t DONE = 1, READY = 2, BUSY = 4;
inline constexpr std::uint64_t BASE = 0x500a0000, WINDOW = 0x10000;
}

// Digital controller only. Bind a hardmacro model (or testbench) to D/EOC.
// adc_clk is the platform-generated 10 MHz clock, not the 80 MHz APB clock.
class Adc : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Adc> socket{"socket"};
    sc_core::sc_in<bool> adc_clk{"adc_clk"}, reset_n{"reset_n"}, eoc{"eoc"};
    sc_core::sc_in<sc_dt::sc_uint<12>> data_in{"data_in"};
    sc_core::sc_out<bool> sampclk{"sampclk"}, cs{"cs"}, pwd{"pwd"},
        macro_reset{"macro_reset"}, samp_sel{"samp_sel"};
    sc_core::sc_out<sc_dt::sc_uint<4>> channel{"channel"};
    enum class State { PowerDown, SetupWait, SampleHigh, SampleLow, WaitEoc,
                       Capture, CheckCount, Done };
    State state() const { return state_; } // Diagnostics, not a firmware register.
    SC_HAS_PROCESS(Adc);
    explicit Adc(sc_core::sc_module_name name,
                 sc_core::sc_time access_latency = sc_core::sc_time(25, sc_core::SC_NS));
private:
    void tick();
    void reset_registers();
    void outputs();
    std::uint32_t read(unsigned offset) const;
    void write(unsigned offset, std::uint32_t value);
    bool validate(tlm::tlm_generic_payload& tx, bool debug) const;
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload&, tlm::tlm_dmi&);
    void transfer(tlm::tlm_generic_payload& tx);
    State state_ = State::PowerDown;
    std::uint32_t ctrl_ = 0, cfg_ = 0x10, timing_ = 0x20040c00,
        count_cfg_ = 1, result_ = 0, sum_ = 0;
    std::array<std::uint32_t, 16> samples_{};
    unsigned setup_ = 32, high_ = 4, low_ = 12, count_ = 1,
        remaining_ = 0, captured_ = 0, active_cfg_ = 0x10;
    std::uint64_t reset_epoch_ = 0;
    sc_core::sc_time access_latency_;
    sc_core::sc_mutex bus_lock_;
};
} // namespace fu3::adc
