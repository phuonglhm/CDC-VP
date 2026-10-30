// SPDX-License-Identifier: Apache-2.0
// Directed tests of the CSR/MAS v1.07 statistics rules (MAS §6.21-6.23, §9.8,
// §9.9.9, Table 9-198) that the end-of-frame vectors cannot reach: AEC/AF
// result banks and leases (ACQUIRE/RELEASE, combined commands, exhaustion,
// same-edge rules, DATA_LO/HI snapshot, INVALID/NO_LEASE), narrow windows as
// projections of the lease, AE taps, clip/threshold equality, bin edges,
// zone coverage and illegal sets, the weighted histogram, AF windows and
// noise, leased geometry, resets, SOF latching, the AWB bank protocol (U2)
// on post-BPC cells, and BPC counters.
// Expected values are analytic (flat per-channel Bayer field, MAS formulas
// written out here) or differential (same frame with another setting).

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
constexpr std::uint16_t flat_values[4] = {100, 1000, 3300, 4000};  // R, Gr, Gb, B
constexpr std::uint32_t flat_sum = (100u + 1000u + 3300u + 4000u) * (W * H / 4);
constexpr std::uint32_t e_invalid = FX1_ISP_TUNE_ERROR_INVALID_MASK, e_no_lease = FX1_ISP_TUNE_ERROR_NO_LEASE_MASK;
constexpr std::uint32_t lacq = FX1_ISP_TUNE_AEC_CMD_ACQUIRE_MASK, lrel = FX1_ISP_TUNE_AEC_CMD_RELEASE_MASK;

// A frame-level stand-in for the engine: accepted SOF, snapshot, rows, EOF.
struct rig {
   control_unit cu;
   pipe::isp_pipeline p;
   pipe::pipeline_config cfg;
   std::uint64_t cycle = 1;
   std::vector<std::uint16_t> raw;

   explicit rig(bool flat_luma = false) : raw(std::size_t{W} * H) {
      for (std::uint32_t y = 0; y < H; ++y) {
         for (std::uint32_t x = 0; x < W; ++x) {
            raw[std::size_t{y} * W + x] = flat_luma ? 2000 : flat_values[((y & 1u) << 1) | (x & 1u)];
         }
      }
      wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, W);
      wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, H);
   }
   void wr(std::uint32_t off, std::uint32_t v) { cu.csr_write(off, v, 0xFFFFFFFFu, cycle++); }
   std::uint32_t rd(std::uint32_t off) const { return cu.csr_read(off); }
   std::uint32_t brd(std::uint32_t off) { return cu.csr_bus_read(off, cycle++); }  // software read
   std::uint32_t rd_sel(std::uint32_t sel_off, std::uint32_t sel, std::uint32_t off) {
      wr(sel_off, sel);
      return rd(off);
   }
   std::uint32_t err() {  // TUNE_ERROR, then W1C
      const std::uint32_t e = rd(FX1_ISP_TUNE_ERROR_OFFSET);
      wr(FX1_ISP_TUNE_ERROR_OFFSET, e);
      return e;
   }
   void sof() {
      cu.accepted_sof(cycle++);
      cfg = pipe::snapshot_config(cu);
      p.begin(cfg, W, H, [](std::vector<std::uint8_t> &&) {}, [](std::vector<std::uint8_t> &&) {});
   }
   void rows(std::uint32_t from, std::uint32_t to) {
      for (std::uint32_t y = from; y < to; ++y) {
         p.push(&raw[std::size_t{y} * W]);
         ++cycle;
      }
   }
   void eof() {
      p.finish();
      cu.advance_to(cycle++);  // the publication edge
      pipe::end_of_frame(cu, cfg, p.stats());
   }
   void frame(const std::function<void()> &mid = nullptr) {
      sof();
      rows(0, H / 2);
      if (mid) {
         mid();
      }
      rows(H / 2, H);
      eof();
   }
   // Profile mode with bank 0 holding `body`'s writes (MAS §9.7).
   void profile(const std::function<void()> &body) {
      wr(FX1_ISP_TUNE_MODE_OFFSET, FX1_ISP_TUNE_MODE_ENABLE_MASK);
      wr(FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_INIT_MASK);
      body();
      wr(FX1_ISP_TUNE_PROFILE_ID_OFFSET, 0x77);
      wr(FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_SEAL_MASK);
      wr(FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_APPLY_MASK);
      ++cycle;
   }
   void aec(std::uint32_t nx, std::uint32_t ny, std::uint32_t zw, std::uint32_t zh, std::uint32_t ctrl) {
      wr(FX1_ISP_AEC_ZONE_CFG_OFFSET, nx | (ny << 8));
      wr(FX1_ISP_AEC_ZONE_SIZE_OFFSET, zw | (zh << 16));
      wr(FX1_ISP_AEC_CTRL_OFFSET, ctrl);
   }
   // AEC/AF leases (MAS §9.8).
   void aec_cmd(std::uint32_t bits) { wr(FX1_ISP_TUNE_AEC_CMD_OFFSET, bits); }
   void af_cmd(std::uint32_t bits) { wr(FX1_ISP_TUNE_AF_CMD_OFFSET, bits); }
   std::uint32_t aec_status() const { return rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET); }
   std::uint32_t af_status() const { return rd(FX1_ISP_TUNE_AF_STATUS_OFFSET); }
   std::uint32_t aec_ready() const { return (aec_status() & FX1_ISP_TUNE_AEC_STATUS_READY_COUNT_MASK) >> 1; }
   bool aec_leased() const { return aec_status() & FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK; }
   std::uint64_t wide(std::uint32_t index_off, std::uint32_t lo_off, std::uint32_t i) {
      wr(index_off, i);
      const std::uint64_t lo = brd(lo_off);
      return lo | (std::uint64_t{brd(lo_off + 4)} << 32);  // DATA_HI follows DATA_LO
   }
   std::uint64_t aec_wide(std::uint32_t i) { return wide(FX1_ISP_TUNE_AEC_INDEX_OFFSET, FX1_ISP_TUNE_AEC_DATA_LO_OFFSET, i); }
   std::uint64_t af_wide(std::uint32_t i) { return wide(FX1_ISP_TUNE_AF_INDEX_OFFSET, FX1_ISP_TUNE_AF_DATA_LO_OFFSET, i); }
   std::uint32_t aec_zone(std::uint32_t z, std::uint32_t sel, std::uint32_t off) {
      wr(FX1_ISP_AEC_ZONE_ADDR_OFFSET, z);
      return rd_sel(FX1_ISP_AEC_CHANNEL_SEL_OFFSET, sel, off);
   }
   std::uint32_t aec_global_sum(std::uint32_t sel) {
      return rd_sel(FX1_ISP_AEC_CHANNEL_SEL_OFFSET, sel, FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET);
   }
   std::uint32_t hist(std::uint32_t b) { return rd_sel(FX1_ISP_AEC_HIST_ADDR_OFFSET, b, FX1_ISP_AEC_HIST_DATA_OFFSET); }
   std::uint32_t af(std::uint32_t z) { return rd_sel(FX1_ISP_AF_STAT_ADDR_OFFSET, z, FX1_ISP_AF_STAT_DATA_OFFSET); }
   // Acquire the oldest AEC/AF result, read it with `f`, release.
   template <class F>
   void with_aec(F f) {
      aec_cmd(lacq);
      f();
      aec_cmd(lrel);
   }
   template <class F>
   void with_af(F f) {
      af_cmd(lacq);
      f();
      af_cmd(lrel);
   }
   // AWB ownership (MAS Table 6-135).
   void awb_cmd(std::uint32_t bits) { wr(FX1_ISP_AWB_RESULT_CTRL_OFFSET, bits); }
   void acquire() { awb_cmd(FX1_ISP_AWB_RESULT_CTRL_ACQUIRE_MASK); }
   void release() { awb_cmd(FX1_ISP_AWB_RESULT_CTRL_RELEASE_MASK); }
   std::uint32_t awb_status() const { return rd(FX1_ISP_AWB_RESULT_STATUS_OFFSET); }
   std::uint32_t ready_count() const { return awb_status() & FX1_ISP_AWB_RESULT_STATUS_READY_COUNT_MASK; }
   bool leased() const { return (awb_status() & FX1_ISP_AWB_RESULT_STATUS_READ_VALID_MASK) != 0; }
   std::uint32_t drops() const { return rd(FX1_ISP_AWB_DROP_COUNT_OFFSET); }
   std::uint32_t awb_zone_count(std::uint32_t z) { return rd_sel(FX1_ISP_AWB_ZONE_ADDR_OFFSET, z, FX1_ISP_AWB_ZONE_COUNT_OFFSET); }
   // Acquire, read FRAME_ID, release: the identity of the oldest READY result.
   std::uint32_t take_awb_frame_id() {
      acquire();
      const std::uint32_t id = rd(FX1_ISP_AWB_FRAME_ID_OFFSET);
      release();
      return id;
   }
   std::uint32_t take_aec_frame_id() {
      std::uint32_t id = 0;
      with_aec([&] { id = rd(FX1_ISP_AEC_FRAME_ID_OFFSET); });
      return id;
   }
};

constexpr std::uint32_t awb_1x1 = 1 | (1 << 1) | (1 << 8);  // enabled, 1 x 1 grid
constexpr std::uint32_t overrun = FX1_ISP_AWB_RESULT_STATUS_OVERRUN_MASK;
constexpr std::uint32_t write_busy = FX1_ISP_AWB_RESULT_STATUS_WRITE_BUSY_MASK;

constexpr std::uint32_t aec_commit = FX1_ISP_AEC_CTRL_EN_MASK | FX1_ISP_AEC_CTRL_COMMIT_MASK;
constexpr std::uint32_t aec_done = FX1_ISP_AEC_STATUS_STAT_DONE_MASK;
constexpr std::uint32_t af_done = FX1_ISP_AF_STATUS_FRAME_DONE_MASK;
constexpr std::uint32_t af_valid = FX1_ISP_AF_STATUS_SCORE_VALID_MASK;

// Flat field, 16 x 12 zones of 4 x 4: every zone holds 4 samples per channel.
void check_flat_aec(rig &r, std::uint32_t zones_x_total) {
   r.with_aec([&] {
      FX1_CHECK_EQ(r.aec_global_sum(0), flat_sum);
      FX1_CHECK_EQ(r.aec_zone(0, 0, FX1_ISP_AEC_ZONE_SUM_OFFSET),
                   zones_x_total == 1 ? flat_sum : (100u + 1000u + 3300u + 4000u) * 4);
   });
}

// ALG-AEC-06 / #296 with v1.07 sets: shadow set, commit transferred at the
// next SOF, TUNE_AEC_SOURCE joins the commit in legacy mode (Table 9-195).
void test_aec_commit() {
   rig r;
   r.aec(16, 12, 4, 4, FX1_ISP_AEC_CTRL_EN_MASK);  // en without commit: no effect
   r.frame();
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_STATUS_OFFSET), 0);
   FX1_CHECK_EQ(r.aec_ready(), 0);

   r.wr(FX1_ISP_AEC_CTRL_OFFSET, aec_commit);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_CTRL_OFFSET), aec_commit);  // pending reads 1
   r.wr(FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_EN_MASK);  // writing 0 to a W1S bit: still pending
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_CTRL_OFFSET), aec_commit);
   r.frame();
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_CTRL_OFFSET), FX1_ISP_AEC_CTRL_EN_MASK);  // self-cleared at SOF
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_STATUS_OFFSET), aec_done);
   FX1_CHECK_EQ(r.take_aec_frame_id(), 2);  // frames started, not frames measured
   r.frame();
   check_flat_aec(r, 16);

   r.wr(FX1_ISP_AEC_ZONE_CFG_OFFSET, 1 | (1 << 8));  // new grid, no commit: kept
   r.wr(FX1_ISP_AEC_ZONE_SIZE_OFFSET, W | (H << 16));
   r.frame();
   check_flat_aec(r, 16);
   r.frame([&] { r.wr(FX1_ISP_AEC_CTRL_OFFSET, aec_commit); });  // commit mid-frame: next SOF
   check_flat_aec(r, 16);
   r.frame();
   check_flat_aec(r, 1);  // 1 x 1 grid: the single zone absorbs the frame

   // The source joins the commit: DG source with a WB gain on R changes the
   // sum only after the commit that carries it.
   r.wr(FX1_ISP_WB_CTRL_OFFSET, 1);
   r.wr(FX1_ISP_WB_GAIN_R_OFFSET, 0x200);
   r.wr(FX1_ISP_TUNE_AEC_SOURCE_OFFSET, 1);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_SOURCE_OFFSET), 1);  // live register in legacy mode
   r.frame();
   check_flat_aec(r, 1);  // still the BPC tap
   r.wr(FX1_ISP_AEC_CTRL_OFFSET, aec_commit);
   r.frame();
   r.with_aec([&] {
      FX1_CHECK_EQ(r.aec_global_sum(0), flat_sum + 100u * (W * H / 4));  // R doubled at the DG tap
      FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_RESULT_META_OFFSET), 0x80000011u);  // lease, RAW12 domain, DG
   });

   // A commit of en = 0 lets the frame in progress complete and publish.
   r.frame([&] { r.wr(FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_COMMIT_MASK); });
   FX1_CHECK_EQ(r.take_aec_frame_id(), 9);
   r.wr(FX1_ISP_AEC_STATUS_OFFSET, aec_done);  // W1C
   r.frame();
   FX1_CHECK_EQ(r.aec_ready(), 0);  // disabled: nothing published, nothing dropped
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_STATUS_OFFSET), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_ERROR_MASK, 0);  // never set
}

// MAS §9.8, Table 9-198: AEC lease protocol, wide port and narrow windows.
void test_aec_lease() {
   rig r;
   r.aec(16, 12, 4, 4, aec_commit);
   // No lease: every result reads zero and a software read sets NO_LEASE.
   FX1_CHECK_EQ(r.brd(FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET), 0);
   FX1_CHECK_EQ(r.err(), e_no_lease);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_FRAME_ID_OFFSET), 0);  // a debug read has no side effect
   FX1_CHECK_EQ(r.err(), 0);
   for (std::uint32_t off : {FX1_ISP_TUNE_AEC_DATA_LO_OFFSET, FX1_ISP_TUNE_AEC_DATA_HI_OFFSET, FX1_ISP_TUNE_AEC_FRAME_OFFSET,
                             FX1_ISP_TUNE_AEC_PROFILE_OFFSET, FX1_ISP_TUNE_AEC_RESULT_META_OFFSET,
                             FX1_ISP_TUNE_AE_FRAME_SIZE_OFFSET, FX1_ISP_TUNE_AE_GRID_OFFSET, FX1_ISP_TUNE_AE_ZONE_SIZE_OFFSET,
                             FX1_ISP_AEC_HIST_DATA_OFFSET, FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET}) {
      FX1_CHECK_EQ(r.brd(off), 0);
      FX1_CHECK_EQ_CTX(r.err(), e_no_lease, std::to_string(off));
   }
   r.aec_cmd(lrel);  // RELEASE without a lease
   FX1_CHECK_EQ(r.err(), e_no_lease);
   r.aec_cmd(lacq);  // nothing READY: no lease, no error
   FX1_CHECK(!r.aec_leased());
   FX1_CHECK_EQ(r.err(), 0);

   r.frame();  // frame 1 -> bank 0
   r.frame();  // frame 2 -> bank 1
   FX1_CHECK_EQ(r.aec_ready(), 2);
   r.frame();  // frame 3: no FREE bank, dropped
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), 1);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET) & FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK, 0);
   // ACQUIRE: readable from the next edge.
   const std::uint64_t c = r.cycle++;
   r.cu.csr_write(FX1_ISP_TUNE_AEC_CMD_OFFSET, lacq, 0xFFFFFFFFu, c);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AEC_FRAME_ID_OFFSET, c), 0);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AEC_FRAME_ID_OFFSET, c + 1), 1);
   // LEASE_VALID rises with readability, not on the ACQUIRE edge (V107-D37).
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_TUNE_AEC_STATUS_OFFSET, c) & FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK, 0);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_TUNE_AEC_STATUS_OFFSET, c + 1) & FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK,
                FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK);
   FX1_CHECK_EQ(r.aec_status(), FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK | (1u << 1));  // one READY left
   r.aec_cmd(lacq);  // duplicate ACQUIRE: NO_LEASE, the lease unchanged
   FX1_CHECK_EQ(r.err(), e_no_lease);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_FRAME_OFFSET), 1);
   // Leased identity and geometry (§9.9.9), stable against the live set.
   r.aec(1, 1, W, H, aec_commit);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AE_FRAME_SIZE_OFFSET), W | (H << 16));
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AE_GRID_OFFSET), 16u | (12u << 16));
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AE_ZONE_SIZE_OFFSET), 4u | (4u << 16));
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_RESULT_META_OFFSET), 0x80000010u);  // BPC, RAW12 domain, RAW12 input
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_PROFILE_OFFSET), 0);  // legacy mode
   // Wide port (Table 9-193): sum, count, OE, UE, min, max, weighted, flags.
   FX1_CHECK_EQ(r.aec_wide(0), flat_sum);
   FX1_CHECK_EQ(r.aec_wide(1), W * H);
   FX1_CHECK_EQ(r.aec_wide(2), W * H / 4);  // B = 4000 > OE 3744
   FX1_CHECK_EQ(r.aec_wide(3), W * H / 4);  // R = 100 < UE 272
   FX1_CHECK_EQ(r.aec_wide(4), 100);
   FX1_CHECK_EQ(r.aec_wide(5), 4000);
   FX1_CHECK_EQ(r.aec_wide(6), 0);  // weighting disabled (legacy mode)
   FX1_CHECK_EQ(r.aec_wide(7), 0);
   FX1_CHECK_EQ(r.aec_wide(8), 0);
   for (std::uint32_t i : {0x10u, 0x11u, 0x12u}) {
      FX1_CHECK_EQ(r.aec_wide(i), 0);
   }
   FX1_CHECK_EQ(r.aec_wide(0x100 + (1000 >> 6)), W * H / 4);  // every sample is binned
   FX1_CHECK_EQ(r.aec_wide(0x100 + (100 >> 6)), W * H / 4);
   FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 5 + 0), (100u + 1000u + 3300u + 4000u) * 4);
   FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 5 + 1), 16);
   FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 5 + 4), 100);
   FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 5 + 5), 4000);
   FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 5 + 6), 0);       // padding: zero, no error
   FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 767 + 0), 0);     // beyond the 16 x 12 grid (V107-A23)
   FX1_CHECK_EQ(r.err(), 0);
   for (std::uint32_t i : {0x9u, 0x13u, 0x180u, 0x200u + 8 * 768}) {  // reserved
      FX1_CHECK_EQ(r.aec_wide(i), 0);
      FX1_CHECK_EQ_CTX(r.err(), e_invalid, std::to_string(i));
   }
   // DATA_LO snapshots the high word; INDEX write, RELEASE invalidate it.
   r.wr(FX1_ISP_TUNE_AEC_INDEX_OFFSET, 0);
   r.brd(FX1_ISP_TUNE_AEC_DATA_LO_OFFSET);
   r.wr(FX1_ISP_TUNE_AEC_INDEX_OFFSET, 1);
   FX1_CHECK_EQ(r.brd(FX1_ISP_TUNE_AEC_DATA_HI_OFFSET), 0);
   FX1_CHECK_EQ(r.err(), e_no_lease);
   // Narrow windows: selector 0 aggregate, 1..3 zero; zones as projections.
   FX1_CHECK_EQ(r.aec_global_sum(0), flat_sum);
   for (std::uint32_t sel = 1; sel < 4; ++sel) {
      FX1_CHECK_EQ(r.aec_global_sum(sel), 0);
      FX1_CHECK_EQ(r.aec_zone(0, sel, FX1_ISP_AEC_ZONE_SUM_OFFSET), 0);
   }
   FX1_CHECK_EQ(r.rd_sel(FX1_ISP_AEC_CHANNEL_SEL_OFFSET, 0, FX1_ISP_AEC_GLOBAL_COUNT_OFFSET), W * H);
   FX1_CHECK_EQ(r.aec_zone(5, 0, FX1_ISP_AEC_ZONE_COUNT_OFFSET), 16);
   FX1_CHECK_EQ(r.aec_zone(5, 0, FX1_ISP_AEC_ZONE_GREEN_OE_UE_OFFSET), (4u << 16) | 4u);
   FX1_CHECK_EQ(r.aec_zone(5, 0, FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET), (4000u << 12) | 100u);
   FX1_CHECK_EQ(r.aec_zone(16 * 12, 0, FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET), 0);  // beyond the grid
   FX1_CHECK_EQ(r.hist(3300 >> 6), W * H / 4);
   // Combined RELEASE + ACQUIRE: release first, then the oldest READY.
   r.aec_cmd(lrel | lacq);
   FX1_CHECK_EQ(r.err(), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_FRAME_OFFSET), 2);
   FX1_CHECK_EQ(r.aec_ready(), 0);  // the released data does not become READY again
   r.aec_cmd(lrel | lacq);  // nothing READY: released, no new lease
   FX1_CHECK(!r.aec_leased());
   FX1_CHECK_EQ(r.err(), 0);
   // DROPS: a nonzero W1C clears the whole word; a drop on that edge wins.
   r.wr(FX1_ISP_TUNE_AEC_DROPS_OFFSET, 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), 1);  // zero write: no effect
   r.wr(FX1_ISP_TUNE_AEC_DROPS_OFFSET, 1);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), 0);
   r.frame();  // 4 -> bank 0
   r.frame();  // 5 -> bank 1
   const std::uint64_t s = r.cycle++;
   r.cu.accepted_sof(s);  // 6: dropped
   r.cu.csr_write(FX1_ISP_TUNE_AEC_DROPS_OFFSET, 0xFFFFFFFFu, 0xFFFFFFFFu, s);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), 1);
   r.cu.stats_abort();
   // A bank published on an ACQUIRE edge is acquirable from the next edge;
   // one released on an SOF edge is eligible from the next edge.
   r.with_aec([] {});
   r.with_aec([] {});
   r.sof();
   r.rows(0, H);
   r.p.finish();
   const std::uint64_t e = r.cycle++;
   r.cu.advance_to(e);
   pipe::end_of_frame(r.cu, r.cfg, r.p.stats());
   r.cu.csr_write(FX1_ISP_TUNE_AEC_CMD_OFFSET, lacq, 0xFFFFFFFFu, e);
   FX1_CHECK(!r.aec_leased());
   r.aec_cmd(lacq);
   FX1_CHECK(r.aec_leased());
   r.frame();  // the other bank READY; the leased one is released on the next SOF edge
   const std::uint64_t s2 = r.cycle++;
   const std::uint32_t d0 = r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET);
   r.cu.csr_write(FX1_ISP_TUNE_AEC_CMD_OFFSET, lrel, 0xFFFFFFFFu, s2);
   r.cu.accepted_sof(s2);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), d0 + 1);
   r.cu.stats_abort();
   r.frame();
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), d0 + 1);
   FX1_CHECK_EQ(r.aec_ready(), 2);
   // A held lease survives later frames; TUNE_MODE cannot change meanwhile.
   r.aec_cmd(lacq);
   const std::uint32_t held = r.rd(FX1_ISP_TUNE_AEC_FRAME_OFFSET);
   r.frame();
   r.frame();
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_FRAME_OFFSET), held);
   FX1_CHECK_EQ(r.aec_wide(0), flat_sum);
   r.err();
   r.wr(FX1_ISP_TUNE_MODE_OFFSET, FX1_ISP_TUNE_MODE_ENABLE_MASK);
   FX1_CHECK_EQ(r.err(), FX1_ISP_TUNE_ERROR_MODE_MASK);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_MODE_OFFSET), 0);
}

// MAS §9.9.3: an AE or an AF lease alone blocks a TUNE_MODE change. The check
// in test_aec_lease runs with frames unretired, where the busy pipeline
// already rejects it; here every frame is retired, so only the lease can.
void test_tune_mode_with_lease_idle() {
   for (const bool af : {false, true}) {
      rig r;
      r.aec(16, 12, 4, 4, aec_commit);
      r.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
      r.frame();
      r.cu.frame_retired();
      FX1_CHECK(r.cu.pipeline_idle());
      af ? r.af_cmd(lacq) : r.aec_cmd(lacq);
      FX1_CHECK(af ? (r.af_status() & FX1_ISP_TUNE_AF_STATUS_LEASE_VALID_MASK) != 0 : r.aec_leased());
      r.err();
      r.wr(FX1_ISP_TUNE_MODE_OFFSET, FX1_ISP_TUNE_MODE_ENABLE_MASK);
      FX1_CHECK_EQ(r.err(), FX1_ISP_TUNE_ERROR_MODE_MASK);
      FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_MODE_OFFSET), 0);
      af ? r.af_cmd(lrel) : r.aec_cmd(lrel);
      r.wr(FX1_ISP_TUNE_MODE_OFFSET, FX1_ISP_TUNE_MODE_ENABLE_MASK);  // released: accepted
      FX1_CHECK_EQ(r.err(), 0);
      FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_MODE_OFFSET), FX1_ISP_TUNE_MODE_ENABLE_MASK);
   }
}

// MAS §6.21.3: equality at the clip and thresholds, bin edges, extrema, the
// last zone holding the remainder, illegal sets skipped and counted, the
// post-DG tap reflecting WB/DG while the BPC tap does not.
void test_aec_numeric() {
   rig r;
   // Columns of samples 63, 64, 271, 272, 273, 3743, 3744, 3745 repeated.
   const std::uint16_t v[8] = {63, 64, 271, 272, 273, 3743, 3744, 3745};
   for (std::uint32_t y = 0; y < H; ++y) {
      for (std::uint32_t x = 0; x < W; ++x) {
         r.raw[std::size_t{y} * W + x] = v[x % 8];
      }
   }
   // Clip [64, 3744] inclusive; UE 272 (<), OE 3744 (>); zones 3 x 2 of 30 x 30:
   // the last column and row hold the remainder (64 = 2*30 + 4; 48 = 30 + 18).
   r.wr(FX1_ISP_AEC_SAMPLE_CLIP_OFFSET, 64u | (3744u << 16));
   r.aec(3, 2, 30, 30, aec_commit);
   r.frame();
   const std::uint32_t per = W * H / 8;  // samples per value
   r.with_aec([&] {
      FX1_CHECK_EQ(r.aec_wide(1), 6 * per);  // 64..3744: six of the eight values
      FX1_CHECK_EQ(r.aec_wide(0), std::uint64_t{per} * (64 + 271 + 272 + 273 + 3743 + 3744));
      FX1_CHECK_EQ(r.aec_wide(3), 3 * per);  // 63, 64, 271 < 272
      FX1_CHECK_EQ(r.aec_wide(2), 1 * per);  // 3745 > 3744
      FX1_CHECK_EQ(r.aec_wide(4), 63);       // extrema over every sample, not clip-qualified
      FX1_CHECK_EQ(r.aec_wide(5), 3745);
      FX1_CHECK_EQ(r.aec_wide(0x100 + 0), per);      // 63 >> 6 = 0
      FX1_CHECK_EQ(r.aec_wide(0x100 + 1), per);      // 64 >> 6 = 1
      FX1_CHECK_EQ(r.aec_wide(0x100 + 4), 3 * per);  // 271, 272, 273
      FX1_CHECK_EQ(r.aec_wide(0x100 + 58), 3 * per); // 3743, 3744, 3745
      // Zone 2 (columns 60..63: values 3743..3745 at x = 61..63), zone 5 likewise.
      FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 2 + 5), 3745);
      FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 2 + 1), 3u * 30u);    // x = 60..63: 273, 3743, 3744 in the clip
      FX1_CHECK_EQ(r.aec_wide(0x200 + 8 * 5 + 1), 3u * 18u);
   });
   // Illegal sets skip the measurement and count a drop (§6.21.4).
   const struct {
      std::uint32_t nx, ny, zw, zh, clip, th;
      const char *what;
   } bad[] = {{3, 2, 20, 30, 0x0FFF0000u, 0x0EA00110u, "grid does not cover the width"},
              {3, 2, 32, 30, 0x0FFF0000u, 0x0EA00110u, "(NX-1)*WZ reaches W"},
              {33, 1, 2, 48, 0x0FFF0000u, 0x0EA00110u, "NX > 32"},
              {1, 1, W, H, 0x00100020u, 0x0EA00110u, "MIN > MAX"},
              {1, 1, W, H, 0x0FFF0000u, 0x01100110u, "UE = OE"}};
   for (const auto &b : bad) {
      rig q;
      q.wr(FX1_ISP_AEC_SAMPLE_CLIP_OFFSET, b.clip);
      q.wr(FX1_ISP_AEC_THRESH_OFFSET, b.th);
      q.aec(b.nx, b.ny, b.zw, b.zh, aec_commit);
      q.frame();
      FX1_CHECK_EQ_CTX(q.aec_ready(), 0u, b.what);
      FX1_CHECK_EQ_CTX(q.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), 1u, b.what);
      FX1_CHECK_EQ_CTX(q.rd(FX1_ISP_AEC_STATUS_OFFSET), 0u, b.what);
   }
   // Taps: WB/DG change the DG-source statistics only (§6.21.1).
   for (std::uint32_t source : {0u, 1u}) {
      rig a, b;
      for (rig *q : {&a, &b}) {
         q->wr(FX1_ISP_TUNE_AEC_SOURCE_OFFSET, source);
         q->aec(1, 1, W, H, aec_commit);
      }
      b.wr(FX1_ISP_DG_CTRL_OFFSET, 1);
      b.wr(FX1_ISP_DG_GAIN_OFFSET, 0x080);  // half
      a.frame();
      b.frame();
      std::uint64_t sa = 0, sb = 0;
      a.with_aec([&] { sa = a.aec_wide(0); });
      b.with_aec([&] { sb = b.aec_wide(0); });
      FX1_CHECK_EQ_CTX(sa == sb, source == 0, "source " + std::to_string(source));
   }
}

// MAS §6.21.3.4: the weighted histogram in profile mode: ROI lattice from
// the origin with the steps, cx = floor(5 (x - x0) / (x1 - x0)), weight per
// cell, zero weight excluded, clip-independent; effective ROI/steps readback.
void test_aec_weighted() {
   rig r;
   for (std::uint32_t y = 0; y < H; ++y) {
      for (std::uint32_t x = 0; x < W; ++x) {
         r.raw[std::size_t{y} * W + x] = static_cast<std::uint16_t>(64 * ((x + y) % 50));
      }
   }
   // Spans not multiple of 5 exercise the floor of 5d / span: X span 38 at
   // column d = 15 (5d = 75, edge 76 = 2 * 38), Y span 37 at row d = 22
   // (5d = 110, edge 111 = 3 * 37).
   const std::uint32_t x0 = 6, y0 = 4, x1 = 44, y1 = 41, sx = 3, sy = 2;
   r.profile([&] {
      r.aec(2, 2, 32, 24, FX1_ISP_AEC_CTRL_EN_MASK);
      r.wr(FX1_ISP_AEC_SAMPLE_CLIP_OFFSET, 1000u << 16);  // clip does not qualify the weighted histogram
      r.wr(FX1_ISP_TUNE_HIST_CTRL_OFFSET, FX1_ISP_TUNE_HIST_CTRL_ENABLE_MASK | FX1_ISP_TUNE_HIST_CTRL_ROI_EN_MASK);
      r.wr(FX1_ISP_TUNE_HIST_START_OFFSET, x0 | (y0 << 16));
      r.wr(FX1_ISP_TUNE_HIST_END_OFFSET, x1 | (y1 << 16));
      r.wr(FX1_ISP_TUNE_HIST_STEP_OFFSET, sx | (sy << 8));
      for (unsigned k = 0; k < 25; ++k) {
         r.wr(FX1_ISP_TUNE_HIST_WEIGHT0_OFFSET + 4 * k, k == 12 ? 0u : k + 1);  // centre cell excluded
      }
   });
   FX1_CHECK_EQ(r.err(), 0);
   r.frame();
   std::array<std::uint64_t, 64> wh{};
   std::uint64_t total = 0, wsum = 0;
   for (std::uint32_t y = y0; y < y1; y += sy) {
      for (std::uint32_t x = x0; x < x1; x += sx) {
         const std::uint32_t s = 64 * ((x + y) % 50);
         const std::uint32_t w = [&] {
            const std::uint32_t k = (5 * (y - y0) / (y1 - y0)) * 5 + 5 * (x - x0) / (x1 - x0);
            return k == 12 ? 0u : k + 1;
         }();
         wh[s >> 6] += w;
         total += w;
         wsum += std::uint64_t{w} * s;
      }
   }
   r.with_aec([&] {
      FX1_CHECK_EQ(r.aec_wide(6), total);
      FX1_CHECK_EQ(r.aec_wide(7), wsum);
      for (unsigned b = 0; b < 64; ++b) {
         FX1_CHECK_EQ_CTX(r.aec_wide(0x140 + b), wh[b], "bin " + std::to_string(b));
      }
      FX1_CHECK_EQ(r.aec_wide(0x10), x0 | (y0 << 16));
      FX1_CHECK_EQ(r.aec_wide(0x11), x1 | (y1 << 16));
      FX1_CHECK_EQ(r.aec_wide(0x12), sx | (sy << 8));
      FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_PROFILE_OFFSET), 0x77);
      FX1_CHECK(r.aec_wide(1) < W * H);  // the clip still qualifies the ordinary sums
   });
   // The same writes in legacy mode are storage only: weighted results zero.
   rig l;
   l.wr(FX1_ISP_TUNE_HIST_CTRL_OFFSET, FX1_ISP_TUNE_HIST_CTRL_ENABLE_MASK);
   l.aec(1, 1, W, H, aec_commit);
   l.frame();
   l.with_aec([&] {
      FX1_CHECK_EQ(l.aec_wide(6), 0);
      FX1_CHECK_EQ(l.aec_wide(0x12), 0);
   });
}

// MAS §6.23: AF in the uniform 4 x 4 grid and in programmed windows, noise
// floor, luma and count, leased narrow and wide readout, EN sampled at SOF.
void test_af() {
   rig flat(true);
   flat.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
   flat.frame();
   flat.with_af([&] {
      for (std::uint32_t z = 0; z < 16; ++z) {
         FX1_CHECK_EQ(flat.af(z), 0);  // flat luma, replicate borders
         FX1_CHECK_EQ(flat.af_wide(0x100 + 4 * z + 2), 16u * 12u);  // count = window area
         // Effective uniform windows: [16 c, 16 (c+1)) x [12 r, 12 (r+1)).
         FX1_CHECK_EQ(flat.af_wide(0x200 + 2 * z), (16u * (z % 4)) | ((12u * (z / 4)) << 16));
         FX1_CHECK_EQ(flat.af_wide(0x201 + 2 * z), (16u * (z % 4 + 1)) | ((12u * (z / 4 + 1)) << 16));
      }
      const std::uint64_t l0 = flat.af_wide(0x101);
      FX1_CHECK(l0 > 0 && l0 % (16 * 12) == 0);  // constant luma
      FX1_CHECK_EQ(flat.af_wide(0x10), W);
      FX1_CHECK_EQ(flat.af_wide(0x11), H);
      FX1_CHECK_EQ(flat.af_wide(0x12), 0xFFFF);
      FX1_CHECK_EQ(flat.af_wide(0x13), 0);
      FX1_CHECK_EQ(flat.af_wide(0x14), 0);
      FX1_CHECK_EQ(flat.err(), e_invalid);
   });

   rig r;
   r.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
   FX1_CHECK_EQ(r.brd(FX1_ISP_AF_STAT_DATA_OFFSET), 0);  // no lease
   FX1_CHECK_EQ(r.err(), e_no_lease);
   r.frame();
   FX1_CHECK_EQ(r.rd(FX1_ISP_AF_STATUS_OFFSET), af_done);
   std::uint32_t full[16];
   r.af_cmd(lacq);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AF_STATUS_OFFSET), af_done | af_valid);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AF_FRAME_ID_OFFSET), 1);
   for (std::uint32_t z = 0; z < 16; ++z) {
      full[z] = r.af(z);
      FX1_CHECK_EQ(r.af_wide(0x100 + 4 * z), full[z]);  // AF_STAT_DATA is the low word
   }
   FX1_CHECK(full[0] > 0);  // Gr != Gb leaves a luma texture after demosaic
   r.af_cmd(lrel);
   r.wr(FX1_ISP_AF_STATUS_OFFSET, af_done);  // W1C
   FX1_CHECK_EQ(r.rd(FX1_ISP_AF_STATUS_OFFSET), 0);

   r.frame([&] { r.wr(FX1_ISP_AF_CTRL_OFFSET, 0); });  // cleared mid-frame: still measured
   r.with_af([&] {
      FX1_CHECK_EQ(r.rd(FX1_ISP_AF_FRAME_ID_OFFSET), 2);
      for (std::uint32_t z = 0; z < 16; ++z) {
         FX1_CHECK_EQ(r.af(z), full[z]);
      }
   });
   r.frame([&] { r.wr(FX1_ISP_AF_CTRL_OFFSET, 1); });  // set mid-frame: ignored completely
   FX1_CHECK_EQ(r.af_status() & FX1_ISP_TUNE_AF_STATUS_READY_COUNT_MASK, 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AF_DROPS_OFFSET), 0);

   // Programmed windows (profile mode): an overlap counts in both windows, a
   // disabled window reads zero, NOISE is subtracted and floored at zero. W15,
   // addressed as W0 + 15 * stride, repeats W1's rectangle.
   auto run = [](std::uint32_t noise) {
      rig q;
      q.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
      q.profile([&] {
         q.wr(FX1_ISP_TUNE_AF_CTRL_OFFSET, FX1_ISP_TUNE_AF_CTRL_WINDOW_MODE_MASK | (0x8003u << 16));  // W0, W1, W15
         const std::uint32_t ws = FX1_ISP_TUNE_AF_W1_START_OFFSET - FX1_ISP_TUNE_AF_W0_START_OFFSET;
         q.wr(FX1_ISP_TUNE_AF_W0_START_OFFSET + 15 * ws, 0);
         q.wr(FX1_ISP_TUNE_AF_W0_END_OFFSET + 15 * ws, 24 | (20 << 16));
         q.wr(FX1_ISP_TUNE_AF_W0_START_OFFSET, 0);
         q.wr(FX1_ISP_TUNE_AF_W0_END_OFFSET, W | (H << 16));
         q.wr(FX1_ISP_TUNE_AF_W1_START_OFFSET, 0);  // touches the textured border
         q.wr(FX1_ISP_TUNE_AF_W1_END_OFFSET, 24 | (20 << 16));
         q.wr(FX1_ISP_TUNE_AF_W2_START_OFFSET, 0);  // programmed but not enabled
         q.wr(FX1_ISP_TUNE_AF_W2_END_OFFSET, W | (H << 16));
         q.wr(FX1_ISP_TUNE_AF_NOISE_OFFSET, noise);
      });
      q.frame();
      std::array<std::uint64_t, 9> v{};
      q.with_af([&] {
         for (unsigned k = 0; k < 3; ++k) {
            for (unsigned j = 0; j < 3; ++j) {
               v[3 * k + j] = q.af_wide(0x100 + 4 * k + j);  // focus, luma, count of W0..W2
            }
         }
         FX1_CHECK_EQ(q.af_wide(0x12), 0x8003);
         for (unsigned j = 0; j < 3; ++j) {
            FX1_CHECK_EQ_CTX(q.af_wide(0x100 + 4 * 15 + j), v[3 + j], "W15 repeats W1, field " + std::to_string(j));
         }
         FX1_CHECK_EQ(q.af_wide(0x200 + 2 * 15 + 1), 24u | (20u << 16));
         FX1_CHECK_EQ(q.af_wide(0x13), 1);
         FX1_CHECK_EQ(q.af_wide(0x203), 24u | (20u << 16));
         FX1_CHECK_EQ(q.af_wide(0x204), 0);  // disabled window: empty rectangle
         FX1_CHECK_EQ(q.rd(FX1_ISP_TUNE_AF_PROFILE_OFFSET), 0x77);
      });
      return v;
   };
   const auto n0 = run(0), n8 = run(8), nbig = run(4095);
   FX1_CHECK_EQ(n0[2], W * H);
   FX1_CHECK_EQ(n0[5], 24u * 20u);
   FX1_CHECK(n0[3] <= n0[0] && n0[3] > 0);  // W1 inside W0
   FX1_CHECK(n0[4] < n0[1]);
   FX1_CHECK_EQ(n0[6] + n0[7] + n0[8], 0);  // W2 disabled
   FX1_CHECK(n8[0] < n0[0] && n8[0] > 0);
   FX1_CHECK_EQ(n8[1], n0[1]);  // luma and count do not depend on NOISE
   FX1_CHECK_EQ(n8[2], n0[2]);
   FX1_CHECK_EQ(nbig[0], 0);  // |gx| + |gy| <= 2040 < 4095
}

// Reset scopes (MAS Table 9-187): COMMON soft reset and i_rst_n clear AEC/AF
// results, leases and drops; the DMA reset aborts only the measurement in
// flight; the frame counter and the active set survive a soft reset.
void test_resets() {
   rig r;
   r.aec(16, 12, 4, 4, aec_commit);
   r.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
   r.wr(FX1_ISP_AEC_CONTEXT_ID_OFFSET, 0xA1);
   r.frame();
   r.frame();
   r.frame();  // dropped
   r.aec_cmd(lacq);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_RESULT_CONTEXT_ID_OFFSET), 0xA1);
   FX1_CHECK(r.rd(FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK);
   // DMA-only reset: the measurement in flight is aborted; READY/LEASED and drops stay.
   r.release();
   r.aec_cmd(lrel);
   r.sof();  // into the freed bank
   r.rows(0, H / 2);
   FX1_CHECK(r.rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET) & FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK);
   r.wr(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
   r.cycle += 64;
   FX1_CHECK_EQ(r.aec_ready(), 1);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), 1);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET) & FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK, 0);
   r.aec_cmd(lacq);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_FRAME_ID_OFFSET), 2);
   // COMMON soft reset: ownership, results and drops cleared.
   r.wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
   r.cycle += 64;
   FX1_CHECK_EQ(r.aec_status(), 0);
   FX1_CHECK_EQ(r.af_status(), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AEC_DROPS_OFFSET), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_STATUS_OFFSET), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AF_STATUS_OFFSET), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_COMMON_IRQ_STATUS_OFFSET), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_CTRL_OFFSET), FX1_ISP_AEC_CTRL_EN_MASK);
   r.frame();  // active set and counter survive: publishes without a new commit
   FX1_CHECK_EQ(r.take_aec_frame_id(), 5);
   // A pending commit survives a soft reset (owner answer Q4).
   r.aec(1, 1, W, H, aec_commit);
   r.wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
   r.cycle += 64;
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_CTRL_OFFSET), aec_commit);
   r.frame();
   check_flat_aec(r, 1);

   r.cu.external_reset();
   FX1_CHECK_EQ(r.aec_status(), 0);
   r.wr(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, W);
   r.wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, H);
   r.wr(FX1_ISP_AEC_CTRL_OFFSET, FX1_ISP_AEC_CTRL_EN_MASK);  // active set was reset: en needs a commit
   r.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
   r.frame();
   FX1_CHECK_EQ(r.aec_ready(), 0);
   r.with_af([&] { FX1_CHECK_EQ(r.rd(FX1_ISP_AF_FRAME_ID_OFFSET), 1); });  // counter restarted
}

// ALG-STAT-04: CONTEXT_ID latched at SOF and published with the result.
void test_context_latch() {
   rig r;
   r.aec(16, 12, 4, 4, aec_commit);
   r.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
   r.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
   r.wr(FX1_ISP_AEC_CONTEXT_ID_OFFSET, 0x11);
   r.wr(FX1_ISP_AWB_CONTEXT_ID_OFFSET, 0x22);
   r.wr(FX1_ISP_AF_CONTEXT_ID_OFFSET, 0x33);
   r.frame([&] {
      r.wr(FX1_ISP_AEC_CONTEXT_ID_OFFSET, 0x44);
      r.wr(FX1_ISP_AWB_CONTEXT_ID_OFFSET, 0x55);
      r.wr(FX1_ISP_AF_CONTEXT_ID_OFFSET, 0x66);
   });
   r.frame();
   for (std::uint32_t expect : {0x11u, 0x44u}) {
      r.with_aec([&] { FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_RESULT_CONTEXT_ID_OFFSET), expect); });
      r.with_af([&] { FX1_CHECK_EQ(r.rd(FX1_ISP_AF_RESULT_CONTEXT_ID_OFFSET), expect + 0x22); });
      r.acquire();
      FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_RESULT_CONTEXT_ID_OFFSET), expect + 0x11);
      r.release();
   }
}

// DEC-38 block busy bits; DEC-39 / M5-A1 warnings for requested features that
// are not modelled.
void test_block_busy_and_warnings() {
   control_unit cu;
   const std::uint32_t regs[] = {FX1_ISP_DEMOSAIC_STATUS_OFFSET, FX1_ISP_CCM_STATUS_OFFSET, FX1_ISP_GAMMA_STATUS_OFFSET,
                                 FX1_ISP_GTM_STATUS_OFFSET,      FX1_ISP_NR_2D_STATUS_OFFSET, FX1_ISP_EE_STATUS_OFFSET,
                                 FX1_ISP_CNF_STATUS_OFFSET,      FX1_ISP_RESIZER_STATUS_OFFSET};
   cu.blocks_busy(true);
   for (std::uint32_t r : regs) {
      FX1_CHECK_EQ(cu.csr_read(r) & 1u, 1);
      cu.csr_write(r, 0xFFFFFFFFu, 0xFFFFFFFFu, 2);  // RO
      FX1_CHECK_EQ(cu.csr_read(r) & 1u, 1);
   }
   cu.csr_write(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK, 0xFFFFFFFFu, 3);
   for (std::uint32_t r : regs) {
      FX1_CHECK_EQ(cu.csr_read(r) & 1u, 0);  // soft reset: no frame in flight
   }
   cu.blocks_busy(true);
   cu.blocks_busy(false);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_CNF_STATUS_OFFSET), 0);

   auto warnings = [](std::uint32_t off, std::uint32_t v) {
      control_unit c;
      c.csr_write(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, W, 0xFFFFFFFFu, 1);
      c.csr_write(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, H, 0xFFFFFFFFu, 2);
      if (off) {
         c.csr_write(off, v, 0xFFFFFFFFu, 3);
      }
      c.accepted_sof(4);
      const pipe::pipeline_config cfg = pipe::snapshot_config(c);
      pipe::isp_pipeline p;
      std::vector<std::string> w;
      p.begin(cfg, W, H, [](std::vector<std::uint8_t> &&) {}, [](std::vector<std::uint8_t> &&) {},
              [&](const std::string &m) { w.push_back(m); });
      return w;
   };
   FX1_CHECK_EQ(warnings(0, 0).size(), 0);
   const auto ofmt = warnings(FX1_ISP_OFMT_CTRL_OFFSET, FX1_ISP_OFMT_CTRL_STRIDE_EN_MASK);
   FX1_CHECK(ofmt.size() == 1 && ofmt[0].find("DEC-39") != std::string::npos);
   const auto wdr = warnings(FX1_ISP_D_WDR_CTRL_OFFSET, FX1_ISP_D_WDR_CTRL_EN_MASK);
   FX1_CHECK(wdr.size() == 1 && wdr[0].find("M5-A1") != std::string::npos);
   const auto tnr = warnings(FX1_ISP_TNR_3D_CTRL_OFFSET, FX1_ISP_TNR_3D_CTRL_EN_MASK);
   FX1_CHECK(tnr.size() == 1 && tnr[0].find("M5-A1") != std::string::npos);
}

// ALG-STAT-05: the AWB set is sampled at SOF.
void test_awb_sampled_at_sof() {
   auto setup = [](rig &r) {
      r.wr(FX1_ISP_AWB_UNDEREXPOSED_LIMIT_OFFSET, 50);
      r.wr(FX1_ISP_AWB_SATURATION_LIMIT_OFFSET, 4050);
      r.wr(FX1_ISP_AWB_CTRL_OFFSET, 1 | (4 << 1) | (3 << 8));
   };
   rig a, b;
   setup(a);
   setup(b);
   a.frame();
   b.frame([&] {
      b.wr(FX1_ISP_AWB_CTRL_OFFSET, 0);
      b.wr(FX1_ISP_AWB_UNDEREXPOSED_LIMIT_OFFSET, 3000);  // would reject every pixel if live
      b.wr(FX1_ISP_AWB_SATURATION_LIMIT_OFFSET, 4095);
   });
   FX1_CHECK_EQ(b.rd(FX1_ISP_AWB_STATUS_OFFSET), FX1_ISP_AWB_STATUS_STAT_DONE_MASK);
   a.acquire();
   b.acquire();
   for (std::uint32_t off = FX1_ISP_AWB_GLOBAL_SUM_R_H_OFFSET; off <= FX1_ISP_AWB_GLOBAL_COUNT_OFFSET; off += 4) {
      FX1_CHECK_EQ(b.rd(off), a.rd(off));
   }
   for (std::uint32_t z = 0; z < 12; ++z) {
      a.wr(FX1_ISP_AWB_ZONE_ADDR_OFFSET, z);
      b.wr(FX1_ISP_AWB_ZONE_ADDR_OFFSET, z);
      FX1_CHECK_EQ(b.rd(FX1_ISP_AWB_ZONE_COUNT_OFFSET), a.rd(FX1_ISP_AWB_ZONE_COUNT_OFFSET));
      FX1_CHECK_EQ(b.rd(FX1_ISP_AWB_ZONE_SUM_G_L_OFFSET), a.rd(FX1_ISP_AWB_ZONE_SUM_G_L_OFFSET));
   }
   FX1_CHECK(a.rd(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET) > 0);
   b.frame();  // disabled at this SOF: no measurement, no loss, the lease untouched
   FX1_CHECK_EQ(b.rd(FX1_ISP_AWB_FRAME_ID_OFFSET), 1);
   FX1_CHECK_EQ(b.rd(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET), a.rd(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET));
   FX1_CHECK_EQ(b.ready_count(), 0);
   FX1_CHECK_EQ(b.drops(), 0);
   FX1_CHECK_EQ(b.awb_status() & overrun, 0);
}

// ALG-STAT-01/09/11: one ID per started frame; an aborted frame consumes an
// ID and publishes nothing; stats_ready_irq per publication.
void test_frame_id_abort_irq() {
   rig r;
   r.aec(16, 12, 4, 4, aec_commit);
   r.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
   r.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
   r.wr(FX1_ISP_COMMON_IRQ_EN_OFFSET, FX1_ISP_COMMON_IRQ_EN_STATS_READY_EN_MASK);
   FX1_CHECK(!r.cu.irq_level());
   r.frame();
   FX1_CHECK_EQ(r.take_aec_frame_id(), 1);
   r.with_af([&] { FX1_CHECK_EQ(r.rd(FX1_ISP_AF_FRAME_ID_OFFSET), 1); });
   FX1_CHECK_EQ(r.take_awb_frame_id(), 1);
   FX1_CHECK(r.cu.irq_level());
   r.wr(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_STATS_READY_IRQ_MASK);
   FX1_CHECK(!r.cu.irq_level());

   r.sof();  // aborted: the engine calls stats_abort() instead of end_of_frame
   r.rows(0, H / 2);
   r.cu.stats_abort();
   FX1_CHECK_EQ(r.rd(FX1_ISP_AEC_STATUS_OFFSET), aec_done);  // busy cleared, old done kept
   FX1_CHECK_EQ(r.rd(FX1_ISP_AF_STATUS_OFFSET) & FX1_ISP_AF_STATUS_BUSY_MASK, 0);
   FX1_CHECK(!r.cu.irq_level());
   FX1_CHECK_EQ(r.aec_status(), 0);  // the aborted banks were freed, nothing published
   FX1_CHECK_EQ(r.af_status(), 0);
   FX1_CHECK_EQ(r.awb_status(), 0);
   r.frame();
   FX1_CHECK_EQ(r.take_aec_frame_id(), 3);  // skips the aborted frame's ID
   FX1_CHECK_EQ(r.take_awb_frame_id(), 3);
   FX1_CHECK(r.cu.irq_level());  // done bits were never cleared: the event still interrupts
}

// A bus read raises NO_LEASE / INVALID on the read's own edge, not on the
// edge of the last write: a W1C on that edge loses to the event, one on the
// next edge clears it (MAS Table 8-180; review V107-2..6 R5).
void test_read_error_edges() {
   const std::uint32_t te = FX1_ISP_TUNE_ERROR_OFFSET, all_lanes = 0xFFFFFFFFu;
   for (const std::uint32_t mask : {e_no_lease, e_invalid}) {
      for (const bool clear_first : {false, true}) {
         rig r;
         r.aec(1, 1, W, H, aec_commit);
         r.frame();
         if (mask == e_invalid) {
            r.aec_cmd(lacq);
            r.wr(FX1_ISP_TUNE_AEC_INDEX_OFFSET, 9);  // reserved index (Table 9-193)
         }
         FX1_CHECK_EQ(r.err(), 0);
         const std::uint64_t last_write = r.cycle - 1, c = r.cycle + 10;  // read well after the last write
         FX1_CHECK(c > last_write + 1);
         if (clear_first) {
            r.cu.csr_write(te, mask, all_lanes, c);  // clear, then the event, on one edge
         }
         FX1_CHECK_EQ(r.cu.csr_bus_read(FX1_ISP_TUNE_AEC_DATA_LO_OFFSET, c), 0);
         if (!clear_first) {
            r.cu.csr_write(te, mask, all_lanes, c);  // the event, then the clear, on one edge
         }
         const std::string ctx = std::string(mask == e_no_lease ? "NO_LEASE" : "INVALID") +
                                 (clear_first ? ", clear before" : ", clear after");
         FX1_CHECK_EQ_CTX(r.rd(te) & mask, mask, ctx + ": the same-edge event wins");
         r.cu.csr_write(te, mask, all_lanes, c + 1);
         FX1_CHECK_EQ_CTX(r.rd(te) & mask, 0, ctx + ": cleared on the next edge");
         r.cycle = c + 2;
      }
   }
}

// The published registers carry the CSR field widths: the narrow windows are
// modulo projections (MAS §6.21.3.3), the wide port carries 64 bits.
void test_readout_widths() {
   control_unit cu;
   cu.csr_write(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, W, 0xFFFFFFFFu, 1);
   cu.csr_write(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, H, 0xFFFFFFFFu, 1);
   cu.csr_write(FX1_ISP_AF_CTRL_OFFSET, 1, 0xFFFFFFFFu, 1);
   cu.csr_write(FX1_ISP_AEC_ZONE_CFG_OFFSET, 1 | (1 << 8), 0xFFFFFFFFu, 1);
   cu.csr_write(FX1_ISP_AEC_ZONE_SIZE_OFFSET, W | (H << 16), 0xFFFFFFFFu, 1);
   cu.csr_write(FX1_ISP_AEC_CTRL_OFFSET, aec_commit, 0xFFFFFFFFu, 1);
   cu.csr_write(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1, 0xFFFFFFFFu, 1);
   cu.accepted_sof(2);
   aec_payload a;
   a.restart(cu.aec_frame_config(), W, H);
   a.sum = 33965568000ull;  // 4K all-4095: 35 bits
   a.count = (FX1_ISP_AEC_GLOBAL_COUNT_GLOBAL_COUNT_MASK + 1ull) + 7;  // 23 bits
   a.zones[0].sum = (1ull << 28) + 9;
   a.zones[0].count = 65536 + 3;
   a.zones[0].oe = 65536 + 0x20;  // bits above 16 must not reach the neighbouring field
   a.zones[0].ue = (3u << 16) + 2;
   a.zones[0].min = 17;
   a.zones[0].max = 4000;
   a.hist[0] = (FX1_ISP_AEC_HIST_DATA_HIST_COUNT_MASK + 1ull) + 11;
   awb_payload b;
   b.global.r = (1ull << 35) + (5ull << 32) + 77;
   b.global.n = (1u << 23) + 4;
   b.mem.nx = b.mem.ny = 1;
   b.mem.zsum.assign(1, {(1ull << 35) + (6ull << 32) + 78, 0, 0});
   b.mem.zcnt.assign(1, (1u << 23) + 5);
   af_payload f;
   f.restart(cu.af_frame_config(), W, H);
   f.win[3].focus = 0x1FFFFFFFFull;
   cu.stats_publish(&a, &b, &f);
   cu.csr_write(FX1_ISP_AWB_RESULT_CTRL_OFFSET, FX1_ISP_AWB_RESULT_CTRL_ACQUIRE_MASK, 0xFFFFFFFFu, 3);
   cu.csr_write(FX1_ISP_TUNE_AEC_CMD_OFFSET, lacq, 0xFFFFFFFFu, 3);
   cu.csr_write(FX1_ISP_TUNE_AF_CMD_OFFSET, lacq, 0xFFFFFFFFu, 3);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AEC_GLOBAL_SUM_HI_OFFSET), 1);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET), static_cast<std::uint32_t>(33965568000ull));
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AEC_GLOBAL_COUNT_OFFSET), 7);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AEC_ZONE_SUM_OFFSET), 9);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AEC_ZONE_COUNT_OFFSET), 3);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AEC_ZONE_GREEN_OE_UE_OFFSET), (0x20u << 16) | 2u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET), (4000u << 12) | 17u);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AEC_HIST_DATA_OFFSET), 11);
   cu.csr_write(FX1_ISP_TUNE_AEC_INDEX_OFFSET, 0, 0xFFFFFFFFu, 4);
   FX1_CHECK_EQ(cu.csr_bus_read(FX1_ISP_TUNE_AEC_DATA_LO_OFFSET, 5), static_cast<std::uint32_t>(33965568000ull));
   FX1_CHECK_EQ(cu.csr_bus_read(FX1_ISP_TUNE_AEC_DATA_HI_OFFSET, 6), 7);  // 33 965 568 000 >> 32
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AWB_GLOBAL_SUM_R_H_OFFSET), 5);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AWB_GLOBAL_SUM_R_L_OFFSET), 77);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET), 4);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AWB_ZONE_SUM_R_H_OFFSET), 6);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AWB_ZONE_SUM_R_L_OFFSET), 78);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AWB_ZONE_COUNT_OFFSET), 5);
   cu.csr_write(FX1_ISP_AF_STAT_ADDR_OFFSET, 3, 0xFFFFFFFFu, 7);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AF_STAT_DATA_OFFSET), 0xFFFFFFFFu);  // low 32 bits
   // Writes to published results are ignored (RO).
   cu.csr_write(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET, 0x123, 0xFFFFFFFFu, 8);
   FX1_CHECK_EQ(cu.csr_read(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET), 4);
}

// MAS §6.22.3: post-BPC 2x2 cells, strict four-sample limits, half-up green,
// zones over the cell raster; in profile mode the cell ROI, component limits
// and ellipse union (§6.22.3.9); leased frame size and profile (§9.9.9).
void test_awb_cells() {
   rig r;
   r.wr(FX1_ISP_AWB_UNDEREXPOSED_LIMIT_OFFSET, 99);  // R = 100 > 99
   r.wr(FX1_ISP_AWB_SATURATION_LIMIT_OFFSET, 4001);  // B = 4000 < 4001
   r.wr(FX1_ISP_AWB_CTRL_OFFSET, 1 | (2 << 1) | (2 << 8));
   r.frame();
   r.acquire();
   FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET), (W / 2) * (H / 2));  // one count per cell
   FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_GLOBAL_SUM_R_L_OFFSET), 100u * (W / 2) * (H / 2));
   FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_GLOBAL_SUM_G_L_OFFSET), 2150u * (W / 2) * (H / 2));  // (1000 + 3300 + 1) >> 1
   FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_GLOBAL_SUM_B_L_OFFSET), 4000u * (W / 2) * (H / 2));
   FX1_CHECK_EQ(r.awb_zone_count(3), (W / 4) * (H / 4));  // 16 x 12 cells per zone
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AWB_FRAME_SIZE_OFFSET), W | (H << 16));
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AWB_PROFILE_OFFSET), 0);
   r.release();
   FX1_CHECK_EQ(r.rd(FX1_ISP_TUNE_AWB_FRAME_SIZE_OFFSET), 0);  // zero without a lease, no error
   // Equality at either limit rejects the whole cell.
   for (const auto &lim : {std::array<std::uint32_t, 2>{100, 4095}, std::array<std::uint32_t, 2>{0, 4000}}) {
      rig q;
      q.wr(FX1_ISP_AWB_UNDEREXPOSED_LIMIT_OFFSET, lim[0]);
      q.wr(FX1_ISP_AWB_SATURATION_LIMIT_OFFSET, lim[1]);
      q.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
      q.frame();
      q.acquire();
      FX1_CHECK_EQ(q.rd(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET), 0);
   }
   // Profile mode: ROI cells [2, 10) x [1, 5); G limit; ellipse union.
   auto cells = [](const std::function<void(rig &)> &body) {
      rig q;
      q.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
      q.profile([&] { body(q); });
      q.frame();
      q.acquire();
      return q.rd(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET);
   };
   FX1_CHECK_EQ(cells([](rig &q) {
                   q.wr(FX1_ISP_TUNE_AWB_CTRL_OFFSET, FX1_ISP_TUNE_AWB_CTRL_ROI_EN_MASK);
                   q.wr(FX1_ISP_TUNE_AWB_ROI_START_OFFSET, 2 | (1 << 16));
                   q.wr(FX1_ISP_TUNE_AWB_ROI_END_OFFSET, 10 | (5 << 16));
                }),
                8u * 4u);
   FX1_CHECK_EQ(cells([](rig &q) { q.wr(FX1_ISP_TUNE_AWB_G_LIMITS_OFFSET, 2149u << 16); }), 0);
   FX1_CHECK_EQ(cells([](rig &q) { q.wr(FX1_ISP_TUNE_AWB_G_LIMITS_OFFSET, 2150u | (2150u << 16)); }),
                (W / 2) * (H / 2));  // inclusive on both ends
   // Colour: rg = R(256*100, 2150) = 12, bg = R(256*4000, 2150) = 476.
   auto ellipse = [](std::uint32_t mask, std::uint32_t rg, std::uint32_t bg, std::uint32_t r2) {
      return [=](rig &q) {
         q.wr(FX1_ISP_TUNE_AWB_CTRL_OFFSET, FX1_ISP_TUNE_AWB_CTRL_COLOR_EN_MASK | (mask << 8));
         q.wr(FX1_ISP_TUNE_AWB_E3_CENTER_OFFSET, rg | (bg << 16));
         q.wr(FX1_ISP_TUNE_AWB_E3_RADIUS2_OFFSET, r2);
      };
   };
   const std::uint32_t all = (W / 2) * (H / 2);
   FX1_CHECK_EQ(cells(ellipse(1u << 3, 12, 476, 0)), all);   // on the centre: metric 0 <= 0
   FX1_CHECK_EQ(cells(ellipse(1u << 3, 12, 478, 3)), 0);     // dB = -2: 4 > 3 (unit A = C = 1.0)
   FX1_CHECK_EQ(cells(ellipse(1u << 3, 12, 478, 4)), all);   // 4 <= 4
   FX1_CHECK_EQ(cells(ellipse(0, 12, 476, 100)), 0);         // empty mask accepts nothing
   FX1_CHECK_EQ(cells(ellipse((1u << 3) | 1u, 12, 476, 0)), all);  // union with E0 (centre 256, 256): once
   // R and B limits, inclusive (R = 100, B = 4000 on every cell).
   auto limits = [](std::uint32_t off, std::uint32_t mn, std::uint32_t mx) {
      return [=](rig &q) { q.wr(off, mn | (mx << 16)); };
   };
   FX1_CHECK_EQ(cells(limits(FX1_ISP_TUNE_AWB_R_LIMITS_OFFSET, 101, 4095)), 0);
   FX1_CHECK_EQ(cells(limits(FX1_ISP_TUNE_AWB_R_LIMITS_OFFSET, 100, 100)), all);
   FX1_CHECK_EQ(cells(limits(FX1_ISP_TUNE_AWB_R_LIMITS_OFFSET, 0, 99)), 0);
   FX1_CHECK_EQ(cells(limits(FX1_ISP_TUNE_AWB_B_LIMITS_OFFSET, 4001, 4095)), 0);
   FX1_CHECK_EQ(cells(limits(FX1_ISP_TUNE_AWB_B_LIMITS_OFFSET, 4000, 4000)), all);
   FX1_CHECK_EQ(cells(limits(FX1_ISP_TUNE_AWB_B_LIMITS_OFFSET, 0, 3999)), 0);
   // Quadratic form A dr^2 + 2 B dr db + C db^2 <= R2 * 2^14 (Q2.14 A, C;
   // signed B) with dr = db = 2 from centre (14, 478), on ellipse k = 5
   // addressed as E0 + k * stride.
   auto quad = [](std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t r2) {
      return [=](rig &q) {
         const std::uint32_t es = FX1_ISP_TUNE_AWB_E1_CENTER_OFFSET - FX1_ISP_TUNE_AWB_E0_CENTER_OFFSET;
         q.wr(FX1_ISP_TUNE_AWB_CTRL_OFFSET, FX1_ISP_TUNE_AWB_CTRL_COLOR_EN_MASK | ((1u << 5) << 8));
         q.wr(FX1_ISP_TUNE_AWB_E0_CENTER_OFFSET + 5 * es, 14 | (478u << 16));
         q.wr(FX1_ISP_TUNE_AWB_E0_A_OFFSET + 5 * es, a);
         q.wr(FX1_ISP_TUNE_AWB_E0_B_OFFSET + 5 * es, b);
         q.wr(FX1_ISP_TUNE_AWB_E0_C_OFFSET + 5 * es, c);
         q.wr(FX1_ISP_TUNE_AWB_E0_RADIUS2_OFFSET + 5 * es, r2);
      };
   };
   FX1_CHECK_EQ(cells(quad(0x8000, 0, 0x4000, 11)), 0);         // 2*4 + 4 = 12 > 11
   FX1_CHECK_EQ(cells(quad(0x8000, 0, 0x4000, 12)), all);       // A = 2.0
   FX1_CHECK_EQ(cells(quad(0x4000, 0, 0x8000, 11)), 0);         // C = 2.0
   FX1_CHECK_EQ(cells(quad(0x4000, 0, 0x8000, 12)), all);
   FX1_CHECK_EQ(cells(quad(0x4000, 0x2000, 0x4000, 11)), 0);    // 4 + 2*0.5*4 + 4 = 12
   FX1_CHECK_EQ(cells(quad(0x4000, 0x2000, 0x4000, 12)), all);
   FX1_CHECK_EQ(cells(quad(0x4000, 0xE000, 0x4000, 3)), 0);     // B = -0.5 (sign-extended): 4 - 4 + 4 = 4
   FX1_CHECK_EQ(cells(quad(0x4000, 0xE000, 0x4000, 4)), all);
}

// AWB-FULL and queue order (MAS §6.22.4.2, §6.22.5.5): with one bank leased
// and the other READY, enabled frames are skipped whole, each counted once;
// video keeps running; admission resumes after RELEASE; READY banks are
// acquired oldest first.
void test_awb_full_and_order() {
   rig r;
   r.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
   r.wr(FX1_ISP_AF_CTRL_OFFSET, 1);
   r.frame();  // frame 1 -> bank 0
   r.acquire();
   FX1_CHECK(r.leased());
   FX1_CHECK_EQ(r.awb_status() & FX1_ISP_AWB_RESULT_STATUS_READ_BANK_MASK, 0);
   const std::uint32_t count1 = r.rd(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET);
   FX1_CHECK(count1 > 0);
   r.frame();  // frame 2 -> bank 1
   FX1_CHECK_EQ(r.ready_count(), 1);
   FX1_CHECK_EQ(r.awb_status() & overrun, 0);
   r.frame();  // frames 3 and 4: no FREE bank
   r.frame();
   FX1_CHECK_EQ(r.drops(), 2);
   FX1_CHECK_EQ(r.awb_status() & overrun, overrun);
   FX1_CHECK_EQ(r.ready_count(), 1);
   FX1_CHECK_EQ(r.cu.frame_id(), 4u);  // video and the other statistics continued
   FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_FRAME_ID_OFFSET), 1);  // the lease is never written
   FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET), count1);
   r.release();
   FX1_CHECK(!r.leased());
   r.frame();  // frame 5 -> bank 0 again
   FX1_CHECK_EQ(r.ready_count(), 2);
   FX1_CHECK_EQ(r.take_awb_frame_id(), 2);  // completion order
   FX1_CHECK_EQ(r.take_awb_frame_id(), 5);
   FX1_CHECK_EQ(r.ready_count(), 0);
   FX1_CHECK_EQ(r.drops(), 2);  // reporting is independent of ownership
}

// AWB-COMMAND (MAS §6.22.4.3): no-ops, RELEASE priority, byte strobes,
// command reads, one clock after ACQUIRE, following-edge eligibility.
void test_awb_commands() {
   rig r;
   const std::uint32_t acq = FX1_ISP_AWB_RESULT_CTRL_ACQUIRE_MASK, rel = FX1_ISP_AWB_RESULT_CTRL_RELEASE_MASK;
   const std::uint32_t valid = FX1_ISP_AWB_RESULT_STATUS_READ_VALID_MASK | FX1_ISP_AWB_RESULT_STATUS_LEASED_MASK;
   r.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
   r.acquire();  // nothing READY
   r.release();  // no lease
   FX1_CHECK_EQ(r.awb_status(), 0);
   r.frame();
   r.cu.csr_write(FX1_ISP_AWB_RESULT_CTRL_OFFSET, acq, 0xFFFFFF00u, r.cycle++);  // byte 0 not strobed
   FX1_CHECK(!r.leased());
   r.awb_cmd(acq | rel);  // RELEASE wins; no implicit acquisition
   FX1_CHECK(!r.leased());
   FX1_CHECK_EQ(r.ready_count(), 1);
   // ACQUIRE: owned and readable from the next edge (user decision 2026-10-09).
   const std::uint64_t c = r.cycle;
   r.cu.csr_write(FX1_ISP_AWB_RESULT_CTRL_OFFSET, acq, 0xFFFFFFFFu, c);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_RESULT_CTRL_OFFSET, c), 0);  // commands read 0
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_RESULT_STATUS_OFFSET, c) & valid, 0);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET, c), 0);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_FRAME_ID_OFFSET, c), 0);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_RESULT_STATUS_OFFSET, c + 1) & valid, valid);
   FX1_CHECK(r.cu.csr_read(FX1_ISP_AWB_GLOBAL_COUNT_OFFSET, c + 1) > 0);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_FRAME_ID_OFFSET, c + 1), 1);
   r.cycle = c + 1;
   // The zone read port is synchronous: on the edge of the AWB_ZONE_ADDR
   // write the data is still the previous address's (MAS §6.22.4.3).
   const std::uint32_t zone0 = r.cu.csr_read(FX1_ISP_AWB_ZONE_COUNT_OFFSET, r.cycle);
   FX1_CHECK(zone0 > 0);
   const std::uint64_t za = r.cycle++;
   r.cu.csr_write(FX1_ISP_AWB_ZONE_ADDR_OFFSET, 1, 0xFFFFFFFFu, za);  // beyond the 1 x 1 grid: reads 0
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_ZONE_COUNT_OFFSET, za), zone0);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_ZONE_COUNT_OFFSET, za + 1), 0);
   r.cu.csr_write(FX1_ISP_AWB_ZONE_ADDR_OFFSET, 0, 0xFFFFFFFFu, r.cycle);
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_ZONE_COUNT_OFFSET, r.cycle), 0);
   ++r.cycle;
   FX1_CHECK_EQ(r.cu.csr_read(FX1_ISP_AWB_ZONE_COUNT_OFFSET, r.cycle), zone0);
   // Grid counts of the leased bank, not of the live configuration.
   r.wr(FX1_ISP_AWB_CTRL_OFFSET, 1 | (8 << 1) | (6 << 8));
   FX1_CHECK_EQ((r.awb_status() & FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_X_MASK) >>
                    FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_X_SHIFT, 1);
   FX1_CHECK_EQ(r.awb_zone_count(1), 0);  // outside the captured 1 x 1 grid
   // A second ACQUIRE while leased is a no-op.
   r.frame();  // frame 2 -> bank 1
   r.acquire();
   FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_FRAME_ID_OFFSET), 1);
   FX1_CHECK_EQ(r.ready_count(), 1);
   r.release();
   r.release();  // no lease: no-op
   FX1_CHECK_EQ(r.ready_count(), 1);
   FX1_CHECK_EQ(r.take_awb_frame_id(), 2);
   // A bank published on an ACQUIRE edge is acquirable from the next edge.
   r.sof();
   r.rows(0, H);
   r.p.finish();
   const std::uint64_t e = r.cycle;
   r.cu.advance_to(e);
   pipe::end_of_frame(r.cu, r.cfg, r.p.stats());
   r.cu.csr_write(FX1_ISP_AWB_RESULT_CTRL_OFFSET, acq, 0xFFFFFFFFu, e);
   FX1_CHECK(!r.leased());
   FX1_CHECK_EQ(r.ready_count(), 1);
   r.cycle = e + 1;
   r.acquire();
   FX1_CHECK(r.leased());
   // A bank released on an SOF edge is eligible from the next edge: with the
   // other bank READY, that SOF is skipped.
   r.frame();  // bank 1 READY (bank 0 leased)
   const std::uint64_t s0 = r.cycle;
   r.cu.csr_write(FX1_ISP_AWB_RESULT_CTRL_OFFSET, rel, 0xFFFFFFFFu, s0);
   const std::uint32_t drops = r.drops();
   r.cu.accepted_sof(s0);
   FX1_CHECK_EQ(r.drops(), drops + 1);
   FX1_CHECK_EQ(r.awb_status() & write_busy, 0);
   r.cycle = s0 + 1;
   r.cu.stats_abort();
   r.frame();  // bank 0 is eligible again
   FX1_CHECK_EQ(r.drops(), drops + 1);
   FX1_CHECK_EQ(r.ready_count(), 2);
}

// AWB-REPORT (MAS §6.22.4.3): CLEAR_REPORT clears OVERRUN and DROP_COUNT only;
// a loss on the same edge survives it; STAT_DONE clear releases nothing.
void test_awb_report() {
   rig r;
   const std::uint32_t clear = FX1_ISP_AWB_RESULT_CTRL_CLEAR_REPORT_MASK;
   r.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
   r.frame();
   r.acquire();
   r.frame();
   r.frame();  // skipped
   FX1_CHECK_EQ(r.drops(), 1);
   r.wr(FX1_ISP_AWB_STATUS_OFFSET, FX1_ISP_AWB_STATUS_STAT_DONE_MASK);  // W1C: no bank released
   FX1_CHECK(r.leased());
   FX1_CHECK_EQ(r.ready_count(), 1);
   r.awb_cmd(clear);
   FX1_CHECK_EQ(r.drops(), 0);
   FX1_CHECK_EQ(r.awb_status() & overrun, 0);
   FX1_CHECK(r.leased());  // reporting cleared, data kept
   FX1_CHECK_EQ(r.ready_count(), 1);
   FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_FRAME_ID_OFFSET), 1);
   // Clear first, then a loss on the same edge: OVERRUN set, count 1.
   const std::uint64_t c = r.cycle;
   r.cu.accepted_sof(c);  // skipped: no FREE bank
   r.cu.csr_write(FX1_ISP_AWB_RESULT_CTRL_OFFSET, clear, 0xFFFFFFFFu, c);
   FX1_CHECK_EQ(r.drops(), 1);
   FX1_CHECK_EQ(r.awb_status() & overrun, overrun);
   r.cycle = c + 1;
   r.cu.stats_abort();
   r.awb_cmd(clear);
   FX1_CHECK_EQ(r.drops(), 0);
}

// AWB-GRID (MAS §6.22.3.7): the grid covers the cell raster, 1 <= NX <=
// min(64, W/2), 1 <= NY <= min(32, H/2); an illegal grid is not accumulated
// or published; it is reported; disabled frames do not count.
void test_awb_grid() {
   const struct {
      std::uint32_t nx, ny;
      bool legal;
   } cases[] = {{0, 1, false}, {1, 0, false}, {W / 2, H / 2, true}, {W / 2 + 1, 1, false}, {1, H / 2 + 1, false},
                {64, 1, false}, {W, H, false}};
   for (const auto &k : cases) {
      rig r;
      const std::string ctx = std::to_string(k.nx) + " x " + std::to_string(k.ny);
      r.wr(FX1_ISP_AWB_CTRL_OFFSET, 1 | ((k.nx & 0x7F) << 1) | ((k.ny & 0x3F) << 8));
      r.frame();
      FX1_CHECK_EQ_CTX(r.ready_count(), k.legal ? 1u : 0u, ctx);
      FX1_CHECK_EQ_CTX(r.drops(), k.legal ? 0u : 1u, ctx);
      FX1_CHECK_EQ_CTX(r.rd(FX1_ISP_AWB_STATUS_OFFSET), k.legal ? 1u : 0u, ctx + " STAT_DONE");
      r.wr(FX1_ISP_AWB_CTRL_OFFSET, 0);
      r.frame();
      FX1_CHECK_EQ_CTX(r.drops(), k.legal ? 0u : 1u, ctx + " disabled frame");
   }
   // Active images below 32 x 32 are not legal for AWB (§6.22.3.7).
   rig n;
   n.wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, 30);
   n.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
   n.cu.accepted_sof(n.cycle++);
   FX1_CHECK_EQ(n.drops(), 1);
}

// AWB-RESET (MAS p248): the DMA-only soft reset aborts the measurement in
// flight and keeps READY/LEASED banks and reporting; the COMMON soft reset and
// i_rst_n clear ownership and reporting.
void test_awb_reset_scopes() {
   const auto setup = [](rig &r) {
      r.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
      r.frame();  // frame 1 -> bank 0, leased
      r.acquire();
      r.frame();  // frame 2 -> bank 1, READY
      r.frame();  // frame 3 skipped
      r.sof();    // frame 4 skipped as well; nothing written
      r.release();
      r.cu.stats_abort();
      r.frame();  // frame 5 -> bank 0 READY
      r.acquire();  // lease frame 2
      r.sof();    // frame 6 -> nothing free (bank 0 READY, bank 1 leased)
      r.rows(0, H / 2);
   };
   {
      rig r;
      setup(r);
      FX1_CHECK_EQ(r.drops(), 3);
      r.wr(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
      r.cycle += 64;
      FX1_CHECK(r.leased());
      FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_FRAME_ID_OFFSET), 2);
      FX1_CHECK_EQ(r.ready_count(), 1);
      FX1_CHECK_EQ(r.drops(), 3);
      FX1_CHECK_EQ(r.awb_status() & overrun, overrun);
      r.release();
      FX1_CHECK_EQ(r.take_awb_frame_id(), 5);
   }
   {
      // The DMA reset aborts a measurement being written: its bank is freed.
      rig r;
      r.wr(FX1_ISP_AWB_CTRL_OFFSET, awb_1x1);
      r.sof();
      r.rows(0, H / 2);
      FX1_CHECK_EQ(r.awb_status() & write_busy, write_busy);
      r.wr(FX1_ISP_DMA_CTRL_OFFSET, FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
      r.cycle += 64;
      FX1_CHECK_EQ(r.awb_status(), 0);
      FX1_CHECK(r.cu.awb_banks().banks[0].st == result_banks<awb_payload>::state::free);
   }
   {
      rig r;
      setup(r);
      r.wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
      r.cycle += 64;
      FX1_CHECK_EQ(r.awb_status(), 0);
      FX1_CHECK_EQ(r.drops(), 0);
      FX1_CHECK_EQ(r.rd(FX1_ISP_AWB_FRAME_ID_OFFSET), 0);
      r.frame();
      FX1_CHECK_EQ(r.take_awb_frame_id(), 7);  // the frame counter is kept
   }
   {
      rig r;
      setup(r);
      r.cu.external_reset();
      FX1_CHECK_EQ(r.awb_status(), 0);
      FX1_CHECK_EQ(r.drops(), 0);
   }
}

// BPC counters (MAS §6.9): cleared at every accepted SOF, counted in an
// enabled frame; DYNAMIC_DET_EN has no effect (DEC-28 superseded).
void test_bpc_counters() {
   rig r;  // the flat channels leave the 5 x 5 same-colour neighbourhood flat
   r.raw[std::size_t{20} * W + 20] = 4095;  // one hot R pixel
   r.wr(FX1_ISP_BPC_THRESH_OFFSET, 100);    // floor 100, k 0
   r.wr(FX1_ISP_BPC_CTRL_OFFSET, 1);
   r.frame();
   const std::uint32_t cand = r.rd(FX1_ISP_BPC_NUM_CANDIDATES_OFFSET);
   FX1_CHECK(cand >= 1);
   FX1_CHECK_EQ(r.rd(FX1_ISP_BPC_NUM_DEFECTIVE_OFFSET), cand);  // k = 0: thresholds coincide
   r.wr(FX1_ISP_BPC_MODE_OFFSET, FX1_ISP_BPC_MODE_DYNAMIC_DET_EN_MASK);
   r.frame();
   FX1_CHECK_EQ(r.rd(FX1_ISP_BPC_NUM_CANDIDATES_OFFSET), cand);  // no effect
   r.sof();
   FX1_CHECK_EQ(r.rd(FX1_ISP_BPC_NUM_CANDIDATES_OFFSET), 0);  // cleared at SOF
   r.rows(0, H);
   r.eof();
   FX1_CHECK_EQ(r.rd(FX1_ISP_BPC_NUM_CANDIDATES_OFFSET), cand);
   r.wr(FX1_ISP_BPC_CTRL_OFFSET, 0);
   r.frame();  // disabled: cleared at SOF, not counted (assumption, owner query U0-Q4)
   FX1_CHECK_EQ(r.rd(FX1_ISP_BPC_NUM_CANDIDATES_OFFSET), 0);
   FX1_CHECK_EQ(r.rd(FX1_ISP_BPC_NUM_DEFECTIVE_OFFSET), 0);
}

}  // namespace

int main() {
   test_block_busy_and_warnings();
   test_aec_commit();
   test_aec_lease();
   test_tune_mode_with_lease_idle();
   test_aec_numeric();
   test_aec_weighted();
   test_af();
   test_resets();
   test_context_latch();
   test_awb_sampled_at_sof();
   test_awb_cells();
   test_frame_id_abort_irq();
   test_readout_widths();
   test_read_error_edges();
   test_awb_full_and_order();
   test_awb_commands();
   test_awb_report();
   test_awb_grid();
   test_awb_reset_scopes();
   test_bpc_counters();
   return fx1_test::summary("fx1_isp_test_stats");
}
