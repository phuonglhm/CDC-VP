// SPDX-License-Identifier: Apache-2.0
// The C reference driver (driver/, docs/ISP_PROGRAMMING_GUIDE.md) running on
// the model. Every committed vector is driven only through the driver API:
// profiles are translated into driver calls (LUT loaders, LSC load, CCM
// commit, AEC configuration; plain writes otherwise), frames are queued
// through the rotation, completions are taken from the interrupt handler,
// and the statistics are read with the driver's coherent readers. Outputs
// and statistics must equal the Python reference. Then the driver's error
// handling: IDMA and ODMA AXI errors in a stream (DEC-19) and a soft reset
// in the middle of a stream (DEC-14).
// Usage: fx1_isp_test_driver_tlm <vectors dir>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "registers/csr_desc.h"
#include "test_memory.h"
#include "vector_io.h"

using namespace sc_core;
using namespace cdc::components::fx1_isp;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 16u << 20;
fx1_isp_buffers buffer_layout(std::uint32_t in_stride, std::uint32_t out_stride) {
   fx1_isp_buffers b{};
   for (unsigned k = 0; k < FX1_ISP_NUM_BUFFERS; ++k) {
      b.in[k] = mem_base + 0x200000ull * k;              // 2 MB each
      b.y[k] = mem_base + 0x800000ull + 0x100000ull * k; // 1 MB each
      b.uv[k] = mem_base + 0xC00000ull + 0x80000ull * k; // 512 KB each
   }
   b.in_stride = in_stride;
   b.y_stride = b.uv_stride = out_stride;
   b.max_burst_beats = 64;
   return b;
}
std::uint32_t align16(std::uint32_t v) { return (v + 15u) & ~15u; }

std::vector<std::uint8_t> read_file(const fs::path &p) {
   std::ifstream f(p, std::ios::binary);
   return {std::istreambuf_iterator<char>(f), {}};
}
const csr::reg_desc *reg(const std::string &name) {
   for (std::size_t i = 0; i < csr::num_registers; ++i) {
      if (name == csr::registers[i].name) {
         return &csr::registers[i];
      }
   }
   return nullptr;
}
std::int16_t sext12(std::uint32_t v) { return static_cast<std::int16_t>((v & 0x800u) ? (v | 0xF000u) : (v & 0xFFFu)); }

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   sc_out<bool> rst_n;
   sc_in<bool> irq;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;
   fs::path root;
   fx1_isp_dev dev{};
   std::size_t vectors = 0, helper_calls = 0;
   std::map<std::string, unsigned> helpers;

   SC_HAS_PROCESS(bench);
   explicit bench(sc_module_name n) : sc_module(n), socket("socket"), rst_n("rst_n"), irq("irq") { SC_THREAD(run); }

   // ---- HAL ----
   std::uint32_t access(tlm::tlm_command cmd, std::uint32_t addr, std::uint32_t v) {
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
      return v;
   }
   static std::uint32_t hal_read(void *ctx, std::uint32_t off) {
      return static_cast<bench *>(ctx)->access(tlm::TLM_READ_COMMAND, off, 0);
   }
   static void hal_write(void *ctx, std::uint32_t off, std::uint32_t v) {
      static_cast<bench *>(ctx)->access(tlm::TLM_WRITE_COMMAND, off, v);
   }
   static void hal_delay(void *ctx, std::uint32_t cycles) {
      static_cast<bench *>(ctx)->wait(static_cast<bench *>(ctx)->dut->params().core_period * static_cast<double>(cycles));
   }
   std::uint32_t rd(std::uint32_t off) { return hal_read(this, off); }

   void reset_and_init() {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      const fx1_isp_hal hal{&bench::hal_read, &bench::hal_write, &bench::hal_delay, this};
      FX1_CHECK_EQ(fx1_isp_init(&dev, &hal), FX1_ISP_OK);
      mem->faults.clear();
   }

   // ---- profile -> driver calls ----
   void apply_profile(const std::vector<fx1_test::csr_line> &lines) {
      std::uint32_t aec_ctx = 0;
      for (std::size_t i = 0; i < lines.size();) {
         const std::string &n = lines[i].reg;
         const std::uint32_t v = lines[i].value;
         auto next_is = [&](std::size_t k, const char *name) { return i + k < lines.size() && lines[i + k].reg == name; };
         if (n == "GAMMA_LUT_ADDR" && v == 0 && i + 256 < lines.size() + 0 && next_is(1, "GAMMA_LUT_DATA")) {
            std::size_t k = 1;
            std::uint16_t lut[256];
            while (k <= 256 && next_is(k, "GAMMA_LUT_DATA")) {
               lut[k - 1] = static_cast<std::uint16_t>(lines[i + k].value);
               ++k;
            }
            if (k == 257) {
               fx1_isp_load_gamma(&dev, lut);
               ++helpers["gamma"];
               i += k;
               continue;
            }
         }
         if (n == "EE_LUT_CTRL" && next_is(1, "EE_LUT_ADDR") && lines[i + 1].value == 0) {
            const unsigned bank = (v >> 1) & 3u, want = bank < 2 ? 64u : 32u;
            std::vector<std::uint16_t> t;
            std::size_t k = 2;
            while (next_is(k, "EE_LUT_WDATA")) {
               t.push_back(static_cast<std::uint16_t>(lines[i + k].value));
               ++k;
            }
            if (t.size() == want) {
               FX1_CHECK_EQ(fx1_isp_load_ee_table(&dev, bank, t.data(), want), FX1_ISP_OK);
               ++helpers["ee_table"];
               i += k;
               continue;
            }
         }
         if (n == "GTM_CTRL" && (v & FX1_ISP_GTM_CTRL_MANUAL_MASK) && next_is(1, "GTM_LUT_ADDR")) {
            std::vector<std::uint16_t> t;
            std::size_t k = 2;
            while (next_is(k, "GTM_LUT_DATA")) {
               t.push_back(static_cast<std::uint16_t>(lines[i + k].value));
               ++k;
            }
            if (t.size() == 65) {
               hal_write(this, FX1_ISP_GTM_CTRL_OFFSET, v);
               fx1_isp_load_gtm_lut(&dev, t.data());
               ++helpers["gtm_lut"];
               i += k;
               continue;
            }
         }
         if (n == "LSC_MESH_NODES" && next_is(1, "LSC_LOAD_CTRL") && (lines[i + 1].value & 0x100u)) {
            const unsigned nx = v & 0xFFu, ny = (v >> 8) & 0xFFu, dest = lines[i + 1].value & 3u;
            std::vector<std::uint32_t> coef;
            std::size_t k = 2;
            while (next_is(k, "LSC_COEF_DATA")) {
               coef.push_back(lines[i + k].value);
               ++k;
            }
            if (next_is(k, "LSC_LOAD_CTRL") && (lines[i + k].value & 0x200u) && coef.size() == 4u * nx * ny) {
               std::uint32_t err = 0;
               FX1_CHECK_EQ(fx1_isp_lsc_load(&dev, dest, nx, ny, coef.data(), &err), FX1_ISP_OK);
               ++helpers["lsc_load"];
               i += k + 1;
               continue;
            }
         }
         if (n == "LSC_PROFILE_SEL") {
            FX1_CHECK_EQ(fx1_isp_lsc_select(&dev, v), FX1_ISP_OK);
            ++helpers["lsc_select"];
            ++i;
            continue;
         }
         if (n == "CCM_CRR") {  // a full coefficient + offset set followed by a commit
            std::size_t k = 0;
            std::int16_t m[9], ofs[3];
            bool full = i + 12 < lines.size();
            for (; full && k < 12; ++k) {
               const csr::reg_desc *d = reg(lines[i + k].reg);
               const auto kk = static_cast<std::uint32_t>(k);
               const std::uint32_t expect_off = kk < 9 ? FX1_ISP_CCM_CRR_OFFSET + 4u * kk : FX1_ISP_CCM_OFS_R_OFFSET + 4u * (kk - 9);
               full = d && d->offset == expect_off;
               (k < 9 ? m[k] : ofs[k - 9]) = sext12(lines[i + k].value);
            }
            if (full && next_is(12, "CCM_CTRL") && (lines[i + 12].value & FX1_ISP_CCM_CTRL_UPDATED_MASK)) {
               fx1_isp_ccm_set(&dev, m, ofs, lines[i + 12].value & FX1_ISP_CCM_CTRL_EN_MASK);
               ++helpers["ccm"];
               i += 13;
               continue;
            }
         }
         if (n == "AEC_ZONE_CFG" && next_is(1, "AEC_ZONE_SIZE") && next_is(2, "AEC_SAMPLE_CLIP") &&
             next_is(3, "AEC_THRESH") && next_is(4, "AEC_CTRL") && (lines[i + 4].value & FX1_ISP_AEC_CTRL_COMMIT_MASK)) {
            fx1_isp_aec_cfg c{};
            c.nx = v & 0x3Fu;
            c.ny = (v >> 8) & 0x1Fu;
            c.zone_w = lines[i + 1].value & 0xFFFu;
            c.zone_h = (lines[i + 1].value >> 16) & 0xFFFu;
            c.clip_min = lines[i + 2].value & 0xFFFu;
            c.clip_max = (lines[i + 2].value >> 16) & 0xFFFu;
            c.th_ue = lines[i + 3].value & 0xFFFu;
            c.th_oe = (lines[i + 3].value >> 16) & 0xFFFu;
            c.context_id = aec_ctx;
            fx1_isp_aec_configure(&dev, &c, lines[i + 4].value & FX1_ISP_AEC_CTRL_EN_MASK);
            ++helpers["aec"];
            i += 5;
            continue;
         }
         if (n == "COMMON_FRAME_WIDTH" && next_is(1, "COMMON_FRAME_HEIGHT")) {
            // Geometry goes through the driver when the Bayer order follows; else plain writes.
            if (next_is(2, "COMMON_BAYER")) {
               FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, v, lines[i + 1].value, lines[i + 2].value), FX1_ISP_OK);
               ++helpers["geometry"];
               i += 3;
               continue;
            }
         }
         const csr::reg_desc *d = reg(n);
         FX1_CHECK_EQ_CTX(d ? 1 : 0, 1, "unknown register " + n);
         if (d) {
            if (n == "AEC_CONTEXT_ID") {
               aec_ctx = v;
            }
            const fx1_isp_reg_write w{d->offset, v};
            fx1_isp_apply(&dev, &w, 1);
         }
         ++i;
      }
   }

   // ---- streaming ----
   void load_input(unsigned slot, const std::vector<std::uint8_t> &raw, std::uint32_t w, std::uint32_t h,
                   std::uint32_t stride, const fx1_isp_buffers &b) {
      for (std::uint32_t y = 0; y < h; ++y) {
         std::memcpy(mem->at(b.in[slot] + std::uint64_t{y} * stride), raw.data() + 2ull * y * w, 2u * w);
      }
   }
   std::vector<std::uint8_t> output(unsigned slot, const fx1_isp_buffers &b) {
      std::vector<std::uint8_t> o;
      for (std::uint32_t y = 0; y < dev.out_height; ++y) {
         const std::uint8_t *p = mem->at(b.y[slot] + std::uint64_t{y} * b.y_stride);
         o.insert(o.end(), p, p + dev.out_width);
      }
      for (std::uint32_t y = 0; y < dev.out_height / 2; ++y) {
         const std::uint8_t *p = mem->at(b.uv[slot] + std::uint64_t{y} * b.uv_stride);
         o.insert(o.end(), p, p + dev.out_width);
      }
      return o;
   }
   // Interrupt-driven completion: waits for the line, runs the handler,
   // recovers DMA errors, collects completed buffers in order.
   std::size_t errors_recovered = 0;
   std::uint32_t last_dma_err = 0;
   bool service(std::vector<std::vector<std::uint8_t>> &done, std::size_t want, const fx1_isp_buffers &b,
                const std::string &ctx, bool recover = true) {
      const sc_time end = sc_time_stamp() + sc_time(200, SC_MS);
      while (done.size() < want) {
         if (!irq.read()) {
            wait(sc_time(1, SC_MS), irq.posedge_event());
         }
         if (sc_time_stamp() >= end) {
            FX1_CHECK_EQ_CTX(0, 1, ctx + ": no completion (deadlock)");
            return false;
         }
         fx1_isp_events ev{};
         fx1_isp_irq(&dev, &ev);
         last_dma_err = ev.dma_err;
         if (recover && (ev.dma_err & (FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT))) {
            FX1_CHECK_EQ_CTX(fx1_isp_recover(&dev, ev.dma_err), FX1_ISP_OK, ctx + " recover");
            ++errors_recovered;
         }
         unsigned idx = 0;
         while (fx1_isp_next_done(&dev, &idx) == FX1_ISP_OK) {
            done.push_back(output(idx, b));
         }
      }
      return true;
   }

   // ---- statistics through the driver readers ----
   void check_stats(const std::string &name, const std::vector<fx1_test::csr_line> &script) {
      std::map<std::string, std::uint32_t> sel;
      std::map<std::string, std::uint32_t> want;  // "REG@selectors" -> value
      for (const fx1_test::csr_line &l : script) {
         if (l.set) {
            sel[l.reg] = l.value;
            continue;
         }
         std::string key = l.reg;
         if (l.reg.rfind("AEC_GLOBAL", 0) == 0) {
            key += "@" + std::to_string(sel["AEC_CHANNEL_SEL"]);
         } else if (l.reg == "AEC_ZONE_SUM" || l.reg == "AEC_ZONE_COUNT") {
            key += "@" + std::to_string(sel["AEC_ZONE_ADDR"]) + "/" + std::to_string(sel["AEC_CHANNEL_SEL"]);
         } else if (l.reg.rfind("AEC_ZONE_GREEN", 0) == 0) {
            key += "@" + std::to_string(sel["AEC_ZONE_ADDR"]);
         } else if (l.reg == "AEC_HIST_DATA") {
            key += "@" + std::to_string(sel["AEC_HIST_ADDR"]);
         } else if (l.reg == "AF_STAT_DATA") {
            key += "@" + std::to_string(sel["AF_STAT_ADDR"]);
         } else if (l.reg.rfind("AWB_ZONE", 0) == 0) {
            continue;  // AWB zones: no driver reader (raw muxes, covered by the vector tests)
         }
         want[key] = l.value;
      }
      if (want.empty()) {
         return;
      }
      auto expect = [&](const std::string &key, std::uint64_t got) {
         const auto it = want.find(key);
         if (it != want.end()) {
            FX1_CHECK_EQ_CTX(got, it->second, name + " driver " + key);
         }
      };
      fx1_isp_aec_global ag{};
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_global(&dev, &ag), FX1_ISP_OK, name + " aec global");
      for (unsigned c = 0; c < 4; ++c) {
         expect("AEC_GLOBAL_SUM_LO@" + std::to_string(c), ag.sum[c] & 0xFFFFFFFFu);
         expect("AEC_GLOBAL_SUM_HI@" + std::to_string(c), ag.sum[c] >> 32);
         expect("AEC_GLOBAL_COUNT@" + std::to_string(c), ag.count[c]);
      }
      expect("AEC_FRAME_ID", ag.frame_id);
      expect("AEC_RESULT_CONTEXT_ID", ag.context_id);
      // Every zone address the script reads, beyond-grid ones included.
      std::vector<unsigned> zones;
      for (const auto &kv : want) {
         if (kv.first.rfind("AEC_ZONE_GREEN_OE_UE@", 0) == 0) {
            zones.push_back(static_cast<unsigned>(std::stoul(kv.first.substr(21))));
         }
      }
      std::uint32_t hist[64];
      for (unsigned z : zones) {
         fx1_isp_aec_zone zr{};
         FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, z, 1, &zr, hist, nullptr), FX1_ISP_OK, name + " aec zones");
         const std::string zs = std::to_string(z);
         expect("AEC_ZONE_GREEN_OE_UE@" + zs, (zr.green_oe << 16) | zr.green_ue);
         expect("AEC_ZONE_GREEN_MIN_MAX@" + zs, (zr.green_max << 12) | zr.green_min);
         for (unsigned c = 0; c < 4; ++c) {
            expect("AEC_ZONE_SUM@" + zs + "/" + std::to_string(c), zr.sum[c]);
            expect("AEC_ZONE_COUNT@" + zs + "/" + std::to_string(c), zr.count[c]);
         }
      }
      if (zones.empty()) {
         FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 0, nullptr, hist, nullptr), FX1_ISP_OK, name + " hist");
      }
      for (unsigned bin = 0; bin < 64; ++bin) {
         expect("AEC_HIST_DATA@" + std::to_string(bin), hist[bin]);
      }
      fx1_isp_awb_global wg{};
      FX1_CHECK_EQ_CTX(fx1_isp_read_awb_global(&dev, &wg), FX1_ISP_OK, name + " awb global");
      const char *ch = "RGB";
      for (unsigned c = 0; c < 3; ++c) {
         expect(std::string("AWB_GLOBAL_SUM_") + ch[c] + "_L", wg.sum[c] & 0xFFFFFFFFu);
         expect(std::string("AWB_GLOBAL_SUM_") + ch[c] + "_H", wg.sum[c] >> 32);
      }
      expect("AWB_GLOBAL_COUNT", wg.count);
      expect("AWB_FRAME_ID", wg.frame_id);
      expect("AWB_RESULT_CONTEXT_ID", wg.context_id);
      fx1_isp_af_scores af{};
      FX1_CHECK_EQ_CTX(fx1_isp_read_af(&dev, &af), FX1_ISP_OK, name + " af");
      for (unsigned z = 0; z < 16; ++z) {
         expect("AF_STAT_DATA@" + std::to_string(z), af.fv[z]);
      }
      expect("AF_FRAME_ID", af.frame_id);
      expect("AF_RESULT_CONTEXT_ID", af.context_id);
      const auto st = want.find("AF_STATUS");
      if (st != want.end()) {
         FX1_CHECK_EQ_CTX(af.score_valid ? 1u : 0u, (st->second & FX1_ISP_AF_STATUS_SCORE_VALID_MASK) ? 1u : 0u,
                          name + " driver score_valid");
      }
      // Remaining plain registers of the script (status, counters) as raw reads.
      for (const auto &kv : want) {
         if (kv.first.find('@') == std::string::npos && kv.first.rfind("AEC_", 0) != 0 &&
             kv.first.rfind("AWB_GLOBAL", 0) != 0 && kv.first.rfind("AF_", 0) != 0 &&
             kv.first.rfind("AWB_FRAME", 0) != 0 && kv.first.rfind("AWB_RESULT", 0) != 0) {
            if (const csr::reg_desc *d = reg(kv.first)) {
               FX1_CHECK_EQ_CTX(rd(d->offset), kv.second, name + " " + kv.first);
            }
         }
      }
   }

   void run_vector(const fs::path &dir) {
      const std::string name = dir.filename().string();
      reset_and_init();
      std::ifstream meta(dir / "meta.txt");
      std::uint32_t in_w = 0, in_h = 0, out_w = 0, out_h = 0, frames = 1;
      meta >> in_w >> in_h >> out_w >> out_h >> frames;
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      const std::uint32_t in_stride = align16(2 * in_w);
      const std::uint32_t ow = rd(FX1_ISP_RESIZER_OUT_W_OFFSET);  // live derived geometry (CSR-09)
      const fx1_isp_buffers b = buffer_layout(in_stride, align16(ow));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, name + " start");
      FX1_CHECK_EQ_CTX(dev.out_width, out_w, name + " out_w");
      FX1_CHECK_EQ_CTX(dev.out_height, out_h, name + " out_h");
      const std::vector<fx1_test::csr_line> between = fx1_test::read_frame_writes(dir / "frames.csrw");
      std::vector<std::vector<std::uint8_t>> done;
      if (between.empty()) {  // stream: every frame queued back to back
         for (std::uint32_t f = 0; f < frames; ++f) {
            load_input(fx1_isp_next_input(&dev), input, in_w, in_h, in_stride, b);
            FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, name + " queue");
         }
         service(done, frames, b, name);
      } else {  // configuration between frames: one frame at a time, idle in between
         for (std::uint32_t f = 0; f < frames; ++f) {
            std::vector<fx1_test::csr_line> writes;
            for (const fx1_test::csr_line &w : between) {
               if (w.frame == f) {
                  writes.push_back(w);
               }
            }
            apply_profile(writes);
            load_input(fx1_isp_next_input(&dev), input, in_w, in_h, in_stride, b);
            FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, name + " queue");
            service(done, f + 1, b, name);
         }
      }
      if (done.size() == frames) {
         std::vector<std::uint8_t> want = read_file(dir / "expected_y.bin");
         const std::vector<std::uint8_t> uv = read_file(dir / "expected_uv.bin");
         want.insert(want.end(), uv.begin(), uv.end());
         FX1_CHECK_EQ_CTX(done.back() == want ? 1 : 0, 1, name + " NV12 of the last frame");
      }
      check_stats(name, fx1_test::read_expected(dir / "expected_stats.txt"));
      ++vectors;
   }

   // Error handling of the driver in a stream. Four distinct inputs (the
   // vector's input XOR k * 0x155) are used so that a wrong buffer, a lost or
   // duplicated frame or an output overwritten before it was consumed shows
   // in the content; their expected outputs come from an error-free stream
   // anchored on the vector's reference output (stateless profile).
   static constexpr std::uint32_t ew = 64, eh = 32;
   std::vector<std::vector<std::uint8_t>> e_inputs, e_expected;
   std::vector<std::uint8_t> variant(const std::vector<std::uint8_t> &raw, unsigned k) {
      std::vector<std::uint8_t> v = raw;
      for (std::size_t i = 0; i + 1 < v.size(); i += 2) {
         const std::uint16_t s = static_cast<std::uint16_t>((v[i] | (v[i + 1] << 8)) ^ ((k * 0x155u) & 0xFFFu));
         v[i] = static_cast<std::uint8_t>(s);
         v[i + 1] = static_cast<std::uint8_t>(s >> 8);
      }
      return v;
   }
   fx1_isp_buffers start_error_case(const fs::path &dir) {
      reset_and_init();
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      const fx1_isp_buffers b = buffer_layout(align16(2 * ew), align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ(fx1_isp_start(&dev, &b), FX1_ISP_OK);
      errors_recovered = 0;
      return b;
   }
   // Queues the frame of input k into the next input buffer.
   void queue_input(unsigned k, const fx1_isp_buffers &b, const std::string &ctx) {
      load_input(fx1_isp_next_input(&dev), e_inputs[k], ew, eh, align16(2 * ew), b);
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
   }
   void check_sequence(const std::vector<std::vector<std::uint8_t>> &done, const std::vector<unsigned> &order,
                       const std::string &ctx) {
      FX1_CHECK_EQ_CTX(done.size(), order.size(), ctx + " frames completed");
      for (std::size_t k = 0; k < std::min(done.size(), order.size()); ++k) {
         FX1_CHECK_EQ_CTX(done[k] == e_expected[order[k]] ? 1 : 0, 1,
                          ctx + " frame " + std::to_string(k) + " = input " + std::to_string(order[k]));
      }
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_ERR_OFFSET) & (FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT |
                                                  FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT),
                       0, ctx + " error state cleared");
   }

   void run_error_cases() {
      const fs::path dir = root / "mandatory_rggb_64x32";
      const std::vector<std::uint8_t> raw = read_file(dir / "input.bin");
      std::vector<std::uint8_t> anchor = read_file(dir / "expected_y.bin");
      const std::vector<std::uint8_t> anchor_uv = read_file(dir / "expected_uv.bin");
      anchor.insert(anchor.end(), anchor_uv.begin(), anchor_uv.end());
      for (unsigned k = 0; k < 4; ++k) {
         e_inputs.push_back(variant(raw, k));
      }
      {  // error-free stream: the expected output of each input
         const fx1_isp_buffers b = start_error_case(dir);
         queue_input(0, b, "reference stream");
         wait(200, SC_NS);
         // The first buffer is queued before the engines start: no start-up underrun (M4-R5).
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT, 0, "no start-up underrun");
         for (unsigned k = 1; k < 4; ++k) {
            queue_input(k, b, "reference stream");
         }
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_EBUSY, "fifth frame refused");
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 4, b, "reference stream");
         e_expected = done;
         // A reused buffer is not reported done before its new frame completes.
         queue_input(0, b, "reused buffer");
         unsigned idx = 0;
         FX1_CHECK_EQ_CTX(fx1_isp_next_done(&dev, &idx), FX1_ISP_ENODATA, "reused buffer not done at once");
         service(done, 5, b, "reused buffer");
         FX1_CHECK_EQ_CTX(done.size() == 5 && done[4] == e_expected[0] ? 1 : 0, 1, "reused buffer content");
         FX1_CHECK_EQ_CTX(e_expected.size() == 4 && e_expected[0] == anchor ? 1 : 0, 1, "reference stream anchored");
         for (unsigned k = 1; k < e_expected.size(); ++k) {
            FX1_CHECK_EQ_CTX(e_expected[k] != e_expected[0] ? 1 : 0, 1, "distinct outputs");
         }
      }
      {  // IDMA read error early in frame 1, while frame 0 is still in the pipeline
         const std::string ctx = "driver idma";
         const fx1_isp_buffers b = start_error_case(dir);
         const std::uint64_t row = b.in[1] + std::uint64_t{1} * align16(2 * ew);
         mem->faults.push_back({tlm::TLM_READ_COMMAND, row, row + 2u * ew, 1});
         for (unsigned k = 0; k < 4; ++k) {
            queue_input(k, b, ctx);
         }
         // A late interrupt handler: frame 0 has completed but is not yet
         // collected when the error is handled.
         while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT)) {
            wait(50, SC_NS);
         }
         while (!(rd(FX1_ISP_ODMA_BUF_DONE_OFFSET) & 1u)) {
            wait(50, SC_NS);
         }
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 4, b, ctx);
         FX1_CHECK_EQ_CTX(errors_recovered, 1, ctx + " recovered once");
         check_sequence(done, {0, 1, 2, 3}, ctx);
         FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " nothing left in flight");
      }
      // ODMA write error in frame 1's output: frame 1 is lost and resubmitted;
      // the stream then runs with four frames in flight. `late`: the interrupt
      // handler runs only once frame 0 is DONE (not yet collected) and the
      // error is flagged, so the failed buffer is not the next one to collect
      // (review M5-R5).
      for (const bool late : {false, true}) {
         const std::string ctx = late ? "driver odma, late handler" : "driver odma";
         const fx1_isp_buffers b = start_error_case(dir);
         const std::uint64_t row = b.y[1] + std::uint64_t{dev.out_height / 2} * b.y_stride;
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, row, row + dev.out_width, 1});
         for (unsigned k = 0; k < 3; ++k) {
            queue_input(k, b, ctx);
         }
         if (late) {
            while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) ||
                   !(rd(FX1_ISP_ODMA_BUF_DONE_OFFSET) & 1u)) {
               wait(50, SC_NS);
            }
            FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_BUF_DONE_OFFSET) & 2u, 0, ctx + " failed buffer not DONE");
            FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_BUF_FREE_OFFSET) & 2u, 0, ctx + " failed buffer not FREE");
         }
         std::vector<std::vector<std::uint8_t>> done;
         bool alive = service(done, 2, b, ctx);
         FX1_CHECK_EQ_CTX(errors_recovered, 1, ctx + " recovered once");
         std::vector<unsigned> order = {0, 2};
         const unsigned more[] = {1, 3, 0, 2, 1, 3, 0, 2};  // the lost frame first, then the stream
         std::size_t next = 0;
         while (alive && next < 8) {
            while (next < 8 && dev.in_flight < FX1_ISP_NUM_BUFFERS) {
               queue_input(more[next], b, ctx);
               order.push_back(more[next++]);
            }
            alive = service(done, done.size() + 1, b, ctx);
         }
         if (alive) {
            service(done, order.size(), b, ctx);
         }
         check_sequence(done, order, ctx);
         FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " nothing left in flight");
         unsigned idx = 0;
         FX1_CHECK_EQ_CTX(fx1_isp_next_done(&dev, &idx), FX1_ISP_ENODATA, ctx + " no spurious completion");
         FX1_CHECK_EQ_CTX(dev.out_next, dev.out_done, ctx + " output rotation consistent");
      }
      {  // An early output fault must not make an IDMA-owned input reusable.
         const std::string ctx = "early ODMA fault, full input ring";
         const fx1_isp_buffers b = start_error_case(dir);
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, b.y[0], b.y[0] + ew, 1});
         for (unsigned k = 0; k < 4; ++k) queue_input(k, b, ctx);
         const auto deadline = sc_time_stamp() + sc_time(1, SC_MS);
         while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) &&
                sc_time_stamp() < deadline) wait(10, SC_NS);
         FX1_CHECK(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT);
         FX1_CHECK_EQ(fx1_isp_recover(&dev, FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT), FX1_ISP_OK);
         FX1_CHECK(rd(FX1_ISP_IDMA_BUF_VALID_OFFSET) & (1u << dev.in_next));
         FX1_CHECK_EQ(fx1_isp_input_ready(&dev), 0);
         const unsigned before = dev.in_flight, next = dev.in_next;
         FX1_CHECK_EQ(fx1_isp_queue_frame(&dev), FX1_ISP_EBUSY);
         FX1_CHECK_EQ(dev.in_flight, before);
         FX1_CHECK_EQ(dev.in_next, next);
         while (!fx1_isp_input_ready(&dev) && sc_time_stamp() < deadline) wait(50, SC_NS);
         FX1_CHECK(fx1_isp_input_ready(&dev));
         std::uint32_t lseq = 99;
         unsigned linput = 99;
         FX1_CHECK_EQ(fx1_isp_next_lost(&dev, &lseq, &linput), FX1_ISP_OK);  // frame 0 is now known lost
         FX1_CHECK_EQ(lseq, 0);
         FX1_CHECK_EQ(linput, 0);
         queue_input(0, b, ctx); // resubmit only after IDMA has released input 0
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 4, b, ctx);
         check_sequence(done, {1, 2, 3, 0}, ctx);
         FX1_CHECK_EQ(dev.in_flight, 0);
      }
      {  // soft reset inside the first of two queued frames, then the stream again
         const std::string ctx = "driver soft reset";
         const fx1_isp_buffers b = start_error_case(dir);
         queue_input(0, b, ctx);
         queue_input(1, b, ctx);
         while (!(rd(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK)) {
            wait(50, SC_NS);
         }
         fx1_isp_soft_reset(&dev);
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0, ctx + " no frame completed");
         FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " driver state reset");
         for (unsigned k = 2; k < 4; ++k) {
            queue_input(k, b, ctx);
         }
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 2, b, ctx);
         check_sequence(done, {2, 3}, ctx);
      }
      {  // zone readout refuses a frame in progress (DEC-30, ALG-STAT-02)
         const fs::path sdir = root / "stats_flat_channels_64x48";
         reset_and_init();
         apply_profile(fx1_test::read_profile(sdir / "profile.csrw"));
         const fx1_isp_buffers b = buffer_layout(align16(2 * 64), align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
         FX1_CHECK_EQ(fx1_isp_start(&dev, &b), FX1_ISP_OK);
         load_input(fx1_isp_next_input(&dev), read_file(sdir / "input.bin"), 64, 48, align16(2 * 64), b);
         FX1_CHECK_EQ(fx1_isp_queue_frame(&dev), FX1_ISP_OK);
         while (!(rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK)) {
            wait(20, SC_NS);
         }
         fx1_isp_aec_zone z{};
         std::uint32_t hist[64];
         FX1_CHECK_EQ(fx1_isp_read_aec_zones(&dev, 0, 1, &z, hist, nullptr), FX1_ISP_EAGAIN);
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 1, b, "zones");
         FX1_CHECK_EQ(fx1_isp_read_aec_zones(&dev, 0, 1, &z, hist, nullptr), FX1_ISP_OK);
      }
      { // Reset timing cannot be guaranteed without the HAL delay callback.
         fx1_isp_dev invalid{};
         const fx1_isp_hal hal{&bench::hal_read, &bench::hal_write, nullptr, this};
         FX1_CHECK_EQ(fx1_isp_init(&invalid, &hal), FX1_ISP_EINVAL);
      }
      // Table loader from a non-zero address; argument checks; LSC refusals.
      reset_and_init();
      hal_write(this, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 77);
      std::uint16_t lut[256];
      for (unsigned i = 0; i < 256; ++i) {
         lut[i] = static_cast<std::uint16_t>((i * 13u + 5u) & 0xFFFu);
      }
      fx1_isp_load_gamma(&dev, lut);
      for (unsigned i : {0u, 1u, 77u, 255u}) {
         hal_write(this, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, i);
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_GAMMA_LUT_RDATA_OFFSET), lut[i], "gamma entry " + std::to_string(i));
      }
      std::vector<std::uint32_t> coef(4 * 2 * 2, 0x40000);
      std::uint32_t err = 0;
      FX1_CHECK_EQ(fx1_isp_lsc_load(&dev, 0, 2, 2, coef.data(), &err), FX1_ISP_OK);
      FX1_CHECK_EQ(fx1_isp_lsc_select(&dev, 0), FX1_ISP_OK);
      dut->debug_accepted_sof();  // profile 0 becomes active
      FX1_CHECK_EQ(fx1_isp_lsc_load(&dev, 0, 2, 2, coef.data(), &err), FX1_ISP_EIO);
      FX1_CHECK_EQ(err & FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK, FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK);
      FX1_CHECK_EQ(fx1_isp_lsc_select(&dev, 2), FX1_ISP_EIO);  // not loaded
      FX1_CHECK_EQ(fx1_isp_lsc_load(&dev, 1, 33, 2, coef.data(), &err), FX1_ISP_EINVAL);  // DEC-31
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 3, 4, 0), FX1_ISP_EINVAL);
      FX1_CHECK_EQ(fx1_isp_load_ee_table(&dev, 2, nullptr, 64), FX1_ISP_EINVAL);
   }

   // ---- Audit follow-up 2026-10-02 (report "các cải thiện nên làm tiếp", items 1-3) ----

   // Item 3: API order and state checks of the reference driver.
   void test_driver_state_machine() {
      const fs::path dir = root / "mandatory_rggb_64x32";
      const std::string ctx = "state machine";
      const fx1_isp_buffers b0 = buffer_layout(align16(2 * ew), align16(ew));
      fx1_isp_dev z{};  // zero-initialised, never passed to init
      unsigned idx = 0;
      fx1_isp_events ev{1u, 1u, 1u, 1u, 1u};
      fx1_isp_aec_global g{};
      fx1_isp_progress pr{};
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&z), FX1_ISP_ESTATE, ctx + " uninit queue");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&z, &b0), FX1_ISP_ESTATE, ctx + " uninit start");
      FX1_CHECK_EQ_CTX(fx1_isp_recover(&z, FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT), FX1_ISP_ESTATE, ctx + " uninit recover");
      FX1_CHECK_EQ_CTX(fx1_isp_next_done(&z, &idx), FX1_ISP_ESTATE, ctx + " uninit next_done");
      FX1_CHECK_EQ_CTX(fx1_isp_input_ready(&z), 0, ctx + " uninit input_ready");
      FX1_CHECK_EQ_CTX(fx1_isp_set_geometry(&z, 64, 32, 0), FX1_ISP_ESTATE, ctx + " uninit geometry");
      FX1_CHECK_EQ_CTX(fx1_isp_stop(&z), FX1_ISP_ESTATE, ctx + " uninit stop");
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_global(&z, &g), FX1_ISP_ESTATE, ctx + " uninit stats");
      FX1_CHECK_EQ_CTX(fx1_isp_get_progress(&z, &pr), FX1_ISP_ESTATE, ctx + " uninit progress");
      fx1_isp_irq(&z, &ev);
      FX1_CHECK_EQ_CTX(ev.common | ev.dma | ev.dma_err | ev.frames_done | ev.frames_lost, 0,
                       ctx + " uninit irq reports nothing");
      const fx1_isp_hal no_delay{&bench::hal_read, &bench::hal_write, nullptr, this};
      FX1_CHECK_EQ_CTX(fx1_isp_init(&z, &no_delay), FX1_ISP_EINVAL, ctx + " init needs delay_cycles");

      // CONFIG: streaming calls are refused until start.
      reset_and_init();
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      FX1_CHECK_EQ_CTX(dev.state, FX1_ISP_STATE_CONFIG, ctx + " after init");
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_ESTATE, ctx + " queue before start");
      FX1_CHECK_EQ_CTX(fx1_isp_next_done(&dev, &idx), FX1_ISP_ESTATE, ctx + " next_done before start");
      FX1_CHECK_EQ_CTX(fx1_isp_recover(&dev, FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT), FX1_ISP_ESTATE, ctx + " recover before start");
      FX1_CHECK_EQ_CTX(fx1_isp_stop(&dev), FX1_ISP_ESTATE, ctx + " stop before start");
      const fx1_isp_buffers b = buffer_layout(align16(2 * ew), align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      fx1_isp_buffers bad = b;
      bad.y_stride = 0;
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &bad), FX1_ISP_EINVAL, ctx + " invalid output stride");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK, 0,
                       ctx + " rejected start does not enable ISP");
      FX1_CHECK_EQ_CTX(dev.state, FX1_ISP_STATE_CONFIG, ctx + " invalid start keeps CONFIG");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      FX1_CHECK_EQ_CTX(dev.state, FX1_ISP_STATE_STREAMING, ctx + " streaming");
      queue_input(0, b, ctx);
      queue_input(1, b, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_EBUSY, ctx + " restart with frames in flight");
      FX1_CHECK_EQ_CTX(fx1_isp_set_geometry(&dev, 64, 32, 0), FX1_ISP_EBUSY, ctx + " geometry with frames in flight");

      // Cancel: stop discards the frames in flight and returns every buffer.
      while (!(rd(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK)) {
         wait(50, SC_NS);
      }
      FX1_CHECK_EQ_CTX(fx1_isp_stop(&dev), 2, ctx + " stop drops two frames");
      FX1_CHECK_EQ_CTX(dev.state, FX1_ISP_STATE_CONFIG, ctx + " config after stop");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_CTRL_OFFSET) & (FX1_ISP_DMA_CTRL_IDMA_EN_MASK | FX1_ISP_DMA_CTRL_ODMA_EN_MASK), 0,
                       ctx + " engines disabled");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK, 0,
                       ctx + " ISP disabled in CONFIG state");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_BUF_VALID_OFFSET) | rd(FX1_ISP_ODMA_BUF_FREE_OFFSET) | rd(FX1_ISP_ODMA_BUF_DONE_OFFSET),
                       0, ctx + " every buffer back to software");
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_ESTATE, ctx + " queue after stop");
      wait(200, SC_US);
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0, ctx + " no completion after stop");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " restart");
      queue_input(2, b, ctx);
      std::vector<std::vector<std::uint8_t>> done;
      service(done, 1, b, ctx);
      check_sequence(done, {2}, ctx + " after restart");

      // Watchdog: a stream that stops progressing (ISP_EN cleared mid-frame,
      // M2-A5) is seen through get_progress and cancelled with stop.
      queue_input(3, b, ctx);
      while (!(rd(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK)) {
         wait(50, SC_NS);
      }
      hal_write(this, FX1_ISP_COMMON_CTRL_OFFSET, 0);
      fx1_isp_progress p1{}, p2{};
      FX1_CHECK_EQ_CTX(fx1_isp_get_progress(&dev, &p1), FX1_ISP_OK, ctx + " progress");
      wait(500, SC_US);
      FX1_CHECK_EQ_CTX(fx1_isp_get_progress(&dev, &p2), FX1_ISP_OK, ctx + " progress");
      FX1_CHECK_EQ_CTX(p2.odma_frames, p1.odma_frames, ctx + " no progress while stalled");
      FX1_CHECK_EQ_CTX(p2.in_flight, 1, ctx + " one frame in flight");
      FX1_CHECK_EQ_CTX(p2.pipeline_busy, 1, ctx + " stalled, not idle");
      FX1_CHECK_EQ_CTX(p2.isp_enabled, 0, ctx + " reason: ISP_EN cleared");
      FX1_CHECK_EQ_CTX(fx1_isp_stop(&dev), 1, ctx + " watchdog stop");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start after watchdog stop");
      queue_input(3, b, ctx);
      done.clear();
      service(done, 1, b, ctx);
      check_sequence(done, {3}, ctx + " after watchdog stop");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_ESTATE, ctx + " drained stream must stop before restart");
      FX1_CHECK_EQ_CTX(fx1_isp_stop(&dev), 0, ctx + " drained stop drops no frames");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT, 0,
                       ctx + " drained stop clears transient underrun");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_COMMON_IRQ_STATUS_OFFSET) & FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK, 0,
                       ctx + " drained stop clears transient error IRQ");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " restart after drained stop");
      queue_input(3, b, ctx);
      done.clear();
      service(done, 1, b, ctx);
      check_sequence(done, {3}, ctx + " after drained stop and restart");
   }

   static bool same_zone(const fx1_isp_aec_zone &a, const fx1_isp_aec_zone &b) {
      return std::memcmp(&a, &b, sizeof a) == 0;
   }

   // Item 2: zone/histogram data of an aborted frame, and after a soft reset,
   // is refused by the driver until a new publication (DEC-30, M3-A7).
   void test_stats_after_abort() {
      const fs::path sdir = root / "stats_flat_channels_64x48";
      const std::string ctx = "stats after abort";
      const std::uint32_t sw = 64, sh = 48, stride = align16(2 * sw);
      reset_and_init();
      apply_profile(fx1_test::read_profile(sdir / "profile.csrw"));
      const fx1_isp_buffers b = buffer_layout(stride, align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ(fx1_isp_start(&dev, &b), FX1_ISP_OK);
      const std::vector<std::uint8_t> raw = read_file(sdir / "input.bin");
      std::vector<std::vector<std::uint8_t>> done;
      auto submit = [&] {
         load_input(fx1_isp_next_input(&dev), raw, sw, sh, stride, b);
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
      };
      constexpr unsigned last_zone = 16 * 12 - 1;  // the profile's 16x12 grid
      fx1_isp_aec_zone z0[2]{}, z[2]{};
      std::uint32_t h0[64], h[64], id = 0;
      submit();
      service(done, 1, b, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 1, &z0[0], h0, &id), FX1_ISP_OK, ctx + " first frame");
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, last_zone, 1, &z0[1], nullptr, nullptr), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(id, 1, ctx + " FRAME_ID of the first frame");

      // The second frame's input fails in its middle; the handler runs late.
      const std::uint64_t row = b.in[1] + std::uint64_t{sh / 2} * stride;
      mem->faults.push_back({tlm::TLM_READ_COMMAND, row, row + 2u * sw, 1});
      submit();
      const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
      while ((!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) ||
              (rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK)) &&
             sc_time_stamp() < end) {
         wait(100, SC_NS);  // until the pipeline has abandoned the frame
      }
      // The hazard the driver guards against: idle, old FRAME_ID, partial memory.
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK, 0, ctx + " AEC idle after abort");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_AEC_FRAME_ID_OFFSET), 1, ctx + " FRAME_ID still the first frame's");
      hal_write(this, FX1_ISP_AEC_ZONE_ADDR_OFFSET, last_zone);
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET), 4095, ctx + " bottom zone not reached (partial)");
      fx1_isp_events ev{};
      fx1_isp_irq(&dev, &ev);
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 1, &z[0], h, nullptr), FX1_ISP_ENODATA,
                       ctx + " partial data refused before recovery");
      fx1_isp_recovery r{};
      FX1_CHECK_EQ_CTX(fx1_isp_recover_ex(&dev, ev.dma_err, &r), FX1_ISP_OK, ctx + " recover");
      FX1_CHECK_EQ_CTX(r.idma_rearmed, 1, ctx + " input re-armed");
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 1, &z[0], h, nullptr) == FX1_ISP_OK ? 1 : 0, 0,
                       ctx + " still refused before the retried frame publishes");
      service(done, 2, b, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 1, &z[0], h, &id), FX1_ISP_OK, ctx + " retried frame");
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, last_zone, 1, &z[1], nullptr, nullptr), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(id, 3, ctx + " the aborted frame consumed FRAME_ID 2");
      FX1_CHECK_EQ_CTX(same_zone(z[0], z0[0]) && same_zone(z[1], z0[1]) && std::memcmp(h, h0, sizeof h) == 0 ? 1 : 0, 1,
                       ctx + " same input, same statistics");

      // Soft reset: the memories read 0 under the kept FRAME_ID.
      fx1_isp_soft_reset(&dev);
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 1, &z[0], h, nullptr), FX1_ISP_ENODATA, ctx + " after soft reset");
      submit();
      done.clear();
      service(done, 1, b, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 1, &z[0], h, &id), FX1_ISP_OK, ctx + " after the next frame");
      FX1_CHECK_EQ_CTX(id, 4, ctx + " FRAME_ID after soft reset");
      FX1_CHECK_EQ_CTX(same_zone(z[0], z0[0]) ? 1 : 0, 1, ctx + " statistics after soft reset");

      // Soft reset, a publication nobody reads, then an aborted frame: the
      // driver must judge the data against the latest event, not the reset.
      fx1_isp_soft_reset(&dev);
      submit();
      done.clear();
      service(done, 1, b, ctx);  // FRAME_ID 5, not read
      const std::uint64_t row2 = b.in[fx1_isp_next_input(&dev)] + std::uint64_t{sh / 2} * stride;
      mem->faults.push_back({tlm::TLM_READ_COMMAND, row2, row2 + 2u * sw, 1});
      submit();
      const sc_time end2 = sc_time_stamp() + sc_time(20, SC_MS);
      while ((!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) ||
              (rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK)) &&
             sc_time_stamp() < end2) {
         wait(100, SC_NS);
      }
      fx1_isp_irq(&dev, &ev);
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_AEC_FRAME_ID_OFFSET), 5, ctx + " last publication before the abort");
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 1, &z[0], h, nullptr), FX1_ISP_ENODATA,
                       ctx + " abort after an unread publication");
      FX1_CHECK_EQ_CTX(fx1_isp_recover(&dev, ev.dma_err), FX1_ISP_OK, ctx + " recover");
      service(done, 2, b, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_zones(&dev, 0, 1, &z[0], h, &id), FX1_ISP_OK, ctx + " after the retry");
      FX1_CHECK_EQ_CTX(id, 7, ctx + " FRAME_ID after the retry");
      mem->faults.clear();
   }

   void test_first_frame_abort_stats() {
      const fs::path sdir = root / "stats_flat_channels_64x48";
      reset_and_init();
      apply_profile(fx1_test::read_profile(sdir / "profile.csrw"));
      const fx1_isp_buffers b = buffer_layout(128, 64);
      FX1_CHECK_EQ(fx1_isp_start(&dev, &b), FX1_ISP_OK);
      const auto raw = read_file(sdir / "input.bin");
      load_input(fx1_isp_next_input(&dev), raw, 64, 48, 128, b);
      const std::uint64_t row = b.in[0] + 24u * 128u;
      mem->faults.push_back({tlm::TLM_READ_COMMAND, row, row + 128u, 1});
      FX1_CHECK_EQ(fx1_isp_queue_frame(&dev), FX1_ISP_OK);
      const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
      while ((!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) ||
              (rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK)) && sc_time_stamp() < end) {
         wait(100, SC_NS);
      }
      FX1_CHECK_EQ(rd(FX1_ISP_AEC_FRAME_ID_OFFSET), 0u);
      fx1_isp_aec_zone zone{};
      std::uint32_t hist[64]{};
      FX1_CHECK_EQ(fx1_isp_read_aec_zones(&dev, 0, 1, &zone, hist, nullptr), FX1_ISP_ENODATA);
      fx1_isp_events ev{};
      fx1_isp_irq(&dev, &ev);
      FX1_CHECK_EQ(fx1_isp_read_aec_zones(&dev, 0, 1, &zone, hist, nullptr), FX1_ISP_ENODATA);
   }

   // Item 1: coordinated faults. Fixed seed; each run injects an IDMA read
   // error and/or an ODMA write error (each possibly hitting twice, i.e.
   // again after the re-arm) at a random buffer and line, and runs the
   // interrupt handler 0 us .. 2 ms late, so several completions and both
   // errors can be pending at once. The caller resubmits every frame the
   // driver reports lost. Checks: every logical frame completes exactly once
   // with the right content, completions in queue order, a stale second
   // recover is a no-op, no deadlock, clean final state.
   struct campaign_stats {
      unsigned runs = 0, both_at_once = 0, repeated = 0, multi_done = 0, lost = 0, rearmed = 0, stale = 0,
               odma_failed = 0;
   };
   void campaign_run(std::mt19937 &rng, unsigned iter, campaign_stats &cs) {
      const fs::path dir = root / "mandatory_rggb_64x32";
      std::string ctx = "campaign " + std::to_string(iter);
      const fx1_isp_buffers b = start_error_case(dir);
      const std::uint32_t stride = align16(2 * ew);
      auto pick = [&](unsigned n) { return static_cast<unsigned>(rng() % n); };
      const unsigned frames = 4 + pick(5);
      std::string desc = " [" + std::to_string(frames) + " frames";
      if (pick(10) < 7) {
         const unsigned slot = pick(4), line = pick(eh);
         const int times = 1 + static_cast<int>(pick(2));
         const std::uint64_t row = b.in[slot] + std::uint64_t{line} * stride;
         mem->faults.push_back({tlm::TLM_READ_COMMAND, row, row + 2u * ew, times});
         desc += ", IDMA in" + std::to_string(slot) + " line " + std::to_string(line) + " x" + std::to_string(times);
      }
      if (pick(10) < 7) {
         const unsigned slot = pick(4);
         const bool chroma = pick(3) == 0;
         const std::uint64_t base = chroma ? b.uv[slot] : b.y[slot];
         const std::uint32_t rows = chroma ? dev.out_height / 2 : dev.out_height;
         const unsigned line = pick(rows);
         const int times = 1 + static_cast<int>(pick(2));
         const std::uint64_t row = base + std::uint64_t{line} * (chroma ? b.uv_stride : b.y_stride);
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, row, row + dev.out_width, times});
         desc += std::string(", ODMA ") + (chroma ? "uv" : "y") + std::to_string(slot) + " line " + std::to_string(line) +
                 " x" + std::to_string(times);
      }
      const sc_time lateness[] = {SC_ZERO_TIME, sc_time(20, SC_US), sc_time(200, SC_US), sc_time(2, SC_MS)};
      const sc_time late = lateness[pick(4)];
      ctx += desc + ", handler +" + late.to_string() + "]";
      std::vector<unsigned> pending;
      for (unsigned f = 0; f < frames; ++f) {
         pending.push_back(f);
      }
      std::map<std::uint32_t, unsigned> logical;
      std::map<std::uint32_t, unsigned> idma_hits;
      std::map<unsigned, unsigned> odma_hits;
      std::vector<unsigned> done_count(frames, 0);
      std::int64_t last_seq = -1;
      unsigned completed = 0;
      const sc_time end = sc_time_stamp() + sc_time(500, SC_MS);
      while (completed < frames) {
         while (!pending.empty() && fx1_isp_input_ready(&dev)) {
            const unsigned lf = pending.front();
            pending.erase(pending.begin());
            load_input(fx1_isp_next_input(&dev), e_inputs[lf % 4], ew, eh, stride, b);
            std::uint32_t seq = 0;
            FX1_CHECK_EQ_CTX(fx1_isp_queue_frame_ex(&dev, &seq), FX1_ISP_OK, ctx + " queue");
            logical[seq] = lf;
         }
         if (!irq.read()) {
            wait(sc_time(1, SC_MS), irq.posedge_event());
         }
         if (sc_time_stamp() >= end) {
            FX1_CHECK_EQ_CTX(0, 1, ctx + ": deadlock, " + std::to_string(completed) + "/" + std::to_string(frames) +
                                       " in_flight " + std::to_string(dev.in_flight) + " VALID " +
                                       std::to_string(rd(FX1_ISP_IDMA_BUF_VALID_OFFSET)) + " FREE " +
                                       std::to_string(rd(FX1_ISP_ODMA_BUF_FREE_OFFSET)) + " DONE " +
                                       std::to_string(rd(FX1_ISP_ODMA_BUF_DONE_OFFSET)) + " DMA_ERR " +
                                       std::to_string(rd(FX1_ISP_DMA_ERR_OFFSET)) + " heads " +
                                       std::to_string(dut->debug_idma_head()) + "/" + std::to_string(dut->debug_odma_head()) +
                                       " drv in/out/done " + std::to_string(dev.in_next) + "/" +
                                       std::to_string(dev.out_next) + "/" + std::to_string(dev.out_done) +
                                       " pending " + std::to_string(pending.size()));
            break;
         }
         if (late > SC_ZERO_TIME) {
            wait(late);
         }
         fx1_isp_events ev{};
         fx1_isp_irq(&dev, &ev);
         cs.multi_done += ev.frames_done >= 2;
         const std::uint32_t axi = FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT;
         if (ev.dma_err & axi) {
            // A second call with the same snapshot handles only errors that are
            // flagged again in hardware (a fault hitting again right after the
            // re-arm); otherwise it is a no-op that keeps the driver state.
            for (int call = 0; call < 2; ++call) {
               const unsigned inf = dev.in_flight, onext = dev.out_next;
               fx1_isp_recovery r{};
               const int rc = fx1_isp_recover_ex(&dev, ev.dma_err, &r);
               if (call == 1 && rc == FX1_ISP_ENODATA) {
                  FX1_CHECK_EQ_CTX(r.handled, 0, ctx + " stale recover handles nothing");
                  FX1_CHECK_EQ_CTX(dev.in_flight == inf && dev.out_next == onext ? 1 : 0, 1,
                                   ctx + " stale recover keeps state");
                  ++cs.stale;
                  break;
               }
               FX1_CHECK_EQ_CTX(rc, FX1_ISP_OK, ctx + " recover");
               FX1_CHECK_EQ_CTX(r.handled & ~ev.dma_err, 0, ctx + " handles only the reported bits");
               cs.both_at_once += (r.handled & axi) == axi;
               if (r.idma_rearmed) {
                  ++cs.rearmed;
                  cs.repeated += idma_hits[r.idma_seq]++ > 0;
               }
               if (r.odma_failed) {
                  ++cs.odma_failed;
                  cs.repeated += odma_hits[r.odma_output]++ > 0;
               }
            }
         }
         // Frames the driver knows are lost (possibly decided only now, once the
         // IDMA retired their input): resubmit them first.
         std::vector<unsigned> lost_now;
         std::uint32_t lseq = 0;
         unsigned linput = 0;
         while (fx1_isp_next_lost(&dev, &lseq, &linput) == FX1_ISP_OK) {
            FX1_CHECK_EQ_CTX(logical.count(lseq), 1, ctx + " lost frame known");
            ++cs.lost;
            lost_now.push_back(logical[lseq]);
         }
         pending.insert(pending.begin(), lost_now.begin(), lost_now.end());
         unsigned idx = 0;
         std::uint32_t seq = 0;
         while (fx1_isp_next_done_ex(&dev, &idx, &seq) == FX1_ISP_OK) {
            FX1_CHECK_EQ_CTX(static_cast<std::int64_t>(seq) > last_seq ? 1 : 0, 1, ctx + " completion order");
            last_seq = seq;
            const unsigned lf = logical.at(seq);
            FX1_CHECK_EQ_CTX(output(idx, b) == e_expected[lf % 4] ? 1 : 0, 1,
                             ctx + " content of logical frame " + std::to_string(lf));
            completed += done_count[lf]++ == 0;
         }
      }
      for (unsigned f = 0; f < frames; ++f) {
         FX1_CHECK_EQ_CTX(done_count[f], 1, ctx + " logical frame " + std::to_string(f) + " exactly once");
      }
      FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " nothing in flight");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), frames, ctx + " ODMA_FRAME_COUNT");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_ERR_OFFSET) & (FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT |
                                                  FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT),
                       0, ctx + " error state clean");
      mem->faults.clear();
      ++cs.runs;
   }
   void run_fault_campaign() {
      std::mt19937 rng(20261002u);
      campaign_stats cs;
      for (unsigned i = 0; i < 48; ++i) {
         campaign_run(rng, i, cs);
      }
      std::cout << "fault campaign: " << cs.runs << " runs, " << cs.rearmed << " IDMA re-arms, " << cs.odma_failed
                << " ODMA failures (" << cs.lost << " frames lost), " << cs.both_at_once << " with both errors at once, " << cs.repeated
                << " repeated after re-arm, " << cs.multi_done << " handler calls with >= 2 completions, " << cs.stale
                << " stale recovers\n";
      // The campaign must actually reach the coordinated cases it is for.
      FX1_CHECK(cs.both_at_once > 0);
      FX1_CHECK(cs.repeated > 0);
      FX1_CHECK(cs.multi_done > 0);
   }

   // A frame whose output fails while the IDMA is still reading its input is
   // only *possibly* lost. (a) The IDMA then fails on the same frame: the frame
   // is retried and completes once, nothing is lost. (b) The IDMA finishes it:
   // the loss is reported, by interrupt only (the IDMA_DONE source), also when
   // it is the last frame and nothing else would raise the line.
   void test_deferred_loss() {
      const fs::path dir = root / "mandatory_rggb_64x32";
      const std::uint32_t stride = align16(2 * ew);
      for (const bool idma_also_fails : {true, false}) {
         const std::string ctx = std::string("deferred loss, ") + (idma_also_fails ? "IDMA retry" : "IDMA completes");
         const fx1_isp_buffers b = start_error_case(dir);
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, b.y[0], b.y[0] + dev.out_width, 1});  // output line 0
         if (idma_also_fails) {
            const std::uint64_t row = b.in[0] + std::uint64_t{eh - 4} * stride;  // late in the same input
            mem->faults.push_back({tlm::TLM_READ_COMMAND, row, row + 2u * ew, 1});
         }
         queue_input(0, b, ctx);
         // The handler reacts to the ODMA error at once, before the IDMA is done.
         const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
         while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) && sc_time_stamp() < end) {
            wait(10, SC_NS);
         }
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_BUF_VALID_OFFSET) & 1u, 1, ctx + " IDMA still reading");
         fx1_isp_events ev{};
         fx1_isp_irq(&dev, &ev);
         fx1_isp_recovery r{};
         FX1_CHECK_EQ_CTX(fx1_isp_recover_ex(&dev, ev.dma_err, &r), FX1_ISP_OK, ctx + " recover ODMA");
         FX1_CHECK_EQ_CTX(r.odma_failed, 1, ctx + " output failure seen");
         std::uint32_t lseq = 0;
         unsigned linput = 0;
         FX1_CHECK_EQ_CTX(fx1_isp_next_lost(&dev, &lseq, &linput), FX1_ISP_ENODATA, ctx + " not lost yet");
         FX1_CHECK_EQ_CTX(dev.in_flight, 1, ctx + " still in flight");
         // From here on, act only on interrupt edges (no polling).
         std::vector<std::vector<std::uint8_t>> done;
         bool lost = false;
         while (sc_time_stamp() < end && done.empty() && !lost) {
            if (!irq.read()) {
               wait(end - sc_time_stamp(), irq.posedge_event());
            }
            if (sc_time_stamp() >= end) {
               break;
            }
            fx1_isp_irq(&dev, &ev);
            if (ev.dma_err & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) {
               FX1_CHECK_EQ_CTX(fx1_isp_recover_ex(&dev, ev.dma_err, &r), FX1_ISP_OK, ctx + " recover IDMA");
               FX1_CHECK_EQ_CTX(r.idma_rearmed, 1, ctx + " same input re-armed");
            }
            lost = fx1_isp_next_lost(&dev, &lseq, &linput) == FX1_ISP_OK;
            unsigned idx = 0;
            while (fx1_isp_next_done(&dev, &idx) == FX1_ISP_OK) {
               done.push_back(output(idx, b));
            }
            if (!irq.read()) {
               continue;
            }
            wait(1, SC_US);  // let the acknowledged line fall
         }
         if (idma_also_fails) {
            FX1_CHECK_EQ_CTX(lost ? 1 : 0, 0, ctx + " not reported lost");
            FX1_CHECK_EQ_CTX(done.size(), 1, ctx + " retried frame completes");
            FX1_CHECK_EQ_CTX(!done.empty() && done[0] == e_expected[0] ? 1 : 0, 1, ctx + " retried frame content");
         } else {
            FX1_CHECK_EQ_CTX(lost ? 1 : 0, 1, ctx + " reported lost, on the IDMA_DONE interrupt");
            FX1_CHECK_EQ_CTX(lseq == 0 && linput == 0 ? 1 : 0, 1, ctx + " lost frame identity");
            FX1_CHECK_EQ_CTX(done.size(), 0, ctx + " no completion");
         }
         FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " nothing in flight");
         mem->faults.clear();
      }
   }

   void run() {
      for (const auto &e : fs::directory_iterator(root)) {
         if (e.is_directory()) {
            run_vector(e.path());
         }
      }
      run_error_cases();
      test_driver_state_machine();
      test_stats_after_abort();
      test_first_frame_abort_stats();
      test_deferred_loss();
      run_fault_campaign();
      sc_stop();
   }
};

}  // namespace

int sc_main(int argc, char *argv[]) {
   if (argc < 2) {
      std::cerr << "usage: " << argv[0] << " <vectors dir>\n";
      return 2;
   }
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_tlm dut("isp", fx1_isp_params{});
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   bench tb("tb");
   tb.root = argv[1];
   tb.socket.bind(dut.csr_socket);
   dut.idma_socket.bind(mem.idma);
   dut.odma_socket.bind(mem.odma);
   dut.rst_n(rst_n);
   dut.irq(irq);
   tb.rst_n(rst_n);
   tb.irq(irq);
   tb.dut = &dut;
   tb.mem = &mem;
   sc_start();
   FX1_CHECK(tb.vectors >= 32);
   std::cout << "driver: " << tb.vectors << " vectors;";
   for (const auto &kv : tb.helpers) {
      std::cout << " " << kv.first << "=" << kv.second;
      FX1_CHECK(kv.second > 0);
   }
   std::cout << "\n";
   FX1_CHECK_EQ(tb.helpers.size(), 8);  // every helper kind exercised
   return fx1_test::summary("fx1_isp_test_driver_tlm");
}
