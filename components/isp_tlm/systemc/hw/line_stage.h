#ifndef ISP_TLM_LINE_STAGE_H
#define ISP_TLM_LINE_STAGE_H

#include <systemc>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "line_channel.h"
#include "stage_runtime.h"

namespace isp_tlm {

/** Counters owned by one line-frame stage. */
struct line_stage_metrics {
    std::uint64_t frames = 0;
    std::uint64_t input_lines = 0;
    std::uint64_t output_lines = 0;
    std::uint64_t issued_lines = 0;
    std::uint64_t retired_lines = 0;
    std::uint32_t in_flight_high_water = 0;
    std::uint64_t issue_stalls = 0;
    std::uint64_t logical_pixels = 0;
    std::uint64_t input_logical_pixels = 0;
    std::uint64_t output_logical_pixels = 0;
    std::uint64_t accepted_input_beats = 0;
    std::uint64_t produced_output_beats = 0;
    operation_counts operations{};
    std::uint64_t processing_beats = 0;
    std::uint64_t active_cycles = 0;
    std::uint64_t bypass_cycles = 0;
    std::uint64_t memory_wait_cycles = 0;
    bool memory_wait_available = false;
    metric_provenance memory_wait_provenance = metric_provenance::unavailable;
    std::uint64_t issue_window_cycles = 0;
    sc_core::sc_time first_issue_time = sc_core::SC_ZERO_TIME;
    sc_core::sc_time last_issue_time = sc_core::SC_ZERO_TIME;
    bool has_issue_time = false;
    double effective_ii = 0.0;
    sc_core::sc_time input_wait = sc_core::SC_ZERO_TIME;
    sc_core::sc_time issue_wait = sc_core::SC_ZERO_TIME;
    sc_core::sc_time retire_wait = sc_core::SC_ZERO_TIME;
    sc_core::sc_time output_wait = sc_core::SC_ZERO_TIME;
    sc_core::sc_time input_starved_time = sc_core::SC_ZERO_TIME;
    sc_core::sc_time output_blocked_time = sc_core::SC_ZERO_TIME;
    sc_core::sc_time completion_wait_time = sc_core::SC_ZERO_TIME;
};

/**
 * A reusable frame-kernel shell with a single SystemC process.
 *
 * Lines are the only inter-stage ownership unit.  A complete input frame is
 * copied into the stage-owned buffers before the functional kernel is called;
 * the input channel slot is released immediately after each copy.  Results
 * are copied to reserved downstream slots and handed to stage_runtime for
 * bounded, timestamped publication.
 * Output metadata is resolved before copying so packed planes can use
 * row-specific offsets and valid sample counts.
 */
template <typename InT, typename OutT>
class sc_line_frame_stage : public sc_core::sc_module {
public:
    using input_type = InT;
    using output_type = OutT;
    using frame_kernel = std::function<void(const InT*, OutT*,
                                            std::uint32_t, std::uint32_t,
                                            std::uint32_t, std::uint32_t,
                                            std::uint64_t)>;
    using line_kernel = std::function<void(const InT*, OutT*,
                                           std::uint32_t, std::uint32_t,
                                           std::uint32_t, std::uint64_t)>;
    // The frame callback receives the complete frame.  The line callback is
    // selected for stages whose functional operation has no row history.
    // Its arguments are row index, input samples, output samples, and frame
    // id.
    // The callback receives the default metadata and may replace any field.
    using metadata_callback = std::function<line_meta(line_meta)>;

    SC_HAS_PROCESS(sc_line_frame_stage);

    sc_line_frame_stage(
        sc_core::sc_module_name name,
        line_channel<InT>* input,
        line_channel<OutT>* output,
        std::uint32_t input_rows,
        std::uint32_t input_valid_samples_per_line,
        std::uint32_t input_width_pixels,
        std::uint32_t output_rows,
        std::uint32_t output_valid_samples_per_line,
        std::uint32_t output_width_pixels,
        std::size_t output_frame_element_count,
        const stage_timing& timing,
        frame_kernel kernel,
        metadata_callback metadata = metadata_callback{},
        line_kernel line = line_kernel{})
        : sc_core::sc_module(name)
        , input_(input)
        , output_(output)
        , input_rows_(input_rows)
        , input_valid_samples_(input_valid_samples_per_line)
        , input_width_pixels_(input_width_pixels)
        , output_rows_(output_rows)
        , output_valid_samples_(output_valid_samples_per_line)
        , output_width_pixels_(output_width_pixels)
        , output_frame_element_count_(output_frame_element_count)
        , kernel_(std::move(kernel))
        , metadata_(std::move(metadata))
        , line_kernel_(std::move(line))
        , runtime_(timing)
        , input_frame_(line_kernel_ ? 0
                                    : checked_size(input_rows,
                                                   input_valid_samples_per_line))
        , output_frame_(line_kernel_ ? 0 : output_frame_element_count) {
        const std::size_t required_output =
            line_kernel_ ? 0 : required_output_size();
        if (input_ == nullptr || output_ == nullptr) {
            SC_REPORT_ERROR("sc_line_frame_stage", "line channel pointer is null");
        }
        if (!kernel_ && !line_kernel_) {
            SC_REPORT_ERROR("sc_line_frame_stage", "stage callback is empty");
        }
        if (output_frame_element_count_ < required_output) {
            SC_REPORT_ERROR("sc_line_frame_stage",
                            "output frame storage is smaller than output rows");
        }
        if (line_kernel_ && (input_rows_ != output_rows_ ||
                             input_valid_samples_ == 0 ||
                             output_valid_samples_ == 0)) {
            SC_REPORT_ERROR("sc_line_frame_stage",
                            "line callback requires one input row per output row");
        }
        // SC_THREAD is deliberately the only process registered by this
        // module.  All line-level concurrency is represented by runtime_.
        SC_THREAD(process_lines);
    }

    // Compatibility overload for callers that place timing after callbacks.
    sc_line_frame_stage(
        sc_core::sc_module_name name,
        line_channel<InT>* input,
        line_channel<OutT>* output,
        std::uint32_t input_rows,
        std::uint32_t input_valid_samples_per_line,
        std::uint32_t input_width_pixels,
        std::uint32_t output_rows,
        std::uint32_t output_valid_samples_per_line,
        std::uint32_t output_width_pixels,
        std::size_t output_frame_element_count,
        frame_kernel kernel,
        metadata_callback metadata,
        const stage_timing& timing,
        line_kernel line = line_kernel{})
        : sc_line_frame_stage(name, input, output, input_rows,
                              input_valid_samples_per_line, input_width_pixels,
                              output_rows, output_valid_samples_per_line,
                              output_width_pixels, output_frame_element_count,
                              timing, std::move(kernel), std::move(metadata),
                              std::move(line)) {}


    const line_stage_metrics& metrics() const noexcept { return metrics_; }
    void reset_metrics() {
        runtime_.reset_metrics();
        metrics_ = line_stage_metrics{};
    }
    void set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    bool enabled() const noexcept { return enabled_; }

    line_channel<InT>* input_channel() const noexcept { return input_; }
    line_channel<OutT>* output_channel() const noexcept { return output_; }

private:
    static std::size_t checked_size(std::uint32_t rows,
                                    std::uint32_t samples) {
        constexpr std::size_t max_size =
            (std::numeric_limits<std::size_t>::max)();
        if (samples != 0 && static_cast<std::size_t>(rows) > max_size / samples) {
            SC_REPORT_ERROR("sc_line_frame_stage", "frame storage size overflows");
            return 0;
        }
        return static_cast<std::size_t>(rows) * samples;
    }
    std::size_t required_output_size() const {
        constexpr std::size_t max_size =
            (std::numeric_limits<std::size_t>::max)();
        std::size_t required = 0;
        for (std::uint32_t row = 0; row < output_rows_; ++row) {
            line_meta meta{};
            meta.row = row;
            meta.width_pixels = output_width_pixels_;
            meta.valid_samples = output_valid_samples_;
            meta.dst_offset_bytes = static_cast<std::uint32_t>(
                static_cast<std::size_t>(row) * output_valid_samples_ *
                sizeof(OutT));
            meta.start_of_frame = row == 0;
            meta.end_of_frame = row + 1 == output_rows_;
            if (metadata_) {
                meta = metadata_(meta);
            }
            if (meta.dst_offset_bytes % sizeof(OutT) != 0) {
                return max_size;
            }
            const std::size_t offset =
                static_cast<std::size_t>(meta.dst_offset_bytes) / sizeof(OutT);
            if (offset > max_size - meta.valid_samples) {
                return max_size;
            }
            required = std::max(required,
                                offset + static_cast<std::size_t>(meta.valid_samples));
        }
        return required;
    }
    void record_input_meta(const line_meta& meta) {
        metrics_.input_logical_pixels =
            checked_add(metrics_.input_logical_pixels, meta.width_pixels);
        metrics_.accepted_input_beats = checked_add(
            metrics_.accepted_input_beats,
            processing_beats(meta.width_pixels, runtime_.timing().pixels_per_cycle));
    }

    void record_output_meta(const line_meta& meta) {
        metrics_.output_logical_pixels =
            checked_add(metrics_.output_logical_pixels, meta.width_pixels);
        metrics_.produced_output_beats = checked_add(
            metrics_.produced_output_beats,
            processing_beats(meta.width_pixels, runtime_.timing().pixels_per_cycle));
    }
    typename line_channel<OutT>::write_type reserve_output_result() {
        auto result = output_->try_reserve_result();
        if (result.valid()) {
            return result;
        }
        const sc_core::sc_time start = sc_core::sc_time_stamp();
        output_->record_producer_blocked_episode();
        while (!result.valid()) {
            runtime_.retire_ready(*output_, sc_core::sc_time_stamp());
            result = output_->try_reserve_result();
            if (!result.valid()) {
                wait_for_output_progress();
            }
        }
        output_->record_producer_wait_duration(sc_core::sc_time_stamp() - start);
        return result;
    }

    void process_lines() {
        while (true) {
            if (line_kernel_) {
                process_line_frame();
                continue;
            }

            std::uint64_t frame_id = 0;
            bool frame_id_valid = false;
            bool frame_ok = true;

            // Ingest exactly the configured number of line tokens.  read()
            // blocks on data_event inside line_channel; no whole-frame
            // availability query is used here.
            for (std::uint32_t row = 0; row < input_rows_; ++row) {
                const bool input_eligible =
                    runtime_.can_issue(sc_core::sc_time_stamp(), output_width_pixels_);
                const sc_core::sc_time wait_start = sc_core::sc_time_stamp();
                auto token = input_->read();
                if (input_eligible) {
                    const sc_core::sc_time waited =
                        sc_core::sc_time_stamp() - wait_start;
                    metrics_.input_wait += waited;
                    metrics_.input_starved_time += waited;
                }

                const auto view = token.view();
                const line_meta meta = token.meta();
                record_input_meta(meta);
                if (!frame_id_valid) {
                    frame_id = meta.frame_id;
                    frame_id_valid = true;
                }
                if (meta.frame_id != frame_id || meta.row != row ||
                    (row == 0 && !meta.start_of_frame) ||
                    (row != 0 && meta.start_of_frame) ||
                    (row + 1 == input_rows_ && !meta.end_of_frame) ||
                    (row + 1 != input_rows_ && meta.end_of_frame)) {
                    frame_ok = false;
                    SC_REPORT_ERROR("sc_line_frame_stage",
                                    "invalid input frame/line metadata");
                }
                if (meta.valid_samples > input_valid_samples_ ||
                    meta.valid_samples > view.size()) {
                    frame_ok = false;
                    SC_REPORT_ERROR("sc_line_frame_stage",
                                    "input line exceeds configured storage");
                }
                const std::size_t copy_count =
                    (meta.valid_samples <= input_valid_samples_ &&
                     meta.valid_samples <= view.size())
                        ? meta.valid_samples
                        : 0;
                InT* destination = input_frame_.data() +
                                   static_cast<std::size_t>(row) *
                                       input_valid_samples_;
                for (std::size_t sample = 0; sample < copy_count; ++sample) {
                    destination[sample] = view[sample];
                }
                input_->release(std::move(token));
                ++metrics_.input_lines;
            }

            if (!frame_ok) {
                continue;
            }

            kernel_(input_frame_.data(), output_frame_.data(),
                    input_width_pixels_, input_rows_, output_width_pixels_,
                    output_rows_, frame_id);

            for (std::uint32_t row = 0; row < output_rows_; ++row) {
                wait_until_issueable();
                auto result = reserve_output_result();

                line_meta meta{};
                meta.frame_id = frame_id;
                meta.row = row;
                meta.width_pixels = output_width_pixels_;
                meta.valid_samples = output_valid_samples_;
                meta.dst_offset_bytes = static_cast<std::uint32_t>(
                    static_cast<std::size_t>(row) * output_valid_samples_ *
                    sizeof(OutT));
                meta.plane = line_plane{};
                meta.start_of_frame = row == 0;
                meta.end_of_frame = row + 1 == output_rows_;
                if (metadata_) {
                    meta = metadata_(meta);
                }

                auto view = result.view();
                const std::size_t offset =
                    static_cast<std::size_t>(meta.dst_offset_bytes) /
                    sizeof(OutT);
                const std::size_t available =
                    (offset < output_frame_.size())
                        ? output_frame_.size() - offset
                        : 0;
                const std::size_t copy_count =
                    std::min<std::size_t>(meta.valid_samples, view.size());
                const std::size_t safe_count =
                    std::min<std::size_t>(copy_count, available);
                for (std::size_t sample = 0; sample < safe_count; ++sample) {
                    view[sample] = output_frame_[offset + sample];
                }
                record_output_meta(meta);

                runtime_.schedule(std::move(result), meta,
                                  sc_core::sc_time_stamp(), enabled_);
                ++metrics_.output_lines;
            }

            while (runtime_.has_in_flight()) {
                runtime_.retire_ready(*output_, sc_core::sc_time_stamp());
                if (runtime_.has_in_flight()) {
                    wait_for_completion_progress();
                }
            }
            refresh_runtime_metrics();
            ++metrics_.frames;
        }
    }

    void process_line_frame() {
        std::uint64_t frame_id = 0;
        bool frame_id_valid = false;
        bool frame_ok = true;

        for (std::uint32_t row = 0; row < input_rows_; ++row) {
            wait_until_issueable();
            const bool input_eligible = true;
            const sc_core::sc_time wait_start = sc_core::sc_time_stamp();
            auto token = input_->read();
            if (input_eligible) {
                const sc_core::sc_time waited =
                    sc_core::sc_time_stamp() - wait_start;
                metrics_.input_wait += waited;
                metrics_.input_starved_time += waited;
            }

            const auto input_view = token.view();
            const line_meta input_meta = token.meta();
            record_input_meta(input_meta);
            if (!frame_id_valid) {
                frame_id = input_meta.frame_id;
                frame_id_valid = true;
            }
            if (input_meta.frame_id != frame_id || input_meta.row != row ||
                (row == 0 && !input_meta.start_of_frame) ||
                (row != 0 && input_meta.start_of_frame) ||
                (row + 1 == input_rows_ && !input_meta.end_of_frame) ||
                (row + 1 != input_rows_ && input_meta.end_of_frame)) {
                frame_ok = false;
                SC_REPORT_ERROR("sc_line_frame_stage",
                                "invalid input frame/line metadata");
            }
            if (input_meta.valid_samples > input_valid_samples_ ||
                input_meta.valid_samples > input_view.size()) {
                frame_ok = false;
                SC_REPORT_ERROR("sc_line_frame_stage",
                                "input line exceeds configured storage");
            }

            if (!frame_ok) {
                input_->release(std::move(token));
                ++metrics_.input_lines;
                continue;
            }

            auto result = reserve_output_result();

            line_meta output_meta{};
            output_meta.frame_id = frame_id;
            output_meta.row = row;
            output_meta.width_pixels = output_width_pixels_;
            output_meta.valid_samples = output_valid_samples_;
            output_meta.dst_offset_bytes = static_cast<std::uint32_t>(
                static_cast<std::size_t>(row) * output_valid_samples_ *
                sizeof(OutT));
            output_meta.plane = line_plane{};
            output_meta.start_of_frame = row == 0;
            output_meta.end_of_frame = row + 1 == output_rows_;
            if (metadata_) {
                output_meta = metadata_(output_meta);
            }

            auto output_view = result.view(output_meta.valid_samples);
            line_kernel_(input_view.data(), output_view.data(), row,
                         static_cast<std::uint32_t>(input_view.size()),
                         output_meta.valid_samples, frame_id);
            input_->release(std::move(token));
            ++metrics_.input_lines;

            record_output_meta(output_meta);
            runtime_.schedule(std::move(result), output_meta,
                              sc_core::sc_time_stamp(), enabled_);
            ++metrics_.output_lines;
            runtime_.retire_ready(*output_, sc_core::sc_time_stamp());
        }

        while (runtime_.has_in_flight()) {
            runtime_.retire_ready(*output_, sc_core::sc_time_stamp());
            if (runtime_.has_in_flight()) {
                wait_for_completion_progress();
            }
        }
        refresh_runtime_metrics();
        if (frame_ok) {
            ++metrics_.frames;
        }
    }

    void wait_until_issueable() {
        while (!runtime_.can_issue(sc_core::sc_time_stamp(), output_width_pixels_)) {
            runtime_.retire_ready(*output_, sc_core::sc_time_stamp());
            if (runtime_.can_issue(sc_core::sc_time_stamp(), output_width_pixels_)) {
                return;
            }
            wait_for_issue_progress(runtime_.capacity_full());
        }
    }

    void wait_for_issue_progress(bool capacity_limited) {
        const sc_core::sc_time start = sc_core::sc_time_stamp();
        const sc_core::sc_time wake = runtime_.next_wakeup();
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        if (wake > now) {
            wait(wake - now, runtime_.completion_event());
        } else {
            wait(runtime_.completion_event());
        }
        const sc_core::sc_time waited = sc_core::sc_time_stamp() - start;
        metrics_.issue_wait += waited;
        if (capacity_limited) {
            metrics_.retire_wait += waited;
            metrics_.completion_wait_time += waited;
        }
    }

    void wait_for_output_progress() {
        const sc_core::sc_time start = sc_core::sc_time_stamp();
        const sc_core::sc_time wake = runtime_.next_wakeup();
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        const auto progress = runtime_.completion_event() | output_->credit_event();
        if (wake > now) {
            wait(wake - now, progress);
        } else {
            wait(progress);
        }
        const sc_core::sc_time waited = sc_core::sc_time_stamp() - start;
        // The episode began with no downstream credit.  Attribute its complete
        // duration to credit blocking; completion is an overlapping cause.
        metrics_.output_wait += waited;
        metrics_.output_blocked_time += waited;
        const std::size_t retired =
            runtime_.retire_ready(*output_, sc_core::sc_time_stamp());
        if (retired != 0) {
            metrics_.retire_wait += waited;
            metrics_.completion_wait_time += waited;
        }
    }
    void wait_for_completion_progress() {
        const sc_core::sc_time start = sc_core::sc_time_stamp();
        const sc_core::sc_time wake = runtime_.next_wakeup();
        const sc_core::sc_time now = sc_core::sc_time_stamp();
        if (wake > now) {
            wait(wake - now, runtime_.completion_event());
        } else {
            wait(runtime_.completion_event());
        }
        const sc_core::sc_time waited = sc_core::sc_time_stamp() - start;
        runtime_.retire_ready(*output_, sc_core::sc_time_stamp());
        metrics_.retire_wait += waited;
        metrics_.completion_wait_time += waited;
    }

    void refresh_runtime_metrics() {
        const auto runtime = runtime_.snapshot();
        metrics_.issued_lines = runtime.issued;
        metrics_.retired_lines = runtime.completed;
        metrics_.in_flight_high_water = runtime.high_water_in_flight;
        metrics_.issue_stalls = runtime.issue_stalls;
        metrics_.logical_pixels = runtime.logical_pixels;
        metrics_.processing_beats = runtime.processing_beats;
        metrics_.active_cycles = runtime.active_cycles;
        metrics_.bypass_cycles = runtime.bypass_cycles;
        metrics_.memory_wait_cycles = runtime.memory_wait_cycles;
        metrics_.memory_wait_available = runtime.memory_service_available;
        metrics_.memory_wait_provenance = runtime.memory_service_provenance;
        metrics_.operations = runtime.operations;
        metrics_.issue_window_cycles = runtime.issue_window_cycles;
        metrics_.first_issue_time = runtime.first_issue_time;
        metrics_.last_issue_time = runtime.last_issue_time;
        metrics_.has_issue_time = runtime.has_issue_time;
        if (runtime.processing_beats != 0) {
            metrics_.effective_ii =
                static_cast<double>(runtime.issue_window_cycles) /
                static_cast<double>(runtime.processing_beats);
        }
    }

    line_channel<InT>* input_;
    line_channel<OutT>* output_;
    std::uint32_t input_rows_;
    std::uint32_t input_valid_samples_;
    std::uint32_t input_width_pixels_;
    std::uint32_t output_rows_;
    std::uint32_t output_valid_samples_;
    std::uint32_t output_width_pixels_;
    std::size_t output_frame_element_count_;
    frame_kernel kernel_;
    metadata_callback metadata_;
    line_kernel line_kernel_;
    bool enabled_ = true;
    stage_runtime<OutT> runtime_;
    std::vector<InT> input_frame_;
    std::vector<OutT> output_frame_;
    line_stage_metrics metrics_;
};

} // namespace isp_tlm

#endif // ISP_TLM_LINE_STAGE_H
