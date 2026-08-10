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

void mxu_config::validate(const std::string& context) const
{
    require_equal(context, "mxu.rows", rows, mxu_rows, "plan §4.3, 128x128");
    require_equal(context, "mxu.columns", columns, mxu_columns,
                  "plan §4.3, 128x128");
    require_equal(context, "mxu.count_per_core", count_per_core, mxus_per_core,
                  "plan §4.1, two MXUs per core");

    if (backend == mxu_backend::sauria) {
        reject(context, "mxu.backend", "sauria",
               "the Sauria-derived detailed backend is not implemented yet "
               "(plan Phase 9, decision record D6). The available Sauria "
               "instance is a 32x32 FP16/INT16 NPU top, not a 128x128 MXU — "
               "see docs/TPU_V3_PHASE0_AUDIT.md §7. Use 'fast'");
    }

    if (arithmetic == mxu_arithmetic::int8_int32) {
        reject(context, "mxu.arithmetic", "int8_int32",
               "the quantized extension is not implemented (Phase 4+). The "
               "TPU_V3 reference arithmetic is BF16 operands with IEEE FP32 "
               "accumulation (decision record D6). Use 'bf16_fp32'");
    }
}

void tpu_core_config::validate(const std::string& context) const
{
    rvv.validate(context);
    mxu.validate(context);
    require_range(context, "svm_size_bytes", svm_size_bytes,
                  address_map::svm_min_capacity, address_map::svm_max_capacity);
    require_power_of_two(context, "svm_size_bytes", svm_size_bytes);
}

void tpu_chip_config::validate(const std::string& context) const
{
    require_equal(context, "cores", cores, cores_per_chip,
                  "plan §4.1, exactly two cores per chip");
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
        chips, chip.core[0].svm_size_bytes, global_ram_size_bytes);
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

    // Both cores in a chip share one SVM capacity in this revision. Enumerating
    // used core 0's value, so a differing core 1 would silently not be mapped.
    if (chip.core[0].svm_size_bytes != chip.core[1].svm_size_bytes) {
        reject(context, "chip.core1.svm_size_bytes",
               std::to_string(chip.core[1].svm_size_bytes),
               "both cores in a chip must currently use the same SVM capacity "
               "(core0 = "
                   + std::to_string(chip.core[0].svm_size_bytes) + ')');
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
    std::ostringstream out;

    out << "TPU_V3 SoC configuration '" << config.name << "'\n"
        << "  mesh                 : " << config.mesh_x << 'x' << config.mesh_y
        << "  (" << config.mesh_nodes() << " nodes)\n"
        << "  NoC timing backend   : " << to_string(config.timing) << '\n'
        << "  chips                : " << config.chips << " of "
        << max_chips << " (frozen NoC manager-id limit)\n"
        << "  TPU cores            : " << config.harts() << "  ("
        << cores_per_chip << " per chip)\n"
        << "  RV32GCV harts        : " << config.harts() << '\n'
        << "  MXUs                 : " << config.mxus() << "  ("
        << mxus_per_core << " per core, " << core0.mxu.rows << 'x'
        << core0.mxu.columns << ")\n"
        << "  MXU backend          : " << to_string(core0.mxu.backend) << '\n'
        << "  MXU arithmetic       : " << to_string(core0.mxu.arithmetic)
        << "  (BF16 operands, IEEE FP32 accumulation)\n"
        << "  RVV                  : v" << core0.rvv.version << ", XLEN="
        << core0.rvv.xlen << ", VLEN=" << core0.rvv.vlen
        << ", ELEN=" << core0.rvv.elen << ", vlenb=" << core0.rvv.vlenb()
        << '\n'
        << "  SVM per core         : " << human_bytes(core0.svm_size_bytes)
        << "  (16 MiB window"
        << (core0.svm_size_bytes < address_map::svm_window
                ? "; below the reference capacity — bring-up configuration"
                : "; reference capacity")
        << ")\n"
        << "  global RAM           : "
        << human_bytes(config.global_ram_size_bytes) << "  (1 GiB window)\n";

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
                << "   svm " << hex(address_map::svm_base(chip, core))
                << "   mxu0 " << hex(address_map::mxu_control(chip, core, 0))
                << "   mxu1 " << hex(address_map::mxu_control(chip, core, 1))
                << '\n';
        }
    }
    const auto global = config.global_node();
    out << "  global targets @ node (" << global.x << ',' << global.y << ")\n";

    return out.str();
}

std::string describe_address_map(const tpu_soc_config& config)
{
    const auto regions = address_map::enumerate_regions(
        config.chips, config.chip.core[0].svm_size_bytes,
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

const char* to_string(mxu_backend backend) noexcept
{
    switch (backend) {
    case mxu_backend::fast:
        return "fast";
    case mxu_backend::sauria:
        return "sauria";
    }
    return "unknown";
}

const char* to_string(mxu_arithmetic arithmetic) noexcept
{
    switch (arithmetic) {
    case mxu_arithmetic::bf16_fp32:
        return "bf16_fp32";
    case mxu_arithmetic::int8_int32:
        return "int8_int32";
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

mxu_backend mxu_backend_from_string(const std::string& text)
{
    if (text == "fast") {
        return mxu_backend::fast;
    }
    if (text == "sauria") {
        return mxu_backend::sauria;
    }
    throw std::invalid_argument("tpu_v3: unknown MXU backend '" + text
                                + "'; accepted values are 'fast' and 'sauria'");
}

mxu_arithmetic mxu_arithmetic_from_string(const std::string& text)
{
    if (text == "bf16_fp32") {
        return mxu_arithmetic::bf16_fp32;
    }
    if (text == "int8_int32") {
        return mxu_arithmetic::int8_int32;
    }
    throw std::invalid_argument(
        "tpu_v3: unknown MXU arithmetic '" + text
        + "'; accepted values are 'bf16_fp32' and 'int8_int32'");
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
