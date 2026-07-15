#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <vector>

namespace hevc {

struct Plane {
    unsigned width = 0;
    unsigned height = 0;
    std::vector<std::uint8_t> samples;

    [[nodiscard]] std::uint8_t at(unsigned x, unsigned y) const {
        return samples.at(static_cast<std::size_t>(y) * width + x);
    }
};

struct Yuv420Frame {
    unsigned width = 0;
    unsigned height = 0;
    Plane y;
    Plane cb;
    Plane cr;
};

[[nodiscard]] std::size_t yuv420_frame_bytes(unsigned width, unsigned height);
bool read_yuv420_frame(std::istream& input, unsigned width, unsigned height,
                       Yuv420Frame& frame);
bool write_yuv420_frame(std::ostream& output, const Yuv420Frame& frame);
Yuv420Frame pad_yuv420_edge(const Yuv420Frame& source, unsigned padded_width,
                            unsigned padded_height);

} // namespace hevc
