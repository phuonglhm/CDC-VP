#pragma once
#include <vector>
#include <cstdint>

namespace h264::mem {
class ExternalMemory {
public:
    ExternalMemory(std::size_t size = 16 * 1024 * 1024); // Default: 16MB DRAM
    
    bool read(std::uint32_t offset, std::uint8_t* data, std::size_t len) const;
    bool write(std::uint32_t offset, const std::uint8_t* data, std::size_t len);

private:
    std::vector<std::uint8_t> dram_;
};
} // namespace h264::mem