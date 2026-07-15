#include "vpu_systemc_native.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace model::systemc_native {
namespace {

constexpr std::uint64_t kDmaReadBytesPerCycle = 16;
constexpr std::uint64_t kPredictionSamplesPerCycle = 16;
constexpr std::uint64_t kTransformSamplesPerCycle = 8;
constexpr std::uint64_t kCabacBytesPerCycle = 2;
constexpr std::uint64_t kDmaWriteBytesPerCycle = 8;

std::uint64_t divide_round_up(std::uint64_t value, std::uint64_t divisor) {
    return value / divisor + (value % divisor != 0);
}

std::uint32_t low32_saturated(std::uint64_t value) {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        value, std::numeric_limits<std::uint32_t>::max()));
}

std::uint32_t pack_depth(std::uint32_t depth) {
    return depth | (depth << 8) | (depth << 16) | (depth << 24);
}

std::uint32_t pack_maximums(const NativePipelineStats& stats) {
    return (stats.max_input_occupancy & 0xffU) |
           ((stats.max_residual_occupancy & 0xffU) << 8) |
           ((stats.max_coefficient_occupancy & 0xffU) << 16) |
           ((stats.max_output_occupancy & 0xffU) << 24);
}

std::uint32_t load_le32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

void store_le32(unsigned char* bytes, std::uint32_t value) {
    bytes[0] = static_cast<unsigned char>(value);
    bytes[1] = static_cast<unsigned char>(value >> 8);
    bytes[2] = static_cast<unsigned char>(value >> 16);
    bytes[3] = static_cast<unsigned char>(value >> 24);
}

std::uint64_t frame_samples(const JobConfig& job) {
    return static_cast<std::uint64_t>(job.width) * job.height * 3U / 2U;
}

} // namespace

TlmDmaBridge::TlmDmaBridge(sc_core::sc_module_name name,
                           std::size_t burst_bytes)
    : sc_module(name), burst_bytes_(burst_bytes) {
    if (burst_bytes_ == 0 ||
        burst_bytes_ > std::numeric_limits<unsigned>::max()) {
        throw std::invalid_argument("DMA burst size must fit TLM data_length");
    }
}

bool TlmDmaBridge::read(std::uint64_t address,
                        std::span<std::uint8_t> destination) {
    return transport(tlm::TLM_READ_COMMAND, address, destination.data(),
                     destination.size());
}

bool TlmDmaBridge::write(std::uint64_t address,
                         std::span<const std::uint8_t> source) {
    return transport(tlm::TLM_WRITE_COMMAND, address,
                     const_cast<std::uint8_t*>(source.data()), source.size());
}

bool TlmDmaBridge::transport(tlm::tlm_command command, std::uint64_t address,
                             std::uint8_t* data, std::size_t size) {
    std::size_t offset = 0;
    while (offset < size) {
        const auto burst = std::min(burst_bytes_, size - offset);
        if (address > std::numeric_limits<std::uint64_t>::max() - offset) {
            return false;
        }

        tlm::tlm_generic_payload transaction;
        transaction.set_command(command);
        transaction.set_address(address + offset);
        transaction.set_data_ptr(data + offset);
        transaction.set_data_length(static_cast<unsigned>(burst));
        transaction.set_streaming_width(static_cast<unsigned>(burst));
        transaction.set_byte_enable_ptr(nullptr);
        transaction.set_dmi_allowed(false);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(transaction, delay);
        if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
        if (transaction.is_response_error()) return false;
        offset += burst;
    }
    return true;
}

VpuController::VpuController(sc_core::sc_module_name name,
                             std::uint32_t fifo_depth,
                             NativePipelineStats& stats,
                             sc_core::sc_time clock_period)
    : sc_module(name), fixed_fifo_depth_(fifo_depth), stats_(stats),
      clock_period_(clock_period) {
    if (fixed_fifo_depth_ == 0 || fixed_fifo_depth_ > 255) {
        throw std::invalid_argument("native SystemC FIFO depth must be 1..255");
    }
    if (clock_period_ == sc_core::SC_ZERO_TIME) {
        throw std::invalid_argument("native SystemC clock period must be non-zero");
    }
    mmio_socket.register_b_transport(this, &VpuController::b_transport);
    irq.initialize(false);
    SC_THREAD(completion_thread);
}

std::uint64_t VpuController::src_address() const {
    return (static_cast<std::uint64_t>(src_hi_) << 32) | src_lo_;
}

std::uint64_t VpuController::dst_address() const {
    return (static_cast<std::uint64_t>(dst_hi_) << 32) | dst_lo_;
}

bool VpuController::busy() const {
    return (status_ & vpu_reg::STATUS_BUSY) != 0;
}

bool VpuController::valid_config() const {
    if (width_ == 0 || height_ == 0 || (width_ & 1U) || (height_ & 1U)) {
        return false;
    }
    const auto stride = stride_y_ == 0 ? width_ : stride_y_;
    if (stride < width_ || (stride & 1U)) return false;
    if (frame_count_ == 0 || frame_count_ > 1'000'000U) return false;
    if (qp_ > 51 || input_format_ != vpu_reg::FORMAT_YUV420P8 ||
        encoder_mode_ > vpu_reg::MODE_INTRA_DIRECTIONAL_TQ) {
        return false;
    }
    if (dst_capacity_ == 0 || !fifo_config_matches_hardware_) return false;

    const auto y_bytes = static_cast<std::uint64_t>(stride) * height_;
    const auto chroma_bytes =
        2ULL * (stride / 2U) * (static_cast<std::uint64_t>(height_) / 2U);
    if (y_bytes > std::numeric_limits<std::uint64_t>::max() - chroma_bytes) {
        return false;
    }
    const auto frame_bytes = y_bytes + chroma_bytes;
    if (frame_bytes > std::numeric_limits<std::uint64_t>::max() / frame_count_) {
        return false;
    }
    const auto all_frames = frame_bytes * frame_count_;
    return src_address() <= std::numeric_limits<std::uint64_t>::max() - all_frames &&
           dst_address() <=
               std::numeric_limits<std::uint64_t>::max() - dst_capacity_;
}

std::uint32_t VpuController::read_register(std::uint32_t offset) const {
    using namespace vpu_reg;
    switch (offset) {
    case ID: return 0x56505533U;
    case VERSION: return 0x00030009U; // native SystemC pipeline revision
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
    case FIFO_CONFIG: return pack_depth(fixed_fifo_depth_);
    case STALL_INPUT_FULL: return low32_saturated(stats_.stall_input_full);
    case STALL_PREDICTION_FULL:
        return low32_saturated(stats_.stall_prediction_full);
    case STALL_TRANSFORM_FULL:
        return low32_saturated(stats_.stall_transform_full);
    case STALL_CABAC_FULL: return low32_saturated(stats_.stall_cabac_full);
    case FIFO_MAX_OCCUPANCY: return pack_maximums(stats_);
    case DMA_READ_ACTIVE: return low32_saturated(stats_.dma_read_active);
    case PREDICTION_ACTIVE:
        return low32_saturated(stats_.prediction_active);
    case TRANSFORM_ACTIVE: return low32_saturated(stats_.transform_active);
    case CABAC_ACTIVE: return low32_saturated(stats_.cabac_active);
    case DMA_WRITE_ACTIVE: return low32_saturated(stats_.dma_write_active);
    default: return 0;
    }
}

void VpuController::write_register(std::uint32_t offset, std::uint32_t value) {
    using namespace vpu_reg;
    switch (offset) {
    case CONTROL:
        if (value & CONTROL_SOFT_RESET) {
            if (busy()) fail(VpuError::StartWhileBusy);
            else reset_registers();
            control_ = value & CONTROL_IRQ_ENABLE;
        } else {
            control_ = value & CONTROL_IRQ_ENABLE;
            if (value & CONTROL_START) start();
        }
        update_irq();
        break;
    case SRC_ADDR_LO: if (!busy()) src_lo_ = value; break;
    case SRC_ADDR_HI: if (!busy()) src_hi_ = value; break;
    case DST_ADDR_LO: if (!busy()) dst_lo_ = value; break;
    case DST_ADDR_HI: if (!busy()) dst_hi_ = value; break;
    case DST_CAPACITY: if (!busy()) dst_capacity_ = value; break;
    case WIDTH: if (!busy()) width_ = value; break;
    case HEIGHT: if (!busy()) height_ = value; break;
    case STRIDE_Y: if (!busy()) stride_y_ = value; break;
    case FRAME_COUNT: if (!busy()) frame_count_ = value; break;
    case QP: if (!busy()) qp_ = value; break;
    case INPUT_FORMAT: if (!busy()) input_format_ = value; break;
    case ENCODER_MODE: if (!busy()) encoder_mode_ = value; break;
    case FIFO_CONFIG:
        if (!busy()) fifo_config_matches_hardware_ =
            value == pack_depth(fixed_fifo_depth_);
        break;
    case IRQ_STATUS:
        irq_status_ &= ~value;
        update_irq();
        break;
    default:
        break;
    }
}

void VpuController::b_transport(tlm::tlm_generic_payload& transaction,
                                sc_core::sc_time& delay) {
    const auto address = transaction.get_address();
    auto* data = transaction.get_data_ptr();
    if (data == nullptr || transaction.get_data_length() != 4 ||
        transaction.get_streaming_width() < 4 ||
        transaction.get_byte_enable_ptr() != nullptr || (address & 3U) != 0 ||
        address + 4 > vpu_reg::REGISTER_SPACE_BYTES) {
        transaction.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    if (transaction.get_command() == tlm::TLM_READ_COMMAND) {
        store_le32(data, read_register(static_cast<std::uint32_t>(address)));
    } else if (transaction.get_command() == tlm::TLM_WRITE_COMMAND) {
        write_register(static_cast<std::uint32_t>(address), load_le32(data));
    } else {
        transaction.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    delay += clock_period_;
    transaction.set_response_status(tlm::TLM_OK_RESPONSE);
}

void VpuController::start() {
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
    stats_ = {};
    if (!valid_config()) {
        fail(VpuError::InvalidConfig);
        return;
    }

    JobConfig job;
    job.job_id = next_job_id_++;
    job.src_address = src_address();
    job.dst_address = dst_address();
    job.dst_capacity = dst_capacity_;
    job.width = width_;
    job.height = height_;
    job.stride_y = stride_y_ == 0 ? width_ : stride_y_;
    job.frame_count = frame_count_;
    job.qp = qp_;
    job.input_format = input_format_;
    job.encoder_mode = encoder_mode_;
    job.fifo_depth = fixed_fifo_depth_;
    if (!jobs.nb_write(job)) {
        fail(VpuError::StartWhileBusy);
        return;
    }
    active_job_id_ = job.job_id;
    start_time_ = sc_core::sc_time_stamp();
    status_ = vpu_reg::STATUS_BUSY;
    update_irq();
}

void VpuController::completion_thread() {
    bool reset_was_low = false;
    for (;;) {
        sc_core::wait(clk.posedge_event());
        if (!reset_n.read()) {
            if (!reset_was_low && !busy()) reset_registers();
            reset_was_low = true;
            continue;
        }
        reset_was_low = false;

        CompletionPacket completion;
        while (completions.nb_read(completion)) {
            if (completion.job_id != active_job_id_) continue;
            const auto elapsed = sc_core::sc_time_stamp() - start_time_;
            cycles_ = elapsed.value() / clock_period_.value();
            bitstream_bytes_ = completion.bitstream_bytes;
            frames_done_ = completion.frames_done;
            status_ &= ~vpu_reg::STATUS_BUSY;
            if (completion.error == VpuError::None) {
                status_ |= vpu_reg::STATUS_DONE;
                irq_status_ |= vpu_reg::IRQ_DONE;
            } else {
                status_ |= vpu_reg::STATUS_ERROR;
                error_code_ = completion.error;
                irq_status_ |= vpu_reg::IRQ_ERROR;
            }
            update_irq();
        }
    }
}

void VpuController::reset_registers() {
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
    active_job_id_ = 0;
    fifo_config_matches_hardware_ = true;
    stats_ = {};
    update_irq();
}

void VpuController::fail(VpuError error) {
    status_ &= ~vpu_reg::STATUS_BUSY;
    status_ |= vpu_reg::STATUS_ERROR;
    error_code_ = error;
    irq_status_ |= vpu_reg::IRQ_ERROR;
    update_irq();
}

void VpuController::update_irq() {
    const bool level = (control_ & vpu_reg::CONTROL_IRQ_ENABLE) != 0 &&
                       irq_status_ != 0;
    if (level) status_ |= vpu_reg::STATUS_IRQ;
    else status_ &= ~vpu_reg::STATUS_IRQ;
    irq.write(level);
}

InputDmaStage::InputDmaStage(sc_core::sc_module_name name,
                             std::uint32_t fifo_depth,
                             NativePipelineStats& stats)
    : sc_module(name), fifo_depth_(fifo_depth), stats_(stats) {
    SC_THREAD(run);
}

bool InputDmaStage::read_frame(const JobConfig& job,
                               std::uint32_t frame_index,
                               hevc::Yuv420Frame& frame) {
    const std::size_t stride = job.stride_y;
    const std::size_t chroma_stride = stride / 2;
    const std::size_t y_bytes = stride * job.height;
    const std::size_t chroma_rows = job.height / 2;
    const std::size_t chroma_bytes = chroma_stride * chroma_rows;
    const std::size_t frame_span = y_bytes + 2 * chroma_bytes;
    const auto base = job.src_address +
        static_cast<std::uint64_t>(frame_index) * frame_span;

    frame.width = job.width;
    frame.height = job.height;
    frame.y = {job.width, job.height,
               std::vector<std::uint8_t>(
                   static_cast<std::size_t>(job.width) * job.height)};
    frame.cb = {job.width / 2, job.height / 2,
                std::vector<std::uint8_t>(
                    static_cast<std::size_t>(job.width / 2) *
                    (job.height / 2))};
    frame.cr = frame.cb;

    std::vector<std::uint8_t> row(job.width);
    for (std::uint32_t y = 0; y < job.height; ++y) {
        if (!dma->read(base + static_cast<std::uint64_t>(y) * stride, row)) {
            return false;
        }
        std::copy(row.begin(), row.end(), frame.y.samples.begin() +
                  static_cast<std::ptrdiff_t>(y) * job.width);
    }
    row.resize(job.width / 2);
    for (std::uint32_t y = 0; y < job.height / 2; ++y) {
        if (!dma->read(base + y_bytes +
                           static_cast<std::uint64_t>(y) * chroma_stride,
                       row)) {
            return false;
        }
        std::copy(row.begin(), row.end(), frame.cb.samples.begin() +
                  static_cast<std::ptrdiff_t>(y) * (job.width / 2));
        if (!dma->read(base + y_bytes + chroma_bytes +
                           static_cast<std::uint64_t>(y) * chroma_stride,
                       row)) {
            return false;
        }
        std::copy(row.begin(), row.end(), frame.cr.samples.begin() +
                  static_cast<std::ptrdiff_t>(y) * (job.width / 2));
    }
    stats_.dma_read_active += divide_round_up(frame_span, kDmaReadBytesPerCycle);
    return true;
}

void InputDmaStage::push(FramePacket packet) {
    while (!output.nb_write(packet)) {
        ++stats_.stall_input_full;
        sc_core::wait(clk.posedge_event());
    }
    stats_.max_input_occupancy = std::max(
        stats_.max_input_occupancy,
        fifo_depth_ - static_cast<std::uint32_t>(output.num_free()));
}

void InputDmaStage::run() {
    for (;;) {
        while (!reset_n.read()) sc_core::wait(reset_n.posedge_event());
        JobConfig job;
        while (!jobs.nb_read(job)) sc_core::wait(clk.posedge_event());

        for (std::uint32_t index = 0; index < job.frame_count; ++index) {
            FramePacket packet;
            packet.job = job;
            packet.frame_index = index;
            packet.last = index + 1 == job.frame_count;
            packet.frame = std::make_shared<hevc::Yuv420Frame>();
            const bool failed = !read_frame(job, index, *packet.frame);
            if (failed) {
                packet.error = VpuError::DmaRead;
                packet.last = true;
                packet.frame.reset();
            }
            push(std::move(packet));
            if (failed) break;
        }
    }
}

PredictionStage::PredictionStage(sc_core::sc_module_name name,
                                 std::uint32_t fifo_depth,
                                 NativePipelineStats& stats)
    : sc_module(name), fifo_depth_(fifo_depth), stats_(stats) {
    SC_THREAD(run);
}

void PredictionStage::wait_active(std::uint64_t cycles) {
    for (std::uint64_t cycle = 0; cycle < cycles; ++cycle) {
        ++stats_.prediction_active;
        sc_core::wait(clk.posedge_event());
    }
}

void PredictionStage::push(FramePacket packet) {
    while (!output.nb_write(packet)) {
        ++stats_.stall_prediction_full;
        sc_core::wait(clk.posedge_event());
    }
    stats_.max_residual_occupancy = std::max(
        stats_.max_residual_occupancy,
        fifo_depth_ - static_cast<std::uint32_t>(output.num_free()));
}

void PredictionStage::run() {
    for (;;) {
        FramePacket packet;
        while (!input.nb_read(packet)) sc_core::wait(clk.posedge_event());
        if (packet.error == VpuError::None &&
            packet.job.encoder_mode != vpu_reg::MODE_PCM) {
            wait_active(divide_round_up(frame_samples(packet.job),
                                        kPredictionSamplesPerCycle));
        }
        push(std::move(packet));
    }
}

TransformStage::TransformStage(sc_core::sc_module_name name,
                               std::uint32_t fifo_depth,
                               NativePipelineStats& stats)
    : sc_module(name), fifo_depth_(fifo_depth), stats_(stats) {
    SC_THREAD(run);
}

void TransformStage::wait_active(std::uint64_t cycles) {
    for (std::uint64_t cycle = 0; cycle < cycles; ++cycle) {
        ++stats_.transform_active;
        sc_core::wait(clk.posedge_event());
    }
}

void TransformStage::push(FramePacket packet) {
    while (!output.nb_write(packet)) {
        ++stats_.stall_transform_full;
        sc_core::wait(clk.posedge_event());
    }
    stats_.max_coefficient_occupancy = std::max(
        stats_.max_coefficient_occupancy,
        fifo_depth_ - static_cast<std::uint32_t>(output.num_free()));
}

void TransformStage::run() {
    for (;;) {
        FramePacket packet;
        while (!input.nb_read(packet)) sc_core::wait(clk.posedge_event());
        if (packet.error == VpuError::None &&
            packet.job.encoder_mode >= vpu_reg::MODE_INTRA_DC_TQ) {
            wait_active(divide_round_up(frame_samples(packet.job),
                                        kTransformSamplesPerCycle));
        }
        push(std::move(packet));
    }
}

CabacStage::CabacStage(sc_core::sc_module_name name,
                       std::uint32_t fifo_depth,
                       NativePipelineStats& stats)
    : sc_module(name), fifo_depth_(fifo_depth), stats_(stats) {
    SC_THREAD(run);
}

void CabacStage::wait_active(std::uint64_t cycles) {
    for (std::uint64_t cycle = 0; cycle < cycles; ++cycle) {
        ++stats_.cabac_active;
        sc_core::wait(clk.posedge_event());
    }
}

void CabacStage::push(BitstreamPacket packet) {
    while (!output.nb_write(packet)) {
        ++stats_.stall_cabac_full;
        sc_core::wait(clk.posedge_event());
    }
    stats_.max_output_occupancy = std::max(
        stats_.max_output_occupancy,
        fifo_depth_ - static_cast<std::uint32_t>(output.num_free()));
}

void CabacStage::run() {
    for (;;) {
        FramePacket input_packet;
        while (!input.nb_read(input_packet)) sc_core::wait(clk.posedge_event());

        BitstreamPacket output_packet;
        output_packet.job = input_packet.job;
        output_packet.frame_index = input_packet.frame_index;
        output_packet.last = input_packet.last;
        output_packet.error = input_packet.error;
        if (output_packet.error == VpuError::None) {
            try {
                hevc::HevcPcmEncoder encoder({
                    input_packet.job.width, input_packet.job.height,
                    static_cast<int>(input_packet.job.qp),
                    static_cast<hevc::CodingMode>(
                        input_packet.job.encoder_mode)});
                if (input_packet.frame_index == 0) {
                    output_packet.bytes = encoder.parameter_sets_annex_b();
                }
                const auto frame_bytes = encoder.encode_idr_frame_annex_b(
                    *input_packet.frame, input_packet.frame_index);
                output_packet.bytes.insert(output_packet.bytes.end(),
                                           frame_bytes.begin(), frame_bytes.end());
                wait_active(divide_round_up(output_packet.bytes.size(),
                                            kCabacBytesPerCycle));
            } catch (const std::exception&) {
                output_packet.error = VpuError::InvalidConfig;
                output_packet.last = true;
                output_packet.bytes.clear();
            }
        }
        push(std::move(output_packet));
    }
}

OutputDmaStage::OutputDmaStage(sc_core::sc_module_name name,
                               NativePipelineStats& stats)
    : sc_module(name), stats_(stats) {
    SC_THREAD(run);
}

void OutputDmaStage::complete(const CompletionPacket& completion) {
    while (!completions.nb_write(completion)) {
        sc_core::wait(clk.posedge_event());
    }
}

void OutputDmaStage::run() {
    std::uint64_t active_job = 0;
    std::uint32_t output_offset = 0;
    std::uint32_t frames_done = 0;
    for (;;) {
        BitstreamPacket packet;
        while (!input.nb_read(packet)) sc_core::wait(clk.posedge_event());
        if (packet.job.job_id != active_job) {
            active_job = packet.job.job_id;
            output_offset = 0;
            frames_done = 0;
        }

        VpuError error = packet.error;
        if (error == VpuError::None) {
            const auto remaining = packet.job.dst_capacity -
                std::min(packet.job.dst_capacity, output_offset);
            if (packet.bytes.size() > remaining) {
                error = VpuError::OutputOverflow;
            } else {
                stats_.dma_write_active += divide_round_up(
                    packet.bytes.size(), kDmaWriteBytesPerCycle);
                if (!dma->write(packet.job.dst_address + output_offset,
                                packet.bytes)) {
                    error = VpuError::DmaWrite;
                } else {
                    output_offset += static_cast<std::uint32_t>(packet.bytes.size());
                    ++frames_done;
                }
            }
        }

        if (packet.last || error != VpuError::None) {
            complete({packet.job.job_id, error, frames_done, output_offset});
        }
    }
}

VpuSystemCNative::VpuSystemCNative(sc_core::sc_module_name name,
                                   std::uint32_t fifo_depth,
                                   sc_core::sc_time clock_period,
                                   std::size_t dma_burst_bytes)
    : sc_module(name), job_fifo_("job_fifo", 1),
      input_fifo_("input_fifo", static_cast<int>(fifo_depth)),
      residual_fifo_("residual_fifo", static_cast<int>(fifo_depth)),
      coefficient_fifo_("coefficient_fifo", static_cast<int>(fifo_depth)),
      output_fifo_("output_fifo", static_cast<int>(fifo_depth)),
      completion_fifo_("completion_fifo", 1),
      dma_bridge_("dma_bridge", dma_burst_bytes),
      controller_("controller", fifo_depth, stats_, clock_period),
      input_dma_("input_dma", fifo_depth, stats_),
      prediction_("prediction", fifo_depth, stats_),
      transform_("transform", fifo_depth, stats_),
      cabac_("cabac", fifo_depth, stats_), output_dma_("output_dma", stats_) {
    if (fifo_depth == 0 || fifo_depth > 255) {
        throw std::invalid_argument("native SystemC FIFO depth must be 1..255");
    }

    controller_.clk(clk);
    controller_.reset_n(reset_n);
    controller_.irq(internal_irq_);
    controller_.jobs(job_fifo_);
    controller_.completions(completion_fifo_);

    input_dma_.clk(clk);
    input_dma_.reset_n(reset_n);
    input_dma_.jobs(job_fifo_);
    input_dma_.output(input_fifo_);
    input_dma_.dma(dma_bridge_);

    prediction_.clk(clk);
    prediction_.reset_n(reset_n);
    prediction_.input(input_fifo_);
    prediction_.output(residual_fifo_);

    transform_.clk(clk);
    transform_.reset_n(reset_n);
    transform_.input(residual_fifo_);
    transform_.output(coefficient_fifo_);

    cabac_.clk(clk);
    cabac_.reset_n(reset_n);
    cabac_.input(coefficient_fifo_);
    cabac_.output(output_fifo_);

    output_dma_.clk(clk);
    output_dma_.reset_n(reset_n);
    output_dma_.input(output_fifo_);
    output_dma_.completions(completion_fifo_);
    output_dma_.dma(dma_bridge_);

    irq.initialize(false);
    SC_METHOD(forward_irq);
    sensitive << internal_irq_;
}

void VpuSystemCNative::forward_irq() {
    irq.write(internal_irq_.read());
}

} // namespace model::systemc_native
