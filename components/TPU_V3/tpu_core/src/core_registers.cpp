// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/core/core_registers.h"

#include <cstring>
#include <sstream>
#include <stdexcept>

namespace cdc::components::tpu_v3::core {

namespace {

constexpr std::uint32_t kMmioWidth = 4;

/// True when every byte the pattern covers is enabled.
///
/// TLM lets `byte_enable_length` be shorter than `data_length`, in which case
/// the pattern repeats. Checking the pattern rather than the first four bytes
/// is what makes a repeating half-enabled pattern fail too.
bool all_bytes_enabled(const tlm::tlm_generic_payload& trans)
{
    const unsigned char* enables = trans.get_byte_enable_ptr();
    if (enables == nullptr) {
        return true;
    }
    const unsigned int length = trans.get_byte_enable_length();
    if (length == 0) {
        return false;
    }
    for (unsigned int i = 0; i < length; ++i) {
        if (enables[i] != TLM_BYTE_ENABLED) {
            return false;
        }
    }
    return true;
}

} // namespace

const char* to_string(register_block block) noexcept
{
    switch (block) {
    case register_block::core:
        return "core";
    case register_block::sa:
        return "sa";
    case register_block::dma:
        return "dma";
    case register_block::transform:
        return "transform";
    case register_block::counters:
        return "counters";
    case register_block::chip:
        return "chip";
    case register_block::chip_counters:
        return "chip_counters";
    }
    return "unknown";
}

std::uint32_t mmio_register_file::status_value() const noexcept
{
    // Zero `implemented` for the engine windows: Phase 3 routes them, it does
    // not build them, and a window that answered "ready" would be claiming a
    // block that does not exist. Shared by `b_transport` and `transport_dbg`
    // so the two cannot drift apart.
    return (block_ == register_block::core
            || block_ == register_block::counters
            || block_ == register_block::chip
            || block_ == register_block::chip_counters)
        ? status_implemented
        : 0u;
}

mmio_register_file::mmio_register_file(sc_core::sc_module_name name,
                                       register_block block,
                                       std::uint64_t base, std::uint64_t size)
    : sc_core::sc_module(name)
    , socket("socket")
    , block_(block)
    , base_(base)
    , size_(size)
{
    if (size_ < 16 || (size_ & (size_ - 1)) != 0) {
        throw std::invalid_argument(
            "tpu_v3::mmio_register_file[" + std::string(name)
            + "]: size must be a power of two of at least 16 bytes, got "
            + std::to_string(size_));
    }
    if (base_ % 4 != 0) {
        throw std::invalid_argument(
            "tpu_v3::mmio_register_file[" + std::string(name)
            + "]: base must be 4-byte aligned, got "
            + std::to_string(base_));
    }
    socket.register_b_transport(this, &mmio_register_file::b_transport);
    socket.register_transport_dbg(this, &mmio_register_file::transport_dbg);
}

bool mmio_register_file::check_mmio_rules(tlm::tlm_generic_payload& trans)
{
    const auto command = trans.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }

    // 4 bytes only, naturally aligned, never silently widened, narrowed or
    // split (`ADDRESS_MAP.md` §6). This is also what keeps accelerator bulk
    // payload off the control plane: a 64-byte vector access is refused here
    // rather than quietly served four bytes at a time.
    if (trans.get_data_length() != kMmioWidth) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    if (trans.get_address() % kMmioWidth != 0) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    const unsigned int streaming = trans.get_streaming_width();
    if (streaming != 0 && streaming < trans.get_data_length()) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    // A partial-strobe register write is refused, not applied as a
    // read-modify-write: on a register with write-1-to-clear or clear-on-read
    // bits, read-modify-write does the wrong thing silently
    // (`INTERFACE_CONTRACT.md` §2).
    if (!all_bytes_enabled(trans)) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    if (trans.get_data_ptr() == nullptr) {
        throw std::runtime_error(
            "tpu_v3::mmio_register_file[" + std::string(name())
            + "]: a transaction carried a null data pointer -- this is a model "
              "or integration defect, not a guest fault");
    }
    if (trans.get_address() < base_
        || trans.get_address() - base_ >= size_
        || size_ - (trans.get_address() - base_) < kMmioWidth) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        ++protocol_errors_;
        return false;
    }
    return true;
}

void mmio_register_file::b_transport(tlm::tlm_generic_payload& trans,
                                     sc_core::sc_time& delay)
{
    trans.set_dmi_allowed(false);
    if (!check_mmio_rules(trans)) {
        return;
    }

    const std::uint64_t offset = trans.get_address() - base_;
    unsigned char* data = trans.get_data_ptr();

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::uint32_t value = 0;
        switch (offset) {
        case reg_id:
            value = identity();
            break;
        case reg_version:
            value = model_version;
            break;
        case reg_status:
            value = status_value();
            break;
        case reg_scratch:
            value = scratch_;
            break;
        default:
            ++unimplemented_reads_;
            value = 0;
            break;
        }
        std::memcpy(data, &value, sizeof(value));
        ++reads_;
    } else {
        std::uint32_t value = 0;
        std::memcpy(&value, data, sizeof(value));
        if (offset == reg_scratch) {
            scratch_ = value;
        } else {
            // Read-only and unimplemented offsets drop the write. Defined,
            // not erroneous, so a register sweep does not need to know the
            // implementation status of every offset.
            ++dropped_writes_;
        }
        ++writes_;
    }

    // No wait(): a register access is annotated, never waited, or one target
    // would stall every node in the detailed mesh
    // (`INTERFACE_CONTRACT.md` §3). Zero is honest here — the control fabric
    // owns the AXI4-Lite path latency, not the register itself.
    (void)delay;
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int mmio_register_file::transport_dbg(tlm::tlm_generic_payload& trans)
{
    // Debug transport bypasses arbitration and latency but not decode or
    // bounds (`INTERFACE_CONTRACT.md` §8), and updates no counter.
    const std::uint64_t address = trans.get_address();
    const unsigned int length = trans.get_data_length();
    const auto command = trans.get_command();
    // A command that is neither read nor write has no debug meaning. Treating
    // "not a write" as "a read", which the first version did, turns
    // `TLM_IGNORE_COMMAND` into a read that overwrites the caller's buffer.
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        return 0;
    }
    if (address < base_ || address - base_ >= size_
        || size_ - (address - base_) < length || length != kMmioWidth
        || address % kMmioWidth != 0 || trans.get_data_ptr() == nullptr) {
        return 0;
    }

    const std::uint64_t offset = address - base_;
    if (command == tlm::TLM_WRITE_COMMAND) {
        if (offset == reg_scratch) {
            std::memcpy(&scratch_, trans.get_data_ptr(), sizeof(scratch_));
        }
        return length;
    }

    // Every register the normal path answers, answered identically. A debug
    // read that disagreed with `b_transport` would make a loader and the
    // firmware it loaded see different hardware — and `reg_status` is exactly
    // the register a driver would check first.
    std::uint32_t value = 0;
    switch (offset) {
    case reg_id:
        value = identity();
        break;
    case reg_version:
        value = model_version;
        break;
    case reg_status:
        value = status_value();
        break;
    case reg_scratch:
        value = scratch_;
        break;
    default:
        value = 0;
        break;
    }
    std::memcpy(trans.get_data_ptr(), &value, sizeof(value));
    return length;
}

void mmio_register_file::reset()
{
    scratch_ = 0;
    reads_ = 0;
    writes_ = 0;
    unimplemented_reads_ = 0;
    dropped_writes_ = 0;
    protocol_errors_ = 0;
}

std::string mmio_register_file::report() const
{
    std::ostringstream out;
    out << name() << "  block=" << to_string(block_) << "  reads=" << reads_
        << " writes=" << writes_
        << " unimplemented_reads=" << unimplemented_reads_
        << " dropped_writes=" << dropped_writes_
        << " protocol_errors=" << protocol_errors_ << '\n';
    return out.str();
}

} // namespace cdc::components::tpu_v3::core
