// SPDX-License-Identifier: Apache-2.0
//
// TLM-2.0 wrapper that puts the cycle-accurate FlooNoC model behind the same
// interface `cdc::components::bus_router` presents, so a platform can swap one
// for the other.
//
// ## What this is, and what it costs
//
// The network underneath is cycle-stepped, and its blocks are RTL-signed
// **individually**: routers, input and output FIFOs, wormhole arbiters, the
// `NoRoB` ordering rule, and inter-node mesh timing all match the frozen
// FlooNoC RTL exactly (see `docs/STATUS.md`).
//
// Be precise about the chimney, because "both directions are signed" is the
// easy overclaim to make here:
//
//  * **manager request path timing** — signed, 141 cycles;
//  * **subordinate request reception and response generation** — signed,
//    221 cycles;
//  * **manager-side response unpacker** — implemented and unit-tested,
//    **not signed**: `axi_chimney_manager_response` is new and has no RTL
//    cross-check yet. That is Step A-1;
//  * **the complete manager-AXI-to-subordinate-AXI composed path** —
//    **not signed**. The integrated datapath does not even instantiate the
//    timed chimney: it composes the combinational `axi_chimney_pack.hpp` with
//    the abstract `axi_endpoint.hpp` transactors. Step A-3 replaces them.
//
// So a transaction's latency here contains signed mesh timing, but the path as
// a whole is not yet an RTL-equivalent one. Do not quote it as though it were.
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
//  * **One AXI ID per initiator, and one transaction in flight per port** —
//    but these come from different places, and conflating them has caused
//    wrong conclusions about congestion.
//
//    The *ID* is the frozen configuration's doing: `ChimneyDefaultCfg` sets
//    `MaxUniqueIds = 1`, which makes the chimney's response metadata a plain
//    in-order FIFO with no ID matching, so responses must return in request
//    order.
//
//    The *one-in-flight* limit is this wrapper's own: one waiter and one
//    `port_busy` bit per upstream port. `MaxUniqueIds = 1` does **not** impose
//    it — the RTL metadata FIFOs are `MaxTxns = 32` deep. Anyone measuring
//    contention should know that the ceiling they are hitting is here, not in
//    FlooNoC.
//  * **`b_transport` consumes simulated time directly** rather than annotating
//    `delay`. The transaction is walked through the network cycle by cycle, so
//    the time it takes is spent, not estimated. A caller relying on temporal
//    decoupling will find its quantum consumed.
//
//    The full delay contract, because "spends time" alone does not say enough:
//
//     - **An incoming non-zero `delay` is waited out before injection**, then
//       cleared. It is time the caller had accounted for but not yet spent, and
//       this wrapper's convention is that time is spent. It is never discarded.
//     - **`delay` is `SC_ZERO_TIME` on return**, always. Everything the
//       transaction cost has already elapsed.
//     - **A downstream target's annotated delay is rounded up** to the next
//       whole network cycle. A latency shorter than one cycle becomes one
//       cycle, not zero: a timing model may report a target as slower than it
//       claimed, never faster. No sub-cycle residue is carried between
//       transactions.
//     - **A downstream target that blocks with `wait()` instead of annotating
//       `delay` stalls the whole network.** There is one process driving the
//       mesh clock, and a target that suspends inside `b_transport` suspends it
//       too, freezing every other node's traffic for the duration. Targets on
//       this interconnect should annotate their latency rather than wait for
//       it. A target that must block needs its own thread and a decoupled
//       response path, which this wrapper does not provide.
//  * **Bursts are split into beats, and one burst is at most 256 of them.**
//    A payload longer than the AXI data width becomes several beats and costs
//    several flits. `AxLEN` is 8 bits and encodes `beats - 1`, so a single AXI
//    burst cannot describe more than 256; the wrapper does not split a longer
//    payload across bursts, it **refuses** it with `TLM_BURST_ERROR_RESPONSE`
//    before anything is injected, and the target is never called.
//
//    The bus is 8 bytes wide, so the limit is 2048 bytes *of beat frame*, not
//    of payload. The frame starts at the bus-aligned address below the request,
//    so a transfer at a non-zero lane offset reaches the limit sooner: the beat
//    count is `ceil((address % 8 + length) / 8)`. At `+0` the longest accepted
//    payload is 2048 bytes; at `+1` it is 2047; at `+7`, 2041.
//
//    Refusing rather than splitting is deliberate. Splitting needs an ordering
//    rule between the pieces and a way to combine their responses into one, and
//    with `MaxUniqueIds = 1` the ordering is not free to choose. A caller that
//    wants more than 2048 bytes should issue several transactions and decide
//    for itself what a partial failure means.
//
// ## Not modelled
//
// ATOPs, virtual channels, multicast/collectives, and the narrow-wide network.
//
// Byte enables **are** modelled, including non-contiguous patterns: every
// enabled byte becomes a `WSTRB` bit in the lane its address selects, and the
// strobes are carried to the downstream target. An earlier version of this
// comment claimed non-contiguous patterns were rejected; in fact they were
// never inspected at all, and a partial write silently became a full one.
//
// A wrapped streaming transfer (`streaming_width < data_length`) is rejected
// with `TLM_BURST_ERROR_RESPONSE`, and a command other than read or write with
// `TLM_COMMAND_ERROR_RESPONSE`.

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

    /// Whether a target tolerates being read wider than the request.
    ///
    /// AXI reads whole beats. A read that is not bus-aligned, or whose length
    /// is not a whole number of beats, therefore fetches bytes the caller did
    /// not ask for — for a 6-byte read at `+5`, the bytes at `+0..+4` and
    /// `+11..+15` as well.
    ///
    /// For RAM that is harmless and is exactly what real hardware does. For
    /// MMIO it is not: a neighbouring register may clear-on-read, pop a FIFO,
    /// or simply refuse an access it is too narrow for. The wrapper cannot tell
    /// the two apart, so the platform declares it.
    ///
    /// `mmio` is the default because it is the safe answer: a widened read to
    /// an `mmio` target is **refused** with `TLM_BURST_ERROR_RESPONSE` before
    /// anything is injected, and the target is never called. Narrow aligned
    /// reads — 1, 2, 4 or 8 bytes at a natural boundary — are never widened and
    /// are always allowed.
    enum class target_kind {
        mmio,
        memory,
    };

    /// Primary upstream port, matching `bus_router::target_socket`.
    cpu_socket_t target_socket;

    /// `mesh_x`/`mesh_y` size the network. `num_targets` and `num_initiators`
    /// match `bus_router`: SystemC requires every socket to exist during
    /// construction, so both pools are sized up front.
    ///
    /// `clock_period` is the network clock. Every cycle of latency the model
    /// reports costs one of these. It must be positive; a zero or negative
    /// period is refused at construction, because every latency here is counted
    /// in cycles.
    ///
    /// **`mesh_x` and `mesh_y` are not free.** The mesh model takes its
    /// dimensions as template parameters — `floo_mesh` sizes its `sc_vector`s
    /// from them, and it is RTL-signed, so it is not worth reworking for a
    /// runtime dimension. A fixed dispatch covers the useful sizes, and
    /// anything else throws `std::invalid_argument` at construction with a
    /// message naming the file to edit.
    ///
    /// Supported today:
    ///
    /// ```text
    /// 2x2   3x3   4x4   4x2   2x4
    /// ```
    ///
    /// To add one, add a line to `make_mesh()` in `src/noc_interconnect.cpp`.
    /// That is a one-line change and costs one template instantiation.
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
    /// `kind` declares whether widened reads are safe here; see `target_kind`.
    initiator_socket& add_target(
        std::uint64_t base, std::uint64_t size, node where = node{0, 0},
        target_kind kind = target_kind::mmio);

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

    /// The AXI-to-TLM response mapping, exactly as `b_transport` uses it.
    ///
    /// Exposed as a named function so a test can assert every input against its
    /// exact output. When the mapping lived inline in the completion path the
    /// only way to reach it was to provoke a real failure end to end, which
    /// covered two of the four codes and left the production `switch` free to
    /// drift from whatever a test believed.
    ///
    /// ```text
    /// OKAY   -> TLM_OK_RESPONSE
    /// EXOKAY -> TLM_OK_RESPONSE             an exclusive access that succeeded
    /// SLVERR -> TLM_GENERIC_ERROR_RESPONSE  the target refused
    /// DECERR -> TLM_ADDRESS_ERROR_RESPONSE  nothing decoded
    /// ```
    static tlm::tlm_response_status tlm_status_for(std::uint8_t axi_resp);

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
