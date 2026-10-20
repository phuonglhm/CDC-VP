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
private:
    Config config_;
    bool enable_=false,valid_=false,collecting_=false,error_=false;
    std::uint32_t delay_=0,counter_=0,data_=0,shift_=0;
    unsigned count_=0;
    std::uint64_t sample_index_=0;
    tlm::tlm_generic_payload* pending_=nullptr;
    sc_core::sc_mutex mutex_;
    sc_core::sc_event completed_,outputs_;
    void run();void publish();void reset();
    unsigned execute(tlm::tlm_generic_payload&);
    bool shape(tlm::tlm_generic_payload&);
    unsigned read(unsigned) const;
    void transport(tlm::tlm_generic_payload&,sc_core::sc_time&);
    unsigned debug(tlm::tlm_generic_payload&);
};
}
