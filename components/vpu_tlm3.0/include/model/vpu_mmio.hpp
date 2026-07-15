#pragma once

#include "hevc/hevc_pcm_encoder.hpp"
#include "model/fifo_cycle_model.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace model {

namespace vpu_reg {
constexpr std::uint32_t ID = 0x000;
constexpr std::uint32_t VERSION = 0x004;
constexpr std::uint32_t CONTROL = 0x008;
constexpr std::uint32_t STATUS = 0x00c;
constexpr std::uint32_t SRC_ADDR_LO = 0x010;
constexpr std::uint32_t SRC_ADDR_HI = 0x014;
constexpr std::uint32_t DST_ADDR_LO = 0x018;
constexpr std::uint32_t DST_ADDR_HI = 0x01c;
constexpr std::uint32_t DST_CAPACITY = 0x020;
constexpr std::uint32_t WIDTH = 0x024;
constexpr std::uint32_t HEIGHT = 0x028;
constexpr std::uint32_t STRIDE_Y = 0x02c;
constexpr std::uint32_t FRAME_COUNT = 0x030;
constexpr std::uint32_t QP = 0x034;
constexpr std::uint32_t INPUT_FORMAT = 0x038;
constexpr std::uint32_t BITSTREAM_BYTES = 0x03c;
constexpr std::uint32_t FRAMES_DONE = 0x040;
constexpr std::uint32_t ERROR_CODE = 0x044;
constexpr std::uint32_t IRQ_STATUS = 0x048;
constexpr std::uint32_t CYCLES_LO = 0x04c;
constexpr std::uint32_t CYCLES_HI = 0x050;
constexpr std::uint32_t ENCODER_MODE = 0x054;
constexpr std::uint32_t FIFO_CONFIG = 0x058;
constexpr std::uint32_t STALL_INPUT_FULL = 0x05c;
constexpr std::uint32_t STALL_PREDICTION_FULL = 0x060;
constexpr std::uint32_t STALL_TRANSFORM_FULL = 0x064;
constexpr std::uint32_t STALL_CABAC_FULL = 0x068;
constexpr std::uint32_t FIFO_MAX_OCCUPANCY = 0x06c;
constexpr std::uint32_t DMA_READ_ACTIVE = 0x070;
constexpr std::uint32_t PREDICTION_ACTIVE = 0x074;
constexpr std::uint32_t TRANSFORM_ACTIVE = 0x078;
constexpr std::uint32_t CABAC_ACTIVE = 0x07c;
constexpr std::uint32_t DMA_WRITE_ACTIVE = 0x080;
constexpr std::uint32_t REGISTER_SPACE_BYTES = 0x100;

constexpr std::uint32_t CONTROL_START = 1U << 0;
constexpr std::uint32_t CONTROL_SOFT_RESET = 1U << 1;
constexpr std::uint32_t CONTROL_IRQ_ENABLE = 1U << 2;

constexpr std::uint32_t STATUS_BUSY = 1U << 0;
constexpr std::uint32_t STATUS_DONE = 1U << 1;
constexpr std::uint32_t STATUS_ERROR = 1U << 2;
constexpr std::uint32_t STATUS_IRQ = 1U << 3;

constexpr std::uint32_t IRQ_DONE = 1U << 0;
constexpr std::uint32_t IRQ_ERROR = 1U << 1;

constexpr std::uint32_t FORMAT_YUV420P8 = 0;
constexpr std::uint32_t MODE_PCM = 0;
constexpr std::uint32_t MODE_INTRA_DC = 1;
constexpr std::uint32_t MODE_HYBRID_DC = 2;
constexpr std::uint32_t MODE_INTRA_DC_TQ = 3;
constexpr std::uint32_t MODE_INTRA_FULL_TQ = 4;
constexpr std::uint32_t MODE_INTRA_FULL_TQ16 = 5;
constexpr std::uint32_t MODE_INTRA_ADAPTIVE_TQ = 6;
constexpr std::uint32_t MODE_INTRA_DIRECTIONAL_TQ = 7;
} // namespace vpu_reg

enum class VpuError : std::uint32_t {
    None = 0,
    InvalidConfig = 1,
    DmaRead = 2,
    OutputOverflow = 3,
    DmaWrite = 4,
    StartWhileBusy = 5,
};

class MemoryInterface {
public:
    virtual ~MemoryInterface() = default;
    virtual bool read(std::uint64_t address, std::span<std::uint8_t> destination) = 0;
    virtual bool write(std::uint64_t address,
                       std::span<const std::uint8_t> source) = 0;
};

class FlatMemory final : public MemoryInterface {
public:
    explicit FlatMemory(std::size_t size) : bytes_(size) {}

    bool read(std::uint64_t address, std::span<std::uint8_t> destination) override;
    bool write(std::uint64_t address,
               std::span<const std::uint8_t> source) override;

    [[nodiscard]] std::vector<std::uint8_t>& bytes() { return bytes_; }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const { return bytes_; }

private:
    std::vector<std::uint8_t> bytes_;
};

class VpuMmioDevice {
public:
    using IrqCallback = std::function<void(bool level)>;

    explicit VpuMmioDevice(MemoryInterface& memory, IrqCallback irq = {});

    [[nodiscard]] std::uint32_t read32(std::uint32_t offset) const;
    void write32(std::uint32_t offset, std::uint32_t value);

    // Process at most one frame. Returns true when work was performed.
    bool step();
    // Advance exactly one architectural FIFO cycle.
    bool tick();
    void run_to_completion();
    void reset();

    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool irq_level() const { return irq_level_; }

private:
    [[nodiscard]] std::uint64_t src_address() const;
    [[nodiscard]] std::uint64_t dst_address() const;
    [[nodiscard]] std::size_t input_frame_span() const;
    [[nodiscard]] bool valid_config() const;
    bool read_frame(std::uint32_t index, hevc::Yuv420Frame& frame);
    bool append_output(std::span<const std::uint8_t> bytes);
    bool prepare_frame();
    void start();
    void finish_success();
    void fail(VpuError error);
    void update_irq();

    MemoryInterface& memory_;
    IrqCallback irq_callback_;

    std::uint32_t control_ = 0;
    std::uint32_t status_ = 0;
    std::uint32_t src_lo_ = 0;
    std::uint32_t src_hi_ = 0;
    std::uint32_t dst_lo_ = 0;
    std::uint32_t dst_hi_ = 0;
    std::uint32_t dst_capacity_ = 0;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t stride_y_ = 0;
    std::uint32_t frame_count_ = 0;
    std::uint32_t qp_ = 26;
    std::uint32_t input_format_ = vpu_reg::FORMAT_YUV420P8;
    std::uint32_t encoder_mode_ = vpu_reg::MODE_PCM;
    std::uint32_t bitstream_bytes_ = 0;
    std::uint32_t frames_done_ = 0;
    VpuError error_code_ = VpuError::None;
    std::uint32_t irq_status_ = 0;
    std::uint64_t cycles_ = 0;
    bool headers_written_ = false;
    bool irq_level_ = false;
    FifoCycleConfig fifo_config_{};
    FifoCycleStats fifo_stats_{};
    std::unique_ptr<FifoCycleModel> cycle_model_;
    std::vector<std::uint8_t> pending_output_;
};

} // namespace model
