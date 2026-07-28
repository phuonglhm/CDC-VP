// SPDX-License-Identifier: SHL-0.51
//
// Response ordering for the frozen v0 configuration.
//
// Source of truth: the `NoRoB` branch of `hw/floo_rob_wrapper.sv`.
//
// `floo_pkg::ChimneyDefaultCfg` sets `BRoBType` and `RRoBType` to `NoRoB`, and
// `floogen/examples/axi_mesh_xy.yml` does not override them. `NoRoB` does not
// mean "no ordering logic". The RTL's own comment says it
//
//   "stalls transactions of the same ID going to different destinations until
//    the previous transaction is completed"
//
// and implements exactly that with a per-ID outstanding counter:
//
//   push = ax_valid_i && (!in_flight || ax_dest_i == prev_dest) && !counter_full
//   pop  = rsp_valid_i && rsp_last_i
//
// where `in_flight`, `prev_dest`, and `counter_full` come from an
// `axi_demux_id_counters` looked up by the request's AXI ID. So a manager that
// reuses an AXI ID for a different destination is serialised, while reusing it
// for the same destination is not.
//
// The reordering RoB types are a different branch of the same wrapper and are
// out of v0 scope; modelling them requires freezing a configuration that
// enables one.

#pragma once

#include "floo_noc_model/floo_types.hpp"

#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace floo::model {

/// Mirrors the `NoRoB` admission rule of `floo_rob_wrapper.sv`.
///
/// This is an admission gate, not a buffer: it decides whether a request may
/// be issued now, and it is released as responses complete.
class no_rob_order_gate {
public:
    /// `MaxRoTxnsPerId` in the RTL, which the chimney drives from
    /// `ChimneyCfg.MaxTxnsPerId`.
    explicit no_rob_order_gate(unsigned max_txns_per_id)
        : max_txns_per_id_(max_txns_per_id)
    {
        if (max_txns_per_id == 0) {
            throw std::invalid_argument("no_rob_order_gate: zero capacity");
        }
    }

    /// `push = ax_valid_i && (!in_flight || ax_dest_i == prev_dest)
    ///         && !counter_full`
    ///
    /// Returns whether a request with this AXI ID may be issued to this
    /// destination in the current cycle.
    bool may_issue(std::uint64_t axi_id, const coordinate& destination) const
    {
        const auto entry = counters_.find(axi_id);
        if (entry == counters_.end()) {
            return true;
        }
        if (entry->second.outstanding >= max_txns_per_id_) {
            return false;
        }
        if (entry->second.outstanding == 0) {
            return true;
        }
        return entry->second.destination == destination;
    }

    /// Record an issued request. Mirrors the counter push, which the RTL takes
    /// only on the request handshake.
    void issue(std::uint64_t axi_id, const coordinate& destination)
    {
        if (!may_issue(axi_id, destination)) {
            throw std::logic_error(
                "no_rob_order_gate: issued a request the gate would block");
        }
        auto& entry = counters_[axi_id];
        entry.destination = destination;
        ++entry.outstanding;
    }

    /// Record a completed response. Mirrors the counter pop, which the RTL
    /// takes on the last beat of a response.
    void complete(std::uint64_t axi_id)
    {
        const auto entry = counters_.find(axi_id);
        if (entry == counters_.end() || entry->second.outstanding == 0) {
            throw std::runtime_error(
                "no_rob_order_gate: response for an ID with nothing in flight");
        }
        --entry->second.outstanding;
    }

    unsigned outstanding(std::uint64_t axi_id) const
    {
        const auto entry = counters_.find(axi_id);
        return entry == counters_.end() ? 0u : entry->second.outstanding;
    }

    /// The destination the in-flight transactions of this ID went to. Only
    /// meaningful while `outstanding(axi_id)` is nonzero.
    coordinate destination_of(std::uint64_t axi_id) const
    {
        const auto entry = counters_.find(axi_id);
        return entry == counters_.end() ? coordinate{} : entry->second.destination;
    }

private:
    struct id_state {
        coordinate destination{};
        unsigned outstanding{0};
    };

    unsigned max_txns_per_id_;
    std::unordered_map<std::uint64_t, id_state> counters_;
};

} // namespace floo::model
