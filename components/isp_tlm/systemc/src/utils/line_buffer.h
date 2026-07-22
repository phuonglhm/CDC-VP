/*
 * Line Buffer - Shift register for storing pixel rows
 * Equivalent to RTL shift_register module
 */

#ifndef LINE_BUFFER_H
#define LINE_BUFFER_H

#include <systemc>
#include <vector>
#include "common/isp_types.h"

//=============================================================================
// Line Buffer Template
// Stores N rows of pixels for window-based processing
//=============================================================================
template<unsigned int BITS, unsigned int WIDTH, unsigned int NUM_ROWS>
class line_buffer : public sc_module {
public:
    // Input port
    sc_in<bool> pclk;
    sc_in<bool> href;       // Horizontal valid signal
    sc_in<uint16_t> pixel_in;

    // Output taps (for 5x5 window)
    sc_out<uint16_t> tap[NUM_ROWS];  // tap[0] = oldest, tap[N-1] = newest

    // Constructor
    line_buffer(const sc_module_name& name)
        : sc_module(name)
        , pclk("pclk")
        , href("href")
        , pixel_in("pixel_in")
    {
        // Initialize storage
        m_storage.resize(WIDTH);

        // Register process
        SC_METHOD(shift_process);
        sensitive << pclk.pos();
    }

    // Alternative constructor with port array names
    line_buffer(const sc_module_name& name,
                sc_in<bool>& _pclk,
                sc_in<bool>& _href,
                sc_in<uint16_t>& _pixel_in)
        : sc_module(name)
        , pclk(_pclk)
        , href(_href)
        , pixel_in(_pixel_in)
    {
        m_storage.resize(WIDTH);
        SC_METHOD(shift_process);
        sensitive << pclk.pos();
    }

private:
    std::vector<uint16_t> m_storage;
    unsigned m_col_idx;
    bool m_in_line;

    void shift_process() {
        if (rst_n.read()) {
            std::cerr << "Warning: rst_n not bound in line_buffer " << name() << std::endl;
        }

        if (pclk.read()) {
            bool valid_pixel = href.read();

            if (valid_pixel) {
                // Shift all pixels in the line
                // In actual RTL, this is done by reading from BRAM/Shift registers
                // Here we simulate with a circular buffer

                // Store current pixel
                m_storage[m_col_idx] = pixel_in.read();

                // Update taps
                for (unsigned i = 0; i < NUM_ROWS; ++i) {
                    unsigned read_idx;
                    if (m_col_idx >= i + 1) {
                        read_idx = m_col_idx - i - 1;
                    } else {
                        // Wrapped to previous line
                        read_idx = WIDTH - (i + 1 - m_col_idx);
                    }
                    tap[i].write(m_storage[read_idx]);
                }

                // Advance column
                if (m_col_idx < WIDTH - 1) {
                    m_col_idx++;
                }
            }

            // Check for line start (vsync falling edge would reset m_col_idx)
        }
    }

    // Internal signal for reset
    sc_signal<bool> rst_n{"rst_n"};
};

//=============================================================================
// Simplified Line Buffer for window extraction
// Stores a fixed number of rows with proper timing
//=============================================================================
template<unsigned int BITS, unsigned int NUM_ROWS>
class window_line_buffer : public sc_module {
public:
    sc_in<bool> pclk;
    sc_in<bool> rst_n;
    sc_in<bool> href;
    sc_in<uint16_t> din;

    sc_out<uint16_t> row_out[NUM_ROWS];  // Current pixel from each row

    window_line_buffer(const sc_module_name& name)
        : sc_module(name)
    {
        SC_CTHREAD(shift_process, pclk.pos());
        async_reset_signal_is(rst_n, true);

        for (auto& r : row_out) {
            r.initialize(0);
        }
    }

private:
    // Line storage: each row is a vector of pixels
    std::vector<uint16_t> m_lines[NUM_ROWS];
    unsigned m_col_idx;
    unsigned m_row_idx;
    bool m_in_frame;

    void shift_process() {
        // Initialize
        for (auto& line : m_lines) {
            line.resize(2048);  // Max width, should be parameterized
        }
        m_col_idx = 0;
        m_row_idx = 0;
        m_in_frame = false;

        while (true) {
            if (href.read()) {
                m_in_frame = true;

                // Shift rows down (newest to oldest)
                for (unsigned i = NUM_ROWS - 1; i > 0; --i) {
                    if (m_col_idx < m_lines[i].size()) {
                        m_lines[i][m_col_idx] = m_lines[i-1][m_col_idx];
                    }
                }

                // Store new pixel in row 0
                if (m_col_idx < m_lines[0].size()) {
                    m_lines[0][m_col_idx] = din.read();
                }

                // Output current column from each row
                for (unsigned i = 0; i < NUM_ROWS; ++i) {
                    if (m_col_idx < m_lines[i].size()) {
                        row_out[i].write(m_lines[i][m_col_idx]);
                    }
                }

                m_col_idx++;
            } else if (m_in_frame) {
                // Line ended
                m_col_idx = 0;
                m_in_frame = false;
            }

            wait();
        }
    }
};

#endif // LINE_BUFFER_H
