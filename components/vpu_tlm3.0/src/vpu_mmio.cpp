#include "model/vpu_mmio.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace model {
namespace {

std::vector<std::size_t>
annex_b_nal_sizes(std::span<const std::uint8_t> bytes) {
    std::vector<std::size_t> starts;
    for (std::size_t i = 0; i + 4 <= bytes.size(); ++i) {
        if (bytes[i] == 0 && bytes[i + 1] == 0 &&
            bytes[i + 2] == 0 && bytes[i + 3] == 1) {
            starts.push_back(i);
            i += 3;
        }
    }
    if (starts.empty() || starts.front() != 0) {
        throw std::runtime_error("encoder returned malformed Annex-B data");
    }
    std::vector<std::size_t> sizes;
    sizes.reserve(starts.size());
    for (std::size_t i = 0; i < starts.size(); ++i) {
        const std::size_t end = i + 1 < starts.size() ? starts[i + 1] : bytes.size();
        sizes.push_back(end - starts[i]);
    }
    return sizes;
}

std::uint32_t low32_saturated(std::uint64_t value) {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        value, std::numeric_limits<std::uint32_t>::max()));
}

std::uint32_t pack_fifo_config(const FifoCycleConfig& config) {
    return (config.input_depth & 0xffU) |
           ((config.residual_depth & 0xffU) << 8) |
           ((config.coefficient_depth & 0xffU) << 16) |
           ((config.output_depth & 0xffU) << 24);
}

std::uint32_t pack_fifo_maximums(const FifoCycleStats& stats) {
    return (stats.max_input_occupancy & 0xffU) |
           ((stats.max_residual_occupancy & 0xffU) << 8) |
           ((stats.max_coefficient_occupancy & 0xffU) << 16) |
           ((stats.max_output_occupancy & 0xffU) << 24);
}

} // namespace

bool FlatMemory::read(std::uint64_t address,
                      std::span<std::uint8_t> destination) {
    if (address > bytes_.size() || destination.size() > bytes_.size() - address) {
        return false;
    }
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(address),
                destination.size(), destination.begin());
    return true;
}

bool FlatMemory::write(std::uint64_t address,
                       std::span<const std::uint8_t> source) {
    if (address > bytes_.size() || source.size() > bytes_.size() - address) {
        return false;
    }
    std::copy(source.begin(), source.end(),
              bytes_.begin() + static_cast<std::ptrdiff_t>(address));
    return true;
}

VpuMmioDevice::VpuMmioDevice(MemoryInterface& memory, IrqCallback irq)
    : memory_(memory), irq_callback_(std::move(irq)) {
    reset();
}

std::uint64_t VpuMmioDevice::src_address() const {
    return (static_cast<std::uint64_t>(src_hi_) << 32) | src_lo_;
}

std::uint64_t VpuMmioDevice::dst_address() const {
    return (static_cast<std::uint64_t>(dst_hi_) << 32) | dst_lo_;
}

std::size_t VpuMmioDevice::input_frame_span() const {
    const std::size_t stride = stride_y_ == 0 ? width_ : stride_y_;
    const std::size_t chroma_stride = stride / 2;
    return stride * height_ + 2 * chroma_stride * (height_ / 2);
}

bool VpuMmioDevice::valid_config() const {
    if (width_ == 0 || height_ == 0 || (width_ & 1U) || (height_ & 1U)) return false;
    const std::uint32_t stride = stride_y_ == 0 ? width_ : stride_y_;
    if (stride < width_ || (stride & 1U)) return false;
    if (frame_count_ == 0 || frame_count_ > 1'000'000U) return false;
    if (qp_ > 51 || input_format_ != vpu_reg::FORMAT_YUV420P8 ||
        encoder_mode_ > vpu_reg::MODE_INTRA_DIRECTIONAL_TQ) return false;
    if (fifo_config_.input_depth == 0 || fifo_config_.residual_depth == 0 ||
        fifo_config_.coefficient_depth == 0 || fifo_config_.output_depth == 0) {
        return false;
    }
    if (dst_capacity_ == 0) return false;

    const auto y_bytes = static_cast<std::uint64_t>(stride) * height_;
    const auto chroma_bytes =
        2ULL * (stride / 2U) * (static_cast<std::uint64_t>(height_) / 2U);
    if (y_bytes > std::numeric_limits<std::uint64_t>::max() - chroma_bytes) {
        return false;
    }
    const auto frame_span = y_bytes + chroma_bytes;
    if (frame_span > std::numeric_limits<std::size_t>::max() ||
        frame_span > std::numeric_limits<std::uint64_t>::max() / frame_count_) {
        return false;
    }
    const auto all_frames = frame_span * frame_count_;
    if (src_address() > std::numeric_limits<std::uint64_t>::max() - all_frames ||
        dst_address() > std::numeric_limits<std::uint64_t>::max() - dst_capacity_) {
        return false;
    }
    return true;
}

std::uint32_t VpuMmioDevice::read32(std::uint32_t offset) const {
    using namespace vpu_reg;
    switch (offset) {
    case ID: return 0x56505533U; // "VPU3"
    case VERSION: return 0x00030008U; // 3.8 M7a FIFO cycle-model revision
    case CONTROL: return control_;
    case STATUS: return status_;
    case SRC_ADDR_LO: return src_lo_;
    case SRC_ADDR_HI: return src_hi_;
    case DST_ADDR_LO: return dst_lo_;
    case DST_ADDR_HI: return dst_hi_;
    case DST_CAPACITY: return dst_capacity_;
    case WIDTH: return width_;
    case HEIGHT: return height_;
    case STRIDE_Y: return stride_y_;
    case FRAME_COUNT: return frame_count_;
    case QP: return qp_;
    case INPUT_FORMAT: return input_format_;
    case BITSTREAM_BYTES: return bitstream_bytes_;
    case FRAMES_DONE: return frames_done_;
    case ERROR_CODE: return static_cast<std::uint32_t>(error_code_);
    case IRQ_STATUS: return irq_status_;
    case CYCLES_LO: return static_cast<std::uint32_t>(cycles_);
    case CYCLES_HI: return static_cast<std::uint32_t>(cycles_ >> 32);
    case ENCODER_MODE: return encoder_mode_;
    case FIFO_CONFIG: return pack_fifo_config(fifo_config_);
    case STALL_INPUT_FULL: return low32_saturated(fifo_stats_.stall_input_full);
    case STALL_PREDICTION_FULL:
        return low32_saturated(fifo_stats_.stall_prediction_full);
    case STALL_TRANSFORM_FULL:
        return low32_saturated(fifo_stats_.stall_transform_full);
    case STALL_CABAC_FULL: return low32_saturated(fifo_stats_.stall_cabac_full);
    case FIFO_MAX_OCCUPANCY: return pack_fifo_maximums(fifo_stats_);
    case DMA_READ_ACTIVE: return low32_saturated(fifo_stats_.dma_read_active);
    case PREDICTION_ACTIVE:
        return low32_saturated(fifo_stats_.prediction_active);
    case TRANSFORM_ACTIVE: return low32_saturated(fifo_stats_.transform_active);
    case CABAC_ACTIVE: return low32_saturated(fifo_stats_.cabac_active);
    case DMA_WRITE_ACTIVE: return low32_saturated(fifo_stats_.dma_write_active);
    default: return 0;
    }
}

void VpuMmioDevice::write32(std::uint32_t offset, std::uint32_t value) {
    using namespace vpu_reg;
    switch (offset) {
    case CONTROL:
        if (value & CONTROL_SOFT_RESET) {
            reset();
            control_ = value & CONTROL_IRQ_ENABLE;
        } else {
            control_ = value & CONTROL_IRQ_ENABLE;
            if (value & CONTROL_START) start();
        }
        update_irq();
        break;
    case SRC_ADDR_LO: src_lo_ = value; break;
    case SRC_ADDR_HI: src_hi_ = value; break;
    case DST_ADDR_LO: dst_lo_ = value; break;
    case DST_ADDR_HI: dst_hi_ = value; break;
    case DST_CAPACITY: dst_capacity_ = value; break;
    case WIDTH: width_ = value; break;
    case HEIGHT: height_ = value; break;
    case STRIDE_Y: stride_y_ = value; break;
    case FRAME_COUNT: frame_count_ = value; break;
    case QP: qp_ = value; break;
    case INPUT_FORMAT: input_format_ = value; break;
    case ENCODER_MODE: encoder_mode_ = value; break;
    case FIFO_CONFIG:
        fifo_config_.input_depth = value & 0xffU;
        fifo_config_.residual_depth = (value >> 8) & 0xffU;
        fifo_config_.coefficient_depth = (value >> 16) & 0xffU;
        fifo_config_.output_depth = (value >> 24) & 0xffU;
        break;
    case IRQ_STATUS:
        irq_status_ &= ~value; // write-one-to-clear
        update_irq();
        break;
    default:
        break; // read-only or reserved
    }
}

void VpuMmioDevice::reset() {
    control_ = 0;
    status_ = 0;
    src_lo_ = src_hi_ = dst_lo_ = dst_hi_ = 0;
    dst_capacity_ = width_ = height_ = stride_y_ = frame_count_ = 0;
    qp_ = 26;
    input_format_ = vpu_reg::FORMAT_YUV420P8;
    encoder_mode_ = vpu_reg::MODE_PCM;
    bitstream_bytes_ = frames_done_ = 0;
    error_code_ = VpuError::None;
    irq_status_ = 0;
    cycles_ = 0;
    headers_written_ = false;
    fifo_config_ = {};
    fifo_stats_ = {};
    cycle_model_.reset();
    pending_output_.clear();
    update_irq();
}

bool VpuMmioDevice::busy() const {
    return (status_ & vpu_reg::STATUS_BUSY) != 0;
}

void VpuMmioDevice::start() {
    using namespace vpu_reg;
    if (busy()) {
        fail(VpuError::StartWhileBusy);
        return;
    }
    status_ = 0;
    bitstream_bytes_ = 0;
    frames_done_ = 0;
    error_code_ = VpuError::None;
    irq_status_ = 0;
    cycles_ = 0;
    headers_written_ = false;
    fifo_stats_ = {};
    cycle_model_.reset();
    pending_output_.clear();
    if (!valid_config()) {
        fail(VpuError::InvalidConfig);
        return;
    }
    status_ = STATUS_BUSY;
    update_irq();
}

bool VpuMmioDevice::read_frame(std::uint32_t index, hevc::Yuv420Frame& frame) {
    const std::size_t stride = stride_y_ == 0 ? width_ : stride_y_;
    const std::size_t cstride = stride / 2;
    const std::size_t y_bytes = stride * height_;
    const std::size_t c_rows = height_ / 2;
    const std::size_t c_bytes = cstride * c_rows;
    const std::uint64_t base = src_address() +
        static_cast<std::uint64_t>(index) * input_frame_span();

    frame.width = width_;
    frame.height = height_;
    frame.y = {width_, height_, std::vector<std::uint8_t>(
        static_cast<std::size_t>(width_) * height_)};
    frame.cb = {width_ / 2, height_ / 2, std::vector<std::uint8_t>(
        static_cast<std::size_t>(width_ / 2) * (height_ / 2))};
    frame.cr = frame.cb;

    std::vector<std::uint8_t> row(width_);
    for (std::uint32_t y = 0; y < height_; ++y) {
        if (!memory_.read(base + static_cast<std::uint64_t>(y) * stride, row)) return false;
        std::copy(row.begin(), row.end(), frame.y.samples.begin() +
                  static_cast<std::ptrdiff_t>(y) * width_);
    }
    row.resize(width_ / 2);
    for (std::uint32_t y = 0; y < height_ / 2; ++y) {
        if (!memory_.read(base + y_bytes + static_cast<std::uint64_t>(y) * cstride,
                          row)) return false;
        std::copy(row.begin(), row.end(), frame.cb.samples.begin() +
                  static_cast<std::ptrdiff_t>(y) * (width_ / 2));
        if (!memory_.read(base + y_bytes + c_bytes +
                          static_cast<std::uint64_t>(y) * cstride, row)) return false;
        std::copy(row.begin(), row.end(), frame.cr.samples.begin() +
                  static_cast<std::ptrdiff_t>(y) * (width_ / 2));
    }
    return true;
}

bool VpuMmioDevice::append_output(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > dst_capacity_ - std::min(dst_capacity_, bitstream_bytes_)) {
        return false;
    }
    if (!memory_.write(dst_address() + bitstream_bytes_, bytes)) return false;
    bitstream_bytes_ += static_cast<std::uint32_t>(bytes.size());
    return true;
}

bool VpuMmioDevice::prepare_frame() {
    try {
        hevc::HevcPcmEncoder encoder({
            width_, height_, static_cast<int>(qp_),
            static_cast<hevc::CodingMode>(encoder_mode_)});
        std::vector<std::uint8_t> headers;
        if (!headers_written_) {
            headers = encoder.parameter_sets_annex_b();
            headers_written_ = true;
        }

        hevc::Yuv420Frame frame;
        if (!read_frame(frames_done_, frame)) {
            fail(VpuError::DmaRead);
            return false;
        }
        const auto encoded = encoder.encode_idr_frame_annex_b(frame, frames_done_);
        const auto remaining =
            dst_capacity_ - std::min(dst_capacity_, bitstream_bytes_);
        if (headers.size() + encoded.size() > remaining) {
            fail(VpuError::OutputOverflow);
            return false;
        }

        pending_output_.clear();
        pending_output_.reserve(headers.size() + encoded.size());
        pending_output_.insert(
            pending_output_.end(), headers.begin(), headers.end());
        pending_output_.insert(
            pending_output_.end(), encoded.begin(), encoded.end());

        FifoFrameSpec spec;
        spec.width = width_;
        spec.height = height_;
        spec.mode = static_cast<hevc::CodingMode>(encoder_mode_);
        spec.prefix_bytes = headers.size();
        spec.ctu_bitstream_bytes = annex_b_nal_sizes(encoded);
        cycle_model_ = std::make_unique<FifoCycleModel>(fifo_config_);
        cycle_model_->start(spec);
        return true;
    } catch (const std::exception&) {
        fail(VpuError::InvalidConfig);
        return false;
    }
}

bool VpuMmioDevice::tick() {
    if (!busy()) return false;
    if (!cycle_model_ && !prepare_frame()) return true;

    (void)cycle_model_->tick();
    ++cycles_;
    if (!cycle_model_->done()) return true;

    if (!append_output(pending_output_)) {
        const auto remaining =
            dst_capacity_ - std::min(dst_capacity_, bitstream_bytes_);
        fail(pending_output_.size() > remaining ? VpuError::OutputOverflow
                                                : VpuError::DmaWrite);
        return true;
    }
    accumulate_fifo_stats(fifo_stats_, cycle_model_->stats());
    cycle_model_.reset();
    pending_output_.clear();
    ++frames_done_;
    if (frames_done_ == frame_count_) finish_success();
    return true;
}

bool VpuMmioDevice::step() {
    if (!busy()) return false;
    const auto target = frames_done_ + 1;
    while (busy() && frames_done_ < target) {
        (void)tick();
    }
    return true;
}

void VpuMmioDevice::run_to_completion() {
    while (busy()) {
        (void)tick();
    }
}

void VpuMmioDevice::finish_success() {
    status_ &= ~vpu_reg::STATUS_BUSY;
    status_ |= vpu_reg::STATUS_DONE;
    irq_status_ |= vpu_reg::IRQ_DONE;
    update_irq();
}

void VpuMmioDevice::fail(VpuError error) {
    status_ &= ~vpu_reg::STATUS_BUSY;
    status_ |= vpu_reg::STATUS_ERROR;
    error_code_ = error;
    irq_status_ |= vpu_reg::IRQ_ERROR;
    update_irq();
}

void VpuMmioDevice::update_irq() {
    const bool next = (control_ & vpu_reg::CONTROL_IRQ_ENABLE) && irq_status_ != 0;
    if (next) status_ |= vpu_reg::STATUS_IRQ;
    else status_ &= ~vpu_reg::STATUS_IRQ;
    if (next != irq_level_) {
        irq_level_ = next;
        if (irq_callback_) irq_callback_(irq_level_);
    }
}

} // namespace model
