// SPDX-License-Identifier: SHL-0.51
//
// Abstract AXI endpoint transactors.
//
// These are the pieces that let AXI traffic be driven into, and collected out
// of, the modelled network. They compose the parts that *are* RTL-signed:
//
//   axi_chimney_pack.hpp   flit assembly and destination decode
//   meta_buffer.hpp        request metadata retention and downstream ID reuse
//   rob_order_gate.hpp     the NoRoB same-ID ordering rule
//
// **Verification status: model-side abstraction, not RTL-signed and not
// RTL-signable.** `hw/floo_axi_chimney.sv` is a signal-level AXI network
// interface with its own arbitration, cuts, and back-pressure; these classes
// are a transaction-level driver and collector built on its verified rules.
// They are not a substitute for a timed chimney model, and no timing claim
// should be made from them. What they do guarantee is that the flits they
// produce follow the assembly and ordering rules that the chimney
// cross-checks established.
//
// Scope: vertical slice v0. Single-beat W bursts, unicast, no ATOPs, no
// collectives.

#pragma once

#include "floo_noc_model/axi_chimney_pack.hpp"
#include "floo_noc_model/meta_buffer.hpp"
#include "floo_noc_model/rob_order_gate.hpp"

#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <vector>

namespace floo::model {

/// One AXI transaction as offered by a manager endpoint.
struct axi_transaction {
    bool is_write{};
    std::uint64_t id{};
    std::uint64_t addr{};
    /// Write data. Ignored for reads.
    std::uint64_t data{};
    std::uint64_t strb{};
};

/// A completed transaction as seen by the manager that issued it.
struct axi_completion {
    bool is_write{};
    std::uint64_t id{};
    std::uint64_t data{};
    std::uint8_t resp{};
};

/// Manager-side endpoint: turns AXI transactions into request flits and
/// matches returning response flits back to their transaction.
class axi_manager_endpoint {
public:
    axi_manager_endpoint(
        coordinate node_id,
        chimney_destination destination,
        unsigned out_id_width = 3,
        std::size_t max_txns = 32,
        unsigned max_txns_per_id = 32)
        : node_id_(node_id)
        , destination_(std::move(destination))
        , buffer_(out_id_width, max_txns)
        , order_gate_(max_txns_per_id)
    {
    }

    /// Offers a transaction. Returns false if the NoRoB ordering rule blocks
    /// it, which happens when the ID is in flight to another destination.
    bool offer(const axi_transaction& txn)
    {
        const auto target = destination_.decode_request(txn.addr);
        if (!target.has_value()) {
            throw std::runtime_error(
                "axi_manager_endpoint: address decodes to no endpoint");
        }
        if (!order_gate_.may_issue(txn.id, *target)) {
            return false;
        }

        response_meta meta{};
        meta.src_id = node_id_;
        meta.axi_id = txn.id;
        // `floo_rob_wrapper.sv` drives `rob_req` high even with `NoRoB`.
        meta.rob_req = true;

        if (txn.is_write) {
            axi_aw_chan aw{};
            aw.id = txn.id;
            aw.addr = txn.addr;
            aw.size = 3;
            aw.burst = 1;
            destination_.accept_aw(*target);
            pending_.push_back(pack_aw(aw, node_id_, *target));

            axi_w_chan w{};
            w.data = txn.data;
            w.strb = txn.strb;
            w.last = true;
            pending_.push_back(
                pack_w(w, node_id_, destination_.write_destination()));

            buffer_.push_write(meta);
        } else {
            axi_ar_chan ar{};
            ar.id = txn.id;
            ar.addr = txn.addr;
            ar.size = 3;
            ar.burst = 1;
            pending_.push_back(pack_ar(ar, node_id_, *target));

            buffer_.push_read(meta);
        }

        order_gate_.issue(txn.id, *target);
        return true;
    }

    bool has_request() const { return !pending_.empty(); }

    /// Pops the next request flit to inject into the network.
    axi_req_flit take_request()
    {
        if (pending_.empty()) {
            throw std::runtime_error("axi_manager_endpoint: no request flit");
        }
        const auto flit = pending_.front();
        pending_.pop_front();
        return flit;
    }

    /// Accepts a response flit returning from the network.
    axi_completion accept_response(const axi_rsp_flit& flit)
    {
        const auto channel =
            static_cast<axi_channel>(flit.hdr.axi_ch.to_uint());
        axi_completion done{};
        if (channel == axi_channel::b) {
            const auto meta = buffer_.pop_write();
            done.is_write = true;
            done.id = meta.axi_id;
            done.resp = flit.b.resp;
        } else if (channel == axi_channel::r) {
            const auto meta = buffer_.pop_read();
            done.is_write = false;
            done.id = meta.axi_id;
            done.data = flit.r.data;
            done.resp = flit.r.resp;
        } else {
            throw std::runtime_error(
                "axi_manager_endpoint: response flit is neither B nor R");
        }
        order_gate_.complete(done.id);
        return done;
    }

    unsigned outstanding(std::uint64_t axi_id) const
    {
        return order_gate_.outstanding(axi_id);
    }

private:
    coordinate node_id_;
    chimney_destination destination_;
    meta_buffer buffer_;
    no_rob_order_gate order_gate_;
    std::deque<axi_req_flit> pending_;
};

/// Subordinate-side endpoint: absorbs request flits and produces the response
/// flits that route back to whoever sent them.
class axi_subordinate_endpoint {
public:
    explicit axi_subordinate_endpoint(
        coordinate node_id, unsigned out_id_width = 3,
        std::size_t max_txns = 32)
        : node_id_(node_id)
        , buffer_(out_id_width, max_txns)
    {
    }

    /// Accepts a request flit. AW is retained until its W arrives, matching
    /// the chimney's AW/W coupling; AR completes immediately.
    void accept_request(const axi_req_flit& flit)
    {
        const auto channel =
            static_cast<axi_channel>(flit.hdr.axi_ch.to_uint());

        response_meta meta{};
        meta.src_id = flit.hdr.src_id;
        meta.rob_req = flit.hdr.rob_req;
        meta.rob_idx = flit.hdr.rob_idx.to_uint();

        switch (channel) {
        case axi_channel::aw:
            meta.axi_id = flit.aw.id;
            pending_write_ = meta;
            break;
        case axi_channel::w:
            if (!pending_write_.has_value()) {
                throw std::runtime_error(
                    "axi_subordinate_endpoint: W without a preceding AW");
            }
            buffer_.push_write(*pending_write_);
            pending_write_.reset();
            break;
        case axi_channel::ar:
            meta.axi_id = flit.ar.id;
            buffer_.push_read(meta);
            break;
        default:
            throw std::runtime_error(
                "axi_subordinate_endpoint: unexpected request channel");
        }
    }

    bool has_write() const { return buffer_.outstanding_writes() != 0; }
    bool has_read() const { return buffer_.outstanding_reads() != 0; }

    /// Answers the oldest outstanding write.
    axi_rsp_flit respond_write(std::uint8_t resp)
    {
        const auto meta = buffer_.pop_write();
        axi_b_chan b{};
        b.id = buffer_.downstream_id();
        b.resp = resp;
        return pack_b(b, node_id_, meta);
    }

    /// Answers the oldest outstanding read.
    axi_rsp_flit respond_read(std::uint64_t data, std::uint8_t resp)
    {
        const auto meta = buffer_.pop_read();
        axi_r_chan r{};
        r.id = buffer_.downstream_id();
        r.data = data;
        r.resp = resp;
        r.last = true;
        return pack_r(r, node_id_, meta);
    }

private:
    coordinate node_id_;
    meta_buffer buffer_;
    std::optional<response_meta> pending_write_;
};

} // namespace floo::model
