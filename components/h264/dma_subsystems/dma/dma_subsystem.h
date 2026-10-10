// Functional integration of four ordered DMA clients (HAS §5–6, §12).
#ifndef H264_DMA_SUBSYSTEM_H
#define H264_DMA_SUBSYSTEM_H
#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include "axi_master_bridge.h"
#include "cmb_dma.h"
#include "ddr_model.h"
#include "df_dma.h"
#include "h264_arb.h"
#include "nal_dma.h"
#include "sw_dma.h"
namespace h264 {
// Values are host-assigned physical bases, not AXI-Lite register offsets.
struct RegisterBases {
    uint64_t REG_CMB = 0x00100000ull;
    uint64_t REG_REFM = 0x00400000ull;
    uint64_t REG_NAL = 0x01000000ull; // beyond 3 full-HD reference frames
    uint32_t cmb_frames = 1;
    uint64_t nal_capacity = 0; // infer up to the next region/end of modeled DDR
};
class DmaSubsystem {
public:
    DmaSubsystem(const CodedDims& dims, const RegisterBases& bases,
                 const AxiBridgeConfig& cfg = {}, size_t ddr_bytes = 64u * 1024u * 1024u)
        : DmaSubsystem(dims, bases, cfg, std::make_unique<DdrModel>(ddr_bytes),
                       nullptr, 0, ddr_bytes) {}
    DmaSubsystem(const CodedDims& dims, const RegisterBases& bases, MemoryIf& memory,
                 uint64_t memory_base, uint64_t memory_bytes, const AxiBridgeConfig& cfg = {})
        : DmaSubsystem(dims, bases, cfg, nullptr, &memory, memory_base, memory_bytes) {}
private:
    DmaSubsystem(const CodedDims& dims, const RegisterBases& bases, const AxiBridgeConfig& cfg,
                 std::unique_ptr<DdrModel> owned, MemoryIf* external,
                 uint64_t memory_base, uint64_t memory_bytes)
        : dims_(dims), bases_(bases), owned_ddr_(std::move(owned)),
          memory_(owned_ddr_ ? static_cast<MemoryIf*>(owned_ddr_.get()) : external),
          memory_base_(memory_base), memory_bytes_(memory_bytes), axi_(cfg),
          cmb_(*memory_, arb_), sw_(arb_, dims, {64,64}), df_(*memory_, arb_), nal_(*memory_, arb_) {
        validate_layout();
        cmb_.configure(dims_, bases_.REG_CMB, CmbFrameMode::CodingOrderPicture,
                       frame_bytes(dims_) * static_cast<uint64_t>(bases_.cmb_frames));
        cmb_.set_working_set_frames(bases_.cmb_frames);
        sw_.set_refm_base(bases_.REG_REFM);
        df_.set_refm_base(bases_.REG_REFM);
        df_.set_dims(dims_);
        nal_.configure(bases_.REG_NAL, bases_.nal_capacity);
    }
public:
    // At zero injected delay this services one full legacy request. When an
    // endpoint/consumer stalls, its grant remains owned across service calls.
    // NONE means idle only; a pending transaction returns its current owner.
    ClientId service_one() {
        ClientId c = arb_.granted();
        if (c == ClientId::NONE) {
            c = arb_.pick();
            if (c == ClientId::NONE) return c;
            if (!arb_.acquire(c)) throw std::logic_error("dma: failed to acquire selected client");
        }
        if (!axi_.active()) {
            try { axi_.begin(arb_.granted_request(), *memory_); }
            catch (const std::invalid_argument&) {
                error_ = true; arb_.complete(false); return c;
            }
        }
        const auto progress = axi_.advance();
        if (progress != AxiProgress::Pending) {
            const auto rsp = axi_.response();
            error_ = error_ || !rsp.ok;
            arb_.complete(rsp.ok, rsp.data);
        }
        return c;
    }
    size_t run(uint32_t max_steps = 1000000) {
        size_t n = 0;
        while (n < max_steps && arb_.busy()) { service_one(); ++n; }
        if (arb_.busy()) throw std::runtime_error("dma_subsystem: run() did not converge");
        return n;
    }
    bool busy() const { return arb_.busy(); }
    bool error() const { return error_; }
    void clear_error() {
        if (busy()) throw std::logic_error("dma: clear error while busy");
        error_ = false;
    }
    H264Arb& arb() { return arb_; }
    DdrModel& ddr() {
        if (!owned_ddr_) throw std::logic_error("dma: external memory has no private DDR");
        return *owned_ddr_;
    }
    MemoryIf& memory() { return *memory_; }
    AxiMasterBridge& axi() { return axi_; }
    CmbDma& cmb() { return cmb_; }
    SwDma& sw() { return sw_; }
    DfDma& df() { return df_; }
    NalDma& nal() { return nal_; }
    const CodedDims& dims() const { return dims_; }
    const RegisterBases& bases() const { return bases_; }
private:
    void validate_layout() {
        validate_dims(dims_);
        if (!bases_.cmb_frames || bases_.cmb_frames > 127)
            throw std::invalid_argument("dma: CMB working set must be 1..127 frames");
        if (memory_base_ >= (1ull << 32) || !memory_bytes_ ||
            memory_bytes_ > (1ull << 32) - memory_base_)
            throw std::invalid_argument("dma: invalid physical memory window");
        const uint64_t limit = memory_base_ + memory_bytes_;
        const uint64_t starts[] = {bases_.REG_CMB, bases_.REG_REFM, bases_.REG_NAL};
        for (auto start : starts)
            if (!start || start < memory_base_ || start >= limit || start % axi_.bus_bytes())
                throw std::invalid_argument("dma: invalid or unaligned DDR base");
        uint64_t nal_end = limit;
        for (auto start : starts)
            if (start > bases_.REG_NAL) nal_end = std::min(nal_end, start);
        if (!bases_.nal_capacity) bases_.nal_capacity = nal_end - bases_.REG_NAL;
        const uint64_t sizes[] = {frame_bytes(dims_) * static_cast<uint64_t>(bases_.cmb_frames),
                                  frame_bytes(dims_) * 3ull, bases_.nal_capacity};
        for (size_t i = 0; i < 3; ++i) {
            if (!sizes[i] || sizes[i] > limit - starts[i])
                throw std::invalid_argument("dma: DDR region exceeds capacity");
            for (size_t j = 0; j < i; ++j)
                if (starts[i] < starts[j] + sizes[j] && starts[j] < starts[i] + sizes[i])
                    throw std::invalid_argument("dma: overlapping DDR regions");
        }
    }
    CodedDims dims_;
    RegisterBases bases_;
    std::unique_ptr<DdrModel> owned_ddr_;
    MemoryIf* memory_;
    uint64_t memory_base_, memory_bytes_;
    H264Arb arb_;
    AxiMasterBridge axi_;
    CmbDma cmb_;
    SwDma sw_;
    DfDma df_;
    NalDma nal_;
    bool error_ = false;
};
}
#endif
