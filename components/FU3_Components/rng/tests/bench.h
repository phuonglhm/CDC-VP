#pragma once
#include <fu3_rng/rng.h>
#include <fu3_bus/bus_system.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
using namespace sc_core;
struct Bench:sc_module {
    unsigned samples=0,checks=0,failures=0,error_edges=0;
    bool finished=false;
    static fu3_bus::BusConfig configuration(){auto c=fu3_bus::BusConfig::fu3();for(auto& t:c.targets)t.enabled=t.name=="RNG";return c;}
    fu3_rng::Rng rng{"rng",{sc_time(10,SC_NS),[this](std::uint64_t index){++samples;return bool(0xa5963cc3u&(1u<<(31-index%32)));}}};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    sc_signal<bool> reset_n{"reset_n"},irq{"irq"},plic20{"plic20"},fast10{"fast10"};
#ifdef RNG_INTEGRATION
    fu3_bus::BusSystem bus{"bus",configuration()};
#endif
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_module_name n):sc_module(n){rng.reset_n(reset_n);rng.irq(irq);
#ifdef RNG_INTEGRATION
        cpu.bind(bus.initiator("IBEX"));bus.target("RNG").bind(rng.socket);
#else
        cpu.bind(rng.socket);
#endif
        SC_METHOD(route);sensitive<<irq;SC_THREAD(run);SC_THREAD(watchdog);
    }
    void route(){plic20=irq.read();fast10=irq.read();if(irq.read())++error_edges;}
    void expect(bool ok,const char* m){++checks;if(!ok){++failures;std::cerr<<sc_time_stamp()<<": "<<m<<'\n';}}
    void settle(){for(unsigned i=0;i<5;++i)wait(SC_ZERO_TIME);}
    unsigned access(unsigned offset,bool write=false,unsigned value=0,unsigned mask=15,
                    tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE){
        unsigned char data[4],be[4];for(unsigned i=0;i<4;++i){data[i]=static_cast<unsigned char>(value>>(8*i));be[i]=(mask&(1u<<i))?255:0;}
        tlm::tlm_generic_payload tx;tx.set_address(offset);
#ifdef RNG_INTEGRATION
        tx.set_address(0x44030000ULL+offset);
#endif
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);tx.set_data_ptr(data);tx.set_data_length(4);tx.set_streaming_width(4);
        tx.set_byte_enable_ptr(be);tx.set_byte_enable_length(4);fu3_bus::ScopedAccess metadata(tx,4);
        auto delay=sc_time(2,SC_NS);cpu->b_transport(tx,delay);settle();expect(tx.get_response_status()==expected,"MMIO response");
        expect(delay==SC_ZERO_TIME&&!tx.is_dmi_allowed(),"delay consumed and DMI disabled");
        value=0;for(unsigned i=0;i<4;++i)value|=unsigned(data[i])<<(8*i);return value;
    }
    void wr(unsigned o,unsigned v,unsigned mask=15){access(o,true,v,mask);}
    unsigned rd(unsigned o){return access(o);}
    void reset(){reset_n=false;wait(30,SC_NS);reset_n=true;wait(20,SC_NS);}
    void ready(){for(unsigned i=0;i<200;++i){if(rd(12)&1)return;wait(10,SC_NS);}expect(false,"VALID polling watchdog");}
    void run(){
        reset();for(unsigned o=0;o<=16;o+=4)expect(rd(o)==0,"reset values");
        wr(8,0x12345678);wr(8,0xaabbccdd,5);expect(rd(8)==0x12bb56dd,"DELAY byte strobes");
        wr(8,0xffffffff,0);expect(rd(8)==0x12bb56dd,"zero strobes preserve DELAY");
        wr(0,1,2);expect(rd(0)==0,"ENABLE requires lane zero");wr(0,0xffffffff);expect(rd(0)==1,"CONTROL reserved mask");
        wr(8,0);wait(400,SC_NS);expect(rd(12)==0&&samples==0,"no spontaneous collection in VP schedule");
        wr(4,1,2);wait(400,SC_NS);expect(rd(12)==0,"NEXT requires lane zero");
        wr(4,1);wait(100,SC_NS);expect(rd(12)==0&&rd(16)==0,"no premature valid or data");
        ready();expect(rd(16)==0xa5963cc3&&rd(16)==0xa5963cc3,"32-bit collection and non-consuming reads");
        expect(irq&&plic20&&fast10,"VALID IRQ reaches fake sinks");expect(rd(4)==0,"COMMAND reads zero");
        wr(0,0);expect(rd(12)==1&&rd(16)==0xa5963cc3,"disable retains valid result in VP policy");
        wr(4,1);expect(!irq&&rd(16)==0,"NEXT clears valid and data while disabled");
        auto before=samples;wait(400,SC_NS);expect(samples==before,"disabled collection paused");wr(0,1);ready();
        wr(4,2);expect(rd(12)==0&&!irq,"RESTART clears valid IRQ");wait(400,SC_NS);expect(rd(12)==0,"restart waits for NEXT in VP policy");
        wr(4,3);ready();expect(rd(16)==0xa5963cc3,"combined RESTART NEXT starts fresh collection");
        wr(4,1);wait(80,SC_NS);wr(4,1);wait(80,SC_NS);expect(rd(12)==0,"NEXT restarts unfinished collection");ready();
        wr(4,2);wr(8,0xffffffff);wr(4,1);before=samples;wait(500,SC_NS);expect(samples==before&&rd(12)==0,"maximum DELAY does not prematurely sample");
        wr(8,0);wr(4,1);ready();expect(rd(12)==1,"NEXT resets counter after delay change");
#ifndef RNG_INTEGRATION
        // An active-low pulse entirely between model edges must not reset the block.
        wait(1,SC_NS);reset_n=false;wait(1,SC_NS);reset_n=true;wait(10,SC_NS);expect(rd(12)==1,"synchronous reset ignores between-edge pulse");
#endif
        reset();expect(rd(12)==0&&!irq,"reset clears pending valid");
        auto edges=error_edges;access(12,true,1,15,tlm::TLM_COMMAND_ERROR_RESPONSE);expect(error_edges>edges,"RO write raises transient local error IRQ");
        wait(30,SC_NS);settle();expect(!irq,"APB error is not sticky");
        access(16,true,1,0,tlm::TLM_COMMAND_ERROR_RESPONSE);
        expect(access(20,false,0,15,tlm::TLM_ADDRESS_ERROR_RESPONSE)==0,"unmapped read errors with zero data");
#ifndef RNG_INTEGRATION
        access(1,false,0,15,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        unsigned char data[4]{};tlm::tlm_generic_payload tx;tx.set_address(20);tx.set_command(tlm::TLM_READ_COMMAND);
        tx.set_data_ptr(data);tx.set_data_length(4);tx.set_streaming_width(4);wait(30,SC_NS);settle();edges=error_edges;
        expect(cpu->transport_dbg(tx)==0,"invalid debug read rejected");settle();expect(error_edges==edges,"debug read never causes physical IRQ");
#endif
        reset();wr(0,1);wr(8,3);wr(4,1);wait(80,SC_NS);reset();expect(rd(12)==0&&rd(16)==0&&rd(8)==0,"reset during collection");
        finished=true;sc_stop();
    }
    void watchdog(){wait(100,SC_US);expect(false,"simulation watchdog");sc_stop();}
};
