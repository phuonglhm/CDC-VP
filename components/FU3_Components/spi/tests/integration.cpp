#include "support/fixture.h"
#include <fu3_spi/spi_bank.h>
#include <fu3_bus/bus_system.h>
struct Bench:sc_core::sc_module,Checks {
    static fu3_bus::BusConfig config(){auto c=fu3_bus::BusConfig::fu3();for(auto& t:c.targets)t.enabled=t.name=="SPI0"||t.name=="SPI1"||t.name=="SPI2"||t.name=="SPI3";return c;}
    fu3_bus::BusSystem bus{"bus",config()};
    fu3_spi::SpiBank bank{"bank",16,{sc_core::sc_time(10,sc_core::SC_NS),sc_core::sc_time(13,sc_core::SC_NS),sc_core::sc_time(17,sc_core::SC_NS),sc_core::sc_time(23,sc_core::SC_NS)}};
    sc_core::sc_vector<Pins> pins{"pins",4};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"},dma{"dma"};
    sc_core::sc_vector<sc_core::sc_signal<bool>> plic{"plic",24};
    sc_core::sc_signal<bool> fast6{"fast6"},fast3{"fast3"};
    sc_core::sc_event concurrent_start,concurrent_done;
    unsigned concurrent_completed=0;
    SC_HAS_PROCESS(Bench);
    Bench(sc_core::sc_module_name n):sc_module(n){cpu.bind(bus.initiator("IBEX"));dma.bind(bus.initiator("DMA0"));
        for(unsigned i=0;i<4;++i){bus.target("SPI"+std::to_string(i)).bind(bank.controllers[i]->socket);pins[i].bind(*bank.controllers[i]);}
        SC_METHOD(route);for(auto& p:pins)sensitive<<p.irq;SC_THREAD(run);SC_THREAD(watchdog);
        SC_THREAD(concurrent_cpu);SC_THREAD(concurrent_dma);}
    void route(){plic[9]=pins[0].irq;plic[16]=pins[1].irq;plic[22]=pins[2].irq;plic[23]=pins[3].irq;
        fast6=pins[0].irq;fast3=pins[1].irq.read()||pins[2].irq.read()||pins[3].irq.read();}
    unsigned access(unsigned i,bool wr,unsigned off,unsigned v=0){
        unsigned char b[4];for(unsigned j=0;j<4;++j)b[j]=v>>(8*j);tlm::tlm_generic_payload t;
        t.set_command(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);const auto address=fu3_spi::SpiBank::bases[i]+off;
        t.set_address(address);t.set_data_ptr(b);t.set_data_length(4);t.set_streaming_width(4);fu3_bus::ScopedAccess meta(t,4);
        auto delay=sc_core::sc_time(2,sc_core::SC_NS);cpu->b_transport(t,delay);
        expect(t.is_response_ok()&&t.get_address()==address&&delay==sc_core::SC_ZERO_TIME,"bus transfer and address restoration");
        unsigned result=0;for(unsigned j=0;j<4;++j)result|=unsigned(b[j])<<(8*j);return result;
    }
    void burst(bool wr,unsigned char* data){tlm::tlm_generic_payload t;t.set_command(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        t.set_address(FU3_SPI0_BASE+(wr?12:16));t.set_data_ptr(data);t.set_data_length(16);t.set_streaming_width(16);
        fu3_bus::ScopedAccess meta(t,4,fu3_bus::Burst::Fixed);auto d=sc_core::SC_ZERO_TIME;dma->b_transport(t,d);expect(t.is_response_ok(),"fake DMA fixed-address burst through real bus");}
    void concurrent_cpu(){
        wait(concurrent_start);
        for(unsigned i=0;i<4;++i)access(0,true,FU3_SPI_TXDR,0x91+i);
        ++concurrent_completed;concurrent_done.notify(sc_core::SC_ZERO_TIME);
    }
    void concurrent_dma(){
        wait(concurrent_start);
        unsigned char data[16]={0x41,0,0,0,0x42,0,0,0,0x43,0,0,0,0x44,0,0,0};
        burst(true,data);++concurrent_completed;concurrent_done.notify(sc_core::SC_ZERO_TIME);
    }
    void run(){
        for(auto& p:pins){p.reset_n=false;p.apb_reset_n=false;}wait(30,sc_core::SC_NS);
        for(auto& p:pins){p.reset_n=true;p.apb_reset_n=true;}wait(30,sc_core::SC_NS);
        const std::uint64_t golden[4]={0x440c0000,0x440b0000,0x50090000,0x500c0000};
        for(unsigned i=0;i<4;++i){expect(fu3_spi::SpiBank::bases[i]==golden[i],"independent expected SoC map");
            access(i,true,0,0xe2);access(i,true,4,2+i);access(i,true,28,4);access(i,true,12,0x81+i);access(i,true,12,0x51+i);}
        for(unsigned i=0;i<4;++i)access(i,true,0,0xe3);
        wait(20,sc_core::SC_US);
        for(unsigned i=0;i<4;++i){expect(pins[i].irq,"all SPI instances complete independently");expect(access(i,false,16)==0x81+i&&access(i,false,16)==0x51+i,"data crosses both PERIBUS paths and SPI pins");}
        expect(plic[9]&&plic[16]&&plic[22]&&plic[23]&&fast6&&fast3,"fake interrupt routing");
        access(1,true,32,4);wait(50,sc_core::SC_NS);expect(!plic[16]&&fast3,"one OR contributor cleared while peers remain asserted");
        for(unsigned i:{0u,2u,3u})access(i,true,32,4);wait(100,sc_core::SC_NS);expect(!fast6&&!fast3,"all IRQ causes clear");
        // DMA stand-in respects the request/burst/acknowledge return-to-zero contract.
        access(0,true,0,0xe2);access(0,true,4,3);access(0,true,36,0x40403);access(0,true,0,0xe3);
        wait(50,sc_core::SC_NS);expect(pins[0].tr,"TX DMA request for four free slots");
        unsigned char data[16]={0x11,0,0,0,0x22,0,0,0,0x33,0,0,0,0x44,0,0,0};burst(true,data);
        pins[0].ta=true;wait(50,sc_core::SC_NS);expect(!pins[0].tr,"TX request acknowledged after bus burst");pins[0].ta=false;
        wait(4,sc_core::SC_US);expect(pins[0].rr,"RX DMA threshold reached");
        unsigned char received[16]={};burst(false,received);for(unsigned i=0;i<16;++i)expect(received[i]==data[i],"DMA loopback data");
        expect(pins[0].rr,"RX request held until acknowledgement despite empty FIFO");
        pins[0].ra=true;wait(50,sc_core::SC_NS);expect(!pins[0].rr,"RX acknowledge drops request");pins[0].ra=false;
        access(0,true,36,0);wait(50,sc_core::SC_NS);expect(!pins[0].tr&&!pins[0].rr,"DMA disabled");
        // Two independent initiators contend for the same real SPI FIFO, while
        // another real SPI transfers through the other peripheral bridge.
        for(unsigned i:{0u,2u}){access(i,true,FU3_SPI_CTRL,0xe2);access(i,true,FU3_SPI_ISR,0x7c);access(i,true,FU3_SPI_DIV,9);}
        access(2,true,FU3_SPI_TXDR,0xb1);access(2,true,FU3_SPI_TXDR,0xb2);
        access(0,true,FU3_SPI_CTRL,0xe3);access(2,true,FU3_SPI_CTRL,0xe3);
        concurrent_start.notify(sc_core::SC_ZERO_TIME);
        while(concurrent_completed<2)wait(concurrent_done);
        wait(100,sc_core::SC_US);
        unsigned cpu_words=0,dma_words=0;
        expect(((access(0,false,FU3_SPI_STAT)>>16)&127)==8,"concurrent CPU/DMA produces exactly eight RX frames");
        for(unsigned i=0;i<8;++i){
            const auto value=access(0,false,FU3_SPI_RXDR);
            if(value>=0x91&&value<=0x94){expect(value==0x91+cpu_words,"CPU stream remains ordered under DMA contention");++cpu_words;}
            else if(value>=0x41&&value<=0x44){expect(value==0x41+dma_words,"DMA stream remains ordered under CPU contention");++dma_words;}
            else expect(false,"concurrent frame has an unexpected or duplicated payload");
        }
        expect(cpu_words==4&&dma_words==4,"both contending initiators make complete progress");
        expect(!(access(0,false,FU3_SPI_ISR)&(FU3_SPI_OVR|FU3_SPI_TXOVF|FU3_SPI_RXUDF)),"concurrent producers cause no FIFO loss");
        expect(access(2,false,FU3_SPI_RXDR)==0xb1&&access(2,false,FU3_SPI_RXDR)==0xb2,"PERIBUS_1 peer retains ordered data during PERIBUS_0 contention");
        for(auto& p:pins)p.reset_n=false;wait(20,sc_core::SC_NS);for(auto& p:pins)expect(!p.irq&&!p.tr&&!p.rr&&p.ss.read()==15,"reset all four controllers");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(1,sc_core::SC_MS);expect(false,"integration watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**){Bench b{"bench"};sc_core::sc_start();std::cout<<"SPI integration: "<<b.checks<<" checks, "<<b.failures<<" failures\n";return b.failures||!b.finished?1:0;}
