#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#include <tlm>

namespace cdc::components::isp {

enum class MemoryPlane : std::uint8_t {
    none,
    raw,
    y,
    cb,
    cr,
};

enum class MemoryError : std::uint8_t {
    none,
    zero_dimension,
    address_misaligned,
    stride_misaligned,
    stride_too_small,
    size_too_small,
    address_overflow,
    transfer_too_large,
    buffer_size_mismatch,
    plane_overlap,
    transport_error,
};

// Functional accounting only. These counters describe successful payload bytes,
// issued blocking transactions, and detected errors. They are deliberately not
// DDR bandwidth, bus-utilization, or cycle-performance counters.
struct MemoryIoCounters {
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;
    std::uint64_t read_transactions = 0;
    std::uint64_t write_transactions = 0;
    std::uint64_t errors = 0;

    void reset()
    {
        read_bytes = 0;
        write_bytes = 0;
        read_transactions = 0;
        write_transactions = 0;
        errors = 0;
    }
};

struct MemoryIoResult {
    MemoryError error = MemoryError::none;
    tlm::tlm_response_status response = tlm::TLM_OK_RESPONSE;
    MemoryPlane plane = MemoryPlane::none;
    std::uint64_t address = 0;
    std::uint32_t row = 0;

    bool ok() const { return error == MemoryError::none; }
    explicit operator bool() const { return ok(); }
};

struct Raw12RggbLe16Descriptor {
    std::uint64_t address = 0;
    std::uint64_t stride_bytes = 0;
    std::uint64_t size_bytes = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct I420PlaneDescriptor {
    std::uint64_t address = 0;
    std::uint64_t stride_bytes = 0;
    std::uint64_t size_bytes = 0;
};

struct I420FrameDescriptor {
    I420PlaneDescriptor y;
    I420PlaneDescriptor cb;
    I420PlaneDescriptor cr;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

namespace memory_detail {

inline bool checked_add_u64(std::uint64_t lhs,
                            std::uint64_t rhs,
                            std::uint64_t& result)
{
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
        return false;
    }
    result = lhs + rhs;
    return true;
}

inline bool checked_mul_u64(std::uint64_t lhs,
                            std::uint64_t rhs,
                            std::uint64_t& result)
{
    if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs) {
        return false;
    }
    result = lhs * rhs;
    return true;
}

inline bool checked_mul_size(std::size_t lhs,
                             std::size_t rhs,
                             std::size_t& result)
{
    if (lhs != 0 && rhs > std::numeric_limits<std::size_t>::max() / lhs) {
        return false;
    }
    result = lhs * rhs;
    return true;
}

// Number of bytes from the first byte of row zero through the last active byte
// of the final row. Padding after the final row is not part of the used span.
inline bool checked_frame_span(std::uint64_t rows,
                               std::uint64_t stride_bytes,
                               std::uint64_t active_row_bytes,
                               std::uint64_t& span_bytes)
{
    if (rows == 0) {
        return false;
    }

    std::uint64_t preceding_rows = 0;
    if (!checked_mul_u64(rows - 1, stride_bytes, preceding_rows)) {
        return false;
    }
    return checked_add_u64(preceding_rows, active_row_bytes, span_bytes);
}

inline std::uint32_t ceil_div2(std::uint32_t value)
{
    return value / 2u + value % 2u;
}

inline bool ranges_overlap(std::uint64_t first_begin,
                           std::uint64_t first_end,
                           std::uint64_t second_begin,
                           std::uint64_t second_end)
{
    return first_begin < second_end && second_begin < first_end;
}

inline MemoryIoResult make_error(MemoryError error,
                                 MemoryPlane plane = MemoryPlane::none,
                                 std::uint64_t address = 0,
                                 std::uint32_t row = 0,
                                 tlm::tlm_response_status response =
                                     tlm::TLM_INCOMPLETE_RESPONSE)
{
    MemoryIoResult result;
    result.error = error;
    result.response = response;
    result.plane = plane;
    result.address = address;
    result.row = row;
    return result;
}

} // namespace memory_detail

} // namespace cdc::components::isp
