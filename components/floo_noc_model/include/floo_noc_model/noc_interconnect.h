// SPDX-License-Identifier: Apache-2.0
//
// TLM-2.0 wrapper that puts the cycle-accurate FlooNoC model behind the same
// interface `cdc::components::bus_router` presents, so a platform can swap one
// for the other.
//
// ## What this is, and what it costs
//
// The network underneath is **cycle accurate and RTL-signed**: routers, input
// and output FIFOs, wormhole arbiters, both chimney directions, the `NoRoB`
// ordering rule, and inter-node timing all match the frozen FlooNoC RTL
// exactly (see `docs/STATUS.md`). That is the point of using it — a
// transaction's latency here is the latency the hardware would take.
//
// It is also why it is slow. A `bus_router` does no work per simulated cycle
// because it does not simulate cycles at all; this module advances a clock and
// evaluates every router in the mesh on every edge. Expect a large slowdown
// against `bus_router` and choose it deliberately.
//
// ## Differences from `bus_router` that a platform must know about
//
//  * **Placement matters.** A flat bus has no geometry. Here every initiator
//    and every target sits on a mesh node, and distance costs cycles. Ports
//    default to node (0,0); call `place_initiator` / pass a node to
//    `add_target` to lay the system out.
//  * **One AXI ID per initiator.** `floo_pkg::ChimneyDefaultCfg` sets
//    `MaxUniqueIds = 1`, which makes the chimney's response metadata a plain
//    in-order FIFO with no ID matching. Each upstream port therefore uses a
//    single AXI ID and its transactions are serialised. That is the frozen
//    configuration's real behaviour, not a modelling shortcut.
//  * **`b_transport` consumes simulated time directly** rather than annotating
//    `delay`. The transaction is walked through the network cycle by cycle, so
//    the time it takes is spent, not estimated. A caller relying on temporal
//    decoupling will find its quantum consumed.
//  * **Bursts are split into beats.** A payload longer than the AXI data width
//    becomes several beats and costs several flits.
//
// ## Not modelled
//
// ATOPs, virtual channels, multicast/collectives, and the narrow-wide network.
// A payload with a byte-enable pattern that is not a contiguous run of set
// bytes is rejected rather than silently mis-modelled.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

class noc_interconnect : public sc_core::sc_module {
public:
    using initiator_socket = tlm_utils::simple_initiator_socket<noc_interconnect>;
    // Tagged, because `b_transport` has to know which upstream port called: the
    // caller's mesh node is what decides where the transaction is injected.
    using cpu_socket_t =
        tlm_utils::simple_target_socket_tagged<noc_interconnect>;

    /// A mesh node. Same field order as the model's `coordinate`.
    struct node {
        unsigned x;
        unsigned y;
    };

    /// Primary upstream port, matching `bus_router::target_socket`.
    cpu_socket_t target_socket;

    /// `mesh_x`/`mesh_y` size the network. `num_targets` and `num_initiators`
    /// match `bus_router`: SystemC requires every socket to exist during
    /// construction, so both pools are sized up front.
    ///
    /// `clock_period` is the network clock. Every cycle of latency the model
    /// reports costs one of these.
    noc_interconnect(
        sc_core::sc_module_name name,
        unsigned mesh_x,
        unsigned mesh_y,
        unsigned num_targets,
        unsigned num_initiators = 1,
        sc_core::sc_time clock_period = sc_core::sc_time(1, sc_core::SC_NS));
    ~noc_interconnect() override;

    /// Map the next target region and return its initiator socket to bind.
    /// `where` places it on the mesh; distance from the requesting initiator is
    /// what the model turns into cycles.
    initiator_socket& add_target(
        std::uint64_t base, std::uint64_t size, node where = node{0, 0});

    /// Upstream port by index: 0 is `target_socket`, 1.. are the extra ports.
    cpu_socket_t& cpu_port(unsigned index);

    /// Place an upstream port on the mesh. Defaults to (0,0).
    void place_initiator(unsigned index, node where);

    /// Cycles the network has advanced. Useful for a platform that wants to
    /// report interconnect time separately from peripheral time.
    std::uint64_t elapsed_cycles() const;

    /// Transactions completed, and the total cycles they spent in the network.
    /// Both are measured, not derived: `noc_counters.hpp` keeps that
    /// distinction and this follows it.
    std::uint64_t completed_transactions() const;
    std::uint64_t total_latency_cycles() const;

    /// Network cycles the most recently completed transaction spent. This is
    /// the interconnect's own share: a target's access latency is charged as a
    /// hold-off at its node and is *not* counted here, so this is what changing
    /// the floorplan actually moves.
    std::uint64_t last_latency_cycles() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;

    void b_transport(
        int port, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(int port, tlm::tlm_generic_payload& trans);
};

} // namespace cdc::components
