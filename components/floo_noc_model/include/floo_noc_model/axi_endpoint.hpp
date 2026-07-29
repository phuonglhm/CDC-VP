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
///
/// A multi-beat write is **one wormhole packet**: an AW with `hdr.last = 0`
/// followed by W flits, the final one carrying `last`. That is what holds a
/// route across the network, and it is the property that distinguishes the NoC
/// from a bus, so a burst must never be split into several transactions.
struct axi_transaction {
    bool is_write{};
    std::uint64_t id{};
    std::uint64_t addr{};
    /// Write data, one entry per beat. Empty for reads.
    std::vector<std::uint64_t> data{};
    /// Byte enables, applied to every beat.
    std::uint64_t strb{};
    /// Beats expected back for a read. Ignored for writes, which take their
    /// beat count from `data`.
    unsigned read_beats{1};
    /// `AxSIZE`: log2 of the bytes per beat. A narrower access than the bus
    /// width is a smaller `size`, not a padded full-width beat — a 32-bit
    /// peripheral rejects an 8-byte access outright.
    unsigned size_log2{3};

    unsigned beats() const
    {
        return is_write ? static_cast<unsigned>(data.size()) : read_beats;
    }
};

/// A completed transaction as seen by the manager that issued it.
struct axi_completion {
    bool is_write{};
    std::uint64_t id{};
    /// Read data, one entry per beat. Empty for writes.
    std::vector<std::uint64_t> data{};
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

        if (txn.beats() == 0) {
            throw std::invalid_argument(
                "axi_manager_endpoint: a transaction needs at least one beat");
        }

        if (txn.is_write) {
            axi_aw_chan aw{};
            aw.id = txn.id;
            aw.addr = txn.addr;
            aw.size = static_cast<std::uint8_t>(txn.size_log2);
            aw.burst = 1;
            aw.len = static_cast<std::uint8_t>(txn.beats() - 1);
            destination_.accept_aw(*target);
            pending_.push_back(pack_aw(aw, node_id_, *target));

            // One packet: every W but the last carries `hdr.last = 0`, so the
            // route stays held from the AW through to the final beat.
            for (unsigned beat = 0; beat < txn.beats(); ++beat) {
                axi_w_chan w{};
                w.data = txn.data[beat];
                w.strb = txn.strb;
                w.last = beat + 1 == txn.beats();
                pending_.push_back(
                    pack_w(w, node_id_, destination_.write_destination()));
            }

            buffer_.push_write(meta);
        } else {
            axi_ar_chan ar{};
            ar.id = txn.id;
            ar.addr = txn.addr;
            ar.size = static_cast<std::uint8_t>(txn.size_log2);
            ar.burst = 1;
            ar.len = static_cast<std::uint8_t>(txn.beats() - 1);
            pending_.push_back(pack_ar(ar, node_id_, *target));

            buffer_.push_read(meta);
        }

        order_gate_.issue(txn.id, *target);
        return true;
    }

    bool has_request() const { return !pending_.empty(); }

    /// The next request flit without consuming it. A signal-level driver has
    /// to present the flit for a whole cycle before it learns whether the
    /// network accepted it, so peek and pop are separate.
    const axi_req_flit& peek_request() const
    {
        if (pending_.empty()) {
            throw std::runtime_error("axi_manager_endpoint: no request flit");
        }
        return pending_.front();
    }

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
            // `ar_no_atop_pop` in `hw/floo_meta_buffer.sv` requires
            // `axi_rsp_o.r.last`, so intermediate beats of a burst must not
            // release the metadata entry.
            read_beats_.push_back(flit.r.data);
            if (!flit.r.last) {
                done.is_write = false;
                done.id = 0;
                done.resp = flit.r.resp;
                incomplete_ = true;
                return done;
            }
            const auto meta = buffer_.pop_read();
            done.is_write = false;
            done.id = meta.axi_id;
            done.data = read_beats_;
            done.resp = flit.r.resp;
            read_beats_.clear();
            incomplete_ = false;
        } else {
            throw std::runtime_error(
                "axi_manager_endpoint: response flit is neither B nor R");
        }
        order_gate_.complete(done.id);
        return done;
    }

    /// True when the last `accept_response` was an intermediate R beat, so no
    /// transaction completed and `accept_response`'s return value is a
    /// placeholder.
    bool response_incomplete() const { return incomplete_; }

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
    std::vector<std::uint64_t> read_beats_;
    bool incomplete_{false};
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
            write_addr_.push_back(flit.aw.addr);
            write_size_.push_back(flit.aw.size);
            break;
        case axi_channel::w:
            if (!pending_write_.has_value()) {
                throw std::runtime_error(
                    "axi_subordinate_endpoint: W without a preceding AW");
            }
            write_beats_.push_back(flit.w.data);
            // A burst is one packet; only its final beat completes the write.
            if (flit.hdr.last) {
                buffer_.push_write(*pending_write_);
                pending_write_.reset();
                completed_writes_.push_back(std::move(write_beats_));
                write_beats_.clear();
            }
            break;
        case axi_channel::ar:
            meta.axi_id = flit.ar.id;
            buffer_.push_read(meta);
            // `ar.len` is beats minus one, so the answer owes that many R flits.
            read_beats_.push_back(
                static_cast<unsigned>(flit.ar.len) + 1u);
            read_addr_.push_back(flit.ar.addr);
            read_size_.push_back(flit.ar.size);
            break;
        default:
            throw std::runtime_error(
                "axi_subordinate_endpoint: unexpected request channel");
        }
    }

    bool has_write() const { return buffer_.outstanding_writes() != 0; }
    bool has_read() const { return buffer_.outstanding_reads() != 0; }

    /// The data of the oldest completed write, and the address it targets.
    /// Both are what a real subordinate would apply to its storage.
    const std::vector<std::uint64_t>& pending_write_data() const
    {
        if (completed_writes_.empty()) {
            throw std::runtime_error(
                "axi_subordinate_endpoint: no completed write");
        }
        return completed_writes_.front();
    }

    std::uint64_t pending_write_addr() const { return write_addr_.front(); }
    /// `AxSIZE` of the oldest request, so a narrow access stays narrow when it
    /// is replayed onto the real subordinate.
    unsigned pending_write_size() const { return write_size_.front(); }

    /// How many R beats the oldest outstanding read expects, and from where.
    unsigned pending_read_beats() const { return read_beats_.front(); }
    std::uint64_t pending_read_addr() const { return read_addr_.front(); }
    unsigned pending_read_size() const { return read_size_.front(); }

    /// Answers the oldest outstanding write.
    axi_rsp_flit respond_write(std::uint8_t resp)
    {
        const auto meta = buffer_.pop_write();
        if (!completed_writes_.empty()) {
            completed_writes_.pop_front();
            write_addr_.pop_front();
            write_size_.pop_front();
        }
        axi_b_chan b{};
        b.id = buffer_.downstream_id();
        b.resp = resp;
        return pack_b(b, node_id_, meta);
    }

    /// Answers the oldest outstanding read with the whole burst, one R flit
    /// per beat and `last` on the final one.
    std::vector<axi_rsp_flit> respond_read_burst(
        const std::vector<std::uint64_t>& data, std::uint8_t resp)
    {
        if (data.size() != pending_read_beats()) {
            throw std::invalid_argument(
                "axi_subordinate_endpoint: answer length is not the burst length");
        }
        const auto meta = buffer_.pop_read();
        read_beats_.erase(read_beats_.begin());
        read_addr_.erase(read_addr_.begin());
        read_size_.erase(read_size_.begin());

        std::vector<axi_rsp_flit> flits;
        flits.reserve(data.size());
        for (std::size_t beat = 0; beat < data.size(); ++beat) {
            axi_r_chan r{};
            r.id = buffer_.downstream_id();
            r.data = data[beat];
            r.resp = resp;
            r.last = beat + 1 == data.size();
            flits.push_back(pack_r(r, node_id_, meta));
        }
        return flits;
    }

    /// Single-beat convenience wrapper.
    axi_rsp_flit respond_read(std::uint64_t data, std::uint8_t resp)
    {
        return respond_read_burst({data}, resp).front();
    }

private:
    coordinate node_id_;
    meta_buffer buffer_;
    std::optional<response_meta> pending_write_;
    std::vector<std::uint64_t> write_beats_;
    std::deque<std::vector<std::uint64_t>> completed_writes_;
    std::deque<std::uint64_t> write_addr_;
    std::deque<unsigned> write_size_;
    std::deque<unsigned> read_beats_;
    std::deque<std::uint64_t> read_addr_;
    std::deque<unsigned> read_size_;
};

} // namespace floo::model
