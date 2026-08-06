// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/noc_interconnect.h"

#include "floo_noc_model/axi_lanes.hpp"
#include "floo_noc_model/axi_noc.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>

namespace cdc::components {

namespace {

using namespace floo::model;

using axi_lanes::axi_shape;
using axi_lanes::bus_bytes;
using axi_lanes::byte_enabled;
using axi_lanes::pack_write;
using axi_lanes::shape_of;
using axi_lanes::unpack_read;

/// Runtime-sized access to a compile-time-sized, chimney-backed NoC.
///
/// `axi_noc` takes its dimensions as template parameters because `floo_mesh`
/// sizes its `sc_vector`s from them. A small dispatch covers the useful sizes
/// without weakening the signed mesh implementation.
struct noc_iface {
    virtual ~noc_iface() = default;
    virtual void bind_clock(
        sc_core::sc_signal<bool>& clk, sc_core::sc_signal<bool>& rst_n) = 0;
    virtual unsigned node_index(unsigned x, unsigned y) const = 0;
    virtual unsigned node_count() const = 0;
    virtual axi_manager_signals& manager(unsigned node) = 0;
    virtual axi_subordinate_signals& subordinate(unsigned node) = 0;
    virtual bool req_eject_valid(unsigned node) const = 0;
    virtual bool req_eject_ready(unsigned node) const = 0;
    virtual axi_req_flit req_eject_data(unsigned node) const = 0;
    virtual bool mesh_quiescent() const = 0;
    virtual mesh_counter_snapshot req_counter_snapshot() const = 0;
    virtual mesh_counter_snapshot rsp_counter_snapshot() const = 0;
    virtual void reset_counters() = 0;
};

template <unsigned Width, unsigned Height>
struct noc_holder final : noc_iface {
    axi_noc<Width, Height> noc;

    explicit noc_holder(const char* name) : noc(name) {}

    void bind_clock(
        sc_core::sc_signal<bool>& clk, sc_core::sc_signal<bool>& rst_n) override
    {
        noc.i_clk(clk);
        noc.i_rst_n(rst_n);
    }
    unsigned node_index(unsigned x, unsigned y) const override
    {
        return axi_noc<Width, Height>::node_index(x, y);
    }
    unsigned node_count() const override
    {
        return axi_noc<Width, Height>::num_nodes;
    }
    axi_manager_signals& manager(unsigned node) override
    {
        return noc.manager(node);
    }
    axi_subordinate_signals& subordinate(unsigned node) override
    {
        return noc.subordinate(node);
    }
    bool req_eject_valid(unsigned node) const override
    {
        return noc.chimney(node).i_req_eject_valid.read();
    }
    bool req_eject_ready(unsigned node) const override
    {
        return noc.chimney(node).o_req_eject_ready.read();
    }
    axi_req_flit req_eject_data(unsigned node) const override
    {
        return noc.chimney(node).i_req_eject_data.read();
    }
    bool mesh_quiescent() const override { return noc.mesh_quiescent(); }
    mesh_counter_snapshot req_counter_snapshot() const override
    {
        return noc.req_counter_snapshot();
    }
    mesh_counter_snapshot rsp_counter_snapshot() const override
    {
        return noc.rsp_counter_snapshot();
    }
    void reset_counters() override { noc.reset_counters(); }
};

std::unique_ptr<noc_iface> make_noc(unsigned x, unsigned y, const char* name)
{
    if (x == 2 && y == 2) return std::make_unique<noc_holder<2, 2>>(name);
    if (x == 3 && y == 3) return std::make_unique<noc_holder<3, 3>>(name);
    if (x == 4 && y == 4) return std::make_unique<noc_holder<4, 4>>(name);
    if (x == 4 && y == 2) return std::make_unique<noc_holder<4, 2>>(name);
    if (x == 2 && y == 4) return std::make_unique<noc_holder<2, 4>>(name);

    std::ostringstream message;
    message << "noc_interconnect: mesh " << x << 'x' << y
            << " is not instantiated. Add it to make_noc() in "
               "src/noc_interconnect.cpp; the dimensions are template "
               "parameters of the signed NoC model.";
    throw std::invalid_argument(message.str());
}

} // namespace

struct noc_interconnect::impl : public sc_core::sc_module {
    /// One downstream region and the socket that reaches it.
    struct target_entry {
        noc_interconnect::target_kind kind =
            noc_interconnect::target_kind::mmio;
        std::uint64_t base = 0;
        std::uint64_t size = 0;
        unsigned node = 0;
        bool mapped = false;
        std::unique_ptr<initiator_socket> socket;
    };

    /// A request that reached a subordinate node and is waiting to be answered.
    struct served_request {
        bool is_write = false;
        std::uint64_t addr = 0;
        unsigned beats = 1;
        /// `AxSIZE`, so the replayed access is as wide as the original TLM
        /// payload rather than padded to the bus width. A 32-bit peripheral
        /// rejects an 8-byte access.
        unsigned size_log2 = 3;
        /// The downstream payload, already resolved to absolute byte
        /// addresses: `write_bytes[i]` is the byte at `addr + i`, and
        /// `write_enables[i]` says whether it is written.
        ///
        /// Resolved once, in `accept_subordinate_w`, where the original AXI address
        /// and the original beat numbering are both still in hand. The raw
        /// `write_data`/`write_strb` are deliberately **not** carried past that
        /// point: an earlier version kept them, moved `addr` to the lowest
        /// enabled byte, and then re-derived `beat0_addr` from the moved
        /// address while still indexing the beats by their original numbers.
        /// A write whose first beat had `WSTRB = 0x00` silently lost its data
        /// and still reported success.
        std::vector<unsigned char> write_bytes;
        std::vector<unsigned char> write_enables;
        /// The cycle at which the downstream target's own latency has elapsed,
        /// and the cycle the downstream access was issued.
        std::uint64_t ready_at = 0;
        std::uint64_t served_at = 0;
        /// Exact bytes the replayed access must carry.
        unsigned byte_length = 0;
        /// Mesh node index of whoever asked, from the request's `src_id`.
        unsigned requester = 0;
        std::vector<std::uint64_t> read_data;
        std::uint8_t resp = 0;
        bool served = false;
    };

    /// A `b_transport` call parked until the network answers it.
    ///
    /// One instance lives on each caller's suspended SystemC stack. Adapter
    /// queues retain its address only until the matching FIFO-ordered B or
    /// final R response removes it and notifies `done`.
    struct waiter {
        sc_core::sc_event done;
        bool complete = false;
        unsigned port = 0;
        std::vector<std::uint64_t> data;
        std::uint8_t resp = to_bits(axi_pkg::axi_resp::okay);
        std::uint64_t issued_cycle = 0;
        // Carried only so a completion can be classified by the observer.
        // Nothing in the datapath reads these.
        std::uint64_t address = 0;
        unsigned length = 0;
        bool is_write = false;
    };

    /// One TLM request being presented on a chimney's manager AXI port.
    ///
    /// AW and W are independent AXI channels. The timed chimney decides their
    /// ready timing; this state only holds each payload stable until its own
    /// handshake. `parked` associates the accepted AW/AR with the caller that
    /// owns the corresponding FIFO-ordered completion.
    struct manager_request {
        bool is_write = false;
        waiter* parked = nullptr;
        coordinate destination{};
        axi_aw_chan aw{};
        bool aw_pending = false;
        std::vector<axi_w_chan> w;
        std::size_t w_index = 0;
        axi_ar_chan ar{};
        bool ar_pending = false;

        bool complete() const
        {
            return is_write ? !aw_pending && w_index == w.size()
                            : !ar_pending;
        }
    };

    /// Write-channel state collected from the subordinate AXI boundary.
    struct write_capture {
        axi_aw_chan aw{};
        coordinate requester{};
        std::vector<std::uint64_t> data;
        std::vector<std::uint64_t> strb;
    };

    /// One subordinate response transaction held until the chimney accepts it.
    struct subordinate_response {
        bool is_write = false;
        axi_b_chan b{};
        std::vector<axi_r_chan> r;
        std::size_t r_index = 0;
    };

    /// Per-node signal-adapter state. A node may host a manager, a subordinate,
    /// or neither; the NoLoopback placement rule prevents it hosting both.
    struct node_state {
        bool has_manager = false;
        bool has_subordinate = false;
        /// Which upstream port injects here, if any.
        int initiator_port = -1;
        std::deque<manager_request> requests;
        std::deque<waiter*> write_waiters;
        std::deque<waiter*> read_waiters;

        // Passive source tracking. The subordinate AXI ID is intentionally
        // rewritten to all ones, so the requester coordinate is observed when
        // the signed req link hands the AW/AR flit to the chimney, then paired
        // with the corresponding AXI handshake in FIFO order. This sideband is
        // used only to exclude target delay from the latency metric.
        std::deque<coordinate> aw_sources;
        std::deque<coordinate> ar_sources;
        std::deque<write_capture> writes;
        std::deque<served_request> serving;
        std::deque<subordinate_response> responses;
    };

    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};

    std::unique_ptr<noc_iface> noc;
    sc_core::sc_time period;

    std::vector<target_entry> targets;
    std::vector<node_state> nodes;
    std::vector<std::unique_ptr<cpu_socket_t>> extra_ports;
    std::vector<node> initiator_nodes;
    std::vector<unsigned> outstanding_by_port;
    std::vector<unsigned> peak_outstanding_by_port;
    // `sc_event` is neither copyable nor movable, so it cannot live in a
    // plain vector that is assigned into.
    std::vector<std::unique_ptr<sc_core::sc_event>> slot_available;
    std::vector<std::deque<std::uint64_t>> write_hold_off;
    std::vector<std::deque<std::uint64_t>> read_hold_off;

    unsigned mesh_x = 0;
    unsigned mesh_y = 0;
    unsigned initiator_count = 0;
    unsigned target_capacity = 0;
    unsigned mapped_targets = 0;
    unsigned max_outstanding_per_port = 0;
    noc_interconnect::timing_mode timing_backend =
        noc_interconnect::timing_mode::detailed;

    static constexpr unsigned axi_id_bits = 3;
    static constexpr unsigned max_manager_ports = 1u << axi_id_bits;
    static constexpr std::uint64_t downstream_id =
        axi_chimney_node<axi_id_bits>::downstream_id;

    std::uint64_t cycle = 0;
    std::uint64_t completed = 0;
    std::uint64_t latency_sum = 0;
    std::uint64_t last_latency = 0;
    std::vector<std::uint64_t> last_latency_by_port;
    noc_interconnect::completion_observer completion_hook;

    /// Passive by construction: it is handed a value, cannot reach the
    /// datapath, and is called after all bookkeeping for this completion is
    /// final so an observer reading a const accessor sees a consistent state.
    void notify_completion(
        unsigned port, std::uint64_t address, unsigned length, bool is_write,
        std::uint64_t latency)
    {
        if (!completion_hook) {
            return;
        }
        noc_interconnect::completion record{};
        record.port = port;
        record.address = address;
        record.length = length;
        record.is_write = is_write;
        record.latency_cycles = latency;
        record.at_cycle = cycle;
        completion_hook(record);
    }
    unsigned in_flight = 0;
    bool started = false;
    /// The mesh needs its reset to elapse before it will carry anything. A
    /// `b_transport` issued at time zero would otherwise hand flits to a
    /// network still in reset, which swallows them.
    bool out_of_reset = false;
    sc_core::sc_event reset_done;
    sc_core::sc_event work;
    std::uint64_t clock_gate_count = 0;
    std::uint64_t mesh_idle_wrapper_busy = 0;
    std::uint64_t mid_half_request_arrivals = 0;
    std::vector<bool> manager_request_driven;

    SC_HAS_PROCESS(impl);

    impl(sc_core::sc_module_name name, unsigned x, unsigned y,
         unsigned num_targets, unsigned num_initiators, sc_core::sc_time tick,
         unsigned outstanding_limit, noc_interconnect::timing_mode mode)
        : sc_core::sc_module(name)
        , noc(make_noc(x, y, "noc"))
        , period(tick)
        , mesh_x(x)
        , mesh_y(y)
        , initiator_count(num_initiators)
        , target_capacity(num_targets)
        , max_outstanding_per_port(outstanding_limit)
        , timing_backend(mode)
    {
        if (num_initiators == 0) {
            throw std::invalid_argument(
                "noc_interconnect: at least one upstream port is required");
        }
        if (num_initiators > max_manager_ports) {
            throw std::invalid_argument(
                "noc_interconnect: the frozen 3-bit AXI ID supports at most "
                "8 upstream ports");
        }
        // The public constructor checks the period before this object is
        // created; this is the belt-and-braces copy for a direct construction.
        if (tick <= sc_core::SC_ZERO_TIME) {
            throw std::invalid_argument(
                "noc_interconnect: the network clock period must be positive");
        }
        noc->bind_clock(clk, rst_n);
        nodes.resize(noc->node_count());
        manager_request_driven.assign(noc->node_count(), false);
        last_latency_by_port.assign(num_initiators, 0);
        initiator_nodes.assign(num_initiators, node{0, 0});
        outstanding_by_port.assign(num_initiators, 0);
        peak_outstanding_by_port.assign(num_initiators, 0);
        write_hold_off.resize(num_initiators);
        read_hold_off.resize(num_initiators);
        for (unsigned port = 0; port < num_initiators; ++port) {
            slot_available.push_back(std::make_unique<sc_core::sc_event>());
        }

        SC_THREAD(network_thread);
    }

    unsigned node_of(const coordinate& id) const
    {
        return noc->node_index(id.x.to_uint(), id.y.to_uint());
    }

    unsigned index_of(node where) const
    {
        if (where.x >= mesh_x || where.y >= mesh_y) {
            throw std::out_of_range("noc_interconnect: node outside the mesh");
        }
        return noc->node_index(where.x, where.y);
    }

    int decode(std::uint64_t addr) const
    {
        for (std::size_t index = 0; index < targets.size(); ++index) {
            const auto& entry = targets[index];
            if (!entry.mapped) {
                continue;
            }
            // Subtraction, not `addr < base + size`. The addition wraps for a
            // region whose last byte is `UINT64_MAX`, making a perfectly valid
            // mapping unreachable. `reference_address_map::decode()` has always
            // done it this way; this one had drifted.
            if (addr >= entry.base && addr - entry.base < entry.size) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    /// Enables the signal adapters after platform placement is complete.
    /// Called lazily on the first transaction; no target may share a node with
    /// an upstream port.
    ///
    /// `floo_router` defaults to `NoLoopback = 1`: the Eject-input to
    /// Eject-output crossbar leg is tied to zero, so a flit addressed to the
    /// node that injected it can never be delivered. Such a target would wedge
    /// that initiator's input FIFO the first time it was accessed, and the
    /// platform would simply hang.
    ///
    /// Run from `end_of_elaboration`, so it fails **before any traffic**,
    /// after every `place_initiator` and `add_target` call has been made. An
    /// earlier version checked this on the first access instead, which is late
    /// enough that a platform could be built, started, and only then hang.
    /// No target may share a node with an upstream port — including a port the
    /// platform never placed.
    ///
    /// `(0,0)` is the documented default, so it is a real placement, not a
    /// "not yet decided". An earlier version skipped unplaced ports here on the
    /// theory that they might still move; the effect was that a target at
    /// `(0,0)` was accepted during configuration and rejected only from
    /// `end_of_elaboration()`, which is precisely the late failure this check
    /// was moved forward to avoid.
    ///
    /// The cost is that `add_target(..., {0,0})` followed by
    /// `place_initiator(0, {1,1})` is refused even though it would have ended
    /// up legal. That ordering is worth refusing: the public contract says
    /// where an unplaced port is, and a configuration that depends on moving it
    /// afterwards is relying on a transient the contract does not promise.
    void reject_self_node_targets() const
    {
        for (const auto& entry : targets) {
            if (!entry.mapped) {
                continue;
            }
            bool hosts_initiator = false;
            for (unsigned port = 0; port < initiator_count; ++port) {
                if (index_of(initiator_nodes[port]) == entry.node) {
                    hosts_initiator = true;
                    break;
                }
            }
            if (!hosts_initiator) {
                continue;
            }
            std::ostringstream message;
            message << "noc_interconnect: target at 0x" << std::hex
                    << entry.base << std::dec << " sits on node "
                    << (entry.node % mesh_x) << ',' << (entry.node / mesh_x)
                    << ", which already hosts an upstream port. The router's"
                       " NoLoopback tie-off makes a self-addressed flit"
                       " undeliverable, so this would hang rather than fail."
                       " Place the target on another node.";
            throw std::runtime_error(message.str());
        }
    }

    void end_of_elaboration() override
    {
        reject_self_node_targets();
    }

    void ensure_started()
    {
        if (started) {
            return;
        }
        started = true;

        for (unsigned port = 0; port < initiator_count; ++port) {
            const unsigned index = index_of(initiator_nodes[port]);
            auto& state = nodes[index];
            if (state.has_manager) {
                throw std::runtime_error(
                    "noc_interconnect: two upstream ports on one mesh node");
            }
            state.has_manager = true;
            state.initiator_port = static_cast<int>(port);
        }

        for (const auto& entry : targets) {
            if (!entry.mapped) {
                continue;
            }
            // `floo_router` defaults to `NoLoopback = 1`: the Eject-input to
            // Eject-output crossbar leg is tied to zero, so a flit addressed to
            // the node that injected it can never be delivered. A target on an
            // initiator's own node would wedge that initiator's input FIFO the
            // first time it was accessed, and the platform would simply hang.
            // Refuse it at construction instead.
            auto& state = nodes[entry.node];
            state.has_subordinate = true;
        }
    }

    /// Advances the signal-driven NoC one clock.
    ///
    /// Drive at clock-low, sample every AXI handshake, raise the clock, then
    /// update the adapter state from exactly what was sampled. This is the same
    /// synchronous-BFM discipline as the RTL cross-checks.
    void step_once()
    {
        clk.write(false);

        std::fill(
            manager_request_driven.begin(), manager_request_driven.end(), false);
        for (unsigned index = 0; index < nodes.size(); ++index) {
            auto& state = nodes[index];
            auto& manager = noc->manager(index);
            auto& subordinate = noc->subordinate(index);
            const bool active = rst_n.read();

            manager.aw_valid.write(false);
            manager.w_valid.write(false);
            manager.ar_valid.write(false);
            if (active && state.has_manager && !state.requests.empty()) {
                manager_request_driven[index] = true;
                const auto& request = state.requests.front();
                if (request.is_write) {
                    manager.aw.write(request.aw);
                    manager.aw_dest.write(request.destination);
                    manager.aw_valid.write(request.aw_pending);
                    if (request.w_index < request.w.size()) {
                        manager.w.write(request.w[request.w_index]);
                        manager.w_valid.write(true);
                    }
                } else {
                    manager.ar.write(request.ar);
                    manager.ar_dest.write(request.destination);
                    manager.ar_valid.write(request.ar_pending);
                }
            }

            manager.b_ready.write(
                active && state.has_manager && !state.write_waiters.empty());
            manager.r_ready.write(
                active && state.has_manager && !state.read_waiters.empty());

            subordinate.aw_ready.write(active && state.has_subordinate);
            subordinate.w_ready.write(active && state.has_subordinate);
            subordinate.ar_ready.write(active && state.has_subordinate);
            subordinate.b_valid.write(false);
            subordinate.r_valid.write(false);
            if (active && state.has_subordinate && !state.responses.empty()) {
                const auto& response = state.responses.front();
                if (response.is_write) {
                    subordinate.b.write(response.b);
                    subordinate.b_valid.write(true);
                } else if (response.r_index < response.r.size()) {
                    subordinate.r.write(response.r[response.r_index]);
                    subordinate.r_valid.write(true);
                }
            }
        }

        wait(period / 2);

        struct sampled {
            bool manager_aw = false;
            bool manager_w = false;
            bool manager_ar = false;
            bool manager_b = false;
            axi_b_chan manager_b_data{};
            bool manager_r = false;
            axi_r_chan manager_r_data{};

            bool subordinate_aw = false;
            axi_aw_chan subordinate_aw_data{};
            bool subordinate_w = false;
            axi_w_chan subordinate_w_data{};
            bool subordinate_ar = false;
            axi_ar_chan subordinate_ar_data{};
            bool subordinate_b = false;
            bool subordinate_r = false;

            bool req_eject = false;
            axi_req_flit req_eject_data{};
        };
        std::vector<sampled> observed(nodes.size());
        for (unsigned index = 0; index < nodes.size(); ++index) {
            auto& state = nodes[index];
            auto& manager = noc->manager(index);
            auto& subordinate = noc->subordinate(index);
            auto& sample = observed[index];
            const bool active = rst_n.read();
            if (active && state.has_manager && !state.requests.empty()
                && !manager_request_driven[index]) {
                ++mid_half_request_arrivals;
            }

            sample.manager_aw = active && state.has_manager
                && manager.aw_valid.read() && manager.aw_ready.read();
            sample.manager_w = active && state.has_manager
                && manager.w_valid.read() && manager.w_ready.read();
            sample.manager_ar = active && state.has_manager
                && manager.ar_valid.read() && manager.ar_ready.read();
            sample.manager_b = active && state.has_manager
                && manager.b_valid.read() && manager.b_ready.read();
            sample.manager_b_data = manager.b.read();
            sample.manager_r = active && state.has_manager
                && manager.r_valid.read() && manager.r_ready.read();
            sample.manager_r_data = manager.r.read();

            sample.subordinate_aw = active && state.has_subordinate
                && subordinate.aw_valid.read()
                && subordinate.aw_ready.read();
            sample.subordinate_aw_data = subordinate.aw.read();
            sample.subordinate_w = active && state.has_subordinate
                && subordinate.w_valid.read()
                && subordinate.w_ready.read();
            sample.subordinate_w_data = subordinate.w.read();
            sample.subordinate_ar = active && state.has_subordinate
                && subordinate.ar_valid.read()
                && subordinate.ar_ready.read();
            sample.subordinate_ar_data = subordinate.ar.read();
            sample.subordinate_b = active && state.has_subordinate
                && subordinate.b_valid.read()
                && subordinate.b_ready.read();
            sample.subordinate_r = active && state.has_subordinate
                && subordinate.r_valid.read()
                && subordinate.r_ready.read();

            sample.req_eject = active && state.has_subordinate
                && noc->req_eject_valid(index)
                && noc->req_eject_ready(index);
            sample.req_eject_data = noc->req_eject_data(index);
        }

        clk.write(true);
        wait(period / 2);
        ++cycle;

        for (unsigned index = 0; index < nodes.size(); ++index) {
            auto& state = nodes[index];
            const auto& sample = observed[index];

            if (!state.requests.empty()) {
                auto& request = state.requests.front();
                if (sample.manager_aw) {
                    if (request.parked == nullptr) {
                        throw std::logic_error(
                            "noc_interconnect: accepted AW has no waiter");
                    }
                    state.write_waiters.push_back(request.parked);
                    request.aw_pending = false;
                }
                if (sample.manager_w) {
                    ++request.w_index;
                }
                if (sample.manager_ar) {
                    if (request.parked == nullptr) {
                        throw std::logic_error(
                            "noc_interconnect: accepted AR has no waiter");
                    }
                    state.read_waiters.push_back(request.parked);
                    request.ar_pending = false;
                }
                if (request.complete()) {
                    state.requests.pop_front();
                }
            }

            // Observe the source before applying AXI handshakes. AR can cross
            // both boundaries in the same cycle; AW emerges from its spill
            // register later. FIFO pairing handles both cases.
            if (sample.req_eject) {
                const auto channel = static_cast<axi_channel>(
                    sample.req_eject_data.hdr.axi_ch.to_uint());
                if (channel == axi_channel::aw) {
                    state.aw_sources.push_back(
                        sample.req_eject_data.hdr.src_id);
                } else if (channel == axi_channel::ar) {
                    state.ar_sources.push_back(
                        sample.req_eject_data.hdr.src_id);
                }
            }

            if (sample.subordinate_aw) {
                accept_subordinate_aw(state, sample.subordinate_aw_data);
            }
            if (sample.subordinate_w) {
                accept_subordinate_w(state, sample.subordinate_w_data);
            }
            if (sample.subordinate_ar) {
                accept_subordinate_ar(state, sample.subordinate_ar_data);
            }

            if (sample.subordinate_b || sample.subordinate_r) {
                accept_subordinate_response(
                    state, sample.subordinate_b, sample.subordinate_r);
            }
            if (sample.manager_b) {
                accept_manager_b(state, sample.manager_b_data);
            }
            if (sample.manager_r) {
                accept_manager_r(state, sample.manager_r_data);
            }
        }

        for (auto& state : nodes) {
            serve_ready_requests(state);
        }
    }

    void accept_subordinate_aw(node_state& state, const axi_aw_chan& aw)
    {
        if (state.aw_sources.empty()) {
            throw std::runtime_error(
                "noc_interconnect: subordinate AW has no req-link source");
        }
        write_capture capture{};
        capture.aw = aw;
        capture.requester = state.aw_sources.front();
        state.aw_sources.pop_front();
        state.writes.push_back(std::move(capture));
    }

    served_request make_write_served_request(
        const axi_aw_chan& aw, const coordinate& requester,
        const std::vector<std::uint64_t>& data,
        const std::vector<std::uint64_t>& strb)
    {
        served_request entry{};
        entry.is_write = true;
        entry.requester = node_of(requester);
        entry.addr = aw.addr;
        entry.size_log2 = aw.size;
        entry.beats = static_cast<unsigned>(aw.len) + 1u;
        if (data.size() != entry.beats || strb.size() != entry.beats) {
            throw std::logic_error(
                "noc_interconnect: write replay lacks a complete AXI frame");
        }

        // Resolve every enabled lane to its absolute byte address, here and
        // only here. Both timing backends use this function: fast mode is
        // allowed to approximate time, not lane placement or byte enables.
        const std::uint64_t aw_addr = entry.addr;
        const unsigned lane0 = static_cast<unsigned>(aw_addr % bus_bytes);
        const std::uint64_t beat0_addr = aw_addr - lane0;

        struct located_byte {
            std::uint64_t address;
            unsigned char value;
        };
        std::vector<located_byte> located;
        located.reserve(
            static_cast<std::size_t>(entry.beats) * bus_bytes);

        for (unsigned beat = 0; beat < entry.beats; ++beat) {
            for (unsigned lane = 0; lane < bus_bytes; ++lane) {
                if (((strb[beat] >> lane) & 1ull) == 0) {
                    continue;
                }
                const std::uint64_t offset =
                    static_cast<std::uint64_t>(beat) * bus_bytes + lane;
                if (offset > UINT64_MAX - beat0_addr) {
                    throw std::overflow_error(
                        "noc_interconnect: write frame wraps the address "
                        "space");
                }
                located.push_back(
                    {beat0_addr + offset,
                     static_cast<unsigned char>(
                         (data[beat] >> (8 * lane)) & 0xFF)});
            }
        }

        if (located.empty()) {
            // Every lane disabled. A legal AXI write that changes nothing,
            // and it must stay side-effect free downstream.
            entry.byte_length = 0;
            return entry;
        }

        std::uint64_t lowest = located.front().address;
        std::uint64_t highest = located.front().address;
        for (const auto& item : located) {
            lowest = std::min(lowest, item.address);
            highest = std::max(highest, item.address);
        }
        entry.addr = lowest;
        entry.byte_length = static_cast<unsigned>(highest - lowest + 1);
        entry.write_bytes.assign(entry.byte_length, 0);
        entry.write_enables.assign(entry.byte_length, 0);
        for (const auto& item : located) {
            const auto index =
                static_cast<std::size_t>(item.address - lowest);
            entry.write_bytes[index] = item.value;
            entry.write_enables[index] = TLM_BYTE_ENABLED;
        }
        return entry;
    }

    served_request make_read_served_request(
        const axi_ar_chan& ar, const coordinate& requester)
    {
        served_request entry{};
        entry.is_write = false;
        entry.requester = node_of(requester);
        entry.addr = ar.addr;
        entry.size_log2 = ar.size;
        entry.beats = static_cast<unsigned>(ar.len) + 1u;
        // A read has no strobes: AXI expresses its length as beats times
        // `ARSIZE`, and a master wanting fewer bytes reads the whole beat and
        // uses part of it. The wrapper trims at the initiator.
        entry.byte_length = entry.beats * (1u << entry.size_log2);
        return entry;
    }

    void accept_subordinate_w(node_state& state, const axi_w_chan& w)
    {
        if (state.writes.empty()) {
            throw std::runtime_error(
                "noc_interconnect: subordinate W arrived before AW");
        }
        auto& capture = state.writes.front();
        capture.data.push_back(w.data);
        capture.strb.push_back(w.strb);
        const unsigned expected = static_cast<unsigned>(capture.aw.len) + 1u;
        if (capture.data.size() > expected) {
            throw std::runtime_error(
                "noc_interconnect: subordinate received too many W beats");
        }
        if (!w.last) {
            if (capture.data.size() == expected) {
                throw std::runtime_error(
                    "noc_interconnect: final expected W beat lacks WLAST");
            }
            return;
        }
        if (capture.data.size() != expected) {
            throw std::runtime_error(
                "noc_interconnect: early WLAST at subordinate");
        }

        state.serving.push_back(make_write_served_request(
            capture.aw, capture.requester, capture.data, capture.strb));
        state.writes.pop_front();
    }

    void accept_subordinate_ar(node_state& state, const axi_ar_chan& ar)
    {
        if (state.ar_sources.empty()) {
            throw std::runtime_error(
                "noc_interconnect: subordinate AR has no req-link source");
        }
        const auto requester = state.ar_sources.front();
        state.ar_sources.pop_front();
        state.serving.push_back(make_read_served_request(ar, requester));
    }

    void accept_subordinate_response(
        node_state& state, bool accepted_b, bool accepted_r)
    {
        if (state.responses.empty() || accepted_b == accepted_r) {
            throw std::runtime_error(
                "noc_interconnect: invalid subordinate response handshake");
        }
        auto& response = state.responses.front();
        if (accepted_b) {
            if (!response.is_write) {
                throw std::runtime_error(
                    "noc_interconnect: B accepted for a read response");
            }
            state.responses.pop_front();
            return;
        }
        if (response.is_write || response.r_index >= response.r.size()) {
            throw std::runtime_error(
                "noc_interconnect: R accepted for an invalid read response");
        }
        ++response.r_index;
        if (response.r_index == response.r.size()) {
            state.responses.pop_front();
        }
    }

    static std::uint8_t collapse_read_resp(
        std::uint8_t current, std::uint8_t incoming)
    {
        if (axi_pkg::is_error(static_cast<axi_pkg::axi_resp>(current))) {
            return current;
        }
        return incoming;
    }

    void complete_manager_transaction(waiter& parked, std::uint64_t hold_off)
    {
        parked.complete = true;
        ++completed;
        const auto elapsed = cycle - parked.issued_cycle;
        last_latency = elapsed > hold_off ? elapsed - hold_off : 0;
        last_latency_by_port[parked.port] = last_latency;
        latency_sum += last_latency;
        if (in_flight > 0) {
            --in_flight;
        }
        if (outstanding_by_port[parked.port] == 0) {
            throw std::logic_error(
                "noc_interconnect: completion underflowed the port slots");
        }
        --outstanding_by_port[parked.port];
        slot_available[parked.port]->notify(sc_core::SC_ZERO_TIME);
        notify_completion(
            parked.port, parked.address, parked.length, parked.is_write,
            last_latency);
        parked.done.notify(sc_core::SC_ZERO_TIME);
    }

    void accept_manager_b(node_state& state, const axi_b_chan& b)
    {
        if (state.initiator_port < 0
            || b.id != static_cast<unsigned>(state.initiator_port)) {
            throw std::runtime_error(
                "noc_interconnect: B response carries the wrong manager ID");
        }
        const auto port = static_cast<unsigned>(state.initiator_port);
        if (state.write_waiters.empty() || write_hold_off[port].empty()) {
            throw std::runtime_error(
                "noc_interconnect: B response has no FIFO completion owner");
        }
        auto* parked = state.write_waiters.front();
        state.write_waiters.pop_front();
        const auto charged = write_hold_off[port].front();
        write_hold_off[port].pop_front();
        parked->resp = b.resp;
        complete_manager_transaction(*parked, charged);
    }

    void accept_manager_r(node_state& state, const axi_r_chan& r)
    {
        if (state.initiator_port < 0
            || r.id != static_cast<unsigned>(state.initiator_port)) {
            throw std::runtime_error(
                "noc_interconnect: R response carries the wrong manager ID");
        }
        const auto port = static_cast<unsigned>(state.initiator_port);
        if (state.read_waiters.empty()) {
            throw std::runtime_error(
                "noc_interconnect: R response has no FIFO completion owner");
        }
        auto* parked = state.read_waiters.front();
        parked->data.push_back(r.data);
        parked->resp = collapse_read_resp(parked->resp, r.resp);
        if (!r.last) {
            return;
        }
        if (read_hold_off[port].empty()) {
            throw std::runtime_error(
                "noc_interconnect: final R has no target-delay owner");
        }
        state.read_waiters.pop_front();
        const auto charged = read_hold_off[port].front();
        read_hold_off[port].pop_front();
        complete_manager_transaction(*parked, charged);
    }

    /// Runs the downstream TLM access for requests whose target latency has
    /// elapsed, then queues a subordinate AXI response.
    void serve_ready_requests(node_state& state)
    {
        if (state.serving.empty()) {
            return;
        }
        auto& entry = state.serving.front();
        if (!entry.served) {
            entry.served_at = cycle;
            perform_downstream(entry);
            entry.served = true;
        }
        if (cycle < entry.ready_at) {
            return;
        }
        // The wait above is the target's own access latency, not the
        // interconnect's. Preserve it in the same per-channel FIFO order as the
        // `MaxUniqueIds = 1` response metadata. A single accumulator per
        // requester is insufficient now that more than one call may be in
        // flight: the first completion would consume another transaction's
        // target delay.
        const auto requester_port = nodes[entry.requester].initiator_port;
        if (requester_port < 0) {
            throw std::runtime_error(
                "noc_interconnect: target delay belongs to no upstream port");
        }
        const auto target_delay = entry.ready_at > entry.served_at
            ? entry.ready_at - entry.served_at
            : 0;
        auto& delay_fifo = entry.is_write
            ? write_hold_off[static_cast<unsigned>(requester_port)]
            : read_hold_off[static_cast<unsigned>(requester_port)];
        delay_fifo.push_back(target_delay);

        subordinate_response response{};
        response.is_write = entry.is_write;
        if (entry.is_write) {
            response.b.id = downstream_id;
            response.b.resp = entry.resp;
        } else {
            response.r.reserve(entry.read_data.size());
            for (std::size_t beat = 0; beat < entry.read_data.size(); ++beat) {
                axi_r_chan r{};
                r.id = downstream_id;
                r.data = entry.read_data[beat];
                r.resp = entry.resp;
                r.last = beat + 1 == entry.read_data.size();
                response.r.push_back(r);
            }
            if (response.r.empty()) {
                throw std::runtime_error(
                    "noc_interconnect: a read response needs at least one beat");
            }
        }
        state.responses.push_back(std::move(response));
        state.serving.pop_front();
    }

    /// Issues the real TLM transaction to the mapped peripheral.
    ///
    /// `delay` is the caller's current local-time annotation. Detailed mode
    /// passes zero and later converts the target's increment into a per-node
    /// hold-off. Fast mode passes incoming plus estimated request time and
    /// returns the target's nondecreasing annotation to its caller.
    void perform_downstream_access(
        served_request& entry, sc_core::sc_time& delay)
    {
        const int slot = decode(entry.addr);
        if (slot < 0) {
            // Nothing decoded, so nothing was ever reached: that is `DECERR`,
            // the decode error, not `SLVERR`. An earlier version wrote the
            // literal `1` here and called it SLVERR in a comment; `1` is
            // `EXOKAY`, so a failed decode was reported upstream as success.
            entry.resp = to_bits(axi_pkg::axi_resp::decerr);
            entry.read_data.assign(entry.beats, 0);
            return;
        }
        auto& target = targets[static_cast<std::size_t>(slot)];

        // Where the replayed access starts. A narrow read is exactly its own
        // `2**ARSIZE` block at the requested address. A full-width read has to
        // start at the bus-aligned base of beat 0, because that is the frame
        // the initiator indexes when it pulls its bytes back out of the lanes.
        const unsigned entry_lane0 =
            static_cast<unsigned>(entry.addr % bus_bytes);
        const std::uint64_t access_addr =
            (entry.is_write || entry.size_log2 < 3)
                ? entry.addr
                : entry.addr - entry_lane0;

        // A write replays exactly the byte addresses its strobes named. A read
        // takes the whole beat span, which is what AXI actually fetches: a
        // master wanting fewer bytes reads the beats and uses part of them, and
        // `ARSIZE` is what keeps a narrow access narrow for a narrow target.
        const unsigned length = entry.is_write
            ? entry.byte_length
            : entry.beats * (1u << entry.size_log2);
        if (length == 0) {
            // Every lane was disabled. Legal, and it must not be turned into a
            // zero-length TLM access, which targets reject.
            entry.resp = to_bits(axi_pkg::axi_resp::okay);
            return;
        }
        // Already resolved to absolute addresses in `accept_subordinate_w`.
        // Nothing
        // here re-derives a beat number from `entry.addr`, which is what made
        // a fully disabled leading beat drop data.
        std::vector<unsigned char> bytes =
            entry.is_write ? entry.write_bytes
                           : std::vector<unsigned char>(length, 0);

        tlm::tlm_generic_payload payload;
        payload.set_command(entry.is_write ? tlm::TLM_WRITE_COMMAND
                                           : tlm::TLM_READ_COMMAND);
        payload.set_address(access_addr - target.base);
        payload.set_data_ptr(bytes.data());
        payload.set_data_length(length);
        payload.set_streaming_width(length);
        // Carry the strobes downstream, but only when they say something.
        //
        // A null byte-enable pointer means "every byte enabled" in TLM, and
        // that is the normal case. Attaching a fully-enabled array instead is
        // not equivalent in practice: a target that does not implement byte
        // enables answers `TLM_BYTE_ENABLE_ERROR_RESPONSE` rather than ignoring
        // them, and `dma_tlm` does exactly that. Doing this unconditionally
        // broke every register write to the DMA and faulted the firmware.
        //
        // Passing a *partial* pattern to such a target still fails, and that is
        // correct: it cannot honour the access, and silently widening the write
        // would corrupt the neighbouring bytes.
        bool all_enabled = true;
        for (const auto enabled : entry.write_enables) {
            if (enabled != TLM_BYTE_ENABLED) {
                all_enabled = false;
                break;
            }
        }
        if (entry.is_write && !entry.write_enables.empty() && !all_enabled) {
            payload.set_byte_enable_ptr(entry.write_enables.data());
            payload.set_byte_enable_length(
                static_cast<unsigned>(entry.write_enables.size()));
        } else {
            payload.set_byte_enable_ptr(nullptr);
            payload.set_byte_enable_length(0);
        }
        payload.set_dmi_allowed(false);
        payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        (*target.socket)->b_transport(payload, delay);

        // The request reached a target, so any failure it reports is the
        // subordinate's: `SLVERR`. `DECERR` is reserved for the decode failure
        // above, which is the interconnect's own answer.
        entry.resp = to_bits(payload.is_response_ok()
                                 ? axi_pkg::axi_resp::okay
                                 : axi_pkg::axi_resp::slverr);
        if (!entry.is_write) {
            // Return the bytes in the lanes AXI would have used, so the
            // initiator's `unpack_read` finds them where it looks.
            entry.read_data.assign(entry.beats, 0);
            const unsigned start_lane =
                static_cast<unsigned>(access_addr % bus_bytes);
            for (unsigned index = 0; index < length; ++index) {
                const unsigned beat = (start_lane + index) / bus_bytes;
                const unsigned lane = (start_lane + index) % bus_bytes;
                if (beat >= entry.read_data.size()) {
                    break;
                }
                entry.read_data[beat] |=
                    static_cast<std::uint64_t>(bytes[index]) << (8 * lane);
            }
        }

    }

    std::uint64_t rounded_cycles(sc_core::sc_time duration) const
    {
        const double cycles = duration / period;
        auto ticks = static_cast<std::uint64_t>(cycles);
        if (static_cast<double>(ticks) < cycles) {
            ++ticks;
        }
        return ticks;
    }

    /// Detailed-mode scheduling wrapper.
    ///
    /// Waiting inside the network thread would freeze every other node, so the
    /// target's annotation becomes a cycle-counted hold-off instead.
    void perform_downstream(served_request& entry)
    {
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        perform_downstream_access(entry, delay);
        // Round **up** to the first cycle on which the response may legally
        // appear. No sub-cycle residue is carried between transactions.
        const auto ticks = rounded_cycles(delay);
        entry.ready_at = cycle + ticks;
    }

    static unsigned manhattan(node from, node to)
    {
        const auto dx = from.x > to.x ? from.x - to.x : to.x - from.x;
        const auto dy = from.y > to.y ? from.y - to.y : to.y - from.y;
        return dx + dy;
    }

    /// Calibrated no-contention split used by the fast backend.
    ///
    /// The aggregate single-beat read is `4*hops + 6`: 10 cycles at one hop
    /// and 30 at six, the two post-A-3 detailed baselines. Splitting the fixed
    /// and hop terms equally lets a downstream target observe request arrival
    /// time. A write has AW plus N W flits, so it costs N serialization cycles
    /// beyond an AR. A read pays one cycle for every response beat after the
    /// first.
    std::uint64_t fast_request_cycles(
        unsigned port, unsigned target_node, bool is_write,
        unsigned beats) const
    {
        const node target{
            target_node % mesh_x,
            target_node / mesh_x};
        const auto hops = manhattan(initiator_nodes[port], target);
        return 2u * hops + 3u + (is_write ? beats : 0u);
    }

    std::uint64_t fast_response_cycles(
        unsigned port, unsigned target_node, bool is_write,
        unsigned beats) const
    {
        const node target{
            target_node % mesh_x,
            target_node / mesh_x};
        const auto hops = manhattan(initiator_nodes[port], target);
        return 2u * hops + 3u
            + (!is_write && beats > 0 ? beats - 1u : 0u);
    }

    void fast_transport(
        unsigned port, int target_slot, tlm::tlm_generic_payload& trans,
        sc_core::sc_time& delay, const axi_shape& shape,
        const unsigned char* enables, unsigned enable_length)
    {
        while (outstanding_by_port[port] >= max_outstanding_per_port) {
            sc_core::wait(*slot_available[port]);
        }
        ++outstanding_by_port[port];
        peak_outstanding_by_port[port] =
            std::max(peak_outstanding_by_port[port],
                     outstanding_by_port[port]);
        ++in_flight;

        // Everything after admission is inside the cleanup boundary. In
        // particular, lane packing and served-request construction allocate;
        // an exception there must not permanently consume a port slot.
        try {
            const bool is_write = trans.is_write();
            const auto& target =
                targets[static_cast<std::size_t>(target_slot)];
            const coordinate requester{
                initiator_nodes[port].x, initiator_nodes[port].y};
            served_request entry{};
            if (is_write) {
                axi_aw_chan aw{};
                aw.id = port;
                aw.addr = trans.get_address();
                aw.len = static_cast<std::uint8_t>(shape.beats - 1);
                aw.size = static_cast<std::uint8_t>(shape.size_log2);
                aw.burst = 1;
                const auto view = pack_write(
                    trans.get_data_ptr(), trans.get_data_length(), shape,
                    enables, enable_length);
                entry = make_write_served_request(
                    aw, requester, view.data, view.strb);
            } else {
                axi_ar_chan ar{};
                ar.id = port;
                ar.addr = trans.get_address();
                ar.len = static_cast<std::uint8_t>(shape.beats - 1);
                ar.size = static_cast<std::uint8_t>(shape.size_log2);
                ar.burst = 1;
                entry = make_read_served_request(ar, requester);
            }

            const auto request_cycles =
                fast_request_cycles(port, target.node, is_write, shape.beats);
            const auto response_cycles =
                fast_response_cycles(port, target.node, is_write, shape.beats);
            const auto network_cycles = request_cycles + response_cycles;

            delay += period * static_cast<double>(request_cycles);
            const auto before_target = delay;
            perform_downstream_access(entry, delay);
            if (delay < before_target) {
                throw std::runtime_error(
                    "noc_interconnect: a fast-mode target decreased the "
                    "annotated delay");
            }
            const auto target_cycles = rounded_cycles(delay - before_target);
            delay = before_target
                + period * static_cast<double>(
                    target_cycles + response_cycles);

            if (!is_write) {
                unpack_read(
                    entry.read_data, trans.get_data_ptr(),
                    trans.get_data_length(), shape, enables, enable_length);
            }
            trans.set_response_status(tlm_status_for(entry.resp));

            ++completed;
            last_latency = network_cycles;
            last_latency_by_port[port] = network_cycles;
            latency_sum += network_cycles;
            notify_completion(
                port, trans.get_address(), trans.get_data_length(), is_write,
                network_cycles);
        } catch (...) {
            --in_flight;
            --outstanding_by_port[port];
            slot_available[port]->notify(sc_core::SC_ZERO_TIME);
            throw;
        }

        --in_flight;
        --outstanding_by_port[port];
        slot_available[port]->notify(sc_core::SC_ZERO_TIME);
    }

    bool network_idle() const
    {
        if (in_flight != 0) {
            return false;
        }
        for (const auto& state : nodes) {
            if (!state.requests.empty() || !state.write_waiters.empty()
                || !state.read_waiters.empty() || !state.aw_sources.empty()
                || !state.ar_sources.empty() || !state.writes.empty()
                || !state.serving.empty() || !state.responses.empty()) {
                return false;
            }
        }
        for (unsigned port = 0; port < initiator_count; ++port) {
            if (outstanding_by_port[port] != 0 || !write_hold_off[port].empty()
                || !read_hold_off[port].empty()) {
                return false;
            }
        }
        return true;
    }

    /// Ticks the mesh while there is anything to do.
    ///
    /// A clock-gated wait is legal only when both layers agree:
    ///
    ///   * `network_idle()` proves the TLM adapters own no pending work;
    ///   * `mesh_quiescent()` directly observes both meshes and every chimney.
    ///
    /// An empty mesh with wrapper work is legitimate (for example target
    /// latency after the request flits drained), so equality is deliberately
    /// not required. Wrapper-idle with retained mesh state violates the
    /// adapter-accounting invariant and is asserted before entering the gate.
    void network_thread()
    {
        if (timing_backend == noc_interconnect::timing_mode::fast) {
            // The hierarchy remains present so both backends have one class and
            // one placement/address-map contract, but fast traffic never
            // toggles it. Returning terminates this private clock process and
            // is the source of the host-side speedup.
            rst_n.write(true);
            out_of_reset = true;
            reset_done.notify(sc_core::SC_ZERO_TIME);
            return;
        }

        rst_n.write(false);
        for (unsigned reset_cycle = 0; reset_cycle < 4; ++reset_cycle) {
            step_once();
        }
        rst_n.write(true);
        out_of_reset = true;
        reset_done.notify(sc_core::SC_ZERO_TIME);

        while (true) {
            const bool wrapper_is_idle = network_idle();
            const bool mesh_is_idle = noc->mesh_quiescent();
            if (mesh_is_idle && !wrapper_is_idle) {
                ++mesh_idle_wrapper_busy;
            }
            if (wrapper_is_idle) {
                sc_assert(mesh_is_idle);
            }
            if (wrapper_is_idle && mesh_is_idle) {
                ++clock_gate_count;
                wait(work);
                continue;
            }
            step_once();
        }
    }
};

noc_interconnect::noc_interconnect(
    sc_core::sc_module_name name, unsigned mesh_x, unsigned mesh_y,
    unsigned num_targets, unsigned num_initiators,
    sc_core::sc_time clock_period, unsigned max_outstanding_per_port,
    timing_mode mode)
    : sc_core::sc_module(name)
    , target_socket("target_socket")
    , impl_(nullptr)
{
    // Validated *before* `impl` exists, so a rejected configuration never
    // builds a mesh. Doing it inside `impl`'s constructor body meant the whole
    // router hierarchy had already been created and had to be torn down from a
    // half-built state.
    if (clock_period <= sc_core::SC_ZERO_TIME) {
        throw std::invalid_argument(
            "noc_interconnect: the network clock period must be positive");
    }
    if (num_initiators == 0) {
        throw std::invalid_argument(
            "noc_interconnect: at least one upstream port is required");
    }
    if (num_initiators > impl::max_manager_ports) {
        throw std::invalid_argument(
            "noc_interconnect: the frozen 3-bit AXI ID supports at most "
            "8 upstream ports");
    }
    if (max_outstanding_per_port == 0
        || max_outstanding_per_port
            > default_max_outstanding_per_port) {
        throw std::invalid_argument(
            "noc_interconnect: max_outstanding_per_port must be in 1..32");
    }
    if (mode != timing_mode::detailed && mode != timing_mode::fast) {
        throw std::invalid_argument(
            "noc_interconnect: invalid timing mode");
    }

    impl_ = std::make_unique<impl>("impl", mesh_x, mesh_y, num_targets,
                                   num_initiators, clock_period,
                                   max_outstanding_per_port, mode);

    target_socket.register_b_transport(
        this, &noc_interconnect::b_transport, 0);
    target_socket.register_transport_dbg(
        this, &noc_interconnect::transport_dbg, 0);

    impl_->targets.resize(num_targets);
    for (unsigned slot = 0; slot < num_targets; ++slot) {
        impl_->targets[slot].socket = std::make_unique<initiator_socket>(
            ("target_" + std::to_string(slot)).c_str());
    }

    for (unsigned port = 1; port < num_initiators; ++port) {
        auto socket = std::make_unique<cpu_socket_t>(
            ("cpu_port_" + std::to_string(port)).c_str());
        socket->register_b_transport(
            this, &noc_interconnect::b_transport, static_cast<int>(port));
        socket->register_transport_dbg(
            this, &noc_interconnect::transport_dbg, static_cast<int>(port));
        impl_->extra_ports.push_back(std::move(socket));
    }
}

noc_interconnect::~noc_interconnect() = default;

noc_interconnect::initiator_socket& noc_interconnect::add_target(
    std::uint64_t base, std::uint64_t size, node where, target_kind kind)
{
    // Every socket already exists: SystemC requires ports to be created during
    // module construction, and `add_target` runs afterwards. This fills the
    // next pre-created slot, which is also how `bus_router` works.
    if (impl_->mapped_targets >= impl_->target_capacity) {
        throw std::runtime_error(
            "noc_interconnect: more targets added than the constructor sized");
    }

    // Validate the mapping before consuming a slot. A rejected `add_target`
    // must leave the table exactly as it was, or the next legal call lands in
    // a slot that is already half-filled.
    if (size == 0) {
        throw std::invalid_argument(
            "noc_interconnect: a target region may not be zero-sized");
    }
    if (size - 1 > UINT64_MAX - base) {
        throw std::invalid_argument(
            "noc_interconnect: target region wraps the address space");
    }
    const std::uint64_t last = base + (size - 1);
    for (const auto& existing : impl_->targets) {
        if (!existing.mapped) {
            continue;
        }
        const std::uint64_t existing_last =
            existing.base + (existing.size - 1);
        if (base <= existing_last && existing.base <= last) {
            std::ostringstream message;
            message << "noc_interconnect: target region 0x" << std::hex << base
                    << "..0x" << last << " overlaps 0x" << existing.base
                    << "..0x" << existing_last << std::dec;
            throw std::invalid_argument(message.str());
        }
    }

    // Placement is checked before the slot is consumed, so a rejected call
    // leaves the target table exactly as it was.
    const unsigned node_index = impl_->index_of(where);
    for (unsigned port = 0; port < impl_->initiator_count; ++port) {
        if (impl_->index_of(impl_->initiator_nodes[port]) != node_index) {
            continue;
        }
        std::ostringstream message;
        message << "noc_interconnect: target at 0x" << std::hex << base
                << std::dec << " sits on node " << where.x << ',' << where.y
                << ", which already hosts upstream port " << port
                << ". The router's NoLoopback tie-off makes a self-addressed"
                   " flit undeliverable, so this would hang rather than fail."
                   " Note (0,0) is the documented default for an unplaced port.";
        throw std::runtime_error(message.str());
    }

    auto& entry = impl_->targets[impl_->mapped_targets++];
    entry.base = base;
    entry.size = size;
    entry.node = node_index;
    entry.kind = kind;
    entry.mapped = true;
    return *entry.socket;
}

noc_interconnect::cpu_socket_t& noc_interconnect::cpu_port(unsigned index)
{
    if (index == 0) {
        return target_socket;
    }
    if (index - 1 >= impl_->extra_ports.size()) {
        throw std::out_of_range("noc_interconnect: upstream port out of range");
    }
    return *impl_->extra_ports[index - 1];
}

void noc_interconnect::place_initiator(unsigned index, node where)
{
    if (index >= impl_->initiator_nodes.size()) {
        throw std::out_of_range("noc_interconnect: upstream port out of range");
    }
    if (impl_->started) {
        throw std::runtime_error(
            "noc_interconnect: placement must happen before the first access");
    }
    // Checked against the mapped targets *before* the move is committed, so a
    // rejected placement leaves the port exactly where it was.
    const unsigned node_index = impl_->index_of(where);
    for (const auto& entry : impl_->targets) {
        if (!entry.mapped || entry.node != node_index) {
            continue;
        }
        std::ostringstream message;
        message << "noc_interconnect: upstream port " << index
                << " would move onto node " << where.x << ',' << where.y
                << ", which already hosts the target at 0x" << std::hex
                << entry.base << std::dec
                << ". The router's NoLoopback tie-off makes a self-addressed"
                   " flit undeliverable, so this would hang rather than fail.";
        throw std::runtime_error(message.str());
    }
    impl_->initiator_nodes[index] = where;
}

tlm::tlm_response_status noc_interconnect::tlm_status_for(std::uint8_t resp)
{
    // Preserves which kind of failure it was. Collapsing both error codes to
    // one loses the distinction between "no such address" and "the target
    // refused", which is the only thing telling a caller where to look.
    switch (static_cast<axi_pkg::axi_resp>(resp)) {
    case axi_pkg::axi_resp::okay:
    case axi_pkg::axi_resp::exokay:
        return tlm::TLM_OK_RESPONSE;
    case axi_pkg::axi_resp::decerr:
        return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    case axi_pkg::axi_resp::slverr:
        return tlm::TLM_GENERIC_ERROR_RESPONSE;
    }
    // Unreachable for a two-bit code, but a response the model does not know
    // must not be reported as success.
    return tlm::TLM_GENERIC_ERROR_RESPONSE;
}

std::uint64_t noc_interconnect::elapsed_cycles() const
{
    return impl_->cycle;
}

std::uint64_t noc_interconnect::completed_transactions() const
{
    return impl_->completed;
}

std::uint64_t noc_interconnect::total_latency_cycles() const
{
    return impl_->latency_sum;
}

std::uint64_t noc_interconnect::last_latency_cycles() const
{
    return impl_->last_latency;
}

std::uint64_t noc_interconnect::last_latency_cycles(unsigned port) const
{
    if (port >= impl_->last_latency_by_port.size()) {
        throw std::out_of_range("noc_interconnect: upstream port out of range");
    }
    return impl_->last_latency_by_port[port];
}

unsigned noc_interconnect::outstanding_transactions(unsigned port) const
{
    if (port >= impl_->outstanding_by_port.size()) {
        throw std::out_of_range("noc_interconnect: upstream port out of range");
    }
    return impl_->outstanding_by_port[port];
}

void noc_interconnect::set_completion_observer(completion_observer observer)
{
    impl_->completion_hook = std::move(observer);
}

unsigned noc_interconnect::peak_outstanding_transactions(unsigned port) const
{
    if (port >= impl_->peak_outstanding_by_port.size()) {
        throw std::out_of_range("noc_interconnect: upstream port out of range");
    }
    return impl_->peak_outstanding_by_port[port];
}

noc_interconnect::timing_mode
noc_interconnect::selected_timing_mode() const noexcept
{
    return impl_->timing_backend;
}

noc_interconnect::detailed_counters
noc_interconnect::detailed_counter_snapshot() const
{
    if (impl_->timing_backend != timing_mode::detailed) {
        throw std::logic_error(
            "noc_interconnect: router counters are unavailable in fast mode");
    }
    return {impl_->noc->req_counter_snapshot(),
            impl_->noc->rsp_counter_snapshot()};
}

void noc_interconnect::reset_detailed_counters()
{
    if (impl_->timing_backend != timing_mode::detailed) {
        throw std::logic_error(
            "noc_interconnect: router counters are unavailable in fast mode");
    }
    impl_->noc->reset_counters();
}

bool noc_interconnect::mesh_quiescent() const
{
    if (impl_->timing_backend == timing_mode::fast) {
        return true;
    }
    return impl_->noc->mesh_quiescent();
}

bool noc_interconnect::wrapper_idle() const
{
    return impl_->network_idle();
}

std::uint64_t noc_interconnect::clock_gate_transitions() const
{
    return impl_->clock_gate_count;
}

std::uint64_t noc_interconnect::mesh_quiescent_wrapper_busy_cycles() const
{
    return impl_->mesh_idle_wrapper_busy;
}

std::uint64_t noc_interconnect::mid_half_cycle_request_arrivals() const
{
    return impl_->mid_half_request_arrivals;
}

void noc_interconnect::b_transport(
    int tag, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    impl_->ensure_started();
    const auto port = static_cast<unsigned>(tag);

    if (impl_->timing_backend == timing_mode::detailed) {
        // The caller's annotated time is real time it has already accounted for
        // but not yet spent. The detailed backend spends rather than annotates,
        // so consume it before injection. Fast mode intentionally leaves it
        // untouched and adds its estimate below.
        if (delay > sc_core::SC_ZERO_TIME) {
            sc_core::wait(delay);
            delay = sc_core::SC_ZERO_TIME;
        }

        while (!impl_->out_of_reset) {
            sc_core::wait(impl_->reset_done);
        }
    }

    // ---- TLM generic-payload contract -------------------------------------
    //
    // Checked before anything is touched, and each failure gets the response
    // code that names it. Previously the only check was the address decode:
    // any command that was not a write was treated as a read, byte enables were
    // never looked at, and the streaming width was ignored.
    const auto command = trans.get_command();
    if (command != tlm::TLM_READ_COMMAND && command != tlm::TLM_WRITE_COMMAND) {
        // `TLM_IGNORE_COMMAND` is a legal payload that carries no access. It
        // must not be silently reinterpreted as a read.
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    const unsigned length = trans.get_data_length();
    if (length == 0) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (trans.get_data_ptr() == nullptr) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    // A streaming width shorter than the payload means the address wraps and
    // the same window is written repeatedly. That is a different access pattern
    // from a linear burst and is not modelled, so it is refused rather than
    // quietly flattened. Zero means "unset" in practice; treat it as linear.
    const auto streaming = trans.get_streaming_width();
    if (streaming != 0 && streaming < length) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    const auto* const enables = trans.get_byte_enable_ptr();
    const auto enable_length = trans.get_byte_enable_length();
    if (enables != nullptr && enable_length == 0) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }

    // ---- address decode over the whole range, not just the first byte ------
    const auto address = trans.get_address();
    // `address + length - 1` can wrap. Checked by subtraction first, so the
    // last-byte address below is only ever computed when it exists.
    if (static_cast<std::uint64_t>(length - 1) > UINT64_MAX - address) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    const std::uint64_t last_byte = address + (length - 1);
    const int first_slot = impl_->decode(address);
    if (first_slot < 0 || impl_->decode(last_byte) != first_slot) {
        // Either nothing is mapped there, or the transfer runs off the end of
        // its region into a different one. Checking only the first byte let a
        // transfer straddle a region boundary and be replayed entirely against
        // the first target.
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    const axi_shape shape = shape_of(address, length);

    // `AxLEN` is 8 bits and encodes `beats - 1`, so one burst describes at most
    // `max_burst_beats`. Refused here rather than narrowed: the cast that used
    // to happen turned 257 beats into `AxLEN = 0`, the subordinate returned one
    // beat, and the caller got `TLM_OK_RESPONSE` with most of its buffer
    // untouched.
    if (shape.beats > axi_pkg::max_burst_beats) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    // A read whose beat frame is wider than the request fetches bytes the
    // caller never asked for: a 6-byte read at `+5` also pulls `+0..+4` and
    // `+11..+15`. That is what AXI does, and it is harmless for memory and
    // unsafe for MMIO, where a neighbouring register may clear on read or be
    // too narrow for the widened access.
    //
    // The wrapper cannot tell the two apart, so the target declares itself and
    // this enforces it. Refused here, before injection — the target is never
    // called, which is the half a response-code check alone would not prove.
    if (command == tlm::TLM_READ_COMMAND && shape.size_log2 == 3
        && (address % bus_bytes != 0 || length % bus_bytes != 0)
        && impl_->targets[static_cast<std::size_t>(first_slot)].kind
               != target_kind::memory) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }

    // A full-width transfer is replayed over its whole beat frame, which starts
    // at `beat0_addr` and can reach past `address + length`. Both ends must be
    // in the same region, or the replay would touch a neighbouring target.
    if (shape.size_log2 == 3) {
        // The frame spans `beats * bus_bytes` from `beat0_addr`. Both the
        // multiplication and the addition are checked: a transfer near the top
        // of the address space would otherwise wrap and be compared against a
        // low address, which decodes to some unrelated region.
        const std::uint64_t frame_bytes =
            static_cast<std::uint64_t>(shape.beats) * bus_bytes;
        if (frame_bytes / bus_bytes != shape.beats
            || frame_bytes - 1 > UINT64_MAX - shape.beat0_addr) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        const std::uint64_t frame_end = shape.beat0_addr + (frame_bytes - 1);
        if (impl_->decode(shape.beat0_addr) != first_slot
            || impl_->decode(frame_end) != first_slot) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
    }

    if (impl_->timing_backend == timing_mode::fast) {
        impl_->fast_transport(
            port, first_slot, trans, delay, shape, enables, enable_length);
        return;
    }

    while (impl_->outstanding_by_port[port]
           >= impl_->max_outstanding_per_port) {
        sc_core::wait(*impl_->slot_available[port]);
    }
    ++impl_->outstanding_by_port[port];
    impl_->peak_outstanding_by_port[port] =
        std::max(impl_->peak_outstanding_by_port[port],
                 impl_->outstanding_by_port[port]);

    const bool is_write = command == tlm::TLM_WRITE_COMMAND;
    impl::waiter parked{};
    parked.port = port;
    parked.issued_cycle = impl_->cycle;
    parked.address = address;
    parked.length = length;
    parked.is_write = is_write;

    impl::manager_request request{};
    request.is_write = is_write;
    request.parked = &parked;
    const auto target_node =
        impl_->targets[static_cast<std::size_t>(first_slot)].node;
    request.destination =
        coordinate{target_node % impl_->mesh_x, target_node / impl_->mesh_x};
    if (is_write) {
        request.aw.id = port;  // one AXI ID per upstream port; see the header
        request.aw.addr = address;
        request.aw.len = static_cast<std::uint8_t>(shape.beats - 1);
        request.aw.size = static_cast<std::uint8_t>(shape.size_log2);
        request.aw.burst = 1;
        request.aw_pending = true;
        const auto view = pack_write(
            trans.get_data_ptr(), length, shape, enables, enable_length);
        request.w.reserve(shape.beats);
        for (unsigned beat = 0; beat < shape.beats; ++beat) {
            axi_w_chan w{};
            w.data = view.data[beat];
            w.strb = view.strb[beat];
            w.last = beat + 1 == shape.beats;
            request.w.push_back(w);
        }
    } else {
        request.ar.id = port;
        request.ar.addr = address;
        request.ar.len = static_cast<std::uint8_t>(shape.beats - 1);
        request.ar.size = static_cast<std::uint8_t>(shape.size_log2);
        request.ar.burst = 1;
        request.ar_pending = true;
        // AXI reads carry no strobes; the requested bytes are selected out of
        // the returned lanes when the response arrives.
    }

    const unsigned node_index = impl_->index_of(impl_->initiator_nodes[port]);
    auto& state = impl_->nodes[node_index];
    state.requests.push_back(std::move(request));
    ++impl_->in_flight;
    impl_->work.notify(sc_core::SC_ZERO_TIME);

    while (!parked.complete) {
        sc_core::wait(parked.done);
    }

    if (!is_write) {
        unpack_read(parked.data, trans.get_data_ptr(), length, shape, enables,
                    enable_length);
    }
    trans.set_response_status(tlm_status_for(parked.resp));

    // Time was spent, not annotated: the transaction really walked the mesh.
    delay = sc_core::SC_ZERO_TIME;
}

unsigned int noc_interconnect::transport_dbg(
    int tag, tlm::tlm_generic_payload& trans)
{
    (void)tag;
    // Debug access bypasses the network entirely: it must not consume
    // simulated time or perturb the interconnect's state.
    impl_->ensure_started();
    const int slot = impl_->decode(trans.get_address());
    if (slot < 0) {
        return 0;
    }
    auto& target = impl_->targets[static_cast<std::size_t>(slot)];
    const auto original = trans.get_address();
    trans.set_address(original - target.base);
    const unsigned int served = (*target.socket)->transport_dbg(trans);
    trans.set_address(original);
    return served;
}

} // namespace cdc::components
