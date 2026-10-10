#include <h264/intra/intra_tlm.h>
#include <tlm_utils/simple_initiator_socket.h>
#include "prediction_oracle.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace h264::intra;
using namespace sc_core;

struct ContractBench : sc_module {
    tlm_utils::simple_initiator_socket<ContractBench> driver{"driver"};
    sc_signal<bool> reset_n{"reset_n"},enable{"enable"};
    Options config;
    IntraTlm dut;
    sc_event inspect_feedback;
    Block observed;
    bool zero,inspection_ok=false,passed=false,finished=false;
    unsigned checks=0,ready_events=0,done_events=0;
    SC_HAS_PROCESS(ContractBench);
    static Options options(bool zero) {
        Options o; o.coded_width=48; o.coded_height=32;
        o.reference_latency=sc_time(zero?0:9,SC_NS);
        o.candidate_latency=sc_time(zero?0:6,SC_NS);
        o.compare_latency=sc_time(zero?0:3,SC_NS);
        o.replay_latency=sc_time(zero?0:8,SC_NS);
        o.feedback_latency=sc_time(zero?0:11,SC_NS);
        return o;
    }
    ContractBench(sc_module_name name,bool instant):sc_module(name),config(options(instant)),dut("dut",config),zero(instant) {
        driver.bind(dut.target_socket); dut.reset_n(reset_n); dut.frame_enable(enable);
        SC_THREAD(run); SC_THREAD(observer); SC_THREAD(events); SC_THREAD(watchdog);
    }
    void require(bool ok,const char* why) { ++checks; if(!ok) throw std::runtime_error(why); }
    tlm::tlm_response_status raw(Operation op,uint8_t* data,unsigned length,Extension* ext,
          unsigned stream=0,tlm::tlm_command command=tlm::TLM_IGNORE_COMMAND,bool bytes=false) {
        tlm::tlm_generic_payload tx; sc_time delay=SC_ZERO_TIME;
        tx.set_command(command==tlm::TLM_IGNORE_COMMAND ?
            (op==Operation::Replay?tlm::TLM_READ_COMMAND:tlm::TLM_WRITE_COMMAND):command);
        tx.set_address(uint64_t(op)); tx.set_data_ptr(data); tx.set_data_length(length);
        tx.set_streaming_width(stream?stream:length); uint8_t mask=255;
        if(bytes) tx.set_byte_enable_ptr(&mask);
        tx.set_dmi_allowed(true); if(ext) tx.set_extension(ext);
        driver->b_transport(tx,delay);
        if(ext) tx.clear_extension<Extension>();
        require(!tx.is_dmi_allowed(),"every response denies DMI");
        require(!dut.transport_active(),"transport flag clear after response");
        return tx.get_response_status();
    }
    tlm::tlm_response_status job(Operation op,std::vector<uint8_t>& data,Extension& ext,sc_time incoming=SC_ZERO_TIME) {
        tlm::tlm_generic_payload tx;
        tx.set_command(op==Operation::Replay?tlm::TLM_READ_COMMAND:tlm::TLM_WRITE_COMMAND);
        tx.set_address(uint64_t(op)); tx.set_data_ptr(data.data());
        tx.set_data_length(unsigned(data.size())); tx.set_streaming_width(unsigned(data.size()));
        tx.set_extension(&ext); driver->b_transport(tx,incoming); tx.clear_extension<Extension>();
        require(incoming==SC_ZERO_TIME && !dut.transport_active(),"incoming delay consumed and call drained");
        return tx.get_response_status();
    }
    void start() {
        enable.write(false); reset_n.write(true); wait(1,SC_NS);
        enable.write(true); wait(1,SC_NS);
        require(!dut.ready() && !dut.block_done(),"frame clears authoritative flags");
    }
    void import(Block block,const std::vector<uint8_t>& pixels) {
        Extension ext; ext.block=block; auto copy=pixels;
        require(job(Operation::ImportReconstructed,copy,ext)==tlm::TLM_OK_RESPONSE,"import decoded samples");
    }
    void observer() {
        while(true) {
            wait(inspect_feedback); wait(1,SC_NS);
            inspection_ok=dut.transport_active() && dut.ready() && !dut.block_done()
                && dut.core().busy() && !dut.core().committed_mode(observed);
        }
    }
    void events() {
        while(true) {
            wait(dut.predictor_ready | dut.block_completed);
            if(dut.predictor_ready.triggered()) ++ready_events;
            if(dut.block_completed.triggered()) ++done_events;
        }
    }
    void watchdog() { wait(100,SC_US); if(!finished) { std::cerr<<"FAIL contract watchdog\n"; sc_stop(); } }
    void complete(Extension& ext,std::vector<uint8_t>& source,unsigned candidates,
                  int expected_mode=-1,uint64_t expected_cost=0) {
        const auto begin=sc_time_stamp(); const unsigned old_ready=ready_events,old_done=done_events;
        require(job(Operation::Evaluate,source,ext,sc_time(7,SC_NS))==tlm::TLM_OK_RESPONSE,"timed evaluation");
        require(sc_time_stamp()-begin==sc_time(7,SC_NS)+config.reference_latency+
            candidates*(config.candidate_latency+config.compare_latency),"exact duration per legal candidate");
        require(ext.decision && ext.decision->candidates.size()==candidates,"exact legal candidate count");
        const Decision snapshot=*ext.decision; const Block block=snapshot.block;
        if(expected_mode>=0) {
            require(snapshot.mode==unsigned(expected_mode) && snapshot.cost==expected_cost,
                "deterministic tied minimum retains first legal candidate");
            unsigned tied=0;
            for(const auto& candidate:snapshot.candidates) if(candidate.cost==expected_cost) ++tied;
            require(tied>=2,"tie vector really contains multiple minimum-cost candidates");
        }
        require(dut.ready() && !dut.block_done() && !dut.core().committed_mode(block),"winner is not committed early");
        wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        require(ready_events==old_ready+1 && done_events==old_done,"ready event without completion event");
        const auto expected=intra_test::prediction(block.kind,snapshot.mode,snapshot.references);
        require(snapshot.predictor==expected,"winner predictor independent oracle");
        std::vector<uint8_t> replay(source.size(),0);
        // Metadata changed by caller must never redirect token-based operations.
        ext.block={static_cast<Plane>(99),static_cast<Kind>(99),UINT32_MAX,UINT32_MAX};
        for(unsigned repeat=0;repeat<2;++repeat) {
            const auto replay_begin=sc_time_stamp();
            require(job(Operation::Replay,replay,ext)==tlm::TLM_OK_RESPONSE,"repeat token replay with altered metadata");
            require(sc_time_stamp()-replay_begin==config.replay_latency,"configured replay latency");
            require(replay==expected && ext.decision->token==snapshot.token && ext.decision->mode==snapshot.mode
                && ext.decision->cost==snapshot.cost && ext.decision->block.x==block.x && ext.decision->block.plane==block.plane,
                "entire winner metadata preserved");
            require(ext.decision->references.top==snapshot.references.top && ext.decision->references.left==snapshot.references.left
                && ext.decision->references.upper_left==snapshot.references.upper_left
                && ext.decision->references.has_top==snapshot.references.has_top
                && ext.decision->references.has_left==snapshot.references.has_left
                && ext.decision->references.has_upper_left==snapshot.references.has_upper_left
                && ext.decision->references.has_upper_right==snapshot.references.has_upper_right,"reference snapshot preserved");
            for(size_t i=0;i<source.size();++i)
                require(ext.decision->residual[i]==int(source[i])-expected[i],"full signed replay residual");
            wait(3,SC_NS); require(!dut.block_done(),"stall does not create done");
        }
        observed=block; inspection_ok=false;
        if(!zero) inspect_feedback.notify(SC_ZERO_TIME);
        std::vector<uint8_t> decoded(source.size(),17);
        const auto feedback_begin=sc_time_stamp();
        require(job(Operation::Reconstruct,decoded,ext)==tlm::TLM_OK_RESPONSE,"feedback accepted");
        require(sc_time_stamp()-feedback_begin==config.feedback_latency,"configured feedback latency");
        if(!zero) require(inspection_ok,"memory/mode and done deferred during feedback wait");
        require(ext.block_done && dut.block_done() && !dut.ready() && !dut.core().busy()
            && dut.core().committed_mode(block)==snapshot.mode,"atomic decoded/mode commit and done");
        wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        require(done_events==old_done+1,"completion event after memory commit");
        Extension bad; bad.block.x=2;
        std::vector<uint8_t> pixels(16);
        require(job(Operation::Evaluate,pixels,bad)==tlm::TLM_GENERIC_ERROR_RESPONSE && dut.block_done(),"failed evaluation preserves sticky done");
        wait(3,SC_NS); require(dut.block_done(),"done remains sticky under idle stall");
    }
    void run() {
        try {
            // Every possible candidate count across the supported block classes.
            for(unsigned count:{1u,2u,3u,4u,6u,9u}) {
                start(); Extension ext;
                if(count==1) ext.block={Plane::V,Kind::Chroma8x8,0,0};
                if(count==2) {
                    import({Plane::U,Kind::Chroma8x8,0,0},std::vector<uint8_t>(64,128));
                    ext.block={Plane::U,Kind::Chroma8x8,8,0};
                }
                if(count==3 || count==4) {
                    import({Plane::Y,Kind::Luma16x16,16,0},std::vector<uint8_t>(256,128));
                    import({Plane::Y,Kind::Luma16x16,0,16},std::vector<uint8_t>(256,128));
                    if(count==4) import({Plane::Y,Kind::Luma16x16,0,0},std::vector<uint8_t>(256,128));
                    ext.block={Plane::Y,Kind::Luma16x16,16,16};
                }
                if(count==6 || count==9) {
                    if(count==9) import({Plane::Y,Kind::Luma4x4,0,0},std::vector<uint8_t>(16,128));
                    import({Plane::Y,Kind::Luma4x4,4,0},std::vector<uint8_t>(16,128));
                    import({Plane::Y,Kind::Luma4x4,0,4},std::vector<uint8_t>(16,128));
                    ext.block={Plane::Y,Kind::Luma4x4,4,4};
                }
                std::vector<uint8_t> pixels(ext.block.size()*ext.block.size());
                for(size_t i=0;i<pixels.size();++i) pixels[i]=i%2 ? 255:0;
                ext.metric=Metric::Satd; complete(ext,pixels,count);
            }
            // Deliberate ties for Y4/Y16/U8/V8, both metrics, nonzero distortion
            // and penalties excluding numeric mode0 from the minimum group.
            for(const Block prototype:{Block{Plane::Y,Kind::Luma4x4,0,0},
                Block{Plane::Y,Kind::Luma16x16,0,0},Block{Plane::U,Kind::Chroma8x8,0,0},
                Block{Plane::V,Kind::Chroma8x8,0,0}})
                for(const Metric metric:{Metric::Sad,Metric::Satd}) for(unsigned scenario=0;scenario<3;++scenario) {
                    start(); const unsigned n=prototype.size();
                    Block block=prototype;
                    import(block,std::vector<uint8_t>(n*n,128));
                    block.x=n; import(block,std::vector<uint8_t>(n*n,128));
                    block.x=0; block.y=n; import(block,std::vector<uint8_t>(n*n,128));
                    block.x=n;
                    Extension ext; ext.block=block; ext.metric=metric;
                    const unsigned value=scenario==0 ? 128:129;
                    std::vector<uint8_t> source(n*n,uint8_t(value));
                    const uint64_t distortion=scenario==0 ? 0:metric==Metric::Sad ? n*n:n*n/2;
                    if(scenario==2) {
                        ext.mode_penalty.fill(1000);
                        ext.mode_penalty[1]=ext.mode_penalty[2]=17;
                    }
                    complete(ext,source,prototype.kind==Kind::Luma4x4 ? 9:4,
                        scenario==2 ? 1:0,distortion+(scenario==2 ? 17:0));
                }
            // Force real right-edge winning directional modes, including modes
            // that consume substituted E..H, and verify every replay sample.
            for(unsigned wanted:{3u,7u}) {
                start(); std::vector<uint8_t> row(256);
                for(unsigned y=0;y<16;++y) for(unsigned x=0;x<16;++x) row[y*16+x]=uint8_t(3*x+11*y);
                import({Plane::Y,Kind::Luma16x16,32,0},row);
                Extension ext; ext.block={Plane::Y,Kind::Luma4x4,44,16};
                auto refs=dut.core().resolve(ext.block);
                require(refs.has_top && !refs.has_upper_right,"right-edge availability");
                for(unsigned i=4;i<8;++i) require(refs.top[i]==refs.top[3],"right-edge E..H replicate D");
                auto source=intra_test::prediction(ext.block.kind,wanted,refs);
                ext.mode_penalty.fill(1000000); ext.mode_penalty[wanted]=0;
                complete(ext,source,4);
                require(dut.core().committed_mode({Plane::Y,Kind::Luma4x4,44,16})==wanted,"directional winning mode committed at right edge");
            }
            start(); Extension ext; std::vector<uint8_t> source(16,128);
            require(job(Operation::Evaluate,source,ext)==tlm::TLM_OK_RESPONSE,"negative-case owner setup");
            const auto token=ext.token; const auto predictor=ext.decision->predictor;
            require(raw(Operation::Evaluate,source.data(),16,nullptr)==tlm::TLM_GENERIC_ERROR_RESPONSE,"missing extension");
            require(raw(Operation::Replay,nullptr,16,&ext)==tlm::TLM_BURST_ERROR_RESPONSE,"null buffer");
            require(raw(Operation::Replay,source.data(),0,&ext)==tlm::TLM_BURST_ERROR_RESPONSE,"empty buffer");
            require(raw(Operation::Replay,source.data(),16,&ext,1)==tlm::TLM_BURST_ERROR_RESPONSE,"short stream");
            require(raw(Operation::Replay,source.data(),16,&ext,0,tlm::TLM_WRITE_COMMAND)==tlm::TLM_COMMAND_ERROR_RESPONSE,"wrong replay command");
            require(raw(Operation::Replay,source.data(),16,&ext,0,tlm::TLM_IGNORE_COMMAND,true)==tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,"byte enables");
            require(raw(static_cast<Operation>(99),source.data(),16,&ext)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"invalid operation");
            require(raw(Operation::Replay,source.data(),15,&ext)==tlm::TLM_BURST_ERROR_RESPONSE,"wrong replay size");
            require(raw(Operation::Reconstruct,source.data(),15,&ext)==tlm::TLM_BURST_ERROR_RESPONSE,"wrong feedback size");
            require(job(Operation::ImportReconstructed,source,ext)==tlm::TLM_GENERIC_ERROR_RESPONSE,"pending import rejected");
            require(job(Operation::Reconstruct,source,ext)==tlm::TLM_GENERIC_ERROR_RESPONSE,"feedback before replay rejected");
            ++ext.token.sequence;
            require(job(Operation::Replay,source,ext)==tlm::TLM_GENERIC_ERROR_RESPONSE,"wrong token"); ext.token=token;
            require(dut.ready() && dut.core().replay(token).predictor==predictor && !dut.block_done(),"all malformed requests preserve owner");
            require(job(Operation::Replay,source,ext)==tlm::TLM_OK_RESPONSE,"owner replay after errors");
            require(job(Operation::Reconstruct,source,ext)==tlm::TLM_OK_RESPONSE,"owner feedback after errors");
            for(const Block invalid:{Block{static_cast<Plane>(99),Kind::Luma4x4,0,0},
                Block{Plane::Y,static_cast<Kind>(99),0,0},Block{Plane::U,Kind::Luma4x4,0,0},
                Block{Plane::Y,Kind::Luma4x4,2,0},Block{Plane::Y,Kind::Luma4x4,48,0}}) {
                ext=Extension{}; ext.block=invalid;
                require(job(Operation::Evaluate,source,ext)==tlm::TLM_GENERIC_ERROR_RESPONSE && dut.block_done(),"invalid block preserves completed context");
            }
            ext=Extension{}; ext.metric=static_cast<Metric>(99);
            require(job(Operation::Evaluate,source,ext)==tlm::TLM_GENERIC_ERROR_RESPONSE && dut.block_done(),"invalid metric preserves done");
            require(dut.core().committed_mode(Block{})==2,"all invalid requests preserve committed mode");
            passed=true; std::cout<<"PASS extended TLM (zero="<<zero<<"): "<<checks<<" checks at "<<sc_time_stamp()<<'\n';
        } catch(const std::exception& e) { std::cerr<<"FAIL extended TLM: "<<e.what()<<'\n'; }
        finished=true; sc_stop();
    }
};
int sc_main(int argc,char** argv) {
    ContractBench bench("bench",argc>1 && std::string(argv[1])=="zero"); sc_start(); return bench.passed ? 0:1;
}
