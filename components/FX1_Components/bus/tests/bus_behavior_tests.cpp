#include "bus/bus_system.h"
#include "support/probe_target.h"
#include "support/test_helpers.h"
#include <algorithm>
#include <array>
#include <memory>
#include <vector>

using bus::TargetPath;
struct Fixture : sc_core::sc_module {
    bus::BusConfig cfg;
    bus::BusSystem fabric;
    sc_core::sc_vector<Master> masters;
    std::vector<std::unique_ptr<bus::test::ProbeTarget>> targets;
    std::vector<bus::TargetConfig> active;
    Fixture(sc_core::sc_module_name name, bus::BusConfig config, bool trace)
        : sc_module(name), cfg(std::move(config)), fabric("fabric", cfg, trace),
          masters("masters", cfg.initiators.size()) {
        for (unsigned i=0; i<masters.size(); ++i) masters[i].socket.bind(fabric.initiator(cfg.initiators[i].name));
        for (const auto& t : cfg.targets) if (t.enabled) {
            active.push_back(t);
            auto ip = std::make_unique<bus::test::ProbeTarget>(sc_core::sc_gen_unique_name("ip"), t.size);
            ip->readonly = t.name == "BootROM" || t.name == "ROM";
            fabric.target(t.name).bind(ip->socket);
            targets.push_back(std::move(ip));
        }
    }
};

bus::BusConfig expanded() {
    auto cfg = bus::BusConfig::fx1();
    cfg.initiators.push_back({"NEW_MASTER"});
    cfg.targets.push_back({"AES", 0x50000000, 0x1000, TargetPath::SysBus0Axi, true});
    cfg.targets.push_back({"QSPI", 0x51000000, 0x1000, TargetPath::SysBus0Axi, true});
    cfg.targets.push_back({"NEW_IP", 0x30000000, 0x1000, TargetPath::SysBus1Axi, true});
    for (auto& t : cfg.targets) {
        if (t.name == "ISP_CSR") { t.base=0x40000000; t.size=0x1000; t.enabled=true; }
        if (t.name == "MIPI_CSR") { t.base=0x40002000; t.size=0x1000; t.enabled=true; }
    }
    std::reverse(cfg.targets.begin(), cfg.targets.end()); // Binding must not depend on descriptor order.
    std::reverse(cfg.initiators.begin(), cfg.initiators.end());
    cfg.router_latency_ns=3; cfg.apb_cycle_ns=11;
    return cfg;
}
bus::BusConfig legacy() {
    bus::BusConfig c;
    c.initiators={{"DBG"},{"CPU"},{"DMA"}};
    c.targets={{"ROM",0,0x10000,TargetPath::SysBus1Axi,true},
               {"ISRAM",0x10000000,0x20000,TargetPath::SysBus1Axi,true},
               {"DSRAM",0x20000000,0x20000,TargetPath::SysBus1Axi,true},
               {"AES",0x50000000,0x10000,TargetPath::SysBus0Axi,true},
               {"QSPI",0x51000000,0x1000000,TargetPath::SysBus0Axi,true}};
    for (unsigned i=0; i<6; ++i)
        c.targets.push_back({"PP1_"+std::to_string(i),0x40000000+i*0x1000ULL,0x1000,TargetPath::Peribus1Apb,true});
    for (unsigned i=0; i<2; ++i)
        c.targets.push_back({"PP0_"+std::to_string(i),0x52000000+i*0x1000ULL,0x1000,TargetPath::Peribus0Apb,true});
    return c;
}
struct Test : sc_core::sc_module, Checks {
    std::vector<std::unique_ptr<Fixture>> fixtures;
    bool finished=false;
    SC_HAS_PROCESS(Test);
    Test(sc_core::sc_module_name n, bool trace) : sc_module(n) {
        fixtures.push_back(std::make_unique<Fixture>("fx1",bus::BusConfig::fx1(),trace));
        fixtures.push_back(std::make_unique<Fixture>("expanded",expanded(),trace));
        fixtures.push_back(std::make_unique<Fixture>("legacy",legacy(),trace));
        // Exercise each path with all other branches absent, including AXI-only SYSBUS_0.
        unsigned k=0;
        for (auto path : {TargetPath::SysBus1Axi,TargetPath::SysBus0Axi,TargetPath::Peribus0Apb,TargetPath::Peribus1Apb}) {
            bus::BusConfig c; c.initiators={{"solo"}};
            c.targets={{"only",0x60000000,0x1000,path,true}};
            fixtures.push_back(std::make_unique<Fixture>(("minimal"+std::to_string(k++)).c_str(),c,trace));
        }
        SC_THREAD(run); SC_THREAD(watchdog);
    }
    void send(Fixture& f, unsigned master, tlm::tlm_generic_payload& tx,
              tlm::tlm_response_status status, const std::string& label, unsigned incoming=0) {
        const auto address=tx.get_address(); sc_core::sc_time delay(incoming,sc_core::SC_NS);
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        f.masters[master].socket->b_transport(tx,delay);
        expect(tx.get_response_status()==status,label+" response");
        expect(tx.get_address()==address,label+" restores global address");
        expect(delay==sc_core::SC_ZERO_TIME,label+" consumes delay");
        expect(!tx.is_dmi_allowed(),label+" disables DMI");
    }
    void exercise(Fixture& f) {
        bool absent=false;
        try { f.fabric.target("UNKNOWN"); } catch (const std::out_of_range&) { absent=true; }
        expect(absent,"unknown target lookup");
        absent=false;
        try { f.fabric.initiator("UNKNOWN"); } catch (const std::out_of_range&) { absent=true; }
        expect(absent,"unknown master lookup");
        for (const auto& t:f.cfg.targets) if (!t.enabled) {
            absent=false; try { f.fabric.target(t.name); } catch(const std::out_of_range&) { absent=true; }
            expect(absent,"disabled target has no socket: "+t.name);
        }
        for(unsigned i=0;i<f.active.size();++i) {
            const auto& t=f.active[i];auto& ip=*f.targets[i];
            const bool apb=t.path==TargetPath::Peribus0Apb||t.path==TargetPath::Peribus1Apb;
            unsigned hops=t.path==TargetPath::SysBus1Axi?1:t.path==TargetPath::Peribus0Apb?3:2;
            const auto label=std::string(f.name())+":"+t.name;
            std::array<unsigned char,16> data{}, read{}; data.fill(0x5A);
            tlm::tlm_generic_payload tx;
            // Debug bypasses APB beat/alignment, initializes even a read-only target.
            payload(tx,tlm::TLM_WRITE_COMMAND,t.base+3,data.data(),data.size());
            auto start=sc_core::sc_time_stamp();
            expect(f.masters[0].socket->transport_dbg(tx)==data.size(),label+" debug length");
            expect(sc_core::sc_time_stamp()==start&&tx.get_address()==t.base+3&&!tx.is_dmi_allowed(),label+" debug time/address/DMI");
            for(unsigned m=0;m<f.masters.size();++m) {
                payload(tx,tlm::TLM_READ_COMMAND,t.base+4,read.data(),4);
                send(f,m,tx,tlm::TLM_OK_RESPONSE,label+" all masters");
                expect(read[0]==0x5A && ip.visits.back().address==4,label+" correct target/local offset");
            }
            // Wait-based and annotated latency are each consumed exactly once.
            for(bool annotate:{false,true}) {
                ip.annotate=annotate;
                payload(tx,tlm::TLM_READ_COMMAND,t.base+4,read.data(),4);
                start=sc_core::sc_time_stamp();send(f,0,tx,tlm::TLM_OK_RESPONSE,label+" timing",5);
                const unsigned expected=5+hops*f.cfg.router_latency_ns+ip.latency_ns+(apb?2*f.cfg.apb_cycle_ns:0);
                expect(sc_core::sc_time_stamp()-start==sc_core::sc_time(expected,sc_core::SC_NS),label+" exact latency");
            }
            ip.annotate=false;
            for(unsigned length:{1U,2U,4U}) {
                payload(tx,tlm::TLM_WRITE_COMMAND,t.base+32,data.data(),length);
                send(f,0,tx,ip.readonly?tlm::TLM_COMMAND_ERROR_RESPONSE:tlm::TLM_OK_RESPONSE,label+" legal sizes");
            }
            if(!ip.readonly) {
                std::array<unsigned char,2> enable{{0xFF,0}};
                payload(tx,tlm::TLM_WRITE_COMMAND,t.base+64,data.data(),4);
                tx.set_byte_enable_ptr(enable.data());tx.set_byte_enable_length(enable.size());
                send(f,0,tx,tlm::TLM_OK_RESPONSE,label+" repeating byte enable");
                tx.set_byte_enable_ptr(nullptr);payload(tx,tlm::TLM_READ_COMMAND,t.base+64,read.data(),4);
                send(f,0,tx,tlm::TLM_OK_RESPONSE,label+" byte readback");
                expect(read[0]==0x5A&&read[1]==0&&read[2]==0x5A&&read[3]==0,label+" masked writes");
                read.fill(0xEE);tx.set_byte_enable_ptr(enable.data());tx.set_byte_enable_length(2);
                send(f,0,tx,tlm::TLM_OK_RESPONSE,label+" masked read");
                expect(read[0]==0x5A&&read[1]==0xEE&&read[2]==0x5A&&read[3]==0xEE,label+" masked read preserves disabled bytes");
                tx.set_byte_enable_ptr(nullptr);
            }
            payload(tx,tlm::TLM_READ_COMMAND,t.base,read.data(),16);
            const auto before=ip.visits.size();
            send(f,0,tx,apb?tlm::TLM_BURST_ERROR_RESPONSE:tlm::TLM_OK_RESPONSE,label+" long payload");
            expect(ip.visits.size()==before+(apb?0:1),label+" burst forwarding policy");
            if(apb) {
                for (unsigned length : {3U, 8U}) {
                    payload(tx,tlm::TLM_WRITE_COMMAND,t.base,data.data(),length);
                    send(f,0,tx,tlm::TLM_BURST_ERROR_RESPONSE,label+" APB unsupported length");
                }
                payload(tx,tlm::TLM_WRITE_COMMAND,t.base+1,data.data(),4);
                send(f,0,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE,label+" APB alignment");
                expect(ip.visits.size()==before,label+" bad APB transfer never reaches IP");
            }
            // End boundary: no partial forward/write even when the next target is adjacent.
            payload(tx,tlm::TLM_READ_COMMAND,t.base+t.size-1,read.data(),1);
            send(f,0,tx,tlm::TLM_OK_RESPONSE,label+" last byte");
            const auto calls=ip.visits.size();
            payload(tx,tlm::TLM_WRITE_COMMAND,t.base+t.size-1,data.data(),4);
            send(f,0,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE,label+" crosses boundary");
            expect(ip.visits.size()==calls,label+" crossing write not forwarded");
            const auto dbg=ip.debug_calls;
            expect(f.masters[0].socket->transport_dbg(tx)==0&&ip.debug_calls==dbg,label+" debug crossing rejected");
            payload(tx,tlm::TLM_READ_COMMAND,t.base+t.size,read.data(),1);
            // Some legacy peripheral regions are adjacent: only assert miss if not mapped.
            bool mapped=false;for(const auto& r:f.active)if(tx.get_address()>=r.base&&tx.get_address()<r.base+r.size)mapped=true;
            if(!mapped)send(f,0,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE,label+" exclusive end");
            ip.forced_status=tlm::TLM_GENERIC_ERROR_RESPONSE;
            payload(tx,tlm::TLM_READ_COMMAND,t.base,read.data(),4);
            send(f,0,tx,tlm::TLM_GENERIC_ERROR_RESPONSE,label+" downstream error");
            ip.forced_status=tlm::TLM_OK_RESPONSE;
            // Invalid payloads must not reach any downstream target.
            const auto invalid_calls=ip.visits.size();
            payload(tx,tlm::TLM_IGNORE_COMMAND,t.base,data.data(),4);
            send(f,0,tx,tlm::TLM_COMMAND_ERROR_RESPONSE,label+" invalid command");
            payload(tx,tlm::TLM_READ_COMMAND,t.base,nullptr,4);
            send(f,0,tx,tlm::TLM_GENERIC_ERROR_RESPONSE,label+" null data");
            payload(tx,tlm::TLM_READ_COMMAND,t.base,data.data(),0);
            send(f,0,tx,tlm::TLM_GENERIC_ERROR_RESPONSE,label+" empty data");
            payload(tx,tlm::TLM_READ_COMMAND,t.base,data.data(),4);tx.set_streaming_width(2);
            send(f,0,tx,tlm::TLM_BURST_ERROR_RESPONSE,label+" streaming");
            unsigned char invalid_enable=0x7F;tx.set_streaming_width(4);tx.set_byte_enable_ptr(&invalid_enable);tx.set_byte_enable_length(1);
            send(f,0,tx,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,label+" invalid byte enable");
            tx.set_byte_enable_length(0);send(f,0,tx,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,label+" zero enable length");
            expect(ip.visits.size()==invalid_calls,label+" invalid payloads never forwarded");
        }
        std::array<unsigned char,4> data{};tlm::tlm_generic_payload tx;
        payload(tx,tlm::TLM_READ_COMMAND,0x70000000,data.data(),4);
        send(f,0,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE,"unmapped hole");
        expect(f.masters[0].socket->transport_dbg(tx)==0,"unmapped debug");
        payload(tx,tlm::TLM_IGNORE_COMMAND,f.active[0].base,data.data(),4);
        expect(f.masters[0].socket->transport_dbg(tx)==0&&tx.get_response_status()==tlm::TLM_COMMAND_ERROR_RESPONSE,"debug invalid command");
        payload(tx,tlm::TLM_READ_COMMAND,f.active[0].base,nullptr,4);
        expect(f.masters[0].socket->transport_dbg(tx)==0&&tx.get_response_status()==tlm::TLM_GENERIC_ERROR_RESPONSE,"debug null data");
        payload(tx,tlm::TLM_READ_COMMAND,0x100000000ULL,data.data(),4);
        send(f,0,tx,tlm::TLM_ADDRESS_ERROR_RESPONSE,"address above 32 bits");
    }
    void run(){for(auto& f:fixtures)exercise(*f);finished=true;sc_core::sc_stop();}
    void watchdog(){wait(1,sc_core::SC_MS);expect(false,"behavior watchdog");sc_core::sc_stop();}
};
int sc_main(int argc,char** argv){
    const bool trace=argc>1&&std::string(argv[1])=="--trace";
    Test t{"test",trace};sc_core::sc_start();
    std::cout<<"Behavior: "<<t.checks<<" checks, "<<t.failures<<" failures\n";
    return t.finished&&!t.failures?0:1;
}
