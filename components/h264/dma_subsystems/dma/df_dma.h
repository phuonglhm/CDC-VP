// Reference write DMA, HAS §6.3 / §11.4.
// Deblocking arithmetic belongs to the producer: supply a completed filtered
// tile, or install a local filter callback run vertical then horizontal.
// Bypass supplies reconstructed pixels directly. No synthetic zero payloads.
#ifndef H264_DF_DMA_H
#define H264_DF_DMA_H
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>
#include "h264_arb.h"
namespace h264 {
enum class FilterPass : int { Vertical = 0, Horizontal = 1 };
class DfDma : public CompletionSink {
public:
    using Filter = std::function<void(FilterPass, MacroblockPixels&)>;
    DfDma(MemoryIf&, H264Arb& arb) : arb_(arb) { frames_.emplace(0, Frame{}); }
    // DFCON.ENABLE, not AVC disable_deblocking_filter_idc.
    void latch_df_enable(bool enabled) {
        configured_enable_ = enabled;
        auto& f = current();
        if (!f.issued && !f.producer_done) f.enabled = enabled;
    }
    // Legacy spelling retained for callers; only accepts the DFCON enable bit.
    void latch_dis_idc(int enable) {
        if (enable != 0 && enable != 1) throw std::invalid_argument("df: expected DFCON.ENABLE");
        latch_df_enable(enable != 0);
    }
    int latched_dis_idc() const { return current().enabled ? 1 : 0; }
    bool bypass_mode() const { return !current().enabled; }
    void set_filter(Filter filter) { filter_ = std::move(filter); }
    void set_refm_base(uint64_t base) {
        require_idle();
        if (!base || base >= (1ull << 32)) throw std::invalid_argument("df: invalid reference base");
        refm_base_ = base;
    }
    void set_dims(const CodedDims& dims) {
        require_idle(); validate_dims(dims); dims_ = dims;
    }
    void set_ref_slot(int slot) {
        (void)refm_slot_base(refm_base_, dims_, slot);
        if (current().issued) throw std::logic_error("df: cannot change an active frame's slot");
        configured_slot_ = current().slot = slot;
    }
    uint64_t frame_id() const { return frame_; }
    void on_sofm() {
        ++frame_;
        Frame next;
        next.enabled = configured_enable_;
        next.slot = configured_slot_;
        frames_.emplace(frame_, next);
        staged_.reset();
        // Older pending writes and their retained done events remain keyed by frame.
    }
    void stage_macroblock(const MacroblockPixels& pixels, bool already_filtered = true) {
        if (staged_) throw std::logic_error("df: unconsumed staged macroblock");
        staged_ = Staged{pixels, already_filtered};
    }
    void schedule_macroblock(uint32_t x, uint32_t y, const MacroblockPixels& pixels,
                             bool already_filtered = true) {
        stage_macroblock(pixels, already_filtered);
        schedule_macroblock(x, y);
    }
    void schedule_macroblock(uint32_t x, uint32_t y) {
        if (!staged_) throw std::logic_error("df: reconstructed/filtered pixels required");
        if (!refm_base_) throw std::logic_error("df: reference region not configured");
        if (x >= dims_.Wc / 16 || y >= dims_.Hc / 16) throw std::out_of_range("df: MB coordinate");
        auto& f = current();
        if (f.producer_done) throw std::logic_error("df: write after producer frame completion");
        MacroblockPixels pixels = staged_->pixels;
        if (f.enabled) {
            if (!staged_->filtered && !filter_)
                throw std::logic_error("df: unfinished filtering without a filter producer");
            for (auto pass : {FilterPass::Vertical, FilterPass::Horizontal}) {
                if (!staged_->filtered) filter_(pass, pixels);
                passes_.push_back(pass);
            }
        }
        staged_.reset(); // snapshot after both passes, before any external write
        const uint64_t base = refm_slot_base(refm_base_, dims_, f.slot);
        for (uint32_t row = 0; row < 16; ++row)
            issue(planar_y_addr(base, dims_, x * 16, y * 16 + row), pixels.y.data() + row * 16, 16);
        for (uint32_t row = 0; row < 8; ++row) {
            issue(planar_u_addr(base, dims_, x * 8, y * 8 + row), pixels.u.data() + row * 8, 8);
            issue(planar_v_addr(base, dims_, x * 8, y * 8 + row), pixels.v.data() + row * 8, 8);
        }
    }
    void filter_macroblock(uint32_t x, uint32_t y) {
        if (bypass_mode()) throw std::logic_error("df: filter request in bypass mode");
        schedule_macroblock(x, y);
    }
    void bypass_macroblock(uint32_t x, uint32_t y) {
        if (!bypass_mode()) throw std::logic_error("df: bypass request in filter mode");
        schedule_macroblock(x, y);
    }
    void dma_fmdone() { dma_fmdone(frame_); }
    void dma_fmdone(uint64_t frame) {
        auto& f = frames_.at(frame);
        if (!f.producer_done) {
            f.producer_done = true;
            retained_.push_back(frame);
        }
    }
    bool done_available() const { return !retained_.empty(); }
    std::optional<uint64_t> retained_frame() const {
        return retained_.empty() ? std::optional<uint64_t>{} : retained_.front();
    }
    bool consume_done() {
        if (retained_.empty()) return false;
        frames_.at(retained_.front()).consumed = true;
        retained_.pop_front();
        return true;
    }
    void on_done(ClientId client, const DmaResponse& rsp) override {
        auto it = writes_.find(rsp.tag);
        if (client != ClientId::DF || it == writes_.end()) return;
        auto& f = frames_.at(it->second);
        if (!rsp.ok) f.failed = true;
        --f.pending;
        writes_.erase(it);
    }
    bool final_write_acked() const { return acknowledged(current()); }
    bool frame_complete() const { return frame_complete(frame_); }
    bool frame_complete(uint64_t frame) const {
        const auto& f = frames_.at(frame);
        return acknowledged(f) && f.consumed;
    }
    bool failed() const { return current().failed; }
    uint32_t pending_writes() const { return current().pending; }
    const std::vector<FilterPass>& pass_log() const { return passes_; }
    void reset_for_frame() { on_sofm(); } // preserve previous retained events
private:
    struct Frame {
        uint32_t pending = 0, issued = 0;
        bool failed = false, producer_done = false, consumed = false, enabled = false;
        int slot = 0;
    };
    struct Staged { MacroblockPixels pixels; bool filtered; };
    Frame& current() { return frames_.at(frame_); }
    const Frame& current() const { return frames_.at(frame_); }
    static bool acknowledged(const Frame& f) {
        return f.producer_done && f.issued != 0 && f.pending == 0 && !f.failed;
    }
    void require_idle() const {
        if (!writes_.empty() || staged_) throw std::logic_error("df: reconfigure while busy");
    }
    void issue(uint64_t addr, const uint8_t* pixels, uint32_t bytes) {
        DmaRequest r;
        r.client = ClientId::DF; r.addr = addr; r.beats = bytes / 4;
        r.is_write = true; r.tag = next_tag_++;
        r.data.assign(pixels, pixels + bytes);
        writes_.emplace(r.tag, frame_);
        ++current().pending; ++current().issued;
        arb_.request(r, this);
    }
    H264Arb& arb_;
    CodedDims dims_{176,144};
    uint64_t refm_base_ = 0, frame_ = 0;
    uint32_t next_tag_ = 0;
    bool configured_enable_ = false;
    int configured_slot_ = 0;
    Filter filter_;
    std::optional<Staged> staged_;
    std::map<uint64_t, Frame> frames_;
    std::map<uint32_t, uint64_t> writes_;
    std::deque<uint64_t> retained_;
    std::vector<FilterPass> passes_;
};
}
#endif
