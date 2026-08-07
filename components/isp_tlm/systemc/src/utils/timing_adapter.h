/*
 * Timing Adapter - Synchronizes timing signals with data
 * Equivalent to RTL vid_mux module
 */

#ifndef TIMING_ADAPTER_H
#define TIMING_ADAPTER_H

#include <systemc>
#include <tlm>
#include <queue>

//=============================================================================
// Timing Adapter
// Forwards timing signals with optional bypass mux
// Matches RTL vid_mux behavior
//=============================================================================
template<unsigned int BITS>
class timing_adapter : public sc_module {
public:
    // Clock and reset
    sc_in<bool> pclk;
    sc_in<bool> rst_n;

    // Select signal (0: pass through, 1: use processed)
    sc_in<bool> sel;

    // Input path 0 (bypass/passthrough)
    sc_in<bool> in_href_0;
    sc_in<bool> in_vsync_0;
    sc_in<uint16_t> in_data_0;

    // Input path 1 (processed)
    sc_in<bool> in_href_1;
    sc_in<bool> in_vsync_1;
    sc_in<uint16_t> in_data_1;

    // Output
    sc_out<bool> out_href;
    sc_out<bool> out_vsync;
    sc_out<uint16_t> out_data;

    // Constructor
    timing_adapter(const sc_module_name& name)
        : sc_module(name)
        , pclk("pclk")
        , rst_n("rst_n")
        , sel("sel")
        , in_href_0("in_href_0")
        , in_vsync_0("in_vsync_0")
        , in_data_0("in_data_0")
        , in_href_1("in_href_1")
        , in_vsync_1("in_vsync_1")
        , in_data_1("in_data_1")
        , out_href("out_href")
        , out_vsync("out_vsync")
        , out_data("out_data")
    {
        SC_METHOD(output_process);
        sensitive << pclk.pos();
    }

private:
    void output_process() {
        if (!rst_n.read()) {
            out_href.write(false);
            out_vsync.write(false);
            out_data.write(0);
        } else {
            // RTL: wire in_href = sel ? in_href_1 : in_href_0;
            //      Always register output
            bool href_sel = sel.read() ? in_href_1.read() : in_href_0.read();
            bool vsync_sel = sel.read() ? in_vsync_1.read() : in_vsync_0.read();
            uint16_t data_sel = sel.read() ? in_data_1.read() : in_data_0.read();

            out_href.write(href_sel);
            out_vsync.write(vsync_sel);
            out_data.write(data_sel);
        }
    }
};

//=============================================================================
// Simple Delay Line for timing signals
// Used when bypass is not needed
//=============================================================================
class timing_delay : public sc_module {
public:
    static constexpr unsigned MAX_DELAY = 16;

    sc_in<bool> pclk;
    sc_in<bool> rst_n;

    sc_in<bool> href_in;
    sc_in<bool> vsync_in;

    sc_out<bool> href_out;
    sc_out<bool> vsync_out;

    // Constructor
    timing_delay(const sc_module_name& name, unsigned delay_cycles = 1)
        : sc_module(name)
        , m_delay(delay_cycles)
    {
        SC_METHOD(delay_process);
        sensitive << pclk.pos();

        // Initialize signals
        href_out.initialize(false);
        vsync_out.initialize(false);
    }

    void set_delay(unsigned cycles) { m_delay = cycles; }

private:
    unsigned m_delay;
    bool m_href_delay[MAX_DELAY];
    bool m_vsync_delay[MAX_DELAY];

    void delay_process() {
        if (!rst_n.read()) {
            for (unsigned i = 0; i < MAX_DELAY; ++i) {
                m_href_delay[i] = false;
                m_vsync_delay[i] = false;
            }
            href_out.write(false);
            vsync_out.write(false);
        } else {
            // Shift delay line
            for (unsigned i = MAX_DELAY - 1; i > 0; --i) {
                m_href_delay[i] = m_href_delay[i-1];
                m_vsync_delay[i] = m_vsync_delay[i-1];
            }
            m_href_delay[0] = href_in.read();
            m_vsync_delay[0] = vsync_in.read();

            // Output delayed signal
            if (m_delay < MAX_DELAY) {
                href_out.write(m_href_delay[m_delay]);
                vsync_out.write(m_vsync_delay[m_delay]);
            }
        }
    }
};

//=============================================================================
// Packet-based Timing Adapter for TLM
// Wraps pixel data with timing information
//=============================================================================
struct pixel_packet {
    uint16_t data;
    bool href;
    bool vsync;
    unsigned x;
    unsigned y;
    bool frame_start;
    bool frame_end;

    pixel_packet() : data(0), href(false), vsync(false), x(0), y(0),
                      frame_start(false), frame_end(false) {}
};

class pixel_timing_adapter : public sc_channel<sc_module> {
public:
    tlm_utils::simple_initiator_socket<pixel_timing_adapter>* isock;
    tlm_utils::simple_target_socket<pixel_timing_adapter>* tsock;

    sc_port<sc_signal_if<bool>>& pclk;
    sc_port<sc_signal_if<bool>>& rst_n;

    pixel_timing_adapter(const sc_module_name& name,
                         sc_port<sc_signal_if<bool>>& _pclk,
                         sc_port<sc_signal_if<bool>>& _rst_n)
        : sc_channel<sc_module>(name)
        , pclk(_pclk)
        , rst_n(_rst_n)
        , m_href(false)
        , m_vsync(false)
        , m_x(0)
        , m_y(0)
    {}

    void bind_initiator(tlm_utils::simple_initiator_socket<pixel_timing_adapter>& sock) {
        isock = &sock;
    }

    void bind_target(tlm_utils::simple_target_socket<pixel_timing_adapter>& sock) {
        tsock = &sock;
    }

    // Helper methods
    bool get_href() const { return m_href; }
    bool get_vsync() const { return m_vsync; }
    void reset_position() { m_x = 0; m_y = 0; }

private:
    bool m_href;
    bool m_vsync;
    unsigned m_x;
    unsigned m_y;
};

#endif // TIMING_ADAPTER_H
