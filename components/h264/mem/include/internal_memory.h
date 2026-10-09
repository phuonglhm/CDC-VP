#pragma once

#include <vector>
#include <cstdint>

namespace h264::mem {
class InternalMemory {
public:
    InternalMemory(std::size_t size = 64 * 1024); // Default: 64KB SRAM
    
    bool read(std::uint32_t offset, std::uint8_t* data, std::size_t len) const;
    bool write(std::uint32_t offset, const std::uint8_t* data, std::size_t len);

private:
    std::vector<std::uint8_t> sram_;
};
} // namespace h264::mem