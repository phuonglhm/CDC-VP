#include <fu3_timer/timer_bank.h>
#include <fu3_bus/bus_system.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
struct Bench:sc_core::sc_module {
    static fu3_bus::BusConfig config(){auto c=fu3_bus::BusConfig::fu3();for(auto& t:c.targets)t.enabled=t.name=="TIMER0"||t.name=="TIMER1"||t.name=="TIMER2"||t.name=="TIMER3";return c;}
    fu3_bus::BusSystem bus{"bus",config()};
    fu3_timer::TimerBank bank{"bank",{sc_core::sc_time(10,sc_core::SC_NS),sc_core::sc_time(13,sc_core::SC_NS),sc_core::sc_time(17,sc_core::SC_NS),sc_core::sc_time(23,sc_core::SC_NS)}};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    sc_core::sc_signal<bool> reset_n{"reset_n"};
    sc_core::sc_vector<sc_core::sc_signal<bool>> gate{"gate",4},ov{"ov",4},cmp{"cmp",4},irq{"irq",4};
    unsigned checks=0,failures=0;
    unsigned events[4]={};bool finished=false;
    // Fake receivers expose exactly the Timer-related SoC routes.
    sc_core::sc_signal<bool> cva6_time{"cva6_time"},ibex_time{"ibex_time"},ibex_fast0{"ibex_fast0"};
    sc_core::sc_vector<sc_core::sc_signal<bool>> plic{"plic",9};
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name n):sc_module(n){
        cpu.bind(bus.initiator("IBEX"));
        for(unsigned i=0;i<4;++i){auto& t=*bank.timers[i];bus.target("TIMER"+std::to_string(i)).bind(t.socket);
            t.reset_n(reset_n);t.clock_enabled(gate[i]);t.overflow_irq(ov[i]);t.compare_irq(cmp[i]);t.irq(irq[i]);}
        SC_METHOD(receive);for(unsigned i=0;i<4;++i)sensitive<<irq[i];
        SC_THREAD(run);SC_THREAD(watchdog);
    }
    void expect(bool ok,const char* why){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<why<<"\n";}}
    void receive(){for(unsigned i=0;i<4;++i)if(irq[i].event()&&irq[i].read())++events[i];
        cva6_time=irq[0];plic[6]=irq[1];plic[7]=irq[2];plic[8]=irq[3];ibex_time=irq[1];ibex_fast0=irq[2].read()||irq[3].read();}
    void settle(){for(unsigned i=0;i<5;++i)wait(sc_core::SC_ZERO_TIME);}
    unsigned transfer(unsigned i,bool write,unsigned off,unsigned v=0,unsigned len=4,tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE){
        unsigned char b[4];for(unsigned j=0;j<4;++j)b[j]=v>>(8*j);
        tlm::tlm_generic_payload tx;tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        auto address=fu3_timer::TimerBank::bases[i]+off;tx.set_address(address);tx.set_data_ptr(b);tx.set_data_length(len);tx.set_streaming_width(len);
        fu3_bus::ScopedAccess access(tx,len);sc_core::sc_time d=sc_core::sc_time(1,sc_core::SC_NS);cpu->b_transport(tx,d);
        expect(tx.get_response_status()==expected,"bus transaction response");expect(tx.get_address()==address&&d==sc_core::SC_ZERO_TIME,"bus restores global address and consumes delay");
        settle();unsigned result=0;for(unsigned j=0;j<4;++j)result|=unsigned(b[j])<<(8*j);return result;
    }
    void run(){
        reset_n=false;for(auto& g:gate)g=true;wait(3,sc_core::SC_NS);reset_n=true;
        for(unsigned i=0;i<4;++i)transfer(i,true,0,100+i);
        for(unsigned i=0;i<4;++i)expect(transfer(i,false,0)==100+i,"independent register state through both PERIBUS paths");
        transfer(0,true,0,7,1);
        expect(transfer(0,false,0)==7,"bus narrow write selects the timer low byte under Common Full strobes");
        for(unsigned i=0;i<4;++i){
            transfer(i,true,8,4); // clear counter; disabled until configured
            transfer(i,true,0,4); // produces an IRQ even while disabled
            expect(irq[i],"selected timer raises IRQ");
            if(i==0)expect(cva6_time,"Timer0 direct CVA6 route");
            else expect(plic[i+5],"separate PLIC timer source");
            if(i==1)expect(ibex_time,"Timer1 Ibex timer route");
            if(i>=2)expect(ibex_fast0,"Timer2/3 Ibex OR route");
            transfer(i,true,4,1);
        }
        unsigned before[4];for(unsigned i=0;i<4;++i)before[i]=events[i];
        wait(600,sc_core::SC_NS);settle();
        for(unsigned i=0;i<4;++i)expect(events[i]>before[i]+2,"all four timers continue periodic operation independently");
        gate[0]=false;settle();auto stopped=events[0],peer=events[3];wait(600,sc_core::SC_NS);settle();
        expect(events[0]==stopped&&events[3]>peer,"gating Timer0 does not block Timer3");
        reset_n=false;settle();for(unsigned i=0;i<4;++i)expect(!irq[i],"reset deasserts all IRQs");
        expect(!cva6_time&&!ibex_time&&!ibex_fast0&&!plic[6]&&!plic[7]&&!plic[8],"receivers observe reset");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(100,sc_core::SC_US);expect(false,"integration watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**){Bench b{"bench"};sc_core::sc_start();std::cout<<"Timer integration: "<<b.checks<<" checks, "<<b.failures<<" failures\n";return b.failures||!b.finished?1:0;}
