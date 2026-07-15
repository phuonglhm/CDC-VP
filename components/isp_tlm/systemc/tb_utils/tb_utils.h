/**

 * @file tb_utils.h

 * @brief Generic Testbench Utilities for SystemC ISP Streaming Architecture

 *

 * Provides templated Driver and Monitor classes for pixel-by-pixel streaming

 * testbench generation. These utilities enable untimed verification of each

 * ISP processing block by driving stimulus and capturing/monitoring responses.

 */

#ifndef ISP_TB_UTILS_H

#define ISP_TB_UTILS_H



#include <systemc>
using namespace sc_core;


#include <vector>
#include <iostream>
#include <iomanip>
#include <cmath>

/**
 * @brief Generic Driver template for pushing pixel data into SystemC FIFOs
 *
 * The Generic_Driver reads a flat std::vector of data and streams it
 * pixel-by-pixel into an sc_fifo output port. It operates as an SC_THREAD
 * that runs an infinite loop, pushing one token per iteration.
 *
 * @tparam T Data type of each pixel/token
 */
template <typename T>
class Generic_Driver : public sc_core::sc_module {
public:
    sc_core::sc_port<sc_fifo_out_if<T>> fifo_out;

    SC_HAS_PROCESS(Generic_Driver);

    /**
     * @brief Constructor
     * @param name Module name
     * @param input_data Vector containing input data to stream
     */
    Generic_Driver(sc_core::sc_module_name name, const std::vector<T>& input_data)
        : sc_module(name), m_input_data(input_data) {
        SC_THREAD(drive_tokens);
    }

    /**
     * @brief Main driving thread
     *
     * Iterates through input_data and writes each element to fifo_out.
     * Terminates when all data is sent. Uses blocking write() for
     * synchronization.
     */
    void drive_tokens() {
        std::size_t token_count = 0;
        for (const T& token : m_input_data) {
            fifo_out->write(token);
            token_count++;

            if (token_count % 10000 == 0) {
                std::cout << "[Driver " << name() << "] Sent " << token_count
                          << " tokens, total=" << m_input_data.size() << std::endl;
            }
        }
        std::cout << "[Driver " << name() << "] Completed sending " << token_count
                  << " tokens" << std::endl;
    }

    /**
     * @brief Get total number of tokens sent
     */
    std::size_t get_token_count() const { return m_input_data.size(); }

private:
    const std::vector<T>& m_input_data;
};

/**
 * @brief Generic Monitor template for capturing pixel data from SystemC FIFOs
 *
 * The Generic_Monitor reads tokens from an sc_fifo input port and stores
 * them in an internal vector. Once a complete frame (pixel_count tokens)
 * is received, it triggers comparison with golden data if provided.
 *
 * @tparam T Data type of each pixel/token
 */
template <typename T>
class Generic_Monitor : public sc_core::sc_module {
public:
    sc_core::sc_port<sc_fifo_in_if<T>> fifo_in;

    SC_HAS_PROCESS(Generic_Monitor);

    /**
     * @brief Constructor
     * @param name Module name
     * @param pixel_count Expected number of tokens per frame
     */
    Generic_Monitor(sc_core::sc_module_name name, std::size_t pixel_count)
        : sc_module(name), m_expected_count(pixel_count), m_golden_data(nullptr),
          m_golden_size(0), m_use_golden(false) {
        SC_THREAD(monitor_tokens);
    }

    /**
     * @brief Set golden reference data for comparison
     * @param golden Pointer to golden data vector
     * @param size Size of golden data
     */
    void set_golden_reference(const T* golden, std::size_t size) {
        m_golden_data = golden;
        m_golden_size = size;
        m_use_golden = true;
    }

    /**
     * @brief Main monitoring thread
     *
     * Reads tokens from fifo_in until expected count is reached.
     * Performs golden comparison if reference data is available.
     */
    void monitor_tokens() {
        std::vector<T> captured_data;
        captured_data.reserve(m_expected_count);
        std::size_t token_count = 0;

        while (token_count < m_expected_count) {
            T token = fifo_in->read();
            captured_data.push_back(token);
            token_count++;

            if (token_count % 10000 == 0) {
                std::cout << "[Monitor " << name() << "] Received " << token_count
                          << " tokens, expected=" << m_expected_count << std::endl;
            }
        }

        std::cout << "[Monitor " << name() << "] Completed receiving " << token_count
                  << " tokens" << std::endl;

        m_captured_data = std::move(captured_data);

        if (m_use_golden && m_golden_data != nullptr) {
            compare_with_golden();
        }
    }

    /**
     * @brief Compare captured data with golden reference
     *
     * Computes MSE (Mean Squared Error) and reports match status.
     * Uses floating-point comparison for pixel values.
     *
     * @return true if MSE < threshold, false otherwise
     */
    bool compare_with_golden() {
        if (m_captured_data.size() != m_golden_size) {
            std::cerr << "[Monitor " << name() << "] ERROR: Size mismatch! "
                      << "Captured=" << m_captured_data.size()
                      << " Golden=" << m_golden_size << std::endl;
            return false;
        }

        double mse = 0.0;
        std::size_t max_err_idx = 0;
        double max_err = 0.0;

        for (std::size_t i = 0; i < m_captured_data.size(); ++i) {
            double diff = static_cast<double>(m_captured_data[i]) -
                          static_cast<double>(m_golden_data[i]);
            mse += diff * diff;

            if (std::abs(diff) > max_err) {
                max_err = std::abs(diff);
                max_err_idx = i;
            }
        }
        mse /= static_cast<double>(m_captured_data.size());

        std::cout << "[Monitor " << name() << "] Comparison Results:" << std::endl;
        std::cout << "  MSE: " << std::scientific << std::setprecision(6) << mse << std::endl;
        std::cout << "  Max Error: " << max_err << " at index " << max_err_idx << std::endl;

        bool pass = (mse < 1.0);  // Threshold for pass/fail
        std::cout << "  Status: " << (pass ? "PASS" : "FAIL") << std::endl;

        return pass;
    }

    /**
     * @brief Get captured data vector
     */
    const std::vector<T>& get_captured_data() const { return m_captured_data; }

    /**
     * @brief Get expected pixel count
     */
    std::size_t get_expected_count() const { return m_expected_count; }

private:
    std::size_t m_expected_count;
    const T* m_golden_data;
    std::size_t m_golden_size;
    bool m_use_golden;
    std::vector<T> m_captured_data;
};

/**
 * @brief Specialized Monitor for uint8_t with YUV420 output
 *
 * This monitor handles YUV420 format which has different output size
 * than input (Y: W*H, U/V: W/2*H/2 each).
 */
template <>
class Generic_Monitor<uint8_t> : public sc_core::sc_module {
public:
    sc_core::sc_port<sc_fifo_in_if<uint8_t>> fifo_in;

    SC_HAS_PROCESS(Generic_Monitor);

    Generic_Monitor(sc_core::sc_module_name name, std::size_t expected_yuv420_size)
        : sc_module(name), m_expected_count(expected_yuv420_size),
          m_golden_data(nullptr), m_golden_size(0), m_use_golden(false) {
        SC_THREAD(monitor_tokens);
    }

    void set_golden_reference(const uint8_t* golden, std::size_t size) {
        m_golden_data = golden;
        m_golden_size = size;
        m_use_golden = true;
    }

    void monitor_tokens() {
        std::vector<uint8_t> captured_data;
        captured_data.reserve(m_expected_count);
        std::size_t token_count = 0;

        while (token_count < m_expected_count) {
            uint8_t token = fifo_in->read();
            captured_data.push_back(token);
            token_count++;

            if (token_count % 10000 == 0) {
                std::cout << "[Monitor " << name() << "] Received " << token_count
                          << " tokens, expected=" << m_expected_count << std::endl;
            }
        }

        std::cout << "[Monitor " << name() << "] Completed receiving " << token_count
                  << " tokens" << std::endl;

        m_captured_data = std::move(captured_data);

        if (m_use_golden && m_golden_data != nullptr) {
            compare_with_golden();
        }
    }

    bool compare_with_golden() {
        if (m_captured_data.size() != m_golden_size) {
            std::cerr << "[Monitor " << name() << "] ERROR: Size mismatch!" << std::endl;
            return false;
        }

        double mse = 0.0;
        std::size_t max_err_idx = 0;
        double max_err = 0.0;

        for (std::size_t i = 0; i < m_captured_data.size(); ++i) {
            double diff = static_cast<double>(m_captured_data[i]) -
                          static_cast<double>(m_golden_data[i]);
            mse += diff * diff;

            if (std::abs(diff) > max_err) {
                max_err = std::abs(diff);
                max_err_idx = i;
            }
        }
        mse /= static_cast<double>(m_captured_data.size());

        std::cout << "[Monitor " << name() << "] YUV420 Comparison Results:" << std::endl;
        std::cout << "  MSE: " << std::scientific << std::setprecision(6) << mse << std::endl;
        std::cout << "  Max Error: " << max_err << " at index " << max_err_idx << std::endl;

        bool pass = (mse < 1.0);
        std::cout << "  Status: " << (pass ? "PASS" : "FAIL") << std::endl;

        return pass;
    }

    const std::vector<uint8_t>& get_captured_data() const { return m_captured_data; }
    std::size_t get_expected_count() const { return m_expected_count; }

private:
    std::size_t m_expected_count;
    const uint8_t* m_golden_data;
    std::size_t m_golden_size;
    bool m_use_golden;
    std::vector<uint8_t> m_captured_data;
};

/**
 * @brief Utility functions for test data generation
 */
namespace tb_utils {

/**
 * @brief Generate a test pattern with controlled pixel values
 *
 * @tparam T Pixel type
 * @param width Frame width
 * @param height Frame height
 * @param pattern_type 0=constant, 1=ramp, 2=checkerboard
 * @param value Base value for constant pattern
 */
template <typename T>
std::vector<T> generate_test_pattern(std::size_t width, std::size_t height,
                                     int pattern_type = 0, T value = T{}) {
    std::vector<T> data(width * height);
    for (std::size_t i = 0; i < height; ++i) {
        for (std::size_t j = 0; j < width; ++j) {
            std::size_t idx = i * width + j;
            switch (pattern_type) {
                case 0:  // Constant
                    data[idx] = value;
                    break;
                case 1:  // Ramp
                    data[idx] = static_cast<T>((idx * 17) % 256);
                    break;
                case 2:  // Checkerboard
                    data[idx] = ((i / 8 + j / 8) % 2 == 0) ?
                                 static_cast<T>(255) : static_cast<T>(0);
                    break;
                default:
                    data[idx] = value;
            }
        }
    }
    return data;
}

/**
 * @brief Generate RGB interleaved test pattern
 */
inline std::vector<uint16_t> generate_rgb_test_pattern(std::size_t width,
                                                       std::size_t height,
                                                       int pattern_type = 0) {
    std::vector<uint16_t> data(width * height * 3);
    for (std::size_t i = 0; i < height; ++i) {
        for (std::size_t j = 0; j < width; ++j) {
            std::size_t base = (i * width + j) * 3;
            data[base + 0] = static_cast<uint16_t>((i * 7 + j * 11) % 4096);  // R
            data[base + 1] = static_cast<uint16_t>((i * 13 + j * 17) % 4096); // G
            data[base + 2] = static_cast<uint16_t>((i * 19 + j * 23) % 4096); // B
        }
    }
    return data;
}

/**
 * @brief Compute statistics of a data vector
 */
template <typename T>
void print_statistics(const std::string& name, const std::vector<T>& data) {
    if (data.empty()) return;

    double sum = 0.0, min_val = std::numeric_limits<double>::max(),
           max_val = std::numeric_limits<double>::min();

    for (const T& val : data) {
        double v = static_cast<double>(val);
        sum += v;
        min_val = std::min(min_val, v);
        max_val = std::max(max_val, v);
    }

    double mean = sum / static_cast<double>(data.size());

    std::cout << "[" << name << "] Statistics:" << std::endl;
    std::cout << "  Count: " << data.size() << std::endl;
    std::cout << "  Min: " << min_val << std::endl;
    std::cout << "  Max: " << max_val << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(2) << mean << std::endl;
}

}  // namespace tb_utils

#endif  // ISP_TB_UTILS_H
