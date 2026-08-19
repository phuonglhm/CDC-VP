// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/sauria/sa_control.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#include "tpu_v3/sauria/sa_registers.h"

namespace cdc::components::tpu_v3::sauria {
namespace {

constexpr unsigned int kWordBytes = 4;

std::uint32_t low32(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value);
}

std::uint32_t high32(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value >> 32);
}

std::uint32_t saturate32(std::uint64_t value)
{
    return value > std::numeric_limits<std::uint32_t>::max()
        ? std::numeric_limits<std::uint32_t>::max()
        : static_cast<std::uint32_t>(value);
}

bool all_bytes_enabled(const tlm::tlm_generic_payload& trans)
{
    const unsigned char* enables = trans.get_byte_enable_ptr();
    if (enables == nullptr) {
        return true;
    }
    if (trans.get_byte_enable_length() != trans.get_data_length()) {
        return false;
    }
    for (unsigned int i = 0; i < trans.get_data_length(); ++i) {
        if (enables[i] != TLM_BYTE_ENABLED) {
            return false;
        }
    }
    return true;
}

} // namespace

void sa_control_config::validate(const std::string& context) const
{
    const auto reject = [&](const std::string& why) {
        throw std::invalid_argument(context + ": invalid SA control config: "
                                    + why);
    };
    if (control_size != address_map::sa_control_size) {
        reject("control_size must equal the architectural 64 KiB window");
    }
    if (control_base % control_size != 0) {
        reject("control_base must be aligned to the control window");
    }
    if (control_base > std::numeric_limits<std::uint64_t>::max() - control_size) {
        reject("control window overflows the address space");
    }
}

sa_control::sa_control(sc_core::sc_module_name name, sa_control_config config,
                       sauria_matrix_if& engine)
    : sc_core::sc_module(name)
    , config_((config.validate(std::string(name)), std::move(config)))
    , engine_(engine)
{
    programmed_.datatype = std::numeric_limits<std::uint32_t>::max();
    control.register_b_transport(this, &sa_control::b_transport);
    control.register_transport_dbg(this, &sa_control::transport_dbg);

    SC_METHOD(observe_completion);
    sensitive << i_clk.pos();

    SC_METHOD(drive_irq);
    sensitive << irq_event_;
    dont_initialize();

    irq.initialize(false);
}

void sa_control::drive_irq()
{
    const bool pending =
        (status_ & (status_bit::done | status_bit::error)) != 0;
    irq.write(pending
              && (irq_enable_ & irq_enable_bit::completion) != 0);
}

void sa_control::update_irq()
{
    // Keep one writer process for the `sc_signal`, but wake it immediately.
    //
    // Calling `drive_irq()` straight from here was correct in the standalone
    // bench and wrong in a NEO-CORE: the register paths run in whichever
    // process issued the MMIO — the hart's thread once a core is composed —
    // while `observe_completion` runs as a clocked method, so the signal
    // acquired two drivers and SystemC refused the elaboration at the first
    // completion. `neo_dma` and `image_transform` already resolve it this way,
    // and notifying rather than deferring keeps the level visible after one
    // delta, which is the convention Phase 6 settled on.
    irq_event_.notify();
}

void sa_control::snapshot_engine()
{
    c_bytes_done_ = engine_.committed_bytes();
    local_requests_ = engine_.local_requests();
    local_bytes_ = engine_.local_bytes();
    timing_ = engine_.last_timing();
}

void sa_control::observe_completion()
{
    // ABORT/reset clear the firmware-visible BUSY bit immediately, while the
    // adapter may still be unwinding from a blocking native transaction. A
    // successful response can therefore add committed C bytes after the first
    // snapshot. Keep reconciling the abandoned owner's registers until a new
    // START claims C_BYTES_DONE. Do this before the BUSY early-return that used
    // to make the first, possibly short snapshot permanent.
    if (abandoned_accounting_open_) {
        snapshot_engine();
    }
    if ((status_ & status_bit::busy) == 0 || engine_.busy()) {
        return;
    }

    status_ &= ~status_bit::busy;
    snapshot_engine();
    abandoned_accounting_open_ = false;
    error_cause_ = engine_.last_error();
    if (error_cause_ == error_cause::none) {
        status_ |= status_bit::done;
    } else if (error_cause_ == error_cause::aborted) {
        status_ |= status_bit::aborted;
        ++abort_count_;
    } else {
        status_ |= status_bit::error;
        ++error_count_;
    }
    update_irq();
}

void sa_control::reset()
{
    const bool was_busy = (status_ & status_bit::busy) != 0;
    engine_.reset();
    if (was_busy) {
        abandoned_accounting_open_ = true;
        snapshot_engine();
    } else {
        abandoned_accounting_open_ = false;
        c_bytes_done_ = 0;
        local_requests_ = 0;
        local_bytes_ = 0;
        timing_ = {};
    }
    status_ = 0;
    irq_enable_ = 0;
    error_cause_ = error_cause::none;
    programmed_ = {};
    programmed_.datatype = std::numeric_limits<std::uint32_t>::max();
    update_irq();
}

bool sa_control::check_control_rules(tlm::tlm_generic_payload& trans)
{
    const auto command = trans.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }
    if (trans.get_data_length() != kWordBytes
        || trans.get_address() % kWordBytes != 0) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    const unsigned int streaming = trans.get_streaming_width();
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
            < kWordBytes) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return false;
    }
    return true;
}

std::uint32_t sa_control::read_register(std::uint64_t offset) const
{
    const engine_identity identity = engine_.identity();
    // A register read must not have to wait for the next SA clock edge to see a
    // late native response. These values remain owned by the abandoned job
    // until START transfers ownership; W1C of ABORTED does not close it.
    const std::uint64_t visible_c_bytes = abandoned_accounting_open_
        ? engine_.committed_bytes() : c_bytes_done_;
    const std::uint64_t visible_local_requests = abandoned_accounting_open_
        ? engine_.local_requests() : local_requests_;
    const std::uint64_t visible_local_bytes = abandoned_accounting_open_
        ? engine_.local_bytes() : local_bytes_;
    const sauria_matrix_if::timing visible_timing = abandoned_accounting_open_
        ? engine_.last_timing() : timing_;
    switch (offset) {
    case reg::id: return identity_value;
    case reg::version: return model_version;
    case reg::control: return 0;
    case reg::status: return status_;
    case reg::dim_m: return programmed_.m;
    case reg::dim_n: return programmed_.n;
    case reg::dim_k: return programmed_.k;
    case reg::a_addr_lo: return low32(programmed_.a_address);
    case reg::a_addr_hi: return high32(programmed_.a_address);
    case reg::b_addr_lo: return low32(programmed_.b_address);
    case reg::b_addr_hi: return high32(programmed_.b_address);
    case reg::c_addr_lo: return low32(programmed_.c_address);
    case reg::c_addr_hi: return high32(programmed_.c_address);
    case reg::a_stride: return programmed_.a_stride_bytes;
    case reg::b_stride: return programmed_.b_stride_bytes;
    case reg::c_stride: return programmed_.c_stride_bytes;
    case reg::datatype: return programmed_.datatype;
    case reg::irq_enable: return irq_enable_;
    case reg::error_cause: return static_cast<std::uint32_t>(error_cause_);
    case reg::job_count: return low32(job_count_);
    case reg::error_count: return low32(error_count_);
    case reg::abort_count: return low32(abort_count_);
    case reg::c_bytes_done_lo: return low32(visible_c_bytes);
    case reg::c_bytes_done_hi: return high32(visible_c_bytes);
    case reg::prefetch_ns: return saturate32(visible_timing.prefetch_ns);
    case reg::compute_ns: return saturate32(visible_timing.compute_ns);
    case reg::writeback_ns: return saturate32(visible_timing.writeback_ns);
    case reg::local_requests: return low32(visible_local_requests);
    case reg::local_bytes_lo: return low32(visible_local_bytes);
    case reg::local_bytes_hi: return high32(visible_local_bytes);
    case reg::overrun_count: return low32(overrun_count_);
    case reg::geometry: return (identity.rows << 16) | identity.columns;
    case reg::capability: return identity.capability;
    default: return 0;
    }
}

error_cause sa_control::cause_of(submit_status status) noexcept
{
    switch (status) {
    case submit_status::invalid_dimension: return error_cause::zero_dimension;
    case submit_status::dimension_exceeds_array:
    case submit_status::staging_capacity_exceeded:
        return error_cause::dimension_too_large;
    case submit_status::invalid_address: return error_cause::address_out_of_range;
    case submit_status::invalid_stride: return error_cause::stride_too_small;
    case submit_status::region_overlap: return error_cause::region_overlap;
    case submit_status::datatype_unsupported:
        return error_cause::datatype_unsupported;
    case submit_status::busy: return error_cause::overrun;
    case submit_status::accepted:
    case submit_status::accumulation_unsupported:
        return error_cause::none;
    }
    return error_cause::none;
}

bool sa_control::write_register(std::uint64_t offset, std::uint32_t value)
{
    const bool descriptor =
        (offset >= reg::dim_m && offset <= reg::dim_k)
        || (offset >= reg::a_addr_lo && offset <= reg::c_stride)
        || offset == reg::datatype;
    if (descriptor && (status_ & status_bit::busy) != 0) {
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
            if ((status_ & status_bit::busy) != 0) {
                engine_.abort();
                abandoned_accounting_open_ = true;
                status_ &= ~status_bit::busy;
                status_ |= status_bit::aborted;
                ++abort_count_;
                snapshot_engine();
                update_irq();
            }
            return true;
        }
        if (start) {
            if ((status_ & status_bit::busy) != 0) {
                ++overrun_count_;
                return false;
            }
            status_ &= ~(status_bit::done | status_bit::error
                         | status_bit::aborted);
            error_cause_ = error_cause::none;
            // START is the ownership boundary. Capture the abandoned job one
            // last time before engine.submit() changes the engine's owner, then
            // initialise the new job's completion account.
            if (abandoned_accounting_open_) {
                snapshot_engine();
            }
            abandoned_accounting_open_ = false;
            c_bytes_done_ = 0;
            timing_ = {};
            const submit_status submitted = engine_.submit(programmed_);
            if (submitted == submit_status::accepted) {
                status_ |= status_bit::busy;
                ++job_count_;
            } else {
                status_ |= status_bit::error;
                error_cause_ = cause_of(submitted);
                ++error_count_;
            }
            update_irq();
        }
        return true;
    }
    case reg::status:
        status_ &= ~(value & status_bit::w1c_mask);
        update_irq();
        return true;
    case reg::dim_m: programmed_.m = value; return true;
    case reg::dim_n: programmed_.n = value; return true;
    case reg::dim_k: programmed_.k = value; return true;
    case reg::a_addr_lo:
        programmed_.a_address =
            (programmed_.a_address & 0xffff'ffff'0000'0000ull) | value;
        return true;
    case reg::a_addr_hi:
        programmed_.a_address = (programmed_.a_address & 0xffff'ffffull)
            | (static_cast<std::uint64_t>(value) << 32);
        return true;
    case reg::b_addr_lo:
        programmed_.b_address =
            (programmed_.b_address & 0xffff'ffff'0000'0000ull) | value;
        return true;
    case reg::b_addr_hi:
        programmed_.b_address = (programmed_.b_address & 0xffff'ffffull)
            | (static_cast<std::uint64_t>(value) << 32);
        return true;
    case reg::c_addr_lo:
        programmed_.c_address =
            (programmed_.c_address & 0xffff'ffff'0000'0000ull) | value;
        return true;
    case reg::c_addr_hi:
        programmed_.c_address = (programmed_.c_address & 0xffff'ffffull)
            | (static_cast<std::uint64_t>(value) << 32);
        return true;
    case reg::a_stride: programmed_.a_stride_bytes = value; return true;
    case reg::b_stride: programmed_.b_stride_bytes = value; return true;
    case reg::c_stride: programmed_.c_stride_bytes = value; return true;
    case reg::datatype: programmed_.datatype = value; return true;
    case reg::irq_enable:
        irq_enable_ = value & irq_enable_bit::writable_mask;
        update_irq();
        return true;
    default:
        return true;
    }
}

void sa_control::b_transport(tlm::tlm_generic_payload& trans,
                             sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    if (!check_control_rules(trans)) {
        return;
    }
    const std::uint64_t offset = trans.get_address() - config_.control_base;
    unsigned char* data = trans.get_data_ptr();
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        const std::uint32_t value = read_register(offset);
        std::memcpy(data, &value, sizeof(value));
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        return;
    }
    std::uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    trans.set_response_status(write_register(offset, value)
                                  ? tlm::TLM_OK_RESPONSE
                                  : tlm::TLM_GENERIC_ERROR_RESPONSE);
    (void)delay;
}

unsigned int sa_control::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const auto command = trans.get_command();
    const std::uint64_t address = trans.get_address();
    if ((command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND)
        || trans.get_data_length() != kWordBytes
        || address % kWordBytes != 0 || trans.get_data_ptr() == nullptr
        || address < config_.control_base
        || address - config_.control_base >= config_.control_size
        || config_.control_size - (address - config_.control_base)
            < kWordBytes) {
        return 0;
    }
    if (command == tlm::TLM_WRITE_COMMAND) {
        return kWordBytes;
    }
    const std::uint32_t value =
        read_register(address - config_.control_base);
    std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
    return kWordBytes;
}

} // namespace cdc::components::tpu_v3::sauria
