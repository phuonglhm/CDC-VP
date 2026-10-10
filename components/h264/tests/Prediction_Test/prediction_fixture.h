#pragma once
#include <h264/top/encoder_vp.h>
#include <h264/platform/ddr_memory.h>
#include <h264/platform/host_driver.h>
#include <h264/intra/intra_tlm.h>
#include <h264/inter/inter_tlm.h>
#include "cmb_dma.h"
#include "sw_dma.h"
#include "df_dma.h"
#include "nal_dma.h"
#include <algorithm>
#include <iostream>
namespace prediction_test {
using namespace sc_core;
inline void require(bool ok,const char* msg) { if(!ok) throw std::runtime_error(msg); }
constexpr uint32_t cmb_base=0x1ff0, ref_base=0x4000, nal_base=0x8000;
inline std::vector<unsigned char> pattern() {
    std::vector<unsigned char> v(384);
    for(unsigned i=0;i<v.size();++i) v[i]=static_cast<unsigned char>((13*i+7*(i/16)+31)%256);
    return v;
}
struct NoDirectMemory: h264::MemoryIf {
    void write(uint64_t,const uint8_t*,size_t) override { throw std::runtime_error("direct memory bypass"); }
    void read(uint64_t,uint8_t*,size_t) const override { throw std::runtime_error("direct memory bypass"); }
    uint8_t read_byte(uint64_t) const override { throw std::runtime_error("direct memory bypass"); }
};
// Test assembly only: real prediction/clients; identity reconstruction and tagged
// output word are backend fixtures. This is not a released codec adapter.
class Pipeline: public sc_module,public h264::FrameExecutorIf {
public:
    sc_in<bool> reset_n{"reset_n"};
    sc_signal<bool> enable{"enable"};
    h264::intra::IntraTlm intra{"intra"};
    h264::inter::InterTlm inter{"inter"};
    tlm_utils::simple_initiator_socket<Pipeline> cmb_port{"cmb"},sw_port{"sw"},nal_port{"nal"},df_port{"df"},
        intra_port{"intra_port"},inter_port{"inter_port"};
    h264::ResetDomain& domain;
    h264::H264Arb queue; // Required client queue, NOT a second AXI bridge.
    NoDirectMemory memory;
    h264::CmbDma cmb{memory,queue};
    h264::SwDma sw{queue,{16,16},{16,16}};
    h264::DfDma df{memory,queue};
    h264::NalDma nal{memory,queue};
    unsigned stage,blocks=0,samples=0,words=0;
    std::array<unsigned,4> transfers{};
    Pipeline(sc_module_name name,h264::ResetDomain& r,h264::DmaTransport& arb,unsigned s)
        :sc_module(name),domain(r),stage(s) {
        cmb_port.bind(arb.clients); sw_port.bind(arb.clients);
        nal_port.bind(arb.clients); df_port.bind(arb.clients);
        intra_port.bind(intra.target_socket); inter_port.bind(inter.target_socket);
        intra.reset_n(reset_n); inter.reset_n(reset_n);
        intra.frame_enable(enable); inter.frame_enable(enable);
    }
    void reset() override { words=0; }
    h264::SyntaxRequirements syntax_requirements(const h264::FrameConfig&) const override { return {0,0,0,0}; }
    bool abort_and_drain(uint64_t g) override { pump(g); return !queue.busy(); }
    void pump(uint64_t g) {
        unsigned steps=0;
        while(queue.busy()) {
            require(++steps<4096,"queue watchdog");
            const auto owner=queue.pick(); require(queue.acquire(owner),"queue grant");
            const auto req=queue.granted_request();
            std::vector<unsigned char> data=req.is_write?req.data:std::vector<unsigned char>(h264::request_bytes(req));
            require(data.size()==h264::request_bytes(req),"request size");
            h264::EpochExtension epoch(domain); epoch.generation=g;
            h264::DmaClientExtension client; client.client=owner;
            tlm::tlm_generic_payload tx; tx.set_extension(&epoch); tx.set_extension(&client);
            tx.set_command(req.is_write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
            tx.set_address(req.addr); tx.set_data_ptr(data.data());
            tx.set_data_length(unsigned(data.size())); tx.set_streaming_width(tx.get_data_length());
            sc_time delay=SC_ZERO_TIME;
            switch(owner) {
            case h264::ClientId::CMB: cmb_port->b_transport(tx,delay); break;
            case h264::ClientId::SW: sw_port->b_transport(tx,delay); break;
            case h264::ClientId::NAL: nal_port->b_transport(tx,delay); break;
            case h264::ClientId::DF: df_port->b_transport(tx,delay); break;
            default: throw std::runtime_error("invalid owner");
            }
            h264::consume_delay(delay); tx.clear_extension<h264::EpochExtension>(); tx.clear_extension<h264::DmaClientExtension>();
            ++transfers[unsigned(owner)];
            queue.complete(!tx.is_response_error() && domain.valid(g),data);
        }
    }
    template<class E> void op(tlm_utils::simple_initiator_socket<Pipeline>& port,
            unsigned address,bool write,std::vector<unsigned char>& data,E& ext,uint64_t g) {
        h264::EpochExtension epoch(domain); epoch.generation=g;
        tlm::tlm_generic_payload tx; tx.set_extension(&ext); tx.set_extension(&epoch);
        tx.set_command(write?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(address); tx.set_data_ptr(data.empty()?nullptr:data.data());
        tx.set_data_length(unsigned(data.size())); tx.set_streaming_width(tx.get_data_length());
        sc_time delay=SC_ZERO_TIME; port->b_transport(tx,delay); h264::consume_delay(delay);
        tx.clear_extension<E>(); tx.clear_extension<h264::EpochExtension>();
        if(tx.is_response_error()) throw std::runtime_error("prediction: "+intra.last_error()+" "+inter.last_error());
    }
    void begin_activation(const h264::FrameConfig& c,uint64_t) override {
        require(c.width==16 && c.height==16,"fixture dimensions");
        enable.write(false); wait(1,SC_NS); enable.write(true); wait(1,SC_NS);
        blocks=samples=words=0;
        cmb.update_enable(false); cmb.configure({16,16},c.cmb,h264::CmbFrameMode::WorkingSet,384);
        cmb.update_enable(true);
        sw.set_refm_base(c.refm); sw.invalidate_all();
        df.set_dims({16,16}); df.set_refm_base(c.refm); df.set_ref_slot(1);
        df.latch_df_enable(false); df.on_sofm();
        nal.on_disable(); nal.configure(c.nal,c.sequence.nal_capacity_bytes);
    }
    void intra_test(const h264::MacroblockPixels& pixels,uint64_t g) {
        const unsigned scan[16]={0,1,4,5,2,3,6,7,8,9,12,13,10,11,14,15};
        for(unsigned b=0;b<18;++b) {
            unsigned p=b<16?0:b-15,n=p?8:4,x=p?0:scan[b]%4*4,y=p?0:scan[b]/4*4;
            const auto* src=p==0?pixels.y.data():p==1?pixels.u.data():pixels.v.data();
            std::vector<unsigned char> original(n*n);
            for(unsigned j=0;j<n;++j) for(unsigned i=0;i<n;++i) original[j*n+i]=src[(y+j)*(p?8:16)+x+i];
            h264::intra::Extension ext;
            ext.block={static_cast<h264::intra::Plane>(p),p?h264::intra::Kind::Chroma8x8:h264::intra::Kind::Luma4x4,x,y};
            auto data=original; op(intra_port,0,true,data,ext,g);
            require(ext.decision && intra.ready() && !intra.block_done(),"intra evaluate/done");
            data.assign(n*n,0); op(intra_port,1,false,data,ext,g);
            require(ext.decision->residual.size()==data.size(),"residual length");
            for(unsigned i=0;i<data.size();++i) {
                require(ext.decision->residual[i]==int(original[i])-int(data[i]),"residual oracle");
                if(!b) require(data[i]==128,"first block DC oracle");
            }
            auto again=data; op(intra_port,1,false,again,ext,g); require(again==data,"held intra predictor");
            data=original; // Identity decoded feedback fixture, NOT TQ/ITQ.
            op(intra_port,2,true,data,ext,g);
            require(ext.block_done && intra.block_done(),"reconstruct completion");
            ++blocks;
        }
    }
    void inter_test(const h264::MacroblockPixels& pixels,uint64_t g) {
        using namespace h264::inter;
        Reference ref{List::L0,{7,0}}; inter.retag(ref.list,ref.tag,16,16);
        for(unsigned p=0;p<3;++p) {
            const auto& v=sw.plane(h264::RefList::List0,p);
            inter.refill(ref,static_cast<Plane>(p),v.x,v.y,v.width,v.height,v.pixels);
        }
        Extension ext; ext.request.integer_candidates={{ref,{0,0},0},{ref,{4,0},0}};
        ext.request.fractional_candidates={{{1,0},0}};
        std::vector<unsigned char> data(pixels.y.begin(),pixels.y.end());
        op(inter_port,0,true,data,ext,g);
        require(ext.decision && ext.syntax && ext.decision->mode.winner.sad==0,"inter exact match");
        require(ext.syntax->mv.x==0 && ext.syntax->mv.y==0,"MV oracle");
        require(std::all_of(ext.decision->residual.begin(),ext.decision->residual.end(),[](int x){return x==0;}),"inter residual");
        data.clear(); op(inter_port,1,true,data,ext,g);
        std::array<bool,384> seen{};
        for(unsigned i=0;i<384;++i) {
            data.assign(1,0); op(inter_port,2,false,data,ext,g);
            require(ext.sample && !inter.done(),"sample ready before done");
            auto sample=*ext.sample; const unsigned p=unsigned(sample.plane);
            const unsigned index=(p==0?0:p==1?256:320)+sample.y*(p?8:16)+sample.x;
            require(index<384 && !seen[index],"sample coordinates/duplicate"); seen[index]=true;
            auto expected=p==0?pixels.y[sample.y*16+sample.x]:p==1?pixels.u[sample.y*8+sample.x]:pixels.v[sample.y*8+sample.x];
            require(data[0]==expected && sample.sequence==i && sample.last==(i==383),"MC oracle/metadata");
            wait(2,SC_NS); auto again=data; op(inter_port,2,false,again,ext,g);
            require(again==data && ext.sample->sequence==i,"held MC output");
            ext.accept_sequence=sample.sequence; op(inter_port,3,true,data,ext,g);
            require(inter.done()==(i==383),"final Accept completion");
            ++samples;
        }
        require(std::all_of(seen.begin(),seen.end(),[](bool v){return v;}),"YUV coverage");
    }
    uint32_t execute(const h264::FrameConfig&,uint64_t g,unsigned) override {
        cmb.fetch_macroblock(0,0); require(!cmb.done(),"premature CMB done");
        pump(g); require(cmb.done() && !cmb.failed(),"CMB completion");
        const auto pixels=cmb.pixels(); auto expected=pattern();
        require(std::equal(pixels.y.begin(),pixels.y.end(),expected.begin()) &&
            std::equal(pixels.u.begin(),pixels.u.end(),expected.begin()+256) &&
            std::equal(pixels.v.begin(),pixels.v.end(),expected.begin()+320),"CMB YUV oracle");
        sw.set_ref_slot(h264::RefList::List0,0,7); sw.fill_window(h264::RefList::List0,0,0);
        require(!sw.ready(h264::RefList::List0),"premature SW ready");
        pump(g); require(sw.ready(h264::RefList::List0),"SW refill");
        for(unsigned p=0;p<3;++p) {
            const auto& v=sw.plane(h264::RefList::List0,p).pixels;
            require(std::equal(v.begin(),v.end(),expected.begin()+(p==0?0:p==1?256:320)),"SW YUV oracle");
        }
        if(stage==2 || stage==4) intra_test(pixels,g);
        if(stage>=3) inter_test(pixels,g);
        df.schedule_macroblock(0,0,pixels); df.dma_fmdone(); df.consume_done();
        require(!df.frame_complete(),"reference premature done");
        nal.accept_word(0x74534554); nal.flush_chunk(); // Tagged fixture bytes, NOT H.264.
        nal_words_accepted(g,1);
        require(!nal.final_b_accepted(),"NAL premature done");
        pump(g);
        require(df.frame_complete() && nal.final_b_accepted() && nal.stm_len()==1,"write completion");
        words=1; return words;
    }
    uint32_t end_activation(const h264::FrameConfig&,uint64_t g,uint32_t) override {
        pump(g); require(!queue.busy(),"final drain"); return words;
    }
};
class Bench: public sc_module {
public:
    sc_signal<bool> reset_n{"reset"},irq{"irq"};
    h264::EncoderVp encoder;
    h264::DdrMemory memory{"memory",1<<20};
    h264::HostDriver host{"host"};
    Pipeline* pipeline;
    unsigned stage; bool fault,passed=false;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name name,unsigned s,unsigned width,bool fail)
      :sc_module(name),encoder("encoder",width,{},sc_time(8,SC_NS),
          [s](sc_module_name n,h264::ResetDomain& r,h264::DmaTransport& a)->std::unique_ptr<h264::FrameExecutorIf>{
              return std::make_unique<Pipeline>(n,r,a,s);
          }),pipeline(dynamic_cast<Pipeline*>(encoder.pipeline.get())),stage(s),fault(fail) {
        pipeline->reset_n(reset_n); encoder.rstn(reset_n); encoder.irq(irq);
        encoder.dma.memory.bind(memory.socket);
        host.memory.bind(memory.socket); host.registers.bind(encoder.registers.socket);
        SC_THREAD(run); SC_THREAD(watchdog);
    }
    void watchdog() { wait(1,SC_MS); std::cerr<<"watchdog expired\n"; sc_stop(); }
    void run() {
        try {
            reset_n.write(false); wait(40,SC_NS); reset_n.write(true); wait(40,SC_NS);
            auto source=pattern(); host.transfer(true,cmb_base,source); host.transfer(true,ref_base,source);
            std::vector<unsigned char> guard(400,0xa5);
            host.transfer(true,ref_base+384,guard); host.transfer(true,nal_base-8,guard);
            if(fault) { memory.error_begin=cmb_base; memory.error_end=cmb_base+16; }
            if(stage==4) {
                host.configure(16,16,cmb_base,ref_base,nal_base);
                require(host.write(h264::reg::SCON,3)==tlm::TLM_OK_RESPONSE,"START");
                const auto r=host.wait_for_completion(irq,sc_time(100,SC_US));
                require(!r.timed_out && r.quiescent,"controller completion");
                require(bool(r.status&h264::reg::ERROR)==fault && bool(r.status&h264::reg::NORMAL)!=fault,"STATUS");
                require(r.words==(fault?0u:1u),"STM_LEN");
                if(!fault) require(r.output==std::vector<unsigned char>({0x54,0x45,0x53,0x74}),"output bytes");
                wait(1,SC_NS); require(!irq.read(),"IRQ acknowledge");
            } else {
                h264::FrameConfig c; c.width=c.height=16; c.cmb=cmb_base; c.refm=ref_base; c.nal=nal_base;
                unsigned progress=0; pipeline->set_nal_progress([&](uint64_t,unsigned n){progress+=n;});
                const auto g=encoder.reset_domain.generation;
                pipeline->begin_activation(c,g); pipeline->execute(c,g,0); pipeline->end_activation(c,g,1);
                require(progress==1,"accepted NAL progress");
                memory.error_begin=cmb_base; memory.error_end=cmb_base+16;
                pipeline->cmb.fetch_macroblock(0,0); pipeline->pump(g);
                require(pipeline->cmb.failed() && !pipeline->cmb.done(),"failed CMB response");
                memory.error_begin=memory.error_end=0;
                pipeline->cmb.fetch_macroblock(0,0); pipeline->pump(g);
                require(pipeline->cmb.done(),"CMB retry");
            }
            if(!fault) {
                std::vector<unsigned char> actual(384); host.transfer(false,ref_base+384,actual);
                require(actual==source,"reference output");
                actual.assign(20,0); host.transfer(false,nal_base-8,actual);
                require(std::all_of(actual.begin(),actual.begin()+8,[](unsigned char x){return x==0xa5;}),"leading byte lanes");
                require(actual[8]==0x54 && actual[9]==0x45 && actual[10]==0x53 && actual[11]==0x74,"NAL payload");
                require(std::all_of(actual.begin()+12,actual.end(),[](unsigned char x){return x==0xa5;}),"trailing byte lanes");
                for(auto count:pipeline->transfers) require(count>0,"four DMA clients exercised");
                if(stage==2 || stage==4) require(pipeline->blocks==18,"intra blocks");
                if(stage>=3) require(pipeline->samples==384,"inter samples");
            }
            reset_n.write(false); wait(40,SC_NS);
            require(!irq.read() && !pipeline->intra.ready() && !pipeline->inter.done(),"idle reset");
            passed=true; std::cout<<"Prediction stage "<<stage<<" PASS (fixture backend)\n";
        } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; }
        sc_stop();
    }
};
inline int run(unsigned stage,int argc,char** argv) {
    try { Bench b("bench",stage,argc>1?unsigned(std::stoul(argv[1])):32,argc>2 && std::string(argv[2])=="fault");
        sc_start(); return b.passed?0:1;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
}
