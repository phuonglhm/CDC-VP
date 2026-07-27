/*
 * CROP (Cropping) Metrics
 * Specialized metrics collection for CROP block
 */

#ifndef CROP_METRICS_H
#define CROP_METRICS_H

#include "metrics/isp_metrics_base.h"

//=============================================================================
// CROP-specific Metrics
//=============================================================================
struct CropMetrics {
    uint64_t input_pixels = 0;
    uint64_t output_pixels = 0;
    uint64_t cropped_pixels = 0;
    uint64_t active_cycles = 0;
    uint64_t stall_cycles = 0;
    uint64_t lines_cropped_top = 0;
    uint64_t lines_cropped_bottom = 0;
    uint64_t pixels_cropped_left = 0;
    uint64_t pixels_cropped_right = 0;

    static constexpr unsigned LUT4_EST = 100;
    static constexpr unsigned FF_COUNT = 200;
    static constexpr unsigned DSP_MULTS = 0;
    static constexpr unsigned BRAM_KBITS = 0;

    void reset() {
        input_pixels = 0;
        output_pixels = 0;
        cropped_pixels = 0;
        active_cycles = 0;
        stall_cycles = 0;
        lines_cropped_top = 0;
        lines_cropped_bottom = 0;
        pixels_cropped_left = 0;
        pixels_cropped_right = 0;
    }
};

//=============================================================================
// CROP Metrics Collector
//=============================================================================
class CropMetricsCollector : public IspMetricsBase {
public:
    CropMetricsCollector()
        : IspMetricsBase("CROP")
    {
        m_metrics.lut4_estimate = CropMetrics::LUT4_EST;
        m_metrics.ff_count = CropMetrics::FF_COUNT;
        m_metrics.dsp_mults = CropMetrics::DSP_MULTS;
        m_metrics.bram_kbits = CropMetrics::BRAM_KBITS;
    }

    void record_input_pixel() { m_crop.input_pixels++; }
    void record_output_pixel() { m_crop.output_pixels++; }
    void record_cropped_pixel() { m_crop.cropped_pixels++; }
    void record_active_cycle() { m_crop.active_cycles++; }
    void record_stall_cycle() { m_crop.stall_cycles++; }

    void record_crop_top() { m_crop.lines_cropped_top++; }
    void record_crop_bottom() { m_crop.lines_cropped_bottom++; }
    void record_crop_left() { m_crop.pixels_cropped_left++; }
    void record_crop_right() { m_crop.pixels_cropped_right++; }

    void reset() override {
        IspMetricsBase::reset();
        m_crop.reset();
        m_metrics.lut4_estimate = CropMetrics::LUT4_EST;
        m_metrics.ff_count = CropMetrics::FF_COUNT;
        m_metrics.dsp_mults = CropMetrics::DSP_MULTS;
        m_metrics.bram_kbits = CropMetrics::BRAM_KBITS;
    }

    const CropMetrics& get_crop_metrics() const { return m_crop; }
    unsigned get_width() const { return m_metrics.width; }
    unsigned get_height() const { return m_metrics.height; }

    void print_summary(std::ostream& os = std::cout) const override {
        IspMetricsBase::print_summary(os);

        os << "\n  [CROP-Specific]" << "\n";
        os << std::setw(30) << std::left << "  Input Pixels:"
           << m_crop.input_pixels << "\n";
        os << std::setw(30) << std::left << "  Output Pixels:"
           << m_crop.output_pixels << "\n";
        os << std::setw(30) << std::left << "  Cropped Pixels:"
           << m_crop.cropped_pixels << "\n";
        os << std::setw(30) << std::left << "  Active Cycles:"
           << m_crop.active_cycles << "\n";
        os << std::setw(30) << std::left << "  Stall Cycles:"
           << m_crop.stall_cycles << "\n";

        if (m_crop.input_pixels > 0) {
            double crop_rate = 100.0 * m_crop.cropped_pixels / m_crop.input_pixels;
            os << std::setw(30) << std::left << "  Crop Rate:"
               << std::fixed << std::setprecision(2) << crop_rate << "%\n";
        }
    }

    std::string to_json() const override {
        std::ostringstream base_json;
        base_json << IspMetricsBase::to_json();

        std::string json = base_json.str();
        json.pop_back();

        json += ",\n";
        json += "  \"crop_specific\": {\n";
        json += "    \"input_pixels\": " + std::to_string(m_crop.input_pixels) + ",\n";
        json += "    \"output_pixels\": " + std::to_string(m_crop.output_pixels) + ",\n";
        json += "    \"cropped_pixels\": " + std::to_string(m_crop.cropped_pixels) + ",\n";
        json += "    \"active_cycles\": " + std::to_string(m_crop.active_cycles) + ",\n";
        json += "    \"stall_cycles\": " + std::to_string(m_crop.stall_cycles) + "\n";
        json += "  }\n";
        json += "}";

        return json;
    }

private:
    CropMetrics m_crop;
};

#endif // CROP_METRICS_H
