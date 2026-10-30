// SPDX-License-Identifier: Apache-2.0
// Statistics configuration, payloads and result banks of AEC (MAS v1.07
// §6.21), AWB (§6.22) and AF (§6.23), shared by the pipeline (accumulation)
// and the control unit (ownership and readout, §9.8, §9.9.9).
//
// Every engine has two result banks: FREE -> FILLING (reserved at an enabled
// local SOF) -> READY (published after the frame's final sample) -> LEASED
// (acquired by software) -> FREE (released). READY and LEASED banks are never
// written. The pipeline accumulates the frame into a payload of its own; the
// control unit copies it into the reserved bank at publication. Readout sees
// only the leased bank, so the copy is equivalent to in-place accumulation.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <limits>
#include <vector>

namespace cdc::components::fx1_isp {

// Saturating 64-bit accumulation; saturation sets the result's overflow flag
// (MAS §6.21.3.3, §6.23.3.7).
inline void sat_add(std::uint64_t &acc, std::uint64_t v, bool &overflow) {
   if (acc > std::numeric_limits<std::uint64_t>::max() - v) {
      acc = std::numeric_limits<std::uint64_t>::max();
      overflow = true;
   } else {
      acc += v;
   }
}

// ---- AEC -----------------------------------------------------------------------

// Spatially weighted histogram (MAS §6.21.3.4; profile mode only).
struct aec_hist_config {
   bool en = false, roi_en = false;
   std::uint32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0, sx = 1, sy = 1;
   std::array<std::uint32_t, 25> w{1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
};

// The AEC set of a frame: legacy mode transfers the shadow set at the SOF
// after AEC_CTRL.commit; profile mode takes the active bank (MAS §8.3).
struct aec_config {
   bool en = false;
   std::uint32_t source = 0;  // TUNE_AEC_SOURCE: 0 BPC, 1 DG
   std::uint32_t nx = 32, ny = 24, zw = 120, zh = 90;
   std::uint32_t min_clip = 0, max_clip = 4095;
   std::uint32_t th_ue = 272, th_oe = 3744;
   aec_hist_config hist;
   // MAS §6.21.4: grid bounds and coverage, clip and threshold order, weighted
   // ROI and steps, for the active image W x H.
   bool legal(std::uint32_t w, std::uint32_t h) const {
      if (nx < 1 || nx > 32 || ny < 1 || ny > 24 || zw == 0 || zh == 0) {
         return false;
      }
      if (!((nx - 1) * zw < w && w <= nx * zw && (ny - 1) * zh < h && h <= ny * zh)) {
         return false;
      }
      if (min_clip > max_clip || th_ue >= th_oe) {
         return false;
      }
      return !(hist.en && (hist.sx == 0 || hist.sy == 0 ||
                           (hist.roi_en && !(hist.x0 < hist.x1 && hist.x1 <= w && hist.y0 < hist.y1 && hist.y1 <= h))));
   }
};

struct aec_zone {
   std::uint64_t sum = 0, count = 0, oe = 0, ue = 0;
   std::uint32_t min = 4095, max = 0;  // empty zone: 4095 / 0
};

// One frame's AEC result: the logical 64-bit indices of MAS Table 9-193.
struct aec_payload {
   std::uint64_t sum = 0, count = 0, oe = 0, ue = 0, wtotal = 0, wsum = 0;
   std::uint32_t min = 4095, max = 0;
   bool overflow = false;
   std::array<std::uint64_t, 64> hist{}, whist{};
   std::vector<aec_zone> zones;  // nx * ny, row-major
   std::uint32_t roi_start = 0, roi_end = 0, steps = 0;  // effective weighted ROI, packed X | Y << 16

   void restart(const aec_config &c, std::uint32_t w, std::uint32_t h) {
      *this = aec_payload{};
      zones.assign(std::size_t{c.nx} * c.ny, aec_zone{});
      if (c.hist.en) {
         const std::uint32_t x0 = c.hist.roi_en ? c.hist.x0 : 0, y0 = c.hist.roi_en ? c.hist.y0 : 0;
         const std::uint32_t x1 = c.hist.roi_en ? c.hist.x1 : w, y1 = c.hist.roi_en ? c.hist.y1 : h;
         roi_start = x0 | (y0 << 16);
         roi_end = x1 | (y1 << 16);
         steps = c.hist.sx | (c.hist.sy << 8);
      }
   }
};

// ---- AF ------------------------------------------------------------------------

struct af_config {
   bool en = false;
   std::uint32_t mode = 0;       // TUNE_AF_CTRL.WINDOW_MODE: 0 uniform 4x4, 1 rectangles
   std::uint32_t mask = 0xFFFF;  // TUNE_AF_CTRL.WINDOW_MASK (mode 1)
   std::uint32_t noise = 0;      // TUNE_AF_NOISE
   std::array<std::array<std::uint32_t, 4>, 16> rects{};  // x0, y0, x1, y1 (half-open)
};

struct af_window {
   std::uint64_t focus = 0, luma = 0, count = 0;
};

// One frame's AF result with its effective windows (§9.9.9).
struct af_payload {
   std::array<af_window, 16> win{};
   std::array<std::array<std::uint32_t, 4>, 16> rects{};
   std::uint32_t mask = 0, mode = 0, w = 0, h = 0;
   bool overflow = false;

   // Effective rectangles: the uniform grid z = floor(y*4/H)*4 + floor(x*4/W)
   // with all sixteen zones in mode 0, the enabled rectangles in mode 1;
   // disabled windows are empty.
   void restart(const af_config &c, std::uint32_t width, std::uint32_t height) {
      *this = af_payload{};
      w = width;
      h = height;
      mode = c.mode;
      auto cut = [](std::uint32_t d, std::uint32_t k) { return (k * d + 3u) / 4u; };
      for (unsigned z = 0; z < 16; ++z) {
         if (c.mode == 0) {
            rects[z] = {cut(w, z % 4), cut(h, z / 4), cut(w, z % 4 + 1), cut(h, z / 4 + 1)};
         } else if (c.mask & (1u << z)) {
            rects[z] = c.rects[z];
         }
      }
      mask = c.mode == 0 ? 0xFFFFu : c.mask & 0xFFFFu;
   }
};

// ---- AWB -----------------------------------------------------------------------

struct awb_ellipse {
   std::uint32_t rg = 0x100, bg = 0x100, a = 0x4000, c = 0x4000, r2 = 0x10000;
   std::int32_t b = 0;
};

struct awb_config {
   bool en = false;
   std::uint32_t nx = 0, ny = 0;
   std::uint32_t under = 0, sat = 4095;
   // Cell ROI, component limits and colour regions (§6.22.3.9; profile mode).
   bool roi_en = false, color_en = false;
   std::uint32_t mask = 0;
   std::array<std::uint32_t, 4> roi{};  // x0, y0, x1, y1 in cells
   std::array<std::array<std::uint32_t, 2>, 3> limits{{{0, 4095}, {0, 4095}, {0, 4095}}};  // R, G, B
   std::uint32_t min_g = 1;
   std::array<awb_ellipse, 8> ellipses{};
};

// AWB global results.
struct awb_result {
   std::uint64_t r = 0, g = 0, b = 0;  // 35-bit
   std::uint32_t n = 0;                // 23-bit
};

// AWB zone entries for the grid captured at admission.
struct awb_memory {
   std::vector<std::array<std::uint64_t, 3>> zsum;
   std::vector<std::uint32_t> zcnt;
   std::uint32_t nx = 0, ny = 0;

   void restart(const awb_config &c) {
      nx = c.nx;
      ny = c.ny;
      const std::size_t n = std::size_t{nx} * ny;
      zsum.assign(n, {});
      zcnt.assign(n, 0);
   }
};

// A complete AWB result: zones and global snapshot.
struct awb_payload {
   awb_memory mem;
   awb_result global;
};

// ---- result banks --------------------------------------------------------------

// Identity and captured geometry published with a result (§9.8, §9.9.9).
struct stat_meta {
   std::uint32_t frame_id = 0, context_id = 0, profile_id = 0;
   std::uint32_t source = 0, depth = 0;  // AEC: tap and native input depth
   std::uint32_t w = 0, h = 0;           // active image
   std::uint32_t nx = 0, ny = 0, zw = 0, zh = 0;
};

constexpr std::uint64_t stats_settled = std::numeric_limits<std::uint64_t>::max();

// Two banks of one engine. `stamp` is the edge of the last state change: a
// bank freed or published on an edge is eligible from the following edge
// (MAS Table 9-198).
template <class Payload>
struct result_banks {
   enum class state { free, filling, ready, leased };
   struct bank {
      state st = state::free;
      Payload payload{};
      stat_meta meta{};
      std::uint64_t stamp = stats_settled;
   };
   std::array<bank, 2> banks{};
   std::deque<unsigned> ready;  // completion order
   int filling = -1, leased = -1;
   std::uint64_t lease_cycle = 0;
   std::uint32_t drops = 0;
   std::uint64_t drop_cycle = stats_settled;  // edge of the latest drop
   std::uint32_t drops_on_edge = 0;           // drops counted on that edge
   bool snap_valid = false;                   // DATA_LO snapshot of the high word
   std::uint32_t snap_hi = 0;

   void clear() { *this = result_banks{}; }
   bool free_at(unsigned i, std::uint64_t cycle) const {
      return banks[i].st == state::free && (banks[i].stamp == stats_settled || banks[i].stamp < cycle);
   }
   // Reserve a FREE bank (bank 0 first); false if none.
   bool admit(const stat_meta &m, std::uint64_t cycle) {
      filling = -1;
      for (unsigned i = 0; i < banks.size(); ++i) {
         if (free_at(i, cycle)) {
            banks[i].st = state::filling;
            banks[i].stamp = cycle;
            banks[i].meta = m;
            filling = static_cast<int>(i);
            return true;
         }
      }
      return false;
   }
   // One lost measurement: saturating count; drops of the latest edge are
   // remembered so a W1C on that edge leaves them (new event wins).
   void drop(std::uint64_t cycle) {
      if (drop_cycle != cycle) {
         drop_cycle = cycle;
         drops_on_edge = 0;
      }
      ++drops_on_edge;
      if (drops != std::numeric_limits<std::uint32_t>::max()) {
         ++drops;
      }
   }
   void clear_drops(std::uint64_t cycle) { drops = drop_cycle == cycle ? drops_on_edge : 0u; }
   void publish(const Payload &p, std::uint64_t cycle) {
      bank &b = banks[static_cast<unsigned>(filling)];
      b.payload = p;
      b.st = state::ready;
      b.stamp = cycle;
      ready.push_back(static_cast<unsigned>(filling));
      filling = -1;
   }
   // The measurement in flight frees its bank. In silicon the next SOF
   // reaches the engine some clocks after the abort; this loosely-timed model
   // can place both on one edge, so the bank is eligible at once (U2-A4).
   void abort() {
      if (filling >= 0) {
         banks[static_cast<unsigned>(filling)].st = state::free;
         banks[static_cast<unsigned>(filling)].stamp = stats_settled;
         filling = -1;
      }
   }
   void release(std::uint64_t cycle) {
      banks[static_cast<unsigned>(leased)].st = state::free;
      banks[static_cast<unsigned>(leased)].stamp = cycle;
      leased = -1;
      snap_valid = false;
   }
   // The oldest READY bank published before this edge; false if none.
   bool acquire(std::uint64_t cycle) {
      if (ready.empty() || banks[ready.front()].stamp >= cycle) {
         return false;
      }
      leased = static_cast<int>(ready.front());
      ready.pop_front();
      banks[static_cast<unsigned>(leased)].st = state::leased;
      banks[static_cast<unsigned>(leased)].stamp = cycle;
      lease_cycle = cycle;
      return true;
   }
   const bank *lease() const { return leased >= 0 ? &banks[static_cast<unsigned>(leased)] : nullptr; }
};

}  // namespace cdc::components::fx1_isp
