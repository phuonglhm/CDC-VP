#include "model/fifo_cycle_model.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace model {
namespace {

constexpr unsigned kCtuSize = 32;

std::uint64_t ceil_div(std::uint64_t value, std::uint64_t divisor) {
    return (value + divisor - 1) / divisor;
}

template <typename T>
class BoundedFifo {
public:
    explicit BoundedFifo(std::size_t depth) : depth_(depth) {
        if (depth == 0) throw std::invalid_argument("FIFO depth must be non-zero");
    }

    [[nodiscard]] bool empty() const { return items_.empty(); }
    [[nodiscard]] bool full() const { return items_.size() == depth_; }
    [[nodiscard]] std::size_t size() const { return items_.size(); }

    void push(T value) {
        if (full()) throw std::logic_error("FIFO overflow");
        items_.push_back(std::move(value));
    }

    T pop() {
        if (empty()) throw std::logic_error("FIFO underflow");
        T value = std::move(items_.front());
        items_.pop_front();
        return value;
    }

private:
    std::size_t depth_;
    std::deque<T> items_;
};

struct CtuToken {
    std::size_t id = 0;
    std::uint64_t input_bytes = 0;
    std::uint64_t output_bytes = 0;
    std::uint64_t prediction_cycles = 1;
    std::uint64_t transform_cycles = 1;
    std::uint64_t cabac_cycles = 1;
};

struct OutputPacket {
    std::uint64_t bytes = 0;
};

struct StageState {
    std::optional<CtuToken> token;
    std::uint64_t remaining = 0;
};

unsigned prediction_candidates(hevc::CodingMode mode) {
    switch (mode) {
    case hevc::CodingMode::IntraAdaptiveTq: return 2;
    case hevc::CodingMode::IntraDirectionalTq: return 4;
    default: return 1;
    }
}

unsigned transform_candidates(hevc::CodingMode mode) {
    switch (mode) {
    case hevc::CodingMode::Pcm:
    case hevc::CodingMode::IntraDc:
    case hevc::CodingMode::HybridDc:
        return 0;
    case hevc::CodingMode::IntraAdaptiveTq:
        return 2;
    case hevc::CodingMode::IntraDirectionalTq:
        return 4;
    default:
        return 1;
    }
}

std::vector<CtuToken> make_tokens(const FifoFrameSpec& frame,
                                  const FifoCycleConfig& config) {
    if (frame.width == 0 || frame.height == 0 ||
        (frame.width & 1U) != 0 || (frame.height & 1U) != 0) {
        throw std::invalid_argument("FIFO frame dimensions must be non-zero and even");
    }
    const unsigned columns = (frame.width + kCtuSize - 1) / kCtuSize;
    const unsigned rows = (frame.height + kCtuSize - 1) / kCtuSize;
    const std::size_t count = static_cast<std::size_t>(columns) * rows;
    if (frame.ctu_bitstream_bytes.size() != count) {
        throw std::invalid_argument("one bitstream packet is required per CTU");
    }

    std::vector<CtuToken> tokens;
    tokens.reserve(count);
    const auto pred_candidates = prediction_candidates(frame.mode);
    const auto tq_candidates = transform_candidates(frame.mode);
    for (std::size_t id = 0; id < count; ++id) {
        const unsigned x0 = static_cast<unsigned>(id % columns) * kCtuSize;
        const unsigned y0 = static_cast<unsigned>(id / columns) * kCtuSize;
        const unsigned luma_width = std::min(kCtuSize, frame.width - x0);
        const unsigned luma_height = std::min(kCtuSize, frame.height - y0);
        const std::uint64_t luma_samples =
            static_cast<std::uint64_t>(luma_width) * luma_height;
        const std::uint64_t chroma_samples =
            2ULL * (luma_width / 2U) * (luma_height / 2U);
        const std::uint64_t all_samples = luma_samples + chroma_samples;

        CtuToken token;
        token.id = id;
        token.input_bytes = all_samples;
        token.output_bytes = frame.ctu_bitstream_bytes[id];
        token.prediction_cycles = frame.mode == hevc::CodingMode::Pcm
            ? 1
            : std::max<std::uint64_t>(1, ceil_div(
                luma_samples * pred_candidates,
                config.prediction_samples_per_cycle));
        token.transform_cycles = tq_candidates == 0
            ? 1
            : std::max<std::uint64_t>(1, ceil_div(
                all_samples * tq_candidates,
                config.transform_samples_per_cycle));
        token.cabac_cycles = std::max<std::uint64_t>(1, ceil_div(
            token.output_bytes, config.cabac_bytes_per_cycle));
        tokens.push_back(token);
    }
    return tokens;
}

void validate_config(const FifoCycleConfig& config) {
    if (config.input_depth == 0 || config.residual_depth == 0 ||
        config.coefficient_depth == 0 || config.output_depth == 0 ||
        config.dma_read_bytes_per_cycle == 0 ||
        config.dma_write_bytes_per_cycle == 0 ||
        config.prediction_samples_per_cycle == 0 ||
        config.transform_samples_per_cycle == 0 ||
        config.cabac_bytes_per_cycle == 0) {
        throw std::invalid_argument("FIFO depths and throughputs must be non-zero");
    }
}

} // namespace

struct FifoCycleModel::Impl {
    explicit Impl(FifoCycleConfig value)
        : config(std::move(value)), input_fifo(config.input_depth),
          residual_fifo(config.residual_depth),
          coefficient_fifo(config.coefficient_depth),
          output_fifo(config.output_depth) {
        validate_config(config);
    }

    FifoCycleConfig config;
    FifoCycleStats stats;
    std::vector<CtuToken> tokens;
    std::size_t next_input = 0;
    std::size_t packets_completed = 0;
    std::size_t packets_expected = 0;
    bool is_running = false;
    bool is_done = false;

    BoundedFifo<CtuToken> input_fifo;
    BoundedFifo<CtuToken> residual_fifo;
    BoundedFifo<CtuToken> coefficient_fifo;
    BoundedFifo<OutputPacket> output_fifo;

    std::optional<CtuToken> dma_reader;
    std::uint64_t dma_read_remaining = 0;
    StageState prediction;
    StageState transform;
    StageState cabac;
    std::optional<OutputPacket> dma_writer;

    void update_maximums() {
        stats.max_input_occupancy = std::max(
            stats.max_input_occupancy,
            static_cast<std::uint32_t>(input_fifo.size()));
        stats.max_residual_occupancy = std::max(
            stats.max_residual_occupancy,
            static_cast<std::uint32_t>(residual_fifo.size()));
        stats.max_coefficient_occupancy = std::max(
            stats.max_coefficient_occupancy,
            static_cast<std::uint32_t>(coefficient_fifo.size()));
        stats.max_output_occupancy = std::max(
            stats.max_output_occupancy,
            static_cast<std::uint32_t>(output_fifo.size()));
    }

    void load_next_dma_reader() {
        if (!dma_reader && next_input < tokens.size()) {
            dma_reader = tokens[next_input++];
            dma_read_remaining = dma_reader->input_bytes;
        }
    }

    template <typename InputFifo, typename OutputFifo, typename Latency>
    void advance_stage(StageState& stage, InputFifo& input, OutputFifo& output,
                       Latency latency, std::uint64_t& active,
                       std::uint64_t& stalled) {
        if (stage.token) {
            ++active;
            if (stage.remaining != 0) --stage.remaining;
            if (stage.remaining == 0) {
                if (output.full()) {
                    ++stalled;
                } else {
                    output.push(*stage.token);
                    stage.token.reset();
                }
            }
        }
        if (!stage.token && !input.empty()) {
            stage.token = input.pop();
            stage.remaining = std::max<std::uint64_t>(1, latency(*stage.token));
        }
    }

    void advance_cabac() {
        if (cabac.token) {
            ++stats.cabac_active;
            if (cabac.remaining != 0) --cabac.remaining;
            if (cabac.remaining == 0) {
                if (output_fifo.full()) {
                    ++stats.stall_cabac_full;
                } else {
                    output_fifo.push(OutputPacket{cabac.token->output_bytes});
                    cabac.token.reset();
                }
            }
        }
        if (!cabac.token && !coefficient_fifo.empty()) {
            cabac.token = coefficient_fifo.pop();
            cabac.remaining = std::max<std::uint64_t>(
                1, cabac.token->cabac_cycles);
        }
    }

    void advance_dma_writer() {
        if (dma_writer) {
            ++stats.dma_write_active;
            const auto transferred = std::min<std::uint64_t>(
                dma_writer->bytes, config.dma_write_bytes_per_cycle);
            dma_writer->bytes -= transferred;
            stats.output_bytes += transferred;
            if (dma_writer->bytes == 0) {
                dma_writer.reset();
                ++packets_completed;
            }
        }
        if (!dma_writer && !output_fifo.empty()) {
            dma_writer = output_fifo.pop();
        }
    }

    void advance_dma_reader() {
        if (dma_reader) {
            ++stats.dma_read_active;
            if (dma_read_remaining != 0) {
                const auto transferred = std::min<std::uint64_t>(
                    dma_read_remaining, config.dma_read_bytes_per_cycle);
                dma_read_remaining -= transferred;
                stats.input_bytes += transferred;
            }
            if (dma_read_remaining == 0) {
                if (input_fifo.full()) {
                    ++stats.stall_input_full;
                } else {
                    input_fifo.push(*dma_reader);
                    dma_reader.reset();
                }
            }
        }
        load_next_dma_reader();
    }

    [[nodiscard]] bool pipeline_empty() const {
        return next_input == tokens.size() && !dma_reader && input_fifo.empty() &&
               !prediction.token && residual_fifo.empty() &&
               !transform.token && coefficient_fifo.empty() &&
               !cabac.token && output_fifo.empty() && !dma_writer;
    }
};

FifoCycleModel::FifoCycleModel(FifoCycleConfig config)
    : impl_(std::make_unique<Impl>(std::move(config))) {}

FifoCycleModel::~FifoCycleModel() = default;
FifoCycleModel::FifoCycleModel(FifoCycleModel&&) noexcept = default;
FifoCycleModel& FifoCycleModel::operator=(FifoCycleModel&&) noexcept = default;

void FifoCycleModel::start(const FifoFrameSpec& frame) {
    if (impl_->is_running) throw std::logic_error("FIFO model is already running");
    impl_->stats = {};
    impl_->tokens = make_tokens(frame, impl_->config);
    impl_->next_input = 0;
    impl_->packets_completed = 0;
    impl_->packets_expected = impl_->tokens.size() + (frame.prefix_bytes != 0);
    impl_->is_running = true;
    impl_->is_done = false;
    impl_->dma_reader.reset();
    impl_->prediction = {};
    impl_->transform = {};
    impl_->cabac = {};
    impl_->dma_writer.reset();
    if (frame.prefix_bytes != 0) {
        impl_->output_fifo.push(OutputPacket{frame.prefix_bytes});
    }
    impl_->load_next_dma_reader();
    impl_->update_maximums();
}

bool FifoCycleModel::tick() {
    if (!impl_->is_running) return false;
    ++impl_->stats.cycles;

    // Reverse order models one synchronous edge: downstream consumes before
    // upstream publishes, while each bounded FIFO still enforces backpressure.
    impl_->advance_dma_writer();
    impl_->advance_cabac();
    impl_->advance_stage(
        impl_->transform, impl_->residual_fifo, impl_->coefficient_fifo,
        [](const CtuToken& token) { return token.transform_cycles; },
        impl_->stats.transform_active, impl_->stats.stall_transform_full);
    impl_->advance_stage(
        impl_->prediction, impl_->input_fifo, impl_->residual_fifo,
        [](const CtuToken& token) { return token.prediction_cycles; },
        impl_->stats.prediction_active, impl_->stats.stall_prediction_full);
    impl_->advance_dma_reader();
    impl_->update_maximums();

    if (impl_->pipeline_empty() &&
        impl_->packets_completed == impl_->packets_expected) {
        impl_->is_running = false;
        impl_->is_done = true;
    }
    return true;
}

bool FifoCycleModel::running() const { return impl_->is_running; }
bool FifoCycleModel::done() const { return impl_->is_done; }
const FifoCycleStats& FifoCycleModel::stats() const { return impl_->stats; }
const FifoCycleConfig& FifoCycleModel::config() const { return impl_->config; }

void accumulate_fifo_stats(FifoCycleStats& total,
                           const FifoCycleStats& frame) {
    total.cycles += frame.cycles;
    total.input_bytes += frame.input_bytes;
    total.output_bytes += frame.output_bytes;
    total.dma_read_active += frame.dma_read_active;
    total.prediction_active += frame.prediction_active;
    total.transform_active += frame.transform_active;
    total.cabac_active += frame.cabac_active;
    total.dma_write_active += frame.dma_write_active;
    total.stall_input_full += frame.stall_input_full;
    total.stall_prediction_full += frame.stall_prediction_full;
    total.stall_transform_full += frame.stall_transform_full;
    total.stall_cabac_full += frame.stall_cabac_full;
    total.max_input_occupancy = std::max(
        total.max_input_occupancy, frame.max_input_occupancy);
    total.max_residual_occupancy = std::max(
        total.max_residual_occupancy, frame.max_residual_occupancy);
    total.max_coefficient_occupancy = std::max(
        total.max_coefficient_occupancy, frame.max_coefficient_occupancy);
    total.max_output_occupancy = std::max(
        total.max_output_occupancy, frame.max_output_occupancy);
}

} // namespace model
