#include "frame_feedback.h"
#include "line_stage.h"
#include "line_channel.h"
#include "stage_runtime.h"

#include <systemc>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace {

using isp_tlm::frame_feedback_latch;
using isp_tlm::line_channel;
using isp_tlm::line_meta;
using isp_tlm::stage_runtime;
using isp_tlm::stage_timing;
using isp_tlm::derive_metrics;
using ::local_memory_service_profile;
using isp_tlm::memory_service_cycles;
using ::metric_provenance;
using isp_tlm::raw_pipeline_metrics;
using ::workload_profile;
using isp_tlm::compute_cycles;
using isp_tlm::processing_beats;

stage_timing latency_stage_timing() {
    stage_timing timing;
    timing.pipeline_latency_cycles = 5;
    timing.pixel_initiation_interval_cycles = 1;
    timing.pixels_per_cycle = 4;
    timing.max_in_flight_lines = 1;
    timing.cycle_period = sc_core::sc_time(1, sc_core::SC_NS);
    return timing;
}


SC_MODULE(primitive_test) {
    line_channel<std::uint16_t> channel;
    line_channel<std::uint16_t> runtime_channel;
    line_channel<std::uint16_t> blocking_channel;
    line_channel<std::uint16_t> latency_input;
    line_channel<std::uint16_t> latency_output;
    isp_tlm::sc_line_frame_stage<std::uint16_t, std::uint16_t> latency_stage;
    line_channel<std::uint16_t>::read_type held_blocking_read;

    SC_CTOR(primitive_test)
        : channel("channel", 2, 4),
          runtime_channel("runtime_channel", 4, 8),
          blocking_channel("blocking_channel", 1, 4),
          latency_input("latency_input", 2, 4),
          latency_output("latency_output", 2, 4),
          latency_stage(
              "latency_stage", &latency_input, &latency_output,
              2, 4, 4, 2, 4, 4, 8, latency_stage_timing(),
              isp_tlm::sc_line_frame_stage<std::uint16_t,
                                            std::uint16_t>::frame_kernel{},
              isp_tlm::sc_line_frame_stage<std::uint16_t,
                                            std::uint16_t>::metadata_callback{},
              [](const std::uint16_t* input, std::uint16_t* output,
                 std::uint32_t, std::uint32_t, std::uint32_t output_samples,
                 std::uint64_t) {
                  for (std::uint32_t sample = 0; sample < output_samples;
                       ++sample) {
                      output[sample] = input[sample];
                  }
              }) {
        SC_THREAD(run);
        SC_THREAD(release_blocked_credit);
        SC_THREAD(feed_latency_stage);
        SC_THREAD(drain_latency_stage);
    }

    void release_blocked_credit() {
        wait(sc_core::sc_time(3, sc_core::SC_NS));
        if (held_blocking_read.valid()) {
            blocking_channel.release(std::move(held_blocking_read));
        }
    }

    void feed_latency_stage() {
        for (std::uint32_t row = 0; row < 2; ++row) {
            auto result = latency_input.reserve_result();
            auto view = result.view();
            for (std::uint32_t sample = 0; sample < 4; ++sample) {
                view[sample] = static_cast<std::uint16_t>(row * 4 + sample);
            }
            line_meta meta{};
            meta.frame_id = 19;
            meta.row = row;
            meta.width_pixels = 4;
            meta.valid_samples = 4;
            meta.start_of_frame = row == 0;
            meta.end_of_frame = row == 1;
            latency_input.publish(std::move(result), meta);
        }
    }

    void drain_latency_stage() {
        for (std::uint32_t row = 0; row < 2; ++row) {
            auto result = latency_output.read();
            assert(result.valid());
            assert(result.meta().row == row);
            latency_output.release(std::move(result));
        }
    }

    void run() {
        auto first = channel.reserve_result();
        assert(first.valid());
        auto second = channel.try_reserve_result();
        assert(second.valid());
        assert(!channel.try_reserve_result().valid());

        auto first_view = first.view();
        assert(first_view.size() == 4);
        first_view[0] = 11;
        first_view[1] = 12;
        line_meta first_meta;
        first_meta.frame_id = 7;
        first_meta.row = 3;
        first_meta.width_pixels = 2;
        first_meta.valid_samples = 2;
        first_meta.start_of_frame = true;
        channel.publish(std::move(first), first_meta);
        assert(!first.valid());

        auto second_view = second.view(3);
        second_view[0] = 21;
        second_view[1] = 22;
        second_view[2] = 23;
        line_meta second_meta = first_meta;
        second_meta.row = 4;
        second_meta.valid_samples = 3;
        second_meta.start_of_frame = false;
        channel.publish(std::move(second), second_meta);
        assert(channel.available() == 2);
        assert(channel.credits() == 0);

        auto read_first = channel.read();
        assert(read_first.valid());
        assert(read_first.meta().row == 3);
        assert(read_first.view().size() == 2);
        assert(read_first.view()[0] == 11);
        channel.release(std::move(read_first));
        assert(!read_first.valid());

        auto reused = channel.reserve_result();
        assert(reused.valid());
        channel.publish(std::move(reused), second_meta);
        auto read_second = channel.try_read();
        assert(read_second.valid() && read_second.meta().row == 4);
        channel.release(std::move(read_second));
        auto read_reused = channel.try_read();
        assert(read_reused.valid());
        channel.release(std::move(read_reused));
        const auto channel_stats = channel.snapshot();
        assert(channel_stats.published == 3);
        assert(channel_stats.read == 3);
        assert(channel_stats.released == 3);
        assert(channel_stats.occupancy_high_water == 2);
        auto blocking_write = blocking_channel.reserve_result();
        line_meta blocking_meta = first_meta;
        blocking_meta.valid_samples = 4;
        blocking_write.view()[0] = 31;
        blocking_channel.publish(std::move(blocking_write), blocking_meta);
        held_blocking_read = blocking_channel.read();
        auto credit_waited_write = blocking_channel.reserve_result();
        assert(credit_waited_write.valid());
        const auto blocking_stats = blocking_channel.snapshot();
        assert(blocking_stats.blocked_producers == 1);
        assert(blocking_stats.producer_wait_events == 1);
        assert(blocking_stats.producer_wait == sc_core::sc_time(3, sc_core::SC_NS));

        assert(channel_stats.logical_bytes == 8 * sizeof(std::uint16_t));
        assert(channel_stats.published_logical_bytes ==
               8 * sizeof(std::uint16_t));
        assert(channel_stats.read_logical_bytes == 8 * sizeof(std::uint16_t));
        channel.reset_metrics();
        const auto reset_channel_stats = channel.snapshot();
        assert(reset_channel_stats.published == 0);
        assert(reset_channel_stats.logical_bytes == 0);
        assert(reset_channel_stats.blocked_producers == 0);

        // An unconstrained runtime channel must not report credit blocking while
        // a line is merely waiting for its scheduled completion.
        assert(runtime_channel.snapshot().blocked_producers == 0);

        stage_timing timing;
        timing.pipeline_latency_cycles = 4;
        timing.pixel_initiation_interval_cycles = 2;
        timing.pixels_per_cycle = 4;
        timing.max_in_flight_lines = 2;
        timing.cycle_period = sc_core::sc_time(1, sc_core::SC_NS);
        stage_runtime<std::uint16_t> runtime("runtime", timing);

        auto h1 = runtime_channel.reserve_result();
        auto h2 = runtime_channel.reserve_result();
        line_meta m1;
        m1.frame_id = 1;
        m1.row = 10;
        assert(processing_beats(9, 4) == 3);
        assert(processing_beats(3, 4) == 1);
        assert(processing_beats(4, 4) == 1);
        const auto max_pixels = (std::numeric_limits<std::uint64_t>::max)();
        assert(processing_beats(max_pixels, 4) == max_pixels / 4 + 1);
        assert(processing_beats(max_pixels - 3, 4) == max_pixels / 4);
        assert(!memory_service_cycles(8, std::nullopt, std::nullopt).has_value());
        workload_profile workload;
        workload.available = true;
        workload.provenance = metric_provenance::measured;
        assert(!workload.is_valid());
        workload.provenance = metric_provenance::modeled;
        workload.reads_per_pixel = 2;
        workload.writes_per_pixel = 1;
        local_memory_service_profile memory;
        memory.available = true;
        memory.provenance = metric_provenance::measured;
        assert(!memory.is_valid());
        memory.provenance = metric_provenance::modeled;
        memory.read_ports = 1;
        memory.write_ports = 1;
        memory.access_cycles = 2;
        assert(memory_service_cycles(8, workload, memory).value() == 32);
        workload.reads_per_pixel = 0;
        workload.writes_per_pixel = 0;
        assert(memory_service_cycles(8, workload, memory).value() == 0);
        workload.reads_per_pixel = 2;
        workload.writes_per_pixel = 1;
        bool overflow = false;
        try {
            (void)compute_cycles((std::numeric_limits<std::uint64_t>::max)(), 1,
                                 2);
        } catch (const std::overflow_error&) {
            overflow = true;
        }
        assert(overflow);
        m1.width_pixels = 8;
        m1.valid_samples = 8;
        line_meta m2 = m1;
        m2.row = 11;
        assert(processing_beats(8, 4) == 2);
        assert(compute_cycles(8, 4, 2) == 4);
        assert(runtime.can_issue(sc_core::SC_ZERO_TIME, 8));
        runtime.schedule(std::move(h1), m1, sc_core::SC_ZERO_TIME, true);
        assert(!runtime.can_issue(sc_core::SC_ZERO_TIME, 8));
        assert(runtime.issue_interval(8) == sc_core::sc_time(4, sc_core::SC_NS));
        runtime.schedule(std::move(h2), m2, sc_core::sc_time(6, sc_core::SC_NS), true);
        assert(runtime.next_wakeup() == sc_core::sc_time(4, sc_core::SC_NS));
        assert(runtime.retire_ready(runtime_channel, sc_core::sc_time(5, sc_core::SC_NS)) == 1);

        assert(runtime_channel.try_read().valid());
        auto runtime_read = runtime_channel.try_read();
        assert(!runtime_read.valid());
        // The first read handle above must be released before the delayed line can retire.
        assert(runtime.next_wakeup() == sc_core::sc_time(10, sc_core::SC_NS));
        assert(runtime.retire_ready(runtime_channel, sc_core::sc_time(10, sc_core::SC_NS)) == 1);
        auto ordered = runtime_channel.try_read();
        assert(ordered.valid() && ordered.meta().row == 11);
        runtime_channel.release(std::move(ordered));
        const auto runtime_stats = runtime.snapshot();
        assert(runtime_stats.completed == 2);
        assert(runtime_stats.logical_pixels == 16);
        assert(runtime_stats.processing_beats == 4);
        assert(runtime_stats.compute_cycles == 8);
        assert(runtime_stats.active_cycles == 8);
        assert(runtime_stats.issue_window_cycles == 10);
        assert(runtime_stats.memory_service_available == false);
        raw_pipeline_metrics raw;
        raw.frame.logical_pixels = 8;
        raw.frame.input_bytes = 16;
        raw.frame.output_bytes = 24;
        raw.frame.input_bytes_available = true;
        raw.frame.output_bytes_available = true;
        raw.frame.cycle_period = sc_core::sc_time(1, sc_core::SC_NS);
        raw.frame.first_input_time = sc_core::sc_time(1, sc_core::SC_NS);
        raw.frame.first_output_time = sc_core::sc_time(5, sc_core::SC_NS);
        raw.frame.last_output_time = sc_core::sc_time(9, sc_core::SC_NS);
        raw.frame.has_first_input = true;
        raw.frame.has_first_output = true;
        raw.frame.has_last_output = true;
        isp_tlm::raw_block_metrics raw_block;
        raw_block.name = "fixture";
        raw_block.input_lines = 1;
        raw_block.output_lines = 1;
        raw_block.logical_pixels = 8;
        raw_block.processing_beats = 2;
        raw_block.accepted_input_beats = 2;
        raw_block.produced_output_beats = 2;
        raw_block.active_cycles = 2;
        raw_block.observation_cycles = 2;
        raw_block.issue_window_cycles = 2;
        raw.blocks.push_back(raw_block);
        const auto derived = derive_metrics(raw);
        assert(derived.frame.first_output_latency_cycles == 4);
        assert(derived.frame.frame_cycles == 8);
        assert(derived.frame.input_bytes == 16);
        assert(derived.frame.output_bytes == 24);
        assert(derived.frame.input_bytes_available);
        assert(derived.frame.output_bytes_available);
        assert(derived.blocks.size() == 1);
        assert(derived.blocks[0].input_lines == 1);
        assert(derived.blocks[0].output_lines == 1);
        assert(derived.blocks[0].logical_pixels == 8);
        assert(derived.blocks[0].processing_beats == 2);
        assert(derived.blocks[0].utilization == 1.0);
        assert(derived.blocks[0].effective_ii == 1.0);
        assert(derived.bottlenecks.empty());
        runtime.reset_metrics();
        const auto reset_runtime_stats = runtime.snapshot();
        assert(reset_runtime_stats.issued == 0);
        assert(reset_runtime_stats.completed == 0);
        assert(reset_runtime_stats.logical_pixels == 0);
        assert(reset_runtime_stats.issue_window_cycles == 0);
        assert(runtime.can_issue(sc_core::sc_time(10, sc_core::SC_NS), 8));
        auto h3 = runtime_channel.reserve_result();
        runtime.schedule(std::move(h3), m1, sc_core::sc_time(10, sc_core::SC_NS), true);
        assert(runtime.retire_ready(runtime_channel,
                                     sc_core::sc_time(14, sc_core::SC_NS)) == 1);
        auto reset_read = runtime_channel.try_read();
        assert(reset_read.valid());
        runtime_channel.release(std::move(reset_read));
        runtime.reset_metrics();

        frame_feedback_latch feedback;
        assert(feedback.ready(0));
        assert(feedback.snapshot(0).awb_r_gain == 1.0f);
        assert(!feedback.ready(1));
        feedback.commit_awb(0, 1.25f, 0.75f, 2);
        assert(!feedback.ready(1));
        feedback.commit_aec(0, -3);
        assert(feedback.ready(1));
        const auto next_feedback = feedback.snapshot(1);
        assert(next_feedback.awb_r_gain == 1.25f);
        assert(next_feedback.awb_b_gain == 0.75f);
        assert(next_feedback.aec_feedback == -3);
        assert(next_feedback.dg_gain == 2);

        frame_feedback_latch retained(true, false, true, false);
        for (std::uint64_t source = 0;
             source < frame_feedback_latch::record_capacity - 1; ++source) {
            retained.commit_awb(source, 1.0f, 1.0f);
        }
        bool storage_full = false;
        try {
            retained.commit_awb(frame_feedback_latch::record_capacity - 1,
                                1.0f, 1.0f);
        } catch (const std::logic_error&) {
            storage_full = true;
        }
        assert(storage_full);
        retained.consume_awb(1);
        retained.commit_awb(frame_feedback_latch::record_capacity - 1,
                            1.0f, 1.0f);
        assert(retained.ready(frame_feedback_latch::record_capacity));

        wait(sc_core::sc_time(20, sc_core::SC_NS));
        const auto latency_metrics = latency_stage.metrics();
        assert(latency_metrics.issued_lines == 2);
        assert(latency_metrics.retired_lines == 2);
        assert(latency_metrics.issue_wait == sc_core::sc_time(5, sc_core::SC_NS));
        assert(latency_metrics.completion_wait_time ==
               sc_core::sc_time(10, sc_core::SC_NS));
        assert(latency_metrics.retire_wait ==
               sc_core::sc_time(10, sc_core::SC_NS));
        assert(latency_metrics.output_blocked_time == sc_core::SC_ZERO_TIME);
        assert(latency_metrics.output_wait == sc_core::SC_ZERO_TIME);
        assert(latency_output.snapshot().blocked_producers == 0);

        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**) {
    primitive_test test("primitive_test");
    sc_core::sc_start();
    return 0;
}
