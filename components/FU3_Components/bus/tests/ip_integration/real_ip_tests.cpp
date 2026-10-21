#include <fu3_bus/bus_system.h>
#include <fu3/dma/ap_dma.h>
#include <fu3_bus/dma_adapter.h>
#include <fu3_spi/spi.h>
#include <fu3_uart/uart.h>
#include <fu3_timer/timer.h>
#include <fu3_wdt/wdt.h>
#include <fu3_plic/plic.h>
#include "support/probe_target.h"
#include "support/test_helpers.h"
#include <string>

struct SpiPins : sc_core::sc_module {
    sc_core::sc_signal<bool> clock,select,mosi_in,unused,miso,sck,mosi;
    sc_core::sc_signal<bool> so,sso,mo,mio,irq,tx_req,rx_req,tx_ack,rx_ack;
    sc_core::sc_signal<sc_dt::sc_uint<4>> ss;
    explicit SpiPins(sc_core::sc_module_name n):sc_module(n){select=true;}
    void bind(fu3_spi::Spi& d,sc_core::sc_signal<bool>& reset){
        d.reset_n(reset);d.apb_reset_n(reset);d.sck_in(clock);d.ss_n_in(select);d.mosi_in(mosi_in);d.miso_in(mosi);
        d.sck(sck);d.ss_n(ss);d.mosi(mosi);d.miso(miso);d.sck_oe(so);d.ss_oe(sso);d.mosi_oe(mo);d.miso_oe(mio);
        d.irq(irq);d.dma_tx_req(tx_req);d.dma_rx_req(rx_req);d.dma_tx_ack(tx_ack);d.dma_rx_ack(rx_ack);
    }
};
struct UartPins : sc_core::sc_module {
    sc_core::sc_signal<bool> rx,tx,cts,dsr,ri,dcd,rts,dtr,out1,out2,irq,tx_req,rx_req,tx_clear,rx_clear;
    explicit UartPins(sc_core::sc_module_name n):sc_module(n){rx=true;cts=true;dsr=true;ri=true;dcd=true;}
    void bind(fu3_uart::Uart& d,sc_core::sc_signal<bool>& reset){
        d.reset_n(reset);d.rxd(rx);d.txd(tx);d.cts_n(cts);d.dsr_n(dsr);d.ri_n(ri);d.dcd_n(dcd);
        d.rts_n(rts);d.dtr_n(dtr);d.out1_n(out1);d.out2_n(out2);d.irq(irq);
        d.dma_tx_request(tx_req);d.dma_rx_request(rx_req);d.dma_tx_clear(tx_clear);d.dma_rx_clear(rx_clear);
    }
};
struct RealIp : sc_core::sc_module, Checks {
    static fu3_bus::BusConfig config(){
        auto c=fu3_bus::BusConfig::fu3();
        c.initiators.push_back({"DMA_TEST",4,true,true,16});
        for(auto& t:c.targets)t.enabled=t.name=="IBEX_ISRAM"||t.name=="DSRAM"||t.name=="CVA6_ISRAM"||
            t.name=="AP_DMA"||t.name=="AP_PLIC"||t.name=="SPI0"||t.name=="UART2"||t.name=="TIMER1"||t.name=="WDT0";
        return c;
    }
    fu3_bus::BusSystem bus{"bus",config()};
    fu3::dma::ApDma dma{"dma",sc_core::sc_time(40,sc_core::SC_NS)};
    fu3_bus::DmaAdapter adapter{"adapter"},test_adapter{"test_adapter"};
    fu3_spi::Spi spi{"spi"};fu3_uart::Uart uart{"uart"};fu3_timer::Timer timer{"timer"};
    fu3_wdt::Watchdog wdt{"wdt"};fu3_plic::Plic plic{"plic"};
    fu3_bus::test::ProbeTarget isram{"isram",65536},dsram{"dsram",65536},program{"program",65536};
    Master cpu{"cpu"},background{"background"},native{"native"},raw{"raw"};
    sc_core::sc_signal<bool> reset_n,unlock,dma_irq,plic_irq,wdt_irq,wdt_request,gate,timer_irq,timer_overflow,timer_compare;
    sc_core::sc_signal<sc_dt::sc_uint<32>> rx_requests,tx_requests,rx_clears,tx_clears;
    sc_core::sc_vector<sc_core::sc_signal<bool>> sources{"sources",fu3_plic::kSources};
    SpiPins sp{"spi_pins"};UartPins up{"uart_pins"};
    sc_core::sc_event background_start,background_done;
    std::string scenario;bool finished=false,background_finished=false;
    unsigned spi_tx_acks=0,spi_rx_acks=0,uart_tx_acks=0,uart_rx_acks=0;
    unsigned previous_tx=0,previous_rx=0;
    SC_HAS_PROCESS(RealIp);
    explicit RealIp(sc_core::sc_module_name n,std::string which):sc_module(n),scenario(std::move(which)){
        cpu.socket.bind(bus.initiator("IBEX"));background.socket.bind(bus.initiator("CVA6"));
        raw.socket.bind(bus.initiator("DBG"));native.socket.bind(test_adapter.input);test_adapter.output.bind(bus.initiator("DMA_TEST"));
        dma.master_socket.bind(adapter.input);adapter.output.bind(bus.initiator("DMA0"));
        bus.target("AP_DMA").bind(dma.target_socket);bus.target("AP_PLIC").bind(plic.socket);
        bus.target("SPI0").bind(spi.socket);bus.target("UART2").bind(uart.socket);bus.target("TIMER1").bind(timer.socket);bus.target("WDT0").bind(wdt.socket);
        bus.target("IBEX_ISRAM").bind(isram.socket);bus.target("DSRAM").bind(dsram.socket);bus.target("CVA6_ISRAM").bind(program.socket);
        dma.reset_n(reset_n);dma.irq(dma_irq);dma.rx_request(rx_requests);dma.tx_request(tx_requests);dma.rx_clear(rx_clears);dma.tx_clear(tx_clears);
        sp.bind(spi,reset_n);up.bind(uart,reset_n);
        timer.reset_n(reset_n);timer.clock_enabled(gate);timer.irq(timer_irq);timer.overflow_irq(timer_overflow);timer.compare_irq(timer_compare);
        wdt.preset_n(reset_n);wdt.wdogres_n(reset_n);wdt.i_wdt_other_unlock(unlock);wdt.irq(wdt_irq);wdt.reset_request(wdt_request);
        plic.reset_n(reset_n);plic.irq(plic_irq);for(unsigned i=0;i<sources.size();++i)plic.irq_sources[i](sources[i]);
        SC_METHOD(route);sensitive<<sp.tx_req<<sp.rx_req<<sp.irq<<up.tx_req<<up.rx_req<<up.irq<<rx_clears<<tx_clears<<timer_irq<<dma_irq;
        SC_METHOD(count_acks);sensitive<<rx_clears<<tx_clears;
        SC_THREAD(run);SC_THREAD(background_work);SC_THREAD(watchdog);
    }
    void route(){
        // Fixture request selectors: native ID zero denotes memory, so external
        // request bits 1..4 are used here. These are not new FU3 channel assignments.
        tx_requests=(sp.tx_req.read()?2u:0u)|(up.tx_req.read()?8u:0u);
        rx_requests=(sp.rx_req.read()?4u:0u)|(up.rx_req.read()?16u:0u);
        sp.tx_ack=(tx_clears.read().to_uint()&2)!=0;sp.rx_ack=(rx_clears.read().to_uint()&4)!=0;
        up.tx_clear=(tx_clears.read().to_uint()&8)!=0;up.rx_clear=(rx_clears.read().to_uint()&16)!=0;
        for(unsigned i=0;i<sources.size();++i){bool level=false;
            if(i==4)level=dma_irq.read();if(i==5)level=timer_irq.read();if(i==8)level=sp.irq.read();if(i==10)level=up.irq.read();sources[i]=level;}
    }
    void count_acks(){
        const auto tx=tx_clears.read().to_uint(),rx=rx_clears.read().to_uint();
        if((tx&2)&&!(previous_tx&2))++spi_tx_acks;if((rx&4)&&!(previous_rx&4))++spi_rx_acks;
        if((tx&8)&&!(previous_tx&8))++uart_tx_acks;if((rx&16)&&!(previous_rx&16))++uart_rx_acks;
        previous_tx=tx;previous_rx=rx;
    }
    void settle(){for(unsigned i=0;i<6;++i)wait(sc_core::SC_ZERO_TIME);}
    unsigned access(bool write,unsigned address,unsigned value=0,unsigned size=4,
                    tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE,unsigned char* enables=nullptr){
        unsigned char b[4]{};for(unsigned i=0;i<4;++i)b[i]=value>>(8*i);
        tlm::tlm_generic_payload t;payload(t,write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND,address,b,size);
        t.set_byte_enable_ptr(enables);t.set_byte_enable_length(enables?4:0);
        auto delay=sc_core::sc_time(2,sc_core::SC_NS);fu3_bus::ScopedAccess a(t,size);cpu.socket->b_transport(t,delay);settle();
        expect(t.get_response_status()==expected,"real-IP MMIO response");expect(t.get_address()==address&&delay==sc_core::SC_ZERO_TIME&&!t.is_dmi_allowed(),"real-IP address/delay/DMI contract");
        value=0;for(unsigned i=0;i<size;++i)value|=unsigned(b[i])<<(8*i);return value;
    }
    void configure(unsigned ch,unsigned src,unsigned dst,unsigned length,unsigned read_cfg,unsigned write_cfg,unsigned peripheral=0){
        const unsigned b=0x44020000+ch*256;
        access(true,b+0x40,0);access(true,b+0xa4,0x1fff);access(true,b,src);access(true,b+4,dst);access(true,b+8,length);
        access(true,b+12,3);access(true,b+16,read_cfg);access(true,b+20,write_cfg);access(true,b+0x20,peripheral);access(true,b+0x40,1);
    }
    void start(unsigned ch){access(true,0x44020044+ch*256,1);}
    void completed(unsigned ch){
        for(unsigned i=0;i<4000;++i){if(!access(false,0x44020048+ch*256)){
                expect(access(false,0x440200a0+ch*256)==1,"real DMA completes without error flags");return;}
            wait(10,sc_core::SC_NS);}
        expect(false,"real DMA bounded completion");
    }
    void dma_case(){
        for(unsigned i=0;i<80;++i)access(true,0x00020080+i,(i*29+7)&255,1);
        // Unaligned byte tails cross target word lanes without CSR RMW.
        configure(6,0x00020081,0x01000001,63,0x8101001f,0x81010017);start(6);completed(6);
        for(unsigned i=0;i<63;++i)expect(access(false,0x01000001+i,0,1)==((i+1)*29+7)%256,"real DMA M2M independent byte oracle");
        for(unsigned i=0;i<8;++i)access(true,0x00020100+i*4,0x40+i);
        // SPI0 uses DMA channel 0 RX / 1 TX; UART2 uses channel 4 RX / 5 TX.
        access(true,0x440c0000,0xe2);access(true,0x440c0004,3);access(true,0x440c0024,0x40403);access(true,0x440c0000,0xe3);
        access(true,0x50050003,0x83,1);access(true,0x50050000,1,1);access(true,0x50050001,0,1);
        access(true,0x50050003,3,1);access(true,0x50050002,9,1);access(true,0x50050004,0x10,1);
        configure(0,0x440c0010,0x01000100,32,0x01010010,0x81010010,2);
        configure(1,0x00020100,0x440c000c,32,0x81010010,0x01010010,1u<<16);
        configure(4,0x50050000,0x01000201,8,0x01010001,0x81010008,4);
        configure(5,0x00020081,0x50050000,8,0x81010008,0x01010001,3u<<16);
        for(unsigned ch:{0u,1u,4u,5u})start(ch);
        for(unsigned ch:{0u,1u,4u,5u})completed(ch);
        for(unsigned i=0;i<8;++i){expect(access(false,0x01000100+i*4)==0x40+i,"real SPI/DMA loopback word order");
            expect(access(false,0x01000201+i,0,1)==((i+1)*29+7)%256,"real UART2/DMA loopback byte order");}
        expect(spi_tx_acks==2&&spi_rx_acks==2,"two four-word SPI request credits serviced per direction");
        expect(uart_tx_acks==8&&uart_rx_acks==8,"UART byte service credits acknowledged exactly once");
        expect(!(access(false,0x440c0020)&(FU3_SPI_OVR|FU3_SPI_RXUDF|FU3_SPI_TXOVF)),"real DMA obeys SPI FIFO bounds");
        expect(!(access(false,0x50050005,0,1)&0x1e),"real DMA UART path has no receive errors");
    }
    void plic_case(){
        access(true,0x4010000c,1u<<20);access(true,0x4010001c,1u<<5);
        access(true,0x44070008,7);access(true,0x44070000,7);wait(30,sc_core::SC_NS);settle();
        expect(!timer_irq.read()&&plic_irq.read(),"real PLIC captures a real one-tick Timer1 pulse");
        for(unsigned size:{1u,2u}){
            access(false,0x40100024+(size==1?1:0),0,size,tlm::TLM_BURST_ERROR_RESPONSE);
            access(true,0x40100024,6,size,tlm::TLM_BURST_ERROR_RESPONSE);
            expect(plic_irq.read(),"rejected narrow claim/complete preserves pending source");
        }
        unsigned char sparse[4]={0xff,0,0,0},none[4]{};
        access(false,0x40100024,0,4,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,sparse);
        access(false,0x40100024,0,4,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,none);
        expect(plic_irq.read(),"disabled or partial read lanes cannot consume the claim");
        expect(access(false,0x40100024)==6,"full word claim returns the Timer1 ID");
        expect(access(false,0x40100024)==0,"source is claimed exactly once");access(true,0x40100024,6);
        expect(!plic_irq.read(),"complete after the timer pulse leaves no pending IRQ");
        expect(access(false,0x40100000,0,1)==32,"unrelated PLIC narrow configuration read remains valid");
    }
    void adapter_case(){
        unsigned char bytes[8]={0x11,0x22,0x33,0x44};tlm::tlm_generic_payload t;
        payload(t,tlm::TLM_WRITE_COMMAND,0x00020001,bytes,4);
        fu3::dma::TransferExtension meta;meta.unit_bytes=1;meta.beats=4;meta.axi_id=4;
        meta.write_strobes={2,4,8,1};t.set_extension(&meta);
        unsigned char enables[2]={0xff,0};t.set_byte_enable_ptr(enables);t.set_byte_enable_length(2);
        auto send=[&](tlm::tlm_response_status expected){auto d=sc_core::sc_time(5,sc_core::SC_NS);native.socket->b_transport(t,d);
            expect(t.get_response_status()==expected&&d==sc_core::SC_ZERO_TIME,"DMA adapter response/delay");
            expect(t.get_extension<fu3::dma::TransferExtension>()==&meta&&!t.get_extension<fu3_bus::Access>()&&
                   t.get_byte_enable_ptr()==enables&&t.get_byte_enable_length()==2&&t.get_address()==0x00020001,"DMA adapter restores caller fields and native metadata");};
        send(tlm::TLM_OK_RESPONSE);
        for(unsigned i=0;i<4;++i)expect(access(false,0x00020001+i,0,1)==(i%2==0?bytes[i]:0),"native WSTRB and original byte enables are both honored");
        meta.increment=false;meta.write_strobes={2,2,2,2};send(tlm::TLM_OK_RESPONSE);
        expect(access(false,0x00020001,0,1)==bytes[2]&&access(false,0x00020002,0,1)==0,
               "native FIXED progression repeats the byte lane without incrementing");
        meta.increment=true;meta.write_strobes={2,4,8,1};
        isram.on_visit=[&](const auto&){
            auto* retained=t.get_extension<fu3::dma::TransferExtension>();
            expect(retained==&meta,"target sees the original native DMA extension");
            retained->response_id_valid=true;retained->response_id=9;retained->faults|=fu3::dma::ReadLastError;
        };
        send(tlm::TLM_OK_RESPONSE);isram.on_visit={};
        expect(meta.response_id_valid&&meta.response_id==9&&(meta.faults&fu3::dma::ReadLastError),
               "adapter preserves downstream response-ID and fault observations");
        const auto before_error=isram.visits.size();isram.forced_status=tlm::TLM_ADDRESS_ERROR_RESPONSE;
        send(tlm::TLM_ADDRESS_ERROR_RESPONSE);isram.forced_status=tlm::TLM_OK_RESPONSE;
        expect(isram.visits.size()==before_error+1,"downstream error stops remaining adapter beats");
        meta.unit_bytes=3;send(tlm::TLM_BURST_ERROR_RESPONSE);meta.unit_bytes=1;
        meta.beats=17;send(tlm::TLM_BURST_ERROR_RESPONSE);meta.beats=4;
        meta.write_strobes[0]=4;send(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);meta.write_strobes[0]=2;
        const auto before_missing=isram.visits.size();t.clear_extension<fu3::dma::TransferExtension>();
        auto missing_delay=sc_core::SC_ZERO_TIME;native.socket->b_transport(t,missing_delay);
        expect(t.get_response_status()==tlm::TLM_GENERIC_ERROR_RESPONSE&&isram.visits.size()==before_missing,
               "missing native metadata is rejected before reaching the fabric target");t.set_extension(&meta);
        {fu3_bus::ScopedAccess conflicting(t,4);auto d=sc_core::SC_ZERO_TIME;native.socket->b_transport(t,d);
            expect(t.get_response_status()==tlm::TLM_COMMAND_ERROR_RESPONSE&&t.get_extension<fu3_bus::Access>()->beat_bytes==4,"conflicting caller metadata is rejected and preserved");}
        isram.throw_once=true;bool threw=false;auto d=sc_core::SC_ZERO_TIME;
        try{native.socket->b_transport(t,d);}catch(const std::runtime_error&){threw=true;}
        expect(threw&&t.get_address()==0x00020001&&t.get_byte_enable_ptr()==enables&&!t.get_extension<fu3_bus::Access>(),"adapter restores payload after a target exception");
        send(tlm::TLM_OK_RESPONSE);
        t.set_byte_enable_ptr(nullptr);t.set_byte_enable_length(0);t.set_command(tlm::TLM_READ_COMMAND);
        t.set_address(0x00020000);auto raw_delay=sc_core::SC_ZERO_TIME;raw.socket->b_transport(t,raw_delay);
        expect(t.get_response_status()==tlm::TLM_GENERIC_ERROR_RESPONSE,"default raw ingress still rejects missing Access metadata");
        t.clear_extension<fu3::dma::TransferExtension>();
    }
    void reset_case(){
        access(true,0x00010000,0x1234);bus.finish_staging();
        access(true,0x00020000,0x93c57a21);
        configure(6,0x00020000,0x01000000,32,0x81010010,0x81010010);
        isram.latency_ns=1000;
        access(true,0x44051004,40);access(true,0x4405100c,3);start(6);
        background_start.notify(sc_core::SC_ZERO_TIME);
        if(!wdt_request.read())wait(wdt_request.posedge_event());
        expect(wdt_irq.read()&&!background_finished,"real watchdog requests reset during active bus traffic");
        bus.quiesce();expect(!bus.accepting()&&background_finished,"CRM stop/drain completes old traffic before resetting IPs");
        reset_n=false;wait(50,sc_core::SC_NS);bus.reset();settle();
        expect(!wdt_request.read()&&!wdt_irq.read()&&!dma_irq.read(),"IP reset clears watchdog and DMA outputs");
        reset_n=true;wait(30,sc_core::SC_NS);bus.resume();isram.latency_ns=7;
        access(true,0x00010000,0x5678);expect(access(false,0x00010000)==0x5678,"reset reopens the boot staging policy");
        configure(0,0x00020000,0x01000020,4,0x81010004,0x81010004);start(0);completed(0);
        expect(access(false,0x01000020)==0x93c57a21,"real DMA restarts cleanly after coordinated reset");
        expect(access(false,0x4405100c)==FU3_WDT_RESEN,"watchdog reset profile is restored");
    }
    void background_work(){wait(background_start);unsigned char data[4]{};tlm::tlm_generic_payload t;
        payload(t,tlm::TLM_READ_COMMAND,0x00020000,data,4);fu3_bus::ScopedAccess a(t,4);auto delay=sc_core::SC_ZERO_TIME;
        background.socket->b_transport(t,delay);expect(t.is_response_ok(),"admitted CPU read drains without replay");background_finished=true;background_done.notify(sc_core::SC_ZERO_TIME);}
    void run(){
        reset_n=false;unlock=true;gate=true;wait(30,sc_core::SC_NS);reset_n=true;wait(30,sc_core::SC_NS);
        if(scenario=="dma")dma_case();else if(scenario=="plic")plic_case();else if(scenario=="adapter")adapter_case();else if(scenario=="reset")reset_case();else expect(false,"unknown real-IP scenario");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(2,sc_core::SC_MS);expect(false,"real-IP integration watchdog");sc_core::sc_stop();}
};
int sc_main(int argc,char** argv){RealIp t{"test",argc>1?argv[1]:"dma"};sc_core::sc_start();std::cout<<"Real IP "<<t.scenario<<": "<<t.checks<<" checks, "<<t.failures<<" failures\n";return !t.finished||t.failures?1:0;}
