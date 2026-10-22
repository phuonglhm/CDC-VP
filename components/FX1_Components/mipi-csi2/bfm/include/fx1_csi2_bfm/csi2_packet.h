// CSI-2 packet construction for the sensor BFM (transmit side only).
//
// Pure C++, no SystemC. Builds the byte stream a CSI-2 v1.3 transmitter puts
// on the D-PHY lanes: short packets (FS/FE/LS/LE/generic), long packets with
// header ECC and payload CRC-16, RAW10/RAW12 packing, and the round-robin
// distribution of a burst over 1, 2 or 4 lanes.
//
// The receiver under test must not reuse these functions to compute its
// expected values; tests use independent constants or a separate decoder.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace fx1::csi2::bfm {

// Data types used by the first milestone (CSI-2 v1.3 codes).
enum DataType : uint8_t {
    DT_FS = 0x00,
    DT_FE = 0x01,
    DT_LS = 0x02,
    DT_LE = 0x03,
    DT_NULL = 0x10,
    DT_BLANKING = 0x11,
    DT_EMBEDDED = 0x12,
    DT_RAW10 = 0x2B,
    DT_RAW12 = 0x2C,
};

// Data identifier byte: VC in [7:6], DT in [5:0].
constexpr uint8_t data_id(uint8_t vc, uint8_t dt) {
    return static_cast<uint8_t>(((vc & 0x3u) << 6) | (dt & 0x3Fu));
}

// 6-bit header ECC over the 24 bits {DI, WC/data LSB, WC/data MSB}
// (bit 0 = DI bit 0). Bits 7:6 of the ECC byte are 0.
uint8_t header_ecc(uint32_t header24);

// CSI-2 payload CRC-16: polynomial x^16 + x^12 + x^5 + 1, seed 0xFFFF,
// LSB-first. Transmitted LSB byte first.
uint16_t payload_crc16(const uint8_t* data, size_t len);

// 4-byte packet header: DI, WC/data LSB, WC/data MSB, ECC.
std::array<uint8_t, 4> packet_header(uint8_t vc, uint8_t dt, uint16_t wc_or_data);

std::vector<uint8_t> short_packet(uint8_t vc, uint8_t dt, uint16_t data);
// Header + payload + CRC (LSB first). payload.size() must fit in 16 bits.
std::vector<uint8_t> long_packet(uint8_t vc, uint8_t dt, const std::vector<uint8_t>& payload);

// Pixel packing. Pixels are right-aligned samples; extra MSBs are ignored.
// RAW10: 4 pixels -> 5 bytes (P0..P3 bits 9:2, then {P3,P2,P1,P0} bits 1:0).
// RAW12: 2 pixels -> 3 bytes (P0, P1 bits 11:4, then {P1,P0} bits 3:0).
// The pixel count must be a multiple of 4 (RAW10) or 2 (RAW12).
std::vector<uint8_t> pack_raw10(const std::vector<uint16_t>& pixels);
std::vector<uint8_t> pack_raw12(const std::vector<uint16_t>& pixels);

// Round-robin lane distribution of one HS burst: byte k goes to lane k % lanes.
// lanes must be 1, 2 or 4. Lanes may end up with unequal lengths (the
// transmitter ends shorter lanes earlier).
std::vector<std::vector<uint8_t>> distribute_lanes(const std::vector<uint8_t>& burst, unsigned lanes);

}  // namespace fx1::csi2::bfm
