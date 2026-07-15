#pragma once

#include "systemc_vpu/packets.hpp"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace model::systemc_native {

class DmaTransportIf : virtual public sc_core::sc_interface {
public:
    virtual bool read(std::uint64_t address,
                      std::span<std::uint8_t> destination) = 0;
    virtual bool write(std::uint64_t address,
                       std::span<const std::uint8_t> source) = 0;
};

class TlmDmaBridge final : public sc_core::sc_module,
                           public DmaTransportIf {
public:
    tlm_utils::simple_initiator_socket<TlmDmaBridge> socket{"socket"};

    TlmDmaBridge(sc_core::sc_module_name name, std::size_t burst_bytes = 64);

    bool read(std::uint64_t address,
              std::span<std::uint8_t> destination) override;
    bool write(std::uint64_t address,
               std::span<const std::uint8_t> source) override;

private:
    bool transport(tlm::tlm_command command, std::uint64_t address,
                   std::uint8_t* data, std::size_t size);

    std::size_t burst_bytes_;
};

class VpuController final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(VpuController);

    tlm_utils::simple_target_socket<VpuController> mmio_socket{"mmio_socket"};
    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_out<bool> irq{"irq"};
    sc_core::sc_fifo_out<JobConfig> jobs{"jobs"};
    sc_core::sc_fifo_in<CompletionPacket> completions{"completions"};

    VpuController(sc_core::sc_module_name name, std::uint32_t fifo_depth,
                  NativePipelineStats& stats,
                  sc_core::sc_time clock_period);

private:
    void b_transport(tlm::tlm_generic_payload& transaction,
                     sc_core::sc_time& delay);
    void completion_thread();
    void start();
    void reset_registers();
    void fail(VpuError error);
    void update_irq();
    [[nodiscard]] bool valid_config() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] std::uint64_t src_address() const;
    [[nodiscard]] std::uint64_t dst_address() const;
    [[nodiscard]] std::uint32_t read_register(std::uint32_t offset) const;
    void write_register(std::uint32_t offset, std::uint32_t value);

    std::uint32_t fixed_fifo_depth_;
    NativePipelineStats& stats_;
    sc_core::sc_time clock_period_;
    sc_core::sc_time start_time_ = sc_core::SC_ZERO_TIME;
    std::uint64_t next_job_id_ = 1;
    std::uint64_t active_job_id_ = 0;
    std::uint64_t cycles_ = 0;
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
    bool fifo_config_matches_hardware_ = true;
};

class InputDmaStage final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(InputDmaStage);

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_fifo_in<JobConfig> jobs{"jobs"};
    sc_core::sc_fifo_out<FramePacket> output{"output"};
    sc_core::sc_port<DmaTransportIf> dma{"dma"};

    InputDmaStage(sc_core::sc_module_name name, std::uint32_t fifo_depth,
                  NativePipelineStats& stats);

private:
    void run();
    bool read_frame(const JobConfig& job, std::uint32_t frame_index,
                    hevc::Yuv420Frame& frame);
    void push(FramePacket packet);

    std::uint32_t fifo_depth_;
    NativePipelineStats& stats_;
};

class PredictionStage final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(PredictionStage);

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_fifo_in<FramePacket> input{"input"};
    sc_core::sc_fifo_out<FramePacket> output{"output"};

    PredictionStage(sc_core::sc_module_name name, std::uint32_t fifo_depth,
                    NativePipelineStats& stats);

private:
    void run();
    void wait_active(std::uint64_t cycles);
    void push(FramePacket packet);

    std::uint32_t fifo_depth_;
    NativePipelineStats& stats_;
};

class TransformStage final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(TransformStage);

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_fifo_in<FramePacket> input{"input"};
    sc_core::sc_fifo_out<FramePacket> output{"output"};

    TransformStage(sc_core::sc_module_name name, std::uint32_t fifo_depth,
                   NativePipelineStats& stats);

private:
    void run();
    void wait_active(std::uint64_t cycles);
    void push(FramePacket packet);

    std::uint32_t fifo_depth_;
    NativePipelineStats& stats_;
};

class CabacStage final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(CabacStage);

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_fifo_in<FramePacket> input{"input"};
    sc_core::sc_fifo_out<BitstreamPacket> output{"output"};

    CabacStage(sc_core::sc_module_name name, std::uint32_t fifo_depth,
               NativePipelineStats& stats);

private:
    void run();
    void wait_active(std::uint64_t cycles);
    void push(BitstreamPacket packet);

    std::uint32_t fifo_depth_;
    NativePipelineStats& stats_;
};

class OutputDmaStage final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(OutputDmaStage);

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_fifo_in<BitstreamPacket> input{"input"};
    sc_core::sc_fifo_out<CompletionPacket> completions{"completions"};
    sc_core::sc_port<DmaTransportIf> dma{"dma"};

    OutputDmaStage(sc_core::sc_module_name name,
                   NativePipelineStats& stats);

private:
    void run();
    void complete(const CompletionPacket& completion);

    NativePipelineStats& stats_;
};

class VpuSystemCNative final : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(VpuSystemCNative);

    sc_core::sc_in<bool> clk{"clk"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_out<bool> irq{"irq"};

    VpuSystemCNative(sc_core::sc_module_name name,
                     std::uint32_t fifo_depth = 4,
                     sc_core::sc_time clock_period =
                         sc_core::sc_time(1, sc_core::SC_NS),
                     std::size_t dma_burst_bytes = 64);

    [[nodiscard]] tlm_utils::simple_target_socket<VpuController>& mmio_socket() {
        return controller_.mmio_socket;
    }
    [[nodiscard]] tlm_utils::simple_initiator_socket<TlmDmaBridge>& dma_socket() {
        return dma_bridge_.socket;
    }
    [[nodiscard]] const NativePipelineStats& stats() const { return stats_; }

private:
    void forward_irq();

    NativePipelineStats stats_{};
    // MMIO writes execute in the initiator process while completion updates run
    // in the controller process. Both legitimately update the level IRQ.
    sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS>
        internal_irq_{"internal_irq"};
    sc_core::sc_fifo<JobConfig> job_fifo_;
    sc_core::sc_fifo<FramePacket> input_fifo_;
    sc_core::sc_fifo<FramePacket> residual_fifo_;
    sc_core::sc_fifo<FramePacket> coefficient_fifo_;
    sc_core::sc_fifo<BitstreamPacket> output_fifo_;
    sc_core::sc_fifo<CompletionPacket> completion_fifo_;
    TlmDmaBridge dma_bridge_;
    VpuController controller_;
    InputDmaStage input_dma_;
    PredictionStage prediction_;
    TransformStage transform_;
    CabacStage cabac_;
    OutputDmaStage output_dma_;
};

} // namespace model::systemc_native
