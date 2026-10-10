#include <h264/top/encoder_vp.h>
#include <h264/platform/ddr_memory.h>
#include <h264/platform/host_driver.h>
#include <h264/stubs/frame_pipeline_stub.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <filesystem>
using namespace sc_core;
using namespace h264;

// Explicit test schedule, NOT an asserted SISLAB release coding order.
class ScheduleFixture : public FramePipelineStub {
public:
    bool duplicate;
    ScheduleFixture(sc_module_name name,ResetDomain& reset,bool bad)
        : FramePipelineStub(name,reset,std::make_unique<ProcessingStub>()),duplicate(bad) {}
    PictureTask picture_task(const FrameConfig& c,unsigned local) override {
        const unsigned order[5]={0,2,1,4,3};
        const unsigned coding=c.completed_frames+local;
        const unsigned display=duplicate && coding==2?0:order[coding];
        return {coding,display,local,display%4==0?PictureType::I:display%2==0?PictureType::P:PictureType::B};
    }
};
PipelineFactory schedule_factory(bool duplicate) {
    return [duplicate](sc_module_name name,ResetDomain& reset,DmaTransport& arbiter)->std::unique_ptr<FrameExecutorIf> {
        auto p=std::make_unique<ScheduleFixture>(name,reset,duplicate);
        p->cmb.bind(arbiter.clients); p->reference.bind(arbiter.clients); p->nal.bind(arbiter.clients);
        return p;
    };
}

class Bench : public sc_module {
public:
    sc_signal<bool> rstn{"rstn"},irq{"irq"};
    EncoderVp encoder;
    DdrMemory ddr{"ddr",1<<20};
    HostDriver host{"host"};
    tlm_utils::simple_initiator_socket<Bench> dma_a{"dma_a"},dma_b{"dma_b"};
    std::string scenario;
    bool passed=false,worker_done=false;
    sc_event launch_worker;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name name,std::string test,unsigned width)
        : sc_module(name),encoder("encoder",width,SequenceParameters{1,test=="invalid_params"?52u:26u},sc_time(8,SC_NS),
          test=="release_schedule" || test=="duplicate_schedule" ? schedule_factory(test=="duplicate_schedule") : PipelineFactory(make_stub_pipeline)),scenario(test) {
        encoder.rstn(rstn); encoder.irq(irq);
        host.registers.bind(encoder.registers.socket); host.memory.bind(ddr.socket);
        encoder.dma.memory.bind(ddr.socket);
        dma_a.bind(encoder.dma.clients); dma_b.bind(encoder.dma.clients);
        if(test=="invalid_params") host.programming.qp=52;
        SC_THREAD(run); SC_THREAD(worker); SC_THREAD(watchdog);
    }
    void require(bool value,const char* message) {
        if(!value) throw std::runtime_error(message);
    }
    void watchdog() { wait(2,SC_MS); SC_REPORT_FATAL("test","simulation timeout"); }
    void write(uint32_t a,uint32_t v,unsigned mask=15) {
        require(host.write(a,v,mask)==tlm::TLM_OK_RESPONSE,"register write response");
    }
    void wait_irq() {
        wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); // Settle a previous read-clear/new start.
        while(!irq.read()) wait(irq.posedge_event());
    }
    tlm::tlm_response_status access(tlm_utils::simple_initiator_socket<Bench>& port,bool wr,
        uint64_t address,std::vector<unsigned char>& data,std::vector<unsigned char> enables={}) {
        tlm::tlm_generic_payload tx;
        tx.set_command(wr?tlm::TLM_WRITE_COMMAND:tlm::TLM_READ_COMMAND);
        tx.set_address(address); tx.set_data_ptr(data.data()); tx.set_data_length(static_cast<unsigned>(data.size()));
        tx.set_streaming_width(static_cast<unsigned>(data.size()));
        if(!enables.empty()) {
            tx.set_byte_enable_ptr(enables.data()); tx.set_byte_enable_length(static_cast<unsigned>(enables.size()));
        }
        sc_time delay=SC_ZERO_TIME; port->b_transport(tx,delay); consume_delay(delay);
        return tx.get_response_status();
    }
    void worker() {
        wait(launch_worker);
        std::vector<unsigned char> bytes(256,0x5a);
        for(unsigned i=0;i<1u;++i)
            require(access(dma_b,true,0x7000,bytes)==tlm::TLM_OK_RESPONSE,"second client response");
        worker_done=true;
    }
    std::vector<unsigned char> stage(unsigned w=16,unsigned h=16,unsigned seed=3) {
        std::vector<unsigned char> data(w*h*3/2);
        for(unsigned i=0;i<data.size();++i) data[i]=static_cast<unsigned char>(i*7+seed);
        host.transfer(true,0x1000,data);
        host.configure(w,h,0x1000,0x20000,0x60000);
        return data;
    }
    std::vector<unsigned char> golden(const std::vector<unsigned char>& source,unsigned w,unsigned h) {
        std::vector<unsigned char> out;
        for(unsigned my=0;my<h/16;++my) for(unsigned mx=0;mx<w/16;++mx) {
            uint32_t hash=2166136261u;
            for(unsigned p=0;p<3;++p) {
                const unsigned side=p?8:16,stride=p?w/2:w;
                const unsigned base=p==0?0:p==1?w*h:w*h+w*h/4;
                for(unsigned y=0;y<side;++y) for(unsigned x=0;x<side;++x)
                    hash=(hash^source[base+(my*side+y)*stride+mx*side+x])*16777619u;
            }
            for(unsigned i=0;i<4;++i) out.push_back(static_cast<unsigned char>(hash>>(8*i)));
        }
        out.insert(out.end(),{0,0,1,0x0b});
        return out;
    }
    void integration() {
        std::vector<unsigned char> previous;
        for(unsigned run=0;run<3;++run) {
            ddr.latency=sc_time(run==0?0:run==1?20:50,SC_NS);
            auto source=stage(32,32,run==2?9:3);
            write(reg::SCON,3);
            require(host.read(reg::STAT)&reg::BUSY,"BUSY must be immediate at accepted start");
            require(host.write(reg::NAL,0x70000)==tlm::TLM_COMMAND_ERROR_RESPONSE,"live buffer mutation rejected");
            wait_irq();
            const auto completion=sc_time_stamp();
            require(ddr.trace.back().write && ddr.trace.back().completed<=completion,"IRQ before memory completion");
            write(reg::SCON,0);
            const auto status=host.read(reg::STAT),words=host.read(reg::STM_LEN);
            require(status==(reg::NORMAL|1),"normal frame status");
            require(words==5,"activation length resets");
            std::vector<unsigned char> actual(words*4),reference(source.size());
            host.transfer(false,0x60000,actual);
            require(actual==golden(source,32,32),"exact stub output differs");
            host.transfer(false,0x20000+static_cast<uint32_t>(source.size()),reference);
            require(reference==source,"reference planar tile addressing");
            if(run==1) require(actual==previous,"latency changed output");
            if(run==2) require(actual!=previous,"CMB reused stale source");
            previous=actual;
            wait(SC_ZERO_TIME); require(!irq.read(),"IRQ not cleared");
        }
    }
    void registers_test() {
        write(reg::SPARA0,0x11223344); write(reg::SPARA0,0xaabbccdd,5);
        require(host.read(reg::SPARA0)==(0x11bb33dd&0x07fff3ffu),"register byte enable");
        require(host.write(reg::STAT,0)==tlm::TLM_COMMAND_ERROR_RESPONSE,"read-only register");
        require(host.write(0x2c,0)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"undefined register");
        require(host.write(1,0)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"unaligned register");
        stage(); write(reg::FMSIZE,(17u<<16)|16); write(reg::SCON,3); wait_irq();
        require(host.read(reg::STAT)==reg::ERROR,"invalid dimensions");
        wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); require(!irq.read(),"STAT read must clear IRQ");
        stage(); write(reg::SCON,3); wait_irq();
        const auto length=host.read(reg::STM_LEN);
        wait(SC_ZERO_TIME); wait(SC_ZERO_TIME); require(!irq.read(),"length read must clear IRQ");
        const auto count=ddr.trace.size();
        write(reg::SCON,3); wait(5,SC_US);
        require(ddr.trace.size()==count,"high enable must not rearm");
        write(reg::SCON,0); require(host.read(reg::STM_LEN)==length,"disable must preserve drain length");
        StickyCompletion done; done.set(7); wait(SC_ZERO_TIME);
        require(done.take(7) && !done.take(7),"sticky completion consumed exactly once");
    }
    void dma_test() {
        for(unsigned lane=0;lane<encoder.dma.bus_bytes;++lane) for(unsigned size: {1u,2u,4u}) {
            std::vector<unsigned char> initial(64,0xa5); host.transfer(true,0x8000,initial);
            std::vector<unsigned char> value(size,static_cast<unsigned char>(0x30+lane));
            require(access(dma_a,true,0x8010+lane,value)==tlm::TLM_OK_RESPONSE,"lane write");
            std::vector<unsigned char> got(64); host.transfer(false,0x8000,got);
            for(unsigned i=0;i<64;++i) require(got[i]==(i>=16+lane && i<16+lane+size?value[0]:0xa5),"lane corruption");
            std::vector<unsigned char> read(size);
            require(access(dma_a,false,0x8010+lane,read)==tlm::TLM_OK_RESPONSE && read==value,"lane read");
        }
        std::vector<unsigned char> bytes(300);
        for(unsigned i=0;i<bytes.size();++i) bytes[i]=static_cast<unsigned char>(i);
        require(access(dma_a,true,0x2ffdu,bytes)==tlm::TLM_OK_RESPONSE,"boundary write");
        std::vector<unsigned char> read(bytes.size());
        require(access(dma_a,false,0x2ffd,read)==tlm::TLM_OK_RESPONSE && read==bytes,"boundary read");
        for(const auto& s:encoder.dma.segments) {
            require(s.address/4096==(s.address+s.bytes-1)/4096,"4 KiB crossing");
            require(s.beats<=encoder.dma.max_beats,"burst limit");
        }
        std::vector<unsigned char> fill(100,0x11),masked(100,0xee),result(100);
        host.transfer(true,0x4ffd,fill);
        require(access(dma_a,true,0x4ffd,masked,{0xff,0,0})==tlm::TLM_OK_RESPONSE,"repeating enables");
        host.transfer(false,0x4ffd,result);
        for(unsigned i=0;i<100;++i) require(result[i]==(i%3==0?0xee:0x11),"enable phase lost across segments");
        require(access(dma_a,true,0xffffffffULL,bytes)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"address overflow");
        require(access(dma_a,true,0x100000,bytes)==tlm::TLM_ADDRESS_ERROR_RESPONSE,"DDR bounds");
        encoder.dma.trace.clear(); ddr.latency=sc_time(100,SC_NS);
        launch_worker.notify(SC_ZERO_TIME);
        std::vector<unsigned char> large(300,0x66);
        require(access(dma_a,true,0x9000,large)==tlm::TLM_OK_RESPONSE,"first concurrent client");
        while(!worker_done) wait(1,SC_NS);
        require(encoder.dma.trace.size()==2,"arbitration lost/duplicated request");
        require(encoder.dma.trace[0].end<=encoder.dma.trace[1].begin,"preempted transaction");
        std::vector<unsigned char> a(300),b(256);
        host.transfer(false,0x9000,a); host.transfer(false,0x7000,b);
        require(a==large && std::all_of(b.begin(),b.end(),[](auto v){return v==0x5a;}),"client ownership corruption");
    }
    void final_write_in_flight() {
        while(!(ddr.in_flight && ddr.active_write && ddr.active_address==0x60004)) wait(1,SC_NS);
    }
    void completion_test() {
        stage(); ddr.latency=sc_time(100,SC_NS); write(reg::SCON,3);
        final_write_in_flight();
        require(!irq.read() && encoder.registers.busy(),"early completion on submitted final write");
        wait(50,SC_NS);
        require(!irq.read() && encoder.registers.busy(),"early completion during response latency");
        wait_irq(); require(host.read(reg::STAT)==(reg::NORMAL|1),"final write completion");
    }
    void reset_test() {
        stage(); std::vector<unsigned char> sentinel(8,0xcc); host.transfer(true,0x60000,sentinel);
        ddr.latency=sc_time(100,SC_NS); write(reg::SCON,3); final_write_in_flight();
        rstn=false; wait(8,SC_NS); rstn=true;
        wait(8,SC_NS);
        require(host.write(reg::SCON,0)==tlm::TLM_GENERIC_ERROR_RESPONSE,"reset released too soon");
        wait(150,SC_NS);
        require(!irq.read() && host.read(reg::STAT)==0,"stale completion after reset");
        std::vector<unsigned char> tail(4); host.transfer(false,0x60004,tail);
        require(std::all_of(tail.begin(),tail.end(),[](auto v){return v==0xcc;}),"stale memory commit after reset");
        stage(); write(reg::SCON,3); wait_irq();
        require(host.read(reg::STAT)==(reg::NORMAL|1),"restart after reset");
    }
    void fault_test() {
        stage(); ddr.error_begin=0x60004; ddr.error_end=0x60008;
        write(reg::SCON,3); wait_irq();
        const auto status=host.read(reg::STAT);
        require((status&reg::ERROR) && !(status&(reg::NORMAL|reg::BUSY)),"error reported as normal");
        require(host.read(reg::STM_LEN)==2,"accepted final word must remain counted after response error");
        ddr.error_begin=ddr.error_end=0;
        stage(); write(reg::SCON,3); wait_irq();
        require(host.read(reg::STAT)==(reg::NORMAL|1),"recovery after memory error");
    }
    void register_fields_test() {
        require(host.read(reg::SPARA0)==0x12c,"PDF table 20-3 reset values");
        stage();
        write(reg::SPARA0,(17u<<21)|(1u<<9)|0x100|(1u<<4)|4);
        write(reg::SPARA1,(2u<<25)|(1u<<24)|(8u<<20)|(9u<<16)|7);
        write(reg::DFCON,(30u<<6)|(3u<<1)|1);
        write(reg::SPARA2,(2u<<6)|63);
        const auto c=encoder.registers.config();
        require(c.sequence.qp==17 && c.sequence.frame_count==7 && c.sequence.cmb_frames==2,"software fields not applied");
        require(c.sequence.gop_m==4 && c.sequence.gop_n==1 && c.sequence.frame_address_mode,"GOP/CMB fields");
        require(c.sequence.log2_fn==8 && c.sequence.log2_poc==9 && c.sequence.force_log,"syntax widths");
        require(c.alpha==-2 && c.beta==3 && c.filter_enabled && c.slice_qp_delta==-1 && c.crop_bottom==2,"signed fields");
        // Restore crop for a complete activation; frame-address mode runs only one picture.
        write(reg::SPARA2,0); write(reg::SCON,3); wait_irq();
        require(host.read(reg::STAT)==(reg::NORMAL|1),"decoded register activation");
    }
    void working_set_test(bool frame_address) {
        host.programming.frame_count=5;
        host.programming.cmb_frames=1;
        host.programming.frame_address_mode=frame_address;
        auto source=stage();
        write(reg::REFM,0x1200); // Adjacent to one source frame, not five contiguous frames.
        for(unsigned activation=0;activation<5;++activation) {
            if(activation) {
                source[0]=static_cast<unsigned char>(activation);
                host.transfer(true,0x1000,source);
                write(reg::CMB,0x1000); write(reg::NAL,0x60000);
            }
            write(reg::SCON,3); wait_irq(); write(reg::SCON,0);
            require(host.read(reg::STAT)==(reg::NORMAL|activation+1),"sequence counter across working sets");
            require(host.read(reg::STM_LEN)==2,"activation-relative length");
            std::vector<unsigned char> output(8); host.transfer(false,0x60000,output);
            require(output==golden(source,16,16),"working-set data mismatch");
        }
    }
    void progress_irq_test() {
        stage(); ddr.latency=sc_time(100,SC_NS); write(reg::SCON,3);
        final_write_in_flight();
        require(host.read(reg::STM_LEN)==2 && !irq.read(),"STM_LEN must advance on acceptance before IRQ");
        wait_irq(); write(reg::SCON,1); wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        require(!irq.read() && encoder.registers.irq_pending(),"GIE masking lost pending event");
        write(reg::SCON,3); wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        require(irq.read(),"GIE re-enable failed to expose retained event");
        host.read(reg::STAT); wait(SC_ZERO_TIME); wait(SC_ZERO_TIME);
        require(!irq.read() && !encoder.registers.irq_pending(),"acknowledge failed");
    }
    void timeout_test(bool stalled) {
        stage(); ddr.latency=sc_time(stalled?10000:100,SC_NS); write(reg::SCON,3);
        if(!stalled) final_write_in_flight();
        const auto result=host.wait_for_completion(irq,sc_time(10,SC_NS),sc_time(stalled?10:500,SC_NS));
        require(result.timed_out && (result.status&reg::BUSY),"timeout snapshot");
        require(host.read(reg::SCON)==0,"timeout did not disable encoder");
        if(stalled) require(!result.quiescent && result.output.empty() && !result.diagnostic.empty(),"live output accessed during unresolved timeout");
        else require(result.quiescent && result.words==2 && result.output.size()==8,"timeout recovery output");
        const auto path=(std::filesystem::current_path()/"timeout_diagnostics").string();
        host.save_diagnostics(path);
        require(std::filesystem::exists(std::filesystem::path(path)/"activation_status.txt"),"diagnostic file missing");
    }
    void priority_test() {
        encoder.dma.set_client(3,ClientId::CMB); // dma_a: pipeline occupies ports 0..2.
        encoder.dma.set_client(4,ClientId::SW); // dma_b
        const ClientId order[4]={ClientId::SW,ClientId::CMB,ClientId::NAL,ClientId::DF};
        encoder.dma.set_priority(order);
        launch_worker.notify(SC_ZERO_TIME);
        std::vector<unsigned char> bytes(4,0x33);
        require(access(dma_a,true,0x9000,bytes)==tlm::TLM_OK_RESPONSE,"priority response");
        while(!worker_done) wait(1,SC_NS);
        require(encoder.dma.trace[0].owner==4,"configured arbitration priority");
        require(encoder.dma.trace.size()==2,"arbitration lost request");
        require(encoder.dma.trace[0].end<=encoder.dma.trace[1].begin,"priority preempted active transaction");
    }
    void schedule_test() {
        host.programming.gop_m=4; host.programming.gop_n=1;
        stage(); auto config=encoder.registers.config(); config.sequence.frame_count=8;
        for(unsigned i=0;i<8;++i) {
            config.completed_frames=i;
            const auto task=encoder.pipeline->picture_task(config,0);
            require(task.coding_index==i && task.display_index==i && task.type==(i%4==0?PictureType::I:PictureType::P),"I/P schedule");
        }
        write(reg::SPARA0,(26u<<21)|0x100|(2u<<4)|4);
        write(reg::SCON,3); wait_irq();
        require(host.read(reg::STAT)==reg::ERROR,"unsupported B schedule must not run as raster I/P");
    }
    void release_schedule_test(bool duplicate) {
        host.programming.frame_count=5; host.programming.cmb_frames=2;
        host.programming.gop_m=4; host.programming.gop_n=2;
        std::vector<unsigned char> data(768,0x45); host.transfer(true,0x1000,data);
        host.configure(16,16,0x1000,0x20000,0x60000);
        for(unsigned activation=0;activation<3;++activation) {
            write(reg::SCON,3); wait_irq(); write(reg::SCON,0);
            const auto status=host.read(reg::STAT);
            if(duplicate && activation==1) {
                require(status==(reg::ERROR|2),"duplicate display index not rejected across activations");
                return;
            }
            const unsigned frames=activation==2?5:(activation+1)*2;
            require(status==(reg::NORMAL|frames),"release schedule progress across activations");
            require(host.read(reg::STM_LEN)==(activation==2?2u:3u),"last partial working set length");
        }
    }
    void run() {
        rstn=false; wait(8,SC_NS); rstn=true; wait(24,SC_NS);
        try {
            if(scenario=="integration") integration();
            else if(scenario=="registers") registers_test();
            else if(scenario=="dma") dma_test();
            else if(scenario=="completion") completion_test();
            else if(scenario=="reset") reset_test();
            else if(scenario=="fault") fault_test();
            else if(scenario=="register_fields") register_fields_test();
            else if(scenario=="working_set") working_set_test(false);
            else if(scenario=="frame_address") working_set_test(true);
            else if(scenario=="nal_progress") progress_irq_test();
            else if(scenario=="host_timeout") timeout_test(false);
            else if(scenario=="host_timeout_stalled") timeout_test(true);
            else if(scenario=="priority") priority_test();
            else if(scenario=="schedule") schedule_test();
            else if(scenario=="release_schedule") release_schedule_test(false);
            else if(scenario=="duplicate_schedule") release_schedule_test(true);
            else if(scenario=="invalid_params") {
                stage(); write(reg::SCON,3); wait_irq();
                require(host.read(reg::STAT)==reg::ERROR,"QP validation");
            } else throw std::runtime_error("unknown scenario");
            passed=true; std::cout<<"PASS "<<scenario<<" at "<<sc_time_stamp()<<'\n';
        } catch(const std::exception& e) { std::cerr<<"FAIL "<<scenario<<": "<<e.what()<<'\n'; }
        sc_stop();
    }
};
int sc_main(int argc,char** argv) {
    Bench test("test",argc>1?argv[1]:"integration",argc>2?static_cast<unsigned>(std::stoul(argv[2])):32);
    sc_start(); return test.passed?0:1;
}
