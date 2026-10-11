#include <h264/top/encoder_vp.h>
#include <h264/platform/ddr_memory.h>
#include <h264/platform/host_driver.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
using namespace h264;
using namespace sc_core;
static std::string mode;
class ParallelPipeline : public sc_module, public FrameExecutorIf {
public:
    tlm_utils::simple_initiator_socket<ParallelPipeline> dma{"dma"};
    ResetDomain& reset_domain;
    sc_event launch,done;
    bool pending=false, abort_called=false, began=false;
    FrameConfig config;
    uint64_t token=0;
    SC_HAS_PROCESS(ParallelPipeline);
    ParallelPipeline(sc_module_name name,ResetDomain& r):sc_module(name),reset_domain(r) { SC_THREAD(worker); }
    void reset() override {} // Worker keeps payload alive until response; epoch invalidates commit.
    SyntaxRequirements syntax_requirements(const FrameConfig& c) const override {
        // Test policy: raw fields encode literal bit counts, requirements 4/5.
        return {4,5,c.sequence.log2_fn,c.sequence.log2_poc};
    }
    void begin_activation(const FrameConfig& c,uint64_t generation) override { began=true; config=c; token=generation; }
    uint32_t execute(const FrameConfig&,uint64_t,unsigned) override {
        pending=true; launch.notify(SC_ZERO_TIME);
        wait(10,SC_NS);
        if(mode=="width_exact") { while(pending) wait(done); return 1; }
        if(mode=="drain_logic") throw std::logic_error("coding module contract failure");
        throw std::runtime_error("peer processing engine failed while DMA writes");
    }
    uint32_t end_activation(const FrameConfig&,uint64_t,uint32_t) override { return 1; }
    bool abort_and_drain(uint64_t generation) override {
        abort_called=true;
        if(mode=="drain_failed") return false;
        if(mode=="drain_throw") throw std::runtime_error("cannot drain");
        while(pending && reset_domain.valid(generation)) wait(done | reset_domain.changed);
        return !pending;
    }
    void worker() {
        for(;;) {
            wait(launch);
            unsigned char bytes[4]={1,2,3,4};
            EpochExtension epoch(reset_domain); epoch.generation=token;
            tlm::tlm_generic_payload tx;
            tx.set_extension(&epoch); tx.set_command(tlm::TLM_WRITE_COMMAND);
            tx.set_address(config.nal); tx.set_data_ptr(bytes); tx.set_data_length(4); tx.set_streaming_width(4);
            nal_words_accepted(token,1);
            sc_time delay=SC_ZERO_TIME;
            dma->b_transport(tx,delay); consume_delay(delay); tx.clear_extension<EpochExtension>();
            pending=false; done.notify(SC_ZERO_TIME);
        }
    }
};
static ParallelPipeline* implementation;
static std::unique_ptr<FrameExecutorIf> factory(sc_module_name name,ResetDomain& r,DmaTransport& arbiter) {
    auto p=std::make_unique<ParallelPipeline>(name,r); p->dma.bind(arbiter.clients);
    implementation=p.get(); return p;
}
class Bench : public sc_module {
public:
    sc_signal<bool> rstn{"rstn"},irq{"irq"};
    EncoderVp encoder{"encoder",32,{},sc_time(8,SC_NS),factory};
    HostDriver host{"host"};
    DdrMemory memory{"memory",1<<20};
    bool passed=false;
    SC_HAS_PROCESS(Bench);
    Bench(sc_module_name name):sc_module(name) {
        encoder.rstn(rstn); encoder.irq(irq);
        host.registers.bind(encoder.registers.socket); host.memory.bind(memory.socket);
        encoder.dma.memory.bind(memory.socket);
        memory.latency=sc_time(200,SC_NS);
        SC_THREAD(run); SC_THREAD(watchdog);
    }
    void check(bool ok,const char* msg) { if(!ok) throw std::runtime_error(msg); }
    void watchdog() { wait(10,SC_US); SC_REPORT_FATAL("test","timeout"); }
    void run() {
        try {
            rstn=false; wait(8,SC_NS); rstn=true; wait(24,SC_NS);
            host.programming.log2_fn=mode=="width_fn_short"?3:4;
            host.programming.log2_poc=mode=="width_poc_short"?4:5;
            host.configure(16,16,0x1000,0x20000,0x60000);
            check(host.write(reg::SCON,3)==tlm::TLM_OK_RESPONSE,"start");
            wait(30,SC_NS);
            if(mode=="width_fn_short" || mode=="width_poc_short") {
                check(host.read(reg::STAT)==reg::ERROR,"width not rejected");
                check(!implementation->began && memory.trace.empty(),"preflight launched work");
            } else {
                check(encoder.registers.busy() && !irq.read(),"early busy release/IRQ");
                check(host.write(reg::NAL,0x70000)==tlm::TLM_COMMAND_ERROR_RESPONSE,"live buffer writable");
                if(mode=="drain_reset") {
                    rstn=false; wait(8,SC_NS); rstn=true; wait(240,SC_NS);
                    check(!irq.read() && host.read(reg::STAT)==0,"stale error after reset");
                    check(memory.trace.empty(),"stale DMA committed");
                } else {
                    wait(230,SC_NS);
                    if(mode=="drain_failed" || mode=="drain_throw") {
                        check(encoder.registers.busy() && !irq.read(),"failed drain released ownership");
                        rstn=false; wait(8,SC_NS); rstn=true; wait(24,SC_NS);
                        check(host.read(reg::STAT)==0,"reset recovery");
                    } else {
                        check(!encoder.registers.busy() && irq.read(),"drain never completed");
                        check(host.read(reg::STAT)==(mode=="width_exact"?(reg::NORMAL|1):reg::ERROR),"final status");
                        check(host.read(reg::STM_LEN)==1 && memory.trace.size()==1,"lost progress/write");
                        check(implementation->abort_called==(mode!="width_exact"),"abort invocation");
                    }
                }
            }
            passed=true; std::cout<<"PASS "<<mode<<'\n';
        } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; }
        sc_stop();
    }
};
int sc_main(int argc,char** argv) {
    mode=argc>1?argv[1]:"drain";
    Bench bench("bench"); sc_start(); return bench.passed?0:1;
}
