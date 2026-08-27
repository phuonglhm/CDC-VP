// SPDX-License-Identifier: Apache-2.0
//
// The standalone NEO-CORE microbenchmark runner (G2, §11).
//
// One invocation runs one fresh case on one Phase 7 `tpu_core` and writes one
// machine-readable result row. Fresh is the operative word: a process per case
// is what makes state leaking between configurations impossible rather than
// unlikely, which is why the plan asks for it and why this does not grow a
// loop over cases.
//
// Scope, from D27: one core at the retained chip-index-0/core-index-0 address
// slot, `mhartid` 0, the external socket bound directly to boot ROM, global RAM
// and simulator-only host I/O. No chip, no NoC — enforced at configure time by
// the target-graph check and after linking by the demangled-symbol guard, both
// shared with the G1 baseline rather than reimplemented here.
//
// ── on the command line, and where it departs from the plan ─────────────────
//
// §11 sketches `--config <config.yaml>`. This runner takes the knobs as
// explicit flags instead and records every one of them in the row. The reason
// is not convenience: inventing a YAML dialect now would mean writing a parser
// and a schema for a file format whose contents G4 has not yet decided, and a
// half-specified configuration file is exactly the place where a value nobody
// chose becomes an architectural constant by accident. The flags cover the
// knobs §7.2 lists as configurable *today*; anything else is refused rather
// than accepted and ignored.
//
// Exit codes: 0 pass, 1 fail, 2 usage error, 77 skip (image not built).

#include "tpu_v3/core/tpu_core.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/bench/benchmark_case.h"
#include "tpu_v3/bench/golden.h"
#include "tpu_v3/bench/result_row.h"
#include "tpu_v3/bench/sha256.h"

#include "bench_world.h"

extern "C" {
#include "bench_map.h"
}

namespace tpu = cdc::components::tpu_v3;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace neo_sauria = cdc::components::tpu_v3::sauria;
namespace am = cdc::components::tpu_v3::address_map;
namespace bench = cdc::components::tpu_v3::bench;

using core::hart_destination;
using core::local_fabric_timing;
using core::tpu_core;
using core::tpu_core_config;
using bench::bench_world;
using bench::default_global_ram_bytes;
using sram::neo_requester;
using sram::neo_requester_count;

namespace {

constexpr int kPass = 0;
constexpr int kFail = 1;
constexpr int kUsage = 2;
constexpr int kSkip = 77;

constexpr tpu::chip_id_t kChip = BENCH_CHIP_ID;
constexpr tpu::core_id_t kCore = BENCH_CORE_ID;

// ── options ──────────────────────────────────────────────────────────────────

/// Why a check failed.
///
/// Failures are classified rather than collected as free text because a
/// negative control has to distinguish "the mutation was detected" from "the
/// run fell over somewhere else and happened to also trip this check". Without
/// the classification a control passes when the harness itself breaks — which
/// is precisely the evidence a control exists to rule out.
enum class failure_kind {
    /// The guest did not run to completion, or ran with the wrong identity or
    /// the wrong parameters. Nothing downstream of it means anything.
    guest,
    /// An accounting identity did not balance (G3). The numbers may each be
    /// individually plausible and still not describe one consistent run, which
    /// is exactly the condition that makes a counter unquotable.
    conservation,
    /// The measured interval did not open, did not close, or opened twice.
    interval,
    /// The result disagrees with the independently computed host golden.
    golden,
    /// The runner or its inputs are at fault, not the machine under test.
    harness,
};

const char* to_string(failure_kind kind)
{
    switch (kind) {
    case failure_kind::guest: return "guest";
    case failure_kind::conservation: return "conservation";
    case failure_kind::interval: return "interval";
    case failure_kind::golden: return "golden";
    case failure_kind::harness: return "harness";
    }
    return "unknown";
}

struct failure {
    failure_kind kind;
    std::string why;
};

/// What a negative control expects the runner to detect.
///
/// A control asserts two things, and the second is the one that was missing
/// when this was first written: the named detection fired, **and** nothing
/// else did. A run whose guest never reached the kernel also mismatches the
/// golden — the poisoned destination guarantees it — so "golden_mismatch
/// fired" alone is satisfied by a machine that never executed the mutation at
/// all.
enum class expected_detection {
    none,
    golden_mismatch,
    interval_not_closed,
    conservation_broken,
};

const char* to_string(expected_detection detection)
{
    switch (detection) {
    case expected_detection::none: return "none";
    case expected_detection::golden_mismatch: return "golden_mismatch";
    case expected_detection::interval_not_closed: return "interval_not_closed";
    case expected_detection::conservation_broken: return "conservation_broken";
    }
    return "unknown";
}

/// The failure kind a named detection produces.
failure_kind permitted_kind(expected_detection detection)
{
    switch (detection) {
    case expected_detection::interval_not_closed: return failure_kind::interval;
    case expected_detection::conservation_broken:
        return failure_kind::conservation;
    default: return failure_kind::golden;
    }
}

/// A usage refusal a control can require, so "the runner said no" is checkable
/// rather than merely "the process exited non-zero".
enum class expected_usage {
    none,
    unimplemented_benchmark,
    dma_fault_needs_end_to_end,
};

const char* to_string(expected_usage usage)
{
    switch (usage) {
    case expected_usage::none: return "none";
    case expected_usage::unimplemented_benchmark:
        return "unimplemented_benchmark";
    case expected_usage::dma_fault_needs_end_to_end:
        return "dma_fault_needs_end_to_end";
    }
    return "unknown";
}

struct options {
    std::string elf_path;
    std::string result_path;
    bench::benchmark_id benchmark = bench::benchmark_id::relu;
    std::size_t case_index = 0;
    bench::bench_mode mode = bench::bench_mode::kernel_only;
    bench::bench_impl implementation = bench::bench_impl::rvv;
    bench::input_pattern pattern = bench::input_pattern::mixed;
    std::uint64_t seed = 1;

    /// Perturb one named conservation identity by a single byte before it is
    /// compared.
    ///
    /// This mutates the *check's input*, not the model, and the distinction is
    /// stated rather than blurred: for `dma_leg_bytes` there is a real
    /// model-side mutation (skip a DMA leg) and it is used. For the other
    /// identities the model offers no way to break the correspondence — two
    /// honest observers of the same bytes cannot be made to disagree from
    /// outside — so what remains provable is that the identity would notice if
    /// they did. G3 asks for a mutation control per counter; this is the
    /// strongest one available for those three, and calling it anything more
    /// would overstate it.
    /// Perturb one named *counter source* by a single byte where it enters an
    /// identity.
    ///
    /// Finer than `--inject-accounting`, and it proves a different thing.
    /// Perturbing an identity's left-hand total shows the equality check
    /// works; several identities sum more than one counter, and a mutation on
    /// the sum cannot tell whether each term is actually wired in. A term that
    /// was dropped, doubled or transposed would leave the aggregate mutation
    /// still detected and the defect still present.
    ///
    /// Still comparator-side: it mutates what the identity is given, not what
    /// the machine did. Only `kernel_reads_input_twice`, `stage_marker_dropped`
    /// and the DMA-leg controls do the latter.
    std::string counter_mutation;
    /// Print every counter source this run hooks, then exit.
    ///
    /// The list is the runner's own, collected as the identities execute, so a
    /// control matrix can be checked against what the code actually reads
    /// rather than against a list someone maintained by hand. That gap has
    /// already been found twice — once as a missing label, once as three
    /// missing terms — and both times by a reviewer rather than by the suite.
    bool list_counter_sources = false;
    std::string accounting_mutation;
    std::string injected_fault;
    std::uint32_t fault_flags = 0;
    /// Every detection this control requires. All must fire, and nothing
    /// outside their kinds may. A mutation is often visible to more than one
    /// check — skipping a DMA leg breaks the golden *and* the byte accounting
    /// — and asserting only one of them leaves the other unproven.
    std::vector<expected_detection> expect;
    expected_usage expect_usage = expected_usage::none;

    // §7.2 knobs that are configurable today.
    local_fabric_timing timing = local_fabric_timing::annotated;
    std::uint32_t bank_width_bits = 128;
    std::uint32_t bank_count = 4;
    std::uint32_t pipeline_stages = 2;
    // The plan's §7.1 reference value is the whole 16 MiB window (D6). It is
    // free to instantiate: core SRAM is sparse 4 KiB page-backed storage, so
    // an unwritten window costs nothing. The previous 64 KiB default was a
    // bring-up number that rows nonetheless labelled `reference`.
    std::uint64_t sram_capacity_bytes = am::core_sram_default_capacity;
    std::uint64_t dma_max_burst_bytes = 2048;
    double core_period_ns = 10.0;

    std::string config_id = "reference";
    /// The VLEN this build is expected to contain, in bits. Not a knob: VP++
    /// compiles it in, so this is what the guest readback is checked against
    /// rather than a value anyone may select.
    std::uint32_t vlen_bits_expected = 512;
    /// The MXU geometry this build is expected to contain, `RxC`. Compile-time
    /// in the current source, so like VLEN it is checked, never selected.
    std::string mxu_geometry_expected = "64x64";
    std::string build_type;
    std::string source_revision;
    std::string source_revision_file;
    double watchdog_ms = 200.0;
    bool quiet = false;
};

void usage(const char* program)
{
    std::cerr
        << "usage: " << program << " --elf <image.elf>\n"
        << "  --benchmark relu|vector_dot|gemv_rvv|gemv_mxu|gemm\n"
        << "  --case 0..9                 the frozen case index\n"
        << "  --mode kernel|end_to_end\n"
        << "  --impl scalar|rvv|mxu\n"
        << "  [--pattern mixed|all_negative|all_positive|zero|boundary]\n"
        << "  [--seed N] [--result row.json]\n"
        << "  [--inject arith|dma_in|dma_out|boundary|extra_read|"
           "skip_stage]\n"
        << "  [--inject-accounting <conservation identity name>]\n"
        << "  [--inject-counter <counter source name>]\n"
        << "  [--list-counter-sources]\n"
        << "  [--expect-detect <detection>[,<detection>...]]  "
           "golden_mismatch,\n"
        << "        interval_not_closed, conservation_broken\n"
        << "  [--expect-usage unimplemented_benchmark|"
           "dma_fault_needs_end_to_end]\n"
        << "  [--timing annotated|arbitrated]\n"
        << "  [--bank-width-bits N] [--bank-count N] [--pipeline-stages N]\n"
        << "  [--sram-capacity-bytes N] [--dma-max-burst N] "
           "[--core-period-ns X]\n"
        << "  [--config-id NAME] [--build-type NAME]\n"
        << "  [--expect-vlen-bits N] [--expect-mxu-geometry RxC]\n"
        << "  [--source-revision REV | --source-revision-file PATH]\n"
        << "  [--watchdog-ms X] [--quiet]\n";
}

/// Parse an unsigned integer, refusing everything that is not one.
///
/// `std::stoull` alone is not enough and the gap is not theoretical: it accepts
/// a leading minus and returns the two's-complement wrap, so `--seed -1`
/// silently becomes 18446744073709551615. It also accepts leading whitespace
/// and a `+`. The sign check therefore happens on the text, before conversion.
///
/// `limit` is the largest value the destination can hold. Checking it here
/// rather than at the cast is the difference between refusing 4294967297 and
/// quietly running with a bank count of 1.
bool parse_unsigned(const std::string& text, std::uint64_t limit,
                    std::uint64_t& out)
{
    if (text.empty()) {
        return false;
    }
    for (char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    try {
        std::size_t consumed = 0;
        const unsigned long long value = std::stoull(text, &consumed, 10);
        if (consumed != text.size() || value > limit) {
            return false;
        }
        out = static_cast<std::uint64_t>(value);
        return true;
    } catch (const std::exception&) {
        // out_of_range for anything past 64 bits.
        return false;
    }
}

/// Parse a strictly positive, finite double.
///
/// `std::stod` accepts "nan", "inf" and "-inf". A NaN watchdog compares false
/// against every bound, so the run would proceed with a simulation limit that
/// is not a number — and `sc_time(NaN)` is not a diagnosable failure.
bool parse_positive_double(const std::string& text, double& out)
{
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (consumed != text.size() || !std::isfinite(value) || value <= 0.0) {
            return false;
        }
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

/// Returns false on a usage error, having already explained it.
bool parse_options(int argc, char* argv[], options& out)
{
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        const auto need_value = [&](std::string& value) {
            if (i + 1 >= argc) {
                std::cerr << flag << " needs a value\n";
                return false;
            }
            value = argv[++i];
            return true;
        };

        std::string value;
        if (flag == "--elf") {
            if (!need_value(value)) return false;
            out.elf_path = value;
        } else if (flag == "--result") {
            if (!need_value(value)) return false;
            out.result_path = value;
        } else if (flag == "--benchmark") {
            if (!need_value(value)) return false;
            if (!bench::parse(value, out.benchmark)) {
                std::cerr << "unknown benchmark '" << value << "'\n";
                return false;
            }
        } else if (flag == "--case") {
            if (!need_value(value)) return false;
            std::uint64_t index = 0;
            if (!parse_unsigned(value, bench::frozen_case_count - 1, index)) {
                std::cerr << "--case must be 0.." << bench::frozen_case_count - 1
                          << ", got '" << value << "'\n";
                return false;
            }
            out.case_index = static_cast<std::size_t>(index);
        } else if (flag == "--mode") {
            if (!need_value(value)) return false;
            if (!bench::parse(value, out.mode)) {
                std::cerr << "unknown mode '" << value << "'\n";
                return false;
            }
        } else if (flag == "--impl") {
            if (!need_value(value)) return false;
            if (!bench::parse(value, out.implementation)) {
                std::cerr << "unknown implementation '" << value << "'\n";
                return false;
            }
        } else if (flag == "--pattern") {
            if (!need_value(value)) return false;
            if (!bench::parse(value, out.pattern)) {
                std::cerr << "unknown input pattern '" << value << "'\n";
                return false;
            }
        } else if (flag == "--seed") {
            if (!need_value(value)) return false;
            if (!parse_unsigned(value, UINT64_MAX, out.seed)) {
                std::cerr << "--seed must be a non-negative integer\n";
                return false;
            }
        } else if (flag == "--inject-counter") {
            if (!need_value(value)) return false;
            out.counter_mutation = value;
        } else if (flag == "--inject-accounting") {
            if (!need_value(value)) return false;
            out.accounting_mutation = value;
        } else if (flag == "--inject") {
            if (!need_value(value)) return false;
            out.injected_fault = value;
            if (value == "arith") {
                out.fault_flags |= BENCH_FAULT_CORRUPT_ARITH;
            } else if (value == "dma_in") {
                out.fault_flags |= BENCH_FAULT_SKIP_DMA_IN;
            } else if (value == "dma_out") {
                out.fault_flags |= BENCH_FAULT_SKIP_DMA_OUT;
            } else if (value == "boundary") {
                out.fault_flags |= BENCH_FAULT_SKIP_END_MARK;
            } else if (value == "extra_read") {
                out.fault_flags |= BENCH_FAULT_EXTRA_READ;
            } else if (value == "skip_stage") {
                out.fault_flags |= BENCH_FAULT_SKIP_STAGE;
            } else {
                std::cerr << "unknown fault '" << value << "'\n";
                return false;
            }
        } else if (flag == "--expect-detect") {
            if (!need_value(value)) return false;
            // Comma-separated, so a control can require every detection a
            // mutation should trip rather than the first one someone thought
            // of.
            std::size_t start = 0;
            while (start <= value.size()) {
                const std::size_t comma = value.find(',', start);
                const std::string name = value.substr(
                    start, comma == std::string::npos ? std::string::npos
                                                      : comma - start);
                if (name == "golden_mismatch") {
                    out.expect.push_back(expected_detection::golden_mismatch);
                } else if (name == "interval_not_closed") {
                    out.expect.push_back(
                        expected_detection::interval_not_closed);
                } else if (name == "conservation_broken") {
                    out.expect.push_back(
                        expected_detection::conservation_broken);
                } else {
                    std::cerr << "unknown detection '" << name << "'\n";
                    return false;
                }
                if (comma == std::string::npos) {
                    break;
                }
                start = comma + 1;
            }
            if (out.expect.empty()) {
                std::cerr << "--expect-detect needs at least one detection\n";
                return false;
            }
        } else if (flag == "--timing") {
            if (!need_value(value)) return false;
            if (value == "annotated") {
                out.timing = local_fabric_timing::annotated;
            } else if (value == "arbitrated") {
                out.timing = local_fabric_timing::arbitrated;
            } else {
                std::cerr << "unknown timing mode '" << value << "'\n";
                return false;
            }
        } else if (flag == "--bank-width-bits" || flag == "--bank-count"
                   || flag == "--pipeline-stages"
                   || flag == "--sram-capacity-bytes"
                   || flag == "--dma-max-burst") {
            if (!need_value(value)) return false;
            // The limit is the destination's, so an out-of-range value is
            // refused rather than truncated into a different configuration
            // than the one the row will claim was run.
            const bool narrow = flag != "--sram-capacity-bytes"
                && flag != "--dma-max-burst";
            std::uint64_t number = 0;
            if (!parse_unsigned(value, narrow ? UINT32_MAX : UINT64_MAX,
                                number)
                || number == 0) {
                std::cerr << flag
                          << " must be a positive integer that fits its field ("
                          << (narrow ? "32-bit" : "64-bit") << ")\n";
                return false;
            }
            if (flag == "--bank-width-bits") {
                out.bank_width_bits = static_cast<std::uint32_t>(number);
            } else if (flag == "--bank-count") {
                out.bank_count = static_cast<std::uint32_t>(number);
            } else if (flag == "--pipeline-stages") {
                out.pipeline_stages = static_cast<std::uint32_t>(number);
            } else if (flag == "--sram-capacity-bytes") {
                out.sram_capacity_bytes = number;
            } else {
                out.dma_max_burst_bytes = number;
            }
        } else if (flag == "--core-period-ns" || flag == "--watchdog-ms") {
            if (!need_value(value)) return false;
            double number = 0.0;
            if (!parse_positive_double(value, number)) {
                std::cerr << flag
                          << " must be a finite positive number (nan and inf "
                             "are refused)\n";
                return false;
            }
            if (flag == "--core-period-ns") {
                out.core_period_ns = number;
            } else {
                out.watchdog_ms = number;
            }
        } else if (flag == "--expect-vlen-bits") {
            if (!need_value(value)) return false;
            std::uint64_t number = 0;
            if (!parse_unsigned(value, UINT32_MAX, number) || number == 0
                || number % 8 != 0) {
                std::cerr << "--expect-vlen-bits must be a positive multiple "
                             "of 8\n";
                return false;
            }
            out.vlen_bits_expected = static_cast<std::uint32_t>(number);
        } else if (flag == "--expect-mxu-geometry") {
            if (!need_value(value)) return false;
            if (value.find('x') == std::string::npos) {
                std::cerr << "--expect-mxu-geometry takes RxC, for example "
                             "64x64\n";
                return false;
            }
            out.mxu_geometry_expected = value;
        } else if (flag == "--config-id") {
            if (!need_value(value)) return false;
            out.config_id = value;
        } else if (flag == "--build-type") {
            if (!need_value(value)) return false;
            out.build_type = value;
        } else if (flag == "--source-revision") {
            if (!need_value(value)) return false;
            out.source_revision = value;
        } else if (flag == "--source-revision-file") {
            if (!need_value(value)) return false;
            out.source_revision_file = value;
        } else if (flag == "--expect-usage") {
            if (!need_value(value)) return false;
            if (value == "unimplemented_benchmark") {
                out.expect_usage = expected_usage::unimplemented_benchmark;
            } else if (value == "dma_fault_needs_end_to_end") {
                out.expect_usage = expected_usage::dma_fault_needs_end_to_end;
            } else {
                std::cerr << "unknown usage refusal '" << value << "'\n";
                return false;
            }
        } else if (flag == "--list-counter-sources") {
            out.list_counter_sources = true;
        } else if (flag == "--quiet") {
            out.quiet = true;
        } else {
            std::cerr << "unknown option '" << flag << "'\n";
            return false;
        }
    }
    return true;
}

// ── the world outside the core ───────────────────────────────────────────────

// ── sampling ─────────────────────────────────────────────────────────────────

/// Everything the row needs, captured at one instant.
///
/// Taken twice — at the opening and closing marker — and reported as deltas.
/// Absolute counters would include the firmware's start-up, its parameter read
/// and, in the closing direction, its verification pass; none of those is the
/// workload.
struct sample {
    bool taken = false;
    double time_ns = 0.0;
    std::uint64_t instret = 0;
    std::uint64_t mcycle = 0;

    std::uint64_t hart_requests[core::hart_destination_count] = {};
    std::uint64_t hart_bytes[core::hart_destination_count] = {};

    core::requester_counters fabric[neo_requester_count];
    /// `[bank][requester]`, sized from the configuration.
    ///
    /// A fixed array with a `min(bank_count, N)` clamp was here first, and it
    /// produced a row that stated `local_bank_count: 32` next to grants for
    /// banks 0..15 — a result that is wrong in the direction nobody checks,
    /// because it looks complete.
    std::vector<std::array<std::uint64_t, neo_requester_count>> bank_grants;

    std::uint64_t dma_bytes_done = 0;
    std::uint64_t dma_local_bytes = 0;
    std::uint64_t dma_external_bytes = 0;
    std::uint64_t dma_transfers = 0;
    std::uint64_t dma_chunks = 0;
    std::uint64_t dma_local_requests = 0;
    std::uint64_t dma_external_requests = 0;
    std::uint64_t dma_errors = 0;

    /// Core SRAM's own view of workload traffic. The debug path does not move
    /// these (`counted=false` in `core_sram::perform`), which is what makes
    /// them comparable with the fabric's requester totals: both count the same
    /// bytes from opposite ends of the same plane.
    std::uint64_t sram_bytes_read = 0;
    std::uint64_t sram_bytes_written = 0;

    /// What the external memory says it served. Counted outside the model, so
    /// reconciling it against the core's external counters compares two
    /// independent observers rather than the model against itself.
    std::uint64_t world_memory_read_bytes = 0;
    std::uint64_t world_memory_write_bytes = 0;
    std::uint64_t world_memory_requests = 0;
    std::uint64_t world_host_io_bytes = 0;

    std::uint64_t mxu_jobs = 0;
    std::uint64_t mxu_local_requests = 0;
    std::uint64_t mxu_local_bytes = 0;
    std::uint64_t mxu_committed_bytes = 0;
    neo_sauria::sauria_matrix_if::timing mxu_timing;
};

sample take_sample(tpu_core& neo, const bench_world& outside,
                   double time_ns)
{
    sample s;
    s.taken = true;
    s.time_ns = time_ns;
    s.instret = neo.cpu().get_instret();
    s.mcycle = neo.cpu().read_mcycle();

    const auto& hart = neo.hart_port();
    for (unsigned i = 0; i < core::hart_destination_count; ++i) {
        const auto destination = static_cast<hart_destination>(i);
        s.hart_requests[i] = hart.requests(destination);
        s.hart_bytes[i] = hart.bytes(destination);
    }

    const auto& fabric = neo.fabric();
    for (unsigned i = 0; i < neo_requester_count; ++i) {
        const auto requester = static_cast<neo_requester>(i);
        if (fabric.is_attached(requester)) {
            s.fabric[i] = fabric.counters(requester);
        }
    }
    s.bank_grants.assign(fabric.config().bank_count,
                         std::array<std::uint64_t, neo_requester_count>{});
    for (unsigned bank = 0; bank < fabric.config().bank_count; ++bank) {
        for (unsigned i = 0; i < neo_requester_count; ++i) {
            const auto requester = static_cast<neo_requester>(i);
            if (fabric.is_attached(requester)) {
                s.bank_grants[bank][i] = fabric.bank_grants(bank, requester);
            }
        }
    }

    const auto& dma = neo.dma();
    s.dma_bytes_done = dma.bytes_done();
    s.dma_local_bytes = dma.local_bytes();
    s.dma_external_bytes = dma.external_bytes();
    s.dma_transfers = dma.transfer_count();
    s.dma_chunks = dma.chunks_completed();
    s.dma_local_requests = dma.local_requests();
    s.dma_external_requests = dma.external_requests();
    s.dma_errors = dma.error_count();
    s.sram_bytes_read = neo.sram().bytes_read();
    s.sram_bytes_written = neo.sram().bytes_written();
    s.world_memory_read_bytes = outside.memory.read_bytes;
    s.world_memory_write_bytes = outside.memory.write_bytes;
    s.world_memory_requests = outside.memory.requests();
    s.world_host_io_bytes = outside.host_io.bytes();

    s.mxu_jobs = neo.matrix_control().job_count();
    s.mxu_local_requests = neo.matrix_engine().local_requests();
    s.mxu_local_bytes = neo.matrix_engine().local_bytes();
    s.mxu_committed_bytes = neo.matrix_engine().committed_bytes();
    s.mxu_timing = neo.matrix_engine().last_timing();
    return s;
}

/// `b - a`, saturating at zero.
///
/// Saturating rather than wrapping: a negative delta would mean a counter went
/// backwards, which is a defect worth seeing as a zero next to a note rather
/// than as 18 quintillion.
std::uint64_t delta(std::uint64_t before, std::uint64_t after) noexcept
{
    return after >= before ? after - before : 0;
}

// ── the firmware's copy of the map, checked against the C++ one ──────────────

/// Until the Phase 10 generator exists, `bench_map.h` is the second copy of the
/// address map plan §8 warns about. This is the only other way to close the
/// gap: assert every constant the image uses against `address_map.h` before the
/// image is allowed to run, so the two cannot drift silently.
std::vector<std::string> firmware_map_disagreements(
    std::uint64_t sram_capacity_bytes)
{
    std::vector<std::string> problems;
    const auto check = [&](bool ok, const std::string& what) {
        if (!ok) {
            problems.push_back(what);
        }
    };

    check(GLOBAL_RAM_BASE == am::global_ram_base,
          "the firmware's global RAM base disagrees with address_map.h");
    check(BENCH_EXPECTED_MHARTID
              == static_cast<unsigned>(kChip) * 2u + kCore,
          "the firmware's expected mhartid disagrees with chip * 2 + core");
    check(BENCH_CORE_BASE == am::core_base(kChip, kCore),
          "the firmware's core base disagrees with address_map.h");
    check(BENCH_CORE_SRAM_BASE == am::core_sram_base(kChip, kCore),
          "the firmware's core SRAM base disagrees with address_map.h");
    check(BENCH_DMA_CONTROL == am::dma_control(kChip, kCore),
          "the firmware's DMA_CONTROL base disagrees with address_map.h");
    check(BENCH_SA_CONTROL == am::sa_control(kChip, kCore),
          "the firmware's SA_CONTROL base disagrees with address_map.h");

    // The linker script enforces the firmware/host RAM split with its own
    // ASSERT against a literal. This is the other half: that the literal and
    // the header agree. Neither check alone would catch a header edited
    // without the script.
    check(BENCH_HOST_REGION_BASE == 0x80008000u,
          "bench_map.h's host region base has moved away from the value "
          "link.ld asserts against");
    check(BENCH_HOST_REGION_END - am::global_ram_base <= default_global_ram_bytes,
          "the staged host buffers do not fit inside the global RAM this "
          "runner instantiates");
    constexpr std::uint64_t sram_extent
        = BENCH_SRAM_DST_ADDR + BENCH_SRAM_BUF_BYTES - BENCH_CORE_SRAM_BASE;
    check(sram_extent <= am::core_sram_window,
          "the core SRAM buffers do not fit inside the core SRAM window");
    // The window always decodes; the capacity is what is backed, and an access
    // inside the window but above the capacity is an error response rather
    // than an alias down into valid storage (D6, ADDRESS_MAP.md §5). A
    // configuration too small for the buffers is therefore a configuration
    // error to report here, not a run to start and watch fail per element.
    check(sram_extent <= sram_capacity_bytes,
          "the core SRAM buffers do not fit inside the instantiated core SRAM "
          "capacity; raise --sram-capacity-bytes");

    // Every frozen case of every benchmark, not only the one being run.
    //
    // The buffers are shared across MB1-MB4 and the case tables are already
    // frozen, so a case that does not fit is a defect the moment the sizes are
    // chosen — not the moment someone implements the benchmark and watches its
    // operands overlap the destination. Checking the whole table here is what
    // stops that being discovered by a wrong result.
    for (auto benchmark :
         {bench::benchmark_id::relu, bench::benchmark_id::vector_dot,
          bench::benchmark_id::gemv_rvv, bench::benchmark_id::gemv_mxu,
          bench::benchmark_id::gemm}) {
        for (std::size_t index = 0; index < bench::frozen_case_count; ++index) {
            const auto shape = bench::case_at(benchmark, index);
            const std::uint64_t in = bench::input_bytes(benchmark, shape);
            const std::uint64_t out = bench::output_bytes(benchmark, shape);
            const std::string where = std::string(bench::to_string(benchmark))
                + " case " + std::to_string(index);
            check(in <= BENCH_HOST_BUF_BYTES && in <= BENCH_SRAM_BUF_BYTES,
                  where + " needs " + std::to_string(in)
                      + " operand bytes, more than the "
                      + std::to_string(BENCH_HOST_BUF_BYTES)
                      + "-byte source buffers hold");
            check(out <= BENCH_HOST_BUF_BYTES && out <= BENCH_SRAM_BUF_BYTES,
                  where + " needs " + std::to_string(out)
                      + " result bytes, more than the "
                      + std::to_string(BENCH_HOST_BUF_BYTES)
                      + "-byte destination buffers hold");
        }
    }
    return problems;
}

/// The engine's capability bitmask as a name.
///
/// Rendered from what the engine reports rather than from what the
/// configuration asked for, so a build whose capability bits differ from the
/// expected profile shows up as a different string instead of the same one.
std::string capability_string(std::uint32_t capability)
{
    std::string text;
    const auto add = [&](std::uint32_t bit, const char* name) {
        if ((capability & bit) != 0) {
            if (!text.empty()) {
                text += '+';
            }
            text += name;
        }
    };
    add(cdc::components::tpu_v3::sauria::capability_bit::int8_int32, "int8xint8->int32");
    add(cdc::components::tpu_v3::sauria::capability_bit::bf16_fp32, "bf16xbf16->fp32");
    add(cdc::components::tpu_v3::sauria::capability_bit::fp16_fp32, "fp16xfp16->fp32");
    add(cdc::components::tpu_v3::sauria::capability_bit::accumulate, "accumulate");
    return text.empty() ? std::string("none") : text;
}

/// Configuration identities D28 reserves for the Neo Lite profiles.
///
/// D28 is explicit that "changing a manifest, a result-row default or a
/// configuration ID while the component still uses another value is a
/// configuration defect and must be refused". A row labelled `neo_lite_c1`
/// emitted by the current machine would claim a 32x32 MXU, VLEN 256, 768 KiB
/// of SRAM and a 1.25 ns period from a core that has none of them — and an
/// aggregation would pool it as promoted C1 evidence.
///
/// The profiles are compile-time build profiles (D28's build-profile
/// boundary), so the honest answer today is refusal, not validation: this
/// binary cannot contain either one.
bool is_reserved_profile_id(const std::string& id) noexcept
{
    return id == "neo_lite_c1" || id == "neo_lite_c2";
}

/// Every §7.1 reference value this runner can set, and what `opt` actually
/// asks for. Empty when the two agree.
///
/// A configuration ID is the key an aggregation script groups rows by. A row
/// labelled `reference` that ran 64 KiB of SRAM is worse than a row with no ID
/// at all: it is silently pooled with rows that ran something else, and the
/// difference shows up as variance nobody can attribute.
std::vector<std::string> reference_configuration_mismatches(const options& opt)
{
    std::vector<std::string> problems;
    const auto check = [&](const char* field, std::uint64_t actual,
                           std::uint64_t reference) {
        if (actual != reference) {
            problems.push_back(std::string(field) + ": "
                               + std::to_string(actual) + ", reference is "
                               + std::to_string(reference));
        }
    };
    check("sram_capacity_bytes", opt.sram_capacity_bytes,
          am::core_sram_default_capacity);
    check("local_bank_width_bits", opt.bank_width_bits, 128);
    check("local_bank_count", opt.bank_count, 4);
    check("fabric_pipeline_stages", opt.pipeline_stages, 2);
    check("dma_max_burst_bytes", opt.dma_max_burst_bytes, 2048);
    check("vlen_bits", opt.vlen_bits_expected, 512);
    if (opt.core_period_ns != 10.0) {
        problems.push_back("core_period_ns: " + std::to_string(opt.core_period_ns)
                           + ", reference is 10");
    }
    return problems;
}

tpu_core_config make_core_config(const options& opt)
{
    tpu_core_config config;
    config.chip = kChip;
    config.core = kCore;
    config.sram_capacity_bytes = opt.sram_capacity_bytes;
    config.fabric.data_width_bits = opt.bank_width_bits;
    config.fabric.bank_count = opt.bank_count;
    config.fabric.mapping = tpu::bank_mapping::low_order_interleaved;
    config.fabric.pipeline_stages = opt.pipeline_stages;
    config.fabric.max_outstanding_per_requester = 1;
    config.timing = opt.timing;
    config.cycle = sc_core::sc_time(opt.core_period_ns, sc_core::SC_NS);
    config.dma_max_burst_bytes = opt.dma_max_burst_bytes;
    return config;
}

/// Read core SRAM over the debug path: no arbitration, no latency, no counters
/// — and no relaxation of decode or bounds. Chunked at the 64-byte maximum a
/// single native access may carry.
bool read_sram(tpu_core& neo, std::uint64_t address, void* into,
               std::uint32_t length)
{
    auto* bytes = static_cast<unsigned char*>(into);
    std::uint32_t moved = 0;
    while (moved < length) {
        const std::uint32_t chunk
            = std::min<std::uint32_t>(length - moved, sram::neo_max_transfer_bytes);
        sram::neo_local_request request;
        request.requester = neo_requester::cpu;
        request.command = sram::neo_command::read;
        request.address = address + moved;
        request.size = chunk;
        request.data = bytes + moved;
        const std::uint32_t served = neo.fabric().dbg_access(request);
        if (served != chunk) {
            return false;
        }
        moved += served;
    }
    return true;
}

bool write_sram(tpu_core& neo, std::uint64_t address, const void* from,
                std::uint32_t length)
{
    auto* bytes = const_cast<unsigned char*>(
        static_cast<const unsigned char*>(from));
    std::uint32_t moved = 0;
    while (moved < length) {
        const std::uint32_t chunk
            = std::min<std::uint32_t>(length - moved, sram::neo_max_transfer_bytes);
        sram::neo_local_request request;
        request.requester = neo_requester::cpu;
        request.command = sram::neo_command::write;
        request.address = address + moved;
        request.size = chunk;
        request.data = bytes + moved;
        const std::uint32_t served = neo.fabric().dbg_access(request);
        if (served != chunk) {
            return false;
        }
        moved += served;
    }
    return true;
}

const char* stage_name(std::uint32_t stage)
{
    switch (stage) {
    case BENCH_STAGE_START: return "START";
    case BENCH_STAGE_PARAMS: return "reading the parameter block";
    case BENCH_STAGE_DMA_IN: return "DMA-in";
    case BENCH_STAGE_KERNEL: return "the kernel";
    case BENCH_STAGE_DMA_OUT: return "DMA-out";
    case BENCH_STAGE_VERIFY: return "guest-side verification";
    case BENCH_STAGE_DONE: return "DONE";
    default: return "before the first stage";
    }
}

} // namespace

int sc_main(int argc, char* argv[])
{
    options opt;
    if (!parse_options(argc, argv, opt)) {
        usage(argv[0]);
        return kUsage;
    }

    // ── refuse rather than substitute ────────────────────────────────────────

    // A usage refusal is checkable rather than merely non-zero. `WILL_FAIL`
    // accepts any failure, including one caused by a typo in the test's own
    // command line, so each refusal control names the refusal it expects.
    const auto refuse = [&](expected_usage which, const std::string& why) {
        std::cerr << why << '\n';
        if (opt.expect_usage == which) {
            std::cout << "usage refusal satisfied: '" << to_string(which)
                      << "'\n";
            return kPass;
        }
        if (opt.expect_usage != expected_usage::none) {
            std::cerr << "but the control expected '"
                      << to_string(opt.expect_usage) << "'\n";
        }
        return kUsage;
    };

    if (!bench::benchmark_runnable(opt.benchmark)) {
        return refuse(expected_usage::unimplemented_benchmark,
                      std::string("benchmark '")
                          + bench::to_string(opt.benchmark)
                          + "' has frozen cases but no implementation yet; G2 "
                            "implements MB1 (relu) only");
    }
    if (!bench::implementation_supported(opt.benchmark, opt.implementation)) {
        std::cerr << "benchmark '" << bench::to_string(opt.benchmark)
                  << "' does not accept implementation '"
                  << bench::to_string(opt.implementation) << "'\n";
        return kUsage;
    }
    if (opt.mode == bench::bench_mode::kernel_only
        && (opt.fault_flags
            & (BENCH_FAULT_SKIP_DMA_IN | BENCH_FAULT_SKIP_DMA_OUT)) != 0u) {
        return refuse(expected_usage::dma_fault_needs_end_to_end,
                      "the DMA fault controls need --mode end_to_end; in "
                      "kernel-only mode there is no DMA leg to skip, so the "
                      "control would pass without proving anything");
    }
    // Placed ahead of every check that can skip. Behind the firmware check it
    // would return 77 on a machine without the cross toolchain, and a
    // `WILL_FAIL` control inverts a skip into a pass — so the control would
    // report the mechanism as exercised on exactly the machines where it was
    // not. Nothing below this point can change the answer: the command line
    // was already accepted by every refusal that exists.
    if (opt.expect_usage != expected_usage::none) {
        std::cerr << "the control expected the usage refusal '"
                  << to_string(opt.expect_usage)
                  << "', but the command line was accepted\n";
        return kFail;
    }

    if (is_reserved_profile_id(opt.config_id)) {
        std::cerr << "--config-id '" << opt.config_id
                  << "' is a D28 Neo Lite profile identity, and this binary "
                     "does not contain that profile: it is the D27 reference "
                     "machine (64x64 MXU, VLEN 512, 10 ns period). D28 refuses "
                     "a configuration ID whose components use other values, so "
                     "the profile identities stay reserved until their build "
                     "profiles and promotion gates exist\n";
        return kUsage;
    }

    if (opt.config_id == "reference") {
        const auto mismatches = reference_configuration_mismatches(opt);
        if (!mismatches.empty()) {
            std::cerr << "--config-id reference was asked for, but these knobs "
                         "are not the plan's §7.1 reference values:\n";
            for (const auto& problem : mismatches) {
                std::cerr << "  " << problem << '\n';
            }
            std::cerr << "give the run its own configuration id (for example "
                         "bringup_64k) rather than labelling it reference\n";
            return kUsage;
        }
    }

    // §8.1 wants the source revision in every row, and `unspecified` does not
    // satisfy a reproducibility contract. The file is regenerated on every
    // build, so it names the tree that produced the binary rather than the
    // tree that was configured.
    if (!opt.source_revision_file.empty()) {
        std::ifstream revision(opt.source_revision_file);
        std::string line;
        if (revision.good() && std::getline(revision, line) && !line.empty()) {
            opt.source_revision = line;
        }
    }
    if (opt.source_revision.empty()) {
        std::cerr << "no source revision: pass --source-revision or "
                     "--source-revision-file. A row without one does not meet "
                     "the §8.1 reproducibility contract\n";
        return kUsage;
    }

    // §8.1 lists the build type beside the revision, and for the same reason:
    // a Release row and a Debug row are not comparable. Leaving it optional
    // meant a direct caller produced `"build_type": "unspecified"` and exited
    // zero, with CTest masking it because it always passes $<CONFIG>.
    if (opt.build_type.empty() || opt.build_type == "unspecified") {
        std::cerr << "no build type: pass --build-type (Release, Debug, ...). "
                     "A row without one does not meet the §8.1 "
                     "reproducibility contract\n";
        return kUsage;
    }

    if (opt.elf_path.empty() || !std::ifstream(opt.elf_path).good()) {
        std::cout << "SKIP: "
                  << (opt.elf_path.empty() ? "<no --elf argument>"
                                           : opt.elf_path)
                  << " was not built (no RISC-V cross toolchain?)\n";
        return kSkip;
    }

    const auto map_problems
        = firmware_map_disagreements(opt.sram_capacity_bytes);
    if (!map_problems.empty()) {
        for (const auto& problem : map_problems) {
            std::cerr << "map disagreement: " << problem << '\n';
        }
        std::cerr << "not running the image, because every address below "
                     "would be suspect\n";
        return kFail;
    }

    // ── the case, and the golden computed independently of the model ────────

    const bench::case_shape shape = bench::case_at(opt.benchmark, opt.case_index);
    const bool is_matrix = opt.benchmark == bench::benchmark_id::gemv_rvv
        || opt.benchmark == bench::benchmark_id::gemv_mxu
        || opt.benchmark == bench::benchmark_id::gemm;
    const std::uint32_t elements = is_matrix ? shape.m * shape.n
                                             : shape.elements;
    const std::uint32_t input_bytes
        = static_cast<std::uint32_t>(bench::input_bytes(opt.benchmark, shape));
    const std::uint32_t output_bytes
        = static_cast<std::uint32_t>(bench::output_bytes(opt.benchmark, shape));

    std::vector<failure> failures;
    const auto fail = [&](failure_kind kind, std::string why) {
        failures.push_back({kind, std::move(why)});
    };
    const auto any_of_kind = [&](failure_kind kind) {
        return std::any_of(failures.begin(), failures.end(),
                           [kind](const failure& f) { return f.kind == kind; });
    };

    // Input and output are separate quantities from here on. MB1 is
    // element-wise so they coincide; MB2 reduces 2N operands to one INT32, and
    // a single `bytes` would have made one of its two DMA legs the wrong
    // length and its comparison the wrong shape.
    std::vector<std::uint8_t> staged_input;
    std::vector<std::int32_t> input;
    std::vector<std::int32_t> golden;
    bench::input_profile profile;
    const char* vacuous_reason = nullptr;

    if (opt.benchmark == bench::benchmark_id::relu) {
        input = bench::generate_int32_inputs(opt.seed, elements, opt.pattern);
        golden = bench::relu_golden(input);
        profile = bench::profile_of(input);
        vacuous_reason = "ReLU would pass without exercising one half of the "
                         "operation";
    } else if (opt.benchmark == bench::benchmark_id::vector_dot) {
        // MB2. One generated block holds both operands back to back, so the
        // seed still determines the whole input and the staged layout is what
        // the guest reads: `a` then `b`.
        const std::int32_t bound = bench::dot_product_magnitude_bound(elements);
        input = bench::generate_dot_operands(opt.seed, elements * 2u,
                                             opt.pattern, bound);
        const std::vector<std::int32_t> a(input.begin(),
                                          input.begin() + elements);
        const std::vector<std::int32_t> b(input.begin() + elements,
                                          input.end());
        try {
            golden.assign(1, bench::dot_product_golden(a, b));
        } catch (const std::exception& error) {
            std::cerr << "the golden refused this case's operands: "
                      << error.what() << '\n';
            return kFail;
        }
        profile = bench::profile_of(input);
        vacuous_reason = "a dot product over one-signed operands cannot "
                         "distinguish a sign error";
    } else {
        const auto a = bench::generate_int8_inputs(
            opt.seed, static_cast<std::size_t>(shape.m) * shape.k, opt.pattern);
        const auto b = bench::generate_int8_inputs(
            opt.seed ^ UINT64_C(0xD1B54A32D192ED03),
            static_cast<std::size_t>(shape.k) * shape.n, opt.pattern);
        staged_input.reserve(a.size() + b.size());
        staged_input.insert(staged_input.end(),
                            reinterpret_cast<const std::uint8_t*>(a.data()),
                            reinterpret_cast<const std::uint8_t*>(a.data())
                                + a.size());
        staged_input.insert(staged_input.end(),
                            reinterpret_cast<const std::uint8_t*>(b.data()),
                            reinterpret_cast<const std::uint8_t*>(b.data())
                                + b.size());
        try {
            golden = bench::matrix_multiply_int8_golden(
                a, b, shape.m, shape.n, shape.k);
        } catch (const std::exception& error) {
            std::cerr << "the golden refused this case's operands: "
                      << error.what() << '\n';
            return kFail;
        }
        std::vector<std::int8_t> both(a);
        both.insert(both.end(), b.begin(), b.end());
        profile = bench::profile_of(both);
        vacuous_reason = "an INT8 matrix product over one-signed operands "
                         "cannot distinguish a sign-path error";
    }

    if (!is_matrix) {
        staged_input.resize(input.size() * sizeof(input.front()));
        std::memcpy(staged_input.data(), input.data(), staged_input.size());
    }

    const std::uint32_t output_elements
        = static_cast<std::uint32_t>(golden.size());
    if (staged_input.size() != input_bytes
        || static_cast<std::uint64_t>(output_elements) * sizeof(std::int32_t)
            != output_bytes) {
        std::cerr << "generated tensor layout disagrees with the frozen byte "
                     "contract\n";
        return kFail;
    }

    if (bench::pattern_requires_both_signs(opt.pattern)
        && (profile.negatives == 0 || profile.positives == 0)) {
        fail(failure_kind::harness,
             "the '" + std::string(bench::to_string(opt.pattern))
             + "' input pattern produced " + std::to_string(profile.negatives)
             + " negative and " + std::to_string(profile.positives)
             + " positive values; " + vacuous_reason);
    }

    // ── the machine ─────────────────────────────────────────────────────────

    tpu_core neo("neo_core", make_core_config(opt));
    bench_world outside("outside");
    neo.external().bind(outside.socket);

    // Nothing drives the core's inbound side: the hart is the only initiator.
    // SystemC still requires the port bound.
    struct idle_remote : sc_core::sc_module {
        tlm_utils::simple_initiator_socket<idle_remote> socket;
        explicit idle_remote(sc_core::sc_module_name name)
            : sc_core::sc_module(name), socket("socket") {}
    } unused_remote("unused_remote");
    unused_remote.socket.bind(neo.inbound());

    sample opened;
    sample closed;
    unsigned begin_marks = 0;
    unsigned end_marks = 0;
    outside.on_measure_begin = [&](double now_ns) {
        ++begin_marks;
        if (!opened.taken) {
            opened = take_sample(neo, outside, now_ns);
        }
    };
    outside.on_measure_end = [&](double now_ns) {
        ++end_marks;
        closed = take_sample(neo, outside, now_ns);
    };

    // Stage transitions, timed. The guest already marks each one; recording
    // when it did turns the interval from a single number into an accounting
    // whose parts can be added up (G3, clause 4).
    struct stage_entry {
        std::uint32_t stage;
        double at_ns;
    };
    std::vector<stage_entry> stages;
    outside.on_stage = [&](std::uint32_t stage_id, double now_ns) {
        stages.push_back({stage_id, now_ns});
    };

    // ── staging, outside the measured interval ──────────────────────────────

    std::uint32_t params[BENCH_PARAM_WORDS] = {};
    params[BENCH_PARAM_OFF_MAGIC / 4] = BENCH_PARAM_MAGIC;
    // The guest checks this against its own identity, so pointing the MB1 ELF
    // at a vector_dot case (or the reverse) is refused by the image rather
    // than producing a result for the benchmark nobody asked for.
    switch (opt.benchmark) {
    case bench::benchmark_id::relu:
        params[BENCH_PARAM_OFF_BENCHMARK / 4] = BENCH_ID_RELU;
        break;
    case bench::benchmark_id::vector_dot:
        params[BENCH_PARAM_OFF_BENCHMARK / 4] = BENCH_ID_VECTOR_DOT;
        break;
    case bench::benchmark_id::gemv_rvv:
        params[BENCH_PARAM_OFF_BENCHMARK / 4] = BENCH_ID_GEMV_RVV;
        break;
    case bench::benchmark_id::gemv_mxu:
        params[BENCH_PARAM_OFF_BENCHMARK / 4] = BENCH_ID_GEMV_MXU;
        break;
    case bench::benchmark_id::gemm:
        params[BENCH_PARAM_OFF_BENCHMARK / 4] = BENCH_ID_GEMM;
        break;
    }
    params[BENCH_PARAM_OFF_CASE_INDEX / 4]
        = static_cast<std::uint32_t>(opt.case_index);
    params[BENCH_PARAM_OFF_IMPL / 4]
        = opt.implementation == bench::bench_impl::rvv ? BENCH_IMPL_RVV
        : opt.implementation == bench::bench_impl::mxu ? BENCH_IMPL_MXU
                                                       : BENCH_IMPL_SCALAR;
    params[BENCH_PARAM_OFF_MODE / 4]
        = opt.mode == bench::bench_mode::end_to_end ? BENCH_MODE_E2E
                                                    : BENCH_MODE_KERNEL;
    params[BENCH_PARAM_OFF_ELEMENTS / 4] = elements;
    params[BENCH_PARAM_OFF_IN_BYTES / 4] = input_bytes;
    params[BENCH_PARAM_OFF_OUT_BYTES / 4] = output_bytes;
    params[BENCH_PARAM_OFF_FAULT_FLAGS / 4] = opt.fault_flags;
    params[BENCH_PARAM_OFF_HOST_SRC / 4] = BENCH_HOST_SRC_ADDR;
    params[BENCH_PARAM_OFF_HOST_DST / 4] = BENCH_HOST_DST_ADDR;
    params[BENCH_PARAM_OFF_SRAM_SRC / 4] = BENCH_SRAM_SRC_ADDR;
    params[BENCH_PARAM_OFF_SRAM_DST / 4] = BENCH_SRAM_DST_ADDR;
    params[BENCH_PARAM_OFF_M / 4] = shape.m;
    params[BENCH_PARAM_OFF_N / 4] = shape.n;
    params[BENCH_PARAM_OFF_K / 4] = shape.k;
    outside.stage(BENCH_PARAMS_ADDR, params, sizeof(params));

    // Poison every destination before the run.
    //
    // Without this, two of the frozen input patterns are vacuous. `zero` and
    // `all_negative` both have an all-zero golden output, and an unwritten
    // buffer reads as zero — so a kernel that did nothing at all would pass
    // them. Poisoning turns "the result is what we expected" into "the result
    // was written, and is what we expected", which is the claim the row makes.
    //
    // The value must not collide with anything the golden can contain, so it
    // is checked against this case's golden rather than assumed.
    std::int32_t poison = static_cast<std::int32_t>(0x5A5A5A5Au);
    if (std::find(golden.begin(), golden.end(), poison) != golden.end()) {
        poison = static_cast<std::int32_t>(0xA5A5A5A5u);
    }
    if (std::find(golden.begin(), golden.end(), poison) != golden.end()) {
        std::cerr << "both poison candidates appear in this case's golden "
                     "output; an unwritten destination would be "
                     "indistinguishable from a correct one\n";
        return kFail;
    }
    const std::vector<std::int32_t> poison_fill(output_elements, poison);

    if (opt.mode == bench::bench_mode::kernel_only) {
        // §6.1: inputs begin in core SRAM and results stay there. The debug
        // path moves no fabric bytes and touches no counter, which is what
        // keeps this initialisation outside the measurement.
        if (!write_sram(neo, BENCH_SRAM_DST_ADDR, poison_fill.data(),
                        output_bytes)
            || !write_sram(neo, BENCH_SRAM_SRC_ADDR, staged_input.data(),
                           input_bytes)) {
            std::cerr << "could not stage the kernel-only buffers into core "
                         "SRAM\n";
            return kFail;
        }
    } else {
        // §6.2: inputs begin in external RAM and the DMA brings them in.
        // Core SRAM gets no input here on purpose — preloading it and then
        // quoting the run as a DMA benchmark is exactly what the plan
        // prohibits, and it would also make the DMA-in control unable to bite.
        // Both core-SRAM buffers are poisoned instead, so a missing DMA-in
        // leg presents as poison rather than as zeros that a zero-golden case
        // would accept.
        const std::vector<std::uint8_t> source_poison(input_bytes, 0xA5u);
        if (!write_sram(neo, BENCH_SRAM_SRC_ADDR, source_poison.data(),
                        input_bytes)
            || !write_sram(neo, BENCH_SRAM_DST_ADDR, poison_fill.data(),
                           output_bytes)) {
            std::cerr << "could not poison the core SRAM buffers\n";
            return kFail;
        }
        outside.stage(BENCH_HOST_SRC_ADDR, staged_input.data(), input_bytes);
        outside.stage(BENCH_HOST_DST_ADDR, poison_fill.data(), output_bytes);
    }

    neo.cpu().load_elf(opt.elf_path);

    sc_core::sc_start(sc_core::sc_time(opt.watchdog_ms, sc_core::SC_MS));

    // ── what the guest reported ─────────────────────────────────────────────

    const auto sim_word = [&](std::uint32_t address) {
        return outside.sim[(address - SIM_BASE) / 4];
    };

    const std::uint32_t stage = sim_word(SIM_STAGE_MARK);
    const std::uint32_t exit_kind = sim_word(SIM_EXIT_KIND);
    const std::uint32_t exit_status = sim_word(SIM_EXIT_STATUS);
    const std::uint32_t mcause = sim_word(SIM_EXIT_MCAUSE);
    const std::uint32_t mepc = sim_word(SIM_EXIT_MEPC);
    const std::uint32_t mhartid = sim_word(SIM_MHARTID);
    const std::uint32_t guest_checksum = sim_word(SIM_GUEST_CHECKSUM);
    const std::uint32_t guest_elements = sim_word(SIM_GUEST_ELEMENTS);
    const std::uint32_t vlenb = sim_word(SIM_VLENB);
    const std::uint32_t vlmax = sim_word(SIM_VLMAX);
    const std::uint32_t vl_first = sim_word(SIM_VL_FIRST);
    const std::uint32_t vl_last = sim_word(SIM_VL_LAST);
    const std::uint32_t vector_iterations = sim_word(SIM_VECTOR_ITERS);
    const std::uint32_t param_echo = sim_word(SIM_PARAM_ECHO);

    if (!outside.exited) {
        fail(failure_kind::guest,
             std::string("the image never reached its exit protocol; the last "
                         "stage it entered was ")
             + stage_name(stage)
             + ". A watchdog expiry here means the kernel never finished or "
               "an engine never left BUSY");
    }
    if (exit_kind != SIM_EXIT_KIND_NORMAL) {
        std::ostringstream why;
        why << "the image trapped instead of finishing: mcause " << mcause
            << ", mepc 0x" << std::hex << mepc << std::dec << ", during "
            << stage_name(stage);
        fail(failure_kind::guest, why.str());
    }
    if (exit_status != SIM_EXIT_PASS) {
        std::ostringstream why;
        why << "the image reported failure status 0x" << std::hex
            << exit_status << std::dec << " during " << stage_name(stage);
        fail(failure_kind::guest, why.str());
    }
    if (mhartid != BENCH_EXPECTED_MHARTID) {
        fail(failure_kind::guest,
             "the hart read mhartid = " + std::to_string(mhartid)
             + ", expected " + std::to_string(BENCH_EXPECTED_MHARTID));
    }
    if (param_echo != elements) {
        fail(failure_kind::guest,
             "the guest echoed an element count of "
             + std::to_string(param_echo) + " but the runner staged "
             + std::to_string(elements)
             + "; the parameter block did not arrive intact");
    }
    if (guest_elements != elements && outside.exited) {
        fail(failure_kind::guest,
             "the guest reported working on " + std::to_string(guest_elements)
             + " elements, not " + std::to_string(elements));
    }

    // ── the measured interval ───────────────────────────────────────────────

    const bool interval_closed = opened.taken && closed.taken;
    if (!opened.taken) {
        fail(failure_kind::interval,
             "the measured interval never opened; the guest did not write the "
             "begin marker");
    }
    if (opened.taken && !closed.taken) {
        fail(failure_kind::interval,
             "the measured interval never closed; the guest did not write the "
             "end marker, so no timing figure from this run means anything");
    }
    if (begin_marks > 1 || end_marks > 1) {
        fail(failure_kind::interval,
             "the interval markers fired " + std::to_string(begin_marks)
             + " and " + std::to_string(end_marks)
             + " times; one run is one interval");
    }

    // ── the result, element by element ──────────────────────────────────────

    std::vector<std::int32_t> observed(output_elements, 0);
    bool readback_ok = true;
    if (opt.mode == bench::bench_mode::kernel_only) {
        readback_ok = read_sram(neo, BENCH_SRAM_DST_ADDR, observed.data(),
                                output_bytes);
        if (!readback_ok) {
            fail(failure_kind::harness,
                 "could not read the result back out of core SRAM");
        }
    } else {
        outside.fetch(BENCH_HOST_DST_ADDR, observed.data(), output_bytes);
    }

    bench::correctness_record correctness;
    correctness.golden_checksum = bench::wrapping_checksum(golden);
    correctness.guest_checksum = guest_checksum;
    correctness.profile = profile;

    // Every element, not "every element until the first that differs".
    //
    // The scan used to stop at the first mismatch while `elements_compared`
    // still reported the whole tensor, so a control that failed at index 0
    // recorded seventeen comparisons after making one. Running the scan out
    // also buys the more useful number: one wrong element and seventeen wrong
    // elements are different defects, and a first-mismatch index alone cannot
    // tell them apart.
    bool mismatch = false;
    std::uint64_t compared = 0;
    if (readback_ok) {
        for (std::uint32_t i = 0; i < output_elements; ++i) {
            ++compared;
            if (observed[i] != golden[i]) {
                if (!mismatch) {
                    correctness.has_mismatch = true;
                    correctness.first_mismatch_index = i;
                    correctness.expected = golden[i];
                    correctness.observed = observed[i];
                    mismatch = true;
                }
                ++correctness.mismatch_count;
            }
        }
    }
    correctness.elements_compared = compared;
    if (mismatch) {
        std::ostringstream why;
        why << correctness.mismatch_count << " of "
            << correctness.elements_compared
            << " elements differ; the first is element "
            << correctness.first_mismatch_index << ", which is "
            << correctness.observed << " where " << correctness.expected
            << " was expected";
        // The guest's own checksum separates a wrong computation from a
        // correct one that did not arrive: it is taken from core SRAM, where
        // the kernel wrote, before any transport back to the host.
        if (guest_checksum == correctness.golden_checksum) {
            why << ". The guest's own checksum of core SRAM matches the "
                   "golden, so the kernel computed the right answer and the "
                   "result did not reach the host intact";
        } else {
            why << ". The guest's own checksum of core SRAM also disagrees, "
                   "so the kernel computed the wrong answer";
        }
        fail(failure_kind::golden, why.str());
    } else if (readback_ok && guest_checksum != correctness.golden_checksum
               && outside.exited) {
        fail(failure_kind::golden,
             "every element matches the golden but the guest's own checksum "
             "of core SRAM does not; the two views of the result disagree");
    }
    // `passed` is **not** decided here. The identity gates below can still
    // reject the run, and a row that fixed its verdict before they ran said
    // `"passed": true` while the process exited non-zero — invisible to an
    // aggregation, which reads JSON rather than exit codes. Both this field
    // and `run_valid` are set once, after every check has had its say.
    if (!readback_ok) {
        correctness.detail
            = "the output could not be read back; see the failure list";
    } else if (mismatch) {
        correctness.detail = "see the failure list";
    } else if (guest_checksum != correctness.golden_checksum && outside.exited) {
        correctness.detail
            = "every output element matches the host golden, but the guest "
              "checksum disagrees; see the failure list";
    } else {
        correctness.detail
            = "all " + std::to_string(output_elements)
            + " output elements match the independently computed host golden";
    }

    // ── the row ─────────────────────────────────────────────────────────────

    bench::result_row row;
    row.identity.benchmark = opt.benchmark;
    row.identity.case_index = opt.case_index;
    row.identity.shape = shape;
    row.identity.datatype = is_matrix ? "int8xint8->int32" : "int32";
    row.identity.mode = opt.mode;
    row.identity.implementation = opt.implementation;
    row.identity.pattern = opt.pattern;
    row.identity.seed = opt.seed;
    row.identity.firmware_elf_path = opt.elf_path;
    row.identity.firmware_elf_sha256 = bench::sha256_file(opt.elf_path);
    row.identity.build_type = opt.build_type;
    row.identity.source_revision = opt.source_revision;
    row.identity.fault_flags = opt.fault_flags;
    if (!opt.accounting_mutation.empty()) {
        row.identity.injected_fault = opt.injected_fault.empty()
            ? "accounting:" + opt.accounting_mutation
            : opt.injected_fault + "+accounting:" + opt.accounting_mutation;
    }
    row.identity.injected_fault
        = opt.injected_fault.empty() ? "none" : opt.injected_fault;

    // ── identity, read back from the components that were instantiated ──────
    //
    // D28's promotion gate: "A report must obtain the instantiated MXU
    // identity, CPU `vlenb`, DMA channel count and AXI width from live
    // components; it must not reproduce literals from the profile factory and
    // call that readback." Geometry and VLEN are compile-time in this source,
    // so a literal here would name whatever the schema's default happened to
    // be rather than whatever was built.
    const auto mxu = neo.matrix_engine().identity();
    const auto& fabric_config = neo.fabric().config();
    row.configuration.id = opt.config_id;
    row.configuration.mxu_geometry = std::to_string(mxu.rows) + "x"
        + std::to_string(mxu.columns);
    row.configuration.mxu_datatype = capability_string(mxu.capability);
    row.configuration.mxu_source_revision
        = mxu.source_revision.empty() ? std::string("unavailable")
                                      : mxu.source_revision;
    // The capacity the SRAM actually backs, not the number the command line
    // asked for. They agree today because the constructor takes the value, and
    // reading it back is what keeps that a fact rather than an assumption.
    row.configuration.sram_capacity_bytes = neo.sram().capacity_bytes();
    // One controller, and as many channel contexts as it owns. `neo_dma` has a
    // single descriptor and worker today, so the honest live figure is 1 — not
    // the 2 or 4 D28's profiles will require, which WP4 has to build.
    row.configuration.dma_controllers = 1;
    row.configuration.dma_channels = 1;
    row.configuration.bank_width_bits = fabric_config.data_width_bits;
    row.configuration.bank_count = fabric_config.bank_count;
    row.configuration.pipeline_stages = fabric_config.pipeline_stages;
    row.configuration.outstanding_per_requester
        = fabric_config.max_outstanding_per_requester;
    row.configuration.bank_mapping = tpu::to_string(fabric_config.mapping);
    row.configuration.arbitration = tpu::to_string(fabric_config.arbitration);
    row.configuration.timing_mode = core::to_string(neo.fabric().timing());
    row.configuration.core_period_ns = opt.core_period_ns;
    row.configuration.dma_max_burst_bytes = opt.dma_max_burst_bytes;
    row.configuration.provisional = true;

    // Where every configuration field came from, stated rather than implied.
    //
    // Four are live today. The rest are not, and saying so is the point: D28
    // wants MXU identity, CPU vlenb, DMA channel count and AXI width read from
    // live components, and only the first two are. `neo_dma` owns one worker
    // and exposes no channel accessor, and the external width is a fixed
    // constant in the bridge — WP4 and WP3 are what make those two readbacks.
    // Until then a row that claimed them live would be exactly the defect D28
    // names.
    row.configuration.identity_provenance = {
        {"id", "configured"},
        {"vlen_bits", "live_readback"},
        {"mxu_geometry", "live_readback"},
        {"mxu_datatype", "live_readback"},
        {"mxu_source_revision", "live_readback"},
        {"sram_capacity_bytes", "live_readback"},
        {"local_bank_width_bits", "live_readback"},
        {"local_bank_count", "live_readback"},
        {"fabric_pipeline_stages", "live_readback"},
        {"outstanding_per_requester", "live_readback"},
        {"bank_mapping", "live_readback"},
        {"arbitration", "live_readback"},
        {"timing_mode", "live_readback"},
        {"dma_max_burst_bytes", "configured"},
        {"core_period_ns", "configured"},
        {"dma_controllers", "structural_literal"},
        {"dma_channels", "structural_literal"},
        {"external_axi_data_width_bits", "structural_literal"},
        {"xlen", "structural_literal"},
        {"elen_bits", "structural_literal"},
        {"rvv_version", "structural_literal"},
        {"physical_values_provisional", "structural_literal"},
    };

    row.correctness = correctness;

    // ── hart and interval ───────────────────────────────────────────────────

    const std::string no_split_reason
        = "the pinned VP++ build compiles ISSStatsDummy (ISS_CT_STATS_ENABLED "
          "is not defined), so the model exposes no scalar/vector instruction "
          "split; only the instret CSR and mcycle are readable";

    row.hart.interval_closed = interval_closed;
    row.hart.scalar_instructions
        = bench::measured<std::uint64_t>::unavailable(no_split_reason);
    row.hart.vector_instructions
        = bench::measured<std::uint64_t>::unavailable(no_split_reason);

    if (interval_closed) {
        row.hart.interval_begin_ns = opened.time_ns;
        row.hart.interval_end_ns = closed.time_ns;
        row.hart.elapsed_ns
            = bench::measured<double>::present(closed.time_ns - opened.time_ns);
        row.hart.retired_instructions = bench::measured<std::uint64_t>::present(
            delta(opened.instret, closed.instret));
        row.hart.mcycle_delta = bench::measured<std::uint64_t>::present(
            delta(opened.mcycle, closed.mcycle));

        row.hart.local_requests
            = delta(opened.hart_requests[0], closed.hart_requests[0]);
        row.hart.local_bytes
            = delta(opened.hart_bytes[0], closed.hart_bytes[0]);
        row.hart.control_requests
            = delta(opened.hart_requests[1], closed.hart_requests[1]);
        row.hart.control_bytes
            = delta(opened.hart_bytes[1], closed.hart_bytes[1]);
        row.hart.external_requests
            = delta(opened.hart_requests[2], closed.hart_requests[2]);
        row.hart.external_bytes
            = delta(opened.hart_bytes[2], closed.hart_bytes[2]);
    } else {
        const std::string why
            = "the measured interval did not open and close, so no delta over "
              "it exists";
        row.hart.elapsed_ns = bench::measured<double>::unavailable(why);
        row.hart.retired_instructions
            = bench::measured<std::uint64_t>::unavailable(why);
        row.hart.mcycle_delta
            = bench::measured<std::uint64_t>::unavailable(why);
    }

    // `vlenb` is CPU identity, not vector-loop evidence: the guest reads the
    // CSR before it looks at the parameter block, so a scalar run has it too.
    // Reporting it unavailable there left half the MB1 matrix with no live CPU
    // identity at all, while `configuration.vlen_bits` carried a literal — the
    // combination D28 names when it says a report "must not reproduce literals
    // from the profile factory and call that readback".
    if (vlenb > 0) {
        row.hart.vlenb = bench::measured<std::uint32_t>::present(vlenb);
        row.configuration.vlen_bits = vlenb * 8u;
    } else {
        row.hart.vlenb = bench::measured<std::uint32_t>::unavailable(
            "the guest did not report vlenb, so this run establishes no CPU "
            "identity");
        fail(failure_kind::guest,
             "the guest never reported vlenb, so the VLEN this row would state "
             "could only come from a literal");
    }

    // The live MXU against what this build is supposed to contain. Geometry is
    // a template parameter, so a binary built for another array would
    // otherwise emit rows naming the array nobody built — the defect D28's
    // promotion gate item 8 exists to make impossible.
    if (row.configuration.mxu_geometry != opt.mxu_geometry_expected) {
        fail(failure_kind::harness,
             "the instantiated MXU reports " + row.configuration.mxu_geometry
                 + " but this build is configured for "
                 + opt.mxu_geometry_expected
                 + ". D28 refuses a configuration whose components use other "
                   "values");
    }
    if (mxu.source_revision.empty()) {
        fail(failure_kind::harness,
             "the MXU reports no source revision, so its geometry is stated "
             "rather than attributable (D14)");
    }
    if (neo.sram().capacity_bytes() != opt.sram_capacity_bytes) {
        fail(failure_kind::harness,
             "core SRAM backs " + std::to_string(neo.sram().capacity_bytes())
                 + " bytes but " + std::to_string(opt.sram_capacity_bytes)
                 + " was requested");
    }

    // The live value against what this build is supposed to contain. A VP++
    // built for another VLEN would otherwise produce rows claiming 512 while
    // the machine granted something else, and every lane-utilization figure
    // derived from it would be wrong in a way nothing reports.
    if (vlenb > 0 && vlenb * 8u != opt.vlen_bits_expected) {
        fail(failure_kind::harness,
             "the guest reports vlenb = " + std::to_string(vlenb) + " ("
                 + std::to_string(vlenb * 8u) + "-bit VLEN) but this build is "
                 "configured for " + std::to_string(opt.vlen_bits_expected)
                 + "-bit. D28 refuses a configuration whose components use "
                   "other values");
    }

    if (opt.implementation == bench::bench_impl::rvv && vector_iterations > 0
        && vlmax > 0) {
        const std::uint64_t lane_slots
            = static_cast<std::uint64_t>(vector_iterations) * vlmax;
        row.hart.vlmax = bench::measured<std::uint32_t>::present(vlmax);
        row.hart.vl_first = bench::measured<std::uint32_t>::present(vl_first);
        row.hart.vl_last = bench::measured<std::uint32_t>::present(vl_last);
        row.hart.vector_iterations
            = bench::measured<std::uint32_t>::present(vector_iterations);
        const std::uint64_t active_elements
            = opt.benchmark == bench::benchmark_id::gemv_rvv
            ? static_cast<std::uint64_t>(shape.k) * shape.n
            : elements;
        row.hart.active_elements
            = bench::measured<std::uint64_t>::present(active_elements);
        row.hart.tail_elements = bench::measured<std::uint64_t>::present(
            lane_slots > active_elements ? lane_slots - active_elements : 0);
        row.hart.lane_utilization = bench::measured<double>::present(
            static_cast<double>(active_elements)
            / static_cast<double>(lane_slots));
        row.hart.derivation
            = "vlenb, vlmax, vl_first, vl_last and vector_iterations are read "
              "back from the guest, vlmax from a vsetvli with rs1=x0 and the "
              "others from the strip-mined loop's own vsetvli results. "
              "Derived, not measured: lane_slots = vector_iterations * vlmax; "
              "tail_elements = lane_slots - active_elements; "
              "lane_utilization = active_elements / lane_slots.";
    } else {
        const std::string why
            = opt.implementation == bench::bench_impl::rvv
            ? "the RVV kernel reported no vsetvli result, so there is nothing "
              "to derive lane utilization from"
            : opt.implementation == bench::bench_impl::mxu
            ? "the MXU implementation issues no RVV kernel instructions"
            : "the scalar implementation issues no vector instructions";
        row.hart.vlmax = bench::measured<std::uint32_t>::unavailable(why);
        row.hart.vl_first = bench::measured<std::uint32_t>::unavailable(why);
        row.hart.vl_last = bench::measured<std::uint32_t>::unavailable(why);
        row.hart.vector_iterations
            = bench::measured<std::uint32_t>::unavailable(why);
        row.hart.active_elements
            = bench::measured<std::uint64_t>::unavailable(why);
        row.hart.tail_elements
            = bench::measured<std::uint64_t>::unavailable(why);
        row.hart.lane_utilization = bench::measured<double>::unavailable(why);
        row.hart.derivation = "not applicable: " + why;
    }

    // ── DMA and external memory (§8.3) ──────────────────────────────────────

    // `bytes_done()` is deliberately not differenced here. It is the
    // `BYTES_DONE` register, which the engine resets when it accepts a
    // descriptor, so a difference across two jobs measures the second job
    // alone and looks exactly like a correct total for a one-leg run. The
    // lifetime path totals are the counters that survive an interval.
    std::uint64_t total_bytes = 0;
    if (interval_closed) {
        row.dma.transfers = delta(opened.dma_transfers, closed.dma_transfers);
        row.dma.chunks = delta(opened.dma_chunks, closed.dma_chunks);
        row.dma.local_requests
            = delta(opened.dma_local_requests, closed.dma_local_requests);
        row.dma.external_requests
            = delta(opened.dma_external_requests, closed.dma_external_requests);
        row.dma.errors = delta(opened.dma_errors, closed.dma_errors);
        row.dma.local_path_bytes
            = delta(opened.dma_local_bytes, closed.dma_local_bytes);
        row.dma.external_path_bytes
            = delta(opened.dma_external_bytes, closed.dma_external_bytes);
    }

    // The direction split, derived from this benchmark's transfer structure
    // and then checked against the measured path totals. Each firmware issues
    // one transfer per direction of exactly `bytes`, and each leg touches the
    // local path once and the external path once — so an end-to-end run must
    // show two transfers and `2 * bytes` on each path. When it does not, the
    // structure this derivation assumes did not hold and the split is reported
    // as unavailable rather than asserted.
    const std::uint64_t expected_legs
        = opt.mode == bench::bench_mode::end_to_end ? 2u : 0u;
    const std::uint64_t expected_path_bytes
        = expected_legs == 0 ? 0u : (input_bytes + output_bytes);
    if (!interval_closed) {
        const std::string why
            = "the measured interval did not open and close, so no DMA delta "
              "over it exists";
        row.dma.bytes_in = bench::measured<std::uint64_t>::unavailable(why);
        row.dma.bytes_out = bench::measured<std::uint64_t>::unavailable(why);
        row.dma.bytes_total = bench::measured<std::uint64_t>::unavailable(why);
    } else if (row.dma.transfers == expected_legs
               && row.dma.local_path_bytes == expected_path_bytes
               && row.dma.external_path_bytes == expected_path_bytes) {
        const std::uint64_t in = expected_legs == 0 ? 0u : input_bytes;
        const std::uint64_t out = expected_legs == 0 ? 0u : output_bytes;
        row.dma.bytes_in = bench::measured<std::uint64_t>::present(in);
        row.dma.bytes_out = bench::measured<std::uint64_t>::present(out);
        row.dma.bytes_total = bench::measured<std::uint64_t>::present(in + out);
        total_bytes = in + out;
    } else {
        std::ostringstream why;
        why << "the measured DMA activity does not match the benchmark's transfer "
               "structure, so the direction split cannot be derived: expected "
            << expected_legs << " transfers and " << expected_path_bytes
            << " bytes on each path, measured " << row.dma.transfers
            << " transfers, " << row.dma.local_path_bytes
            << " local-path bytes and " << row.dma.external_path_bytes
            << " external-path bytes";
        row.dma.bytes_in
            = bench::measured<std::uint64_t>::unavailable(why.str());
        row.dma.bytes_out
            = bench::measured<std::uint64_t>::unavailable(why.str());
        row.dma.bytes_total
            = bench::measured<std::uint64_t>::unavailable(why.str());
    }

    if (row.dma.chunks > 0 && row.dma.bytes_total.available) {
        row.dma.average_chunk_bytes = bench::measured<double>::present(
            static_cast<double>(row.dma.bytes_total.value)
            / static_cast<double>(row.dma.chunks));
    } else {
        row.dma.average_chunk_bytes = bench::measured<double>::unavailable(
            opt.mode == bench::bench_mode::kernel_only
                ? "kernel-only mode moves no DMA bytes by construction (§6.1)"
                : "the DMA completed no chunks in the measured interval, or "
                  "its byte total could not be derived");
    }

    if (total_bytes > 0 && interval_closed && row.hart.elapsed_ns.available
        && row.hart.elapsed_ns.value > 0.0) {
        const double cycles = row.hart.elapsed_ns.value / opt.core_period_ns;
        row.dma.achieved_bytes_per_cycle = bench::measured<double>::present(
            static_cast<double>(total_bytes) / cycles);
    } else {
        row.dma.achieved_bytes_per_cycle
            = bench::measured<double>::unavailable(
                "no DMA bytes crossed the measured interval, so a rate over it "
                "would be a division producing zero rather than a measurement");
    }

    const std::string no_dma_cycles
        = "neo_dma exposes byte, request, chunk and transfer counters but no "
          "busy, service or wait cycle counters; §8.3 asks for all three and "
          "the model implements none of them";
    row.dma.busy_cycles = bench::measured<double>::unavailable(no_dma_cycles);
    row.dma.service_cycles
        = bench::measured<double>::unavailable(no_dma_cycles);
    row.dma.wait_cycles = bench::measured<double>::unavailable(no_dma_cycles);
    row.dma.overlap_with_compute = bench::measured<double>::unavailable(
        "the firmware runs its DMA legs and its kernel strictly in sequence, so there "
        "is no overlap to measure; a benchmark that overlaps them is a "
        "separate experiment");
    row.dma.external_memory_model
        = bench::measured<std::string>::present(
            "fixed 1 ns annotated per access, no bandwidth model, no "
            "back-pressure and no outstanding bound; uncalibrated (§7.2), so "
            "no DMA-sufficiency or DMA-channel conclusion follows from this "
            "row");

    // ── the local SRAM fabric (§8.4) ────────────────────────────────────────

    std::uint64_t fabric_bytes = 0;
    for (unsigned i = 0; i < neo_requester_count; ++i) {
        const auto requester = static_cast<neo_requester>(i);
        if (!neo.fabric().is_attached(requester)) {
            continue;
        }
        bench::fabric_requester_record record;
        record.requester = sram::to_string(requester);
        if (interval_closed) {
            record.requests
                = delta(opened.fabric[i].request_count,
                        closed.fabric[i].request_count);
            record.physical_beats
                = delta(opened.fabric[i].physical_beat_count,
                        closed.fabric[i].physical_beat_count);
            record.bank_conflicts
                = delta(opened.fabric[i].bank_conflict_count,
                        closed.fabric[i].bank_conflict_count);
            record.arbitration_events
                = delta(opened.fabric[i].arbitration_event_count,
                        closed.fabric[i].arbitration_event_count);
            record.transferred_bytes
                = delta(opened.fabric[i].transferred_bytes,
                        closed.fabric[i].transferred_bytes);
            record.errors = delta(opened.fabric[i].error_count,
                                  closed.fabric[i].error_count);
            record.total_latency_ns
                = (closed.fabric[i].total_latency
                   - opened.fabric[i].total_latency)
                      .to_seconds()
                * 1e9;
            fabric_bytes += record.transferred_bytes;
        }
        row.fabric.requesters.push_back(record);
        row.fabric.bank_grant_requesters.push_back(record.requester);
    }

    if (interval_closed) {
        for (std::size_t bank = 0; bank < closed.bank_grants.size(); ++bank) {
            std::vector<std::uint64_t> grants;
            for (unsigned i = 0; i < neo_requester_count; ++i) {
                if (!neo.fabric().is_attached(static_cast<neo_requester>(i))) {
                    continue;
                }
                grants.push_back(delta(opened.bank_grants[bank][i],
                                       closed.bank_grants[bank][i]));
            }
            row.fabric.bank_grants.push_back(std::move(grants));
        }
    }

    if (interval_closed && row.hart.elapsed_ns.available
        && row.hart.elapsed_ns.value > 0.0) {
        const double cycles = row.hart.elapsed_ns.value / opt.core_period_ns;
        const double achieved = static_cast<double>(fabric_bytes) / cycles;
        row.fabric.achieved_bytes_per_cycle
            = bench::measured<double>::present(achieved);
        const double peak = static_cast<double>(fabric_config.stripe_bytes());
        row.fabric.percent_of_configured_peak
            = peak > 0.0 ? bench::measured<double>::present(achieved / peak * 100.0)
                         : bench::measured<double>::unavailable(
                             "the configured stripe width is zero");
    } else {
        const std::string why
            = "no closed interval, so there is no window to divide by";
        row.fabric.achieved_bytes_per_cycle
            = bench::measured<double>::unavailable(why);
        row.fabric.percent_of_configured_peak
            = bench::measured<double>::unavailable(why);
    }
    const std::string outstanding_reason
        = "neo_local_sram_fabric keeps no outstanding-count counter; the "
          "configuration bounds it at max_outstanding_per_requester = "
        + std::to_string(fabric_config.max_outstanding_per_requester)
        + " per requester (D15), which is a bound rather than a measurement";
    row.fabric.peak_outstanding
        = bench::measured<std::uint64_t>::unavailable(outstanding_reason);
    row.fabric.average_outstanding
        = bench::measured<double>::unavailable(outstanding_reason);
    row.fabric.arbitration_wait_cycles = bench::measured<double>::unavailable(
        "neo_local_sram_fabric counts arbitration events and bank conflicts "
        "but keeps no arbitration wait-cycle counter. In `arbitrated` mode the "
        "per-requester total_latency includes waiting, but it is total latency "
        "and may not be reported as the arbitration wait §8.4 asks for");

    // ── the MXU (§8.5) ──────────────────────────────────────────────────────

    if (opt.implementation == bench::bench_impl::mxu && interval_closed) {
        const std::uint64_t jobs = delta(opened.mxu_jobs, closed.mxu_jobs);
        const std::uint64_t requests = delta(opened.mxu_local_requests,
                                             closed.mxu_local_requests);
        const std::uint64_t local_bytes = delta(opened.mxu_local_bytes,
                                                closed.mxu_local_bytes);
        const std::uint32_t executed_k
            = (opt.fault_flags & BENCH_FAULT_CORRUPT_ARITH) != 0u
            ? shape.k - 1u : shape.k;
        const std::uint64_t executed_operand_bytes
            = static_cast<std::uint64_t>(shape.m) * executed_k
            + static_cast<std::uint64_t>(executed_k) * shape.n;
        const std::uint64_t expected_local_bytes
            = executed_operand_bytes + output_bytes;
        if (jobs != 1) {
            fail(failure_kind::harness,
                 "the MXU implementation accepted " + std::to_string(jobs)
                     + " jobs inside one benchmark interval, expected exactly one");
        }
        if (local_bytes != expected_local_bytes) {
            fail(failure_kind::harness,
                 "the MXU moved " + std::to_string(local_bytes)
                     + " local bytes, expected A+B+C = "
                     + std::to_string(expected_local_bytes));
        }
        if (closed.mxu_committed_bytes != output_bytes) {
            fail(failure_kind::harness,
                 "the MXU committed "
                     + std::to_string(closed.mxu_committed_bytes)
                     + " result bytes, expected " + std::to_string(output_bytes));
        }

        const auto as_cycles = [&](std::uint64_t ns) {
            return static_cast<std::uint64_t>(std::llround(
                static_cast<double>(ns) / opt.core_period_ns));
        };
        const std::uint64_t prefetch_cycles
            = as_cycles(closed.mxu_timing.prefetch_ns);
        const std::uint64_t compute_cycles
            = as_cycles(closed.mxu_timing.compute_ns);
        const std::uint64_t writeback_cycles
            = as_cycles(closed.mxu_timing.writeback_ns);
        row.mxu.prefetch_cycles
            = bench::measured<std::uint64_t>::present(prefetch_cycles);
        row.mxu.source_compute_cycles
            = bench::measured<std::uint64_t>::present(compute_cycles);
        row.mxu.writeback_cycles
            = bench::measured<std::uint64_t>::present(writeback_cycles);
        row.mxu.array_utilization = bench::measured<double>::present(
            static_cast<double>(shape.m) * shape.n
            / (static_cast<double>(mxu.rows) * mxu.columns));
        row.mxu.operand_bytes
            = bench::measured<std::uint64_t>::present(executed_operand_bytes);
        row.mxu.result_bytes
            = bench::measured<std::uint64_t>::present(output_bytes);
        row.mxu.native_requests
            = bench::measured<std::uint64_t>::present(requests);
        if (compute_cycles > 0) {
            const double useful_operations
                = 2.0 * shape.m * shape.n * executed_k;
            row.mxu.useful_operations_per_cycle
                = bench::measured<double>::present(
                    useful_operations / static_cast<double>(compute_cycles));
        } else {
            row.mxu.useful_operations_per_cycle
                = bench::measured<double>::unavailable(
                    "the MXU reported zero source-compute cycles");
        }
    } else {
        const std::string no_mxu
            = opt.implementation == bench::bench_impl::mxu
            ? "the measured interval did not close, so no MXU job delta exists"
            : std::string(bench::to_string(opt.benchmark))
                + " does not program the MXU";
        row.mxu.prefetch_cycles
            = bench::measured<std::uint64_t>::unavailable(no_mxu);
        row.mxu.source_compute_cycles
            = bench::measured<std::uint64_t>::unavailable(no_mxu);
        row.mxu.writeback_cycles
            = bench::measured<std::uint64_t>::unavailable(no_mxu);
        row.mxu.array_utilization = bench::measured<double>::unavailable(no_mxu);
        row.mxu.operand_bytes
            = bench::measured<std::uint64_t>::unavailable(no_mxu);
        row.mxu.result_bytes
            = bench::measured<std::uint64_t>::unavailable(no_mxu);
        row.mxu.native_requests
            = bench::measured<std::uint64_t>::unavailable(no_mxu);
        row.mxu.useful_operations_per_cycle
            = bench::measured<double>::unavailable(no_mxu);
    }

    row.accuracy_notes = bench::standard_accuracy_notes(opt.mode);
    row.accuracy_notes.emplace_back(
        "Every destination buffer was filled with a poison value before the "
        "run, so a kernel that wrote nothing cannot pass a case whose golden "
        "output is all zeros. The result therefore evidences that it was "
        "written, not only that it matches.");
    row.accuracy_notes.emplace_back(
        "The hart's external request and byte counts include instruction "
        "fetch: the reset PC is in global boot ROM, which is outside the core, "
        "so every fetch crosses the external bridge (D15). They are not a "
        "data-traffic figure.");
    row.accuracy_notes.emplace_back(
        "The interval markers are ordinary stores to the simulator host-I/O "
        "window, so each marker store is itself inside the interval it opens "
        "or closes. This is a boundary skew of one store per side, recorded "
        "rather than silently corrected.");
    if (opt.fault_flags != 0) {
        row.accuracy_notes.emplace_back(
            "A fault was deliberately injected into this run. It is a negative "
            "control, not a measurement, and its numbers describe a machine "
            "that was made to misbehave.");
    }

    // ── G3: accounting conservation ─────────────────────────────────────────
    //
    // Every identity below compares two quantities that must be equal if the
    // run describes one consistent machine. They are checked, not merely
    // reported: a row whose bytes do not reconcile is not a slower correct
    // measurement, it is a measurement of something nobody can name. An
    // unbalanced identity therefore fails the run like any other gate.
    //
    // Each pairs an observer inside the core with one outside it wherever the
    // model allows, because two counters incremented by the same line of code
    // agree by construction and prove nothing.

    bool counter_mutation_applied = false;
    std::vector<std::string> hooked_sources;
    // Wraps a counter where it enters an identity, so a control can name one
    // term of a sum rather than the sum.
    const auto counter = [&](const char* name, std::uint64_t value) {
        if (std::find(hooked_sources.begin(), hooked_sources.end(), name)
            == hooked_sources.end()) {
            hooked_sources.emplace_back(name);
        }
        if (opt.counter_mutation == name) {
            ++value;
            counter_mutation_applied = true;
        }
        return value;
    };

    bool accounting_mutation_applied = false;
    const auto reconcile = [&](const char* name, const char* left_label,
                               std::uint64_t left, const char* right_label,
                               std::uint64_t right, const char* basis) {
        if (opt.accounting_mutation == name) {
            ++left;
            accounting_mutation_applied = true;
        }
        bench::conservation_identity identity;
        identity.name = name;
        identity.left_label = left_label;
        identity.left = left;
        identity.right_label = right_label;
        identity.right = right;
        identity.balanced = left == right;
        identity.basis = basis;
        row.conservation.push_back(identity);
        if (!identity.balanced) {
            fail(failure_kind::conservation,
                 std::string(name) + ": " + left_label + " = "
                     + std::to_string(left) + " but " + right_label + " = "
                     + std::to_string(right));
        }
    };

    // 1. Tensor footprint. Everything the runner placed before the interval
    //    went through the debug path, which core_sram counts separately from
    //    workload traffic — so the memory's own figure is an independent view
    //    of what was staged.
    reconcile("tensor_footprint",
              "core SRAM debug-written bytes",
              counter("sram_debug_bytes", neo.sram().debug_bytes_written()),
              "input + output tensor bytes",
              static_cast<std::uint64_t>(input_bytes) + output_bytes,
              "the runner stages exactly one input tensor and one poisoned "
              "output tensor into core SRAM before the interval opens, and the "
              "debug path is the only writer of those bytes");

    if (interval_closed) {
        // 2. The local plane, from both ends. The fabric counts what its
        //    requesters moved; core SRAM counts what it served. Neither sees
        //    the other's counter.
        std::uint64_t requester_bytes = 0;
        for (const auto& requester : row.fabric.requesters) {
            requester_bytes += counter(
                (std::string("fabric_") + requester.requester + "_bytes")
                    .c_str(),
                requester.transferred_bytes);
        }
        reconcile("local_plane_bytes",
                  "fabric requester bytes",
                  requester_bytes,
                  "core SRAM read + written bytes",
                  counter("sram_bytes_read",
                          delta(opened.sram_bytes_read,
                                closed.sram_bytes_read))
                      + counter("sram_bytes_written",
                                delta(opened.sram_bytes_written,
                                      closed.sram_bytes_written)),
                  "every byte a requester moves on the native local plane is a "
                  "byte core SRAM serves; the debug path bypasses both");

        // 3. The external boundary, from both sides of the socket. The memory
        //    is outside the model, so this is the one identity neither end can
        //    satisfy on its own.
        //    The host-I/O window belongs on the right-hand side. It is not
        //    memory, but it is behind the same socket, and the hart reaches it
        //    through the same external port that reaches boot ROM — so a
        //    right-hand side of memory alone is short by exactly the stage and
        //    end markers the guest wrote inside the interval. Measured: 8
        //    bytes in kernel mode (kernel mark plus the end marker) and 16 in
        //    end-to-end (both DMA marks as well). It is added as its own term
        //    rather than folded into the memory figure, because a
        //    memory-bandwidth number must never include it.
        reconcile("external_boundary_bytes",
                  "core external bytes (hart + DMA)",
                  counter("hart_external_bytes", row.hart.external_bytes)
                      + counter("dma_external_bytes",
                                delta(opened.dma_external_bytes,
                                      closed.dma_external_bytes)),
                  "bytes served behind the external socket (memory + host I/O)",
                  // Read and write are separate terms, not one wrapped sum.
                  // Wrapping the sum would leave a dropped read or a doubled
                  // write undetected — the same argument that split the
                  // identity's own total into per-counter mutations, applied
                  // one level further down.
                  counter("world_memory_read_bytes",
                          delta(opened.world_memory_read_bytes,
                                closed.world_memory_read_bytes))
                      + counter("world_memory_write_bytes",
                                delta(opened.world_memory_write_bytes,
                                      closed.world_memory_write_bytes))
                      + counter("world_host_io_bytes",
                                delta(opened.world_host_io_bytes,
                                      closed.world_host_io_bytes)),
                  "the hart's external port and the DMA's external port are "
                  "the only initiators on the core's one external socket, and "
                  "boot ROM, global RAM and the simulator host-I/O window are "
                  "the only targets behind it");

        // 4. The traffic each requester was *supposed* to move, against what
        //    it did. Identity 2 only proves the fabric and core SRAM agree on
        //    a total; a kernel that read its input twice would inflate both
        //    and still balance, with the golden still correct. Tying the
        //    measurement to the algorithm's declared traffic is what notices.
        //
        //    Split by requester rather than lumped, because a lumped total
        //    lets an excess in one requester hide a shortfall in another.
        const auto requester_bytes_of = [&](const char* name) {
            for (const auto& entry : row.fabric.requesters) {
                if (entry.requester == name) {
                    return entry.transferred_bytes;
                }
            }
            return std::uint64_t{0};
        };

        const std::uint32_t executed_k
            = (opt.fault_flags & BENCH_FAULT_CORRUPT_ARITH) != 0u && shape.k > 0
            ? shape.k - 1u
            : shape.k;
        const char* kernel_requester
            = opt.implementation == bench::bench_impl::mxu ? "sa" : "cpu";
        reconcile("kernel_local_bytes",
                  opt.implementation == bench::bench_impl::mxu
                      ? "MXU local-plane bytes"
                      : "hart local-plane bytes",
                  counter("kernel_requester_bytes",
                          requester_bytes_of(kernel_requester)),
                  "bytes this algorithm declares it moves",
                  bench::expected_kernel_local_bytes(opt.benchmark, shape,
                                                     executed_k),
                  "each kernel's local traffic follows from its loop: one pass "
                  "over input and output, except GEMV over RVV which re-reads A "
                  "once per output column");

        reconcile("dma_local_bytes",
                  "DMA local-plane bytes",
                  counter("dma_requester_bytes", requester_bytes_of("dma")),
                  "bytes the DMA legs must move",
                  bench::expected_dma_local_bytes(opt.benchmark, shape,
                                                  opt.mode),
                  "end-to-end writes the input in and reads the result out "
                  "exactly once; kernel-only mode forbids the DMA any traffic "
                  "at all");

        // 5. The DMA's external side, on its own. Identity 3 lumps the hart
        //    with the DMA and memory with host I/O, so an extra DMA
        //    transaction increments both of its sides and stays balanced —
        //    the volume can be wrong while the boundary reconciles.
        reconcile("dma_external_bytes",
                  "DMA external-path bytes",
                  counter("dma_external_path_bytes",
                          delta(opened.dma_external_bytes,
                                closed.dma_external_bytes)),
                  "bytes the DMA legs must move",
                  bench::expected_dma_local_bytes(opt.benchmark, shape,
                                                  opt.mode),
                  "every byte a leg puts on the local plane it also takes off "
                  "the external one, so the DMA's two ports carry equal totals");

        if (opt.mode == bench::bench_mode::end_to_end) {
            reconcile("dma_leg_bytes",
                      "DMA local-path bytes",
                      counter("dma_local_path_bytes",
                              row.dma.local_path_bytes),
                      "input + output tensor bytes",
                      static_cast<std::uint64_t>(input_bytes) + output_bytes,
                      "end-to-end moves the input in and the result out "
                      "exactly once, and each leg touches the local path once");
        }
    }

    // 5. Stage times. The marks are the guest's own, so their span is an
    //    independent account of where the interval went.
    if (interval_closed) {
        double previous = row.hart.interval_begin_ns;
        bool monotonic = true;
        std::uint64_t inside = 0;
        for (const auto& entry : stages) {
            if (entry.at_ns < row.hart.interval_begin_ns
                || entry.at_ns > row.hart.interval_end_ns) {
                continue;
            }
            ++inside;
            if (entry.at_ns < previous) {
                monotonic = false;
            }
            previous = entry.at_ns;
        }
        if (!monotonic) {
            fail(failure_kind::conservation,
                 "the guest's stage marks inside the measured interval are not "
                 "monotonic in time, so no stage span derived from them is "
                 "meaningful");
        }
        // Record the spans as well as checking them: G4 needs to know where
        // an interval went, and a stage boundary that exists only inside this
        // function is one nobody downstream can use.
        //
        // The prologue is a span like any other. The interval opens at the
        // begin marker and the guest reaches its first stage mark a few
        // instructions later — measured at 86 ns on a GEMM end-to-end run —
        // and leaving that unnamed would make the stage list quietly fail to
        // add up to the interval it describes.
        double first_inside = row.hart.interval_end_ns;
        for (const auto& entry : stages) {
            if (entry.at_ns >= row.hart.interval_begin_ns
                && entry.at_ns <= row.hart.interval_end_ns) {
                first_inside = entry.at_ns;
                break;
            }
        }
        if (first_inside > row.hart.interval_begin_ns) {
            bench::stage_span prologue;
            prologue.stage = "interval prologue (before the first stage mark)";
            prologue.begin_ns = row.hart.interval_begin_ns;
            prologue.end_ns = first_inside;
            prologue.duration_ns = first_inside - row.hart.interval_begin_ns;
            row.stage_timing.push_back(prologue);
        }

        for (std::size_t i = 0; i < stages.size(); ++i) {
            if (stages[i].at_ns < row.hart.interval_begin_ns
                || stages[i].at_ns > row.hart.interval_end_ns) {
                continue;
            }
            bench::stage_span span;
            span.stage = stage_name(stages[i].stage);
            span.begin_ns = stages[i].at_ns;
            span.end_ns = i + 1 < stages.size()
                    && stages[i + 1].at_ns <= row.hart.interval_end_ns
                ? stages[i + 1].at_ns
                : row.hart.interval_end_ns;
            span.duration_ns = span.end_ns - span.begin_ns;
            row.stage_timing.push_back(span);
        }

        if (inside == 0) {
            fail(failure_kind::conservation,
                 "no stage mark falls inside the measured interval, so the "
                 "interval cannot be attributed to any stage");
        }

        // The exact sequence, not merely "at least one mark".
        //
        // `interval_accounted_ns` cannot see a missing intermediate marker: a
        // neighbouring span simply widens and the total still equals the
        // interval. Only the expected sequence notices, and without it every
        // per-stage duration derived below could be attributed to the wrong
        // stage while the arithmetic stayed consistent.
        std::vector<std::uint32_t> expected_sequence;
        if (opt.mode == bench::bench_mode::end_to_end
            && (opt.fault_flags & BENCH_FAULT_SKIP_DMA_IN) == 0u) {
            expected_sequence.push_back(BENCH_STAGE_DMA_IN);
        }
        // The kernel stage is always expected. `BENCH_FAULT_SKIP_STAGE`
        // omits only the *marker* — the kernel still runs — so subtracting it
        // here would make the expectation adjust to the mutation and the
        // control prove nothing. The DMA faults are different: skipping a leg
        // means the guest genuinely does not perform that stage, so those two
        // are subtracted.
        expected_sequence.push_back(BENCH_STAGE_KERNEL);
        if (opt.mode == bench::bench_mode::end_to_end
            && (opt.fault_flags & BENCH_FAULT_SKIP_DMA_OUT) == 0u) {
            expected_sequence.push_back(BENCH_STAGE_DMA_OUT);
        }

        std::vector<std::uint32_t> observed_sequence;
        for (const auto& entry : stages) {
            if (entry.at_ns >= row.hart.interval_begin_ns
                && entry.at_ns <= row.hart.interval_end_ns) {
                observed_sequence.push_back(entry.stage);
            }
        }

        if (observed_sequence != expected_sequence) {
            const auto render = [](const std::vector<std::uint32_t>& list) {
                std::string text;
                for (std::uint32_t stage : list) {
                    if (!text.empty()) {
                        text += " -> ";
                    }
                    text += stage_name(stage);
                }
                return text.empty() ? std::string("(none)") : text;
            };
            fail(failure_kind::conservation,
                 "the stages inside the measured interval were "
                     + render(observed_sequence) + ", expected "
                     + render(expected_sequence)
                     + "; a per-stage duration cannot be attributed to a stage "
                       "the guest did not announce");
        }

        // 5. The interval, accounted for. Every nanosecond between the two
        //    markers belongs to exactly one span, so the spans must add up to
        //    the interval — no gap, no double-counting. Rounded to whole
        //    nanoseconds because both sides are derived from the same
        //    picosecond-resolution timestamps and an exact float comparison
        //    would test the arithmetic of `double`, not the accounting.
        if (inside > 0) {
            double accounted = 0.0;
            for (const auto& span : row.stage_timing) {
                accounted += span.duration_ns;
            }
            reconcile("interval_accounted_ns",
                      "sum of stage spans, ns",
                      static_cast<std::uint64_t>(std::llround(accounted)),
                      "measured interval, ns",
                      static_cast<std::uint64_t>(
                          std::llround(row.hart.elapsed_ns.value)),
                      "the interval is partitioned by the guest's own stage "
                      "marks plus the prologue before the first of them, so "
                      "the parts leave no gap and overlap nowhere");
        }

        // The MXU's own staging must fit inside the kernel stage that
        // contains it. Its three terms come from the engine, the span from
        // the guest; neither derives from the other.
        if (opt.implementation == bench::bench_impl::mxu
            && row.mxu.source_compute_cycles.available) {
            const double mxu_ns = closed.mxu_timing.prefetch_ns
                + closed.mxu_timing.compute_ns
                + closed.mxu_timing.writeback_ns;
            double kernel_span = 0.0;
            for (std::size_t i = 0; i < stages.size(); ++i) {
                if (stages[i].stage != BENCH_STAGE_KERNEL) {
                    continue;
                }
                const double ends = i + 1 < stages.size()
                    ? stages[i + 1].at_ns
                    : row.hart.interval_end_ns;
                kernel_span = ends - stages[i].at_ns;
                break;
            }
            if (kernel_span > 0.0 && mxu_ns > kernel_span) {
                std::ostringstream why;
                why << "the MXU reports " << mxu_ns
                    << " ns of prefetch, compute and writeback inside a kernel "
                       "stage that lasted "
                    << kernel_span
                    << " ns; a stage total larger than the stage containing it "
                       "cannot be attributed to that interval";
                fail(failure_kind::conservation, why.str());
            }
        }
    }

    if (!opt.counter_mutation.empty()) {
        if (!counter_mutation_applied) {
            std::cerr << "--inject-counter named '" << opt.counter_mutation
                      << "', which no identity in this run reads; the control "
                         "would have proved nothing\n";
            return kUsage;
        }
        if (!any_of_kind(failure_kind::conservation)) {
            std::cerr << "--inject-counter perturbed '" << opt.counter_mutation
                      << "' and no identity noticed; that counter is not "
                         "actually wired into the accounting it appears in\n";
            return kFail;
        }
    }

    if (!opt.accounting_mutation.empty() && !accounting_mutation_applied) {
        std::cerr << "--inject-accounting named '" << opt.accounting_mutation
                  << "', which is not an identity this run evaluates; the "
                     "control would have proved nothing\n";
        return kUsage;
    }

    if (opt.list_counter_sources) {
        std::sort(hooked_sources.begin(), hooked_sources.end());
        for (const auto& name : hooked_sources) {
            std::cout << "counter-source: " << name << '\n';
        }
        return kPass;
    }

    // ── the verdict, after every check has run ──────────────────────────────

    row.run_valid = failures.empty();
    // Correctness and validity intentionally answer different questions. A
    // VLEN or MXU identity mismatch rejects the row, but does not turn output
    // that matched the independent golden into an arithmetic failure.
    row.correctness.passed
        = readback_ok && !any_of_kind(failure_kind::golden);
    for (const auto& entry : failures) {
        row.failures.push_back({to_string(entry.kind), entry.why});
    }

    // ── output ──────────────────────────────────────────────────────────────

    if (!opt.result_path.empty()) {
        std::ofstream result(opt.result_path, std::ios::trunc);
        if (!result.good()) {
            std::cerr << "could not open '" << opt.result_path
                      << "' for writing\n";
            return kFail;
        }
        bench::write_json(result, row);
        result.close();
        if (!result.good()) {
            std::cerr << "failed while writing '" << opt.result_path << "'\n";
            return kFail;
        }
    }

    if (!opt.quiet) {
        // Human-readable, and explicitly not the source of numeric truth: §11
        // requires the aggregation to read the JSON row, never a parsed log.
        std::cout << "neo_core_bench_runner  "
                  << bench::to_string(opt.benchmark) << " case "
                  << opt.case_index << "  N=" << elements << "  "
                  << bench::to_string(opt.implementation) << "  "
                  << bench::to_string(opt.mode) << "  pattern "
                  << bench::to_string(opt.pattern) << '\n'
                  << "  run valid        "
                  << (row.run_valid ? "yes" : "no") << '\n'
                  << "  correctness      "
                  << (row.correctness.passed ? "PASS" : "FAIL") << '\n';
        if (interval_closed) {
            std::cout << "  interval         " << row.hart.interval_begin_ns
                      << " ns .. " << row.hart.interval_end_ns << " ns ("
                      << row.hart.elapsed_ns.value << " ns)\n"
                      << "  retired instr    "
                      << row.hart.retired_instructions.value << '\n'
                      << "  hart local/ctrl/ext requests   "
                      << row.hart.local_requests << " / "
                      << row.hart.control_requests << " / "
                      << row.hart.external_requests << '\n';
            if (row.hart.lane_utilization.available) {
                std::cout << "  vlmax/iters/tail " << vlmax << " / "
                          << vector_iterations << " / "
                          << row.hart.tail_elements.value << "  (lane use "
                          << row.hart.lane_utilization.value << ")\n";
            }
            if (row.dma.transfers > 0) {
                std::cout << "  dma transfers    " << row.dma.transfers
                          << ", local/external path bytes "
                          << row.dma.local_path_bytes << " / "
                          << row.dma.external_path_bytes;
                if (row.dma.bytes_in.available) {
                    std::cout << "  (in " << row.dma.bytes_in.value << ", out "
                              << row.dma.bytes_out.value << ")";
                }
                std::cout << '\n';
            }
        }
        std::cout << "  (this text is a summary; the machine-readable row is "
                     "the JSON file)\n";
    }

    for (const auto& entry : failures) {
        std::cerr << "FAIL [" << to_string(entry.kind) << "]: " << entry.why
                  << '\n';
    }

    // ── negative-control accounting ─────────────────────────────────────────
    //
    // A control asserts two things:
    //
    //   1. the named detection fired;
    //   2. **nothing else** failed.
    //
    // The second is what makes the first mean anything. Every destination is
    // poisoned before the run, so a guest that never reaches the kernel — a
    // watchdog expiry, a trap, a parameter block that did not arrive — also
    // mismatches the golden. An earlier version of this checked only (1), and
    // a control run with a watchdog too short to reach the kernel reported
    // `negative control satisfied: golden_mismatch` while the mutation under
    // test had never executed. That is the failure mode a control exists to
    // rule out, reproduced by the control itself.
    //
    // So the preconditions must hold — the guest ran to completion with the
    // right identity and parameters, the interval is well formed, the harness
    // is not at fault — except for whichever one is itself the mutation.
    if (!opt.expect.empty()) {
        // Every named detection must fire, and no failure may fall outside
        // the kinds those detections name. Both halves matter: the first
        // proves the mutation was caught by each check that should catch it,
        // the second proves the run was otherwise complete — a guest that
        // never reached the kernel also mismatches a poisoned destination.
        std::vector<failure_kind> allowed;
        std::vector<std::string> not_fired;
        for (expected_detection detection : opt.expect) {
            allowed.push_back(permitted_kind(detection));
            const bool fired
                = detection == expected_detection::golden_mismatch ? mismatch
                : detection == expected_detection::interval_not_closed
                    ? !interval_closed
                    : any_of_kind(failure_kind::conservation);
            if (!fired) {
                not_fired.emplace_back(to_string(detection));
            }
        }

        std::vector<const failure*> unexpected;
        for (const auto& entry : failures) {
            if (std::find(allowed.begin(), allowed.end(), entry.kind)
                == allowed.end()) {
                unexpected.push_back(&entry);
            }
        }

        if (!unexpected.empty()) {
            std::cerr << "NEGATIVE CONTROL FAILED: " << unexpected.size()
                      << " failure(s) outside the categories this control "
                         "permits. The mutation cannot be said to have been "
                         "detected when the run did not otherwise complete:\n";
            for (const failure* entry : unexpected) {
                std::cerr << "  [" << to_string(entry->kind) << "] "
                          << entry->why << '\n';
            }
            return kFail;
        }
        if (!not_fired.empty()) {
            std::cerr << "NEGATIVE CONTROL FAILED: the run completed without "
                         "detecting";
            for (const auto& name : not_fired) {
                std::cerr << " '" << name << "'";
            }
            std::cerr << "; the injected fault was not caught by every check "
                         "that should have caught it\n";
            return kFail;
        }
        if (failures.empty()) {
            std::cerr << "NEGATIVE CONTROL FAILED: detections were expected "
                         "but no check reported a failure\n";
            return kFail;
        }
        std::cout << "negative control satisfied:";
        for (expected_detection detection : opt.expect) {
            std::cout << " '" << to_string(detection) << "'";
        }
        std::cout << " fired, and nothing outside their categories did\n";
        return kPass;
    }

    if (!failures.empty()) {
        std::cerr << failures.size() << " check(s) failed\n";
        return kFail;
    }
    return kPass;
}
