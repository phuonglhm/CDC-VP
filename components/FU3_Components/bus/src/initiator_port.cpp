#include "fu3_bus/target_port.h"

namespace fu3_bus {
InitiatorPort::InitiatorPort(sc_core::sc_module_name name) : sc_module(name) {
    socket.register_b_transport(this,&InitiatorPort::b_transport);
    socket.register_transport_dbg(this,&InitiatorPort::transport_dbg);
}
void InitiatorPort::configure(const InitiatorConfig& descriptor, const BusConfig& config,
                              const bool& staging) {
    descriptor_=descriptor; regions_=config.targets; staging_=&staging;
    rejection_latency_ns_=config.router_latency_ns;
    reads_.capacity(descriptor.outstanding); writes_.capacity(descriptor.outstanding);
}
void InitiatorPort::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    struct Count { unsigned& n; explicit Count(unsigned& v):n(v){++n;} ~Count(){--n;} } count(in_flight_);
    consume_delay(delay);
    sc_core::wait(sc_core::SC_ZERO_TIME);
    auto reject=[&](tlm::tlm_response_status status) {
        tx.set_dmi_allowed(false); tx.set_response_status(status);
        sc_core::wait(rejection_latency_ns_,sc_core::SC_NS);
    };
    if(!validate(tx)) { sc_core::wait(rejection_latency_ns_,sc_core::SC_NS); return; }
    const auto* access=tx.get_extension<Access>();
    if(descriptor_.require_access && !access) { reject(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
    if(access && (access->instruction && !tx.is_read())) { reject(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
    if(access && (access->burst!=Burst::Increment && access->burst!=Burst::Fixed)) {
        reject(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    const auto length=tx.get_data_length();
    const auto address=tx.get_address();
    const unsigned beat=access?access->beat_bytes:length;
    if(access && ((beat!=1 && beat!=2 && beat!=4) || length%beat || length/beat>descriptor_.max_beats)) {
        reject(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    const bool fixed=access && access->burst==Burst::Fixed;
    if (access && descriptor_.word_increment_only && (beat!=4 || fixed)) {
        reject(tlm::TLM_BURST_ERROR_RESPONSE); return;
    }
    const TargetConfig* selected=nullptr;
    for(const auto& t:regions_) if(t.enabled && address>=t.base && address-t.base<t.size) { selected=&t; break; }
    // Check the entire physical footprint before any beat or CSR side effect.
    if(!selected) { reject(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    const auto policy=selected->access;
    if((policy==AccessPolicy::BootRom && (descriptor_.name!="IBEX" || tx.is_write())) ||
       (policy==AccessPolicy::Cva6Program && descriptor_.name=="IBEX" && tx.is_write() && !*staging_) ||
       (policy==AccessPolicy::DataOnly && access && access->instruction)) {
        reject(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
    }
    if((fixed?beat:length)>selected->size-(address-selected->base)) {
        reject(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
    }
    if(access && address%beat) { reject(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    Admission::Lease slot(tx.is_write() && descriptor_.separate_read_write?writes_:reads_);
    // Preserve each adapter ordering stream; independent IDs and directions overlap.
    auto& queue=ordering_[{tx.is_write(),access?access->ordering_id:0}];
    if(!queue) queue=std::make_unique<Fifo>();
    Fifo::Lease ordered(*queue);
    if(policy==AccessPolicy::Cva6Program && descriptor_.name=="IBEX" && tx.is_write() && !*staging_) {
        reject(tlm::TLM_COMMAND_ERROR_RESPONSE); return;
    }
    if(!access || selected->preserve_burst) {
        out->b_transport(tx,delay); consume_delay(delay); tx.set_dmi_allowed(false); return;
    }
    struct Restore {
        tlm::tlm_generic_payload& t; sc_dt::uint64 a; unsigned char *d,*be;
        unsigned len,sw,bel;
        explicit Restore(tlm::tlm_generic_payload& tx):t(tx),a(tx.get_address()),d(tx.get_data_ptr()),
            be(tx.get_byte_enable_ptr()),len(tx.get_data_length()),sw(tx.get_streaming_width()),bel(tx.get_byte_enable_length()){}
        ~Restore(){t.set_address(a);t.set_data_ptr(d);t.set_data_length(len);t.set_streaming_width(sw);
            t.set_byte_enable_ptr(be);t.set_byte_enable_length(bel);t.set_dmi_allowed(false);}
    } restore(tx);
    for(unsigned offset=0;offset<length;offset+=beat) {
        unsigned char enables[4]={};
        tx.set_byte_enable_ptr(restore.be);tx.set_byte_enable_length(restore.bel);
        tx.set_address(address+(fixed?0:offset));tx.set_data_ptr(restore.d+offset);
        tx.set_data_length(beat);tx.set_streaming_width(beat);
        if(restore.be) {
            for(unsigned j=0;j<beat;++j) enables[j]=restore.be[(offset+j)%restore.bel];
            tx.set_byte_enable_ptr(enables);tx.set_byte_enable_length(beat);
        }
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        // Normalize bus-facing metadata: every dispatched operation is a single beat.
        // Word-only target interfaces receive aligned lane strobes, without RMW.
        unsigned char word[4]={}, lanes[4]={};
        const unsigned lane=static_cast<unsigned>(tx.get_address()%4);
        const bool widen=selected->word_interface && beat<4;
        if(widen) {
            for(unsigned j=0;j<beat;++j) {
                if(tx.is_write()) word[lane+j]=restore.d[offset+j];
                lanes[lane+j]=restore.be?restore.be[(offset+j)%restore.bel]:0xff;
            }
            tx.set_address(tx.get_address()-lane);tx.set_data_ptr(word);
            tx.set_data_length(4);tx.set_streaming_width(4);
            tx.set_byte_enable_ptr(lanes);tx.set_byte_enable_length(4);
        }
        {
            ScopedAccess downstream(tx,widen?4:beat,Burst::Increment,access->instruction,access->ordering_id);
            out->b_transport(tx,delay);consume_delay(delay);
        }
        if(widen && tx.is_read() && tx.is_response_ok())
            for(unsigned j=0;j<beat;++j) if(lanes[lane+j]) restore.d[offset+j]=word[lane+j];
        if(!tx.is_response_ok()) break;
    }
}
}
