/*
 * OECF (Opto-Electronic Conversion Function) Metrics
 */

#ifndef OECF_METRICS_H
#define OECF_METRICS_H

#include "metrics/isp_metrics_base.h"

struct OecfMetrics {
    uint64_t lut_reads = 0;
    uint64_t r_channel_reads = 0;
    uint64_t gr_channel_reads = 0;
    uint64_t gb_channel_reads = 0;
    uint64_t b_channel_reads = 0;

    static constexpr unsigned LUT4_EST = 100;
    static constexpr unsigned FF_COUNT = 50;
    static constexpr unsigned DSP_MULTS = 0;
    static constexpr unsigned BRAM_KBITS = 32;

    void reset() {
        lut_reads = 0;
        r_channel_reads = 0;
        gr_channel_reads = 0;
        gb_channel_reads = 0;
        b_channel_reads = 0;
    }
};

class OecfMetricsCollector : public IspMetricsBase {
public:
    OecfMetricsCollector()
        : IspMetricsBase("OECF")
    {
        m_metrics.lut4_estimate = OecfMetrics::LUT4_EST;
        m_metrics.ff_count = OecfMetrics::FF_COUNT;
        m_metrics.dsp_mults = OecfMetrics::DSP_MULTS;
        m_metrics.bram_kbits = OecfMetrics::BRAM_KBITS;
    }

    void record_lut_read() { m_oecf.lut_reads++; }
    void record_r_channel() { m_oecf.r_channel_reads++; }
    void record_gr_channel() { m_oecf.gr_channel_reads++; }
    void record_gb_channel() { m_oecf.gb_channel_reads++; }
    void record_b_channel() { m_oecf.b_channel_reads++; }

    void reset() override {
        IspMetricsBase::reset();
        m_oecf.reset();
    }

    const OecfMetrics& get_oecf_metrics() const { return m_oecf; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);
        os << "\n  [OECF-Specific]" << "\n";
        os << std::setw(30) << std::left << "  LUT Reads:"
           << m_oecf.lut_reads << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();
        std::string json = base_json.str();
        json.pop_back();
        json += ",\n  \"oecf_specific\": {\n";
        json += "    \"lut_reads\": " + std::to_string(m_oecf.lut_reads) + "\n";
        json += "  }\n}";
        return json;
    }

private:
    OecfMetrics m_oecf;
};

#endif // OECF_METRICS_H
