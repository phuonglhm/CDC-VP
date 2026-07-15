#include "model/tlm_pipeline.hpp"

#include "hevc/yuv420.hpp"

#include <stdexcept>

namespace model {

RawYuvDma::RawYuvDma(std::string path, unsigned width, unsigned height)
    : input_(std::move(path), std::ios::binary), width_(width), height_(height) {
    if (!input_) {
        throw std::runtime_error("cannot open input YUV file");
    }
}

bool RawYuvDma::transport(std::uint64_t frame_index, FrameTransaction& transaction,
                          TimingStats& timing) {
    auto frame = std::make_shared<hevc::Yuv420Frame>();
    if (!hevc::read_yuv420_frame(input_, width_, height_, *frame)) {
        return false;
    }
    transaction = {frame_index, std::move(frame)};
    constexpr std::uint64_t kDmaBytesPerCycle = 16;
    timing.dma_cycles +=
        (hevc::yuv420_frame_bytes(width_, height_) + kDmaBytesPerCycle - 1) /
        kDmaBytesPerCycle;
    return true;
}

std::vector<std::uint8_t> HevcVpuTarget::headers() const {
    return encoder_.parameter_sets_annex_b();
}

std::vector<std::uint8_t>
HevcVpuTarget::b_transport(const FrameTransaction& transaction, TimingStats& timing) const {
    if (!transaction.frame) {
        throw std::invalid_argument("null frame transaction");
    }
    constexpr std::uint64_t kPcmCyclesPerLumaSample = 2;
    timing.encoder_cycles += static_cast<std::uint64_t>(transaction.frame->width) *
                             transaction.frame->height * kPcmCyclesPerLumaSample;
    return encoder_.encode_idr_frame_annex_b(*transaction.frame, transaction.frame_index);
}

hevc::Yuv420Frame
HevcVpuTarget::reconstruct(const FrameTransaction& transaction) const {
    if (!transaction.frame) {
        throw std::invalid_argument("null frame transaction");
    }
    return encoder_.reconstruct_idr_frame(*transaction.frame);
}

AnnexBSink::AnnexBSink(const std::string& path) : output_(path, std::ios::binary) {
    if (!output_) {
        throw std::runtime_error("cannot open output bitstream file");
    }
}

void AnnexBSink::b_transport(const std::vector<std::uint8_t>& payload,
                             TimingStats& timing) {
    output_.write(reinterpret_cast<const char*>(payload.data()),
                  static_cast<std::streamsize>(payload.size()));
    if (!output_) {
        throw std::runtime_error("failed while writing output bitstream");
    }
    constexpr std::uint64_t kSinkBytesPerCycle = 8;
    timing.sink_cycles += (payload.size() + kSinkBytesPerCycle - 1) / kSinkBytesPerCycle;
}

} // namespace model
