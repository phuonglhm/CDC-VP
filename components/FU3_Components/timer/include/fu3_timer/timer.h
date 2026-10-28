#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include "fu3_timer/registers.h"

namespace fu3_timer {
// Single-instance Timer HDS state. All edge decisions use pre-edge values.
struct State {
    std::uint32_t count=0, control=0, compare=0, cycle=0;
    bool overflow() const { return count==UINT32_MAX; }
    bool match() const { return compare!=0 && count==compare; }
    void edge(bool write=false, unsigned offset=12, std::uint32_t data=0, std::uint32_t mask=UINT32_MAX);
    std::uint32_t read(unsigned offset) const;
};

class Timer : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Timer> socket{"socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_in<bool> clock_enabled{"clock_enabled"};
    sc_core::sc_out<bool> overflow_irq{"overflow_irq"}, compare_irq{"compare_irq"}, irq{"irq"};
    SC_HAS_PROCESS(Timer);
    explicit Timer(sc_core::sc_module_name name,
                   sc_core::sc_time period=sc_core::sc_time(10,sc_core::SC_NS));
    // Clock period from the clock controller (platform CRM, decision Q05-f);
    // used from the next tick. Zero is ignored (a stopped clock is clock_enabled).
    void set_period(sc_core::sc_time period) { if(period!=sc_core::SC_ZERO_TIME) period_=period; }
    sc_core::sc_time period() const { return period_; }
private:
    State state_;
    sc_core::sc_time period_;
    sc_core::sc_mutex access_;
    sc_core::sc_event completed_, changed_;
    bool pending_=false, write_=false;
    unsigned offset_=0;
    std::uint32_t data_=0, result_=0, mask_=UINT32_MAX;
    tlm::tlm_response_status status_=tlm::TLM_INCOMPLETE_RESPONSE;
    void run();
    void publish();
    void transport(tlm::tlm_generic_payload&,sc_core::sc_time&);
    unsigned debug(tlm::tlm_generic_payload&);
    bool validate(tlm::tlm_generic_payload&) const;
};
} // namespace fu3_timer
