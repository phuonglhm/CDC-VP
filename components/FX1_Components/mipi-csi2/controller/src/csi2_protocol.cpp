#include "fx1_csi2/csi2_protocol.h"

namespace fx1::csi2 {

namespace {
// Parity equations P0..P5 of the CSI-2 header ECC over D0..D23.
uint8_t parity(uint32_t d) {
    auto b = [d](int i) { return (d >> i) & 1u; };
    const unsigned p0 = b(0) ^ b(1) ^ b(2) ^ b(4) ^ b(5) ^ b(7) ^ b(10) ^ b(11) ^ b(13) ^ b(16) ^ b(20) ^ b(21) ^ b(22) ^ b(23);
    const unsigned p1 = b(0) ^ b(1) ^ b(3) ^ b(4) ^ b(6) ^ b(8) ^ b(10) ^ b(12) ^ b(14) ^ b(17) ^ b(20) ^ b(21) ^ b(22) ^ b(23);
    const unsigned p2 = b(0) ^ b(2) ^ b(3) ^ b(5) ^ b(6) ^ b(9) ^ b(11) ^ b(12) ^ b(15) ^ b(18) ^ b(20) ^ b(21) ^ b(22);
    const unsigned p3 = b(1) ^ b(2) ^ b(3) ^ b(7) ^ b(8) ^ b(9) ^ b(13) ^ b(14) ^ b(15) ^ b(19) ^ b(20) ^ b(21) ^ b(23);
    const unsigned p4 = b(4) ^ b(5) ^ b(6) ^ b(7) ^ b(8) ^ b(9) ^ b(16) ^ b(17) ^ b(18) ^ b(19) ^ b(20) ^ b(22) ^ b(23);
    const unsigned p5 = b(10) ^ b(11) ^ b(12) ^ b(13) ^ b(14) ^ b(15) ^ b(16) ^ b(17) ^ b(18) ^ b(19) ^ b(21) ^ b(22) ^ b(23);
    return static_cast<uint8_t>(p0 | p1 << 1 | p2 << 2 | p3 << 3 | p4 << 4 | p5 << 5);
}
}  // namespace

HeaderCheck check_header(const uint8_t bytes[4]) {
    const uint32_t d = bytes[0] | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16);
    const uint8_t syndrome = static_cast<uint8_t>((parity(d) ^ bytes[3]) & 0x3F);
    if (syndrome == 0) return {EccResult::Ok, d, 0};
    // Error in a parity bit: data unchanged.
    for (unsigned p = 0; p < 6; ++p)
        if (syndrome == (1u << p)) return {EccResult::Corrected, d, 24 + p};
    // Error in a data bit: the syndrome equals that bit's parity signature.
    for (unsigned i = 0; i < 24; ++i)
        if (parity(1u << i) == syndrome) return {EccResult::Corrected, d ^ (1u << i), i};
    return {EccResult::Uncorrectable, d, 0};
}

uint16_t crc16_update(uint16_t crc, uint8_t byte) {
    crc ^= byte;
    for (int b = 0; b < 8; ++b) crc = (crc & 1u) ? static_cast<uint16_t>((crc >> 1) ^ 0x8408u) : static_cast<uint16_t>(crc >> 1);
    return crc;
}

}  // namespace fx1::csi2
