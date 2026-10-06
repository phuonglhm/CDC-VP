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

void register_file::reset() {
   for (auto &e : entries_) {
      e.value = e.desc->reset & e.desc->defined_mask() & ~e.desc->w1sc_mask;
      e.held = 0;
      e.set_cycle = 0;
      e.set_mask = 0;
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
   // W1SC fields always read back zero (HAS Table 7-10).
   return value & e.desc->defined_mask() & ~e.desc->w1sc_mask;
}

void register_file::sw_write(std::uint32_t addr, std::uint32_t data, std::uint32_t lane_mask,
                             std::uint64_t cycle) {
   if (!find(addr) || lane_mask == 0) {
      return;  // unmapped, or no strobes: accepted and changes nothing
   }
   entry &e = at(addr & ~3u);
   const csr::reg_desc &d = *e.desc;
   const std::uint32_t written = data & lane_mask;

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
      v |= fx.w1s_set;
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

void register_file::note_hw_set(entry &e, std::uint32_t newly, std::uint64_t cycle) {
   if (e.set_cycle != cycle) {
      e.set_cycle = cycle;
      e.set_mask = 0;
   }
   e.set_mask |= newly;
}

void register_file::hw_set(std::uint32_t offset, std::uint32_t mask, std::uint64_t cycle) {
   entry &e = at(offset);
   mask &= e.desc->defined_mask() & ~e.desc->w1sc_mask;
   const std::uint32_t newly = mask & ~e.value;
   e.value |= mask;
   note_hw_set(e, mask, cycle);
   if (newly && hw_set_observer_) {
      hw_set_observer_(e.desc->offset, newly);
   }
}

void register_file::hw_clear(std::uint32_t offset, std::uint32_t mask) {
   entry &e = at(offset);
   e.value &= ~(mask & ~e.held);
}

void register_file::hw_write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value) {
   entry &e = at(offset);
   mask &= e.desc->defined_mask() & ~e.desc->w1sc_mask;
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

}  // namespace cdc::components::fx1_isp
