/*
 * 2DNR (2D Noise Reduction) Metrics
 * Specialized metrics collection for 2DNR block
 */

#ifndef NR2D_METRICS_H
#define NR2D_METRICS_H

#include "metrics/isp_metrics_base.h"

struct Nr2dMetrics {
    uint64_t filtered_pixels = 0;
    uint64_t weight_distributions[32] = {0};
    uint64_t noise_reduction_sum = 0;

    static constexpr unsigned LUT4_EST = 1500;
    static constexpr unsigned FF_COUNT = 1000;
    static constexpr unsigned DSP_MULTS = 4;
    static constexpr unsigned BRAM_KBITS = 64;

    void reset() {
        filtered_pixels = 0;
        noise_reduction_sum = 0;
        for (int i = 0; i < 32; i++) {
            weight_distributions[i] = 0;
        }
    }
};

class Nr2dMetricsCollector : public IspMetricsBase {
public:
    Nr2dMetricsCollector()
        : IspMetricsBase("2DNR")
    {
        m_metrics.lut4_estimate = Nr2dMetrics::LUT4_EST;
        m_metrics.ff_count = Nr2dMetrics::FF_COUNT;
        m_metrics.dsp_mults = Nr2dMetrics::DSP_MULTS;
        m_metrics.bram_kbits = Nr2dMetrics::BRAM_KBITS;
    }

    void record_filtered_pixel() { m_n2d.filtered_pixels++; }
    void record_weight_bin(unsigned idx) {
        if (idx < 32) m_n2d.weight_distributions[idx]++;
    }
    void record_noise_reduction(int diff) {
        m_n2d.noise_reduction_sum += (diff > 0) ? diff : -diff;
    }

    void reset() override {
        IspMetricsBase::reset();
        m_n2d.reset();
        m_metrics.lut4_estimate = Nr2dMetrics::LUT4_EST;
        m_metrics.ff_count = Nr2dMetrics::FF_COUNT;
        m_metrics.dsp_mults = Nr2dMetrics::DSP_MULTS;
        m_metrics.bram_kbits = Nr2dMetrics::BRAM_KBITS;
    }

    const Nr2dMetrics& get_nr2d_metrics() const { return m_n2d; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);
        os << "\n  [2DNR-Specific]" << "\n";
        os << std::setw(30) << std::left << "  Filtered Pixels:"
           << m_n2d.filtered_pixels << "\n";
        os << std::setw(30) << std::left << "  Noise Reduction Sum:"
           << m_n2d.noise_reduction_sum << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();
        std::string json = base_json.str();
        json.pop_back();
        json += ",\n  \"nr2d_specific\": {\n";
        json += "    \"filtered_pixels\": " + std::to_string(m_n2d.filtered_pixels) + "\n";
        json += "  }\n}";
        return json;
    }

private:
    Nr2dMetrics m_n2d;
};

#endif // NR2D_METRICS_H
