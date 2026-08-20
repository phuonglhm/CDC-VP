// SPDX-License-Identifier: Apache-2.0
//
// The shared SRAM of one NEO-CORE (plan §11.3).
//
// ## What this is, and what it deliberately is not
//
// It is *storage plus policy*: sparse page-backed bytes, the window/capacity
// rule, byte enables, bounds and overflow checks, deterministic
// initialisation, an explicit reset policy and its own counters.
//
// It is **not** the arbiter. Banking, per-bank round-robin, back-pressure and
// requester attribution live in `neo_local_sram_fabric` (decision record D15),
// which is the only thing that calls the access methods below. Keeping the two
// apart is what makes the SRAM testable without an arbiter and the arbiter
// testable without worrying about page allocation.
//
// It also exposes **no pointer into its backing store**, and that omission is
// load-bearing rather than incidental: D15 forbids an accelerator obtaining a
// direct pointer to SRAM backing and bypassing arbitration, bounds checks,
// byte enables and counters. There is no `data()`, no `raw()`, no
// `get_direct_mem_ptr()`. Everything goes through `read`/`write`, so
// `bytes_written()` here and the fabric's per-requester totals must agree —
// and `test_neo_external_bridge` checks exactly that conservation.
//
// ## Reset policy
//
// `reset()` releases every allocated page and restores the all-zero state, and
// clears the traffic counters. It does **not** clear the peak backing
// high-water mark, which answers "how much host memory did this run need" — a
// question a reset does not un-ask. Reset is synchronous and is driven by the
// core's reset sequencing (`ARCHITECTURE.md` §6); there is no in-flight work
// inside the SRAM to abandon, because it never waits.
//
// ## Timing
//
// None. The SRAM annotates nothing and waits for nothing; access latency is
// the fabric's to model, because latency is a property of the path and the
// contention, not of the bytes.

#pragma once

#include <cstdint>
#include <string>

#include <systemc>

#include "tpu_v3/sparse_memory.h"
#include "tpu_v3/sram/native_port.h"
#include "tpu_v3/sram/sram_config.h"

namespace cdc::components::tpu_v3::sram {

class core_sram : public sc_core::sc_module {
public:
    core_sram(sc_core::sc_module_name name, core_sram_config config);

    const core_sram_config& config() const noexcept { return config_; }

    // ── decode ───────────────────────────────────────────────────────────────

    /// True when `[address, length)` is inside the decoded window. The window
    /// decodes whatever the capacity is (D6).
    bool in_window(std::uint64_t address, std::uint64_t length) const noexcept;

    /// True when `[address, length)` is inside the window **and** backed.
    bool is_backed(std::uint64_t address, std::uint64_t length) const noexcept;

    /// Classify an access without performing it: `ok`, `decode_error`,
    /// `capacity_error` or `size_error`. The fabric calls this first so a
    /// refused access never reaches storage and never allocates a page.
    neo_status classify(std::uint64_t address, std::uint32_t size) const noexcept;

    // ── access ───────────────────────────────────────────────────────────────
    //
    // Absolute addresses. `strobes` is one byte per data byte or null for all
    // enabled. A refused access transfers nothing and leaves the caller's
    // buffer untouched — an error is never converted to zero data.

    neo_status read(std::uint64_t address, std::uint32_t size,
                    unsigned char* out, const unsigned char* strobes);
    neo_status write(std::uint64_t address, std::uint32_t size,
                     const unsigned char* in, const unsigned char* strobes);

    /// Host-side loading and test setup. Same decode and bounds rules, no
    /// counters: a loader is not workload traffic (`INTERFACE_CONTRACT.md` §8).
    neo_status debug_read(std::uint64_t address, std::uint32_t size,
                          unsigned char* out, const unsigned char* strobes);
    neo_status debug_write(std::uint64_t address, std::uint32_t size,
                           const unsigned char* in,
                           const unsigned char* strobes);

    /// Full component reset: releases the page-backed storage, restoring the
    /// deterministic all-zero state (D6), **and** clears the counters.
    ///
    /// This is a component-level and platform-initialisation operation. A
    /// NEO-CORE reset does *not* call it — see `reset_counters()`.
    void reset();

    /// Clears the workload counters and leaves the stored data alone — and
    /// leaves `debug_bytes_written()` alone with it, because that counter is
    /// evidence a loader ran and the bytes it loaded are still there.
    ///
    /// A core reset needs exactly this. It must not erase the memory, because
    /// every engine in the core reports bytes it already committed to that
    /// memory as still committed after a reset (plan §11.5, D17) — wiping it
    /// would make those registers describe data that is gone. But it must
    /// clear the counters, because `neo_local_sram_fabric` and every requester
    /// open a new counter epoch on reset, and storage counters left running
    /// across that boundary would no longer reconcile with the fabric's.
    void reset_counters();

    // ── counters ─────────────────────────────────────────────────────────────
    //
    // Named for what they measure, and none of them is a vector-instruction
    // or bus-transaction count (decision record D7). Requester attribution is
    // the fabric's; these are totals at the storage.

    std::uint64_t read_accesses() const noexcept { return read_accesses_; }
    std::uint64_t write_accesses() const noexcept { return write_accesses_; }
    std::uint64_t bytes_read() const noexcept { return bytes_read_; }
    std::uint64_t bytes_written() const noexcept { return bytes_written_; }
    std::uint64_t error_count() const noexcept { return error_count_; }

    /// Debug traffic, kept apart from the workload totals above so a report
    /// can show that a loader ran without it polluting the measurement.
    std::uint64_t debug_bytes_written() const noexcept
    {
        return debug_bytes_written_;
    }

    // ── capacity versus backing ──────────────────────────────────────────────

    std::uint64_t window_bytes() const noexcept { return config_.window_bytes; }
    std::uint64_t capacity_bytes() const noexcept
    {
        return config_.capacity_bytes;
    }
    std::uint64_t allocated_backing_bytes() const noexcept
    {
        return storage_.allocated_bytes();
    }
    std::uint64_t peak_allocated_backing_bytes() const noexcept
    {
        return storage_.peak_allocated_bytes();
    }
    std::size_t allocated_page_count() const noexcept
    {
        return storage_.allocated_pages();
    }

    /// Multi-line report: window, capacity, backing and traffic, each labelled
    /// with what it is. The three memory numbers are printed together on
    /// purpose — read separately they are routinely mistaken for each other.
    std::string report() const;

private:
    neo_status perform(std::uint64_t address, std::uint32_t size,
                       const unsigned char* in, unsigned char* out,
                       const unsigned char* strobes, bool counted);

    core_sram_config config_;
    sparse_memory storage_;

    std::uint64_t read_accesses_ = 0;
    std::uint64_t write_accesses_ = 0;
    std::uint64_t bytes_read_ = 0;
    std::uint64_t bytes_written_ = 0;
    std::uint64_t error_count_ = 0;
    std::uint64_t debug_bytes_written_ = 0;
};

} // namespace cdc::components::tpu_v3::sram
