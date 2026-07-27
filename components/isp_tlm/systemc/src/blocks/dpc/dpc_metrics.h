/*
 * DPC (Defective Pixel Correction) Metrics
 * Specialized metrics collection for DPC block
 */

#ifndef DPC_METRICS_H
#define DPC_METRICS_H

#include "metrics/isp_metrics_base.h"

struct DpcMetrics {
    uint64_t corrected_pixels = 0;
    uint64_t hot_pixels = 0;
    uint64_t dead_pixels = 0;
    uint64_t gradient_vertical = 0;
    uint64_t gradient_horizontal = 0;
    uint64_t gradient_left_diag = 0;
    uint64_t gradient_right_diag = 0;

    static constexpr unsigned LUT4_EST = 600;
    static constexpr unsigned FF_COUNT = 500;
    static constexpr unsigned DSP_MULTS = 2;
    static constexpr unsigned BRAM_KBITS = 16;

    void reset() {
        corrected_pixels = 0;
        hot_pixels = 0;
        dead_pixels = 0;
        gradient_vertical = 0;
        gradient_horizontal = 0;
        gradient_left_diag = 0;
        gradient_right_diag = 0;
    }
};

class DpcMetricsCollector : public IspMetricsBase {
public:
    DpcMetricsCollector()
        : IspMetricsBase("DPC")
    {
        m_metrics.lut4_estimate = DpcMetrics::LUT4_EST;
        m_metrics.ff_count = DpcMetrics::FF_COUNT;
        m_metrics.dsp_mults = DpcMetrics::DSP_MULTS;
        m_metrics.bram_kbits = DpcMetrics::BRAM_KBITS;
    }

    void record_corrected() { m_dpc.corrected_pixels++; }
    void record_hot_pixel() { m_dpc.hot_pixels++; }
    void record_dead_pixel() { m_dpc.dead_pixels++; }
    void record_vertical_grad() { m_dpc.gradient_vertical++; }
    void record_horizontal_grad() { m_dpc.gradient_horizontal++; }
    void record_left_diag_grad() { m_dpc.gradient_left_diag++; }
    void record_right_diag_grad() { m_dpc.gradient_right_diag++; }

    void reset() override {
        IspMetricsBase::reset();
        m_dpc.reset();
        m_metrics.lut4_estimate = DpcMetrics::LUT4_EST;
        m_metrics.ff_count = DpcMetrics::FF_COUNT;
        m_metrics.dsp_mults = DpcMetrics::DSP_MULTS;
        m_metrics.bram_kbits = DpcMetrics::BRAM_KBITS;
    }

    const DpcMetrics& get_dpc_metrics() const { return m_dpc; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);
        os << "\n  [DPC-Specific]" << "\n";
        os << std::setw(30) << std::left << "  Corrected Pixels:"
           << m_dpc.corrected_pixels << "\n";
        os << std::setw(30) << std::left << "  Hot Pixels:"
           << m_dpc.hot_pixels << "\n";
        os << std::setw(30) << std::left << "  Dead Pixels:"
           << m_dpc.dead_pixels << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();
        std::string json = base_json.str();
        json.pop_back();
        json += ",\n  \"dpc_specific\": {\n";
        json += "    \"corrected_pixels\": " + std::to_string(m_dpc.corrected_pixels) + ",\n";
        json += "    \"hot_pixels\": " + std::to_string(m_dpc.hot_pixels) + ",\n";
        json += "    \"dead_pixels\": " + std::to_string(m_dpc.dead_pixels) + "\n";
        json += "  }\n}";
        return json;
    }

private:
    DpcMetrics m_dpc;
};

#endif // DPC_METRICS_H
