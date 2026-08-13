// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3_soc_top.h"

#include <array>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "tpu_v3/address_map.h"

namespace cdc::platforms::tpu_v3_soc {

namespace tpu = cdc::components::tpu_v3;
namespace am = cdc::components::tpu_v3::address_map;

namespace {

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

} // namespace

tpu_v3_soc_top::tpu_v3_soc_top(sc_core::sc_module_name name,
                               tpu::tpu_soc_config config)
    : sc_core::sc_module(name)
    , config_((config.validate(), std::move(config)))
    , global_ram_(config_.global_ram_size_bytes)
{
    // One shared SRAM per NEO-CORE (decision record D14). Named by chip and
    // core so a report, a counter and a hierarchy path all agree about which
    // one they mean.
    core_srams_.reserve(std::size_t{config_.chips} * tpu::cores_per_chip);
    for (tpu::chip_id_t chip = 0; chip < config_.chips; ++chip) {
        for (tpu::core_id_t core = 0; core < tpu::cores_per_chip; ++core) {
            tpu::sram::core_sram_config sram_config;
            sram_config.base_address = am::core_sram_base(chip, core);
            sram_config.window_bytes = am::core_sram_window;
            sram_config.capacity_bytes = config_.chip.core[core].sram_size_bytes;

            const std::string instance = "chip" + std::to_string(chip) + "_core"
                + std::to_string(core) + "_sram";
            core_srams_.push_back(
                std::make_unique<tpu::sram::core_sram>(instance.c_str(),
                                                       sram_config));
        }
    }
}

std::uint64_t tpu_v3_soc_top::logical_memory_bytes() const noexcept
{
    std::uint64_t total = global_ram_.capacity();
    for (const auto& sram : core_srams_) {
        total += sram->capacity_bytes();
    }
    return total;
}

std::uint64_t tpu_v3_soc_top::allocated_backing_bytes() const noexcept
{
    std::uint64_t total = global_ram_.allocated_bytes();
    for (const auto& sram : core_srams_) {
        total += sram->allocated_backing_bytes();
    }
    return total;
}

std::size_t tpu_v3_soc_top::allocated_page_count() const noexcept
{
    std::size_t total = global_ram_.allocated_pages();
    for (const auto& sram : core_srams_) {
        total += sram->allocated_page_count();
    }
    return total;
}

tpu::sram::core_sram& tpu_v3_soc_top::core_sram(tpu::chip_id_t chip,
                                                tpu::core_id_t core)
{
    if (chip >= config_.chips || core >= tpu::cores_per_chip) {
        throw std::out_of_range("tpu_v3_soc_top: chip " + std::to_string(chip)
                                + " core " + std::to_string(core)
                                + " is not instantiated");
    }
    return *core_srams_[std::size_t{chip} * tpu::cores_per_chip + core];
}

std::string tpu_v3_soc_top::report() const
{
    std::ostringstream out;

    out << tpu::describe(config_);

    out << "\nMemory\n"
        << "  core SRAM            : " << core_srams_.size() << " x "
        << human_bytes(config_.chip.core[0].sram_size_bytes) << " backed, "
        << human_bytes(am::core_sram_window) << " window each\n"
        << "  global RAM           : " << human_bytes(global_ram_.capacity())
        << " backed, " << human_bytes(am::global_ram_window) << " window\n"
        << "  logical total        : " << human_bytes(logical_memory_bytes())
        << '\n'
        << "  host backing in use  : " << human_bytes(allocated_backing_bytes())
        << " across " << allocated_page_count() << " pages of "
        << human_bytes(tpu::sparse_memory::page_size) << '\n'
        << "  backing policy       : deterministic sparse pages. An "
           "unallocated page reads as zero\n"
           "                         and costs nothing; the first write "
           "commits only the touched\n"
           "                         pages (decision record D6). Logical "
           "memory is address space,\n"
           "                         not a host allocation.\n";

    out << "\nInstantiated in this build\n"
        << "  configuration model  : yes\n"
        << "  address map          : yes (validated, non-overlapping)\n"
        << "  SystemC elaboration  : yes\n"
        << "  core SRAM            : yes  (sparsely page-backed)\n"
        << "  global RAM store     : yes  (storage only; not a TLM target "
           "until Phase 9)\n"
        << "  control fabric       : no   (built and tested as a component; "
           "composed in Phase 7)\n"
        << "  local SRAM fabric    : no   (built and tested as a component; "
           "composed in Phase 7)\n"
        << "  external bridge      : no   (built and tested as a component; "
           "composed in Phase 7)\n"
        << "  RV32GCV hart         : no   (backend built and tested in "
           "Phase 2; instantiated in Phase 7)\n"
        << "  NEO DMA              : no   (Phase 4)\n"
        << "  Sauria matrix engine : no   (Phase 5)\n"
        << "  ImageTransform       : no   (Phase 6)\n"
        << "  NoC                  : no   (Phase 9)\n"
        << '\n'
        << "This is the Phase 3 platform. It owns the memories and the "
           "validated map;\n"
        << "the fabrics exist as components and are composed into a NEO-CORE "
           "in Phase 7.\n";

    return out.str();
}

void tpu_v3_soc_top::start_of_simulation()
{
    started_ = true;
}

void tpu_v3_soc_top::end_of_simulation()
{
    finished_ = true;
}

} // namespace cdc::platforms::tpu_v3_soc
