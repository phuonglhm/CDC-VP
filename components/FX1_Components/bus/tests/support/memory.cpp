#include "memory.h"
#include "bus/transaction.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace bus::test {
Memory::Memory(sc_core::sc_module_name name, std::size_t bytes, bool readonly, unsigned latency_ns)
    : sc_module(name), bytes_(bytes, 0), readonly_(readonly), latency_ns_(latency_ns) {
    target.register_b_transport(this, &Memory::b_transport);
    target.register_transport_dbg(this, &Memory::transport_dbg);
}
void Memory::load(std::size_t offset, const std::vector<unsigned char>& bytes) {
    if (sc_core::sc_is_running()) throw std::logic_error("Load images before sc_start");
    if (offset > bytes_.size() || bytes.size() > bytes_.size() - offset)
        throw std::out_of_range("Image does not fit memory");
    std::copy(bytes.begin(), bytes.end(), bytes_.begin() + offset);
}
void Memory::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    consume_delay(delay);
    if (!validate(tx)) return;
    const auto address = tx.get_address();
    if (address >= bytes_.size() || tx.get_data_length() > bytes_.size() - address) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    sc_core::wait(latency_ns_, sc_core::SC_NS);
    if (readonly_ && tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    for (unsigned i = 0; i < tx.get_data_length(); ++i) {
        if (tx.get_byte_enable_ptr() &&
            tx.get_byte_enable_ptr()[i % tx.get_byte_enable_length()] == TLM_BYTE_DISABLED) continue;
        if (tx.is_write()) bytes_[address+i] = tx.get_data_ptr()[i];
        else tx.get_data_ptr()[i] = bytes_[address+i];
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}
unsigned Memory::transport_dbg(tlm::tlm_generic_payload& tx) {
    const auto address = tx.get_address();
    if (!tx.get_data_ptr() || address >= bytes_.size()) return 0;
    if (!tx.is_read() && !tx.is_write()) return 0;
    const auto n = static_cast<unsigned>(std::min<std::uint64_t>(tx.get_data_length(), bytes_.size()-address));
    for (unsigned i = 0; i < n; ++i) {
        if (tx.is_write()) bytes_[address+i] = tx.get_data_ptr()[i];
        else tx.get_data_ptr()[i] = bytes_[address+i];
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    return n; // Backdoor writes intentionally initialize read-only ROM too.
}
} // namespace bus::test
