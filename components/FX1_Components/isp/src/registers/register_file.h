// SPDX-License-Identifier: Apache-2.0
// Generic access semantics of the FX1 ISP register map over the generated
// register table: MAS v1.07 Table 8-178 (access types) and Table 8-180
// (per-field update and priority rules). No SystemC dependency: time enters
// only as a core-clock cycle number for the same-edge priority rules:
//   W1C  (Q & ~(D&M | H)) | S  -- hardware set wins over software and hardware clear
//   W1S  (Q & ~H) | (D&M)      -- software set wins over hardware clear
//   W1P  pulse D&M for one clock, reads zero (a command; not stored)
//   W1SC pulse D&M as a recovery request, reads zero (not stored)
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
      std::uint32_t w1p_pulse;    // W1P bits written with 1 (one-clock command)
      std::uint32_t w1c_cleared;  // W1C bits actually cleared
   };

   // Pre-write filter: called before the write is merged, with the raw data
   // and strobes. It returns the strobes the write may use; 0 rejects the
   // whole write (no storage change, no pulse, no write hook). A consumer
   // uses it for LOCKED/MODE protection (MAS §9.7, §9.9.3).
   using write_filter = std::function<std::uint32_t(std::uint32_t data, std::uint32_t lane_mask,
                                                     std::uint64_t cycle)>;
   // Bus-read action: side effect of a CSR-port read (e.g. a result snapshot
   // or a NO_LEASE error, MAS §9.8). Never run by debug reads.
   using bus_read_action = std::function<void(std::uint64_t cycle)>;

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
   // sw_read has no side effects (debug view); bus_read runs the register's
   // bus-read action first, then returns the same value.
   std::uint32_t sw_read(std::uint32_t addr) const;
   std::uint32_t bus_read(std::uint32_t addr, std::uint64_t cycle);
   void sw_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lane_mask,
                 std::uint64_t cycle);

   // Software writes whose W1S/W1C/W1P effects are suppressed while `true`
   // (the soft-reset window: the state they would change is held in reset).
   void set_command_block(bool blocked) { command_block_ = blocked; }

   // Stored value without read hooks.
   std::uint32_t peek(std::uint32_t offset) const;
   // The value a consumer clocked on edge `cycle` samples: the registered Q
   // of that edge. A W1S bit software set from 0 on `cycle` reaches its
   // consumer on the next edge (MAS Table 8-180: Q updates on the edge, the
   // consumer's clear term is derived from Q).
   std::uint32_t registered(std::uint32_t offset, std::uint64_t cycle) const;

   // Hardware side. Masks are clipped to the defined bits of the register.
   // hw_set records the cycle so that a W1C (software or hardware clear) in
   // the same cycle loses; hw_clear on `cycle` does not clear a W1S bit that
   // software set on that cycle, nor a W1C bit hardware set on it, whatever
   // the order of the calls within the cycle.
   void hw_set(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle);
   void hw_clear(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle);
   // A consumer acknowledging its own self-clearing W1S command inside the
   // write that issued it ("self-feedback clear", MAS §8.1.3): clears at once,
   // without the same-edge priority of hw_clear.
   void hw_ack(std::uint32_t offset, std::uint32_t mask);
   void hw_write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value);
   // Level-held bits (e.g. DMA_ERR.IDMA_UNDERRUN): set and kept set while the
   // level is true; W1C has no effect on them until the level drops.
   void hw_hold(std::uint32_t offset, std::uint32_t mask, bool level, std::uint64_t cycle);
   std::uint32_t held(std::uint32_t offset) const;

   void on_write(std::uint32_t offset, write_hook hook);
   void on_read(std::uint32_t offset, read_hook hook);
   void on_pre_write(std::uint32_t offset, write_filter filter);
   void on_bus_read(std::uint32_t offset, bus_read_action action);
   void on_hw_set(hw_set_observer observer) { hw_set_observer_ = std::move(observer); }

private:
   struct entry {
      const csr::reg_desc *desc = nullptr;
      std::uint32_t value = 0;
      std::uint32_t held = 0;
      std::uint64_t set_cycle = 0;
      std::uint32_t set_mask = 0;     // bits hardware set during set_cycle
      std::uint64_t sw_set_cycle = 0;
      std::uint32_t sw_set_mask = 0;  // W1S bits software set during sw_set_cycle
      std::uint64_t edge_cycle = 0;
      std::uint32_t edge_q = 0;       // registered value of edge_cycle (before its first update)
      write_hook on_write;
      read_hook on_read;
      write_filter pre_write;
      bus_read_action bus_read;
   };

   entry &at(std::uint32_t offset);
   const entry &at(std::uint32_t offset) const;
   void note_hw_set(entry &e, std::uint32_t newly, std::uint64_t cycle);
   static void begin_edge(entry &e, std::uint64_t cycle);

   std::vector<entry> entries_;
   std::array<std::int16_t, aperture_bytes / 4> index_{};
   hw_set_observer hw_set_observer_;
   bool command_block_ = false;
};

}  // namespace cdc::components::fx1_isp
