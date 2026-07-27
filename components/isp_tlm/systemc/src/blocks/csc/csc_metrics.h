/*
 * CSC (Color Space Conversion) Metrics
 * Specialized metrics collection for CSC block
 */

#ifndef CSC_METRICS_H
#define CSC_METRICS_H

#include "metrics/isp_metrics_base.h"

//=============================================================================
// CSC-specific Metrics
//=============================================================================
struct CscMetrics {
    uint64_t rgb_to_yuv_conversions = 0;
    uint64_t y_pixels = 0;
    uint64_t u_pixels = 0;
    uint64_t v_pixels = 0;
    uint64_t bt601_conversions = 0;
    uint64_t bt709_conversions = 0;

    static constexpr unsigned LUT4_EST = 300;
    static constexpr unsigned FF_COUNT = 350;
    static constexpr unsigned DSP_MULTS = 3;
    static constexpr unsigned BRAM_KBITS = 0;

    void reset() {
        rgb_to_yuv_conversions = 0;
        y_pixels = 0;
        u_pixels = 0;
        v_pixels = 0;
        bt601_conversions = 0;
        bt709_conversions = 0;
    }
};

//=============================================================================
// CSC Metrics Collector
//=============================================================================
class CscMetricsCollector : public IspMetricsBase {
public:
    CscMetricsCollector()
        : IspMetricsBase("CSC")
    {
        m_metrics.lut4_estimate = CscMetrics::LUT4_EST;
        m_metrics.ff_count = CscMetrics::FF_COUNT;
        m_metrics.dsp_mults = CscMetrics::DSP_MULTS;
        m_metrics.bram_kbits = CscMetrics::BRAM_KBITS;
    }

    void record_rgb_to_yuv() { m_csc.rgb_to_yuv_conversions++; }
    void record_y_pixel() { m_csc.y_pixels++; }
    void record_u_pixel() { m_csc.u_pixels++; }
    void record_v_pixel() { m_csc.v_pixels++; }
    void record_bt601() { m_csc.bt601_conversions++; }
    void record_bt709() { m_csc.bt709_conversions++; }

    void reset() override {
        IspMetricsBase::reset();
        m_csc.reset();
        m_metrics.lut4_estimate = CscMetrics::LUT4_EST;
        m_metrics.ff_count = CscMetrics::FF_COUNT;
        m_metrics.dsp_mults = CscMetrics::DSP_MULTS;
        m_metrics.bram_kbits = CscMetrics::BRAM_KBITS;
    }

    const CscMetrics& get_csc_metrics() const { return m_csc; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);

        os << "\n  [CSC-Specific]" << "\n";
        os << std::setw(30) << std::left << "  RGB to YUV Conversions:"
           << m_csc.rgb_to_yuv_conversions << "\n";
        os << std::setw(30) << std::left << "  Y Pixels:"
           << m_csc.y_pixels << "\n";
        os << std::setw(30) << std::left << "  U Pixels:"
           << m_csc.u_pixels << "\n";
        os << std::setw(30) << std::left << "  V Pixels:"
           << m_csc.v_pixels << "\n";
        os << std::setw(30) << std::left << "  BT.601 Conversions:"
           << m_csc.bt601_conversions << "\n";
        os << std::setw(30) << std::left << "  BT.709 Conversions:"
           << m_csc.bt709_conversions << "\n";
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();

        std::string json = base_json.str();
        json.pop_back();

        json += ",\n";
        json += "  \"csc_specific\": {\n";
        json += "    \"rgb_to_yuv_conversions\": " + std::to_string(m_csc.rgb_to_yuv_conversions) + ",\n";
        json += "    \"y_pixels\": " + std::to_string(m_csc.y_pixels) + ",\n";
        json += "    \"u_pixels\": " + std::to_string(m_csc.u_pixels) + ",\n";
        json += "    \"v_pixels\": " + std::to_string(m_csc.v_pixels) + ",\n";
        json += "    \"bt601_conversions\": " + std::to_string(m_csc.bt601_conversions) + ",\n";
        json += "    \"bt709_conversions\": " + std::to_string(m_csc.bt709_conversions) + "\n";
        json += "  }\n";
        json += "}";

        return json;
    }

private:
    CscMetrics m_csc;
};

#endif // CSC_METRICS_H
