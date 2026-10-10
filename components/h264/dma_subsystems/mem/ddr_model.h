// ddr_model.h — Byte-addressed external DDR model.
//
// Spec: §15.1 — the AXI DDR responder in the primary verification flow is a
// BYTE-ADDRESSED MEMORY MODEL. Keeping that shape here means VP read logs and
// golden comparisons line up with the cocotb testbench described in §15.
//
// §12.2 regions modeled: CMB working set, REFM (3 slots), NAL activation
// buffer. No fixed physical addresses come from the PDF — bases are assigned
// by the harness.
#ifndef H264_DDR_MODEL_H
#define H264_DDR_MODEL_H

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "dma_if.h"
#include "h264_spec.h"

namespace h264 {

class DdrModel : public MemoryIf {
public:
    explicit DdrModel(size_t bytes = 64u * 1024u * 1024u)
        : mem_(bytes, 0u) {}

    void write(uint64_t addr, const uint8_t* data, size_t len) override {
        check(addr, len);
        std::memcpy(mem_.data() + addr, data, len);
    }

    void read(uint64_t addr, uint8_t* out, size_t len) const override {
        check(addr, len);
        std::memcpy(out, mem_.data() + addr, len);
    }

    uint8_t read_byte(uint64_t addr) const override {
        check(addr, 1);
        return mem_[addr];
    }

    // Deterministically stage a source frame so read addresses can be checked
    // against a known pattern (§15.3 HAS-V-CMB-01, HAS-V-QCIF-01).
    void fill_pattern(uint64_t base, size_t len, uint8_t seed = 0) {
        check(base, len);
        for (size_t i = 0; i < len; ++i) {
            mem_[base + i] = static_cast<uint8_t>((seed + i) & 0xFFu);
        }
    }

    size_t size() const { return mem_.size(); }

    // Zero the whole array. §12.6: a full clear on reset is not required when
    // the controller invalidates tags, but the model offers it for tests.
    void clear() { std::fill(mem_.begin(), mem_.end(), 0u); }

private:
    void check(uint64_t addr, size_t len) const {
        if (addr > mem_.size() || len > mem_.size() - static_cast<size_t>(addr)) {
            throw std::out_of_range("ddr: access outside modeled memory");
        }
        // §5.5: baseline DMA physical address must be representable in 32 bits.
        const uint64_t limit = static_cast<uint64_t>(kMaxPhysAddr32) + 1ull;
        if (addr > limit || len > limit - addr) {
            throw std::out_of_range("ddr: address exceeds 4 GiB baseline");
        }
    }

    std::vector<uint8_t> mem_;
};

}  // namespace h264

#endif  // H264_DDR_MODEL_H
