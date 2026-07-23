/*
 * BLC (Black Level Correction) Metrics
 * Specialized metrics collection for BLC block
 */

#ifndef BLC_METRICS_H
#define BLC_METRICS_H

#include "metrics/isp_metrics_base.h"

//=============================================================================
// BLC-specific Metrics
//=============================================================================
struct BlcMetrics {
    // BLC-specific operation counts
    uint64_t r_channel_corrections = 0;
    uint64_t gr_channel_corrections = 0;
    uint64_t gb_channel_corrections = 0;
    uint64_t b_channel_corrections = 0;
    uint64_t saturation_clips = 0;      // Pixels clipped to max
    uint64_t linear_corrections = 0;   // Using linear mode
    uint64_t nonlinear_corrections = 0; // Using nonlinear mode

    // Saturation analysis
    uint32_t max_value_seen = 0;
    uint32_t min_value_seen = 0x3FF;  // 10-bit max

    // BLC is simple subtraction - resources are minimal
    static constexpr unsigned LUT4_EST = 150;   // 4 subtracters + mux
    static constexpr unsigned FF_COUNT = 128;   // 4 channels x 32 bits + state
    static constexpr unsigned DSP_MULTS = 0;     // No multipliers needed
    static constexpr unsigned BRAM_KBITS = 0;    // No BRAM needed

    void reset() {
        r_channel_corrections = 0;
        gr_channel_corrections = 0;
        gb_channel_corrections = 0;
        b_channel_corrections = 0;
        saturation_clips = 0;
        linear_corrections = 0;
        nonlinear_corrections = 0;
        max_value_seen = 0;
        min_value_seen = 0x3FF;
    }
};

//=============================================================================
// BLC Metrics Collector
//=============================================================================
class BlcMetricsCollector : public IspMetricsBase {
public:
    BlcMetricsCollector()
        : IspMetricsBase("BLC")
    {
        // Set BLC-specific resource estimates
        m_metrics.lut4_estimate = BlcMetrics::LUT4_EST;
        m_metrics.ff_count = BlcMetrics::FF_COUNT;
        m_metrics.dsp_mults = BlcMetrics::DSP_MULTS;
        m_metrics.bram_kbits = BlcMetrics::BRAM_KBITS;
    }

    // Record channel-specific corrections
    void record_channel_correction(int channel) {
        switch (channel) {
            case 0: m_blc.r_channel_corrections++; break;  // R
            case 1: m_blc.gr_channel_corrections++; break; // Gr
            case 2: m_blc.gb_channel_corrections++; break; // Gb
            case 3: m_blc.b_channel_corrections++; break;  // B
        }
    }

    void record_saturation_clip() { m_blc.saturation_clips++; }
    void record_linear_correction() { m_blc.linear_corrections++; }
    void record_nonlinear_correction() { m_blc.nonlinear_corrections++; }

    // Record value range (for debugging)
    void record_pixel_value(uint32_t value) {
        if (value > m_blc.max_value_seen) m_blc.max_value_seen = value;
        if (value < m_blc.min_value_seen) m_blc.min_value_seen = value;
    }

    // Reset
    void reset() override {
        IspMetricsBase::reset();
        m_blc.reset();

        // Re-set resource estimates
        m_metrics.lut4_estimate = BlcMetrics::LUT4_EST;
        m_metrics.ff_count = BlcMetrics::FF_COUNT;
        m_metrics.dsp_mults = BlcMetrics::DSP_MULTS;
        m_metrics.bram_kbits = BlcMetrics::BRAM_KBITS;
    }

    // Get BLC-specific metrics
    const BlcMetrics& get_blc_metrics() const { return m_blc; }

    // Image dimensions
    unsigned get_width() const { return m_metrics.width; }
    unsigned get_height() const { return m_metrics.height; }

    // Print BLC-specific summary
    void print_summary(std::ostream& os = std::cout) const override {
        // Call base class summary
        IspMetricsBase::print_summary(os);

        // Add BLC-specific info
        os << "\n  [BLC-Specific]" << "\n";
        os << std::setw(30) << std::left << "  R Channel Corrections:"
           << m_blc.r_channel_corrections << "\n";
        os << std::setw(30) << std::left << "  Gr Channel Corrections:"
           << m_blc.gr_channel_corrections << "\n";
        os << std::setw(30) << std::left << "  Gb Channel Corrections:"
           << m_blc.gb_channel_corrections << "\n";
        os << std::setw(30) << std::left << "  B Channel Corrections:"
           << m_blc.b_channel_corrections << "\n";
        os << std::setw(30) << std::left << "  Saturation Clips:"
           << m_blc.saturation_clips << "\n";
        os << std::setw(30) << std::left << "  Linear Corrections:"
           << m_blc.linear_corrections << "\n";
        os << std::setw(30) << std::left << "  Nonlinear Corrections:"
           << m_blc.nonlinear_corrections << "\n";

        if (m_metrics.pixel_count > 0) {
            double clip_rate = 100.0 * m_blc.saturation_clips / m_metrics.pixel_count;
            os << std::setw(30) << std::left << "  Saturation Clip Rate:"
               << std::fixed << std::setprecision(4) << clip_rate << "%\n";
        }

        os << std::setw(30) << std::left << "  Max Value Seen:"
           << m_blc.max_value_seen << "\n";
        os << std::setw(30) << std::left << "  Min Value Seen:"
           << m_blc.min_value_seen << "\n";
    }

    // Export to JSON with BLC-specific fields
    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();

        // Parse base JSON and extend it
        std::string json = base_json.str();
        // Remove closing brace
        json.pop_back();

        // Add BLC-specific fields
        json += ",\n";
        json += "  \"blc_specific\": {\n";
        json += "    \"r_channel_corrections\": " + std::to_string(m_blc.r_channel_corrections) + ",\n";
        json += "    \"gr_channel_corrections\": " + std::to_string(m_blc.gr_channel_corrections) + ",\n";
        json += "    \"gb_channel_corrections\": " + std::to_string(m_blc.gb_channel_corrections) + ",\n";
        json += "    \"b_channel_corrections\": " + std::to_string(m_blc.b_channel_corrections) + ",\n";
        json += "    \"saturation_clips\": " + std::to_string(m_blc.saturation_clips) + ",\n";
        json += "    \"linear_corrections\": " + std::to_string(m_blc.linear_corrections) + ",\n";
        json += "    \"nonlinear_corrections\": " + std::to_string(m_blc.nonlinear_corrections) + ",\n";
        json += "    \"max_value_seen\": " + std::to_string(m_blc.max_value_seen) + ",\n";
        json += "    \"min_value_seen\": " + std::to_string(m_blc.min_value_seen) + "\n";
        json += "  }\n";
        json += "}";

        return json;
    }

private:
    BlcMetrics m_blc;
};

#endif // BLC_METRICS_H
