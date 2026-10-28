#include "fu3_rng/rng.h"
#include <stdexcept>
#include <utility>
namespace fu3_rng {
Rng::Rng(sc_core::sc_module_name n,Config c):sc_module(n),config_(std::move(c)) {
    if(config_.clock_period<=sc_core::SC_ZERO_TIME||!config_.sample_bit)
        throw std::invalid_argument("RNG requires a positive clock period and an explicit simulation sample source");
    socket.register_b_transport(this,&Rng::transport);socket.register_transport_dbg(this,&Rng::debug);
    SC_THREAD(run);SC_METHOD(publish);sensitive<<outputs_;
}
void Rng::publish(){irq=valid_||error_;}
void Rng::reset(){enable_=valid_=collecting_=error_=false;delay_=counter_=data_=shift_=count_=0;sample_index_=0;}
unsigned Rng::read(unsigned offset) const {
    switch(offset){case 0:return enable_;case 4:return 0;case 8:return delay_;
    case 12:return valid_;case 16:return valid_?data_:0;default:return 0;}
}
bool Rng::shape(tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    if(!tx.is_read()&&!tx.is_write()){tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);return false;}
    if(!tx.get_data_ptr()){tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);return false;}
    if(tx.get_data_length()!=4||tx.get_streaming_width()<4){tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);return false;}
    if(auto* be=tx.get_byte_enable_ptr()){
        if(!tx.get_byte_enable_length()){tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);return false;}
        for(unsigned i=0;i<4;++i)if(be[i%tx.get_byte_enable_length()]!=0&&be[i%tx.get_byte_enable_length()]!=255){
            tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);return false;}
    }return true;
}
unsigned Rng::execute(tlm::tlm_generic_payload& tx) {
    auto* data=tx.get_data_ptr();const auto address=tx.get_address();
    if(address>16||address%4||(tx.is_write()&&address>=12)) {
        if(tx.is_read())for(unsigned i=0;i<4;++i)data[i]=0;
        tx.set_response_status(address>16||address%4?tlm::TLM_ADDRESS_ERROR_RESPONSE:tlm::TLM_COMMAND_ERROR_RESPONSE);
        error_=true;return 0;
    }
    auto enabled=[&](unsigned i){return !tx.get_byte_enable_ptr()||tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()]!=0;};
    if(tx.is_read()) {const auto value=read(unsigned(address));for(unsigned i=0;i<4;++i)if(enabled(i))data[i]=static_cast<unsigned char>(value>>(8*i));}
    else {
        if(address==0&&enabled(0))enable_=data[0]&1;
        if(address==8)for(unsigned i=0;i<4;++i)if(enabled(i))delay_=(delay_&~(255u<<(8*i)))|(unsigned(data[i])<<(8*i));
        if(address==4&&enabled(0)){tx.set_response_status(tlm::TLM_OK_RESPONSE);return data[0]&3;}
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);return 0;
}
void Rng::wait_clock_edge() {
    // A gate during the period cancels the edge; the phase restarts on enable.
    while(true){
        while(!clock_enabled_)wait(clock_gate_changed_);
        const auto due=sc_core::sc_time_stamp()+config_.clock_period;
        wait(config_.clock_period,clock_gate_changed_);
        if(clock_enabled_&&sc_core::sc_time_stamp()>=due)return;
    }
}
void Rng::run() {
    while(true){wait_clock_edge();error_=false;
        if(!reset_n.read()){
            reset();if(pending_)pending_->set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        }else{
            const unsigned command=pending_?execute(*pending_):0;
            if(command){valid_=false;data_=shift_=count_=counter_=0;
                if(command&reg::RESTART)sample_index_=0;
                collecting_=bool(command&reg::NEXT);
            }else if(enable_&&collecting_){
                // Explicit VP schedule, not an RTL timing equation: DELAY+1 clocks per bit.
                if(counter_==delay_){counter_=0;shift_=(shift_<<1)|unsigned(config_.sample_bit(sample_index_++));
                    if(++count_==32){data_=shift_;valid_=true;collecting_=false;}
                }else ++counter_;
            }
        }
        if(pending_){pending_=nullptr;completed_.notify(sc_core::SC_ZERO_TIME);}
        outputs_.notify(sc_core::SC_ZERO_TIME);
    }
}
void Rng::transport(tlm::tlm_generic_payload& tx,sc_core::sc_time& delay){
    const auto incoming=delay;delay=sc_core::SC_ZERO_TIME;wait(incoming);
    struct Guard{sc_core::sc_mutex& m;explicit Guard(sc_core::sc_mutex& x):m(x){m.lock();}~Guard(){m.unlock();}}guard(mutex_);
    if(!shape(tx)){wait(config_.clock_period);return;}
    pending_=&tx;try{wait(completed_);}catch(...){if(pending_==&tx)pending_=nullptr;throw;}
}
unsigned Rng::debug(tlm::tlm_generic_payload& tx){
    if(!shape(tx))return 0;
    if(!tx.is_read()){tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);return 0;}
    if(tx.get_address()>16||tx.get_address()%4){tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);return 0;}
    auto value=read(unsigned(tx.get_address()));for(unsigned i=0;i<4;++i)
        if(!tx.get_byte_enable_ptr()||tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()])tx.get_data_ptr()[i]=static_cast<unsigned char>(value>>(8*i));
    tx.set_response_status(tlm::TLM_OK_RESPONSE);return 4;
}
}
