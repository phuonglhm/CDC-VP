// SPDX-License-Identifier: Apache-2.0
// VP-only post-Demosaic debug tap (fx1_isp/fx1_isp_debug.h, decisions §12):
//  - pipeline level: one row per active row at the Demosaic output (the flush
//    rows of finish() included), before CCM: CCM/Gamma changes do not change
//    the capture but change the output, WB does change it; a flat Bayer field
//    gives the hand-computed R, G, B for all four patterns, also with an odd
//    input size (phase trim); the tap does not change the output;
//  - TLM: the frame lifecycle the observer sees (begin, rows, pipeline done,
//    one end), "completed" only at retirement with every output DONE (CH1/CH2
//    crops end before the shared IQ), failure, soft and hard reset, unique
//    frame keys; the observer must be set before the simulation;
//  - the writer (demosaic_dump): RGB16 bytes, PNG, JSON SHA-256 and index,
//    nothing published for a frame that did not complete, a host I/O error
//    recorded without any effect on the model.
//   fx1_isp_test_demosaic_tlm OUT_DIR

#include <sys/stat.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "control/control_unit.h"
#include "debug/sha256.h"
#include "engine/engines.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_debug.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "pipeline/isp_pipeline.h"
#include "test_memory.h"

using namespace sc_core;
using cdc::components::fx1_isp::control_unit;
using cdc::components::fx1_isp::demosaic_dump;
using cdc::components::fx1_isp::demosaic_frame_end;
using cdc::components::fx1_isp::demosaic_frame_info;
using cdc::components::fx1_isp::demosaic_observer;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;
using cdc::components::fx1_isp::num_channels;
namespace pipe = cdc::components::fx1_isp::pipe;

#define CHECK_CTX(cond, ctx) ::fx1_test::report(static_cast<bool>(cond), #cond, __FILE__, __LINE__, std::string(ctx))
#define CHECK_STR(actual, expected)                                                                     \
   do {                                                                                                 \
      const std::string fx1_a_ = (actual), fx1_e_ = (expected);                                         \
      ::fx1_test::report(fx1_a_ == fx1_e_, #actual " == " #expected, __FILE__, __LINE__,                \
                         "got '" + fx1_a_ + "', expected '" + fx1_e_ + "'");                            \
   } while (0)

namespace {

// ---------------------------------------------------------------- pipeline level

using image = std::vector<std::uint16_t>;  // interleaved R, G, B
using sample_fn = std::function<std::uint16_t(std::uint32_t x, std::uint32_t y)>;

pipe::pipeline_config default_config(std::uint32_t bayer) {
   control_unit cu;
   cu.csr_write(FX1_ISP_COMMON_BAYER_OFFSET, bayer, 0xFFFFFFFFu, 1);
   cu.accepted_sof(1000);
   return pipe::snapshot_config(cu);
}

struct pipeline_run {
   image capture;
   std::vector<std::uint32_t> rows;  // y of every tap call, in order
   std::size_t rows_in_finish = 0;
   std::uint32_t width = 0, height = 0;  // active
   std::vector<std::uint8_t> nv12;
};

pipeline_run run_pipeline(const pipe::pipeline_config &cfg, std::uint32_t w, std::uint32_t h, const sample_fn &s,
                          bool tap) {
   pipeline_run r;
   pipe::isp_pipeline p;
   bool in_finish = false;
   if (tap) {
      p.set_rgb_tap([&](std::uint32_t y, const std::vector<pipe::rgb> &row) {
         r.rows.push_back(y);
         r.rows_in_finish += in_finish;
         FX1_CHECK_EQ_CTX(row.size(), std::size_t{r.width}, "tap row width");
         for (const pipe::rgb &px : row) {
            r.capture.insert(r.capture.end(), {px.r, px.g, px.b});
         }
      });
   }
   std::vector<std::uint8_t> uv;
   p.begin(cfg, w, h, [&](std::vector<std::uint8_t> &&b) { r.nv12.insert(r.nv12.end(), b.begin(), b.end()); },
           [&](std::vector<std::uint8_t> &&b) { uv.insert(uv.end(), b.begin(), b.end()); });
   r.width = p.active().width;
   r.height = p.active().height;
   std::vector<std::uint16_t> line(w);
   for (std::uint32_t y = 0; y < h; ++y) {
      for (std::uint32_t x = 0; x < w; ++x) {
         line[x] = s(x, y);
      }
      p.push(line.data());
   }
   in_finish = true;
   p.finish();
   r.nv12.insert(r.nv12.end(), uv.begin(), uv.end());
   return r;
}

std::uint16_t texture(std::uint32_t x, std::uint32_t y) {
   return static_cast<std::uint16_t>((x * 37u + y * 91u + ((x ^ y) & 7u) * 211u) & 0xFFFu);
}

// One tap call per active row, rows 0..H-1 in order, the Demosaic flush rows
// delivered by finish(); the tap does not change the output.
void test_tap_rows() {
   const pipe::pipeline_config cfg = default_config(0);
   const pipeline_run on = run_pipeline(cfg, 64, 32, texture, true);
   const pipeline_run off = run_pipeline(cfg, 64, 32, texture, false);
   FX1_CHECK_EQ(on.rows.size(), std::size_t{32});
   for (std::size_t i = 0; i < on.rows.size(); ++i) {
      FX1_CHECK_EQ_CTX(on.rows[i], static_cast<std::uint32_t>(i), "tap row order");
   }
   FX1_CHECK(on.rows_in_finish > 0);  // the last rows come from the Demosaic flush
   FX1_CHECK_EQ(on.capture.size(), std::size_t{3} * 64 * 32);
   FX1_CHECK(on.nv12 == off.nv12);
   FX1_CHECK(off.capture.empty());
}

// The tap is before CCM: CCM and Gamma change the output, not the capture;
// WB (before the tap) changes both.
void test_tap_position() {
   const pipe::pipeline_config base = default_config(0);
   const pipeline_run ref = run_pipeline(base, 64, 32, texture, true);
   pipe::pipeline_config ccm = base;
   ccm.ccm_en = true;
   ccm.ccm = {{{{0x300, -0x80, -0x80}}, {{-0x40, 0x280, -0x40}}, {{0, -0x100, 0x300}}}};  // Q3.9
   const pipeline_run c = run_pipeline(ccm, 64, 32, texture, true);
   FX1_CHECK(c.capture == ref.capture);
   FX1_CHECK(c.nv12 != ref.nv12);
   static std::array<std::uint16_t, 256> lut{};
   for (unsigned i = 0; i < 256; ++i) {
      lut[i] = static_cast<std::uint16_t>(4095u - 16u * i);  // inverted curve
   }
   pipe::pipeline_config gamma = base;
   gamma.gamma_en = true;
   gamma.gamma_lut = &lut;
   const pipeline_run g = run_pipeline(gamma, 64, 32, texture, true);
   FX1_CHECK(g.capture == ref.capture);
   FX1_CHECK(g.nv12 != ref.nv12);
   pipe::pipeline_config wb = base;
   wb.wb_en = true;
   wb.wb_gain = {0x180, 0x100, 0x0C0};
   wb.wb_gain_gb = 0x100;
   const pipeline_run b = run_pipeline(wb, 64, 32, texture, true);
   FX1_CHECK(b.capture != ref.capture);
   FX1_CHECK(b.nv12 != ref.nv12);
}

// A flat Bayer field (one value per CFA colour, Gr = Gb) demosaics to the
// same R, G, B everywhere: hand-computed through BLC, WB and DG, for every
// pattern and an odd input size whose phase trim drops a row and a column.
void test_flat_field() {
   const std::uint16_t raw[3] = {1000, 2000, 3000};  // R, G, B before BLC/WB/DG
   for (std::uint32_t bayer = 0; bayer < 4; ++bayer) {
      for (const std::pair<std::uint32_t, std::uint32_t> &size :
           {std::pair<std::uint32_t, std::uint32_t>{64, 32}, {67, 35}}) {
         const std::string ctx = "pattern " + std::to_string(bayer) + " " + std::to_string(size.first) + "x" +
                                 std::to_string(size.second);
         pipe::pipeline_config cfg = default_config(bayer);
         cfg.blc_en = true;
         cfg.blc_pedestal = 64;
         cfg.wb_en = true;
         cfg.wb_gain = {0x1A0, 0x100, 0x150};
         cfg.wb_gain_gb = 0x100;
         cfg.dg_en = true;
         cfg.dg_gain = 0x110;
         const std::uint32_t sh = bayer & 1u, sv = bayer >> 1;
         // CFA colour at raw (x, y): the phase is RGGB from (sh, sv) on.
         const auto colour = [sh, sv](std::uint32_t x, std::uint32_t y) {
            const unsigned ch = 2u * ((y + sv) & 1u) + ((x + sh) & 1u);  // 0 R, 1 Gr, 2 Gb, 3 B
            return ch;
         };
         const pipeline_run r = run_pipeline(
             cfg, size.first, size.second,
             [&](std::uint32_t x, std::uint32_t y) {
                const unsigned ch = colour(x, y);
                return raw[ch == 0 ? 0 : ch == 3 ? 2 : 1];
             },
             true);
         FX1_CHECK_EQ_CTX(r.width, (size.first - sh) / 2u * 2u, ctx + " active width");
         FX1_CHECK_EQ_CTX(r.height, (size.second - sv) / 2u * 2u, ctx + " active height");
         std::array<std::uint16_t, 3> want{};
         const unsigned chan_of[3] = {0, 1, 3};
         for (unsigned c = 0; c < 3; ++c) {
            const unsigned ch = chan_of[c];
            want[c] = pipe::dg_pixel(cfg, pipe::wb_pixel(cfg, pipe::blc_pixel(cfg, raw[c], ch), ch));
         }
         FX1_CHECK(want[0] != want[1] && want[1] != want[2]);  // distinct: order is observable
         std::size_t bad = 0;
         for (std::size_t i = 0; i < r.capture.size(); ++i) {
            bad += r.capture[i] != want[i % 3u];
         }
         FX1_CHECK_EQ_CTX(r.capture.size(), std::size_t{3} * r.width * r.height, ctx + " size");
         FX1_CHECK_EQ_CTX(bad, std::size_t{0}, ctx + " R, G, B");
      }
   }
}

// ---------------------------------------------------------------- writer level

std::vector<std::uint8_t> read_file(const std::string &p);
bool exists(const std::string &p);
std::vector<std::string> read_lines(const std::string &p);

std::string status_of(const demosaic_dump &d, std::size_t i) { return d.captures().at(i).status; }

// The writer's end statuses that the TLM cannot reach on demand: "failed"
// (the pipeline drops a frame itself), a broken call sequence, a 13-bit
// component, a frame open at finish(); none publishes a file.
void test_writer_contract(const std::string &dir) {
   demosaic_dump d(dir);
   demosaic_frame_info f;
   f.width = 4;
   f.height = 4;
   const std::vector<std::uint16_t> row(12, 0x123);
   const auto frame = [&](std::uint64_t seq) {
      f.sequence = seq;
      d.frame_begin(f);
   };
   frame(1);  // failed after every row
   for (std::uint32_t y = 0; y < 4; ++y) d.row(1, y, row.data());
   d.pipeline_done(1);
   d.frame_end(1, demosaic_frame_end::failed);
   frame(2);  // a row out of order
   d.row(2, 1, row.data());
   d.frame_end(2, demosaic_frame_end::completed);
   frame(3);  // completed although the pipeline did not complete it
   for (std::uint32_t y = 0; y < 3; ++y) d.row(3, y, row.data());
   d.frame_end(3, demosaic_frame_end::completed);
   frame(4);  // 13-bit component
   std::vector<std::uint16_t> big = row;
   big[5] = 0x1000;
   d.row(4, 0, big.data());
   d.frame_end(4, demosaic_frame_end::completed);
   frame(5);  // good
   for (std::uint32_t y = 0; y < 4; ++y) d.row(5, y, row.data());
   d.pipeline_done(5);
   d.frame_end(5, demosaic_frame_end::completed);
   d.row(5, 0, row.data());  // after the end: ignored
   frame(6);  // never ends
   d.row(6, 0, row.data());
   frame(7);  // good, other values
   const std::vector<std::uint16_t> row7(12, 0x456);
   for (std::uint32_t y = 0; y < 4; ++y) d.row(7, y, row7.data());
   d.pipeline_done(7);
   d.frame_end(7, demosaic_frame_end::completed);
   d.finish();
   CHECK_STR(status_of(d, 0), "frame_failed");
   CHECK_STR(status_of(d, 1), "capture_error");
   CHECK_STR(status_of(d, 2), "capture_error");
   CHECK_STR(status_of(d, 3), "capture_error");
   CHECK_STR(status_of(d, 4), "frame_completed");
   CHECK_STR(status_of(d, 5), "incomplete");
   CHECK_STR(status_of(d, 6), "frame_completed");
   FX1_CHECK(!d.ok());
   FX1_CHECK_EQ(d.errors().size(), 3u);
   FX1_CHECK_EQ(read_lines(dir + "/index.jsonl").size(), 7u);
   for (const demosaic_dump::capture &c : d.captures()) {
      char b[96];
      std::snprintf(b, sizeof b, "/frames/f%04llu_e0_s%llu", static_cast<unsigned long long>(c.ordinal),
                    static_cast<unsigned long long>(c.info.sequence));
      const bool published = c.status == "frame_completed";
      CHECK_CTX(exists(dir + b + ".rgb16") == published && exists(dir + b + ".png") == published &&
                    exists(dir + b + ".json") == published && !exists(dir + b + ".rgb16.part") &&
                    !exists(dir + b + ".png.part"),
                c.status);
   }
   FX1_CHECK_EQ(read_file(d.captures()[4].base + ".rgb16").size(), 4u * 4u * 6u);

   // copy_capture replaces a set as a whole (review R3): a, then b over it.
   const std::string a = d.captures()[4].base, b = d.captures()[6].base, sel = dir + "/selected";
   const auto same_set = [&](const std::string &x, const std::string &y) {
      return read_file(x + ".rgb16") == read_file(y + ".rgb16") && read_file(x + ".png") == read_file(y + ".png") &&
             read_file(x + ".json") == read_file(y + ".json");
   };
   const auto no_parts = [&] {
      return !exists(sel + ".rgb16.part") && !exists(sel + ".png.part") && !exists(sel + ".json.part");
   };
   FX1_CHECK(read_file(a + ".rgb16") != read_file(b + ".rgb16"));
   demosaic_dump::copy_capture(a, sel);
   FX1_CHECK(same_set(sel, a));
   // The second file cannot be staged: the old set stays, whole.
   ::mkdir((sel + ".png.part").c_str(), 0755);
   bool threw = false;
   try {
      demosaic_dump::copy_capture(b, sel);
   } catch (const std::runtime_error &) {
      threw = true;
   }
   std::remove((sel + ".png.part").c_str());  // POSIX remove: an empty directory too
   FX1_CHECK(threw);
   FX1_CHECK(same_set(sel, a));
   FX1_CHECK(no_parts());
   // The second rename fails (a directory in the way): no JSON is left.
   std::remove((sel + ".png").c_str());
   ::mkdir((sel + ".png").c_str(), 0755);
   threw = false;
   try {
      demosaic_dump::copy_capture(b, sel);
   } catch (const std::runtime_error &) {
      threw = true;
   }
   std::remove((sel + ".png").c_str());  // POSIX remove: an empty directory too
   FX1_CHECK(threw);
   FX1_CHECK(!exists(sel + ".json"));
   FX1_CHECK(!exists(sel + ".rgb16"));
   FX1_CHECK(no_parts());
   demosaic_dump::copy_capture(b, sel);
   FX1_CHECK(same_set(sel, b));

   // One run per directory (review R2): a used directory is refused and
   // left as it is; an existing empty one is accepted.
   threw = false;
   try {
      demosaic_dump again(dir);
   } catch (const std::runtime_error &) {
      threw = true;
   }
   FX1_CHECK(threw);
   FX1_CHECK(same_set(sel, b));
   FX1_CHECK(exists(a + ".json") && read_lines(dir + "/index.jsonl").size() == 7u);
   const std::string empty = dir + "/empty run";
   ::mkdir(empty.c_str(), 0755);
   threw = false;
   try {
      demosaic_dump fresh(empty);
   } catch (const std::runtime_error &) {
      threw = true;
   }
   FX1_CHECK(!threw);
   FX1_CHECK(exists(empty + "/frames") && exists(empty + "/index.jsonl"));
}

// ---------------------------------------------------------------- TLM level

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 16u << 20;
constexpr std::uint32_t W = 64, H = 48;

struct chan {
   std::uint32_t w, h, stride;
   std::uint64_t y, uv;
   std::uint32_t free, y_stride, uv_stride, y_addr0, uv_addr0;
};
const std::array<chan, num_channels> chans = {{
   {W, H, W, mem_base + 0x100000ull, mem_base + 0x200000ull, FX1_ISP_ODMA_BUF_FREE_OFFSET,
    FX1_ISP_ODMA_Y_STRIDE_OFFSET, FX1_ISP_ODMA_UV_STRIDE_OFFSET, FX1_ISP_ODMA_Y_ADDR_L0_OFFSET,
    FX1_ISP_ODMA_UV_ADDR_L0_OFFSET},
   {32, 24, 32, mem_base + 0x300000ull, mem_base + 0x400000ull, FX1_ISP_CH1_ODMA_BUF_FREE_OFFSET,
    FX1_ISP_CH1_ODMA_Y_STRIDE_OFFSET, FX1_ISP_CH1_ODMA_UV_STRIDE_OFFSET, FX1_ISP_CH1_ODMA_Y_ADDR_L0_OFFSET,
    FX1_ISP_CH1_ODMA_UV_ADDR_L0_OFFSET},
   {32, 32, 96, mem_base + 0x500000ull, 0, FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET, FX1_ISP_CH2_ODMA_Y_STRIDE_OFFSET, 0,
    FX1_ISP_CH2_ODMA_Y_ADDR_L0_OFFSET, 0},
}};
std::uint64_t plane(std::uint64_t base, unsigned k) { return base + 0x40000ull * k; }
std::uint64_t input(unsigned k) { return mem_base + 0x10000ull * k; }

// CH1 crop rows 4..27 and CH2 crop rows 8..39 of 48: both outputs end before
// the shared IQ has consumed the last rows (BGGR: active 62 x 46).
const std::vector<std::pair<std::uint32_t, std::uint32_t>> config = {
   {FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, W},           {FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, H},
   {FX1_ISP_COMMON_BAYER_OFFSET, 3},                 {FX1_ISP_WB_CTRL_OFFSET, FX1_ISP_WB_CTRL_EN_MASK},
   {FX1_ISP_WB_GAIN_R_OFFSET, 0x1C0},                {FX1_ISP_WB_GAIN_B_OFFSET, 0x140},
   {FX1_ISP_CH1_CROP_ORIGIN_OFFSET, 8 | (4u << 16)}, {FX1_ISP_CH1_CROP_SIZE_OFFSET, 32 | (24u << 16)},
   {FX1_ISP_CH1_CROP_CTRL_OFFSET, 1},                {FX1_ISP_CH2_CROP_ORIGIN_OFFSET, 16 | (8u << 16)},
   {FX1_ISP_CH2_CROP_SIZE_OFFSET, 32 | (32u << 16)}, {FX1_ISP_CH2_CROP_CTRL_OFFSET, 1},
};

std::uint16_t sample(unsigned tag, std::uint32_t x, std::uint32_t y) {
   return static_cast<std::uint16_t>((x * 37u + y * 91u + tag * 513u) & 0xFFFu);
}

// The capture the stand-alone pipeline taps on the same configuration.
image expected_capture(unsigned tag) {
   control_unit cu;
   std::uint64_t t = 1;
   for (const auto &w : config) {
      cu.csr_write(w.first, w.second, 0xFFFFFFFFu, t++);
   }
   cu.accepted_sof(1000);
   return run_pipeline(pipe::snapshot_config(cu), W, H, [tag](std::uint32_t x, std::uint32_t y) { return sample(tag, x, y); },
                       true)
       .capture;
}

// Records what the model reports and checks the call protocol.
struct recorder : demosaic_observer {
   struct frame {
      demosaic_frame_info info;
      image img;
      std::uint32_t rows = 0;
      bool done = false, ended = false;
      demosaic_frame_end how = demosaic_frame_end::failed;
      std::uint32_t status_at_end = 0;  // COMMON_STATUS when the end is reported
   };
   fx1_isp_tlm *dut = nullptr;
   std::map<std::uint64_t, frame> frames;
   std::vector<std::uint64_t> order;  // sequences in begin order
   unsigned protocol_errors = 0;

   void frame_begin(const demosaic_frame_info &f) override {
      protocol_errors += frames.count(f.sequence) ? 1u : 0u;
      frames[f.sequence].info = f;
      order.push_back(f.sequence);
   }
   void row(std::uint64_t s, std::uint32_t y, const std::uint16_t *rgb) override {
      frame &f = frames.at(s);
      protocol_errors += f.ended || f.done || y != f.rows;
      f.img.insert(f.img.end(), rgb, rgb + 3u * f.info.width);
      ++f.rows;
   }
   void pipeline_done(std::uint64_t s) override {
      frame &f = frames.at(s);
      protocol_errors += f.ended || f.done || f.rows != f.info.height;
      f.done = true;
   }
   void frame_end(std::uint64_t s, demosaic_frame_end how) override {
      frame &f = frames.at(s);
      protocol_errors += f.ended;
      f.ended = true;
      f.how = how;
      f.status_at_end = dut->debug_peek(FX1_ISP_COMMON_STATUS_OFFSET);
   }
   const frame &last() const { return frames.at(order.back()); }
};

struct tee : demosaic_observer {
   std::vector<demosaic_observer *> to;
   void frame_begin(const demosaic_frame_info &f) override {
      for (auto *o : to) o->frame_begin(f);
   }
   void row(std::uint64_t s, std::uint32_t y, const std::uint16_t *rgb) override {
      for (auto *o : to) o->row(s, y, rgb);
   }
   void pipeline_done(std::uint64_t s) override {
      for (auto *o : to) o->pipeline_done(s);
   }
   void frame_end(std::uint64_t s, demosaic_frame_end how) override {
      for (auto *o : to) o->frame_end(s, how);
   }
};

std::vector<std::uint8_t> read_file(const std::string &p) {
   std::ifstream f(p, std::ios::binary);
   return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(f), {});
}
bool exists(const std::string &p) {
   struct stat st {};
   return ::stat(p.c_str(), &st) == 0;
}
std::uint32_t be32(const std::uint8_t *p) {
   return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) | p[3];
}

// Independent PNG reader for the writer's subset (RGB8, stored deflate):
// returns the scanline bytes without filter bytes, empty on any defect.
std::vector<std::uint8_t> png_rgb8(const std::vector<std::uint8_t> &f, std::uint32_t &w, std::uint32_t &h) {
   static const std::uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
   if (f.size() < 8 || std::memcmp(f.data(), sig, 8) != 0) {
      return {};
   }
   std::vector<std::uint8_t> z;
   bool end = false;
   for (std::size_t at = 8; at + 12 <= f.size() && !end;) {
      const std::uint32_t len = be32(&f[at]);
      const std::string type(reinterpret_cast<const char *>(&f[at + 4]), 4);
      if (at + 12 + len > f.size()) {
         return {};
      }
      std::uint32_t crc = 0xFFFFFFFFu;
      for (std::size_t i = at + 4; i < at + 8 + len; ++i) {
         crc ^= f[i];
         for (int k = 0; k < 8; ++k) {
            crc = (crc & 1u) ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
         }
      }
      if ((crc ^ 0xFFFFFFFFu) != be32(&f[at + 8 + len])) {
         return {};
      }
      const std::uint8_t *d = &f[at + 8];
      if (type == "IHDR") {
         w = be32(d);
         h = be32(d + 4);
         if (len != 13 || d[8] != 8 || d[9] != 2 || d[12] != 0) {
            return {};
         }
      } else if (type == "IDAT") {
         z.insert(z.end(), d, d + len);
      } else if (type == "IEND") {
         end = true;
      }
      at += 12 + len;
   }
   if (!end || z.size() < 6 || z[0] != 0x78 || ((z[0] << 8) | z[1]) % 31 != 0) {
      return {};
   }
   std::vector<std::uint8_t> raw;
   std::size_t at = 2;
   for (bool final_block = false; !final_block;) {
      if (at + 5 > z.size() || (z[at] & 0x06) != 0) {
         return {};  // stored blocks only
      }
      final_block = z[at] & 1u;
      const unsigned len = z[at + 1] | (z[at + 2] << 8), nlen = z[at + 3] | (z[at + 4] << 8);
      if ((len ^ 0xFFFFu) != nlen || at + 5 + len > z.size()) {
         return {};
      }
      raw.insert(raw.end(), z.begin() + static_cast<std::ptrdiff_t>(at + 5),
                 z.begin() + static_cast<std::ptrdiff_t>(at + 5 + len));
      at += 5 + len;
   }
   std::uint32_t a = 1, b = 0;
   for (const std::uint8_t v : raw) {
      a = (a + v) % 65521u;
      b = (b + a) % 65521u;
   }
   if (at + 4 != z.size() || be32(&z[at]) != ((b << 16) | a) || raw.size() != std::size_t{h} * (1u + 3u * w)) {
      return {};
   }
   std::vector<std::uint8_t> px;
   for (std::uint32_t y = 0; y < h; ++y) {
      const std::size_t row = std::size_t{y} * (1u + 3u * w);
      if (raw[row] != 0) {
         return {};
      }
      px.insert(px.end(), raw.begin() + static_cast<std::ptrdiff_t>(row + 1),
                raw.begin() + static_cast<std::ptrdiff_t>(row + 1 + 3u * w));
   }
   return px;
}

std::vector<std::string> read_lines(const std::string &p) {
   std::ifstream f(p);
   std::vector<std::string> v;
   for (std::string l; std::getline(f, l);) {
      v.push_back(l);
   }
   return v;
}

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   sc_out<bool> rst_n;
   sc_in<bool> irq;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   recorder *rec = nullptr;
   demosaic_dump *dump = nullptr;
   std::string dir;
   bool finished = false;

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n"), irq("irq") { SC_THREAD(run); }

   void access(tlm::tlm_command cmd, std::uint32_t addr, std::uint32_t &v) {
      unsigned char b[4];
      std::memcpy(b, &v, 4);
      tlm::tlm_generic_payload t;
      t.set_command(cmd);
      t.set_address(addr);
      t.set_data_ptr(b);
      t.set_data_length(4);
      t.set_streaming_width(4);
      sc_time d = SC_ZERO_TIME;
      socket->b_transport(t, d);
      FX1_CHECK(t.is_response_ok());
      wait(d);
      std::memcpy(&v, b, 4);
   }
   void wr(std::uint32_t a, std::uint32_t v) { access(tlm::TLM_WRITE_COMMAND, a, v); }
   std::uint32_t rd(std::uint32_t a) {
      std::uint32_t v = 0;
      access(tlm::TLM_READ_COMMAND, a, v);
      return v;
   }
   std::uint32_t peek(std::uint32_t a) { return dut->debug_peek(a); }
   bool wait_until(const std::function<bool()> &cond, const sc_time &limit = sc_time(20, SC_MS)) {
      const sc_time end = sc_time_stamp() + limit;
      while (!cond()) {
         if (sc_time_stamp() >= end) {
            return false;
         }
         wait(1, SC_US);
      }
      return true;
   }
   bool idle() {
      return wait_until([&] { return (peek(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) == 0; });
   }
   bool ended(std::size_t frames) {
      return wait_until([&] { return rec->order.size() >= frames && rec->last().ended; });
   }

   void reset_and_program(std::uint32_t outputs, std::uint32_t height = H) {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      for (const auto &w : config) {
         wr(w.first, w.second);
      }
      wr(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, height);
      wr(FX1_ISP_OUTPUT_ENABLE_OFFSET, outputs);
      wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      wait(dut->params().core_period * 80.0);
      for (unsigned k = 0; k < 4; ++k) {
         wr(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, static_cast<std::uint32_t>(input(k)));
         for (unsigned c = 0; c < num_channels; ++c) {
            wr(chans[c].y_addr0 + 16u * k, static_cast<std::uint32_t>(plane(chans[c].y, k)));
            if (chans[c].uv) {
               wr(chans[c].uv_addr0 + 16u * k, static_cast<std::uint32_t>(plane(chans[c].uv, k)));
            }
         }
      }
      wr(FX1_ISP_IDMA_STRIDE_OFFSET, 2 * W);
      for (unsigned c = 0; c < num_channels; ++c) {
         wr(chans[c].y_stride, chans[c].stride);
         if (chans[c].uv) {
            wr(chans[c].uv_stride, chans[c].stride);
         }
      }
      wr(FX1_ISP_DMA_CTRL_OFFSET, 0x3F03);
   }
   void load(unsigned k, unsigned tag) {
      for (std::uint32_t y = 0; y < H; ++y) {
         for (std::uint32_t x = 0; x < W; ++x) {
            mem->put16(input(k) + 2u * (y * W + x), sample(tag, x, y));
         }
      }
      wr(FX1_ISP_IDMA_BUF_TAG0_OFFSET + 4u * k, 0x7A600 + tag);
   }
   // Every frame end so far in the index, one line each, with this status.
   std::string index_status(std::size_t line) {
      const std::vector<std::string> l = read_lines(dir + "/index.jsonl");
      if (line >= l.size()) {
         return "";
      }
      const std::size_t at = l[line].find("\"status\": \"");
      return at == std::string::npos ? "" : l[line].substr(at + 11, l[line].find('"', at + 11) - at - 11);
   }
   // No temporary file left in <dir>/frames.
   bool no_part_files() {
      for (const demosaic_dump::capture &c : dump->captures()) {
         char b[96];
         std::snprintf(b, sizeof b, "/frames/f%04llu_e%llu_s%llu", static_cast<unsigned long long>(c.ordinal),
                       static_cast<unsigned long long>(c.info.epoch),
                       static_cast<unsigned long long>(c.info.sequence));
         if (exists(dir + b + ".rgb16.part") || exists(dir + b + ".png.part") || exists(dir + b + ".json.part")) {
            return false;
         }
         if (c.status != "frame_completed" && (exists(dir + b + ".rgb16") || exists(dir + b + ".json"))) {
            return false;
         }
      }
      return true;
   }

   // All three outputs, crops ending early: completed only at retirement
   // (FRAME_DONE set), after all rows; the capture equals the pipeline's and
   // the files are exact.
   void test_completed() {
      reset_and_program(7);
      for (const chan &c : chans) {
         wr(c.free, 0x1);
      }
      load(0, 1);
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(ended(1));
      FX1_CHECK(idle());
      const recorder::frame &f = rec->last();
      FX1_CHECK(f.how == demosaic_frame_end::completed);
      FX1_CHECK(f.done);
      FX1_CHECK_EQ(f.status_at_end & FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK);
      FX1_CHECK_EQ(f.info.width, 62u);  // BGGR phase trim
      FX1_CHECK_EQ(f.info.height, 46u);
      FX1_CHECK_EQ(f.info.trim_x, 1u);
      FX1_CHECK_EQ(f.info.input_width, W);
      FX1_CHECK_EQ(f.info.outputs, 7u);  // the OUTPUT_ENABLE mask, not the number of channels
      FX1_CHECK_EQ(f.info.input_tag, 0x7A601u);
      FX1_CHECK_EQ(f.info.frame_id, rd(FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET));
      FX1_CHECK_EQ(f.info.wb_gain_r, 0x1C0u);
      FX1_CHECK(f.info.wb_en);
      FX1_CHECK(f.img == expected_capture(1));
      // Files: RGB16 LE, PNG = v >> 4, JSON SHA-256 of the data, index line.
      const demosaic_dump::capture &c = dump->captures().back();
      CHECK_STR(c.status, std::string("frame_completed"));
      const std::vector<std::uint8_t> data = read_file(c.base + ".rgb16");
      FX1_CHECK_EQ(data.size(), f.img.size() * 2u);
      std::size_t bad = 0;
      for (std::size_t i = 0; i < f.img.size() && 2u * i + 1u < data.size(); ++i) {
         bad += (data[2u * i] | (data[2u * i + 1u] << 8)) != f.img[i];
      }
      FX1_CHECK_EQ(bad, std::size_t{0});
      std::uint32_t pw = 0, ph = 0;
      const std::vector<std::uint8_t> px = png_rgb8(read_file(c.base + ".png"), pw, ph);
      FX1_CHECK_EQ(pw, f.info.width);
      FX1_CHECK_EQ(ph, f.info.height);
      FX1_CHECK_EQ(px.size(), f.img.size());
      bad = 0;
      for (std::size_t i = 0; i < f.img.size() && i < px.size(); ++i) {
         bad += px[i] != (f.img[i] >> 4);
      }
      FX1_CHECK_EQ(bad, std::size_t{0});
      cdc::components::fx1_isp::debug::sha256 sha;
      sha.update(data.data(), data.size());
      const std::string digest = sha.hex();
      CHECK_STR(c.sha256, digest);
      const std::vector<std::uint8_t> json = read_file(c.base + ".json");
      const std::string j(json.begin(), json.end());
      FX1_CHECK(j.find("\"sha256\": \"" + digest + "\"") != std::string::npos);
      FX1_CHECK(j.find("\"status\": \"frame_completed\"") != std::string::npos);
      FX1_CHECK(j.find("\"stage\": \"post_demosaic_pre_ccm\"") != std::string::npos);
      FX1_CHECK(j.find("\"width\": 62, \"height\": 46") != std::string::npos);
      FX1_CHECK(j.find("\"outputs\": 7}") != std::string::npos);
      CHECK_STR(index_status(dump->captures().size() - 1), std::string("frame_completed"));
      // copy_capture: byte copies.
      demosaic_dump::copy_capture(c.base, dir + "/copy");
      FX1_CHECK(read_file(dir + "/copy.rgb16") == data);
      FX1_CHECK(read_file(dir + "/copy.json") == json);
      FX1_CHECK(no_part_files());
   }

   // The metadata keeps the OUTPUT_ENABLE mask of each SOF (a single CH2 is
   // 4, not 1); every mask completes with the same shared capture.
   void test_output_masks() {
      for (const std::uint32_t mask : {1u, 2u, 4u, 5u, 6u, 7u}) {
         const std::string ctx = "mask " + std::to_string(mask);
         reset_and_program(mask);
         for (unsigned c = 0; c < num_channels; ++c) {
            if (mask & (1u << c)) {
               wr(chans[c].free, 0x1);
            }
         }
         load(0, 10 + mask);
         const std::size_t before = rec->order.size();
         wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
         CHECK_CTX(ended(before + 1), ctx);
         CHECK_CTX(idle(), ctx);
         const recorder::frame &f = rec->last();
         FX1_CHECK_EQ_CTX(f.info.outputs, mask, ctx);
         CHECK_CTX(f.how == demosaic_frame_end::completed, ctx);
         CHECK_CTX(f.img == expected_capture(10 + mask), ctx);
      }
   }

   // Two frames in flight: distinct keys and images, both completed in order.
   void test_two_frames() {
      reset_and_program(1);
      wr(chans[0].free, 0x3);
      const std::size_t before = rec->order.size();
      load(0, 2);
      load(1, 3);
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x3);
      FX1_CHECK(ended(before + 2));
      FX1_CHECK(idle());
      const recorder::frame &a = rec->frames.at(rec->order[before]);
      const recorder::frame &b = rec->frames.at(rec->order[before + 1]);
      FX1_CHECK(a.how == demosaic_frame_end::completed && b.how == demosaic_frame_end::completed);
      FX1_CHECK(a.info.sequence + 1 == b.info.sequence);
      FX1_CHECK_EQ(a.info.epoch, b.info.epoch);
      FX1_CHECK_EQ(b.info.frame_id, a.info.frame_id + 1u);
      FX1_CHECK(a.img == expected_capture(2));
      FX1_CHECK(b.img == expected_capture(3));
      FX1_CHECK(dump->captures()[before].base != dump->captures()[before + 1].base);
   }

   // ODMA write error after the tap has every row: the DMA error starts a
   // recovery (MAS v1.07 §9.9.8), which discards the frame: "reset", nothing published.
   void test_odma_error() {
      reset_and_program(1);
      wr(chans[0].free, 0x1);
      load(0, 4);
      // The last chroma row of the 62 x 46 output (BGGR active image), written
      // after the pipeline has completed the frame.
      const std::uint64_t last_uv = plane(chans[0].uv, 0) + std::uint64_t{chans[0].stride} * (46u / 2u - 1u);
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, last_uv, last_uv + chans[0].stride, -1});
      const std::size_t before = rec->order.size();
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(ended(before + 1));
      mem->faults.clear();
      const recorder::frame &f = rec->last();
      FX1_CHECK(f.how == demosaic_frame_end::reset);
      FX1_CHECK_EQ(f.rows, f.info.height);  // every row had passed the tap
      FX1_CHECK_EQ(f.status_at_end & FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK, 0u);
      CHECK_STR(dump->captures().back().status, std::string(to_string(f.how)));
      CHECK_STR(index_status(dump->captures().size() - 1), std::string(to_string(f.how)));
      FX1_CHECK(no_part_files());
   }

   // IDMA read error in the middle of the frame: part of the rows, recovery: "reset".
   void test_idma_error() {
      reset_and_program(1);
      wr(chans[0].free, 0x1);
      load(0, 5);
      mem->faults.push_back({tlm::TLM_READ_COMMAND, input(0) + 2u * W * 20u, input(0) + 2u * W * H, -1});
      const std::size_t before = rec->order.size();
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(ended(before + 1));
      mem->faults.clear();
      const recorder::frame &f = rec->last();
      FX1_CHECK(f.how == demosaic_frame_end::reset);
      FX1_CHECK(!f.done);
      FX1_CHECK(f.rows < f.info.height);
      FX1_CHECK(no_part_files());
   }

   // Soft reset (COMMON_CTRL.SOFT_RST) and hard reset (i_rst_n) in the middle
   // of a frame: "reset", nothing published; the next frame has a new epoch.
   void test_resets() {
      for (int hard = 0; hard < 2; ++hard) {
         const std::string ctx = hard ? "hard reset" : "soft reset";
         reset_and_program(1);
         wr(chans[0].free, 0x1);
         load(0, 6);
         const std::size_t before = rec->order.size();
         wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
         CHECK_CTX(wait_until([&] { return rec->order.size() > before && rec->last().rows >= 10; }), ctx);
         const std::uint64_t epoch = rec->last().info.epoch;
         if (hard) {
            rst_n.write(false);
            wait(10, SC_NS);
            rst_n.write(true);
            wait(10, SC_NS);
         } else {
            wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
         }
         CHECK_CTX(ended(before + 1), ctx);
         const recorder::frame &f = rec->last();
         CHECK_CTX(f.how == demosaic_frame_end::reset, ctx);
         CHECK_CTX(f.rows < f.info.height, ctx);
         CHECK_STR(dump->captures().back().status, std::string("frame_reset"));
         CHECK_CTX(no_part_files(), ctx);
         // A complete frame afterwards: a later epoch, a new key.
         reset_and_program(1);
         wr(chans[0].free, 0x1);
         load(0, 7);
         wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
         CHECK_CTX(ended(before + 2), ctx);
         CHECK_CTX(rec->last().how == demosaic_frame_end::completed, ctx);
         CHECK_CTX(rec->last().info.epoch > epoch, ctx);
         CHECK_CTX(rec->last().img == expected_capture(7), ctx);
      }
   }

   // A host I/O error (frames directory not writable) is recorded by the
   // writer; the model completes the frame as usual.
   void test_io_error() {
      reset_and_program(1);
      wr(chans[0].free, 0x1);
      load(0, 8);
      FX1_CHECK(dump->ok());
      ::chmod((dir + "/frames").c_str(), 0555);
      const std::size_t before = rec->order.size();
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(ended(before + 1));
      FX1_CHECK(idle());
      ::chmod((dir + "/frames").c_str(), 0755);
      FX1_CHECK(rec->last().how == demosaic_frame_end::completed);
      FX1_CHECK_EQ(rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 1u);
      FX1_CHECK_EQ(rd(FX1_ISP_DMA_ERR_OFFSET) & ~FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT, 0u);
      FX1_CHECK(!dump->ok());
      CHECK_STR(dump->captures().back().status, std::string("capture_error"));
      CHECK_STR(index_status(dump->captures().size() - 1), std::string("capture_error"));
      FX1_CHECK(no_part_files());
   }

   // Retirement bookkeeping (engine_context): every consumer done, one of
   // them not completed -> "failed" (from the TLM boundary only reachable by a
   // pipeline that drops a frame itself: IDMA/ODMA errors take the recovery,
   // see test_odma_error); all completed -> "completed"; a reset -> "reset".
   void test_retirement_paths() {
      control_unit cu;
      const fx1_isp_params params;
      cdc::components::fx1_isp::engine_context ctx(cu, params);
      ctx.state_changed = [] {};
      recorder r;
      r.dut = dut;
      ctx.demosaic = &r;
      demosaic_frame_info i;
      for (std::uint64_t f = 1; f <= 3; ++f) {
         i.sequence = f;
         r.frame_begin(i);
         ctx.accept_frame();
         ctx.frame_begin(f, 2);  // the pipeline and two outputs
      }
      ctx.consumer_done(1, true);
      ctx.consumer_done(1, true);
      FX1_CHECK(!r.frames.at(1).ended);  // one consumer still pending
      ctx.consumer_done(1, false);
      for (int k = 0; k < 3; ++k) {
         ctx.consumer_done(2, true);
      }
      ctx.reset_engines();  // frame 3 in flight
      FX1_CHECK(r.frames.at(1).ended && r.frames.at(1).how == demosaic_frame_end::failed);
      FX1_CHECK(r.frames.at(2).ended && r.frames.at(2).how == demosaic_frame_end::completed);
      FX1_CHECK(r.frames.at(3).ended && r.frames.at(3).how == demosaic_frame_end::reset);
      ctx.consumer_done(3, true);  // late response of the reset frame: no second end
      FX1_CHECK_EQ(r.protocol_errors, 0u);
   }

   void run() {
      test_retirement_paths();
      bool threw = false;
      try {
         dut->debug_set_demosaic_observer(nullptr);
      } catch (const std::logic_error &) {
         threw = true;
      }
      FX1_CHECK(threw);  // only before the simulation starts
      test_completed();
      test_output_masks();
      test_two_frames();
      test_odma_error();
      test_idma_error();
      test_resets();
      test_io_error();
      FX1_CHECK_EQ(rec->protocol_errors, 0u);
      finished = true;
      sc_stop();
   }
};

}  // namespace

int sc_main(int argc, char *argv[]) {
   test_tap_rows();
   test_tap_position();
   test_flat_field();
   const std::string root = argc > 1 ? argv[1] : ".";
   std::system(("rm -rf '" + root + "/demosaic writer'").c_str());
   test_writer_contract(root + "/demosaic writer");

   const std::string dir = std::string(argc > 1 ? argv[1] : ".") + "/demosaic dump";  // a space in the path
   bool refused = false;
   try {
      demosaic_dump bad("/proc/fx1_isp_no_such_dir/x");
   } catch (const std::runtime_error &) {
      refused = true;
   }
   FX1_CHECK(refused);

   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_tlm dut("isp", fx1_isp_params{});
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   recorder rec;
   rec.dut = &dut;
   std::system(("rm -rf '" + dir + "'").c_str());
   demosaic_dump dump(dir);
   tee both;
   both.to = {&rec, &dump};
   dut.debug_set_demosaic_observer(&both);
   bench tb("tb");
   tb.socket.bind(dut.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   tb.rst_n(rst_n);
   tb.irq(irq);
   tb.dut = &dut;
   tb.mem = &mem;
   tb.rec = &rec;
   tb.dump = &dump;
   tb.dir = dir;
   sc_start();
   FX1_CHECK(tb.finished);
   dump.finish();
   return fx1_test::summary("fx1_isp_test_demosaic_tlm");
}
