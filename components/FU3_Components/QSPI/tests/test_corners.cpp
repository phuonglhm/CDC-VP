#include "fu3/qspi/qspi_controller.h"
#include "fu3/qspi/qspi_ahb_adapter.h"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace sc_core;
using namespace fu3::qspi;
namespace {
struct Record {
    FlashTransaction phase;
    tlm::tlm_command command;
    std::uint64_t address;
    std::vector<unsigned char> bytes;
    sc_time time;
};
// Virtual 4-GiB address space, so DLR/FSIZE boundaries never allocate gigabytes.
// Expected packing is checked against host byte vectors, not FIFO internals.
struct Target : sc_module {
    tlm_utils::simple_target_socket<Target> socket{"socket"};
    std::vector<Record> records;
    std::map<std::uint64_t,unsigned char> written;
    std::vector<std::uint32_t> statuses{0};
    unsigned polls=0;
    tlm::tlm_response_status response=tlm::TLM_OK_RESPONSE;
    bool throws=false;
    sc_time latency=SC_ZERO_TIME;
    explicit Target(sc_module_name n):sc_module(n) { socket.register_b_transport(this,&Target::transfer); }
    static unsigned char pattern(std::uint64_t a) { return static_cast<unsigned char>((a*29)^(a>>8)^0x96); }
    unsigned char byte(std::uint64_t a) const {
        const auto found=written.find(a); return found==written.end()?pattern(a):found->second;
    }
    void transfer(tlm::tlm_generic_payload& tx,sc_time& delay) {
        auto* m=tx.get_extension<FlashTransaction>();
        if (!m) throw std::runtime_error("missing flash extension");
        const auto n=tx.get_data_length();
        if (n>4) throw std::runtime_error("unbounded backend chunk");
        if (!m->cancel && throws) throw std::runtime_error("injected backend exception");
        if (!m->cancel && response!=tlm::TLM_OK_RESPONSE) { tx.set_response_status(response); return; }
        if (!m->cancel) {
            if (tx.is_read() && m->instruction==5) {
                const auto status=statuses[std::min<std::size_t>(polls++,statuses.size()-1)];
                for (unsigned i=0;i<n;++i) tx.get_data_ptr()[i]=static_cast<unsigned char>(status>>(8*i));
            } else for (unsigned i=0;i<n;++i) {
                if (tx.is_read()) tx.get_data_ptr()[i]=byte(tx.get_address()+i);
                if (tx.is_write()) written[tx.get_address()+i]=tx.get_data_ptr()[i];
            }
            delay+=latency;
        }
        std::vector<unsigned char> bytes;
        if (n) bytes.assign(tx.get_data_ptr(),tx.get_data_ptr()+n);
        records.push_back({*m,tx.get_command(),tx.get_address(),bytes,sc_time_stamp()});
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};
struct Reply { tlm::tlm_response_status status; std::uint32_t value; sc_time delay; };
struct Job {
    bool mapped=true,write=false,done=false;
    std::uint64_t address=0;
    std::uint32_t value=0;
    sc_time annotation=SC_ZERO_TIME;
    Reply reply{tlm::TLM_INCOMPLETE_RESPONSE,0,SC_ZERO_TIME};
    sc_time ended=SC_ZERO_TIME;
};
struct Bench : sc_module {
    Config config;
    sc_time core_period,p_period,h_period;
    sc_signal<bool> ck{"ck"},pc{"pc"},hc{"hc"},dc{"dc"};
    sc_signal<bool> rn{"rn"},prn{"prn"},hrn{"hrn"};
    sc_signal<bool> rxclr{"rxclr"},txclr{"txclr"},irq{"irq"},rxreq{"rxreq"},txreq{"txreq"};
    QspiController dut;
    QspiAhbAdapter bridge{"bridge"};
    Target target{"target"};
    tlm_utils::simple_initiator_socket<Bench> ap{"ap"},mm{"mm"};
    bool core_on=true,p_on=true,h_on=true,d_on=true,passed=false;
    std::string scenario;
    unsigned assertions=0;
    std::array<Job,2> jobs;
    sc_event go0,go1;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name n,std::string test,unsigned depth,unsigned rx_depth,unsigned cache_words=0):sc_module(n),
        config{depth,rx_depth,test=="hds_cache" || cache_words?4000u:(test=="hds_timeout"?64u:200u),
            cache_words?cache_words:(depth==4 && rx_depth==4?1u:(depth==8?4u:8u)),
            cache_words?cache_words:(depth==4 && rx_depth==4?1u:(depth==8?4u:8u)),
            depth==4 && rx_depth==4?2u:4u,depth==4 && rx_depth==4?2u:8u},
        core_period(depth==32?10:6+(depth%5)*2,SC_NS),
        p_period(depth==32?14:11+(depth%4)*4,SC_NS),
        h_period(depth==32?22:7+(depth%7)*4,SC_NS),
        dut("dut",config),scenario(std::move(test)) {
        dut.clk(ck); dut.pclk(pc); dut.hclk(hc); dut.dma_clk(dc);
        dut.reset_n(rn); dut.preset_n(prn); dut.hreset_n(hrn); dut.dma_reset_n(rn);
        dut.dma_rx_clr(rxclr); dut.dma_tx_clr(txclr);
        dut.irq(irq); dut.dma_rx_req(rxreq); dut.dma_tx_req(txreq);
        ap.bind(dut.ap_qspic); mm.bind(bridge.socket); bridge.ahb.bind(dut.xip); dut.flash.bind(target.socket);
        SC_THREAD(core_clock); SC_THREAD(p_clock); SC_THREAD(h_clock); SC_THREAD(d_clock);
        SC_THREAD(worker0); SC_THREAD(worker1); SC_THREAD(run);
    }
    void core_clock() { for (;;) { ck=false; wait(core_period/2); ck=core_on; wait(core_period/2); } }
    void p_clock() { for (;;) { pc=false; wait(p_period/2); pc=p_on; wait(p_period/2); } }
    void h_clock() { for (;;) { hc=false; wait(h_period/2); hc=h_on; wait(h_period/2); } }
    void d_clock() { for (;;) { dc=false; wait(9,SC_NS); dc=d_on; wait(9,SC_NS); } }
    void check(bool ok,const std::string& what) {
        ++assertions; if (!ok) throw std::runtime_error(what+" at "+sc_time_stamp().to_string());
    }
    void settle() { wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); }
    void tick(unsigned n=1) { for (unsigned i=0;i<n;++i) wait(ck.posedge_event()); settle(); }
    void until(const std::function<bool()>& predicate,const char* message,unsigned limit=20000) {
        for (unsigned i=0;i<limit;++i) { if (predicate()) return; tick(); }
        check(false,message);
    }
    Reply access(bool mapped,bool write,std::uint64_t address,std::uint32_t value=0,
                 unsigned n=4,unsigned width=4,unsigned char* enables=nullptr,unsigned enable_length=0,
                 bool null=false,int command=-1,sc_time annotation=SC_ZERO_TIME,
                 AhbTransaction::Transfer transfer=AhbTransaction::Nonseq,bool selected=true) {
        std::array<unsigned char,8> data{};
        for (unsigned i=0;i<4;++i) data[i]=static_cast<unsigned char>(value>>(8*i));
        tlm::tlm_generic_payload tx;
        const auto cmd=command<0?(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND):static_cast<tlm::tlm_command>(command);
        tx.set_command(cmd); tx.set_address(address); tx.set_data_ptr(null?nullptr:data.data());
        tx.set_data_length(n); tx.set_streaming_width(width);
        tx.set_byte_enable_ptr(enables); tx.set_byte_enable_length(enable_length); tx.set_dmi_allowed(true);
        AhbTransaction control; control.transfer=transfer; control.selected=selected;
        if (mapped) tx.set_extension(&control);
        if (mapped) mm->b_transport(tx,annotation); else ap->b_transport(tx,annotation);
        tx.clear_extension<AhbTransaction>();
        check(!tx.is_dmi_allowed(),"DMI disabled on every response");
        check(tx.get_address()==address && tx.get_command()==cmd && tx.get_data_length()==n &&
              tx.get_streaming_width()==width && tx.get_data_ptr()==(null?nullptr:data.data()) &&
              tx.get_byte_enable_ptr()==enables && tx.get_byte_enable_length()==enable_length,"payload fields preserved");
        std::uint32_t result=0;
        for (unsigned i=0;i<4;++i) result|=std::uint32_t(data[i])<<(8*i);
        return {tx.get_response_status(),result,annotation};
    }
    void wr(unsigned offset,std::uint32_t value) { check(access(false,true,offset,value).status==tlm::TLM_OK_RESPONSE,"AP write"); }
    std::uint32_t rd(unsigned offset) {
        const auto result=access(false,false,offset); check(result.status==tlm::TLM_OK_RESPONSE,"AP read"); return result.value;
    }
    std::uint32_t peek(unsigned offset) {
        unsigned char data[4]{}; tlm::tlm_generic_payload tx;
        tx.set_command(tlm::TLM_READ_COMMAND); tx.set_address(offset); tx.set_data_ptr(data);
        tx.set_data_length(4); tx.set_streaming_width(4);
        check(ap->transport_dbg(tx)==4,"debug snapshot");
        std::uint32_t value=0; for (unsigned i=0;i<4;++i) value|=std::uint32_t(data[i])<<(8*i); return value;
    }
    void reset() {
        core_on=p_on=h_on=d_on=true; rn=false; prn=false; hrn=false; rxclr=false; txclr=false;
        wait(31,SC_NS); rn=true; prn=true; hrn=true; tick(2);
        target.latency=SC_ZERO_TIME; target.response=tlm::TLM_OK_RESPONSE; target.throws=false;
        target.records.clear(); target.written.clear(); target.statuses={0}; target.polls=0;
    }
    void setup(std::uint32_t bits=0,unsigned fsize=16,unsigned prescaler=0) {
        wr(reg::DCR,fsize<<16); wr(reg::CR,(prescaler<<24)|reg::EN|bits);
    }
    static std::uint32_t command(unsigned mode,unsigned ad=1,unsigned data=1,unsigned instruction=3) {
        return (mode<<26)|(data<<24)|(2u<<12)|(ad<<10)|(1u<<8)|instruction;
    }
    void read_start(std::uint32_t address,std::uint32_t dlr) {
        wr(reg::FCR,63); wr(reg::DLR,dlr); wr(reg::CCR,command(1)); wr(reg::AR,address);
    }
    void done() { until([&]{ return (peek(reg::SR)&reg::TCF)!=0; },"TCF timeout",100000); check(!(peek(reg::SR)&reg::TEF),"no transfer error"); }
    void abort() { wr(reg::CR,peek(reg::CR)|reg::ABORT); until([&]{return !(peek(reg::CR)&reg::ABORT);},"abort completion"); check(!(peek(reg::CR)&reg::ABORT) && !(peek(reg::SR)&reg::BUSY),"abort self clears and ends BUSY"); }
    void job(unsigned index) {
        auto& j=jobs[index]; j.reply=access(j.mapped,j.write,j.address,j.value,4,4,nullptr,0,false,-1,j.annotation);
        j.ended=sc_time_stamp(); j.done=true;
    }
    void worker0() { for (;;) { wait(go0); job(0); } }
    void worker1() { for (;;) { wait(go1); job(1); } }
    void launch(unsigned index,bool mapped,bool write,std::uint64_t address,std::uint32_t value=0,sc_time delay=SC_ZERO_TIME) {
        jobs[index]=Job{}; jobs[index].mapped=mapped; jobs[index].write=write;
        jobs[index].address=address; jobs[index].value=value; jobs[index].annotation=delay;
        (index?go1:go0).notify(SC_ZERO_TIME); settle();
    }
    void join(unsigned index) { until([&]{return jobs[index].done;},"worker timeout"); }
    static std::uint32_t word(const std::vector<unsigned char>& bytes,unsigned offset,unsigned n) {
        std::uint32_t value=0; for (unsigned i=0;i<n;++i) value|=std::uint32_t(bytes[offset+i])<<(8*i); return value;
    }
    void drain(const std::vector<unsigned char>& expected) {
        unsigned offset=0;
        while (offset<expected.size()) {
            until([&]{ return (peek(reg::SR)>>16)!=0; },"RX data timeout");
            const unsigned available=(peek(reg::SR)>>16)&63;
            const unsigned n=std::min(4u,static_cast<unsigned>(expected.size()-offset));
            check(available>0,"at least one RX word");
            check(offset+n<=expected.size(),"RX never supplies extra bytes");
            check(rd(reg::DR)==word(expected,offset,n),"independent RX packing oracle"); offset+=n;
        }
        done(); check(!(peek(reg::SR)&reg::BUSY),"RX drained clears BUSY");
    }
    void audit(std::uint64_t length,std::uint32_t address,tlm::tlm_command cmd) {
        std::uint64_t offset=0,id=0; unsigned begins=0,ends=0;
        for (const auto& r:target.records) {
            check(!r.phase.cancel && r.phase.offset==offset && r.phase.length==length,"frame offset/length integrity");
            if (!id) id=r.phase.frame;
            check(r.phase.frame==id && r.command==cmd && r.address==std::uint64_t(address)+offset,"frame identity/direction/address");
            check(r.bytes.size()<=4,"bounded chunk size");
            begins+=r.phase.begin; ends+=r.phase.end;
            check(r.phase.begin==(offset==0) && r.phase.end==(offset+r.bytes.size()==length),"one begin/end at exact boundaries");
            if (offset) check(r.phase.instruction_lines==0,"instruction not repeated between chunks");
            offset+=r.bytes.size();
        }
        check(offset==length && begins==1 && ends==1,"exact frame byte count");
    }
    void trigger_combinations();
    void dlr_boundaries();
    void fifo_packing();
    void irq_w1c();
    void active_reset();
    void bus_errors();
    void polling_edges();
    void dma_edges();
    void domains();
    void parameters();
    void hds_cache();
    void hds_control();
    void hds_dma_clock();
    void hds_flags();
    void hds_timeout();
    void hds_abort();
    void hds_frame_matrix();
    void hds_dividers();
    void hds_parameters();
    void hds_cache_parameters();
    void run();
};

void Bench::trigger_combinations() {
    // HDS 6.4 / Table 6-8, including EN and absent instruction.
    for (unsigned enabled=0;enabled<2;++enabled) for (unsigned mode=0;mode<4;++mode)
        for (unsigned ad=0;ad<4;++ad) for (unsigned data=0;data<4;++data) for (unsigned im=0;im<2;++im) {
            reset(); wr(reg::DCR,16u<<16); wr(reg::DLR,0); wr(reg::PSMKR,1); wr(reg::PSMAR,0);
            wr(reg::CR,0x00400000|enabled);
            // Alternate phase keeps IMODE=0/no-address/no-data a nonempty frame.
            const auto c=(mode<<26)|(data<<24)|(2u<<12)|(ad<<10)|(im<<8)|(im?0:1u<<14)|5;
            wr(reg::CCR,c);
            const bool invalid_poll=enabled && mode==2 && !data;
            auto verify=[&](bool starts) {
                check(bool(peek(reg::SR)&reg::BUSY)==(starts&&!invalid_poll),"trigger-stage BUSY truth table");
                check(bool(peek(reg::SR)&reg::TEF)==(starts&&invalid_poll),"poll-without-data legality at trigger");
            };
            bool started=enabled && ad==0 && ((mode==0 && data==0) || mode==1 || mode==2);
            verify(started);
            if (!started) {
                wr(reg::AR,16);
                started=enabled && ad!=0 && ((mode==0 && data==0) || mode==1 || mode==2);
                verify(started);
            }
            if (!started) {
                wr(reg::DR,0x12345678);
                started=enabled && mode==0 && data!=0; verify(started);
            }
            if (started && !invalid_poll) {
                until([&]{return target.records.size()==1;},"trigger did not issue a frame");
                check(target.records.front().phase.instruction_lines==im,"instruction phase independent of trigger");
                check(target.records.front().phase.address_lines==(ad?(1u<<(ad-1)):0),"address mode reaches backend");
                if (mode==1 && data) rd(reg::DR);
                tick(3); check(target.records.size()==1,"one trigger issues exactly one frame");
            } else {
                tick(100); check(target.records.empty(),"disabled/mapped/nontrigger writes never issue commands");
            }
        }
}

void Bench::dlr_boundaries() {
    // HDS 6.4.1 / DEC-013: finite and undefined lengths without 32-bit wrap.
    for (unsigned n:{1u,2u,3u,4u,5u,31u,32u,33u}) {
        reset(); setup(); read_start(128,n-1);
        std::vector<unsigned char> expected(n); for (unsigned i=0;i<n;++i) expected[i]=Target::pattern(128+i);
        drain(expected); audit(n,128,tlm::TLM_READ_COMMAND);
    }
    for (auto dlr:{0xfffffffeu,0xffffffffu}) {
        reset(); setup(); read_start(0,dlr);
        if (dlr==0xfffffffeu) check((peek(reg::SR)&reg::TEF) && target.records.empty(),"finite huge DLR rejected without allocation");
        else {
            until([&]{return !target.records.empty();},"undefined DLR started");
            check(target.records[0].phase.length==0x20000,"undefined length follows FSIZE"); abort();
        }
    }
    reset(); setup(0,0); read_start(0,1);
    drain({Target::pattern(0),Target::pattern(1)}); // Minimum FSIZE is two bytes.
    wr(reg::FCR,63); wr(reg::DLR,2); wr(reg::AR,0);
    check(peek(reg::SR)&reg::TEF,"one byte beyond smallest FSIZE");
    for (auto dlr:{3u,4u}) {
        reset(); setup(0,31); wr(reg::DLR,dlr); wr(reg::CCR,command(1)|(3u<<12)); wr(reg::AR,0xfffffffc);
        if (dlr==4) check((peek(reg::SR)&reg::TEF) && target.records.empty(),"4-GiB FSIZE boundary rejects overflow");
        else {
            std::vector<unsigned char> expected(4); for(unsigned i=0;i<4;++i) expected[i]=Target::pattern(0xfffffffcULL+i);
            drain(expected); audit(4,0xfffffffc,tlm::TLM_READ_COMMAND);
        }
    }
    for (auto dlr:{0xfffffffeu,0xffffffffu}) {
        reset(); setup(0,31); wr(reg::DLR,dlr); wr(reg::CCR,command(1)|(3u<<12)); wr(reg::AR,0);
        until([&]{return !target.records.empty();},"large valid length starts bounded");
        check(target.records[0].phase.length==(dlr==0xffffffffu?0x100000000ULL:0xffffffffULL),"DLR length retained in 64 bits");
        abort(); check(target.records.size()==2 && target.records.back().phase.cancel,"large streaming frame cancels cleanly");
    }
}

void Bench::fifo_packing() {
    for (unsigned n:{1u,2u,3u,4u,5u,7u,31u,32u,33u,63u,64u,65u}) {
        reset(); setup(); std::vector<unsigned char> bytes(n);
        for(unsigned i=0;i<n;++i) bytes[i]=static_cast<unsigned char>(i*13+7);
        wr(reg::DLR,n-1); wr(reg::CCR,command(0,1,1,2)); wr(reg::AR,256);
        for(unsigned i=0;i<n;i+=4) {
            const auto count=std::min(4u,n-i);
            until([&]{return ((peek(reg::SR)>>8)&63)<32;},"TX space");
            wr(reg::DR,word(bytes,i,count));
        }
        done(); audit(n,256,tlm::TLM_WRITE_COMMAND);
        for(unsigned i=0;i<n;++i) check(target.written.at(256+i)==bytes[i],"TX bytes independently match host buffer");
        check(target.written.count(256+n)==0,"no high-lane bytes beyond DLR");
        target.records.clear(); read_start(256,n-1); drain(bytes); audit(n,256,tlm::TLM_READ_COMMAND);
    }
    reset(); setup(); read_start(0,128); until([&]{return ((peek(reg::SR)>>16)&63)==32;},"RX full");
    check(target.records.size()==32,"exact 32-word RX capacity"); tick(200);
    check(target.records.size()==32 && !(peek(reg::SR)&reg::TCF),"129th byte stalls without drop or early TCF");
    std::vector<unsigned char> expected(129); for(unsigned i=0;i<129;++i) expected[i]=Target::pattern(i);
    drain(expected); audit(129,0,tlm::TLM_READ_COMMAND);
    reset(); setup(0,16,62); wr(reg::DLR,135); wr(reg::CCR,command(0,1,1,2)); wr(reg::AR,0);
    for(unsigned i=0;i<33;++i) wr(reg::DR,0x01010101u*i);
    check(((peek(reg::SR)>>8)&63)==32,"TX full with first word in flight");
    wr(reg::DR,0xdeadbeef); check(peek(reg::DMASR)&reg::TXOVRF,"full TX write flags overrun");
    until([&]{return ((peek(reg::SR)>>8)&63)<=31;},"TX resumes",100000);
    wr(reg::DR,0xfeedface); done();
    std::vector<unsigned char> sent;
    for(const auto& r:target.records) sent.insert(sent.end(),r.bytes.begin(),r.bytes.end());
    check(sent.size()==136 && word(sent,132,4)==0xfeedface,"overrun word discarded; retry keeps packing");
    for(unsigned i=0;i<33;++i) check(word(sent,4*i,4)==0x01010101u*i,"full FIFO ordering preserved");
}

void Bench::irq_w1c() {
    // HDS 6.4.4: threshold flags reassert if the condition remains after W1C.
    setup(reg::RXFTIE|reg::TCIE); read_start(0,3); done(); tick();
    check(irq.read() && (peek(reg::SR)&10)==10,"simultaneous RXFTF and TCF");
    wr(reg::FCR,0); check((peek(reg::SR)&10)==10 && irq.read(),"writing zero preserves flags");
    wr(reg::FCR,reg::TCF); check(!(peek(reg::SR)&reg::TCF) && irq.read(),"clearing TCF leaves threshold IRQ");
    wr(reg::FCR,reg::RXFTF); settle();
    check(!(peek(reg::SR)&reg::RXFTF) && !irq.read(),"RX threshold W1C observed before next edge");
    tick(); check((peek(reg::SR)&reg::RXFTF) && irq.read(),"RX condition reasserts on subsequent edge");
    wr(reg::CR,0x00000001); check(!irq.read() && (peek(reg::SR)&reg::RXFTF),"masking IRQ preserves flag");
    wr(reg::CR,0x00000001|reg::RXFTIE); check(irq.read(),"unmask pending threshold immediately");
    rd(reg::DR); wr(reg::FCR,reg::RXFTF); tick(3);
    check(!(peek(reg::SR)&reg::RXFTF) && !irq.read(),"draining condition prevents reassertion");
    wr(reg::DMACR,8); rd(reg::DR); check(irq.read() && peek(reg::DMASR)==8,"empty DR read sets underrun");
    wr(reg::FCR,63); check(peek(reg::DMASR)==8 && irq.read(),"FCR does not clear DMA flags");
    wr(reg::DMAFCR,0); check(peek(reg::DMASR)==8,"DMA W1C zero ignored");
    wr(reg::DMAFCR,8); tick(5); check(!irq.read() && peek(reg::DMASR)==0,"underrun does not recur without another DR read");
    rd(reg::DR); check(irq.read(),"new empty read produces a new underrun");
    reset(); setup(reg::TXFTIE|(31u<<8)); tick();
    check(irq.read() && (peek(reg::SR)&reg::TXFTF),"empty TX threshold 32 free words");
    wr(reg::FCR,reg::TXFTF); settle(); check(!irq.read(),"TX W1C lowers IRQ for cleared flag");
    tick(); check(irq.read(),"unchanged TX free-space condition reasserts");
    reset(); setup(reg::SMIE); target.statuses={0}; wr(reg::DLR,0); wr(reg::PSMKR,1); wr(reg::PSMAR,0); wr(reg::PIR,1000);
    wr(reg::CCR,command(2,0,1,5)); until([&]{return target.polls==1;},"first matching poll"); tick();
    check(irq.read(),"status match IRQ while APMS disabled");
    wr(reg::FCR,reg::SMF); tick(20);
    check(!irq.read() && !(peek(reg::SR)&reg::SMF) && target.polls==1,"SMF is an event, not an RX-level condition");
    until([&]{return target.polls==2;},"next matching poll"); tick(); check(irq.read(),"new poll can set SMF again"); abort();
}

void Bench::active_reset() {
    auto pulse_core=[&] {
        core_on=false; wait(core_period*2); rn=false; wait(1,SC_NS);
        for(unsigned off=0;off<64;off+=4) check(peek(off)==0,"asynchronous full register reset with stopped core");
        check(!irq.read() && !rxreq.read() && !txreq.read(),"reset clears outputs");
        rn=true; core_on=true; tick(3);
    };
    setup(); read_start(0,7);
    check((peek(reg::SR)&reg::BUSY) && target.records.empty(),"prefix phase active before first backend callback");
    pulse_core(); tick(200); check(target.records.empty(),"reset during serial prefix prevents any backend frame");
    // Mid-frame TX starved after its first chunk; no fabricated completion.
    setup(reg::TCIE); wr(reg::DLR,15); wr(reg::CCR,command(0,1,1,2)); wr(reg::AR,0); wr(reg::DR,0x12345678);
    until([&]{return target.records.size()==1;},"first TX chunk");
    check(peek(reg::SR)&reg::BUSY,"TX waiting for remaining data"); pulse_core();
    check(target.records.size()==2 && target.records.back().phase.cancel,"core reset closes open TX frame once");
    tick(300); check(target.records.size()==2 && peek(reg::SR)==reg::TXFTF,"no late TX completion after reset");
    setup(); read_start(0,255); until([&]{return ((peek(reg::SR)>>16)&63)==32;},"RX-full reset stage");
    pulse_core(); check(target.records.back().phase.cancel,"RX-full frame cancelled");
    setup(); read_start(0,3); done(); check(peek(reg::SR)&reg::BUSY,"unread completed RX stage");
    pulse_core(); check(peek(reg::DR)==0 && peek(reg::SR)==reg::TXFTF,"reset discards unread data and TCF");
    setup(reg::SMIE); wr(reg::DLR,0); wr(reg::PSMKR,1); wr(reg::PSMAR,1); wr(reg::PIR,1000);
    wr(reg::CCR,command(2,0,1,5)); until([&]{return target.polls==1;},"poll reset stage");
    pulse_core(); const auto old=target.polls; tick(1500); check(target.polls==old,"reset prevents next scheduled poll");
    // HRESET while a backend has returned annotated latency: caller must abort.
    reset(); setup(); wr(reg::CCR,command(3)); target.latency=sc_time(600,SC_NS);
    launch(0,true,false,0); until([&]{return target.records.size()==1;},"XIP backend delay stage");
    hrn=false; wait(1,SC_NS); hrn=true; join(0);
    check(jobs[0].reply.status==tlm::TLM_GENERIC_ERROR_RESPONSE,"HRESET releases active XIP caller");
    wait(1,SC_US); check(peek(reg::CR)==0x00000001 && !(peek(reg::SR)&(reg::BUSY|reg::TCF)),"isolated HRESET retains configuration without stale success (VP policy)");
    target.latency=SC_ZERO_TIME; check(access(true,false,4).status==tlm::TLM_OK_RESPONSE,"XIP recovery after HRESET");
    target.records.clear(); target.latency=sc_time(600,SC_NS);
    launch(0,true,false,8); until([&]{return target.records.size()==1;},"core reset during XIP backend latency");
    rn=false; wait(1,SC_NS); rn=true; join(0); wait(1,SC_US);
    check(jobs[0].reply.status==tlm::TLM_GENERIC_ERROR_RESPONSE && peek(reg::CR)==0 && peek(reg::SR)==reg::TXFTF,
        "core reset cancels delayed XIP and prevents stale flags");
    target.latency=SC_ZERO_TIME; setup(); wr(reg::CCR,command(3));
    // PRESET pulse must invalidate both admitted and mutex-queued AP writes.
    core_on=false; wait(core_period*2);
    launch(0,false,true,reg::ABR,0x11223344); launch(1,false,true,reg::ABR,0xaabbccdd); wait(p_period*3);
    prn=false; wait(1,SC_NS); prn=true; core_on=true; join(0); join(1);
    check(jobs[0].reply.status==tlm::TLM_GENERIC_ERROR_RESPONSE && jobs[1].reply.status==tlm::TLM_GENERIC_ERROR_RESPONSE,"PRESET cancels active and queued AP callers");
    check(peek(reg::ABR)==0,"cancelled writes do not leak after reset pulse");
    launch(0,true,false,0,0,sc_time(100,SC_NS)); wait(20,SC_NS); rn=false; wait(1,SC_NS); rn=true; join(0);
    check(jobs[0].reply.status==tlm::TLM_GENERIC_ERROR_RESPONSE,"core reset invalidates caller still consuming incoming delay");
}

void Bench::bus_errors() {
    const auto before=peek(reg::DCR);
    for(bool mapped:{false,true}) {
        for(auto address:{0x100000000ULL})
            check(access(mapped,false,address,0x5a5a5a5a).status==tlm::TLM_ADDRESS_ERROR_RESPONSE,"high address error");
        for(unsigned length:{0u,1u,2u,3u,5u,8u})
            check(access(mapped,false,0,0,length,8).status==tlm::TLM_BURST_ERROR_RESPONSE,"non-word length error");
        for(unsigned width:{0u,1u,2u,3u})
            check(access(mapped,false,0,0,4,width).status==tlm::TLM_BURST_ERROR_RESPONSE,"streaming wrap rejected");
        unsigned char be[]={255,0,255,255};
        check(access(mapped,true,0,0xffffffff,4,4,be,4).status==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"sparse lanes rejected");
        check(access(mapped,false,0,0,4,4,be,0).status==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"zero-length byte enable rejected");
        check(access(mapped,false,0,0,4,4,nullptr,0,true).status==tlm::TLM_GENERIC_ERROR_RESPONSE,"null data response");
        check(access(mapped,false,0,0,4,4,nullptr,0,false,tlm::TLM_IGNORE_COMMAND).status==tlm::TLM_COMMAND_ERROR_RESPONSE,"invalid bus command response");
    }
    check(peek(reg::DCR)==before && target.records.empty(),"rejected traffic is side-effect free");
    check(access(false,true,0x40,0xffffffff).status==tlm::TLM_ADDRESS_ERROR_RESPONSE,"unmapped AP word");
    check(access(false,false,0x40,0xffffffff).value==0,"unmapped AP read returns zero");
    setup(0,7); wr(reg::CCR,command(3));
    check(access(true,false,252).status==tlm::TLM_OK_RESPONSE,"last mapped word succeeds");
    const auto count=target.records.size();
    check(access(true,false,256).status==tlm::TLM_ADDRESS_ERROR_RESPONSE && target.records.size()==count,"FSIZE failure does not reach flash");
    for(auto response:{tlm::TLM_ADDRESS_ERROR_RESPONSE,tlm::TLM_COMMAND_ERROR_RESPONSE,tlm::TLM_BURST_ERROR_RESPONSE,tlm::TLM_GENERIC_ERROR_RESPONSE}) {
        target.response=response;
        check(access(true,false,0).status==tlm::TLM_GENERIC_ERROR_RESPONSE,"backend failure maps to functional AHB error");
        check(!(peek(reg::SR)&reg::TEF),"mapped failures do not set indirect TEF");
        target.response=tlm::TLM_OK_RESPONSE;
        check(access(true,false,4).status==tlm::TLM_OK_RESPONSE,"error releases admission for retry");
    }
    target.throws=true; check(access(true,false,0).status==tlm::TLM_GENERIC_ERROR_RESPONSE,"backend exception maps to AHB error");
    target.throws=false; target.response=tlm::TLM_ADDRESS_ERROR_RESPONSE; read_start(0,3);
    until([&]{return (peek(reg::SR)&reg::TEF)!=0;},"indirect backend error");
    check(!(peek(reg::SR)&(reg::BUSY|reg::TCF)),"failed indirect transfer has no false completion");
    target.response=tlm::TLM_OK_RESPONSE;
    read_start(0,3); drain({Target::pattern(0),Target::pattern(1),Target::pattern(2),Target::pattern(3)});
    // Illegal peripheral configuration is not an AP protocol error.
    wr(reg::FCR,63); wr(reg::CR,0); wr(reg::CR,0x01000001);
    check(access(false,true,reg::CCR,command(0,0,0,6)).status==tlm::TLM_OK_RESPONSE && (peek(reg::SR)&reg::TEF),"legal AP write with illegal divider sets TEF");
}

void Bench::polling_edges() {
    for(unsigned bytes=1;bytes<=4;++bytes) for(bool use_or:{false,true}) for(bool stop:{false,true}) {
        reset(); setup((use_or?reg::PMM:0)|(stop?reg::APMS:0));
        const std::uint32_t value=0x96814253;
        target.statuses={value}; wr(reg::DLR,bytes-1);
        const auto high_bit=1u<<(8*(bytes-1));
        wr(reg::PSMKR,high_bit); wr(reg::PSMAR,value&high_bit); wr(reg::PIR,1000);
        wr(reg::DMACR,3); wr(reg::CCR,command(2,0,1,5)); until([&]{return target.polls==1;},"poll-width completion"); tick(4);
        check((peek(reg::SR)&reg::SMF) && bool(peek(reg::SR)&reg::BUSY)==!stop,"AND/OR APMS matrix");
        const auto expected=bytes==4?value:value&((1u<<(8*bytes))-1);
        check(rd(reg::DR)==expected && rd(reg::DR)==expected,"last status zero-extends without consuming data");
        check(peek(reg::DMASR)==0 && !rxreq.read() && !txreq.read(),"poll DR bypasses RX FIFO and DMA");
        if(!stop) abort();
    }
    // Empty AND/OR masks use the mathematical identity. DEC-024 explicitly
    // requires both SMF and STOF when the final read matches at timeout.
    for(bool use_or:{false,true}) for(bool beyond:{false,true}) {
        reset(); setup(reg::APMS|(use_or?reg::PMM:0));
        wr(reg::DLR,0); wr(reg::PSMKR,beyond?0xffffff00:0); wr(reg::PSMAR,0xffffffff); wr(reg::PIR,1000);
        wr(reg::CCR,command(2,0,1,5)); until([&]{return target.polls==1;},"empty effective mask"); tick(4);
        check(bool(peek(reg::SR)&reg::SMF)==!use_or && bool(peek(reg::SR)&reg::BUSY)==use_or,"VP empty AND/OR convention ignores bytes beyond DLR");
        if(use_or) abort();
    }
    for(unsigned timeout:{0u,1u,31u,32u}) for(bool stop:{false,true}) {
        reset(); setup(reg::PTCEN|(stop?reg::APMS:0)); target.statuses={1};
        wr(reg::DLR,0); wr(reg::PSMKR,1); wr(reg::PSMAR,1); wr(reg::PTR,timeout);
        wr(reg::CCR,command(2,0,1,5));
        until([&]{return !(peek(reg::SR)&reg::BUSY);},"simultaneous poll stop");
        check(target.polls==1 && (peek(reg::SR)&reg::SMF),"timeout does not truncate in-progress status read");
        check(peek(reg::SR)&reg::STOF,"DEC-024 simultaneous match and timeout both reported");
    }
    reset(); setup(); wr(reg::DLR,0); wr(reg::PSMKR,1); wr(reg::PSMAR,1); wr(reg::PIR,65535);
    wr(reg::CCR,command(2,0,1,5)); until([&]{return target.polls==1;},"maximum PIR first frame");
    wait(100,SC_US); check(target.polls==1 && (peek(reg::SR)&reg::BUSY),"maximum PIR gap retained without spurious reads"); abort();
}

void Bench::dma_edges() {
    for(unsigned threshold:{0u,1u,7u,8u,31u}) {
        reset(); setup(threshold<<8); wr(reg::DMACR,1); wr(reg::DLR,127); wr(reg::CCR,command(0,1,1,2)); wr(reg::AR,0); tick(12);
        check(txreq.read(),"all legal word thresholds fit the default 32-word FIFO");
        txclr=true; tick(12); check(!txreq.read(),"held TX clear suppresses request");
        txclr=false; tick(12); check(txreq.read(),"TX re-evaluation after clear handshake");
        wr(reg::DMACR,0); tick(12); check(!txreq.read(),"live TX DMA disable");
        reset(); setup(threshold<<3); wr(reg::DMACR,2); read_start(0,127); done(); tick(12);
        check(rxreq.read(),"RX word threshold up to 32");
        rxclr=true; tick(12); check(!rxreq.read(),"held RX clear suppresses request");
        rxclr=false; tick(12); check(rxreq.read(),"RX request re-evaluated");
        wr(reg::DMACR,0); tick(12); check(!rxreq.read(),"live RX disable");
        wr(reg::DMACR,2); tick(12); check(rxreq.read(),"live RX re-enable");
        abort(); tick(12); check(!rxreq.read() && !txreq.read(),"abort inhibits both requests");
        wr(reg::DLR,127); wr(reg::AR,0); tick(12); check(!rxreq.read(),"DLR/AR do not re-arm after abort");
    }
    reset(); setup(1u<<3); wr(reg::DMACR,2); read_start(0,6); done(); tick(12);
    check(rxreq.read(),"seven bytes form two readable zero-padded words");
    rd(reg::DR); rxclr=true; tick(12); rxclr=false; tick(12);
    check(!rxreq.read(),"three-byte residue smaller than two-word burst");
    check((rd(reg::DR)&0xff000000u)==0,"partial RX high lane is zero");
    reset(); setup(7u<<8); wr(reg::DMACR,1); wr(reg::DLR,0); wr(reg::CCR,command(0,1,1,2)); wr(reg::AR,0); tick(12);
    check(!txreq.read(),"DEC-029 no eight-word DMA burst for a one-byte transfer");
    wr(reg::DR,0x55); done();
}

void Bench::domains() {
    setup(); wr(reg::CCR,command(3));
    launch(0,true,false,0); tick(3); launch(1,true,false,4);
    wr(reg::CR,0x00000001|reg::TCIE); check(peek(reg::CR)&reg::TCIE,"AP configuration-live field during XIP");
    join(0); join(1);
    check(jobs[0].reply.status==tlm::TLM_OK_RESPONSE && jobs[1].reply.status==tlm::TLM_OK_RESPONSE && jobs[0].ended<=jobs[1].ended,"one XIP outstanding, ordered completion");
    check(target.records.size()==2 && target.records[0].address==0 && target.records[1].address==4,"no duplicated or interleaved frames");
    // A gated unrelated bus domain does not stop the core/other interface.
    p_on=false; wait(p_period*2); launch(0,true,false,8); join(0);
    check(jobs[0].reply.status==tlm::TLM_OK_RESPONSE,"mapped path runs with PCLK stopped");
    p_on=true; wait(p_period*2); check(rd(reg::CCR)==command(3),"AP recovers when PCLK resumes");
    h_on=false; wait(h_period*2); read_start(16,3);
    drain({Target::pattern(16),Target::pattern(17),Target::pattern(18),Target::pattern(19)});
    h_on=true; wait(h_period*2);
    core_on=false; wait(core_period*2);
    launch(0,false,true,reg::ABR,0x11223344); launch(1,false,true,reg::ABR,0xaabbccdd);
    wait(p_period*(config.bus_timeout_cycles*2+10));
    check(!jobs[0].done && !jobs[1].done && peek(reg::ABR)==0,"APB callers wait without a timeout");
    core_on=true; join(0); join(1);
    check(jobs[0].reply.status==tlm::TLM_OK_RESPONSE && jobs[1].reply.status==tlm::TLM_OK_RESPONSE,"queued AP accesses complete when core resumes");
    check(peek(reg::ABR)==0xaabbccdd,"AP writes preserve order and payload");
}

void Bench::parameters() {
    const unsigned depth=config.rx_fifo_words;
    setup();
    const unsigned length=depth*8+3;
    std::vector<unsigned char> expected(length);
    for(unsigned i=0;i<length;++i) expected[i]=Target::pattern(64+i);
    read_start(64,length-1); drain(expected); audit(length,64,tlm::TLM_READ_COMMAND);
    wr(reg::FCR,63); wr(reg::CR,reg::EN|((depth-1)<<3)); read_start(0,depth*4-1); done(); tick();
    check(((peek(reg::SR)>>16)&63)==depth && (peek(reg::SR)&reg::RXFTF),"RX configured capacity and word threshold");
    abort(); wr(reg::FCR,63); wr(reg::DLR,2); wr(reg::CCR,command(0,1,1,2)); wr(reg::AR,512);
    wr(reg::DR,0x78563412); done();
    for(unsigned i=0;i<3;++i) check(target.written.at(512+i)==((0x78563412u>>(8*i))&255),"TX partial final word");
    check(target.written.count(515)==0,"no transmitted padding");
    wr(reg::CCR,command(3)); check(access(true,false,0).status==tlm::TLM_OK_RESPONSE,"XIP word at each legal FIFO depth");
    until([&]{return !(peek(reg::SR)&reg::BUSY);},"XIP STOP");
    core_on=false; wait(core_period*2); const auto start=sc_time_stamp();
    check(access(true,false,128).status==tlm::TLM_GENERIC_ERROR_RESPONSE,"AHB-only response timeout");
    check(sc_time_stamp()-start>=h_period*config.bus_timeout_cycles,"timeout counts actual HCLK cycles");
    core_on=true; tick(2);
    if (depth==32 && config.tx_fifo_words==32) {
        check(Config{}.bus_timeout_cycles==5000,"HDS default timeout");
        for(unsigned invalid:{0u,1u,3u,7u,33u,64u}) {
            auto c=Config{}; c.tx_fifo_words=invalid; bool rejected=false;
            try { c.validate(); } catch(const std::invalid_argument&) { rejected=true; }
            check(rejected,"illegal FIFO depth rejected before elaboration");
        }
        for(unsigned invalid:{0u,15u,1048577u}) {
            auto c=Config{}; c.bus_timeout_cycles=invalid; bool rejected=false;
            try { c.validate(); } catch(const std::invalid_argument&) { rejected=true; }
            check(rejected,"illegal AHB timeout rejected");
        }
        auto c=Config{}; c.prefetch_length=9; bool rejected=false;
        try { c.validate(); } catch(const std::invalid_argument&) { rejected=true; }
        check(rejected,"prefetch cannot exceed cache lines");
    }
}

void Bench::hds_cache() {
    // HDS 6.4.3 / DEC-016, DEC-027, DEC-030. Check backend frames,
    // independent expected bytes and bus completion, including an in-flight fill.
    setup(0,7); wr(reg::CCR,command(3));
    auto idle=[&] { until([&]{return !(peek(reg::SR)&reg::BUSY);},"XIP idle"); };
    auto mapped=[&](unsigned address,AhbTransaction::Transfer type=AhbTransaction::Nonseq) {
        return access(true,false,address,0,4,4,nullptr,0,false,-1,SC_ZERO_TIME,type);
    };
    auto expected=[](unsigned address) {
        std::uint32_t v=0; for(unsigned i=0;i<4;++i) v|=std::uint32_t(Target::pattern(address+i))<<(8*i); return v;
    };
    check(mapped(0).value==expected(0),"NONSEQ returns correct little-endian word"); idle();
    check(target.records.size()==1 && target.records[0].phase.length==4,"NONSEQ fetches exactly one word");
    const auto before=sc_time_stamp();
    check(mapped(3).value==expected(0) && target.records.size()==1,"cache hit ignores low address bits");
    check(sc_time_stamp()-before<h_period*3,"cache hit is served in AHB domain");
    check(mapped(4).value==expected(4),"second NONSEQ miss"); idle();
    check(mapped(0).value==expected(0),"prior line invalidated by miss"); idle();
    check(target.records.size()==3,"invalidate-on-miss replaces previous cache contents");
    target.records.clear();
    check(mapped(32,AhbTransaction::Seq).value==expected(32),"SEQ first word returns before prefetch finishes");
    check((peek(reg::SR)&reg::BUSY) && target.records.size()==1,"one fetch continues after first response");
    check(mapped(60,AhbTransaction::Seq).value==expected(60),"read waits for an in-flight prefetched word"); idle();
    check(target.records.size()==8,"SEQ fetch has exactly eight words, no duplicate miss");
    audit(32,32,tlm::TLM_READ_COMMAND);
    check(mapped(36).value==expected(36) && target.records.size()==8,"all prefetched lines remain cached");
    // A cache hit still works with the serial core stopped.
    core_on=false; wait(core_period*2);
    check(mapped(40).value==expected(40) && target.records.size()==8,"HCLK cache hit independent of core clock");
    core_on=true; tick(2);
    target.records.clear();
    check(mapped(252,AhbTransaction::Seq).value==expected(252),"last-word SEQ read"); idle();
    check(target.records.size()==1 && target.records[0].phase.length==4,"prefetch clamped at flash end");
    check(mapped(256).status==tlm::TLM_ADDRESS_ERROR_RESPONSE,"offset at FSIZE is rejected");
    for(auto type:{AhbTransaction::Idle,AhbTransaction::Busy}) {
        const auto time=sc_time_stamp();
        check(mapped(0xffffffffu,type).status==tlm::TLM_OK_RESPONSE && sc_time_stamp()==time,"IDLE/BUSY zero-wait OKAY without range check");
    }
    for(unsigned offset:{reg::ABR,reg::DCR,reg::CCR}) {
        mapped(64); idle(); const auto count=target.records.size();
        wr(offset,offset==reg::ABR?1:(offset==reg::DCR?8u<<16:command(3)));
        check(mapped(64).status==tlm::TLM_OK_RESPONSE,"read after cache flush"); idle();
        check(target.records.size()==count+1,"ABR, FSIZE and CCR updates invalidate cache");
    }
    wr(reg::CR,0); const auto count=target.records.size();
    check(mapped(64).status==tlm::TLM_GENERIC_ERROR_RESPONSE,"mapped mode disabled rejects cached read");
    wr(reg::CR,1); mapped(64); idle(); check(target.records.size()==count+1,"disable flushes cache");
    wr(reg::CR,0x01000001); wr(reg::CCR,command(3));
    check(mapped(128).status==tlm::TLM_GENERIC_ERROR_RESPONSE && (peek(reg::SR)&reg::TEF),"illegal mapped divider sets TEF and AHB ERROR");
    check(target.records.size()==count+1,"illegal divider never reaches backend");
}

void Bench::hds_control() {
    // DEC-038: disabled DR writes are retained, while EN only gates new frames.
    wr(reg::DCR,16u<<16); wr(reg::DLR,7); wr(reg::CCR,command(0,1,1,2)); wr(reg::AR,256);
    wr(reg::DR,0x44332211);
    check(((peek(reg::SR)>>8)&63)==1 && target.records.empty(),"disabled DR write pre-fills one word without trigger");
    wr(reg::CR,1); tick(20); check(target.records.empty(),"EN alone is not a frame trigger");
    wr(reg::DR,0x88776655); done(); audit(8,256,tlm::TLM_WRITE_COMMAND);
    check(target.written.at(256)==0x11 && target.written.at(263)==0x88,"prefilled words precede triggering DR word");
    reset(); setup(); wr(reg::DLR,7); wr(reg::CCR,command(0,1,1,2)); wr(reg::AR,0); wr(reg::DR,0x44332211);
    wr(reg::CR,0); wr(reg::DR,0x88776655); done();
    check(target.written.at(7)==0x88 && !(peek(reg::SR)&reg::BUSY),"EN clear completes current write with later DR data");
    const auto count=target.records.size(); wr(reg::DR,0x12345678); tick(100);
    check(target.records.size()==count && ((peek(reg::SR)>>8)&63)==1,"disabled idle DR does not start next frame");
    reset(); setup(); read_start(0,7); wr(reg::CR,0); done();
    check(((peek(reg::SR)>>16)&63)==2,"EN clear lets read finish and retains RX words");
    std::vector<unsigned char> bytes(8); for(unsigned i=0;i<8;++i) bytes[i]=Target::pattern(i); drain(bytes);
    reset(); setup(); wr(reg::DLR,0); wr(reg::PSMKR,1); wr(reg::PSMAR,1); wr(reg::CCR,command(2,0,1,5));
    tick(2); wr(reg::CR,0); until([&]{return !(peek(reg::SR)&reg::BUSY);},"disabled polling completes frame");
    check(target.polls==1 && !(peek(reg::SR)&reg::TCF),"EN clear stops polling after current status, without indirect TCF");
    // DEC-013: no address phase means no FSIZE check.
    reset(); setup(0,0); wr(reg::DLR,8); wr(reg::CCR,command(1,0,1));
    bytes.resize(9); for(unsigned i=0;i<9;++i) bytes[i]=Target::pattern(i); drain(bytes);
    check(!(peek(reg::SR)&reg::TEF),"address-less transfer is not constrained by FSIZE");
    reset(); setup(); wr(reg::ABR,0xaabbccdd); wr(reg::DLR,0);
    wr(reg::CCR,(command(1)&~(3u<<12))|(1u<<14)); wr(reg::AR,0x1234); done();
    check(target.records[0].phase.address==0x34 && target.records[0].phase.alternate==0xdd,"address and alternate phases use selected low bytes"); rd(reg::DR);
    reset(); setup(); wr(reg::CCR,0); done(); check(!(peek(reg::SR)&reg::TEF),"all-skipped frame proceeds to STOP");
    // ABORT is an observable request, and writing zero cannot cancel it.
    reset(); setup(); read_start(0,15); wr(reg::FCR,reg::TCF);
    wr(reg::CR,reg::EN|reg::ABORT);
    check((peek(reg::CR)&reg::ABORT) && (peek(reg::SR)&reg::BUSY) && !(peek(reg::SR)&reg::TCF),"abort request stays BUSY before byte boundary");
    wr(reg::CR,reg::EN);
    check(peek(reg::CR)&reg::ABORT,"writing zero does not withdraw ABORT");
    until([&]{return !(peek(reg::CR)&reg::ABORT);},"byte-boundary abort completes");
    check((peek(reg::SR)&(reg::BUSY|reg::TCF|0x3f3f00))==reg::TCF,"abort completion atomically clears BUSY, flushes FIFOs and sets TCF");
}

void Bench::hds_dma_clock() {
    setup(); wr(reg::DMACR,1); wr(reg::DLR,31);
    d_on=false; wait(40,SC_NS); wr(reg::CCR,command(0,1,1,2)); tick(20);
    check(!txreq.read(),"DMA request does not change with DMA_CLK stopped");
    d_on=true; tick(12); check(txreq.read(),"request crosses to independent DMA clock");
    // A clear cannot re-arm using an old condition when core evaluation is stopped.
    core_on=false; wait(core_period*2); txclr=true; wait(50,SC_NS); txclr=false; wait(200,SC_NS);
    check(!txreq.read(),"clear holds req low until core acknowledges and re-evaluates");
    core_on=true; tick(15); check(txreq.read(),"clear handshake re-arms after core response");
    abort(); tick(15); check(!txreq.read() && !rxreq.read(),"abort cancels DMA");
    wr(reg::DLR,31); wr(reg::AR,0); tick(15); check(!txreq.read(),"DMA stays inhibited after AR and DLR");
    wr(reg::CCR,command(0,1,1,2)); tick(15); check(txreq.read(),"accepted CCR is the DMA re-arm point");
    rn=false; prn=false; hrn=false; wait(1,SC_NS);
    check(!txreq.read() && !rxreq.read() && !irq.read(),"global reset clears DMA outputs asynchronously");
    rn=true; prn=true; hrn=true; tick(3); check(peek(reg::SR)==reg::TXFTF,"SR TXFTF after reset release");
    setup(1u<<3); wr(reg::DMACR,2); read_start(0,6); done(); tick(15);
    check(rxreq.read(),"two-word RX burst before drain");
    rd(reg::DR); rd(reg::DR);
    rxclr=true; wait(dc.posedge_event()); settle();
    check(!rxreq.read(),"RX clear removes completed burst request"); rxclr=false;
    for(unsigned i=0;i<12;++i) {
        wait(dc.posedge_event()); settle();
        check(!rxreq.read(),"pre-clear synchronized condition never re-arms an empty RX burst");
    }
}

void Bench::hds_flags() {
    // Hardware completion and software W1C are arranged on the same core edge.
    setup(reg::TCIE); read_start(0,3);
    until([&]{return !target.records.empty();},"last read word arrives"); tick();
    core_on=false; wait(core_period*2);
    launch(0,false,true,reg::FCR,reg::TCF); wait(p_period*3);
    core_on=true; join(0); done();
    check((peek(reg::SR)&reg::TCF) && irq.read(),"hardware set takes priority over simultaneous software clear");
    rd(reg::DR); wr(reg::FCR,reg::TCF); tick(); check(!(peek(reg::SR)&reg::TCF),"later W1C clears completion");
    // Both endpoints ignore low two address bits, but unmapped reads return zero.
    wr(reg::DCR+3,7u<<16); check(rd(reg::DCR+1)==7u<<16,"APB low address bits ignored");
    check(access(false,false,0x40,0xdeadbeef).value==0,"unmapped APB read returns zero");
    // A core reset by itself cannot manufacture an APB timeout response.
    rn=false; wait(core_period*2); launch(0,false,true,reg::ABR,0x11223344);
    wait(p_period*(config.bus_timeout_cycles+10)); check(!jobs[0].done,"APB waits while core reset asserted");
    rn=true; join(0); check(jobs[0].reply.status==tlm::TLM_OK_RESPONSE && rd(reg::ABR)==0x11223344,"APB completes same request after core reset release");
}

void Bench::hds_timeout() {
    // HDS CFG-TMO: timeout at 64 HCLKs and no cache fill from late responses.
    setup(); wr(reg::CCR,command(3)); target.latency=sc_time(10,SC_US);
    const auto time=sc_time_stamp();
    check(access(true,false,0).status==tlm::TLM_GENERIC_ERROR_RESPONSE,"CFG-TMO AHB miss times out");
    check(sc_time_stamp()-time>=h_period*64 && sc_time_stamp()-time<h_period*68,"timeout counts 64 HCLK cycles plus response");
    const auto count=target.records.size(); wait(11,SC_US); target.latency=SC_ZERO_TIME;
    const auto v=access(true,false,0);
    check(v.status==tlm::TLM_OK_RESPONSE && target.records.size()==count+1,"late timed-out word was not cached");
    until([&]{return !(peek(reg::SR)&reg::BUSY);},"XIP STOP");
    check(access(true,false,0).value==v.value && target.records.size()==count+1,"successful retry is cached");
    // In-flight prefetch: an unrelated miss waits, then starts a new frame.
    wr(reg::CCR,command(3));
    check(access(true,false,0,0,4,4,nullptr,0,false,-1,SC_ZERO_TIME,AhbTransaction::Seq).status==tlm::TLM_OK_RESPONSE,"prefetch starts");
    const auto before=target.records.size();
    check(access(true,false,128).status==tlm::TLM_GENERIC_ERROR_RESPONSE,"unrelated miss timeout includes time behind active fetch");
    wait(5,SC_US);
    check(target.records.size()>=before,"prior fetch cleanup is bounded");
    check(access(true,false,128).status==tlm::TLM_OK_RESPONSE,"new request recovers after queued-miss timeout");
}

void Bench::hds_abort() {
    // Long individual phases make the byte-boundary request observable through
    // APB. Functional backend cancellation is checked; there are no pad pins.
    const std::vector<std::uint32_t> phases={
        1u<<8, (1u<<10)|(3u<<12), (1u<<14)|(3u<<16), 31u<<18, 1u<<24};
    for(unsigned mode=0;mode<4;++mode) for(auto phase:phases) {
        reset(); setup(0,16,2); wr(reg::DLR,15);
        const auto cmd=(mode<<26)|phase|(mode==2?1u<<24:0)|(mode==3?((1u<<24)|(1u<<10)|(2u<<12)):0);
        wr(reg::CCR,cmd);
        if (mode==3) launch(0,true,false,0);
        else if ((cmd>>10)&3) wr(reg::AR,0);
        if (mode==0 && ((cmd>>24)&3)) wr(reg::DR,0x12345678);
        // Polling DLR is limited to four bytes.
        if (mode==2) {
            check(peek(reg::SR)&reg::TEF,"invalid poll length rejected before abort");
            wr(reg::DLR,0); wr(reg::CCR,cmd); if ((cmd>>10)&3) wr(reg::AR,0);
        }
        tick(2); wr(reg::FCR,reg::TCF); abort();
        check((peek(reg::SR)&reg::TCF) && !(peek(reg::SR)&(reg::BUSY|0x3f3f00)),"abort flushes and completes across mode/phase combinations");
        tick(10); check(!rxreq.read() && !txreq.read(),"abort keeps both requests low");
        if (mode==3) { join(0); check(jobs[0].reply.status==tlm::TLM_GENERIC_ERROR_RESPONSE,"pending XIP abort returns ERROR, phase="+std::to_string(phase)); }
    }
}

void Bench::hds_frame_matrix() {
    // HDS 6.3 and Section 10: vary each phase width independently. Each of
    // the 384 width/rate combinations writes and reads a short unaligned tail.
    unsigned index=0;
    std::array<std::array<bool,4>,4> seen_address{},seen_alternate{};
    std::array<bool,32> seen_dummy{};
    for(unsigned ddr=0;ddr<2;++ddr) for(unsigned im=0;im<4;++im)
        for(unsigned ad=0;ad<4;++ad) for(unsigned alt=0;alt<4;++alt)
            for(unsigned data=1;data<4;++data,++index) {
                reset();
                const unsigned address_bytes=1+index%4,alternate_bytes=1+(index/4)%4;
                const unsigned dummy=index%32,mode3=index%2,sioo=(index/2)%2;
                const unsigned count=1+index%9,divisor=ddr?4:2;
                const std::uint32_t raw=0x76543210,abr=0xa1b2c3d4;
                const auto low=[](std::uint32_t value,unsigned bytes) {
                    return bytes==4?value:value&((1u<<(bytes*8))-1);
                };
                const auto width=[](unsigned mode) { return mode?1u<<(mode-1):0; };
                const auto address=ad?low(raw,address_bytes):0;
                const auto alternate=low(abr,alternate_bytes);
                wr(reg::DCR,(31u<<16)|((index%8)<<8)|mode3);
                wr(reg::CR,(ddr?2u<<24:0)|reg::EN);
                wr(reg::ABR,abr); wr(reg::DLR,count-1);
                const auto settings=(ddr?reg::DDRM:0)|(sioo?reg::SIOO:0)|(data<<24)|
                    (dummy<<18)|((alternate_bytes-1)<<16)|(alt<<14)|
                    ((address_bytes-1)<<12)|(ad<<10)|(im<<8)|0xa6;
                wr(reg::CCR,settings); wr(reg::AR,raw);
                std::vector<unsigned char> bytes(count);
                for(unsigned i=0;i<count;++i) bytes[i]=static_cast<unsigned char>(index*17+i*29);
                const auto trigger=sc_time_stamp();
                for(unsigned i=0;i<count;i+=4) wr(reg::DR,word(bytes,i,std::min(4u,count-i)));
                done(); audit(count,address,tlm::TLM_WRITE_COMMAND);
                const auto& descriptor=target.records.front().phase;
                check(descriptor.instruction_lines==width(im) && descriptor.address_lines==width(ad) &&
                    descriptor.alternate_lines==width(alt) && descriptor.data_lines==width(data),"independent phase widths");
                check(descriptor.address_bytes==(ad?address_bytes:0) && descriptor.alternate_bytes==(alt?alternate_bytes:0) &&
                    descriptor.address==address && descriptor.alternate==alternate && descriptor.dummy_cycles==dummy &&
                    descriptor.ddr==bool(ddr) && descriptor.clock_mode3==bool(mode3),"sizes, low phase bytes, dummy and rate metadata");
                for(unsigned i=0;i<count;++i) check(target.written.at(std::uint64_t(address)+i)==bytes[i],"write byte oracle across all phase widths");
                check(target.written.size()==count,"last word sends only DLR bytes");
                // Timing oracle follows HDS byte rates, independent of helper methods.
                const unsigned prefix=(im?8/width(im)*divisor:0)+
                    (ad?address_bytes*8/width(ad)*divisor/(ddr?2:1):0)+
                    (alt?alternate_bytes*8/width(alt)*divisor/(ddr?2:1):0)+dummy*divisor;
                const unsigned payload=std::min(4u,count)*8/width(data)*divisor/(ddr?2:1);
                check(target.records.front().time-trigger>=core_period*(prefix+payload),"instruction stays SDR; other phases use selected rate");
                target.records.clear(); wr(reg::FCR,63);
                wr(reg::CCR,settings|(1u<<26)); if(ad) wr(reg::AR,raw);
                drain(bytes); audit(count,address,tlm::TLM_READ_COMMAND);
                check(target.records.front().phase.instruction_lines==width(im),"accepted CCR re-arms SIOO instruction");
                if(ad) {
                    target.records.clear(); wr(reg::FCR,63); wr(reg::AR,raw);
                    drain(bytes); audit(count,address,tlm::TLM_READ_COMMAND);
                    check(target.records.front().phase.instruction_lines==(sioo?0:width(im)),"SIOO across AR-triggered frames");
                }
                seen_address[ad][address_bytes-1]=true; seen_alternate[alt][alternate_bytes-1]=true;
                seen_dummy[dummy]=true;
            }
    check(index==384,"all independent width/rate combinations visited");
    for(unsigned mode=1;mode<4;++mode) for(unsigned size=0;size<4;++size)
        check(seen_address[mode][size] && seen_alternate[mode][size],"each active line mode covers all address and alternate sizes");
    for(bool seen:seen_dummy) check(seen,"every dummy setting 0..31 visited");
}

void Bench::hds_dividers() {
    // HDS 6.6: all encoded prescalers, both rates. Instruction remains SDR.
    for(unsigned ddr=0;ddr<2;++ddr) for(unsigned prescaler=0;prescaler<256;++prescaler) {
        reset(); setup(0,16,prescaler);
        const unsigned divisor=prescaler+2;
        const bool legal=divisor%(ddr?4:2)==0;
        const auto trigger=sc_time_stamp();
        wr(reg::CCR,(ddr?reg::DDRM:0)|(1u<<8)|6);
        if(legal) {
            done(); check(target.records.size()==1,"legal divider starts exactly one frame");
            check(target.records[0].time-trigger>=core_period*(8*divisor),"all legal instruction durations use divisor+2 and SDR");
        } else check((peek(reg::SR)&reg::TEF) && !(peek(reg::SR)&reg::BUSY) && target.records.empty(),"illegal divider sets TEF without starting");
    }
}

void Bench::hds_parameters() {
    // HDS Table 5-1: validate each configurable field independently.
    auto valid=[&](const Config& c) { c.validate(); check(true,"legal HDS parameter accepted"); };
    auto invalid=[&](const Config& c) {
        bool rejected=false; try { c.validate(); } catch(const std::invalid_argument&) { rejected=true; }
        check(rejected,"illegal HDS parameter rejected");
    };
    for(unsigned depth:{4u,8u,16u,32u}) { auto c=Config{}; c.tx_fifo_words=depth; valid(c); c.rx_fifo_words=depth; valid(c); }
    for(unsigned depth:{2u,4u,8u,16u}) { auto c=Config{}; c.apb_async_fifo_depth=depth; valid(c); c.ahb_async_fifo_depth=depth; valid(c); }
    for(unsigned n=1;n<=16;++n) for(unsigned prefetch=1;prefetch<=n;++prefetch) {
        auto c=Config{}; c.mem_cache_count=n; c.prefetch_length=prefetch; valid(c);
    }
    for(unsigned timeout:{16u,64u,5000u,1048576u}) { auto c=Config{}; c.bus_timeout_cycles=timeout; valid(c); }
    for(unsigned n:{0u,1u,2u,3u,7u,33u,64u,0xffffffffu}) {
        auto c=Config{}; c.tx_fifo_words=n; invalid(c); c=Config{}; c.rx_fifo_words=n; invalid(c);
    }
    for(unsigned n:{0u,1u,3u,32u,0xffffffffu}) {
        auto c=Config{}; c.apb_async_fifo_depth=n; invalid(c); c=Config{}; c.ahb_async_fifo_depth=n; invalid(c);
    }
    for(unsigned n:{0u,17u,0xffffffffu}) { auto c=Config{}; c.mem_cache_count=n; invalid(c); }
    for(unsigned n:{0u,9u,0xffffffffu}) { auto c=Config{}; c.prefetch_length=n; invalid(c); }
    for(unsigned n:{0u,15u,1048577u,0xffffffffu}) { auto c=Config{}; c.bus_timeout_cycles=n; invalid(c); }
}

void Bench::hds_cache_parameters() {
    // Exercise runtime storage/lookup for smallest, largest and non-power-of-two caches.
    const unsigned count=config.mem_cache_count;
    setup(0,7); wr(reg::DLR,0xffffffff); wr(reg::CCR,command(3));
    auto idle=[&] { until([&]{return !(peek(reg::SR)&reg::BUSY);},"parameterized cache fill completes"); };
    const auto result=access(true,false,32,0,4,4,nullptr,0,false,-1,SC_ZERO_TIME,AhbTransaction::Seq);
    check(result.status==tlm::TLM_OK_RESPONSE,"parameterized SEQ read"); idle();
    check(target.records.size()==count && target.records.front().phase.length==count*4,"configured prefetch length used at runtime");
    audit(count*4,32,tlm::TLM_READ_COMMAND);
    for(unsigned i=0;i<count;++i) {
        const auto hit=access(true,false,32+i*4);
        std::uint32_t expected=0;
        for(unsigned b=0;b<4;++b) expected|=std::uint32_t(Target::pattern(32+i*4+b))<<(8*b);
        check(hit.status==tlm::TLM_OK_RESPONSE && hit.value==expected && target.records.size()==count,"each cache line returns its tagged word");
    }
    const auto before=sc_time_stamp();
    check(access(true,true,0xffffffff,0,4,4,nullptr,0,false,-1,SC_ZERO_TIME,AhbTransaction::Seq,false).status==tlm::TLM_OK_RESPONSE &&
        sc_time_stamp()==before && target.records.size()==count,"HSEL=0 ignores write/mode/range with zero wait");
    check(access(true,false,160).status==tlm::TLM_OK_RESPONSE,"miss replaces cache"); idle();
    check(access(true,false,32).status==tlm::TLM_OK_RESPONSE,"old line must refetch"); idle();
    check(target.records.size()==count+2,"miss invalidates all lines at each cache count");
}

void Bench::run() {
    try {
        reset();
        if (scenario=="p0_triggers") trigger_combinations();
        else if (scenario=="p0_dlr") dlr_boundaries();
        else if (scenario=="p0_fifo") fifo_packing();
        else if (scenario=="p0_irq_w1c") irq_w1c();
        else if (scenario=="p0_active_reset") active_reset();
        else if (scenario=="p0_bus_errors") bus_errors();
        else if (scenario=="p1_polling") polling_edges();
        else if (scenario=="p1_dma") dma_edges();
        else if (scenario=="p1_domains") domains();
        else if (scenario.rfind("p1_parameters_",0)==0) parameters();
        else if (scenario=="hds_cache") hds_cache();
        else if (scenario=="hds_control") hds_control();
        else if (scenario=="hds_dma_clock") hds_dma_clock();
        else if (scenario=="hds_flags") hds_flags();
        else if (scenario=="hds_timeout") hds_timeout();
        else if (scenario=="hds_abort") hds_abort();
        else if (scenario=="hds_frame_matrix") hds_frame_matrix();
        else if (scenario=="hds_dividers") hds_dividers();
        else if (scenario=="hds_parameters") hds_parameters();
        else if (scenario.rfind("hds_cache_parameters_",0)==0) hds_cache_parameters();
        else check(false,"unknown scenario");
        passed=true; std::cout<<"PASS "<<scenario<<" assertions="<<assertions<<" time="<<sc_time_stamp()<<'\n';
    } catch(const std::exception& e) { std::cerr<<"FAIL "<<scenario<<": "<<e.what()<<'\n'; }
    sc_stop();
}
}
int sc_main(int argc,char** argv) {
    const std::string scenario=argc>1?argv[1]:"p0_triggers";
    const auto depth=argc>2?static_cast<unsigned>(std::stoul(argv[2])):32u;
    const auto rx_depth=argc>3?static_cast<unsigned>(std::stoul(argv[3])):depth;
    const auto cache_words=argc>4?static_cast<unsigned>(std::stoul(argv[4])):0u;
    Bench bench{"bench",scenario,depth,rx_depth,cache_words}; sc_start(sc_time(100,SC_MS)); return bench.passed?0:1;
}
