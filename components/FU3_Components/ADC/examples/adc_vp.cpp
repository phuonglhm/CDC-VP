#include "fu3/adc/adc.h"
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <stdexcept>

using namespace sc_core;
using fu3::adc::Adc;
// Minimal executable VP: software-style polling master + external digital
// hardmacro stimulus. No CPU ISA simulator or analog transfer model is implied.
struct Platform : sc_module {
    sc_clock adc_clk{"adc_clk", 100, SC_NS};
    sc_signal<bool> reset_n, eoc, sampclk, cs, pwd, macro_reset, samp_sel;
    sc_signal<sc_dt::sc_uint<12>> data;
    sc_signal<sc_dt::sc_uint<4>> channel;
    tlm_utils::simple_initiator_socket<Platform> cpu{"cpu"};
    Adc adc{"adc"};
    bool passed = false;
    SC_HAS_PROCESS(Platform);
    explicit Platform(sc_module_name n) : sc_module(n) {
        cpu.bind(adc.socket); adc.adc_clk(adc_clk); adc.reset_n(reset_n);
        adc.eoc(eoc); adc.data_in(data); adc.sampclk(sampclk); adc.cs(cs);
        adc.pwd(pwd); adc.macro_reset(macro_reset); adc.samp_sel(samp_sel); adc.channel(channel);
        SC_THREAD(software); SC_THREAD(macro);
    }
    unsigned access(bool write, unsigned off, unsigned value = 0) {
        unsigned char bytes[4];
        for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<unsigned char>(value >> (8*i));
        tlm::tlm_generic_payload tx;
        tx.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        tx.set_address(off); tx.set_data_ptr(bytes); tx.set_data_length(4); tx.set_streaming_width(4);
        sc_time delay = SC_ZERO_TIME; cpu->b_transport(tx, delay);
        if (!tx.is_response_ok()) throw std::runtime_error(tx.get_response_string());
        value = 0; for (unsigned i = 0; i < 4; ++i) value |= unsigned(bytes[i]) << (8*i);
        return value;
    }
    void macro() {
        unsigned sample = 0; eoc = false;
        for (;;) {
            wait(sampclk.negedge_event());
            if (pwd || !reset_n) continue;
            wait(1200, SC_NS); data = 1000 + 100 * sample++;
            wait(50, SC_NS); eoc = true;
            wait(sampclk.posedge_event() | pwd.posedge_event());
            wait(250, SC_NS); eoc = false;
        }
    }
    void software() {
        reset_n = false; wait(225, SC_NS); reset_n = true;
        wait(100, SC_NS);
        try {
            if (access(false, 0) != 0x10000) throw std::runtime_error("VERSION mismatch");
            access(true, 4, 0); wait(200, SC_NS);
            access(true, 8, 0x13); access(true, 0x14, 0x20040c00);
            access(true, 0x18, 4); access(true, 4, 1);
            bool done = false;
            for (unsigned i = 0; i < 2000; ++i) {
                if (access(false, 0xc) & 1) { done = true; break; }
                wait(100, SC_NS);
            }
            if (!done) throw std::runtime_error("ADC timeout");
            const auto average = access(false, 0x10);
            std::cout << "Channel 3, samples:";
            for (unsigned i = 0; i < 4; ++i) {
                const auto v = access(false, 0x1c + 4*i);
                std::cout << ' ' << v;
                if (v != 1000 + 100*i) throw std::runtime_error("sample mismatch");
            }
            std::cout << ", average: " << average << " at " << sc_time_stamp() << '\n';
            if (average != 1150) throw std::runtime_error("average mismatch");
            access(true, 4, 0); wait(200, SC_NS);
            if (access(false, 0xc)) throw std::runtime_error("disable failed");
            passed = true;
        } catch (const std::exception& e) { std::cerr << e.what() << '\n'; }
        sc_stop();
    }
};
int sc_main(int, char**) {
    Platform p("vp"); sc_start(1, SC_MS); return p.passed ? 0 : 1;
}
