#include "rec_mem_target.h"
#include <algorithm>

RecMemory::RecMemory(sc_core::sc_module_name name, uint32_t width, uint32_t height, bool use_dummy, uint8_t dummy_value)
    : sc_core::sc_module(name), width_(width), height_(height), use_dummy_(use_dummy), dummy_value_(dummy_value)
{
    buf_y_.resize(static_cast<size_t>(width_) * height_);
    buf_u_.resize(static_cast<size_t>(width_) * height_);
    buf_v_.resize(static_cast<size_t>(width_) * height_);

    if (!use_dummy_) {
        // Fill simple patterns so returned blocks are deterministic
        for (uint32_t y = 0; y < height_; ++y) {
            for (uint32_t x = 0; x < width_; ++x) {
                buf_y_[y * width_ + x] = static_cast<uint8_t>((x + y) & 0xFF);
                buf_u_[y * width_ + x] = static_cast<uint8_t>((x * 3 + y * 7) & 0xFF);
                buf_v_[y * width_ + x] = static_cast<uint8_t>((x * 11 + y * 5) & 0xFF);
            }
        }
    }
}

void RecMemory::setUseDummy(bool use_dummy, uint8_t dummy_value) {
    use_dummy_ = use_dummy;
    dummy_value_ = dummy_value;
    if (use_dummy_) {
        // fill entire buffers with dummy
        std::fill(buf_y_.begin(), buf_y_.end(), dummy_value_);
        std::fill(buf_u_.begin(), buf_u_.end(), dummy_value_);
        std::fill(buf_v_.begin(), buf_v_.end(), dummy_value_);
    } else {
        // populate deterministic patterns (same as constructor non-dummy branch)
        for (uint32_t y = 0; y < height_; ++y) {
            for (uint32_t x = 0; x < width_; ++x) {
                buf_y_[y * width_ + x] = static_cast<uint8_t>((x + y) & 0xFF);
                buf_u_[y * width_ + x] = static_cast<uint8_t>((x * 3 + y * 7) & 0xFF);
                buf_v_[y * width_ + x] = static_cast<uint8_t>((x * 11 + y * 5) & 0xFF);
            }
        }
    }
}

static inline uint8_t read_plane_sample(const std::vector<uint8_t> &buf, uint32_t w, uint32_t h, int32_t sx, int32_t sy, PaddingMode pad) {
    if (sx >= 0 && sy >= 0 && static_cast<uint32_t>(sx) < w && static_cast<uint32_t>(sy) < h) {
        return buf[static_cast<size_t>(sy) * w + sx];
    }
    switch (pad) {
        case PaddingMode::ZERO:
            return 0;
        case PaddingMode::EDGE: {
            int32_t cx = std::min(std::max(sx, 0), static_cast<int32_t>(w - 1));
            int32_t cy = std::min(std::max(sy, 0), static_cast<int32_t>(h - 1));
            return buf[static_cast<size_t>(cy) * w + cx];
        }
        case PaddingMode::PADFIL:
        default: {
            int32_t cx = std::min(std::max(sx, 0), static_cast<int32_t>(w - 1));
            int32_t cy = std::min(std::max(sy, 0), static_cast<int32_t>(h - 1));
            return buf[static_cast<size_t>(cy) * w + cx];
        }
    }
}

bool RecMemory::getRefBlock(RecPlane plane,
                            uint32_t x,
                            uint32_t y,
                            uint8_t size4x4,
                            PaddingMode pad,
                            RefBlock &out)
{
    const uint32_t edge = recSizeToPixels(size4x4);
    // return an (N+2)x(N+2) window anchored at (x,y) so callers can
    // access top/left plus the top-right and bottom-left extras.
    const uint32_t ext = edge + 2;
    out.width = ext;
    out.height = ext;
    out.stride = ext;
    out.data.assign(static_cast<size_t>(ext) * ext, 0);

    // Interpret x,y as pixel coordinates for simplicity (caller may pass block coords)
    int32_t base_x = static_cast<int32_t>(x);
    int32_t base_y = static_cast<int32_t>(y);

    const std::vector<uint8_t> *plane_buf = &buf_y_;
    uint32_t pw = width_, ph = height_;
    if (plane == RecPlane::U) { plane_buf = &buf_u_; }
    else if (plane == RecPlane::V) { plane_buf = &buf_v_; }

    if (use_dummy_) {
        // Fill entire returned window with the dummy value
        std::fill(out.data.begin(), out.data.end(), dummy_value_);
        return true;
    }

    for (uint32_t r = 0; r < ext; ++r) {
        for (uint32_t c = 0; c < ext; ++c) {
            int32_t sx = base_x + static_cast<int32_t>(c);
            int32_t sy = base_y + static_cast<int32_t>(r);
            out.data[static_cast<size_t>(r) * ext + c] = read_plane_sample(*plane_buf, pw, ph, sx, sy, pad);
        }
    }
    return true;
}
