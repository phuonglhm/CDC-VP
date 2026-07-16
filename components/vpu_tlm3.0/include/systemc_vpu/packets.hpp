#pragma once

#include "hevc/hevc_pcm_encoder.hpp"
#include "model/vpu_mmio.hpp"

#include <cstdint>
#include <memory>
#include <ostream>
#include <vector>

namespace model::systemc_native {

struct JobConfig {
    std::uint64_t job_id = 0;
    std::uint64_t src_address = 0;
    std::uint64_t dst_address = 0;
    std::uint32_t dst_capacity = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride_y = 0;
    std::uint32_t frame_count = 0;
    std::uint32_t qp = 26;
    std::uint32_t input_format = vpu_reg::FORMAT_YUV420P8;
    std::uint32_t encoder_mode = vpu_reg::MODE_PCM;
    std::uint32_t fifo_depth = 4;
};

struct FramePacket {
    JobConfig job;
    std::uint32_t frame_index = 0;
    bool last = false;
    VpuError error = VpuError::None;
    std::shared_ptr<hevc::Yuv420Frame> frame;
};

struct BitstreamPacket {
    JobConfig job;
    std::uint32_t frame_index = 0;
    bool last = false;
    VpuError error = VpuError::None;
    std::vector<std::uint8_t> bytes;
};

struct CompletionPacket {
    std::uint64_t job_id = 0;
    VpuError error = VpuError::None;
    std::uint32_t frames_done = 0;
    std::uint32_t bitstream_bytes = 0;
};

struct NativePipelineStats {
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

    // Counts below are collected by the native SystemC/TLM data path.  The
    // timestamps are absolute simulation clock indices; the report converts
    // them to job-relative fill/drain latency.
    std::uint64_t dma_read_bytes = 0;
    std::uint64_t dma_write_bytes = 0;
    std::uint64_t dma_read_bursts = 0;
    std::uint64_t dma_write_bursts = 0;
    std::uint64_t dma_read_wait_cycles = 0;
    std::uint64_t dma_write_wait_cycles = 0;
    std::uint64_t frames_input = 0;
    std::uint64_t frames_prediction = 0;
    std::uint64_t frames_transform = 0;
    std::uint64_t frames_cabac = 0;
    std::uint64_t frames_output = 0;
    std::uint64_t job_start_cycle = 0;
    std::uint64_t first_input_cycle = 0;
    std::uint64_t first_prediction_cycle = 0;
    std::uint64_t first_transform_cycle = 0;
    std::uint64_t first_cabac_cycle = 0;
    std::uint64_t first_output_cycle = 0;
    std::uint64_t last_input_cycle = 0;
    std::uint64_t last_output_cycle = 0;
};

inline std::ostream& operator<<(std::ostream& stream, const JobConfig& value) {
    return stream << "job=" << value.job_id << ", frames=" << value.frame_count;
}

inline std::ostream& operator<<(std::ostream& stream, const FramePacket& value) {
    return stream << "job=" << value.job.job_id << ", frame=" << value.frame_index;
}

inline std::ostream& operator<<(std::ostream& stream,
                                const BitstreamPacket& value) {
    return stream << "job=" << value.job.job_id << ", frame=" << value.frame_index
                  << ", bytes=" << value.bytes.size();
}

inline std::ostream& operator<<(std::ostream& stream,
                                const CompletionPacket& value) {
    return stream << "job=" << value.job_id << ", frames=" << value.frames_done
                  << ", bytes=" << value.bitstream_bytes;
}

} // namespace model::systemc_native
