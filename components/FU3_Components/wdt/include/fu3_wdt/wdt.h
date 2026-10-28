#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <cstdint>
#include "fu3_wdt/registers.h"

namespace fu3_wdt {
class Watchdog : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Watchdog> socket{"socket"};
    sc_core::sc_in<bool> preset_n{"preset_n"}, wdogres_n{"wdogres_n"};
    // Deprecated V1.1 compatibility binding; V1.2 write protection is internal WD_LOCK.
    sc_core::sc_port<sc_core::sc_signal_in_if<bool>,1,sc_core::SC_ZERO_OR_MORE_BOUND> i_wdt_other_unlock{"i_wdt_other_unlock"};
    sc_core::sc_port<sc_core::sc_signal_out_if<bool>,1,sc_core::SC_ZERO_OR_MORE_BOUND> wdogpause{"wdogpause"};
    sc_core::sc_out<bool> irq{"irq"}, reset_request{"reset_request"};
    SC_HAS_PROCESS(Watchdog);
    explicit Watchdog(sc_core::sc_module_name,
        sc_core::sc_time tick=sc_core::sc_time(5,sc_core::SC_NS),
        unsigned apb_wait_cycles=10,
        sc_core::sc_time apb_period=sc_core::sc_time(5,sc_core::SC_NS));
    // Watchdog tick and APB clock from the clock controller (platform CRM,
    // decision Q05-f). Ticks elapsed so far count with the old tick; zero is ignored.
    void set_clock(sc_core::sc_time tick, sc_core::sc_time apb_period);
    sc_core::sc_time tick() const { return tick_; }
    // Clock gate from the clock controller (platform CRM): while disabled no
    // tick elapses (the counter holds, as during a debug halt) and the APB side
    // is reached only through the platform's gate. Enabled by default.
    void set_clock_enabled(bool on);
    bool clock_enabled() const { return clock_enabled_; }
private:
    unsigned apb_wait_cycles_;
    std::uint32_t load_=FU3_WDT_RESET_VALUE, count_=FU3_WDT_RESET_VALUE,
                  control_=FU3_WDT_RESEN;
    bool timeout_seen_=false, reset_pending_=false, halt_=false, locked_=false;
    bool clock_enabled_=true;
    bool core_reset_=true;
    sc_core::sc_time tick_, apb_latency_, last_tick_;
    sc_core::sc_mutex access_mutex_;
    sc_core::sc_event changed_, outputs_;
    void synchronize();
    void programming_reset();
    void watchdog_reset();
    void watchdog_tick();
    void run();
    void publish();
    bool validate(tlm::tlm_generic_payload&);
    std::uint32_t read(unsigned) const;
    void write(unsigned,std::uint32_t,std::uint32_t);
    void transport(tlm::tlm_generic_payload&,sc_core::sc_time&);
    unsigned debug(tlm::tlm_generic_payload&);
};
} // namespace fu3_wdt
