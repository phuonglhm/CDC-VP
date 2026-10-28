#include "fu3_hyperram/controller.h"
#include <algorithm>
#include <stdexcept>
namespace fu3_hyperram {
Controller::Controller(sc_core::sc_module_name n,Config c):sc_module(n),config_(c),ram_(memory_size,0) {
    if(c.core_period<=sc_core::SC_ZERO_TIME)throw std::invalid_argument("HyperRAM clock period must be positive");
    cr1_=c.industrial_plus?0xffc2:0xffc1;
    reads_.capacity(4);writes_.capacity(4);
    memory.register_b_transport(this,&Controller::memory_transport);
    csr.register_b_transport(this,&Controller::csr_transport);csr.register_transport_dbg(this,&Controller::debug);
    SC_METHOD(reset);sensitive<<reset_n.neg()<<device_reset_n.neg();
    SC_METHOD(publish);sensitive<<changed_;
    SC_THREAD(worker);
}
void Controller::configure_bus(fu3_bus::BusConfig& config) {
    for(auto& t:config.targets)if(t.name=="HYPERBUS") {
        t.preserve_burst=true;t.word_interface=false;t.outstanding=8;t.non_executable=true;
    }
}
void Controller::reset() {
    if(reset_n.read()&&device_reset_n.read())return;
    ++epoch_;r_.fill(0);active_.fill(0);valid_=pending_=done_=command_error_=false;
    if(!device_reset_n.read()) {
        std::fill(ram_.begin(),ram_.end(),0);cr0_=0x8f2f;cr1_=config_.industrial_plus?0xffc2:0xffc1;
    }
    cancel_.notify(sc_core::SC_ZERO_TIME);changed_.notify(sc_core::SC_ZERO_TIME);
}
void Controller::publish(){irq=bool(r_[reg::ERRORS/4]&r_[reg::IRQ_ENABLE/4]);}
bool Controller::coherent() const {
    const unsigned encoding=(cr0_>>4)&15;
    const unsigned lc=encoding==14?3:encoding==15?4:encoding<=2?encoding+5:0;
    return lc==(active_[0]&15)&&bool(cr0_&8)==bool(active_[0]&256);
}
bool Controller::ready() const{return reset_n.read()&&device_reset_n.read()&&(r_[2]&1)&&valid_&&coherent()&&!pending_;}
unsigned Controller::status() const {
    return unsigned(ready())*reg::READY|unsigned(pending_)*reg::PENDING|unsigned(accepted_!=0)*reg::BUSY|
        unsigned(!(r_[2]&1)&&!accepted_&&!pending_)*reg::QUIESCED|unsigned(valid_)*reg::VALID|
        unsigned(done_)*reg::DONE|unsigned(command_error_)*reg::COMMAND_ERROR;
}
bool Controller::pause(unsigned cycles,unsigned epoch) {
    // Core cycles elapse only while the core clock runs (gate pauses them).
    auto remaining=config_.core_period*double(cycles);
    while(true){
        if(epoch!=epoch_||!reset_n.read()||!device_reset_n.read())return false;
        if(!clock_enabled_){wait(clock_gate_changed_|cancel_);continue;}
        const auto start=sc_core::sc_time_stamp();
        wait(remaining,cancel_|clock_gate_changed_);
        const auto elapsed=sc_core::sc_time_stamp()-start;
        if(elapsed>=remaining&&clock_enabled_)
            return epoch==epoch_&&reset_n.read()&&device_reset_n.read();
        remaining=elapsed>=remaining?sc_core::SC_ZERO_TIME:remaining-elapsed;
    }
}
void Controller::fault(unsigned code,unsigned address,unsigned words,bool partial) {
    if(!r_[reg::ERRORS/4]){r_[reg::FAULT_ADDRESS/4]=address;r_[reg::FAULT_WORDS/4]=words;r_[reg::FAULT_FLAGS/4]=partial?1:0;}
    r_[reg::ERRORS/4]|=code;changed_.notify(sc_core::SC_ZERO_TIME);
}
bool Controller::config_valid(const std::array<unsigned,8>& q) const {
    const auto lc=q[0]&15;
    if((q[0]&~0x10fu)||lc<3||lc>7||!q[1]||!q[2]||!q[3]||!q[4])return false;
    const double mhz=1e-6/config_.core_period.to_seconds();
    const unsigned maximum[]{85,104,133,166,200};
    if(mhz>maximum[lc-3]+0.001)return false;
    const auto max_cs=sc_core::sc_time(config_.industrial_plus?1:4,sc_core::SC_US);
    return config_.core_period*double(q[1])<=max_cs;
}
bool Controller::shape(tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    if(!tx.is_read()&&!tx.is_write()){tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);return false;}
    if(!tx.get_data_ptr()||!tx.get_data_length()||tx.get_streaming_width()<tx.get_data_length()) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);return false;
    }
    if(auto* be=tx.get_byte_enable_ptr()) {
        if(!tx.get_byte_enable_length()){tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);return false;}
        for(unsigned i=0;i<tx.get_byte_enable_length();++i)if(be[i]!=0&&be[i]!=255) {
            tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);return false;
        }
    }
    return true;
}
void Controller::memory_transport(tlm::tlm_generic_payload& tx,sc_core::sc_time& delay) {
    const auto incoming=delay;delay=sc_core::SC_ZERO_TIME;wait(incoming);wait(sc_core::SC_ZERO_TIME);
    if(!shape(tx)){wait(config_.core_period);return;}
    const auto address=tx.get_address();const auto size=tx.get_data_length();
    auto reject=[&](unsigned code,tlm::tlm_response_status response){fault(code,unsigned(address));tx.set_response_status(response);wait(config_.core_period);};
    if(address>=memory_size||size>memory_size-address){reject(reg::DECODE,tlm::TLM_ADDRESS_ERROR_RESPONSE);return;}
    const auto* a=tx.get_extension<fu3_bus::Access>();
    if(a && a->instruction){reject(reg::DECODE,tlm::TLM_COMMAND_ERROR_RESPONSE);return;}
    if(!a||a->beat_bytes!=4||a->burst!=fu3_bus::Burst::Increment||a->ordering_id>=64||
       address%4||size%4||size>1024||size>4096-(address&4095)) {
        reject(reg::PROFILE,tlm::TLM_BURST_ERROR_RESPONSE);return;
    }
    const unsigned epoch=epoch_;
    fu3_bus::Admission::Lease admission(tx.is_write()?writes_:reads_);
    if(epoch!=epoch_||!ready()){reject(reg::NOT_READY,tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
    struct Count {unsigned& n;explicit Count(unsigned& v):n(v){++n;}~Count(){--n;}} count(accepted_);
    fu3_bus::Fifo::Lease engine(engine_);
    if(epoch!=epoch_){tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
    const unsigned words=size/4,lc=active_[0]&15;
    // Conservative full-burst admission uses worst-case doubled variable latency.
    const unsigned overhead=3+2*lc+active_[2]+active_[3];
    if(std::uint64_t(overhead)+words>active_[1]){reject(reg::TIMING,tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
    auto* injection=tx.get_extension<Fault>();
    const unsigned fail_after=injection?injection->fail_after_words:~0u;
    const bool doubled=(active_[0]&256)||(injection&&injection->refresh);
    if(!pause(3+(doubled?2:1)*lc+active_[2],epoch)){tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
    ++r_[reg::TRANSACTIONS/4];r_[reg::LAST_WORDS/4]=words;
    auto* data=tx.get_data_ptr();auto* be=tx.get_byte_enable_ptr();
    bool failed=false;unsigned completed=0;
    for(unsigned w=0;w<words;++w) {
        if(w==fail_after){failed=true;break;}
        if(!pause(1,epoch)){tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
        for(unsigned lane=0;lane<4;++lane){const unsigned i=w*4+lane;
            if(!be||be[i%tx.get_byte_enable_length()]) {
                if(tx.is_write())ram_[address+i]=data[i];else data[i]=ram_[address+i];
            }
        }
        ++completed;
    }
    if(!pause(active_[3]+active_[4],epoch)){tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
    if(failed){
        if(tx.is_read())for(unsigned i=completed*4;i<size;++i)if(!be||be[i%tx.get_byte_enable_length()])data[i]=0;
        fault(tx.is_write()?reg::UNDERFLOW:reg::READ_TIMEOUT,unsigned(address),completed,tx.is_write()&&completed);
    }
    tx.set_response_status(failed?tlm::TLM_GENERIC_ERROR_RESPONSE:tlm::TLM_OK_RESPONSE);
}
unsigned Controller::read(unsigned offset) const {
    if(offset==reg::ID)return 0x48525650; // HRVP: identifies a provisional VP ABI.
    if(offset==reg::VERSION)return 1;
    if(offset==reg::STATUS)return status();
    if(offset==reg::ACTIVE_LATENCY)return valid_?active_[0]:0;
    if(offset==reg::APPLY||offset==reg::DEV_CMD||offset==reg::DELAY_APPLY||offset==reg::ERROR_CLEAR)return 0;
    return offset/4<r_.size()?r_[offset/4]:0;
}
void Controller::write(unsigned offset,unsigned value) {
    if(offset==reg::ERROR_CLEAR){r_[reg::ERRORS/4]&=~value;
        if(!r_[reg::ERRORS/4])r_[reg::FAULT_ADDRESS/4]=r_[reg::FAULT_WORDS/4]=r_[reg::FAULT_FLAGS/4]=0;
        return;}
    if(offset==reg::APPLY||offset==reg::DEV_CMD||offset==reg::DELAY_APPLY) {
        if(!value)return;
        if(pending_){fault(reg::COMMAND_BUSY);return;}
        operation_=offset;request_.fill(0);
        if(offset==reg::APPLY)for(unsigned i=0;i<5;++i)request_[i]=r_[4+i];
        else if(offset==reg::DEV_CMD){request_[0]=value;request_[1]=r_[12];request_[2]=r_[13];}
        else {request_[0]=r_[21];request_[1]=r_[22];}
        pending_=true;done_=command_error_=false;work_.notify(sc_core::SC_ZERO_TIME);return;
    }
    switch(offset){
    case reg::CTRL:r_[2]=value&1;break;
    case reg::LATENCY:case reg::TCSM:case reg::SETUP:case reg::HOLD:case reg::RECOVERY:
    case reg::DEV_ADDR:case reg::DEV_WDATA:case reg::DELAY0:case reg::DELAY1:r_[offset/4]=value&65535;break;
    case reg::IRQ_ENABLE:r_[offset/4]=value&255;break;
    default:break;
    }
}
void Controller::worker() {
    while(true){
        while(!pending_)wait(work_);
        const unsigned epoch=epoch_,op=operation_;const auto q=request_;
        fu3_bus::Fifo::Lease engine(engine_);
        if(!pause(4,epoch))continue;
        bool ok=true;
        if(op==reg::APPLY){ok=config_valid(q);if(ok){std::copy_n(q.begin(),5,active_.begin());valid_=true;++r_[11];}}
        else if(op==reg::DELAY_APPLY){ok=q[0]<32&&q[1]<32;if(ok)r_[24]=q[0]|(q[1]<<8);}
        else if(op==reg::DEV_CMD){
            if(q[0]==1&&(valid_?!coherent():(cr0_&0xf8)!=0x28))ok=false;
            else if(q[0]==1){switch(q[1]){case 0:r_[15]=0xe76;break;case 1:r_[15]=9;break;
                case 2:r_[15]=cr0_;break;case 3:r_[15]=cr1_;break;default:ok=false;}}
            else if(q[0]==2&&q[1]==2){const auto value=q[2]&65535;const auto lc=(value>>4)&15;
                ok=(value&0x8f00)==0x8f00&&(lc<=2||lc>=14);
                if(ok)cr0_=value;
            }else if(q[0]==2&&q[1]==3){ // Normal, full-refresh, static single-ended clock profile only.
                ok=(q[2]&0xfffc)==0xffc0;if(ok)cr1_=(q[2]&0xfffc)|(cr1_&3);
            }else ok=false;
        }else ok=false;
        if(!ok)fault(reg::CONFIG);
        pending_=false;done_=true;command_error_=!ok;changed_.notify(sc_core::SC_ZERO_TIME);
    }
}
void Controller::csr_transport(tlm::tlm_generic_payload& tx,sc_core::sc_time& delay) {
    const auto incoming=delay;delay=sc_core::SC_ZERO_TIME;wait(incoming);
    const unsigned epoch=epoch_;if(!pause(1,epoch)){tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
    if(!shape(tx))return;
    if(tx.get_address()>=65536||tx.get_address()%4){tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);return;}
    if(tx.get_data_length()!=4){tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);return;}
    auto* data=tx.get_data_ptr();unsigned value=0;
    if(tx.is_write()){
        unsigned mask=0;for(unsigned i=0;i<4;++i)if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()]){
            mask|=255u<<(8*i);value|=unsigned(data[i])<<(8*i);
        }
        const auto off=unsigned(tx.get_address());
        const bool command=off==reg::APPLY || off==reg::DEV_CMD || off==reg::DELAY_APPLY || off==reg::ERROR_CLEAR;
        if(mask)write(off,command?value:(read(off)&~mask)|value);
    }
    else{value=read(unsigned(tx.get_address()));for(unsigned i=0;i<4;++i)if(!tx.get_byte_enable_ptr() || tx.get_byte_enable_ptr()[i%tx.get_byte_enable_length()])data[i]=static_cast<unsigned char>(value>>(8*i));}
    tx.set_response_status(tlm::TLM_OK_RESPONSE);changed_.notify(sc_core::SC_ZERO_TIME);
}
unsigned Controller::debug(tlm::tlm_generic_payload& tx) {
    if(!shape(tx))return 0;
    if(!tx.is_read()||tx.get_data_length()!=4||tx.get_address()%4||tx.get_address()>=65536){tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);return 0;}
    const auto value=read(unsigned(tx.get_address()));for(unsigned i=0;i<4;++i)tx.get_data_ptr()[i]=static_cast<unsigned char>(value>>(8*i));
    tx.set_response_status(tlm::TLM_OK_RESPONSE);return 4;
}
}
