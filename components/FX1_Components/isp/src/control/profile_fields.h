// SPDX-License-Identifier: Apache-2.0
// Bank-owned algorithm scalar fields of the profile controller (MAS v1.07
// §9.7, Table 9-194). In profile mode their CSR reads and writes address the
// TUNE_BANK-selected bank; an `extension` (TUNE_*) field is bank storage in
// both modes (§9.9.2). The register file keeps the datapath value: the
// active bank in profile mode, the legacy value (legacy fields) or the field
// reset (extension fields) in legacy mode.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cdc::components::fx1_isp {

struct profile_field {
   std::uint32_t offset;
   std::uint32_t mask;  // bank-owned bits of the word
   bool extension;
};

extern const profile_field profile_fields[];
extern const std::size_t num_profile_fields;

}  // namespace cdc::components::fx1_isp
