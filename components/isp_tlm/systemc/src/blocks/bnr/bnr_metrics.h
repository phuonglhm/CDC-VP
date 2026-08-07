/*
 * BNR (Bayer Noise Reduction) Metrics
 * Specialized metrics collection for BNR block
 */

#ifndef BNR_METRICS_H
#define BNR_METRICS_H

#include "metrics/isp_metrics_base.h"

struct BnrMetrics {
    uint64_t filtered_pixels = 0;
    uint64_t kernel_applications = 0;
    uint64_t interpolated_green_pixels = 0;
    uint64_t jbf_applications = 0;

    static constexpr unsigned LUT4_EST = 2000;
    static constexpr unsigned FF_COUNT = 1200;
    static constexpr unsigned DSP_MULTS = 8;
    static constexpr unsigned BRAM_KBITS = 128;

    void reset() {
        filtered_pixels = 0;
        kernel_applications = 0;
        interpolated_green_pixels = 0;
        jbf_applications = 0;
    }
};

class BnrMetricsCollector : public IspMetricsBase {
public:
    BnrMetricsCollector()
        : IspMetricsBase("BNR")
    {
        m_metrics.lut4_estimate = BnrMetrics::LUT4_EST;
        m_metrics.ff_count = BnrMetrics::FF_COUNT;
        m_metrics.dsp_mults = BnrMetrics::DSP_MULTS;
        m_metrics.bram_kbits = BnrMetrics::BRAM_KBITS;
    }

    void record_filtered_pixel() { m_bnr.filtered_pixels++; }
    void record_kernel_app() { m_bnr.kernel_applications++; }
    void record_green_interp() { m_bnr.interpolated_green_pixels++; }
    void record_jbf_app() { m_bnr.jbf_applications++; }

    void reset() override {
        IspMetricsBase::reset();
        m_bnr.reset();
        m_metrics.lut4_estimate = BnrMetrics::LUT4_EST;
        m_metrics.ff_count = BnrMetrics::FF_COUNT;
        m_metrics.dsp_mults = BnrMetrics::DSP_MULTS;
        m_metrics.bram_kbits = BnrMetrics::BRAM_KBITS;
    }

    const BnrMetrics& get_bnr_metrics() const { return m_bnr; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);
        os << "\n  [BNR-Specific]" << "\n";
        os << std::setw(30) << std::left << "  Filtered Pixels:"
           << m_bnr.filtered_pixels << "\n";
        os << std::setw(30) << std::left << "  Kernel Applications:"
           << m_bnr.kernel_applications << "\n";
        os << std::setw(30) << std::left << "  Green Interpolations:"
           << m_bnr.interpolated_green_pixels << "\n";
        os << std::setw(30) << std::left << "  JBF Applications:"
           << m_bnr.jbf_applications << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();
        std::string json = base_json.str();
        json.pop_back();
        json += ",\n  \"bnr_specific\": {\n";
        json += "    \"filtered_pixels\": " + std::to_string(m_bnr.filtered_pixels) + "\n";
        json += "  }\n}";
        return json;
    }

private:
    BnrMetrics m_bnr;
};

#endif // BNR_METRICS_H
