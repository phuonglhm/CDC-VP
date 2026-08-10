// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/address_map.h"

#include <sstream>
#include <stdexcept>
#include <string>

namespace cdc::components::tpu_v3::address_map {

namespace {

void require_range(const char* what, std::uint64_t value, std::uint64_t low,
                   std::uint64_t high)
{
    if (value >= low && value <= high) {
        return;
    }
    std::ostringstream message;
    message << "tpu_v3::address_map: " << what << " = " << value
            << " is outside the accepted range [" << low << ", " << high
            << "]";
    throw std::invalid_argument(message.str());
}

void require_power_of_two(const char* what, std::uint64_t value)
{
    if (value != 0 && (value & (value - 1)) == 0) {
        return;
    }
    std::ostringstream message;
    message << "tpu_v3::address_map: " << what << " = " << value
            << " must be a power of two";
    throw std::invalid_argument(message.str());
}

std::string core_prefix(chip_id_t chip, core_id_t core)
{
    return "chip" + std::to_string(chip) + ".core" + std::to_string(core) + '.';
}

} // namespace

std::vector<region> enumerate_regions(unsigned chips,
                                      std::uint64_t svm_capacity,
                                      std::uint64_t global_ram_capacity)
{
    require_range("chips", chips, 1, max_chips);
    require_range("svm_capacity", svm_capacity, svm_min_capacity,
                  svm_max_capacity);
    require_power_of_two("svm_capacity", svm_capacity);
    require_range("global_ram_capacity", global_ram_capacity,
                  global_ram_min_capacity, global_ram_max_capacity);
    require_power_of_two("global_ram_capacity", global_ram_capacity);

    std::vector<region> regions;
    // 3 global + per chip: 2 chip-level + 2 cores * 5 core-level.
    regions.reserve(3 + std::size_t{chips} * (2 + cores_per_chip * 5));

    // `size` is the decoded extent and never depends on a capacity; only
    // `capacity` does. An MMIO register file backs its whole window, so the
    // two are equal there.
    const auto mmio = [](std::uint64_t base, std::uint64_t size,
                         std::string name, chip_id_t chip, core_id_t core) {
        return region{base,        size, size, region_kind::mmio,
                      std::move(name), chip, core};
    };
    const auto memory = [](std::uint64_t base, std::uint64_t window,
                           std::uint64_t capacity, std::string name,
                           chip_id_t chip, core_id_t core) {
        return region{base,        window, capacity, region_kind::memory,
                      std::move(name), chip, core};
    };

    regions.push_back(memory(boot_rom_base, boot_rom_size, boot_rom_size,
                             "global.boot_rom", max_chips, cores_per_chip));
    regions.push_back(mmio(global_control_base, global_control_size,
                           "global.control", max_chips, cores_per_chip));
    regions.push_back(memory(global_ram_base, global_ram_window,
                             global_ram_capacity, "global.ram", max_chips,
                             cores_per_chip));

    for (chip_id_t chip = 0; chip < chips; ++chip) {
        const std::string chip_name = "chip" + std::to_string(chip) + '.';

        for (core_id_t core = 0; core < cores_per_chip; ++core) {
            const std::string prefix = core_prefix(chip, core);

            regions.push_back(memory(svm_base(chip, core), svm_window,
                                     svm_capacity, prefix + "svm", chip, core));
            regions.push_back(mmio(core_control(chip, core), core_control_size,
                                   prefix + "control", chip, core));
            for (mxu_id_t mxu = 0; mxu < mxus_per_core; ++mxu) {
                regions.push_back(mmio(mxu_control(chip, core, mxu),
                                       mxu_control_size,
                                       prefix + "mxu" + std::to_string(mxu)
                                           + "_control",
                                       chip, core));
            }
            regions.push_back(mmio(core_counters(chip, core),
                                   core_counters_size, prefix + "counters",
                                   chip, core));
        }

        regions.push_back(mmio(chip_control(chip), chip_control_size,
                               chip_name + "control", chip, cores_per_chip));
        regions.push_back(mmio(chip_counters(chip), chip_counters_size,
                               chip_name + "counters", chip, cores_per_chip));
    }

    return regions;
}

const region* find_region(const std::vector<region>& regions,
                          std::uint64_t address, std::uint64_t length) noexcept
{
    for (const auto& entry : regions) {
        if (contains(entry.base, entry.size, address, length)) {
            return &entry;
        }
    }
    return nullptr;
}

bool backs(const region& entry, std::uint64_t address,
           std::uint64_t length) noexcept
{
    return contains(entry.base, entry.capacity, address, length);
}

} // namespace cdc::components::tpu_v3::address_map
