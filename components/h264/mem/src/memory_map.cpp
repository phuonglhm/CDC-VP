#include "memory_map.h"
namespace h264::mem {
MemoryMap::MemoryMap(sc_core::sc_module_name name):sc_module(name) {
    internal_mem_.resize(INT_MEM_SIZE); external_mem_.resize(EXT_MEM_SIZE);
    socket.register_b_transport(this,&MemoryMap::b_transport);
}
void MemoryMap::b_transport(int,tlm::tlm_generic_payload& tx,sc_core::sc_time& delay) {
    tx.set_dmi_allowed(false);
    auto fail=[&](tlm::tlm_response_status s){tx.set_response_status(s);};
    if(!tx.is_read() && !tx.is_write()) { fail(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    const uint64_t a=tx.get_address(); const unsigned n=tx.get_data_length();
    auto* p=tx.get_data_ptr(); auto* be=tx.get_byte_enable_ptr(); const auto bn=tx.get_byte_enable_length();
    if(!p || !n || tx.get_streaming_width()<n) { fail(tlm::TLM_BURST_ERROR_RESPONSE); return; }
    if(be) {
        if(!bn) { fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
        for(unsigned i=0;i<bn;++i) if(be[i]!=0 && be[i]!=255) { fail(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return; }
    }
    auto inside=[&](uint64_t base,uint64_t size){return a>=base && a-base<size && n<=size-(a-base);};
    uint8_t* dst=nullptr;
    if(inside(INT_MEM_BASE,INT_MEM_SIZE)) dst=internal_mem_.data()+(a-INT_MEM_BASE);
    else if(inside(EXT_MEM_BASE,EXT_MEM_SIZE)) dst=external_mem_.data()+(a-EXT_MEM_BASE);
    else { fail(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    for(unsigned i=0;i<n;++i) if(!be || be[i%bn]==255) {
        if(tx.is_write()) dst[i]=p[i]; else p[i]=dst[i];
    }
    delay+=sc_core::sc_time(10,sc_core::SC_NS);
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
}
}
