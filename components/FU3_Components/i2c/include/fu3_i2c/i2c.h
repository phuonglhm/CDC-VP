#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <array>
#include <deque>
#include "fu3_i2c/registers.h"
namespace fu3_i2c {
struct Config { sc_core::sc_time clock_period{10,sc_core::SC_NS}; };
class I2c : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<I2c> socket{"socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"}, scl_i{"scl_i"}, sda_i{"sda_i"};
    // Active-low enables: false drives zero, true releases the external wired-AND bus.
    sc_core::sc_out<bool> scl_oe_n{"scl_oe_n"}, sda_oe_n{"sda_oe_n"}, irq{"irq"};
    SC_HAS_PROCESS(I2c);
    explicit I2c(sc_core::sc_module_name, Config = {});
private:
    Config config_;
    std::array<unsigned,17> r_{};
    struct Filter { bool ff1=true,ff2=true,value=true; unsigned stable=0,history=7; };
    Filter scl_,sda_;
    unsigned filter_div_=0;
    bool drive_scl_=true,drive_sda_=true,busy_=false,owned_=false,al_=false,if_=false,tip_=false;
    bool rxack_=false, active_=false,dir_=false;
    unsigned slave_flags_=0,free_ticks_=0;
    enum class Slave { Idle, Address, AddressAck, Receive, ReceiveAck, Transmit, TransmitAck, Ignore };
    Slave slave_=Slave::Idle;
    unsigned bits_=0,shift_=0,slave_tx_=0,ack_phase_=0;
    bool address_match_=false,master_nack_=false;
    enum class Action { None, Read, Ack, Finish };
    struct Step { bool scl,sda; unsigned ticks; bool high; Action action; bool arbitrate; };
    std::deque<Step> steps_;
    unsigned remaining_=0,master_rx_=0;
    bool entered_=false,starting_=false,stopping_=false;
    tlm::tlm_generic_payload* pending_=nullptr;
    sc_core::sc_mutex mutex_;
    sc_core::sc_event completed_,outputs_;
    void run(); void publish(); void reset(); void cycle();
    void filter(Filter&,bool,bool); void slave_tick(bool,bool,bool,bool);
    void master_tick(); void command(unsigned); void abort(); void lose_arbitration();
    unsigned timing(unsigned) const; void add(bool,bool,unsigned,bool=false,Action=Action::None,bool=false);
    unsigned read(unsigned) const; void write(unsigned,unsigned);
    bool validate(tlm::tlm_generic_payload&); void execute(tlm::tlm_generic_payload&);
    void transport(tlm::tlm_generic_payload&,sc_core::sc_time&); unsigned debug(tlm::tlm_generic_payload&);
};
}
