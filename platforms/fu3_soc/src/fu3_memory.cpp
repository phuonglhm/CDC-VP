#include "fu3_memory.h"

#include <algorithm>

namespace cdc::platforms::fu3_soc {

fu3_memory::fu3_memory(sc_core::sc_module_name name, std::uint64_t size, bool read_only)
    : sc_core::sc_module(name), bytes_(size, 0), read_only_(read_only)
{
    socket.register_b_transport(this, &fu3_memory::b_transport);
    socket.register_transport_dbg(this, &fu3_memory::transport_dbg);
}

bool fu3_memory::in_range(const tlm::tlm_generic_payload& tx) const
{
    const auto address = tx.get_address();
    return address < bytes_.size() && tx.get_data_length() <= bytes_.size() - address;
}

void fu3_memory::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time&)
{
    if (!tx.is_read() && !tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    if (!in_range(tx)) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    if (tx.get_streaming_width() != tx.get_data_length()) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (tx.is_write() && read_only_) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    const auto* enables = tx.get_byte_enable_ptr();
    const auto enable_length = tx.get_byte_enable_length();
    auto* data = tx.get_data_ptr();
    auto* mem = bytes_.data() + tx.get_address();
    for (unsigned i = 0; i < tx.get_data_length(); ++i) {
        if (enables && enables[i % enable_length] != TLM_BYTE_ENABLED)
            continue;
        if (tx.is_write())
            mem[i] = data[i];
        else
            data[i] = mem[i];
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned fu3_memory::transport_dbg(tlm::tlm_generic_payload& tx)
{
    if ((!tx.is_read() && !tx.is_write()) || tx.get_address() >= bytes_.size())
        return 0;
    const auto n = static_cast<unsigned>(
        std::min<std::uint64_t>(tx.get_data_length(), bytes_.size() - tx.get_address()));
    auto* mem = bytes_.data() + tx.get_address();
    for (unsigned i = 0; i < n; ++i) {
        if (tx.is_write())
            mem[i] = tx.get_data_ptr()[i];
        else
            tx.get_data_ptr()[i] = mem[i];
    }
    return n;
}

} // namespace cdc::platforms::fu3_soc
