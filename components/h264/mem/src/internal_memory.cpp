#include "internal_memory.h"

#include <cstring>

namespace h264::mem {
InternalMemory::InternalMemory(std::size_t size) : sram_(size, 0) {}

bool InternalMemory::read(std::uint32_t offset, std::uint8_t* data, std::size_t len) const {
    if (offset + len > sram_.size()) 
        return false;

    std::memcpy(data, sram_.data() + offset, len);
    return true;
}

bool InternalMemory::write(std::uint32_t offset, const std::uint8_t* data, std::size_t len) {
    if (offset + len > sram_.size()) 
        return false;

    std::memcpy(sram_.data() + offset, data, len);
    return true;
}
} // namespace h264::mem