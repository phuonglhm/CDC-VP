// SPDX-License-Identifier: Apache-2.0
// EE resident profiles through the control unit and the pipeline (MAS
// §6.18.4.1, §9.2; milestone U3). Differential checks: a frame run with a
// disturbance is compared with the same frame undisturbed, or with a frame
// run on profile 0 programmed with the same content.
// - three profiles with different content give three different pictures, each
//   equal to the same content loaded into profile 0;
// - loading a profile that is not in use, during a frame, leaves the frame
//   unchanged;
// - PROFILE_SEL and the active profile's scalars written during a frame apply
//   from the next frame (no mixed frame);
// - a table write to the active profile during a frame reaches the rows
//   processed after it (live tables, tearing) and not the rows before; it
//   clears the resident's confirmation, so the next frame passes through until
//   it is confirmed again (MAS v1.07 §9.9.1);
// - PROFILE_SEL code 3 selects profile 0.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "control/control_unit.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "pipeline/isp_pipeline.h"

using namespace cdc::components::fx1_isp;

namespace {

constexpr std::uint32_t W = 64, H = 48;

struct rig {
   control_unit cu;
   pipe::isp_pipeline p;
   pipe::pipeline_config cfg;
   std::uint64_t cycle = 1;
   std::vector<std::uint16_t> raw;
   std::vector<std::vector<std::uint8_t>> y;  // output luma rows of the last frame

   rig() : raw(std::size_t{W} * H) {
      for (std::uint32_t r = 0; r < H; ++r) {
         for (std::uint32_t x = 0; x < W; ++x) {  // texture with edges at every scale
            raw[std::size_t{r} * W + x] = static_cast<std::uint16_t>(
                (600 + (x * 97 + r * 61) % 1800 + ((x / 4 + r / 4) % 2) * 900) & 0xFFF);
         }
      }
      wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, W);
      wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, H);
   }
   void wr(std::uint32_t off, std::uint32_t v) { cu.csr_write(off, v, 0xFFFFFFFFu, cycle++); }
   void frame(std::uint32_t mid_row = 0, const std::function<void()> &mid = nullptr) {
      cu.accepted_sof(cycle++);
      cfg = pipe::snapshot_config(cu);
      y.clear();
      p.begin(cfg, W, H, [this](std::vector<std::uint8_t> &&r) { y.push_back(std::move(r)); },
              [](std::vector<std::uint8_t> &&) {});
      for (std::uint32_t r = 0; r < H; ++r) {
         if (mid && r == mid_row) {
            mid();
         }
         p.push(&raw[std::size_t{r} * W]);
         ++cycle;
      }
      p.finish();
      cu.advance_to(cycle++);
      pipe::end_of_frame(cu, cfg, p.stats());
      cu.frame_retired();  // the frame has left: the pipeline is idle again (V107-3)
   }
   // A distinct EE profile k: scalars and the four tables.
   void load(unsigned dest, unsigned k) {
      wr(FX1_ISP_EE_CTRL_OFFSET, (cu.csr_read(FX1_ISP_EE_CTRL_OFFSET) & ~FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK) |
                                     (dest << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT));
      wr(FX1_ISP_EE_ALPHA_OFFSET, 0x2000 + 0x2000 * k);
      wr(FX1_ISP_EE_BETA_OFFSET, 0x6000 - 0x1800 * k);
      wr(FX1_ISP_EE_CLAMP_OFFSET, ((20 + 10 * k) << 8) | (12 + 14 * k));
      wr(FX1_ISP_EE_FEATURE_EN_OFFSET, k == 0 ? 0u : k == 1 ? 1u : 3u);
      for (unsigned bank = 0; bank < 4; ++bank) {
         wr(FX1_ISP_EE_LUT_CTRL_OFFSET, bank << FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT);
         wr(FX1_ISP_EE_LUT_ADDR_OFFSET, 0);
         for (unsigned i = 0; i < control_unit::ee_bank_entries(bank); ++i) {
            wr(FX1_ISP_EE_LUT_WDATA_OFFSET, 0x4000 + ((i * (37 + 11 * k) + bank * 101 + k * 977) % 0x4000));
         }
      }
      confirm();
   }
   // PROFILE_LOAD_CONFIRM of the write resident with EE disabled (MAS v1.07
   // §9.9.1); refused (the resident stays invalid) while a frame is in flight.
   void confirm() {
      const std::uint32_t c = cu.csr_read(FX1_ISP_EE_CTRL_OFFSET);
      wr(FX1_ISP_EE_CTRL_OFFSET, c & ~FX1_ISP_EE_CTRL_EN_MASK);
      wr(FX1_ISP_EE_LUT_CTRL_OFFSET, FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK);
      wr(FX1_ISP_EE_CTRL_OFFSET, c);
   }
   void select(unsigned code) {
      wr(FX1_ISP_EE_CTRL_OFFSET, (cu.csr_read(FX1_ISP_EE_CTRL_OFFSET) & ~FX1_ISP_EE_CTRL_PROFILE_SEL_MASK) |
                                     FX1_ISP_EE_CTRL_EN_MASK | (code << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT));
   }
};

using rows = std::vector<std::vector<std::uint8_t>>;

// Output rows that differ between two frames.
std::vector<std::uint32_t> differing_rows(const rows &a, const rows &b) {
   std::vector<std::uint32_t> d;
   for (std::uint32_t r = 0; r < std::min(a.size(), b.size()); ++r) {
      if (a[r] != b[r]) {
         d.push_back(r);
      }
   }
   return d;
}

// The same content in profile 0 (reference picture for profile content k).
rows on_profile0(unsigned k) {
   rig r;
   r.load(0, k);
   r.select(0);
   r.frame();
   return r.y;
}

void test_three_profiles() {
   rig r;
   for (unsigned k = 0; k < 3; ++k) {
      r.load(k, k);
   }
   std::vector<rows> pictures;
   for (unsigned k = 0; k < 3; ++k) {
      r.select(k);
      r.frame();
      const rows ref = on_profile0(k);
      FX1_CHECK_EQ_CTX(differing_rows(r.y, ref).size(), 0u, "profile " + std::to_string(k) + " = its content in profile 0");
      FX1_CHECK_EQ_CTX(r.y.size(), H, "rows");
      pictures.push_back(r.y);
   }
   FX1_CHECK(pictures[0] != pictures[1] && pictures[1] != pictures[2] && pictures[0] != pictures[2]);
   // Code 3 selects profile 0.
   r.select(3);
   r.frame();
   FX1_CHECK(r.y == pictures[0]);
}

void test_frame_coherence() {
   // Reference: profile 1 active, nothing written during the frame.
   rig base;
   base.load(1, 1);
   base.select(1);
   base.frame();
   // Loading the inactive profile 2 during the frame leaves it unchanged.
   rig a;
   a.load(1, 1);
   a.select(1);
   a.frame(H / 2, [&] { a.load(2, 2); });
   FX1_CHECK_EQ_CTX(differing_rows(a.y, base.y).size(), 0u, "inactive load during the frame");
   // Clearing EN during the frame: the frame stays enhanced (EN sampled at SOF).
   rig e;
   e.load(1, 1);
   e.select(1);
   e.frame(H / 2, [&] { e.wr(FX1_ISP_EE_CTRL_OFFSET, e.cu.csr_read(FX1_ISP_EE_CTRL_OFFSET) & ~FX1_ISP_EE_CTRL_EN_MASK); });
   FX1_CHECK_EQ_CTX(differing_rows(e.y, base.y).size(), 0u, "EN cleared during the frame");
   e.frame();
   FX1_CHECK(!differing_rows(e.y, base.y).empty());  // the next frame bypasses EE
   // PROFILE_SEL and the active profile's scalars during the frame: next frame.
   rig b;
   b.load(1, 1);
   b.load(2, 2);
   b.select(1);
   b.frame(H / 2, [&] {
      b.select(2);
      b.wr(FX1_ISP_EE_CTRL_OFFSET, b.cu.csr_read(FX1_ISP_EE_CTRL_OFFSET) & ~FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK);
      b.wr(FX1_ISP_EE_CTRL_OFFSET, b.cu.csr_read(FX1_ISP_EE_CTRL_OFFSET) | (1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT));
      b.wr(FX1_ISP_EE_ALPHA_OFFSET, 0x8000);  // profile 1 scalar
      b.wr(FX1_ISP_EE_CLAMP_OFFSET, 0xFFFF);
   });
   FX1_CHECK_EQ_CTX(differing_rows(b.y, base.y).size(), 0u, "selector and scalars during the frame");
   b.frame();
   const rows ref2 = on_profile0(2);
   FX1_CHECK_EQ_CTX(differing_rows(b.y, ref2).size(), 0u, "the next frame uses profile 2");
}

void test_live_table_tearing() {
   rig base;
   base.load(1, 1);
   base.select(1);
   base.frame();
   // Rewrite profile 1's luma table while it is in use: the rows the EE
   // processes after the write change, the rows before do not.
   rig t;
   t.load(1, 1);
   t.select(1);
   const std::uint32_t at = H / 2;
   std::size_t rows_before = 0;
   t.frame(at, [&] {
      rows_before = t.y.size();
      t.wr(FX1_ISP_EE_CTRL_OFFSET, t.cu.csr_read(FX1_ISP_EE_CTRL_OFFSET) & ~FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK);
      t.wr(FX1_ISP_EE_CTRL_OFFSET, t.cu.csr_read(FX1_ISP_EE_CTRL_OFFSET) | (1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT));
      t.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, 0);
      t.wr(FX1_ISP_EE_LUT_ADDR_OFFSET, 0);
      for (unsigned i = 0; i < 64; ++i) {
         t.wr(FX1_ISP_EE_LUT_WDATA_OFFSET, 0x8000);  // luma gain 1.0 everywhere
      }
   });
   const auto d = differing_rows(t.y, base.y);
   FX1_CHECK(!d.empty());
   FX1_CHECK(rows_before > 0);
   for (std::uint32_t r : d) {
      FX1_CHECK_EQ_CTX(r >= rows_before ? 1 : 0, 1, "row " + std::to_string(r) + " changed before the write");
   }
   // v1.07: the write cleared the resident's confirmation; the next frame
   // passes through until it is confirmed again (validity is captured at SOF).
   t.frame();
   FX1_CHECK_EQ_CTX(differing_rows(t.y, base.y).size() > 0 ? 1 : 0, 1, "unconfirmed resident after the write");
   t.wr(FX1_ISP_EE_CTRL_OFFSET, t.cu.csr_read(FX1_ISP_EE_CTRL_OFFSET) | (1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT));
   t.confirm();
   // Confirmed again: the next frame uses the new table everywhere, equal to
   // a frame loaded that way from the start.
   t.frame();
   rig ref;
   ref.load(1, 1);
   ref.wr(FX1_ISP_EE_LUT_CTRL_OFFSET, 0);
   ref.wr(FX1_ISP_EE_LUT_ADDR_OFFSET, 0);
   for (unsigned i = 0; i < 64; ++i) {
      ref.wr(FX1_ISP_EE_LUT_WDATA_OFFSET, 0x8000);
   }
   ref.confirm();
   ref.select(1);
   ref.frame();
   FX1_CHECK_EQ_CTX(differing_rows(t.y, ref.y).size(), 0u, "next frame with the rewritten table");
}

}  // namespace

int main() {
   test_three_profiles();
   test_frame_coherence();
   test_live_table_tearing();
   return fx1_test::summary("fx1_isp_test_ee_profiles");
}
