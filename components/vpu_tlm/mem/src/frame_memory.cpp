#include "frame_memory.h"
// Install or replace a frame under given id and make it active.
void FrameMemory::load_frame(uint8_t frame_id, const cdc::components::frame &f) {
    std::lock_guard<std::mutex> lk(mutex_);
    FrameData &fd = frames_[frame_id];
    fd.frame = f;
    fd.version++;
    active_frame_id_ = frame_id;
    have_active_ = true;
}

// Set active frame without modifying contents (no-op if id unknown)
bool FrameMemory::set_active_frame(uint8_t frame_id) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (frames_.count(frame_id) == 0) return false;
    active_frame_id_ = frame_id;
    have_active_ = true;
    return true;
}

// Retrieve an (N+2)x(N+2) window anchored at (x,y). Returns false if
// no active frame exists.
bool FrameMemory::getRefBlock(RecPlane plane,
                        uint32_t x,
                        uint32_t y,
                        uint8_t size4x4,
                        PaddingMode pad,
                        RefBlock &out)
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (!have_active_) return false;
    auto it = frames_.find(active_frame_id_);
    if (it == frames_.end()) return false;

    const cdc::components::frame &frm = it->second.frame;
    const uint32_t N = recSizeToPixels(size4x4);
    const uint32_t ext = N + 2u;

    out.width = ext;
    out.height = ext;
    out.stride = ext;
    out.data.assign(static_cast<size_t>(ext) * ext, 0);
    out.version = it->second.version;

    // Helper to read a single sample from the stored frame with padding
    auto read_sample = [&](int64_t sx, int64_t sy) -> uint8_t {
        // Luma plane
        if (plane == RecPlane::Y) {
            // Padding handling
            if (pad == PaddingMode::ZERO) {
                if (sx < 0 || sy < 0 || static_cast<uint64_t>(sx) >= frm.width || static_cast<uint64_t>(sy) >= frm.height) return 0;
                return frm.get_luma(static_cast<uint32_t>(sx), static_cast<uint32_t>(sy));
            }

            // EDGE or PADFIL or NONE: treat as EDGE for out-of-bounds
            int64_t cx = sx;
            int64_t cy = sy;
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            if (static_cast<uint64_t>(cx) >= frm.width) cx = static_cast<int64_t>(frm.width) - 1;
            if (static_cast<uint64_t>(cy) >= frm.height) cy = static_cast<int64_t>(frm.height) - 1;
            if (frm.empty()) return 128;
            return frm.get_luma(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy));
        }

        // Chroma planes (assume 4:2:0) — map luma coords to chroma by /2
        int64_t cx = sx / 2;
        int64_t cy = sy / 2;
        if (pad == PaddingMode::ZERO) {
            if (cx < 0 || cy < 0 || static_cast<uint64_t>(cx) >= frm.chroma_width() || static_cast<uint64_t>(cy) >= frm.chroma_height()) return 0;
        } else {
            if (cx < 0) cx = 0;
            if (cy < 0) cy = 0;
            if (static_cast<uint64_t>(cx) >= frm.chroma_width()) cx = static_cast<int64_t>(frm.chroma_width()) - 1;
            if (static_cast<uint64_t>(cy) >= frm.chroma_height()) cy = static_cast<int64_t>(frm.chroma_height()) - 1;
        }

        if (plane == RecPlane::U) {
            return frm.get_cb(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy));
        }
        return frm.get_cr(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy));
    };

    for (uint32_t r = 0; r < ext; ++r) {
        for (uint32_t c = 0; c < ext; ++c) {
            int64_t sx = static_cast<int64_t>(x) + static_cast<int64_t>(c);
            int64_t sy = static_cast<int64_t>(y) + static_cast<int64_t>(r);
            out.data[static_cast<size_t>(r) * ext + c] = read_sample(sx, sy);
        }
    }

    return true;
}

bool FrameMemory::read_row(RecPlane plane, uint32_t x, uint32_t y, uint8_t *dst, size_t len) const {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!have_active_) return false;
    auto it = frames_.find(active_frame_id_);
    if (it == frames_.end()) return false;

    const cdc::components::frame &frm = it->second.frame;
    if (plane == RecPlane::Y) {
        // read len luma samples starting at (x,y)
        for (size_t i = 0; i < len; ++i) {
            uint32_t sx = x + static_cast<uint32_t>(i);
            uint32_t sy = y;
            if (sx < frm.width && sy < frm.height && !frm.empty()) dst[i] = frm.get_luma(sx, sy);
            else dst[i] = 0;
        }
        return true;
    }

    // Chroma: map luma coords to chroma by /2
    for (size_t i = 0; i < len; ++i) {
        int64_t sx = static_cast<int64_t>(x) + static_cast<int64_t>(i);
        int64_t sy = static_cast<int64_t>(y);
        int64_t cx = sx / 2;
        int64_t cy = sy / 2;
        if (cx < 0 || cy < 0 || static_cast<uint64_t>(cx) >= frm.chroma_width() || static_cast<uint64_t>(cy) >= frm.chroma_height()) {
            dst[i] = 128;
        } else {
            if (plane == RecPlane::U) dst[i] = frm.get_cb(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy));
            else dst[i] = frm.get_cr(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy));
        }
    }
    return true;
}

void FrameMemory::write_row(RecPlane plane, uint32_t x, uint32_t y, const uint8_t *src, size_t len, uint64_t version) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!have_active_) return;
    auto it = frames_.find(active_frame_id_);
    if (it == frames_.end()) return;

    cdc::components::frame &frm = it->second.frame;
    if (plane == RecPlane::Y) {
        for (size_t i = 0; i < len; ++i) {
            uint32_t sx = x + static_cast<uint32_t>(i);
            uint32_t sy = y;
            if (sx < frm.width && sy < frm.height) frm.set_luma(sx, sy, src[i]);
        }
    } else {
        for (size_t i = 0; i < len; ++i) {
            int64_t sx = static_cast<int64_t>(x) + static_cast<int64_t>(i);
            int64_t sy = static_cast<int64_t>(y);
            int64_t cx = sx / 2;
            int64_t cy = sy / 2;
            if (cx < 0 || cy < 0) continue;
            if (static_cast<uint64_t>(cx) < frm.chroma_width() && static_cast<uint64_t>(cy) < frm.chroma_height()) {
                if (plane == RecPlane::U) frm.set_cb(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), src[i]);
                else frm.set_cr(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy), src[i]);
            }
        }
    }

    if (version) it->second.version = version;
    else it->second.version++;
}

// Push a RefBlock back into the active frame's storage. We interpret the
// supplied block as an (N+2)x(N+2) window anchored at (x,y) and copy only
// the central N x N pixels into the frame at (x+1, y+1).
void FrameMemory::pushRefBlock(RecPlane plane,
                            uint32_t x,
                            uint32_t y,
                            uint8_t size4x4,
                            const RefBlock &block,
                            uint64_t version)
{
    std::lock_guard<std::mutex> lk(mutex_);
    if (!have_active_) return;
    auto it = frames_.find(active_frame_id_);
    if (it == frames_.end()) return;
    cdc::components::frame &frm = it->second.frame;
    const uint32_t N = recSizeToPixels(size4x4);
    const uint32_t ext = N + 2u;
    if (block.data.size() < static_cast<size_t>(ext) * ext) return;

    // Write central N x N into frame at (x+1, y+1)
    for (uint32_t r = 0; r < N; ++r) {
        for (uint32_t c = 0; c < N; ++c) {
            int64_t sx = static_cast<int64_t>(x) + 1 + static_cast<int64_t>(c);
            int64_t sy = static_cast<int64_t>(y) + 1 + static_cast<int64_t>(r);
            if (sx < 0 || sy < 0) continue;
            if (plane == RecPlane::Y) {
                if (static_cast<uint64_t>(sx) < frm.width && static_cast<uint64_t>(sy) < frm.height) {
                    frm.set_luma(static_cast<uint32_t>(sx), static_cast<uint32_t>(sy),
                                    block.data[static_cast<size_t>(r+1) * ext + (c+1)]);
                }
            } else {
                int64_t cx = sx / 2;
                int64_t cy = sy / 2;
                if (cx < 0 || cy < 0) continue;
                if (static_cast<uint64_t>(cx) < frm.chroma_width() && static_cast<uint64_t>(cy) < frm.chroma_height()) {
                    if (plane == RecPlane::U) frm.set_cb(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy),
                                                            block.data[static_cast<size_t>(r+1) * ext + (c+1)]);
                    else frm.set_cr(static_cast<uint32_t>(cx), static_cast<uint32_t>(cy),
                                        block.data[static_cast<size_t>(r+1) * ext + (c+1)]);
                }
            }
        }
    }

    // update version if provided
    it->second.version = version ? version : (it->second.version + 1);
}

uint64_t FrameMemory::regionVersion(RecPlane /*plane*/, uint32_t /*x*/, uint32_t /*y*/, uint8_t /*size4x4*/) const {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!have_active_) return 0;
    auto it = frames_.find(active_frame_id_);
    if (it == frames_.end()) return 0;
    return it->second.version;
}

bool FrameMemory::get_active_frame(cdc::components::frame &out) const {
    std::lock_guard<std::mutex> lk(mutex_);
    if (!have_active_) return false;
    auto it = frames_.find(active_frame_id_);
    if (it == frames_.end()) return false;
    out = it->second.frame;
    return true;
}