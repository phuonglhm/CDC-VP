// Unit tests of the BFM packet builder. Expected values come from published
// reference vectors or from a second, independently written formulation; the
// builder is never used to compute its own expected value.
#include "fx1_csi2_bfm/csi2_packet.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using namespace fx1::csi2::bfm;

namespace {
int g_fail = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++g_fail;                                                        \
        }                                                                    \
    } while (0)

// Row form of the ECC generator, written from the parity equations
// P0..P5 rather than from the per-bit column table used by header_ecc().
uint8_t ecc_rows(uint32_t d) {
    auto b = [d](int i) { return (d >> i) & 1u; };
    unsigned p0 = b(0) ^ b(1) ^ b(2) ^ b(4) ^ b(5) ^ b(7) ^ b(10) ^ b(11) ^ b(13) ^ b(16) ^ b(20) ^ b(21) ^ b(22) ^ b(23);
    unsigned p1 = b(0) ^ b(1) ^ b(3) ^ b(4) ^ b(6) ^ b(8) ^ b(10) ^ b(12) ^ b(14) ^ b(17) ^ b(20) ^ b(21) ^ b(22) ^ b(23);
    unsigned p2 = b(0) ^ b(2) ^ b(3) ^ b(5) ^ b(6) ^ b(9) ^ b(11) ^ b(12) ^ b(15) ^ b(18) ^ b(20) ^ b(21) ^ b(22);
    unsigned p3 = b(1) ^ b(2) ^ b(3) ^ b(7) ^ b(8) ^ b(9) ^ b(13) ^ b(14) ^ b(15) ^ b(19) ^ b(20) ^ b(21) ^ b(23);
    unsigned p4 = b(4) ^ b(5) ^ b(6) ^ b(7) ^ b(8) ^ b(9) ^ b(16) ^ b(17) ^ b(18) ^ b(19) ^ b(20) ^ b(22) ^ b(23);
    unsigned p5 = b(10) ^ b(11) ^ b(12) ^ b(13) ^ b(14) ^ b(15) ^ b(16) ^ b(17) ^ b(18) ^ b(19) ^ b(21) ^ b(22) ^ b(23);
    return static_cast<uint8_t>(p0 | p1 << 1 | p2 << 2 | p3 << 3 | p4 << 4 | p5 << 5);
}

void test_ecc() {
    // Two formulations agree on a sweep of headers.
    uint32_t x = 0x12345u;
    for (int i = 0; i < 200000; ++i) {
        x = x * 1103515245u + 12345u;
        const uint32_t h = x & 0xFFFFFF;
        if (header_ecc(h) != ecc_rows(h)) { CHECK(header_ecc(h) == ecc_rows(h)); break; }
    }
    // SEC-DED structure: the 24 data-bit syndromes and 6 parity-bit
    // syndromes are non-zero and pairwise distinct, so every single-bit error
    // is correctable; every data-bit syndrome has odd weight >= 3, so a
    // double error never aliases to a single-bit syndrome of a data bit.
    std::vector<uint8_t> syn;
    for (int i = 0; i < 24; ++i) syn.push_back(header_ecc(1u << i));
    for (int i = 0; i < 6; ++i) syn.push_back(static_cast<uint8_t>(1u << i));
    for (size_t i = 0; i < syn.size(); ++i) {
        CHECK(syn[i] != 0);
        for (size_t j = i + 1; j < syn.size(); ++j) CHECK(syn[i] != syn[j]);
    }
    for (int i = 0; i < 24; ++i) CHECK(__builtin_popcount(syn[i]) >= 3 && (__builtin_popcount(syn[i]) & 1));
    CHECK(header_ecc(0) == 0);
}

void test_crc() {
    // CRC-16 with poly 0x1021 reflected, init 0xFFFF, no xorout is the
    // catalogued CRC-16/MCRF4XX; its check value over "123456789" is 0x6F91.
    const char* check = "123456789";
    CHECK(payload_crc16(reinterpret_cast<const uint8_t*>(check), std::strlen(check)) == 0x6F91);
    // MIPI CSI-2 specification CRC examples (24-byte payloads).
    const uint8_t ex1[] = {0xFF, 0x00, 0x00, 0x02, 0xB9, 0xDC, 0xF3, 0x72, 0xBB, 0xD4, 0xB8, 0x5A,
                           0xC8, 0x75, 0xC2, 0x7C, 0x81, 0xF8, 0x05, 0xDF, 0xFF, 0x00, 0x00, 0x01};
    const uint8_t ex2[] = {0xFF, 0x00, 0x00, 0x00, 0x1E, 0xF0, 0x1E, 0xC7, 0x4F, 0x82, 0x78, 0xC5,
                           0x82, 0xE0, 0x8C, 0x70, 0xD2, 0x3C, 0x78, 0xE9, 0xFF, 0x00, 0x00, 0x01};
    CHECK(payload_crc16(ex1, sizeof ex1) == 0x00F0);
    CHECK(payload_crc16(ex2, sizeof ex2) == 0xE569);
    CHECK(payload_crc16(nullptr, 0) == 0xFFFF);
}

void test_packets() {
    const auto fs = short_packet(0, DT_FS, 0x0001);
    CHECK(fs.size() == 4 && fs[0] == 0x00 && fs[1] == 0x01 && fs[2] == 0x00 && fs[3] == ecc_rows(0x000100));
    const auto le = short_packet(3, DT_LE, 0xBEEF);
    CHECK(le[0] == 0xC3 && le[1] == 0xEF && le[2] == 0xBE);
    const std::vector<uint8_t> pl = {1, 2, 3, 4, 5};
    const auto lp = long_packet(1, DT_RAW10, pl);
    CHECK(lp.size() == 4 + 5 + 2);
    CHECK(lp[0] == 0x6B && lp[1] == 5 && lp[2] == 0);
    CHECK(lp[3] == ecc_rows(0x6Bu | 5u << 8));
    CHECK(std::memcmp(lp.data() + 4, pl.data(), 5) == 0);
}

void test_pack() {
    // RAW10: pixel values chosen so each bit field is visible.
    // P0=0x3FF P1=0x000 P2=0x155 P3=0x2AA
    // MSB bytes: 0xFF, 0x00, 0x55, 0xAA; LSB byte: P3[1:0]=10 P2=01 P1=00 P0=11 -> 0b10010011 = 0x93
    const auto r10 = pack_raw10({0x3FF, 0x000, 0x155, 0x2AA});
    CHECK((r10 == std::vector<uint8_t>{0xFF, 0x00, 0x55, 0xAA, 0x93}));
    // RAW12: P0=0xABC P1=0x123 -> 0xAB, 0x12, {P1[3:0]=3, P0[3:0]=C} = 0x3C
    const auto r12 = pack_raw12({0xABC, 0x123});
    CHECK((r12 == std::vector<uint8_t>{0xAB, 0x12, 0x3C}));
    bool threw = false;
    try { pack_raw10({1, 2, 3}); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

void test_lanes() {
    const std::vector<uint8_t> b = {0, 1, 2, 3, 4, 5, 6};
    const auto l4 = distribute_lanes(b, 4);
    CHECK((l4[0] == std::vector<uint8_t>{0, 4}) && (l4[1] == std::vector<uint8_t>{1, 5}));
    CHECK((l4[2] == std::vector<uint8_t>{2, 6}) && (l4[3] == std::vector<uint8_t>{3}));
    const auto l1 = distribute_lanes(b, 1);
    CHECK(l1.size() == 1 && l1[0] == b);
    bool threw = false;
    try { distribute_lanes(b, 3); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}
}  // namespace

int main(int argc, char** argv) {
    const std::string which = argc > 1 ? argv[1] : "all";
    if (which == "all" || which == "ecc") test_ecc();
    if (which == "all" || which == "crc") test_crc();
    if (which == "all" || which == "packets") test_packets();
    if (which == "all" || which == "pack") test_pack();
    if (which == "all" || which == "lanes") test_lanes();
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
