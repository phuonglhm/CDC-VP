#ifndef MEMORY_IF_H
#define MEMORY_IF_H

#include <systemc>
#include <cstdint>
#include <cstddef>
#include <vector>

// Minimal memory interface types for Rec modules.
enum class RecPlane : uint8_t {
    Y = 0,
    U = 1,
    V = 2
};

enum class PaddingMode : uint8_t {
    NONE = 0, // no padding; caller expects exact pixels
    EDGE     , // edge-repeat padding
    ZERO     , // zero padding
    PADFIL     // apply RTL-style padding/filtering if applicable
};

struct RefBlock {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t stride{0};
    std::vector<uint8_t> data;
    uint64_t version{0};
    RefBlock() = default;
    size_t bytes() const { return data.size(); }
};

inline uint32_t recSizeToPixels(uint8_t size4x4) {
    switch (size4x4) {
        case 0: return 4;
        case 1: return 8;
        case 2: return 16;
        case 3: return 32;
        default: return 4;
    }
}

class MemoryIf {
public:
    virtual ~MemoryIf() {}

    virtual bool getRefBlock(RecPlane plane,
                             uint32_t x,
                             uint32_t y,
                             uint8_t size4x4,
                             PaddingMode pad,
                             RefBlock &out) = 0;

    virtual void pushRefBlock(RecPlane plane,
                              uint32_t x,
                              uint32_t y,
                              uint8_t size4x4,
                              const RefBlock &block,
                              uint64_t version) { (void)plane; (void)x; (void)y; (void)size4x4; (void)block; (void)version; }

    virtual uint64_t regionVersion(RecPlane plane,
                                   uint32_t x,
                                   uint32_t y,
                                   uint8_t size4x4) const { (void)plane; (void)x; (void)y; (void)size4x4; return 0; }
};

#endif
