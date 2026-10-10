#pragma once
#include <h264/top/dma_transport.h>
#include <h264/control/encoder_controller.h>
#include <h264/top/pipeline_factory.h>
namespace h264 {
class EncoderVp : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> rstn{"rstn"};
    sc_core::sc_out<bool> irq{"irq"};
    ResetDomain reset_domain;
    ControlRegs registers;
    DmaTransport dma;
    std::unique_ptr<FrameExecutorIf> pipeline;
    EncoderController controller;
    SC_HAS_PROCESS(EncoderVp);
    EncoderVp(sc_core::sc_module_name,unsigned width=32,SequenceParameters params={},
              sc_core::sc_time period=sc_core::sc_time(8,sc_core::SC_NS),
              PipelineFactory factory=make_stub_pipeline);
private:
    sc_core::sc_time period_;
    void assert_reset();
    void release_reset();
};
}
