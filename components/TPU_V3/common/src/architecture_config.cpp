// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/architecture_config.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace cdc::components::tpu_v3 {

namespace {

[[noreturn]] void reject(const std::string& context, const std::string& field,
                         const std::string& value, const std::string& expected)
{
    std::ostringstream message;
    message << "tpu_v3 configuration: " << context << '.' << field << " = "
            << value << " is not accepted; " << expected;
    throw std::invalid_argument(message.str());
}

void require_equal(const std::string& context, const std::string& field,
                   unsigned actual, unsigned expected, const char* why)
{
    if (actual == expected) {
        return;
    }
    reject(context, field, std::to_string(actual),
           "it is frozen at " + std::to_string(expected) + " (" + why + ')');
}

void require_power_of_two(const std::string& context, const std::string& field,
                          std::uint64_t value)
{
    if (value != 0 && (value & (value - 1)) == 0) {
        return;
    }
    reject(context, field, std::to_string(value), "it must be a power of two");
}

void require_range(const std::string& context, const std::string& field,
                   std::uint64_t value, std::uint64_t low, std::uint64_t high)
{
    if (value >= low && value <= high) {
        return;
    }
    reject(context, field, std::to_string(value),
           "the accepted range is [" + std::to_string(low) + ", "
               + std::to_string(high) + ']');
}

/// A physical value D15 leaves open must be *stated*, and zero is how the
/// schema spells "not stated". Saying so explicitly is worth a separate
/// message: "0 is outside [8, 1024]" sends the reader looking for a range
/// mistake instead of at the missing configuration key.
void require_stated(const std::string& context, const std::string& field,
                    unsigned value, const char* key)
{
    if (value != 0) {
        return;
    }
    reject(context, field, "0",
           std::string("decision record D15 leaves this physical value open "
                       "pending the SRAM macro, clock target and PD "
                       "constraints, so it has no architectural default. "
                       "State it explicitly (configuration key '")
               + key + "')");
}

std::string human_bytes(std::uint64_t bytes)
{
    static constexpr std::array<const char*, 5> units{"B", "KiB", "MiB", "GiB",
                                                      "TiB"};
    std::size_t unit = 0;
    std::uint64_t scaled = bytes;
    while (scaled >= 1024 && (scaled % 1024) == 0 && unit + 1 < units.size()) {
        scaled /= 1024;
        ++unit;
    }
    return std::to_string(scaled) + ' ' + units[unit];
}

std::string hex(std::uint64_t value, int width = 8)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(width) << std::setfill('0') << value;
    return out.str();
}

} // namespace

// ── validation ───────────────────────────────────────────────────────────────

void rvv_config::validate(const std::string& context) const
{
    require_equal(context, "rvv.xlen", xlen, 32, "plan §4.2, RV32");
    require_equal(context, "rvv.vlen", vlen, 512, "plan §4.2, VLEN=512");
    require_equal(context, "rvv.elen", elen, 64, "plan §4.2, ELEN=64");
    if (version != "1.0") {
        reject(context, "rvv.version", version,
               "the frozen vector specification is RISC-V V 1.0");
    }
    // vlenb is derived, so it cannot disagree — but firmware reads it as a CSR
    // and the plan states the expected value, so assert the derivation here
    // rather than discovering a units mistake in a firmware test.
    if (vlenb() != 64) {
        reject(context, "rvv.vlenb", std::to_string(vlenb()),
               "VLEN=512 must yield vlenb=64");
    }
}

std::string sauria_matrix_config::geometry() const
{
    return std::to_string(rows) + 'x' + std::to_string(columns);
}

void sauria_matrix_config::validate(const std::string& context) const
{
    require_equal(context, "sa.count_per_core", count_per_core, sa_per_core,
                  "decision record D14, one matrix engine per NEO-CORE");

    const bool bringup =
        rows == sa_bringup_rows && columns == sa_bringup_columns;
    if (!bringup && !is_target_geometry()) {
        reject(context, "sa.geometry", geometry(),
               "the only named geometries are the verified 64x64 bring-up "
               "array and the 128x128 promotion target (decision record D14). "
               "An arbitrary geometry has no source and no golden tests");
    }
    if (is_target_geometry()) {
        reject(context, "sa.geometry", geometry(),
               "the 128x128 engine is an NPU-team delivery that has not "
               "arrived, so its promotion gate cannot have passed (decision "
               "record D14). Use 64x64; no build, manifest or report may call "
               "the bring-up array 128x128");
    }

    // No datatype is refused here any more, and the reason is worth recording.
    //
    // This used to reject `int8_int32` as "not implemented". That was true and
    // right until Phase 5, whose whole deliverable is the verified v4.2
    // `int8_64x64` profile — INT8 operands, INT32 accumulation — which D6
    // explicitly permits as an opt-in quantized extension. A schema that
    // refuses the only arithmetic the source actually provides would make the
    // phase unconfigurable.
    //
    // What D6 forbids is not selecting INT8. It is INT8 *replacing* the
    // BF16/FP32 reference path, and letting a bring-up run be read as BF16
    // equivalence. That is a reporting property, not a validation one, so it
    // is enforced where it can actually be broken: `describe()` names the
    // arithmetic each datatype really performs and says outright when it is
    // not the reference path.
    //
    // The remaining check belongs to the engine, not the schema: the pinned
    // v4.2 source has **no BF16 profile in any geometry**, so an instantiated
    // Sauria engine must refuse a datatype its source cannot provide. That
    // refusal lives in the Phase 5 configuration gate, because it depends on
    // which source is pinned, while this validator must keep working for the
    // platform configurations that instantiate no engine at all.
}

void dma_config::validate(const std::string& context) const
{
    require_equal(context, "dma.count_per_core", count_per_core, dma_per_core,
                  "decision record D14, one independent DMA per NEO-CORE");
    require_range(context, "dma.max_burst_bytes", max_burst_bytes, 8, 2048);
    require_power_of_two(context, "dma.max_burst_bytes", max_burst_bytes);
}

void image_transform_config::validate(const std::string& context) const
{
    require_equal(context, "transform.count_per_core", count_per_core,
                  transform_per_core,
                  "decision record D14, one ImageTransform engine per "
                  "NEO-CORE");

    // D18 exposes only what Phase 6 can trace. Col2Im has no source or overlap
    // contract, so even a non-empty free-form revision must not enable it.
    if (col2im_available) {
        reject(context, "transform.col2im_available", "true",
               "Phase 6 found no Col2Im implementation or semantic contract; "
               "it must remain unavailable until a separately reviewed "
               "NPU-team delivery is promoted");
    }
    if (im2col_available && source_revision != im2col_source_revision) {
        reject(context, "transform.source_revision",
               source_revision.empty() ? "(empty)" : source_revision,
               std::string("available Im2Col is pinned to '")
                   + im2col_source_revision
                   + "'; audit and rebaseline before selecting another "
                     "source");
    }
}

void local_sram_fabric_config::validate(const std::string& context) const
{
    require_stated(context, "local_sram_fabric.data_width_bits",
                   data_width_bits, "local_sram_data_width_bits");
    require_stated(context, "local_sram_fabric.bank_count", bank_count,
                   "local_sram_banks");
    require_stated(context, "local_sram_fabric.pipeline_stages",
                   pipeline_stages, "local_sram_pipeline_stages");

    if (data_width_bits % 8 != 0) {
        reject(context, "local_sram_fabric.data_width_bits",
               std::to_string(data_width_bits),
               "a bank access is a whole number of bytes, so the width must "
               "be a multiple of 8");
    }
    require_range(context, "local_sram_fabric.data_width_bits",
                  data_width_bits, 8, 1024);
    require_power_of_two(context, "local_sram_fabric.data_width_bits",
                         data_width_bits);

    require_range(context, "local_sram_fabric.bank_count", bank_count, 1, 64);
    require_power_of_two(context, "local_sram_fabric.bank_count", bank_count);

    // A deep pipeline is legal; an unbounded one is a typo. Eight stages is
    // already far past anything a local SRAM path would carry.
    require_range(context, "local_sram_fabric.pipeline_stages",
                  pipeline_stages, 1, 8);

    if (mapping != bank_mapping::low_order_interleaved) {
        reject(context, "local_sram_fabric.mapping", to_string(mapping),
               "the only implemented mapping is 'low_order_interleaved' "
               "(decision record D15)");
    }
    if (arbitration != arbitration_policy::round_robin) {
        reject(context, "local_sram_fabric.arbitration", to_string(arbitration),
               "arbitration must be deterministic round-robin (decision "
               "record D15); an arbiter whose outcome depends on host "
               "scheduling makes every contention measurement "
               "unreproducible");
    }
    if (max_outstanding_per_requester != 1) {
        reject(context, "local_sram_fabric.max_outstanding_per_requester",
               std::to_string(max_outstanding_per_requester),
               "Revision 1 is strictly in order with one request in flight "
               "per requester (decision record D15). More than one needs a "
               "reordering and response-ownership contract that does not "
               "exist yet");
    }
}

local_sram_fabric_config provisional_local_sram_fabric() noexcept
{
    local_sram_fabric_config fabric;
    // Placeholders, not a decision. See the declaration and D15: the real
    // values come from the SRAM macro, the clock target and PD. They are
    // chosen wide enough to make bank conflicts observable in tests and
    // narrow enough that a 64-byte vector-width access still spans several
    // beats, which is the behaviour Phase 3 has to be able to measure.
    fabric.data_width_bits = 128;
    fabric.bank_count = 4;
    fabric.mapping = bank_mapping::low_order_interleaved;
    fabric.pipeline_stages = 2;
    fabric.max_outstanding_per_requester = 1;
    fabric.arbitration = arbitration_policy::round_robin;
    return fabric;
}

void tpu_core_config::validate(const std::string& context) const
{
    rvv.validate(context);
    sa.validate(context);
    dma.validate(context);
    transform.validate(context);
    local_sram_fabric.validate(context);

    require_range(context, "sram_size_bytes", sram_size_bytes,
                  address_map::core_sram_min_capacity,
                  address_map::core_sram_max_capacity);
    require_power_of_two(context, "sram_size_bytes", sram_size_bytes);

    // One stripe is the granularity at which the logical address space wraps
    // across banks. A capacity smaller than that would leave banks that no
    // address ever selects, so the arbitration the fabric reports would be
    // measured on a structure the configuration does not actually describe.
    if (sram_size_bytes < local_sram_fabric.stripe_bytes()) {
        reject(context, "sram_size_bytes", std::to_string(sram_size_bytes),
               "it must cover at least one full bank stripe ("
                   + std::to_string(local_sram_fabric.bank_count) + " banks x "
                   + std::to_string(local_sram_fabric.bytes_per_beat())
                   + " bytes = "
                   + std::to_string(local_sram_fabric.stripe_bytes())
                   + " bytes); a smaller capacity leaves banks that no address "
                     "selects");
    }
}

void tpu_chip_config::validate(const std::string& context) const
{
    require_equal(context, "cores", cores, cores_per_chip,
                  "plan §4.1, exactly two NEO-COREs per chip");
    for (unsigned index = 0; index < cores_per_chip; ++index) {
        core[index].validate(context + ".core" + std::to_string(index));
    }
}

void tpu_soc_config::validate() const
{
    const std::string context = "soc[" + name + ']';

    require_range(context, "chips", chips, 1, max_chips);

    if (!mesh_size_supported(mesh_x, mesh_y)) {
        reject(context, "mesh",
               std::to_string(mesh_x) + 'x' + std::to_string(mesh_y),
               "noc_interconnect instantiates only 2x2, 3x3, 4x4, 4x2 and 2x4. "
               "Adding a size is a one-line change to make_noc() in "
               "components/floo_noc_model/src/noc_interconnect.cpp");
    }

    // The global targets need a node of their own: noc_interconnect refuses a
    // target on a node that hosts an upstream port, and every chip hosts one.
    // Catching it here names the actual constraint; letting the NoC catch it
    // produces a runtime_error from inside end_of_elaboration.
    if (chips + 1 > mesh_nodes()) {
        reject(context, "chips", std::to_string(chips),
               "a " + std::to_string(mesh_x) + 'x' + std::to_string(mesh_y)
                   + " mesh has " + std::to_string(mesh_nodes())
                   + " nodes and one of them must host the global targets, so "
                     "at most "
                   + std::to_string(mesh_nodes() - 1)
                   + " chips fit. The NoC refuses a target on a node that "
                     "hosts an initiator (NoLoopback)");
    }

    chip.validate(context + ".chip");

    require_range(context, "global_ram_size_bytes", global_ram_size_bytes,
                  address_map::global_ram_min_capacity,
                  address_map::global_ram_max_capacity);
    require_power_of_two(context, "global_ram_size_bytes",
                         global_ram_size_bytes);

    // Enumerating the map is the real non-overlap proof; doing it here means a
    // configuration cannot be accepted and then produce a map that collides.
    const auto regions = address_map::enumerate_regions(
        chips, chip.core[0].sram_size_bytes, global_ram_size_bytes);
    for (std::size_t i = 0; i < regions.size(); ++i) {
        for (std::size_t j = i + 1; j < regions.size(); ++j) {
            const auto& a = regions[i];
            const auto& b = regions[j];
            if (a.base < b.base + b.size && b.base < a.base + a.size) {
                std::ostringstream message;
                message << "tpu_v3 configuration: " << context
                        << " produces overlapping regions '" << a.name
                        << "' at " << hex(a.base) << " size " << hex(a.size)
                        << " and '" << b.name << "' at " << hex(b.base)
                        << " size " << hex(b.size);
                throw std::invalid_argument(message.str());
            }
        }
    }

    // Both cores in a chip share one SRAM capacity in this revision.
    // Enumerating used core 0's value, so a differing core 1 would silently
    // not be mapped.
    if (chip.core[0].sram_size_bytes != chip.core[1].sram_size_bytes) {
        reject(context, "chip.core1.sram_size_bytes",
               std::to_string(chip.core[1].sram_size_bytes),
               "both cores in a chip must currently use the same core SRAM "
               "capacity (core0 = "
                   + std::to_string(chip.core[0].sram_size_bytes) + ')');
    }

    // The same argument, for everything `describe()` reports from core 0.
    //
    // `describe()` prints `chip.core[0]`'s matrix geometry, datatype and source
    // revision and calls them the configuration's. Nothing stopped a caller
    // giving core 1 different ones, so a two-core chip could run half its
    // matrix work on an arithmetic the report never mentioned — the exact
    // mislabelling D6 and D14 forbid, arrived at from the other direction.
    //
    // Refusing divergence is the right fix rather than reporting both, because
    // there is no configuration this project wants in which the two SAs of one
    // chip disagree about their numeric contract or their source. If that ever
    // changes, the report has to change with it, and this check is what will
    // force the conversation.
    if (chip.core[0].sa.datatype != chip.core[1].sa.datatype) {
        reject(context, "chip.core1.sa.datatype",
               to_string(chip.core[1].sa.datatype),
               std::string("both cores in a chip must use the same matrix "
                           "arithmetic (core0 = ")
                   + to_string(chip.core[0].sa.datatype)
                   + "). Reports name core 0's datatype for the whole "
                     "configuration, so a differing core 1 would be invisible");
    }
    if (chip.core[0].sa.source_revision != chip.core[1].sa.source_revision) {
        reject(context, "chip.core1.sa.source_revision",
               chip.core[1].sa.source_revision.empty()
                   ? std::string("(empty)")
                   : chip.core[1].sa.source_revision,
               "both cores in a chip must be extracted from the same Sauria "
               "source revision (core0 = "
                   + (chip.core[0].sa.source_revision.empty()
                          ? std::string("(empty)")
                          : chip.core[0].sa.source_revision)
                   + "). Reports and the build manifest name core 0's");
    }
    if (chip.core[0].sa.rows != chip.core[1].sa.rows
        || chip.core[0].sa.columns != chip.core[1].sa.columns) {
        reject(context, "chip.core1.sa.geometry", chip.core[1].sa.geometry(),
               "both cores in a chip must use the same matrix geometry (core0 = "
                   + chip.core[0].sa.geometry()
                   + "). Reports name core 0's geometry, and decision record "
                     "D14 forbids a build or report calling one array by "
                     "another's name");
    }
}

bool mesh_size_supported(unsigned mesh_x, unsigned mesh_y) noexcept
{
    // Mirrors make_noc() in components/floo_noc_model/src/noc_interconnect.cpp.
    // It is a copy, and the reason it is tolerable is that it fails *closed*:
    // a size supported there but missing here is refused with a message
    // pointing at that file, while a size accepted here but missing there
    // would still be refused by the NoC constructor.
    static constexpr std::array<std::pair<unsigned, unsigned>, 5> supported{{
        {2, 2}, {3, 3}, {4, 4}, {4, 2}, {2, 4},
    }};
    return std::any_of(supported.begin(), supported.end(),
                       [&](const auto& entry) {
                           return entry.first == mesh_x && entry.second == mesh_y;
                       });
}

// ── description ──────────────────────────────────────────────────────────────

std::string describe(const tpu_soc_config& config)
{
    const auto& core0 = config.chip.core[0];
    const auto& fabric = core0.local_sram_fabric;
    std::ostringstream out;

    out << "TPU_V3 SoC configuration '" << config.name << "'\n"
        << "  mesh                 : " << config.mesh_x << 'x' << config.mesh_y
        << "  (" << config.mesh_nodes() << " nodes)\n"
        << "  NoC timing backend   : " << to_string(config.timing) << '\n'
        << "  chips                : " << config.chips << " of "
        << max_chips << " (frozen NoC manager-id limit)\n"
        << "  NEO-COREs            : " << config.harts() << "  ("
        << cores_per_chip << " per chip)\n"
        << "  RV32GCV harts        : " << config.harts() << '\n'
        << "  matrix engines       : " << config.matrix_engines() << "  ("
        << sa_per_core << " per core, " << core0.sa.geometry()
        << " bring-up; 128x128 is the promotion target)\n"
        << "  matrix datatype      : " << to_string(core0.sa.datatype)
        << "  (" << matrix_datatype_note(core0.sa.datatype) << ")\n"
        << "  matrix source        : "
        << (core0.sa.source_revision.empty()
                ? std::string("not integrated (Phase 5)")
                : core0.sa.source_revision)
        << '\n'
        << "  DMA engines          : " << config.dma_engines()
        << "  (independent of Sauria; max burst "
        << human_bytes(core0.dma.max_burst_bytes) << ")\n"
        << "  transform engines    : " << config.transform_engines()
        << "  (im2col "
        << (core0.transform.im2col_available ? "available" : "unavailable")
        << ", col2im "
        << (core0.transform.col2im_available ? "available" : "unavailable")
        << ")\n"
        << "  RVV                  : v" << core0.rvv.version << ", XLEN="
        << core0.rvv.xlen << ", VLEN=" << core0.rvv.vlen
        << ", ELEN=" << core0.rvv.elen << ", vlenb=" << core0.rvv.vlenb()
        << '\n'
        << "  core SRAM per core   : " << human_bytes(core0.sram_size_bytes)
        << "  (16 MiB window"
        << (core0.sram_size_bytes < address_map::core_sram_window
                ? "; below the reference capacity — bring-up configuration"
                : "; reference capacity")
        << ")\n"
        << "  local SRAM fabric    : " << fabric.data_width_bits << "-bit x "
        << fabric.bank_count << " banks, " << to_string(fabric.mapping) << ", "
        << fabric.pipeline_stages << " pipeline stage"
        << (fabric.pipeline_stages == 1 ? "" : "s") << ", "
        << to_string(fabric.arbitration) << ", "
        << fabric.max_outstanding_per_requester << " outstanding/requester\n"
        << "                         provisional physical values pending SRAM "
           "macro, frequency and PD (D15)\n"
        << "  global RAM           : "
        << human_bytes(config.global_ram_size_bytes) << "  (1 GiB window)\n"
        << "  logical memory       : "
        << human_bytes(config.logical_memory_bytes())
        << "  (sparsely page-backed; not a host allocation — D6)\n";

    out << "\nHierarchy\n";
    for (chip_id_t chip = 0; chip < config.chips; ++chip) {
        const auto node = config.chip_node(chip);
        out << "  chip" << chip << " @ node (" << node.x << ',' << node.y
            << ")  aperture " << hex(address_map::chip_base(chip)) << "..."
            << hex(address_map::chip_base(chip)
                   + address_map::chip_aperture_stride - 1)
            << '\n';
        for (core_id_t core = 0; core < cores_per_chip; ++core) {
            out << "    core" << core << "  hart " << hart_id_of(chip, core)
                << "   sram " << hex(address_map::core_sram_base(chip, core))
                << "   sa " << hex(address_map::sa_control(chip, core))
                << "   dma " << hex(address_map::dma_control(chip, core))
                << "   transform "
                << hex(address_map::transform_control(chip, core)) << '\n';
        }
    }
    const auto global = config.global_node();
    out << "  global targets @ node (" << global.x << ',' << global.y << ")\n";

    return out.str();
}

std::string describe_address_map(const tpu_soc_config& config)
{
    const auto regions = address_map::enumerate_regions(
        config.chips, config.chip.core[0].sram_size_bytes,
        config.global_ram_size_bytes);

    std::ostringstream out;
    out << "TPU_V3 address map (" << regions.size() << " regions)\n"
        << "  base        end         window      kind    name\n";
    for (const auto& entry : regions) {
        out << "  " << hex(entry.base) << "  "
            << hex(entry.base + entry.size - 1) << "  " << hex(entry.size)
            << "  "
            << (entry.kind == address_map::region_kind::memory ? "memory"
                                                               : "mmio  ")
            << "  " << entry.name;
        // Only annotate when the window is larger than the storage. Printing
        // "backed 0x00010000" on every register file would bury the one case
        // that matters.
        if (entry.is_partially_backed()) {
            out << "  [backed " << human_bytes(entry.capacity) << " of "
                << human_bytes(entry.size)
                << "; above that the target reports an error, never an alias]";
        }
        out << '\n';
    }
    return out.str();
}

// ── enum plumbing ────────────────────────────────────────────────────────────

const char* to_string(matrix_datatype datatype) noexcept
{
    switch (datatype) {
    case matrix_datatype::bf16_fp32:
        return "bf16_fp32";
    case matrix_datatype::fp16_fp32:
        return "fp16_fp32";
    case matrix_datatype::int8_int32:
        return "int8_int32";
    }
    return "unknown";
}

const char* to_string(bank_mapping mapping) noexcept
{
    switch (mapping) {
    case bank_mapping::low_order_interleaved:
        return "low_order_interleaved";
    }
    return "unknown";
}

const char* to_string(arbitration_policy policy) noexcept
{
    switch (policy) {
    case arbitration_policy::round_robin:
        return "round_robin";
    }
    return "unknown";
}

const char* to_string(noc_timing timing) noexcept
{
    switch (timing) {
    case noc_timing::fast:
        return "fast";
    case noc_timing::detailed:
        return "detailed";
    }
    return "unknown";
}

const char* matrix_datatype_note(matrix_datatype datatype) noexcept
{
    switch (datatype) {
    case matrix_datatype::bf16_fp32:
        return "BF16 operands, IEEE FP32 accumulation - the TPU_V3 reference "
               "path (decision record D6)";
    case matrix_datatype::fp16_fp32:
        return "FP16 operands, FP32 accumulation - a v4.2 bring-up datatype, "
               "NOT the BF16 reference path and not evidence of BF16 "
               "equivalence";
    case matrix_datatype::int8_int32:
        return "INT8 operands, INT32 accumulation - D6's opt-in quantized "
               "extension, NOT the BF16 reference path";
    }
    return "unknown arithmetic";
}

matrix_datatype matrix_datatype_from_string(const std::string& text)
{
    if (text == "bf16_fp32") {
        return matrix_datatype::bf16_fp32;
    }
    if (text == "fp16_fp32") {
        return matrix_datatype::fp16_fp32;
    }
    if (text == "int8_int32") {
        return matrix_datatype::int8_int32;
    }
    throw std::invalid_argument(
        "tpu_v3: unknown matrix datatype '" + text
        + "'; accepted values are 'bf16_fp32', 'fp16_fp32' and 'int8_int32'");
}

bank_mapping bank_mapping_from_string(const std::string& text)
{
    if (text == "low_order_interleaved") {
        return bank_mapping::low_order_interleaved;
    }
    throw std::invalid_argument(
        "tpu_v3: unknown bank mapping '" + text
        + "'; the only implemented mapping is 'low_order_interleaved'");
}

arbitration_policy arbitration_policy_from_string(const std::string& text)
{
    if (text == "round_robin") {
        return arbitration_policy::round_robin;
    }
    throw std::invalid_argument(
        "tpu_v3: unknown arbitration policy '" + text
        + "'; the only implemented policy is 'round_robin'");
}

noc_timing noc_timing_from_string(const std::string& text)
{
    if (text == "fast") {
        return noc_timing::fast;
    }
    if (text == "detailed") {
        return noc_timing::detailed;
    }
    throw std::invalid_argument(
        "tpu_v3: unknown NoC timing mode '" + text
        + "'; accepted values are 'fast' and 'detailed'");
}

} // namespace cdc::components::tpu_v3
