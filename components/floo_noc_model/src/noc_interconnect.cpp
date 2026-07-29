// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/noc_interconnect.h"

#include "floo_noc_model/axi_endpoint.hpp"
#include "floo_noc_model/axi_noc.hpp"

#include <algorithm>
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

/// AXI data width in the frozen configuration, in bytes.
constexpr unsigned bus_bytes = 8;

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

/// A TLM payload reduced to what the AXI model carries.
struct beat_view {
    std::vector<std::uint64_t> data;
    std::uint64_t strb = 0;
};

/// Packs a byte buffer into 64-bit beats, and builds the byte-enable pattern.
/// A payload shorter than the bus width is one beat with a partial strobe.
beat_view pack_beats(const unsigned char* bytes, unsigned length)
{
    beat_view view{};
    const unsigned beats = (length + bus_bytes - 1) / bus_bytes;
    view.data.resize(beats, 0);
    for (unsigned index = 0; index < length; ++index) {
        view.data[index / bus_bytes] |=
            static_cast<std::uint64_t>(bytes[index]) << (8 * (index % bus_bytes));
    }
    const unsigned tail = length % bus_bytes;
    view.strb = tail == 0 ? 0xFFull : ((1ull << tail) - 1ull);
    return view;
}

void unpack_beats(
    const std::vector<std::uint64_t>& beats, unsigned char* bytes,
    unsigned length)
{
    for (unsigned index = 0; index < length; ++index) {
        const auto beat = beats[index / bus_bytes];
        bytes[index] =
            static_cast<unsigned char>((beat >> (8 * (index % bus_bytes))) & 0xFF);
    }
}

} // namespace

struct noc_interconnect::impl : public sc_core::sc_module {
    /// One downstream region and the socket that reaches it.
    struct target_entry {
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
        std::vector<std::uint64_t> write_data;
        std::uint64_t strb = 0;
        /// The cycle at which the downstream target's own latency has elapsed,
        /// and the cycle the downstream access was issued.
        std::uint64_t ready_at = 0;
        std::uint64_t served_at = 0;
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
            if (addr >= entry.base && addr < entry.base + entry.size) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    /// Builds the manager endpoints once every target is known. Called lazily
    /// on the first transaction, because `add_target` runs during platform
    /// construction and the address map is only complete afterwards.
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
            if (nodes[entry.node].manager.has_value()) {
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
            entry.write_data = state.subordinate->pending_write_data();
            entry.beats = static_cast<unsigned>(entry.write_data.size());
            state.serving.push_back(std::move(entry));
        } else if (channel == axi_channel::ar) {
            served_request entry{};
            entry.is_write = false;
            entry.requester = node_of(flit.hdr.src_id);
            entry.addr = state.subordinate->pending_read_addr();
            entry.size_log2 = state.subordinate->pending_read_size();
            entry.beats = state.subordinate->pending_read_beats();
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
            entry.resp = 1;  // SLVERR
            entry.read_data.assign(entry.beats, 0);
            entry.ready_at = cycle;
            return;
        }
        auto& target = targets[static_cast<std::size_t>(slot)];

        const unsigned length = entry.beats * (1u << entry.size_log2);
        std::vector<unsigned char> bytes(length, 0);
        if (entry.is_write) {
            unpack_beats(entry.write_data, bytes.data(), length);
        }

        tlm::tlm_generic_payload payload;
        payload.set_command(entry.is_write ? tlm::TLM_WRITE_COMMAND
                                           : tlm::TLM_READ_COMMAND);
        payload.set_address(entry.addr - target.base);
        payload.set_data_ptr(bytes.data());
        payload.set_data_length(length);
        payload.set_streaming_width(length);
        payload.set_byte_enable_ptr(nullptr);
        payload.set_dmi_allowed(false);
        payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        (*target.socket)->b_transport(payload, delay);

        entry.resp = payload.is_response_ok() ? 0 : 1;
        if (!entry.is_write) {
            const auto view = pack_beats(bytes.data(), length);
            entry.read_data = view.data;
            entry.read_data.resize(entry.beats, 0);
        }

        const auto ticks = period > sc_core::SC_ZERO_TIME
            ? static_cast<std::uint64_t>(delay / period)
            : 0;
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
    , impl_(new impl("impl", mesh_x, mesh_y, num_targets, num_initiators,
                     clock_period))
{
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
    std::uint64_t base, std::uint64_t size, node where)
{
    // Every socket already exists: SystemC requires ports to be created during
    // module construction, and `add_target` runs afterwards. This fills the
    // next pre-created slot, which is also how `bus_router` works.
    if (impl_->mapped_targets >= impl_->target_capacity) {
        throw std::runtime_error(
            "noc_interconnect: more targets added than the constructor sized");
    }
    auto& entry = impl_->targets[impl_->mapped_targets++];
    entry.base = base;
    entry.size = size;
    entry.node = impl_->index_of(where);
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
    impl_->initiator_nodes[index] = where;
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

    while (!impl_->out_of_reset) {
        sc_core::wait(impl_->reset_done);
    }

    const auto address = trans.get_address();
    if (impl_->decode(address) < 0) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    // AXI carries the access width in `AxSIZE`, which must be a power of two.
    // Below the bus width that is one narrow beat; at or above it, full-width
    // beats. Anything else would need unaligned-burst modelling, which v0 does
    // not have.
    const unsigned length = trans.get_data_length();
    unsigned size_log2 = 3;
    unsigned beats = 1;
    if (length == 0) {
        trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (length < bus_bytes) {
        if ((length & (length - 1)) != 0) {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }
        size_log2 = 0;
        while ((1u << size_log2) < length) {
            ++size_log2;
        }
    } else {
        if (length % bus_bytes != 0) {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }
        beats = length / bus_bytes;
    }

    while (impl_->port_busy[port]) {
        sc_core::wait(*impl_->port_free[port]);
    }
    impl_->port_busy[port] = true;

    axi_transaction txn{};
    txn.is_write = trans.is_write();
    txn.id = port;  // one AXI ID per upstream port; see the header
    txn.addr = address;

    txn.size_log2 = size_log2;
    if (txn.is_write) {
        const auto view = pack_beats(trans.get_data_ptr(), length);
        txn.data = view.data;
        txn.strb = view.strb;
    } else {
        txn.read_beats = beats;
        txn.strb = 0xFF;
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
        std::vector<unsigned char> bytes(
            static_cast<std::size_t>(beats) * bus_bytes, 0);
        unpack_beats(parked.data, bytes.data(), length);
        std::memcpy(trans.get_data_ptr(), bytes.data(), length);
    }
    trans.set_response_status(parked.resp == 0
                                  ? tlm::TLM_OK_RESPONSE
                                  : tlm::TLM_GENERIC_ERROR_RESPONSE);

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
