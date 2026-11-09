// SPDX-License-Identifier: Apache-2.0
// FX1 ISP model -- VP-only debug observation of the post-Demosaic image.
//
// NOT a hardware output: no CSR, DMA, IRQ or DDR buffer, and nothing the
// firmware can see. The tap reads the 12-bit RGB rows at the Demosaic output,
// before CCM (BLC, LSC, BPC, WB, DG and Demosaic applied), from the frame the
// model is processing. Enabling it changes no pixel, register, interrupt,
// ownership or simulated time (docs/ISP_DECISIONS_AND_DISCREPANCIES.md §12).
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cdc::components::fx1_isp {

// Identity and SOF configuration of a frame entering the image pipeline.
struct demosaic_frame_info {
   // Frame key: the model's reset epoch (soft or hard reset count) and the
   // pipeline's frame sequence (accepted SOFs since construction, never reset).
   std::uint64_t epoch = 0;
   std::uint64_t sequence = 0;
   // Ingress frame ID (statistics FRAME_ID counter, also the
   // TUNE_CHn_BUFm_FRAME of this frame's outputs) and active profile ID.
   std::uint32_t frame_id = 0;
   std::uint32_t profile_id = 0;
   // IDMA buffer tag and timestamp of the input (ODMA_BUF_TAG / _TS).
   std::uint32_t input_tag = 0;
   std::uint32_t input_ts = 0;
   double sof_time_ns = 0;
   // IDMA working set and the active image after the Bayer phase trim.
   std::uint32_t input_width = 0, input_height = 0;
   std::uint32_t width = 0, height = 0;
   std::uint32_t trim_x = 0, trim_y = 0;
   unsigned outputs = 0;  // OUTPUT_ENABLE mask of this SOF: bit n = CHn (CH0..CH2 all on: 7)
   // The blocks before the tap, as the frame's configuration snapshot.
   std::uint32_t bayer_pattern = 0;  // COMMON_BAYER code: 0 RGGB, 1 GRBG, 2 GBRG, 3 BGGR
   std::uint32_t input_depth = 0;    // 0 RAW12, 1 RAW10
   bool blc_en = false, lsc_en = false, bpc_en = false, wb_en = false, dg_en = false;
   std::uint32_t wb_gain_r = 0, wb_gain_gr = 0, wb_gain_gb = 0, wb_gain_b = 0;  // UQ4.8
   std::uint32_t dg_gain = 0;                                                  // UQ4.8
   std::uint32_t dms_margin_green = 0, dms_margin_rb = 0;
};

// How a frame ended for its consumers.
enum class demosaic_frame_end {
   completed,  // retired with every enabled output DONE (COMMON_STATUS.FRAME_DONE)
   failed,     // retired without: IDMA abort, ODMA error, unsupported geometry
   reset,      // discarded in flight by a soft or hard reset (recovery included)
};

// Called from the model's simulation processes; must not call wait(). Every
// frame_begin is followed by exactly one frame_end; rows and pipeline_done
// come in between (none of them for a frame abandoned before its first row).
class demosaic_observer {
public:
   virtual ~demosaic_observer() = default;
   virtual void frame_begin(const demosaic_frame_info &f) = 0;
   // Row y of 0..height-1: 3 * width components R, G, B of 12 bits, valid
   // during the call only.
   virtual void row(std::uint64_t sequence, std::uint32_t y, const std::uint16_t *rgb) = 0;
   // All rows of the frame have passed the tap and the pipeline completed it.
   virtual void pipeline_done(std::uint64_t sequence) = 0;
   virtual void frame_end(std::uint64_t sequence, demosaic_frame_end how) = 0;
};

const char *to_string(demosaic_frame_end how);

// Writer used by fx1_isp_run_raw and fx1_soc. Each frame streams into
// temporary files (<base>.rgb16.part / .png.part); a completed frame is
// published by rename as
//   <base>.rgb16  R, G, B interleaved, unsigned LE16 per component, 12
//                 significant bits [11:0], no header, no row padding
//   <base>.png    RGB8 preview, v8 = v12 >> 4 (no WB/gamma/tone/auto-level)
//   <base>.json   stage, frame key, geometry, layout, SHA-256, SOF snapshot
// and any other end removes its temporary files. Every frame end is also
// appended to <dir>/index.jsonl. A host I/O error never reaches the model:
// that frame is dropped and recorded, and ok() turns false.
// The stream ends with a single call: on a model reset or a DMA recovery the
// frame ends "frame_reset" (IDMA/ODMA AXI errors start a recovery, MAS v1.07
// §9.9.8); "frame_failed" is a pipeline that dropped the frame itself.
class demosaic_dump : public demosaic_observer {
public:
   struct capture {
      std::uint64_t ordinal = 0;  // 1, 2, ... in frame_begin order
      demosaic_frame_info info;
      // to_string(end); "incomplete": no end by finish(); "capture_error": a
      // host I/O error or a broken call sequence (see errors()), nothing published.
      std::string status;
      std::string base;    // published files without extension (completed only)
      std::string sha256;  // of <base>.rgb16 (completed only)
   };
   // Base path of a frame's files; default: <dir>/frames/f<ordinal 4 digits>_e<epoch>_s<sequence>.
   using namer = std::function<std::string(const capture &)>;

   // Creates <dir> and <dir>/frames; <dir> must be new or empty (one run per
   // directory: keys restart with the model). Throws std::runtime_error if it
   // cannot create it or <dir> already has files.
   explicit demosaic_dump(const std::string &dir, namer name = nullptr);
   ~demosaic_dump() override;

   void frame_begin(const demosaic_frame_info &f) override;
   void row(std::uint64_t sequence, std::uint32_t y, const std::uint16_t *rgb) override;
   void pipeline_done(std::uint64_t sequence) override;
   void frame_end(std::uint64_t sequence, demosaic_frame_end how) override;

   // End of run: frames without an end are dropped as "incomplete". Idempotent.
   void finish();
   const std::vector<capture> &captures() const;
   bool ok() const;  // no host I/O error so far
   const std::vector<std::string> &errors() const;

   // Copies a published frame's three files to another base path (the JSON
   // names no file, so the copy stays valid), replacing an existing set. The
   // JSON is the set's commit marker: on an I/O error the destination is
   // either the old set unchanged (copy failed) or has no JSON (rename
   // failed), never a JSON next to data it does not describe. Throws
   // std::runtime_error on an I/O error.
   static void copy_capture(const std::string &from_base, const std::string &to_base);

private:
   struct impl;
   std::unique_ptr<impl> impl_;
};

}  // namespace cdc::components::fx1_isp
