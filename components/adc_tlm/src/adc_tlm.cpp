#include "adc_tlm.h"

#include <cstdint>
#include <cstring>

namespace cdc::components {

adc_tlm::adc_tlm(sc_core::sc_module_name name, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , irq_out("irq_out")
    , access_latency_(access_latency)
{
    socket.register_b_transport(this, &adc_tlm::b_transport);
    socket.register_transport_dbg(this, &adc_tlm::transport_dbg);
}

void adc_tlm::start_of_simulation()
{
    irq_out.write(false);
}

void adc_tlm::update_irq()
{
    irq_out.write(core_.hasInterrupt());
}

void adc_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += access_latency_;

    const std::uint64_t offset = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || offset > 0x0Cu || ptr == nullptr) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        const std::uint32_t value = core_.readReg(static_cast<std::uint32_t>(offset));
        std::memcpy(ptr, &value, sizeof(value));
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        core_.writeReg(static_cast<std::uint32_t>(offset), value);
    } else {
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    update_irq();
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int adc_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const std::uint64_t offset = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || offset > 0x0Cu || ptr == nullptr) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return 0;
    }

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        const std::uint32_t value = core_.debugReadReg(static_cast<std::uint32_t>(offset));
        std::memcpy(ptr, &value, sizeof(value));
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t value = 0;
        std::memcpy(&value, ptr, sizeof(value));
        core_.debugWriteReg(static_cast<std::uint32_t>(offset), value);
    } else {
        return 0;
    }

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return sizeof(std::uint32_t);
}

} // namespace cdc::components
