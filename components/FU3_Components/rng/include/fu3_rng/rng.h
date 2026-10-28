#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <functional>
#include "fu3_rng/registers.h"
namespace fu3_rng {
struct Config {
    sc_core::sc_time clock_period{10,sc_core::SC_NS};
    // Required simulation-only source. Must not wait or throw; reset index repeats sequences.
    std::function<bool(std::uint64_t)> sample_bit;
};
class Rng:public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Rng> socket{"socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_out<bool> irq{"irq"};
    SC_HAS_PROCESS(Rng);
    explicit Rng(sc_core::sc_module_name,Config);
    // Clock from the clock controller (platform CRM); zero is ignored.
    void set_clock_period(sc_core::sc_time period){if(period>sc_core::SC_ZERO_TIME)config_.clock_period=period;}
    // Clock gate from the clock controller (platform CRM): while disabled the
    // IP gets no clock edge, keeps its state and resumes on the next enabled
    // edge. Enabled by default.
    void set_clock_enabled(bool on){if(on!=clock_enabled_){clock_enabled_=on;clock_gate_changed_.notify(sc_core::SC_ZERO_TIME);}}
    bool clock_enabled() const {return clock_enabled_;}
private:
    Config config_;
    bool enable_=false,valid_=false,collecting_=false,error_=false;
    std::uint32_t delay_=0,counter_=0,data_=0,shift_=0;
    unsigned count_=0;
    std::uint64_t sample_index_=0;
    tlm::tlm_generic_payload* pending_=nullptr;
    sc_core::sc_mutex mutex_;
    sc_core::sc_event completed_,outputs_;
    bool clock_enabled_=true;
    sc_core::sc_event clock_gate_changed_;
    void wait_clock_edge();
    void run();void publish();void reset();
    unsigned execute(tlm::tlm_generic_payload&);
    bool shape(tlm::tlm_generic_payload&);
    unsigned read(unsigned) const;
    void transport(tlm::tlm_generic_payload&,sc_core::sc_time&);
    unsigned debug(tlm::tlm_generic_payload&);
};
}
