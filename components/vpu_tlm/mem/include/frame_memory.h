#ifndef FRAME_MEMORY_H
#define FRAME_MEMORY_H

#include <systemc>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <map>
#include <mutex>
#include <tuple>
#include "memory_if.h"
#include "frame.h"


class FrameMemory : public sc_core::sc_module, public MemoryIf {
public:
    FrameMemory(sc_core::sc_module_name name)
        : sc_core::sc_module(name), active_frame_id_(0), have_active_(false) {};
    void load_frame(uint8_t frame_id, const cdc::components::frame &f);
    bool set_active_frame(uint8_t frame_id);
    bool get_active_frame(cdc::components::frame &out) const;
    // Read a contiguous row of pixels starting at (x,y). For chroma planes
    // coordinates are in luma-pixel space (mapping to chroma via /2).
    bool read_row(RecPlane plane, uint32_t x, uint32_t y, uint8_t *dst, size_t len) const;

    // Write a contiguous row of pixels starting at (x,y). This is used by
    // fetch wrappers that write per-row data (e.g., WRITE_4x4). `version`
    // if non-zero will update the region version.
    void write_row(RecPlane plane, uint32_t x, uint32_t y, const uint8_t *src, size_t len, uint64_t version = 0);
    bool getRefBlock(RecPlane plane,
                     uint32_t x,
                     uint32_t y,
                     uint8_t size4x4,
                     PaddingMode pad,
                     RefBlock &out) override;

    void pushRefBlock(RecPlane plane,
                      uint32_t x,
                      uint32_t y,
                      uint8_t size4x4,
                      const RefBlock &block,
                      uint64_t version) override;

    uint64_t regionVersion(RecPlane plane,
                           uint32_t x,
                           uint32_t y,
                           uint8_t size4x4) const override;

private:
    struct FrameData {
        cdc::components::frame frame;
        uint64_t version{0};
    };

    mutable std::mutex mutex_;
    std::unordered_map<uint8_t, FrameData> frames_;
    uint8_t active_frame_id_;
    bool have_active_;
    //kept for future extension
    std::map<std::tuple<uint8_t,uint32_t,uint32_t,uint8_t>, uint64_t> region_versions_;
};

#endif 
