// h264_arb.h — DMA arbitration.
//
// Spec: §5.4 Arbitration — h264_arb.
//   [X] Four client groups: CMB, SW, NAL, DF/reference.
//   [X] A client receives a grant for the WHOLE life of one legacy
//       transaction; another client must not preempt mid-transaction.
//   [X] A waiting or granted client must hold its request/address/direction/
//       size/burst/data valid until completion.
//   [X] Read data is returned to the correct owner; CMB and SW responses must
//       not be mixed.
//   [X] Fairness is bounded by transaction length; there is NO formal
//       round-robin maximum-latency guarantee.
//   [TBD] Priority order is implementation-specific and the PDF does not
//       publish it — configurable, not guessed (see h264_spec.h).
#ifndef H264_ARB_H
#define H264_ARB_H

#include <array>
#include <cstdint>
#include <deque>
#include <stdexcept>
#include <vector>

#include "dma_if.h"
#include "h264_spec.h"

namespace h264 {

class H264Arb {
public:
    H264Arb() {
        for (int i = 0; i < 4; ++i) priority_[i] = kDefaultPriority[i];
    }

    // Override the priority order once the RTL order is known from
    // h264_arb.vhd (the [TBD] in §5.4).
    void set_priority(const ClientId p[4]) {
        std::array<bool, 4> seen{};
        for (int i = 0; i < 4; ++i) {
            const int c = idx(p[i]);
            if (seen[c]) throw std::invalid_argument("arb: priority must be a permutation");
            seen[c] = true;
        }
        for (int i = 0; i < 4; ++i) priority_[i] = p[i];
    }

    // A client posts a request. A client may have several transactions queued
    // (a filter pass or a window fill issues many reads), but it can only be
    // granted one legacy transaction at a time and never be preempted
    // mid-transaction (§5.4).
    void request(const DmaRequest& r, CompletionSink* sink) {
        Slot& s = slot(r.client);
        if (!r.beats) throw std::invalid_argument("arb: empty transaction");
        (void)request_bytes(r);
        s.queue.push_back({r, sink});
        log_.record(r);
    }

    bool busy() const { return granted_ != ClientId::NONE || any_pending(); }

    // Pick the highest-priority client that has a queued transaction.
    ClientId pick() const {
        if (granted_ != ClientId::NONE) return granted_;
        for (int i = 0; i < 4; ++i) {
            ClientId c = priority_[i];
            if (!slot(c).queue.empty()) return c;
        }
        return ClientId::NONE;
    }

    // §5.4 invariant: acquire a grant covering the entire transaction. While
    // granted, no other client may be selected — this is enforced by keeping
    // granted_ set until complete() is called.
    bool acquire(ClientId c) {
        if (granted_ != ClientId::NONE && granted_ != c) return false;
        if (slot(c).queue.empty()) return false;
        granted_ = c;
        return true;
    }

    const DmaRequest& granted_request() const {
        if (granted_ == ClientId::NONE) {
            throw std::logic_error("arb: no granted transaction");
        }
        return slot(granted_).queue.front().request;
    }

    // Complete the granted transaction and hand the response to its owner.
    // §5.4: read data returns to the correct owner — the response carries the
    // client tag so a mismatched delivery is detectable.
    void complete(bool ok, const std::vector<uint8_t>& data = {}) {
        if (granted_ == ClientId::NONE) {
            throw std::logic_error("arb: complete() with no grant");
        }
        Slot& s = slot(granted_);
        DmaResponse rsp;
        rsp.ok    = ok;
        rsp.tag   = s.queue.front().request.tag;
        rsp.beats = s.queue.front().request.beats;
        if (ok) rsp.data = data;
        CompletionSink* sink = s.queue.front().sink;
        s.queue.pop_front();
        ClientId done_client = granted_;
        granted_ = ClientId::NONE;
        if (sink) sink->on_done(done_client, rsp);
    }

    bool     is_pending(ClientId c) const { return !slot(c).queue.empty(); }
    ClientId granted() const { return granted_; }

    const RequestLog& log() const { return log_; }
    RequestLog&       log() { return log_; }

private:
    struct Slot {
        struct Entry { DmaRequest request; CompletionSink* sink; };
        std::deque<Entry> queue;
    };

    static int idx(ClientId c) {
        const int i = static_cast<int>(c);
        if (i < 0 || i >= 4) throw std::out_of_range("arb: invalid client");
        return i;
    }
    Slot&       slot(ClientId c) { return slots_[idx(c)]; }
    const Slot& slot(ClientId c) const { return slots_[idx(c)]; }

    bool any_pending() const {
        for (const Slot& s : slots_) {
            if (!s.queue.empty()) return true;
        }
        return false;
    }

    std::array<Slot, 4> slots_;
    ClientId            priority_[4];
    ClientId            granted_ = ClientId::NONE;
    RequestLog          log_;
};

}  // namespace h264

#endif  // H264_ARB_H
