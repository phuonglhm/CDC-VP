#include <fu3_bus/bus_system.h>
#include <fu3_bus/irq_adapter.h>
#include "support/probe_target.h"
#include "support/test_helpers.h"
#include <memory>

struct CommonTest : sc_core::sc_module, Checks {
    static fu3_bus::BusConfig config(){
        auto c=fu3_bus::BusConfig::fu3();
        for(auto& t:c.targets)t.enabled=t.name=="ROM" || t.name=="CVA6_ISRAM" ||
            t.name=="IBEX_ISRAM" || t.name=="DSRAM" || t.name=="IBEX_DBG" ||
            t.name=="AES" || t.name=="AP_PLIC" || t.name=="HYPERBUS";
        return c;
    }
    fu3_bus::BusConfig cfg=config();fu3_bus::BusSystem bus{"bus",cfg};
    Master cva6{"cva6"},ibex{"ibex"},dbg{"dbg"};
    std::vector<std::unique_ptr<fu3_bus::test::ProbeTarget>> peers;
    fu3_bus::IrqAdapter adapter{"irq_adapter"};
    sc_core::sc_signal<bool> reset_n,source,level;
    bool finished=false;
    SC_HAS_PROCESS(CommonTest);
    explicit CommonTest(sc_core::sc_module_name n):sc_module(n){
        cva6.socket.bind(bus.initiator("CVA6"));ibex.socket.bind(bus.initiator("IBEX"));dbg.socket.bind(bus.initiator("DBG"));
        for(const auto& t:cfg.targets)if(t.enabled){
            auto p=std::make_unique<fu3_bus::test::ProbeTarget>(sc_core::sc_gen_unique_name("peer"),t.size);
            bus.target(t.name).bind(p->socket);peers.push_back(std::move(p));
        }
        adapter.reset_n(reset_n);adapter.source(source);adapter.irq(level);SC_THREAD(run);
    }
    void request(Master& master,unsigned address,bool write,bool fetch,bool debug,tlm::tlm_response_status status){
        unsigned char bytes[4]={0x31,0x72,0xa5,0xc9};tlm::tlm_generic_payload t;
        payload(t,write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND,address,bytes,4);
        fu3_bus::ScopedAccess access(t,4,fu3_bus::Burst::Increment,fetch,0,debug);
        const auto before=visits();auto delay=sc_core::SC_ZERO_TIME;master.socket->b_transport(t,delay);
        expect(t.get_response_status()==status,"Common CPU access matrix at "+std::to_string(address));
        if(status!=tlm::TLM_OK_RESPONSE)expect(visits()==before,"denied CPU request cannot cause target effects");
    }
    unsigned visits(){unsigned n=0;for(auto& p:peers)n+=p->visits.size();return n;}
    void settle(){for(unsigned i=0;i<5;++i)wait(sc_core::SC_ZERO_TIME);}
    void run(){
        using namespace tlm;
        request(ibex,0,false,true,false,TLM_OK_RESPONSE);
        request(cva6,0,false,true,false,TLM_COMMAND_ERROR_RESPONSE);
        for(bool write:{false,true})for(unsigned a:{0x20000u,0x2fffcu,0x1a110000u})
            request(cva6,a,write,false,false,TLM_COMMAND_ERROR_RESPONSE);
        request(ibex,0x10000,true,false,false,TLM_OK_RESPONSE);
        request(ibex,0x10000,false,false,false,TLM_OK_RESPONSE); // Trusted verification read.
        request(ibex,0x1f000,true,false,false,TLM_OK_RESPONSE);
        request(ibex,0x1f000,false,false,false,TLM_OK_RESPONSE);
        request(ibex,0x20000,true,false,false,TLM_OK_RESPONSE);
        request(ibex,0x2f000,false,true,false,TLM_COMMAND_ERROR_RESPONSE);
        request(ibex,0x2f000,false,true,true,TLM_OK_RESPONSE);
        request(ibex,0x1a110000,false,false,false,TLM_COMMAND_ERROR_RESPONSE);
        request(ibex,0x1a110000,false,false,true,TLM_OK_RESPONSE);
        request(dbg,0x1a110000,false,false,false,TLM_OK_RESPONSE);
        for(unsigned a:{0x01000000u,0x40000000u,0x40100000u,0x60000000u}){
            request(cva6,a,false,true,false,TLM_COMMAND_ERROR_RESPONSE);
            request(ibex,a,false,true,false,TLM_COMMAND_ERROR_RESPONSE);
        }
        bus.finish_staging();
        request(ibex,0x10000,false,false,false,TLM_COMMAND_ERROR_RESPONSE);
        request(ibex,0x10000,true,false,false,TLM_COMMAND_ERROR_RESPONSE);
        request(ibex,0x20000,true,false,false,TLM_COMMAND_ERROR_RESPONSE);
        request(ibex,0x20000,false,true,false,TLM_OK_RESPONSE);
        request(cva6,0x10000,true,false,false,TLM_OK_RESPONSE);
        request(cva6,0x1f000,false,true,false,TLM_COMMAND_ERROR_RESPONSE);
        request(cva6,0x1f000,false,true,true,TLM_OK_RESPONSE);
        request(cva6,0x1f000,true,false,false,TLM_COMMAND_ERROR_RESPONSE);
        request(cva6,0x1f000,true,false,true,TLM_COMMAND_ERROR_RESPONSE);
        request(dbg,0x1f000,true,false,false,TLM_OK_RESPONSE);
        bus.quiesce();bus.reset();bus.resume();request(ibex,0x20000,true,false,false,TLM_OK_RESPONSE);
        reset_n=false;source=false;settle();reset_n=true;settle();
        source=true;settle();source=false;settle();expect(level,"IRQ pulse is held until service acknowledgment");
        adapter.acknowledge();settle();expect(!level,"ack clears an ended pulse");
        source=true;settle();adapter.acknowledge();settle();expect(level,"held source wins simultaneous clear");
        reset_n=false;settle();expect(!level,"reset clears pending adapter state");
        finished=true;sc_core::sc_stop();
    }
};
int sc_main(int,char**){CommonTest t{"test"};sc_core::sc_start();return !t.finished||t.failures?1:0;}
