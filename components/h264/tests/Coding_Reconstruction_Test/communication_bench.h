#pragma once
#include "communication_pipeline.h"
#include "memory_map.h"
#include "nal_formatter.h"
#include <tuple>
namespace communication_test {
struct Bus:sc_module {
    tlm_utils::multi_passthrough_target_socket<Bus> input{"input"};
    tlm_utils::simple_initiator_socket<Bus> output{"output"};
    struct Trace {uint64_t address;unsigned size;bool write;sc_time end;};
    std::vector<Trace> dma_trace;
    sc_event dma_started;
    uint64_t current_address=0; bool current_write=false;
    unsigned active=0;uint64_t fault_begin=0,fault_end=0;unsigned stall;
    Bus(sc_module_name n,unsigned ns):sc_module(n),stall(ns){input.register_b_transport(this,&Bus::transport);}
    void transport(int id,tlm::tlm_generic_payload& tx,sc_time& delay) {
        // Host binds first, DMA second. Only DMA receives injected faults/stalls.
        if(id==1) {++active;current_address=tx.get_address();current_write=tx.is_write();dma_started.notify(SC_ZERO_TIME);}
        if(id==1 && fault_end>fault_begin && tx.get_address()<fault_end &&
                tx.get_address()+tx.get_data_length()>fault_begin)
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        else output->b_transport(tx,delay);
        if(id==1) {
            delay+=sc_time(double(stall+(dma_trace.size()%3)),SC_NS);
            h264::consume_delay(delay);
            dma_trace.push_back({tx.get_address(),tx.get_data_length(),tx.is_write(),sc_time_stamp()});
            --active;
        }
    }
};
class Bench:public sc_module {
public:
    sc_signal<bool> rst{"reset"},irq{"irq"};
    h264::EncoderVp encoder;
    h264::HostDriver host{"host"};
    h264::mem::MemoryMap memory{"nguyen_memory"};
    Bus bus;
    tlm_utils::simple_initiator_socket<Bench> probe{"probe"};
    Pipeline* p;
    bool full,passed=false,behavior;
    unsigned size,frames,frame_bytes,capacity;
    std::string scenario;
    std::vector<unsigned char> input_source;
    unsigned irq_count=0;
    sc_time last_irq;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name n,bool f,unsigned width,unsigned stall,std::string s,unsigned edge=16,unsigned count=2,bool behavioral=false)
      :sc_module(n),encoder("encoder",width,h264::SequenceParameters{count,26,s=="capacity"?64u:behavioral?16384u:4096u},sc_time(8,SC_NS),
        [f,edge,count,behavioral](sc_module_name name,h264::ResetDomain& r,h264::DmaTransport& dma)->std::unique_ptr<h264::FrameExecutorIf>{
            return std::make_unique<Pipeline>(name,r,dma,f,edge,count,behavioral);
        }),bus("bus",stall),p(dynamic_cast<Pipeline*>(encoder.pipeline.get())),full(f),behavior(behavioral),size(edge),frames(count),frame_bytes(edge*edge*3/2),capacity(s=="capacity"?64:behavioral?16384:4096),scenario(std::move(s)) {
        encoder.rstn(rst);encoder.irq(irq);p->reset_n(rst);
        host.registers.bind(encoder.registers.socket);
        host.memory.bind(bus.input);encoder.dma.memory.bind(bus.input);probe.bind(bus.input);
        bus.output.bind(memory.socket);
        host.programming={frames,26,capacity};host.programming.gop_m=full?frames:1;
        SC_THREAD(run);SC_THREAD(watchdog);SC_METHOD(irq_check);sensitive<<irq.posedge_event();dont_initialize();
    }
    void watchdog(){wait(20,SC_MS);std::cerr<<"watchdog\n";sc_stop();}
    void irq_check(){
        ++irq_count;last_irq=sc_time_stamp();
        if(bus.active || !encoder.dma.idle() || encoder.registers.busy()) {
            std::cerr<<"premature IRQ\n";passed=false;sc_stop();
        }
    }
    void reset(){
        rst.write(false);wait(80,SC_NS);
        auto deadline=sc_time_stamp()+sc_time(20,SC_US);
        while((!encoder.dma.idle() || p->queue.busy()) && sc_time_stamp()<deadline) wait(1,SC_NS);
        require(encoder.dma.idle() && !p->queue.busy(),"reset drain timeout");
        rst.write(true);wait(40,SC_NS);
        require(!irq.read() && encoder.dma.idle(),"reset quiescence");
        unsigned char v[4]={};
        for(auto* port:{&p->tq_port,&p->ec_port,&p->filter_port})
            require(p->raw(*port,0x40,false,v,4)==tlm::TLM_OK_RESPONSE && h264::load_le(v)==0,"reset VALID");
    }
    void protocol_checks() {
        h264::ec::EcResult reservoir{};bool overflow=false;
        h264::ec::BitWriter bw(reservoir.nal_stream,reservoir.stream_length);
        try {for(unsigned i=0;i<129;++i) bw.write_bits(0,8);}
        catch(const std::overflow_error&) {overflow=true;}
        require(overflow,"EC reservoir must not overrun");
        reservoir={};reservoir.stream_length=128;overflow=false;
        try {h264::ec::NalFormatter::wrap_nal_unit(reservoir,3,5);}
        catch(const std::overflow_error&) {overflow=true;}
        require(overflow,"NAL formatter must not truncate");
        unsigned char start=0x80;
        require(p->raw(p->tq_port,0xc,true,&start,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"TQ incomplete START");
        require(p->raw(p->ec_port,0x28,true,&start,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"EC incomplete START");
        require(p->raw(p->filter_port,0x28,true,&start,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"DF incomplete START");
        unsigned char bad=5;
        require(p->raw(p->filter_port,0x20,true,&bad,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"DF invalid bS");
        bad=52;require(p->raw(p->tq_port,8,true,&bad,1)==tlm::TLM_GENERIC_ERROR_RESPONSE,"TQ invalid QP");
        unsigned char invalid_length[2]={26,0};
        require(p->raw(p->ec_port,0x20,true,invalid_length,2)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"EC exact register length");
        unsigned char data[4]={1,2,3,4},out[4]={99,99,99,99},mask[4]={255,0,255,0};
        auto call=[&](tlm::tlm_command cmd,uint64_t address,unsigned char* bytes,unsigned char* be,unsigned bn) {
            tlm::tlm_generic_payload tx;tx.set_command(cmd);tx.set_address(address);tx.set_data_ptr(bytes);
            tx.set_data_length(4);tx.set_streaming_width(4);tx.set_byte_enable_ptr(be);tx.set_byte_enable_length(bn);
            sc_time d=SC_ZERO_TIME;probe->b_transport(tx,d);h264::consume_delay(d);return tx.get_response_status();
        };
        require(call(tlm::TLM_WRITE_COMMAND,0x10000000,data,nullptr,0)==tlm::TLM_OK_RESPONSE,"external write");
        require(call(tlm::TLM_READ_COMMAND,0x10000000,out,mask,4)==tlm::TLM_OK_RESPONSE,"masked read");
        require(out[0]==1 && out[1]==99 && out[2]==3 && out[3]==99,"disabled read lanes changed");
        require(call(tlm::TLM_WRITE_COMMAND,0x10000000,data,mask,0)==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"zero BE length");
        require(call(tlm::TLM_IGNORE_COMMAND,0x10000000,data,nullptr,0)==tlm::TLM_COMMAND_ERROR_RESPONSE,"invalid command");
        require(call(tlm::TLM_READ_COMMAND,UINT64_MAX-1,data,nullptr,0)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"address overflow");
    }
    void prepare(unsigned variation) {
        std::vector<unsigned char> source(frame_bytes*frames);
        for(unsigned i=0;i<source.size();++i) source[i]=(13*i+7*(i/16)+31+variation*17)%256;
        if(scenario=="filter" || scenario=="filter_toggle" || scenario=="filter_offsets")
            for(unsigned f=0;f<frames;++f) for(unsigned plane=0;plane<3;++plane) {
                unsigned n=plane?size/2:size,base=plane==0?0:plane==1?size*size:size*size*5/4;
                for(unsigned y=0;y<n;++y) for(unsigned x=0;x<n;++x)
                    source[f*frame_bytes+base+y*n+x]=128+2*(x/4)+2*(y/4)+2*f+variation;
            }
        if(behavior) for(unsigned plane=0;plane<3;++plane) {
            unsigned n=plane?8:16,stride=plane?size/2:size,base=plane==0?0:plane==1?size*size:size*size*5/4;
            for(unsigned y=0;y<n;++y) for(unsigned x=0;x<n;++x) source[base+y*stride+x]=128;
        }
        input_source=source;host.transfer(true,cmb_base,source);
        std::vector<unsigned char> ref(3*frame_bytes+16,0xa5);host.transfer(true,ref_base,ref);
        std::vector<unsigned char> output(capacity+16,0xa5);host.transfer(true,nal_base-8,output);
        host.configure(size,size,cmb_base,ref_base,nal_base);
        if(scenario=="filter" || scenario=="filter_toggle" || scenario=="filter_offsets")
            require(host.write(h264::reg::DFCON,scenario=="filter_offsets" ? (1u | (4u<<6) | (2u<<1)) : 1u)==tlm::TLM_OK_RESPONSE,"DF enable");
    }
    void start(){require(host.write(h264::reg::SCON,3)==tlm::TLM_OK_RESPONSE,"START");}
    void activation(bool expect_error,bool already_started=false) {
        size_t trace_start=bus.dma_trace.size();unsigned previous_irq=irq_count;
        if(!already_started) start();auto result=host.wait_for_completion(irq,sc_time(5,SC_MS));
        require(!result.timed_out && result.quiescent,"controller timeout/drain");
        require(bool(result.status&h264::reg::ERROR)==expect_error &&
                bool(result.status&h264::reg::NORMAL)!=expect_error,"STATUS");
        require(irq_count==previous_irq+1,"IRQ count");
        require(!bus.active && encoder.dma.idle(),"pending memory after completion");
        for(size_t i=trace_start;i<bus.dma_trace.size();++i) {
            const auto& t=bus.dma_trace[i];require(t.end<=last_irq,"late completion");
            if(t.write) require((t.address>=ref_base && t.address+t.size<=ref_base+3*frame_bytes) ||
                                (t.address>=nal_base && t.address+t.size<=nal_base+capacity),"DMA write bounds");
        }
        if(!expect_error) {
            require(result.status==(h264::reg::NORMAL|frames),"frame count");
            unsigned mb_count=size*size/256;
            auto expected_passes=p->filter_enabled?2*frames*mb_count:0;
            require(p->df.pass_log().size()-p->filter_pass_begin==expected_passes,"frame-latched reference path");
            require(p->filter_enabled?p->changed_filter_samples>0:p->changed_filter_samples==0,"DF enabled/bypass behavior");
            unsigned edges=2*((size/4-1)*(size/4)+2*(size/8-1)*(size/8));
            require(p->coded_blocks==frames*mb_count*24 && p->filter_edges==frames*edges,"block/edge count");
            std::set<std::tuple<unsigned,unsigned,unsigned,unsigned>> keys;
            unsigned last_mb=0;
            for(const auto& b:p->block_events) {
                unsigned order=b.frame*mb_count+b.mby*(size/16)+b.mbx;
                require(order>=last_mb && b.frame<frames && b.plane<3,"block metadata order");
                last_mb=order;require(keys.emplace(b.frame,b.plane,b.x,b.y).second,"duplicate block metadata");
            }
            require(keys.size()==frames*mb_count*24,"block metadata coverage");
            if(behavior) require(std::set<unsigned>(p->record_lengths.begin(),p->record_lengths.end()).size()>1,"output lengths did not vary");
            require(p->expected_reference.size()==frames && p->source_seen.size()==frames,"two reconstructed pictures");
            for(unsigned f=0;f<frames;++f)
                require(std::equal(p->source_seen[f].begin(),p->source_seen[f].end(),input_source.begin()+frame_bytes*f),"CMB source scoreboard");
            auto expected=p->expected_output;
            require(result.words*4==expected.size() && result.output==expected,"output scoreboard");
            if(scenario=="bad_expected") {
                auto corrupt=expected;corrupt[10]^=1;
                require(result.output!=corrupt,"scoreboard failed to reject corrupt expected");
            }
            for(unsigned frame=frames>3?frames-3:0;frame<frames;++frame) {
                std::vector<unsigned char> got(frame_bytes);host.transfer(false,ref_base+((frame+1)%3)*frame_bytes,got);
                require(got==p->expected_reference[frame],"reference scoreboard");
            }
            if(full) require(p->intra_feedback==18*mb_count && p->inter_samples==384*(frames-1)*mb_count,"real prediction participation");
            std::vector<unsigned char> guard(8);host.transfer(false,nal_base-8,guard);
            require(std::all_of(guard.begin(),guard.end(),[](auto b){return b==0xa5;}),"leading guard");
            host.transfer(false,nal_base+expected.size(),guard);
            require(std::all_of(guard.begin(),guard.end(),[](auto b){return b==0xa5;}),"trailing guard");
            host.transfer(false,ref_base+3*frame_bytes,guard);
            require(std::all_of(guard.begin(),guard.end(),[](auto b){return b==0xa5;}),"reference guard");
        }
        wait(1,SC_NS);require(!irq.read(),"IRQ acknowledge");
        auto count=bus.dma_trace.size();wait(100,SC_NS);require(bus.dma_trace.size()==count,"late DMA");
    }
    void run(){
        try {
            reset();protocol_checks();prepare(0);
            if(scenario=="reset_active") {
                start();wait(bus.dma_started);
                unsigned before=irq_count;reset();wait(100,SC_NS);
                require(irq_count==before,"stale completion after reset");prepare(0);
            }
            if(scenario=="source_fault" || scenario=="nal_fault" || scenario=="reference_fault") {
                bus.fault_begin=scenario=="source_fault"?cmb_base:scenario=="nal_fault"?nal_base:ref_base+frame_bytes;
                bus.fault_end=bus.fault_begin+16;activation(true);bus.fault_begin=bus.fault_end=0;
                reset();protocol_checks();prepare(0);
            }
            if(behavior && scenario=="capacity") {
                activation(true);require(!p->nal.busy(),"capacity left queued NAL");
                reset();passed=true;sc_stop();return;
            }
            if(behavior && (scenario.rfind("reset_",0)==0 || scenario.rfind("error_",0)==0 ||
                           scenario=="live_config" || scenario=="early_reference")) {
                bool reset_case=scenario.rfind("reset_",0)==0;
                bool error_case=scenario.rfind("error_",0)==0;
                std::string phase=(reset_case || error_case)?scenario.substr(6):
                    scenario=="early_reference"?"reference_done":"prediction";
                bool bus_phase=phase=="nal_write" || phase=="reference_write" || phase=="sw";
                if(!bus_phase) p->hold_at=phase;
                start();
                if(bus_phase) {
                    do {wait(bus.dma_started);}
                    while(!((phase=="nal_write" && bus.current_write && bus.current_address>=nal_base) ||
                            (phase=="reference_write" && bus.current_write && bus.current_address>=ref_base && bus.current_address<nal_base) ||
                            (phase=="sw" && !bus.current_write && bus.current_address>=ref_base && bus.current_address<nal_base)));
                } else {
                    do {wait(p->checkpoint_event);}while(p->checkpoint_name!=phase);
                }
                require(encoder.registers.busy() && !irq.read(),"held work reported complete");
                if(reset_case) {
                    auto before=irq_count;reset();require(irq_count==before,"stale stage IRQ");
                } else if(error_case) {
                    p->stop_requested=true;p->release_checkpoint.notify(SC_ZERO_TIME);activation(true,true);
                    reset();
                } else {
                    require(host.write(h264::reg::DFCON,1)==tlm::TLM_COMMAND_ERROR_RESPONSE,"live DFCON mutation");
                    require(host.write(h264::reg::CMB,cmb_base+4)==tlm::TLM_COMMAND_ERROR_RESPONSE,"live source mutation");
                    // A level-held ENABLE cannot launch a second activation.
                    require(host.write(h264::reg::SCON,3)==tlm::TLM_OK_RESPONSE,"held ENABLE");
                    auto blocks=p->coded_blocks;wait(50,SC_NS);
                    require(blocks==p->coded_blocks && !irq.read(),"consumer stall advanced producer");
                    p->release_checkpoint.notify(SC_ZERO_TIME);p->hold_at.clear();activation(false,true);
                }
                p->hold_at.clear();p->stop_requested=false;prepare(0);
            }
            activation(false);prepare(1);
            if(scenario=="filter_toggle") require(host.write(h264::reg::DFCON,0)==tlm::TLM_OK_RESPONSE,"disable filter between activations");
            activation(false);reset();protocol_checks();
            passed=true;
            std::cout<<"PASS "<<(full?"full pipeline I/P":"coding/reconstruction")
                     <<" communication/behavior; "<<size<<"x"<<size<<", "<<frames<<" pictures; NOT H.264 bitstream\n";
        }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";}
        sc_stop();
    }
};
inline int run(bool full,int argc,char** argv) {
    try {
        unsigned width=argc>1?std::stoul(argv[1]):32,stall=argc>2?std::stoul(argv[2]):50;
        Bench bench("bench",full,width,stall,argc>3?argv[3]:"normal");
        sc_start();return bench.passed?0:1;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
}
