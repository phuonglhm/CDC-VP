/*
 * Window Buffer - Creates sliding window for neighborhood processing
 * Used for DPC, BNR, Sharpen and other spatial filters
 */

#ifndef WINDOW_BUFFER_H
#define WINDOW_BUFFER_H

#include <systemc>
#include <array>
#include "common/isp_types.h"

//=============================================================================
// 5x5 Window Buffer
// Extracts 5x5 neighborhood from input stream
// Matches RTL DPC implementation
//=============================================================================
template<unsigned int BITS>
class window_5x5 : public sc_module {
public:
    // Clock and reset
    sc_in<bool> pclk;
    sc_in<bool> rst_n;

    // Input
    sc_in<bool> href_in;
    sc_in<bool> vsync_in;
    sc_in<uint16_t> din;

    // Output - 5x5 window pixels
    // p11 p12 p13 p14 p15  (row 0 = oldest)
    // p21 p22 p23 p24 p25  (row 4 = newest)
    // p31 p32 p33 p34 p35
    // p41 p42 p43 p44 p45
    // p51 p52 p53 p54 p55

    sc_out<uint16_t> p11, p12, p13, p14, p15;
    sc_out<uint16_t> p21, p22, p23, p24, p25;
    sc_out<uint16_t> p31, p32, p33, p34, p35;
    sc_out<uint16_t> p41, p42, p43, p44, p45;
    sc_out<uint16_t> p51, p52, p53, p54, p55;

    // Window valid signal (true when full 5x5 window is available)
    sc_out<bool> window_valid;

    // Constructor
    window_5x5(const sc_module_name& name)
        : sc_module(name)
        , pclk("pclk")
        , rst_n("rst_n")
        , href_in("href_in")
        , vsync_in("vsync_in")
        , din("din")
        , window_valid("window_valid")
    {
        SC_CTHREAD(shift_process, pclk.pos());
        async_reset_signal_is(rst_n, true);

        // Initialize outputs
        window_valid.initialize(false);
    }

private:
    // Line storage (4 rows + 1 current)
    // Each row: {p1, p2, p3, p4, p5}
    std::array<std::array<uint16_t, 5>, 5> m_window;

    // Shift register state
    unsigned m_col_in_row;  // Column position within current row
    bool m_in_line;
    bool m_line_started;
    unsigned m_lines_received;  // Number of complete lines received

    void shift_process() {
        // Reset state
        for (auto& row : m_window) {
            row.fill(0);
        }
        m_col_in_row = 0;
        m_in_line = false;
        m_line_started = false;
        m_lines_received = 0;

        while (true) {
            bool href = href_in.read();
            bool vsync = vsync_in.read();
            uint16_t pixel = din.read();

            // Detect frame start (vsync falling edge)
            bool vsync_falling = m_in_line && !vsync && href_in.read() == false;
            if (vsync_falling) {
                // Frame ended
                m_lines_received = 0;
                m_col_in_row = 0;
                m_line_started = false;
            }

            if (href) {
                m_in_line = true;

                if (!m_line_started) {
                    m_line_started = true;
                    m_col_in_row = 0;
                }

                // Shift window - move all rows up
                for (unsigned row = 4; row > 0; --row) {
                    for (unsigned col = 0; col < 5; ++col) {
                        m_window[row][col] = m_window[row-1][col];
                    }
                }

                // Add new pixel to row 0
                // Shift within row
                m_window[0][4] = m_window[0][3];
                m_window[0][3] = m_window[0][2];
                m_window[0][2] = m_window[0][1];
                m_window[0][1] = m_window[0][0];
                m_window[0][0] = pixel;

                m_col_in_row++;

                // Output window when we have enough data
                // Need at least 4 complete rows before center pixel is valid
                if (m_col_in_row >= 2) {  // At least 2 pixels in current row
                    bool valid = (m_col_in_row >= 2);
                    window_valid.write(valid);
                }

                // Write outputs (with 2-cycle delay to match RTL)
                wait();  // Delay 1 cycle

                // Update outputs
                p11.write(m_window[4][0]);
                p12.write(m_window[4][1]);
                p13.write(m_window[4][2]);
                p14.write(m_window[4][3]);
                p15.write(m_window[4][4]);

                p21.write(m_window[3][0]);
                p22.write(m_window[3][1]);
                p23.write(m_window[3][2]);
                p24.write(m_window[3][3]);
                p25.write(m_window[3][4]);

                p31.write(m_window[2][0]);
                p32.write(m_window[2][1]);
                p33.write(m_window[2][2]);
                p34.write(m_window[2][3]);
                p35.write(m_window[2][4]);

                p41.write(m_window[1][0]);
                p42.write(m_window[1][1]);
                p43.write(m_window[1][2]);
                p44.write(m_window[1][3]);
                p45.write(m_window[1][4]);

                p51.write(m_window[0][0]);
                p52.write(m_window[0][1]);
                p53.write(m_window[0][2]);
                p54.write(m_window[0][3]);
                p55.write(m_window[0][4]);
            }

            wait();
        }
    }
};

//=============================================================================
// 3x3 Window Buffer
// Simpler version for blocks that only need 3x3 neighborhood
//=============================================================================
template<unsigned int BITS>
class window_3x3 : public sc_module {
public:
    sc_in<bool> pclk;
    sc_in<bool> rst_n;
    sc_in<bool> href_in;
    sc_in<uint16_t> din;

    // 3x3 window output
    sc_out<uint16_t> p11, p12, p13;
    sc_out<uint16_t> p21, p22, p23;
    sc_out<uint16_t> p31, p32, p33;

    sc_out<bool> window_valid;

    window_3x3(const sc_module_name& name)
        : sc_module(name)
    {
        SC_CTHREAD(shift_process, pclk.pos());
        async_reset_signal_is(rst_n, true);
    }

private:
    std::array<std::array<uint16_t, 3>, 3> m_window;
    unsigned m_col_in_row;
    bool m_in_line;

    void shift_process() {
        m_window.fill({0, 0, 0});
        m_col_in_row = 0;
        m_in_line = false;

        while (true) {
            if (href_in.read()) {
                // Shift window
                for (unsigned row = 2; row > 0; --row) {
                    m_window[row][0] = m_window[row-1][1];
                    m_window[row][1] = m_window[row-1][2];
                    m_window[row][2] = m_window[row-1][0];
                }
                m_window[0][2] = m_window[0][1];
                m_window[0][1] = m_window[0][0];
                m_window[0][0] = din.read();

                wait();

                // Output
                p11 = m_window[2][0];
                p12 = m_window[2][1];
                p13 = m_window[2][2];
                p21 = m_window[1][0];
                p22 = m_window[1][1];
                p23 = m_window[1][2];
                p31 = m_window[0][0];
                p32 = m_window[0][1];
                p33 = m_window[0][2];

                window_valid = (m_col_in_row >= 1);
                m_col_in_row++;
            }

            wait();
        }
    }
};

//=============================================================================
// 9x9 Window Buffer (for Sharpening)
//=============================================================================
template<unsigned int BITS>
class window_9x9 : public sc_module {
public:
    sc_in<bool> pclk;
    sc_in<bool> rst_n;
    sc_in<bool> href_in;
    sc_in<uint16_t> din;

    // Center pixel position in window
    sc_out<uint16_t> center_pixel;

    // Full 9x9 window as 2D array output
    sc_out<uint16_t> window_out[9][9];

    window_9x9(const sc_module_name& name)
        : sc_module(name)
    {
        SC_CTHREAD(shift_process, pclk.pos());
        async_reset_signal_is(rst_n, true);
    }

private:
    // Store last 8 rows (current row + 8 stored = 9 total)
    std::array<std::vector<uint16_t>, 8> m_rows;
    unsigned m_max_width;

    void shift_process() {
        m_max_width = 2048;  // Default, should be parameterized

        for (auto& row : m_rows) {
            row.resize(m_max_width);
            row.fill(0);
        }

        while (true) {
            if (href_in.read()) {
                // Shift rows and insert new pixel
                for (unsigned r = 7; r > 0; --r) {
                    m_rows[r] = m_rows[r-1];
                }
                m_rows[0].push_back(din.read());
                if (m_rows[0].size() > m_max_width) {
                    m_rows[0].erase(m_rows[0].begin());
                }

                wait();

                // Output 9x9 window
                unsigned cur_size = m_rows[0].size();
                for (unsigned row = 0; row < 9; ++row) {
                    for (unsigned col = 0; col < 9; ++col) {
                        unsigned read_idx = (cur_size > col + 1) ? (cur_size - col - 1) : 0;
                        unsigned row_idx = (row < 8) ? row : 7;
                        window_out[row][col] = m_rows[row_idx][read_idx];
                    }
                }

                // Center is at (4,4) in 0-indexed
                center_pixel = window_out[4][4];
            }

            wait();
        }
    }
};

#endif // WINDOW_BUFFER_H
