#include "hevc/bit_writer.hpp"

#include <bit>

namespace hevc {

void BitWriter::put_bit(bool bit) {
    current_ = static_cast<std::uint8_t>((current_ << 1) | (bit ? 1 : 0));
    ++bits_in_current_;
    if (bits_in_current_ == 8) {
        bytes_.push_back(current_);
        current_ = 0;
        bits_in_current_ = 0;
    }
}

void BitWriter::put_bits(std::uint64_t value, unsigned count) {
    if (count > 64) {
        throw std::invalid_argument("put_bits count exceeds 64");
    }
    for (unsigned i = count; i > 0; --i) {
        put_bit(((value >> (i - 1)) & 1U) != 0);
    }
}

void BitWriter::put_ue(std::uint32_t value) {
    const std::uint64_t code_num = static_cast<std::uint64_t>(value) + 1;
    const unsigned width = 64U - std::countl_zero(code_num);
    for (unsigned i = 1; i < width; ++i) {
        put_bit(false);
    }
    put_bits(code_num, width);
}

void BitWriter::put_se(std::int32_t value) {
    const std::uint32_t mapped = value <= 0
        ? static_cast<std::uint32_t>(-2LL * value)
        : static_cast<std::uint32_t>(2LL * value - 1);
    put_ue(mapped);
}

void BitWriter::byte_align_zero() {
    while (!byte_aligned()) {
        put_bit(false);
    }
}

void BitWriter::rbsp_trailing_bits() {
    put_bit(true);
    byte_align_zero();
}

void BitWriter::put_byte(std::uint8_t value) {
    if (!byte_aligned()) {
        throw std::logic_error("put_byte requires byte alignment");
    }
    bytes_.push_back(value);
}

std::size_t BitWriter::bits_written() const {
    return bytes_.size() * 8 + bits_in_current_;
}

std::vector<std::uint8_t> rbsp_to_ebsp(const std::vector<std::uint8_t>& rbsp) {
    std::vector<std::uint8_t> ebsp;
    ebsp.reserve(rbsp.size() + rbsp.size() / 32 + 8);
    unsigned zero_count = 0;
    for (const auto byte : rbsp) {
        if (zero_count >= 2 && byte <= 0x03) {
            ebsp.push_back(0x03);
            zero_count = 0;
        }
        ebsp.push_back(byte);
        zero_count = byte == 0 ? zero_count + 1 : 0;
    }
    return ebsp;
}

} // namespace hevc
