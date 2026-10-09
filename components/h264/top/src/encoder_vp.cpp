#include <h264/top/encoder_vp.h>
namespace h264 {
namespace {
FrameExecutorIf& checked_pipeline(const std::unique_ptr<FrameExecutorIf>& pipeline) {
    if (!pipeline) throw std::invalid_argument("pipeline factory returned null");
    return *pipeline;
}
}
EncoderVp::EncoderVp(sc_core::sc_module_name name,unsigned width,SequenceParameters params,sc_core::sc_time period,PipelineFactory factory)
    : sc_module(name),registers("registers",reset_domain,params),arbiter("arbiter"),bridge("bridge",width),
      pipeline(factory("pipeline",reset_domain,arbiter)),controller("controller",registers,reset_domain,checked_pipeline(pipeline)),period_(period) {
    if(period_<=sc_core::SC_ZERO_TIME) throw std::invalid_argument("clock period must be positive");
    registers.irq(irq);
    arbiter.memory.bind(bridge.input);
    SC_METHOD(assert_reset); sensitive << rstn.neg();
    SC_THREAD(release_reset);
}
void EncoderVp::assert_reset() {
    if(!rstn.read()) { reset_domain.assert_reset(); registers.reset(); pipeline->reset(); }
}
void EncoderVp::release_reset() {
    for(;;) {
        if(!rstn.read()) wait(rstn.posedge_event());
        const auto generation=reset_domain.generation;
        wait(period_*2);
        if(rstn.read() && generation==reset_domain.generation) reset_domain.release();
        if(!reset_domain.active) wait(rstn.negedge_event());
    }
}
}
