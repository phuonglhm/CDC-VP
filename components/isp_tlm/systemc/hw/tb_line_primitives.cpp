#include "frame_feedback.h"
#include "line_channel.h"
#include "stage_runtime.h"

#include <systemc>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <stdexcept>

namespace {

using isp_tlm::frame_feedback_latch;
using isp_tlm::line_channel;
using isp_tlm::line_meta;
using isp_tlm::stage_runtime;
using isp_tlm::stage_timing;

SC_MODULE(primitive_test) {
    line_channel<std::uint16_t> channel;
    line_channel<std::uint16_t> runtime_channel;

    SC_CTOR(primitive_test)
        : channel("channel", 2, 4), runtime_channel("runtime_channel", 4, 8) {
        SC_THREAD(run);
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

        stage_timing timing;
        timing.compute_latency_cycles = 4;
        timing.pixel_ii = 2;
        timing.pixels_per_cycle = 4;
        timing.max_in_flight_lines = 2;
        timing.cycle_period = sc_core::sc_time(1, sc_core::SC_NS);
        stage_runtime<std::uint16_t> runtime("runtime", timing);

        auto h1 = runtime_channel.reserve_result();
        auto h2 = runtime_channel.reserve_result();
        line_meta m1;
        m1.frame_id = 1;
        m1.row = 10;
        m1.width_pixels = 8;
        m1.valid_samples = 8;
        line_meta m2 = m1;
        m2.row = 11;
        assert(runtime.can_issue(sc_core::SC_ZERO_TIME, 8));
        runtime.schedule(std::move(h1), m1, sc_core::SC_ZERO_TIME);
        assert(!runtime.can_issue(sc_core::SC_ZERO_TIME, 8));
        assert(runtime.issue_interval(8) == sc_core::sc_time(2, sc_core::SC_NS));
        runtime.schedule(std::move(h2), m2, sc_core::sc_time(2, sc_core::SC_NS));
        assert(runtime.next_wakeup() == sc_core::sc_time(4, sc_core::SC_NS));
        assert(runtime.retire_ready(runtime_channel, sc_core::sc_time(5, sc_core::SC_NS)) == 1);
        assert(runtime_channel.try_read().valid());
        auto runtime_read = runtime_channel.try_read();
        assert(!runtime_read.valid());
        // The first read handle above must be released before the second line can be read.
        assert(runtime.next_wakeup() == sc_core::sc_time(4, sc_core::SC_NS));
        // (The temporary handle's destructor safely returns the slot.)
        assert(runtime.retire_ready(runtime_channel, sc_core::sc_time(6, sc_core::SC_NS)) == 1);
        auto ordered = runtime_channel.try_read();
        assert(ordered.valid() && ordered.meta().row == 11);
        runtime_channel.release(std::move(ordered));
        const auto runtime_stats = runtime.snapshot();
        assert(runtime_stats.completed == 2);

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

        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**) {
    primitive_test test("primitive_test");
    sc_core::sc_start();
    return 0;
}
