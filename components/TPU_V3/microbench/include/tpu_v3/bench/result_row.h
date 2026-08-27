// SPDX-License-Identifier: Apache-2.0
//
// One benchmark case's machine-readable result row (§8, §11).
//
// Two properties are deliberate and neither is cosmetic.
//
// **A field is present, or it says why it is not.** §8 requires every listed
// measurement "or explicitly say `unavailable`". An absent JSON key would let a
// consumer treat a missing counter as a zero, and a zero cycle count is a
// performance claim. So `measured<T>` carries a reason string and an
// unavailable field serialises as `{"unavailable": "<why>"}` — the row explains
// itself without the reader consulting the plan.
//
// **Derived is not measured.** Lane utilization is computed from values the
// *guest* reported, and the row carries the formula that produced it under
// `derivation`. §12 draws this line sharply, and a number whose provenance is
// only in a commit message is one that will eventually be quoted as measured.
//
// The runner writes this. An aggregation script reads it. Nothing may parse the
// simulator's human-readable log as the source of numeric truth (§11).

#ifndef CDC_COMPONENTS_TPU_V3_BENCH_RESULT_ROW_H
#define CDC_COMPONENTS_TPU_V3_BENCH_RESULT_ROW_H

#include <cstdint>
#include <iosfwd>
#include <string>
#include <utility>
#include <vector>

#include "tpu_v3/bench/benchmark_case.h"
#include "tpu_v3/bench/golden.h"

namespace cdc::components::tpu_v3::bench {

/// A quantity that is either measured or explicitly not available, with the
/// reason travelling with it.
template <typename T>
struct measured {
    T value{};
    bool available = false;
    /// Why the value is missing. Only read when `available` is false; a
    /// non-empty reason on an available value is a defect, not a note.
    std::string reason;

    static measured present(T v)
    {
        measured m;
        m.value = v;
        m.available = true;
        return m;
    }

    static measured unavailable(std::string why)
    {
        measured m;
        m.available = false;
        m.reason = std::move(why);
        return m;
    }
};

/// The configuration the run used, in full (§8.1). Every physical local-plane
/// value is printed with `provisional` set, because D15 leaves them open
/// pending SRAM-macro, frequency and PD inputs and every report that quotes
/// them has to say so.
struct config_record {
    std::string id;
    std::uint32_t xlen = 32;
    std::uint32_t vlen_bits = 512;
    std::uint32_t elen_bits = 64;
    std::string rvv_version = "1.0";
    /// Read back from the instantiated engine, never written as a literal.
    ///
    /// D28's promotion gate requires a report to obtain the MXU identity from
    /// the live component, and the reason is not procedural: the geometry is a
    /// compile-time template parameter, so a binary built for another array
    /// would otherwise emit rows naming the one nobody built. The source
    /// revision is the engine's own `profile@hash`, which is what makes the
    /// geometry attributable rather than merely stated.
    std::string mxu_geometry;
    std::string mxu_datatype;
    std::string mxu_source_revision;
    /// D28: "Result schemas must report `dma_controllers=1` separately from
    /// `dma_channels=2|4`." Channels are contexts inside one controller
    /// sharing one external AXI boundary, so collapsing the two into one field
    /// would let a four-channel row read as four DMA engines and four times
    /// the external bandwidth.
    std::uint32_t dma_controllers = 1;
    std::uint32_t dma_channels = 1;
    std::uint64_t dma_max_burst_bytes = 2048;
    std::uint32_t external_axi_data_width_bits = 64;
    std::uint64_t sram_capacity_bytes = 0;
    std::uint32_t bank_width_bits = 0;
    std::uint32_t bank_count = 0;
    std::uint32_t pipeline_stages = 0;
    std::uint32_t outstanding_per_requester = 0;
    std::string bank_mapping;
    std::string arbitration = "round_robin";
    std::string timing_mode;
    double core_period_ns = 0.0;
    bool provisional = true;

    /// Where each configuration field above came from, field name to one of
    /// `live_readback`, `configured` or `structural_literal`.
    ///
    /// D28 forbids a report that reproduces "literals from the profile factory
    /// and calls that readback". The defensible answer is not to claim every
    /// field is live — some cannot be, because the model has no accessor for
    /// them yet — but to say which is which, in the row, so a consumer can see
    /// the difference instead of assuming it.
    ///
    /// `live_readback`      read from the instantiated component
    /// `configured`         the value this run was constructed with
    /// `structural_literal` fixed in the source; no component exposes it
    std::vector<std::pair<std::string, std::string>> identity_provenance;
};

/// Identity and reproducibility (§8.1).
struct identity_record {
    benchmark_id benchmark = benchmark_id::relu;
    std::size_t case_index = 0;
    case_shape shape;
    std::string datatype = "int32";
    bench_mode mode = bench_mode::kernel_only;
    bench_impl implementation = bench_impl::scalar;
    input_pattern pattern = input_pattern::mixed;
    std::uint64_t seed = 0;
    std::string firmware_elf_path;
    std::string firmware_elf_sha256;
    std::string build_type;
    std::string source_revision;
    /// Non-zero only on a deliberate negative-control run. Present in every row
    /// so that a control cannot be mistaken for a measurement.
    std::uint32_t fault_flags = 0;
    std::string injected_fault;
};

/// Correctness (§8.1). This is what the row is worth: a timing number attached
/// to a wrong result is not a slower correct machine.
struct correctness_record {
    bool passed = false;
    /// Elements actually compared, which is every element: the scan does not
    /// stop at the first difference, so this figure and `mismatch_count`
    /// describe the same pass.
    std::uint64_t elements_compared = 0;
    /// How many differ. One wrong element and every element wrong are
    /// different defects, and a first-mismatch index cannot separate them.
    std::uint64_t mismatch_count = 0;
    std::uint64_t first_mismatch_index = 0;
    bool has_mismatch = false;
    std::int32_t expected = 0;
    std::int32_t observed = 0;
    std::uint32_t golden_checksum = 0;
    std::uint32_t guest_checksum = 0;
    input_profile profile;
    std::string detail;
};

/// The measured interval and what the hart did inside it (§8.2).
struct hart_record {
    double interval_begin_ns = 0.0;
    double interval_end_ns = 0.0;
    bool interval_closed = false;
    measured<double> elapsed_ns;
    measured<std::uint64_t> retired_instructions;
    measured<std::uint64_t> mcycle_delta;
    measured<std::uint64_t> scalar_instructions;
    measured<std::uint64_t> vector_instructions;

    // Guest-reported vector geometry, and what is derived from it.
    measured<std::uint32_t> vlenb;
    /// What `vsetvli rd, x0, e32, m1` returned in the guest: the vector length
    /// the machine grants, not the one a configuration file claims.
    measured<std::uint32_t> vlmax;
    measured<std::uint32_t> vl_first;
    measured<std::uint32_t> vl_last;
    measured<std::uint32_t> vector_iterations;
    measured<std::uint64_t> active_elements;
    measured<std::uint64_t> tail_elements;
    measured<double> lane_utilization;
    std::string derivation;

    std::uint64_t local_requests = 0;
    std::uint64_t local_bytes = 0;
    std::uint64_t control_requests = 0;
    std::uint64_t control_bytes = 0;
    std::uint64_t external_requests = 0;
    std::uint64_t external_bytes = 0;
};

/// DMA and external memory (§8.3).
///
/// The direction split is `measured<>` rather than a plain number because the
/// DMA does not keep one. `BYTES_DONE` is a per-job register that the engine
/// resets when it accepts a descriptor, so it cannot be differenced over an
/// interval; `local_bytes` and `external_bytes` are lifetime path totals and
/// each leg touches both paths once. What the runner does instead is derive
/// the split from the benchmark's known transfer structure and then check the
/// derivation against those measured totals — and report `unavailable` when
/// the check does not hold, rather than a plausible number nothing confirmed.
struct dma_record {
    measured<std::uint64_t> bytes_in;
    measured<std::uint64_t> bytes_out;
    measured<std::uint64_t> bytes_total;
    /// Bytes that crossed each path, measured. Each leg of a transfer touches
    /// the local path once and the external path once, so for a two-leg
    /// end-to-end run these are twice one leg — they are not a direction
    /// split and must not be quoted as one.
    std::uint64_t local_path_bytes = 0;
    std::uint64_t external_path_bytes = 0;
    std::uint64_t transfers = 0;
    std::uint64_t chunks = 0;
    std::uint64_t local_requests = 0;
    std::uint64_t external_requests = 0;
    std::uint64_t errors = 0;
    measured<double> average_chunk_bytes;
    measured<double> achieved_bytes_per_cycle;
    measured<double> busy_cycles;
    /// Cycles spent actually moving bytes, as distinct from busy (§8.3). An
    /// absent key would leave a consumer unable to tell "the model does not
    /// measure this" from "the writer forgot it".
    measured<double> service_cycles;
    measured<double> wait_cycles;
    measured<double> overlap_with_compute;
    measured<std::string> external_memory_model;
};

/// One requester's local-fabric traffic (§8.4).
struct fabric_requester_record {
    std::string requester;
    std::uint64_t requests = 0;
    std::uint64_t physical_beats = 0;
    std::uint64_t bank_conflicts = 0;
    std::uint64_t arbitration_events = 0;
    std::uint64_t transferred_bytes = 0;
    std::uint64_t errors = 0;
    double total_latency_ns = 0.0;
};

struct fabric_record {
    std::vector<fabric_requester_record> requesters;
    std::vector<std::vector<std::uint64_t>> bank_grants; ///< [bank][requester]
    std::vector<std::string> bank_grant_requesters;
    measured<double> achieved_bytes_per_cycle;
    measured<double> percent_of_configured_peak;
    /// §8.4 asks for arbitration wait cycles beside the bank-conflict count,
    /// and for both peak and average outstanding. All three are listed here so
    /// that a row states what the model cannot measure rather than omitting
    /// the key and leaving the gap invisible.
    measured<double> arbitration_wait_cycles;
    measured<std::uint64_t> peak_outstanding;
    measured<double> average_outstanding;
};

/// MXU (§8.5). Every field is unavailable for a benchmark that does not use it,
/// and the reason says which — "not exercised by this benchmark" is a different
/// statement from "not measurable".
struct mxu_record {
    measured<std::uint64_t> prefetch_cycles;
    measured<std::uint64_t> source_compute_cycles;
    measured<std::uint64_t> writeback_cycles;
    measured<double> array_utilization;
    measured<std::uint64_t> operand_bytes;
    measured<std::uint64_t> result_bytes;
    measured<std::uint64_t> native_requests;
    measured<double> useful_operations_per_cycle;
};

/// One accounting identity and the two sides that must agree (G3).
///
/// Reported rather than merely asserted, and reported whether or not it
/// balances. A conservation check that only appears when it fails leaves a
/// passing row unable to say *what* was reconciled — and the plan's G3 clause
/// is about the reconciliation existing, not about a run happening not to
/// trip it.
struct conservation_identity {
    std::string name;
    std::string left_label;
    std::uint64_t left = 0;
    std::string right_label;
    std::uint64_t right = 0;
    bool balanced = false;
    /// Why these two quantities must be equal, in one sentence. A reader who
    /// does not already know the model has no way to judge an identity whose
    /// justification lives only in the code that computed it.
    std::string basis;
};

/// One stage of the run, timed from the guest's own marks (G3, clause 4).
struct stage_span {
    std::string stage;
    double begin_ns = 0.0;
    double end_ns = 0.0;
    double duration_ns = 0.0;
};

/// One check that failed, and what kind of thing it was.
struct row_failure {
    std::string kind;
    std::string detail;
};

/// The whole row.
struct result_row {
    std::string schema = "tpu-v3-neo-core-microbench-row-v1";
    std::string scope = "standalone_neo_core";
    bool chip_composition = false;
    bool noc_instantiated = false;

    /// True only when **every** check passed, identity gates included.
    ///
    /// Separate from `correctness.passed`, and above it, because the two
    /// answer different questions: correctness says the numbers match the
    /// golden, `run_valid` says the run that produced them was one whose
    /// results may be used at all. A row whose live VLEN disagreed with its
    /// build has a correct arithmetic result from a machine that is not the
    /// one the row claims to describe.
    ///
    /// An aggregation reads JSON, not exit codes. Before this field existed a
    /// run that exited non-zero still wrote `"passed": true`, and nothing in
    /// the file said otherwise.
    bool run_valid = false;
    /// Every failed check, so a rejected row explains itself without its
    /// stderr.
    std::vector<row_failure> failures;

    /// The G3 accounting identities, balanced or not.
    std::vector<conservation_identity> conservation;
    /// Where the measured interval went, by stage. Derived from the guest's
    /// stage marks, which is why it is an account of the interval rather than
    /// a second opinion about its length.
    std::vector<stage_span> stage_timing;

    identity_record identity;
    config_record configuration;
    correctness_record correctness;
    hart_record hart;
    dma_record dma;
    fabric_record fabric;
    mxu_record mxu;

    /// Reporting limits reproduced into every row (README "Reporting limits",
    /// §12). A row that travels to a spreadsheet without them is a row whose
    /// caveats did not travel with it.
    std::vector<std::string> accuracy_notes;
};

/// Return an empty string when the provenance map covers every serialized
/// configuration field exactly once and uses only the documented labels.
/// Otherwise return a diagnostic suitable for a test failure or exception.
std::string validate_configuration_provenance(const config_record& configuration);

/// Serialise as JSON. Deterministic key order, so two rows diff cleanly.
void write_json(std::ostream& out, const result_row& row);

/// The accuracy notes every standalone row carries.
std::vector<std::string> standard_accuracy_notes(bench_mode mode);

} // namespace cdc::components::tpu_v3::bench

#endif // CDC_COMPONENTS_TPU_V3_BENCH_RESULT_ROW_H
