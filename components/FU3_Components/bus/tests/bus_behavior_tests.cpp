#include "fu3_bus/bus_system.h"
#include "support/probe_target.h"
#include "support/test_helpers.h"
#include <array>
#include <memory>

struct Test : sc_core::sc_module, Checks {
    fu3_bus::BusConfig cfg = fu3_bus::BusConfig::fu3();
    fu3_bus::BusSystem fabric{"fabric", cfg};
    sc_core::sc_vector<Master> masters{"masters",4};
    std::vector<std::unique_ptr<fu3_bus::test::ProbeTarget>> targets;
    bool finished=false;
    sc_core::sc_event flood_start, flood_done;
    unsigned rejected=0, flood_workers_done=0;
    SC_HAS_PROCESS(Test);
    explicit Test(sc_core::sc_module_name n) : sc_module(n) {
        for(unsigned i=0;i<4;++i) masters[i].socket.bind(fabric.initiator(cfg.initiators[i].name));
        for(const auto& t:cfg.targets) {
            // Disabled windows get no fixture: their slot stays empty.
            if(!t.enabled) { targets.emplace_back(); continue; }
            auto ip=std::make_unique<fu3_bus::test::ProbeTarget>(sc_core::sc_gen_unique_name("ip"),t.size);
            fabric.target(t.name).bind(ip->socket); targets.push_back(std::move(ip));
        }
        SC_THREAD(run); SC_THREAD(watchdog); SC_THREAD(rejected_flood); SC_THREAD(timed_victim);
    }
    unsigned index(const std::string& name) {
        for(unsigned i=0;i<cfg.targets.size();++i) if(cfg.targets[i].name==name) return i;
        throw std::runtime_error("missing fixture target");
    }
    void send(unsigned master, tlm::tlm_generic_payload& tx, tlm::tlm_response_status status,
              unsigned incoming=0) {
        fu3_bus::Access implicit; implicit.beat_bytes=tx.get_data_length()<4?tx.get_data_length():4;
        const bool attach=!tx.get_extension<fu3_bus::Access>();
        if(attach) tx.set_extension(&implicit);
        auto address=tx.get_address(); auto* data=tx.get_data_ptr(); auto len=tx.get_data_length();
        auto* be=tx.get_byte_enable_ptr(); auto belen=tx.get_byte_enable_length();
        auto streaming=tx.get_streaming_width();
        sc_core::sc_time delay(incoming,sc_core::SC_NS);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        masters[master].socket->b_transport(tx,delay);
        if(attach) tx.clear_extension(&implicit);
        expect(tx.get_response_status()==status,"response at "+std::to_string(address));
        expect(tx.get_address()==address && tx.get_data_ptr()==data && tx.get_data_length()==len &&
               tx.get_byte_enable_ptr()==be && tx.get_byte_enable_length()==belen &&
               tx.get_streaming_width()==streaming,"payload restored");
        expect(delay==sc_core::SC_ZERO_TIME && !tx.is_dmi_allowed(),"delay consumed, DMI suppressed");
    }
    void run() {
        unsigned char data[80]={0x5a,0x12,0x34,0x56};
        tlm::tlm_generic_payload tx;
        // Independently check both boundaries of every physical aperture.
        for(unsigned i=0;i<cfg.targets.size();++i) {
            const auto& t=cfg.targets[i];
            if(!t.enabled) {
                // R2 windows stay in the map but decode nowhere until integrated.
                payload(tx,tlm::TLM_READ_COMMAND,t.base,data,4); send(1,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE);
                continue;
            }
            for(auto offset:{std::uint64_t{0},t.size-4}) {
                payload(tx,tlm::TLM_READ_COMMAND,t.base+offset,data,4);
                fu3_bus::ScopedAccess debug_access(tx,4,fu3_bus::Burst::Increment,false,0,true);
                send(t.name=="IBEX_DBG"?2:1,tx,tlm::TLM_OK_RESPONSE);
                expect(targets[i]->visits.back().address==offset,"correct target and local offset");
            }
            const auto visits=targets[i]->visits.size();
            payload(tx,tlm::TLM_WRITE_COMMAND,t.base+t.size-2,data,4);
            // Policy is checked before the window footprint: writes the master may
            // never make are slave errors, the rest cross the window edge.
            send(1,tx,(t.name=="ROM" || t.name=="IBEX_DBG" || t.name=="QSPI_XIP")?
                 tlm::TLM_GENERIC_ERROR_RESPONSE:tlm::TLM_ADDRESS_ERROR_RESPONSE);
            expect(targets[i]->visits.size()==visits,"cross-boundary transfer never reaches target");
        }
        // Reserved rows of Common v2.1 MemoryMap and the disabled R2 windows.
        for(auto a:{0x1000ULL,0xfffcULL,0x41000ULL,0x4fffcULL,0x61000ULL,0x1040000ULL,0x1a111000ULL,
                    0x31000000ULL,0x31010000ULL,0x31020000ULL,0x40010000ULL,0x40110000ULL,
                    0x44100000ULL,0x44110000ULL,0x44120000ULL,0x50000000ULL,0x50100000ULL,
                    0x50110000ULL,0x62000000ULL,0xffffffffULL,0x100000000ULL}) {
            payload(tx,tlm::TLM_READ_COMMAND,a,data,1); send(1,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }
        for(auto name:{"AP_PLIC","SYSCSR"}) {
            auto i=index(name); auto base=cfg.targets[i].base;
            payload(tx,tlm::TLM_READ_COMMAND,base,data,4);
            auto before=sc_core::sc_time_stamp(); send(1,tx,tlm::TLM_OK_RESPONSE,5);
            expect(sc_core::sc_time_stamp()-before==sc_core::sc_time(name==std::string("AP_PLIC")?50:54,sc_core::SC_NS),
                   "PERIBUS_1 bypasses SYSBUS_0; both paths consume each stage once");
            targets[i]->annotate=true;
            before=sc_core::sc_time_stamp(); send(1,tx,tlm::TLM_OK_RESPONSE,5);
            expect(sc_core::sc_time_stamp()-before==sc_core::sc_time(name==std::string("AP_PLIC")?50:54,sc_core::SC_NS),"annotated target delay once");
            for(unsigned j=0;j<64;++j) data[j]=static_cast<unsigned char>(j+1);
            unsigned char enables[3]={0xff,0,0xff};
            payload(tx,tlm::TLM_WRITE_COMMAND,base+0x100,data,64);
            tx.set_byte_enable_ptr(enables); tx.set_byte_enable_length(3);
            auto count=targets[i]->visits.size();
            if(name==std::string("AP_PLIC")){
                // Only CLAIM/COMPLETE is a bus-protected side-effect word (Q03-a).
                payload(tx,tlm::TLM_WRITE_COMMAND,base+0x3000,data,8);
                tx.set_byte_enable_ptr(enables); tx.set_byte_enable_length(3);
                send(3,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
                expect(targets[i]->visits.size()==count,"partial CLAIM/COMPLETE burst rejected before any CSR effect");
                tx.set_byte_enable_ptr(nullptr);tx.set_byte_enable_length(0);
                payload(tx,tlm::TLM_READ_COMMAND,base+0x3006,data,2); send(0,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
                expect(targets[i]->visits.size()==count,"narrow CLAIM read cannot claim");
                payload(tx,tlm::TLM_WRITE_COMMAND,base+0x100,data,64);
                tx.set_byte_enable_ptr(enables); tx.set_byte_enable_length(3);
            }
            send(3,tx,tlm::TLM_OK_RESPONSE);
            expect(targets[i]->visits.size()==count+16,"DMA 16-word payload becomes 16 APB transfers");
            tx.set_byte_enable_ptr(nullptr); tx.set_byte_enable_length(0);
            std::fill(std::begin(data),std::end(data),0);
            payload(tx,tlm::TLM_READ_COMMAND,base+0x100,data,64); send(0,tx,tlm::TLM_OK_RESPONSE);
            for(unsigned j=0;j<64;++j) expect(data[j]==(j%3==1?0:j+1),"byte enable pattern across beats");
            for(unsigned n:{1U,2U,4U}) {
                payload(tx,tlm::TLM_WRITE_COMMAND,base+0x200,data,n); send(3,tx,tlm::TLM_OK_RESPONSE);
            }
            payload(tx,tlm::TLM_WRITE_COMMAND,base+1,data,4);send(3,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE);
            payload(tx,tlm::TLM_WRITE_COMMAND,base,data,68);send(3,tx,tlm::TLM_BURST_ERROR_RESPONSE);
            payload(tx,tlm::TLM_WRITE_COMMAND,base,data,8);
            count=targets[i]->visits.size(); targets[i]->forced_status=tlm::TLM_GENERIC_ERROR_RESPONSE;
            send(3,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
            expect(targets[i]->visits.size()==count+1,"burst stops at first failed beat");
            targets[i]->forced_status=tlm::TLM_OK_RESPONSE;
            unsigned beat_count=0;
            targets[i]->on_visit=[&](const auto&) { if(++beat_count==2) throw std::runtime_error("second beat"); };
            payload(tx,tlm::TLM_WRITE_COMMAND,base+0x300,data,12);
            tx.set_byte_enable_ptr(enables);tx.set_byte_enable_length(3);
            sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
            bool threw=false;
            try { fu3_bus::ScopedAccess request(tx,4); masters[3].socket->b_transport(tx,delay); } catch(const std::runtime_error&) { threw=true; }
            expect(threw && beat_count==2,"second beat exception propagated");
            expect(tx.get_address()==base+0x300 && tx.get_data_ptr()==data && tx.get_data_length()==12 &&
                   tx.get_streaming_width()==12 && tx.get_byte_enable_ptr()==enables &&
                   tx.get_byte_enable_length()==3,"all burst fields restored on exception");
            targets[i]->on_visit={};
            send(3,tx,tlm::TLM_OK_RESPONSE); // Router and bridge ownership released by unwinding.
            tx.set_byte_enable_ptr(nullptr);tx.set_byte_enable_length(0);
        }
        payload(tx,tlm::TLM_READ_COMMAND,0,data,4); send(0,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
        send(1,tx,tlm::TLM_OK_RESPONSE); send(3,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
        payload(tx,tlm::TLM_WRITE_COMMAND,0,data,4); send(1,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
        payload(tx,tlm::TLM_WRITE_COMMAND,0x10000,data,4); send(1,tx,tlm::TLM_OK_RESPONSE);
        fabric.finish_staging(); send(1,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);send(0,tx,tlm::TLM_OK_RESPONSE);
        payload(tx,tlm::TLM_WRITE_COMMAND,0x30000000,data,4);
        for(unsigned m:{0U,1U,3U}) send(m,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
        payload(tx,tlm::TLM_READ_COMMAND,0x30000000,data,4); send(0,tx,tlm::TLM_OK_RESPONSE);
        fu3_bus::Access access; access.instruction=true;
        payload(tx,tlm::TLM_READ_COMMAND,0x1000000,data,4); tx.set_extension(&access);
        send(0,tx,tlm::TLM_GENERIC_ERROR_RESPONSE); tx.clear_extension(&access); send(0,tx,tlm::TLM_OK_RESPONSE);
        // Firmware loader backdoor: known bytes, no kernel time and no functional policy bypass via b_transport.
        unsigned char image[8]={0x13,0,0,0,0x73,0,0x10,0};
        payload(tx,tlm::TLM_WRITE_COMMAND,0x50000,image,8);
        auto before=sc_core::sc_time_stamp();
        expect(masters[2].socket->transport_dbg(tx)==8,"load image into IBEX_ISRAM");
        expect(sc_core::sc_time_stamp()==before && tx.get_address()==0x50000 && !tx.is_dmi_allowed(),"untimed loader");
        payload(tx,tlm::TLM_READ_COMMAND,0x50000,data,8);send(1,tx,tlm::TLM_OK_RESPONSE);
        expect(std::equal(image,image+8,data),"loaded image readback (no CPU execution claimed)");
        payload(tx,tlm::TLM_READ_COMMAND,0x50000,data,4);tx.set_streaming_width(2);send(1,tx,tlm::TLM_BURST_ERROR_RESPONSE);
        payload(tx,tlm::TLM_IGNORE_COMMAND,0x50000,data,4);send(1,tx,tlm::TLM_COMMAND_ERROR_RESPONSE);
        payload(tx,tlm::TLM_READ_COMMAND,0x50000,nullptr,4);send(1,tx,tlm::TLM_GENERIC_ERROR_RESPONSE);
        unsigned char invalid_enable=1;
        payload(tx,tlm::TLM_READ_COMMAND,0x50000,data,4);
        tx.set_byte_enable_ptr(&invalid_enable);tx.set_byte_enable_length(1);
        send(1,tx,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        tx.set_byte_enable_ptr(nullptr);tx.set_byte_enable_length(0);
        bool unknown=false;
        try { fabric.target("NO_TARGET"); } catch(const std::out_of_range&) { unknown=true; }
        expect(unknown,"unknown target lookup");
        flood_start.notify(sc_core::SC_ZERO_TIME);
        while(flood_workers_done<2) wait(flood_done);
        finished=true; sc_core::sc_stop();
    }
    void rejected_flood() {
        wait(flood_start);
        unsigned char data[4]={}; tlm::tlm_generic_payload tx;
        payload(tx,tlm::TLM_READ_COMMAND,0,data,4);
        for(unsigned i=0;i<100;++i) { send(0,tx,tlm::TLM_GENERIC_ERROR_RESPONSE); ++rejected; }
        ++flood_workers_done; flood_done.notify(sc_core::SC_ZERO_TIME);
    }
    void timed_victim() {
        wait(flood_start); wait(1,sc_core::SC_NS);
        unsigned char data[4]={}; tlm::tlm_generic_payload tx;
        payload(tx,tlm::TLM_READ_COMMAND,0x50000,data,4);send(1,tx,tlm::TLM_OK_RESPONSE);
        expect(rejected<100,"continuous denied requests do not starve a time-scheduled master");
        ++flood_workers_done; flood_done.notify(sc_core::SC_ZERO_TIME);
    }
    void watchdog(){wait(1,sc_core::SC_MS);expect(false,"watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**) {
    Test t{"test"}; sc_core::sc_start();
    std::cout<<"Behavior: "<<t.checks<<" checks, "<<t.failures<<" failures\n";
    return t.finished && !t.failures ? 0 : 1;
}
