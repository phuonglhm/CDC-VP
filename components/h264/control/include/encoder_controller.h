#pragma once
#include <h264/control/control_regs.h>
#include <h264/frame_executor_if.h>
#include <set>

namespace h264 {
class EncoderController : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(EncoderController);
    EncoderController(sc_core::sc_module_name, ControlRegs&, ResetDomain&, FrameExecutorIf&);
private:
    ControlRegs& regs_;
    ResetDomain& reset_;
    FrameExecutorIf& executor_;
    std::set<unsigned> display_seen_;
    void run();
    bool validate(const FrameConfig&) const;
};
}
