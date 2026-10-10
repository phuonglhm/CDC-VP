#pragma once
#include <h264/sync/reset_domain.h>
#include "h264_arb.h"
#include "axi_master_bridge.h"
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <map>
namespace h264 {
// Optional explicit client identity for TLM producers. Untagged fixtures use CMB.
struct DmaClientExtension: tlm::tlm_extension<DmaClientExtension> {
    ClientId client=ClientId::CMB;
    tlm_extension_base* clone() const override { return new DmaClientExtension(*this); }
    void copy_from(const tlm_extension_base& b) override { *this=static_cast<const DmaClientExtension&>(b); }
};
struct GrantRecord { int owner; uint64_t address; unsigned length; sc_core::sc_time begin,end; };
struct SegmentRecord { uint64_t address; unsigned bytes,beats; sc_core::sc_time completed; };
// SystemC transport adapter only. Arbitration/AXI planning are Vinh's cores.
class DmaTransport: public sc_core::sc_module, private MemoryIf {
public:
    tlm_utils::multi_passthrough_target_socket<DmaTransport> clients{"clients"};
    tlm_utils::simple_initiator_socket<DmaTransport> memory{"memory"};
    std::vector<GrantRecord> trace;
    std::vector<SegmentRecord> segments;
    const unsigned bus_bytes,max_beats;
    SC_HAS_PROCESS(DmaTransport);
    DmaTransport(sc_core::sc_module_name,unsigned width=32,unsigned burst=16);
    void set_client(int owner,ClientId client);
    void set_priority(const ClientId order[4]) { arb_.set_priority(order); }
    bool idle() const { return pending_.empty(); }
    bool wait_idle(sc_core::sc_time);
private:
    struct Pending: CompletionSink {
        tlm::tlm_generic_payload* tx;
        int owner;
        bool done=false;
        tlm::tlm_response_status status=tlm::TLM_OK_RESPONSE;
        sc_core::sc_event completed;
        void on_done(ClientId,const DmaResponse&) override {
            done=true; completed.notify(sc_core::SC_ZERO_TIME);
        }
    };
    H264Arb arb_;
    AxiBridgeConfig config_;
    std::map<int,ClientId> identities_;
    std::map<uint32_t,Pending*> pending_;
    uint32_t sequence_=0;
    Pending* active_=nullptr;
    sc_core::sc_event changed_;
    void transport(int,tlm::tlm_generic_payload&,sc_core::sc_time&);
    void service();
    void access(bool,uint64_t,uint8_t*,size_t);
    void write(uint64_t a,const uint8_t* d,size_t n) override { access(true,a,const_cast<uint8_t*>(d),n); }
    void read(uint64_t a,uint8_t* d,size_t n) const override { const_cast<DmaTransport*>(this)->access(false,a,d,n); }
    uint8_t read_byte(uint64_t a) const override { uint8_t v=0; read(a,&v,1); return v; }
};
}
