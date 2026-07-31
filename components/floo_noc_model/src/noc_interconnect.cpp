// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/noc_interconnect.h"

#include "floo_noc_model/axi_endpoint.hpp"
#include "floo_noc_model/axi_lanes.hpp"
#include "floo_noc_model/axi_noc.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <optional>
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

/// Runtime-sized access to a compile-time-sized mesh.
///
/// `axi_noc` takes its dimensions as template parameters because `floo_mesh`
/// sizes its `sc_vector`s from them, and `floo_mesh` is RTL-signed — it is not
/// worth reworking to gain a runtime dimension. A small dispatch covers the
/// useful sizes instead.
struct mesh_iface {
    virtual ~mesh_iface() = default;
    virtual void bind_clock(
        sc_core::sc_signal<bool>& clk, sc_core::sc_signal<bool>& rst_n) = 0;
    virtual unsigned node_index(unsigned x, unsigned y) const = 0;
    virtual unsigned node_count() const = 0;
    virtual network_port<axi_req_flit>& req(unsigned node) = 0;
    virtual network_port<axi_rsp_flit>& rsp(unsigned node) = 0;
};

template <unsigned Width, unsigned Height>
struct mesh_holder final : mesh_iface {
    axi_noc<Width, Height> noc;

    explicit mesh_holder(const char* name) : noc(name) {}

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
    network_port<axi_req_flit>& req(unsigned node) override
    {
        return noc.req(node);
    }
    network_port<axi_rsp_flit>& rsp(unsigned node) override
    {
        return noc.rsp(node);
    }
};

std::unique_ptr<mesh_iface> make_mesh(unsigned x, unsigned y, const char* name)
{
    if (x == 2 && y == 2) return std::make_unique<mesh_holder<2, 2>>(name);
    if (x == 3 && y == 3) return std::make_unique<mesh_holder<3, 3>>(name);
    if (x == 4 && y == 4) return std::make_unique<mesh_holder<4, 4>>(name);
    if (x == 4 && y == 2) return std::make_unique<mesh_holder<4, 2>>(name);
    if (x == 2 && y == 4) return std::make_unique<mesh_holder<2, 4>>(name);

    std::ostringstream message;
    message << "noc_interconnect: mesh " << x << 'x' << y
            << " is not instantiated. Add it to make_mesh() in "
               "src/noc_interconnect.cpp; the dimensions are template "
               "parameters of the signed mesh model.";
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
        /// Resolved once, in `absorb_request`, where the original AXI address
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

    /// Per-node state. A node may host a manager, a subordinate, or both.
    struct node_state {
        std::optional<axi_manager_endpoint> manager;
        std::optional<axi_subordinate_endpoint> subordinate;
        /// Which upstream port injects here, if any.
        int initiator_port = -1;
        std::deque<axi_rsp_flit> outbox;
        std::deque<served_request> serving;
    };

    /// A `b_transport` call parked until the network answers it.
    struct waiter {
        sc_core::sc_event done;
        bool complete = false;
        std::vector<std::uint64_t> data;
        std::uint8_t resp = 0;
        std::uint64_t issued_cycle = 0;
    };

    sc_core::sc_signal<bool> clk{"clk"};
    sc_core::sc_signal<bool> rst_n{"rst_n"};

    std::unique_ptr<mesh_iface> mesh;
    sc_core::sc_time period;

    std::vector<target_entry> targets;
    std::vector<node_state> nodes;
    std::vector<std::unique_ptr<cpu_socket_t>> extra_ports;
    std::vector<node> initiator_nodes;
    std::vector<std::unique_ptr<waiter>> waiters;
    std::vector<bool> port_busy;
    // `sc_event` is neither copyable nor movable, so it cannot live in a
    // plain vector that is assigned into.
    std::vector<std::unique_ptr<sc_core::sc_event>> port_free;

    unsigned mesh_x = 0;
    unsigned mesh_y = 0;
    unsigned initiator_count = 0;
    unsigned target_capacity = 0;
    unsigned mapped_targets = 0;

    std::uint64_t cycle = 0;
    std::uint64_t completed = 0;
    std::uint64_t latency_sum = 0;
    std::uint64_t last_latency = 0;
    unsigned in_flight = 0;
    /// Target access latency accumulated per requesting node, so it can be
    /// removed from that node's network-latency figure.
    std::vector<std::uint64_t> hold_off;
    bool started = false;
    /// The mesh needs its reset to elapse before it will carry anything. A
    /// `b_transport` issued at time zero would otherwise hand flits to a
    /// network still in reset, which swallows them.
    bool out_of_reset = false;
    sc_core::sc_event reset_done;
    sc_core::sc_event work;

    SC_HAS_PROCESS(impl);

    impl(sc_core::sc_module_name name, unsigned x, unsigned y,
         unsigned num_targets, unsigned num_initiators, sc_core::sc_time tick)
        : sc_core::sc_module(name)
        , mesh(make_mesh(x, y, "mesh"))
        , period(tick)
        , mesh_x(x)
        , mesh_y(y)
        , initiator_count(num_initiators)
        , target_capacity(num_targets)
    {
        if (num_initiators == 0) {
            throw std::invalid_argument(
                "noc_interconnect: at least one upstream port is required");
        }
        // The public constructor checks the period before this object is
        // created; this is the belt-and-braces copy for a direct construction.
        if (tick <= sc_core::SC_ZERO_TIME) {
            throw std::invalid_argument(
                "noc_interconnect: the network clock period must be positive");
        }
        mesh->bind_clock(clk, rst_n);
        nodes.resize(mesh->node_count());
        hold_off.assign(mesh->node_count(), 0);
        initiator_nodes.assign(num_initiators, node{0, 0});
        waiters.reserve(num_initiators);
        for (unsigned port = 0; port < num_initiators; ++port) {
            waiters.push_back(std::make_unique<waiter>());
        }
        port_busy.assign(num_initiators, false);
        for (unsigned port = 0; port < num_initiators; ++port) {
            port_free.push_back(std::make_unique<sc_core::sc_event>());
        }

        SC_THREAD(network_thread);
    }

    unsigned node_of(const coordinate& id) const
    {
        return mesh->node_index(id.x.to_uint(), id.y.to_uint());
    }

    unsigned index_of(node where) const
    {
        if (where.x >= mesh_x || where.y >= mesh_y) {
            throw std::out_of_range("noc_interconnect: node outside the mesh");
        }
        return mesh->node_index(where.x, where.y);
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

    /// Builds the manager endpoints once every target is known. Called lazily
    /// on the first transaction, because `add_target` runs during platform
    /// construction and the address map is only complete afterwards.
    /// No target may share a node with an upstream port.
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

        std::vector<endpoint_region> regions;
        regions.reserve(targets.size());
        for (const auto& entry : targets) {
            if (!entry.mapped) {
                continue;
            }
            regions.push_back(endpoint_region{
                entry.base, entry.size,
                coordinate{entry.node % mesh_x, entry.node / mesh_x}});
        }
        reference_address_map map{regions};

        for (unsigned port = 0; port < initiator_count; ++port) {
            const unsigned index = index_of(initiator_nodes[port]);
            auto& state = nodes[index];
            if (state.manager.has_value()) {
                throw std::runtime_error(
                    "noc_interconnect: two upstream ports on one mesh node");
            }
            const coordinate id{initiator_nodes[port].x,
                                initiator_nodes[port].y};
            state.manager.emplace(id, chimney_destination{map});
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
            if (!state.subordinate.has_value()) {
                state.subordinate.emplace(
                    coordinate{entry.node % mesh_x, entry.node / mesh_x});
            }
        }
    }

    /// Advances the network one clock. Injects, samples, and applies the
    /// handshakes, mirroring the discipline the cross-check harnesses use.
    void step_once()
    {
        clk.write(false);

        // What was actually offered to the mesh this cycle. It has to be
        // remembered rather than re-read after the wait below: `b_transport`
        // runs in the caller's process and can push a new request into a
        // manager while this thread is suspended. Re-reading `has_request()`
        // then would pop a flit that was never driven onto `inject_valid`, and
        // it would vanish. That only shows with two managers on the mesh — one
        // manager always offers while this thread is idle.
        std::vector<bool> drove_request(nodes.size(), false);
        std::vector<bool> drove_response(nodes.size(), false);

        for (unsigned index = 0; index < nodes.size(); ++index) {
            auto& state = nodes[index];
            auto& req_port = mesh->req(index);
            auto& rsp_port = mesh->rsp(index);

            const bool has_request =
                state.manager.has_value() && state.manager->has_request();
            if (has_request) {
                req_port.inject_data.write(state.manager->peek_request());
            }
            req_port.inject_valid.write(has_request);
            req_port.eject_ready.write(state.subordinate.has_value());
            drove_request[index] = has_request;

            const bool has_response = !state.outbox.empty();
            if (has_response) {
                rsp_port.inject_data.write(state.outbox.front());
            }
            rsp_port.inject_valid.write(has_response);
            rsp_port.eject_ready.write(state.manager.has_value());
            drove_response[index] = has_response;
        }

        wait(period / 2);

        struct sampled {
            bool request_accepted = false;
            bool response_accepted = false;
            bool request_arrived = false;
            axi_req_flit request{};
            bool response_arrived = false;
            axi_rsp_flit response{};
        };
        std::vector<sampled> observed(nodes.size());
        for (unsigned index = 0; index < nodes.size(); ++index) {
            auto& state = nodes[index];
            auto& req_port = mesh->req(index);
            auto& rsp_port = mesh->rsp(index);
            auto& sample = observed[index];
            (void)state;

            sample.request_accepted = rst_n.read() && drove_request[index]
                && req_port.inject_ready.read();
            sample.response_accepted = rst_n.read() && drove_response[index]
                && rsp_port.inject_ready.read();
            sample.request_arrived =
                state.subordinate.has_value() && req_port.eject_valid.read();
            sample.request = req_port.eject_data.read();
            sample.response_arrived =
                state.manager.has_value() && rsp_port.eject_valid.read();
            sample.response = rsp_port.eject_data.read();
        }

        clk.write(true);
        wait(period / 2);
        ++cycle;

        for (unsigned index = 0; index < nodes.size(); ++index) {
            auto& state = nodes[index];
            const auto& sample = observed[index];

            if (sample.request_accepted) {
                state.manager->take_request();
            }
            if (sample.response_accepted) {
                state.outbox.pop_front();
            }
            if (sample.request_arrived) {
                absorb_request(state, sample.request);
            }
            if (sample.response_arrived) {
                absorb_response(state, sample.response);
            }
        }

        for (auto& state : nodes) {
            serve_ready_requests(state);
        }
    }

    /// A request flit reached a subordinate node.
    void absorb_request(node_state& state, const axi_req_flit& flit)
    {
        state.subordinate->accept_request(flit);
        const auto channel =
            static_cast<axi_channel>(flit.hdr.axi_ch.to_uint());

        if (channel == axi_channel::w && flit.hdr.last) {
            served_request entry{};
            entry.is_write = true;
            entry.requester = node_of(flit.hdr.src_id);
            entry.addr = state.subordinate->pending_write_addr();
            entry.size_log2 = state.subordinate->pending_write_size();
            const auto& beat_data = state.subordinate->pending_write_data();
            const auto& beat_strb = state.subordinate->pending_write_strbs();
            entry.beats = static_cast<unsigned>(beat_data.size());

            // Resolve every enabled lane to its absolute byte address, here and
            // only here. `aw_addr` is the AXI address as issued; the beats are
            // numbered from its bus-aligned base.
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
                    if (((beat_strb[beat] >> lane) & 1ull) == 0) {
                        continue;
                    }
                    // The frame cannot wrap the address space: a transfer that
                    // did would have been refused upstream, and reconstructing
                    // it here would produce addresses below the AW.
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
                             (beat_data[beat] >> (8 * lane)) & 0xFF)});
                }
            }

            if (located.empty()) {
                // Every lane disabled. A legal AXI write that changes nothing,
                // and it must stay side-effect free downstream.
                entry.byte_length = 0;
            } else {
                std::uint64_t lowest = located.front().address;
                std::uint64_t highest = located.front().address;
                for (const auto& item : located) {
                    lowest = std::min(lowest, item.address);
                    highest = std::max(highest, item.address);
                }
                entry.addr = lowest;
                entry.byte_length =
                    static_cast<unsigned>(highest - lowest + 1);
                entry.write_bytes.assign(entry.byte_length, 0);
                entry.write_enables.assign(entry.byte_length, 0);
                for (const auto& item : located) {
                    const auto index =
                        static_cast<std::size_t>(item.address - lowest);
                    entry.write_bytes[index] = item.value;
                    entry.write_enables[index] = TLM_BYTE_ENABLED;
                }
            }
            state.serving.push_back(std::move(entry));
        } else if (channel == axi_channel::ar) {
            served_request entry{};
            entry.is_write = false;
            entry.requester = node_of(flit.hdr.src_id);
            entry.addr = state.subordinate->pending_read_addr();
            entry.size_log2 = state.subordinate->pending_read_size();
            entry.beats = state.subordinate->pending_read_beats();
            // A read has no strobes: AXI expresses its length as beats times
            // `ARSIZE`, and a master wanting fewer bytes reads the whole beat
            // and uses part of it. The wrapper trims at the initiator.
            entry.byte_length = entry.beats * (1u << entry.size_log2);
            state.serving.push_back(std::move(entry));
        }
    }

    /// A response flit reached a manager node.
    void absorb_response(node_state& state, const axi_rsp_flit& flit)
    {
        const auto done = state.manager->accept_response(flit);
        if (state.manager->response_incomplete()) {
            return;  // an intermediate R beat
        }
        const int port = state.initiator_port;
        if (port < 0) {
            return;
        }
        auto& parked = *waiters[static_cast<unsigned>(port)];
        parked.data = done.data;
        parked.resp = done.resp;
        parked.complete = true;
        ++completed;
        const auto elapsed = cycle - parked.issued_cycle;
        const auto index = static_cast<std::size_t>(
            index_of(initiator_nodes[static_cast<unsigned>(port)]));
        const auto charged = index < hold_off.size() ? hold_off[index] : 0;
        last_latency = elapsed > charged ? elapsed - charged : 0;
        if (index < hold_off.size()) {
            hold_off[index] = 0;
        }
        latency_sum += last_latency;
        if (in_flight > 0) {
            --in_flight;
        }
        parked.done.notify(sc_core::SC_ZERO_TIME);
    }

    /// Runs the downstream TLM access for requests whose target latency has
    /// elapsed, then queues the response flits.
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
        // interconnect's. Charge it to the requester that caused it, keyed by
        // the `src_id` the request carried. Attributing it to every waiting
        // transaction — as an earlier version did — underflows the moment more
        // than one is in flight.
        if (entry.requester < hold_off.size()) {
            hold_off[entry.requester] +=
                entry.ready_at > entry.served_at
                    ? entry.ready_at - entry.served_at
                    : 0;
        }

        if (entry.is_write) {
            state.outbox.push_back(state.subordinate->respond_write(entry.resp));
        } else {
            for (auto& flit :
                 state.subordinate->respond_read_burst(entry.read_data,
                                                       entry.resp)) {
                state.outbox.push_back(flit);
            }
        }
        state.serving.pop_front();
    }

    /// Issues the real TLM transaction to the mapped peripheral.
    ///
    /// The delay the peripheral annotates is turned into a per-node hold-off in
    /// cycles rather than waited on here: waiting inside the network thread
    /// would freeze every other node's traffic for the duration, which a real
    /// subordinate does not do.
    void perform_downstream(served_request& entry)
    {
        const int slot = decode(entry.addr);
        if (slot < 0) {
            // Nothing decoded, so nothing was ever reached: that is `DECERR`,
            // the decode error, not `SLVERR`. An earlier version wrote the
            // literal `1` here and called it SLVERR in a comment; `1` is
            // `EXOKAY`, so a failed decode was reported upstream as success.
            entry.resp = to_bits(axi_pkg::axi_resp::decerr);
            entry.read_data.assign(entry.beats, 0);
            entry.ready_at = cycle;
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
            entry.ready_at = cycle;
            return;
        }
        // Already resolved to absolute addresses in `absorb_request`. Nothing
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

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
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

        // Round **up** to the first cycle on which the response may legally
        // appear. Truncating turned any latency shorter than one network cycle
        // into no latency at all, so a target annotating 0.4 cycles answered as
        // if it were free. A conservative ceiling is the right bias for a
        // timing model: it never claims a target is faster than it said.
        //
        // No sub-cycle residue is carried between transactions. This target
        // model is not pipelined here, so there is nothing for a residue to
        // accumulate into; the ceiling is applied per access and documented as
        // such in the header.
        const double cycles = delay / period;
        auto ticks = static_cast<std::uint64_t>(cycles);
        if (static_cast<double>(ticks) < cycles) {
            ++ticks;
        }
        entry.ready_at = cycle + ticks;
    }

    bool network_idle() const
    {
        if (in_flight != 0) {
            return false;
        }
        for (const auto& state : nodes) {
            if (!state.outbox.empty() || !state.serving.empty()) {
                return false;
            }
            if (state.manager.has_value() && state.manager->has_request()) {
                return false;
            }
        }
        return true;
    }

    /// Ticks the mesh while there is anything to do.
    ///
    /// An idle network is skipped rather than clocked. That is exact, not an
    /// approximation: with no `valid` asserted anywhere, every register in the
    /// mesh holds its value, so a skipped cycle changes nothing. It also keeps
    /// a mostly-idle platform from paying for the interconnect.
    void network_thread()
    {
        rst_n.write(false);
        for (unsigned reset_cycle = 0; reset_cycle < 4; ++reset_cycle) {
            step_once();
        }
        rst_n.write(true);
        out_of_reset = true;
        reset_done.notify(sc_core::SC_ZERO_TIME);

        while (true) {
            if (network_idle()) {
                wait(work);
            }
            step_once();
        }
    }
};

noc_interconnect::noc_interconnect(
    sc_core::sc_module_name name, unsigned mesh_x, unsigned mesh_y,
    unsigned num_targets, unsigned num_initiators,
    sc_core::sc_time clock_period)
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

    impl_ = std::make_unique<impl>("impl", mesh_x, mesh_y, num_targets,
                                   num_initiators, clock_period);

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

void noc_interconnect::b_transport(
    int tag, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    impl_->ensure_started();
    const auto port = static_cast<unsigned>(tag);

    // The caller's annotated time is real time it has already accounted for but
    // not yet spent. This wrapper does not annotate — it spends — so the two
    // conventions are reconciled here by consuming it before the transaction
    // enters the network. Dropping it, as an earlier version did, silently made
    // every temporally decoupled caller's transaction start too early.
    if (delay > sc_core::SC_ZERO_TIME) {
        sc_core::wait(delay);
        delay = sc_core::SC_ZERO_TIME;
    }

    while (!impl_->out_of_reset) {
        sc_core::wait(impl_->reset_done);
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

    while (impl_->port_busy[port]) {
        sc_core::wait(*impl_->port_free[port]);
    }
    impl_->port_busy[port] = true;

    axi_transaction txn{};
    txn.is_write = command == tlm::TLM_WRITE_COMMAND;
    txn.id = port;  // one AXI ID per upstream port; see the header
    txn.addr = address;
    txn.size_log2 = shape.size_log2;
    if (txn.is_write) {
        const auto view = pack_write(
            trans.get_data_ptr(), length, shape, enables, enable_length);
        txn.data = view.data;
        txn.strb = view.strb;
    } else {
        txn.read_beats = shape.beats;
        // AXI reads carry no strobes; the requested bytes are selected out of
        // the returned lanes when the response arrives.
        txn.strb.clear();
    }

    auto& parked = *impl_->waiters[port];
    parked.complete = false;
    parked.issued_cycle = impl_->cycle;

    const unsigned node_index = impl_->index_of(impl_->initiator_nodes[port]);
    auto& state = impl_->nodes[node_index];

    // The ordering gate can refuse: with `MaxUniqueIds = 1` an ID in flight to
    // another destination is serialised. Retry on the next network cycle.
    while (!state.manager->offer(txn)) {
        impl_->work.notify(sc_core::SC_ZERO_TIME);
        sc_core::wait(impl_->period);
    }
    ++impl_->in_flight;
    impl_->work.notify(sc_core::SC_ZERO_TIME);

    while (!parked.complete) {
        sc_core::wait(parked.done);
    }

    if (!txn.is_write) {
        unpack_read(parked.data, trans.get_data_ptr(), length, shape, enables,
                    enable_length);
    }
    trans.set_response_status(tlm_status_for(parked.resp));

    impl_->port_busy[port] = false;
    impl_->port_free[port]->notify(sc_core::SC_ZERO_TIME);

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
