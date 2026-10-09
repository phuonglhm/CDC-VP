#include "external_memory.h"

#include <cstring>

namespace h264::mem {
ExternalMemory::ExternalMemory(std::size_t size) : dram_(size, 0) {}

bool ExternalMemory::read(std::uint32_t offset, std::uint8_t* data, std::size_t len) const {
    if (offset + len > dram_.size()) 
        return false;

    std::memcpy(data, dram_.data() + offset, len);
    return true;
}

bool ExternalMemory::write(std::uint32_t offset, const std::uint8_t* data, std::size_t len) {
    if (offset + len > dram_.size()) 
        return false;
        
    std::memcpy(dram_.data() + offset, data, len);
    return true;
}
} // namespace h264::mem