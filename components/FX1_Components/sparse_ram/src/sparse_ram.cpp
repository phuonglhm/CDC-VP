#include "fx1/sparse_ram.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace fx1 {
namespace {
// Same encoding as TLM_BYTE_ENABLED; kept local so SparseStorage needs no TLM.
constexpr std::uint8_t kByteEnabled = 0xFF;
}

SparseStorage::SparseStorage(std::uint64_t size, std::uint8_t fill) : size_(size), fill_(fill) {
    if (!size) throw std::invalid_argument("SparseStorage size must be nonzero");
}

void SparseStorage::check(std::uint64_t offset, std::uint64_t length) const {
    if (!contains(offset, length)) throw std::out_of_range("SparseStorage access out of range");
}

const SparseStorage::Page* SparseStorage::find(std::uint64_t page) const {
    const auto it = pages_.find(page);
    return it == pages_.end() ? nullptr : it->second.get();
}

SparseStorage::Page& SparseStorage::touch(std::uint64_t page) {
    auto& slot = pages_[page];
    if (!slot) {
        slot = std::make_unique<Page>();
        slot->fill(fill_);
    }
    return *slot;
}

void SparseStorage::read(std::uint64_t offset, std::uint8_t* data, std::size_t length) const {
    read_masked(offset, data, length, nullptr, 0);
}

void SparseStorage::write(std::uint64_t offset, const std::uint8_t* data, std::size_t length) {
    write_masked(offset, data, length, nullptr, 0);
}

void SparseStorage::read_masked(std::uint64_t offset, std::uint8_t* data, std::size_t length,
                                const std::uint8_t* enables, std::size_t enable_length) const {
    check(offset, length);
    for (std::size_t done = 0; done < length;) {
        const auto address = offset + done;
        const auto in_page = static_cast<std::size_t>(address % kPageBytes);
        const auto chunk = std::min(length - done, kPageBytes - in_page);
        const Page* page = find(address / kPageBytes);
        for (std::size_t i = 0; i < chunk; ++i) {
            const auto index = done + i;
            if (enables && enables[index % enable_length] != kByteEnabled) continue;
            data[index] = page ? (*page)[in_page + i] : fill_;
        }
        done += chunk;
    }
}

void SparseStorage::write_masked(std::uint64_t offset, const std::uint8_t* data, std::size_t length,
                                 const std::uint8_t* enables, std::size_t enable_length) {
    check(offset, length);
    for (std::size_t done = 0; done < length;) {
        const auto address = offset + done;
        const auto in_page = static_cast<std::size_t>(address % kPageBytes);
        const auto chunk = std::min(length - done, kPageBytes - in_page);
        Page& page = touch(address / kPageBytes);
        if (!enables) {
            std::memcpy(page.data() + in_page, data + done, chunk);
        } else {
            for (std::size_t i = 0; i < chunk; ++i) {
                const auto index = done + i;
                if (enables[index % enable_length] == kByteEnabled) page[in_page + i] = data[index];
            }
        }
        done += chunk;
    }
}

SparseRam::SparseRam(sc_core::sc_module_name name, std::uint64_t size, sc_core::sc_time latency,
                     bool read_only, std::uint8_t fill)
    : sc_module(name), storage_(size, fill), latency_(latency), read_only_(read_only) {
    socket.register_b_transport(this, &SparseRam::b_transport);
    socket.register_transport_dbg(this, &SparseRam::transport_dbg);
}

void SparseRam::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);
    if (!tx.is_read() && !tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    const auto length = tx.get_data_length();
    if (!tx.get_data_ptr() || !length) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (tx.get_streaming_width() < length) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    const auto* enables = tx.get_byte_enable_ptr();
    const auto enable_length = tx.get_byte_enable_length();
    if (enables) {
        if (!enable_length) {
            tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return;
        }
        for (unsigned i = 0; i < enable_length; ++i) {
            if (enables[i] != TLM_BYTE_ENABLED && enables[i] != TLM_BYTE_DISABLED) {
                tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
                return;
            }
        }
    }
    if (!storage_.contains(tx.get_address(), length)) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    if (tx.is_write() && read_only_) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (tx.is_write()) {
        storage_.write_masked(tx.get_address(), tx.get_data_ptr(), length, enables, enable_length);
        ++writes_;
    } else {
        storage_.read_masked(tx.get_address(), tx.get_data_ptr(), length, enables, enable_length);
        ++reads_;
    }
    delay += latency_;
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned SparseRam::transport_dbg(tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    if ((!tx.is_read() && !tx.is_write()) || !tx.get_data_ptr()) return 0;
    const auto address = tx.get_address();
    if (address >= storage_.size()) return 0;
    const auto length = static_cast<unsigned>(
        std::min<std::uint64_t>(tx.get_data_length(), storage_.size() - address));
    if (tx.is_write()) storage_.write(address, tx.get_data_ptr(), length);
    else storage_.read(address, tx.get_data_ptr(), length);
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return length;
}

} // namespace fx1
