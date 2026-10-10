// CMB planar tile fetch, HAS §6.1. One active tile, response-backed pixel banks.
#ifndef H264_CMB_DMA_H
#define H264_CMB_DMA_H
#include <algorithm>
#include <cstdint>
#include <map>
#include <stdexcept>
#include "h264_arb.h"
namespace h264 {
enum class CmbFrameMode : int { WorkingSet = 0, CodingOrderPicture = 1 };
class CmbDma : public CompletionSink {
public:
    CmbDma(MemoryIf&, H264Arb& arb) : arb_(arb) {}
    void configure(const CodedDims& dims, uint64_t base, CmbFrameMode mode,
                   uint64_t capacity = 0) {
        if (armed_ || !rows_.empty()) throw std::logic_error("cmb: reconfigure while enabled/busy");
        validate_dims(dims);
        if (!base || base >= (1ull << 32)) throw std::invalid_argument("cmb: invalid base");
        const uint64_t limit = capacity ? capacity :
            (base == base_ && source_capacity_ ? source_capacity_ : (1ull << 32) - base);
        if (limit > (1ull << 32) - base ||
            static_cast<uint64_t>(frame_bytes(dims)) * working_set_frames_ > limit)
            throw std::out_of_range("cmb: source working set exceeds allocation");
        dims_ = dims; base_ = base; mode_ = mode; done_ = failed_ = false;
        source_capacity_ = limit;
        frame_slot_ = 0;
    }
    void set_working_set_frames(uint32_t n) {
        if (armed_ || !rows_.empty()) throw std::logic_error("cmb: working-set change while enabled");
        if (!n || n > 127) throw std::invalid_argument("cmb: working set must be 1..127");
        if (static_cast<uint64_t>(n) * frame_bytes(dims_) > source_capacity_)
            throw std::out_of_range("cmb: working set exceeds source capacity");
        working_set_frames_ = n;
        frame_slot_ = 0;
    }
    // The controller supplies the storage slot in coding order; HAS does not
    // publish a complete GOP->preload mapping, so this client does not guess it.
    void select_working_frame(uint32_t slot) {
        if (armed_ || !rows_.empty()) throw std::logic_error("cmb: frame selection while enabled");
        if (slot >= working_set_frames_) throw std::out_of_range("cmb: frame slot");
        frame_slot_ = slot;
    }
    void update_enable(bool enable) {
        if (!enable) { armed_ = false; frame_start_accepted_ = false; return; }
        if (!armed_) { armed_ = true; frame_start_accepted_ = true; done_ = false; }
    }
    bool has_frame_start() const { return armed_ && frame_start_accepted_; }
    bool consume_frame_start() {
        if (!has_frame_start()) return false;
        frame_start_accepted_ = false;
        return true;
    }
    bool frame_start() { return consume_frame_start(); }
    bool armed() const { return armed_; }
    void fetch_macroblock(uint32_t x, uint32_t y) {
        if (!armed_) throw std::logic_error("cmb: fetch without frame start");
        if (!rows_.empty()) throw std::logic_error("cmb: previous tile is still pending");
        if (x >= dims_.Wc / 16 || y >= dims_.Hc / 16) throw std::out_of_range("cmb: MB coordinate");
        done_ = failed_ = false;
        pixels_ = {};
        const uint64_t base = source_base();
        for (uint32_t row = 0; row < 16; ++row)
            issue(planar_y_addr(base, dims_, x * 16, y * 16 + row), 16, 0, row * 16);
        for (uint32_t row = 0; row < 8; ++row) {
            issue(planar_u_addr(base, dims_, x * 8, y * 8 + row), 8, 1, row * 8);
            issue(planar_v_addr(base, dims_, x * 8, y * 8 + row), 8, 2, row * 8);
        }
    }
    uint64_t luma_row_addr(uint32_t x, uint32_t y, uint32_t row) const {
        return planar_y_addr(source_base(), dims_, x * 16, y * 16 + row);
    }
    void on_done(ClientId client, const DmaResponse& rsp) override {
        auto it = rows_.find(rsp.tag);
        if (client != ClientId::CMB || it == rows_.end()) {
            if (!rsp.ok) { failed_ = true; done_ = false; }
            return;
        }
        const Row row = it->second;
        if (!rsp.ok || rsp.data.size() != row.bytes) failed_ = true;
        else {
            uint8_t* dst = row.plane == 0 ? pixels_.y.data() :
                           row.plane == 1 ? pixels_.u.data() : pixels_.v.data();
            std::copy(rsp.data.begin(), rsp.data.end(), dst + row.offset);
        }
        rows_.erase(it);
        done_ = rows_.empty() && !failed_;
    }
    bool done() const { return done_; }
    bool failed() const { return failed_; }
    const MacroblockPixels& pixels() const {
        if (!done_) throw std::logic_error("cmb: pixels not resident");
        return pixels_;
    }
private:
    struct Row { uint32_t plane, offset, bytes; };
    uint64_t source_base() const {
        return base_ + (mode_ == CmbFrameMode::WorkingSet ?
                        static_cast<uint64_t>(frame_slot_) * frame_bytes(dims_) : 0);
    }
    void issue(uint64_t addr, uint32_t bytes, uint32_t plane, uint32_t offset) {
        DmaRequest r;
        r.client = ClientId::CMB; r.addr = addr; r.beats = bytes / 4;
        r.tag = next_tag_++;
        rows_.emplace(r.tag, Row{plane, offset, bytes});
        arb_.request(r, this);
    }
    H264Arb& arb_;
    CodedDims dims_{176,144};
    uint64_t base_ = 0, source_capacity_ = 0;
    CmbFrameMode mode_ = CmbFrameMode::WorkingSet;
    uint32_t working_set_frames_ = 1, frame_slot_ = 0, next_tag_ = 0;
    bool armed_ = false, frame_start_accepted_ = false, done_ = false, failed_ = false;
    MacroblockPixels pixels_;
    std::map<uint32_t, Row> rows_;
};
}
#endif
