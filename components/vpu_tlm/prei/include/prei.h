#pragma once

#include <cstdint>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "prei_result.h"
#include "prediction_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// PREI: Pre-Intra estimation
//
// TLM equivalent of xk265 rtl/prei.
//
// Responsibilities:
// - Evaluate HEVC intra modes 0..34
// - Create mode candidates similar to mode_write/md_ram behavior
// - Generate modebest64 summary
// - Apply rate-control configuration
// - Output a PREI result for POSI/TOP
// -----------------------------------------------------------------------------

class prei {
public:
    prei() = default;

    prei_result run(const frame& input,
                    const block& ctu) const;

    prei_result run(const frame& input,
                    const block& ctu,
                    const prei_rate_control_config& rc_config) const;

private:
    void run_mode_decision(const frame& input,
                           const block& ctu,
                           prei_result& result) const;

    std::vector<block> build_cu_list(const block& ctu) const;

    prei_mode_entry evaluate_cu_modes(const frame& input,
                                      const block& cu,
                                      std::uint32_t qp) const;

    prei_mode_candidate evaluate_intra_mode(const frame& input,
                                            const block& cu,
                                            std::uint32_t mode_index,
                                            std::uint32_t qp) const;

    std::uint8_t predict_intra_sample(const frame& input,
                                      const block& cu,
                                      std::uint32_t local_x,
                                      std::uint32_t local_y,
                                      std::uint32_t mode_index) const;

    std::uint8_t predict_planar_sample(const frame& input,
                                       const block& cu,
                                       std::uint32_t local_x,
                                       std::uint32_t local_y) const;

    std::uint8_t predict_dc_sample(const frame& input,
                                   const block& cu) const;

    std::uint8_t predict_angular_sample(const frame& input,
                                        const block& cu,
                                        std::uint32_t local_x,
                                        std::uint32_t local_y,
                                        std::uint32_t mode_index) const;

    std::uint32_t estimate_mode_rate(std::uint32_t mode_index,
                                     const block& cu) const;

    std::uint32_t calculate_modebest64_sum(const prei_result& result) const;

    std::uint32_t run_rate_control(const block& ctu,
                                   const prei_result& md_result,
                                   const prei_rate_control_config& rc_config) const;

    bool is_inside_roi(const block& ctu,
                       const prei_rate_control_config& rc_config) const;

    std::uint32_t clamp_qp(std::uint32_t qp,
                           std::uint32_t min_qp,
                           std::uint32_t max_qp) const;

    intra_prediction_mode map_intra_mode(std::uint32_t mode_index) const;
};

} // namespace cdc::components
