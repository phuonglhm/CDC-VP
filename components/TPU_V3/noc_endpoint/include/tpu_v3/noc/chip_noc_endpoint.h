// SPDX-License-Identifier: Apache-2.0
//
// The chip's NoC endpoint — the only TPU_V3 component that talks to
// `noc_interconnect` (plan §11.10, `INTERFACE_CONTRACT.md` §7).
//
//   tpu_chip.external() ──> from_chip ──[chunking]──> to_noc ──> noc port
//   tpu_chip.inbound()  <── to_chip   <──[chunking]── from_noc <── chip aperture
//
// It exists to absorb the constraints the interconnect imposes, so that nothing
// above it has to know them. Two of those constraints are hard limits that
// produce a *refusal* rather than a slow path, which is why an adapter is
// needed at all:
//
// **The burst limit.** The bus is 8 bytes wide and one AXI burst is at most 256
// beats, so the largest frame is 2048 bytes at bus alignment and fewer at an
// offset — `ceil((address % 8 + length) / 8) <= 256`. `noc_interconnect`
// **refuses** a longer payload rather than splitting it, because with
// `MaxUniqueIds = 1` the ordering between the pieces is not free to choose
// (decision record D23, freeze 3). Choosing it is this component's job.
//
// **The inbound transfer limit.** A NEO-CORE's external bridge refuses an
// inbound SRAM access longer than `sram::neo_max_transfer_bytes`. A remote
// master may legitimately write a kilobyte into a core's SRAM, so the endpoint
// splits it on the way in as well. MMIO is deliberately *not* split: AXI4-Lite
// is 4 bytes and a wider access is an error the control plane must report, not
// something to quietly turn into several registers written in sequence.
//
// ## The chunking contract, as `INTERFACE_CONTRACT.md` §7 states it
//
//  * chunks are bus-aligned **except possibly the first**, so a transfer at a
//    lane offset pays for its offset once rather than on every chunk;
//  * chunks are issued in **ascending address order**;
//  * the **first failing chunk stops** the transfer and its status is returned;
//  * bytes already transferred **stay transferred**, and the completion reports
//    how many. A partially completed transfer is not rolled back — there is
//    nothing to roll it back with, and pretending otherwise would be worse;
//  * every chunk is attributed in metrics.
//
// ## What this component cannot attribute, stated rather than faked
//
// A chip presents **one** aggregated manager to the mesh (plan §4.4), so the
// endpoint sees one socket and cannot tell core 0's traffic from core 1's. That
// is by design and not a gap: `chip_local_fabric` holds the per-core
// attribution, one level down. The counters here are per direction and per
// transfer; none of them is a per-core figure and none may be presented as one.
//
// ## Local containment
//
// An address inside this chip's own aperture must never be handed to the
// interconnect. The chip fabric already answers such traffic internally, so
// reaching here means that decoder is wrong — the endpoint refuses it and
// counts it rather than forwarding it. With decision record D1 in place the
// consequence of forwarding would no longer be a hang; it would be worse,
// because the bypass would quietly deliver it back into the chip and the broken
// decode would never be noticed.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/types.h"

namespace cdc::components::tpu_v3::noc {

/// The apertures and limits one endpoint works to. All absolute; the endpoint
/// carries no address constant of its own (plan §12).
struct chip_endpoint_config {
    chip_id_t chip = 0;

    /// The largest frame `noc_interconnect` accepts, in bytes at bus
    /// alignment. Configurable because it is a property of the interconnect
    /// rather than of this component, and a test wants to reach the boundary
    /// without moving 2 KiB.
    std::uint64_t max_frame_bytes = 2048;

    /// The interconnect's data width. Chunk boundaries are aligned to it.
    std::uint32_t bus_bytes = 8;

    /// The largest inbound access a NEO-CORE's SRAM path accepts, which is
    /// `sram::neo_max_transfer_bytes` and **not** a free parameter above it.
    ///
    /// Configurable downward so a test can reach the splitting boundary
    /// cheaply; bounded upward because the bound is somebody else's constant.
    /// Two rules, both enforced in `validate()`:
    ///
    ///  * a whole number of `bus_bytes` — the splitter aligns chunk boundaries
    ///    to the bus, so a limit that is not a bus multiple makes later chunks
    ///    start mid-word, and one smaller than the lane offset underflows and
    ///    emits a chunk *larger* than the limit;
    ///  * at most `sram::neo_max_transfer_bytes`. Above it this endpoint emits
    ///    chunks a core's external bridge refuses by design, so the
    ///    configuration elaborates cleanly and then fails every inbound SRAM
    ///    transfer. The sentence above used to assert that relationship and
    ///    nothing checked it.
    std::uint32_t max_inbound_sram_bytes = 64;

    /// Whether a chip aperture reaches the mesh as `target_kind::mmio`, which
    /// `ADDRESS_MAP.md` §7 says it does.
    ///
    /// It decides how an outbound **read** into a remote core's SRAM is cut
    /// up, and it is a configuration field rather than a constant so that the
    /// one assumption this component makes about how the platform registered
    /// its targets is stated where it can be changed. If §7's escape hatch is
    /// ever taken — core SRAM mapped as separate `memory` sub-regions — this
    /// becomes false and the extra shaping stops.
    ///
    /// Why the shaping is needed at all: `noc_interconnect` refuses a read
    /// against an `mmio` target when the beat frame is full width and
    /// **either** the address **or** the length is not bus-aligned
    /// (`noc_interconnect.cpp:2084`). The length half is the one that decides
    /// this: `neo_dma`'s frozen Phase 4 contract accepts lengths 1, 2, 3, 7,
    /// 15, 63, 65 and odd addresses, so publishing "remote SRAM reads must be
    /// bus-aligned" would narrow a contract that is already gated, without a
    /// decision. Decision record D25.
    bool chip_apertures_are_mmio = true;

    /// Whether the interconnect below **spends** an incoming `delay` rather
    /// than annotating it.
    ///
    /// `noc_interconnect` in `detailed` mode waits the caller's delay out
    /// before injecting; in `fast` mode it preserves it and adds an estimate.
    /// The endpoint cannot see which, and the difference matters for reset: a
    /// delay handed downstream is slept inside a call this component cannot
    /// interrupt, so an abandoned transfer would still enter the mesh. When
    /// this is true the endpoint consumes the delay itself, interruptibly,
    /// and hands the interconnect zero — the same behaviour, in a place a
    /// reset can reach (decision record D23). When false the delay is passed
    /// through untouched, because consuming it would destroy the temporal
    /// decoupling the loosely-timed backend exists to provide.
    bool downstream_spends_delay = true;

    /// Throws `std::invalid_argument` naming the field and the accepted range.
    void validate(const std::string& context) const;
};

class chip_noc_endpoint : public sc_core::sc_module {
public:
    chip_noc_endpoint(sc_core::sc_module_name name,
                      chip_endpoint_config config);

    // ── chip side ────────────────────────────────────────────────────────────

    /// Bind to `tpu_chip::external()`.
    tlm_utils::simple_target_socket<chip_noc_endpoint> from_chip;
    /// Bind to `tpu_chip::inbound()`.
    tlm_utils::simple_initiator_socket<chip_noc_endpoint> to_chip;

    // ── mesh side ────────────────────────────────────────────────────────────

    /// Bind to `noc_interconnect::cpu_port(port)`.
    tlm_utils::simple_initiator_socket<chip_noc_endpoint> to_noc;
    /// Bind to the initiator socket `noc_interconnect::add_target()` returns
    /// for this chip's aperture — mapped with `local_owner` set to the same
    /// upstream port `to_noc` uses, so the chip's own traffic to itself never
    /// becomes a flit (decision record D1).
    ///
    /// **Addresses arriving here are relative to the chip aperture, not
    /// absolute.** `noc_interconnect` subtracts the mapped target's base before
    /// calling it — `payload.set_address(access_addr - target.base)`
    /// (`floo_noc_model/src/noc_interconnect.cpp:1189`, and the same for
    /// `transport_dbg` at `:2233`). Everything inside the chip uses the one
    /// absolute address per resource that `ADDRESS_MAP.md` §4 defines, so the
    /// endpoint rebases on the way in and restores the caller's view on the way
    /// out. Getting this wrong is not subtle in effect and is completely
    /// invisible to a stub that supplies absolute addresses: every inbound
    /// access would be refused as foreign.
    ///
    /// ## One socket means one `target_kind`, and that is a map decision
    ///
    /// This is **one** `simple_target_socket`, which is `N = 1` upstream
    /// (`tlm_target_socket<BUSWIDTH, TYPES, 1, POL>`,
    /// `tlm_utils/simple_target_socket.h:50`). So the whole 128 MiB aperture is
    /// one `add_target()` call carrying one `target_kind`, and the endpoint
    /// assumes the one `ADDRESS_MAP.md` §7 specifies: **`mmio`**, for the whole
    /// aperture, even though most of it is core SRAM.
    ///
    /// That assumption is stated here because it is invisible from inside this
    /// component and it has a firmware-visible cost. An 8-byte-beat **read**
    /// against an `mmio` target whose address or length is not bus-aligned is
    /// refused with `TLM_BURST_ERROR_RESPONSE` *before injection*
    /// (`noc_interconnect.cpp:2084`). Two things narrow that, and both matter
    /// when reading the rule:
    ///
    ///  * **writes are unaffected** — the check is on `TLM_READ_COMMAND`;
    ///  * a naturally aligned power-of-two read of 1, 2, 4 or 8 bytes stays a
    ///    narrow beat and is never widened (`axi_lanes.hpp:88`), so it is
    ///    always allowed.
    ///
    /// What is left is exactly the cost `ADDRESS_MAP.md` §7 records: a remote
    /// read of this chip's SRAM must be bus-aligned. Note what that does to
    /// `chunk_span()` in the other direction — a remote read starting at lane
    /// `+k` produces a first chunk of `limit - k` bytes at `+k`, which is
    /// neither bus-aligned nor a narrow power of two, so it is refused and the
    /// first-failing-chunk rule stops the transfer having committed nothing.
    ///
    /// **Not a defect to fix here.** `target_kind` is chosen by the platform at
    /// `add_target()` and enforced inside the interconnect before this endpoint
    /// is called, so nothing this component does can influence it. If the cost
    /// becomes a real limitation the fix is the one §7 names — map core SRAM as
    /// separate `memory` sub-regions, deliberately — and that needs per-region
    /// ingress here, because a second bind to this socket is refused at
    /// elaboration rather than silently mis-addressed.
    /// `simple_target_socket_tagged` does **not** supply it: the tagged variant
    /// is also `N = 1` (`:580`), which is a trap, because "tagged" is what one
    /// would reach for. The multi-bind shape is
    /// `multi_passthrough_target_socket`, or one plain socket per region.
    tlm_utils::simple_target_socket<chip_noc_endpoint> from_noc;

    const chip_endpoint_config& config() const noexcept { return config_; }

    /// `[chip_base, chip_base + stride)` for this chip.
    std::uint64_t aperture_base() const noexcept;
    std::uint64_t aperture_size() const noexcept;

    /// Beats `[address, length)` would occupy, by the interconnect's rule.
    /// Exposed so a test can state its expectation in the interconnect's terms
    /// rather than recomputing this component's arithmetic.
    std::uint64_t beats_for(std::uint64_t address,
                            std::uint64_t length) const noexcept;

    /// The largest payload accepted at `address` without chunking.
    std::uint64_t frame_capacity_at(std::uint64_t address) const noexcept;

    // ── counters ─────────────────────────────────────────────────────────────
    //
    // Three properties hold here, and each of them was once false:
    //
    //  * **bytes are published with the chunk that moved them**, so `bytes` and
    //    `chunks` agree at every instant a reader can observe, including while
    //    a transfer is suspended inside a downstream call. Accumulating into
    //    the member counter after the whole transfer unwound made a
    //    five-chunk transfer blocked in chunk three report "2 chunks, 0
    //    bytes", and lost the bytes outright when a simulation ended in
    //    flight — which is endpoint-boundary conservation failing, the §6
    //    evidence item this component owes;
    //  * **a target's refusal is counted even when it moved nothing.**
    //    `*_partial_failures_` answers "how often did a transfer stop
    //    part-way", which is the wrong question for the common case: an
    //    unsplit over-frame MMIO transfer is refused having moved zero bytes,
    //    and `moved > 0` cannot see it. `*_target_errors_` answers "how often
    //    did the target refuse a chunk". The residue that was left before —
    //    `chunks > 0 && bytes == 0` — is also what a legitimate fully
    //    byte-disabled write produces, so the two were not merely uncounted
    //    but indistinguishable;
    //  * **the two directions carry the same counters**, and `report()` prints
    //    both. Inbound had a `last_*` value and no count, so how often an
    //    inbound transfer failed after committing bytes was recorded nowhere
    //    and appeared in no report.
    //
    // Plan §11.10 also requires **outstanding and latency** counters, and
    // decision record **D24** splits them by quantity rather than by
    // component:
    //
    //  * **latency is measured here**, per transfer, because only this
    //    component can. A transfer larger than the frame limit is one thing
    //    the chip asked for and several transactions to the interconnect, so
    //    `noc_interconnect::last_latency_cycles(port)` answers a different
    //    question and cannot be made to answer this one. The two figures count
    //    different units of work and neither may be quoted as, compared with
    //    or summed with the other;
    //  * **outstanding is measured here too, per direction.** D24 first
    //    deferred it on the reasoning that an endpoint-level outbound count
    //    is `<= 1` by construction, because `neo_external_bridge` arbitrates
    //    a core's two outbound initiators onto one external socket and
    //    `chip_local_fabric` allows one transaction per initiator. That
    //    reasoning covers **one core**. Two cores are two initiators on the
    //    chip fabric, and in `annotated` mode the fabric charges port
    //    occupancy to the caller's delay rather than blocking — so while one
    //    core is suspended inside the NoC the other enters this endpoint.
    //    Measured on the single-chip composition: **peak 2 in `detailed` NoC
    //    mode, peak 1 in `fast`** — and the difference is the quantity, not
    //    noise in it. Nothing downstream blocks in fast mode: the interconnect
    //    annotates, `downstream_spends_delay` is off, and every call returns
    //    before the next arrives. A fast-mode peak above 1 means something
    //    suspended where it should have annotated, which is temporal
    //    decoupling gone. D24 is corrected accordingly;
    //    `TPU_V3_PHASE8_AUDIT.md` §5 had already recorded the fabric behaviour
    //    that makes the detailed figure 2.

    /// Transfers presented by the chip, whatever their outcome.
    std::uint64_t outbound_transfers() const noexcept
    {
        return outbound_transfers_;
    }
    /// Chunks those transfers became. **Never** presented as a transfer count:
    /// one transfer is one thing the chip asked for, and the split is this
    /// component's doing.
    std::uint64_t outbound_chunks() const noexcept { return outbound_chunks_; }
    std::uint64_t outbound_bytes() const noexcept { return outbound_bytes_; }
    /// Transfers that **needed** more than one chunk.
    ///
    /// Decided from the original address and length, not from how many chunks
    /// were attempted: a transfer whose very first chunk fails still needed
    /// splitting, and counting attempts instead would quietly under-report
    /// exactly the transfers that went wrong.
    std::uint64_t outbound_split_transfers() const noexcept
    {
        return outbound_split_transfers_;
    }
    /// Transfers a chunk failed part-way through. The response carries the
    /// failing chunk's status; this counts how often it happened.
    std::uint64_t outbound_partial_failures() const noexcept
    {
        return outbound_partial_failures_;
    }
    /// Outbound accesses refused for naming an address inside this chip.
    /// Should stay zero; counted rather than asserted so an integration defect
    /// becomes a number rather than taking the simulation down.
    std::uint64_t outbound_local_refused() const noexcept
    {
        return outbound_local_refused_;
    }
    /// Outbound transfers refused for spanning more than one architectural
    /// region.
    ///
    /// Checked **before** splitting, and that ordering is the whole point. The
    /// interconnect refuses such a transfer outright, having touched nothing;
    /// chunking it first would hand the leading chunks to the region they do
    /// decode in, commit them, and only then fail — turning a clean refusal
    /// into a partial side effect.
    std::uint64_t outbound_span_refused() const noexcept
    {
        return outbound_span_refused_;
    }
    /// Outbound transfers a **downstream target** refused, whether or not any
    /// byte had moved first.
    ///
    /// Distinct from `outbound_partial_failures()`, which counts only those
    /// that had committed something, and distinct from the `*_refused()`
    /// counters, which count refusals this endpoint made having touched
    /// nothing. Merging them would destroy exactly the distinction that makes
    /// the refusal counters worth reading: who said no.
    std::uint64_t outbound_target_errors() const noexcept
    {
        return outbound_target_errors_;
    }

    std::uint64_t inbound_transfers() const noexcept
    {
        return inbound_transfers_;
    }
    std::uint64_t inbound_chunks() const noexcept { return inbound_chunks_; }
    std::uint64_t inbound_bytes() const noexcept { return inbound_bytes_; }
    /// Inbound accesses naming an address this chip does not own.
    std::uint64_t inbound_foreign_refused() const noexcept
    {
        return inbound_foreign_refused_;
    }
    /// Inbound transfers a chunk failed part-way through, matching the
    /// outbound counter of the same name. Inbound previously had only the
    /// most-recent byte value, so a second failure erased the evidence of the
    /// first and the frequency was unrecoverable.
    std::uint64_t inbound_partial_failures() const noexcept
    {
        return inbound_partial_failures_;
    }
    /// Inbound transfers a downstream target refused. Same distinction as
    /// `outbound_target_errors()`.
    std::uint64_t inbound_target_errors() const noexcept
    {
        return inbound_target_errors_;
    }

    /// Refused for violating the shared payload rules.
    std::uint64_t protocol_errors() const noexcept { return protocol_errors_; }

    /// Bytes the last partially failed **outbound** transfer had committed
    /// before it stopped. The contract says the completion reports how many,
    /// and this is where it is reported.
    ///
    /// Direction-specific on purpose. One shared field let an inbound failure
    /// silently rewrite the number an outbound completion had reported, which
    /// under bidirectional traffic is a metric that changes without its own
    /// transfer doing anything.
    std::uint64_t last_partial_bytes() const noexcept
    {
        return last_partial_bytes_;
    }
    /// The same, for the last partially failed inbound transfer.
    std::uint64_t last_inbound_partial_bytes() const noexcept
    {
        return last_inbound_partial_bytes_;
    }

    // ── transfer latency (decision record D24) ───────────────────────────────
    //
    // Measured as **logical** time — `sc_time_stamp() + delay` at return minus
    // the same sum at entry — not as an `sc_time_stamp()` difference. An
    // annotating target consumes no simulated time and adds to `delay`, so a
    // stamp difference reads zero for every transfer on a loosely timed path,
    // which is the configuration a full-system run uses. The sum is the
    // quantity D16 and the Phase 8 arbiters already treat as a caller's
    // logical time, and it is mode-independent: absorbing an incoming delay
    // moves time from `delay` into the stamp and leaves the sum alone.
    //
    // Sampled only for transfers this endpoint actually **forwarded**. One
    // refused at the endpoint returns almost immediately and describes nothing
    // about the mesh; including it would drag the mean toward zero in
    // proportion to how many integration defects were present. A transfer
    // abandoned by a reset is not sampled either.

    /// Transfers inside `chip_b_transport()` right now, and the most there
    /// have ever been.
    ///
    /// A **transfer-level** quantity, and not the same thing as
    /// `noc_interconnect::outstanding_transactions(port)`: a chunked transfer
    /// is one of these and several of those. Neither may be quoted as the
    /// other, which is the rule D24 states for latency and which applies here
    /// for the same reason.
    std::uint64_t outbound_in_flight() const noexcept
    {
        return outbound_in_flight_;
    }
    std::uint64_t peak_outbound_in_flight() const noexcept
    {
        return peak_outbound_in_flight_;
    }
    std::uint64_t inbound_in_flight() const noexcept
    {
        return inbound_in_flight_;
    }
    std::uint64_t peak_inbound_in_flight() const noexcept
    {
        return peak_inbound_in_flight_;
    }

    sc_core::sc_time outbound_latency_total() const noexcept
    {
        return outbound_latency_total_;
    }
    sc_core::sc_time outbound_latency_max() const noexcept
    {
        return outbound_latency_max_;
    }
    /// Transfers that contributed to the two figures above. Published so a
    /// mean is a ratio of two numbers counting the same thing — the same rule
    /// `INTERFACE_CONTRACT.md` §6 states for request and error counts.
    std::uint64_t outbound_latency_samples() const noexcept
    {
        return outbound_latency_samples_;
    }

    sc_core::sc_time inbound_latency_total() const noexcept
    {
        return inbound_latency_total_;
    }
    sc_core::sc_time inbound_latency_max() const noexcept
    {
        return inbound_latency_max_;
    }
    std::uint64_t inbound_latency_samples() const noexcept
    {
        return inbound_latency_samples_;
    }

    /// Abandon nothing in flight, clear the counters.
    ///
    /// The D23 rule: a chip reset abandons **this chip's** queued and in-flight
    /// work at its endpoint and reports it as an error rather than completing
    /// it; it does **not** reset the shared mesh, which belongs to every chip.
    /// A chunk already inside a downstream `b_transport()` keeps running and
    /// reports its own result — reset cannot unwind a blocked C++ call, the
    /// lesson Phase 8 recorded twice — but the transfer it belongs to is
    /// abandoned at the next chunk boundary rather than continuing.
    void reset();

    std::string report() const;

private:
    void chip_b_transport(tlm::tlm_generic_payload& trans,
                          sc_core::sc_time& delay);
    unsigned int chip_transport_dbg(tlm::tlm_generic_payload& trans);
    void noc_b_transport(tlm::tlm_generic_payload& trans,
                         sc_core::sc_time& delay);
    unsigned int noc_transport_dbg(tlm::tlm_generic_payload& trans);

    /// Split `[address, length)` and drive `socket` with each piece in
    /// ascending order. Returns the status of the first failing chunk, or
    /// `TLM_OK_RESPONSE`; `committed` receives the bytes transferred before the
    /// failure.
    /// `chunk_limit` of zero means "do not split": the transfer is forwarded
    /// whole. `moved` receives the **enabled** bytes, which is what the
    /// contract counts as transferred; `committed` is address progress and the
    /// two differ whenever byte enables mask anything.
    tlm::tlm_response_status forward_chunked(
        tlm_utils::simple_initiator_socket<chip_noc_endpoint>& socket,
        tlm::tlm_generic_payload& trans, sc_core::sc_time& delay,
        std::uint64_t chunk_limit, bool shape_reads, bool outbound,
        std::uint64_t& committed, std::uint64_t& moved,
        std::uint64_t& chunks);

    /// A caller's logical time: `sc_time_stamp() + delay`. The one quantity
    /// that is meaningful in both timing modes.
    static sc_core::sc_time logical_time(
        const sc_core::sc_time& delay) noexcept;

    /// Add one forwarded transfer's latency to the direction's figures.
    void sample_latency(bool outbound, const sc_core::sc_time& entry,
                        const sc_core::sc_time& exit) noexcept;

    /// Consume an incoming delay where a reset can still reach the process.
    /// Returns false when a reset abandoned the transfer, leaving the
    /// unelapsed remainder in `delay` — a caller's logical time is
    /// `sc_time_stamp() + delay` and must never move backwards.
    bool absorb_incoming_delay(sc_core::sc_time& delay,
                               std::uint64_t generation);

    /// One decoded region of `ADDRESS_MAP.md`, or `valid == false` for an
    /// address in no region at all.
    struct decoded_region {
        bool valid = false;
        std::uint64_t base = 0;
        std::uint64_t size = 0;
        bool is_memory = false;
    };

    /// Decode `address` to the **finest** region the map defines, not to the
    /// chip aperture that contains it.
    ///
    /// Computed arithmetically rather than by scanning
    /// `address_map::enumerate_regions()`, which is a linear search its own
    /// header says must not be used on a per-transaction path. The coarse
    /// version of this check accepted any span inside a 128 MiB chip aperture,
    /// which let a transfer cross from `CORE_SRAM` into `SA_CONTROL` — and then
    /// be split, committing the SRAM chunks before the MMIO one failed.
    decoded_region decode_region(std::uint64_t address) const noexcept;

    /// True when the whole span lies inside `region`.
    ///
    /// **One implementation, used by both transports.** The first version of
    /// this component wrote the rule inline in `b_transport` and called a
    /// separate helper from `transport_dbg` — two expressions of one rule, and
    /// the mutation control aimed at the helper reported a clean pass while the
    /// normal path was untouched. A rule with two copies is a rule with one
    /// test.
    static bool span_fits(const decoded_region& region, std::uint64_t address,
                          std::uint64_t length) noexcept;

    /// Largest chunk starting at `address` given a bus-aligned limit.
    ///
    /// Two shapes, and `shape_reads` picks between them:
    ///
    ///  * **false** — the transfer pays for its lane offset once: an unaligned
    ///    start runs to the next bus boundary and every chunk after it is
    ///    aligned. This is what a write does, and what a read against a
    ///    `memory` target does;
    ///  * **true** — every chunk is a shape `noc_interconnect` will not widen:
    ///    a naturally-aligned power-of-two prefix up to the bus boundary, then
    ///    bus-aligned full-width bulk, then a naturally-aligned power-of-two
    ///    suffix. Needed for a read into a chip aperture, which is registered
    ///    `mmio` (D25).
    std::uint64_t chunk_span(std::uint64_t address, std::uint64_t remaining,
                             std::uint64_t limit,
                             bool shape_reads) const noexcept;

    /// True when `[address, length)` lies in some chip's aperture. This
    /// endpoint refuses its **own** aperture before this is reached, so a hit
    /// here is a remote chip.
    static bool in_a_chip_aperture(std::uint64_t address,
                                   std::uint64_t length) noexcept;

    bool overlaps_aperture(std::uint64_t address,
                           std::uint64_t length) const noexcept;
    bool inside_aperture(std::uint64_t address,
                         std::uint64_t length) const noexcept;
    /// True when the whole transfer lies in one core's SRAM window, which is
    /// the only inbound traffic that gets split.
    bool inside_a_core_sram(std::uint64_t address,
                            std::uint64_t length) const noexcept;

    chip_endpoint_config config_;

    /// Bumped by `reset()`. A transfer carrying an older generation stops at
    /// its next chunk boundary instead of continuing into a new epoch.
    std::uint64_t generation_ = 0;
    /// Wakes a transfer parked on an incoming delay when `reset()` arrives.
    sc_core::sc_event reset_event_;

    std::uint64_t outbound_transfers_ = 0;
    std::uint64_t outbound_chunks_ = 0;
    std::uint64_t outbound_bytes_ = 0;
    std::uint64_t outbound_split_transfers_ = 0;
    std::uint64_t outbound_partial_failures_ = 0;
    std::uint64_t outbound_target_errors_ = 0;
    std::uint64_t outbound_local_refused_ = 0;
    std::uint64_t outbound_span_refused_ = 0;

    std::uint64_t inbound_transfers_ = 0;
    std::uint64_t inbound_chunks_ = 0;
    std::uint64_t inbound_bytes_ = 0;
    std::uint64_t inbound_partial_failures_ = 0;
    std::uint64_t inbound_target_errors_ = 0;
    std::uint64_t inbound_foreign_refused_ = 0;

    std::uint64_t protocol_errors_ = 0;
    std::uint64_t last_partial_bytes_ = 0;
    std::uint64_t last_inbound_partial_bytes_ = 0;

    std::uint64_t outbound_in_flight_ = 0;
    std::uint64_t peak_outbound_in_flight_ = 0;
    std::uint64_t inbound_in_flight_ = 0;
    std::uint64_t peak_inbound_in_flight_ = 0;

    sc_core::sc_time outbound_latency_total_ = sc_core::SC_ZERO_TIME;
    sc_core::sc_time outbound_latency_max_ = sc_core::SC_ZERO_TIME;
    std::uint64_t outbound_latency_samples_ = 0;
    sc_core::sc_time inbound_latency_total_ = sc_core::SC_ZERO_TIME;
    sc_core::sc_time inbound_latency_max_ = sc_core::SC_ZERO_TIME;
    std::uint64_t inbound_latency_samples_ = 0;
};

} // namespace cdc::components::tpu_v3::noc
