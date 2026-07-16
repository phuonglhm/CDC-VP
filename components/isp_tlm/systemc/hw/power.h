/**
 * @file power.h
 * @brief Power estimation for ISP pipeline blocks
 *
 * Provides power models for different block types:
 *   - Static power (leakage)
 *   - Dynamic power (switching)
 *   - Memory power (SRAM)
 *   - Total power estimation
 *
 * Power values are estimates based on typical ISP hardware.
 * Actual values should be calibrated against silicon measurements.
 */

#ifndef ISP_POWER_H
#define ISP_POWER_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <string>
#include <vector>
#include <cmath>

// ============================================================================
// Power Units
// ============================================================================
struct power_values {
    double static_mw = 0.0;      // Static/leakage power (mW)
    double dynamic_mw = 0.0;      // Dynamic power (mW)
    double memory_mw = 0.0;       // Memory power (mW)
    double total_mw = 0.0;        // Total power (mW)

    power_values operator+(const power_values& other) const {
        return power_values{
            static_mw + other.static_mw,
            dynamic_mw + other.dynamic_mw,
            memory_mw + other.memory_mw,
            total_mw + other.total_mw
        };
    }

    power_values& operator+=(const power_values& other) {
        static_mw += other.static_mw;
        dynamic_mw += other.dynamic_mw;
        memory_mw += other.memory_mw;
        total_mw += other.total_mw;
        return *this;
    }
};

// ============================================================================
// Block Power Configuration
// ============================================================================
struct block_power_config {
    std::string name = "block";

    // Static power (leakage) - typically small
    double leakage_mw = 1.0;           // mW per mm^2 at 28nm (estimate)

    // Dynamic power coefficients
    double switched_cap_ff = 10.0;      // fF per gate (switching capacitance)
    double activity_factor = 0.25;       // Average switching activity (0-1)
    double clock_freq_mhz = 200.0;      // Operating frequency

    // Memory power (if block has local memory)
    bool has_memory = false;
    std::uint32_t memory_kbits = 0;     // Memory size in Kbits
    double memory_access_energy_pj = 5.0; // pJ per access

    // Compute intensity
    std::uint32_t ops_per_pixel = 0;    // Arithmetic operations per pixel
    double mac_energy_pj = 2.0;         // pJ per MAC operation

    // I/O power
    double io_bandwidth_gbps = 0.0;     // Input+output bandwidth in Gbps
    double io_energy_pj_per_bit = 0.5;   // pJ per bit transferred
};

// ============================================================================
// Power Estimator
// ============================================================================
class power_estimator {
public:
    explicit power_estimator(const std::string& output_dir = "output/power")
        : m_output_dir(output_dir) {}

    // Calculate power for a block based on configuration and activity
    power_values calculate_power(const block_power_config& cfg,
                                std::uint64_t cycles_active,
                                std::uint64_t cycles_total,
                                std::uint64_t memory_accesses,
                                std::uint64_t pixels_processed) {
        power_values p;

        // Static power (always on)
        p.static_mw = cfg.leakage_mw;

        // Dynamic power based on switching
        // P_dynamic = 0.5 * C * V^2 * f * activity
        // Simplified: P = C * f * activity (assuming V normalized)
        double capacitance_ff = cfg.switched_cap_ff;
        double freq_mhz = cfg.clock_freq_mhz;
        double activity = cfg.activity_factor;

        // P_dynamic (mW) = 0.5 * C(fF) * V^2 * f(MHz) * activity
        // Assuming V = 1.0V normalized
        double vdd = 1.0;  // Normalized voltage
        p.dynamic_mw = 0.5 * capacitance_ff * vdd * vdd * freq_mhz * activity * 1e-6;

        // Memory power
        if (cfg.has_memory) {
            // Memory power scales with size and access rate
            double mem_power_static = cfg.memory_kbits * 0.1;  // mW per Kbit (estimate)
            double mem_power_dynamic = memory_accesses * cfg.memory_access_energy_pj * freq_mhz * 1e-6;
            p.memory_mw = mem_power_static + mem_power_dynamic;
        }

        // Compute power for arithmetic operations
        if (cfg.ops_per_pixel > 0 && pixels_processed > 0) {
            std::uint64_t total_ops = cfg.ops_per_pixel * pixels_processed;
            double compute_power = total_ops * cfg.mac_energy_pj * freq_mhz * 1e-6;
            p.dynamic_mw += compute_power;
        }

        // I/O power
        if (cfg.io_bandwidth_gbps > 0) {
            double io_power = cfg.io_bandwidth_gbps * 1000 * cfg.io_energy_pj_per_bit * freq_mhz * 1e-6;
            p.dynamic_mw += io_power;
        }

        // Total
        p.total_mw = p.static_mw + p.dynamic_mw + p.memory_mw;

        return p;
    }

    // Get utilization-scaled power
    power_values scaled_power(const power_values& base,
                             double utilization) {
        power_values p;
        p.static_mw = base.static_mw;  // Leakage doesn't scale with utilization
        p.dynamic_mw = base.dynamic_mw * utilization;
        p.memory_mw = base.memory_mw * utilization;
        p.total_mw = p.static_mw + p.dynamic_mw + p.memory_mw;
        return p;
    }

    // Energy per frame
    double energy_per_frame(const power_values& power, double frame_time_us) {
        // Power (mW) * time (us) = energy (nJ)
        return power.total_mw * frame_time_us / 1000.0;  // nJ
    }

    // Frame power (average over time)
    double frame_power(const power_values& power, double frame_time_us,
                      double frame_period_us) {
        if (frame_period_us <= 0) return 0;
        // Energy / period = average power
        double energy = energy_per_frame(power, frame_time_us);
        return energy / frame_period_us;  // mW
    }

    // Output directory
    void set_output_dir(const std::string& dir) { m_output_dir = dir; }

    std::string output_dir() const { return m_output_dir; }

private:
    std::string m_output_dir;
};

// ============================================================================
// Pre-defined Block Power Configurations
// ============================================================================
namespace block_power_defaults {

// Point operations: BLC, DG, WB, CCM, GC, CSC, CSE
inline block_power_config point_op() {
    block_power_config cfg;
    cfg.name = "point_op";
    cfg.leakage_mw = 0.5;
    cfg.switched_cap_ff = 5.0;
    cfg.activity_factor = 0.15;
    cfg.ops_per_pixel = 3;  // Multiplication + addition per channel
    cfg.mac_energy_pj = 1.5;
    cfg.has_memory = false;
    return cfg;
}

// BLC (Black Level Correction) - simple subtraction
inline block_power_config blc() {
    block_power_config cfg;
    cfg.name = "blc";
    cfg.leakage_mw = 0.3;
    cfg.switched_cap_ff = 3.0;
    cfg.activity_factor = 0.1;
    cfg.ops_per_pixel = 1;  // One subtraction per channel
    cfg.mac_energy_pj = 0.5;
    cfg.has_memory = false;
    return cfg;
}

// DG (Digital Gain)
inline block_power_config dg() {
    block_power_config cfg;
    cfg.name = "dg";
    cfg.leakage_mw = 0.3;
    cfg.switched_cap_ff = 4.0;
    cfg.activity_factor = 0.15;
    cfg.ops_per_pixel = 1;  // Multiplication
    cfg.mac_energy_pj = 1.0;
    cfg.has_memory = false;
    return cfg;
}

// WB (White Balance) - same as DG with 3 channels
inline block_power_config wb() {
    block_power_config cfg;
    cfg.name = "wb";
    cfg.leakage_mw = 0.4;
    cfg.switched_cap_ff = 5.0;
    cfg.activity_factor = 0.15;
    cfg.ops_per_pixel = 3;  // 3 multiplications (R, G, B gains)
    cfg.mac_energy_pj = 1.0;
    cfg.has_memory = false;
    return cfg;
}

// CCM (Color Correction Matrix) - 3x3 matrix multiplication
inline block_power_config ccm() {
    block_power_config cfg;
    cfg.name = "ccm";
    cfg.leakage_mw = 0.5;
    cfg.switched_cap_ff = 8.0;
    cfg.activity_factor = 0.2;
    cfg.ops_per_pixel = 9;  // 3x3 = 9 multiplications + 6 additions
    cfg.mac_energy_pj = 1.5;
    cfg.has_memory = false;
    return cfg;
}

// CSC (Color Space Conversion) - RGB to YUV
inline block_power_config csc() {
    block_power_config cfg;
    cfg.name = "csc";
    cfg.leakage_mw = 0.4;
    cfg.switched_cap_ff = 6.0;
    cfg.activity_factor = 0.18;
    cfg.ops_per_pixel = 6;  // 3 multiplications + 3 additions
    cfg.mac_energy_pj = 1.0;
    cfg.has_memory = false;
    return cfg;
}

// Statistical blocks: AWB, AEC - complex, need memory
inline block_power_config statistical() {
    block_power_config cfg;
    cfg.name = "statistical";
    cfg.leakage_mw = 1.0;
    cfg.switched_cap_ff = 10.0;
    cfg.activity_factor = 0.05;  // Low activity - just accumulating
    cfg.has_memory = true;
    cfg.memory_kbits = 64;
    cfg.memory_access_energy_pj = 3.0;
    return cfg;
}

// AWB (Auto White Balance)
inline block_power_config awb() {
    block_power_config cfg = statistical();
    cfg.name = "awb";
    cfg.memory_kbits = 128;  // Needs histogram storage
    return cfg;
}

// AEC (Auto Exposure Control)
inline block_power_config aec() {
    block_power_config cfg = statistical();
    cfg.name = "aec";
    cfg.memory_kbits = 64;
    return cfg;
}

// Spatial operations: DPC, BNR, demosaic, sharpen, 2dnr
inline block_power_config spatial() {
    block_power_config cfg;
    cfg.name = "spatial";
    cfg.leakage_mw = 1.5;
    cfg.switched_cap_ff = 15.0;
    cfg.activity_factor = 0.3;
    cfg.has_memory = true;
    cfg.memory_kbits = 256;  // Line buffer
    cfg.memory_access_energy_pj = 4.0;
    return cfg;
}

// DPC (Defect Pixel Correction)
inline block_power_config dpc() {
    block_power_config cfg;
    cfg.name = "dpc";
    cfg.leakage_mw = 0.6;
    cfg.switched_cap_ff = 8.0;
    cfg.activity_factor = 0.2;
    cfg.ops_per_pixel = 5;  // Neighbor comparisons
    cfg.mac_energy_pj = 1.0;
    cfg.has_memory = true;
    cfg.memory_kbits = 32;  // Small buffer
    cfg.memory_access_energy_pj = 3.0;
    return cfg;
}

// BNR (Bayer Noise Reduction)
inline block_power_config bnr() {
    block_power_config cfg = spatial();
    cfg.name = "bnr";
    cfg.leakage_mw = 1.2;
    cfg.switched_cap_ff = 20.0;
    cfg.ops_per_pixel = 25;  // 5x5 kernel
    cfg.mac_energy_pj = 1.5;
    cfg.memory_kbits = 512;  // 5 line buffers
    return cfg;
}

// Demosaic
inline block_power_config demosaic() {
    block_power_config cfg = spatial();
    cfg.name = "demosaic";
    cfg.leakage_mw = 2.0;
    cfg.switched_cap_ff = 25.0;
    cfg.ops_per_pixel = 49;  // 7x7 kernel for edge-directed
    cfg.mac_energy_pj = 2.0;
    cfg.memory_kbits = 1024;  // Multiple line buffers
    return cfg;
}

// Sharpen
inline block_power_config sharpen() {
    block_power_config cfg = spatial();
    cfg.name = "sharpen";
    cfg.leakage_mw = 0.8;
    cfg.switched_cap_ff = 12.0;
    cfg.ops_per_pixel = 9;  // 3x3 kernel
    cfg.mac_energy_pj = 1.0;
    cfg.memory_kbits = 256;
    return cfg;
}

// 2DNR (2D Noise Reduction)
inline block_power_config tdnoise_reduction() {
    block_power_config cfg = spatial();
    cfg.name = "2dnr";
    cfg.leakage_mw = 1.5;
    cfg.switched_cap_ff = 18.0;
    cfg.ops_per_pixel = 25;  // 5x5 kernel
    cfg.mac_energy_pj = 1.5;
    cfg.memory_kbits = 512;
    return cfg;
}

// LSC (Lens Shading Correction) - needs LUT memory
inline block_power_config lsc() {
    block_power_config cfg;
    cfg.name = "lsc";
    cfg.leakage_mw = 0.5;
    cfg.switched_cap_ff = 6.0;
    cfg.activity_factor = 0.2;
    cfg.ops_per_pixel = 3;  // Lookup + 3 multiplications
    cfg.mac_energy_pj = 1.0;
    cfg.has_memory = true;
    cfg.memory_kbits = 256;  // LUT storage
    cfg.memory_access_energy_pj = 3.0;
    return cfg;
}

// Scale (resize)
inline block_power_config scale() {
    block_power_config cfg;
    cfg.name = "scale";
    cfg.leakage_mw = 1.0;
    cfg.switched_cap_ff = 20.0;
    cfg.activity_factor = 0.25;
    cfg.ops_per_pixel = 4;  // Bilinear interpolation
    cfg.mac_energy_pj = 1.5;
    cfg.has_memory = true;
    cfg.memory_kbits = 128;  // Line buffer for scaling
    cfg.memory_access_energy_pj = 3.0;
    return cfg;
}

// YUV420 (Chroma subsampling)
inline block_power_config yuv420() {
    block_power_config cfg;
    cfg.name = "yuv420";
    cfg.leakage_mw = 0.4;
    cfg.switched_cap_ff = 8.0;
    cfg.activity_factor = 0.15;
    cfg.has_memory = true;
    cfg.memory_kbits = 256;  // Chroma buffer
    cfg.memory_access_energy_pj = 3.0;
    return cfg;
}

// Normalizer (input processing)
inline block_power_config normalizer() {
    block_power_config cfg;
    cfg.name = "normalizer";
    cfg.leakage_mw = 0.2;
    cfg.switched_cap_ff = 3.0;
    cfg.activity_factor = 0.1;
    cfg.ops_per_pixel = 1;
    cfg.mac_energy_pj = 0.5;
    cfg.has_memory = false;
    return cfg;
}

}  // namespace block_power_defaults

// ============================================================================
// Pipeline Power Summary
// ============================================================================
struct pipeline_power_summary {
    std::vector<std::pair<std::string, power_values>> block_powers;
    power_values total;
    double frame_time_us = 0.0;
    double frame_energy_nj = 0.0;
    double avg_power_mw = 0.0;
    double peak_power_mw = 0.0;

    void add_block(const std::string& name, const power_values& power) {
        block_powers.emplace_back(name, power);
        total += power;
        if (power.total_mw > peak_power_mw) {
            peak_power_mw = power.total_mw;
        }
    }

    void calculate_frame_stats(double frame_time, double fps) {
        frame_time_us = frame_time;
        double period_us = (fps > 0) ? (1e6 / fps) : 0;
        frame_energy_nj = total.total_mw * frame_time / 1000.0;  // nJ
        avg_power_mw = (period_us > 0) ? (frame_energy_nj * 1000.0 / period_us) : 0;
    }
};

#endif  // ISP_POWER_H
