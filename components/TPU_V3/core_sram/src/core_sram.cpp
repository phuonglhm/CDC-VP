// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/sram/core_sram.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "tpu_v3/address_map.h"

namespace cdc::components::tpu_v3::sram {

namespace {

[[noreturn]] void reject(const std::string& context, const std::string& field,
                         const std::string& value, const std::string& expected)
{
    std::ostringstream message;
    message << "tpu_v3::core_sram: " << context << '.' << field << " = "
            << value << " is not accepted; " << expected;
    throw std::invalid_argument(message.str());
}

std::string human_bytes(std::uint64_t bytes)
{
    static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    std::size_t unit = 0;
    std::uint64_t scaled = bytes;
    while (scaled >= 1024 && (scaled % 1024) == 0 && unit + 1 < 5) {
        scaled /= 1024;
        ++unit;
    }
    return std::to_string(scaled) + ' ' + units[unit];
}

} // namespace

void core_sram_config::validate(const std::string& context) const
{
    if (window_bytes == 0 || (window_bytes & (window_bytes - 1)) != 0) {
        reject(context, "window_bytes", std::to_string(window_bytes),
               "it must be a non-zero power of two");
    }
    if (capacity_bytes == 0 || (capacity_bytes & (capacity_bytes - 1)) != 0) {
        reject(context, "capacity_bytes", std::to_string(capacity_bytes),
               "it must be a non-zero power of two");
    }
    if (capacity_bytes > window_bytes) {
        reject(context, "capacity_bytes", std::to_string(capacity_bytes),
               "the backed capacity cannot exceed the decoded window ("
                   + std::to_string(window_bytes)
                   + "). The window may be larger than the capacity, never "
                     "the other way round");
    }
    if (base_address % window_bytes != 0) {
        reject(context, "base_address", std::to_string(base_address),
               "the region must be naturally aligned to its window (plan §12 "
               "rule 2)");
    }
    // Every RV32-visible address is below 4 GiB (plan §12 rule 1), and the
    // arithmetic that checks it is done in 64 bits (rule 5).
    if (base_address > 0x1'0000'0000ull - window_bytes) {
        reject(context, "base_address", std::to_string(base_address),
               "the window would end above the 4 GiB RV32 boundary");
    }
}

core_sram::core_sram(sc_core::sc_module_name name, core_sram_config config)
    : sc_core::sc_module(name)
    , config_((config.validate(std::string(name)), std::move(config)))
    , storage_(config_.capacity_bytes)
{
}

bool core_sram::in_window(std::uint64_t address,
                          std::uint64_t length) const noexcept
{
    return address_map::contains(config_.base_address, config_.window_bytes,
                                 address, length);
}

bool core_sram::is_backed(std::uint64_t address,
                          std::uint64_t length) const noexcept
{
    return address_map::contains(config_.base_address, config_.capacity_bytes,
                                 address, length);
}

neo_status core_sram::classify(std::uint64_t address,
                               std::uint32_t size) const noexcept
{
    if (size == 0 || size > neo_max_transfer_bytes) {
        return neo_status::size_error;
    }
    if (!in_window(address, size)) {
        return neo_status::decode_error;
    }
    if (!is_backed(address, size)) {
        // Inside the window, above the capacity. Reported as its own status
        // and never aliased down into valid storage (D6): aliasing would turn
        // a firmware pointer bug into silent corruption that changes symptom
        // with the configured SRAM size.
        return neo_status::capacity_error;
    }
    return neo_status::ok;
}

neo_status core_sram::perform(std::uint64_t address, std::uint32_t size,
                              const unsigned char* in, unsigned char* out,
                              const unsigned char* strobes, bool counted)
{
    const neo_status status = classify(address, size);
    if (status != neo_status::ok) {
        if (counted) {
            ++error_count_;
        }
        return status;
    }

    const std::uint64_t offset = address - config_.base_address;
    const bool writing = in != nullptr;

    // `classify()` already proved the range is inside the capacity, so these
    // cannot fail; checking anyway costs nothing and keeps the two bounds
    // rules from silently drifting apart.
    const bool moved = writing ? storage_.write(offset, size, in, strobes)
                               : storage_.read(offset, size, out, strobes);
    if (!moved) {
        if (counted) {
            ++error_count_;
        }
        return neo_status::capacity_error;
    }

    // Bytes the access actually moved, not bytes it named. A masked write
    // transfers fewer than `size`, and a counter that ignored the mask would
    // report bandwidth the model never carried — decision record D7 requires
    // each counter to mean what its name says.
    const std::uint32_t moved_bytes = enabled_byte_count(strobes, size);

    if (counted) {
        if (writing) {
            ++write_accesses_;
            bytes_written_ += moved_bytes;
        } else {
            ++read_accesses_;
            bytes_read_ += moved_bytes;
        }
    } else if (writing) {
        debug_bytes_written_ += moved_bytes;
    }
    return neo_status::ok;
}

neo_status core_sram::read(std::uint64_t address, std::uint32_t size,
                           unsigned char* out, const unsigned char* strobes)
{
    return perform(address, size, nullptr, out, strobes, /*counted=*/true);
}

neo_status core_sram::write(std::uint64_t address, std::uint32_t size,
                            const unsigned char* in,
                            const unsigned char* strobes)
{
    return perform(address, size, in, nullptr, strobes, /*counted=*/true);
}

neo_status core_sram::debug_read(std::uint64_t address, std::uint32_t size,
                                 unsigned char* out,
                                 const unsigned char* strobes)
{
    return perform(address, size, nullptr, out, strobes, /*counted=*/false);
}

neo_status core_sram::debug_write(std::uint64_t address, std::uint32_t size,
                                  const unsigned char* in,
                                  const unsigned char* strobes)
{
    return perform(address, size, in, nullptr, strobes, /*counted=*/false);
}

void core_sram::reset()
{
    storage_.reset();
    read_accesses_ = 0;
    write_accesses_ = 0;
    bytes_read_ = 0;
    bytes_written_ = 0;
    error_count_ = 0;
    debug_bytes_written_ = 0;
    // The peak backing high-water mark deliberately survives; see the header.
}

std::string core_sram::report() const
{
    std::ostringstream out;
    out << name() << '\n'
        << "  base            : 0x" << std::hex << std::setw(8)
        << std::setfill('0') << config_.base_address << std::dec
        << std::setfill(' ') << '\n'
        << "  decoded window  : " << human_bytes(config_.window_bytes) << '\n'
        << "  backed capacity : " << human_bytes(config_.capacity_bytes)
        << (config_.capacity_bytes < config_.window_bytes
                ? "  (above this the SRAM reports an error, never an alias)"
                : "  (the full window)")
        << '\n'
        << "  host backing    : " << human_bytes(allocated_backing_bytes())
        << " allocated, " << human_bytes(peak_allocated_backing_bytes())
        << " peak, " << allocated_page_count() << " pages of "
        << human_bytes(sparse_memory::page_size) << '\n'
        << "  traffic         : " << read_accesses_ << " reads / "
        << write_accesses_ << " writes, " << bytes_read_ << " bytes read, "
        << bytes_written_ << " bytes written, " << error_count_ << " errors\n"
        << "  debug traffic   : " << debug_bytes_written_
        << " bytes written (not workload traffic; excluded from the counters "
           "above)\n";
    return out.str();
}

} // namespace cdc::components::tpu_v3::sram
