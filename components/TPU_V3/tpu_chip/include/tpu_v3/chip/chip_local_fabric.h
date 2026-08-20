// SPDX-License-Identifier: Apache-2.0
//
// Chip-local fabric — the decode and arbitration layer between the two
// NEO-COREs of one TPU chip, the chip's own register windows, and the single
// aggregated NoC endpoint (plan §11.9, `ARCHITECTURE.md` §1).
//
// One rule shapes everything here, and it is the same rule the core's external
// bridge enforces one level down: **containment is a decode consequence, not a
// routing decision made per transaction** (plan §9.2). A core naming its
// sibling's aperture is answered inside the chip and never reaches the mesh; a
// core naming an address outside the chip leaves through one port and one port
// only. Neither is a choice made by looking at a transaction — it falls out of
// where the address is.
//
// That is what makes the FlooNoC `NoLoopback = 1` constraint survivable. The
// existing `noc_interconnect` refuses any target on a node that hosts an
// upstream port, and a TPU chip needs both, so traffic between the two cores of
// a chip must never be handed to the mesh in the first place
// (`TPU_V3_PHASE0_AUDIT.md` §5.1, decision record D1). A local bypass added at
// the endpoint would be a routing decision; done here it is arithmetic.
//
// ## Directions
//
// **Outbound** arrives on `from[core0]` / `from[core1]`, the sockets each
// core's `neo_external_bridge::external` binds to. Four outcomes: the sibling
// core, a chip register window, out of the chip, or refused.
//
// **Inbound** arrives on `from[external_inbound]`, from the NoC endpoint or the
// host loader. It may name anything *inside* this chip and nothing outside it:
// an inbound access to a foreign address means the endpoint delivered a packet
// to the wrong node, and forwarding it back out would turn one mis-route into a
// loop.
//
// ## Two timing modes, for the same reason the local SRAM fabric has two
//
// `annotated` never blocks. Port occupancy is tracked as a busy-until
// timestamp and the resulting serialisation is added to the caller's `delay`.
// This is the mode a chip attached to the detailed NoC must be in, because a
// fabric that waits stalls the one process that advances the mesh clock
// (`INTERFACE_CONTRACT.md` §3).
//
// `arbitrated` blocks on a real rotating-priority arbiter per downstream port.
// Round-robin *fairness* is only a behaviour in this mode: with annotation
// alone, requests are processed in call order and round-robin is
// indistinguishable from first-come-first-served, so a "round-robin arbiter"
// would be an untested claim. The Phase 8 fairness and back-pressure checks
// run here, under a watchdog.
//
// Both modes decode identically and return identical data and status. What
// differs is only how contention is charged.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/types.h"

namespace cdc::components::tpu_v3::chip {

/// The named initiators on the chip-local fabric. Closed, like every other
/// requester list in this tree: traffic with no owner is traffic whose metrics
/// nothing can be traced back to (`INTERFACE_CONTRACT.md` §5).
enum class chip_initiator : unsigned {
    core0 = 0,
    core1 = 1,
    /// The NoC endpoint or the host loader, reaching into this chip.
    external_inbound = 2,
};

inline constexpr unsigned chip_initiator_count = cores_per_chip + 1;

const char* to_string(chip_initiator initiator) noexcept;

/// Where an address decodes. The first five values are also the downstream
/// port indices, in order; `none` is not a port.
enum class chip_destination : unsigned {
    core0 = 0,
    core1 = 1,
    chip_control = 2,
    chip_counters = 3,
    /// Anything outside this chip: global RAM, boot ROM, another chip.
    outside = 4,
    /// Refused. Either the transfer straddles two regions, or an inbound
    /// access named an address this chip does not own.
    none = 5,
};

inline constexpr unsigned chip_port_count = 5;

const char* to_string(chip_destination destination) noexcept;

/// The apertures this fabric decodes against. Absolute, and all of them come
/// from `address_map.h`: plan §12 forbids a component carrying its own copy of
/// a base address, and `chip_aperture_of()` is how one is obtained.
struct chip_aperture_spec {
    std::uint64_t chip_base = 0;
    std::uint64_t chip_size = 0;
    std::uint64_t core_base[cores_per_chip] = {};
    std::uint64_t core_size = 0;
    std::uint64_t control_base = 0;
    std::uint64_t control_size = 0;
    std::uint64_t counters_base = 0;
    std::uint64_t counters_size = 0;

    /// Throws `std::invalid_argument` naming the field when a window is empty,
    /// leaves the chip aperture, or overlaps another.
    void validate(const std::string& context) const;
};

/// The apertures of one chip, computed from the address map.
chip_aperture_spec chip_aperture_of(chip_id_t chip);

enum class chip_fabric_timing {
    /// Loosely timed. No process ever waits; contention is annotated onto the
    /// caller's delay. Safe behind any TLM target path, including the NoC.
    annotated,
    /// Approximately timed. Initiators block on a real rotating-priority
    /// arbiter per downstream port. Callers must be processes that may
    /// `wait()`.
    arbitrated,
};

const char* to_string(chip_fabric_timing timing) noexcept;

class chip_local_fabric : public sc_core::sc_module {
public:
    /// `cycle` is the chip fabric's clock period: one forwarded transaction
    /// occupies its downstream port for one cycle.
    chip_local_fabric(sc_core::sc_module_name name, chip_aperture_spec spec,
                      chip_fabric_timing timing = chip_fabric_timing::annotated,
                      sc_core::sc_time cycle
                          = sc_core::sc_time(1, sc_core::SC_NS));

    /// One per `chip_initiator`, indexed by its value: the two cores'
    /// outbound ports and the chip's inbound port.
    sc_core::sc_vector<
        tlm_utils::simple_target_socket_tagged<chip_local_fabric>>
        from;

    /// To each core's `neo_external_bridge::inbound`, indexed by core id.
    sc_core::sc_vector<tlm_utils::simple_initiator_socket<chip_local_fabric>>
        to_core;

    /// `CHIP_CONTROL` and `CHIP_COUNTERS`.
    tlm_utils::simple_initiator_socket<chip_local_fabric> to_control;
    tlm_utils::simple_initiator_socket<chip_local_fabric> to_counters;

    /// The chip's one way out. Plan §4.4: a chip presents exactly one
    /// aggregated manager to the mesh, never its harts and engines as
    /// unrelated NoC managers.
    tlm_utils::simple_initiator_socket<chip_local_fabric> external;

    const chip_aperture_spec& aperture() const noexcept { return spec_; }
    chip_fabric_timing timing() const noexcept { return timing_; }
    sc_core::sc_time cycle() const noexcept { return cycle_; }

    /// Where `[address, length)` decodes, without side effects.
    ///
    /// `none` covers both refusal cases the decode itself can see: a transfer
    /// that straddles two regions has no single correct answer for who should
    /// answer it, and an address that is inside the chip aperture but in none
    /// of its windows is a hole. Whether `outside` is legal depends on which
    /// initiator asked, which is a routing rule rather than a decode one.
    chip_destination decode(std::uint64_t address,
                            std::uint64_t length) const noexcept;

    // ── counters ─────────────────────────────────────────────────────────────

    std::uint64_t requests(chip_initiator initiator) const;
    std::uint64_t bytes(chip_initiator initiator) const;
    std::uint64_t routed(chip_initiator initiator,
                         chip_destination destination) const;

    /// Core-to-sibling traffic answered inside the chip. **The number that
    /// says the NoLoopback constraint is being respected by construction**: it
    /// counts accesses that were addressed across the chip and never offered
    /// to the mesh.
    std::uint64_t local_bypass() const noexcept { return local_bypass_; }

    /// Requests handed to the single external port.
    std::uint64_t outbound_requests() const noexcept
    {
        return outbound_requests_;
    }

    /// A core naming its own aperture on its outbound port.
    ///
    /// Should stay at zero: the core's own external bridge already refuses
    /// this, so a non-zero value means that decoder is wrong. Counted rather
    /// than asserted, for the same reason the bridge counts it — an
    /// integration bug becomes a number in a report instead of taking the
    /// simulation down, and a negative test can prove the refusal happens.
    std::uint64_t self_refused() const noexcept { return self_refused_; }

    /// Inbound accesses naming an address outside this chip. The endpoint
    /// delivered them to the wrong node; sending them back out would turn one
    /// mis-route into a loop.
    std::uint64_t inbound_foreign_refused() const noexcept
    {
        return inbound_foreign_refused_;
    }

    /// Transfers refused for spanning two regions or falling in a hole.
    std::uint64_t decode_refused() const noexcept { return decode_refused_; }

    /// Refused for violating the shared payload rules.
    std::uint64_t protocol_errors() const noexcept { return protocol_errors_; }

    /// Refused by the downstream target itself. Counted apart from the three
    /// above because it means something different: the access was routable and
    /// the target declined it.
    std::uint64_t target_errors() const noexcept { return target_errors_; }

    /// Requests that found their downstream port occupied. This is the
    /// back-pressure, in both timing modes.
    std::uint64_t port_conflicts(chip_destination destination) const;

    /// Grants one port issued to one initiator. This is what makes
    /// rotating-priority fairness measurable rather than claimed.
    std::uint64_t port_grants(chip_destination destination,
                              chip_initiator initiator) const;

    /// Largest number of transactions ever in flight from one initiator.
    /// Revision 1 allows one; the counter exists so a test can show the limit
    /// is enforced rather than merely never exercised.
    unsigned peak_in_flight(chip_initiator initiator) const;

    /// Abandon in-flight work, release every port, clear the counters.
    ///
    /// An initiator blocked on an arbiter when this is called does **not** keep
    /// waiting: reset bumps a generation counter and wakes every port, and a
    /// request from an older generation completes with
    /// `TLM_GENERIC_ERROR_RESPONSE` rather than being forwarded. Clearing the
    /// waiting flags without waking anyone would leave a blocked initiator
    /// waiting for a grant no arbiter would ever issue — the same defect the
    /// local SRAM fabric had once, and the same fix.
    void reset();

    std::string report() const;

private:
    struct port_state {
        // ── annotated mode ───────────────────────────────────────────────────
        sc_core::sc_time busy_until = sc_core::SC_ZERO_TIME;

        // ── arbitrated mode ──────────────────────────────────────────────────
        bool busy = false;
        bool waiting[chip_initiator_count] = {};
        /// Rotating priority: the next grant starts one past this.
        unsigned last_granted = chip_initiator_count - 1;
        /// Notified whenever `busy` clears or a waiter appears, so a blocked
        /// initiator re-evaluates instead of polling.
        sc_core::sc_event changed;

        std::uint64_t grants[chip_initiator_count] = {};
        std::uint64_t conflicts = 0;
    };

    void b_transport(int id, tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay);
    unsigned int transport_dbg(int id, tlm::tlm_generic_payload& trans);

    /// Applies the routing rules on top of `decode()` and returns the port, or
    /// `none` after setting the response status and counting the refusal.
    chip_destination route(chip_initiator initiator,
                           tlm::tlm_generic_payload& trans);

    tlm_utils::simple_initiator_socket<chip_local_fabric>& port_of(
        chip_destination destination);

    void charge_annotated(chip_destination destination,
                          sc_core::sc_time& delay);

    /// Block until this initiator owns the downstream port, then hold it.
    ///
    /// Acquire and release are separate calls because the port has to stay
    /// held **across the downstream transaction**, not merely for the cycle
    /// before it. The first version released it as soon as the grant cycle
    /// elapsed, which let two initiators be inside one target at the same
    /// time — an arbiter that only serialises its own bookkeeping. The bench's
    /// `peak_in_flight` on the probe is what caught it.
    ///
    /// Returns false when a reset abandoned the request while it waited, in
    /// which case the port must **not** be released: reset already did, and it
    /// may already belong to somebody else.
    bool acquire_port(chip_initiator initiator, chip_destination destination,
                      sc_core::sc_time& delay,
                      std::uint64_t request_generation);
    void release_port(chip_destination destination);

    unsigned select_waiter(const port_state& port) const noexcept;

    bool in_region(std::uint64_t base, std::uint64_t size,
                   std::uint64_t address, std::uint64_t length) const noexcept;

    chip_aperture_spec spec_;
    chip_fabric_timing timing_;
    sc_core::sc_time cycle_;

    /// One request in flight per initiator (Revision 1). A blocking call gives
    /// that for free only while one process drives one initiator; two
    /// processes sharing an identity overlap the moment the first one waits,
    /// and from then on the arbiter sees one contender where there are two.
    unsigned in_flight_[chip_initiator_count] = {};
    unsigned peak_in_flight_[chip_initiator_count] = {};

    std::uint64_t generation_ = 0;

    std::uint64_t requests_[chip_initiator_count] = {};
    std::uint64_t bytes_[chip_initiator_count] = {};
    std::uint64_t routed_[chip_initiator_count][chip_port_count] = {};

    std::uint64_t local_bypass_ = 0;
    std::uint64_t outbound_requests_ = 0;
    std::uint64_t self_refused_ = 0;
    std::uint64_t inbound_foreign_refused_ = 0;
    std::uint64_t decode_refused_ = 0;
    std::uint64_t protocol_errors_ = 0;
    std::uint64_t target_errors_ = 0;

    std::vector<std::unique_ptr<port_state>> ports_;
};

} // namespace cdc::components::tpu_v3::chip
