/*
 * AE (Auto Exposure) Metrics
 * Specialized metrics collection for AE block
 */

#ifndef AE_METRICS_H
#define AE_METRICS_H

#include "metrics/isp_metrics_base.h"

struct AeMetrics {
    uint64_t ae_iterations = 0;
    uint64_t exposure_adjustments = 0;
    double skewness_value = 0.0;
    double target_skewness = 0.0;

    static constexpr unsigned LUT4_EST = 500;
    static constexpr unsigned FF_COUNT = 400;
    static constexpr unsigned DSP_MULTS = 4;
    static constexpr unsigned BRAM_KBITS = 64;

    void reset() {
        ae_iterations = 0;
        exposure_adjustments = 0;
        skewness_value = 0.0;
        target_skewness = 0.0;
    }
};

class AeMetricsCollector : public IspMetricsBase {
public:
    AeMetricsCollector()
        : IspMetricsBase("AE")
    {
        m_metrics.lut4_estimate = AeMetrics::LUT4_EST;
        m_metrics.ff_count = AeMetrics::FF_COUNT;
        m_metrics.dsp_mults = AeMetrics::DSP_MULTS;
        m_metrics.bram_kbits = AeMetrics::BRAM_KBITS;
    }

    void record_ae_iteration() { m_ae.ae_iterations++; }
    void record_exposure_adjustment() { m_ae.exposure_adjustments++; }
    void record_skewness(double s) { m_ae.skewness_value = s; }

    void reset() override {
        IspMetricsBase::reset();
        m_ae.reset();
        m_metrics.lut4_estimate = AeMetrics::LUT4_EST;
        m_metrics.ff_count = AeMetrics::FF_COUNT;
        m_metrics.dsp_mults = AeMetrics::DSP_MULTS;
        m_metrics.bram_kbits = AeMetrics::BRAM_KBITS;
    }

    const AeMetrics& get_ae_metrics() const { return m_ae; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);
        os << "\n  [AE-Specific]" << "\n";
        os << std::setw(30) << std::left << "  AE Iterations:"
           << m_ae.ae_iterations << "\n";
        os << std::setw(30) << std::left << "  Exposure Adjustments:"
           << m_ae.exposure_adjustments << "\n";
        os << std::setw(30) << std::left << "  Current Skewness:"
           << std::fixed << std::setprecision(4) << m_ae.skewness_value << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();
        std::string json = base_json.str();
        json.pop_back();
        json += ",\n  \"ae_specific\": {\n";
        json += "    \"ae_iterations\": " + std::to_string(m_ae.ae_iterations) + ",\n";
        json += "    \"skewness_value\": " + std::to_string(m_ae.skewness_value) + "\n";
        json += "  }\n}";
        return json;
    }

private:
    AeMetrics m_ae;
};

#endif // AE_METRICS_H
