#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace hevc {

class BitWriter {
public:
    void put_bit(bool bit);
    void put_bits(std::uint64_t value, unsigned count);
    void put_ue(std::uint32_t value);
    void put_se(std::int32_t value);
    void byte_align_zero();
    void rbsp_trailing_bits();
    void put_byte(std::uint8_t value);

    [[nodiscard]] bool byte_aligned() const { return bits_in_current_ == 0; }
    [[nodiscard]] std::size_t bits_written() const;
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
    std::uint8_t current_ = 0;
    unsigned bits_in_current_ = 0;
};

std::vector<std::uint8_t> rbsp_to_ebsp(const std::vector<std::uint8_t>& rbsp);

} // namespace hevc
