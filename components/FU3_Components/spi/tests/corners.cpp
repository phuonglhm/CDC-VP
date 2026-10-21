#include "support/fixture.h"
#include <algorithm>
#include <string>

// Expectations come from HDS pp.23-28 and the checklist on pp.43-44.
// Only public pins and MMIO are used; no production FSM helpers are shared.
struct Corners : sc_core::sc_module, Checks {
    fu3_spi::Spi dut;
    Pins p{"pins"};
    tlm_utils::simple_initiator_socket<Corners> cpu{"cpu"};
    const unsigned depth;
    const sc_core::sc_time apb;
    std::string scenario;
    sc_core::sc_time origin;
    sc_core::sc_event reset_start, reset_done;
    bool reset_completed=false;
    SC_HAS_PROCESS(Corners);
    Corners(sc_core::sc_module_name n, std::string which, unsigned d, unsigned apb_ns)
        : sc_module(n), dut("dut",d,sc_core::sc_time(10,sc_core::SC_NS),
                            sc_core::sc_time(apb_ns,sc_core::SC_NS)),
          depth(d),apb(apb_ns,sc_core::SC_NS),scenario(std::move(which)) {
        p.bind(dut);cpu.bind(dut.socket);
        SC_THREAD(run);SC_THREAD(reset_writer);SC_THREAD(watchdog);
    }
    void settle(){for(unsigned i=0;i<6;++i)wait(sc_core::SC_ZERO_TIME);}
    void cold(){
        p.reset_n=false;p.apb_reset_n=false;p.select=true;p.si=false;p.mi=false;p.ta=false;p.ra=false;
        wait(25,sc_core::SC_NS);p.reset_n=true;p.apb_reset_n=true;
        origin=sc_core::sc_time_stamp();wait(25,sc_core::SC_NS);settle();
    }
    sc_core::sc_time next_tick(sc_core::sc_time after) const {
        const auto tick=sc_core::sc_time(10,sc_core::SC_NS);
        return origin+sc_core::sc_time::from_value(((after-origin).value()/tick.value()+1)*tick.value());
    }
    void off_edge(){wait(next_tick(sc_core::sc_time_stamp())-sc_core::sc_time_stamp()+sc_core::sc_time(1,sc_core::SC_NS));settle();}
    unsigned access(bool write,unsigned address,unsigned value=0,bool debug=false,
                    tlm::tlm_response_status expected=tlm::TLM_OK_RESPONSE){
        unsigned char data[4]{};for(unsigned i=0;i<4;++i)data[i]=value>>(8*i);
        tlm::tlm_generic_payload t;t.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        t.set_address(address);t.set_data_ptr(data);t.set_data_length(4);t.set_streaming_width(4);
        auto delay=sc_core::SC_ZERO_TIME;const auto before=sc_core::sc_time_stamp();
        if(debug)expect(cpu->transport_dbg(t)==4,"untimed register inspection returns four bytes");
        else cpu->b_transport(t,delay);
        expect(t.get_response_status()==expected,"corner MMIO response");
        expect(delay==sc_core::SC_ZERO_TIME&&!t.is_dmi_allowed(),"corner MMIO consumes delay and disables DMI");
        if(debug)expect(sc_core::sc_time_stamp()==before,"inspection consumes no time");
        settle();value=0;for(unsigned i=0;i<4;++i)value|=unsigned(data[i])<<(8*i);return value;
    }
    void w(unsigned a,unsigned v){access(true,a,v);}
    unsigned r(unsigned a){return access(false,a);}
    unsigned peek(unsigned a){return access(false,a,0,true);}
    unsigned txlevel(){return (peek(FU3_SPI_STAT)>>8)&127;}
    unsigned rxlevel(){return (peek(FU3_SPI_STAT)>>16)&127;}
    static unsigned mask(unsigned bits){return bits==32?~0u:(1u<<bits)-1;}
    void tick_wait(unsigned n){wait(n*10,sc_core::SC_NS);settle();}
    void wait_eot(){for(unsigned i=0;i<200000;++i){if(peek(FU3_SPI_ISR)&FU3_SPI_EOT)return;tick_wait(10);}expect(false,"EOT bounded deadline");}
    unsigned frame(unsigned input,unsigned bits,bool lsb,bool cpol,bool cpha){
        unsigned result=0;
        for(unsigned i=0;i<bits;++i){
            const unsigned position=lsb?i:bits-1-i;
            p.mi=bool(input&(1u<<position));wait(43,sc_core::SC_NS);
            if(!cpha)result|=unsigned(p.miso.read())<<position;
            p.si=!cpol;wait(47,sc_core::SC_NS);
            if(cpha)result|=unsigned(p.miso.read())<<position;
            p.si=cpol;wait(41,sc_core::SC_NS);
        }
        wait(23,sc_core::SC_NS);settle();return result;
    }
    void slave_formats(){
        for(unsigned encoding=0;encoding<32;++encoding)for(unsigned mode=0;mode<4;++mode)for(unsigned lsb=0;lsb<2;++lsb){
            cold();const unsigned bits=std::max(4u,encoding+1);
            const bool cpol=mode&2,cpha=mode&1;
            const unsigned control=(encoding<<5)|(cpol?4:0)|(cpha?8:0)|(lsb?16:0);
            p.si=cpol;w(FU3_SPI_CTRL,control);w(FU3_SPI_TXDR,0xa153c6e9);w(FU3_SPI_TXDR,0x72ac1956);
            w(FU3_SPI_CTRL,control|FU3_SPI_EN);p.select=false;wait(47,sc_core::SC_NS);settle();
            expect(txlevel()==2&&p.mio,"slave preload keeps both FIFO entries before first edge");
            expect(frame(0x96c35a17,bits,lsb,cpol,cpha)==(0xa153c6e9&mask(bits)),"independent first slave MISO frame");
            expect(frame(0x693ca5e8,bits,lsb,cpol,cpha)==(0x72ac1956&mask(bits)),"independent second slave MISO frame");
            expect(txlevel()==0&&rxlevel()==2,"two complete slave frames, no duplicates");
            p.select=true;wait(27,sc_core::SC_NS);settle();
            expect(!p.mio&&(peek(FU3_SPI_ISR)&FU3_SPI_EOT),"deselect releases MISO and sets EOT");
            expect(r(FU3_SPI_RXDR)==(0x96c35a17&mask(bits)),"slave RX first frame, upper bits zero");
            expect(r(FU3_SPI_RXDR)==(0x693ca5e8&mask(bits)),"slave RX second frame, upper bits zero");
            expect(!(peek(FU3_SPI_ISR)&(FU3_SPI_OVR|FU3_SPI_UDR|FU3_SPI_TXOVF|FU3_SPI_RXUDF)),"valid slave frames have no FIFO errors");
        }
    }
    void fifo_flush(){
        cold();w(FU3_SPI_CTRL,0xe1);
        for(unsigned i=0;i<depth;++i)w(FU3_SPI_TXDR,0x100+i);
        w(FU3_SPI_CTRL,0xe1|FU3_SPI_TXFLUSH);
        tick_wait(3); // Allow the flush request to reach the FIFO reader before refilling.
        // Writes concurrent with an active drain may extend it, but must not overflow.
        for(unsigned i=0;i<depth/2;++i)w(FU3_SPI_TXDR,0x200+i);
        tick_wait(depth+12);
        expect(txlevel()==0&&!(peek(FU3_SPI_CTRL)&FU3_SPI_TXFLUSH),"concurrent TX writes are drained and flush completes");
        expect(!(peek(FU3_SPI_ISR)&FU3_SPI_TXOVF),"flush/write sequence does not overflow");
        w(FU3_SPI_TXDR,0x5a);tick_wait(10);
        expect(txlevel()==1,"word written after flush acknowledgement is retained");
        expect(!(peek(FU3_SPI_CTRL)&FU3_SPI_TXFLUSH),"completed flush does not retrigger itself");
        w(FU3_SPI_CTRL,0xe1|FU3_SPI_TXFLUSH);w(FU3_SPI_CTRL,0xe1|FU3_SPI_TXFLUSH);tick_wait(12);
        expect(txlevel()==0&&!(peek(FU3_SPI_CTRL)&FU3_SPI_TXFLUSH),"repeated TX flush requests terminate");
        for(unsigned i=0;i<depth;++i)w(FU3_SPI_TXDR,i);
        w(FU3_SPI_CTRL,0xe1|FU3_SPI_TXFLUSH);
        for(unsigned i=0;i<depth+8&&txlevel()!=1;++i)tick_wait(1);
        expect(txlevel()==1,"observe the final entry before flush completion");
        w(FU3_SPI_CTRL,0xe1|FU3_SPI_TXFLUSH);
        expect(txlevel()==0&&!(peek(FU3_SPI_CTRL)&FU3_SPI_TXFLUSH),"hardware flush completion wins over same-tick software set");
        // This write reaches the request/acknowledge return phase. It may cause
        // a later empty flush, but must finish without draining a future word.
        w(FU3_SPI_CTRL,0xe1|FU3_SPI_TXFLUSH);tick_wait(12);
        expect(!(peek(FU3_SPI_CTRL)&FU3_SPI_TXFLUSH),"flush rewrite during acknowledgement return completes");
        w(FU3_SPI_TXDR,0xa5);p.select=false;wait(47,sc_core::SC_NS);
        expect(frame(0x3c,8,false,false,false)==0xa5,"post-flush frame retains data order");
        p.select=true;tick_wait(4);expect(r(FU3_SPI_RXDR)==0x3c,"post-flush RX data");

        // Fill RX externally, then start a flush while another complete frame arrives.
        p.select=false;wait(47,sc_core::SC_NS);
        for(unsigned i=0;i<depth;++i)frame(i,8,false,false,false);
        expect(rxlevel()==depth,"RX FIFO full before flush");
        w(FU3_SPI_ISR,FU3_SPI_OVR);w(FU3_SPI_CTRL,0xe1|FU3_SPI_RXFLUSH);
        frame(0x7b,8,false,false,false);
        expect(!(peek(FU3_SPI_CTRL)&FU3_SPI_RXFLUSH),"RX flush self-clears when drained");
        expect(rxlevel()==1&&r(FU3_SPI_RXDR)==0x7b,"frame arriving after drain is retained once");
        expect(!(peek(FU3_SPI_ISR)&FU3_SPI_OVR),"RX flush plus reception avoids spurious overrun");
        p.select=true;tick_wait(4);

        // A receive completion while an RX flush is actively draining follows
        // the documented VP order: the older entry drains, then the new one pushes.
        cold();w(FU3_SPI_CTRL,0xe1);p.select=false;tick_wait(5);
        for(unsigned i=0;i<depth;++i)frame(i,8,false,false,false);
        for(unsigned i=0;i<15;++i){p.mi=false;p.si=(i%2)==0;wait(43,sc_core::SC_NS);}
        w(FU3_SPI_CTRL,0xe1|FU3_SPI_RXFLUSH);
        const auto before=rxlevel();
        p.si=false;tick_wait(1);
        expect(rxlevel()==before&&!(peek(FU3_SPI_ISR)&FU3_SPI_OVR),"active RX flush and frame completion preserve capacity without overrun");
        tick_wait(depth+4);expect(rxlevel()==0&&!(peek(FU3_SPI_CTRL)&FU3_SPI_RXFLUSH),"active flush eventually drains the newly received word too");
        p.select=true;tick_wait(4);

        // Software can stop RX flush by clearing its bit, preserving the tail.
        cold();w(FU3_SPI_CTRL,0xe1);p.select=false;tick_wait(5);
        for(unsigned i=0;i<depth;++i)frame(i,8,false,false,false);
        p.select=true;tick_wait(4);w(FU3_SPI_CTRL,0xe1|FU3_SPI_RXFLUSH);w(FU3_SPI_CTRL,0xe1);
        const auto retained=rxlevel();tick_wait(5);
        expect(retained>0&&retained<depth&&rxlevel()==retained,"clearing RXFLUSH stops draining with a stable unread tail");
        for(unsigned i=depth-retained;i<depth;++i)expect(r(FU3_SPI_RXDR)==i,"stopped flush retains the ordered FIFO tail");
    }
    void master_delays(){
        for(unsigned divider:{0u,1u,2u,65535u})master_delay_case(divider,0);
        for(unsigned delay:{1u,255u})master_delay_case(0,delay);
    }
    void master_delay_case(unsigned divider,unsigned field){
        cold();w(FU3_SPI_CTRL,0x62); // Four-bit CPOL=0, CPHA=0 master.
        w(FU3_SPI_DIV,divider);w(FU3_SPI_DLY,field|(field<<8)|(field<<16)|(field<<24));
        w(FU3_SPI_TXDR,9);w(FU3_SPI_TXDR,6);w(FU3_SPI_CTRL,0x63);
        if(p.ss.read()==15)wait(p.ss.value_changed_event());settle();
        const auto selected=sc_core::sc_time_stamp();
        const auto half=sc_core::sc_time(10,sc_core::SC_NS)*double(divider+1);
        sc_core::sc_time last;
        unsigned sampled[2]{};
        for(unsigned edge=0;edge<16;++edge){
            wait(p.sck.value_changed_event());settle();const auto now=sc_core::sc_time_stamp();
            if(edge==0)expect(now-selected==half*double(2*field+1),"CSSU delays the first edge by its documented functional interval");
            else if(edge==8)expect(now-last==half*double(2*field+1),"IFD preserves the documented frame-to-frame interval");
            else expect(now-last==half,"DIV controls the interval between adjacent frame edges");
            if((edge%2)==0)sampled[edge/8]=(sampled[edge/8]<<1)|unsigned(p.mosi.read());
            last=now;
        }
        expect(sampled[0]==9&&sampled[1]==6,"independent MOSI observer sees exactly two ordered frames");
        expect(!(peek(FU3_SPI_ISR)&FU3_SPI_EOT),"automatic EOT absent before hold completes");
        wait(p.ss.value_changed_event());settle();
        const auto released=sc_core::sc_time_stamp();
        expect(released-last==half*double(2*field+1),"CSHD holds select after the last edge");
        expect((peek(FU3_SPI_ISR)&FU3_SPI_EOT)&&rxlevel()==2,"EOT accompanies completed hold with two RX frames");
        expect(r(FU3_SPI_RXDR)==9&&r(FU3_SPI_RXDR)==6,"divider/delay extremes preserve RX data");
        w(FU3_SPI_ISR,FU3_SPI_EOT);w(FU3_SPI_TXDR,3);
        if(p.ss.read()==15)wait(p.ss.value_changed_event());settle();
        expect(sc_core::sc_time_stamp()-released>=half*double(2*field)+sc_core::sc_time(10,sc_core::SC_NS),"CSIDLE enforces minimum gap before a new transfer");
        wait_eot();expect(r(FU3_SPI_RXDR)==3,"new transfer completes after idle without lost data");
    }
    void event_priority(){
        cold();w(FU3_SPI_CTRL,0xe1);w(FU3_SPI_IER,FU3_SPI_EOT|FU3_SPI_UDR|FU3_SPI_OVR);
        p.select=false;tick_wait(5);
        off_edge();p.si=true;w(FU3_SPI_ISR,FU3_SPI_UDR);
        expect(peek(FU3_SPI_ISR)&FU3_SPI_UDR,"first-edge UDR wins over same-tick W1C");
        tick_wait(2);expect(p.irq,"new sticky cause reaches registered IRQ");
        w(FU3_SPI_ISR,FU3_SPI_UDR);tick_wait(2);
        expect(!(peek(FU3_SPI_ISR)&FU3_SPI_UDR)&&!p.irq,"later W1C clears the event");
        off_edge();p.select=true;w(FU3_SPI_ISR,FU3_SPI_EOT);
        expect(peek(FU3_SPI_ISR)&FU3_SPI_EOT,"deselect EOT wins over same-tick W1C");
        w(FU3_SPI_ISR,FU3_SPI_EOT);expect(!(peek(FU3_SPI_ISR)&FU3_SPI_EOT),"later EOT clear succeeds");

        cold();w(FU3_SPI_CTRL,0xe1);p.select=false;tick_wait(5);
        for(unsigned i=0;i<depth;++i)frame(i,8,false,false,false);
        // Supply all but the final edge of one extra frame, with RX still full.
        p.mi=false;
        for(unsigned i=0;i<15;++i){p.si=(i%2)==0;wait(43,sc_core::SC_NS);}
        off_edge();p.si=false;w(FU3_SPI_ISR,FU3_SPI_OVR);
        expect((peek(FU3_SPI_ISR)&FU3_SPI_OVR)&&rxlevel()==depth,"frame-end OVR wins over same-tick W1C and drops new frame");
        for(unsigned i=0;i<depth;++i)expect(r(FU3_SPI_RXDR)==(i&255),"overrun preserves every earlier RX word");
        p.select=true;tick_wait(3);
    }
    void abort_states(){
        for(unsigned phase=0;phase<6;++phase)for(unsigned clear_master=0;clear_master<3;++clear_master){
            cold();const unsigned control=0xe7; // CPOL high makes idle-output mistakes visible.
            w(FU3_SPI_CTRL,control&~FU3_SPI_EN);w(FU3_SPI_DIV,4);
            w(FU3_SPI_DLY,10u|(10u<<8)|(10u<<16)|(100u<<24));
            if(phase){w(FU3_SPI_TXDR,0x5a);if(phase==3)w(FU3_SPI_TXDR,0xa5);}
            w(FU3_SPI_CTRL,control);
            if(phase){
                // Observe pins instead of accessing the private state enum.
                if(p.ss.read()==15)wait(p.ss.value_changed_event());settle();
                if(phase>=2){wait(p.sck.value_changed_event());settle();}
                if(phase>=3){for(unsigned edge=1;edge<16;++edge)wait(p.sck.value_changed_event());settle();}
                if(phase==5){wait(p.ss.value_changed_event());settle();w(FU3_SPI_ISR,FU3_SPI_EOT);}
            }
            const auto completed_rx=rxlevel();
            if(clear_master==2){
                // System reset is issued only after the MMIO response has completed.
                p.reset_n=false;wait(1,sc_core::SC_NS);settle();
                expect(p.ss.read()==15&&!p.sck&&!p.so&&!p.mo&&!p.mio&&!p.irq&&!p.tr&&!p.rr,
                       "system reset releases outputs and requests in every master state");
                p.reset_n=true;origin=sc_core::sc_time_stamp();tick_wait(3);
                expect(peek(FU3_SPI_CTRL)==0xe0&&rxlevel()==0&&txlevel()==0&&peek(FU3_SPI_ISR)==FU3_SPI_TXT,
                       "system reset discards partial work and restores the register/FIFO profile");
                continue;
            }
            w(FU3_SPI_CTRL,control&~(clear_master?FU3_SPI_MSTR:FU3_SPI_EN));tick_wait(3);
            expect(p.ss.read()==15&&!p.sck,"abort releases CS and drives SCK low in every master state");
            expect(!(peek(FU3_SPI_ISR)&FU3_SPI_EOT),"abort generates no new EOT");
            expect(rxlevel()==completed_rx,"abort preserves completed RX and adds no partial frame");
            expect(bool(p.so.read())==!clear_master&&bool(p.mo.read())==!clear_master,"abort output enables follow MSTR");
            if(phase==3)expect(txlevel()==1,"GAP abort preserves the unstarted TX word");
        }
    }
    void dma_boundaries(){
        for(unsigned threshold:{0u,1u,depth}){
            cold();const unsigned effective=std::max(1u,threshold);
            w(FU3_SPI_CTRL,0xe1);w(FU3_SPI_DMACR,3u|(threshold<<8)|(threshold<<16));tick_wait(5);
            expect(p.tr&&!p.rr,"empty FIFO requests TX, never RX, including threshold zero");
            for(unsigned i=0;i<depth;++i)w(FU3_SPI_TXDR,0x80+i);
            expect(p.tr,"TX REQ survives loss of vacancy condition");
            p.ta=true;tick_wait(5);expect(!p.tr,"TX ACK enters WAIT");
            w(FU3_SPI_CTRL,0xe1|FU3_SPI_TXFLUSH);tick_wait(depth+8);expect(!p.tr,"TX WAIT ignores restored vacancy while ACK high");
            p.ta=false;tick_wait(6);expect(p.tr,"TX rearms after ACK returns low");
            p.select=false;tick_wait(5);
            for(unsigned i=0;i<effective;++i){
                frame(i+1,8,false,false,false);
                if(i+1<effective)expect(!p.rr,"RX request absent below threshold");
            }
            tick_wait(3);expect(p.rr,"RX asserts exactly when effective threshold is reached");
            for(unsigned i=0;i<effective;++i)expect(r(FU3_SPI_RXDR)==i+1,"DMA-sized RX drain preserves order");
            expect(p.rr,"RX REQ survives empty FIFO until ACK");
            p.ra=true;tick_wait(5);expect(!p.rr,"RX ACK enters WAIT");
            for(unsigned i=0;i<effective;++i)frame(i+11,8,false,false,false);
            expect(!p.rr,"RX WAIT holds despite enough new data");
            p.ra=false;tick_wait(6);expect(p.rr,"RX rearms after ACK low");
            w(FU3_SPI_DMACR,0);tick_wait(3);expect(!p.tr&&!p.rr,"DMACR disable cancels both REQ directions");
            w(FU3_SPI_DMACR,3u|(threshold<<8)|(threshold<<16));tick_wait(5);p.ra=true;p.ta=true;tick_wait(5);
            w(FU3_SPI_CTRL,0xe0);tick_wait(3);expect(!p.tr&&!p.rr,"EN disable cancels requests and WAIT state");
        }
    }
    void reset_writer(){while(true){wait(reset_start);access(true,FU3_SPI_TXDR,0x75,false,tlm::TLM_GENERIC_ERROR_RESPONSE);reset_completed=true;reset_done.notify(sc_core::SC_ZERO_TIME);}}
    void reset_phases(){
        for(unsigned phase=0;phase<3;++phase){
            cold();off_edge();reset_completed=false;
            const auto commit=next_tick(sc_core::sc_time_stamp()+apb);
            reset_start.notify(sc_core::SC_ZERO_TIME);
            const auto when=commit+apb*(0.25+phase);
            wait(when-sc_core::sc_time_stamp());p.apb_reset_n=false;
            wait(apb*0.25);p.apb_reset_n=true;
            while(!reset_completed)wait(reset_done);
            expect(txlevel()==1,"APB reset after commit keeps exactly one TXDR side effect");
            w(FU3_SPI_TXDR,0x76);expect(txlevel()==2,"next access recovers without replay");
        }
        cold();w(FU3_SPI_CTRL,0xe2);w(FU3_SPI_DIV,4);w(FU3_SPI_TXDR,0x96);w(FU3_SPI_CTRL,0xe3);
        p.apb_reset_n=false;tick_wait(7);p.apb_reset_n=true;
        wait_eot();expect(r(FU3_SPI_RXDR)==0x96,"APB-only reset preserves ongoing serial transfer");
        p.reset_n=false;wait(1,sc_core::SC_NS);settle();
        expect(!p.irq&&!p.tr&&!p.rr&&p.ss.read()==15&&!p.so&&!p.mo&&!p.mio,"system reset clears all output enables and requests");
        p.reset_n=true;origin=sc_core::sc_time_stamp();tick_wait(3);
        expect(peek(FU3_SPI_CTRL)==0xe0&&txlevel()==0&&rxlevel()==0,"system reset restores registers and FIFOs");
        cold();w(FU3_SPI_CTRL,0xe0);w(FU3_SPI_TXDR,0x5a);w(FU3_SPI_CTRL,0xe1);
        p.select=false;tick_wait(5);
        for(unsigned i=0;i<4;++i){p.si=(i%2)==0;wait(43,sc_core::SC_NS);}
        p.reset_n=false;wait(1,sc_core::SC_NS);settle();expect(!p.mio,"reset releases selected slave MISO during a partial frame");
        p.select=true;p.si=false;p.reset_n=true;origin=sc_core::sc_time_stamp();tick_wait(3);
        expect(rxlevel()==0&&txlevel()==0,"slave reset leaves no partial RX or stale TX word");
        w(FU3_SPI_TXDR,0xa5);w(FU3_SPI_CTRL,0xe1);p.select=false;tick_wait(5);
        expect(frame(0x69,8,false,false,false)==0xa5,"slave restarts with fresh TX data after reset");
        p.select=true;tick_wait(3);expect(r(FU3_SPI_RXDR)==0x69,"slave reset recovery contains only the new complete frame");
    }
    void run(){
        if(scenario=="slave_formats")slave_formats();
        else if(scenario=="fifo_flush")fifo_flush();
        else if(scenario=="event_priority")event_priority();
        else if(scenario=="abort_states")abort_states();
        else if(scenario=="dma_boundaries")dma_boundaries();
        else if(scenario=="reset_phases")reset_phases();
        else if(scenario=="master_delays")master_delays();
        else expect(false,"unknown corner scenario");
        finished=true;sc_core::sc_stop();
    }
    void watchdog(){wait(100,sc_core::SC_MS);expect(false,"corner simulation watchdog");sc_core::sc_stop();}
};
int sc_main(int argc,char** argv){
    const std::string scenario=argc>1?argv[1]:"slave_formats";
    const unsigned depth=argc>2?std::stoul(argv[2]):16,apb=argc>3?std::stoul(argv[3]):1;
    Corners b{"bench",scenario,depth,apb};sc_core::sc_start();
    std::cout<<"SPI "<<scenario<<" depth="<<depth<<" APB="<<apb<<"ns: "<<b.checks<<" checks, "<<b.failures<<" failures\n";
    return !b.finished||b.failures?1:0;
}
