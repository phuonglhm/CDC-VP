#pragma once
#include <array>
#include <cstdint>

namespace h264::ec {

// Bit Buffer mô phỏng thanh ghi dịch phần cứng
struct BitWriter {
    std::array<std::uint8_t, 128>& stream;
    std::uint32_t& byte_count;
    std::uint8_t bit_offset{0};

    BitWriter(std::array<std::uint8_t, 128>& buf, std::uint32_t& count) 
        : stream(buf), byte_count(count) {
        byte_count = 0;
        stream.fill(0);
    }

    void write_bits(std::uint32_t val, int len) {
        for (int i = len - 1; i >= 0; --i) {
            std::uint8_t bit = (val >> i) & 1;
            stream[byte_count] |= (bit << (7 - bit_offset));
            if (++bit_offset == 8) {
                bit_offset = 0;
                byte_count++;
            }
        }
    }
    
    void align_byte() {
        if (bit_offset > 0) { byte_count++; bit_offset = 0; }
    }
};

struct EcRequest {
    std::array<std::int16_t, 16> levels; // Data từ TQ
    std::uint8_t qp; // Quantization Parameter
    std::uint8_t entropy_coding_mode; // 0: CAVLC, 1: CABAC

    std::uint8_t nal_ref_idc{3};
    std::uint8_t nal_unit_type{5};
};

struct EcResult {
    std::array<std::uint8_t, 128> nal_stream; // Mảng tĩnh mô phỏng buffer phần cứng
    std::uint32_t stream_length;

    bool valid;
};



} // namespace h264::ec