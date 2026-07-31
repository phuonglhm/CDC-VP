// SPDX-License-Identifier: Apache-2.0
//
// The second translation unit for the ODR test. It exists only to include
// `axi_lanes.hpp` a second time in the same link.

#include "floo_noc_model/axi_lanes.hpp"

unsigned shape_beats_from_other_tu(unsigned long long addr, unsigned length)
{
    return cdc::components::axi_lanes::shape_of(addr, length).beats;
}
