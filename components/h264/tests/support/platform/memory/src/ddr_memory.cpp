#include <h264/platform/ddr_memory.h>
namespace h264 {
DdrMemory::DdrMemory(sc_core::sc_module_name name,size_t bytes) : sc_module(name),bytes_(bytes,0) {
    socket.register_b_transport(this,&DdrMemory::transport);
}
void DdrMemory::transport(int,tlm::tlm_generic_payload& tx,sc_core::sc_time& delay) {
    consume_delay(delay);
    tx.set_dmi_allowed(false);
    const uint64_t a=tx.get_address(); const unsigned n=tx.get_data_length();
    auto* be=tx.get_byte_enable_ptr(); const unsigned bel=tx.get_byte_enable_length();
    if (!tx.is_read() && !tx.is_write()) { tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    if (!n || !tx.get_data_ptr() || tx.get_streaming_width()<n) { tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return; }
    if (a>bytes_.size() || n>bytes_.size()-a) { tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    if (be && !bel) { tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
    if (be) for(unsigned i=0;i<bel;++i) if(be[i]!=0 && be[i]!=0xff) {
        tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return;
    }
    lock_.lock();
    in_flight=true; active_write=tx.is_write(); active_address=a;
    // Commit AFTER latency, allowing reset/fault tests to observe in-flight writes.
    wait(latency);
    if (!epoch_valid(tx)) tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
    else if (a<error_end && a+n>error_begin) tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    else {
        for (unsigned i=0;i<n;++i) if (!be || be[i%bel]) {
            if (tx.is_write()) bytes_[a+i]=tx.get_data_ptr()[i];
            else tx.get_data_ptr()[i]=bytes_[a+i];
        }
        trace.push_back({tx.is_write(),a,n,sc_core::sc_time_stamp()});
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    in_flight=false;
    lock_.unlock();
}
}
