#include "hevc/yuv420.hpp"

#include <algorithm>
#include <stdexcept>

namespace hevc {

std::size_t yuv420_frame_bytes(unsigned width, unsigned height) {
    if (width == 0 || height == 0 || (width & 1U) || (height & 1U)) {
        throw std::invalid_argument("YUV420p dimensions must be non-zero and even");
    }
    return static_cast<std::size_t>(width) * height * 3 / 2;
}

static bool read_plane(std::istream& input, Plane& plane) {
    input.read(reinterpret_cast<char*>(plane.samples.data()),
               static_cast<std::streamsize>(plane.samples.size()));
    return input.gcount() == static_cast<std::streamsize>(plane.samples.size());
}

bool read_yuv420_frame(std::istream& input, unsigned width, unsigned height,
                       Yuv420Frame& frame) {
    (void)yuv420_frame_bytes(width, height);
    frame.width = width;
    frame.height = height;
    frame.y = {width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height)};
    frame.cb = {width / 2, height / 2,
                std::vector<std::uint8_t>(static_cast<std::size_t>(width / 2) * (height / 2))};
    frame.cr = frame.cb;

    if (!read_plane(input, frame.y)) {
        return false;
    }
    if (!read_plane(input, frame.cb) || !read_plane(input, frame.cr)) {
        throw std::runtime_error("truncated YUV420p frame");
    }
    return true;
}

bool write_yuv420_frame(std::ostream& output, const Yuv420Frame& frame) {
    (void)yuv420_frame_bytes(frame.width, frame.height);
    const auto write_plane = [&](const Plane& plane) {
        output.write(reinterpret_cast<const char*>(plane.samples.data()),
                     static_cast<std::streamsize>(plane.samples.size()));
    };
    write_plane(frame.y);
    write_plane(frame.cb);
    write_plane(frame.cr);
    return static_cast<bool>(output);
}

static Plane pad_plane_edge(const Plane& source, unsigned width, unsigned height) {
    if (width < source.width || height < source.height) {
        throw std::invalid_argument("padded plane cannot be smaller than source");
    }
    Plane result{width, height,
                 std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height)};
    for (unsigned y = 0; y < height; ++y) {
        const unsigned sy = std::min(y, source.height - 1);
        for (unsigned x = 0; x < width; ++x) {
            const unsigned sx = std::min(x, source.width - 1);
            result.samples[static_cast<std::size_t>(y) * width + x] = source.at(sx, sy);
        }
    }
    return result;
}

Yuv420Frame pad_yuv420_edge(const Yuv420Frame& source, unsigned padded_width,
                            unsigned padded_height) {
    if ((padded_width & 1U) || (padded_height & 1U)) {
        throw std::invalid_argument("padded YUV420 dimensions must be even");
    }
    return {
        padded_width,
        padded_height,
        pad_plane_edge(source.y, padded_width, padded_height),
        pad_plane_edge(source.cb, padded_width / 2, padded_height / 2),
        pad_plane_edge(source.cr, padded_width / 2, padded_height / 2),
    };
}

} // namespace hevc
