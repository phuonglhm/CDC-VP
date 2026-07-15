#pragma once

#include "hevc/hevc_pcm_encoder.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace model {

struct FifoCycleConfig {
    std::uint32_t input_depth = 4;
    std::uint32_t residual_depth = 4;
    std::uint32_t coefficient_depth = 4;
    std::uint32_t output_depth = 4;
    std::uint32_t dma_read_bytes_per_cycle = 16;
    std::uint32_t dma_write_bytes_per_cycle = 8;
    std::uint32_t prediction_samples_per_cycle = 16;
    std::uint32_t transform_samples_per_cycle = 8;
    std::uint32_t cabac_bytes_per_cycle = 2;
};

struct FifoFrameSpec {
    unsigned width = 0;
    unsigned height = 0;
    hevc::CodingMode mode = hevc::CodingMode::Pcm;
    std::size_t prefix_bytes = 0;
    std::vector<std::size_t> ctu_bitstream_bytes;
};

struct FifoCycleStats {
    std::uint64_t cycles = 0;
    std::uint64_t input_bytes = 0;
    std::uint64_t output_bytes = 0;
    std::uint64_t dma_read_active = 0;
    std::uint64_t prediction_active = 0;
    std::uint64_t transform_active = 0;
    std::uint64_t cabac_active = 0;
    std::uint64_t dma_write_active = 0;
    std::uint64_t stall_input_full = 0;
    std::uint64_t stall_prediction_full = 0;
    std::uint64_t stall_transform_full = 0;
    std::uint64_t stall_cabac_full = 0;
    std::uint32_t max_input_occupancy = 0;
    std::uint32_t max_residual_occupancy = 0;
    std::uint32_t max_coefficient_occupancy = 0;
    std::uint32_t max_output_occupancy = 0;
};

class FifoCycleModel {
public:
    explicit FifoCycleModel(FifoCycleConfig config = {});
    ~FifoCycleModel();

    FifoCycleModel(FifoCycleModel&&) noexcept;
    FifoCycleModel& operator=(FifoCycleModel&&) noexcept;
    FifoCycleModel(const FifoCycleModel&) = delete;
    FifoCycleModel& operator=(const FifoCycleModel&) = delete;

    void start(const FifoFrameSpec& frame);
    bool tick();

    [[nodiscard]] bool running() const;
    [[nodiscard]] bool done() const;
    [[nodiscard]] const FifoCycleStats& stats() const;
    [[nodiscard]] const FifoCycleConfig& config() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

void accumulate_fifo_stats(FifoCycleStats& total,
                           const FifoCycleStats& frame);

} // namespace model
