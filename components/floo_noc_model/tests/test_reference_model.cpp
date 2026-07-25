// SPDX-License-Identifier: Apache-2.0

#include "floo_noc_model/reference_model.hpp"

#include <systemc>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        SC_REPORT_ERROR("test_reference_model", message);
    }
}

} // namespace

int sc_main(int, char**)
{
    using floo::model::coordinate;
    using floo::model::direction;
    using floo::model::endpoint_region;
    using floo::model::reference_address_map;

    const reference_address_map map({
        endpoint_region{0x0000, 0x1000, coordinate{0, 0}},
        endpoint_region{0x8000, 0x2000, coordinate{3, 1}},
    });

    expect(map.decode(0x0000) == coordinate(0, 0),
           "base address must decode");
    expect(map.decode(0x0fff) == coordinate(0, 0),
           "last byte in a region must decode");
    expect(!map.decode(0x1000).has_value(),
           "end address is exclusive");
    expect(map.decode(0x9fff) == coordinate(3, 1),
           "second region must decode");

    bool rejected_overlap = false;
    try {
        const reference_address_map invalid({
            endpoint_region{0x1000, 0x1000, coordinate{0, 0}},
            endpoint_region{0x1800, 0x1000, coordinate{1, 0}},
        });
        (void)invalid;
    } catch (const std::invalid_argument&) {
        rejected_overlap = true;
    }
    expect(rejected_overlap, "overlapping address ranges must be rejected");

    const auto path =
        floo::model::xy_path(coordinate{0, 0}, coordinate{2, 1}, 4, 4);
    const std::vector<direction> expected{
        direction::east,
        direction::east,
        direction::north,
        direction::eject,
    };
    expect(path == expected, "XY path must resolve X before Y");

    bool rejected_out_of_mesh = false;
    try {
        (void)floo::model::xy_path(
            coordinate{0, 0}, coordinate{4, 0}, 4, 4);
    } catch (const std::out_of_range&) {
        rejected_out_of_mesh = true;
    }
    expect(rejected_out_of_mesh, "out-of-mesh destination must be rejected");

    const int errors =
        sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);
    if (errors == 0) {
        std::cout << "PASS: reference address map and XY route\n";
    }
    return errors == 0 ? 0 : 1;
}
