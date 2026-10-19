#include "support/fixture.h"
#include <vector>
struct Bench:sc_core::sc_module,Checks {
    fu3_spi::Spi dut;
    Pins p{"pins"};
    tlm_utils::simple_initiator_socket<Bench> cpu{"cpu"};
    unsigned depth;
    sc_core::sc_event launch_reset_access,reset_access_done;
    unsigned reset_accesses=0;
    SC_HAS_PROCESS(Bench);
    Bench(sc_core::sc_module_name n,unsigned d):sc_module(n),dut("dut",d),depth(d){p.bind(dut);cpu.bind(dut.socket);SC_THREAD(run);SC_THREAD(reset_access);SC_THREAD(watchdog);}
    void reset_access(){while(true){wait(launch_reset_access);access(true,12,0x75,nullptr,4,false,tlm::TLM_GENERIC_ERROR_RESPONSE);++reset_accesses;reset_access_done.notify(sc_core::SC_ZERO_TIME);}}
    void settle(){for(unsigned i=0;i<5;++i)wait(sc_core::SC_ZERO_TIME);}
    void cold(){p.reset_n=false;p.apb_reset_n=false;p.select=true;p.si=false;p.ta=false;p.ra=false;wait(25,sc_core::SC_NS);p.reset_n=true;p.apb_reset_n=true;wait(25,sc_core::SC_NS);}
    unsigned access(bool wr,unsigned a,unsigned v=0,unsigned char* enables=nullptr,unsigned n=4,bool dbg=false,
        tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE){
        unsigned char b[4];for(unsigned i=0;i<4;++i)b[i]=v>>(8*i);
        tlm::tlm_generic_payload t;t.set_command(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);t.set_address(a);t.set_data_ptr(b);t.set_data_length(n);t.set_streaming_width(n);
        t.set_byte_enable_ptr(enables);t.set_byte_enable_length(enables?4:0);
        auto delay=sc_core::sc_time(3,sc_core::SC_NS);if(dbg)cpu->transport_dbg(t);else cpu->b_transport(t,delay);
        expect(t.get_response_status()==expected,"TLM response");expect(!t.is_dmi_allowed(),"no DMI");
        if(!dbg)expect(delay==sc_core::SC_ZERO_TIME,"annotation consumed");settle();
        unsigned result=0;for(unsigned i=0;i<4;++i)result|=unsigned(b[i])<<(8*i);return result;
    }
    void w(unsigned a,unsigned v){access(true,a,v);}unsigned r(unsigned a){return access(false,a);}unsigned peek(unsigned a){return access(false,a,0,nullptr,4,true);}
    void finish(){for(unsigned i=0;i<10000;++i){if(peek(32)&4)return;wait(100,sc_core::SC_NS);}expect(false,"EOT deadline");}
    unsigned mask(unsigned bits){return bits==32?0xffffffffu:(1u<<bits)-1;}
    // Independent external master, holding each half period for four SYS cycles.
    unsigned frame(unsigned input,unsigned bits,bool lsb,bool cpol,bool cpha){
        unsigned result=0;
        for(unsigned i=0;i<bits;++i){unsigned pos=lsb?i:bits-1-i;
            p.mi=(input>>pos)&1;wait(40,sc_core::SC_NS);
            if(!cpha)result|=unsigned(p.miso.read())<<pos;
            p.si=!cpol;wait(40,sc_core::SC_NS);
            if(cpha)result|=unsigned(p.miso.read())<<pos;
            p.si=cpol;wait(40,sc_core::SC_NS); // Hold MOSI across the trailing sample edge.
        }
        wait(40,sc_core::SC_NS);return result;
    }
    void run(){
        cold();unsigned defaults[12]={0xe0,0,0x11,0,0,6,0x100,0,1,0x10100,(depth<<16)|depth,0};
        for(unsigned i=0;i<12;++i)if(i!=4)expect(r(i*4)==defaults[i],"register reset value");
        expect(!p.irq&&!p.tr&&!p.rr&&p.ss.read()==15&&!p.so&&!p.mio,"reset outputs");
        expect(r(0xfffc)==0,"reserved offset reads zero");w(0xfffc,0xffffffff);expect(r(0xfffc)==0,"reserved write ignored");
        w(5,0x1234);expect(r(4)==0x1234,"word PADDR low bits ignored");
        unsigned char lanes[4]={255,0,255,0};access(true,44,0xaabbccdd,lanes);expect(r(44)==0x00bb00dd,"DLY byte strobes");
        w(28,0x7f);expect(r(16)==0,"empty RXDR reads zero");wait(20,sc_core::SC_NS);expect(p.irq&&(r(32)&64),"underflow sticky interrupt");
        unsigned char none[4]={};access(true,32,64,none);expect(r(32)&64,"W1C requires lane zero");w(32,64);expect(!(r(32)&64),"W1C clears sticky cause");
        cold();for(unsigned i=0;i<depth;++i)access(true,12,i,none); // TXDR ignores all strobes.
        expect(((r(20)>>8)&127)==depth&&!(r(20)&4),"FIFO fills even with PSTRB zero");w(12,99);expect(r(32)&32,"TX overflow drops word and flags");
        w(0,0xe0|1024);wait((depth+8)*10,sc_core::SC_NS);expect((r(20)&6)==6&&!(r(0)&1024),"TX flush drains and self clears");
        access(true,12,7,nullptr,1,false,tlm::TLM_BURST_ERROR_RESPONSE);

        // Every frame-size encoding, bit order and CPOL/CPHA mode, with pin loopback.
        for(unsigned fsz=0;fsz<32;++fsz)for(unsigned mode=0;mode<4;++mode)for(unsigned lsb=0;lsb<2;++lsb){
            cold();const unsigned bits=std::max(4u,fsz+1),ctrl=2|(fsz<<5)|((mode&2)?4:0)|((mode&1)?8:0)|(lsb?16:0);
            w(0,ctrl);w(4,mode%3);w(12,0xa153c6e9);w(12,0x8a6c3592);w(0,ctrl|1);finish();
            expect(r(16)==(0xa153c6e9&mask(bits)),"first master frame data");expect(r(16)==(0x8a6c3592&mask(bits)),"back-to-back master frame data");
            expect(!(r(32)&(8|16))&&p.ss.read()==15,"master has no OVR/UDR and auto CS releases");
        }
        // Nonzero IFD retains the next word until GAP ends, in all clock modes.
        for(unsigned mode=0;mode<4;++mode){
            const unsigned control=0xe3|((mode&2)?4:0)|((mode&1)?8:0);
            for(unsigned action=0;action<3;++action){
                cold();w(0,control&~1u);w(44,100u<<16);w(12,0x31);w(12,0x72);w(0,control);
                wait(300,sc_core::SC_NS);
                const auto gap=peek(20);
                expect(((gap>>8)&127)==1&&((gap>>16)&127)==1,"IFD retains one waiting TX word after first RX frame");
                expect((peek(40)&127)==depth-1&&p.ss.read()==14,"GAP preserves TX vacancy and asserted CS");
                if(action==0){
                    finish();expect(r(16)==0x31&&r(16)==0x72,"nonzero IFD resumes ordered frames");
                }else if(action==1){
                    w(0,control|1024);finish();
                    expect(((peek(20)>>16)&127)==1&&((peek(20)>>8)&127)==0,"GAP flush prevents transmission of waiting word");
                    expect(r(16)==0x31&&!(r(0)&1024)&&p.ss.read()==15,"flush preserves completed RX and releases select");
                }else{
                    w(0,control&~1u);wait(50,sc_core::SC_NS);
                    expect(((peek(20)>>8)&127)==1&&!(peek(32)&4),"GAP abort retains unstarted word without EOT");
                }
            }
        }
        // RX-full backpressure retains CS and resumes without losing a queued frame.
        cold();w(0,0xe2);for(unsigned i=0;i<depth;++i)w(12,i);w(0,0xe3);
        for(unsigned i=0;i<depth+1;++i){if(((peek(20)>>16)&127)==depth)break;wait(2000,sc_core::SC_NS);}
        w(12,0xab);w(12,0xcd);w(32,4);wait(500,sc_core::SC_NS);
        expect(((r(20)>>16)&127)==depth&&!(r(32)&8),"full RX stalls master without overrun");
        expect(r(16)==0,"drain oldest frame");wait(2000,sc_core::SC_NS);
        expect(p.ss.read()==14&&!(r(32)&4),"CS remains selected while waiting for second RX slot");
        for(unsigned i=1;i<depth;++i)expect(r(16)==i,"FIFO order under backpressure");finish();
        expect(r(16)==0xab&&r(16)==0xcd,"stalled frames resume in order");
        w(0,0xe2|2048);wait(50,sc_core::SC_NS);expect(!(r(0)&2048),"RX flush self clears");

        // Manual multi-select and TX-only transfers do not fill RX.
        cold();w(8,0x25);w(0,0x10e3);expect(p.ss.read()==10,"manual broadcast CS mask");
        w(12,0x55);finish();expect(((r(20)>>16)&127)==0&&p.ss.read()==10,"TXONLY discards RX and manual SS stays asserted");
        w(8,5);expect(p.ss.read()==15,"manual deselect");w(0,0x10e2);expect(p.so&&p.mo,"master OE independent of EN");
        // Abort discards the in-flight frame without EOT.
        cold();w(4,100);w(0,0xe3);w(12,0x33);w(0,0xe2);wait(100,sc_core::SC_NS);
        expect(!(r(32)&4)&&!(r(20)&1)&&p.ss.read()==15&&!p.sck,"EN abort without EOT");
        for(unsigned cpha=0;cpha<2;++cpha){
            cold();const unsigned control=0xe7|(cpha?8:0);w(0,control&~1u);w(4,100);w(12,0x55);w(0,control);
            expect(p.sck&&p.so,"CPOL-high master is driving before abort");
            w(0,control&~2u);wait(50,sc_core::SC_NS);
            expect(!p.sck&&!p.so&&!p.mo&&p.ss.read()==15&&!(r(32)&4),"MSTR abort drives SCK zero without EOT for CPOL high");
        }

        // External master verifies slave data, first-edge pop and early SS termination.
        for(unsigned mode=0;mode<4;++mode)for(unsigned lsb=0;lsb<2;++lsb){
            cold();bool cpol=mode&2,cpha=mode&1;p.si=cpol;w(0,0xe0|(cpol?4:0)|(cpha?8:0)|(lsb?16:0));
            w(12,0x96);w(0,0xe1|(cpol?4:0)|(cpha?8:0)|(lsb?16:0));p.select=false;wait(50,sc_core::SC_NS);
            expect(p.mio&&((peek(20)>>8)&127)==1,"slave peeks without popping at SS assertion");
            expect(frame(0x69,8,lsb,cpol,cpha)==0x96,"slave response bit order");
            p.select=true;wait(40,sc_core::SC_NS);expect(r(16)==0x69&&(r(32)&4)&&!p.mio,"slave receives data and EOT at SS rise");
        }
        cold();w(0,0xe1);p.select=false;wait(50,sc_core::SC_NS);w(12,0xa5);
        expect(frame(0x11,8,false,false,false)==0,"late TX word cannot replace preloaded zeros");expect(r(32)&16,"underrun on first edge");
        expect(frame(0x22,8,false,false,false)==0xa5,"late word used for next frame");
        for(unsigned i=0;i<depth;++i)frame(i,8,false,false,false);
        expect(r(32)&8,"slave RX overflow flags lost frame");p.select=true;wait(30,sc_core::SC_NS);
        cold();w(0,0xe1);p.select=false;wait(50,sc_core::SC_NS);p.si=true;wait(40,sc_core::SC_NS);p.select=true;wait(40,sc_core::SC_NS);
        expect(((r(20)>>16)&127)==0&&(r(32)&4),"partial slave frame discarded with EOT");

        // Four-phase DMA request must survive threshold changes until acknowledge.
        cold();w(36,0x10101);w(0,0xe1);wait(40,sc_core::SC_NS);expect(p.tr,"TX DMA request");
        for(unsigned i=0;i<depth;++i)w(12,i);expect(p.tr,"DMA request held after threshold ceases");
        p.ta=true;wait(40,sc_core::SC_NS);expect(!p.tr,"acknowledge drops request");
        w(0,0xe1|1024);wait((depth+8)*10,sc_core::SC_NS);expect(!p.tr,"WAIT blocks request while ack high");
        p.ta=false;wait(50,sc_core::SC_NS);expect(p.tr,"request rearms after ack low");w(36,0);expect(!p.tr,"DMA disable cancels request");

        // APB-only reset retains state; debug RX reads have the documented side effect.
        cold();w(0,0xe2);w(12,0x5a);w(0,0xe3);finish();
        p.apb_reset_n=false;wait(20,sc_core::SC_NS);p.apb_reset_n=true;wait(20,sc_core::SC_NS);
        expect(peek(16)==0x5a&&((peek(20)>>16)&127)==0,"debug RXDR pops and APB reset retains FIFO");
        expect(peek(16)==0&&(peek(32)&64),"debug empty RXDR raises underflow");
        cold();launch_reset_access.notify(sc_core::SC_ZERO_TIME);
        wait(15,sc_core::SC_NS);p.apb_reset_n=false; // Request has launched; response has not returned.
        wait(10,sc_core::SC_NS);p.apb_reset_n=true;wait(reset_access_done);
        expect(reset_accesses==1&&((peek(20)>>8)&127)==1,"APB reset drops response but commits exactly one FIFO push");
        w(12,0x76);expect(((r(20)>>8)&127)==2,"transport recovers without retrying the old write");
        // Thresholds are levels, including while disabled; W1C cannot suppress them.
        cold();w(24,0x7f7f);w(28,1);wait(20,sc_core::SC_NS);expect(p.irq&&(r(32)&1),"TX threshold 127 asserts with EN clear");
        w(32,1);expect(r(32)&1,"TXT is not W1C");w(28,0);expect(!p.irq,"IER masks level IRQ");
        // RX threshold zero still requires nonempty FIFO; flush removes a stored frame.
        w(24,0);expect(!(r(32)&2),"RX threshold zero does not assert on empty FIFO");
        w(0,0xe2);w(12,0x5c);w(0,0xe3);finish();expect(r(32)&2,"RX threshold zero asserts on data");
        w(0,0xe2|2048);wait(30,sc_core::SC_NS);expect(!(r(20)&8)&&!(r(0)&2048)&&!(r(32)&2),"RX flush drains data and clears threshold");
        cold();expect(r(0)==0xe0&&r(32)==1&&r(40)==((depth<<16)|depth),"system reset clears all state");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(20,sc_core::SC_MS);expect(false,"behavior watchdog");sc_core::sc_stop();}
};
int sc_main(int argc,char** argv){unsigned depth=argc>1?unsigned(std::stoul(argv[1])):16;Bench b{"bench",depth};sc_core::sc_start();std::cout<<"SPI behavior depth "<<depth<<": "<<b.checks<<" checks, "<<b.failures<<" failures\n";return b.failures||!b.finished?1:0;}
