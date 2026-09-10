#pragma once

#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace tutorial {

// Educational 32-bit countdown timer.
//
// The bus_router forwards region-local addresses, so these are offsets:
//   0x00 CTRL   R/W bit0 ENABLE, bit1 PERIODIC, bit2 IRQ_ENABLE
//   0x04 LOAD   R/W reload value; a write also reloads VALUE
//   0x08 VALUE  R   current counter
//   0x0C STATUS R/W bit0 IRQ_PENDING, write-one-to-clear
//   0xFC ID     R   constant 0x45445554 (ASCII "EDUT")
class edu_timer_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<edu_timer_tlm> socket;
    sc_core::sc_in<bool> reset_n;
    sc_core::sc_out<bool> irq;

    static constexpr std::uint64_t kCtrlOffset = 0x00;
    static constexpr std::uint64_t kLoadOffset = 0x04;
    static constexpr std::uint64_t kValueOffset = 0x08;
    static constexpr std::uint64_t kStatusOffset = 0x0C;
    static constexpr std::uint64_t kIdOffset = 0xFC;

    static constexpr std::uint32_t kCtrlEnable = 1u << 0;
    static constexpr std::uint32_t kCtrlPeriodic = 1u << 1;
    static constexpr std::uint32_t kCtrlIrqEnable = 1u << 2;
    static constexpr std::uint32_t kStatusPending = 1u << 0;
    static constexpr std::uint32_t kIdValue = 0x45445554u;

    SC_HAS_PROCESS(edu_timer_tlm);
    explicit edu_timer_tlm(
        sc_core::sc_module_name name,
        sc_core::sc_time tick_period = sc_core::sc_time(20, sc_core::SC_NS),
        sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    bool get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
                            tlm::tlm_dmi& dmi_data);
    void timer_thread();
    void drive_irq();

    tlm::tlm_response_status access(tlm::tlm_generic_payload& trans);
    void reset_registers();
    void update_irq();

    static std::uint32_t byte_mask(const tlm::tlm_generic_payload& trans);
    static void copy_read_data(tlm::tlm_generic_payload& trans,
                               std::uint32_t value);

    std::uint32_t ctrl_ = 0;
    std::uint32_t load_ = 0;
    std::uint32_t value_ = 0;
    std::uint32_t status_ = 0;

    sc_core::sc_event state_changed_;
    sc_core::sc_event irq_changed_;
    sc_core::sc_time tick_period_;
    sc_core::sc_time access_latency_;
};

} // namespace tutorial
