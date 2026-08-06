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
//  * **manager-side response unpacker** — signed, 78 cycles
//    (`axi_chimney_manager_response`, Step A-1). That completes all four
//    chimney quadrants;
//  * **the complete manager-AXI-to-subordinate-AXI composed path** — the
//    integrated datapath now instantiates all those timed blocks through
//    `axi_noc` (Step A-3), but there is no one-piece RTL harness for the
//    composition. Each hardware block is signed separately; the TLM-to-AXI
//    adapters above the manager/subordinate signal boundaries have no RTL
//    counterpart and are covered by model-level integration tests.
//
// So a transaction's latency here traverses the signed timing blocks, but the
// path as a whole is not one monolithic RTL equivalence proof. Do not quote it
// as though it were.
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
//  * **One AXI ID per initiator, with bounded concurrent transactions per
//    port** — these are separate policy choices.
//
//    The *ID* is the frozen configuration's doing: `ChimneyDefaultCfg` sets
//    `MaxUniqueIds = 1`, which makes the chimney's response metadata a plain
//    in-order FIFO with no ID matching, so responses must return in request
//    order.
//
//    The wrapper therefore keeps independent FIFO completion queues for B and
//    R and admits up to `max_outstanding_per_port` calls on one upstream port.
//    The default and hard maximum are 32, matching the frozen metadata FIFO
//    depth. A smaller constructor value is useful when a platform deliberately
//    wants tighter back-pressure. Increasing this bound past 32, or allowing
//    out-of-order response matching, requires a new FlooNoC configuration and
//    an RTL cross-check of the `MaxUniqueIds > 1` metadata path.
//  * **Timing is a construction-time policy.** `timing_mode::detailed` walks
//    the signed signal-driven network cycle by cycle and spends simulated time.
//    `timing_mode::fast` uses the same payload checks and downstream replay but
//    annotates a calibrated no-contention estimate instead.
//
//    The detailed delay contract, because "spends time" alone does not say
//    enough:
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
//
//    The fast delay contract is deliberately different:
//
//     - **The interconnect itself waits for neither incoming nor newly
//       estimated time.** The incoming delay is preserved, the calibrated
//       request delay is presented to the target, and target plus response
//       latency are returned in `delay`. This LT contract requires downstream
//       targets to annotate latency too. A target that calls `wait()` inside
//       its own `b_transport` will still advance global time and is therefore
//       incompatible with the fast backend.
//     - **The estimate is a no-contention model:** four cycles per Manhattan
//       hop plus six fixed cycles, with one extra cycle per write-data beat or
//       per read beat after the first. The constants are pinned against the
//       detailed 10-cycle one-hop and 30-cycle six-hop calibration points.
//     - **Target delay is rounded up per access** exactly as in detailed mode.
//     - **Contention, link back-pressure, router locks and clock-gating
//       activity are not estimated.** Use detailed mode for those questions.
//       Fast mode preserves blocking-call order and functional target effects,
//       but its latency must not be described as cycle accurate.
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

#include "floo_noc_model/noc_counters.hpp"

#include <cstdint>
#include <functional>
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

    /// Selects the timing backend without changing the socket or address-map
    /// interface.
    enum class timing_mode {
        detailed,
        fast,
    };

    /// Frozen `ChimneyDefaultCfg.MaxTxns`. The wrapper uses a conservative
    /// combined read/write admission bound of this size per upstream port.
    static constexpr unsigned default_max_outstanding_per_port = 32;

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
    /// To add one, add a line to `make_noc()` in `src/noc_interconnect.cpp`.
    /// That is a one-line change and costs one template instantiation.
    ///
    /// `num_initiators` must be 1..8. The wrapper assigns one AXI ID per
    /// upstream port and the frozen chimney's manager ID is 3 bits; refusing a
    /// ninth port avoids silent ID truncation and ordering-counter aliasing.
    ///
    /// `max_outstanding_per_port` must be 1..32. In detailed mode it bounds concurrent
    /// `b_transport` calls admitted on one tagged upstream socket. Calls above
    /// the bound wait for a slot; they are not dropped or assigned a new AXI
    /// ID. Read completions remain FIFO among reads and write completions FIFO
    /// among writes, as required by the frozen `MaxUniqueIds = 1` branch.
    ///
    /// `mode` defaults to `detailed` for source compatibility. Fast mode is the
    /// long-run LT backend: it invokes the same mapped target and reports the
    /// same TLM response/data effects, but returns an annotated no-contention
    /// estimate and never injects a flit into the cycle-stepped mesh.
    noc_interconnect(
        sc_core::sc_module_name name,
        unsigned mesh_x,
        unsigned mesh_y,
        unsigned num_targets,
        unsigned num_initiators = 1,
        sc_core::sc_time clock_period = sc_core::sc_time(1, sc_core::SC_NS),
        unsigned max_outstanding_per_port =
            default_max_outstanding_per_port,
        timing_mode mode = timing_mode::detailed);
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

    /// Per-upstream-port form of `last_latency_cycles()`.
    ///
    /// The unqualified form is a convenient global "most recent completion"
    /// metric. Under concurrent traffic it can be overwritten by another
    /// manager in the same delta cycle; this indexed form preserves requester
    /// ownership and is the one a multi-initiator scoreboard should use.
    std::uint64_t last_latency_cycles(unsigned port) const;

    /// Current and peak admitted calls for one upstream port. The peak is a
    /// diagnostic proving whether a workload actually exercised concurrency;
    /// it never exceeds the constructor's admission bound.
    unsigned outstanding_transactions(unsigned port) const;
    unsigned peak_outstanding_transactions(unsigned port) const;

    /// One completed transaction, reported where its latency becomes final.
    ///
    /// This exists because `last_latency_cycles()` cannot attribute anything
    /// under concurrent traffic: it holds only the most recent completion, so
    /// a platform monitor that polls it later may read a different manager's
    /// transaction. Sampling at the completion point removes the race, and
    /// carrying the address and command lets the observer classify without the
    /// interconnect having to know what any particular register means.
    struct completion {
        /// Upstream port that issued it.
        unsigned port = 0;
        /// Address as presented by the caller, before any beat alignment.
        std::uint64_t address = 0;
        /// Payload length in bytes, as presented by the caller.
        unsigned length = 0;
        bool is_write = false;
        /// Network cycles only. A target's own access latency is charged as a
        /// hold-off at its node and is excluded, exactly as for
        /// `last_latency_cycles()`.
        std::uint64_t latency_cycles = 0;
        /// Network cycle at which it completed. Zero in fast mode, which does
        /// not advance a network clock.
        std::uint64_t at_cycle = 0;
    };

    using completion_observer = std::function<void(const completion&)>;

    /// Install a passive observer, or an empty function to remove one.
    ///
    /// The observer is called synchronously from the completion path, so it
    /// must be cheap, must not throw, and must not call anything on this
    /// object other than a `const` accessor. In particular it must never
    /// `wait()`: the model is mid-completion and consuming time there would
    /// change the very numbers being measured.
    ///
    /// It fires in both timing modes. In fast mode the latency reported is the
    /// no-contention estimate, which is what fast mode computes; it is not a
    /// measurement, and anything derived from it must not be called one.
    void set_completion_observer(completion_observer observer);

    /// The construction-time backend. It never changes during simulation.
    timing_mode selected_timing_mode() const noexcept;

    /// Passive measured counters from both physical meshes.
    ///
    /// FlooNoC carries requests and responses on separate fabrics over the
    /// same coordinates, so combining them here would hide which direction
    /// was congested. The snapshots retain one router record per node in
    /// row-major order and the canonical RTL port order
    /// North/East/South/West/Eject.
    struct detailed_counters {
        floo::model::mesh_counter_snapshot request;
        floo::model::mesh_counter_snapshot response;
    };

    /// Snapshot/reset the detailed mesh's passive counters.
    ///
    /// Fast mode never clocks or traverses the mesh, so accepting a fast-mode
    /// snapshot as measurement would manufacture zeros that look real. Both
    /// functions therefore throw `std::logic_error` in fast mode.
    detailed_counters detailed_counter_snapshot() const;
    void reset_detailed_counters();

    /// Passive clock-gating diagnostics.
    ///
    /// `mesh_quiescent()` includes both physical meshes and every chimney:
    /// router input/output FIFO occupancy, route/arbiter locks, live endpoint
    /// valids, metadata FIFOs and manager-side RoB counters.
    ///
    /// `wrapper_idle()` is intentionally separate. The mesh may be empty while
    /// a request waits in the TLM adapter or for target latency; production
    /// gating requires both, not equality between them on every cycle.
    bool mesh_quiescent() const;
    bool wrapper_idle() const;
    std::uint64_t clock_gate_transitions() const;
    std::uint64_t mesh_quiescent_wrapper_busy_cycles() const;
    std::uint64_t mid_half_cycle_request_arrivals() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;

    void b_transport(
        int port, tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(int port, tlm::tlm_generic_payload& trans);
};

} // namespace cdc::components
