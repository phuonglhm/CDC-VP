#include "mem_instance.h"

#include <algorithm>
#include <limits>

namespace cdc::components {

mem_instance::mem_instance(const mem_config& config)
    : config_(config)
{
    width_bytes_ = (config_.width_bits + 7u) / 8u;

    if (valid()) {
        storage_.resize(config_.depth);
        for (auto& word : storage_) {
            word.resize(width_bytes_, 0);
        }
    }
}

const mem_config& mem_instance::config() const
{
    return config_;
}

const std::string& mem_instance::name() const
{
    return config_.name;
}

std::uint32_t mem_instance::depth() const
{
    return config_.depth;
}

std::uint32_t mem_instance::width_bits() const
{
    return config_.width_bits;
}

std::uint32_t mem_instance::width_bytes() const
{
    return width_bytes_;
}

bool mem_instance::valid() const
{
    return !config_.name.empty() &&
           config_.depth > 0 &&
           config_.width_bits > 0 &&
           width_bytes_ > 0;
}

mem_status mem_instance::reset()
{
    if (!valid()) {
        return mem_status::invalid_config;
    }

    for (auto& word : storage_) {
        std::fill(word.begin(), word.end(), 0);
    }

    return mem_status::ok;
}

mem_status mem_instance::read(std::uint32_t addr,
                              std::vector<std::uint8_t>& data) const
{
    if (!valid()) {
        return mem_status::invalid_config;
    }

    if (!addr_valid(addr)) {
        return mem_status::out_of_range;
    }

    data = storage_[addr];
    return mem_status::ok;
}

mem_status mem_instance::write(std::uint32_t addr,
                               const std::vector<std::uint8_t>& data)
{
    if (!valid()) {
        return mem_status::invalid_config;
    }

    if (!addr_valid(addr)) {
        return mem_status::out_of_range;
    }

    if (data.size() != width_bytes_) {
        return mem_status::width_mismatch;
    }

    storage_[addr] = data;
    mask_unused_bits(storage_[addr]);

    return mem_status::ok;
}

mem_status mem_instance::write_be(std::uint32_t addr,
                                  const std::vector<std::uint8_t>& data,
                                  const std::vector<bool>& byte_enable)
{
    if (!valid()) {
        return mem_status::invalid_config;
    }

    if (!config_.byte_enable) {
        return mem_status::byte_enable_unsupported;
    }

    if (!addr_valid(addr)) {
        return mem_status::out_of_range;
    }

    if (data.size() != width_bytes_) {
        return mem_status::width_mismatch;
    }

    if (byte_enable.size() != width_bytes_) {
        return mem_status::byte_enable_size_mismatch;
    }

    for (std::size_t i = 0; i < width_bytes_; ++i) {
        if (byte_enable[i]) {
            storage_[addr][i] = data[i];
        }
    }

    mask_unused_bits(storage_[addr]);

    return mem_status::ok;
}

mem_status mem_instance::read_u64(std::uint32_t addr,
                                  std::uint64_t& value) const
{
    std::vector<std::uint8_t> data;
    const mem_status status = read(addr, data);

    if (status != mem_status::ok) {
        return status;
    }

    if (data.size() > sizeof(std::uint64_t)) {
        return mem_status::width_mismatch;
    }

    value = 0;

    for (std::size_t i = 0; i < data.size(); ++i) {
        value |= static_cast<std::uint64_t>(data[i]) << (i * 8u);
    }

    return mem_status::ok;
}

mem_status mem_instance::write_u64(std::uint32_t addr,
                                   std::uint64_t value)
{
    if (width_bytes_ > sizeof(std::uint64_t)) {
        return mem_status::width_mismatch;
    }

    std::vector<std::uint8_t> data(width_bytes_, 0);

    for (std::size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<std::uint8_t>((value >> (i * 8u)) & 0xffu);
    }

    return write(addr, data);
}

void mem_instance::mask_unused_bits(std::vector<std::uint8_t>& word) const
{
    if (word.empty()) {
        return;
    }

    const std::uint32_t used_bits_last_byte = config_.width_bits % 8u;

    if (used_bits_last_byte == 0u) {
        return;
    }

    const std::uint8_t mask =
        static_cast<std::uint8_t>((1u << used_bits_last_byte) - 1u);

    word.back() &= mask;
}

bool mem_instance::addr_valid(std::uint32_t addr) const
{
    return addr < config_.depth;
}

} // namespace cdc::components
