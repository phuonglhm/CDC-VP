#include "gpio_tlm.h"

#include <cstring>

namespace cdc::components {

gpio_tlm::gpio_tlm(sc_core::sc_module_name name, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , access_latency_(access_latency)
{
    socket.register_b_transport(this, &gpio_tlm::b_transport);
    socket.register_transport_dbg(this, &gpio_tlm::transport_dbg);
}

void gpio_tlm::set_pin(unsigned pin, bool level)
{
    if (pin >= 32) return;
    if (level)
        ext_in_ |= (1u << pin);
    else
        ext_in_ &= ~(1u << pin);
}

bool gpio_tlm::pin(unsigned pin) const
{
    if (pin >= 32) return false;
    return (value_reg() >> pin) & 1u;
}

std::uint32_t gpio_tlm::value_reg() const
{
    // Output pins read back OUT, input pins the external stimulus.
    return (out_ & dir_) | (ext_in_ & ~dir_);
}

bool gpio_tlm::access(tlm::tlm_generic_payload& trans)
{
    const std::uint64_t addr = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (ptr == nullptr || len != 4 || (addr & 0x3u) != 0)
        return false;

    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::uint32_t v;
        switch (addr) {
            case kValueOffset: v = value_reg(); break;
            case kOutOffset:   v = out_;        break;
            case kDirOffset:   v = dir_;        break;
            default:           return false;
        }
        std::memcpy(ptr, &v, 4);
        return true;
    }

    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::uint32_t v;
        std::memcpy(&v, ptr, 4);
        switch (addr) {
            case kOutOffset: out_ = v; return true;
            case kDirOffset: dir_ = v; return true;
            case kValueOffset:  // read-only
            default:
                return false;
        }
    }

    return false;
}

void gpio_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += access_latency_;
    trans.set_response_status(access(trans) ? tlm::TLM_OK_RESPONSE
                                            : tlm::TLM_COMMAND_ERROR_RESPONSE);
}

unsigned int gpio_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    if (!access(trans)) return 0;
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return trans.get_data_length();
}

} // namespace cdc::components
