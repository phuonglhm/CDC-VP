/*
 * SHARP (Sharpening) Metrics
 * Specialized metrics collection for SHARP block
 */

#ifndef SHARPEN_METRICS_H
#define SHARPEN_METRICS_H

#include "metrics/isp_metrics_base.h"

struct SharpenMetrics {
    uint64_t sharpened_pixels = 0;
    uint64_t kernel_applications = 0;
    uint64_t edge_enhancement = 0;
    uint64_t overflow_clips = 0;

    static constexpr unsigned LUT4_EST = 1200;
    static constexpr unsigned FF_COUNT = 800;
    static constexpr unsigned DSP_MULTS = 4;
    static constexpr unsigned BRAM_KBITS = 64;

    void reset() {
        sharpened_pixels = 0;
        kernel_applications = 0;
        edge_enhancement = 0;
        overflow_clips = 0;
    }
};

class SharpenMetricsCollector : public IspMetricsBase {
public:
    SharpenMetricsCollector()
        : IspMetricsBase("SHARP")
    {
        m_metrics.lut4_estimate = SharpenMetrics::LUT4_EST;
        m_metrics.ff_count = SharpenMetrics::FF_COUNT;
        m_metrics.dsp_mults = SharpenMetrics::DSP_MULTS;
        m_metrics.bram_kbits = SharpenMetrics::BRAM_KBITS;
    }

    void record_sharpened_pixel() { m_sh.sharpened_pixels++; }
    void record_kernel_app() { m_sh.kernel_applications++; }
    void record_edge_enhancement() { m_sh.edge_enhancement++; }
    void record_overflow_clip() { m_sh.overflow_clips++; }

    void reset() override {
        IspMetricsBase::reset();
        m_sh.reset();
        m_metrics.lut4_estimate = SharpenMetrics::LUT4_EST;
        m_metrics.ff_count = SharpenMetrics::FF_COUNT;
        m_metrics.dsp_mults = SharpenMetrics::DSP_MULTS;
        m_metrics.bram_kbits = SharpenMetrics::BRAM_KBITS;
    }

    const SharpenMetrics& get_sharpen_metrics() const { return m_sh; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);
        os << "\n  [SHARP-Specific]" << "\n";
        os << std::setw(30) << std::left << "  Sharpened Pixels:"
           << m_sh.sharpened_pixels << "\n";
        os << std::setw(30) << std::left << "  Kernel Applications:"
           << m_sh.kernel_applications << "\n";
        os << std::setw(30) << std::left << "  Edge Enhancements:"
           << m_sh.edge_enhancement << "\n";
        os << std::setw(30) << std::left << "  Overflow Clips:"
           << m_sh.overflow_clips << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();
        std::string json = base_json.str();
        json.pop_back();
        json += ",\n  \"sharpen_specific\": {\n";
        json += "    \"sharpened_pixels\": " + std::to_string(m_sh.sharpened_pixels) + ",\n";
        json += "    \"kernel_applications\": " + std::to_string(m_sh.kernel_applications) + "\n";
        json += "  }\n}";
        return json;
    }

private:
    SharpenMetrics m_sh;
};

#endif // SHARPEN_METRICS_H
