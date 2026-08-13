// SPDX-License-Identifier: Apache-2.0
//
// The independent NEO DMA (plan §11.5, decision record D14).
//
// ## What it is
//
// One DMA per NEO-CORE, one descriptor slot, owned by TPU_V3. It moves bytes
// between core SRAM and chip/global/NoC-visible memory, and it does so only
// through the two ports it is given: a native local-SRAM requester and an
// external TLM initiator. It holds no pointer into anyone's backing store,
// because `core_sram` exposes none and this component never asks for one.
//
// ## What it is deliberately not
//
// It is **not** the repository-wide PL330-style DMA under another name. That
// model is eight channels of microprogram — `DMAMOV`/`DMALD`/`DMAST`, a debug
// launch path, an MFIFO and an event-vector interrupt model — behind a single
// generic TLM master socket. Useful prior art, wrong contract: NEO-CORE needs
// one descriptor, a split local/external data path, and a level interrupt. The
// shared component is left untouched because three other platforms depend on
// it, and nothing here includes, wraps, links or inherits from it. It is also
// not Sauria's DMA, which D14 forbids outright.
//
// `neo_dma_independence` is the gate for all of that: it scans the sources for
// forbidden includes and symbols and the built archive for forbidden
// references, so the separation survives someone reaching for a convenient
// header later.
//
// ## Shape of a job
//
//   1. firmware writes the descriptor registers while idle;
//   2. firmware writes `START`; `BUSY` is set **before that access returns**
//      and no data has moved yet;
//   3. the worker thread validates, then copies, consuming simulated time;
//   4. on completion or first error it clears `BUSY`, sets `DONE` or `ERROR`,
//      publishes the byte counts and raises the level IRQ if enabled;
//   5. firmware writes 1 to the status bit, which deasserts the IRQ.
//
// Validation happens in the worker rather than in the `START` write, so an
// invalid descriptor reports through exactly the same status and IRQ path as a
// downstream failure. A driver that had to handle "rejected synchronously" and
// "failed asynchronously" as two different shapes would get one of them wrong.
//
// ## Epochs
//
// `reset()` and explicit `ABORT` bump a job generation. The worker re-checks it
// at every point where it can resume — after arbitration, after a transaction,
// after consuming an annotated delay — and abandons the job rather than
// publishing into a state epoch that no longer exists. Bytes already committed
// stay committed; nothing is rolled back, and nothing that was abandoned is
// ever reported as completion.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/dma/dma_registers.h"
#include "tpu_v3/sram/native_port.h"

namespace cdc::components::tpu_v3::dma {

/// Where this DMA lives and what it may reach.
struct neo_dma_config {
    /// Absolute base of the 64 KiB `DMA_CONTROL` window, i.e.
    /// `address_map::dma_control(chip, core)`.
    std::uint64_t control_base = 0;
    std::uint64_t control_size = address_map::dma_control_size;

    /// The core SRAM window this DMA's `local` port reaches. Classification is
    /// by absolute address against this window; there is no direction bit,
    /// because a direction bit is a second source of truth that can disagree
    /// with the addresses.
    std::uint64_t sram_base = 0;
    std::uint64_t sram_window = address_map::core_sram_window;

    /// Largest external payload the DMA will issue, from
    /// `dma_config::max_burst_bytes` (plan §11.1). The frame formula bounds it
    /// further at a lane offset.
    std::uint64_t max_burst_bytes = 2048;

    /// Cost charged per issued chunk, so a transfer takes simulated time and a
    /// reset has a window to land in. Not a claim about DMA throughput.
    sc_core::sc_time chunk_latency = sc_core::sc_time(10, sc_core::SC_NS);

    void validate(const std::string& context) const;
};

/// Which side of the machine an address names.
enum class endpoint_kind {
    /// Wholly inside the core-SRAM window.
    local,
    /// Wholly outside it.
    external,
    /// Overlapping the boundary. Not a route — a span that is partly local and
    /// partly not has no single owner, and splitting it would invent a routing
    /// rule Revision 1 does not define.
    straddling,
};

class neo_dma : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(neo_dma);

    neo_dma(sc_core::sc_module_name name, neo_dma_config config);

    /// 32-bit AXI4-Lite MMIO target, bound behind `neo_control_fabric`.
    tlm_utils::simple_target_socket<neo_dma> control;

    /// Native local-SRAM requester. Every request carries
    /// `neo_requester::dma`, so every byte it moves is attributable.
    sc_core::sc_port<sram::neo_local_sram_if> local;

    /// External AXI4/NoC-facing initiator, bound to the external bridge's
    /// outbound port.
    tlm_utils::simple_initiator_socket<neo_dma> external;

    /// Level interrupt. High while an unacknowledged `DONE` or `ERROR` is
    /// enabled; an explicit abort never raises it.
    ///
    /// Driven by one process. The state behind it changes from two — a
    /// register write and the worker — and in RTL that is unremarkable,
    /// because the line is combinational from the status bits. A SystemC
    /// signal is not: two processes writing it is an elaboration error, and
    /// the alternative of asking every user to declare `SC_MANY_WRITERS`
    /// pushes this component's internal structure into its callers' port
    /// declarations. So both sides raise an event and `drive_irq` is the only
    /// writer.
    sc_core::sc_out<bool> irq;

    const neo_dma_config& config() const noexcept { return config_; }

    /// Synchronous reset from the NEO-CORE's hierarchical reset path.
    ///
    /// Abandons any active job, clears the status bits, deasserts the IRQ and
    /// counts an abort.
    ///
    /// It clears the **path traffic** counters — `LOCAL_BYTES`,
    /// `EXTERNAL_BYTES` and the request and chunk counts — and opens a new
    /// counter epoch, because those are reconciled against
    /// `neo_local_sram_fabric`, which clears its own on reset. In Phase 7 the
    /// core's hierarchical reset drives both, so a DMA that kept lifetime
    /// traffic totals would disagree with the fabric from the first reset
    /// onwards. Conservation is therefore a per-epoch property.
    ///
    /// It does **not** clear the **event** counters — `TRANSFER_COUNT`,
    /// `ERROR_COUNT`, `ABORT_COUNT`, `OVERRUN_COUNT`. They have no counterpart
    /// on the fabric and record this engine's own history, and `ABORT_COUNT`
    /// is *incremented* by this very call, so the block cannot also be zeroed
    /// by it.
    ///
    /// `BYTES_DONE` is neither: it belongs to a job, not to a measurement
    /// window. An active-job reset keeps it and keeps its owner, so an access
    /// still in flight can still report the bytes it committed; a reset with
    /// no active job zeroes it.
    ///
    /// Must be called from a SystemC process: it drives the IRQ signal and
    /// wakes the worker.
    void reset();

    // ── introspection, for tests and reports ─────────────────────────────────
    //
    // Everything here is also readable through MMIO. These accessors exist so
    // a test can state what it means without a register decode in the middle,
    // and so the platform report does not have to issue transactions.

    endpoint_kind classify(std::uint64_t address,
                           std::uint64_t length) const noexcept;

    bool busy() const noexcept { return (status_ & status_bit::busy) != 0; }
    std::uint32_t status() const noexcept { return status_; }
    error_cause last_error() const noexcept { return error_cause_; }

    std::uint64_t bytes_done() const noexcept { return bytes_done_; }
    std::uint64_t local_bytes() const noexcept { return local_bytes_; }
    std::uint64_t external_bytes() const noexcept { return external_bytes_; }
    std::uint64_t transfer_count() const noexcept { return transfer_count_; }
    std::uint64_t error_count() const noexcept { return error_count_; }
    std::uint64_t abort_count() const noexcept { return abort_count_; }
    std::uint64_t overrun_count() const noexcept { return overrun_count_; }

    /// Native local-SRAM requests and external TLM transactions issued.
    ///
    /// Kept apart from the byte totals and from each other: a chunk count is
    /// not a byte count and neither is a hardware bus transaction count
    /// (decision record D7).
    std::uint64_t local_requests() const noexcept { return local_requests_; }
    std::uint64_t external_requests() const noexcept
    {
        return external_requests_;
    }
    /// Chunks that completed **both** their source and destination access.
    ///
    /// Named for that on purpose: a failing chunk was issued and is not
    /// counted here, so calling it "issued" would make the number disagree
    /// with itself on exactly the runs where it matters.
    std::uint64_t chunks_completed() const noexcept
    {
        return chunks_completed_;
    }

    std::string report() const;

private:
    /// The descriptor as it was when `START` was accepted. Copied rather than
    /// read live, so a register write racing the worker cannot change a job
    /// that is already running.
    struct descriptor {
        std::uint64_t source = 0;
        std::uint64_t destination = 0;
        std::uint64_t length = 0;
    };

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);
    /// The single writer of `irq`. Runs once at time zero so the line starts
    /// low, then on every change of the state behind it.
    void drive_irq();

    /// The AXI4-Lite payload rules. Sets the response and returns false when
    /// the access is refused.
    bool check_control_rules(tlm::tlm_generic_payload& trans);

    std::uint32_t read_register(std::uint64_t offset) const;
    /// Returns false when the write is refused — a descriptor write while
    /// busy, or a `START`+`ABORT` in the same access.
    bool write_register(std::uint64_t offset, std::uint32_t value);

    void worker();
    /// Validates the snapshot. Returns `none` when the route is legal.
    error_cause validate_descriptor(const descriptor& job) const noexcept;
    /// Runs one job. Returns the first error, or `none`.
    error_cause run_transfer(const descriptor& job, std::uint64_t generation);

    /// One native access, split by the caller into 1..64-byte pieces.
    ///
    /// `destination` marks the side whose bytes count as committed. It is
    /// published as each piece lands, **before** the annotated delay is
    /// consumed, so a reset arriving inside that wait cannot un-report memory
    /// that already changed — and only while the job's epoch is still
    /// current, so a transaction that completes after a reset cannot add its
    /// bytes to whatever job started next.
    bool local_access(sram::neo_command command, std::uint64_t address,
                      std::uint32_t size, unsigned char* data,
                      std::uint64_t generation, bool destination);
    /// One external transaction, already trimmed to a legal frame.
    bool external_access(tlm::tlm_command command, std::uint64_t address,
                         std::uint32_t size, unsigned char* data,
                         std::uint64_t generation, bool destination);

    void finish(std::uint32_t bit, error_cause cause, std::uint64_t generation);
    /// Requests a re-evaluation of the interrupt level. Never writes the
    /// signal itself; see `irq`.
    void update_irq();
    bool superseded(std::uint64_t generation) const noexcept
    {
        return generation != generation_;
    }

    neo_dma_config config_;

    // ── programmed state ─────────────────────────────────────────────────────
    std::uint64_t src_addr_ = 0;
    std::uint64_t dst_addr_ = 0;
    std::uint32_t length_ = 0;
    std::uint32_t irq_enable_ = 0;

    // ── job state ────────────────────────────────────────────────────────────
    std::uint32_t status_ = 0;
    error_cause error_cause_ = error_cause::none;
    descriptor active_{};
    /// Bumped by `reset()` and by explicit `ABORT`, so a worker that resumes
    /// into a new epoch abandons its job instead of publishing into it.
    std::uint64_t generation_ = 0;

    /// Which job `BYTES_DONE` currently describes.
    ///
    /// The publish rule is *ownership of the register*, not sameness of the
    /// epoch, and the difference is a bug this model had. A native access can
    /// write beats into SRAM, wait for arbitration, have a reset land, and
    /// only then return with those bytes reported. Refusing to count them
    /// because the epoch moved loses a commit that happened **before** the
    /// reset — precisely what plan §11.5 says must be reported.
    ///
    /// What must not happen is one job's bytes landing in another job's
    /// count. That is a different condition: it holds only once a *new* job
    /// has claimed the register. A reset alone does not claim it, so an
    /// interrupted job keeps reporting into its own snapshot; a `START`
    /// does, and from that moment the old worker publishes nothing.
    std::uint64_t bytes_done_generation_ = 0;

    /// Which counter epoch the path traffic totals belong to.
    ///
    /// Advanced by `reset()` — the call that clears those totals — and **not**
    /// by `ABORT`, which leaves them alone. A request still in flight when a
    /// reset lands returns afterwards, and adding its bytes to the freshly
    /// cleared totals would let an old request re-populate a new epoch:
    /// `LOCAL_BYTES` would read 64 immediately after a reset that had just
    /// zeroed it. `neo_local_sram_fabric` already excludes old-generation
    /// responses from its own counters for the same reason, so a DMA that did
    /// not would also stop reconciling with it.
    ///
    /// Deliberately separate from `bytes_done_generation_`. They answer
    /// different questions: this one is "which measurement window does this
    /// traffic belong to", the other is "which job does this destination
    /// count describe". A reset opens a new measurement window without
    /// handing the job's register to anyone, so the two move independently.
    std::uint64_t traffic_epoch_ = 0;

    /// A job is waiting to be picked up.
    ///
    /// A flag, not just the event, and the difference is a bug this model had.
    /// `ABORT` and `reset()` clear `BUSY` immediately, so firmware may start a
    /// new job while the old worker is still unwinding out of a bank
    /// arbitration, an external transaction or a chunk delay. At that moment
    /// nobody is waiting on `start_event_`, so the notification is delivered
    /// to no one and is gone; the worker then reaches `wait(start_event_)`,
    /// blocks, and the new job holds `BUSY` forever. A flag survives the gap,
    /// so the worker finds the request whenever it gets back to the top.
    bool start_pending_ = false;
    sc_core::sc_event start_event_;
    sc_core::sc_event irq_event_;

    // ── counters ─────────────────────────────────────────────────────────────
    std::uint64_t bytes_done_ = 0;
    std::uint64_t local_bytes_ = 0;
    std::uint64_t external_bytes_ = 0;
    std::uint64_t local_requests_ = 0;
    std::uint64_t external_requests_ = 0;
    std::uint64_t chunks_completed_ = 0;
    std::uint64_t transfer_count_ = 0;
    std::uint64_t error_count_ = 0;
    std::uint64_t abort_count_ = 0;
    std::uint64_t overrun_count_ = 0;

    /// At most one legal external frame. Sized once; never a pointer into
    /// anyone else's memory.
    std::vector<unsigned char> staging_;
};

} // namespace cdc::components::tpu_v3::dma
