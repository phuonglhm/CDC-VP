// SPDX-License-Identifier: Apache-2.0
//
// Passive transaction-level metric primitives for the FlooNoC dashboard.
//
// These types do not advance SystemC time and do not touch the datapath. They
// are suitable for use from noc_interconnect's synchronous completion observer:
// add() is noexcept and converts integer overflow or allocation failure into an
// explicit invalid flag for the final report to reject.

#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>

namespace floo::model {

enum class metric_source {
    measured,
    derived,
    analytic,
    static_spec,
};

inline constexpr const char* metric_source_tag(metric_source source)
{
    switch (source) {
    case metric_source::measured: return "M";
    case metric_source::derived: return "D";
    case metric_source::analytic: return "A";
    case metric_source::static_spec: return "S";
    }
    return "?";
}

/// Exact histogram over integer network-cycle latency.
class latency_histogram {
public:
    void add(std::uint64_t cycles) noexcept
    {
        if (invalid_) {
            return;
        }
        constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
        if (count_ == limit || sum_ > limit - cycles) {
            invalid_ = true;
            return;
        }

        try {
            auto& bin = bins_[cycles];
            if (bin == limit) {
                invalid_ = true;
                return;
            }
            ++bin;
        } catch (...) {
            invalid_ = true;
            return;
        }

        ++count_;
        sum_ += cycles;
    }

    void reset()
    {
        bins_.clear();
        count_ = 0;
        sum_ = 0;
        invalid_ = false;
    }

    bool valid() const noexcept { return !invalid_; }
    bool empty() const noexcept { return count_ == 0; }
    std::uint64_t count() const noexcept { return count_; }
    std::uint64_t sum() const noexcept { return sum_; }
    const std::map<std::uint64_t, std::uint64_t>& bins() const noexcept
    {
        return bins_;
    }

    std::uint64_t min() const
    {
        require_data();
        return bins_.begin()->first;
    }

    std::uint64_t max() const
    {
        require_data();
        return bins_.rbegin()->first;
    }

    double mean() const
    {
        require_data();
        return static_cast<double>(sum_) / static_cast<double>(count_);
    }

    /// Exact nearest-rank percentile. `numerator/denominator` must be in
    /// `(0,1]`; rank is ceil(count * ratio), one based.
    std::uint64_t percentile(
        std::uint64_t numerator, std::uint64_t denominator) const
    {
        require_data();
        if (numerator == 0 || denominator == 0 || numerator > denominator) {
            throw std::invalid_argument(
                "latency percentile must be in the range (0,1]");
        }

        // ceil(count*numerator/denominator) without multiplying first, so a
        // very long run cannot wrap merely while asking for a percentile.
        const std::uint64_t quotient = count_ / denominator;
        const std::uint64_t remainder = count_ % denominator;
        constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
        if (quotient != 0 && numerator > limit / quotient) {
            throw std::overflow_error("latency percentile rank overflow");
        }
        std::uint64_t rank = quotient * numerator;
        if (remainder != 0) {
            if (remainder > limit / numerator) {
                throw std::overflow_error("latency percentile rank overflow");
            }
            const std::uint64_t tail = remainder * numerator;
            const std::uint64_t tail_rank =
                tail / denominator + (tail % denominator != 0 ? 1u : 0u);
            if (rank > limit - tail_rank) {
                throw std::overflow_error("latency percentile rank overflow");
            }
            rank += tail_rank;
        }

        std::uint64_t cumulative = 0;
        for (const auto& [cycles, samples] : bins_) {
            cumulative += samples;
            if (cumulative >= rank) {
                return cycles;
            }
        }
        throw std::logic_error("latency histogram count does not match bins");
    }

private:
    std::map<std::uint64_t, std::uint64_t> bins_;
    std::uint64_t count_{};
    std::uint64_t sum_{};
    bool invalid_{};

    void require_data() const
    {
        if (invalid_) {
            throw std::runtime_error("latency histogram is invalid");
        }
        if (empty()) {
            throw std::logic_error("latency histogram is empty");
        }
    }
};

/// Transaction count, useful payload bytes and the corresponding latency
/// histogram for one manager/target/traffic class.
class transaction_metrics {
public:
    void add(unsigned payload_bytes, std::uint64_t latency_cycles) noexcept
    {
        if (invalid_) {
            return;
        }
        constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
        if (transactions_ == limit
            || bytes_ > limit - static_cast<std::uint64_t>(payload_bytes)) {
            invalid_ = true;
            return;
        }
        latency_.add(latency_cycles);
        if (!latency_.valid()) {
            invalid_ = true;
            return;
        }
        ++transactions_;
        bytes_ += payload_bytes;
    }

    void reset()
    {
        transactions_ = 0;
        bytes_ = 0;
        invalid_ = false;
        latency_.reset();
    }

    bool valid() const noexcept
    {
        return !invalid_ && latency_.valid()
            && transactions_ == latency_.count();
    }
    std::uint64_t transactions() const noexcept { return transactions_; }
    std::uint64_t payload_bytes() const noexcept { return bytes_; }
    const latency_histogram& latency() const noexcept { return latency_; }

private:
    std::uint64_t transactions_{};
    std::uint64_t bytes_{};
    bool invalid_{};
    latency_histogram latency_;
};

} // namespace floo::model
