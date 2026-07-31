// SPDX-License-Identifier: Apache-2.0
//
// The AXI fields a TLM transfer turns into: `AxSIZE`, the beat count that
// becomes `AxLEN`, and `WSTRB` per beat.
//
// Entry point is `sc_main` only because linking libsystemc demands one; no
// kernel runs here, the arithmetic is pure.
//
// These are checked here rather than only through a memory target, because a
// memory target sees the *replayed* access — the subordinate has already
// unpacked the lanes by then, so a packing error and a matching unpacking
// error cancel. What Step A-3 will drive into the timed chimney is exactly the
// fields below, so they are pinned directly.

#include "floo_noc_model/axi_lanes.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>

namespace {

using namespace cdc::components::axi_lanes;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

/// `AxLEN` is the encoded beat count: beats - 1.
unsigned axlen_of(const axi_shape& shape) { return shape.beats - 1; }

} // namespace

int sc_main(int, char**)
{
    // ---- AxSIZE: narrow only when naturally aligned ------------------------
    //
    // A power-of-two length is not sufficient. A 4-byte access at `+4` is
    // aligned and stays a 4-byte access; the same length at `+2` is not, and
    // has to become a full-width beat expressed through WSTRB. Getting this
    // wrong is how a 32-bit peripheral ends up seeing a 64-bit access, or a
    // narrow access lands in the wrong half of the bus.
    struct size_case {
        std::uint64_t addr;
        unsigned length;
        unsigned expect_size;
        unsigned expect_beats;
        unsigned expect_lane;
    };
    const size_case sizes[] = {
        {0x1000, 1, 0, 1, 0},   // aligned byte
        {0x1003, 1, 0, 1, 3},   // a byte is aligned at any address
        {0x1000, 2, 1, 1, 0},
        {0x1006, 2, 1, 1, 6},   // aligned halfword in the top lanes
        {0x1000, 4, 2, 1, 0},
        {0x1004, 4, 2, 1, 4},   // aligned word at +4
        {0x1000, 8, 3, 1, 0},
        {0x1002, 4, 3, 1, 2},   // *un*aligned word: full width, not AxSIZE 2
        {0x1001, 2, 3, 1, 1},   // unaligned halfword
        {0x1000, 6, 3, 1, 0},   // odd length: full width, one beat
        {0x1005, 6, 3, 2, 5},   // odd length straddling a bus boundary
        {0x1000, 16, 3, 2, 0},  // two full beats
        {0x1004, 16, 3, 3, 4},  // two beats' worth, offset: three beats
    };

    for (const auto& item : sizes) {
        const auto shape = shape_of(item.addr, item.length);
        const std::string where = "addr 0x" + std::to_string(item.addr)
                                + " length " + std::to_string(item.length);
        check(shape.size_log2 == item.expect_size, where + ": AxSIZE");
        check(shape.beats == item.expect_beats, where + ": beat count");
        check(shape.lane_offset == item.expect_lane, where + ": lane offset");
        check(shape.beat0_addr == item.addr - item.expect_lane,
              where + ": beat 0 address");
        check(axlen_of(shape) == item.expect_beats - 1, where + ": AxLEN");
    }

    // ---- WSTRB: which lanes, not how many ----------------------------------
    {
        // A 32-bit write at +4 strobes the upper half, not the lower.
        const unsigned char bytes[4] = {0x11, 0x22, 0x33, 0x44};
        const auto shape = shape_of(0x1004, 4);
        const auto view = pack_write(bytes, 4, shape, nullptr, 0);
        check(view.strb.size() == 1, "a 32-bit aligned write is one beat");
        check(view.strb[0] == 0xF0ull,
              "a 32-bit write at +4 must strobe lanes 4..7");
        check(view.data[0] == 0x4433'2211'0000'0000ull,
              "its bytes must sit in lanes 4..7, in order");
    }
    {
        // A 16-bit write at +6 strobes only the top two lanes.
        const unsigned char bytes[2] = {0xEF, 0xBE};
        const auto shape = shape_of(0x1006, 2);
        const auto view = pack_write(bytes, 2, shape, nullptr, 0);
        check(view.strb[0] == 0xC0ull,
              "a 16-bit write at +6 must strobe lanes 6 and 7");
        check(view.data[0] == 0xBEEF'0000'0000'0000ull,
              "and place its bytes there");
    }
    {
        // Six bytes from +5: three lanes of beat 0, three of beat 1. Both beats
        // are partial — the old single-strobe model could not express that.
        const unsigned char bytes[6] = {1, 2, 3, 4, 5, 6};
        const auto shape = shape_of(0x1005, 6);
        const auto view = pack_write(bytes, 6, shape, nullptr, 0);
        check(view.strb.size() == 2, "a straddling transfer is two beats");
        check(view.strb[0] == 0xE0ull, "beat 0 strobes lanes 5..7");
        check(view.strb[1] == 0x07ull, "beat 1 strobes lanes 0..2");
        check(((view.data[0] >> 40) & 0xFFFFFFull) == 0x030201ull,
              "beat 0 carries the first three bytes in its top lanes");
        check((view.data[1] & 0xFFFFFFull) == 0x060504ull,
              "beat 1 carries the rest in its bottom lanes");
    }
    {
        // Byte enables punch holes anywhere, including in the middle.
        const unsigned char bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        const unsigned char enables[8] = {
            TLM_BYTE_ENABLED, 0, TLM_BYTE_ENABLED, 0,
            TLM_BYTE_ENABLED, 0, TLM_BYTE_ENABLED, 0};
        const auto shape = shape_of(0x1000, 8);
        const auto view = pack_write(bytes, 8, shape, enables, 8);
        check(view.strb[0] == 0x55ull,
              "a disabled byte must clear its own strobe bit and no other");
        check(((view.data[0] >> 8) & 0xFFull) == 0,
              "a disabled lane must carry no data");
    }
    {
        // TLM byte enables repeat when shorter than the payload.
        const unsigned char bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        const unsigned char enables[2] = {TLM_BYTE_ENABLED, 0};
        const auto shape = shape_of(0x1000, 8);
        const auto view = pack_write(bytes, 8, shape, enables, 2);
        check(view.strb[0] == 0x55ull,
              "a short byte-enable array must repeat across the payload");
    }

    // ---- reads come back out of the same lanes -----------------------------
    {
        const auto shape = shape_of(0x1005, 6);
        // What a subordinate would return for the packing checked above.
        const std::vector<std::uint64_t> beats = {
            0x0302'0100'0000'0000ull, 0x0000'0000'0006'0504ull};
        unsigned char out[6] = {};
        unpack_read(beats, out, 6, shape, nullptr, 0);
        // beat 0 lanes 5,6,7 hold 0x01,0x02,0x03; beat 1 lanes 0,1,2 hold
        // 0x04,0x05,0x06 — which is the packing checked a few blocks above,
        // read back the other way.
        const unsigned char expect[6] = {1, 2, 3, 4, 5, 6};
        for (unsigned index = 0; index < 6; ++index) {
            check(out[index] == expect[index],
                  "a straddling read must be taken from the lanes it arrived in");
        }
    }
    {
        // A disabled byte is left as the caller had it, per TLM.
        const auto shape = shape_of(0x1000, 4);
        const std::vector<std::uint64_t> beats = {0xAABB'CCDDull};
        unsigned char out[4] = {0x77, 0x77, 0x77, 0x77};
        const unsigned char enables[4] = {TLM_BYTE_ENABLED, 0,
                                          TLM_BYTE_ENABLED, 0};
        unpack_read(beats, out, 4, shape, enables, 4);
        check(out[0] == 0xDD && out[2] == 0xBB,
              "enabled bytes must be delivered");
        check(out[1] == 0x77 && out[3] == 0x77,
              "disabled bytes must be left untouched");
    }

    if (failures == 0) {
        std::cout << "PASS: AXI lane placement\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " lane checks failed\n";
    return 1;
}
