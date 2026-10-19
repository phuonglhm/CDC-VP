#include "fu3_spi/spi.h"
#include <algorithm>
#include <stdexcept>
namespace fu3_spi {
Spi::Spi(sc_core::sc_module_name name,unsigned depth,sc_core::sc_time sys,sc_core::sc_time apb)
    :sc_module(name),depth_(depth),sys_(sys),apb_(apb) {
    if((depth!=16&&depth!=32&&depth!=64)||sys==sc_core::SC_ZERO_TIME||apb==sc_core::SC_ZERO_TIME)
        throw std::invalid_argument("SPI requires FIFO depth 16/32/64 and positive clock periods");
    reset();socket.register_b_transport(this,&Spi::transport);socket.register_transport_dbg(this,&Spi::debug);
    SC_THREAD(run);SC_METHOD(publish);sensitive<<changed_;
    SC_METHOD(observe_reset);sensitive<<apb_reset_n.neg()<<reset_n.neg();dont_initialize();
}
void Spi::observe_reset(){if(!apb_reset_n.read())++apb_epoch_;if(!reset_n.read())++sys_epoch_;}
void Spi::reset(){
    regs_={0xe0,0,0x11,0,0,0,0x100,0,0,0x10100,0,0};tx_.clear();rx_.clear();
    phase_=Phase::Idle;ticks_=edges_=flush_=tx_dma_=rx_dma_=0;bits_=8;shift_tx_=shift_rx_=0;
    slave_=loaded_=out_sck_=out_mosi_=out_miso_=irq_=ens_=false;prev_ss_=true;prev_sck_=false;
    tx_ack_={};rx_ack_={};has_set_={};
}
void Spi::run(){
    while(true){
        if(!reset_n.read()){
            reset();if(pending_){pending_->set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);pending_=nullptr;complete_.notify(sc_core::SC_ZERO_TIME);}
            changed_.notify(sc_core::SC_ZERO_TIME);wait(reset_n.posedge_event());continue;
        }
        wait(sys_,reset_n.negedge_event());if(!reset_n.read())continue;
        cycle();changed_.notify(sc_core::SC_ZERO_TIME);
    }
}
void Spi::flag(unsigned mask){regs_[8]|=mask;for(unsigned i=2;i<7;++i)if(mask&(1u<<i)){last_set_[i]=sc_core::sc_time_stamp();has_set_[i]=true;}}
unsigned Spi::interrupt_status()const {
    return regs_[8]|(tx_.size()<=(regs_[6]&127)?1u:0u)|
        (!rx_.empty()&&rx_.size()>=((regs_[6]>>8)&127)?2u:0u);
}
unsigned Spi::status()const{
    bool busy=(regs_[0]&2)?phase_!=Phase::Idle:!ss_n_in.read();
    return unsigned(busy)|(tx_.empty()?2u:0u)|(tx_.size()<depth_?4u:0u)|(!rx_.empty()?8u:0u)|
        (rx_.size()==depth_?16u:0u)|(ens_?32u:0u)|(unsigned(tx_.size())<<8)|(unsigned(rx_.size())<<16);
}
void Spi::dma(unsigned& state,bool on,bool condition,bool ack){
    if(!on)state=0;
    else if(state==0){if(condition)state=1;}
    else if(state==1){if(ack)state=2;}
    else if(!ack)state=0;
}
void Spi::cycle(){
    const auto old_ctrl=regs_[0];irq_=(interrupt_status()&regs_[7]&127)!=0;
    ens_=(old_ctrl&1)||phase_!=Phase::Idle||slave_;
    if(pending_){execute(*pending_);pending_=nullptr;complete_.notify(sc_core::SC_ZERO_TIME);}
    // Flush work is hardware: clearing a completed request wins over a same-tick write.
    if(old_ctrl&FU3_SPI_RXFLUSH){if(!rx_.empty())rx_.pop_front();if(rx_.empty())regs_[0]&=~FU3_SPI_RXFLUSH;}
    if(flush_==0){if(old_ctrl&FU3_SPI_TXFLUSH)flush_=1;}
    else if(flush_==1){if(!tx_.empty())tx_.pop_front();if(tx_.empty()){flush_=2;regs_[0]&=~FU3_SPI_TXFLUSH;}}
    else if(flush_==2)flush_=3;
    else flush_=0;
    if(!(regs_[0]&1)) {phase_=Phase::Idle;slave_=false;out_sck_=false;}
    else if(regs_[0]&2){slave_=false;master();}
    else {phase_=Phase::Idle;out_sck_=false;slave();}
    const auto on=(regs_[0]&1)!=0;
    dma(tx_dma_,on&&(regs_[9]&1),depth_-tx_.size()>=std::max(1u,(regs_[9]>>8)&127),tx_ack_[1]);
    dma(rx_dma_,on&&(regs_[9]&2),rx_.size()>=std::max(1u,(regs_[9]>>16)&127),rx_ack_[1]);
    tx_ack_[1]=tx_ack_[0];tx_ack_[0]=dma_tx_ack.read();rx_ack_[1]=rx_ack_[0];rx_ack_[0]=dma_rx_ack.read();
    prev_ss_=ss_n_in.read();prev_sck_=sck_in.read();
}
void Spi::load(bool pop){
    bits_=std::max(4u,((regs_[0]>>5)&31)+1);edges_=0;shift_rx_=0;
    loaded_=!tx_.empty();shift_tx_=loaded_?tx_.front():0;if(pop&&loaded_)tx_.pop_front();
    if(!(regs_[0]&8)){
        bool bit=(shift_tx_>>((regs_[0]&16)?0:bits_-1))&1;
        if(pop)out_mosi_=bit;else out_miso_=bit;
    }
}
void Spi::edge(bool input,bool master_mode){
    const bool leading=(edges_%2)==0;
    const bool sample=(regs_[0]&8)?!leading:leading;
    unsigned bit=edges_/2;
    if(sample){const unsigned position=(regs_[0]&16)?bit:bits_-1-bit;if(input)shift_rx_|=1u<<position;}
    else {
        if(!(regs_[0]&8))++bit;
        if(bit<bits_){const unsigned position=(regs_[0]&16)?bit:bits_-1-bit;
            if(master_mode)out_mosi_=(shift_tx_>>position)&1;else out_miso_=(shift_tx_>>position)&1;}
    }
    ++edges_;
    if(edges_==2*bits_ && !(regs_[0]&FU3_SPI_TXONLY)){
        if(rx_.size()<depth_)rx_.push_back(shift_rx_);else flag(FU3_SPI_OVR);
    }
}
void Spi::master(){
    const auto half=half_cycles();const bool automatic=regs_[2]&16;
    if(phase_!=Phase::Shift)out_sck_=(regs_[0]&4)!=0;
    if(phase_==Phase::Idle){
        if(!ens_||tx_.empty()||!space()||flush_)return;
        load(true);phase_=Phase::Setup;ticks_=(automatic?2*(regs_[11]&255)+1:1)*half;return;
    }
    if(ticks_ && --ticks_)return;
    switch(phase_){
    case Phase::Setup:phase_=Phase::Shift;[[fallthrough]];
    case Phase::Shift:
        out_sck_=!out_sck_;edge(miso_in.read(),true);ticks_=half;
        if(edges_==2*bits_){
            if(!tx_.empty()&&!flush_){
                const unsigned ifd=(regs_[11]>>16)&255;
                // Only a continuous frame may pop immediately. During GAP the
                // queued word must remain visible to flush, status and DMA.
                if(ifd==0&&space()){load(true);phase_=Phase::Setup;ticks_=half;}
                else {phase_=Phase::Gap;ticks_=std::max(1u,2*ifd*half);}
            }
            else if(automatic){phase_=Phase::Hold;ticks_=(2*((regs_[11]>>8)&255)+1)*half;}
            else {phase_=Phase::Idle;flag(FU3_SPI_EOT);}
        }break;
    case Phase::Gap:
        if(flush_){ticks_=1;break;}
        // A flush can remove all waiting words even while RX remains full.
        // Finishing the transfer does not require space for another RX frame.
        if(tx_.empty()){
            if(automatic){phase_=Phase::Hold;ticks_=(2*((regs_[11]>>8)&255)+1)*half;}
            else {phase_=Phase::Idle;flag(FU3_SPI_EOT);}
            break;
        }
        if(!space()){ticks_=1;break;}
        load(true);phase_=Phase::Setup;ticks_=half;break;
    case Phase::Hold:flag(FU3_SPI_EOT);phase_=Phase::CsIdle;ticks_=2*((regs_[11]>>24)&255)*half+1;break;
    case Phase::CsIdle:phase_=Phase::Idle;break;
    default:break;
    }
}
void Spi::slave(){
    const bool ss=ss_n_in.read(),clock=sck_in.read();
    if(ss){if(slave_){flag(FU3_SPI_EOT);slave_=false;}return;}
    if(!slave_){slave_=true;load(false);return;}
    if(clock!=prev_sck_){
        if(edges_==0){if(!loaded_)flag(FU3_SPI_UDR);else if(!tx_.empty())tx_.pop_front();}
        edge(mosi_in.read(),false);
        if(edges_==2*bits_)load(false);
    }
}
void Spi::publish(){
    const bool active=reset_n.read(),master_mode=active&&(regs_[0]&2),enabled=active&&(regs_[0]&1);
    unsigned select=15;
    const bool automatic=regs_[2]&16;
    if(master_mode&&enabled&&((automatic&&phase_!=Phase::Idle&&phase_!=Phase::CsIdle)||(!automatic&&(regs_[2]&32))))select&=~regs_[2];
    ss_n.write(select&15);sck.write(active&&out_sck_);mosi.write(active&&out_mosi_);miso.write(active&&out_miso_);
    sck_oe.write(master_mode);ss_oe.write(master_mode);mosi_oe.write(master_mode);miso_oe.write(enabled&&!master_mode&&!ss_n_in.read());
    irq.write(active&&irq_);dma_tx_req.write(active&&tx_dma_==1);dma_rx_req.write(active&&rx_dma_==1);
}
unsigned Spi::read(unsigned off,bool pop){
    if(off>=48)return 0;
    switch(off){case 12:return 0;case 16:{unsigned v=rx_.empty()?0:rx_.front();if(pop){if(rx_.empty())flag(FU3_SPI_RXUDF);else rx_.pop_front();}return v;}
    case 20:return status();case 32:return interrupt_status();case 40:return (depth_-tx_.size())|((depth_-rx_.size())<<16);default:return regs_[off/4];}
}
void Spi::write(unsigned off,unsigned value,unsigned mask){
    if(off>=48)return;
    if(off==12){if(tx_.size()==depth_)flag(FU3_SPI_TXOVF);else tx_.push_back(value);return;}
    if(off==32){for(unsigned i=2;i<7;++i)if((mask&value&(1u<<i))&&!(has_set_[i]&&last_set_[i]==sc_core::sc_time_stamp()))regs_[8]&=~(1u<<i);return;}
    static constexpr unsigned masks[12]={0x1fff,0xffff,0x3f,0,0,0,0x7f7f,0x7f,0,0x7f7f03,0,0xffffffff};
    mask&=masks[off/4];regs_[off/4]=(regs_[off/4]&~mask)|(value&mask);
}
bool Spi::validate(tlm::tlm_generic_payload& t){
    t.set_dmi_allowed(false);auto fail=[&](tlm::tlm_response_status s){t.set_response_status(s);return false;};
    if(!t.is_read()&&!t.is_write())return fail(tlm::TLM_COMMAND_ERROR_RESPONSE);
    if(!t.get_data_ptr())return fail(tlm::TLM_GENERIC_ERROR_RESPONSE);
    unsigned n=t.get_data_length();auto a=t.get_address();
    if((n!=1&&n!=2&&n!=4)||t.get_streaming_width()<n)return fail(tlm::TLM_BURST_ERROR_RESPONSE);
    if(a>=65536||(n<4&&(a%n||(a&3)+n>4)))return fail(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    if(t.is_write()&&(a&~3ull)==12&&n!=4)return fail(tlm::TLM_BURST_ERROR_RESPONSE);
    if(auto* b=t.get_byte_enable_ptr()){if(!t.get_byte_enable_length())return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        for(unsigned i=0;i<n;++i)if(b[i%t.get_byte_enable_length()]!=0&&b[i%t.get_byte_enable_length()]!=255)return fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);}
    return true;
}
void Spi::execute(tlm::tlm_generic_payload& t){
    const unsigned off=static_cast<unsigned>(t.get_address())&~3u;
    const unsigned lane=t.get_data_length()==4?0:unsigned(t.get_address()&3);
    unsigned value=0,mask=0;
    if(t.is_write()){
        for(unsigned i=0;i<t.get_data_length();++i){value|=unsigned(t.get_data_ptr()[i])<<(8*(lane+i));
            if(!t.get_byte_enable_ptr()||t.get_byte_enable_ptr()[i%t.get_byte_enable_length()])mask|=255u<<(8*(lane+i));}
        write(off,value,mask);
    }else{
        value=read(off,true);for(unsigned i=0;i<t.get_data_length();++i)
            if(!t.get_byte_enable_ptr()||t.get_byte_enable_ptr()[i%t.get_byte_enable_length()])t.get_data_ptr()[i]=value>>(8*(lane+i));
    }
    t.set_response_status(tlm::TLM_OK_RESPONSE);
}
void Spi::transport(tlm::tlm_generic_payload& t,sc_core::sc_time& delay){
    const auto d=delay;delay=sc_core::SC_ZERO_TIME;wait(d+apb_);
    if(!validate(t))return;
    struct Guard{sc_core::sc_mutex& m;explicit Guard(sc_core::sc_mutex& x):m(x){m.lock();}~Guard(){m.unlock();}}guard(access_);
    if(!reset_n.read()||!apb_reset_n.read()){t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
    const auto epoch=apb_epoch_,system=sys_epoch_;pending_=&t;
    try{wait(complete_);}catch(...){if(pending_==&t)pending_=nullptr;throw;}
    wait(3*apb_);
    if(epoch!=apb_epoch_||system!=sys_epoch_||!apb_reset_n.read()||!reset_n.read())t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
}
unsigned Spi::debug(tlm::tlm_generic_payload& t){
    if(!validate(t))return 0;
    if(!t.is_read()){t.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);return 0;}
    if(!reset_n.read()){t.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);return 0;}
    execute(t);changed_.notify(sc_core::SC_ZERO_TIME);return t.get_data_length();
}
}
