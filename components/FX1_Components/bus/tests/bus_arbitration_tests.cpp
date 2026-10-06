#include "bus/bus_system.h"
#include "support/probe_target.h"
#include "support/test_helpers.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

struct Worker : Master {
    std::function<void()> body;
    SC_HAS_PROCESS(Worker);
    explicit Worker(sc_core::sc_module_name n):Master(n){SC_THREAD(run);}
    void run(){body();}
};
struct Test : sc_core::sc_module, Checks {
    static constexpr unsigned masters_count=8, rounds=6;
    bus::BusConfig cfg;
    bus::BusSystem fabric;
    sc_core::sc_vector<Worker> workers;
    Worker independent{"independent"};
    std::vector<std::unique_ptr<bus::test::ProbeTarget>> targets;
    std::vector<unsigned> order;
    sc_core::sc_event start_event, done_event;
    unsigned completed=0, path=0;
    bool exception_phase=false, finished=false;
    sc_core::sc_time independent_done, first_done;
    SC_HAS_PROCESS(Test);
    static bus::BusConfig config() {
        auto c=bus::BusConfig::fx1();
        c.initiators.push_back({"independent"});c.targets.clear();
        unsigned index=0;
        for(auto p:{bus::TargetPath::SysBus1Axi,bus::TargetPath::SysBus0Axi,bus::TargetPath::Peribus0Apb,bus::TargetPath::Peribus1Apb}) {
            c.targets.push_back({"target"+std::to_string(index),0x20000000+index*0x10000ULL,0x1000,p,true});
            ++index;
        }
        c.targets.push_back({"spare",0x60000000,0x1000,bus::TargetPath::SysBus1Axi,true});
        return c;
    }
    explicit Test(sc_core::sc_module_name n):sc_module(n),cfg(config()),fabric("fabric",cfg),workers("workers",masters_count) {
        for(unsigned i=0;i<masters_count;++i){workers[i].socket.bind(fabric.initiator(cfg.initiators[i].name));workers[i].body=[this,i]{work(i);};}
        independent.socket.bind(fabric.initiator("independent"));independent.body=[this]{free_output();};
        for(unsigned i=0;i<cfg.targets.size();++i){
            auto ip=std::make_unique<bus::test::ProbeTarget>(sc_core::sc_gen_unique_name("ip"),0x1000);
            ip->latency_ns=i==4?1:20;
            ip->on_visit=[this,i](const auto& v){if(i==path)order.push_back(v.value);};
            fabric.target(cfg.targets[i].name).bind(ip->socket);targets.push_back(std::move(ip));
        }
        SC_THREAD(control);SC_THREAD(watchdog);
    }
    void complete(){++completed;done_event.notify(sc_core::SC_ZERO_TIME);}
    void access(Master& master,unsigned id,unsigned target_index) {
        unsigned char data[4]={static_cast<unsigned char>(id),0,0,0};
        tlm::tlm_generic_payload tx;const auto addr=cfg.targets[target_index].base+16;
        payload(tx,tlm::TLM_WRITE_COMMAND,addr,data,4);sc_core::sc_time delay=sc_core::SC_ZERO_TIME;
        bool threw=false;
        try{master.socket->b_transport(tx,delay);expect(tx.is_response_ok(),"contended transaction succeeds");}
        catch(const std::runtime_error&){threw=true;}
        expect(threw==(exception_phase&&id==0&&target_index==path),"only injected request throws");
        expect(tx.get_address()==addr,"restore address on success and exception");
    }
    void work(unsigned id){
        while(true){wait(start_event);wait(id,sc_core::SC_NS);
            for(unsigned r=0;r<(exception_phase?1U:rounds);++r){
                access(workers[id],id,path);
                if(id==0&&r==0)first_done=sc_core::sc_time_stamp();
            }
            complete();
        }
    }
    void free_output(){
        while(true){wait(start_event);wait(1,sc_core::SC_NS);access(independent,99,4);independent_done=sc_core::sc_time_stamp();complete();}
    }
    void control(){
        for(path=0;path<4;++path) for(bool inject:{false,true}) {
            exception_phase=inject;completed=0;order.clear();targets[path]->throw_once=inject;
            start_event.notify(sc_core::SC_ZERO_TIME);
            while(completed<masters_count+1)wait(done_event);
            expect(order.size()==masters_count*(inject?1:rounds),"all queued requests reach target");
            for(unsigned i=0;i<order.size();++i)expect(order[i]==i%masters_count,"FIFO order, no flood bypass");
            expect(independent_done<first_done,"independent output completes while other target busy");
            // Exception has unwound both SYSBUS routers and APB lock, if present.
            exception_phase=false;access(workers[0],0,path);
        }
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(1,sc_core::SC_MS);expect(false,"arbitration watchdog (queue stuck)");sc_core::sc_stop();}
};
int sc_main(int,char**){Test t{"test"};sc_core::sc_start();std::cout<<"Arbitration: "<<t.checks<<" checks, "<<t.failures<<" failures\n";return t.finished&&!t.failures?0:1;}
