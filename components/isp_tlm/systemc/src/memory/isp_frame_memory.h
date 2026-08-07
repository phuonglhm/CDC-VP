#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "memory/isp_memory_types.h"

namespace cdc::components::isp {

// ISP-owned functional frame-memory master. The module issues physical
// addresses exactly as programmed; address decoding/translation belongs to the
// platform interconnect. Methods must be called from a SystemC process because
// annotated b_transport delay is consumed with wait().
class IspFrameMemory : public sc_core::sc_module {
public:
    static constexpr std::uint64_t kRawAddressAlignment = 2;
    static constexpr std::uint64_t kI420AddressAlignment = 4;

    tlm_utils::simple_initiator_socket<IspFrameMemory> memory_socket;

    explicit IspFrameMemory(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , memory_socket("memory_socket")
    {
    }

    const MemoryIoCounters& counters() const { return counters_; }
    void reset_counters() { counters_.reset(); }

    // RAW12_RGGB_LE16: one little-endian 16-bit memory word per pixel. Bits
    // [11:0] contain the sample; bits [15:12] are reserved and ignored.
    MemoryIoResult read_raw12_frame(const Raw12RggbLe16Descriptor& descriptor,
                                    std::vector<std::uint16_t>& pixels)
    {
        std::uint64_t row_bytes = 0;
        std::uint64_t span_bytes = 0;
        std::size_t pixel_count = 0;

        MemoryIoResult validation =
            validate_raw_descriptor(descriptor, row_bytes, span_bytes, pixel_count);
        if (!validation) {
            return record_error(validation);
        }

        std::vector<std::uint16_t> decoded(pixel_count, 0);
        std::vector<unsigned char> row_storage(static_cast<std::size_t>(row_bytes), 0);

        for (std::uint32_t row = 0; row < descriptor.height; ++row) {
            std::uint64_t row_offset = 0;
            std::uint64_t row_address = 0;
            if (!memory_detail::checked_mul_u64(row,
                                                descriptor.stride_bytes,
                                                row_offset) ||
                !memory_detail::checked_add_u64(descriptor.address,
                                                row_offset,
                                                row_address)) {
                return record_error(memory_detail::make_error(
                    MemoryError::address_overflow, MemoryPlane::raw, descriptor.address, row));
            }

            MemoryIoResult access = transport(tlm::TLM_READ_COMMAND,
                                              row_address,
                                              row_storage.data(),
                                              static_cast<unsigned int>(row_bytes),
                                              MemoryPlane::raw,
                                              row);
            if (!access) {
                return access;
            }

            const std::size_t output_offset =
                static_cast<std::size_t>(row) * descriptor.width;
            for (std::uint32_t column = 0; column < descriptor.width; ++column) {
                const std::size_t byte_offset = static_cast<std::size_t>(column) * 2u;
                const std::uint16_t word =
                    static_cast<std::uint16_t>(row_storage[byte_offset]) |
                    static_cast<std::uint16_t>(row_storage[byte_offset + 1u] << 8u);
                decoded[output_offset + column] = word & 0x0fffu;
            }
        }

        pixels.swap(decoded);
        return MemoryIoResult {};
    }

    // Writes planar I420 in Y, Cb, Cr order. Input vectors are tightly packed;
    // destination row padding is controlled independently by each plane stride.
    MemoryIoResult write_i420_frame(const I420FrameDescriptor& descriptor,
                                    const std::vector<std::uint8_t>& y,
                                    const std::vector<std::uint8_t>& cb,
                                    const std::vector<std::uint8_t>& cr)
    {
        if (descriptor.width == 0 || descriptor.height == 0) {
            return record_error(memory_detail::make_error(MemoryError::zero_dimension));
        }

        const std::uint32_t chroma_width = memory_detail::ceil_div2(descriptor.width);
        const std::uint32_t chroma_height = memory_detail::ceil_div2(descriptor.height);

        std::size_t expected_y = 0;
        std::size_t expected_chroma = 0;
        if (!memory_detail::checked_mul_size(descriptor.width,
                                             descriptor.height,
                                             expected_y) ||
            !memory_detail::checked_mul_size(chroma_width,
                                             chroma_height,
                                             expected_chroma)) {
            return record_error(memory_detail::make_error(MemoryError::transfer_too_large));
        }

        if (y.size() != expected_y) {
            return record_error(memory_detail::make_error(
                MemoryError::buffer_size_mismatch, MemoryPlane::y));
        }
        if (cb.size() != expected_chroma) {
            return record_error(memory_detail::make_error(
                MemoryError::buffer_size_mismatch, MemoryPlane::cb));
        }
        if (cr.size() != expected_chroma) {
            return record_error(memory_detail::make_error(
                MemoryError::buffer_size_mismatch, MemoryPlane::cr));
        }

        PlaneLayout y_layout;
        PlaneLayout cb_layout;
        PlaneLayout cr_layout;

        MemoryIoResult validation = validate_plane(descriptor.y,
                                                   descriptor.width,
                                                   descriptor.height,
                                                   MemoryPlane::y,
                                                   y_layout);
        if (!validation) {
            return record_error(validation);
        }
        validation = validate_plane(descriptor.cb,
                                    chroma_width,
                                    chroma_height,
                                    MemoryPlane::cb,
                                    cb_layout);
        if (!validation) {
            return record_error(validation);
        }
        validation = validate_plane(descriptor.cr,
                                    chroma_width,
                                    chroma_height,
                                    MemoryPlane::cr,
                                    cr_layout);
        if (!validation) {
            return record_error(validation);
        }

        if (memory_detail::ranges_overlap(y_layout.begin,
                                          y_layout.end,
                                          cb_layout.begin,
                                          cb_layout.end) ||
            memory_detail::ranges_overlap(y_layout.begin,
                                          y_layout.end,
                                          cr_layout.begin,
                                          cr_layout.end) ||
            memory_detail::ranges_overlap(cb_layout.begin,
                                          cb_layout.end,
                                          cr_layout.begin,
                                          cr_layout.end)) {
            return record_error(memory_detail::make_error(MemoryError::plane_overlap));
        }

        MemoryIoResult result = write_plane(y_layout, y);
        if (!result) {
            return result;
        }
        result = write_plane(cb_layout, cb);
        if (!result) {
            return result;
        }
        return write_plane(cr_layout, cr);
    }

private:
    struct PlaneLayout {
        const I420PlaneDescriptor* descriptor = nullptr;
        MemoryPlane plane = MemoryPlane::none;
        std::uint32_t row_bytes = 0;
        std::uint32_t rows = 0;
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
    };

    MemoryIoCounters counters_;

    MemoryIoResult record_error(MemoryIoResult result)
    {
        ++counters_.errors;
        return result;
    }

    static MemoryIoResult validate_raw_descriptor(
        const Raw12RggbLe16Descriptor& descriptor,
        std::uint64_t& row_bytes,
        std::uint64_t& span_bytes,
        std::size_t& pixel_count)
    {
        if (descriptor.width == 0 || descriptor.height == 0) {
            return memory_detail::make_error(MemoryError::zero_dimension,
                                             MemoryPlane::raw);
        }
        if ((descriptor.address % kRawAddressAlignment) != 0) {
            return memory_detail::make_error(MemoryError::address_misaligned,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }
        if ((descriptor.stride_bytes % kRawAddressAlignment) != 0) {
            return memory_detail::make_error(MemoryError::stride_misaligned,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }

        if (!memory_detail::checked_mul_u64(descriptor.width, 2u, row_bytes)) {
            return memory_detail::make_error(MemoryError::transfer_too_large,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }
        if (descriptor.stride_bytes < row_bytes) {
            return memory_detail::make_error(MemoryError::stride_too_small,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }
        if (row_bytes > std::numeric_limits<unsigned int>::max()) {
            return memory_detail::make_error(MemoryError::transfer_too_large,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }
        if (!memory_detail::checked_frame_span(descriptor.height,
                                               descriptor.stride_bytes,
                                               row_bytes,
                                               span_bytes)) {
            return memory_detail::make_error(MemoryError::address_overflow,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }
        if (descriptor.size_bytes < span_bytes) {
            return memory_detail::make_error(MemoryError::size_too_small,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }

        std::uint64_t used_end = 0;
        std::uint64_t declared_end = 0;
        if (!memory_detail::checked_add_u64(descriptor.address,
                                            span_bytes,
                                            used_end) ||
            !memory_detail::checked_add_u64(descriptor.address,
                                            descriptor.size_bytes,
                                            declared_end)) {
            return memory_detail::make_error(MemoryError::address_overflow,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }

        if (!memory_detail::checked_mul_size(descriptor.width,
                                             descriptor.height,
                                             pixel_count)) {
            return memory_detail::make_error(MemoryError::transfer_too_large,
                                             MemoryPlane::raw,
                                             descriptor.address);
        }

        return MemoryIoResult {};
    }

    static MemoryIoResult validate_plane(const I420PlaneDescriptor& descriptor,
                                         std::uint32_t row_bytes,
                                         std::uint32_t rows,
                                         MemoryPlane plane,
                                         PlaneLayout& layout)
    {
        if ((descriptor.address % kI420AddressAlignment) != 0) {
            return memory_detail::make_error(MemoryError::address_misaligned,
                                             plane,
                                             descriptor.address);
        }
        if (descriptor.stride_bytes < row_bytes) {
            return memory_detail::make_error(MemoryError::stride_too_small,
                                             plane,
                                             descriptor.address);
        }

        std::uint64_t span_bytes = 0;
        if (!memory_detail::checked_frame_span(rows,
                                               descriptor.stride_bytes,
                                               row_bytes,
                                               span_bytes)) {
            return memory_detail::make_error(MemoryError::address_overflow,
                                             plane,
                                             descriptor.address);
        }
        if (descriptor.size_bytes < span_bytes) {
            return memory_detail::make_error(MemoryError::size_too_small,
                                             plane,
                                             descriptor.address);
        }

        std::uint64_t used_end = 0;
        std::uint64_t declared_end = 0;
        if (!memory_detail::checked_add_u64(descriptor.address,
                                            span_bytes,
                                            used_end) ||
            !memory_detail::checked_add_u64(descriptor.address,
                                            descriptor.size_bytes,
                                            declared_end)) {
            return memory_detail::make_error(MemoryError::address_overflow,
                                             plane,
                                             descriptor.address);
        }

        layout.descriptor = &descriptor;
        layout.plane = plane;
        layout.row_bytes = row_bytes;
        layout.rows = rows;
        layout.begin = descriptor.address;
        layout.end = used_end;
        return MemoryIoResult {};
    }

    MemoryIoResult write_plane(const PlaneLayout& layout,
                               const std::vector<std::uint8_t>& source)
    {
        for (std::uint32_t row = 0; row < layout.rows; ++row) {
            std::uint64_t row_offset = 0;
            std::uint64_t row_address = 0;
            if (!memory_detail::checked_mul_u64(row,
                                                layout.descriptor->stride_bytes,
                                                row_offset) ||
                !memory_detail::checked_add_u64(layout.descriptor->address,
                                                row_offset,
                                                row_address)) {
                return record_error(memory_detail::make_error(
                    MemoryError::address_overflow,
                    layout.plane,
                    layout.descriptor->address,
                    row));
            }

            const std::size_t source_offset =
                static_cast<std::size_t>(row) * layout.row_bytes;
            MemoryIoResult access = transport(
                tlm::TLM_WRITE_COMMAND,
                row_address,
                const_cast<unsigned char*>(source.data() + source_offset),
                layout.row_bytes,
                layout.plane,
                row);
            if (!access) {
                return access;
            }
        }

        return MemoryIoResult {};
    }

    MemoryIoResult transport(tlm::tlm_command command,
                             std::uint64_t address,
                             unsigned char* data,
                             unsigned int length,
                             MemoryPlane plane,
                             std::uint32_t row)
    {
        tlm::tlm_generic_payload transaction;
        transaction.set_command(command);
        transaction.set_address(address);
        transaction.set_data_ptr(data);
        transaction.set_data_length(length);
        transaction.set_streaming_width(length);
        transaction.set_byte_enable_ptr(nullptr);
        transaction.set_byte_enable_length(0);
        transaction.set_dmi_allowed(false);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        if (command == tlm::TLM_READ_COMMAND) {
            ++counters_.read_transactions;
        } else {
            ++counters_.write_transactions;
        }

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        try {
            memory_socket->b_transport(transaction, delay);
        } catch (...) {
            return record_error(memory_detail::make_error(
                MemoryError::transport_error,
                plane,
                address,
                row,
                tlm::TLM_GENERIC_ERROR_RESPONSE));
        }

        if (delay != sc_core::SC_ZERO_TIME) {
            sc_core::wait(delay);
        }

        if (transaction.get_response_status() != tlm::TLM_OK_RESPONSE) {
            return record_error(memory_detail::make_error(
                MemoryError::transport_error,
                plane,
                address,
                row,
                transaction.get_response_status()));
        }

        if (command == tlm::TLM_READ_COMMAND) {
            counters_.read_bytes += length;
        } else {
            counters_.write_bytes += length;
        }

        return MemoryIoResult {};
    }
};

} // namespace cdc::components::isp
