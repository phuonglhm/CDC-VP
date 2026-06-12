#include "memory_tlm.h"

#include <algorithm>
#include <cstring>

namespace cdc::components {

memory_tlm::memory_tlm(sc_core::sc_module_name name,
                       std::size_t size_bytes,
                       bool read_only,
                       sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , data_(size_bytes, 0)
    , read_only_(read_only)
    , access_latency_(access_latency)
{
    socket.register_b_transport(this, &memory_tlm::b_transport);
    socket.register_transport_dbg(this, &memory_tlm::transport_dbg);
    socket.register_get_direct_mem_ptr(this, &memory_tlm::get_direct_mem_ptr);
}

void memory_tlm::load(const std::uint8_t* data, std::size_t len, std::uint64_t offset)
{
    if (offset > data_.size() || len > data_.size() - offset) {
        SC_REPORT_ERROR("memory_tlm", "load() out of bounds");
        return;
    }
    std::memcpy(data_.data() + offset, data, len);
}

void memory_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += access_latency_;

    const std::uint64_t addr = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (addr > data_.size() || len > data_.size() - addr) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    if (ptr == nullptr) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::memcpy(ptr, data_.data() + addr, len);
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        if (read_only_) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        std::memcpy(data_.data() + addr, ptr, len);
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    trans.set_dmi_allowed(true);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int memory_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const std::uint64_t addr = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (addr > data_.size() || ptr == nullptr) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return 0;
    }

    const unsigned n = static_cast<unsigned>(
        std::min<std::uint64_t>(len, data_.size() - addr));

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::memcpy(ptr, data_.data() + addr, n);
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND && !read_only_) {
        std::memcpy(data_.data() + addr, ptr, n);
    } else {
        return 0;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return n;
}

bool memory_tlm::get_direct_mem_ptr(tlm::tlm_generic_payload& /*trans*/, tlm::tlm_dmi& dmi)
{
    dmi.set_dmi_ptr(data_.data());
    dmi.set_start_address(0);
    dmi.set_end_address(data_.empty() ? 0 : data_.size() - 1);
    dmi.set_read_latency(access_latency_);
    dmi.set_write_latency(access_latency_);
    dmi.allow_read();
    if (!read_only_) {
        dmi.allow_read_write();
    }
    return true;
}

} // namespace cdc::components
