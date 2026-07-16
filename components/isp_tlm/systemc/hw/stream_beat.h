/**
 * @file stream_beat.h
 * @brief Frame-aware stream beat type for ISP pipeline
 *
 * Defines the standard beat type that flows through the ISP pipeline.
 * Each beat contains:
 *   - data: array of lane values (pixel data)
 *   - valid_lanes: number of valid lanes in this beat
 *   - frame markers: start_of_frame, end_of_line, end_of_frame
 *   - frame_id: unique identifier for multi-frame support
 *   - timestamp: simulation time when beat was created
 */

#ifndef STREAM_BEAT_H
#define STREAM_BEAT_H

#include <array>
#include <cstdint>
#include <systemc>
using namespace sc_core;

// ============================================================================
// Stream Beat Type
// ============================================================================
template <typename T, std::size_t Lanes = 1>
struct stream_beat {
    std::array<T, Lanes> data;             // Pixel data per lane
    std::uint32_t valid_lanes = 0;          // Number of valid lanes (0 to Lanes)

    // Frame markers
    bool start_of_frame = false;            // First beat of a frame
    bool end_of_line = false;               // Last beat of a line
    bool end_of_frame = false;              // Last beat of a frame

    // Metadata
    std::uint64_t frame_id = 0;            // Frame counter
    std::uint32_t line_id = 0;             // Current line number
    std::uint32_t pixel_id = 0;             // Global pixel index
    sc_time timestamp = SC_ZERO_TIME;       // Creation timestamp

    // Convenience methods
    bool is_valid() const { return valid_lanes > 0; }
    bool is_marker_only() const { return valid_lanes == 0 && (start_of_frame || end_of_line || end_of_frame); }

    // Get lane value (bounds-checked)
    T get_lane(std::size_t idx) const {
        return (idx < valid_lanes) ? data[idx] : T{};
    }

    // Set lane value
    void set_lane(std::size_t idx, T value) {
        if (idx < Lanes) {
            data[idx] = value;
            if (idx >= valid_lanes) valid_lanes = idx + 1;
        }
    }

    // Reset beat to empty state
    void reset() {
        data.fill(T{});
        valid_lanes = 0;
        start_of_frame = false;
        end_of_line = false;
        end_of_frame = false;
        timestamp = SC_ZERO_TIME;
    }

    // Create frame start marker
    static stream_beat frame_start(std::uint64_t frame_id, std::uint32_t line_id = 0) {
        stream_beat beat;
        beat.start_of_frame = true;
        beat.frame_id = frame_id;
        beat.line_id = line_id;
        beat.timestamp = sc_time_stamp();
        return beat;
    }

    // Create line end marker
    static stream_beat line_end(std::uint64_t frame_id, std::uint32_t line_id) {
        stream_beat beat;
        beat.end_of_line = true;
        beat.frame_id = frame_id;
        beat.line_id = line_id;
        beat.timestamp = sc_time_stamp();
        return beat;
    }

    // Create frame end marker
    static stream_beat frame_end(std::uint64_t frame_id, std::uint32_t line_id) {
        stream_beat beat;
        beat.end_of_frame = true;
        beat.frame_id = frame_id;
        beat.line_id = line_id;
        beat.timestamp = sc_time_stamp();
        return beat;
    }
};

// ============================================================================
// Common instantiations
// ============================================================================
using raw_beat = stream_beat<std::uint16_t, 1>;    // RAW domain (Bayer)
using rgb_beat = stream_beat<std::uint16_t, 1>;     // RGB domain (per channel)
using yuv_beat = stream_beat<std::uint8_t, 1>;       // YUV domain

// Multi-lane variants for parallelism exploration
using raw_beat_4x = stream_beat<std::uint16_t, 4>;  // 4-lane RAW
using yuv_beat_4x = stream_beat<std::uint8_t, 4>;    // 4-lane YUV

// ============================================================================
// Stream Constants
// ============================================================================
namespace stream_const {
    constexpr std::uint64_t INVALID_FRAME_ID = UINT64_MAX;
    constexpr std::uint32_t INVALID_LINE_ID = UINT32_MAX;
    constexpr std::uint32_t INVALID_PIXEL_ID = UINT32_MAX;
}

#endif  // STREAM_BEAT_H
