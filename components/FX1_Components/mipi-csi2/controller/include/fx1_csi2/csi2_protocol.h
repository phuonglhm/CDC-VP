// Receive-side CSI-2 protocol helpers of the controller: header ECC check and
// correction (SEC-DED (30,24)) and payload CRC-16.
//
// Written independently of the BFM's packet builder (bfm/csi2_packet.h): the
// ECC is evaluated from the parity equations (row form) rather than from the
// per-bit column table the builder uses, so a shared mistake does not cancel
// out. Both are checked against the MIPI CRC examples in the unit tests.
#pragma once

#include <cstddef>
#include <cstdint>

namespace fx1::csi2 {

enum class EccResult { Ok, Corrected, Uncorrectable };

struct HeaderCheck {
    EccResult result;
    uint32_t header24;  // corrected {DI, WC/data LSB, WC/data MSB} (bit 0 = DI bit 0)
    unsigned bit;       // corrected bit: 0..23 data, 24..29 parity P0..P5; undefined otherwise
};

// `bytes` = DI, WC LSB, WC MSB, ECC. ECC bits 7:6 are not checked (MAS §6.4).
HeaderCheck check_header(const uint8_t bytes[4]);

// CSI-2 payload CRC-16 (x^16 + x^12 + x^5 + 1, seed 0xFFFF, LSB first),
// updated byte by byte.
uint16_t crc16_update(uint16_t crc, uint8_t byte);

}  // namespace fx1::csi2
