#include "support/fixture.h"
#include <fu3_uart/uart_bank.h>
#include <fu3_bus/bus_system.h>

struct Bench : sc_core::sc_module, Checks {
    static fu3_bus::BusConfig bus_config() {
        auto c=fu3_bus::BusConfig::fu3();
        for(auto& target:c.targets)target.enabled=target.name=="UART0"||target.name=="UART1"||target.name=="UART2";
        return c;
    }
    static std::array<fu3_uart::Config,3> configs() {
        std::array<fu3_uart::Config,3> c;
        c[0].clock_period=sc_core::sc_time(50,sc_core::SC_NS); // HAS boot source: 20 MHz.
        c[1].clock_period=sc_core::sc_time(10,sc_core::SC_NS);
        c[2].clock_period=sc_core::sc_time(13,sc_core::SC_NS);
        return c;
    }
    fu3_bus::BusSystem bus{"bus",bus_config()};
    fu3_uart::UartBank bank{"bank",configs()};
    sc_core::sc_vector<Pins> pins{"pins",3};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"},dma{"dma"};
    sc_core::sc_vector<sc_core::sc_signal<bool>> plic_source{"plic_source",12};
    sc_core::sc_signal<bool> fast7{"fast7"},fast4{"fast4"};
    SC_HAS_PROCESS(Bench);
    explicit Bench(sc_core::sc_module_name name):sc_module(name) {
        cpu.bind(bus.initiator("IBEX"));dma.bind(bus.initiator("DMA0"));
        for(unsigned i=0;i<3;++i){pins[i].bind(*bank.controllers[i]);bus.target("UART"+std::to_string(i)).bind(bank.controllers[i]->socket);}
        SC_METHOD(route);for(auto& p:pins)sensitive<<p.irq;
        SC_THREAD(run);SC_THREAD(watchdog);
    }
    void route(){plic_source[2]=pins[0].irq;plic_source[10]=pins[1].irq;plic_source[11]=pins[2].irq;
        fast7=pins[0].irq;fast4=pins[1].irq.read()||pins[2].irq.read();}
    void settle(){for(unsigned i=0;i<6;++i)wait(sc_core::SC_ZERO_TIME);}
    unsigned access(unsigned i,bool write,unsigned offset,unsigned value=0) {
        unsigned char data=static_cast<unsigned char>(value);tlm::tlm_generic_payload tx;
        // Deliberately independent constants verify the bank's published address map.
        const std::uint64_t bases[]{0x440d0000,0x440e0000,0x50050000};
        tx.set_address(bases[i]+offset);tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_data_ptr(&data);tx.set_data_length(1);tx.set_streaming_width(1);fu3_bus::ScopedAccess metadata(tx,1);
        auto delay=sc_core::sc_time(2,sc_core::SC_NS);cpu->b_transport(tx,delay);settle();
        expect(tx.is_response_ok()&&delay==sc_core::SC_ZERO_TIME&&tx.get_address()==bases[i]+offset,"byte MMIO through actual FU3 bus");
        return data;
    }
    void burst(bool write,unsigned char* data) {
        tlm::tlm_generic_payload tx;tx.set_address(0x440e0000);tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_data_ptr(data);tx.set_data_length(4);tx.set_streaming_width(4);
        fu3_bus::ScopedAccess metadata(tx,1,fu3_bus::Burst::Fixed);auto delay=sc_core::SC_ZERO_TIME;
        dma->b_transport(tx,delay);settle();expect(tx.is_response_ok()&&delay==sc_core::SC_ZERO_TIME,"DMA fixed byte transfers to THR/RBR");
    }
    void run() {
        for(auto& p:pins){p.reset=false;p.cts=false;}wait(200,sc_core::SC_NS);
        for(auto& p:pins)p.reset=true;wait(200,sc_core::SC_NS);
        for(unsigned i=0;i<3;++i){
            const unsigned div=i==0?65:2;
            access(i,true,3,0x9b);access(i,true,0,div);access(i,true,1,0);access(i,true,2,0x21);
            access(i,true,3,0x1b); // 8 data, even parity, one stop: explicit boot parity choice.
            access(i,true,4,0x10); // Internal loopback still traverses serializer and receiver.
            access(i,true,1,1);access(i,true,7,0x80+i);access(i,true,0,0x41+i);
        }
        wait(1,sc_core::SC_MS);settle();
        for(unsigned i=0;i<3;++i){expect(access(i,false,7)==0x80+i,"three independent UART banks");
            expect((access(i,false,2)&15)==4,"RDA through each controller");}
        expect(plic_source[2]&&plic_source[10]&&plic_source[11]&&fast7&&fast4,"fake PLIC and Ibex routes");
        expect(access(1,false,0)==0x42,"UART1 received byte");expect(!plic_source[10]&&fast4,"UART2 retains shared fast interrupt");
        expect(access(2,false,0)==0x43&&access(0,false,0)==0x41,"independent serial results including boot-rate UART0");
        expect(!fast7&&!fast4,"all receive causes cleared");
        // DMA channels are platform-owned: UART1 RX/TX map to HAS channels 2/3.
        // Select 16-byte mode under DLAB: trigger encoding 01 then means four bytes.
        access(1,true,3,0x9b);access(1,true,2,0x49);access(1,true,3,0x1b);
        wait(50,sc_core::SC_NS);expect(pins[1].tx_req,"UART1 DMA requests TX capacity");
        unsigned char data[]{0x11,0x22,0x33,0x44};burst(true,data);
        pins[1].tx_clear=true;wait(40,sc_core::SC_NS);expect(!pins[1].tx_req,"TX DMA clear pulse");pins[1].tx_clear=false;
        wait(20,sc_core::SC_US);expect(pins[1].rx_req,"RX DMA reaches trigger");
        unsigned char received[4]{};burst(false,received);
        for(unsigned i=0;i<4;++i)expect(received[i]==data[i],"DMA data order across APB and serial engine");
        pins[1].rx_clear=true;wait(40,sc_core::SC_NS);pins[1].rx_clear=false;wait(40,sc_core::SC_NS);
        expect(!pins[1].rx_req,"RX DMA clears after drain");
        for(auto& p:pins)p.reset=false;wait(200,sc_core::SC_NS);
        for(auto& p:pins)expect(p.tx&&!p.irq&&!p.tx_req&&!p.rx_req,"all three reset independently wired");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(5,sc_core::SC_MS);expect(false,"integration simulation watchdog");sc_core::sc_stop();}
};
int sc_main(int,char**) {
    Bench bench{"bench"};sc_core::sc_start();
    std::cout<<"UART integration: "<<bench.checks<<" checks, "<<bench.failures<<" failures\n";
    return bench.failures||!bench.finished?1:0;
}
