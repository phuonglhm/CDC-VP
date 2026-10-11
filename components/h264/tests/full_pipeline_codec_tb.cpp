#include "Coding_Reconstruction_Test/codec_pipeline.h"
#include "Coding_Reconstruction_Test/communication_bench.h"
#include <fstream>
using namespace prediction_test;
struct CodecBench:sc_module {
    sc_signal<bool> rst{"rst"},irq{"irq"};h264::EncoderVp encoder;h264::HostDriver host{"host"};
    h264::mem::MemoryMap memory{"memory"};communication_test::Bus bus{"bus",50};
    codec_integration::Pipeline* p;bool passed=false,bad_irq=false;unsigned qp,irq_count=0;std::string prefix,scenario;
    SC_HAS_PROCESS(CodecBench);
    CodecBench(sc_module_name name,unsigned width,unsigned q,std::string path,std::string which)
        :sc_module(name),encoder("encoder",width,h264::SequenceParameters{4,q,16384},sc_time(8,SC_NS),
          [](sc_module_name n,h264::ResetDomain& r,h264::DmaTransport& d)->std::unique_ptr<h264::FrameExecutorIf>{return std::make_unique<codec_integration::Pipeline>(n,r,d);}),
          p(dynamic_cast<codec_integration::Pipeline*>(encoder.pipeline.get())),qp(q),prefix(path),scenario(which) {
        encoder.rstn(rst);encoder.irq(irq);p->reset_n(rst);host.registers.bind(encoder.registers.socket);
        host.memory.bind(bus.input);encoder.dma.memory.bind(bus.input);bus.output.bind(memory.socket);
        host.programming={4,qp,16384};host.programming.gop_m=4;host.programming.log2_fn=4;host.programming.log2_poc=4;host.programming.force_log=true;
        if(scenario=="syntax" || scenario=="syntax_negative"){host.programming.log2_fn=6;host.programming.log2_poc=7;}
        SC_THREAD(run);SC_THREAD(watchdog);SC_METHOD(check_irq);sensitive<<irq.posedge_event();dont_initialize();
    }
    void watchdog(){wait(20,SC_MS);std::cerr<<"codec watchdog\n";sc_stop();}
    void check_irq(){++irq_count;if(bus.active || !encoder.dma.idle() || encoder.registers.busy())bad_irq=true;}
    void reset(){rst.write(false);wait(80,SC_NS);auto deadline=sc_time_stamp()+sc_time(50,SC_US);
        while((!encoder.dma.idle() || p->queue.busy()) && sc_time_stamp()<deadline)wait(1,SC_NS);
        require(encoder.dma.idle() && !p->queue.busy(),"reset drain");rst.write(true);wait(40,SC_NS);require(!irq.read(),"reset IRQ");}
    std::vector<unsigned char> source;
    void prepare(unsigned variation){source.resize(4*1536);for(unsigned i=0;i<source.size();++i)source[i]=(i*13+7*(i/16)+31+variation*37)%256;
        if(scenario.rfind("filter",0)==0) for(unsigned f=0;f<4;++f) for(unsigned plane=0;plane<3;++plane) {
            unsigned n=plane?16:32,base=f*1536+(plane==0?0:plane==1?1024:1280);
            for(unsigned y=0;y<n;++y) for(unsigned x=0;x<n;++x) source[base+y*n+x]=96+2*(x/4)+2*(y/4)+f*2+variation;
        }
        host.transfer(true,cmb_base,source);std::vector<unsigned char> ref(3*1536+16,0xa5),out(16384+16,0xa5);
        host.transfer(true,ref_base,ref);host.transfer(true,nal_base-8,out);host.configure(32,32,cmb_base,ref_base,nal_base);
        if(scenario=="syntax" || scenario=="syntax_negative") require(host.write(h264::reg::SPARA2,(2u<<6)|(scenario=="syntax_negative"?61u:3u))==tlm::TLM_OK_RESPONSE,"crop/QP delta config");
        if(scenario=="filter" || scenario=="filter_offsets" || scenario=="filter_negative") require(host.write(h264::reg::DFCON,scenario=="filter_offsets"?(1u|(4u<<6)|(2u<<1)):scenario=="filter_negative"?(1u|(28u<<6)|(30u<<1)):1u)==tlm::TLM_OK_RESPONSE,"DF config");
    }
    void start(){require(host.write(h264::reg::SCON,3)==tlm::TLM_OK_RESPONSE,"start");}
    void save(const std::string& name,const std::vector<unsigned char>& bytes){std::ofstream f(name,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(f),"artifact write");}
    void complete(bool error,unsigned pass){unsigned before=irq_count;start();auto result=host.wait_for_completion(irq,sc_time(5,SC_MS));
        require(!result.timed_out && result.quiescent && !bad_irq,"completion/drain");require(irq_count==before+1,"IRQ count");
        require(bool(result.status&h264::reg::ERROR)==error && bool(result.status&h264::reg::NORMAL)!=error,"completion status");
        if(!error){require(result.status==(h264::reg::NORMAL|4),"frame count");require(result.output==p->expected_output && result.words*4==result.output.size(),"DDR output/count");
            if(p->filter_enabled) require(p->changed_filter_samples>0,"active DF must change samples");
            require(p->intra_feedback==72 && p->inter_samples==4608 && p->chroma_groups==32 && p->coded_blocks==384,"actual block participation");
            std::vector<unsigned char> recon;
            for(unsigned f=0;f<4;++f){require(std::equal(p->source_seen[f].begin(),p->source_seen[f].end(),source.begin()+f*1536),"source DMA");recon.insert(recon.end(),p->expected_reference[f].begin(),p->expected_reference[f].end());
                if(f>=1){std::vector<unsigned char> stored(1536);host.transfer(false,ref_base+((f+1)%3)*1536,stored);require(stored==p->expected_reference[f],"reference DDR");}}
            for(auto address:{nal_base-8,nal_base+unsigned(result.output.size()),ref_base+3*1536}){std::vector<unsigned char> guard(8);host.transfer(false,address,guard);require(std::all_of(guard.begin(),guard.end(),[](auto x){return x==0xa5;}),"guard");}
            save(prefix+"_"+std::to_string(pass)+".h264",result.output);save(prefix+"_"+std::to_string(pass)+".yuv",recon);
        }
        auto count=bus.dma_trace.size();wait(100,SC_NS);require(!irq.read() && bus.dma_trace.size()==count,"late IRQ/DMA");}
    void run(){try{reset();prepare(0);
        if(scenario=="nal_fault"){bus.fault_begin=nal_base;bus.fault_end=nal_base+16;complete(true,0);bus.fault_begin=bus.fault_end=0;reset();}
        else if(scenario=="invalid_width" || scenario=="invalid_df_offsets") {
            if(scenario=="invalid_width") {auto v=host.read(h264::reg::SPARA1);require(host.write(h264::reg::SPARA1,(v&~(15u<<20))|(3u<<20))==tlm::TLM_OK_RESPONSE,"set invalid syntax width");}
            else require(host.write(h264::reg::DFCON,1u|(3u<<6))==tlm::TLM_OK_RESPONSE,"odd DF offset");
            complete(true,0);reset();
        }
        else if(scenario=="reset"){start();do{wait(bus.dma_started);}while(!bus.current_write || bus.current_address<nal_base);unsigned before=irq_count;reset();wait(100,SC_NS);require(irq_count==before,"stale IRQ after reset");}
        else require(scenario=="normal" || scenario=="syntax" || scenario=="syntax_negative" || scenario=="filter" || scenario=="filter_offsets" || scenario=="filter_negative","unknown scenario");
        for(unsigned pass=0;pass<2;++pass){if(pass)reset();prepare(pass+1);complete(false,pass);}
        passed=true;std::cout<<"PASS integrated control/DMA/prediction/TQ/CAVLC/syntax pipeline\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';}sc_stop();}
};
int sc_main(int argc,char** argv){try{if(argc!=5)throw std::invalid_argument("WIDTH QP PREFIX SCENARIO");CodecBench b("bench",std::stoul(argv[1]),std::stoul(argv[2]),argv[3],argv[4]);sc_start();return b.passed?0:1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
