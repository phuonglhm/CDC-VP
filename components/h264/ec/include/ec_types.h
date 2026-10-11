#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace h264::ec {

// Bit Buffer mô phỏng thanh ghi dịch phần cứng
struct BitStorage {
    std::uint8_t* data; std::size_t capacity;
    std::size_t size() const { return capacity; }
    std::uint8_t& operator[](std::size_t i) { return data[i]; }
    void fill(std::uint8_t v) { std::fill(data,data+capacity,v); }
};
struct BitWriter {
    BitStorage stream;
    std::uint32_t& byte_count;
    std::uint8_t bit_offset{0};

    BitWriter(std::array<std::uint8_t, 128>& buf, std::uint32_t& count)
        : stream{buf.data(),buf.size()}, byte_count(count) {
        byte_count = 0;
        stream.fill(0);
    }

    // Caller owns a pre-sized bounded slice buffer; it must not resize while writing.
    BitWriter(std::vector<std::uint8_t>& buf, std::uint32_t& count)
        : stream{buf.data(),buf.size()}, byte_count(count) { count=0;stream.fill(0); }
    std::size_t bits() const { return std::size_t(byte_count)*8+bit_offset; }
    void write_bits(std::uint32_t val, int len) {
        if (len < 0 || len > 32) throw std::invalid_argument("bit field width");
        if (byte_count > stream.size() || bit_offset > 7 ||
            std::uint64_t(byte_count)*8 + bit_offset + len > stream.size()*8)
            throw std::overflow_error("EC bit reservoir full");
        for (int i = len - 1; i >= 0; --i) {
            std::uint8_t bit = (val >> i) & 1;
            if(byte_count>=stream.size()) throw std::overflow_error("EC bit reservoir full");
            stream[byte_count] |= (bit << (7 - bit_offset));
            if (++bit_offset == 8) {
                bit_offset = 0;
                byte_count++;
            }
        }
    }

    void rbsp_trailing_bits() {
        write_bits(1, 1);
        if (bit_offset) write_bits(0, 8-bit_offset);
    }

    void align_byte() {
        if (bit_offset > 0) { byte_count++; bit_offset = 0; }
    }
};

struct EcRequest {
    std::array<std::int16_t, 16> levels; // Data từ TQ
    std::uint8_t qp; // Quantization Parameter
    std::uint8_t entropy_coding_mode; // 0: CAVLC, 1: CABAC

    int nc{0}; // Available-neighbour coefficient-count context.
    unsigned max_coeff{16}; // 16, 15 (AC only), or 4 (4:2:0 chroma DC).
    std::uint8_t nal_ref_idc{3};
    std::uint8_t nal_unit_type{5};
};

struct EcResult {
    std::array<std::uint8_t, 128> nal_stream; // Mảng tĩnh mô phỏng buffer phần cứng
    std::uint32_t stream_length;

    bool valid;
};



} // namespace h264::ec
