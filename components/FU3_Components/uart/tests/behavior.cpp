#include "support/fixture.h"
#include <stdexcept>

struct Bench : sc_core::sc_module, Checks {
    fu3_uart::Uart dut;
    Pins pins{"pins"};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    unsigned depth, os;
    SC_HAS_PROCESS(Bench);
    Bench(sc_core::sc_module_name name, unsigned d, unsigned sampling)
        :sc_module(name),dut("dut",{sc_core::sc_time(10,sc_core::SC_NS),d,sampling,true}),depth(d),os(sampling) {
        pins.bind(dut); cpu.bind(dut.socket); SC_THREAD(run); SC_THREAD(watchdog);
    }
    void settle(){for(unsigned i=0;i<5;++i)wait(sc_core::SC_ZERO_TIME);}
    void cold() {
        pins.reset=false; pins.rx=true; pins.cts=false; pins.dsr=true; pins.ri=true; pins.dcd=true;
        pins.tx_clear=false; pins.rx_clear=false;
        wait(30,sc_core::SC_NS); pins.reset=true; wait(30,sc_core::SC_NS);settle();
        r(6); // Clear initial CTS delta before independent interrupt checks.
    }
    unsigned access(bool wr,unsigned address,unsigned value=0,bool debug=false,unsigned length=1,
                    tlm::tlm_response_status status=tlm::TLM_OK_RESPONSE,unsigned char* enables=nullptr) {
        unsigned char bytes[4]={static_cast<unsigned char>(value),0x55,0x66,0x77};
        tlm::tlm_generic_payload tx;tx.set_command(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(address);tx.set_data_ptr(bytes);tx.set_data_length(length);tx.set_streaming_width(length);
        tx.set_byte_enable_ptr(enables);tx.set_byte_enable_length(enables?1:0);
        auto delay=sc_core::sc_time(3,sc_core::SC_NS);const auto before=sc_core::sc_time_stamp();
        if(debug){cpu->transport_dbg(tx);expect(before==sc_core::sc_time_stamp(),"debug consumes no time");}
        else {cpu->b_transport(tx,delay);expect(delay==sc_core::SC_ZERO_TIME&&sc_core::sc_time_stamp()>before,"MMIO yields and consumes annotation");}
        expect(tx.get_response_status()==status,"transaction response");expect(!tx.is_dmi_allowed(),"no CSR DMI");settle();
        if(!wr&&length==4&&tx.is_response_ok())expect(bytes[1]==0&&bytes[2]==0&&bytes[3]==0,"word read zero extends one register");
        return bytes[0];
    }
    void w(unsigned a,unsigned v){access(true,a,v);} unsigned r(unsigned a){return access(false,a);}
    unsigned peek(unsigned a){return access(false,a,0,true);}
    void configure(unsigned format=3,unsigned div=1,unsigned fifo=0x21) {
        w(3,128|format);w(0,div&255);w(1,div>>8);w(2,fifo);w(3,format);
    }
    sc_core::sc_time bit(unsigned div=1)const{return sc_core::sc_time(10.0*os*div,sc_core::SC_NS);}
    // Independent stimulus: derive UART framing directly from LCR bit definitions.
    void send(unsigned data,unsigned format=3,bool bad_parity=false,bool bad_stop=false,unsigned div=1) {
        const auto duration=bit(div);pins.rx=true;wait(duration);
        pins.rx=false;wait(duration);
        const unsigned bits=5+(format&3);unsigned ones=0;
        for(unsigned i=0;i<bits;++i){const bool level=(data>>i)&1;ones+=level;pins.rx=level;wait(duration);}
        if(format&8){bool parity=(format&32)?!(format&16):((ones+((format&16)?0:1))&1);
            pins.rx=parity^bad_parity;wait(duration);}
        pins.rx=!bad_stop;wait(duration*((format&4)?(bits==5?1.5:2.0):1.0));
        pins.rx=true;wait(duration);settle();
    }
    void check_transmit(unsigned data,unsigned format,unsigned div=1) {
        const auto duration=bit(div);w(0,data);
        expect(!pins.tx,"THR starts serial start bit");
        expect((peek(5)&0x60)==0x20,"THRE may be set while TSR remains active");
        wait(duration*0.5);expect(!pins.tx,"start bit midpoint");
        unsigned ones=0;
        for(unsigned i=0;i<5+(format&3);++i){wait(duration);const bool expected=(data>>i)&1;
            ones+=expected;expect(pins.tx.read()==expected,"TX data bit is LSB first");}
        if(format&8){wait(duration);bool expected=(format&32)?!(format&16):((ones+((format&16)?0:1))&1);
            expect(pins.tx.read()==expected,"TX parity bit");}
        wait(duration);expect(pins.tx,"stop bit midpoint");
        const double stop=(format&4)?((format&3)?2.0:1.5):1.0;
        wait(duration*(stop-0.5)-sc_core::sc_time(1,sc_core::SC_NS));
        expect(!(peek(5)&0x40),"TEMT is not set before final stop completes");
        wait(20,sc_core::SC_NS);settle();expect((peek(5)&0x60)==0x60,"TEMT after full stop duration");
    }
    void run() {
        cold();expect(r(3)==0&&r(4)==0&&r(7)==0&&r(1)==0&&r(2)==1&&r(5)==0x60,"documented VP reset profile");
        expect(pins.tx&&pins.rts&&pins.dtr&&pins.out1&&pins.out2&&!pins.irq&&!pins.tx_req,"reset outputs");
        w(7,0xa5);expect(r(7)==0xa5,"scratch round trip");
        w(3,128);w(0,0x34);w(1,0x12);expect(r(0)==0x34&&r(1)==0x12,"DLAB selects both divisor bytes");
        w(3,3);expect(r(1)==0,"DLM write does not alter IER");
        w(4,255);expect(r(4)==31,"reserved MCR bits are zero");w(4,0);
        access(false,7,0,false,4);access(true,7,0,false,2,tlm::TLM_BURST_ERROR_RESPONSE);
        access(false,8,0,false,1,tlm::TLM_ADDRESS_ERROR_RESPONSE);
        unsigned char disabled=0;access(true,7,0,false,1,tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,&disabled);
        expect(r(7)==0xa5,"rejected access has no side effect");
        access(true,7,0,true,1,tlm::TLM_COMMAND_ERROR_RESPONSE);

        // All 5-8 bit lengths, stop choices, and none/odd/even/mark/space parity.
        for(unsigned bits=0;bits<4;++bits)for(unsigned stops=0;stops<2;++stops)
            for(unsigned parity:{0u,8u,24u,40u,56u}) {
                cold();const unsigned format=bits|(stops<<2)|parity;configure(format);
                check_transmit(0xa6,format);send(0x59,format);
                expect((r(5)&0x1f)==1,"RX clean frame status");expect(r(0)==(0x59&((1u<<(bits+5))-1)),"RX data mask and framing");
            }
        cold();configure(3,2);check_transmit(0x81,3,2);send(0x42,3,false,false,2);expect(r(0)==0x42,"integer divisor two");

        // Error bits follow their FIFO entry; LSR clears the interrupt, not the data.
        cold();configure(27);w(1,5);send(0x35,27,true);expect((r(2)&15)==6,"line error has highest priority");
        expect((peek(5)&0x85)==0x85,"parity error and FIFO error summary");
        expect((r(5)&4)&&!(r(5)&4),"LSR read acknowledges head error once");
        expect((r(2)&15)==4&&r(0)==0x35,"RDA remains after error acknowledge");
        send(0x51,27,false,true);expect(r(5)&8,"bad stop produces framing error");r(0);
        pins.rx=false;wait(bit()*14);settle();expect((r(5)&0x19)==0x19,"continuous low produces break and framing error");
        r(0);wait(bit()*12);expect(!(r(5)&1),"held break produces one character until line recovers");pins.rx=true;wait(bit());
        // False starts and a single noisy midpoint sample must not corrupt data.
        pins.rx=false;wait(bit()*0.2);pins.rx=true;wait(bit()*12);expect(!(r(5)&1),"false start discarded");
        configure(3);pins.rx=false;wait(bit());
        for(unsigned i=0;i<8;++i){pins.rx=true;wait(bit()*0.5);pins.rx=false;wait(10,sc_core::SC_NS);pins.rx=true;wait(bit()*0.5-sc_core::sc_time(10,sc_core::SC_NS));}
        pins.rx=true;wait(bit()*2);expect(r(0)==255&&!(r(5)&0x1e),"majority voting rejects one-sample data glitch");

        // FIFO capacity, trigger tables, and DLAB protection of FCR[5].
        for(unsigned large=0;large<2;++large)for(unsigned level=0;level<4;++level) {
            cold();configure(3,1,1|(large?32:0)|(level<<6));w(1,1);
            const unsigned thresholds16[]{1,4,8,14},thresholds64[]{1,16,32,56};
            const unsigned threshold=large?thresholds64[level]:thresholds16[level];
            const unsigned cap=large?depth:16;
            for(unsigned i=0;i<std::min(threshold,cap);++i){send(i);expect(((peek(2)&15)==4)==(i+1>=threshold),"RX trigger boundary");}
            if(threshold<=cap){expect((r(2)&15)==4,"threshold interrupt");r(0);expect((r(2)&15)!=4,"RBR drops below trigger");}
            else expect((r(2)&15)!=4,"unreachable trigger on depth 32 is not silently clamped");
        }
        cold();configure(3,1,0x21);w(2,1);expect((r(2)&0xe0)==0xe0,"extended FIFO selection retained when DLAB is zero");
        for(unsigned i=0;i<depth;++i)send(i);send(0xee);expect(r(5)&2,"full RX sets OE");
        for(unsigned i=0;i<depth;++i)expect(r(0)==i,"overflow preserves FIFO order and discards incoming word");
        expect(!(r(5)&1),"RX drained");
        cold();configure(3,1,0);send(0x11);send(0x22);expect(r(5)&2,"single-byte holding register overflow");expect(r(0)==0x11,"non-FIFO VP overflow policy");
        // RX flush clears pending data/errors; TX flush preserves current shift frame.
        cold();configure();send(3);w(2,3);expect(!(r(5)&0x9f),"RX reset clears data and errors");
        pins.cts=true;wait(20,sc_core::SC_NS);w(0,0x11);w(0,0x22);expect(!(r(5)&0x20),"CTS blocks launch without dropping data");
        w(2,5);expect((r(5)&0x60)==0x60,"TX FIFO reset while CTS blocks");
        pins.cts=false;wait(20,sc_core::SC_NS);w(0,0x55);w(0,0xaa);w(2,5);
        expect(!(r(5)&0x40),"TX flush does not truncate current frame");wait(bit()*12);expect(r(5)&0x40,"active frame completes after flush");

        // THRE latches, receive timeout, and priority/clear semantics.
        cold();configure();w(1,2);expect(pins.irq&&(peek(2)&15)==2,"enabling THRE on empty transmitter interrupts");
        expect((r(2)&15)==2&&(r(2)&15)==1&&!pins.irq,"IIR acknowledges THRE without immediate reassertion");
        w(0,0x73);expect((r(2)&15)==2,"new empty transition rearms THRE");w(1,0);wait(bit()*12);
        configure(3,1,0xc1);w(1,1);send(0x47);expect((peek(2)&15)==1,"below threshold is not RDA");
        wait(bit()*20);expect((peek(2)&15)==1,"no premature timeout");
        wait(bit()*25);settle();expect((peek(2)&15)==12&&pins.irq,"four-character inactivity timeout");
        expect(peek(0)==0x47&&(peek(2)&15)==12,"debug inspection never pops or clears timeout");
        expect(r(0)==0x47&&(r(2)&15)==1,"RBR clears timeout");
        // Modem transitions, RI trailing edge and loopback wiring.
        cold();configure();w(1,8);pins.dsr=false;pins.dcd=false;pins.ri=false;wait(20,sc_core::SC_NS);
        expect((peek(6)&0xee)==0xea&&pins.irq,"modem inputs and delta bits");r(6);expect(!pins.irq,"MSR read clears modem IRQ");
        pins.ri=true;wait(20,sc_core::SC_NS);expect(r(6)&4,"RI low-to-high generates TERI");
        w(4,15);expect(!pins.dtr&&!pins.out1&&!pins.out2&&!pins.rts,"active-low modem outputs");
        w(4,31);expect((r(6)&0xf0)==0xf0&&pins.tx&&pins.rts,"loopback modem mapping and released external pins");
        w(0,0x69);wait(bit()*12);expect(r(0)==0x69,"internal serial loopback");
        w(4,0);w(3,67);expect(!pins.tx,"break forces TX low");w(3,3);expect(pins.tx,"break release returns idle high");
        // Auto RTS at threshold, resuming only after FIFO drain.
        cold();configure(3,1,0x41);w(4,2);expect(!pins.rts,"auto RTS initially ready");
        for(unsigned i=0;i<4;++i)send(i);expect(pins.rts,"auto RTS stops sender at trigger");
        r(0);expect(pins.rts,"auto RTS holds until FIFO empty");for(unsigned i=0;i<3;++i)r(0);wait(20,sc_core::SC_NS);expect(!pins.rts,"auto RTS resumes after drain");
        // DMA clear pulses suppress requests; conditions determine reassertion.
        cold();configure(3,1,0x49);expect(pins.tx_req&&!pins.rx_req,"DMA TX space and RX threshold");
        pins.tx_clear=true;wait(20,sc_core::SC_NS);expect(!pins.tx_req,"TX clear suppresses request");
        pins.tx_clear=false;wait(20,sc_core::SC_NS);expect(pins.tx_req,"TX DMA rearms if capacity remains");
        send(0x81);wait(bit()*45);expect(pins.rx_req,"RX DMA timeout below trigger");
        pins.rx_clear=true;wait(20,sc_core::SC_NS);expect(!pins.rx_req,"RX clear suppresses request");
        r(0);pins.rx_clear=false;wait(20,sc_core::SC_NS);expect(!pins.rx_req,"drained FIFO does not re-request");
        w(2,1);expect(!pins.tx_req&&!pins.rx_req,"DMA disable");
        // Reset aborts active TX/RX and restores defaults on a clock edge.
        w(0,0x00);pins.rx=false;wait(bit()*2);pins.reset=false;wait(20,sc_core::SC_NS);settle();
        expect(pins.tx&&!pins.irq&&!pins.tx_req&&(peek(5)&0x61)==0x60,"synchronous reset aborts active work");
        access(false,3,0,false,1,tlm::TLM_GENERIC_ERROR_RESPONSE);
        pins.reset=true;pins.rx=true;wait(20,sc_core::SC_NS);expect(r(3)==0&&r(2)==1,"post-reset register defaults");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(20,sc_core::SC_MS);expect(false,"behavior simulation watchdog");sc_core::sc_stop();}
};
int sc_main(int argc,char** argv) {
    const unsigned depth=argc>1?unsigned(std::stoul(argv[1])):64;
    const unsigned os=argc>2?unsigned(std::stoul(argv[2])):16;
    Bench bench{"bench",depth,os};sc_core::sc_start();
    std::cout<<"UART behavior depth="<<depth<<" oversampling="<<os<<": "<<bench.checks<<" checks, "<<bench.failures<<" failures\n";
    return bench.failures||!bench.finished?1:0;
}
