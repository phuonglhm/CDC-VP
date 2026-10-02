// gvu_scratchpad.h -- Scratchpad SRAM of the HAS drawings (every RCE / ELEM_WISE stage reads and writes it).
// Capacity is a knob (HAS: TBD; plan default 24 KB). Out-of-range access THROWS (lesson of REVIEW_FINDINGS A3: the
// SRAM models wrap silently). Counts bytes read/written and the peak footprint so the report can answer H8 with data.
#ifndef HAS_SCRATCHPAD_H
#define HAS_SCRATCHPAD_H

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace has
{
    class HasScratchpad
    {
    public:
        explicit HasScratchpad(uint32_t bytes) : mem_(bytes, 0) {}

        uint32_t capacity() const { return static_cast<uint32_t>(mem_.size()); }

        void write(uint32_t addr, const void *src, uint32_t n)
        {
            check(addr, n, "write");
            std::memcpy(&mem_[addr], src, n);
            wr_bytes += n;
            peak = addr + n > peak ? addr + n : peak;
        }
        void read(uint32_t addr, void *dst, uint32_t n)
        {
            check(addr, n, "read");
            std::memcpy(dst, &mem_[addr], n);
            rd_bytes += n;
        }
        // Typed element access (little endian, like the SRAM models)
        template <typename T> T get(uint32_t addr) { T v; read(addr, &v, sizeof(T)); return v; }
        template <typename T> void put(uint32_t addr, T v) { write(addr, &v, sizeof(T)); }

        uint64_t rd_bytes{0}, wr_bytes{0};
        uint32_t peak{0};

    private:
        std::vector<uint8_t> mem_;
        void check(uint32_t addr, uint32_t n, const char *op) const
        {
            if (static_cast<uint64_t>(addr) + n > mem_.size())
                throw std::runtime_error(std::string("HasScratchpad ") + op + " out of range: addr " +
                                         std::to_string(addr) + " + " + std::to_string(n) + " > " +
                                         std::to_string(mem_.size()));
        }
    };
} // namespace has

#endif // HAS_SCRATCHPAD_H
