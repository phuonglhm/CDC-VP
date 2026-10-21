#include "fu3_timer/timer.h"
#include <stdexcept>

namespace fu3_timer {
void State::edge(bool write, unsigned offset, std::uint32_t data, std::uint32_t mask) {
    write=write && mask;
    const auto previous_control=control;
    const unsigned prescaler=(control>>3)&7;
    const bool increment=(control&1) && (!prescaler || prescaler==(cycle&7));
    const bool clear=overflow() || match();
    // The HDS compares the cycle counter against the entire control word.
    cycle=cycle>=previous_control ? 0 : cycle+1;
    if(write && offset==0) count=(count&~mask)|(data&mask);
    else if(write && offset==8) count=0;
    else if(clear) count=0;
    else if(increment) ++count;
    if(write && offset==4) control=(control&~mask)|(data&mask);
    if(write && offset==8) compare=(compare&~mask)|(data&mask);
}
std::uint32_t State::read(unsigned offset) const {
    switch(offset) { case 0:return count;case 4:return control;case 8:return compare;default:return 0; }
}
Timer::Timer(sc_core::sc_module_name name,sc_core::sc_time period):sc_module(name),period_(period) {
    if(period==sc_core::SC_ZERO_TIME) throw std::invalid_argument("Timer period must be positive");
    socket.register_b_transport(this,&Timer::transport);
    socket.register_transport_dbg(this,&Timer::debug);
    SC_THREAD(run);
    SC_METHOD(publish);sensitive<<changed_;
}
void Timer::publish() {
    overflow_irq.write(state_.overflow());compare_irq.write(state_.match());
    irq.write(state_.overflow() || state_.match());
}
void Timer::run() {
    while(true) {
        if(!reset_n.read()) {
            state_=State{};changed_.notify(sc_core::SC_ZERO_TIME);
            if(pending_) {pending_=false;status_=tlm::TLM_GENERIC_ERROR_RESPONSE;completed_.notify(sc_core::SC_ZERO_TIME);}
            wait(reset_n.posedge_event());
            continue;
        }
        wait(period_,reset_n.negedge_event());
        if(!reset_n.read()) continue;
        if(!clock_enabled.read()) {
            if(pending_) {pending_=false;status_=tlm::TLM_GENERIC_ERROR_RESPONSE;completed_.notify(sc_core::SC_ZERO_TIME);}
            continue;
        }
        // Reads sample pre-edge state. Writes are committed on this functional tick.
        if(pending_) result_=state_.read(offset_);
        state_.edge(pending_ && write_,offset_,data_,mask_);
        changed_.notify(sc_core::SC_ZERO_TIME);
        if(pending_) {pending_=false;status_=tlm::TLM_OK_RESPONSE;completed_.notify(sc_core::SC_ZERO_TIME);}
    }
}
bool Timer::validate(tlm::tlm_generic_payload& tx) const {
    tx.set_dmi_allowed(false);
    auto error=[&](tlm::tlm_response_status s){tx.set_response_status(s);return false;};
    if(!tx.is_read() && !tx.is_write()) return error(tlm::TLM_COMMAND_ERROR_RESPONSE);
    if(!tx.get_data_ptr()) return error(tlm::TLM_GENERIC_ERROR_RESPONSE);
    if(tx.get_address()>=0x10000 || tx.get_address()%4) return error(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    if(tx.get_data_length()!=4 || tx.get_streaming_width()<4) return error(tlm::TLM_BURST_ERROR_RESPONSE);
    if(auto* enables=tx.get_byte_enable_ptr()) {
        const auto n=tx.get_byte_enable_length();
        if(!n) return error(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        for(unsigned i=0;i<4;++i) if(enables[i%n]!=0xff && enables[i%n]!=0) return error(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    }
    return true;
}
void Timer::transport(tlm::tlm_generic_payload& tx,sc_core::sc_time& delay) {
    if(delay!=sc_core::SC_ZERO_TIME){auto d=delay;delay=sc_core::SC_ZERO_TIME;wait(d);}
    // Yield even on rejected accesses, so a polling master cannot starve peers.
    wait(sc_core::SC_ZERO_TIME);
    if(!validate(tx)) {wait(period_);return;}
    struct Lock {sc_core::sc_mutex& m;explicit Lock(sc_core::sc_mutex& x):m(x){m.lock();}~Lock(){m.unlock();}} lock(access_);
    if(!reset_n.read() || !clock_enabled.read()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);wait(period_);return;
    }
    write_=tx.is_write();offset_=static_cast<unsigned>(tx.get_address()&12);
    data_=0;mask_=0;
    for(unsigned i=0;i<4;++i)if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()]) mask_|=255u<<(8*i);
    if(write_) for(unsigned i=0;i<4;++i)data_|=std::uint32_t(tx.get_data_ptr()[i])<<(8*i);
    status_=tlm::TLM_INCOMPLETE_RESPONSE;pending_=true;
    try {wait(completed_);} catch(...) {pending_=false;throw;}
    if(status_==tlm::TLM_OK_RESPONSE && tx.is_read())
        for(unsigned i=0;i<4;++i)if(mask_&(255u<<(8*i)))tx.get_data_ptr()[i]=static_cast<unsigned char>(result_>>(8*i));
    tx.set_response_status(status_);
}
unsigned Timer::debug(tlm::tlm_generic_payload& tx) {
    if(!validate(tx)) return 0;
    // Inspection only: no state changes, no timing and no interrupt acknowledge.
    if(!tx.is_read()) {tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);return 0;}
    const auto value=state_.read(static_cast<unsigned>(tx.get_address()&12));
    for(unsigned i=0;i<4;++i)tx.get_data_ptr()[i]=static_cast<unsigned char>(value>>(8*i));
    tx.set_response_status(tlm::TLM_OK_RESPONSE);return 4;
}
} // namespace fu3_timer
