#include <fu3_wdt/wdt_bank.h>
#include <fu3_bus/bus_system.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>

struct Bench : sc_core::sc_module {
    static fu3_bus::BusConfig config() {
        auto result=fu3_bus::BusConfig::fu3();
        for (auto& target : result.targets) target.enabled=target.name=="WDT0" || target.name=="WDT1";
        return result;
    }
    fu3_bus::BusSystem bus{"bus",config()};
    fu3_wdt::WatchdogBank bank{"bank"};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    sc_core::sc_signal<bool> preset{"preset"}, unlock0{"unlock0"}, unlock1{"unlock1"};
    sc_core::sc_vector<sc_core::sc_signal<bool>> watchdog_reset_n{"watchdog_reset_n",2};
    sc_core::sc_vector<sc_core::sc_signal<bool>> irq{"irq",2}, reset_request{"reset_request",2};
    sc_core::sc_signal<bool> ibex_nmi{"ibex_nmi"}, plic18{"plic18"};
    unsigned reset_count[2]={}, checks=0, failures=0;
    bool finished=false;
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name name) : sc_module(name) {
        cpu.bind(bus.initiator("IBEX"));
        for (unsigned i=0;i<2;++i) {
            auto& w=*bank.watchdogs[i];
            bus.target("WDT"+std::to_string(i)).bind(w.socket);
            w.preset_n(preset);
            w.wdogres_n(watchdog_reset_n[i]);
            if (i==0) w.i_wdt_other_unlock(unlock0);
            else w.i_wdt_other_unlock(unlock1);
            w.irq(irq[i]);
            w.reset_request(reset_request[i]);
        }
        SC_METHOD(route); sensitive << irq[0] << irq[1];
        SC_THREAD(run); SC_THREAD(crm0); SC_THREAD(crm1); SC_THREAD(watchdog);
    }
    void route() { ibex_nmi=irq[0]; plic18=irq[1]; }
    void crm(unsigned i) {
        watchdog_reset_n[i]=false; wait(10,sc_core::SC_NS); watchdog_reset_n[i]=true;
        while (true) {
            wait(reset_request[i].posedge_event());
            ++reset_count[i];
            wait(2,sc_core::SC_NS); watchdog_reset_n[i]=false;
            wait(10,sc_core::SC_NS); watchdog_reset_n[i]=true;
        }
    }
    void crm0() { crm(0); }
    void crm1() { crm(1); }
    void expect(bool ok,const char* message) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL @ " << sc_core::sc_time_stamp() << ": " << message << '\n'; }
    }
    void settle() { for(unsigned i=0;i<5;++i) wait(sc_core::SC_ZERO_TIME); }
    unsigned access(unsigned i,bool write,unsigned offset,unsigned value=0) {
        unsigned char bytes[4]{};
        for(unsigned j=0;j<4;++j) bytes[j]=static_cast<unsigned char>(value>>(8*j));
        tlm::tlm_generic_payload tx;
        const auto address=fu3_wdt::WatchdogBank::bases[i]+offset;
        tx.set_address(address); tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_data_ptr(bytes); tx.set_data_length(4); tx.set_streaming_width(4);
        fu3_bus::ScopedAccess access(tx,4);
        auto delay=sc_core::sc_time(2,sc_core::SC_NS);
        cpu->b_transport(tx,delay); settle();
        expect(tx.is_response_ok(),"V1.2 word access succeeds through the FU3 bus");
        expect(tx.get_address()==address && delay==sc_core::SC_ZERO_TIME,
               "bus restores address and consumes annotated delay");
        unsigned result=0;
        for(unsigned j=0;j<4;++j) result|=unsigned(bytes[j])<<(8*j);
        return result;
    }
    void run() {
        preset=false;unlock0=false;unlock1=false;settle();preset=true;settle();
        wait(20,sc_core::SC_NS);settle();
        for(unsigned i=0;i<2;++i){
            expect(access(i,false,FU3_WDT_LOCK)==0,"both V1.2 locks reset unlocked without SYSCSR");
            access(i,true,FU3_WDT_CNT_LD,1000);access(i,true,FU3_WDT_CNT_CTRL,3);access(i,true,FU3_WDT_LOCK,0);
        }
        wait(5100,sc_core::SC_NS);settle();
        expect(ibex_nmi&&plic18,"independent warnings reach fixture sinks; Ibex route remains provisional");
        access(0,true,FU3_WDT_CNT_LD,1);expect(access(0,false,FU3_WDT_CNT_LD)==1000,"locked service has no effect");
        access(0,true,FU3_WDT_LOCK,FU3_WDT_UNLOCK_KEY);access(0,true,FU3_WDT_CNT_LD,1000);access(0,true,FU3_WDT_LOCK,0);
        expect(!ibex_nmi&&plic18,"local WD_LOCK servicing affects only WDT0");
        wait(5000,sc_core::SC_NS);settle();
        expect(reset_count[1]==1&&reset_count[0]==0,"WDT1 recovery is independent of serviced WDT0");
        expect(access(1,false,FU3_WDT_CNT_LD)==1000&&access(1,false,FU3_WDT_CNT_CTRL)==3&&access(1,false,FU3_WDT_LOCK)==1,"watchdog-only reset retains configuration and restarts locked");
        finished=true;sc_core::sc_stop();
    }
    void watchdog() { wait(100,sc_core::SC_US); expect(false,"integration watchdog"); sc_core::sc_stop(); }
};
int sc_main(int,char**) {
    Bench bench{"bench"}; sc_core::sc_start();
    std::cout << "WDT V1.2 integration: " << bench.checks << " checks, " << bench.failures << " failures\n";
    return bench.failures || !bench.finished ? 1 : 0;
}
