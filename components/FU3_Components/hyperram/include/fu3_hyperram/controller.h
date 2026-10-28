#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <fu3_bus/access.h>
#include <fu3_bus/config.h>
#include <fu3_bus/admission.h>
#include <fu3_bus/fifo.h>
#include <array>
#include <vector>
#include "fu3_hyperram/registers.h"
namespace fu3_hyperram {
struct Config {
    sc_core::sc_time core_period{5,sc_core::SC_NS};
    bool industrial_plus=true; // Conservative VP default: 1 us tCSM; false selects 4 us.
};
// Test/platform-only fault injection, not a firmware CSR or a raw AXI W channel.
struct Fault : tlm::tlm_extension<Fault> {
    unsigned fail_after_words=~0u;
    bool refresh=false;
    tlm_extension_base* clone() const override {return new Fault(*this);}
    void copy_from(const tlm_extension_base& other) override {*this=static_cast<const Fault&>(other);}
};
class Controller : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Controller> memory{"memory"}, csr{"csr"};
    sc_core::sc_in<bool> reset_n{"reset_n"}, device_reset_n{"device_reset_n"};
    sc_core::sc_out<bool> irq{"irq"};
    SC_HAS_PROCESS(Controller);
    explicit Controller(sc_core::sc_module_name,Config = {});
    static void configure_bus(fu3_bus::BusConfig&);
    // Controller clock from the clock controller (platform CRM); zero is ignored.
    void set_core_period(sc_core::sc_time period) {if(period>sc_core::SC_ZERO_TIME)config_.core_period=period;}
    // Core clock gate from the clock controller (platform CRM): while disabled
    // the engine and an in-flight memory transfer make no progress and resume
    // where they stopped on enable. Enabled by default.
    void set_clock_enabled(bool on){if(on!=clock_enabled_){clock_enabled_=on;clock_gate_changed_.notify(sc_core::SC_ZERO_TIME);}}
    bool clock_enabled() const {return clock_enabled_;}
    unsigned peak_reads() const {return reads_.peak();}
    unsigned peak_writes() const {return writes_.peak();}
private:
    Config config_;
    std::vector<unsigned char> ram_;
    std::array<unsigned,28> r_{};
    std::array<unsigned,5> active_{};
    std::array<unsigned,8> request_{};
    unsigned cr0_=0x8f2f,cr1_=0,epoch_=0,accepted_=0,operation_=0;
    bool valid_=false,pending_=false,done_=false,command_error_=false;
    fu3_bus::Admission reads_,writes_;
    fu3_bus::Fifo engine_;
    sc_core::sc_event work_,changed_,cancel_,clock_gate_changed_;
    bool clock_enabled_=true;
    void reset(); void publish(); void worker();
    bool coherent() const; bool ready() const; unsigned status() const;
    bool pause(unsigned,unsigned); void fault(unsigned,unsigned=0,unsigned=0,bool=false);
    bool config_valid(const std::array<unsigned,8>&) const;
    void memory_transport(tlm::tlm_generic_payload&,sc_core::sc_time&);
    void csr_transport(tlm::tlm_generic_payload&,sc_core::sc_time&);
    unsigned debug(tlm::tlm_generic_payload&);
    unsigned read(unsigned) const; void write(unsigned,unsigned);
    bool shape(tlm::tlm_generic_payload&);
};
}
