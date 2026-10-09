#include <h264/dma/dma_arbiter.h>
namespace h264 {
DmaArbiter::DmaArbiter(sc_core::sc_module_name name) : sc_module(name) {
    clients.register_b_transport(this, &DmaArbiter::transport);
    SC_THREAD(dispatch);
}
bool DmaArbiter::wait_idle(sc_core::sc_time timeout) {
    if(timeout<sc_core::SC_ZERO_TIME) throw std::invalid_argument("negative drain timeout");
    const auto deadline=sc_core::sc_time_stamp()+timeout;
    while(!idle() && sc_core::sc_time_stamp()<deadline)
        wait(deadline-sc_core::sc_time_stamp(),changed_);
    return idle();
}
void DmaArbiter::dispatch() {
    for(;;) {
        wait(changed_);
        wait(sc_core::SC_ZERO_TIME); // Collect same-time requests before choosing.
        if(busy_ || waiting_.empty()) continue;
        size_t selected=0;
        bool aged=false;
        for(size_t i=0;i<waiting_.size();++i) {
            if(waiting_[i]->bypasses>=8) { selected=i; aged=true; break; }
        }
        if(!aged) for(size_t i=1;i<waiting_.size();++i)
            if(priorities_[waiting_[i]->owner]<priorities_[waiting_[selected]->owner]) selected=i;
        auto* winner=waiting_[selected];
        waiting_.erase(waiting_.begin()+selected);
        for(auto* request:waiting_) ++request->bypasses;
        busy_=true; winner->granted=true; winner->grant.notify(sc_core::SC_ZERO_TIME);
    }
}
void DmaArbiter::transport(int owner, tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    consume_delay(delay);
    Pending request; request.owner=owner;
    waiting_.push_back(&request); changed_.notify(sc_core::SC_ZERO_TIME);
    while(!request.granted) wait(request.grant);
    const auto begin = sc_core::sc_time_stamp();
    try {
        if (epoch_valid(tx)) {
            memory->b_transport(tx, delay);
            consume_delay(delay);
        } else tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    } catch (...) { busy_=false; changed_.notify(sc_core::SC_ZERO_TIME); throw; }
    trace.push_back({owner,tx.get_address(),tx.get_data_length(),begin,sc_core::sc_time_stamp()});
    busy_=false; changed_.notify(sc_core::SC_ZERO_TIME);
}
}
