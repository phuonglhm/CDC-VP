#include "fx1_csi2_bfm/csi2_packet.h"

#include <stdexcept>

namespace fx1::csi2::bfm {

namespace {
// Parity contribution of each header bit D0..D23 to ECC bits P5..P0
// (column form of the CSI-2 SEC-DED (30,24) generator).
constexpr uint8_t kEccColumn[24] = {
    0x07, 0x0B, 0x0D, 0x0E, 0x13, 0x15, 0x16, 0x19, 0x1A, 0x1C, 0x23, 0x25,
    0x26, 0x29, 0x2A, 0x2C, 0x31, 0x32, 0x34, 0x38, 0x1F, 0x2F, 0x37, 0x3B,
};
}  // namespace

uint8_t header_ecc(uint32_t header24) {
    uint8_t ecc = 0;
    for (unsigned i = 0; i < 24; ++i)
        if (header24 >> i & 1u) ecc ^= kEccColumn[i];
    return ecc;
}

uint16_t payload_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc & 1u) ? static_cast<uint16_t>((crc >> 1) ^ 0x8408u) : static_cast<uint16_t>(crc >> 1);
    }
    return crc;
}

std::array<uint8_t, 4> packet_header(uint8_t vc, uint8_t dt, uint16_t wc_or_data) {
    const uint8_t di = data_id(vc, dt);
    const uint8_t lo = static_cast<uint8_t>(wc_or_data & 0xFF);
    const uint8_t hi = static_cast<uint8_t>(wc_or_data >> 8);
    const uint32_t h24 = di | (uint32_t(lo) << 8) | (uint32_t(hi) << 16);
    return {di, lo, hi, header_ecc(h24)};
}

std::vector<uint8_t> short_packet(uint8_t vc, uint8_t dt, uint16_t data) {
    const auto h = packet_header(vc, dt, data);
    return {h.begin(), h.end()};
}

std::vector<uint8_t> long_packet(uint8_t vc, uint8_t dt, const std::vector<uint8_t>& payload) {
    if (payload.size() > 0xFFFF) throw std::invalid_argument("long_packet: payload exceeds 16-bit word count");
    const auto h = packet_header(vc, dt, static_cast<uint16_t>(payload.size()));
    const size_t n = payload.size();
    std::vector<uint8_t> out(h.size() + n + 2);
    for (size_t i = 0; i < h.size(); ++i) out[i] = h[i];
    for (size_t i = 0; i < n; ++i) out[h.size() + i] = payload[i];
    const uint16_t crc = payload_crc16(payload.data(), n);
    out[h.size() + n] = static_cast<uint8_t>(crc & 0xFF);
    out[h.size() + n + 1] = static_cast<uint8_t>(crc >> 8);
    return out;
}

std::vector<uint8_t> pack_raw10(const std::vector<uint16_t>& px) {
    if (px.size() % 4) throw std::invalid_argument("pack_raw10: pixel count not a multiple of 4");
    std::vector<uint8_t> out;
    out.reserve(px.size() / 4 * 5);
    for (size_t i = 0; i < px.size(); i += 4) {
        uint8_t lsbs = 0;
        for (unsigned k = 0; k < 4; ++k) {
            const uint16_t p = px[i + k] & 0x3FF;
            out.push_back(static_cast<uint8_t>(p >> 2));
            lsbs |= static_cast<uint8_t>((p & 0x3) << (2 * k));
        }
        out.push_back(lsbs);
    }
    return out;
}

std::vector<uint8_t> pack_raw12(const std::vector<uint16_t>& px) {
    if (px.size() % 2) throw std::invalid_argument("pack_raw12: pixel count not a multiple of 2");
    std::vector<uint8_t> out;
    out.reserve(px.size() / 2 * 3);
    for (size_t i = 0; i < px.size(); i += 2) {
        const uint16_t p0 = px[i] & 0xFFF, p1 = px[i + 1] & 0xFFF;
        out.push_back(static_cast<uint8_t>(p0 >> 4));
        out.push_back(static_cast<uint8_t>(p1 >> 4));
        out.push_back(static_cast<uint8_t>((p0 & 0xF) | ((p1 & 0xF) << 4)));
    }
    return out;
}

std::vector<std::vector<uint8_t>> distribute_lanes(const std::vector<uint8_t>& burst, unsigned lanes) {
    if (lanes != 1 && lanes != 2 && lanes != 4) throw std::invalid_argument("distribute_lanes: lanes must be 1, 2 or 4");
    std::vector<std::vector<uint8_t>> out(lanes);
    for (size_t k = 0; k < burst.size(); ++k) out[k % lanes].push_back(burst[k]);
    return out;
}

}  // namespace fx1::csi2::bfm
