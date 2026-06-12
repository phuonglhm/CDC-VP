#pragma once

#include <cstdint>
#include <iostream>
#include <string>

namespace cdc::components::common {

// A half-open memory-mapped region [base, base + size).
struct address_range {
    std::uint64_t base = 0;
    std::uint64_t size = 0;

    bool contains(std::uint64_t addr) const
    {
        return addr >= base && addr < base + size;
    }
};

// Minimal logging helper. Routes to std::cerr so it does not interfere with
// peripheral output (e.g. UART) on std::cout.
inline void log(const std::string& who, const std::string& msg)
{
    std::cerr << "[" << who << "] " << msg << '\n';
}

} // namespace cdc::components::common
