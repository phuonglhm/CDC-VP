// Functional ordered DMA -> AXI adapter (HAS §5).
// advance() progresses to completion or an injected handshake/consumer stall.
// One call is not a hardware clock. RTL attributes/endian remain separate TBDs.
#ifndef H264_AXI_MASTER_BRIDGE_H
#define H264_AXI_MASTER_BRIDGE_H
#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <stdexcept>
#include <vector>
#include "dma_if.h"

namespace h264 {
struct AxiBridgeConfig {
    uint32_t data_width_bits = 32;
    uint32_t burst_limit = 16; // configurable model policy, not an RTL claim
    bool delay_bvalid = false; // one pending service step per B response
    bool drop_bresp = false;
    uint32_t awready_delay = 0, wready_delay = 0, arready_delay = 0;
    uint32_t rvalid_delay = 0, r_beat_gap = 0, bvalid_delay = 0;
    uint32_t consumer_delay = 0;
    bool drop_rresp = false, bad_rlast = false;
    uint32_t fail_b_segment = std::numeric_limits<uint32_t>::max();
};
struct AxiSegment {
    uint64_t addr;
    uint32_t len; // EXTERNAL AXI beats minus one
    uint32_t size_bytes; // EXTERNAL beat width (ARSIZE/AWSIZE)
    bool is_write;
    ClientId client;
};
struct AxiWriteBeat {
    uint64_t addr;
    std::vector<uint8_t> data;
    uint16_t strobe;
    bool last;
    ClientId client;
};
enum class AxiProgress { Pending, Success, Failure };

class AxiMasterBridge {
public:
    explicit AxiMasterBridge(const AxiBridgeConfig& cfg = {}) : cfg_(cfg) {
        if (cfg.data_width_bits != 32 && cfg.data_width_bits != 64 &&
            cfg.data_width_bits != 128)
            throw std::invalid_argument("axi: data width must be 32/64/128");
        if (!cfg.burst_limit || cfg.burst_limit > 256)
            throw std::invalid_argument("axi: burst limit must be 1..256");
        bus_bytes_ = cfg.data_width_bits / 8;
    }
    uint32_t bus_bytes() const { return bus_bytes_; }
    uint32_t beats_until_next_4kib(uint64_t addr) const {
        const uint64_t aligned = addr - addr % bus_bytes_;
        return static_cast<uint32_t>((4096 - aligned % 4096) / bus_bytes_);
    }
    uint32_t segment_beats(uint64_t addr, uint32_t remaining) const {
        return std::min({remaining, cfg_.burst_limit, beats_until_next_4kib(addr)});
    }
    std::vector<AxiSegment> plan_segments(const DmaRequest& r) const {
        const uint64_t bytes = request_bytes(r);
        const uint64_t limit = 1ull << 32;
        if (!bytes || r.addr >= limit || bytes > limit - r.addr)
            throw std::invalid_argument("axi: empty or out-of-range transaction");
        uint64_t addr = r.addr - r.addr % bus_bytes_;
        uint32_t remaining = static_cast<uint32_t>(
            (r.addr % bus_bytes_ + bytes + bus_bytes_ - 1) / bus_bytes_);
        std::vector<AxiSegment> result;
        while (remaining) {
            const uint32_t n = segment_beats(addr, remaining);
            result.push_back({addr, n - 1, bus_bytes_, r.is_write, r.client});
            addr += static_cast<uint64_t>(n) * bus_bytes_;
            remaining -= n;
        }
        return result; // planning queries have no effect on issued logs
    }
    bool all_segments_in_page(const DmaRequest& r) const {
        for (const auto& s : plan_segments(r))
            if (s.addr / 4096 != (s.addr + (s.len + 1ull) * bus_bytes_ - 1) / 4096)
                return false;
        return true;
    }
    void begin(const DmaRequest& r, MemoryIf& mem) {
        if (active_) throw std::logic_error("axi: another transaction is active");
        segments_ = plan_segments(r);
        if (r.is_write && r.data.size() != request_bytes(r))
            throw std::invalid_argument("axi: payload length differs from transfer size");
        request_ = r;
        memory_ = &mem;
        segment_ = beat_ = 0;
        response_ = {true, r.tag, r.beats, {}};
        fifo_.clear();
        final_b_accepted_ = false;
        consumer_wait_ = cfg_.consumer_delay;
        active_ = true;
        enter_segment();
    }
    bool active() const { return active_; }
    const DmaResponse& response() const { return response_; }

    AxiProgress advance() {
        if (!active_) throw std::logic_error("axi: advance without transaction");
        try {
            for (;;) {
                const auto& s = segments_[segment_];
                if (wait_) {
                    --wait_;
                    if (state_ == State::B) ++b_delay_cycles_;
                    return AxiProgress::Pending;
                }
                if (state_ == State::Address) {
                    log_.push_back(s); // accepted AW or AR, exactly once
                    state_ = request_.is_write ? State::W : State::R;
                    wait_ = request_.is_write ? cfg_.wready_delay : cfg_.rvalid_delay;
                    continue;
                }
                const uint64_t addr = s.addr + static_cast<uint64_t>(beat_) * bus_bytes_;
                if (state_ == State::W) {
                    const auto bounds = valid_range(addr);
                    AxiWriteBeat w{addr, std::vector<uint8_t>(bus_bytes_, 0), 0,
                                   beat_ == s.len, request_.client};
                    for (uint64_t p = bounds.first; p < bounds.second; ++p) {
                        const size_t lane = static_cast<size_t>(p - addr);
                        w.data[lane] = request_.data[static_cast<size_t>(p - request_.addr)];
                        w.strobe |= static_cast<uint16_t>(1u << lane);
                    }
                    writes_.push_back(std::move(w)); // accepted W, stable during wait
                    if (++beat_ > s.len) {
                        state_ = State::B;
                        wait_ = cfg_.bvalid_delay + (cfg_.delay_bvalid ? 1u : 0u);
                    } else wait_ = cfg_.wready_delay;
                    continue;
                }
                if (state_ == State::B) {
                    ++b_responses_;
                    if (cfg_.drop_bresp || segment_ == cfg_.fail_b_segment) return fail();
                    // Apply only selected bytes after this segment's OKAY B.
                    // This VP stages segment writes; it does not promise RTL rollback.
                    const uint64_t first = std::max(s.addr, request_.addr);
                    const uint64_t last = std::min<uint64_t>(s.addr + (s.len + 1ull) * bus_bytes_,
                                                  request_.addr + request_bytes(request_));
                    memory_->write(first, request_.data.data() + (first - request_.addr),
                                   static_cast<size_t>(last - first));
                    if (++segment_ == segments_.size()) {
                        final_b_accepted_ = true;
                        active_ = false;
                        return AxiProgress::Success;
                    }
                    enter_segment();
                    continue;
                }
                if (state_ == State::R) {
                    const auto bounds = valid_range(addr);
                    const size_t n = static_cast<size_t>(bounds.second - bounds.first);
                    const size_t words = (n + 3) / 4;
                    if (fifo_.size() + words > kReadFifoDepth) {
                        if (!drain()) return AxiProgress::Pending;
                    }
                    if (cfg_.drop_rresp || (cfg_.bad_rlast && beat_ == s.len))
                        return fail();
                    std::vector<uint8_t> bytes(n);
                    memory_->read(bounds.first, bytes.data(), bytes.size());
                    for (size_t i = 0; i < n; i += 4) {
                        fifo_.emplace_back(bytes.begin() + i,
                                           bytes.begin() + std::min(n, i + 4));
                    }
                    max_fifo_words_ = std::max(max_fifo_words_, fifo_.size());
                    if (++beat_ > s.len) state_ = State::Drain;
                    else wait_ = cfg_.r_beat_gap;
                    continue;
                }
                if (!drain()) return AxiProgress::Pending;
                if (++segment_ == segments_.size()) {
                    active_ = false;
                    return AxiProgress::Success;
                }
                enter_segment();
            }
        } catch (const std::out_of_range&) {
            return fail(); // modeled DDR/address error becomes a failed response
        }
    }
    std::vector<uint8_t> read(const DmaRequest& r, const MemoryIf& mem) {
        if (r.is_write) throw std::invalid_argument("axi: read with write request");
        // read uses no mutation of the memory; begin stores the common MemoryIf.
        begin(r, const_cast<MemoryIf&>(mem));
        AxiProgress p;
        do { p = advance(); } while (p == AxiProgress::Pending);
        if (p == AxiProgress::Failure) throw std::runtime_error("axi: failed read response");
        return response_.data;
    }
    bool write_payload(const DmaRequest& r, MemoryIf& mem,
                       const std::vector<uint8_t>& payload) {
        if (!r.is_write) throw std::invalid_argument("axi: write with read request");
        auto captured = r;
        captured.data = payload;
        begin(captured, mem);
        AxiProgress p;
        do { p = advance(); } while (p == AxiProgress::Pending);
        return p == AxiProgress::Success;
    }
    const std::vector<AxiSegment>& issued_log() const { return log_; }
    const std::vector<AxiWriteBeat>& write_log() const { return writes_; }
    void clear_log() { log_.clear(); writes_.clear(); }
    uint32_t b_delay_cycles() const { return b_delay_cycles_; }
    uint32_t b_responses() const { return b_responses_; }
    bool final_b_accepted() const { return final_b_accepted_; }
    size_t max_fifo_words() const { return max_fifo_words_; }
private:
    enum class State { Address, W, B, R, Drain };
    void enter_segment() {
        beat_ = 0;
        state_ = State::Address;
        wait_ = request_.is_write ? cfg_.awready_delay : cfg_.arready_delay;
    }
    std::pair<uint64_t, uint64_t> valid_range(uint64_t beat_addr) const {
        return {std::max(beat_addr, request_.addr),
                std::min(beat_addr + bus_bytes_, request_.addr + request_bytes(request_))};
    }
    bool drain() {
        if (consumer_wait_) { --consumer_wait_; return false; }
        while (!fifo_.empty()) {
            response_.data.insert(response_.data.end(), fifo_.front().begin(), fifo_.front().end());
            fifo_.pop_front();
        }
        return true;
    }
    AxiProgress fail() {
        response_.ok = false;
        response_.data.clear();
        fifo_.clear();
        active_ = final_b_accepted_ = false;
        return AxiProgress::Failure;
    }
    AxiBridgeConfig cfg_;
    uint32_t bus_bytes_ = 4, beat_ = 0, wait_ = 0, consumer_wait_ = 0;
    uint32_t b_delay_cycles_ = 0, b_responses_ = 0;
    size_t segment_ = 0, max_fifo_words_ = 0;
    State state_ = State::Address;
    bool active_ = false, final_b_accepted_ = false;
    MemoryIf* memory_ = nullptr;
    DmaRequest request_;
    DmaResponse response_;
    std::vector<AxiSegment> segments_, log_;
    std::vector<AxiWriteBeat> writes_;
    std::deque<std::vector<uint8_t>> fifo_; // at most 16 internal 32-bit words
};
}
#endif
