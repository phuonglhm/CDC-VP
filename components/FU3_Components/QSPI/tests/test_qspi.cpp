#include "fu3/qspi/qspi.h"
#include "fu3/qspi/a2h_qspi.h"
#include <algorithm>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sc_core;
using namespace fu3::qspi;
namespace {
// Test-only byte memory/status responder. It is deliberately not a NOR model:
// expected byte order, phase descriptors, timing and errors are checked here.
struct Record { FlashTransaction phase; std::vector<unsigned char> data; sc_time time; };
struct Memory : sc_module {
    tlm_utils::simple_target_socket<Memory> socket{"socket"};
    std::vector<unsigned char> mem=std::vector<unsigned char>(0x20000);
    std::vector<Record> records;
    std::vector<std::uint32_t> statuses;
    unsigned polls=0;
    bool fail=false;
    sc_time latency=SC_ZERO_TIME;
    explicit Memory(sc_module_name n):sc_module(n) {
        for (unsigned i=0;i<mem.size();++i) mem[i]=static_cast<unsigned char>((i*37)^0xa5);
        socket.register_b_transport(this,&Memory::transport);
    }
    void transport(tlm::tlm_generic_payload& tx, sc_time& delay) {
        auto* p=tx.get_extension<FlashTransaction>();
        if (!p) throw std::runtime_error("missing flash descriptor");
        records.push_back({*p,{},sc_time_stamp()});
        if (p->cancel) { tx.set_response_status(tlm::TLM_OK_RESPONSE); return; }
        const auto n=tx.get_data_length();
        if (fail) { tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }
        if (p->instruction==5 && tx.is_read()) {
            const auto v=statuses.empty()?0:statuses[std::min<std::size_t>(polls,statuses.size()-1)];
            ++polls;
            for (unsigned i=0;i<n;++i) tx.get_data_ptr()[i]=static_cast<unsigned char>(v>>(8*i));
        } else if (n) {
            if (tx.get_address()+n>mem.size()) { tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE); return; }
            if (tx.is_read()) std::copy_n(mem.begin()+tx.get_address(),n,tx.get_data_ptr());
            else std::copy_n(tx.get_data_ptr(),n,mem.begin()+tx.get_address());
        }
        records.back().data.assign(tx.get_data_ptr(),tx.get_data_ptr()+n);
        delay+=latency;
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};
struct Bench : sc_module {
    sc_clock pc{"pc",14,SC_NS}, hc{"hc",22,SC_NS};
    sc_signal<bool> ck{"ck"};
    sc_signal<bool,SC_MANY_WRITERS> rn{"rn"}, prn{"prn"}, hrn{"hrn"};
    sc_signal<bool> rxclr{"rxclr"}, txclr{"txclr"}, irq{"irq"}, rxreq{"rxreq"}, txreq{"txreq"};
    Qspi dut;
    A2hQspi bridge{"bridge"};
    Memory memory{"memory"};
    tlm_utils::simple_initiator_socket<Bench> ap{"ap"}, mm{"mm"};
    std::string scenario;
    unsigned assertions=0;
    bool failed=false, clock_enabled=true, worker_done=false, worker2_done=false, workers_ap=false;
    tlm::tlm_response_status worker_result=tlm::TLM_INCOMPLETE_RESPONSE, worker2_result=tlm::TLM_INCOMPLETE_RESPONSE;
    sc_event worker_start, worker2_start;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name n,std::string s):sc_module(n),
        dut("dut",Config{32,32,200}),scenario(std::move(s)) {
        dut.clk(ck); dut.pclk(pc); dut.hclk(hc); dut.dma_clk(pc);
        dut.reset_n(rn); dut.preset_n(prn); dut.hreset_n(hrn); dut.dma_reset_n(rn);
        dut.dma_rx_clr(rxclr); dut.dma_tx_clr(txclr);
        dut.irq(irq); dut.dma_rx_req(rxreq); dut.dma_tx_req(txreq);
        ap.bind(dut.ap_qspic); mm.bind(bridge.socket); bridge.ahb.bind(dut.xip); dut.flash.bind(memory.socket);
        SC_THREAD(clock_thread); SC_THREAD(run); SC_THREAD(worker); SC_THREAD(worker2);
    }
    void clock_thread() { while (true) { ck.write(false); wait(5,SC_NS); ck.write(clock_enabled); wait(5,SC_NS); } }
    void check(bool ok,const std::string& message) {
        ++assertions;
        if (!ok) throw std::runtime_error(message+" at "+sc_time_stamp().to_string());
    }
    void ticks(unsigned n=1) { for(unsigned i=0;i<n;++i) wait(ck.posedge_event()); wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); }
    tlm::tlm_response_status bus(bool x,bool wr,std::uint64_t addr,std::uint32_t& value,
        unsigned length=4,unsigned char* enables=nullptr,unsigned be_len=0,unsigned stream=4,
        bool null_data=false,tlm::tlm_command command=tlm::TLM_IGNORE_COMMAND) {
        unsigned char data[8]{};
        for(unsigned i=0;i<4;++i) data[i]=static_cast<unsigned char>(value>>(8*i));
        tlm::tlm_generic_payload tx;
        tx.set_command(command==tlm::TLM_IGNORE_COMMAND?(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND):command);
        tx.set_address(addr); tx.set_data_ptr(null_data?nullptr:data); tx.set_data_length(length);
        tx.set_streaming_width(stream); tx.set_byte_enable_ptr(enables); tx.set_byte_enable_length(be_len);
        sc_time delay=SC_ZERO_TIME;
        if(x) mm->b_transport(tx,delay); else ap->b_transport(tx,delay);
        if (x && tx.is_response_ok()) { for(unsigned i=0; i<10000 && (peek(8)&reg::BUSY); ++i) ticks(); }
        value=0; for(unsigned i=0;i<4;++i) value|=std::uint32_t(data[i])<<(8*i);
        return tx.get_response_status();
    }
    void wr(unsigned a,std::uint32_t v) {
        check(bus(false,true,a,v)==tlm::TLM_OK_RESPONSE,"AP write "+std::to_string(a));
        if (a==0 && (v&reg::ABORT)) {
            for(unsigned i=0; i<10000 && (peek(0)&reg::ABORT); ++i) ticks();
            check(!(peek(0)&reg::ABORT),"abort completes");
        }
    }
    std::uint32_t rd(unsigned a) { std::uint32_t v=0; check(bus(false,false,a,v)==tlm::TLM_OK_RESPONSE,"AP read"); return v; }
    std::uint32_t peek(unsigned a) {
        unsigned char data[4]{}; tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_READ_COMMAND); tx.set_address(a); tx.set_data_ptr(data);
        tx.set_data_length(4); tx.set_streaming_width(4);
        check(ap->transport_dbg(tx)==4,"debug read");
        std::uint32_t v=0; for(unsigned i=0;i<4;++i) v|=std::uint32_t(data[i])<<(8*i);
        return v;
    }
    void reset() {
        rn=false; prn=false; hrn=false; rxclr=false; txclr=false;
        wait(31,SC_NS); rn=true; prn=true; hrn=true; ticks(2);
    }
    void configure(std::uint32_t extra=0) { wr(0x04,16u<<16); wr(0,0x00000001|extra); }
    static std::uint32_t ccr(unsigned mode,unsigned opcode=3,bool address=true) {
        return (mode<<26)|(1u<<24)|(address?((2u<<12)|(1u<<10)):0)|(1u<<8)|opcode;
    }
    void wait_complete() {
        for (unsigned i=0;i<10000;++i) { const auto s=rd(8); check(!(s&1),"unexpected TEF"); if(s&2) return; }
        check(false,"transfer timeout");
    }
    std::uint32_t expected(unsigned address,unsigned n=4) {
        std::uint32_t v=0; for(unsigned i=0;i<n;++i) v|=std::uint32_t(memory.mem[address+i])<<(8*i); return v;
    }
    void start_read(unsigned address,unsigned length) {
        wr(0xc,0x3f); wr(0x10,length-1); wr(0x14,ccr(1)); wr(0x18,address);
    }
    void consume(unsigned address,unsigned length) {
        unsigned done=0;
        for(unsigned tries=0;done<length && tries<10000;++tries) {
            const unsigned n=std::min(4u,length-done);
            if (((rd(8)>>16)&63)==0) continue;
            check(rd(0x20)==expected(address+done,n),"read data/tail/byte order"); done+=n;
        }
        check(done==length,"all bytes read");
        check((rd(8)&0x42)==2,"TCF with drained FIFO clears BUSY");
    }
    void worker() {
        while(true) { wait(worker_start); std::uint32_t v=0x11223344; worker_result=bus(!workers_ap,workers_ap,workers_ap?28:0,v); worker_done=true; }
    }
    void worker2() {
        while(true) { wait(worker2_start); std::uint32_t v=0xaabbccdd; worker2_result=bus(!workers_ap,workers_ap,workers_ap?28:4,v); worker2_done=true; }
    }
    void registers() {
        for(unsigned a=0;a<64;a+=4) check(peek(a)==(a==8?4u:0u),"HDS reset values");
        const std::vector<std::pair<unsigned,std::uint32_t>> cases={
            {4,0x001f0701},{16,0xffffffff},{20,0x9f7fffff},{28,0xffffffff},
            {36,0xffffffff},{40,0xffffffff},{44,0xffff},{48,0xffffffff},{52,15}};
        for(auto [a,mask]:cases) { wr(a,0xffffffff); check(rd(a)==mask,"register mask"); wr(a,0); }
        wr(24,0xffffffff); check(rd(24)==0xffffffff,"AR width");
        wr(0,0xffff1ffd); check(rd(0)==0xffff1ffd,"CR reserved bits");
        wr(0,0); wr(8,0xffffffff); wr(56,0xffffffff);
        check((rd(8)&~4u)==0 && rd(56)==0,"RO writes ignored");
        wr(12,0x3f); check(rd(12)==0 && rd(60)==0,"W1C read zero");
    }
    void protocol() {
        std::uint32_t v=0x11223344;
        for(auto a:{0x40ULL,0x100000000ULL}) check(bus(false,true,a,v)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"unmapped/high addresses rejected");
        for(auto len:{0u,1u,2u,8u}) check(bus(false,true,4,v,len)==tlm::TLM_BURST_ERROR_RESPONSE,"word only");
        check(bus(false,true,4,v,4,nullptr,0,0)==tlm::TLM_BURST_ERROR_RESPONSE,"stream width zero");
        check(bus(false,true,4,v,4,nullptr,0,4,true)==tlm::TLM_GENERIC_ERROR_RESPONSE,"null data");
        unsigned char be[]={255,0,255,255};
        check(bus(false,true,4,v,4,be,4)==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"partial strobe rejected");
        be[1]=255;
        check(bus(false,true,4,v,4,be,4)==tlm::TLM_OK_RESPONSE,"explicit full strobes");
        check(bus(false,true,4,v,4,be,0)==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"zero BE length");
        check(bus(true,true,0,v)==tlm::TLM_COMMAND_ERROR_RESPONSE,"XIP read only");
        check(bus(true,false,0,v)==tlm::TLM_GENERIC_ERROR_RESPONSE,"disabled XIP");
        unsigned char data[4]{}; tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_IGNORE_COMMAND); tx.set_data_ptr(data); tx.set_data_length(4); tx.set_streaming_width(4);
        sc_time delay=SC_ZERO_TIME; ap->b_transport(tx,delay);
        check(tx.get_response_status()==tlm::TLM_COMMAND_ERROR_RESPONSE,"ignore command rejected");
        tx.set_command(tlm::TLM_WRITE_COMMAND);
        check(ap->transport_dbg(tx)==0,"debug writes rejected");
    }
    void triggers() {
        configure(); wr(0x14,(1u<<8)|6); wait_complete();
        check(memory.records.size()==1 && memory.records[0].data.empty(),"CCR command-only trigger");
        wr(12,63); wr(20,(1u<<8)|(1u<<10)|0x20); ticks(5);
        check(!(rd(8)&64),"CCR address command waits AR");
        wr(24,8); wait_complete(); check(memory.records.size()==2,"AR command-only trigger");
        wr(12,63); wr(16,3); wr(20,ccr(0,2)); wr(24,16); ticks(5);
        check(!(rd(8)&64),"data write waits DR");
        wr(32,0x78563412); check(rd(8)&64,"DR starts data write"); wait_complete();
        check(expected(16)==0x78563412,"LSB first write");
        wr(12,63); wr(20,ccr(1,5,false)); wait_complete();
        check(memory.polls==1,"CCR starts no-address read"); rd(32);
    }
    void indirect() {
        configure();
        for(unsigned n:{1u,2u,3u,4u,5u,31u,32u,33u,129u}) {
            wr(12,63); wr(16,n-1); wr(20,ccr(0,2)); wr(24,128);
            unsigned sent=0;
            while(sent<n) {
                const auto count=std::min(4u,n-sent);
                if (((rd(8)>>8)&63)>32-count) continue;
                wr(32,0x03020100u+0x04040404u*sent); sent+=count;
            }
            wait_complete(); check(!(rd(8)&64),"write idle");
            start_read(128,n); consume(128,n);
        }
        wr(4,7u<<16); wr(16,0xffffffff); wr(20,ccr(1)); wr(24,251);
        consume(251,5); // Undefined DLR stops at FSIZE end, no huge allocation.
    }
    void fifo() {
        configure(reg::TCIE|reg::RXFTIE); start_read(0,129);
        wait(30,SC_US);
        check(((rd(8)>>16)&63)==32 && (rd(8)&64),"RX stalls full");
        const auto first=peek(32); check(first==peek(32),"debug DR non-destructive");
        check(irq.read(),"RX threshold interrupt");
        consume(0,129); ticks(2); check(irq.read(),"TC interrupt");
        wr(0,0x00000001); wr(12,63); ticks(2); check(!irq.read(),"IRQ masks");
        wr(52,8); check(rd(32)==0,"empty RX zero"); ticks(2);
        check(rd(56)==8 && irq.read(),"underrun interrupt"); wr(60,8); ticks(2); check(rd(56)==0 && !irq.read(),"underrun W1C");
        wr(12,63); wr(0,0xfe000001); wr(16,255); wr(20,ccr(0,2)); wr(24,0);
        for(unsigned i=0;i<33;++i) wr(32,i); // First word is in flight; 32 words fill FIFO.
        check(((rd(8)>>8)&63)==32,"TX capacity 32 words");
        wr(52,4); wr(32,0xdeadbeef); ticks(2); check(rd(56)==4 && irq.read(),"TX overflow discard/IRQ");
        wr(60,4); check(rd(56)==0,"TX overflow W1C"); wr(0,0xfe000003);
        check((rd(8)&0x3f00)==0,"abort flushes TX");
    }
    void polling() {
        configure(reg::APMS|reg::SMIE); memory.statuses={0x02,0x80};
        wr(36,0x80); wr(40,0x80); wr(16,0); wr(44,40); wr(20,ccr(2,5,false));
        wait(3,SC_US); check((rd(8)&0x50)==0x10 && irq.read(),"AND match stops and IRQ");
        check(rd(32)==0x80 && memory.polls==2,"last status retained");
        check(!rxreq.read() && !txreq.read(),"no polling DMA");
        wr(12,16); ticks(2); check(!irq.read(),"SMF W1C");
        memory.statuses={1}; memory.polls=0;
        wr(0,0x00000001|reg::APMS|reg::PMM); wr(36,0x80000003); wr(40,3); wr(20,ccr(2,5,false));
        wait(1,SC_US); check((rd(8)&0x50)==0x10,"OR match any considered bit; upper bytes ignored");
        wr(12,63); wr(0,0x00000001); wr(36,0); wr(20,ccr(2,5,false)); wait(1,SC_US);
        check((rd(8)&0x50)==0x50,"APMS=0 keeps polling after match");
        wr(0,0x00000003); check(!(rd(8)&64) && (rd(8)&2),"abort polling TCF");
    }
    void timeout() {
        configure(reg::PTCEN|reg::STOIE); memory.statuses={0};
        wr(36,1); wr(40,1); wr(16,0); wr(48,20); wr(20,ccr(2,5,false));
        wait(1,SC_US);
        check(memory.polls==1 && (rd(8)&0x60)==0x20,"timeout completes in-progress read");
        check(irq.read(),"timeout IRQ"); wr(12,32); ticks(2); check(!irq.read(),"timeout W1C");
        wr(20,ccr(3)); std::uint32_t v=0;
        check(bus(true,false,4,v)==tlm::TLM_OK_RESPONSE,"PTR has no XIP timeout meaning");
        wait(10,SC_US); check(!(rd(8)&32),"no mapped inactivity STOF");
    }
    void dma() {
        configure((1u<<3)|(1u<<8)); wr(52,3); wr(16,11); wr(20,ccr(0,2)); wr(24,0); ticks(10);
        check(txreq.read(),"TX two-word burst eligible");
        txclr=true; ticks(10); check(!txreq.read(),"held TX clear");
        txclr=false; ticks(10); check(txreq.read(),"TX re-evaluated after clear acknowledgement");
        wr(32,1); wr(32,2); ticks(10);
        check(!txreq.read(),"one-word TX residue is below the two-word burst");
        wr(32,3); wait_complete();
        start_read(0,11); wait(3,SC_US); ticks(10); check(rxreq.read(),"RX full burst");
        rd(32); rd(32); rxclr=true; ticks(10); rxclr=false; ticks(10);
        check(!rxreq.read(),"RX residue below burst"); check(rd(32)==3,"CPU drains partial word");
        wr(20,ccr(0,2)); wr(0,3); ticks(10); wr(52,1); wr(16,7); wr(24,32); ticks(10);
        check(!txreq.read(),"AR/DLR do not re-arm DMA after abort");
        wr(20,ccr(0,2)); ticks(10); check(txreq.read(),"accepted CCR re-arms DMA");
        wr(32,0xaabbccdd); wr(32,0x11223344); wait_complete();
        wr(16,0); wr(24,48); ticks(10); check(!txreq.read(),"shorter-than-burst TX residue uses CPU");
        wr(32,0x55); wait_complete();
    }
    void xip() {
        configure(); wr(16,0xffffffff); wr(20,ccr(3));
        for(unsigned a:{0u,4u,124u,0x1fffcu}) {
            std::uint32_t v=0; check(bus(true,false,a,v)==tlm::TLM_OK_RESPONSE && v==expected(a),"XIP bytes");
        }
        check(rd(32)==0 && !(rd(8)&2),"XIP no DR/TCF");
        memory.fail=true; std::uint32_t bad=0;
        check(bus(true,false,0,bad)==tlm::TLM_GENERIC_ERROR_RESPONSE && !(rd(8)&1),"XIP target error without TEF");
        memory.fail=false;
        wr(24,0xffffffff); check(rd(24)==0,"mapped AR writes ignored");
        std::uint32_t v=0; check(bus(true,false,0x20000,v)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"FSIZE XIP limit");
        wr(4,7u<<16);
        check(bus(true,false,252,v)==tlm::TLM_OK_RESPONSE && v==expected(252),"last word inside FSIZE");
        check(bus(true,false,256,v)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"controller FSIZE smaller than A2H aperture");
        wr(4,16u<<16);
        check(!(rd(8)&1),"mapped errors do not set indirect TEF");
        check(bus(true,false,2,v)==tlm::TLM_OK_RESPONSE && v==expected(0),"XIP ignores low address bits");
        wr(20,ccr(1)); check(bus(true,false,0,v)==tlm::TLM_GENERIC_ERROR_RESPONSE,"wrong mode XIP");
    }
    void phases() {
        configure(); wr(0,0x02000001); wr(4,(16u<<16)|(3u<<8)|1); wr(28,0xa55a);
        const auto command=(3u<<26)|(3u<<24)|(7u<<18)|(1u<<16)|(2u<<14)|(2u<<12)|(3u<<10)|(1u<<8)|0xeb|reg::DDRM|reg::SIOO;
        wr(20,command); std::uint32_t v=0;
        check(bus(true,false,32,v)==tlm::TLM_OK_RESPONSE,"DDR quad XIP");
        check(bus(true,false,36,v)==tlm::TLM_OK_RESPONSE,"SIOO repeated XIP");
        const auto a=memory.records[0].phase,b=memory.records[1].phase;
        check(a.instruction_lines==1 && b.instruction_lines==0,"SIOO first instruction only");
        check(a.address_lines==4 && a.address_bytes==3 && a.alternate_lines==2 && a.alternate_bytes==2,"phase widths and sizes");
        check(a.alternate==0xa55a && a.dummy_cycles==7 && a.data_lines==4 && a.ddr && a.clock_mode3,"all phase metadata");
        check(a.begin && a.end && a.offset==0 && a.length==4,"frame boundaries");
        wr(20,command); check(bus(true,false,40,v)==tlm::TLM_OK_RESPONSE,"SIOO rearm");
        check(memory.records.back().phase.instruction_lines==1,"CCR re-arms instruction");
        // Metadata and timing independently exercise all legal lane widths,
        // address/alternate sizes, and both SDR/DDR rates.
        for(unsigned ddr=0;ddr<2;++ddr) for(unsigned lanes=1;lanes<=3;++lanes)
            for(unsigned bytes=1;bytes<=4;++bytes) {
                wr(0,ddr?0x02000001:0x00000001);
                wr(20,(ddr?reg::DDRM:0)|(3u<<26)|(lanes<<24)|(lanes<<14)|
                    ((bytes-1)<<16)|((bytes-1)<<12)|(lanes<<10)|(lanes<<8)|0x03);
                const auto start=sc_time_stamp();
                check(bus(true,false,0,v)==tlm::TLM_OK_RESPONSE,"phase combination");
                const auto& phase=memory.records.back().phase;
                check(phase.instruction_lines==(1u<<(lanes-1)) && phase.address_lines==phase.instruction_lines &&
                    phase.alternate_lines==phase.instruction_lines && phase.data_lines==phase.instruction_lines &&
                    phase.address_bytes==bytes && phase.alternate_bytes==bytes && phase.ddr==bool(ddr),"phase combination descriptor");
                const unsigned divisor=ddr?4:2, width=1u<<(lanes-1);
                const unsigned expected_cycles=8/width*divisor+(bytes*16+32)/width*divisor/(ddr?2:1);
                check(sc_time_stamp()-start>=sc_time(expected_cycles*10,SC_NS),"serial phase duration lower bound");
            }
    }
    void errors() {
        configure(); wr(0,0x01000001); wr(20,(1u<<8)|6); check(rd(8)&1,"illegal SDR prescaler");
        wr(12,63); wr(0,0x00000001); wr(20,reg::DDRM|(1u<<8)|6); check(rd(8)&1,"illegal DDR prescaler");
        wr(12,63); wr(16,4); wr(20,ccr(2,5,false)); check(rd(8)&1,"poll length >4 illegal");
        wr(12,63); wr(16,7); wr(20,ccr(1)); wr(24,0x1fffc); check(rd(8)&1,"indirect bound check");
        wr(12,63); wr(16,0); wr(20,ccr(1)&~(3u<<12)); wr(24,256); consume(0,1); check(!(rd(8)&1),"address phase transmits low selected bytes");
        wr(12,63); memory.fail=true; start_read(0,4); wait(2,SC_US);
        check((rd(8)&0x43)==1,"backend error TEF without TCF or BUSY");
        memory.fail=false; wr(12,63); start_read(0,4);
        const auto cr=rd(0); wr(0,0xff401ffd); check((rd(0)&~0x3f0003u)==(cr&~0x3f0003u),"busy CR fields frozen");
        for(unsigned a:{4u,16u,20u,24u,28u,36u,40u,44u,48u}) { const auto old=rd(a); wr(a,0xdeadbeef); check(rd(a)==old,"busy config ignored"); }
        consume(0,4);
    }
    void reset_cases() {
        configure(); start_read(0,64); wait(2,SC_US); rn=false; wait(1,SC_NS);
        for(unsigned a=0;a<64;a+=4) check(peek(a)==0,"async core reset registers");
        check(!irq.read() && !rxreq.read() && !txreq.read(),"reset outputs");
        rn=true; ticks(2); configure(); wr(20,ccr(3));
        worker_start.notify(SC_ZERO_TIME); wait(100,SC_NS); hrn=false; wait(1,SC_NS); hrn=true;
        wait(100,SC_NS); check(worker_done && worker_result==tlm::TLM_GENERIC_ERROR_RESPONSE,"hreset cancels mapped transfer");
        check(rd(0)==0x00000001 && rd(20)==ccr(3),"interface reset preserves core config");
        std::uint32_t v=0; check(bus(true,false,0,v)==tlm::TLM_OK_RESPONSE,"XIP recovers");
        prn=false; ticks(); check(bus(false,true,4,v)==tlm::TLM_GENERIC_ERROR_RESPONSE,"AP accesses reject held reset");
        prn=true; ticks(2); check(rd(4)==16u<<16,"AP reset preserves core config");
    }
    void clock_cases() {
        configure(); clock_enabled=false; wait(20,SC_NS); workers_ap=true;
        worker_start.notify(SC_ZERO_TIME); wait(5000,SC_NS);
        check(!worker_done && peek(28)==0,"APB has no stopped-core timeout");
        clock_enabled=true; ticks(10); check(worker_done && worker_result==tlm::TLM_OK_RESPONSE,"APB resumes with core");
        check(rd(28)==0x11223344,"pending APB write applied exactly once");
        wr(20,ccr(3)); memory.latency=sc_time(10,SC_US); std::uint32_t v=0;
        check(bus(true,false,0,v)==tlm::TLM_GENERIC_ERROR_RESPONSE,"AHB HCLK timeout");
        memory.latency=SC_ZERO_TIME;
        check(bus(true,false,4,v)==tlm::TLM_OK_RESPONSE,"no stale timed-out completion");
    }
    void concurrency() {
        configure(); wr(20,ccr(3));
        worker_start.notify(SC_ZERO_TIME); worker2_start.notify(sc_time(20,SC_NS));
        wait(3,SC_US); check(worker_done && worker2_done && worker_result==tlm::TLM_OK_RESPONSE && worker2_result==tlm::TLM_OK_RESPONSE,"serialize two XIP callers");
        check(memory.records.size()==2 && memory.records[0].phase.address==0 && memory.records[1].phase.address==4,"ordered frames");
        worker_done=worker2_done=false; wr(20,ccr(3));
        worker_start.notify(SC_ZERO_TIME); worker2_start.notify(sc_time(20,SC_NS));
        wait(100,SC_NS); rn=false; hrn=false; prn=false; wait(1,SC_NS); rn=true; hrn=true; prn=true; wait(100,SC_NS);
        check(worker_done && worker2_done && worker_result==tlm::TLM_GENERIC_ERROR_RESPONSE && worker2_result==tlm::TLM_GENERIC_ERROR_RESPONSE,"reset invalidates active and queued requests");
        worker_done=worker2_done=false; workers_ap=true; clock_enabled=false; wait(20,SC_NS);
        worker_start.notify(SC_ZERO_TIME); worker2_start.notify(sc_time(20,SC_NS));
        wait(100,SC_NS); prn=false; wait(1,SC_NS); prn=true; wait(100,SC_NS);
        check(worker_done && worker2_done && worker_result==tlm::TLM_GENERIC_ERROR_RESPONSE && worker2_result==tlm::TLM_GENERIC_ERROR_RESPONSE,"AP reset invalidates active and queued writes with stopped core");
        clock_enabled=true; ticks(3); check(peek(28)==0,"cancelled AP writes never applied");
    }
    void random_test() {
        configure(); std::mt19937 rng(0x51535049);
        for(unsigned i=0;i<100;++i) {
            const unsigned address=rng()%0x1ff00, length=1+rng()%96;
            start_read(address,length); consume(address,length);
            wr(20,ccr(3)); std::uint32_t v=0; const unsigned x=rng()%0x8000*4;
            check(bus(true,false,x,v)==tlm::TLM_OK_RESPONSE && v==expected(x),"random XIP");
        }
    }
    void components() {
        QspiRegisterModel registers;
        check(registers.write(4,0xffffffff,false) && registers.read(4)==0x001f0701,"register masks");
        check(!registers.write(4,0,true),"BUSY lock");
        QspiTxRxFifo fifo;
        for (unsigned i=0;i<32;++i) check(fifo.tx.push_word(0x78563412,4),"32-word capacity");
        check(!fifo.tx.push_word(0,4) && fifo.levels()==(32u<<8),"word level and full rejection");
        check(!fifo.tx_threshold(0),"full TX has no free word");
        fifo.reset(); check(fifo.tx_threshold(31),"32 free words");
        QspiInterrupt interrupt;
        for(unsigned source=0;source<6;++source) {
            check(interrupt.evaluate(1u<<(16+source),1u<<source,0,0),"each IRQ source");
            check(!interrupt.evaluate(1u<<(16+((source+1)%6)),1u<<source,0,0),"IRQ mask isolation");
        }
        check(interrupt.evaluate(0,0,4,4) && interrupt.evaluate(0,0,8,8),"DMA error IRQ independent of DMA enable");
        check(QspiTimingModel::phase_cycles(3,4,true,0x02000000)==12,"DDR address duration");
        check(QspiTimingModel::cs_high(0,0x700)==16,"CSHT duration");
        check(QspiTimingModel::poll_gap(0,0x700,40)==40,"poll interval");
        check(QspiTimingModel::legal_divider(0,false) && !QspiTimingModel::legal_divider(0,true) &&
              QspiTimingModel::legal_divider(0x02000000,true),"PRESCALER + 2 legality");
    }
    void threshold_words() {
        configure(3u<<3); start_read(0,12); wait(5,SC_US);
        check(((rd(8)>>16)&63)==3 && !(rd(8)&8),"word RX threshold does not fire at 12 bytes for four words");
        consume(0,12);
        wr(0,0x00000001|(2u<<3)); start_read(0,12); wait(5,SC_US);
        check(rd(8)&8,"word RX threshold fires at three words"); consume(0,12);
        wr(0,0x00000001|(8u<<8)); wr(20,ccr(0,2)); wr(12,63); ticks(2);
        check(rd(8)&4,"nine-word TX threshold fits 32-word FIFO");
    }
    void spec_timing() {
        // HDS 6.4.2: PIR and CSHT minima both apply.
        // Independent timestamps come from complete one-byte status frames.
        for (const auto interval:{500u,1u}) {
            configure(reg::APMS);
            wr(4,(16u<<16)|(7u<<8)); // Eight SCLK cycles = 16 core cycles.
            wr(12,63); wr(16,0); wr(36,1); wr(40,1); wr(44,interval);
            memory.statuses={0,1}; memory.polls=0; memory.records.clear();
            wr(20,ccr(2,5,false));
            bool stopped=false;
            for (unsigned tries=0;tries<10000;++tries) {
                if (!(rd(8)&64)) { stopped=true; break; }
            }
            check(stopped && memory.records.size()==2,"two polls then APMS stop");
            check((rd(8)&16)!=0 && rd(32)==1,"status match and last status");
            // Command 8 SDR bits + data 8 SDR bits, divisor 2, core 10 ns.
            const auto serial=sc_time(320,SC_NS);
            const auto high=sc_time(160,SC_NS);
            const auto pir=sc_time(interval*10,SC_NS);
            const auto gap=pir>high?pir:high;
            check(memory.records[1].time-memory.records[0].time>=gap+serial,
                "HDS polling interval and CSHT minima respected");
        }
    }
    void run() {
        try {
            reset();
            if(scenario=="registers") registers(); else if(scenario=="protocol") protocol();
            else if(scenario=="triggers") triggers(); else if(scenario=="indirect") indirect();
            else if(scenario=="fifo") fifo(); else if(scenario=="polling") polling();
            else if(scenario=="timeout") timeout(); else if(scenario=="dma") dma();
            else if(scenario=="xip") xip(); else if(scenario=="phases") phases();
            else if(scenario=="errors") errors(); else if(scenario=="reset") reset_cases();
            else if(scenario=="clock") clock_cases(); else if(scenario=="concurrency") concurrency();
            else if(scenario=="random") random_test(); else if(scenario=="components") components();
            else if(scenario=="threshold_words") threshold_words(); else if(scenario=="spec_timing") spec_timing();
            else check(false,"unknown scenario");
            std::cout<<"PASS "<<scenario<<" assertions="<<assertions<<" time="<<sc_time_stamp()<<'\n';
        } catch(const std::exception& e) { failed=true; std::cerr<<"FAIL "<<scenario<<": "<<e.what()<<'\n'; }
        sc_stop();
    }
};
}
int sc_main(int argc,char** argv) {
    Bench b{"bench",argc>1?argv[1]:"registers"};
    sc_start(sc_time(50,SC_MS));
    if(!sc_end_of_simulation_invoked()) { std::cerr<<"simulation watchdog expired\n"; return 2; }
    return b.failed?1:0;
}
