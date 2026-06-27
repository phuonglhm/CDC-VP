#include "rtc_tlm.h"

#include <cstdint>
#include <cstring>

namespace cdc::components {

namespace {
constexpr std::uint64_t kRegMax = 0x1C; // highest valid register offset
}

rtc_tlm::rtc_tlm(sc_core::sc_module_name name,
                 sc_core::sc_time tick_period,
                 sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , reset_n("reset_n")
    , irq_out("irq_out")
    , tick_period_(tick_period)
    , access_latency_(access_latency)
{
    socket.register_b_transport(this, &rtc_tlm::b_transport);
    socket.register_transport_dbg(this, &rtc_tlm::transport_dbg);

    SC_THREAD(tick_thread);

    SC_METHOD(drive_outputs);
    sensitive << reset_n << irq_update_event_;
    dont_initialize();
}

void rtc_tlm::start_of_simulation()
{
    if (!reset_n.read()) {
        core_.reset();
    }
    irq_level_ = false;
    irq_out.write(false);
}

void rtc_tlm::tick_thread()
{
    while (true) {
        wait(tick_period_);
        if (!reset_n.read()) {
            continue; // counter is frozen while held in reset
        }
        core_.tick();
        update_irq();
    }
}

void rtc_tlm::drive_outputs()
{
    if (!reset_n.read()) {
        core_.reset();
        irq_level_ = false;
    }
    irq_out.write(irq_level_);
}

void rtc_tlm::update_irq()
{
    irq_level_ = core_.hasInterrupt();
    irq_update_event_.notify(sc_core::SC_ZERO_TIME);
}

void rtc_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += access_latency_;

    const std::uint64_t offset = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || offset > kRegMax || ptr == nullptr) {
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

unsigned int rtc_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    const std::uint64_t offset = trans.get_address();
    const unsigned len = trans.get_data_length();
    unsigned char* ptr = trans.get_data_ptr();

    if (len != sizeof(std::uint32_t) || (offset & 0x3u) != 0u || offset > kRegMax || ptr == nullptr) {
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
