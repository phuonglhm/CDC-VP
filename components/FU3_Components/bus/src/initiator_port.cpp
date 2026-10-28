#include "fu3_bus/target_port.h"

namespace fu3_bus {
namespace {
// Q02-f: a well-formed request that the access policy, a debug partition or a
// side-effect word refuses is a slave error (SLVERR). Decode misses stay
// TLM_ADDRESS_ERROR_RESPONSE (DECERR); malformed payloads keep their diagnostic.
constexpr auto kDenied = tlm::TLM_GENERIC_ERROR_RESPONSE;
} // namespace
InitiatorPort::InitiatorPort(sc_core::sc_module_name name) : sc_module(name) {
    socket.register_b_transport(this,&InitiatorPort::b_transport);
    socket.register_transport_dbg(this,&InitiatorPort::transport_dbg);
}
void InitiatorPort::configure(const InitiatorConfig& descriptor, const BusConfig& config,
                              const bool& staging, const bool& accepting,
                              sc_core::sc_event& activity) {
    descriptor_=descriptor; regions_=config.targets; staging_=&staging;
    accepting_=&accepting; activity_=&activity;
    rejection_latency_=sc_core::sc_time(config.router_latency_ns,sc_core::SC_NS);
    reads_.capacity(descriptor.outstanding); writes_.capacity(descriptor.outstanding);
}
void InitiatorPort::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    // Rejected new work is not part of the set being drained during quiescence.
    if (!*accepting_) {
        consume_delay(delay); sc_core::wait(rejection_latency_);
        tx.set_dmi_allowed(false); tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return;
    }
    struct Count {
        InitiatorPort& port; const std::uint64_t seq;
        explicit Count(InitiatorPort& p):port(p),seq(p.issued_++){++port.in_flight_;port.live_.insert(seq);}
        ~Count(){--port.in_flight_;port.live_.erase(seq);port.activity_->notify(sc_core::SC_ZERO_TIME);}
    } count(*this);
    consume_delay(delay);
    sc_core::wait(sc_core::SC_ZERO_TIME);
    auto reject=[&](tlm::tlm_response_status status) {
        tx.set_dmi_allowed(false); tx.set_response_status(status);
        sc_core::wait(rejection_latency_);
    };
    if(!validate(tx)) { sc_core::wait(rejection_latency_); return; }
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
    const bool ibex=descriptor_.name=="IBEX", cva6=descriptor_.name=="CVA6";
    const bool debug=access && access->debug;
    // DBG may read the ROM (FU3_VP_ABI_1 access profile); only Ibex executes it.
    if((policy==AccessPolicy::BootRom && ((descriptor_.name!="IBEX" && descriptor_.name!="DBG") || tx.is_write())) ||
       (policy==AccessPolicy::Cva6Program && ibex && !*staging_) ||
       (policy==AccessPolicy::IbexProgram && (cva6 || (ibex && tx.is_write() && !*staging_))) ||
       (policy==AccessPolicy::DebugControl && descriptor_.name!="DBG" && !(ibex && debug)) ||
       (policy==AccessPolicy::ReadExecute && tx.is_write()) ||
       ((policy==AccessPolicy::DataOnly || selected->non_executable) && access && access->instruction)) {
        reject(kDenied); return;
    }
    if((fixed?beat:length)>selected->size-(address-selected->base)) {
        reject(tlm::TLM_ADDRESS_ERROR_RESPONSE); return;
    }
    const bool fetch=access && access->instruction;
    for(const auto& t:descriptor_.no_execute) if(fetch && t==selected->name) { reject(kDenied); return; }
    if(descriptor_.grants_only) {
        const auto local=address-selected->base, end=local+(fixed?beat:length);
        bool granted=false;
        for(const auto& g:descriptor_.grants) {
            if(g.target!=selected->name) continue;
            if(tx.is_write()?!g.write:(fetch?!g.execute:!g.read)) continue;
            bool inside=g.offsets.empty();
            for(const auto& [b,e]:g.offsets) inside=inside || (local>=b && end<=e);
            if(inside) { granted=true; break; }
        }
        if(!granted) { reject(kDenied); return; }
    }
    if(access && address%beat) { reject(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
    const auto footprint=fixed?beat:length;
    const bool boot_data=ibex && *staging_ && !(access && access->instruction);
    if(selected->debug_offset && address-selected->base+footprint>selected->debug_offset &&
       (ibex || cva6) && ((tx.is_write() && !boot_data) || (!debug && !boot_data))) {
        reject(kDenied);return;
    }
    if(selected->strict_word_access) {
        if(beat!=4 || address%4) {reject(kDenied);return;}
        if(const auto* enables=tx.get_byte_enable_ptr())
            for(unsigned i=0;i<length;++i)if(enables[i%tx.get_byte_enable_length()]!=0xff) {
                reject(kDenied);return;
            }
    }
    // Validate every protected beat before the first possible target side effect.
    for (const auto offset : selected->strict_word_offsets) {
        const auto word=selected->base+offset;
        const auto footprint=fixed?beat:length;
        if (address>=word+4 || word>=address+footprint) continue;
        if (beat!=4 || address%4) { reject(kDenied); return; }
        if (const auto* enables=tx.get_byte_enable_ptr()) {
            const unsigned first=fixed?0:static_cast<unsigned>(word-address);
            const unsigned end=fixed?length:first+4;
            for(unsigned i=first;i<end;++i)
                if(enables[i%tx.get_byte_enable_length()]!=0xff) {
                    reject(kDenied); return;
                }
        }
    }
    Admission::Lease slot(tx.is_write() && descriptor_.separate_read_write?writes_:reads_);
    // Preserve each adapter ordering stream; independent IDs and directions overlap.
    auto& queue=ordering_[{tx.is_write(),access?access->ordering_id:0}];
    if(!queue) queue=std::make_unique<Fifo>();
    Fifo::Lease ordered(*queue);
    if((policy==AccessPolicy::Cva6Program || (policy==AccessPolicy::IbexProgram && tx.is_write())) && ibex && !*staging_) {
        reject(kDenied); return;
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
            ScopedAccess downstream(tx,widen?4:beat,Burst::Increment,access->instruction,access->ordering_id,access->debug);
            out->b_transport(tx,delay);consume_delay(delay);
        }
        if(widen && tx.is_read() && tx.is_response_ok())
            for(unsigned j=0;j<beat;++j) if(lanes[lane+j]) restore.d[offset+j]=word[lane+j];
        if(!tx.is_response_ok()) break;
    }
}
}
