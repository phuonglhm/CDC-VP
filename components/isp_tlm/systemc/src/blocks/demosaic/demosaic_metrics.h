/*
 * DEMOSAIC (Demosaicing/CFA Interpolation) Metrics
 * Specialized metrics collection for DEMOSAIC block
 */

#ifndef DEMOSAIC_METRICS_H
#define DEMOSAIC_METRICS_H

#include "metrics/isp_metrics_base.h"

//=============================================================================
// DEMOSAIC-specific Metrics
//=============================================================================
struct DemosaicMetrics {
    uint64_t r_pixels = 0;
    uint64_t g_pixels = 0;
    uint64_t b_pixels = 0;
    uint64_t edge_pixels = 0;
    uint64_t bilinear_interpolations = 0;
    uint64_t window_operations = 0;

    static constexpr unsigned LUT4_EST = 800;
    static constexpr unsigned FF_COUNT = 600;
    static constexpr unsigned DSP_MULTS = 2;
    static constexpr unsigned BRAM_KBITS = 32;

    void reset() {
        r_pixels = 0;
        g_pixels = 0;
        b_pixels = 0;
        edge_pixels = 0;
        bilinear_interpolations = 0;
        window_operations = 0;
    }
};

//=============================================================================
// DEMOSAIC Metrics Collector
//=============================================================================
class DemosaicMetricsCollector : public IspMetricsBase {
public:
    DemosaicMetricsCollector()
        : IspMetricsBase("DEMOSAIC")
    {
        m_metrics.lut4_estimate = DemosaicMetrics::LUT4_EST;
        m_metrics.ff_count = DemosaicMetrics::FF_COUNT;
        m_metrics.dsp_mults = DemosaicMetrics::DSP_MULTS;
        m_metrics.bram_kbits = DemosaicMetrics::BRAM_KBITS;
    }

    void record_r_pixel() { m_dm.r_pixels++; }
    void record_g_pixel() { m_dm.g_pixels++; }
    void record_b_pixel() { m_dm.b_pixels++; }
    void record_edge_pixel() { m_dm.edge_pixels++; }
    void record_bilinear() { m_dm.bilinear_interpolations++; }
    void record_window_op() { m_dm.window_operations++; }

    void reset() override {
        IspMetricsBase::reset();
        m_dm.reset();
        m_metrics.lut4_estimate = DemosaicMetrics::LUT4_EST;
        m_metrics.ff_count = DemosaicMetrics::FF_COUNT;
        m_metrics.dsp_mults = DemosaicMetrics::DSP_MULTS;
        m_metrics.bram_kbits = DemosaicMetrics::BRAM_KBITS;
    }

    const DemosaicMetrics& get_demosaic_metrics() const { return m_dm; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);

        os << "\n  [DEMOSAIC-Specific]" << "\n";
        os << std::setw(30) << std::left << "  R Pixels:"
           << m_dm.r_pixels << "\n";
        os << std::setw(30) << std::left << "  G Pixels:"
           << m_dm.g_pixels << "\n";
        os << std::setw(30) << std::left << "  B Pixels:"
           << m_dm.b_pixels << "\n";
        os << std::setw(30) << std::left << "  Edge Pixels:"
           << m_dm.edge_pixels << "\n";
        os << std::setw(30) << std::left << "  Bilinear Interpolations:"
           << m_dm.bilinear_interpolations << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();

        std::string json = base_json.str();
        json.pop_back();

        json += ",\n";
        json += "  \"demosaic_specific\": {\n";
        json += "    \"r_pixels\": " + std::to_string(m_dm.r_pixels) + ",\n";
        json += "    \"g_pixels\": " + std::to_string(m_dm.g_pixels) + ",\n";
        json += "    \"b_pixels\": " + std::to_string(m_dm.b_pixels) + ",\n";
        json += "    \"edge_pixels\": " + std::to_string(m_dm.edge_pixels) + ",\n";
        json += "    \"bilinear_interpolations\": " + std::to_string(m_dm.bilinear_interpolations) + "\n";
        json += "  }\n";
        json += "}";

        return json;
    }

private:
    DemosaicMetrics m_dm;
};

#endif // DEMOSAIC_METRICS_H
