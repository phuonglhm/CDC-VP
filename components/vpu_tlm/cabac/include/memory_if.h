#ifndef VPU_TLM_CABAC_MEMORY_IF_H
#define VPU_TLM_CABAC_MEMORY_IF_H

#include <systemc>
#include <cstdint>
#include <cstddef>
#include <vector>

// Minimal memory interface types local to the CABAC block.
enum class CabacRecPlane : uint8_t {
    Y = 0,
    U = 1,
    V = 2
};

enum class CabacPaddingMode : uint8_t {
    NONE = 0, // no padding; caller expects exact pixels
    EDGE     , // edge-repeat padding
    ZERO     , // zero padding
    PADFIL     // apply RTL-style padding/filtering if applicable
};

struct CabacRefBlock {
    uint32_t width{0};
    uint32_t height{0};
    uint32_t stride{0};
    std::vector<uint8_t> data;
    uint64_t version{0};
    CabacRefBlock() = default;
    size_t bytes() const { return data.size(); }
};

inline uint32_t cabacSizeToPixels(uint8_t size4x4) {
    switch (size4x4) {
        case 0: return 4;
        case 1: return 8;
        case 2: return 16;
        case 3: return 32;
        default: return 4;
    }
}

class CabacMemoryIf {
public:
    virtual ~CabacMemoryIf() {}

    virtual bool getRefBlock(CabacRecPlane plane,
                             uint32_t x,
                             uint32_t y,
                             uint8_t size4x4,
                             CabacPaddingMode pad,
                             CabacRefBlock &out) = 0;

    virtual void pushRefBlock(CabacRecPlane plane,
                              uint32_t x,
                              uint32_t y,
                              uint8_t size4x4,
                              const CabacRefBlock &block,
                              uint64_t version) { (void)plane; (void)x; (void)y; (void)size4x4; (void)block; (void)version; }

    virtual uint64_t regionVersion(CabacRecPlane plane,
                                   uint32_t x,
                                   uint32_t y,
                                   uint8_t size4x4) const { (void)plane; (void)x; (void)y; (void)size4x4; return 0; }
};

#endif
