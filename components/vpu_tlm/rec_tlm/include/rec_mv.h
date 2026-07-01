#ifndef REC_MV_H
#define REC_MV_H

#include <cstdint>
#include <vector>
#include <mutex>

// Simple representation of a motion vector + optional MVP index bit.
struct MotionVector {
    int32_t x{0};
    int32_t y{0};
    bool mvp_idx{false};
    MotionVector() = default;
    MotionVector(int32_t _x, int32_t _y, bool _m = false) : x(_x), y(_y), mvp_idx(_m) {}
};

class RecMvIf {
public:
    virtual ~RecMvIf() = default;

    // Read/write raw packed word (opaque representation chosen by caller).
    virtual bool readRaw(uint32_t addr, uint64_t &raw) = 0;
    virtual bool writeRaw(uint32_t addr, uint64_t raw) = 0;
    virtual size_t size() const = 0;

    // Convenience typed accessors that pack/unpack using `compBits` per
    // component (default 16). Layout: [mvp_idx][mv_y][mv_x].
    virtual bool readMV(uint32_t addr, MotionVector &mv, unsigned compBits = 16) {
        uint64_t raw;
        if (!readRaw(addr, raw)) return false;
        mv = unpackMV(raw, compBits);
        return true;
    }

    virtual bool writeMV(uint32_t addr, const MotionVector &mv, unsigned compBits = 16) {
        return writeRaw(addr, packMV(mv, compBits));
    }

    // Pack/unpack helpers
    static inline uint64_t packMV(const MotionVector &mv, unsigned compBits = 16) {
        const uint64_t mask = (compBits >= 64) ? ~0ull : ((1ull << compBits) - 1ull);
        uint64_t ux = static_cast<uint32_t>(mv.x) & mask;
        uint64_t uy = static_cast<uint32_t>(mv.y) & mask;
        uint64_t raw = ux | (uy << compBits);
        raw |= (static_cast<uint64_t>(mv.mvp_idx ? 1ull : 0ull) << (2 * compBits));
        return raw;
    }

    static inline MotionVector unpackMV(uint64_t raw, unsigned compBits = 16) {
        const uint64_t mask = (compBits >= 64) ? ~0ull : ((1ull << compBits) - 1ull);
        uint32_t ux = static_cast<uint32_t>(raw & mask);
        uint32_t uy = static_cast<uint32_t>((raw >> compBits) & mask);
        bool mvp = ((raw >> (2 * compBits)) & 1ull) != 0;

        auto sign_extend = [compBits](uint32_t v) -> int32_t {
            if (compBits == 0 || compBits >= 32) return static_cast<int32_t>(v);
            uint32_t sign = 1u << (compBits - 1);
            if (v & sign) {
                uint32_t ext = (~0u) << compBits;
                return static_cast<int32_t>(v | ext);
            }
            return static_cast<int32_t>(v);
        };

        MotionVector mv;
        mv.x = sign_extend(ux);
        mv.y = sign_extend(uy);
        mv.mvp_idx = mvp;
        return mv;
    }
};

// Simple array-backed MV memory suitable for simulation and unit tests.
class SimpleRecMvMem : public RecMvIf {
public:
    explicit SimpleRecMvMem(size_t entries = 1024) : mem(entries, 0) {}

    bool readRaw(uint32_t addr, uint64_t &raw) override {
        std::lock_guard<std::mutex> g(mu);
        if (addr >= mem.size()) return false;
        raw = mem[addr];
        return true;
    }

    bool writeRaw(uint32_t addr, uint64_t raw) override {
        std::lock_guard<std::mutex> g(mu);
        if (addr >= mem.size()) mem.resize(addr + 1, 0);
        mem[addr] = raw;
        return true;
    }

    size_t size() const override { return mem.size(); }

private:
    std::vector<uint64_t> mem;
    mutable std::mutex mu;
};

#endif
