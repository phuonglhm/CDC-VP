// SPDX-License-Identifier: Apache-2.0
//
// Address-map unit tests. Plan §12 rule 10 requires a test that enumerates
// every region and proves non-overlap; that is `all_regions_are_disjoint`
// below, and it runs at every legal chip count rather than at one.
//
// Plain `main()`: the map has no SystemC in it, so its test should not need an
// elaboration to run.

#include "tpu_v3/address_map.h"
#include "tpu_v3/types.h"

#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace am = cdc::components::tpu_v3::address_map;
using cdc::components::tpu_v3::chip_id_t;
using cdc::components::tpu_v3::core_id_t;
using cdc::components::tpu_v3::cores_per_chip;
using cdc::components::tpu_v3::hart_id_of;
using cdc::components::tpu_v3::max_chips;
using cdc::components::tpu_v3::mxu_id_t;
using cdc::components::tpu_v3::mxus_per_core;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__ << ':' \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

// Calls `fn` and reports whether it threw std::invalid_argument.
template <typename Fn>
bool throws_invalid_argument(Fn&& fn)
{
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

void fixed_bases_match_the_document()
{
    // These literals are the one place a copy of the map is intentional: the
    // test is what would catch an accidental edit to the header, so reading
    // them from the header would make the test assert nothing.
    CHECK(am::boot_rom_base == 0x00000000ull);
    CHECK(am::global_control_base == 0x00010000ull);
    CHECK(am::global_ram_base == 0x80000000ull);
    CHECK(am::chip_aperture_base == 0xC0000000ull);
    CHECK(am::chip_aperture_stride == 0x08000000ull);
    CHECK(am::core_aperture_stride == 0x02000000ull);

    CHECK(am::chip_base(0) == 0xC0000000ull);
    CHECK(am::chip_base(7) == 0xF8000000ull);
    CHECK(am::core_base(0, 0) == 0xC0000000ull);
    CHECK(am::core_base(0, 1) == 0xC2000000ull);
    CHECK(am::core_base(7, 1) == 0xFA000000ull);

    CHECK(am::svm_base(0, 0) == 0xC0000000ull);
    CHECK(am::core_control(0, 0) == 0xC1000000ull);
    CHECK(am::mxu_control(0, 0, 0) == 0xC1010000ull);
    CHECK(am::mxu_control(0, 0, 1) == 0xC1020000ull);
    CHECK(am::core_counters(0, 0) == 0xC1030000ull);
    CHECK(am::chip_control(0) == 0xC4000000ull);
    CHECK(am::chip_counters(0) == 0xC4010000ull);
}

void everything_fits_below_4_gib()
{
    // Plan §12 rule 1. The last byte of the last chip aperture is the highest
    // address the map can produce.
    const std::uint64_t top =
        am::chip_base(max_chips - 1) + am::chip_aperture_stride - 1;
    CHECK(top == 0xFFFFFFFFull);

    for (unsigned chips = 1; chips <= max_chips; ++chips) {
        const auto regions = am::enumerate_regions(
            chips, am::svm_default_capacity, am::global_ram_default_capacity);
        for (const auto& r : regions) {
            CHECK_MSG(r.base + r.size <= 0x100000000ull,
                      r.name + " ends above 4 GiB");
        }
    }
}

void all_regions_are_disjoint()
{
    // Every legal chip count, and every SVM capacity extreme. The decoded map
    // is the same in all of them — capacity moves `capacity`, never `size` —
    // and this is what proves it: an overlap appearing only at one capacity
    // would mean the decode had started depending on the memory size.
    for (unsigned chips = 1; chips <= max_chips; ++chips) {
        for (std::uint64_t svm :
             {am::svm_min_capacity, am::svm_default_capacity,
              am::svm_max_capacity}) {
            const auto regions = am::enumerate_regions(
                chips, svm, am::global_ram_min_capacity);

            for (std::size_t i = 0; i < regions.size(); ++i) {
                for (std::size_t j = i + 1; j < regions.size(); ++j) {
                    const auto& a = regions[i];
                    const auto& b = regions[j];
                    const bool overlap =
                        a.base < b.base + b.size && b.base < a.base + a.size;
                    CHECK_MSG(!overlap,
                              a.name + " overlaps " + b.name + " at chips="
                                  + std::to_string(chips)
                                  + " svm=" + std::to_string(svm));
                }
            }
        }
    }
}

void decoded_extent_does_not_depend_on_capacity()
{
    // Decision record D6 and ADDRESS_MAP.md §5: the full window always
    // decodes. If the map shrank with the capacity, the same address would be
    // unmapped in one configuration and valid in another, and a firmware
    // pointer bug would change symptom with the SVM size.
    const auto small = am::enumerate_regions(2, am::svm_min_capacity,
                                             am::global_ram_min_capacity);
    const auto large = am::enumerate_regions(2, am::svm_max_capacity,
                                             am::global_ram_max_capacity);

    CHECK(small.size() == large.size());
    for (std::size_t i = 0; i < small.size() && i < large.size(); ++i) {
        CHECK_MSG(small[i].name == large[i].name, "region order changed");
        CHECK_MSG(small[i].base == large[i].base,
                  small[i].name + ": base moved with the capacity");
        CHECK_MSG(small[i].size == large[i].size,
                  small[i].name + ": decoded size moved with the capacity");
    }

    // ...while `capacity` is what actually tracks the configuration.
    const auto* svm = am::find_region(small, am::svm_base(0, 0), 4);
    CHECK(svm != nullptr);
    if (svm != nullptr) {
        CHECK(svm->size == am::svm_window);
        CHECK(svm->capacity == am::svm_min_capacity);
        CHECK(svm->is_partially_backed());
    }

    const auto* ram = am::find_region(large, am::global_ram_base, 4);
    CHECK(ram != nullptr);
    if (ram != nullptr) {
        CHECK(ram->size == am::global_ram_window);
        CHECK(ram->capacity == am::global_ram_max_capacity);
        CHECK(!ram->is_partially_backed());
    }

    // At the reference capacity the SVM window is fully backed.
    const auto reference = am::enumerate_regions(
        1, am::svm_default_capacity, am::global_ram_default_capacity);
    const auto* full = am::find_region(reference, am::svm_base(0, 0), 4);
    CHECK(full != nullptr);
    if (full != nullptr) {
        CHECK(full->capacity == am::svm_window);
        CHECK(!full->is_partially_backed());
    }
}

void unbacked_addresses_decode_but_are_not_backed()
{
    const auto regions = am::enumerate_regions(1, am::svm_min_capacity,
                                               am::global_ram_min_capacity);

    const std::uint64_t base = am::svm_base(0, 0);
    const std::uint64_t above = base + am::svm_min_capacity;

    const auto* svm = am::find_region(regions, above, 4);
    CHECK_MSG(svm != nullptr,
              "an address inside the SVM window above the capacity must still "
              "decode to the SVM, so the target reports the error");
    if (svm != nullptr) {
        CHECK(svm->name == "chip0.core0.svm");
        CHECK(!am::backs(*svm, above, 4));
        CHECK(am::backs(*svm, base, 4));
        CHECK(am::backs(*svm, above - 4, 4));
        // A transfer straddling the capacity boundary is not backed either.
        CHECK(!am::backs(*svm, above - 2, 4));
    }

    // The byte immediately past the window belongs to core_control, not to a
    // hole and not to the SVM.
    const auto* next = am::find_region(regions, base + am::svm_window, 4);
    CHECK(next != nullptr);
    if (next != nullptr) {
        CHECK(next->name == "chip0.core0.control");
    }
}

void region_names_are_unique()
{
    // Non-overlap alone would not catch two regions given the same identity,
    // and the name is what a metrics report attributes traffic to.
    const auto regions = am::enumerate_regions(max_chips,
                                               am::svm_default_capacity,
                                               am::global_ram_default_capacity);
    std::set<std::string> names;
    for (const auto& r : regions) {
        CHECK_MSG(names.insert(r.name).second, "duplicate region name " + r.name);
    }
    // 3 global + 8 chips * (2 chip-level + 2 cores * 5 core-level)
    CHECK(regions.size() == 3 + 8 * (2 + 2 * 5));
}

void every_named_resource_is_inside_its_aperture()
{
    for (chip_id_t chip = 0; chip < max_chips; ++chip) {
        const std::uint64_t cbase = am::chip_base(chip);
        const std::uint64_t csize = am::chip_aperture_stride;

        CHECK(am::contains(cbase, csize, am::chip_control(chip),
                           am::chip_control_size));
        CHECK(am::contains(cbase, csize, am::chip_counters(chip),
                           am::chip_counters_size));

        for (core_id_t core = 0; core < cores_per_chip; ++core) {
            const std::uint64_t base = am::core_base(chip, core);
            const std::uint64_t size = am::core_aperture_stride;

            CHECK(am::contains(cbase, csize, base, size));
            CHECK(am::contains(base, size, am::svm_base(chip, core),
                               am::svm_max_capacity));
            CHECK(am::contains(base, size, am::core_control(chip, core),
                               am::core_control_size));
            CHECK(am::contains(base, size, am::core_counters(chip, core),
                               am::core_counters_size));
            for (mxu_id_t mxu = 0; mxu < mxus_per_core; ++mxu) {
                CHECK(am::contains(base, size, am::mxu_control(chip, core, mxu),
                                   am::mxu_control_size));
            }
        }
    }
}

void contains_does_not_wrap()
{
    // Plan §12 rule 5. A length chosen to make base+length wrap must not be
    // reported as inside; this is the negative control for the arithmetic in
    // `contains()`.
    constexpr std::uint64_t base = 0xC0000000ull;
    constexpr std::uint64_t size = 0x00400000ull;

    CHECK(am::contains(base, size, base, size));
    CHECK(am::contains(base, size, base + size - 1, 1));
    CHECK(!am::contains(base, size, base + size, 1));
    CHECK(!am::contains(base, size, base - 1, 1));
    CHECK(!am::contains(base, size, base, size + 1));

    // base + huge wraps in 64-bit arithmetic; the check must still say no.
    CHECK(!am::contains(base, size, base, 0xFFFFFFFFFFFFFFFFull));
    CHECK(!am::contains(base, size, base + size - 1, 0xFFFFFFFFFFFFFFFFull));

    // A region ending exactly at 2^64 must not swallow everything either.
    CHECK(!am::contains(0xFFFFFFFFFFFFF000ull, 0x1000ull, 0ull, 1ull));
}

void hart_ids_are_unique_and_reversible()
{
    std::set<unsigned> seen;
    for (chip_id_t chip = 0; chip < max_chips; ++chip) {
        for (core_id_t core = 0; core < cores_per_chip; ++core) {
            const auto hart = hart_id_of(chip, core);
            CHECK_MSG(seen.insert(hart).second,
                      "duplicate hart id " + std::to_string(hart));
            CHECK(cdc::components::tpu_v3::chip_of_hart(hart) == chip);
            CHECK(cdc::components::tpu_v3::core_of_hart(hart) == core);
        }
    }
    CHECK(seen.size() == max_chips * cores_per_chip);
    CHECK(hart_id_of(3, 1) == 7);
}

void find_region_locates_and_rejects()
{
    const auto regions = am::enumerate_regions(2, am::svm_default_capacity,
                                               am::global_ram_default_capacity);

    const auto* svm = am::find_region(regions, am::svm_base(1, 1), 64);
    CHECK(svm != nullptr);
    if (svm != nullptr) {
        CHECK(svm->name == "chip1.core1.svm");
        CHECK(svm->kind == am::region_kind::memory);
        CHECK(svm->chip == 1);
        CHECK(svm->core == 1);
    }

    const auto* mxu = am::find_region(regions, am::mxu_control(0, 1, 1), 4);
    CHECK(mxu != nullptr);
    if (mxu != nullptr) {
        CHECK(mxu->name == "chip0.core1.mxu1_control");
        CHECK(mxu->kind == am::region_kind::mmio);
    }

    // A chip that is not instantiated does not decode.
    CHECK(am::find_region(regions, am::chip_base(5), 4) == nullptr);

    // The reserved hole between the low peripherals and global RAM.
    CHECK(am::find_region(regions, 0x00020000ull, 4) == nullptr);

    // A transfer that starts inside a region and runs past its end.
    CHECK(am::find_region(regions, am::boot_rom_base + am::boot_rom_size - 4, 8)
          == nullptr);
}

void enumerate_rejects_bad_arguments()
{
    CHECK(throws_invalid_argument(
        [] { am::enumerate_regions(0, am::svm_default_capacity,
                                   am::global_ram_default_capacity); }));
    CHECK(throws_invalid_argument(
        [] { am::enumerate_regions(max_chips + 1, am::svm_default_capacity,
                                   am::global_ram_default_capacity); }));
    // Above the 16 MiB window.
    CHECK(throws_invalid_argument(
        [] { am::enumerate_regions(1, am::svm_max_capacity * 2,
                                   am::global_ram_default_capacity); }));
    // Not a power of two.
    CHECK(throws_invalid_argument(
        [] { am::enumerate_regions(1, 0x300000ull,
                                   am::global_ram_default_capacity); }));
    // Global RAM above its window.
    CHECK(throws_invalid_argument(
        [] { am::enumerate_regions(1, am::svm_default_capacity,
                                   am::global_ram_max_capacity * 2); }));
}

} // namespace

int main()
{
    fixed_bases_match_the_document();
    everything_fits_below_4_gib();
    all_regions_are_disjoint();
    decoded_extent_does_not_depend_on_capacity();
    unbacked_addresses_decode_but_are_not_backed();
    region_names_are_unique();
    every_named_resource_is_inside_its_aperture();
    contains_does_not_wrap();
    hart_ids_are_unique_and_reversible();
    find_region_locates_and_rejects();
    enumerate_rejects_bad_arguments();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_address_map: all checks passed\n";
    return 0;
}
