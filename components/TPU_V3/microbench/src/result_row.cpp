// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/bench/result_row.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace cdc::components::tpu_v3::bench {
namespace {

/// A small deterministic JSON emitter.
///
/// Hand-written because the row has to be diffable: keys come out in the order
/// they are written and a double always prints with the same precision, so two
/// runs that measured the same thing produce byte-identical text and a
/// regression is a diff rather than a comparison of floating-point spellings.
class json_writer {
public:
    explicit json_writer(std::ostream& out) : out_(out)
    {
        out_ << std::setprecision(12);
    }

    void begin_object(const char* key = nullptr)
    {
        separate();
        indent();
        if (key != nullptr) {
            out_ << quoted(key) << ": ";
        }
        out_ << "{\n";
        ++depth_;
        first_.push_back(true);
    }

    void end_object()
    {
        --depth_;
        first_.pop_back();
        out_ << '\n';
        indent();
        out_ << '}';
        mark_written();
    }

    void begin_array(const char* key)
    {
        separate();
        indent();
        out_ << quoted(key) << ": [\n";
        ++depth_;
        first_.push_back(true);
    }

    void end_array()
    {
        --depth_;
        first_.pop_back();
        out_ << '\n';
        indent();
        out_ << ']';
        mark_written();
    }

    void field(const char* key, const std::string& value)
    {
        scalar(key, quoted(value));
    }

    void field(const char* key, const char* value)
    {
        scalar(key, quoted(std::string(value)));
    }

    void field(const char* key, bool value)
    {
        scalar(key, value ? "true" : "false");
    }

    void field(const char* key, std::uint64_t value)
    {
        scalar(key, std::to_string(value));
    }

    void field(const char* key, std::uint32_t value)
    {
        scalar(key, std::to_string(value));
    }

    void field(const char* key, std::int32_t value)
    {
        scalar(key, std::to_string(value));
    }

    void field(const char* key, double value)
    {
        std::ostringstream text;
        text << std::setprecision(12) << value;
        scalar(key, text.str());
    }

    /// A `measured<T>`: the value, or an object saying why it is missing.
    template <typename T>
    void field(const char* key, const measured<T>& value)
    {
        if (value.available) {
            field(key, value.value);
            return;
        }
        separate();
        indent();
        // An empty reason is a defect in the caller, not a valid state: §8
        // asks for `unavailable` *and* why. Saying so in the row is better
        // than emitting an empty string that reads like an intentional blank.
        const std::string why = value.reason.empty()
            ? std::string("no reason recorded; the writer of this row did not "
                          "set one, which is a defect")
            : value.reason;
        out_ << quoted(key) << ": {\"unavailable\": " << quoted(why) << '}';
        mark_written();
    }

    void raw_array_element(const std::string& text)
    {
        separate();
        indent();
        out_ << text;
        mark_written();
    }

private:
    void scalar(const char* key, const std::string& text)
    {
        separate();
        indent();
        out_ << quoted(key) << ": " << text;
        mark_written();
    }

    void separate()
    {
        if (first_.empty()) {
            return;
        }
        if (!first_.back()) {
            out_ << ",\n";
        }
    }

    void mark_written()
    {
        if (!first_.empty()) {
            first_.back() = false;
        }
    }

    void indent()
    {
        for (int i = 0; i < depth_; ++i) {
            out_ << "  ";
        }
    }

    static std::string quoted(const std::string& text)
    {
        std::string result = "\"";
        for (char c : text) {
            switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    static const char digits[] = "0123456789abcdef";
                    result += "\\u00";
                    result += digits[(c >> 4) & 0xF];
                    result += digits[c & 0xF];
                } else {
                    result += c;
                }
            }
        }
        result += '"';
        return result;
    }

    std::ostream& out_;
    int depth_ = 0;
    std::vector<bool> first_;
};

} // namespace

std::vector<std::string> standard_accuracy_notes(bench_mode mode)
{
    std::vector<std::string> notes{
        "RISC-V VP++ is an instruction-functional model; its vector memory "
        "interface emits element-wise TLM accesses, so a 512-bit register "
        "operation is not one 64-byte SRAM transaction (D7, plan section 12).",
        "Local-fabric request and beat counts are TLM/native quantities. They "
        "are not hardware AXI beats and not vector-instruction counts.",
        "No result from this standalone study is a NoC, multi-core or "
        "chip-level result (D27).",
        "The model has no calibrated area, power or post-place-and-route "
        "frequency model; no configuration may be called best on this "
        "evidence alone.",
    };
    if (mode == bench_mode::kernel_only) {
        notes.emplace_back(
            "Kernel-only: host debug initialisation of core SRAM is outside "
            "the measured interval and moves no DMA or fabric bytes. This mode "
            "may not be quoted as a DMA or external-memory result (section "
            "6.1).");
    } else {
        notes.emplace_back(
            "End-to-end: the external memory model is not calibrated for "
            "latency, bandwidth or outstanding depth, so DMA sufficiency and "
            "DMA-channel conclusions are not supported by this row (section "
            "7.2).");
    }
    return notes;
}

std::string validate_configuration_provenance(
    const config_record& configuration)
{
    // Deliberately transcribed from the serialized configuration object below.
    // Generating this from identity_provenance would agree with an omission in
    // that same map and therefore prove nothing.
    static constexpr std::array<const char*, 22> required = {
        "id",
        "xlen",
        "vlen_bits",
        "elen_bits",
        "rvv_version",
        "mxu_geometry",
        "mxu_datatype",
        "mxu_source_revision",
        "dma_controllers",
        "dma_channels",
        "dma_max_burst_bytes",
        "external_axi_data_width_bits",
        "sram_capacity_bytes",
        "local_bank_width_bits",
        "local_bank_count",
        "fabric_pipeline_stages",
        "outstanding_per_requester",
        "bank_mapping",
        "arbitration",
        "timing_mode",
        "core_period_ns",
        "physical_values_provisional",
    };

    std::set<std::string> seen;
    for (const auto& entry : configuration.identity_provenance) {
        const bool known_label = entry.second == "live_readback"
            || entry.second == "configured"
            || entry.second == "structural_literal";
        if (!known_label) {
            return "configuration field '" + entry.first
                + "' has unknown provenance label '" + entry.second + "'";
        }
        const bool known_field
            = std::any_of(required.begin(), required.end(), [&](const char* name) {
                  return entry.first == name;
              });
        if (!known_field) {
            return "unexpected configuration provenance field '" + entry.first
                + "'";
        }
        if (!seen.insert(entry.first).second) {
            return "duplicate configuration provenance field '" + entry.first
                + "'";
        }
    }

    for (const char* name : required) {
        if (seen.count(name) == 0) {
            return std::string("missing configuration provenance field '") + name
                + "'";
        }
    }
    return {};
}

void write_json(std::ostream& out, const result_row& row)
{
    const std::string provenance_error
        = validate_configuration_provenance(row.configuration);
    if (!provenance_error.empty()) {
        throw std::invalid_argument(provenance_error);
    }

    json_writer json(out);

    json.begin_object();

    json.field("schema", row.schema);
    json.field("scope", row.scope);
    json.field("chip_composition", row.chip_composition);
    json.field("noc_instantiated", row.noc_instantiated);
    json.field("run_valid", row.run_valid);
    json.begin_array("conservation");
    for (const auto& identity : row.conservation) {
        json.begin_object();
        json.field("name", identity.name);
        json.field("balanced", identity.balanced);
        json.field("left_label", identity.left_label);
        json.field("left", identity.left);
        json.field("right_label", identity.right_label);
        json.field("right", identity.right);
        json.field("basis", identity.basis);
        json.end_object();
    }
    json.end_array();
    json.begin_array("stage_timing");
    for (const auto& span : row.stage_timing) {
        json.begin_object();
        json.field("stage", span.stage);
        json.field("begin_ns", span.begin_ns);
        json.field("end_ns", span.end_ns);
        json.field("duration_ns", span.duration_ns);
        json.end_object();
    }
    json.end_array();
    json.begin_array("failures");
    for (const auto& entry : row.failures) {
        json.begin_object();
        json.field("kind", entry.kind);
        json.field("detail", entry.detail);
        json.end_object();
    }
    json.end_array();

    // ── identity and reproducibility (section 8.1) ────────────────────────
    json.begin_object("identity");
    json.field("benchmark", to_string(row.identity.benchmark));
    json.field("case_index",
               static_cast<std::uint64_t>(row.identity.case_index));
    json.begin_object("shape");
    json.field("elements", row.identity.shape.elements);
    json.field("m", row.identity.shape.m);
    json.field("n", row.identity.shape.n);
    json.field("k", row.identity.shape.k);
    json.end_object();
    json.field("datatype", row.identity.datatype);
    json.field("mode", to_string(row.identity.mode));
    json.field("implementation", to_string(row.identity.implementation));
    json.field("input_pattern", to_string(row.identity.pattern));
    json.field("seed", row.identity.seed);
    json.field("firmware_elf", row.identity.firmware_elf_path);
    json.field("firmware_elf_sha256",
               row.identity.firmware_elf_sha256.empty()
                   ? std::string("unavailable")
                   : row.identity.firmware_elf_sha256);
    json.field("build_type", row.identity.build_type);
    json.field("source_revision", row.identity.source_revision);
    json.field("fault_flags", row.identity.fault_flags);
    json.field("injected_fault", row.identity.injected_fault);
    json.end_object();

    // ── configuration (section 8.1) ──────────────────────────────────────
    json.begin_object("configuration");
    json.field("id", row.configuration.id);
    json.field("xlen", row.configuration.xlen);
    json.field("vlen_bits", row.configuration.vlen_bits);
    json.field("elen_bits", row.configuration.elen_bits);
    json.field("rvv_version", row.configuration.rvv_version);
    json.field("mxu_geometry", row.configuration.mxu_geometry);
    json.field("mxu_datatype", row.configuration.mxu_datatype);
    json.field("mxu_source_revision", row.configuration.mxu_source_revision);
    json.field("dma_controllers", row.configuration.dma_controllers);
    json.field("dma_channels", row.configuration.dma_channels);
    json.field("dma_max_burst_bytes", row.configuration.dma_max_burst_bytes);
    json.field("external_axi_data_width_bits",
               row.configuration.external_axi_data_width_bits);
    json.field("sram_capacity_bytes", row.configuration.sram_capacity_bytes);
    json.field("local_bank_width_bits", row.configuration.bank_width_bits);
    json.field("local_bank_count", row.configuration.bank_count);
    json.field("fabric_pipeline_stages", row.configuration.pipeline_stages);
    json.field("outstanding_per_requester",
               row.configuration.outstanding_per_requester);
    json.field("bank_mapping", row.configuration.bank_mapping);
    json.field("arbitration", row.configuration.arbitration);
    json.field("timing_mode", row.configuration.timing_mode);
    json.field("core_period_ns", row.configuration.core_period_ns);
    json.field("physical_values_provisional", row.configuration.provisional);
    json.begin_object("identity_provenance");
    for (const auto& entry : row.configuration.identity_provenance) {
        json.field(entry.first.c_str(), entry.second);
    }
    json.end_object();
    json.end_object();

    // ── correctness (section 8.1) ────────────────────────────────────────
    json.begin_object("correctness");
    json.field("passed", row.correctness.passed);
    json.field("elements_compared", row.correctness.elements_compared);
    json.field("has_mismatch", row.correctness.has_mismatch);
    json.field("mismatch_count", row.correctness.mismatch_count);
    json.field("first_mismatch_index", row.correctness.first_mismatch_index);
    json.field("expected", row.correctness.expected);
    json.field("observed", row.correctness.observed);
    json.field("golden_checksum",
               static_cast<std::uint64_t>(row.correctness.golden_checksum));
    json.field("guest_checksum",
               static_cast<std::uint64_t>(row.correctness.guest_checksum));
    json.field("input_negatives", row.correctness.profile.negatives);
    json.field("input_positives", row.correctness.profile.positives);
    json.field("input_zeros", row.correctness.profile.zeros);
    json.field("detail", row.correctness.detail);
    json.end_object();

    // ── the hart and the measured interval (section 8.2) ─────────────────
    json.begin_object("hart");
    json.field("interval_begin_ns", row.hart.interval_begin_ns);
    json.field("interval_end_ns", row.hart.interval_end_ns);
    json.field("interval_closed", row.hart.interval_closed);
    json.field("elapsed_ns", row.hart.elapsed_ns);
    json.field("retired_instructions", row.hart.retired_instructions);
    json.field("mcycle_delta", row.hart.mcycle_delta);
    json.field("scalar_instructions", row.hart.scalar_instructions);
    json.field("vector_instructions", row.hart.vector_instructions);
    json.field("vlenb", row.hart.vlenb);
    json.field("vlmax", row.hart.vlmax);
    json.field("vl_first", row.hart.vl_first);
    json.field("vl_last", row.hart.vl_last);
    json.field("vector_iterations", row.hart.vector_iterations);
    json.field("active_elements", row.hart.active_elements);
    json.field("tail_elements", row.hart.tail_elements);
    json.field("lane_utilization", row.hart.lane_utilization);
    json.field("derivation", row.hart.derivation);
    json.field("local_requests", row.hart.local_requests);
    json.field("local_bytes", row.hart.local_bytes);
    json.field("control_requests", row.hart.control_requests);
    json.field("control_bytes", row.hart.control_bytes);
    json.field("external_requests", row.hart.external_requests);
    json.field("external_bytes", row.hart.external_bytes);
    json.end_object();

    // ── DMA and external memory (section 8.3) ────────────────────────────
    json.begin_object("dma");
    json.field("bytes_in", row.dma.bytes_in);
    json.field("bytes_out", row.dma.bytes_out);
    json.field("bytes_total", row.dma.bytes_total);
    json.field("local_path_bytes", row.dma.local_path_bytes);
    json.field("external_path_bytes", row.dma.external_path_bytes);
    json.field("transfers", row.dma.transfers);
    json.field("chunks", row.dma.chunks);
    json.field("local_requests", row.dma.local_requests);
    json.field("external_requests", row.dma.external_requests);
    json.field("errors", row.dma.errors);
    json.field("average_chunk_bytes", row.dma.average_chunk_bytes);
    json.field("achieved_bytes_per_cycle", row.dma.achieved_bytes_per_cycle);
    json.field("busy_cycles", row.dma.busy_cycles);
    json.field("service_cycles", row.dma.service_cycles);
    json.field("wait_cycles", row.dma.wait_cycles);
    json.field("overlap_with_compute", row.dma.overlap_with_compute);
    json.field("external_memory_model", row.dma.external_memory_model);
    json.end_object();

    // ── the local SRAM fabric (section 8.4) ──────────────────────────────
    json.begin_object("local_fabric");
    json.field("achieved_bytes_per_cycle", row.fabric.achieved_bytes_per_cycle);
    json.field("percent_of_configured_peak",
               row.fabric.percent_of_configured_peak);
    json.field("arbitration_wait_cycles", row.fabric.arbitration_wait_cycles);
    json.field("peak_outstanding", row.fabric.peak_outstanding);
    json.field("average_outstanding", row.fabric.average_outstanding);
    json.begin_array("requesters");
    for (const auto& requester : row.fabric.requesters) {
        json.begin_object();
        json.field("requester", requester.requester);
        json.field("requests", requester.requests);
        json.field("physical_beats", requester.physical_beats);
        json.field("bank_conflicts", requester.bank_conflicts);
        json.field("arbitration_events", requester.arbitration_events);
        json.field("transferred_bytes", requester.transferred_bytes);
        json.field("errors", requester.errors);
        json.field("total_latency_ns", requester.total_latency_ns);
        json.end_object();
    }
    json.end_array();
    json.begin_array("bank_grants");
    for (std::size_t bank = 0; bank < row.fabric.bank_grants.size(); ++bank) {
        json.begin_object();
        json.field("bank", static_cast<std::uint64_t>(bank));
        json.begin_array("grants");
        const auto& per_requester = row.fabric.bank_grants[bank];
        for (std::size_t i = 0; i < per_requester.size(); ++i) {
            json.begin_object();
            json.field("requester",
                       i < row.fabric.bank_grant_requesters.size()
                           ? row.fabric.bank_grant_requesters[i]
                           : std::string("unknown"));
            json.field("count", per_requester[i]);
            json.end_object();
        }
        json.end_array();
        json.end_object();
    }
    json.end_array();
    json.end_object();

    // ── the MXU (section 8.5) ────────────────────────────────────────────
    json.begin_object("mxu");
    json.field("prefetch_cycles", row.mxu.prefetch_cycles);
    json.field("source_compute_cycles", row.mxu.source_compute_cycles);
    json.field("writeback_cycles", row.mxu.writeback_cycles);
    json.field("array_utilization", row.mxu.array_utilization);
    json.field("operand_bytes", row.mxu.operand_bytes);
    json.field("result_bytes", row.mxu.result_bytes);
    json.field("native_requests", row.mxu.native_requests);
    json.field("useful_operations_per_cycle",
               row.mxu.useful_operations_per_cycle);
    json.end_object();

    // ── the caveats, carried with the numbers ────────────────────────────
    json.begin_array("accuracy_notes");
    for (const auto& note : row.accuracy_notes) {
        json.begin_object();
        json.field("note", note);
        json.end_object();
    }
    json.end_array();

    json.end_object();
    out << '\n';
}

} // namespace cdc::components::tpu_v3::bench
