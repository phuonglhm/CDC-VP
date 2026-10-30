// SPDX-License-Identifier: Apache-2.0
#include "registers/register_file.h"

#include <stdexcept>
#include <string>

namespace cdc::components::fx1_isp {

namespace {

std::uint32_t word_index(std::uint32_t addr) {
   return (addr % register_file::aperture_bytes) >> 2;
}

}  // namespace

register_file::register_file() {
   index_.fill(-1);
   entries_.resize(csr::num_registers);
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      entries_[i].desc = &csr::registers[i];
      index_[word_index(csr::registers[i].offset)] = static_cast<std::int16_t>(i);
   }
   reset();
}

namespace {

// Command bits have no storage and always read zero (W1SC, W1P).
std::uint32_t pulse_mask(const csr::reg_desc &d) {
   return d.w1sc_mask | d.w1p_mask;
}

}  // namespace

void register_file::reset() {
   for (auto &e : entries_) {
      e.value = e.desc->reset & e.desc->defined_mask() & ~pulse_mask(*e.desc);
      e.held = 0;
      e.set_cycle = 0;
      e.set_mask = 0;
      e.sw_set_cycle = 0;
      e.sw_set_mask = 0;
      e.edge_cycle = 0;
      e.edge_q = e.value;
   }
   command_block_ = false;
}

const csr::reg_desc *register_file::find(std::uint32_t addr) const {
   if (addr >= aperture_bytes) {
      return nullptr;
   }
   const std::int16_t i = index_[word_index(addr)];
   return i < 0 ? nullptr : entries_[static_cast<std::size_t>(i)].desc;
}

register_file::entry &register_file::at(std::uint32_t offset) {
   return const_cast<entry &>(static_cast<const register_file &>(*this).at(offset));
}

const register_file::entry &register_file::at(std::uint32_t offset) const {
   const std::int16_t i = offset < aperture_bytes ? index_[word_index(offset)] : -1;
   if (i < 0) {
      throw std::out_of_range("fx1_isp: no register at offset " + std::to_string(offset));
   }
   return entries_[static_cast<std::size_t>(i)];
}

std::uint32_t register_file::sw_read(std::uint32_t addr) const {
   if (!find(addr)) {
      return 0;
   }
   const entry &e = at(addr & ~3u);
   const std::uint32_t stored = e.value;
   const std::uint32_t value = e.on_read ? e.on_read(stored) : stored;
   // W1SC and W1P fields always read back zero (MAS Table 8-180).
   return value & e.desc->defined_mask() & ~pulse_mask(*e.desc);
}

std::uint32_t register_file::bus_read(std::uint32_t addr, std::uint64_t cycle) {
   if (!find(addr)) {
      return 0;
   }
   entry &e = at(addr & ~3u);
   if (e.bus_read) {
      e.bus_read(cycle);
   }
   return sw_read(addr);
}

void register_file::sw_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lane_mask,
                             std::uint64_t cycle) {
   if (!find(addr) || lane_mask == 0) {
      return;  // unmapped, or no strobes: accepted and changes nothing
   }
   entry &e = at(addr & ~3u);
   const csr::reg_desc &d = *e.desc;
   if (e.pre_write) {
      lane_mask &= e.pre_write(data, lane_mask, cycle);
      if (lane_mask == 0) {
         return;  // rejected by the consumer: nothing stored, no pulse, no hook
      }
   }
   const std::uint32_t written = data & lane_mask;
   begin_edge(e, cycle);

   write_effect fx{};
   fx.offset = d.offset;
   fx.data = data;
   fx.lane_mask = lane_mask;
   fx.old_value = e.value;
   fx.rw_written = d.rw_mask & lane_mask;
   fx.w1sc_pulse = d.w1sc_mask & written;

   std::uint32_t v = (e.value & ~fx.rw_written) | (written & d.rw_mask);
   if (!command_block_) {
      fx.w1s_set = d.w1s_mask & written;
      fx.w1p_pulse = d.w1p_mask & written;
      v |= fx.w1s_set;
      if (fx.w1s_set) {  // software set wins over a hardware clear on this edge
         if (e.sw_set_cycle != cycle) {
            e.sw_set_cycle = cycle;
            e.sw_set_mask = 0;
         }
         e.sw_set_mask |= fx.w1s_set;
      }
      const std::uint32_t protect = e.held | (e.set_cycle == cycle ? e.set_mask : 0u);
      fx.w1c_cleared = d.w1c_mask & written & ~protect & e.value;
      v &= ~fx.w1c_cleared;
   }
   e.value = v;
   fx.new_value = v;
   if (e.on_write) {
      e.on_write(fx);
   }
}

std::uint32_t register_file::peek(std::uint32_t offset) const {
   return at(offset).value;
}

std::uint32_t register_file::registered(std::uint32_t offset, std::uint64_t cycle) const {
   const entry &e = at(offset);
   // sw_set_cycle == cycle implies edge_cycle == cycle (sw_write opened the edge).
   const std::uint32_t fresh = e.sw_set_cycle == cycle ? e.sw_set_mask & ~e.edge_q : 0u;
   return e.value & ~fresh;
}

void register_file::begin_edge(entry &e, std::uint64_t cycle) {
   if (e.edge_cycle != cycle) {
      e.edge_cycle = cycle;
      e.edge_q = e.value;
   }
}

void register_file::note_hw_set(entry &e, std::uint32_t newly, std::uint64_t cycle) {
   if (e.set_cycle != cycle) {
      e.set_cycle = cycle;
      e.set_mask = 0;
   }
   e.set_mask |= newly;
}

void register_file::hw_set(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle) {
   entry &e = at(offset);
   begin_edge(e, cycle);
   mask &= e.desc->defined_mask() & ~pulse_mask(*e.desc);
   const std::uint32_t newly = mask & ~e.value;
   e.value |= mask;
   note_hw_set(e, mask, cycle);
   if (newly && hw_set_observer_) {
      hw_set_observer_(e.desc->offset, newly);
   }
}

void register_file::hw_clear(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle) {
   entry &e = at(offset);
   begin_edge(e, cycle);
   std::uint32_t keep = e.held;
   if (e.sw_set_cycle == cycle) {
      keep |= e.sw_set_mask & e.desc->w1s_mask;  // W1S: software set wins
   }
   if (e.set_cycle == cycle) {
      keep |= e.set_mask & e.desc->w1c_mask;  // W1C: hardware set wins over any clear
   }
   e.value &= ~(mask & ~keep);
}

void register_file::hw_ack(std::uint32_t offset, std::uint32_t mask) {
   entry &e = at(offset);
   e.value &= ~(mask & ~e.held);
}

void register_file::hw_write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value) {
   entry &e = at(offset);
   mask &= e.desc->defined_mask() & ~pulse_mask(*e.desc);
   e.value = (e.value & ~mask) | (value & mask);
}

void register_file::hw_hold(std::uint32_t offset, std::uint32_t mask, bool level,
                            std::uint64_t cycle) {
   entry &e = at(offset);
   mask &= e.desc->defined_mask();
   if (level) {
      e.held |= mask;
      hw_set(offset, mask, cycle);
   } else {
      e.held &= ~mask;  // the sticky bit stays set until software clears it
   }
}

std::uint32_t register_file::held(std::uint32_t offset) const {
   return at(offset).held;
}

void register_file::on_write(std::uint32_t offset, write_hook hook) {
   at(offset).on_write = std::move(hook);
}

void register_file::on_read(std::uint32_t offset, read_hook hook) {
   at(offset).on_read = std::move(hook);
}

void register_file::on_pre_write(std::uint32_t offset, write_filter filter) {
   at(offset).pre_write = std::move(filter);
}

void register_file::on_bus_read(std::uint32_t offset, bus_read_action action) {
   at(offset).bus_read = std::move(action);
}

}  // namespace cdc::components::fx1_isp
