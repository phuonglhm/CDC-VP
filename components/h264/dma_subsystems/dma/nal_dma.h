// NAL ordered output DMA, HAS §6.4. Pointer/count are activation-relative.
#ifndef H264_NAL_DMA_H
#define H264_NAL_DMA_H
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
#include "h264_arb.h"
namespace h264 {
enum class WordByteOrder { LittleEndian, BigEndian }; // RTL endian is HAS §5.5 TBD
class NalDma : public CompletionSink {
public:
    NalDma(MemoryIf&, H264Arb& arb) : arb_(arb) {}
    void configure(uint64_t base, uint64_t capacity = 0,
                   WordByteOrder order = WordByteOrder::LittleEndian) {
        require_idle();
        if (enabled_ && activation_started_)
            throw std::logic_error("nal: disable before reprogramming activation");
        if (!base || base >= (1ull << 32) || base % 4 ||
            capacity > (1ull << 32) - base)
            throw std::invalid_argument("nal: invalid output region");
        base_ = base; capacity_ = capacity ? capacity : (1ull << 32) - base;
        order_ = order;
        begin_activation();
    }
    void on_disable() {
        require_idle(); // pointer cannot be reloaded while a write owns the region
        enabled_ = false;
        write_ptr_ = reserved_ptr_ = base_;
        // STM_LEN remains readable for the host's disable->read->drain sequence.
    }
    void accept_word(uint32_t word) {
        if (!enabled_ || failed_) throw std::logic_error("nal: disabled or failed activation");
        if (reserved_ptr_ - base_ + (pending_.size() + 1ull) * 4 > capacity_)
            throw std::out_of_range("nal: activation exceeds output capacity");
        if (word_count_ + static_cast<uint64_t>(pending_.size()) + queued_words_ >=
            std::numeric_limits<uint32_t>::max())
            throw std::overflow_error("nal: word count overflow");
        pending_.push_back(word);
        activation_started_ = true;
        final_b_accepted_ = false;
    }
    static constexpr uint32_t kEosWord = 0x0B010000u; // little-endian bytes 00 00 01 0B
    void accept_eos() {
        accept_word(order_ == WordByteOrder::LittleEndian ? kEosWord : 0x0000010Bu);
    }
    void flush_chunk() {
        if (pending_.empty()) return;
        DmaRequest r;
        r.client = ClientId::NAL; r.addr = reserved_ptr_;
        r.beats = static_cast<uint32_t>(pending_.size());
        r.is_write = true; r.tag = next_tag_++;
        for (uint32_t word : pending_)
            for (uint32_t b = 0; b < 4; ++b) {
                const uint32_t shift = (order_ == WordByteOrder::LittleEndian ? b : 3 - b) * 8;
                r.data.push_back(static_cast<uint8_t>(word >> shift));
            }
        inflight_.emplace(r.tag, r); // each queued request owns its immutable snapshot
        arb_.request(r, this);
        reserved_ptr_ += r.data.size();
        queued_words_ += r.beats;
        pending_.clear();
    }
    std::vector<uint8_t> take_staged_bytes() const {
        return inflight_.empty() ? std::vector<uint8_t>{} : inflight_.begin()->second.data;
    }
    // Compatibility with old harnesses. Memory is now written only by the bridge;
    // successful on_done commits accounting exactly once.
    void commit_payload() {}
    void on_done(ClientId client, const DmaResponse& rsp) override {
        auto it = inflight_.find(rsp.tag);
        if (client != ClientId::NAL || it == inflight_.end()) return;
        const uint32_t words = it->second.beats;
        if (!rsp.ok || rsp.beats != words) failed_ = true;
        if (!failed_) { word_count_ += words; write_ptr_ = base_ + word_count_ * 4ull; }
        queued_words_ -= words;
        inflight_.erase(it);
        final_b_accepted_ = !failed_ && inflight_.empty() && pending_.empty();
    }
    uint32_t stm_len() const { return word_count_; }
    uint64_t byte_count() const { return word_count_ * 4ull; }
    bool final_b_accepted() const { return final_b_accepted_; }
    bool b_failed() const { return failed_; }
    uint64_t write_ptr() const { return write_ptr_; }
    uint32_t words_staged() const { return static_cast<uint32_t>(pending_.size()); }
    void begin_activation() {
        require_idle();
        if (enabled_ && activation_started_)
            throw std::logic_error("nal: disable phase required before reactivation");
        write_ptr_ = reserved_ptr_ = base_;
        word_count_ = queued_words_ = 0;
        final_b_accepted_ = failed_ = false;
        enabled_ = true;
        activation_started_ = false;
    }
    bool busy() const { return !inflight_.empty(); }
private:
    void require_idle() const {
        if (!inflight_.empty() || !pending_.empty())
            throw std::logic_error("nal: activation still owns pending output");
    }
    H264Arb& arb_;
    uint64_t base_ = 0, capacity_ = 0, write_ptr_ = 0, reserved_ptr_ = 0;
    uint32_t word_count_ = 0, queued_words_ = 0, next_tag_ = 0;
    bool enabled_ = false, activation_started_ = false;
    bool failed_ = false, final_b_accepted_ = false;
    WordByteOrder order_ = WordByteOrder::LittleEndian;
    std::vector<uint32_t> pending_;
    std::map<uint32_t, DmaRequest> inflight_;
};
}
#endif
