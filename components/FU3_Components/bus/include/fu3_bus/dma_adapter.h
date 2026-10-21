#pragma once
#include "fu3/dma/transfer_extension.h"
#include <fu3_bus/access.h>
#include <fu3_bus/transaction.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <vector>

namespace fu3_bus {
// Optional bus-owned header-only integration: link both DMA and fu3::bus_system.
// The native DMA library remains usable with ordinary TLM targets without the bus.
class DmaAdapter : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<DmaAdapter> input{"input"};
    tlm_utils::simple_initiator_socket<DmaAdapter> output{"output"};
    explicit DmaAdapter(sc_core::sc_module_name name):sc_module(name) {
        input.register_b_transport(this,&DmaAdapter::transport);
    }
private:
    void transport(tlm::tlm_generic_payload& tx,sc_core::sc_time& delay) {
        fu3_bus::consume_delay(delay);
        auto reject=[&](tlm::tlm_response_status status){
            tx.set_dmi_allowed(false);tx.set_response_status(status);
            // Bound invalid-request polling without retaining any fabric resource.
            sc_core::wait(1,sc_core::SC_NS);
        };
        if(!fu3_bus::validate(tx)){reject(tx.get_response_status());return;}
        const auto* native=tx.get_extension<fu3::dma::TransferExtension>();
        if(!native){reject(tlm::TLM_GENERIC_ERROR_RESPONSE);return;}
        const auto unit=native->unit_bytes;
        if((unit!=1 && unit!=2 && unit!=4) || !native->beats || native->beats>16 ||
            tx.get_data_length()!=unit*native->beats){reject(tlm::TLM_BURST_ERROR_RESPONSE);return;}
        if(tx.get_address()%unit){reject(tlm::TLM_ADDRESS_ERROR_RESPONSE);return;}
        const auto burst=native->increment?fu3_bus::Burst::Increment:fu3_bus::Burst::Fixed;
        if(const auto* prior=tx.get_extension<fu3_bus::Access>())
            if(prior->beat_bytes!=unit || prior->burst!=burst || prior->instruction || prior->debug ||
               prior->ordering_id!=native->axi_id){reject(tlm::TLM_COMMAND_ERROR_RESPONSE);return;}
        std::vector<unsigned char> enables;
        if(tx.is_write() && !native->write_strobes.empty()) {
            if(native->write_strobes.size()!=native->beats){reject(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);return;}
            enables.resize(tx.get_data_length());
            for(unsigned beat=0;beat<native->beats;++beat){
                const unsigned lane=(tx.get_address()+(native->increment?beat*unit:0))&3;
                const unsigned allowed=((1u<<unit)-1)<<lane;
                if(native->write_strobes[beat]&~allowed){reject(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);return;}
                for(unsigned byte=0;byte<unit;++byte){
                    const unsigned index=beat*unit+byte;
                    const bool original=!tx.get_byte_enable_ptr() ||
                        tx.get_byte_enable_ptr()[index%tx.get_byte_enable_length()]==0xff;
                    enables[index]=original && (native->write_strobes[beat]&(1u<<(lane+byte)))?0xff:0;
                }
            }
        }
        struct Restore {
            tlm::tlm_generic_payload& tx;unsigned char* be;unsigned length;
            explicit Restore(tlm::tlm_generic_payload& t):tx(t),be(t.get_byte_enable_ptr()),length(t.get_byte_enable_length()){}
            ~Restore(){tx.set_byte_enable_ptr(be);tx.set_byte_enable_length(length);tx.set_dmi_allowed(false);}
        } restore(tx);
        if(!enables.empty()){tx.set_byte_enable_ptr(enables.data());tx.set_byte_enable_length(enables.size());}
        // Same native AXI ID retains conservative per-direction ordering. The
        // native extension, response observations, faults and callback stay attached.
        fu3_bus::ScopedAccess access(tx,unit,burst,false,native->axi_id);
        output->b_transport(tx,delay);
        fu3_bus::consume_delay(delay);
        tx.set_dmi_allowed(false);
    }
};
}
