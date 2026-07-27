/*
 * GAMMA Metrics
 * Specialized metrics collection for GAMMA block
 */

#ifndef GAMMA_METRICS_H
#define GAMMA_METRICS_H

#include "metrics/isp_metrics_base.h"

struct GammaMetrics {
    uint64_t lut_lookups = 0;
    uint64_t r_lut_reads = 0;
    uint64_t g_lut_reads = 0;
    uint64_t b_lut_reads = 0;
    uint64_t input_distribution[16] = {0};
    uint64_t output_distribution[16] = {0};

    static constexpr unsigned LUT4_EST = 600;
    static constexpr unsigned FF_COUNT = 400;
    static constexpr unsigned DSP_MULTS = 0;
    static constexpr unsigned BRAM_KBITS = 128;

    void reset() {
        lut_lookups = 0;
        r_lut_reads = 0;
        g_lut_reads = 0;
        b_lut_reads = 0;
        for (int i = 0; i < 16; i++) {
            input_distribution[i] = 0;
            output_distribution[i] = 0;
        }
    }
};

class GammaMetricsCollector : public IspMetricsBase {
public:
    GammaMetricsCollector()
        : IspMetricsBase("GAMMA")
    {
        m_metrics.lut4_estimate = GammaMetrics::LUT4_EST;
        m_metrics.ff_count = GammaMetrics::FF_COUNT;
        m_metrics.dsp_mults = GammaMetrics::DSP_MULTS;
        m_metrics.bram_kbits = GammaMetrics::BRAM_KBITS;
    }

    void record_lut_lookup() { m_gm.lut_lookups++; }
    void record_r_lut() { m_gm.r_lut_reads++; }
    void record_g_lut() { m_gm.g_lut_reads++; }
    void record_b_lut() { m_gm.b_lut_reads++; }
    void record_input_bin(uint16_t val) {
        if (val <= 1023) {
            m_gm.input_distribution[val >> 6]++;
        }
    }
    void record_output_bin(uint16_t val) {
        if (val <= 1023) {
            m_gm.output_distribution[val >> 6]++;
        }
    }

    void reset() override {
        IspMetricsBase::reset();
        m_gm.reset();
        m_metrics.lut4_estimate = GammaMetrics::LUT4_EST;
        m_metrics.ff_count = GammaMetrics::FF_COUNT;
        m_metrics.dsp_mults = GammaMetrics::DSP_MULTS;
        m_metrics.bram_kbits = GammaMetrics::BRAM_KBITS;
    }

    const GammaMetrics& get_gamma_metrics() const { return m_gm; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);
        os << "\n  [GAMMA-Specific]" << "\n";
        os << std::setw(30) << std::left << "  LUT Lookups:"
           << m_gm.lut_lookups << "\n";
        os << std::setw(30) << std::left << "  R LUT Reads:"
           << m_gm.r_lut_reads << "\n";
        os << std::setw(30) << std::left << "  G LUT Reads:"
           << m_gm.g_lut_reads << "\n";
        os << std::setw(30) << std::left << "  B LUT Reads:"
           << m_gm.b_lut_reads << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();
        std::string json = base_json.str();
        json.pop_back();
        json += ",\n  \"gamma_specific\": {\n";
        json += "    \"lut_lookups\": " + std::to_string(m_gm.lut_lookups) + "\n";
        json += "  }\n}";
        return json;
    }

private:
    GammaMetrics m_gm;
};

#endif // GAMMA_METRICS_H
