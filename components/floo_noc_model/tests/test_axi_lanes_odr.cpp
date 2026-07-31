// SPDX-License-Identifier: Apache-2.0
//
// Two translation units, both including `axi_lanes.hpp`, linked into one
// executable. If the header's free functions lose their `inline`, this fails to
// link with a duplicate-symbol error — which is the whole point of the test.
//
// Every other test has a single source file, so none of them can catch it.

#include "floo_noc_model/axi_lanes.hpp"

#include <iostream>

#include <systemc>

// Defined in test_axi_lanes_odr_second.cpp, which includes the same header.
unsigned shape_beats_from_other_tu(unsigned long long addr, unsigned length);

int sc_main(int, char**)
{
    const auto here = cdc::components::axi_lanes::shape_of(0x1005, 6).beats;
    const auto there = shape_beats_from_other_tu(0x1005, 6);
    if (here != there || here != 2) {
        std::cerr << "FAIL: shape_of disagrees across translation units: "
                  << here << " vs " << there << '\n';
        return 1;
    }
    std::cout << "PASS: axi_lanes.hpp links from multiple translation units\n";
    return 0;
}
