#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mem_types.h"

namespace cdc::components {

class mem_instance {
public:
    explicit mem_instance(const mem_config& config);

    const mem_config& config() const;

    const std::string& name() const;

    std::uint32_t depth() const;
    std::uint32_t width_bits() const;
    std::uint32_t width_bytes() const;

    bool valid() const;

    mem_status reset();

    mem_status read(std::uint32_t addr,
                    std::vector<std::uint8_t>& data) const;

    mem_status write(std::uint32_t addr,
                     const std::vector<std::uint8_t>& data);

    mem_status write_be(std::uint32_t addr,
                        const std::vector<std::uint8_t>& data,
                        const std::vector<bool>& byte_enable);

    mem_status read_u64(std::uint32_t addr,
                        std::uint64_t& value) const;

    mem_status write_u64(std::uint32_t addr,
                         std::uint64_t value);

private:
    mem_config config_ {};
    std::uint32_t width_bytes_ = 0;

    std::vector<std::vector<std::uint8_t>> storage_;

    void mask_unused_bits(std::vector<std::uint8_t>& word) const;
    bool addr_valid(std::uint32_t addr) const;
};

} // namespace cdc::components
