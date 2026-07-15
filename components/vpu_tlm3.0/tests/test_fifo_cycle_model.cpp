#include "model/fifo_cycle_model.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

model::FifoCycleStats run(model::FifoCycleConfig config,
                          const model::FifoFrameSpec& frame) {
    model::FifoCycleModel pipeline(config);
    pipeline.start(frame);
    std::uint64_t ticks = 0;
    while (pipeline.running()) {
        assert(pipeline.tick());
        ++ticks;
        assert(ticks < 10'000'000);
    }
    assert(pipeline.done());
    assert(!pipeline.tick());
    assert(pipeline.stats().cycles == ticks);
    return pipeline.stats();
}

} // namespace

int main() {
    model::FifoFrameSpec frame;
    frame.width = 64;
    frame.height = 64;
    frame.mode = hevc::CodingMode::IntraDirectionalTq;
    frame.prefix_bytes = 7;
    frame.ctu_bitstream_bytes = {800, 800, 800, 800};

    model::FifoCycleConfig fast;
    const auto first = run(fast, frame);
    const auto second = run(fast, frame);
    assert(first.cycles == second.cycles);
    assert(first.input_bytes == 64U * 64U * 3U / 2U);
    assert(first.output_bytes == 7U + 4U * 800U);
    assert(first.max_input_occupancy <= fast.input_depth);
    assert(first.max_residual_occupancy <= fast.residual_depth);
    assert(first.max_coefficient_occupancy <= fast.coefficient_depth);
    assert(first.max_output_occupancy <= fast.output_depth);

    model::FifoCycleConfig constrained = fast;
    constrained.input_depth = 1;
    constrained.residual_depth = 1;
    constrained.coefficient_depth = 1;
    constrained.output_depth = 1;
    constrained.prediction_samples_per_cycle = 4096;
    constrained.transform_samples_per_cycle = 4096;
    constrained.cabac_bytes_per_cycle = 32;
    constrained.dma_write_bytes_per_cycle = 1;
    auto unconstrained = constrained;
    unconstrained.output_depth = 4;
    unconstrained.dma_write_bytes_per_cycle = 64;
    const auto fast_stream = run(unconstrained, frame);
    const auto slow = run(constrained, frame);
    assert(slow.cycles > fast_stream.cycles);
    assert(slow.stall_cabac_full > 0);
    assert(slow.max_input_occupancy <= 1);
    assert(slow.max_residual_occupancy <= 1);
    assert(slow.max_coefficient_occupancy <= 1);
    assert(slow.max_output_occupancy <= 1);

    bool rejected = false;
    try {
        model::FifoCycleConfig invalid;
        invalid.output_depth = 0;
        model::FifoCycleModel unused(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    std::cout << "FIFO cycle/backpressure tests passed\n";
    return 0;
}
