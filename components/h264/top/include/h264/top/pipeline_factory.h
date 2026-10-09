#pragma once
#include <h264/frame_executor_if.h>
#include <h264/dma/dma_arbiter.h>
#include <functional>
#include <memory>
namespace h264 {
using PipelineFactory = std::function<std::unique_ptr<FrameExecutorIf>(
    sc_core::sc_module_name, ResetDomain&, DmaArbiter&)>;
std::unique_ptr<FrameExecutorIf> make_stub_pipeline(sc_core::sc_module_name, ResetDomain&, DmaArbiter&);
// Provided only by the team's h264_released_pipeline adapter target.
std::unique_ptr<FrameExecutorIf> make_released_pipeline(sc_core::sc_module_name, ResetDomain&, DmaArbiter&);
}
