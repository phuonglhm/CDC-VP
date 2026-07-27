/*
 * CCM (Color Correction Matrix) Metrics
 * Specialized metrics collection for CCM block
 */

#ifndef CCM_METRICS_H
#define CCM_METRICS_H

#include "metrics/isp_metrics_base.h"

//=============================================================================
// CCM-specific Metrics
//=============================================================================
struct CcmMetrics {
    uint64_t ccm_multiplications = 0;
    uint64_t clipped_pixels = 0;
    uint64_t saturation_count = 0;
    double saturation_rate = 0.0;

    static constexpr unsigned LUT4_EST = 500;
    static constexpr unsigned FF_COUNT = 400;
    static constexpr unsigned DSP_MULTS = 6;
    static constexpr unsigned BRAM_KBITS = 0;

    void reset() {
        ccm_multiplications = 0;
        clipped_pixels = 0;
        saturation_count = 0;
        saturation_rate = 0.0;
    }
};

//=============================================================================
// CCM Metrics Collector
//=============================================================================
class CcmMetricsCollector : public IspMetricsBase {
public:
    CcmMetricsCollector()
        : IspMetricsBase("CCM")
    {
        m_metrics.lut4_estimate = CcmMetrics::LUT4_EST;
        m_metrics.ff_count = CcmMetrics::FF_COUNT;
        m_metrics.dsp_mults = CcmMetrics::DSP_MULTS;
        m_metrics.bram_kbits = CcmMetrics::BRAM_KBITS;
    }

    void record_ccm_multiplication() { m_ccm.ccm_multiplications++; }
    void record_clip() { m_ccm.clipped_pixels++; }
    void record_saturation() { m_ccm.saturation_count++; }

    void compute_saturation_rate() {
        if (m_metrics.pixel_count > 0) {
            m_ccm.saturation_rate = 100.0 * m_ccm.saturation_count / m_metrics.pixel_count;
        }
    }

    void reset() override {
        IspMetricsBase::reset();
        m_ccm.reset();
        m_metrics.lut4_estimate = CcmMetrics::LUT4_EST;
        m_metrics.ff_count = CcmMetrics::FF_COUNT;
        m_metrics.dsp_mults = CcmMetrics::DSP_MULTS;
        m_metrics.bram_kbits = CcmMetrics::BRAM_KBITS;
    }

    const CcmMetrics& get_ccm_metrics() const { return m_ccm; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);

        os << "\n  [CCM-Specific]" << "\n";
        os << std::setw(30) << std::left << "  CCM Multiplications:"
           << m_ccm.ccm_multiplications << "\n";
        os << std::setw(30) << std::left << "  Clipped Pixels:"
           << m_ccm.clipped_pixels << "\n";
        os << std::setw(30) << std::left << "  Saturation Count:"
           << m_ccm.saturation_count << "\n";
        os << std::setw(30) << std::left << "  Saturation Rate:"
           << std::fixed << std::setprecision(4) << m_ccm.saturation_rate << "%\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();

        std::string json = base_json.str();
        json.pop_back();

        json += ",\n";
        json += "  \"ccm_specific\": {\n";
        json += "    \"ccm_multiplications\": " + std::to_string(m_ccm.ccm_multiplications) + ",\n";
        json += "    \"clipped_pixels\": " + std::to_string(m_ccm.clipped_pixels) + ",\n";
        json += "    \"saturation_count\": " + std::to_string(m_ccm.saturation_count) + ",\n";
        json += "    \"saturation_rate\": " + std::to_string(m_ccm.saturation_rate) + "\n";
        json += "  }\n";
        json += "}";

        return json;
    }

private:
    CcmMetrics m_ccm;
};

#endif // CCM_METRICS_H
