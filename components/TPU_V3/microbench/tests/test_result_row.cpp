// SPDX-License-Identifier: Apache-2.0
//
// The result-row schema (§8, §11).
//
// Two things are checked, and the second is the one that would otherwise be
// discovered by an aggregation script at three in the morning:
//
//   * an unavailable measurement serialises as an object carrying its reason,
//     never as a zero and never as an absent key. §8 requires "or explicitly
//     say `unavailable`", and a consumer that read a missing counter as zero
//     would be reading a performance claim;
//   * the emitted text is actually parseable JSON. The parser below is written
//     here rather than linked, so it agrees with the specification instead of
//     agreeing with the emitter -- the same reason the golden is recomputed on
//     the host rather than read back from the model.

#include "tpu_v3/bench/result_row.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace bench = cdc::components::tpu_v3::bench;

namespace {

int failures = 0;

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

/// A minimal, strict JSON reader. It accepts exactly what RFC 8259 accepts for
/// the value forms this schema emits and rejects a trailing comma, which is the
/// failure a hand-written emitter actually produces.
class json_parser {
public:
    explicit json_parser(const std::string& text) : text_(text) {}

    bool parse()
    {
        skip_space();
        if (!value()) {
            return false;
        }
        skip_space();
        return position_ == text_.size();
    }

    const std::string& error() const { return error_; }

private:
    bool value()
    {
        if (position_ >= text_.size()) {
            return fail("unexpected end of input");
        }
        const char c = text_[position_];
        if (c == '{') {
            return object();
        }
        if (c == '[') {
            return array();
        }
        if (c == '"') {
            return string();
        }
        if (c == 't') {
            return literal("true");
        }
        if (c == 'f') {
            return literal("false");
        }
        if (c == 'n') {
            return literal("null");
        }
        return number();
    }

    bool object()
    {
        ++position_; // '{'
        skip_space();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            return true;
        }
        for (;;) {
            skip_space();
            if (!string()) {
                return false;
            }
            skip_space();
            if (position_ >= text_.size() || text_[position_] != ':') {
                return fail("expected ':' after an object key");
            }
            ++position_;
            skip_space();
            if (!value()) {
                return false;
            }
            skip_space();
            if (position_ >= text_.size()) {
                return fail("unterminated object");
            }
            if (text_[position_] == ',') {
                ++position_;
                skip_space();
                if (position_ < text_.size() && text_[position_] == '}') {
                    return fail("trailing comma before '}'");
                }
                continue;
            }
            if (text_[position_] == '}') {
                ++position_;
                return true;
            }
            return fail("expected ',' or '}' in an object");
        }
    }

    bool array()
    {
        ++position_; // '['
        skip_space();
        if (position_ < text_.size() && text_[position_] == ']') {
            ++position_;
            return true;
        }
        for (;;) {
            skip_space();
            if (!value()) {
                return false;
            }
            skip_space();
            if (position_ >= text_.size()) {
                return fail("unterminated array");
            }
            if (text_[position_] == ',') {
                ++position_;
                skip_space();
                if (position_ < text_.size() && text_[position_] == ']') {
                    return fail("trailing comma before ']'");
                }
                continue;
            }
            if (text_[position_] == ']') {
                ++position_;
                return true;
            }
            return fail("expected ',' or ']' in an array");
        }
    }

    bool string()
    {
        if (position_ >= text_.size() || text_[position_] != '"') {
            return fail("expected a string");
        }
        ++position_;
        while (position_ < text_.size()) {
            const char c = text_[position_];
            if (c == '"') {
                ++position_;
                return true;
            }
            if (c == '\\') {
                position_ += 2;
                continue;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return fail("a raw control character inside a string");
            }
            ++position_;
        }
        return fail("unterminated string");
    }

    bool number()
    {
        const std::size_t start = position_;
        if (position_ < text_.size() && text_[position_] == '-') {
            ++position_;
        }
        while (position_ < text_.size()
               && (std::isdigit(static_cast<unsigned char>(text_[position_]))
                   || text_[position_] == '.' || text_[position_] == 'e'
                   || text_[position_] == 'E' || text_[position_] == '+'
                   || text_[position_] == '-')) {
            ++position_;
        }
        if (position_ == start) {
            return fail("expected a value");
        }
        return true;
    }

    bool literal(const char* expected)
    {
        const std::string word(expected);
        if (text_.compare(position_, word.size(), word) != 0) {
            return fail("expected " + word);
        }
        position_ += word.size();
        return true;
    }

    void skip_space()
    {
        while (position_ < text_.size()
               && (text_[position_] == ' ' || text_[position_] == '\n'
                   || text_[position_] == '\t' || text_[position_] == '\r')) {
            ++position_;
        }
    }

    bool fail(const std::string& why)
    {
        if (error_.empty()) {
            error_ = why + " at offset " + std::to_string(position_);
        }
        return false;
    }

    const std::string& text_;
    std::size_t position_ = 0;
    std::string error_;
};

std::vector<std::pair<std::string, std::string>> complete_provenance()
{
    // Independently transcribed from the configuration schema. Keeping this in
    // the test means a field added only to the writer is not automatically
    // blessed as complete.
    return {
        {"id", "configured"},
        {"xlen", "structural_literal"},
        {"vlen_bits", "live_readback"},
        {"elen_bits", "structural_literal"},
        {"rvv_version", "structural_literal"},
        {"mxu_geometry", "live_readback"},
        {"mxu_datatype", "live_readback"},
        {"mxu_source_revision", "live_readback"},
        {"dma_controllers", "structural_literal"},
        {"dma_channels", "structural_literal"},
        {"dma_max_burst_bytes", "configured"},
        {"external_axi_data_width_bits", "structural_literal"},
        {"sram_capacity_bytes", "live_readback"},
        {"local_bank_width_bits", "live_readback"},
        {"local_bank_count", "live_readback"},
        {"fabric_pipeline_stages", "live_readback"},
        {"outstanding_per_requester", "live_readback"},
        {"bank_mapping", "live_readback"},
        {"arbitration", "live_readback"},
        {"timing_mode", "live_readback"},
        {"core_period_ns", "configured"},
        {"physical_values_provisional", "structural_literal"},
    };
}

bench::result_row sample_row()
{
    bench::result_row row;
    row.identity.benchmark = bench::benchmark_id::relu;
    row.identity.case_index = 8;
    row.identity.shape = bench::case_at(bench::benchmark_id::relu, 8);
    row.identity.mode = bench::bench_mode::kernel_only;
    row.identity.implementation = bench::bench_impl::rvv;
    row.identity.pattern = bench::input_pattern::mixed;
    row.identity.seed = 0xDEADBEEF;
    row.identity.firmware_elf_path = "/tmp/relu.elf";
    row.identity.firmware_elf_sha256 = std::string(64, 'a');
    row.identity.build_type = "Release";
    row.identity.source_revision = "9c51595";

    row.configuration.id = "reference";
    row.configuration.mxu_geometry = "64x64";
    row.configuration.mxu_datatype = "int8xint8->int32";
    row.configuration.mxu_source_revision = "int8_64x64@deadbeef";
    row.configuration.timing_mode = "annotated";
    row.configuration.bank_mapping = "low_order_interleaved";
    row.configuration.bank_width_bits = 128;
    row.configuration.bank_count = 4;
    row.configuration.pipeline_stages = 2;
    row.configuration.core_period_ns = 10.0;
    row.configuration.identity_provenance = complete_provenance();

    row.correctness.passed = true;
    row.correctness.elements_compared = 17;
    row.correctness.detail = "all 17 elements match the host golden";

    row.hart.elapsed_ns = bench::measured<double>::present(1234.5);
    row.hart.retired_instructions
        = bench::measured<std::uint64_t>::present(4242);
    row.hart.scalar_instructions = bench::measured<std::uint64_t>::unavailable(
        "the pinned VP++ build compiles ISSStatsDummy, so no scalar/vector "
        "instruction split exists");
    row.hart.vector_instructions = row.hart.scalar_instructions;
    row.hart.lane_utilization = bench::measured<double>::present(0.53125);
    row.hart.derivation = "lane_utilization = N / (iterations * vl_first)";

    row.fabric.requesters.push_back({"cpu", 34, 34, 0, 34, 136, 0, 340.0});
    row.fabric.bank_grant_requesters = {"cpu", "dma"};
    row.fabric.bank_grants = {{17, 0}, {17, 0}, {0, 0}, {0, 0}};

    row.mxu.source_compute_cycles = bench::measured<std::uint64_t>::unavailable(
        "MB1 does not use the MXU");

    row.accuracy_notes
        = bench::standard_accuracy_notes(bench::bench_mode::kernel_only);
    return row;
}

void the_emitted_row_is_parseable_json()
{
    std::ostringstream text;
    bench::write_json(text, sample_row());
    const std::string json = text.str();

    json_parser parser(json);
    const bool parsed = parser.parse();
    if (!parsed) {
        std::cerr << "the emitted row is not valid JSON: " << parser.error()
                  << "\n---\n"
                  << json << "\n---\n";
        ++failures;
    }
}

void an_unavailable_measurement_carries_its_reason()
{
    std::ostringstream text;
    bench::write_json(text, sample_row());
    const std::string json = text.str();

    CHECK_MSG(json.find("\"scalar_instructions\": {\"unavailable\":")
                  != std::string::npos,
              "an unavailable measurement did not serialise as an object "
              "carrying its reason");
    CHECK_MSG(json.find("ISSStatsDummy") != std::string::npos,
              "the reason a measurement is unavailable did not reach the row");
    CHECK_MSG(json.find("\"scalar_instructions\": 0") == std::string::npos,
              "an unavailable measurement serialised as zero, which a consumer "
              "would read as a measurement");
}

/// A measurement left unavailable without a reason is a defect in the code
/// that built the row, and the row says so rather than emitting a blank that
/// reads like a deliberate omission.
void an_unavailable_field_without_a_reason_announces_itself()
{
    bench::result_row row;
    row.configuration.identity_provenance = complete_provenance();
    row.hart.retired_instructions = bench::measured<std::uint64_t>{};
    std::ostringstream text;
    bench::write_json(text, row);
    CHECK_MSG(text.str().find("which is a defect") != std::string::npos,
              "an unavailable field with no reason serialised as a blank "
              "instead of announcing that a reason was never recorded");
}

void every_row_carries_its_caveats_and_its_scope()
{
    std::ostringstream text;
    bench::write_json(text, sample_row());
    const std::string json = text.str();

    CHECK_MSG(json.find("\"chip_composition\": false") != std::string::npos,
              "the row does not record that no chip was composed");
    CHECK_MSG(json.find("\"noc_instantiated\": false") != std::string::npos,
              "the row does not record that no NoC was instantiated");
    CHECK_MSG(json.find("element-wise TLM accesses") != std::string::npos,
              "the D7 element-wise granularity caveat did not travel with the "
              "numbers");
    CHECK_MSG(json.find("\"derivation\":") != std::string::npos,
              "a derived quantity is present without the formula that "
              "produced it");
    CHECK_MSG(json.find("\"fault_flags\": 0") != std::string::npos,
              "the row does not state that no fault was injected, so a "
              "negative control could be mistaken for a measurement");
}

/// Every measurement §8 lists must appear in the row, present or explicitly
/// unavailable.
///
/// This test exists because two of them were missing and nobody noticed: §8.3
/// asks for busy, **service** and wait cycles and only two were emitted, and
/// §8.4 asks for arbitration wait cycles and **peak and average** outstanding
/// while the row carried neither the wait nor the average. An absent key is
/// the one failure mode the `measured<>` wrapper cannot catch, because there
/// is nothing there to wrap.
///
/// The list below is transcribed from the plan, not from the writer. A list
/// derived from the emitter would agree with whatever the emitter happens to
/// emit.
void every_field_section_8_asks_for_is_present()
{
    std::ostringstream text;
    bench::write_json(text, sample_row());
    const std::string json = text.str();

    const char* required[] = {
        // §8.1 identity and reproducibility
        "\"benchmark\"", "\"case_index\"", "\"shape\"", "\"datatype\"",
        "\"seed\"", "\"mode\"", "\"implementation\"",
        "\"firmware_elf_sha256\"", "\"build_type\"", "\"source_revision\"",
        "\"timing_mode\"", "\"id\"",
        // D28 identity: read back from live components, never literals
        "\"mxu_geometry\"", "\"mxu_datatype\"", "\"mxu_source_revision\"",
        "\"dma_controllers\"", "\"dma_channels\"", "\"vlen_bits\"",
        "\"identity_provenance\"", "\"run_valid\"", "\"failures\"",
        "\"conservation\"", "\"stage_timing\"",
        // §8.1 correctness
        "\"passed\"", "\"golden_checksum\"", "\"elements_compared\"",
        // §8.2 total and hart
        "\"interval_begin_ns\"", "\"interval_end_ns\"", "\"elapsed_ns\"",
        "\"retired_instructions\"", "\"scalar_instructions\"",
        "\"vector_instructions\"", "\"active_elements\"",
        "\"tail_elements\"", "\"lane_utilization\"",
        "\"local_requests\"", "\"local_bytes\"", "\"control_requests\"",
        "\"control_bytes\"", "\"external_requests\"",
        "\"external_bytes\"",
        // §8.3 DMA and external memory
        "\"bytes_in\"", "\"bytes_out\"", "\"bytes_total\"",
        "\"transfers\"", "\"chunks\"", "\"average_chunk_bytes\"",
        "\"busy_cycles\"", "\"service_cycles\"", "\"wait_cycles\"",
        "\"achieved_bytes_per_cycle\"", "\"overlap_with_compute\"",
        "\"external_memory_model\"",
        // §8.4 local SRAM fabric
        "\"requesters\"", "\"transferred_bytes\"", "\"errors\"",
        "\"bank_grants\"", "\"bank_conflicts\"",
        "\"arbitration_wait_cycles\"", "\"peak_outstanding\"",
        "\"average_outstanding\"", "\"percent_of_configured_peak\"",
        // §8.5 MXU
        "\"prefetch_cycles\"", "\"source_compute_cycles\"",
        "\"writeback_cycles\"", "\"array_utilization\"",
        "\"operand_bytes\"", "\"result_bytes\"", "\"native_requests\"",
        "\"useful_operations_per_cycle\"",
    };

    for (const char* key : required) {
        CHECK_MSG(json.find(key) != std::string::npos,
                  std::string("the row omits ") + key
                      + ", a measurement section 8 requires present or "
                        "explicitly unavailable");
    }
}

/// A row may not carry an identity field whose origin it does not state.
///
/// D28 forbids reproducing literals and calling them readback. The defence is
/// not to claim everything is live — some fields have no accessor yet — but to
/// label each one, and to keep the labels honest as fields are added.
void every_configuration_field_states_where_it_came_from()
{
    auto row = sample_row();
    CHECK_MSG(bench::validate_configuration_provenance(row.configuration).empty(),
              "the complete configuration provenance map was rejected");
    CHECK_MSG(row.configuration.identity_provenance.size() == 22,
              "the test's independently transcribed provenance set drifted");

    std::ostringstream text;
    bench::write_json(text, row);
    const std::string json = text.str();

    CHECK_MSG(json.find("\"identity_provenance\"") != std::string::npos,
              "the row does not state where its identity fields came from");
    CHECK_MSG(json.find("\"live_readback\"") != std::string::npos
                  && json.find("\"structural_literal\"") != std::string::npos,
              "the provenance labels did not reach the row");

    auto missing = row.configuration;
    missing.identity_provenance.erase(missing.identity_provenance.begin());
    CHECK_MSG(bench::validate_configuration_provenance(missing).find("missing")
                  != std::string::npos,
              "a missing provenance field was accepted");

    bool writer_rejected_missing = false;
    try {
        std::ostringstream rejected;
        bench::result_row incomplete = row;
        incomplete.configuration = missing;
        bench::write_json(rejected, incomplete);
    } catch (const std::invalid_argument&) {
        writer_rejected_missing = true;
    }
    CHECK_MSG(writer_rejected_missing,
              "write_json accepted an incomplete provenance map");

    auto duplicate = row.configuration;
    duplicate.identity_provenance.push_back(
        duplicate.identity_provenance.front());
    CHECK_MSG(bench::validate_configuration_provenance(duplicate).find("duplicate")
                  != std::string::npos,
              "a duplicate provenance field was accepted");

    auto unexpected = row.configuration;
    unexpected.identity_provenance.push_back(
        {"not_a_configuration_field", "configured"});
    CHECK_MSG(bench::validate_configuration_provenance(unexpected).find(
                  "unexpected")
                  != std::string::npos,
              "an unexpected provenance field was accepted");

    auto bad_label = row.configuration;
    bad_label.identity_provenance.front().second = "probably_live";
    CHECK_MSG(bench::validate_configuration_provenance(bad_label).find(
                  "unknown provenance label")
                  != std::string::npos,
              "an unknown provenance label was accepted");
}

/// A rejected run must say so in the row, not only in its exit code.
void a_rejected_run_is_visible_in_the_row()
{
    auto row = sample_row();
    row.run_valid = false;
    row.correctness.passed = false;
    row.failures.push_back({"harness", "the guest reports vlenb = 64"});
    std::ostringstream text;
    bench::write_json(text, row);
    const std::string json = text.str();

    CHECK_MSG(json.find("\"run_valid\": false") != std::string::npos,
              "a rejected run does not say so at the top level; an aggregation "
              "reads JSON, not exit codes");
    CHECK_MSG(json.find("\"failures\"") != std::string::npos
                  && json.find("vlenb = 64") != std::string::npos,
              "the row does not carry the checks that failed");

    auto clean = sample_row();
    clean.run_valid = true;
    std::ostringstream ok;
    bench::write_json(ok, clean);
    CHECK_MSG(ok.str().find("\"run_valid\": true") != std::string::npos,
              "a valid run is not marked valid");
}

/// G3's identities and stage spans must reach the row whether they balance or
/// not, and an unbalanced one must be visible without reading stderr.
void conservation_reaches_the_row_balanced_or_not()
{
    auto row = sample_row();
    row.conservation.push_back({"local_plane_bytes", "fabric bytes", 4096,
                                "core SRAM bytes", 4096, true,
                                "two observers of the same plane"});
    row.conservation.push_back({"external_boundary_bytes", "core bytes", 148,
                                "memory bytes", 156, false,
                                "one socket, three targets"});
    row.stage_timing.push_back({"the kernel", 100.0, 400.0, 300.0});
    std::ostringstream text;
    bench::write_json(text, row);
    const std::string json = text.str();

    CHECK_MSG(json.find("\"conservation\"") != std::string::npos,
              "the row does not carry its G3 identities");
    CHECK_MSG(json.find("\"balanced\": true") != std::string::npos
                  && json.find("\"balanced\": false") != std::string::npos,
              "a row does not distinguish a balanced identity from a broken "
              "one");
    CHECK_MSG(json.find("\"basis\"") != std::string::npos,
              "an identity reached the row without the reason its two sides "
              "must be equal; a reader cannot judge an accounting whose "
              "justification lives only in the code that computed it");
    CHECK_MSG(json.find("\"stage_timing\"") != std::string::npos
                  && json.find("\"duration_ns\"") != std::string::npos,
              "the row does not say where the measured interval went");
}

void the_two_modes_carry_different_caveats()
{
    const auto kernel
        = bench::standard_accuracy_notes(bench::bench_mode::kernel_only);
    const auto end_to_end
        = bench::standard_accuracy_notes(bench::bench_mode::end_to_end);
    CHECK_MSG(kernel != end_to_end,
              "both measurement modes carry the same caveats, but only one of "
              "them may be quoted about DMA");

    bool kernel_says_no_dma = false;
    for (const auto& note : kernel) {
        if (note.find("may not be quoted as a DMA") != std::string::npos) {
            kernel_says_no_dma = true;
        }
    }
    CHECK_MSG(kernel_says_no_dma,
              "a kernel-only row does not say it is not a DMA result");

    bool end_to_end_says_uncalibrated = false;
    for (const auto& note : end_to_end) {
        if (note.find("not calibrated") != std::string::npos) {
            end_to_end_says_uncalibrated = true;
        }
    }
    CHECK_MSG(end_to_end_says_uncalibrated,
              "an end-to-end row does not say the external memory model is "
              "uncalibrated");
}

/// The parser has to be able to fail, or the JSON check above proves nothing.
void the_parser_rejects_what_it_should()
{
    const char* bad[] = {
        "{\"a\": 1,}",        // trailing comma in an object
        "[1, 2,]",            // trailing comma in an array
        "{\"a\" 1}",          // missing colon
        "{\"a\": }",          // missing value
        "{\"a\": 1",          // unterminated object
        "\"unterminated",     // unterminated string
        "{\"a\": 1} garbage", // trailing content
    };
    for (const char* text : bad) {
        const std::string owned(text);
        json_parser parser(owned);
        CHECK_MSG(!parser.parse(),
                  std::string("the JSON parser accepted malformed input: ")
                      + text);
    }

    const std::string good = "{\"a\": [1, -2.5e3, true, null, {\"b\": \"c\"}]}";
    json_parser parser(good);
    CHECK_MSG(parser.parse(),
              "the JSON parser rejected well-formed input: " + parser.error());
}

} // namespace

int main()
{
    the_parser_rejects_what_it_should();
    the_emitted_row_is_parseable_json();
    an_unavailable_measurement_carries_its_reason();
    an_unavailable_field_without_a_reason_announces_itself();
    every_row_carries_its_caveats_and_its_scope();
    every_field_section_8_asks_for_is_present();
    every_configuration_field_states_where_it_came_from();
    a_rejected_run_is_visible_in_the_row();
    conservation_reaches_the_row_balanced_or_not();
    the_two_modes_carry_different_caveats();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_result_row: all checks passed\n";
    return 0;
}
