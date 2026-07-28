// SPDX-License-Identifier: SHL-0.51
//
// Request metadata retention and downstream AXI ID management.
//
// Source of truth: `hw/floo_meta_buffer.sv` at the frozen revision.
//
// Only the `MaxUniqueIds == 1` branch is modeled, which is what
// `floo_pkg::ChimneyDefaultCfg` selects. In that branch the RTL:
//
//   * issues every non-atomic downstream transaction under the constant ID
//     `'1`, i.e. all ones for `OutIdWidth` bits, not zero;
//   * retains the per-request metadata in a plain in-order `fifo_v3` of depth
//     `MaxTxns`, one for writes and one for reads.
//
// The `MaxUniqueIds > 1` branch is different in both respects: it keys an
// `id_queue` by the original AXI ID and issues downstream IDs in the range
// `[MaxAtomicTxns, 2**OutIdWidth)`. It is out of v0 scope and deliberately not
// modeled here rather than guessed at.
//
// Atomic transactions take a separate path with their own IDs; `AtopSupport`
// is enabled in the frozen configuration but ATOPs are outside v0 traffic.

#pragma once

#include "floo_noc_model/axi_chimney_pack.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>

namespace floo::model {

/// Mirrors the `MaxUniqueIds == 1` branch of `floo_meta_buffer.sv`.
class meta_buffer {
public:
    meta_buffer(unsigned out_id_width, std::size_t max_txns)
        : out_id_width_(out_id_width)
        , max_txns_(max_txns)
    {
        if (out_id_width == 0 || out_id_width >= 64) {
            throw std::invalid_argument("meta_buffer: bad OutIdWidth");
        }
        if (max_txns == 0) {
            throw std::invalid_argument("meta_buffer: MaxTxns must be nonzero");
        }
    }

    /// The downstream ID every non-atomic transaction is reissued under.
    /// `assign no_atop_aw_req_id = '1;`
    std::uint64_t downstream_id() const
    {
        return (1ull << out_id_width_) - 1ull;
    }

    bool write_full() const { return writes_.size() >= max_txns_; }
    bool read_full() const { return reads_.size() >= max_txns_; }

    /// Retain the metadata of an accepted write request.
    void push_write(const response_meta& meta)
    {
        if (write_full()) {
            throw std::runtime_error("meta_buffer: write buffer overflow");
        }
        writes_.push_back(meta);
    }

    /// Retain the metadata of an accepted read request.
    void push_read(const response_meta& meta)
    {
        if (read_full()) {
            throw std::runtime_error("meta_buffer: read buffer overflow");
        }
        reads_.push_back(meta);
    }

    /// Pop the metadata for the next B response. In-order, because the RTL
    /// branch being modeled uses a plain FIFO rather than an ID-keyed queue.
    response_meta pop_write()
    {
        if (writes_.empty()) {
            throw std::runtime_error("meta_buffer: B response with no request");
        }
        const auto meta = writes_.front();
        writes_.pop_front();
        return meta;
    }

    /// Pop the metadata for the next R response.
    response_meta pop_read()
    {
        if (reads_.empty()) {
            throw std::runtime_error("meta_buffer: R response with no request");
        }
        const auto meta = reads_.front();
        reads_.pop_front();
        return meta;
    }

    std::size_t outstanding_writes() const { return writes_.size(); }
    std::size_t outstanding_reads() const { return reads_.size(); }

private:
    unsigned out_id_width_;
    std::size_t max_txns_;
    std::deque<response_meta> writes_;
    std::deque<response_meta> reads_;
};

} // namespace floo::model
