#include <h264/top/dma_transport.h>
namespace h264 {
DmaTransport::DmaTransport(sc_core::sc_module_name name,unsigned width,unsigned burst)
    :sc_module(name),bus_bytes(width/8),max_beats(burst) {
    config_.data_width_bits=width; config_.burst_limit=burst;
    AxiMasterBridge validate(config_);
    clients.register_b_transport(this,&DmaTransport::transport);
    SC_THREAD(service);
}
void DmaTransport::set_client(int owner,ClientId c) {
    if(!idle() || owner<0 || unsigned(c)>=4) throw std::invalid_argument("DMA client mapping");
    identities_[owner]=c;
}
bool DmaTransport::wait_idle(sc_core::sc_time timeout) {
    if(timeout<sc_core::SC_ZERO_TIME) throw std::invalid_argument("negative drain timeout");
    auto deadline=sc_core::sc_time_stamp()+timeout;
    while(!idle() && sc_core::sc_time_stamp()<deadline) wait(deadline-sc_core::sc_time_stamp(),changed_);
    return idle();
}
void DmaTransport::transport(int owner,tlm::tlm_generic_payload& tx,sc_core::sc_time& delay) {
    consume_delay(delay); tx.set_dmi_allowed(false);
    auto* be=tx.get_byte_enable_ptr(); unsigned bel=tx.get_byte_enable_length(),n=tx.get_data_length();
    if(!tx.is_read() && !tx.is_write()) { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    if(!n || !tx.get_data_ptr() || tx.get_streaming_width()<n) { tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return; }
    if(tx.get_address()>0xffffffffULL || n>(1ull<<32)-tx.get_address()) { tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    if(be && !bel) { tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
    if(be) for(unsigned i=0;i<bel;++i) if(be[i]!=0 && be[i]!=255) { tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
    auto client=identities_.count(owner)?identities_.at(owner):ClientId::CMB;
    if(auto* ext=tx.get_extension<DmaClientExtension>()) client=ext->client;
    if(unsigned(client)>=4) { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    Pending pending; pending.tx=&tx; pending.owner=owner;
    DmaRequest req; req.client=client; req.addr=tx.get_address(); req.beats=n;
    req.size=TransferSize::B1; req.is_write=tx.is_write(); req.tag=++sequence_;
    if(pending_.count(req.tag)) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
    if(req.is_write) req.data.assign(tx.get_data_ptr(),tx.get_data_ptr()+n);
    pending_.emplace(req.tag,&pending);
    arb_.request(req,&pending); changed_.notify(sc_core::SC_ZERO_TIME);
    while(!pending.done) wait(pending.completed);
    tx.set_response_status(pending.status);
}
void DmaTransport::access(bool wr,uint64_t a,uint8_t* data,size_t n) {
    auto& caller=*active_->tx;
    if(!epoch_valid(caller)) { active_->status=tlm::TLM_GENERIC_ERROR_RESPONSE; throw std::out_of_range("stale DMA"); }
    tlm::tlm_generic_payload tx;
    tx.set_command(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
    tx.set_address(a); tx.set_data_ptr(data); tx.set_data_length(unsigned(n)); tx.set_streaming_width(unsigned(n));
    auto* epoch=caller.get_extension<EpochExtension>();
    if(epoch) tx.set_extension(epoch);
    sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
    try { memory->b_transport(tx,delay); consume_delay(delay); }
    catch(...) { if(epoch) tx.clear_extension<EpochExtension>(); throw; }
    if(epoch) tx.clear_extension<EpochExtension>();
    if(tx.is_response_error() || !epoch_valid(caller)) {
        active_->status=tx.is_response_error()?tx.get_response_status():tlm::TLM_GENERIC_ERROR_RESPONSE;
        throw std::out_of_range("DMA memory response");
    }
}
void DmaTransport::service() {
    for(;;) {
        while(!arb_.busy()) wait(changed_);
        wait(sc_core::SC_ZERO_TIME); // Collect simultaneous requests; core selects priority.
        auto client=arb_.pick(); arb_.acquire(client);
        auto request=arb_.granted_request();
        active_=pending_.at(request.tag);
        auto& tx=*active_->tx;
        const auto begin=sc_core::sc_time_stamp();
        try {
            if(!epoch_valid(tx)) throw std::out_of_range("stale request");
            auto* be=tx.get_byte_enable_ptr(); auto bel=tx.get_byte_enable_length();
            // Keep the grant for the whole TLM request, including all enabled runs.
            for(unsigned first=0;first<tx.get_data_length();) {
                if(be && !be[first%bel]) { ++first; continue; }
                unsigned end=first+1;
                while(end<tx.get_data_length() && (!be || be[end%bel])) ++end;
                auto run=request; run.addr+=first; run.beats=end-first;
                if(run.is_write) run.data.assign(request.data.begin()+first,request.data.begin()+end);
                AxiMasterBridge bridge(config_);
                bridge.begin(run,*this);
                auto state=AxiProgress::Pending;
                while(state==AxiProgress::Pending) {
                    state=bridge.advance();
                    if(state==AxiProgress::Pending) wait(sc_core::SC_ZERO_TIME);
                }
                for(const auto& s:bridge.issued_log())
                    segments.push_back({s.addr,(s.len+1)*s.size_bytes,s.len+1,sc_core::sc_time_stamp()});
                if(state!=AxiProgress::Success) throw std::out_of_range("DMA bridge failure");
                if(tx.is_read()) {
                    const auto& data=bridge.response().data;
                    std::copy(data.begin(),data.end(),tx.get_data_ptr()+first);
                }
                first=end;
            }
        } catch(const std::exception&) {
            if(active_->status==tlm::TLM_OK_RESPONSE) active_->status=tlm::TLM_GENERIC_ERROR_RESPONSE;
        }
        trace.push_back({active_->owner,tx.get_address(),tx.get_data_length(),begin,sc_core::sc_time_stamp()});
        const bool ok=active_->status==tlm::TLM_OK_RESPONSE;
        arb_.complete(ok); pending_.erase(request.tag); active_=nullptr;
        changed_.notify(sc_core::SC_ZERO_TIME);
    }
}
}
