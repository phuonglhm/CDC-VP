#pragma once

#include "hevc/hevc_pcm_encoder.hpp"

#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <string>

namespace model {

struct FrameTransaction {
    std::uint64_t frame_index = 0;
    std::shared_ptr<hevc::Yuv420Frame> frame;
};

struct TimingStats {
    std::uint64_t dma_cycles = 0;
    std::uint64_t encoder_cycles = 0;
    std::uint64_t sink_cycles = 0;
};

class RawYuvDma {
public:
    RawYuvDma(std::string path, unsigned width, unsigned height);
    bool transport(std::uint64_t frame_index, FrameTransaction& transaction,
                   TimingStats& timing);

private:
    std::ifstream input_;
    unsigned width_;
    unsigned height_;
};

class HevcVpuTarget {
public:
    explicit HevcVpuTarget(hevc::EncoderConfig config) : encoder_(config) {}

    [[nodiscard]] std::vector<std::uint8_t> headers() const;
    [[nodiscard]] std::vector<std::uint8_t>
    b_transport(const FrameTransaction& transaction, TimingStats& timing) const;
    [[nodiscard]] hevc::Yuv420Frame
    reconstruct(const FrameTransaction& transaction) const;

private:
    hevc::HevcPcmEncoder encoder_;
};

class AnnexBSink {
public:
    explicit AnnexBSink(const std::string& path);
    void b_transport(const std::vector<std::uint8_t>& payload, TimingStats& timing);

private:
    std::ofstream output_;
};

} // namespace model
