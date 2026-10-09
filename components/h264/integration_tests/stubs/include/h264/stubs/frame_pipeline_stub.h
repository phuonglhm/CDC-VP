#pragma once
#include <h264/control/encoder_controller.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <memory>
namespace h264 {
class ProcessingStub : public ProcessingIf {
public:
    sc_core::sc_time latency{80,sc_core::SC_NS};
    ProcessedBlock process(const Macroblock&) override;
};
// Integration fixture ONLY. No prediction, transform, entropy, filtering or P/B coding.
class FramePipelineStub : public sc_core::sc_module, public FrameExecutorIf {
public:
    tlm_utils::simple_initiator_socket<FramePipelineStub> cmb{"cmb"}, reference{"reference"}, nal{"nal"};
    FramePipelineStub(sc_core::sc_module_name,ResetDomain&,std::unique_ptr<ProcessingIf>);
    void reset() override {} // Stateless fixture; transfers check epoch.
    // Checksum fixture emits no FN/POC syntax. Never reuse this exemption in a real adapter.
    SyntaxRequirements syntax_requirements(const FrameConfig&) const override { return {0,0,0,0}; }
    bool abort_and_drain(uint64_t) override { return true; } // All fixture transport is synchronous.
    void begin_activation(const FrameConfig&,uint64_t) override;
    uint32_t execute(const FrameConfig&,uint64_t,unsigned) override;
    uint32_t end_activation(const FrameConfig&,uint64_t,uint32_t) override;
private:
    ResetDomain& reset_;
    std::unique_ptr<ProcessingIf> processor_;
    void transfer(tlm_utils::simple_initiator_socket<FramePipelineStub>&,bool,uint64_t,
                  unsigned char*,unsigned,uint64_t);
    void tile(const FrameConfig&,Macroblock&,bool,uint64_t,uint64_t);
};
}
