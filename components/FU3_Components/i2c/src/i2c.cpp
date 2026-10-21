#include "fu3_i2c/i2c.h"
#include <algorithm>
#include <stdexcept>
namespace fu3_i2c {
I2c::I2c(sc_core::sc_module_name name,Config config):sc_module(name),config_(config) {
    if(config.clock_period<=sc_core::SC_ZERO_TIME) throw std::invalid_argument("I2C clock period must be positive");
    socket.register_b_transport(this,&I2c::transport); socket.register_transport_dbg(this,&I2c::debug);
    SC_THREAD(run); SC_METHOD(publish); sensitive<<outputs_;
}
void I2c::publish(){scl_oe_n=drive_scl_;sda_oe_n=drive_sda_;irq=if_&&bool(r_[1]&reg::IEN);}
void I2c::abort(){steps_.clear();tip_=owned_=starting_=stopping_=entered_=false;remaining_=0;
    drive_scl_=drive_sda_=true;slave_=Slave::Idle;active_=false;bits_=ack_phase_=0;r_[5]=0;}
void I2c::reset(){r_.fill(0);abort();scl_=Filter{};sda_=Filter{};filter_div_=0;
    busy_=al_=if_=rxack_=dir_=false;slave_flags_=free_ticks_=master_rx_=0;}
void I2c::run(){
    auto next=sc_core::sc_time_stamp()+config_.clock_period;
    while(true){
        if(!reset_n.read()) reset();
        else if(sc_core::sc_time_stamp()>=next) cycle();
        if(!reset_n.read()&&pending_){pending_->set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);pending_=nullptr;completed_.notify(sc_core::SC_ZERO_TIME);}
        outputs_.notify(sc_core::SC_ZERO_TIME);
        if(sc_core::sc_time_stamp()>=next)next+=config_.clock_period;
        wait(next-sc_core::sc_time_stamp(),reset_n.negedge_event());
    }
}
void I2c::filter(Filter& f,bool raw,bool sample){
    const bool input=f.ff2;f.ff2=f.ff1;f.ff1=raw;
    if(r_[1]&reg::TIMING_EN){
        if(input==f.value)f.stable=0;
        else if(++f.stable>=r_[13]+1){f.value=input;f.stable=0;}
    }else if(sample){f.history=((f.history<<1)|unsigned(input))&7;
        f.value=((f.history&1)+((f.history>>1)&1)+((f.history>>2)&1))>=2;}
}
void I2c::cycle(){
    // MMIO first, then external events: a new event wins over software clear.
    if(pending_){execute(*pending_);pending_=nullptr;completed_.notify(sc_core::SC_ZERO_TIME);}
    const bool old_scl=scl_.value,old_sda=sda_.value;
    const bool sample=filter_div_==0;
    if(sample)filter_div_=std::max(1u,(r_[0]+1)/4)-1;else --filter_div_;
    filter(scl_,scl_i.read(),sample);filter(sda_,sda_i.read(),sample);
    const bool start=old_sda&&!sda_.value&&scl_.value&&old_scl;
    const bool stop=!old_sda&&sda_.value&&scl_.value&&old_scl;
    if(start)busy_=true;
    if(stop)busy_=false;
    if(!busy_&&scl_.value&&sda_.value){if(free_ticks_!=~0u)++free_ticks_;}else free_ticks_=0;
    if(!(r_[1]&reg::EN))return;
    if(r_[15]&1)slave_tick(!old_scl&&scl_.value,old_scl&&!scl_.value,start,stop);
    else{
        if(stop&&tip_&&owned_&&!starting_&&!stopping_)lose_arbitration();
        master_tick();
    }
}
unsigned I2c::timing(unsigned offset) const{
    if(r_[1]&reg::TIMING_EN)return r_[offset/4]+1;
    const auto p=r_[0]+1;
    return offset==reg::TLOW?3*p:offset==reg::THIGH?2*p:p;
}
void I2c::add(bool scl,bool sda,unsigned ticks,bool high,Action action,bool arbitrate){
    steps_.push_back({scl,sda,std::max(1u,ticks),high,action,arbitrate});
}
void I2c::command(unsigned cmd){
    if(!(r_[1]&reg::EN))return;
    if(cmd&reg::IACK)if_=false;
    if((r_[15]&1)||tip_)return;
    cmd&=0xf8;
    if(!(cmd&0xf0)||((cmd&(reg::RD|reg::WR))==(reg::RD|reg::WR)))return;
    if(!(cmd&reg::STA)&&!owned_)return;
    r_[5]=cmd;tip_=true;master_rx_=0;
    starting_=(cmd&reg::STA)&&!owned_;stopping_=false;
    bool data=drive_sda_;
    if(cmd&reg::STA){
        al_=false;
        if(owned_)add(false,true,timing(reg::TLOW));
        add(true,true,timing(reg::TSU_STA),true);
        add(true,false,timing(reg::THD_STA),true);data=false;
    }
    const auto low=timing(reg::TLOW),hold=std::min(timing(reg::THD_DAT),low);
    if(cmd&(reg::WR|reg::RD)){
        for(unsigned bit=0;bit<9;++bit){
            const bool value=bit==8?((cmd&reg::WR)?true:bool(cmd&reg::NACK)):
                ((cmd&reg::RD)?true:bool(r_[4]&(0x80u>>bit)));
            add(false,data,hold);add(false,value,std::max(1u,low-hold));
            const auto action=bit==8?((cmd&reg::WR)?Action::Ack:Action::None):
                ((cmd&reg::RD)?Action::Read:Action::None);
            add(true,value,timing(reg::THIGH),true,action,(cmd&reg::WR)&&bit<8&&value);
            data=value;
        }
    }
    if(cmd&reg::STO){
        add(false,data,hold);add(false,false,low);
        add(true,false,timing(reg::TSU_STO),true);
        add(true,true,r_[13]+5,true,Action::Finish);
    }else add(false,data,1,false,Action::Finish);
}
void I2c::lose_arbitration(){abort();al_=true;if_=true;}
void I2c::master_tick(){
    if(!tip_||steps_.empty())return;
    if(starting_){if(busy_||!scl_.value||!sda_.value||free_ticks_<timing(reg::TBUF))return;
        starting_=false;owned_=true;}
    auto& step=steps_.front();
    if(!entered_){drive_scl_=step.scl;drive_sda_=step.sda;remaining_=step.ticks;entered_=true;
        if((r_[5]&reg::STO)&&steps_.size()<=3)stopping_=true;
        return;}
    if(step.high&&!scl_.value)return; // External clock stretching.
    if(step.arbitrate&&!sda_.value){lose_arbitration();return;}
    if(--remaining_)return;
    const auto action=step.action;steps_.pop_front();entered_=false;
    if(action==Action::Read)master_rx_=((master_rx_<<1)|unsigned(sda_.value))&255;
    if(action==Action::Ack)rxack_=sda_.value;
    if(action==Action::Finish){
        if(r_[5]&reg::RD)r_[2]=master_rx_;
        if(r_[5]&reg::STO)owned_=false;
        r_[5]=0;tip_=false;stopping_=false;if_=true;
    }
}
void I2c::slave_tick(bool rise,bool fall,bool start,bool stop){
    if(stop){if(active_)slave_flags_|=reg::STOP_DET;active_=false;slave_=Slave::Idle;drive_sda_=true;return;}
    if(start){active_=false;slave_=Slave::Address;bits_=shift_=ack_phase_=0;drive_sda_=true;return;}
    switch(slave_){
    case Slave::Address: case Slave::Receive:
        if(rise){shift_=((shift_<<1)|unsigned(sda_.value))&255;
            if(++bits_==8){bits_=0;ack_phase_=0;
                if(slave_==Slave::Address){address_match_=(shift_>>1)==r_[14]&&(shift_>>1)!=0;
                    if(address_match_){dir_=shift_&1;active_=true;slave_flags_|=reg::ADDR_MATCH;}
                    slave_=Slave::AddressAck;
                }else{r_[2]=shift_;slave_flags_|=reg::RX_VALID;if_=true;slave_=Slave::ReceiveAck;}
            }}break;
    case Slave::AddressAck: case Slave::ReceiveAck:
        if(fall){if(ack_phase_++==0)drive_sda_=slave_==Slave::AddressAck?!address_match_:false;
            else {const bool addressed=slave_==Slave::AddressAck;
                drive_sda_=true;shift_=bits_=0;
                if(addressed&&!address_match_)slave_=Slave::Ignore;
                else if(addressed&&dir_){slave_=Slave::Transmit;slave_tx_=r_[4];drive_sda_=bool(slave_tx_&128);}
                else slave_=Slave::Receive;
            }}break;
    case Slave::Transmit:
        if(rise)++bits_;
        if(fall){if(bits_==8){drive_sda_=true;slave_=Slave::TransmitAck;ack_phase_=0;}
            else drive_sda_=bool(slave_tx_&(0x80u>>bits_));}break;
    case Slave::TransmitAck:
        if(rise){master_nack_=sda_.value;slave_flags_|=reg::TX_DONE;
            if(master_nack_)slave_flags_|=reg::MASTER_NACK;
            if_=true;ack_phase_=1;}
        if(fall&&ack_phase_){if(master_nack_){slave_=Slave::Ignore;drive_sda_=true;}
            else{slave_=Slave::Transmit;bits_=0;slave_tx_=r_[4];drive_sda_=bool(slave_tx_&128);}}break;
    default:break;
    }
}
unsigned I2c::read(unsigned offset) const{
    const auto index=(offset>>2)&31;
    if(index>=r_.size())return 0;
    if(index==3)return (rxack_?128:0)|(busy_?64:0)|(al_?32:0)|(tip_?2:0)|(if_?1:0);
    if(index==16)return slave_flags_|(active_?8:0)|(dir_?4:0);
    return r_[index];
}
void I2c::write(unsigned offset,unsigned value){
    const auto index=(offset>>2)&31;if(index>=r_.size())return;
    switch(index){
    case 1: if((r_[1]&reg::EN)&&!(value&reg::EN))abort();r_[1]=value&0xe0;break;
    case 2: case 3:break;
    case 4:r_[4]=value&255;break;
    case 5:command(value);break;
    case 14:r_[14]=value&127;break;
    case 15:if((value&1)!=r_[15])abort();r_[15]=value&1;break;
    case 16:slave_flags_&=~(value&0xf2);break;
    default:r_[index]=value&65535;break;
    }
}
bool I2c::validate(tlm::tlm_generic_payload& tx){
    tx.set_dmi_allowed(false);
    auto fail=[&](tlm::tlm_response_status s){tx.set_response_status(s);return false;};
    if(!tx.is_read()&&!tx.is_write())return fail(tlm::TLM_COMMAND_ERROR_RESPONSE);
    if(!tx.get_data_ptr())return fail(tlm::TLM_GENERIC_ERROR_RESPONSE);
    if(tx.get_address()>65535)return fail(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    if(tx.get_data_length()!=4||tx.get_streaming_width()<4)return fail(tlm::TLM_BURST_ERROR_RESPONSE);
    if(auto* be=tx.get_byte_enable_ptr()){
        if(!tx.get_byte_enable_length())return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        for(unsigned i=0;i<4;++i)if(be[i%tx.get_byte_enable_length()]!=255 && be[i%tx.get_byte_enable_length()]!=0)return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
    }return true;
}
void I2c::execute(tlm::tlm_generic_payload& tx){
    auto* d=tx.get_data_ptr();unsigned value=0;
    if(tx.is_write()){
        unsigned mask=0;for(unsigned i=0;i<4;++i)if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()]){
            mask|=255u<<(8*i);value|=unsigned(d[i])<<(8*i);
        }
        const unsigned index=(unsigned(tx.get_address())>>2)&31;
        // Commands/W1C use only enabled bits; storage merges internally without a bus read.
        if(mask)write(unsigned(tx.get_address()),(index==5 || index==16)?value:(read(unsigned(tx.get_address()))&~mask)|value);
    }
    else{value=read(unsigned(tx.get_address()));for(unsigned i=0;i<4;++i)if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()])d[i]=static_cast<unsigned char>(value>>(8*i));}
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}
void I2c::transport(tlm::tlm_generic_payload& tx,sc_core::sc_time& delay){
    auto incoming=delay;delay=sc_core::SC_ZERO_TIME;wait(incoming);
    struct Guard{sc_core::sc_mutex& m;explicit Guard(sc_core::sc_mutex& x):m(x){m.lock();}~Guard(){m.unlock();}}guard(mutex_);
    if(!validate(tx)){wait(config_.clock_period);return;}
    pending_=&tx;try{wait(completed_);}catch(...){if(pending_==&tx)pending_=nullptr;throw;}
}
unsigned I2c::debug(tlm::tlm_generic_payload& tx){
    if(!validate(tx))return 0;
    if(!tx.is_read()){tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);return 0;}
    execute(tx);return 4;
}
}
