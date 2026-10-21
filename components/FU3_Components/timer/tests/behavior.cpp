#include <fu3_timer/timer.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <vector>
unsigned checks=0, failures=0;
void check(bool yes,const char* message){++checks;if(!yes){++failures;std::cerr<<"FAIL: "<<message<<"\n";}}
void state_tests(){
    using fu3_timer::State;
    State s; s.edge(true,4,1);check(s.count==0,"enable write uses old control");
    s.edge();check(s.count==1,"enabled increment");
    s.edge(true,4,0);check(s.count==2 && s.control==0,"disable write permits old enabled increment");
    s.edge();check(s.count==2,"disabled hold");
    s.edge(true,8,7);check(s.count==0 && s.compare==7,"compare write clears count");
    s.edge(true,0,7);check(s.match(),"disabled software equality asserts compare");
    s.edge(true,0,7);check(s.match(),"counter write overrides interrupt clear");
    s.edge();check(s.count==0 && !s.match(),"disabled interrupt clears on next tick");
    s.edge(true,0,UINT32_MAX);check(s.overflow(),"overflow is equality, even disabled");
    s.edge(true,8,UINT32_MAX);check(s.count==0,"CMP write wins over overflow");
    s.edge(true,0,UINT32_MAX);check(s.overflow()&&s.match(),"both interrupt conditions may coexist");
    s.edge();check(!s.overflow()&&!s.match(),"both conditions clear together");
    s=State{};s.control=1;s.count=UINT32_MAX-1;
    s.edge();check(s.overflow(),"overflow not delayed until wrap");s.edge();check(s.count==0,"overflow clears counter");
    s=State{};s.control=1;s.compare=2;
    const unsigned sequence[]={1,2,0,1,2,0};
    for(auto expected:sequence){s.edge();check(s.count==expected,"compare periodic sequence includes clear tick");}
    s=State{};s.control=9; // p=1, cycle threshold=9; independent oracle: increments at ticks 2,10,12,20.
    std::vector<unsigned> ticks;
    for(unsigned t=1;t<=20;++t){auto before=s.count;s.edge();if(s.count!=before)ticks.push_back(t);}
    check(ticks==std::vector<unsigned>({2,10,12,20}),"nonstandard prescaler uses full control threshold");
    s=State{};s.control=73; // same p=1, upper bit changes threshold.
    ticks.clear();for(unsigned t=1;t<=20;++t){auto before=s.count;s.edge();if(s.count!=before)ticks.push_back(t);}
    check(ticks==std::vector<unsigned>({2,10,18}),"upper control bits affect prescaler cadence");
    s=State{};s.control=8;for(unsigned i=0;i<5;++i)s.edge();
    check(s.cycle==5 && s.count==0,"cycle counter runs while timer disabled");
    s.edge(true,4,9);check(s.cycle==6 && s.count==0,"enable does not reset prescaler phase");
    s=State{};s.control=1;s.edge(true,12,123);check(s.count==1,"reserved write does not suppress counting");
    check(s.read(12)==0,"reserved read is zero");
}
struct Bench:sc_core::sc_module {
    fu3_timer::Timer timer{"timer"};
    tlm_utils::simple_initiator_socket<Bench> master{"master"};
    sc_core::sc_signal<bool> reset_n{"reset_n"},gate{"gate"},ov{"ov"},cmp{"cmp"},irq{"irq"};
    bool finished=false, pending_done=false;
    sc_core::sc_event start_pending;
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name n):sc_module(n){
        master.bind(timer.socket);timer.reset_n(reset_n);timer.clock_enabled(gate);
        timer.overflow_irq(ov);timer.compare_irq(cmp);timer.irq(irq);
        SC_THREAD(run);SC_THREAD(watchdog);SC_THREAD(pending_access);
    }
    void settle(){wait(sc_core::SC_ZERO_TIME);wait(sc_core::SC_ZERO_TIME);wait(sc_core::SC_ZERO_TIME);}
    unsigned access(bool write,unsigned addr,unsigned value=0,unsigned length=4,unsigned char* be=nullptr,
                    tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE){
        unsigned char bytes[4];for(unsigned i=0;i<4;++i)bytes[i]=value>>(8*i);
        tlm::tlm_generic_payload tx;tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(addr);tx.set_data_ptr(bytes);tx.set_data_length(length);tx.set_streaming_width(length);
        tx.set_byte_enable_ptr(be);tx.set_byte_enable_length(be?4:0);
        sc_core::sc_time d=sc_core::SC_ZERO_TIME;master->b_transport(tx,d);
        check(tx.get_response_status()==expected,"TLM response");check(d==sc_core::SC_ZERO_TIME&&!tx.is_dmi_allowed(),"delay consumed and DMI disabled");
        settle();unsigned result=0;for(unsigned i=0;i<4;++i)result|=unsigned(bytes[i])<<(8*i);return result;
    }
    unsigned inspect(unsigned addr){
        unsigned char bytes[4]={};tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_READ_COMMAND);tx.set_address(addr);tx.set_data_ptr(bytes);tx.set_data_length(4);tx.set_streaming_width(4);
        auto before=sc_core::sc_time_stamp();check(master->transport_dbg(tx)==4,"debug read succeeds");
        check(sc_core::sc_time_stamp()==before,"debug read consumes no time");
        unsigned v=0;for(unsigned i=0;i<4;++i)v|=unsigned(bytes[i])<<(8*i);return v;
    }
    void pending_access(){
        wait(start_pending);
        access(false,0,0,4,nullptr,tlm::TLM_GENERIC_ERROR_RESPONSE);
        pending_done=true;
    }
    void run(){
        reset_n=false;gate=true;wait(3,sc_core::SC_NS);reset_n=true;settle();
        check(access(false,0)==0 && access(false,4)==0 && access(false,8)==0,"reset registers");
        access(true,8,5);access(true,0,5);check(cmp&&irq&&!ov,"disabled write asserts raw IRQ");
        wait(9,sc_core::SC_NS);check(irq,"IRQ remains until next tick");
        wait(1,sc_core::SC_NS);settle();check(!irq && inspect(0)==0,"IRQ clears next tick without software acknowledge");
        access(true,0,UINT32_MAX);check(ov&&irq,"overflow wire asserted");
        reset_n=false;settle();check(!ov&&!cmp&&!irq&&inspect(0)==0,"asynchronous reset clears state and IRQ");
        wait(3,sc_core::SC_NS);reset_n=true;settle();
        access(true,0x14,0x12345678);check(access(false,4)==0x12345678,"single-instance upper address aliases");
        access(true,4,0);access(true,0,123);access(true,12,999);check(access(false,12)==0&&access(false,0)==123,"reserved accesses succeed without register writes");
        unsigned char partial[4]={255,0,255,255};
        access(true,0,999,4,partial);
        check(inspect(0)==231,"disabled second byte retains zero instead of incoming 0x03");
        access(true,0,99,2,nullptr,tlm::TLM_BURST_ERROR_RESPONSE);
        access(true,1,99,4,nullptr,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        access(false,0x10000,0,4,nullptr,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        unsigned char full[4]={255,255,255,255};access(true,0,42,4,full);check(inspect(0)==42,"full strobes accepted");
        for(unsigned bits=0;bits<16;++bits){
            access(true,0,0xa5b6c7d8);unsigned char enables[4];unsigned mask=0;
            for(unsigned i=0;i<4;++i){enables[i]=(bits&(1u<<i))?255:0;if(enables[i])mask|=255u<<(8*i);}
            access(true,0,0x12345678,4,enables);
            check(inspect(0)==((0xa5b6c7d8u&~mask)|(0x12345678u&mask)),"all timer strobe masks preserve disabled lanes");
        }
        access(true,0,42);
        access(true,4,1);gate=false;wait(40,sc_core::SC_NS);check(inspect(0)==42,"gated clock freezes count");
        access(false,0,0,4,nullptr,tlm::TLM_GENERIC_ERROR_RESPONSE);
        gate=true;wait(10,sc_core::SC_NS);settle();check(inspect(0)==43,"ungated clock resumes count");
        reset_n=false;settle();check(inspect(4)==0,"reset control disabled");
        // Reset must cancel a request already admitted and release its access lock.
        wait(3,sc_core::SC_NS);reset_n=true;settle();
        start_pending.notify(sc_core::SC_ZERO_TIME);wait(1,sc_core::SC_NS);
        reset_n=false;settle();wait(1,sc_core::SC_NS);
        check(pending_done,"reset cancels pending access without deadlock");
        reset_n=true;settle();access(true,0,77);
        check(inspect(0)==77,"access lock reusable after reset cancellation");
        // Debug writes cannot bypass functional write semantics.
        unsigned char bytes[4]={};tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_WRITE_COMMAND);tx.set_address(0);tx.set_data_ptr(bytes);
        tx.set_data_length(4);tx.set_streaming_width(4);
        check(master->transport_dbg(tx)==0 && tx.get_response_status()==tlm::TLM_COMMAND_ERROR_RESPONSE,
              "debug writes rejected");
        check(inspect(0)==77,"debug write has no side effect");
        tx.set_command(tlm::TLM_IGNORE_COMMAND);sc_core::sc_time delay=sc_core::sc_time(3,sc_core::SC_NS);
        auto begin=sc_core::sc_time_stamp();master->b_transport(tx,delay);
        check(tx.get_response_status()==tlm::TLM_COMMAND_ERROR_RESPONSE && delay==sc_core::SC_ZERO_TIME &&
              sc_core::sc_time_stamp()-begin==sc_core::sc_time(13,sc_core::SC_NS),"invalid command and incoming delay");
        tx.set_command(tlm::TLM_READ_COMMAND);tx.set_data_ptr(nullptr);master->b_transport(tx,delay);
        check(tx.get_response_status()==tlm::TLM_GENERIC_ERROR_RESPONSE,"null data rejected");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(100,sc_core::SC_US);check(false,"behavior watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**){state_tests();Bench b{"bench"};sc_core::sc_start();std::cout<<"Timer behavior: "<<checks<<" checks, "<<failures<<" failures\n";return failures||!b.finished?1:0;}
