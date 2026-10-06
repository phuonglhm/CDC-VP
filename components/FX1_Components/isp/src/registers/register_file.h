// SPDX-License-Identifier: Apache-2.0
// Generic access semantics of the FX1 ISP register map (HAS Table 8-1 and
// Table 7-10) over the generated register table. No SystemC dependency:
// time enters only as a core-clock cycle number used for the
// "hardware set wins over a same-cycle software clear" rule.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "registers/csr_desc.h"

namespace cdc::components::fx1_isp {

class register_file {
public:
   static constexpr std::uint32_t aperture_bytes = 0x10000u;

   // What one software write did to one register.
   struct write_effect {
      std::uint32_t offset;
      std::uint32_t data;         // raw write data
      std::uint32_t lane_mask;    // 0xFF per enabled byte lane
      std::uint32_t old_value;    // stored value before the write
      std::uint32_t new_value;    // stored value after the write
      std::uint32_t rw_written;   // RW bits covered by the strobes
      std::uint32_t w1s_set;      // W1S bits written with 1
      std::uint32_t w1sc_pulse;   // W1SC bits written with 1
      std::uint32_t w1c_cleared;  // W1C bits actually cleared
   };

   using write_hook = std::function<void(const write_effect &)>;
   using read_hook = std::function<std::uint32_t(std::uint32_t stored)>;
   using hw_set_observer = std::function<void(std::uint32_t offset, std::uint32_t newly_set)>;

   register_file();

   // i_rst_n: every register to its reset value, holds and race state cleared.
   void reset();

   // Descriptor of the register at a byte offset (bits [1:0] ignored), or
   // nullptr for an unmapped word.
   const csr::reg_desc *find(std::uint32_t addr) const;

   // Software (CSR port) access. `addr` is relative to the ISP base; bits
   // [1:0] are ignored. `lane_mask` has 0xFF in every byte lane whose strobe is
   // set. Unmapped words read 0 and ignore writes; reserved bits read 0.
   std::uint32_t sw_read(std::uint32_t addr) const;
   void sw_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lane_mask,
                 std::uint64_t cycle);

   // Software writes whose W1S/W1C effects are suppressed while `true` (the
   // soft-reset window: the state they would change is held in reset).
   void set_command_block(bool blocked) { command_block_ = blocked; }

   // Stored value without read hooks.
   std::uint32_t peek(std::uint32_t offset) const;

   // Hardware side. Masks are clipped to the defined bits of the register.
   // hw_set records the cycle so that a software W1C in the same cycle loses.
   void hw_set(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle);
   void hw_clear(std::uint32_t offset, std::uint32_t mask);
   void hw_write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value);
   // Level-held bits (e.g. DMA_ERR.IDMA_UNDERRUN): set and kept set while the
   // level is true; W1C has no effect on them until the level drops.
   void hw_hold(std::uint32_t offset, std::uint32_t mask, bool level, std::uint64_t cycle);
   std::uint32_t held(std::uint32_t offset) const;

   void on_write(std::uint32_t offset, write_hook hook);
   void on_read(std::uint32_t offset, read_hook hook);
   void on_hw_set(hw_set_observer observer) { hw_set_observer_ = std::move(observer); }

private:
   struct entry {
      const csr::reg_desc *desc = nullptr;
      std::uint32_t value = 0;
      std::uint32_t held = 0;
      std::uint64_t set_cycle = 0;
      std::uint32_t set_mask = 0;  // bits hardware set during set_cycle
      write_hook on_write;
      read_hook on_read;
   };

   entry &at(std::uint32_t offset);
   const entry &at(std::uint32_t offset) const;
   void note_hw_set(entry &e, std::uint32_t newly, std::uint64_t cycle);

   std::vector<entry> entries_;
   std::array<std::int16_t, aperture_bytes / 4> index_{};
   hw_set_observer hw_set_observer_;
   bool command_block_ = false;
};

}  // namespace cdc::components::fx1_isp
