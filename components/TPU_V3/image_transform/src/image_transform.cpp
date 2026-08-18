// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/transform/image_transform.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "tpu_v3/transform/transform_provenance.h"

namespace cdc::components::tpu_v3::transform {
namespace {

constexpr unsigned kMmioBytes = 4;
constexpr std::uint64_t kRv32Limit = 0x1'0000'0000ull;

std::uint32_t low32(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value);
}

std::uint32_t high32(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value >> 32);
}

std::string hex(std::uint64_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

bool all_bytes_enabled(const tlm::tlm_generic_payload& trans)
{
    const unsigned char* enables = trans.get_byte_enable_ptr();
    if (enables == nullptr) {
        return true;
    }
    const unsigned length = trans.get_byte_enable_length();
    if (length == 0) {
        return false;
    }
    for (unsigned i = 0; i < length; ++i) {
        if (enables[i] != TLM_BYTE_ENABLED) {
            return false;
        }
    }
    return true;
}

bool span_inside(std::uint64_t base, std::uint64_t window,
                 std::uint64_t address, std::uint64_t length) noexcept
{
    return address_map::contains(base, window, address, length);
}

bool overlap(std::uint64_t a, std::uint64_t a_length,
             std::uint64_t b, std::uint64_t b_length) noexcept
{
    if (a <= b) {
        return b - a < a_length;
    }
    return a - b < b_length;
}

} // namespace

void image_transform_config::validate(const std::string& context) const
{
    const auto reject = [&](const char* field, const std::string& value,
                            const std::string& expected) {
        throw std::invalid_argument("tpu_v3::image_transform: " + context + '.'
                                    + field + " = " + value
                                    + " is not accepted; " + expected);
    };
    if (control_size < reg::implemented_end
        || (control_size & (control_size - 1)) != 0) {
        reject("control_size", std::to_string(control_size),
               "it must be a power of two large enough for the register map");
    }
    if (control_base % control_size != 0) {
        reject("control_base", hex(control_base),
               "the AXI4-Lite window must be naturally aligned");
    }
    if (sram_window == 0 || (sram_window & (sram_window - 1)) != 0) {
        reject("sram_window", std::to_string(sram_window),
               "it must be a non-zero power of two");
    }
    if (sram_base % sram_window != 0) {
        reject("sram_base", hex(sram_base),
               "the core SRAM window must be naturally aligned");
    }
    if (sram_base > kRv32Limit - sram_window) {
        reject("sram_base", hex(sram_base),
               "the SRAM window must remain below the RV32 4 GiB limit");
    }
}

image_transform::image_transform(sc_core::sc_module_name name,
                                 image_transform_config config)
    : sc_core::sc_module(name)
    , config_((config.validate(std::string(name)), std::move(config)))
{
    control.register_b_transport(this, &image_transform::b_transport);
    control.register_transport_dbg(this, &image_transform::transport_dbg);

    SC_THREAD(worker);
    SC_METHOD(drive_irq);
    sensitive << irq_event_;
}

void image_transform::update_irq()
{
    // Keep one writer process for sc_signal, but wake it immediately. The
    // resulting signal update is visible after one delta, matching DMA/SA and
    // avoiding an unnecessary second delta after completion or W1C.
    irq_event_.notify();
}

void image_transform::drive_irq()
{
    const bool pending =
        (status_ & (status_bit::done | status_bit::error)) != 0;
    irq.write(pending
              && (irq_enable_ & irq_enable_bit::completion) != 0);
}

void image_transform::reset()
{
    const bool was_busy = busy();
    ++generation_;
    start_pending_ = false;
    status_ = 0;
    error_cause_ = error_cause::none;
    irq_enable_ = 0;
    programmed_ = {};
    active_ = {};
    ++traffic_epoch_;
    local_requests_ = 0;
    local_bytes_ = 0;
    if (was_busy) {
        ++abort_count_;
    } else {
        bytes_done_ = 0;
    }
    start_event_.notify(sc_core::SC_ZERO_TIME);
    update_irq();
}

bool image_transform::check_control_rules(tlm::tlm_generic_payload& trans)
{
    const auto command = trans.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_data_length() != kMmioBytes
        || trans.get_address() % kMmioBytes != 0) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    const unsigned streaming = trans.get_streaming_width();
    if ((streaming != 0 && streaming < trans.get_data_length())
        || !all_bytes_enabled(trans)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_data_ptr() == nullptr) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return false;
    }
    const std::uint64_t address = trans.get_address();
    if (address < config_.control_base
        || address - config_.control_base >= config_.control_size
        || config_.control_size - (address - config_.control_base)
            < kMmioBytes) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    return true;
}

std::uint32_t image_transform::read_register(std::uint64_t offset) const
{
    matrix_shape shape;
    if (derive_shape(programmed_, shape) != error_cause::none) {
        shape = {};
    }
    switch (offset) {
    case reg::id: return identity;
    case reg::version: return model_version;
    case reg::capability: return capability_bit::value;
    case reg::control: return 0;
    case reg::status: return status_;
    case reg::operation: return programmed_.operation;
    case reg::src_addr_lo: return low32(programmed_.source);
    case reg::src_addr_hi: return high32(programmed_.source);
    case reg::dst_addr_lo: return low32(programmed_.destination);
    case reg::dst_addr_hi: return high32(programmed_.destination);
    case reg::input_channels: return programmed_.channels;
    case reg::input_height: return programmed_.input_height;
    case reg::input_width: return programmed_.input_width;
    case reg::kernel_height: return programmed_.kernel_height;
    case reg::kernel_width: return programmed_.kernel_width;
    case reg::stride_height: return programmed_.stride_height;
    case reg::stride_width: return programmed_.stride_width;
    case reg::dilation_height: return programmed_.dilation_height;
    case reg::dilation_width: return programmed_.dilation_width;
    case reg::pad_top: return programmed_.pad_top;
    case reg::pad_left: return programmed_.pad_left;
    case reg::pad_bottom: return programmed_.pad_bottom;
    case reg::pad_right: return programmed_.pad_right;
    case reg::datatype: return programmed_.datatype;
    case reg::irq_enable: return irq_enable_;
    case reg::error_cause: return static_cast<std::uint32_t>(error_cause_);
    case reg::output_height: return shape.output_height;
    case reg::output_width: return shape.output_width;
    case reg::matrix_rows: return low32(shape.rows);
    case reg::matrix_columns: return low32(shape.columns);
    case reg::bytes_done_lo: return low32(bytes_done_);
    case reg::bytes_done_hi: return high32(bytes_done_);
    case reg::local_requests: return low32(local_requests_);
    case reg::local_bytes_lo: return low32(local_bytes_);
    case reg::local_bytes_hi: return high32(local_bytes_);
    case reg::job_count: return low32(job_count_);
    case reg::error_count: return low32(error_count_);
    case reg::abort_count: return low32(abort_count_);
    case reg::overrun_count: return low32(overrun_count_);
    case reg::source_tag: return provenance::source_tag;
    default: return 0;
    }
}

bool image_transform::write_register(std::uint64_t offset,
                                     std::uint32_t value)
{
    const bool descriptor_register =
        offset >= reg::operation && offset <= reg::datatype;
    if (descriptor_register && busy()) {
        return false;
    }

    switch (offset) {
    case reg::control: {
        const bool start = (value & control_bit::start) != 0;
        const bool abort = (value & control_bit::abort) != 0;
        if (start && abort) {
            return false;
        }
        if (abort) {
            if (busy()) {
                ++generation_;
                start_pending_ = false;
                status_ &= ~status_bit::busy;
                status_ |= status_bit::aborted;
                error_cause_ = error_cause::none;
                ++abort_count_;
                start_event_.notify(sc_core::SC_ZERO_TIME);
                update_irq();
            }
            return true;
        }
        if (start) {
            if (busy()) {
                ++overrun_count_;
                return false;
            }
            active_ = programmed_;
            active_generation_ = ++generation_;
            bytes_owner_generation_ = active_generation_;
            bytes_done_ = 0;
            error_cause_ = error_cause::none;
            status_ &= ~(status_bit::done | status_bit::error
                         | status_bit::aborted);
            status_ |= status_bit::busy;
            ++job_count_;
            start_pending_ = true;
            start_event_.notify(sc_core::SC_ZERO_TIME);
            update_irq();
        }
        return true;
    }
    case reg::status:
        status_ &= ~(value & status_bit::w1c_mask);
        update_irq();
        return true;
    case reg::operation: programmed_.operation = value; return true;
    case reg::src_addr_lo:
        programmed_.source =
            (programmed_.source & 0xFFFF'FFFF'0000'0000ull) | value;
        return true;
    case reg::src_addr_hi:
        programmed_.source = (programmed_.source & 0xFFFF'FFFFull)
            | (std::uint64_t(value) << 32);
        return true;
    case reg::dst_addr_lo:
        programmed_.destination =
            (programmed_.destination & 0xFFFF'FFFF'0000'0000ull) | value;
        return true;
    case reg::dst_addr_hi:
        programmed_.destination = (programmed_.destination & 0xFFFF'FFFFull)
            | (std::uint64_t(value) << 32);
        return true;
    case reg::input_channels: programmed_.channels = value; return true;
    case reg::input_height: programmed_.input_height = value; return true;
    case reg::input_width: programmed_.input_width = value; return true;
    case reg::kernel_height: programmed_.kernel_height = value; return true;
    case reg::kernel_width: programmed_.kernel_width = value; return true;
    case reg::stride_height: programmed_.stride_height = value; return true;
    case reg::stride_width: programmed_.stride_width = value; return true;
    case reg::dilation_height:
        programmed_.dilation_height = value; return true;
    case reg::dilation_width:
        programmed_.dilation_width = value; return true;
    case reg::pad_top: programmed_.pad_top = value; return true;
    case reg::pad_left: programmed_.pad_left = value; return true;
    case reg::pad_bottom: programmed_.pad_bottom = value; return true;
    case reg::pad_right: programmed_.pad_right = value; return true;
    case reg::datatype: programmed_.datatype = value; return true;
    case reg::irq_enable:
        irq_enable_ = value & irq_enable_bit::writable_mask;
        update_irq();
        return true;
    default:
        // Reserved and read-only registers drop writes with an OK response.
        return true;
    }
}

void image_transform::b_transport(tlm::tlm_generic_payload& trans,
                                  sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    if (!check_control_rules(trans)) {
        return;
    }
    const std::uint64_t offset = trans.get_address() - config_.control_base;
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        const std::uint32_t value = read_register(offset);
        std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }
    std::uint32_t value = 0;
    std::memcpy(&value, trans.get_data_ptr(), sizeof(value));
    trans.set_response_status(write_register(offset, value)
                                  ? tlm::TLM_OK_RESPONSE
                                  : tlm::TLM_GENERIC_ERROR_RESPONSE);
    (void)delay;
}

unsigned int image_transform::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const auto command = trans.get_command();
    const std::uint64_t address = trans.get_address();
    if ((command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND)
        || trans.get_data_length() != kMmioBytes
        || address % kMmioBytes != 0 || trans.get_data_ptr() == nullptr
        || address < config_.control_base
        || address - config_.control_base >= config_.control_size
        || config_.control_size - (address - config_.control_base)
            < kMmioBytes) {
        return 0;
    }
    if (command == tlm::TLM_WRITE_COMMAND) {
        return kMmioBytes;
    }
    const std::uint32_t value =
        read_register(address - config_.control_base);
    std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
    return kMmioBytes;
}

error_cause image_transform::validate_descriptor(
    const descriptor& work, matrix_shape& shape) const noexcept
{
    error_cause error = derive_shape(work, shape);
    if (error != error_cause::none) {
        return error;
    }
    if (shape.rows > std::numeric_limits<std::uint32_t>::max()
        || shape.columns > std::numeric_limits<std::uint32_t>::max()
        || shape.source_bytes > std::numeric_limits<std::size_t>::max()
        || shape.destination_bytes > std::numeric_limits<std::size_t>::max()) {
        return error_cause::arithmetic_overflow;
    }
    if (!span_inside(config_.sram_base, config_.sram_window, work.source,
                     shape.source_bytes)) {
        return error_cause::source_out_of_range;
    }
    if (!span_inside(config_.sram_base, config_.sram_window, work.destination,
                     shape.destination_bytes)) {
        return error_cause::destination_out_of_range;
    }
    if (overlap(work.source, shape.source_bytes, work.destination,
                shape.destination_bytes)) {
        return error_cause::region_overlap;
    }
    return error_cause::none;
}

bool image_transform::local_access(sram::neo_command command,
                                   std::uint64_t address,
                                   std::uint32_t size, unsigned char* data,
                                   std::uint64_t generation,
                                   std::uint64_t traffic_epoch,
                                   bool destination, error_cause& cause)
{
    if (superseded(generation)) {
        return false;
    }
    sram::neo_local_request request;
    request.requester = sram::neo_requester::transform;
    request.command = command;
    request.address = address;
    request.size = size;
    request.data = data;

    sram::neo_local_response response;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (traffic_epoch == traffic_epoch_) {
        ++local_requests_;
    }
    local->b_access(request, response, delay);
    if (traffic_epoch == traffic_epoch_) {
        local_bytes_ += response.bytes;
    }
    if (destination && bytes_owner_generation_ == generation) {
        bytes_done_ += response.bytes;
    }
    if (delay > sc_core::SC_ZERO_TIME) {
        wait(delay);
    }
    if (superseded(generation)) {
        return false;
    }
    if (response.status != sram::neo_status::ok || response.bytes != size) {
        cause = command == sram::neo_command::read
            ? error_cause::local_read : error_cause::local_write;
        return false;
    }
    return true;
}

error_cause image_transform::run_im2col(const descriptor& work,
                                        std::uint64_t generation,
                                        std::uint64_t traffic_epoch)
{
    matrix_shape shape;
    error_cause cause = validate_descriptor(work, shape);
    if (cause != error_cause::none) {
        return cause;
    }

    input_staging_.assign(static_cast<std::size_t>(shape.source_bytes), 0);
    for (std::uint64_t offset = 0; offset < shape.source_bytes;) {
        const auto chunk = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(shape.source_bytes - offset,
                                    sram::neo_max_transfer_bytes));
        if (!local_access(sram::neo_command::read, work.source + offset, chunk,
                          input_staging_.data() + offset, generation,
                          traffic_epoch, false, cause)) {
            return superseded(generation) ? error_cause::none : cause;
        }
        offset += chunk;
    }

    output_row_.resize(static_cast<std::size_t>(shape.columns));
    for (std::uint64_t row = 0; row < shape.rows; ++row) {
        for (std::uint64_t column = 0; column < shape.columns; ++column) {
            output_row_[static_cast<std::size_t>(column)] =
                input_staging_[static_cast<std::size_t>(
                    source_element_index(work, shape, row, column))];
        }
        for (std::uint64_t offset = 0; offset < shape.columns;) {
            const auto chunk = static_cast<std::uint32_t>(
                std::min<std::uint64_t>(shape.columns - offset,
                                        sram::neo_max_transfer_bytes));
            const std::uint64_t destination =
                work.destination + row * shape.columns + offset;
            if (!local_access(sram::neo_command::write, destination, chunk,
                              output_row_.data() + offset, generation,
                              traffic_epoch, true, cause)) {
                return superseded(generation) ? error_cause::none : cause;
            }
            offset += chunk;
        }
    }
    return error_cause::none;
}

void image_transform::finish(std::uint32_t bit, error_cause cause,
                             std::uint64_t generation)
{
    if (superseded(generation)) {
        return;
    }
    status_ &= ~status_bit::busy;
    status_ |= bit;
    error_cause_ = cause;
    if (bit == status_bit::error) {
        ++error_count_;
    }
    update_irq();
}

void image_transform::worker()
{
    for (;;) {
        while (!start_pending_) {
            wait(start_event_);
        }
        const descriptor work = active_;
        const std::uint64_t generation = active_generation_;
        const std::uint64_t traffic_epoch = traffic_epoch_;
        start_pending_ = false;

        const error_cause cause = run_im2col(work, generation, traffic_epoch);
        if (superseded(generation)) {
            continue;
        }
        finish(cause == error_cause::none ? status_bit::done
                                         : status_bit::error,
               cause, generation);
    }
}

std::string image_transform::report() const
{
    matrix_shape shape;
    const error_cause descriptor_status = derive_shape(programmed_, shape);
    std::ostringstream out;
    out << "ImageTransform\n"
        << "  source             : " << provenance::source_revision << '\n'
        << "  Im2Col             : available\n"
        << "  Col2Im             : unavailable (no NPU-team source)\n"
        << "  layout             : CHW -> [OH*OW][C*KH*KW], row-major\n"
        << "  datatype           : INT8\n"
        << "  padding            : unsupported (all fields must be zero)\n"
        << "  descriptor         : " << to_string(descriptor_status) << '\n'
        << "  output matrix      : " << shape.rows << 'x' << shape.columns
        << '\n'
        << "  local requests     : " << local_requests_ << '\n'
        << "  local bytes        : " << local_bytes_ << '\n'
        << "  committed bytes    : " << bytes_done_ << '\n';
    return out.str();
}

} // namespace cdc::components::tpu_v3::transform
