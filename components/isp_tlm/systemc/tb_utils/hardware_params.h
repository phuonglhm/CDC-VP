/**
 * @file hardware_params.h
 * @brief Hardware parameter configuration for timed ISP simulation
 *
 * Provides a centralized hardware configuration struct that drives
 * throughput and timing calculations throughout the ISP pipeline.
 *
 * Key parameters:
 *   - clk_mhz:      Clock frequency in MHz (affects ns/cycle conversion)
 *   - bus_width_bits: Data bus width in bits (determines tokens/cycle)
 *   - pixel_bits:  Bits per token on each domain (uint16_t=16, uint8_t=8)
 *   - fifo_depth:  Depth of inter-block FIFOs
 *
 * Usage:
 *   hw_params cfg;
 *   cfg.clk_mhz = 200.0f;       // 200 MHz
 *   cfg.bus_width_bits = 64;    // 64-bit bus
 *   cfg.pixel_bits = 16;        // 16-bit tokens
 *   cfg.fifo_depth = 1024;       // 1024-entry FIFOs
 *
 *   float tokens_per_cycle = cfg.tokens_per_cycle();     // 64/16 = 4
 *   float cycle_ns = cfg.cycle_ns();                     // 5.0 ns @ 200 MHz
 *   float mpix_s = cfg.throughput_mpixels_s();          // 50.0 Mpixels/s @ 200MHz, 4pix/cycle
 */

#ifndef ISP_HARDWARE_PARAMS_H
#define ISP_HARDWARE_PARAMS_H

#include <cstddef>
#include <cstdint>
#include <string>

struct hw_params {
    // --- Clock ---
    float clk_mhz = 200.0f;           // MHz

    // --- Bus ---
    int bus_width_bits = 64;          // bits per cycle
    int pixel_bits = 16;              // bits per token (uint16_t=16, uint8_t=8)

    // --- FIFO ---
    std::size_t fifo_depth = 1024;   // entries

    // --- Simulation mode ---
    bool timed_mode = true;          // true = add wait(cycles, CLK) in blocks
    int default_cycles_per_pixel = 1; // used when timed_mode=true

    // --- Derived getters ---
    float cycle_ns() const {
        return (clk_mhz > 0.0f) ? (1000.0f / clk_mhz) : 0.0f;
    }

    float tokens_per_cycle() const {
        return (pixel_bits > 0) ? (static_cast<float>(bus_width_bits) / static_cast<float>(pixel_bits)) : 0.0f;
    }

    float throughput_mpixels_s() const {
        if (tokens_per_cycle() <= 0.0f) return 0.0f;
        return (clk_mhz * 1e6f) / tokens_per_cycle();
    }

    float throughput_tokens_s() const {
        if (cycle_ns() <= 0.0f) return 0.0f;
        return 1e9f / cycle_ns() * tokens_per_cycle();
    }

    std::string to_string() const {
        std::string result;
        result += "clk_mhz          : " + std::to_string(clk_mhz) + " MHz\n";
        result += "bus_width_bits   : " + std::to_string(bus_width_bits) + " bits\n";
        result += "pixel_bits       : " + std::to_string(pixel_bits) + " bits\n";
        result += "tokens/cycle     : " + std::to_string(tokens_per_cycle()) + "\n";
        result += "cycle_ns         : " + std::to_string(cycle_ns()) + " ns\n";
        result += "throughput       : " + std::to_string(throughput_mpixels_s() / 1e6f) + " Mpixels/s\n";
        result += "fifo_depth       : " + std::to_string(fifo_depth) + "\n";
        result += "timed_mode       : " + std::string(timed_mode ? "true" : "false") + "\n";
        return result;
    }
};

#endif  // ISP_HARDWARE_PARAMS_H
