/*
 * WB (White Balance) Metrics
 * Specialized metrics collection for WB block
 */

#ifndef WB_METRICS_H
#define WB_METRICS_H

#include "metrics/isp_metrics_base.h"

//=============================================================================
// WB-specific Metrics
//=============================================================================
struct WbMetrics {
    uint64_t r_gain_applications = 0;
    uint64_t gr_gain_applications = 0;
    uint64_t gb_gain_applications = 0;
    uint64_t b_gain_applications = 0;
    uint64_t clipped_pixels = 0;
    uint64_t total_gain_multiplications = 0;

    static constexpr unsigned LUT4_EST = 200;
    static constexpr unsigned FF_COUNT = 300;
    static constexpr unsigned DSP_MULTS = 1;
    static constexpr unsigned BRAM_KBITS = 0;

    void reset() {
        r_gain_applications = 0;
        gr_gain_applications = 0;
        gb_gain_applications = 0;
        b_gain_applications = 0;
        clipped_pixels = 0;
        total_gain_multiplications = 0;
    }
};

//=============================================================================
// WB Metrics Collector
//=============================================================================
class WbMetricsCollector : public IspMetricsBase {
public:
    WbMetricsCollector()
        : IspMetricsBase("WB")
    {
        m_metrics.lut4_estimate = WbMetrics::LUT4_EST;
        m_metrics.ff_count = WbMetrics::FF_COUNT;
        m_metrics.dsp_mults = WbMetrics::DSP_MULTS;
        m_metrics.bram_kbits = WbMetrics::BRAM_KBITS;
    }

    void record_r_gain() { m_wb.r_gain_applications++; m_wb.total_gain_multiplications++; }
    void record_gr_gain() { m_wb.gr_gain_applications++; }
    void record_gb_gain() { m_wb.gb_gain_applications++; }
    void record_b_gain() { m_wb.b_gain_applications++; m_wb.total_gain_multiplications++; }
    void record_clip() { m_wb.clipped_pixels++; }
    void record_mul() { m_wb.total_gain_multiplications++; }

    void reset() override {
        IspMetricsBase::reset();
        m_wb.reset();
        m_metrics.lut4_estimate = WbMetrics::LUT4_EST;
        m_metrics.ff_count = WbMetrics::FF_COUNT;
        m_metrics.dsp_mults = WbMetrics::DSP_MULTS;
        m_metrics.bram_kbits = WbMetrics::BRAM_KBITS;
    }

    const WbMetrics& get_wb_metrics() const { return m_wb; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);

        os << "\n  [WB-Specific]" << "\n";
        os << std::setw(30) << std::left << "  R Gain Applications:"
           << m_wb.r_gain_applications << "\n";
        os << std::setw(30) << std::left << "  Gr Gain Applications:"
           << m_wb.gr_gain_applications << "\n";
        os << std::setw(30) << std::left << "  Gb Gain Applications:"
           << m_wb.gb_gain_applications << "\n";
        os << std::setw(30) << std::left << "  B Gain Applications:"
           << m_wb.b_gain_applications << "\n";
        os << std::setw(30) << std::left << "  Clipped Pixels:"
           << m_wb.clipped_pixels << "\n";
        os << std::setw(30) << std::left << "  Total Multiplications:"
           << m_wb.total_gain_multiplications << "\n";

        if (m_metrics.pixel_count > 0) {
            double clip_rate = 100.0 * m_wb.clipped_pixels / m_metrics.pixel_count;
            os << std::setw(30) << std::left << "  Saturation Clip Rate:"
               << std::fixed << std::setprecision(4) << clip_rate << "%\n";
        }
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();

        std::string json = base_json.str();
        json.pop_back();

        json += ",\n";
        json += "  \"wb_specific\": {\n";
        json += "    \"r_gain_applications\": " + std::to_string(m_wb.r_gain_applications) + ",\n";
        json += "    \"gr_gain_applications\": " + std::to_string(m_wb.gr_gain_applications) + ",\n";
        json += "    \"gb_gain_applications\": " + std::to_string(m_wb.gb_gain_applications) + ",\n";
        json += "    \"b_gain_applications\": " + std::to_string(m_wb.b_gain_applications) + ",\n";
        json += "    \"clipped_pixels\": " + std::to_string(m_wb.clipped_pixels) + ",\n";
        json += "    \"total_multiplications\": " + std::to_string(m_wb.total_gain_multiplications) + "\n";
        json += "  }\n";
        json += "}";

        return json;
    }

private:
    WbMetrics m_wb;
};

#endif // WB_METRICS_H
