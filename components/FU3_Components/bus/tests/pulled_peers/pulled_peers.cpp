#include <fu3_bus/bus_system.h>
#include <fu3/qspi/qspi.h>
#include <fu3/qspi/a2h_qspi.h>
#include <fu3/qspi/qspi_flash_model.h>
#include <fu3/gpio/gpio.h>
#include "../support/test_helpers.h"
#include <array>
#include <string>

struct GpioSignals:sc_core::sc_module {
    sc_core::sc_signal<sc_dt::sc_uint<32>> input,output,direction,sampled;
    sc_core::sc_signal<bool> irq,wake;
    sc_core::sc_vector<sc_core::sc_signal<sc_dt::sc_uint<6>>> pads{"pads",32};
    explicit GpioSignals(sc_core::sc_module_name n):sc_module(n){}
    void bind(fu3::gpio::Gpio& d,sc_core::sc_clock& clock,sc_core::sc_signal<bool>& reset){
        d.clk(clock);d.reset_n(reset);d.gpio_in(input);d.gpio_out(output);
        d.gpio_dir(direction);d.gpio_in_sync(sampled);d.interrupt(irq);d.power_event(wake);
        for(unsigned i=0;i<32;++i)d.gpio_padcfg[i](pads[i]);
    }
};
struct PulledPeers:sc_core::sc_module,Checks {
    static fu3_bus::BusConfig config(){
        auto c=fu3_bus::BusConfig::fu3();
        for(auto& t:c.targets)t.enabled=t.name=="AP_QSPIC" || t.name=="QSPI_XIP" ||
            t.name=="GPIOA" || t.name=="GPIOB" || t.name=="GPIOC";
        return c;
    }
    fu3_bus::BusSystem bus{"bus",config()};Master cpu{"cpu"};
    sc_core::sc_clock clock{"clock",10,sc_core::SC_NS},pclk{"pclk",14,sc_core::SC_NS},hclk{"hclk",22,sc_core::SC_NS};
    sc_core::sc_signal<bool> reset,clear,irq,rxreq,txreq;
    fu3::qspi::Qspi qspi{"qspi"};fu3::qspi::A2hQspi a2h{"a2h"};fu3::qspi::QspiFlashModel flash{"flash"};
    sc_core::sc_vector<fu3::gpio::Gpio> gpio{"gpio",3};sc_core::sc_vector<GpioSignals> pins{"pins",3};
    static constexpr std::array<unsigned,3> bases={0x50030000,0x50040000,0x440f0000};
    std::string scenario;bool finished=false;
    SC_HAS_PROCESS(PulledPeers);
    PulledPeers(sc_core::sc_module_name n,std::string which):sc_module(n),scenario(std::move(which)){
        cpu.socket.bind(bus.initiator("CVA6"));
        bus.target("AP_QSPIC").bind(qspi.ap_qspic);bus.target("QSPI_XIP").bind(a2h.socket);a2h.ahb.bind(qspi.xip);
        qspi.flash.bind(flash.socket);flash.reset_n(reset);flash.load({0x13,0x05,0x10,0,0x93,0x05,0x20,0,0x33,0x06,0xb5,0,0x73,0,0x10,0});
        qspi.clk(clock);qspi.pclk(pclk);qspi.hclk(hclk);qspi.dma_clk(pclk);
        qspi.reset_n(reset);qspi.preset_n(reset);qspi.hreset_n(reset);qspi.dma_reset_n(reset);
        qspi.dma_rx_clr(clear);qspi.dma_tx_clr(clear);qspi.irq(irq);qspi.dma_rx_req(rxreq);qspi.dma_tx_req(txreq);
        const char* names[]={"GPIOA","GPIOB","GPIOC"};
        for(unsigned i=0;i<3;++i){pins[i].bind(gpio[i],pclk,reset);bus.target(names[i]).bind(gpio[i].socket);}
        SC_THREAD(run);SC_THREAD(watchdog);
    }
    unsigned access(bool write,unsigned address,unsigned value=0,unsigned char* enables=nullptr,
                    tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE){
        unsigned char bytes[4];for(unsigned i=0;i<4;++i)bytes[i]=value>>(8*i);
        tlm::tlm_generic_payload t;payload(t,write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND,address,bytes,4);
        t.set_byte_enable_ptr(enables);t.set_byte_enable_length(enables?4:0);
        fu3_bus::ScopedAccess metadata(t,4);auto delay=sc_core::SC_ZERO_TIME;cpu.socket->b_transport(t,delay);
        expect(t.get_response_status()==expected,"pulled peer response at "+std::to_string(address));
        expect(t.get_address()==address && t.get_byte_enable_ptr()==enables &&
            t.get_byte_enable_length()==(enables?4u:0u) && delay==sc_core::SC_ZERO_TIME && !t.is_dmi_allowed(),"peer call restores metadata and consumes delay");
        value=0;for(unsigned i=0;i<4;++i)value|=unsigned(bytes[i])<<(8*i);return value;
    }
    void run(){
        reset=false;wait(31,sc_core::SC_NS);reset=true;wait(30,sc_core::SC_NS);
        unsigned char full[]={255,255,255,255};
        if(scenario=="gpio" || scenario=="strobes")for(unsigned i=0;i<3;++i){
            access(true,bases[i]+8,0x12345678+i,full);
            expect(access(false,bases[i]+8,0,full)==0x12345678+i,"full strobes work on actual pulled GPIO through both bridges");
        }
        if(scenario=="qspi"){
            access(true,0x44000004,16u<<16);access(true,0x44000000,1,full);access(true,0x44000014,0x0d002503);
            unsigned char bytes[16]{};tlm::tlm_generic_payload t;payload(t,tlm::TLM_READ_COMMAND,0x30000000,bytes,16);
            fu3_bus::ScopedAccess fetch(t,4,fu3_bus::Burst::Increment,true);auto delay=sc_core::SC_ZERO_TIME;cpu.socket->b_transport(t,delay);
            expect(t.is_response_ok(),"current QSPI serves a four-beat CPU XIP fetch through SYSBUS_0");
            const unsigned char expected[]={0x13,0x05,0x10,0,0x93,0x05,0x20,0,0x33,0x06,0xb5,0,0x73,0,0x10,0};
            for(unsigned i=0;i<16;++i)expect(bytes[i]==expected[i],"XIP bytes match an independent image oracle");
        }
        if(scenario=="strobes"){
            unsigned char sparse[]={255,0,255,0},none[]={0,0,0,0};
            for(unsigned i=0;i<3;++i){
                access(true,bases[i]+8,0xffffffff,sparse,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
                access(true,bases[i]+8,0,none,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
                expect(access(false,bases[i]+8)==0x12345678+i,"GPIO rejects unsupported masks without corrupting data");
            }
            access(true,0x44000000,1,full);
            access(true,0x44000000,0,sparse,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            access(true,0x44000000,0,none,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            expect(access(false,0x44000000)==1,"QSPI rejects unsupported masks without disabling its controller");
            // Characterization only: these explicit rejections are owner gaps
            // against Common Full strobes, not successful partial-write conformance.
            std::cout<<"OWNER_GAP: GPIO and AP_QSPIC reject partial/zero masks required by Common Full strobes\n";
        }
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(1,sc_core::SC_MS);expect(false,"pulled peer watchdog");sc_core::sc_stop();}
};
int sc_main(int argc,char** argv){PulledPeers p{"test",argc>1?argv[1]:"qspi"};sc_core::sc_start();return !p.finished||p.failures?1:0;}
