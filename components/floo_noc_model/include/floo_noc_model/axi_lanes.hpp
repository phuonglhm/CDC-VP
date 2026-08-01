// SPDX-License-Identifier: Apache-2.0
//
// How a TLM byte range becomes AXI beats, byte lanes and `WSTRB`.
//
// Extracted from `src/noc_interconnect.cpp` so it can be tested directly.
// Asserting the generated `AxSIZE`, `AxLEN` and `WSTRB` through a memory
// target only ever shows their *effect* after the subordinate has replayed
// them; the fields themselves are what the A-3 signal adapter drives into the
// real timed chimney, so they are worth pinning on their own.

#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <tlm>

namespace cdc::components::axi_lanes {

// Every free function here is `inline`. They are non-template definitions in a
// public header, so without it two translation units that both include this
// file violate the one-definition rule and fail to link. The existing tests
// each have a single source file, which is exactly why it went unnoticed —
// `test_axi_lanes_odr` exists to keep it noticed.

/// AXI data width in the frozen configuration, in bytes.
inline constexpr unsigned bus_bytes = 8;

/// How one TLM transfer maps onto AXI beats and byte lanes.
///
/// The rule AXI actually uses: the byte at address `A` travels in lane
/// `A % bus_bytes`. An earlier version of this file ignored that and always put
/// the first payload byte in lane 0 with `WSTRB` built from bit 0, which is
/// only correct for a transfer that happens to start on a bus boundary. A
/// 4-byte access at `+4` was placed in lanes 0..3 and strobed there, so it
/// would have written the wrong half of the bus.
///
/// The defect was invisible through the old abstract endpoint path because the
/// subordinate side unpacked with the same wrong convention and the two errors
/// cancelled. Step A-3 put the timed chimney in the integrated path, so the
/// directed field and target-memory checks keep this mapping explicit.
struct axi_shape {
    /// Bus-aligned address of beat 0.
    std::uint64_t beat0_addr = 0;
    /// `addr % bus_bytes`: the lane the first byte occupies.
    unsigned lane_offset = 0;
    unsigned beats = 1;
    unsigned size_log2 = 3;

    unsigned beat_of(unsigned byte_index) const
    {
        return (lane_offset + byte_index) / bus_bytes;
    }
    unsigned lane_of(unsigned byte_index) const
    {
        return (lane_offset + byte_index) % bus_bytes;
    }
};

/// Chooses `AxSIZE` and the beat count for a byte range.
///
/// A naturally aligned power-of-two transfer no wider than the bus stays a
/// single narrow beat, because a 32-bit peripheral must see a 32-bit access and
/// not a padded 64-bit one. Everything else uses full-width beats and expresses
/// its edges through `WSTRB`.
///
/// The alignment condition is the part that was missing before: `length` being
/// a power of two is not enough, the address has to be a multiple of it too.
///
/// **`length` must be non-zero.** A zero length would evaluate `addr % length`,
/// which is undefined, and a zero-byte AXI transfer does not exist. This is a
/// public helper, so it enforces the precondition rather than documenting it
/// and hoping: every caller already rejects a zero-length payload earlier, and
/// if one ever stops doing so the failure should be loud and immediate rather
/// than undefined behaviour deep in a modulo.
inline axi_shape shape_of(std::uint64_t addr, unsigned length)
{
    if (length == 0) {
        throw std::invalid_argument(
            "axi_lanes::shape_of: a transfer needs at least one byte");
    }
    axi_shape shape{};
    shape.lane_offset = static_cast<unsigned>(addr % bus_bytes);
    shape.beat0_addr = addr - shape.lane_offset;

    const bool power_of_two = (length & (length - 1)) == 0;
    if (power_of_two && length <= bus_bytes && (addr % length) == 0) {
        shape.size_log2 = 0;
        while ((1u << shape.size_log2) < length) {
            ++shape.size_log2;
        }
    } else {
        shape.size_log2 = 3;
    }
    // Correct for both cases: natural alignment guarantees a narrow transfer
    // never straddles a bus boundary, so this evaluates to 1 there.
    //
    // Computed in 64 bits. `lane_offset + length + bus_bytes - 1` overflows a
    // 32-bit unsigned for a length within about 8 bytes of `UINT32_MAX`, which
    // would produce a small beat count for an enormous transfer — the burst
    // limit above would then wave it through.
    const std::uint64_t span = static_cast<std::uint64_t>(shape.lane_offset)
                             + length + (bus_bytes - 1);
    shape.beats = static_cast<unsigned>(span / bus_bytes);
    return shape;
}

/// True when byte `index` of the payload is enabled.
///
/// TLM byte enables repeat with period `byte_enable_length` when that is
/// shorter than the payload; a null pointer means every byte is enabled.
inline bool byte_enabled(
    const unsigned char* enables, unsigned enable_length, unsigned index)
{
    if (enables == nullptr) {
        return true;
    }
    return enables[index % enable_length] == TLM_BYTE_ENABLED;
}

/// Places payload bytes into AXI lanes and builds one `WSTRB` per beat.
struct write_view {
    std::vector<std::uint64_t> data;
    std::vector<std::uint64_t> strb;
};

inline write_view pack_write(
    const unsigned char* bytes, unsigned length, const axi_shape& shape,
    const unsigned char* enables, unsigned enable_length)
{
    write_view view{};
    view.data.assign(shape.beats, 0);
    view.strb.assign(shape.beats, 0);
    for (unsigned index = 0; index < length; ++index) {
        if (!byte_enabled(enables, enable_length, index)) {
            continue;
        }
        const unsigned beat = shape.beat_of(index);
        const unsigned lane = shape.lane_of(index);
        view.data[beat] |= static_cast<std::uint64_t>(bytes[index])
                        << (8 * lane);
        view.strb[beat] |= 1ull << lane;
    }
    return view;
}

/// Extracts payload bytes back out of the lanes they returned in.
inline void unpack_read(
    const std::vector<std::uint64_t>& beats, unsigned char* bytes,
    unsigned length, const axi_shape& shape, const unsigned char* enables,
    unsigned enable_length)
{
    for (unsigned index = 0; index < length; ++index) {
        // A disabled byte is left as the caller had it. TLM says a read only
        // delivers the bytes it was asked for.
        if (!byte_enabled(enables, enable_length, index)) {
            continue;
        }
        const unsigned beat = shape.beat_of(index);
        if (beat >= beats.size()) {
            break;
        }
        bytes[index] = static_cast<unsigned char>(
            (beats[beat] >> (8 * shape.lane_of(index))) & 0xFF);
    }
}


} // namespace cdc::components::axi_lanes
