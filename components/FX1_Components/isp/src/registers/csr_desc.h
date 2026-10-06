// SPDX-License-Identifier: Apache-2.0
// Static description of the FX1 ISP register map. The table itself is
// generated (csr_table_gen.cpp, tools/gen_csr.py); this header only defines
// its shape.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cdc::components::fx1_isp::csr {

// Access types of HAS Table 8-1.
enum class access : std::uint8_t { rw, ro, w1c, w1s, w1sc };

struct field_desc {
   const char *name;
   std::uint8_t lsb;
   std::uint8_t width;
   std::uint32_t mask;   // register-relative, tied-zero bits removed
   access type;
   std::uint32_t reset;  // field-relative
   bool implemented;     // false when the map says no hardware is behind it
   std::uint16_t source_row;  // worksheet "Registers" row
};

struct reg_desc {
   std::uint32_t offset;
   const char *name;
   std::uint32_t reset;
   std::uint32_t rw_mask;
   std::uint32_t ro_mask;
   std::uint32_t w1c_mask;
   std::uint32_t w1s_mask;
   std::uint32_t w1sc_mask;
   const field_desc *fields;
   std::uint8_t num_fields;
   std::uint16_t source_row;

   std::uint32_t defined_mask() const {
      return rw_mask | ro_mask | w1c_mask | w1s_mask | w1sc_mask;
   }
};

extern const reg_desc registers[];
extern const std::size_t num_registers;

}  // namespace cdc::components::fx1_isp::csr
