/*
 * AWB (Auto White Balance) Metrics
 * Specialized metrics collection for AWB block
 */

#ifndef AWB_METRICS_H
#define AWB_METRICS_H

#include "metrics/isp_metrics_base.h"

struct AwbMetrics {
    uint64_t valid_pixels = 0;
    uint64_t overexposed_count = 0;
    uint64_t underexposed_count = 0;
    double computed_r_gain = 0.0;
    double computed_b_gain = 0.0;
    uint64_t frames_processed = 0;

    static constexpr unsigned LUT4_EST = 400;
    static constexpr unsigned FF_COUNT = 300;
    static constexpr unsigned DSP_MULTS = 3;
    static constexpr unsigned BRAM_KBITS = 32;

    void reset() {
        valid_pixels = 0;
        overexposed_count = 0;
        underexposed_count = 0;
        computed_r_gain = 0.0;
        computed_b_gain = 0.0;
        frames_processed = 0;
    }
};

class AwbMetricsCollector : public IspMetricsBase {
public:
    AwbMetricsCollector()
        : IspMetricsBase("AWB")
    {
        m_metrics.lut4_estimate = AwbMetrics::LUT4_EST;
        m_metrics.ff_count = AwbMetrics::FF_COUNT;
        m_metrics.dsp_mults = AwbMetrics::DSP_MULTS;
        m_metrics.bram_kbits = AwbMetrics::BRAM_KBITS;
    }

    void record_valid_pixel() { m_awb.valid_pixels++; }
    void record_overexposed() { m_awb.overexposed_count++; }
    void record_underexposed() { m_awb.underexposed_count++; }
    void record_r_gain(double g) { m_awb.computed_r_gain = g; }
    void record_b_gain(double g) { m_awb.computed_b_gain = g; }
    void record_frame_processed() { m_awb.frames_processed++; }

    void reset() override {
        IspMetricsBase::reset();
        m_awb.reset();
        m_metrics.lut4_estimate = AwbMetrics::LUT4_EST;
        m_metrics.ff_count = AwbMetrics::FF_COUNT;
        m_metrics.dsp_mults = AwbMetrics::DSP_MULTS;
        m_metrics.bram_kbits = AwbMetrics::BRAM_KBITS;
    }

    const AwbMetrics& get_awb_metrics() const { return m_awb; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);
        os << "\n  [AWB-Specific]" << "\n";
        os << std::setw(30) << std::left << "  Valid Pixels:"
           << m_awb.valid_pixels << "\n";
        os << std::setw(30) << std::left << "  Overexposed Count:"
           << m_awb.overexposed_count << "\n";
        os << std::setw(30) << std::left << "  Underexposed Count:"
           << m_awb.underexposed_count << "\n";
        os << std::setw(30) << std::left << "  Computed R Gain:"
           << std::fixed << std::setprecision(4) << m_awb.computed_r_gain << "\n";
        os << std::setw(30) << std::left << "  Computed B Gain:"
           << std::fixed << std::setprecision(4) << m_awb.computed_b_gain << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();
        std::string json = base_json.str();
        json.pop_back();
        json += ",\n  \"awb_specific\": {\n";
        json += "    \"valid_pixels\": " + std::to_string(m_awb.valid_pixels) + ",\n";
        json += "    \"computed_r_gain\": " + std::to_string(m_awb.computed_r_gain) + ",\n";
        json += "    \"computed_b_gain\": " + std::to_string(m_awb.computed_b_gain) + "\n";
        json += "  }\n}";
        return json;
    }

private:
    AwbMetrics m_awb;
};

#endif // AWB_METRICS_H
