#include "fu3_bus/bus_system.h"
#include "support/probe_target.h"
#include "support/test_helpers.h"
#include <array>

struct FixedTarget : sc_core::sc_module, Checks {
    tlm_utils::simple_target_socket<FixedTarget> socket{"socket"};
    std::array<unsigned char,16> memory{};
    unsigned visits=0;
    explicit FixedTarget(sc_core::sc_module_name n):sc_module(n){socket.register_b_transport(this,&FixedTarget::transport);}
    void transport(tlm::tlm_generic_payload& t,sc_core::sc_time& delay){
        ++visits;const auto* a=t.get_extension<fu3_bus::Access>();
        expect(a&&a->burst==fu3_bus::Burst::Fixed&&a->beat_bytes==4,"endpoint receives original FIXED metadata");
        expect(t.get_address()==12&&t.get_data_length()==16,"endpoint receives full payload at final local word");
        for(unsigned i=0;i<t.get_data_length();++i){
            if(t.get_byte_enable_ptr()&&!t.get_byte_enable_ptr()[i%t.get_byte_enable_length()])continue;
            const auto address=t.get_address()+i%4;
            if(t.is_write())memory[address]=t.get_data_ptr()[i];else t.get_data_ptr()[i]=memory[address];
        }
        delay+=sc_core::sc_time(3,sc_core::SC_NS);t.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};
struct Safety : sc_core::sc_module, Checks {
    static fu3_bus::BusConfig config(){
        fu3_bus::BusConfig c;c.initiators={{"IBEX",2,false,true,16},{"DMA0",4,true,true,16}};
        c.targets={{"fixed1",0x1000,16,fu3_bus::TargetPath::SysBus1Axi,true},
                   {"fixed0",0x2000,16,fu3_bus::TargetPath::SysBus0Axi,true},
                   {"csr0",0x44000000,256,fu3_bus::TargetPath::Peribus0Apb,true},
                   {"csr1",0x40100000,256,fu3_bus::TargetPath::Peribus1Apb,true},
                   {"slow",0x3000,256,fu3_bus::TargetPath::SysBus1Axi,true}};
        c.targets[0].preserve_burst=c.targets[1].preserve_burst=true;
        for(unsigned i:{2u,3u}){c.targets[i].word_interface=true;c.targets[i].strict_word_offsets={0x24};}
        return c;
    }
    fu3_bus::BusSystem bus{"bus",config()};
    Master main_cpu{"main_cpu"},worker_cpu{"worker_cpu"};
    FixedTarget fixed1{"fixed1"},fixed0{"fixed0"};
    fu3_bus::test::ProbeTarget csr0{"csr0",256},csr1{"csr1",256},slow{"slow",256};
    sc_core::sc_event start,closed,done;
    bool worker_finished=false,flood_finished=false,inject=false,finished=false;
    unsigned rejected=0;
    SC_HAS_PROCESS(Safety);
    explicit Safety(sc_core::sc_module_name n):sc_module(n){
        main_cpu.socket.bind(bus.initiator("IBEX"));worker_cpu.socket.bind(bus.initiator("DMA0"));
        bus.target("fixed1").bind(fixed1.socket);bus.target("fixed0").bind(fixed0.socket);
        bus.target("csr0").bind(csr0.socket);bus.target("csr1").bind(csr1.socket);bus.target("slow").bind(slow.socket);
        slow.latency_ns=100;SC_THREAD(run);SC_THREAD(worker);SC_THREAD(flood);SC_THREAD(watchdog);
    }
    void transfer(Master& m,tlm::tlm_generic_payload& t,tlm::tlm_response_status status,unsigned incoming=0){
        const auto address=t.get_address();const auto* data=t.get_data_ptr();const auto length=t.get_data_length();
        auto delay=sc_core::sc_time(incoming,sc_core::SC_NS);m.socket->b_transport(t,delay);
        expect(t.get_response_status()==status,"safety response matches contract");
        expect(t.get_address()==address&&t.get_data_ptr()==data&&t.get_data_length()==length&&
               delay==sc_core::SC_ZERO_TIME&&!t.is_dmi_allowed(),"safety transfer restores fields and consumes delay");
    }
    void run(){
        unsigned char bytes[16];tlm::tlm_generic_payload t;
        for(const unsigned base:{0x1000u,0x2000u}){
            for(unsigned i=0;i<16;++i)bytes[i]=i+1;
            payload(t,tlm::TLM_WRITE_COMMAND,base+12,bytes,16);
            unsigned char be[3]={0xff,0,0xff};t.set_byte_enable_ptr(be);t.set_byte_enable_length(3);
            {fu3_bus::ScopedAccess a(t,4,fu3_bus::Burst::Fixed);transfer(main_cpu,t,tlm::TLM_OK_RESPONSE);}
            expect(t.get_byte_enable_ptr()==be&&t.get_byte_enable_length()==3,"preserved FIXED retains repeating strobes");
            t.set_byte_enable_ptr(nullptr);t.set_byte_enable_length(0);
            std::fill(std::begin(bytes),std::end(bytes),0);
            payload(t,tlm::TLM_READ_COMMAND,base+12,bytes,16);
            {fu3_bus::ScopedAccess a(t,4,fu3_bus::Burst::Fixed);transfer(main_cpu,t,tlm::TLM_OK_RESPONSE);}
            const unsigned char expected[4]={13,10,15,16};
            for(unsigned i=0;i<16;++i)expect(bytes[i]==expected[i%4],"independent final-word FIXED byte oracle");
            {fu3_bus::ScopedAccess a(t,4);transfer(main_cpu,t,tlm::TLM_ADDRESS_ERROR_RESPONSE);}
        }
        expect(fixed1.visits==2&&fixed0.visits==2,"INCR window rejection has no target side effects");
        for(const unsigned base:{0x44000000u,0x40100000u}){
            auto& ip=base==0x44000000u?csr0:csr1;const auto before=ip.visits.size();
            for(unsigned size:{1u,2u})for(auto cmd:{tlm::TLM_READ_COMMAND,tlm::TLM_WRITE_COMMAND}){
                payload(t,cmd,base+0x24,bytes,size);fu3_bus::ScopedAccess a(t,size);
                transfer(main_cpu,t,tlm::TLM_BURST_ERROR_RESPONSE);
            }
            unsigned char sparse[4]={0xff,0,0xff,0xff};
            payload(t,tlm::TLM_READ_COMMAND,base+0x24,bytes,4);t.set_byte_enable_ptr(sparse);t.set_byte_enable_length(4);
            {fu3_bus::ScopedAccess a(t,4);transfer(main_cpu,t,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);}
            payload(t,tlm::TLM_WRITE_COMMAND,base+0x20,bytes,8);
            {fu3_bus::ScopedAccess a(t,4);transfer(main_cpu,t,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);}
            t.set_byte_enable_ptr(nullptr);t.set_byte_enable_length(0);
            payload(t,tlm::TLM_READ_COMMAND,base+0x20,bytes,8);
            {fu3_bus::ScopedAccess a(t,2);transfer(main_cpu,t,tlm::TLM_BURST_ERROR_RESPONSE);}
            expect(ip.visits.size()==before,"every rejected footprint is checked before preceding-register side effects");
            payload(t,tlm::TLM_READ_COMMAND,base+0x24,bytes,4);
            {fu3_bus::ScopedAccess a(t,4);transfer(main_cpu,t,tlm::TLM_OK_RESPONSE);}
            payload(t,tlm::TLM_WRITE_COMMAND,base+0x01,bytes,1);
            {fu3_bus::ScopedAccess a(t,1);transfer(main_cpu,t,tlm::TLM_OK_RESPONSE);}
            expect(ip.visits.size()==before+2,"safe word and unrelated narrow CSR accesses remain supported");
        }
        for(bool fault:{false,true}){
            inject=fault;worker_finished=flood_finished=false;rejected=0;slow.throw_once=fault;
            const auto before=slow.visits.size();start.notify(sc_core::SC_ZERO_TIME);wait(1,sc_core::SC_NS);
            bool blocked=false;try{bus.reset();}catch(const std::logic_error&){blocked=true;}
            expect(blocked,"reset still rejects active admitted work");
            closed.notify(sc_core::SC_ZERO_TIME);const auto begin=sc_core::sc_time_stamp();bus.quiesce();
            expect(!bus.accepting()&&worker_finished&&sc_core::sc_time_stamp()-begin>=sc_core::sc_time(100,sc_core::SC_NS),"quiescence drains delayed work before returning");
            while(!flood_finished)wait(done);
            expect(rejected==8&&slow.visits.size()==before+1,"closed ingress cannot be monopolized by new requests");
            bus.reset();expect(!bus.accepting(),"reset keeps explicitly closed ingress closed");bus.resume();
            payload(t,tlm::TLM_READ_COMMAND,0x3000,bytes,4);
            {fu3_bus::ScopedAccess a(t,4);transfer(main_cpu,t,tlm::TLM_OK_RESPONSE);}
        }
        expect(!fixed1.failures&&!fixed0.failures,"preserved target validates its full request contract");
        finished=true;sc_core::sc_stop();
    }
    void worker(){while(true){wait(start);unsigned char b[4]{};tlm::tlm_generic_payload t;payload(t,tlm::TLM_READ_COMMAND,0x3000,b,4);
        bool threw=false;try{fu3_bus::ScopedAccess a(t,4);transfer(worker_cpu,t,tlm::TLM_OK_RESPONSE,50);}catch(const std::runtime_error&){threw=true;}
        expect(threw==inject&&t.get_address()==0x3000,"exception unwinds admitted work and restores address");worker_finished=true;done.notify(sc_core::SC_ZERO_TIME);}}
    void flood(){while(true){wait(closed);for(unsigned i=0;i<8;++i){unsigned char b[4]{};tlm::tlm_generic_payload t;payload(t,tlm::TLM_READ_COMMAND,0x3000,b,4);
        fu3_bus::ScopedAccess a(t,4);transfer(main_cpu,t,tlm::TLM_GENERIC_ERROR_RESPONSE);++rejected;}
        flood_finished=true;done.notify(sc_core::SC_ZERO_TIME);}}
    void watchdog(){wait(100,sc_core::SC_US);expect(false,"safety watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**){Safety t{"test"};sc_core::sc_start();std::cout<<"Safety: "<<t.checks<<" checks, "<<t.failures<<" failures\n";return !t.finished||t.failures?1:0;}
