#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "dma_subsystem.h"

namespace cdc::components {
// Codec/workload input is a simulation API, never a fabricated encoder.
// NAL words and filtered samples must come from a codec producer or a test trace.
struct H264ReferenceSelection {
    h264::RefList list = h264::RefList::List0;
    int slot = 0;
    uint64_t picture_tag = 0;
};
struct H264MacroblockJob {
    uint32_t x = 0, y = 0;
    std::vector<H264ReferenceSelection> references;
    std::optional<h264::MacroblockPixels> reconstructed;
    bool already_filtered = true;
    // Explicit DMA-only trace replay: copy CMB pixels to REFM with DF disabled.
    // A codec producer normally supplies reconstructed samples instead.
    bool replay_source_for_bypass = false;
};
struct H264FrameWorkload {
    uint32_t source_slot = 0;
    int reference_slot = 0;
    bool is_b_picture = false;
    std::vector<H264MacroblockJob> macroblocks;
    std::vector<uint32_t> nal_words;
};
struct H264ActivationWorkload {
    std::vector<H264FrameWorkload> frames;
    bool eos = false;
};
struct H264ActivationConfig {
    h264::CodedDims dims;
    h264::RegisterBases bases;
    uint32_t dfcon = 0, spara0 = 0, spara1 = 0, spara2 = 0;
};
enum class H264UndefinedAccess { AddressError, ReadZero };
struct H264DmaOptions {
    uint64_t memory_base = 0x80000000ull;
    uint64_t memory_bytes = 64ull << 20;
    uint64_t nal_capacity = 0; // allocator budget; zero infers available region
    h264::AxiBridgeConfig bridge;
    h264::WordByteOrder nal_word_order = h264::WordByteOrder::LittleEndian;
    sc_core::sc_time register_latency{10, sc_core::SC_NS};
    sc_core::sc_time service_latency{8, sc_core::SC_NS};
    uint32_t max_service_steps = 1000000;
    H264UndefinedAccess undefined_access = H264UndefinedAccess::AddressError;
};
struct H264DmaTransfer {
    uint64_t address;
    size_t bytes;
    bool is_write;
    h264::ClientId client;
    sc_core::sc_time issued, completed;
    tlm::tlm_response_status response;
    bool cancelled;
};

class h264_dma_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<h264_dma_tlm> target_socket;
    tlm_utils::simple_initiator_socket<h264_dma_tlm> master_socket;
    sc_core::sc_in<bool> reset_n;
    sc_core::sc_out<bool> irq;

    enum Register : uint32_t {
        SCON=0x00, STAT=0x04, FMSIZE=0x08, DFCON=0x0C,
        SPARA0=0x10, SPARA1=0x14, SPARA2=0x18,
        REFM=0x1C, NAL=0x20, CMB=0x24, STM_LEN=0x28
    };
    static constexpr uint32_t NORMAL = 1u << 18, ERROR = 1u << 17, BUSY = 1u << 16;
    SC_HAS_PROCESS(h264_dma_tlm);
    explicit h264_dma_tlm(sc_core::sc_module_name, H264DmaOptions options = {});
    ~h264_dma_tlm() override;

    // Install before activation. A provider runs in the worker SC_THREAD and
    // sees the latched register configuration. Queued traces take precedence.
    using WorkloadProvider = std::function<H264ActivationWorkload(const H264ActivationConfig&)>;
    void set_workload_provider(WorkloadProvider);
    void set_filter(h264::DfDma::Filter);
    void enqueue_workload(H264ActivationWorkload);
    const std::vector<H264DmaTransfer>& transfers() const { return transfers_; }
    const std::string& last_error() const { return last_error_; }

private:
    struct Cancelled {};
    class TlmMemory : public h264::MemoryIf {
    public:
        TlmMemory(h264_dma_tlm& owner, uint64_t epoch) : owner_(owner), epoch_(epoch) {}
        void write(uint64_t, const uint8_t*, size_t) override;
        void read(uint64_t, uint8_t*, size_t) const override;
        uint8_t read_byte(uint64_t) const override;
    private:
        h264_dma_tlm& owner_;
        uint64_t epoch_;
    };
    struct Launch { uint64_t epoch; std::array<uint32_t,11> registers; };
    void b_transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    unsigned transport_dbg(tlm::tlm_generic_payload&);
    void worker();
    void reset_changed();
    void drive_irq();
    void check_epoch(uint64_t) const;
    void check_running(uint64_t) const;
    void transport_memory(tlm::tlm_command, uint64_t, uint8_t*, size_t, uint64_t);
    H264ActivationConfig configuration(const Launch&) const;
    void execute(const H264ActivationConfig&, const H264ActivationWorkload&, uint64_t);
    void drain(uint64_t, uint32_t&);
    void finish(bool error, uint64_t);
    void reset_registers();
    uint32_t read_register(uint32_t) const;
    H264DmaOptions options_;
    std::array<uint32_t,11> registers_{};
    uint32_t stm_len_ = 0, frames_done_ = 0;
    bool busy_ = false, normal_ = false, error_ = false, pending_irq_ = false, abort_ = false;
    uint64_t epoch_ = 0;
    std::optional<Launch> launch_;
    sc_core::sc_event start_event_, irq_event_;
    WorkloadProvider provider_;
    h264::DfDma::Filter filter_;
    std::deque<H264ActivationWorkload> workloads_;
    std::unique_ptr<TlmMemory> memory_;
    std::unique_ptr<h264::DmaSubsystem> core_;
    std::vector<H264DmaTransfer> transfers_;
    std::string last_error_;
};
}
