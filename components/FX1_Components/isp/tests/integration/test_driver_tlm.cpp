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
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "dma/dma_rules.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "registers/csr_desc.h"
#include "test_memory.h"
#include "vector_io.h"

// The firmware fixture (fw/fx1_soc/tests/isp_stream), when this component
// is built inside the CDC-VP tree: the `isp selftest profile` flow replay.
#if __has_include("../../../../../fw/fx1_soc/tests/isp_stream/isp_fixture.h")
#include "../../../../../fw/fx1_soc/tests/isp_stream/isp_fixture.h"
#define FX1_HAVE_FW_FIXTURE 1
#endif

using namespace sc_core;
using namespace cdc::components::fx1_isp;
namespace fs = std::filesystem;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 24u << 20;
// CH1 / CH2 planes, armed by the bench until the driver's channel buffer API (V107-10).
constexpr std::uint64_t ch1_y_base = mem_base + 0x1000000ull, ch1_uv_base = mem_base + 0x1200000ull;
constexpr std::uint64_t ch2_base = mem_base + 0x1400000ull;
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
   bool fast_csr = false;  // CSR accesses shorter than a core clock (review U2-R2)
   std::size_t vectors = 0, helper_calls = 0, profile_vectors = 0, profile_refused = 0;
   std::uint32_t expect_profile_id = 0;  // profile-mode run of a legacy vector
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
      bench *b = static_cast<bench *>(ctx);
      if (b->before_write) {  // test hook: an event right before a driver write
         b->before_write(off, v);
      }
      b->access(tlm::TLM_WRITE_COMMAND, off, v);
   }
   std::function<void(std::uint32_t, std::uint32_t)> before_write;
   static void hal_delay(void *ctx, std::uint32_t cycles) {
      static_cast<bench *>(ctx)->delay_requested += cycles;
      static_cast<bench *>(ctx)->wait(static_cast<bench *>(ctx)->dut->params().core_period * static_cast<double>(cycles));
   }
   std::uint64_t delay_requested = 0;  // core clocks the driver asked the HAL to wait
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
               FX1_CHECK_EQ(fx1_isp_load_gamma(&dev, lut), FX1_ISP_OK);  // written, verified, confirmed
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
               FX1_CHECK_EQ(fx1_isp_load_gtm_lut(&dev, t.data()), FX1_ISP_OK);  // no frame yet: nothing pending
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
               const std::uint32_t gh = lines[i + 1].value, gp = lines[i + 2].value;
               if (dma::active_geometry_legal(v, gh, gp)) {
                  FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, v, gh, gp), FX1_ISP_OK);
                  ++helpers["geometry"];
                  i += 3;
                  continue;
               }
               // Kernel-edge vectors below the v1.07 active minimum: the driver
               // refuses them (software rule, MAS §6.6.3.5); the hardware does
               // not, so the replay programs the registers directly.
               FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, v, gh, gp), FX1_ISP_EINVAL);
               ++helpers["geometry_below_active_minimum"];
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
   // recovers DMA errors, collects completed buffers in order. After a
   // recovery (MAS v1.07 §9.9.8) every frame queued through queue_input that
   // the driver reports lost is resubmitted, oldest first (`resubmitted`).
   std::size_t errors_recovered = 0;
   std::uint32_t last_dma_err = 0;
   std::map<std::uint32_t, unsigned> seq_input;  // driver sequence -> e_inputs index
   std::deque<unsigned> to_resubmit;
   std::vector<unsigned> resubmitted;
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
         if (recover) {
            std::uint32_t lseq = 0;
            while (fx1_isp_next_lost(&dev, &lseq, nullptr) == FX1_ISP_OK) {
               FX1_CHECK_EQ_CTX(seq_input.count(lseq), 1u, ctx + " lost frame known");
               to_resubmit.push_back(seq_input[lseq]);
            }
            while (!to_resubmit.empty() && fx1_isp_input_ready(&dev)) {
               const unsigned k = to_resubmit.front();
               to_resubmit.pop_front();
               queue_input(k, b, ctx + " resubmit");
               resubmitted.push_back(k);
            }
         }
      }
      return true;
   }

   // ---- statistics through the driver readers ----
   // One AWB result of the script: the reads between ACQUIRE and RELEASE.
   struct awb_block {
      std::map<std::string, std::uint32_t> want;  // "REG" or "AWB_ZONE_*@zone"
   };

   // AWB through the ownership API (MAS §6.22.4): every result the script
   // acquires, in the same order, with its identity, grid, globals and zones.
   void check_awb(const std::string &name, const std::vector<awb_block> &blocks) {
      const char *ch = "RGB";
      for (std::size_t k = 0; k < blocks.size(); ++k) {
         const auto &w = blocks[k].want;
         const std::string ctx = name + " awb result " + std::to_string(k);
         const auto want_of = [&](const std::string &key) {
            const auto it = w.find(key);
            return it == w.end() ? 0u : it->second;
         };
         fx1_isp_awb_lease lease{};
         FX1_CHECK_EQ_CTX(fx1_isp_awb_acquire(&dev, &lease), FX1_ISP_OK, ctx + " acquire");
         FX1_CHECK_EQ_CTX(fx1_isp_awb_acquire(&dev, nullptr), FX1_ISP_EBUSY, ctx + " second acquire");
         const std::uint32_t st = want_of("AWB_RESULT_STATUS");
         FX1_CHECK_EQ_CTX(lease.frame_id, want_of("AWB_FRAME_ID"), ctx + " FRAME_ID");
         FX1_CHECK_EQ_CTX(lease.context_id, want_of("AWB_RESULT_CONTEXT_ID"), ctx + " CONTEXT_ID");
         FX1_CHECK_EQ_CTX(lease.nx, (st & FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_X_MASK) >>
                                        FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_X_SHIFT, ctx + " nx");
         FX1_CHECK_EQ_CTX(lease.ny, (st & FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_Y_MASK) >>
                                        FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_Y_SHIFT, ctx + " ny");
         FX1_CHECK_EQ_CTX(lease.bank, (st & FX1_ISP_AWB_RESULT_STATUS_READ_BANK_MASK) >>
                                          FX1_ISP_AWB_RESULT_STATUS_READ_BANK_SHIFT, ctx + " bank");
         fx1_isp_awb_global g{};
         FX1_CHECK_EQ_CTX(fx1_isp_awb_read_global(&dev, &g), FX1_ISP_OK, ctx + " global");
         for (unsigned c = 0; c < 3; ++c) {
            FX1_CHECK_EQ_CTX(g.sum[c] & 0xFFFFFFFFu, want_of(std::string("AWB_GLOBAL_SUM_") + ch[c] + "_L"), ctx);
            FX1_CHECK_EQ_CTX(g.sum[c] >> 32, want_of(std::string("AWB_GLOBAL_SUM_") + ch[c] + "_H"), ctx);
         }
         FX1_CHECK_EQ_CTX(g.count, want_of("AWB_GLOBAL_COUNT"), ctx + " count");
         const unsigned n = lease.nx * lease.ny;
         for (const auto &kv : w) {
            if (kv.first.rfind("AWB_ZONE_COUNT@", 0) != 0) {
               continue;
            }
            const unsigned z = static_cast<unsigned>(std::stoul(kv.first.substr(15)));
            fx1_isp_awb_zone zr{};
            if (z >= n) {  // beyond the captured grid: the driver refuses, the hardware reads 0
               FX1_CHECK_EQ_CTX(fx1_isp_awb_read_zones(&dev, z, 1, &zr), FX1_ISP_EINVAL, ctx + " zone beyond grid");
               FX1_CHECK_EQ_CTX(kv.second, 0u, ctx + " script zone beyond grid");
               continue;
            }
            FX1_CHECK_EQ_CTX(fx1_isp_awb_read_zones(&dev, z, 1, &zr), FX1_ISP_OK, ctx + " zone");
            const std::string zs = "@" + std::to_string(z);
            FX1_CHECK_EQ_CTX(zr.count, kv.second, ctx + " zone count" + zs);
            for (unsigned c = 0; c < 3; ++c) {
               FX1_CHECK_EQ_CTX(zr.sum[c] & 0xFFFFFFFFu, want_of(std::string("AWB_ZONE_SUM_") + ch[c] + "_L" + zs), ctx + zs);
               FX1_CHECK_EQ_CTX(zr.sum[c] >> 32, want_of(std::string("AWB_ZONE_SUM_") + ch[c] + "_H" + zs), ctx + zs);
            }
         }
         FX1_CHECK_EQ_CTX(fx1_isp_awb_release(&dev), FX1_ISP_OK, ctx + " release");
      }
      FX1_CHECK_EQ_CTX(fx1_isp_awb_acquire(&dev, nullptr), FX1_ISP_ENODATA, name + " awb: nothing left");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_release(&dev), FX1_ISP_ESTATE, name + " awb: release without a lease");
   }

   // One AEC or AF lease of the script: the 64-bit results it reads through
   // the wide port (index -> value) and its narrow reads ("REG@selectors").
   struct lease_block {
      std::map<std::uint32_t, std::uint64_t> wide;
      std::map<std::string, std::uint32_t> want;
   };

   // AEC through the lease API (MAS v1.07 §9.8): every result the script
   // acquires, in the same order: identity, geometry, globals, zones, bins.
   void check_aec(const std::string &name, const std::vector<lease_block> &blocks) {
      for (std::size_t k = 0; k < blocks.size(); ++k) {
         const lease_block &b = blocks[k];
         const std::string ctx = name + " aec result " + std::to_string(k);
         const auto want_of = [&](const std::string &key) {
            const auto it = b.want.find(key);
            return it == b.want.end() ? 0u : it->second;
         };
         const auto wide_of = [&](std::uint32_t i) {
            const auto it = b.wide.find(i);
            return it == b.wide.end() ? std::uint64_t{0} : it->second;
         };
         fx1_isp_stat_lease lease{};
         FX1_CHECK_EQ_CTX(fx1_isp_aec_acquire(&dev, &lease), FX1_ISP_OK, ctx + " acquire");
         FX1_CHECK_EQ_CTX(fx1_isp_aec_acquire(&dev, nullptr), FX1_ISP_EBUSY, ctx + " second acquire");
         FX1_CHECK_EQ_CTX(lease.frame_id, want_of("TUNE_AEC_FRAME"), ctx + " FRAME");
         FX1_CHECK_EQ_CTX(lease.context_id, want_of("AEC_RESULT_CONTEXT_ID"), ctx + " CONTEXT_ID");
         // The expected scripts are legacy runs (profile ID 0); a profile-mode
         // run publishes the active profile's ID with the result.
         FX1_CHECK_EQ_CTX(lease.profile_id, expect_profile_id ? expect_profile_id : want_of("TUNE_AEC_PROFILE"),
                          ctx + " PROFILE");
         FX1_CHECK_EQ_CTX(lease.width | (lease.height << 16), want_of("TUNE_AE_FRAME_SIZE"), ctx + " FRAME_SIZE");
         FX1_CHECK_EQ_CTX(lease.nx | (lease.ny << 16), want_of("TUNE_AE_GRID"), ctx + " GRID");
         FX1_CHECK_EQ_CTX(lease.zone_w | (lease.zone_h << 16), want_of("TUNE_AE_ZONE_SIZE"), ctx + " ZONE_SIZE");
         FX1_CHECK_EQ_CTX(lease.source, want_of("TUNE_AEC_RESULT_META") & 1u, ctx + " source");
         fx1_isp_aec_global g{};
         FX1_CHECK_EQ_CTX(fx1_isp_aec_read_global(&dev, &g), FX1_ISP_OK, ctx + " global");
         const std::uint64_t gv[] = {g.sum, g.count, g.oe, g.ue, g.min, g.max, g.weighted_total, g.weighted_sum,
                                     g.overflow ? 1u : 0u};
         for (std::uint32_t i = 0; i < 9; ++i) {
            FX1_CHECK_EQ_CTX(gv[i], wide_of(i), ctx + " index " + std::to_string(i));
         }
         FX1_CHECK_EQ_CTX(g.sum & 0xFFFFFFFFu, want_of("AEC_GLOBAL_SUM_LO@0"), ctx + " narrow sum");
         std::uint64_t hist[64], weighted[64];
         FX1_CHECK_EQ_CTX(fx1_isp_aec_read_hist(&dev, hist, weighted), FX1_ISP_OK, ctx + " hist");
         for (std::uint32_t i = 0; i < 64; ++i) {
            FX1_CHECK_EQ_CTX(hist[i], wide_of(0x100 + i), ctx + " bin " + std::to_string(i));
            FX1_CHECK_EQ_CTX(weighted[i], wide_of(0x140 + i), ctx + " weighted bin " + std::to_string(i));
         }
         const unsigned n = lease.nx * lease.ny;
         for (const auto &kv : b.wide) {
            if (kv.first < 0x200 || (kv.first - 0x200) % 8 != 0) {
               continue;
            }
            const unsigned z = (kv.first - 0x200) / 8;
            fx1_isp_aec_zone zr{};
            if (z >= n) {  // beyond the captured grid: the driver refuses, the hardware reads 0
               FX1_CHECK_EQ_CTX(fx1_isp_aec_read_zones(&dev, z, 1, &zr), FX1_ISP_EINVAL, ctx + " zone beyond grid");
               continue;
            }
            FX1_CHECK_EQ_CTX(fx1_isp_aec_read_zones(&dev, z, 1, &zr), FX1_ISP_OK, ctx + " zone");
            const std::uint64_t zv[] = {zr.sum, zr.count, zr.oe, zr.ue, zr.min, zr.max};
            for (unsigned j = 0; j < 6; ++j) {
               FX1_CHECK_EQ_CTX(zv[j], wide_of(kv.first + j), ctx + " zone " + std::to_string(z) + "/" + std::to_string(j));
            }
         }
         std::uint64_t v = 0;
         FX1_CHECK_EQ_CTX(fx1_isp_aec_result(&dev, 0x9, &v), FX1_ISP_EINVAL, ctx + " reserved index");
         FX1_CHECK_EQ_CTX(fx1_isp_aec_release(&dev), FX1_ISP_OK, ctx + " release");
      }
      FX1_CHECK_EQ_CTX(fx1_isp_aec_acquire(&dev, nullptr), FX1_ISP_ENODATA, name + " aec: nothing left");
      FX1_CHECK_EQ_CTX(fx1_isp_aec_release(&dev), FX1_ISP_ESTATE, name + " aec: release without a lease");
   }

   void check_af(const std::string &name, const std::vector<lease_block> &blocks) {
      for (std::size_t k = 0; k < blocks.size(); ++k) {
         const lease_block &b = blocks[k];
         const std::string ctx = name + " af result " + std::to_string(k);
         const auto want_of = [&](const std::string &key) {
            const auto it = b.want.find(key);
            return it == b.want.end() ? 0u : it->second;
         };
         const auto wide_of = [&](std::uint32_t i) {
            const auto it = b.wide.find(i);
            return it == b.wide.end() ? std::uint64_t{0} : it->second;
         };
         fx1_isp_stat_lease lease{};
         FX1_CHECK_EQ_CTX(fx1_isp_af_acquire(&dev, &lease), FX1_ISP_OK, ctx + " acquire");
         FX1_CHECK_EQ_CTX(lease.frame_id, want_of("TUNE_AF_FRAME"), ctx + " FRAME");
         FX1_CHECK_EQ_CTX(lease.context_id, want_of("AF_RESULT_CONTEXT_ID"), ctx + " CONTEXT_ID");
         FX1_CHECK_EQ_CTX(lease.width, wide_of(0x10), ctx + " width");
         fx1_isp_af_window win[16];
         std::uint32_t mask = 0;
         unsigned mode = 0;
         FX1_CHECK_EQ_CTX(fx1_isp_af_read(&dev, win, &mask, &mode), FX1_ISP_OK, ctx + " read");
         FX1_CHECK_EQ_CTX(mask, wide_of(0x12), ctx + " mask");
         FX1_CHECK_EQ_CTX(mode, wide_of(0x13), ctx + " mode");
         for (std::uint32_t w = 0; w < 16; ++w) {
            const std::string ws = " window " + std::to_string(w);
            FX1_CHECK_EQ_CTX(win[w].focus, wide_of(0x100 + 4 * w), ctx + ws + " focus");
            FX1_CHECK_EQ_CTX(win[w].luma, wide_of(0x101 + 4 * w), ctx + ws + " luma");
            FX1_CHECK_EQ_CTX(win[w].count, wide_of(0x102 + 4 * w), ctx + ws + " count");
            FX1_CHECK_EQ_CTX(win[w].x0 | (win[w].y0 << 16), wide_of(0x200 + 2 * w), ctx + ws + " start");
            FX1_CHECK_EQ_CTX(win[w].x1 | (win[w].y1 << 16), wide_of(0x201 + 2 * w), ctx + ws + " end");
            FX1_CHECK_EQ_CTX(win[w].focus & 0xFFFFFFFFu, want_of("AF_STAT_DATA@" + std::to_string(w)), ctx + ws);
         }
         FX1_CHECK_EQ_CTX(fx1_isp_af_release(&dev), FX1_ISP_OK, ctx + " release");
      }
      FX1_CHECK_EQ_CTX(fx1_isp_af_acquire(&dev, nullptr), FX1_ISP_ENODATA, name + " af: nothing left");
   }

   void check_stats(const std::string &name, const std::vector<fx1_test::csr_line> &script) {
      std::map<std::string, std::uint32_t> sel;
      std::map<std::string, std::uint32_t> want;  // plain registers outside a lease
      std::vector<awb_block> awb;
      std::vector<lease_block> aec, af;
      enum { none, in_awb, in_aec, in_af } in = none;
      std::uint32_t lo = 0;
      for (const fx1_test::csr_line &l : script) {
         if (l.set) {
            const bool acquire = (l.value & 1u) != 0;
            if (l.reg == "AWB_RESULT_CTRL") {
               in = acquire ? in_awb : none;
               if (acquire) {
                  awb.emplace_back();
               }
            } else if (l.reg == "TUNE_AEC_CMD") {
               in = acquire ? in_aec : none;
               if (acquire) {
                  aec.emplace_back();
               }
            } else if (l.reg == "TUNE_AF_CMD") {
               in = acquire ? in_af : none;
               if (acquire) {
                  af.emplace_back();
               }
            }
            sel[l.reg] = l.value;
            continue;
         }
         if (in == in_awb) {
            const bool zone = l.reg.rfind("AWB_ZONE", 0) == 0;
            awb.back().want[zone ? l.reg + "@" + std::to_string(sel["AWB_ZONE_ADDR"]) : l.reg] = l.value;
            continue;
         }
         if (in == in_aec || in == in_af) {
            lease_block &b = in == in_aec ? aec.back() : af.back();
            const std::string prefix = in == in_aec ? "TUNE_AEC" : "TUNE_AF";
            if (l.reg == prefix + "_DATA_LO") {
               lo = l.value;
            } else if (l.reg == prefix + "_DATA_HI") {
               b.wide[sel[prefix + "_INDEX"]] = (std::uint64_t{l.value} << 32) | lo;
            } else if (l.reg == "AEC_GLOBAL_SUM_LO") {
               b.want[l.reg + "@" + std::to_string(sel["AEC_CHANNEL_SEL"])] = l.value;
            } else if (l.reg == "AF_STAT_DATA") {
               b.want[l.reg + "@" + std::to_string(sel["AF_STAT_ADDR"])] = l.value;
            } else {
               b.want[l.reg] = l.value;
            }
            continue;
         }
         // Results outside a lease read 0 (checked by the vector tests).
         if (l.reg.rfind("AWB_", 0) == 0 || l.reg.rfind("AEC_", 0) == 0 || l.reg.rfind("AF_", 0) == 0 ||
             l.reg.rfind("TUNE_", 0) == 0) {
            if (l.reg != "AWB_STATUS" && l.reg != "AWB_DROP_COUNT" && l.reg != "AEC_STATUS" && l.reg != "AF_STATUS" &&
                l.reg != "TUNE_AEC_DROPS" && l.reg != "TUNE_AF_DROPS") {
               continue;
            }
         }
         want[l.reg] = l.value;
      }
      check_aec(name, aec);
      check_awb(name, awb);
      check_af(name, af);
      // Plain registers of the script (status, counters) as raw reads.
      for (const auto &kv : want) {
         if (kv.first == "AEC_STATUS" || kv.first == "AF_STATUS") {
            continue;  // done bits: the driver services them (IRQ handler)
         }
         if (const csr::reg_desc *d = reg(kv.first)) {
            FX1_CHECK_EQ_CTX(rd(d->offset), kv.second, name + " " + kv.first);
         }
      }
   }

   // profile_mode: the same vector through the firmware's default bring-up
   // (V107-10, review A44): the legacy profile applied, then
   // fx1_isp_profile_from_legacy builds bank 0 from it (INIT, payloads
   // reloaded, SEAL, APPLY); every frame and statistic must equal legacy's.
   void run_vector(const fs::path &dir, bool profile_mode = false) {
      const std::string name = dir.filename().string() + (profile_mode ? " (profile mode)" : "");
      reset_and_init();
      std::ifstream meta(dir / "meta.txt");
      std::uint32_t in_w = 0, in_h = 0, out_w = 0, out_h = 0, frames = 1;
      meta >> in_w >> in_h >> out_w >> out_h >> frames;
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      const std::uint32_t in_stride = align16(2 * in_w);
      // Below the 32 x 32 active minimum the channel geometry is invalid
      // (MAS v1.07 §8.3, ERROR 1): the driver refuses to start, no frame runs.
      std::uint32_t gst = 0;
      FX1_CHECK_EQ_CTX(fx1_isp_geometry_status(&dev, 0, &gst), FX1_ISP_OK, name + " geometry settles");
      if (!(gst & FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK)) {
         FX1_CHECK_EQ_CTX(gst, 1u << FX1_ISP_CH0_GEOMETRY_STATUS_ERROR_SHIFT, name + " ERROR 1 (active size)");
         const fx1_isp_buffers b = buffer_layout(in_stride, align16(in_w));
         FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_EINVAL, name + " start refused");
         ++helpers["start_refused_geometry"];
         return;
      }
      if (profile_mode) {
         std::vector<fx1_isp_reg_write> writes;
         for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
            const csr::reg_desc *d = reg(l.reg);
            FX1_CHECK_EQ_CTX(d != nullptr ? 1 : 0, 1, name + " register " + l.reg);
            if (d) {
               writes.push_back({d->offset, l.value});
            }
         }
         // A configuration legacy mode measures with drops (an illegal AEC/AWB
         // grid: skipped and counted) is refused at SEAL in profile mode
         // (INVALID, MAS v1.07 Table 9-196): reported, no silent fallback.
         bool drops = false;
         for (const fx1_test::csr_line &l : fx1_test::read_expected(dir / "expected_stats.txt")) {
            drops = drops || ((l.reg == "TUNE_AEC_DROPS" || l.reg == "AWB_DROP_COUNT") && !l.set && l.value != 0);
         }
         std::uint32_t err = 0;
         const int rc = fx1_isp_profile_from_legacy(&dev, 0, writes.data(), writes.size(), 0x5Au, &err);
         if (drops) {
            FX1_CHECK_EQ_CTX(rc == FX1_ISP_EIO && (err & FX1_ISP_TUNE_ERROR_INVALID_MASK) ? 1 : 0, 1,
                             name + " illegal statistics set refused at SEAL (TUNE_ERROR " + fx1_test::hex(err) + ")");
            ++profile_refused;
            return;
         }
         FX1_CHECK_EQ_CTX(rc, FX1_ISP_OK, name + " profile built (TUNE_ERROR " + fx1_test::hex(err) + ")");
         ++profile_vectors;
      }
      const std::uint32_t ow = rd(FX1_ISP_RESIZER_OUT_W_OFFSET);  // live derived geometry (CSR-09)
      const fx1_isp_buffers b = buffer_layout(in_stride, align16(ow));
      // CH1 / CH2 of the vector (MAS v1.07 §6.25.3.8) through the driver's
      // channel-buffer API: it arms them with every queued frame.
      {
         std::ifstream chf(dir / "channels.txt");
         unsigned ch = 0;
         std::uint32_t w = 0, h = 0;
         while (chf >> ch >> w >> h) {
            fx1_isp_channel_buffers cb{};
            cb.y_stride = align16(ch == 2 ? 3 * w : w);
            cb.uv_stride = ch == 1 ? cb.y_stride : 0u;
            for (unsigned k = 0; k < 4; ++k) {
               cb.y[k] = (ch == 1 ? ch1_y_base : ch2_base) + 0x80000ull * k;
               cb.uv[k] = ch1_uv_base + 0x80000ull * k;
            }
            FX1_CHECK_EQ_CTX(fx1_isp_set_channel_buffers(&dev, ch, &cb), FX1_ISP_OK, name + " channel buffers");
            ++helpers["channel_buffers"];
         }
      }
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
      if (profile_mode && !done.empty()) {
         std::uint32_t prof = 0;
         fx1_isp_output_meta(&dev, 0, (frames - 1u) % FX1_ISP_NUM_BUFFERS, nullptr, nullptr, &prof);
         FX1_CHECK_EQ_CTX(prof, 0x5Au, name + " output carries the profile ID");
      }
      expect_profile_id = profile_mode ? 0x5Au : 0u;
      check_stats(name, fx1_test::read_expected(dir / "expected_stats.txt"));
      expect_profile_id = 0;
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
      seq_input.clear();
      to_resubmit.clear();
      resubmitted.clear();
      return b;
   }
   // Queues the frame of input k into the next input buffer.
   // EAGAIN: a recovery restarted the rotation after the slot was filled.
   void queue_input(unsigned k, const fx1_isp_buffers &b, const std::string &ctx) {
      std::uint32_t seq = 0;
      int rc = FX1_ISP_EAGAIN;
      for (int attempt = 0; attempt < 3 && rc == FX1_ISP_EAGAIN; ++attempt) {
         load_input(fx1_isp_next_input(&dev), e_inputs[k], ew, eh, align16(2 * ew), b);
         rc = fx1_isp_queue_frame_ex(&dev, &seq);
      }
      FX1_CHECK_EQ_CTX(rc, FX1_ISP_OK, ctx + " queue");
      seq_input[seq] = k;
   }
   // The report of the last recovery (MAS v1.07 §9.9.8) through the driver.
   fx1_isp_abort_report recovery_report(const std::string &ctx) {
      fx1_isp_abort_report r{};
      FX1_CHECK_EQ_CTX(fx1_isp_recovery_report(&dev, &r), FX1_ISP_OK, ctx + " recovery report");
      return r;
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
         // The error aborts every unretired frame (MAS v1.07 §9.9.8): frame 1
         // and frame 0 if it is still in the pipeline; frames 2 and 3 were
         // never admitted. Every buffer is released and the rotations restart.
         const sc_time limit = sc_time_stamp() + sc_time(5, SC_MS);
         while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) && sc_time_stamp() < limit) {
            wait(50, SC_NS);
         }
         FX1_CHECK_EQ_CTX(fx1_isp_input_ready(&dev), 0, ctx + " no queueing during the recovery");
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 4, b, ctx);
         FX1_CHECK_EQ_CTX(errors_recovered, 1, ctx + " recovered once");
         check_sequence(done, {0, 1, 2, 3}, ctx);
         FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " nothing left in flight");
         FX1_CHECK_EQ_CTX(resubmitted.size() >= 3 && resubmitted.back() == 3 && resubmitted[resubmitted.size() - 3] == 1 ? 1 : 0,
                          1, ctx + " frames 1..3 resubmitted");
         const fx1_isp_abort_report rr = recovery_report(ctx);
         FX1_CHECK_EQ_CTX(rr.sequence, dev.rec_seq, ctx + " report of the settled recovery");
         // The abort set is the admitted, unretired frames: frame 1 is read
         // ahead by the IDMA but not admitted (its SOF waits behind frame 0),
         // so it has no record. Frame 0 has one unless it had retired.
         FX1_CHECK_EQ_CTX(rr.count <= 1 ? 1 : 0, 1, ctx + " only frame 0 can be recorded");
         if (rr.count == 1) {
            FX1_CHECK_EQ_CTX(rr.rec[0].frame, 1u, ctx + " frame 0 = ingress ID 1");
            FX1_CHECK_EQ_CTX(rr.rec[0].failed_outputs, resubmitted.size() == 4 ? 1u : 0u, ctx + " frame 0 outcome");
            FX1_CHECK_EQ_CTX(rr.rec[0].cause, FX1_ISP_ABORT_CAUSE_READ, ctx + " cause");
         } else {
            FX1_CHECK_EQ_CTX(resubmitted.size(), 3u, ctx + " frame 0 retired and kept");
         }
      }
      // ODMA write error in frame 1's output: the recovery aborts frames 1 and
      // 2, frame 0 (DONE) is kept; both are resubmitted and the stream then
      // runs with four frames in flight. `late`: the interrupt handler runs
      // only once frame 0 is DONE (not yet collected) and the error is
      // flagged (review M5-R5): the restarted rotation must wait for output 0.
      for (const bool late : {false, true}) {
         const std::string ctx = late ? "driver odma, late handler" : "driver odma";
         const fx1_isp_buffers b = start_error_case(dir);
         const std::uint64_t row = b.y[1] + std::uint64_t{dev.out_height / 2} * b.y_stride;
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, row, row + dev.out_width, 1});
         for (unsigned k = 0; k < 3; ++k) {
            queue_input(k, b, ctx);
         }
         if (late) {
            const sc_time limit = sc_time_stamp() + sc_time(5, SC_MS);
            while ((!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) ||
                    !(rd(FX1_ISP_ODMA_BUF_DONE_OFFSET) & 1u)) &&
                   sc_time_stamp() < limit) {
               wait(50, SC_NS);
            }
            FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_BUF_DONE_OFFSET) & 2u, 0, ctx + " failed buffer not DONE");
            FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_BUF_FREE_OFFSET) & 2u, 0, ctx + " failed buffer not FREE");
         }
         std::vector<std::vector<std::uint8_t>> done;
         bool alive = service(done, 3, b, ctx);
         FX1_CHECK_EQ_CTX(errors_recovered, 1, ctx + " recovered once");
         FX1_CHECK_EQ_CTX(resubmitted == std::vector<unsigned>({1, 2}) ? 1 : 0, 1, ctx + " frames 1 and 2 resubmitted");
         const fx1_isp_abort_report rr = recovery_report(ctx);
         FX1_CHECK_EQ_CTX(rr.count >= 1 ? rr.rec[0].frame : 0u, 2u, ctx + " oldest aborted = ingress ID 2");
         FX1_CHECK_EQ_CTX(rr.count >= 1 ? rr.rec[0].failed_outputs : 0u, 1u, ctx + " CH0 failed");
         FX1_CHECK_EQ_CTX(rr.count >= 1 ? rr.rec[0].cause : 0u, FX1_ISP_ABORT_CAUSE_WRITE, ctx + " cause");
         std::vector<unsigned> order = {0, 1, 2};
         const unsigned more[] = {3, 0, 1, 2, 3, 0, 1, 2};
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
         FX1_CHECK_EQ_CTX(dev.out_next, dev.in_next, ctx + " rotations consistent");
      }
      {  // An early output fault with a full input ring: no input or output is
         // reusable while the recovery runs; afterwards all four are lost and
         // free, and both rotations restart at 0 (MAS v1.07 §9.9.8).
         // A slow memory keeps the recovery in its drain phase long enough
         // for the checks below (it waits for the responses already issued).
         const std::string ctx = "early ODMA fault, full input ring";
         const fx1_isp_buffers b = start_error_case(dir);
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, b.y[0], b.y[0] + ew, 1});
         for (unsigned k = 0; k < 4; ++k) queue_input(k, b, ctx);
         mem->latency = sc_time(20, SC_US);
         const auto deadline = sc_time_stamp() + sc_time(5, SC_MS);
         while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) &&
                sc_time_stamp() < deadline) wait(10, SC_NS);
         FX1_CHECK(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT);
         FX1_CHECK_EQ_CTX(hal_read(this, FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK,
                          FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK, ctx + " recovery running");
         FX1_CHECK_EQ(fx1_isp_input_ready(&dev), 0);
         FX1_CHECK_EQ(fx1_isp_queue_frame(&dev), FX1_ISP_EBUSY);
         FX1_CHECK_EQ(dev.in_flight, 4u);
         mem->latency = SC_ZERO_TIME;
         fx1_isp_recovery r{};
         FX1_CHECK_EQ(fx1_isp_recover_ex(&dev, FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT, &r), FX1_ISP_OK);
         FX1_CHECK_EQ_CTX(r.aborted, 4u, ctx + " all four lost");
         FX1_CHECK_EQ_CTX(r.odma_failed == 1 && r.odma_seq == 0 && r.odma_output == 0 ? 1 : 0, 1, ctx + " oldest aborted");
         FX1_CHECK_EQ_CTX(r.idma_rearmed, 0, ctx + " no in-place retry");
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_BUF_VALID_OFFSET) | rd(FX1_ISP_ODMA_BUF_FREE_OFFSET), 0u, ctx + " ownership cleared");
         FX1_CHECK_EQ_CTX(dev.in_flight == 0 && dev.in_next == 0 && dev.out_next == 0 ? 1 : 0, 1, ctx + " rotation 0");
         FX1_CHECK(fx1_isp_input_ready(&dev));
         for (unsigned k = 0; k < 4; ++k) {
            std::uint32_t lseq = 99;
            unsigned linput = 99;
            FX1_CHECK_EQ(fx1_isp_next_lost(&dev, &lseq, &linput), FX1_ISP_OK);
            FX1_CHECK_EQ_CTX(lseq == k && linput == k ? 1 : 0, 1, ctx + " lost in queue order " + std::to_string(k));
         }
         FX1_CHECK_EQ(fx1_isp_next_lost(&dev, nullptr, nullptr), FX1_ISP_ENODATA);
         for (unsigned k = 0; k < 4; ++k) queue_input(k, b, ctx);
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 4, b, ctx);
         check_sequence(done, {0, 1, 2, 3}, ctx);
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
      {  // nothing to acquire while the only frame is being measured (MAS v1.07 §9.8)
         const fs::path sdir = root / "stats_flat_channels_64x48";
         reset_and_init();
         apply_profile(fx1_test::read_profile(sdir / "profile.csrw"));
         const fx1_isp_buffers b = buffer_layout(align16(2 * 64), align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
         FX1_CHECK_EQ(fx1_isp_start(&dev, &b), FX1_ISP_OK);
         load_input(fx1_isp_next_input(&dev), read_file(sdir / "input.bin"), 64, 48, align16(2 * 64), b);
         FX1_CHECK_EQ(fx1_isp_queue_frame(&dev), FX1_ISP_OK);
         const sc_time limit = sc_time_stamp() + sc_time(1, SC_MS);  // a dropped measurement never fills
         while (!(rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET) & FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK) && sc_time_stamp() < limit) {
            wait(20, SC_NS);
         }
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET) & FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK,
                          FX1_ISP_TUNE_AEC_STATUS_BUSY_MASK, "zones: a bank is FILLING");
         FX1_CHECK_EQ(fx1_isp_aec_acquire(&dev, nullptr), FX1_ISP_ENODATA);
         fx1_isp_aec_zone z{};
         FX1_CHECK_EQ(fx1_isp_aec_read_zones(&dev, 0, 1, &z), FX1_ISP_ESTATE);  // no lease
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 1, b, "zones");
         FX1_CHECK_EQ(fx1_isp_aec_acquire(&dev, nullptr), FX1_ISP_OK);
         FX1_CHECK_EQ(fx1_isp_aec_read_zones(&dev, 0, 1, &z), FX1_ISP_OK);
         FX1_CHECK_EQ(fx1_isp_aec_release(&dev), FX1_ISP_OK);
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
      FX1_CHECK_EQ(fx1_isp_load_gamma(&dev, lut), FX1_ISP_OK);
      FX1_CHECK_EQ(rd(FX1_ISP_GAMMA_STATUS_OFFSET) & FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK,
                   FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK);
      hal_write(this, FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK);
      FX1_CHECK_EQ(fx1_isp_load_gamma(&dev, lut), FX1_ISP_EBUSY);  // v1.07: load and confirm with EN = 0
      hal_write(this, FX1_ISP_GAMMA_CTRL_OFFSET, 0);
      for (unsigned i : {0u, 1u, 77u, 255u}) {
         hal_write(this, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, i);
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_GAMMA_LUT_RDATA_OFFSET), lut[i], "gamma entry " + std::to_string(i));
      }
      std::vector<std::uint32_t> coef(4 * 2 * 2, 0x40000);
      std::uint32_t err = 0;
      // Profile 0 is active at reset and is never a destination, valid or not
      // (MAS Table 31, owner Q5): the boot sequence loads profile 1 and selects it.
      FX1_CHECK_EQ(fx1_isp_lsc_load(&dev, 0, 2, 2, coef.data(), &err), FX1_ISP_EIO);
      FX1_CHECK_EQ(err & FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK, FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK);
      FX1_CHECK_EQ(fx1_isp_lsc_load(&dev, 1, 2, 2, coef.data(), &err), FX1_ISP_OK);
      FX1_CHECK_EQ(fx1_isp_lsc_select(&dev, 1), FX1_ISP_OK);
      dut->debug_accepted_sof();  // profile 1 becomes active
      FX1_CHECK_EQ(fx1_isp_lsc_load(&dev, 1, 2, 2, coef.data(), &err), FX1_ISP_EIO);
      FX1_CHECK_EQ(err & FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK, FX1_ISP_LSC_ERROR_ACTIVE_LOAD_REJECT_MASK);
      FX1_CHECK_EQ(fx1_isp_lsc_select(&dev, 2), FX1_ISP_EIO);  // not loaded
      FX1_CHECK_EQ(fx1_isp_lsc_load(&dev, 2, 33, 2, coef.data(), &err), FX1_ISP_EINVAL);  // 2..32 nodes (MAS v1.07)
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 3, 4, 0), FX1_ISP_EINVAL);
      // MAS v1.07 §6.6.3.4-6.6.3.5: external even up to 3842x2162, derived
      // active 32x32..3840x2160, validated by software (the DMA does not).
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 3842, 2160, 1), FX1_ISP_OK);     // GRBG: 3840 active
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 3842, 2160, 0), FX1_ISP_EINVAL);  // RGGB: 3842 active
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 3842, 2162, 3), FX1_ISP_OK);
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 3844, 2162, 3), FX1_ISP_EINVAL);  // beyond the external capacity
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 34, 34, 3), FX1_ISP_OK);          // 32x32 active
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 32, 32, 3), FX1_ISP_EINVAL);      // 30x30 active
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 32, 30, 0), FX1_ISP_EINVAL);
      FX1_CHECK_EQ(fx1_isp_set_input_format(&dev, 2), FX1_ISP_EINVAL);
      FX1_CHECK_EQ(fx1_isp_set_input_format(&dev, FX1_ISP_INPUT_RAW10), FX1_ISP_OK);
      FX1_CHECK_EQ(dut->debug_peek(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET), FX1_ISP_INPUT_RAW10);
      FX1_CHECK_EQ(fx1_isp_set_input_format(&dev, FX1_ISP_INPUT_RAW12), FX1_ISP_OK);
      {
         const std::uint16_t upper_raw[4] = {0x0FFF, 0x03FF, 0x1000, 0x0400};
         FX1_CHECK_EQ(fx1_isp_raw_upper_bits(upper_raw, 4, FX1_ISP_INPUT_RAW12), 1u);  // 0x1000
         FX1_CHECK_EQ(fx1_isp_raw_upper_bits(upper_raw, 4, FX1_ISP_INPUT_RAW10), 3u);  // 0x0FFF, 0x1000, 0x0400
         FX1_CHECK_EQ(fx1_isp_raw_upper_bits(upper_raw + 1, 1, FX1_ISP_INPUT_RAW10), 0u);
         FX1_CHECK_EQ(fx1_isp_raw_upper_bits(nullptr, 4, FX1_ISP_INPUT_RAW12), 0u);
      }
      FX1_CHECK_EQ(fx1_isp_set_geometry(&dev, 64, 32, 0), FX1_ISP_OK);
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
      fx1_isp_events ev{1u, 1u, 1u, 1u, 1u, 1u};
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
      FX1_CHECK_EQ_CTX(ev.common | ev.dma | ev.dma_err | ev.frames_done | ev.frames_lost | ev.output, 0,
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
      FX1_CHECK_EQ_CTX(fx1_isp_set_input_format(&dev, FX1_ISP_INPUT_RAW10), FX1_ISP_EBUSY,
                       ctx + " input format with frames in flight");

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
      return a.sum == b.sum && a.count == b.count && a.oe == b.oe && a.ue == b.ue && a.min == b.min && a.max == b.max;
   }

   // AEC results around an aborted frame and resets (MAS v1.07 Table 9-198,
   // Table 9-187): the aborted measurement publishes nothing; READY results
   // survive the DMA recovery; a soft reset clears them. The aborted frame
   // still consumes its frame ID.
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
      // Acquire the oldest result, read its first/last zones and histogram, release.
      auto take = [&](fx1_isp_aec_zone z[2], std::uint64_t h[64], std::uint32_t &id) {
         fx1_isp_stat_lease l{};
         const int rc = fx1_isp_aec_acquire(&dev, &l);
         if (rc != FX1_ISP_OK) {
            return rc;
         }
         id = l.frame_id;
         FX1_CHECK_EQ_CTX(fx1_isp_aec_read_zones(&dev, 0, 1, &z[0]), FX1_ISP_OK, ctx);
         FX1_CHECK_EQ_CTX(fx1_isp_aec_read_zones(&dev, l.nx * l.ny - 1, 1, &z[1]), FX1_ISP_OK, ctx);
         FX1_CHECK_EQ_CTX(fx1_isp_aec_read_hist(&dev, h, nullptr), FX1_ISP_OK, ctx);
         FX1_CHECK_EQ_CTX(fx1_isp_aec_release(&dev), FX1_ISP_OK, ctx);
         return FX1_ISP_OK;
      };
      fx1_isp_aec_zone z0[2]{}, z[2]{};
      std::uint64_t h0[64], h[64];
      std::uint32_t id = 0;
      submit();
      service(done, 1, b, ctx);
      FX1_CHECK_EQ_CTX(take(z0, h0, id), FX1_ISP_OK, ctx + " first frame");
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
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET), 0, ctx + " nothing READY, nothing filling after the abort");
      fx1_isp_events ev{};
      fx1_isp_irq(&dev, &ev);
      FX1_CHECK_EQ_CTX(take(z, h, id), FX1_ISP_ENODATA, ctx + " the aborted frame published nothing");
      fx1_isp_recovery r{};
      FX1_CHECK_EQ_CTX(fx1_isp_recover_ex(&dev, ev.dma_err, &r), FX1_ISP_OK, ctx + " recover");
      FX1_CHECK_EQ_CTX(r.aborted, 1u, ctx + " the frame is lost");  // MAS v1.07 §9.9.8: no in-place retry
      FX1_CHECK_EQ_CTX(fx1_isp_next_lost(&dev, nullptr, nullptr), FX1_ISP_OK, ctx + " lost reported");
      submit();  // the caller resubmits it
      service(done, 2, b, ctx);
      FX1_CHECK_EQ_CTX(take(z, h, id), FX1_ISP_OK, ctx + " retried frame");
      FX1_CHECK_EQ_CTX(id, 3, ctx + " the aborted frame consumed FRAME_ID 2");
      FX1_CHECK_EQ_CTX(same_zone(z[0], z0[0]) && same_zone(z[1], z0[1]) && std::memcmp(h, h0, sizeof h) == 0 ? 1 : 0, 1,
                       ctx + " same input, same statistics");

      // A READY result survives the DMA recovery of a later aborted frame.
      submit();
      done.clear();
      service(done, 1, b, ctx);  // FRAME_ID 4, READY, not read yet
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
      FX1_CHECK_EQ_CTX(fx1_isp_recover(&dev, ev.dma_err), FX1_ISP_OK, ctx + " recover");
      FX1_CHECK_EQ_CTX(fx1_isp_next_lost(&dev, nullptr, nullptr), FX1_ISP_OK, ctx + " lost reported (2)");
      submit();
      service(done, 2, b, ctx);
      FX1_CHECK_EQ_CTX(take(z, h, id), FX1_ISP_OK, ctx + " kept result");
      FX1_CHECK_EQ_CTX(id, 4, ctx + " the result published before the abort");
      FX1_CHECK_EQ_CTX(take(z, h, id), FX1_ISP_OK, ctx + " retried frame");
      FX1_CHECK_EQ_CTX(id, 6, ctx + " FRAME_ID after the retry");

      // Soft reset: results and ownership cleared; the frame counter kept.
      submit();
      done.clear();
      service(done, 1, b, ctx);
      fx1_isp_soft_reset(&dev);
      FX1_CHECK_EQ_CTX(take(z, h, id), FX1_ISP_ENODATA, ctx + " after soft reset");
      submit();
      done.clear();
      service(done, 1, b, ctx);
      FX1_CHECK_EQ_CTX(take(z, h, id), FX1_ISP_OK, ctx + " after the next frame");
      FX1_CHECK_EQ_CTX(id, 8, ctx + " FRAME_ID after soft reset");
      FX1_CHECK_EQ_CTX(same_zone(z[0], z0[0]) ? 1 : 0, 1, ctx + " statistics after soft reset");
      mem->faults.clear();
   }

   // The compatibility reader sweeps every READY result and keeps the newest
   // (fx1_isp_read_aec_global contract): with both banks READY it reports the
   // second frame, and with none READY the result it read last.
   void test_compat_aec_newest() {
      const fs::path sdir = root / "stats_flat_channels_64x48";
      const std::string ctx = "compat aec newest";
      const std::uint32_t sw = 64, sh = 48, stride = align16(2 * sw);
      reset_and_init();
      apply_profile(fx1_test::read_profile(sdir / "profile.csrw"));
      const fx1_isp_buffers b = buffer_layout(stride, align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      const std::vector<std::uint8_t> raw = read_file(sdir / "input.bin");
      std::vector<std::vector<std::uint8_t>> done;
      for (std::size_t k = 0; k < 2; ++k) {
         load_input(fx1_isp_next_input(&dev), raw, sw, sh, stride, b);
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
         service(done, k + 1, b, ctx);
      }
      const auto ready = [&] {
         return (rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET) & FX1_ISP_TUNE_AEC_STATUS_READY_COUNT_MASK) >>
                FX1_ISP_TUNE_AEC_STATUS_READY_COUNT_SHIFT;
      };
      FX1_CHECK_EQ_CTX(ready(), 2u, ctx + " both banks READY");
      fx1_isp_aec_global g{};
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_global(&dev, &g), FX1_ISP_OK, ctx + " read");
      FX1_CHECK_EQ_CTX(g.frame_id, 2u, ctx + " the newest result");
      FX1_CHECK_EQ_CTX(ready(), 0u, ctx + " both results consumed");
      fx1_isp_aec_global again{};
      FX1_CHECK_EQ_CTX(fx1_isp_read_aec_global(&dev, &again), FX1_ISP_OK, ctx + " read with none READY");
      FX1_CHECK_EQ_CTX(again.frame_id, 2u, ctx + " the result read last");
   }

   // AWB ownership through the driver (MAS §6.22.4; plan U2 item 5): the
   // explicit API, the compatibility reader, loss reporting and a lease ended
   // by a reset the driver did not issue.
   void test_awb_driver() {
      const fs::path sdir = root / "stats_flat_channels_64x48";  // AWB 4 x 3, context 0x102
      const std::string ctx = "awb driver";
      const std::uint32_t sw = 64, sh = 48, stride = align16(2 * sw);
      reset_and_init();
      apply_profile(fx1_test::read_profile(sdir / "profile.csrw"));
      const fx1_isp_buffers b = buffer_layout(stride, align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ(fx1_isp_start(&dev, &b), FX1_ISP_OK);
      const std::vector<std::uint8_t> raw = read_file(sdir / "input.bin");
      std::vector<std::vector<std::uint8_t>> done;
      auto frames = [&](unsigned n) {
         for (unsigned k = 0; k < n; ++k) {
            load_input(fx1_isp_next_input(&dev), raw, sw, sh, stride, b);
            FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
         }
         done.clear();
         service(done, n, b, ctx);
         // Statistics are published when the pipeline ends the frame, which
         // in this model can follow the ODMA completion under output
         // backpressure (U2-A7). Software waits for the publication, as MAS
         // §6.22.4.1 asks (READY_COUNT / stats_ready), here for an idle pipeline.
         const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);
         while ((rd(FX1_ISP_DEMOSAIC_STATUS_OFFSET) & FX1_ISP_DEMOSAIC_STATUS_BUSY_MASK) && sc_time_stamp() < end) {
            wait(dut->params().core_period * 4.0);
         }
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_DEMOSAIC_STATUS_OFFSET) & FX1_ISP_DEMOSAIC_STATUS_BUSY_MASK, 0, ctx + " pipeline idle");
      };
      fx1_isp_awb_lease lease{};
      fx1_isp_awb_global g{}, g1{};
      fx1_isp_awb_zone zones[12]{};
      fx1_isp_awb_status rep{};
      FX1_CHECK_EQ_CTX(fx1_isp_read_awb_global(&dev, &g), FX1_ISP_ENODATA, ctx + " nothing published yet");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_acquire(&dev, &lease), FX1_ISP_ENODATA, ctx + " nothing to acquire");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_read_global(&dev, &g), FX1_ISP_ESTATE, ctx + " read without a lease");

      // Explicit API: one result, held while the next frame is measured. The
      // ACQUIRE is issued at the start of a core clock, so with fast CSR
      // accesses the next one falls on the same clock unless the driver waits.
      frames(1);
      const auto now = static_cast<std::uint64_t>(sc_time_stamp() / dut->params().core_period);
      wait(dut->params().core_period * static_cast<double>(now + 1) - sc_time_stamp());
      FX1_CHECK_EQ_CTX(fx1_isp_awb_acquire(&dev, &lease), FX1_ISP_OK, ctx + " acquire");
      FX1_CHECK_EQ_CTX(dev.awb_leased, 1, ctx + " lease recorded");
      FX1_CHECK_EQ_CTX(lease.frame_id, 1, ctx + " FRAME_ID");
      FX1_CHECK_EQ_CTX(lease.context_id, 0x102, ctx + " CONTEXT_ID");
      FX1_CHECK_EQ_CTX(lease.nx * 10 + lease.ny, 43, ctx + " captured grid");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_read_global(&dev, &g1), FX1_ISP_OK, ctx + " global");
      FX1_CHECK_EQ_CTX(g1.count > 0 ? 1 : 0, 1, ctx + " qualified pixels");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_read_zones(&dev, 0, 12, zones), FX1_ISP_OK, ctx + " zones");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_read_zones(&dev, 11, 2, zones), FX1_ISP_EINVAL, ctx + " beyond the grid");
      std::uint64_t zsum = 0;
      for (const auto &z : zones) {
         zsum += z.count;
      }
      FX1_CHECK_EQ_CTX(zsum, g1.count, ctx + " zones add up to the global count");
      FX1_CHECK_EQ_CTX(fx1_isp_read_awb_global(&dev, &g), FX1_ISP_EBUSY, ctx + " compatibility reader while leased");
      frames(1);  // frame 2 goes to the other bank; the lease does not change
      FX1_CHECK_EQ_CTX(fx1_isp_awb_read_global(&dev, &g), FX1_ISP_OK, ctx + " lease after a new frame");
      FX1_CHECK_EQ_CTX(g.count, g1.count, ctx + " leased result stable");
      FX1_CHECK_EQ_CTX(g.frame_id, 1, ctx + " leased result identity");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_AWB_FRAME_ID_OFFSET), 1, ctx + " leased FRAME_ID");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_release(&dev), FX1_ISP_OK, ctx + " release");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_release(&dev), FX1_ISP_ESTATE, ctx + " second release");

      // Compatibility reader: drains the ready results, returns the newest,
      // and keeps returning it while nothing new is ready.
      frames(1);  // frame 3; frame 2 still ready
      FX1_CHECK_EQ_CTX(fx1_isp_read_awb_global(&dev, &g), FX1_ISP_OK, ctx + " compatibility reader");
      FX1_CHECK_EQ_CTX(g.frame_id, 3, ctx + " newest result");
      FX1_CHECK_EQ_CTX(g.count, g1.count, ctx + " same input, same result");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_report(&dev, &rep, 0), FX1_ISP_OK, ctx + " report");
      FX1_CHECK_EQ_CTX(rep.ready_count, 0, ctx + " drained");
      FX1_CHECK_EQ_CTX(fx1_isp_read_awb_global(&dev, &g), FX1_ISP_OK, ctx + " again");
      FX1_CHECK_EQ_CTX(g.frame_id, 3, ctx + " cached");

      // Unread results: the third enabled frame finds no free bank.
      frames(3);  // frames 4, 5 ready; 6 skipped
      FX1_CHECK_EQ_CTX(fx1_isp_awb_report(&dev, &rep, 1), FX1_ISP_OK, ctx + " report and clear");
      FX1_CHECK_EQ_CTX(rep.ready_count, 2, ctx + " two ready");
      FX1_CHECK_EQ_CTX(rep.drop_count, 1, ctx + " one skipped");
      FX1_CHECK_EQ_CTX(rep.overrun, 1, ctx + " overrun");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_report(&dev, &rep, 0), FX1_ISP_OK, ctx + " report");
      FX1_CHECK_EQ_CTX(rep.drop_count, 0, ctx + " drops cleared");
      FX1_CHECK_EQ_CTX(rep.overrun, 0, ctx + " overrun cleared");
      FX1_CHECK_EQ_CTX(rep.ready_count, 2, ctx + " data kept");
      FX1_CHECK_EQ_CTX(fx1_isp_read_awb_global(&dev, &g), FX1_ISP_OK, ctx + " after the loss");
      FX1_CHECK_EQ_CTX(g.frame_id, 5, ctx + " newest published (6 was skipped)");

      // A COMMON soft reset the driver did not issue ends the lease.
      frames(1);  // frame 7
      FX1_CHECK_EQ_CTX(fx1_isp_awb_acquire(&dev, &lease), FX1_ISP_OK, ctx + " acquire before reset");
      hal_write(this, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
      wait(dut->params().core_period * 40.0);
      FX1_CHECK_EQ_CTX(fx1_isp_awb_read_global(&dev, &g), FX1_ISP_ENODATA, ctx + " lease gone");
      FX1_CHECK_EQ_CTX(fx1_isp_awb_release(&dev), FX1_ISP_ESTATE, ctx + " lease dropped by the driver");
      // The cache describes results the reset cleared (review U2-R1).
      FX1_CHECK_EQ_CTX(fx1_isp_read_awb_global(&dev, &g), FX1_ISP_ENODATA, ctx + " cache dropped with the lease");
      fx1_isp_soft_reset(&dev);
      FX1_CHECK_EQ_CTX(fx1_isp_read_awb_global(&dev, &g), FX1_ISP_ENODATA, ctx + " cache dropped by soft reset");
      fx1_isp_stop(&dev);
   }

   // EE resident profiles through the driver (MAS §6.18.4.1, §9.2; U3): the
   // vector ee_features_96x64's EE content is loaded into profile 2 with
   // fx1_isp_ee_load_profile, other content into profile 0, profile 2 is
   // selected, and the frame must equal the vector's expected output. Then
   // the refusal of a profile a frame in flight may use.
   void test_ee_driver() {
      const fs::path dir = root / "ee_features_96x64";
      const std::string ctx = "ee driver";
      std::ifstream meta(dir / "meta.txt");
      std::uint32_t in_w = 0, in_h = 0, out_w = 0, out_h = 0, frames = 1;
      meta >> in_w >> in_h >> out_w >> out_h >> frames;
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      // Split the profile: the EE content into a driver profile, the rest as is.
      fx1_isp_ee_profile ep{};
      std::vector<fx1_test::csr_line> others;
      std::uint16_t *tables[4] = {ep.luma, ep.activity, ep.contrast_pos, ep.contrast_neg};
      unsigned bank = 0, addr = 0;
      bool enable = false;
      for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
         const std::uint32_t off = reg(l.reg) ? reg(l.reg)->offset : 0;
         if (off >= FX1_ISP_EE_ALPHA_OFFSET && off <= FX1_ISP_EE_RADIAL_GAIN_23_OFFSET) {
            uint32_t *sc[10] = {&ep.alpha, &ep.beta, &ep.clamp, &ep.feature_en, &ep.radial_center, &ep.r2_th[0],
                                &ep.r2_th[1], &ep.r2_th[2], &ep.radial_gain_01, &ep.radial_gain_23};
            *sc[(off - FX1_ISP_EE_ALPHA_OFFSET) / 4] = l.value;
         } else if (l.reg == "EE_LUT_CTRL") {
            bank = (l.value >> FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT) & 3u;
         } else if (l.reg == "EE_LUT_ADDR") {
            addr = l.value;
         } else if (l.reg == "EE_LUT_WDATA") {
            tables[bank][addr++ & (bank < 2 ? 63u : 31u)] = static_cast<std::uint16_t>(l.value);
         } else if (l.reg == "EE_CTRL") {
            enable = (l.value & FX1_ISP_EE_CTRL_EN_MASK) != 0;
         } else {
            others.push_back(l);
         }
      }
      FX1_CHECK_EQ_CTX(enable ? 1 : 0, 1, ctx + " vector enables EE");
      fx1_isp_ee_profile other = ep;
      other.alpha = 0x8000;
      other.clamp = 0xFFFF;
      for (auto &v : other.luma) {
         v = 0x4000;
      }
      reset_and_init();
      apply_profile(others);
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 3, &ep), FX1_ISP_EINVAL, ctx + " profile 3");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_select(&dev, 3, 1), FX1_ISP_EINVAL, ctx + " select 3");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 0, &other), FX1_ISP_OK, ctx + " load 0");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 2, &ep), FX1_ISP_OK, ctx + " load 2");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_select(&dev, 2, 1), FX1_ISP_OK, ctx + " select 2");
      const std::uint32_t in_stride = align16(2 * in_w);
      const fx1_isp_buffers b = buffer_layout(in_stride, align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      std::vector<std::vector<std::uint8_t>> done;
      load_input(fx1_isp_next_input(&dev), input, in_w, in_h, in_stride, b);
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
      service(done, 1, b, ctx);
      std::vector<std::uint8_t> want = read_file(dir / "expected_y.bin");
      const std::vector<std::uint8_t> uv = read_file(dir / "expected_uv.bin");
      want.insert(want.end(), uv.begin(), uv.end());
      FX1_CHECK_EQ_CTX(!done.empty() && done.back() == want ? 1 : 0, 1, ctx + " profile 2 = the vector's EE");
      // While a frame is in EE, the selected profile and the one selected
      // before it are refused; another one may be loaded.
      load_input(fx1_isp_next_input(&dev), input, in_w, in_h, in_stride, b);
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
      const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
      while (!(rd(FX1_ISP_EE_STATUS_OFFSET) & FX1_ISP_EE_STATUS_BUSY_MASK) && sc_time_stamp() < end) {
         wait(100, SC_NS);
      }
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_EE_STATUS_OFFSET) & FX1_ISP_EE_STATUS_BUSY_MASK, FX1_ISP_EE_STATUS_BUSY_MASK,
                       ctx + " EE busy");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 2, &ep), FX1_ISP_EBUSY, ctx + " selected profile while busy");
      // Written while EE runs: legacy mode cannot confirm it now (MAS v1.07 §9.9.1).
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 1, &other), FX1_ISP_EAGAIN, ctx + " unused profile while busy");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_select(&dev, 0, 1), FX1_ISP_OK, ctx + " select 0 while busy");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 2, &ep), FX1_ISP_EBUSY, ctx + " previous profile while busy");
      // A second switch in the same busy period: profile 2 is still the
      // in-flight frame's (review U3-R1); every profile selected is kept.
      FX1_CHECK_EQ_CTX(fx1_isp_ee_select(&dev, 1, 1), FX1_ISP_OK, ctx + " select 1 while busy");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_EE_STATUS_OFFSET) & FX1_ISP_EE_STATUS_BUSY_MASK, FX1_ISP_EE_STATUS_BUSY_MASK,
                       ctx + " still busy");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 2, &other), FX1_ISP_EBUSY, ctx + " in-flight profile after two switches");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 0, &other), FX1_ISP_EBUSY, ctx + " intermediate selection");
      done.clear();
      service(done, 1, b, ctx);
      FX1_CHECK_EQ_CTX(!done.empty() && done.back() == want ? 1 : 0, 1, ctx + " frame in flight kept profile 2");
      const sc_time end2 = sc_time_stamp() + sc_time(20, SC_MS);
      while ((rd(FX1_ISP_EE_STATUS_OFFSET) & FX1_ISP_EE_STATUS_BUSY_MASK) && sc_time_stamp() < end2) {
         wait(100, SC_NS);
      }
      FX1_CHECK_EQ_CTX(fx1_isp_ee_load_profile(&dev, 2, &ep), FX1_ISP_EAGAIN, ctx + " previous profile once EE was idle");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_confirm(&dev, 2), FX1_ISP_EAGAIN, ctx + " confirm while EE is enabled");
      FX1_CHECK_EQ_CTX(fx1_isp_ee_select(&dev, 2, 0), FX1_ISP_OK, ctx + " disable EE");
      // Still streaming (IDMA_EN and ISP_EN): not idle (MAS v1.07 §9.9.3).
      FX1_CHECK_EQ_CTX(fx1_isp_ee_confirm(&dev, 2), FX1_ISP_EAGAIN, ctx + " confirm while streaming");
      fx1_isp_stop(&dev);
      FX1_CHECK_EQ_CTX(fx1_isp_ee_confirm(&dev, 2), FX1_ISP_OK, ctx + " confirm once stopped");
   }

   // GTM software table through the driver while GTM is enabled: the memory
   // takes writes only while EN = 0 (MAS p146), so the driver clears EN for
   // the load and restores it (U3).
   void test_gtm_driver() {
      const std::string ctx = "gtm driver";
      std::uint16_t lut[65];
      for (unsigned i = 0; i < 65; ++i) {
         lut[i] = static_cast<std::uint16_t>(0x100 + 37 * i);
      }
      auto lut_mismatches = [&] {
         unsigned bad = 0;
         for (unsigned i = 0; i < 65; ++i) {
            hal_write(this, FX1_ISP_GTM_LUT_ADDR_OFFSET, i);
            bad += rd(FX1_ISP_GTM_LUT_RDATA_OFFSET) != lut[i];
         }
         return bad;
      };
      // Cold: no frame since init, nothing can be pending.
      reset_and_init();
      hal_write(this, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_EN_MASK);
      FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_OK, ctx + " cold load");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_GTM_CTRL_OFFSET), FX1_ISP_GTM_CTRL_EN_MASK | FX1_ISP_GTM_CTRL_MANUAL_MASK,
                       ctx + " EN restored, MANUAL set");
      FX1_CHECK_EQ_CTX(lut_mismatches(), 0u, ctx + " cold read-back");
      // Step 4 catches a load the memory did not take: EN set again behind
      // the driver after step 1, so the writes are discarded.
      for (auto &v : lut) {
         v = static_cast<std::uint16_t>(v ^ 0x5A5A);
      }
      FX1_CHECK_EQ_CTX(fx1_isp_gtm_manual_begin(&dev), FX1_ISP_OK, ctx + " begin");
      hal_write(this, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_EN_MASK | FX1_ISP_GTM_CTRL_MANUAL_MASK);
      FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_EIO, ctx + " discarded writes detected");
      for (auto &v : lut) {
         v = static_cast<std::uint16_t>(v ^ 0x5A5A);
      }
      // Auto mode running: the frame's end built a curve that waits for the
      // next SOF (review U3-R2, MAS §9.2 step 2).
      const fs::path dir = root / "gtm_auto_3frames_96x64";
      std::ifstream meta(dir / "meta.txt");
      std::uint32_t in_w = 0, in_h = 0, out_w = 0, out_h = 0, frames = 1;
      meta >> in_w >> in_h >> out_w >> out_h >> frames;
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      reset_and_init();
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      const std::uint32_t in_stride = align16(2 * in_w);
      const fx1_isp_buffers b = buffer_layout(in_stride, align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      std::vector<std::vector<std::uint8_t>> done;
      auto one_frame = [&] {
         load_input(fx1_isp_next_input(&dev), input, in_w, in_h, in_stride, b);
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
         done.clear();
         service(done, 1, b, ctx);
         const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);  // the frame's end (U2-A7)
         while ((rd(FX1_ISP_GTM_STATUS_OFFSET) & FX1_ISP_GTM_STATUS_BUSY_MASK) && sc_time_stamp() < end) {
            wait(100, SC_NS);
         }
      };
      one_frame();  // automatic curve built at its end, pending
      FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_EAGAIN, ctx + " refused: a curve may be pending");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_GTM_CTRL_OFFSET), FX1_ISP_GTM_CTRL_MANUAL_MASK, ctx + " step 1: EN 0, MANUAL 1");
      FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_EAGAIN, ctx + " still refused");
      one_frame();  // step 2: one frame started after step 1 completes
      FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_OK, ctx + " load after one frame");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_GTM_CTRL_OFFSET), FX1_ISP_GTM_CTRL_EN_MASK | FX1_ISP_GTM_CTRL_MANUAL_MASK,
                       ctx + " EN restored");
      FX1_CHECK_EQ_CTX(lut_mismatches(), 0u, ctx + " read-back");
      one_frame();
      one_frame();
      FX1_CHECK_EQ_CTX(lut_mismatches(), 0u, ctx + " the software curve survives the next SOFs");
      fx1_isp_stop(&dev);

      // The first automatic frame loses its output (ODMA write error, review
      // U3-R3). The recovery stops the IDMA inside the frame (MAS v1.07
      // §9.9.8): the pipeline never took it completely and its end built no
      // curve, so the state is cold and the load proceeds at once.
      const fx1_isp_hal hal{&bench::hal_read, &bench::hal_write, &bench::hal_delay, this};
      reset_and_init();
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start (lost frame)");
      load_input(fx1_isp_next_input(&dev), input, in_w, in_h, in_stride, b);
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, b.y[0], b.y[0] + 64u, 1});
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
      const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
      while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_AXI_WR_BIT) && sc_time_stamp() < end) {
         wait(100, SC_NS);
      }
      wait(sc_time(1, SC_MS));  // the frame's end
      fx1_isp_events ev{};
      fx1_isp_irq(&dev, &ev);
      fx1_isp_recovery rec{};
      FX1_CHECK_EQ_CTX(fx1_isp_recover_ex(&dev, ev.dma_err, &rec), FX1_ISP_OK, ctx + " recover");
      FX1_CHECK_EQ_CTX(fx1_isp_next_lost(&dev, nullptr, nullptr), FX1_ISP_OK, ctx + " frame lost");
      mem->faults.clear();
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0u, ctx + " no output succeeded");
      FX1_CHECK_EQ_CTX(dev.in_flight, 0u, ctx + " nothing queued");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0u, ctx + " the aborted frame was not taken");
      FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_OK, ctx + " lost frame: cold");
      one_frame();
      one_frame();
      FX1_CHECK_EQ_CTX(lut_mismatches(), 0u, ctx + " curve kept after a lost first frame");
      fx1_isp_stop(&dev);

      // Re-initialised without i_rst_n after an automatic frame: the pending
      // curve survives the soft reset of fx1_isp_init, so this is not cold.
      reset_and_init();
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start (warm init)");
      one_frame();
      FX1_CHECK_EQ_CTX(fx1_isp_init(&dev, &hal), FX1_ISP_OK, ctx + " warm init");
      FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_EAGAIN, ctx + " warm init: not cold");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " restart");
      one_frame();
      FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_OK, ctx + " load after the handover frame");
      one_frame();
      FX1_CHECK_EQ_CTX(lut_mismatches(), 0u, ctx + " curve kept after a warm init");
      fx1_isp_stop(&dev);

      // The first frame after i_rst_n runs on a manual curve: it reads the read
      // bank live, so a load while it is in the pipeline would change it, even
      // though IDMA_FRAME_COUNT is still 0 (review U3-R3 C1). A queued frame
      // is not cold.
      std::uint16_t lut_b[65];
      for (unsigned i = 0; i < 65; ++i) {
         lut_b[i] = static_cast<std::uint16_t>(lut[64 - i] ^ 0x0F0F);
      }
      const fs::path mdir = root / "mandatory_rggb_64x32";
      const std::vector<std::uint8_t> mraw = read_file(mdir / "input.bin");
      std::vector<std::vector<std::uint8_t>> baseline, guarded;
      auto manual_first_frame = [&](bool load_during) {
         reset_and_init();
         apply_profile(fx1_test::read_profile(mdir / "profile.csrw"));
         hal_write(this, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_EN_MASK);
         FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_OK, ctx + " cold manual load");
         const fx1_isp_buffers mb = buffer_layout(128, align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
         FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &mb), FX1_ISP_OK, ctx + " start (manual)");
         load_input(fx1_isp_next_input(&dev), mraw, 64, 32, 128, mb);
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue (manual)");
         std::vector<std::vector<std::uint8_t>> &out = load_during ? guarded : baseline;
         if (load_during) {
            const sc_time t_end = sc_time_stamp() + sc_time(1, SC_MS);
            while (!(rd(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) && sc_time_stamp() < t_end) {
               wait(10, SC_NS);
            }
            FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), 0u, ctx + " first frame not taken yet");
            FX1_CHECK_EQ_CTX(dev.in_flight, 1u, ctx + " one frame queued");
            FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut_b), FX1_ISP_EAGAIN, ctx + " frame in flight: not cold");
         }
         service(out, 1, mb, ctx);
         fx1_isp_stop(&dev);
      };
      manual_first_frame(false);
      manual_first_frame(true);
      FX1_CHECK_EQ_CTX(!baseline.empty() && !guarded.empty() && baseline.back() == guarded.back() ? 1 : 0, 1,
                       ctx + " the first manual frame keeps its curve");
   }

   // The driver waits for the Resizer's published set (MAS §6.20.4.3, Table
   // 124): 35 clocks after the configuration, and after a soft reset the
   // 15-clock pulse plus a derivation. In this TLM no scalable frame can
   // reach its SOF that early (its first line is >= 1280 bytes), so the waits
   // are checked as the driver's timing contract.
   void test_resizer_waits() {
      const std::string ctx = "resizer waits";
      reset_and_init();
      std::uint64_t before = delay_requested;
      fx1_isp_soft_reset(&dev);
      FX1_CHECK_EQ_CTX(delay_requested - before >= 15u + 36u ? 1 : 0, 1, ctx + " soft reset");
      const fx1_isp_buffers b = buffer_layout(align16(2 * 1920), align16(1920));
      before = delay_requested;
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      // v1.07: the start polls GEOMETRY_STATUS until BUSY clears (MAS §8.3)
      // instead of a fixed two-derivation wait; the set it reads is settled.
      FX1_CHECK_EQ_CTX(delay_requested - before >= 1u ? 1 : 0, 1, ctx + " start: waited for the geometry");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET), FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK,
                       ctx + " start: settled and valid");
      fx1_isp_stop(&dev);
      // The key changed while the dividers work on a previous one (review
      // U4-R1): the start must see the last key's set, for a stride sized for
      // that mode and for one sized for the largest.
      for (const std::uint32_t stride : {640u, 3840u}) {
         const std::string sc = ctx + " changed key, stride " + std::to_string(stride);
         reset_and_init();
         FX1_CHECK_EQ_CTX(fx1_isp_set_geometry(&dev, 3840, 2160, 0), FX1_ISP_OK, sc + " geometry");
         wait(dut->params().core_period * 100.0);
         hal_write(this, FX1_ISP_RESIZER_CTRL_OFFSET,
                   1u | (FX1_ISP_RESIZER_CTRL_SCALE_RES_1280X720 << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT));
         wait(dut->params().core_period * 2.0);  // the HD derivation has started
         hal_write(this, FX1_ISP_RESIZER_CTRL_OFFSET,
                   1u | (FX1_ISP_RESIZER_CTRL_SCALE_RES_640X480 << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT));
         const fx1_isp_buffers vb = buffer_layout(align16(2 * 3840), stride);
         FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &vb), FX1_ISP_OK, sc + " start");
         FX1_CHECK_EQ_CTX(dev.out_width, 640u, sc + " width");
         FX1_CHECK_EQ_CTX(dev.out_height, 480u, sc + " height");
         fx1_isp_stop(&dev);
      }
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
      // The aborted first frame publishes nothing (MAS v1.07 Table 9-198).
      FX1_CHECK_EQ(rd(FX1_ISP_TUNE_AEC_STATUS_OFFSET), 0u);
      FX1_CHECK_EQ(fx1_isp_aec_acquire(&dev, nullptr), FX1_ISP_ENODATA);
      fx1_isp_events ev{};
      fx1_isp_irq(&dev, &ev);
      FX1_CHECK_EQ(fx1_isp_aec_acquire(&dev, nullptr), FX1_ISP_ENODATA);
      fx1_isp_aec_global g{};
      FX1_CHECK_EQ(fx1_isp_read_aec_global(&dev, &g), FX1_ISP_ENODATA);
   }

   // Item 1: coordinated faults. Fixed seed; each run injects an IDMA read
   // error and/or an ODMA write error (each possibly hitting twice, i.e.
   // again after the resubmission) at a random buffer and line, and runs the
   // interrupt handler 0 us .. 2 ms late, so several completions and both
   // errors can be pending at once. Every error is an automatic recovery
   // (MAS v1.07 §9.9.8); the caller resubmits every frame the driver reports
   // lost. Checks: every logical frame completes exactly once with the right
   // content, completions in queue order, a stale second recover is a no-op,
   // each recovery report is stable and consistent, no deadlock, clean state.
   struct campaign_stats {
      unsigned runs = 0, both_at_once = 0, repeated = 0, multi_done = 0, lost = 0, recoveries = 0, stale = 0,
               odma_failed = 0, kept = 0, requeued = 0;
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
      std::set<std::uint32_t> seen_seq;
      std::vector<unsigned> done_count(frames, 0);
      std::int64_t last_seq = -1;
      unsigned completed = 0;
      const sc_time end = sc_time_stamp() + sc_time(500, SC_MS);
      while (completed < frames) {
         while (!pending.empty() && fx1_isp_input_ready(&dev)) {
            const unsigned lf = pending.front();
            load_input(fx1_isp_next_input(&dev), e_inputs[lf % 4], ew, eh, stride, b);
            std::uint32_t seq = 0;
            const int rc = fx1_isp_queue_frame_ex(&dev, &seq);
            if (rc == FX1_ISP_EAGAIN) {  // a recovery restarted the rotation after the slot was filled
               ++cs.requeued;
               continue;
            }
            FX1_CHECK_EQ_CTX(rc, FX1_ISP_OK, ctx + " queue");
            pending.erase(pending.begin());
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
                                       " drv in/out " + std::to_string(dev.in_next) + "/" +
                                       std::to_string(dev.out_next) +
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
            // flagged again in hardware; otherwise it is a no-op that keeps
            // the driver state.
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
               FX1_CHECK_EQ_CTX(r.idma_rearmed, 0, ctx + " no in-place retry");
               if (rc == FX1_ISP_OK && !seen_seq.count(r.sequence)) {
                  seen_seq.insert(r.sequence);
                  ++cs.recoveries;
                  cs.repeated += seen_seq.size() == 2;
                  cs.kept += dev.in_flight > 0;
                  fx1_isp_abort_report rr{};
                  FX1_CHECK_EQ_CTX(fx1_isp_recovery_report(&dev, &rr), FX1_ISP_OK, ctx + " report");
                  FX1_CHECK_EQ_CTX(rr.sequence, r.sequence, ctx + " report of this recovery");
                  FX1_CHECK_EQ_CTX(rr.count <= 4u && rr.count <= r.aborted + dev.in_flight ? 1 : 0, 1,
                                   ctx + " records <= frames in flight");
                  for (unsigned i = 0; i < rr.count; ++i) {
                     const unsigned c = rr.rec[i].cause;
                     FX1_CHECK_EQ_CTX(c == FX1_ISP_ABORT_CAUSE_READ || c == FX1_ISP_ABORT_CAUSE_WRITE ? 1 : 0, 1,
                                      ctx + " AXI cause");
                     FX1_CHECK_EQ_CTX(i == 0 || rr.rec[i].frame == rr.rec[i - 1].frame + 1u ? 1 : 0, 1,
                                      ctx + " records in admission order");
                  }
               }
               if (r.odma_failed) {
                  ++cs.odma_failed;
               }
            }
         }
         // Frames the driver knows are lost: resubmit them first.
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
      std::cout << "fault campaign: " << cs.runs << " runs, " << cs.recoveries << " recoveries (" << cs.kept
                << " keeping DONE frames), " << cs.odma_failed << " ODMA failures, " << cs.lost << " frames lost, "
                << cs.both_at_once << " with both errors at once, " << cs.repeated << " runs with a repeated fault, "
                << cs.multi_done << " handler calls with >= 2 completions, " << cs.stale << " stale recovers, " << cs.requeued
                << " queue calls refused by a restarted rotation\n";
      // The campaign must actually reach the coordinated cases it is for.
      FX1_CHECK(cs.repeated > 0);
      FX1_CHECK(cs.multi_done > 0);
      FX1_CHECK(cs.kept > 0);
   }

   // Buffer metadata through the driver (MAS Table 160, milestone U5): the
   // tag and capture timestamp given at queue time come back with the
   // completed buffer, with output timestamps in the same time base; the
   // stream runs past the four-buffer rotation.
   void test_metadata_driver() {
      const std::string ctx = "metadata";
      const fs::path dir = root / "mandatory_rggb_64x32";
      const fx1_isp_buffers b = start_error_case(dir);
      const unsigned frames = 7;
      std::vector<std::uint32_t> tag(frames), ts(frames), seqs(frames);
      unsigned queued = 0, completed = 0;
      std::uint32_t prev_eof = 0;
      const sc_time end = sc_time_stamp() + sc_time(50, SC_MS);
      while (completed < frames && sc_time_stamp() < end) {
         while (queued < frames && fx1_isp_input_ready(&dev) && dev.in_flight < 4) {
            load_input(fx1_isp_next_input(&dev), e_inputs[queued % 4], ew, eh, align16(2 * ew), b);
            tag[queued] = 0xF00D0000u + queued;
            ts[queued] = fx1_isp_frame_counter(&dev);  // capture time in the IP time base
            FX1_CHECK_EQ_CTX(fx1_isp_queue_frame_meta(&dev, tag[queued], ts[queued], &seqs[queued]), FX1_ISP_OK,
                             ctx + " queue");
            ++queued;
         }
         unsigned idx = 0;
         std::uint32_t seq = 0;
         fx1_isp_frame_meta m{};
         if (fx1_isp_next_done_meta(&dev, &idx, &seq, &m) == FX1_ISP_OK) {
            const std::string fc = ctx + " frame " + std::to_string(completed);
            FX1_CHECK_EQ_CTX(seq, seqs[completed], fc + " order");
            FX1_CHECK_EQ_CTX(m.tag, tag[completed], fc + " tag");
            FX1_CHECK_EQ_CTX(m.ts, ts[completed], fc + " ts");
            FX1_CHECK_EQ_CTX(output(idx, b) == e_expected[completed % 4] ? 1 : 0, 1, fc + " content");
            FX1_CHECK_EQ_CTX(m.sof_ts > m.ts && m.eof_ts > m.sof_ts && m.sof_ts > prev_eof ? 1 : 0, 1, fc + " time order");
            FX1_CHECK_EQ_CTX(fx1_isp_frame_counter(&dev) > m.eof_ts ? 1 : 0, 1, fc + " published after EOF");
            prev_eof = m.eof_ts;
            ++completed;
         } else {
            wait(5, SC_US);
         }
      }
      FX1_CHECK_EQ_CTX(completed, frames, ctx + " all frames");
      // Queued while the IDMA is enabled and waiting (underrun): it starts
      // the frame at IDMA_BUF_VALID, so the tag must already be written.
      wait(20, SC_US);
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT,
                       FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT, ctx + " IDMA waiting");
      load_input(fx1_isp_next_input(&dev), e_inputs[1], ew, eh, align16(2 * ew), b);
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame_meta(&dev, 0xBEEF0001u, 0x1234u, nullptr), FX1_ISP_OK, ctx + " idle queue");
      {
         unsigned idx = 0;
         fx1_isp_frame_meta m{};
         const sc_time until = sc_time_stamp() + sc_time(10, SC_MS);
         while (fx1_isp_next_done_meta(&dev, &idx, nullptr, &m) != FX1_ISP_OK && sc_time_stamp() < until) {
            wait(5, SC_US);
         }
         FX1_CHECK_EQ_CTX(m.tag, 0xBEEF0001u, ctx + " idle queue tag");
         FX1_CHECK_EQ_CTX(m.ts, 0x1234u, ctx + " idle queue ts");
      }
      // The older queue calls leave IDMA_BUF_TAG/TS as they are.
      const unsigned next = fx1_isp_next_input(&dev);
      const std::uint32_t left = rd(FX1_ISP_IDMA_BUF_TAG0_OFFSET + 4u * next);
      load_input(next, e_inputs[0], ew, eh, align16(2 * ew), b);
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " old queue");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_BUF_TAG0_OFFSET + 4u * next), left, ctx + " old queue leaves the tag");
      fx1_isp_stop(&dev);
   }

   // fx1_isp_dma_reset in the middle of a frame (MAS Table 183, DMA_CTRL
   // soft reset): the frame is discarded, nothing completes for it, both
   // rotations restart at buffer 0 with the stream still enabled, and the
   // next frames are correct. Published statistics are kept (the AWB lease
   // across a DMA reset is checked on the platform, isp_mas0710).
   void test_dma_reset_driver() {
      const std::string ctx = "dma reset";
      const fs::path dir = root / "mandatory_rggb_64x32";
      const fx1_isp_buffers b = start_error_case(dir);
      queue_input(0, b, ctx);
      std::vector<std::vector<std::uint8_t>> done;
      FX1_CHECK(service(done, 1, b, ctx + " first frame"));
      const std::uint32_t next_id = rd(FX1_ISP_TUNE_NEXT_FRAME_OFFSET);  // no lease needed (§9.9.9)
      const std::uint32_t odma_before = rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
      mem->latency = sc_time(20, SC_NS);
      const std::size_t writes_before = mem->count(tlm::TLM_WRITE_COMMAND, false);
      queue_input(1, b, ctx);
      const sc_time end = sc_time_stamp() + sc_time(5, SC_MS);
      while (mem->count(tlm::TLM_WRITE_COMMAND, false) < writes_before + 8 && sc_time_stamp() < end) {
         wait(100, SC_NS);  // the second frame is being written
      }
      // The abort point (review U6): the ODMA is writing the frame, not done.
      FX1_CHECK_EQ_CTX(mem->count(tlm::TLM_WRITE_COMMAND, false) >= writes_before + 8 ? 1 : 0, 1, ctx + " writes");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_ODMA_BUSY_MASK,
                       FX1_ISP_DMA_STAT_ODMA_BUSY_MASK, ctx + " ODMA busy at the reset");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), odma_before, ctx + " not completed at the reset");
      FX1_CHECK_EQ_CTX(fx1_isp_dma_reset(&dev), 1, ctx + " one frame dropped");
      mem->latency = SC_ZERO_TIME;
      FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " nothing in flight");
      FX1_CHECK_EQ_CTX(fx1_isp_next_input(&dev), 0u, ctx + " rotation restarts");
      wait(200, SC_US);
      unsigned idx = 0;
      FX1_CHECK_EQ_CTX(fx1_isp_next_done(&dev, &idx), FX1_ISP_ENODATA, ctx + " no completion of the aborted frame");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), odma_before, ctx + " ODMA count");
      // The aborted frame was admitted: its ingress ID stays consumed and the
      // counter is kept. (Published statistics across a DMA reset are read
      // under leases in test_dma_reset_stats.)
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_TUNE_NEXT_FRAME_OFFSET), next_id + 1u, ctx + " ingress counter kept");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_CTRL_OFFSET) & 0x3u, 0x3u, ctx + " enables kept");
      for (unsigned k = 0; k < 4; ++k) {
         queue_input(k, b, ctx);
      }
      done.clear();
      FX1_CHECK(service(done, 4, b, ctx + " restart"));
      for (unsigned k = 0; k < std::min<std::size_t>(4, done.size()); ++k) {
         FX1_CHECK_EQ_CTX(done[k] == e_expected[k] ? 1 : 0, 1, ctx + " frame " + std::to_string(k));
      }
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_FRAME_COUNT_OFFSET), odma_before + 4u, ctx + " four more outputs");
      fx1_isp_stop(&dev);
      FX1_CHECK_EQ_CTX(fx1_isp_dma_reset(&dev), FX1_ISP_ESTATE, ctx + " only while streaming");
   }

   // DMA resets and AEC results (MAS v1.07 Table 9-187): READY results survive
   // every DMA reset; a frame aborted while being measured publishes nothing.
   // (Review U6-R1/U6-R3 sequences, with the v1.07 result banks.)
   void test_dma_reset_stats() {
      const std::string ctx = std::string("dma reset stats") + (fast_csr ? " (fast CSR)" : "");
      const fs::path dir = root / "stats_flat_channels_64x48";
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      reset_and_init();
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      const std::uint32_t in_stride = align16(2 * 64);
      const fx1_isp_buffers b = buffer_layout(in_stride, align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      unsigned collected = 0;
      auto one_frame = [&] {
         const unsigned out = dev.out_next;  // the queue is empty
         collected = out;
         load_input(fx1_isp_next_input(&dev), input, 64, 48, in_stride, b);
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 1, b, ctx);
         // Collected means acknowledged: no DONE left behind, also when the
         // W1C met the completion's core clock (review U6-R3).
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_BUF_DONE_OFFSET) & (1u << out), 0u, ctx + " DONE acknowledged");
         const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);  // publication (U2-A7)
         while ((rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK) && sc_time_stamp() < end) {
            wait(100, SC_NS);
         }
      };
      // Reset while a frame of the given output is being measured.
      auto abort_measuring = [&](const std::string &what) {
         mem->latency = sc_time(20, SC_NS);
         load_input(fx1_isp_next_input(&dev), input, 64, 48, in_stride, b);
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue " + what);
         const sc_time end = sc_time_stamp() + sc_time(5, SC_MS);
         while (!(rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK) && sc_time_stamp() < end) {
            wait(100, SC_NS);
         }
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK, FX1_ISP_AEC_STATUS_BUSY_MASK,
                          ctx + " AEC measuring at the reset " + what);
         FX1_CHECK_EQ_CTX(fx1_isp_dma_reset(&dev), 1, ctx + " frame in progress dropped " + what);
         mem->latency = SC_ZERO_TIME;
      };
      // Acquire the oldest result, read zones 0..3 and the histogram, release.
      fx1_isp_aec_zone z0[4], z1[4];
      std::uint64_t h0[64], h1[64];
      std::uint32_t f0 = 0, f1 = 0;
      auto take = [&](fx1_isp_aec_zone *z, std::uint64_t *h, std::uint32_t &f) {
         fx1_isp_stat_lease l{};
         const int rc = fx1_isp_aec_acquire(&dev, &l);
         if (rc == FX1_ISP_OK) {
            f = l.frame_id;
            FX1_CHECK_EQ_CTX(fx1_isp_aec_read_zones(&dev, 0, 4, z), FX1_ISP_OK, ctx + " zones");
            FX1_CHECK_EQ_CTX(fx1_isp_aec_read_hist(&dev, h, nullptr), FX1_ISP_OK, ctx + " hist");
            FX1_CHECK_EQ_CTX(fx1_isp_aec_release(&dev), FX1_ISP_OK, ctx + " release");
         }
         return rc;
      };
      auto same = [&] {
         bool eq = std::memcmp(h0, h1, sizeof h0) == 0;
         for (unsigned k = 0; k < 4; ++k) {
            eq = eq && z0[k].sum == z1[k].sum && z0[k].count == z1[k].count && z0[k].min == z1[k].min &&
                 z0[k].max == z1[k].max && z0[k].oe == z1[k].oe && z0[k].ue == z1[k].ue;
         }
         return eq ? 1 : 0;
      };
      one_frame();
      FX1_CHECK_EQ_CTX(dev.in_flight, 0u, ctx + " idle");
      FX1_CHECK_EQ_CTX(fx1_isp_dma_reset(&dev), 0, ctx + " idle reset drops nothing");
      FX1_CHECK_EQ_CTX(take(z0, h0, f0), FX1_ISP_OK, ctx + " READY result kept by the reset");
      // A frame completed but not yet collected: its publication is kept.
      load_input(fx1_isp_next_input(&dev), input, 64, 48, in_stride, b);
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue (left DONE)");
      const sc_time end = sc_time_stamp() + sc_time(5, SC_MS);
      while (!rd(FX1_ISP_ODMA_BUF_DONE_OFFSET) && sc_time_stamp() < end) {
         wait(1, SC_US);
      }
      wait(20, SC_US);  // its publication
      FX1_CHECK_EQ_CTX(fx1_isp_dma_reset(&dev), 1, ctx + " DONE frame dropped from the queue");
      FX1_CHECK_EQ_CTX(take(z1, h1, f1), FX1_ISP_OK, ctx + " readable after a DONE frame");
      FX1_CHECK_EQ_CTX(f1, f0 + 1u, ctx + " its publication");
      FX1_CHECK_EQ_CTX(same(), 1, ctx + " same zones and histogram");
      // A frame in progress publishes nothing.
      abort_measuring("(aborted)");
      FX1_CHECK_EQ_CTX(take(z1, h1, f1), FX1_ISP_ENODATA, ctx + " aborted measurement not published");
      one_frame();
      FX1_CHECK_EQ_CTX(take(z1, h1, f1), FX1_ISP_OK, ctx + " next publication");
      FX1_CHECK_EQ_CTX(same(), 1, ctx + " next publication data");
      // Review U6-R3 sequence: a collected frame, then an abort while measuring.
      one_frame();
      abort_measuring("(after a collected frame)");
      FX1_CHECK_EQ_CTX(take(z1, h1, f1), FX1_ISP_OK, ctx + " the collected frame's result");
      FX1_CHECK_EQ_CTX(take(z1, h1, f1), FX1_ISP_ENODATA, ctx + " nothing from the aborted frame");
      // ... and with a DONE bit left set on the collected buffer.
      one_frame();
      dut->debug_hw_set(FX1_ISP_ODMA_BUF_DONE_OFFSET, 1u << collected);
      abort_measuring("(stale DONE on another buffer)");
      FX1_CHECK_EQ_CTX(take(z1, h1, f1), FX1_ISP_OK, ctx + " result before the stale-DONE abort");
      FX1_CHECK_EQ_CTX(take(z1, h1, f1), FX1_ISP_ENODATA, ctx + " nothing from the aborted frame (2)");
      one_frame();
      FX1_CHECK_EQ_CTX(take(z1, h1, f1), FX1_ISP_OK, ctx + " next publication (2)");
      FX1_CHECK_EQ_CTX(same(), 1, ctx + " next publication data (2)");
      fx1_isp_stop(&dev);
   }

   // Review U6-R3: next_done's W1C of DONE on the core clock on which the
   // hardware sets the bit (hardware set wins). The completion is replayed
   // on a clock edge with debug_hw_set, and next_done runs at once: with CSR
   // accesses shorter than a core clock (fast CSR) its W1C falls on that
   // clock. The bit must still be clear when next_done returns, so the
   // buffer does not look complete when it is queued again.
   void test_done_ack_race() {
      const std::string ctx = std::string("DONE acknowledgement") + (fast_csr ? " (fast CSR)" : "");
      const fs::path dir = root / "mandatory_rggb_64x32";
      const std::vector<std::uint8_t> raw = read_file(dir / "input.bin");
      std::vector<std::uint8_t> want = read_file(dir / "expected_y.bin");
      const std::vector<std::uint8_t> want_uv = read_file(dir / "expected_uv.bin");
      want.insert(want.end(), want_uv.begin(), want_uv.end());
      const fx1_isp_buffers b = start_error_case(dir);
      for (unsigned k = 0; k < 6; ++k) {  // through the rotation, each buffer reused
         const unsigned out = dev.out_next;  // the queue is empty
         load_input(fx1_isp_next_input(&dev), raw, ew, eh, align16(2 * ew), b);
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
         const sc_time end = sc_time_stamp() + sc_time(5, SC_MS);
         while (!(dut->debug_peek(FX1_ISP_ODMA_BUF_DONE_OFFSET) & (1u << out)) && sc_time_stamp() < end) {
            wait(100, SC_NS);
         }
         const sc_time p = dut->params().core_period;
         const double now = std::floor(sc_time_stamp() / p);
         wait(p * (now + 1.0) - sc_time_stamp());  // the start of a core clock
         dut->debug_hw_set(FX1_ISP_ODMA_BUF_DONE_OFFSET, 1u << out);  // set on this clock
         unsigned idx = 99;
         FX1_CHECK_EQ_CTX(fx1_isp_next_done(&dev, &idx), FX1_ISP_OK, ctx + " collected");
         FX1_CHECK_EQ_CTX(idx, out, ctx + " buffer");
         FX1_CHECK_EQ_CTX(dut->debug_peek(FX1_ISP_ODMA_BUF_DONE_OFFSET) & (1u << out), 0u,
                          ctx + " DONE clear after next_done, frame " + std::to_string(k));
         FX1_CHECK_EQ_CTX(output(idx, b) == want ? 1 : 0, 1, ctx + " content");
      }
      fx1_isp_stop(&dev);
   }

   // Review U6-R2: a DMA reset during a GTM hand-over. The frames counted at
   // step 1 that the reset dropped are not waited for: one frame taken after
   // the reset completes step 2. Before it, a pending curve still blocks.
   void test_dma_reset_gtm() {
      const std::string ctx = std::string("dma reset gtm") + (fast_csr ? " (fast CSR)" : "");
      std::uint16_t lut[65];
      for (unsigned i = 0; i < 65; ++i) {
         lut[i] = static_cast<std::uint16_t>(0x200 + 29 * i);
      }
      auto lut_ok = [&] {
         unsigned bad = 0;
         for (unsigned i = 0; i < 65; ++i) {
            hal_write(this, FX1_ISP_GTM_LUT_ADDR_OFFSET, i);
            bad += rd(FX1_ISP_GTM_LUT_RDATA_OFFSET) != lut[i];
         }
         return bad == 0;
      };
      const fs::path dir = root / "gtm_auto_3frames_96x64";
      std::ifstream meta(dir / "meta.txt");
      std::uint32_t in_w = 0, in_h = 0, out_w = 0, out_h = 0, frames = 1;
      meta >> in_w >> in_h >> out_w >> out_h >> frames;
      const std::vector<std::uint8_t> input = read_file(dir / "input.bin");
      const std::uint32_t in_stride = align16(2 * in_w);
      for (const bool cold : {false, true}) {
         const std::string cc = ctx + (cold ? " cold" : " warm");
         reset_and_init();
         apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
         const fx1_isp_buffers b = buffer_layout(in_stride, align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
         FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, cc + " start");
         auto one_frame = [&] {
            load_input(fx1_isp_next_input(&dev), input, in_w, in_h, in_stride, b);
            FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, cc + " queue");
            std::vector<std::vector<std::uint8_t>> done;
            service(done, 1, b, cc);
            const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);
            while ((rd(FX1_ISP_GTM_STATUS_OFFSET) & FX1_ISP_GTM_STATUS_BUSY_MASK) && sc_time_stamp() < end) {
               wait(100, SC_NS);
            }
         };
         if (!cold) {
            one_frame();  // automatic curve built at its end, pending
         }
         const std::uint32_t count = rd(FX1_ISP_IDMA_FRAME_COUNT_OFFSET);
         FX1_CHECK_EQ_CTX(count, cold ? 0u : 1u, cc + " IDMA_FRAME_COUNT");
         // A frame whose input is still being read when step 1 runs.
         mem->latency = sc_time(1, SC_US);
         load_input(fx1_isp_next_input(&dev), input, in_w, in_h, in_stride, b);
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, cc + " queue (to drop)");
         const sc_time end = sc_time_stamp() + sc_time(1, SC_MS);
         while (!(rd(FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_IDMA_BUSY_MASK) && sc_time_stamp() < end) {
            wait(100, SC_NS);
         }
         FX1_CHECK_EQ_CTX(fx1_isp_gtm_manual_begin(&dev), FX1_ISP_OK, cc + " step 1 with a frame in flight");
         FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_EAGAIN, cc + " refused before the reset");
         FX1_CHECK_EQ_CTX(fx1_isp_dma_reset(&dev), 1, cc + " the reset drops it");
         mem->latency = SC_ZERO_TIME;
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_FRAME_COUNT_OFFSET), count, cc + " the dropped input was not taken");
         if (cold) {
            // Nothing was taken since i_rst_n and nothing is queued: no
            // curve can be pending, the load proceeds at once.
            FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_OK, cc + " load after the reset");
         } else {
            FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_EAGAIN, cc + " the pending curve still blocks");
            one_frame();  // one frame taken after the reset: step 2 done
            FX1_CHECK_EQ_CTX(fx1_isp_load_gtm_lut(&dev, lut), FX1_ISP_OK, cc + " load after one new frame");
         }
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_GTM_CTRL_OFFSET), FX1_ISP_GTM_CTRL_EN_MASK | FX1_ISP_GTM_CTRL_MANUAL_MASK,
                          cc + " EN restored, MANUAL kept");
         FX1_CHECK_EQ_CTX(lut_ok() ? 1 : 0, 1, cc + " read-back");
         one_frame();
         one_frame();
         FX1_CHECK_EQ_CTX(lut_ok() ? 1 : 0, 1, cc + " the software curve survives the next SOFs");
         fx1_isp_stop(&dev);
      }
   }

   // V107-11 platform CI (fw isp_recovery, phase A): the caller read
   // fx1_isp_next_input() and filled that slot; before its queue another
   // driver call (here fx1_isp_input_ready) settled an automatic recovery,
   // which restarted the rotation at 0. The queue must refuse (EAGAIN) rather
   // than arm slot 0, which still holds an older frame; after a refill of the
   // new slot the frame completes with its own content.
   // Second case: a caller that tracks the slot itself (no
   // fx1_isp_next_input() since its last queue) fills slot 1, and the queue's
   // own readiness check settles the finished recovery: the slot moved under
   // it, so EAGAIN as well.
   void test_queue_after_foreign_settle() {
      const fs::path dir = root / "mandatory_rggb_64x32";
      for (const bool read_next : {true, false}) {
         const std::string ctx = std::string("queue after a recovery settled ") +
                                 (read_next ? "elsewhere" : "inside the queue, slot tracked by the caller");
         const fx1_isp_buffers b = start_error_case(dir);
         load_input(1, e_inputs[0], ew, eh, align16(2 * ew), b);  // slot 1 holds another frame
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, b.y[0], b.y[0] + dev.out_width, 1});  // output line 0
         queue_input(0, b, ctx + ", faulted frame");
         if (read_next) {
            const unsigned slot = fx1_isp_next_input(&dev);
            FX1_CHECK_EQ_CTX(slot, 1u, ctx + ", next slot");
         }
         load_input(1, e_inputs[1], ew, eh, align16(2 * ew), b);
         const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
         while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) && sc_time_stamp() < end) {
            wait(10, SC_NS);  // raw reads: nothing settles yet
         }
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT,
                          FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT, ctx + ", the output failed");
         if (read_next) {
            while (!fx1_isp_input_ready(&dev) && sc_time_stamp() < end) {  // settles the recovery
               wait(100, SC_NS);
            }
            FX1_CHECK_EQ_CTX(dev.in_next, 0u, ctx + ", the recovery restarted the rotation");
         } else {
            while ((rd(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) &
                    (FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK | FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK)) !=
                       FX1_ISP_COMMON_RECOVERY_STATUS_DONE_MASK &&
                   sc_time_stamp() < end) {
               wait(100, SC_NS);  // the hardware finishes; the driver has not settled it
            }
            FX1_CHECK_EQ_CTX(dev.in_next, 1u, ctx + ", not settled before the queue");
         }
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_EAGAIN, ctx + ", the filled slot is stale");
         FX1_CHECK_EQ_CTX(dev.in_next, 0u, ctx + ", rotation restarted");
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0u, ctx + ", nothing armed");
         mem->faults.clear();
         std::uint32_t lseq = 0;
         unsigned linput = 0;
         while (fx1_isp_next_lost(&dev, &lseq, &linput) == FX1_ISP_OK) {
         }
         std::vector<std::vector<std::uint8_t>> done;
         queue_input(1, b, ctx + ", refilled");
         service(done, 1, b, ctx);
         FX1_CHECK_EQ_CTX(!done.empty() && done[0] == e_expected[1] ? 1 : 0, 1, ctx + ", the frame's own content");
         fx1_isp_stop(&dev);
      }
   }

   // A frame whose output fails while the IDMA is still reading its input is
   // lost at once (MAS v1.07 §9.9.8; the deferred loss of DEC-19 is gone):
   // the recovery stops the IDMA, so (a) a later read fault in the same input
   // is never reached, (b) nothing else is read. The loss is known as soon as
   // the recovery has finished; its report names the frame, CH0 and cause 2.
   void test_immediate_loss() {
      const fs::path dir = root / "mandatory_rggb_64x32";
      const std::uint32_t stride = align16(2 * ew);
      for (const bool idma_fault_later : {true, false}) {
         const std::string ctx = std::string("immediate loss") + (idma_fault_later ? ", read fault later" : "");
         const fx1_isp_buffers b = start_error_case(dir);
         mem->faults.push_back({tlm::TLM_WRITE_COMMAND, b.y[0], b.y[0] + dev.out_width, 1});  // output line 0
         if (idma_fault_later) {
            const std::uint64_t row = b.in[0] + std::uint64_t{eh - 4} * stride;  // late in the same input
            mem->faults.push_back({tlm::TLM_READ_COMMAND, row, row + 2u * ew, 1});
         }
         queue_input(0, b, ctx);
         const sc_time end = sc_time_stamp() + sc_time(20, SC_MS);
         while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) && sc_time_stamp() < end) {
            wait(10, SC_NS);
         }
         fx1_isp_events ev{};
         fx1_isp_irq(&dev, &ev);
         FX1_CHECK_EQ_CTX(ev.common & FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK, FX1_ISP_COMMON_IRQ_STATUS_ERROR_IRQ_MASK,
                          ctx + " ERROR_IRQ on the accepted recovery");
         fx1_isp_recovery r{};
         FX1_CHECK_EQ_CTX(fx1_isp_recover_ex(&dev, ev.dma_err, &r), FX1_ISP_OK, ctx + " recover");
         FX1_CHECK_EQ_CTX(r.odma_failed == 1 && r.aborted == 1 && r.odma_seq == 0 && r.odma_output == 0 ? 1 : 0, 1,
                          ctx + " output failure, one frame aborted");
         std::uint32_t lseq = 99;
         unsigned linput = 99;
         FX1_CHECK_EQ_CTX(fx1_isp_next_lost(&dev, &lseq, &linput), FX1_ISP_OK, ctx + " lost at once");
         FX1_CHECK_EQ_CTX(lseq == 0 && linput == 0 ? 1 : 0, 1, ctx + " lost frame identity");
         FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " nothing in flight");
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_IDMA_BUF_VALID_OFFSET), 0u, ctx + " input released");
         const fx1_isp_abort_report rr = recovery_report(ctx);
         FX1_CHECK_EQ_CTX(rr.sequence, r.sequence, ctx + " report sequence");
         FX1_CHECK_EQ_CTX(rr.count, 1u, ctx + " one record");
         FX1_CHECK_EQ_CTX(rr.rec[0].frame == 1 && rr.rec[0].failed_outputs == 1 && rr.rec[0].cause == FX1_ISP_ABORT_CAUSE_WRITE ? 1 : 0,
                          1, ctx + " record: ingress 1, CH0, write error");
         if (idma_fault_later) {
            FX1_CHECK_EQ_CTX(mem->faults.size() == 2 && mem->faults[1].remaining == 1 ? 1 : 0, 1,
                             ctx + " the read fault was never reached");
         }
         mem->faults.clear();
         std::vector<std::vector<std::uint8_t>> done;
         queue_input(0, b, ctx + " resubmit");
         service(done, 1, b, ctx);
         FX1_CHECK_EQ_CTX(!done.empty() && done[0] == e_expected[0] ? 1 : 0, 1, ctx + " resubmitted frame content");
         FX1_CHECK_EQ_CTX(dev.in_flight, 0, ctx + " nothing in flight at the end");
         // A soft reset with a frame in flight: a common-scope recovery, cause 4.
         queue_input(1, b, ctx);
         const sc_time lim = sc_time_stamp() + sc_time(5, SC_MS);
         while (!(rd(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) && sc_time_stamp() < lim) {
            wait(50, SC_NS);
         }
         fx1_isp_soft_reset(&dev);
         const fx1_isp_abort_report rs = recovery_report(ctx + " soft reset");
         FX1_CHECK_EQ_CTX(rs.sequence, rr.sequence + 1u, ctx + " soft reset: next sequence");
         FX1_CHECK_EQ_CTX(rs.count == 1 && rs.rec[0].frame == 3 && rs.rec[0].cause == FX1_ISP_ABORT_CAUSE_COMMON ? 1 : 0, 1,
                          ctx + " soft reset record");
         FX1_CHECK_EQ_CTX(dev.rec_seq, rs.sequence, ctx + " the driver's own recovery is settled");
         fx1_isp_stop(&dev);
      }
   }

   // V107-10 driver API: discovery, CH1/CH2 buffers through the driver (one
   // index per frame on every enabled channel, completion when all are DONE,
   // a recovery clearing a partial DONE), and the profile transaction.
   std::vector<std::uint8_t> channel_plane(std::uint64_t base, std::uint32_t row_bytes, std::uint32_t rows,
                                           std::uint32_t stride) {
      std::vector<std::uint8_t> o;
      for (std::uint32_t y = 0; y < rows; ++y) {
         const std::uint8_t *p = mem->at(base + std::uint64_t{y} * stride);
         o.insert(o.end(), p, p + row_bytes);
      }
      return o;
   }
   void test_v107_api() {
      const fs::path dir = root / "mandatory_rggb_64x32";
      std::string ctx = "v107 api discovery";
      reset_and_init();
      fx1_isp_identity id{};
      FX1_CHECK_EQ_CTX(fx1_isp_discover(&dev, &id), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(id.magic, FX1_ISP_TUNE_ID_MAGIC_RESET, ctx + " TUNE_ID");
      FX1_CHECK_EQ_CTX(id.revision, FX1_ISP_TUNE_REV_VALUE_RESET, ctx + " TUNE_REV 1.07");
      FX1_CHECK_EQ_CTX(id.caps, 0x003FFFFFu, ctx + " TUNE_CAPS");
      FX1_CHECK_EQ_CTX(fx1_isp_require_caps(&dev, FX1_ISP_TUNE_CAPS_RECOVERY_REPORT_MASK | FX1_ISP_TUNE_CAPS_PROFILE_MASK),
                       FX1_ISP_OK, ctx + " required caps present");
      FX1_CHECK_EQ_CTX(fx1_isp_require_caps(&dev, 1u << 22), FX1_ISP_ENOTSUP, ctx + " unknown cap");

      // Three outputs through the driver.
      ctx = "v107 api channels";
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      hal_write(this, FX1_ISP_OUTPUT_ENABLE_OFFSET, 7);
      const std::uint32_t w = rd(FX1_ISP_RESIZER_OUT_W_OFFSET), h = rd(FX1_ISP_RESIZER_OUT_H_OFFSET);
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_CH1_RESIZER_OUT_W_OFFSET) == w && rd(FX1_ISP_CH2_RESIZER_OUT_W_OFFSET) == w ? 1 : 0, 1,
                       ctx + " pass-through on every channel");
      const fx1_isp_buffers b = buffer_layout(align16(2 * ew), align16(w));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_EINVAL, ctx + " start refused: no CH1/CH2 buffers");
      fx1_isp_channel_buffers c1{}, c2{};
      c1.y_stride = c1.uv_stride = align16(w);
      c2.y_stride = 3 * w - 16;  // too short for RGB888
      for (unsigned k = 0; k < 4; ++k) {
         c1.y[k] = ch1_y_base + 0x80000ull * k;
         c1.uv[k] = ch1_uv_base + 0x80000ull * k;
         c2.y[k] = ch2_base + 0x80000ull * k;
      }
      FX1_CHECK_EQ_CTX(fx1_isp_set_channel_buffers(&dev, 0, &c1), FX1_ISP_EINVAL, ctx + " CH0 uses fx1_isp_buffers");
      FX1_CHECK_EQ_CTX(fx1_isp_set_channel_buffers(&dev, 1, &c1), FX1_ISP_OK, ctx + " CH1");
      FX1_CHECK_EQ_CTX(fx1_isp_set_channel_buffers(&dev, 2, &c2), FX1_ISP_OK, ctx + " CH2 (short stride)");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_EINVAL, ctx + " start refused: CH2 stride < 3W");
      c2.y_stride = align16(3 * w);
      FX1_CHECK_EQ_CTX(fx1_isp_set_channel_buffers(&dev, 2, &c2), FX1_ISP_OK, ctx + " CH2");
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      FX1_CHECK_EQ_CTX(fx1_isp_set_channel_buffers(&dev, 2, &c2), FX1_ISP_ESTATE, ctx + " not while streaming");
      seq_input.clear();
      to_resubmit.clear();
      resubmitted.clear();
      errors_recovered = 0;
      std::map<unsigned, std::vector<std::uint8_t>> rgb_of_input;
      std::uint32_t prev_frame = 0;
      unsigned collected = 0;
      auto collect = [&](unsigned want, const std::vector<unsigned> &order) {
         const sc_time end = sc_time_stamp() + sc_time(50, SC_MS);
         while (collected < want && sc_time_stamp() < end) {
            if (!irq.read()) {
               wait(sc_time(1, SC_MS), irq.posedge_event());
            }
            fx1_isp_events ev{};
            fx1_isp_irq(&dev, &ev);
            if (ev.dma_err & (FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT)) {
               FX1_CHECK_EQ_CTX(fx1_isp_recover(&dev, ev.dma_err), FX1_ISP_OK, ctx + " recover");
               ++errors_recovered;
            }
            unsigned idx = 0;
            std::uint32_t seq = 0;
            while (fx1_isp_next_done_ex(&dev, &idx, &seq) == FX1_ISP_OK) {
               const std::string fc = ctx + " frame " + std::to_string(collected);
               const unsigned in = seq_input.at(seq);
               FX1_CHECK_EQ_CTX(collected < order.size() && in == order[collected] ? 1 : 0, 1, fc + " order");
               const std::vector<std::uint8_t> ch0 = output(idx, b);
               std::vector<std::uint8_t> ch1 = channel_plane(c1.y[idx], w, h, c1.y_stride);
               const std::vector<std::uint8_t> uv1 = channel_plane(c1.uv[idx], w, h / 2, c1.uv_stride);
               ch1.insert(ch1.end(), uv1.begin(), uv1.end());
               FX1_CHECK_EQ_CTX(ch0 == e_expected[in] ? 1 : 0, 1, fc + " CH0 content");
               FX1_CHECK_EQ_CTX(ch1 == ch0 ? 1 : 0, 1, fc + " CH1 = CH0 (same geometry)");
               const std::vector<std::uint8_t> rgb = channel_plane(c2.y[idx], 3 * w, h, c2.y_stride);
               if (!rgb_of_input.count(in)) {
                  rgb_of_input[in] = rgb;
               }
               FX1_CHECK_EQ_CTX(rgb == rgb_of_input[in] ? 1 : 0, 1, fc + " CH2 depends only on the input");
               std::uint32_t f[3] = {0, 0, 0};
               for (unsigned c = 0; c < 3; ++c) {
                  FX1_CHECK_EQ_CTX(fx1_isp_output_meta(&dev, c, idx, nullptr, &f[c], nullptr), FX1_ISP_OK, fc);
               }
               FX1_CHECK_EQ_CTX(f[0] == f[1] && f[1] == f[2] && f[0] > prev_frame ? 1 : 0, 1,
                                fc + " one ingress ID on every channel, increasing");
               prev_frame = f[0];
               for (const std::uint32_t off : {FX1_ISP_ODMA_BUF_DONE_OFFSET, FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET,
                                               FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET}) {
                  FX1_CHECK_EQ_CTX(rd(off) & (1u << idx), 0u, fc + " DONE returned on every channel");
               }
               ++collected;
            }
            std::uint32_t lseq = 0;
            while (fx1_isp_next_lost(&dev, &lseq, nullptr) == FX1_ISP_OK) {
               to_resubmit.push_back(seq_input.at(lseq));
            }
            while (!to_resubmit.empty() && fx1_isp_input_ready(&dev)) {
               const unsigned k = to_resubmit.front();
               to_resubmit.pop_front();
               queue_input(k, b, ctx + " resubmit");
               resubmitted.push_back(k);
            }
         }
         FX1_CHECK_EQ_CTX(collected, want, ctx + " frames collected");
      };
      for (unsigned k = 0; k < 4; ++k) queue_input(k, b, ctx);
      collect(4, {0, 1, 2, 3});
      for (unsigned k = 0; k < 2; ++k) queue_input(k, b, ctx);  // through the rotation
      collect(6, {0, 1, 2, 3, 0, 1});
      // CH2 fails on the last line of buffer 2 (CH0/CH1 may have published):
      // the frame is lost, a partial DONE is cleared, the rotation restarts.
      ctx = "v107 api channels, CH2 write error";
      const std::uint64_t row = c2.y[2] + std::uint64_t{h - 1} * c2.y_stride;
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, row, row + 3u * w, 1});
      queue_input(2, b, ctx);
      queue_input(3, b, ctx);
      collect(8, {0, 1, 2, 3, 0, 1, 2, 3});
      FX1_CHECK_EQ_CTX(errors_recovered, 1u, ctx + " one recovery");
      FX1_CHECK_EQ_CTX(resubmitted.size() >= 1 && resubmitted[0] == 2 ? 1 : 0, 1, ctx + " frame of input 2 resubmitted");
      const fx1_isp_abort_report rr = recovery_report(ctx);
      FX1_CHECK_EQ_CTX(rr.count >= 1 && (rr.rec[0].failed_outputs & 4u) && rr.rec[0].cause == FX1_ISP_ABORT_CAUSE_WRITE ? 1 : 0,
                       1, ctx + " record: CH2 failed, write error");
      mem->faults.clear();
      FX1_CHECK_EQ_CTX(dev.in_flight, 0u, ctx + " nothing in flight");
      // A channel that published DONE for the aborted frame keeps it (DMA
      // scope); the driver cleared it, so no buffer looks complete when reused.
      for (const std::uint32_t off : {FX1_ISP_ODMA_BUF_DONE_OFFSET, FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET,
                                      FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET}) {
         FX1_CHECK_EQ_CTX(rd(off), 0u, ctx + " no stale DONE @" + fx1_test::hex(off));
      }
      fx1_isp_stop(&dev);

      // A partial DONE for certain: CH1 crops the top half, so it publishes
      // mid-frame; CH2 then fails on its last line. The recovery keeps CH1's
      // DONE (DMA scope); the driver must clear it before reusing the index.
      ctx = "v107 api channels, partial DONE";
      reset_and_init();
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      hal_write(this, FX1_ISP_OUTPUT_ENABLE_OFFSET, 7);
      FX1_CHECK_EQ_CTX(fx1_isp_set_crop(&dev, 1, 1, 0, 0, w, h / 2), FX1_ISP_OK, ctx + " CH1 crop top half");
      FX1_CHECK_EQ_CTX(fx1_isp_set_channel_buffers(&dev, 1, &c1), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_set_channel_buffers(&dev, 2, &c2), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      seq_input.clear();
      to_resubmit.clear();
      resubmitted.clear();
      const std::uint64_t last = c2.y[0] + std::uint64_t{h - 1} * c2.y_stride;
      mem->faults.push_back({tlm::TLM_WRITE_COMMAND, last, last + 3u * w, 1});
      queue_input(0, b, ctx);
      const sc_time lim = sc_time_stamp() + sc_time(5, SC_MS);
      while (!(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) && sc_time_stamp() < lim) {
         wait(50, SC_NS);
      }
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET) & 1u, 1u, ctx + " CH1 published before CH2 failed");
      fx1_isp_events ev{};
      fx1_isp_irq(&dev, &ev);
      FX1_CHECK_EQ_CTX(fx1_isp_recover(&dev, ev.dma_err), FX1_ISP_OK, ctx + " recover");
      for (const std::uint32_t off : {FX1_ISP_ODMA_BUF_DONE_OFFSET, FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET,
                                      FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET}) {
         FX1_CHECK_EQ_CTX(rd(off), 0u, ctx + " partial DONE cleared @" + fx1_test::hex(off));
      }
      FX1_CHECK_EQ_CTX(fx1_isp_next_lost(&dev, nullptr, nullptr), FX1_ISP_OK, ctx + " frame lost");
      mem->faults.clear();
      queue_input(1, b, ctx);  // index 0 again, another input
      std::vector<std::vector<std::uint8_t>> pd;
      service(pd, 1, b, ctx);
      FX1_CHECK_EQ_CTX(!pd.empty() && pd[0] == e_expected[1] ? 1 : 0, 1, ctx + " CH0 of the next frame");
      fx1_isp_stop(&dev);

      // Profile transaction (MAS v1.07 §9.7-9.9).
      ctx = "v107 api profile";
      reset_and_init();
      apply_profile(fx1_test::read_profile(dir / "profile.csrw"));
      std::uint32_t err = 0;
      FX1_CHECK_EQ_CTX(fx1_isp_profile_apply(&dev, 0, 0, 0, &err), FX1_ISP_EIO, ctx + " APPLY in legacy mode");
      FX1_CHECK_EQ_CTX(err & FX1_ISP_TUNE_ERROR_MODE_MASK, FX1_ISP_TUNE_ERROR_MODE_MASK, ctx + " MODE");
      // Review R4: a MODE bit already sticky from a raw command does not hide
      // the same rejection of the next one.
      hal_write(this, FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_APPLY_MASK);
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_MODE_MASK, FX1_ISP_TUNE_ERROR_MODE_MASK,
                       ctx + " MODE sticky before the call");
      dev.tune_error_prior = 0;
      FX1_CHECK_EQ_CTX(fx1_isp_profile_apply(&dev, 0, 0, 0, &err), FX1_ISP_EIO, ctx + " repeated rejection reported");
      FX1_CHECK_EQ_CTX(err & FX1_ISP_TUNE_ERROR_MODE_MASK, FX1_ISP_TUNE_ERROR_MODE_MASK, ctx + " MODE again");
      FX1_CHECK_EQ_CTX(dev.tune_error_prior & FX1_ISP_TUNE_ERROR_MODE_MASK, FX1_ISP_TUNE_ERROR_MODE_MASK,
                       ctx + " the earlier bit kept as prior");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_mode(&dev, 1), FX1_ISP_OK, ctx + " enter profile mode");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_begin(&dev, 2, &err), FX1_ISP_EINVAL, ctx + " bank 0..1");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_begin(&dev, 0, &err), FX1_ISP_OK, ctx + " INIT bank 0");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_apply(&dev, 0, 0, 0, &err), FX1_ISP_EIO, ctx + " APPLY before SEAL");
      FX1_CHECK_EQ_CTX(err & FX1_ISP_TUNE_ERROR_NOT_READY_MASK, FX1_ISP_TUNE_ERROR_NOT_READY_MASK, ctx + " NOT_READY");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_seal(&dev, 0, 0x77u, &err), FX1_ISP_OK, ctx + " SEAL (Gamma off)");
      hal_write(this, FX1_ISP_WB_GAIN_R_OFFSET, rd(FX1_ISP_WB_GAIN_R_OFFSET) ^ 0x10u);  // sealed bank
      FX1_CHECK_EQ_CTX(hal_read(this, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_LOCKED_MASK,
                       FX1_ISP_TUNE_ERROR_LOCKED_MASK, ctx + " sealed bank is LOCKED");
      hal_write(this, FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_LOCKED_MASK);
      FX1_CHECK_EQ_CTX(fx1_isp_profile_apply(&dev, 0, 0, 0, &err), FX1_ISP_OK, ctx + " APPLY");
      fx1_isp_profile_status ps{};
      FX1_CHECK_EQ_CTX(fx1_isp_profile_state(&dev, &ps), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(ps.status & FX1_ISP_TUNE_STATUS_BANK0_PENDING_MASK, FX1_ISP_TUNE_STATUS_BANK0_PENDING_MASK,
                       ctx + " bank 0 pending");
      const fx1_isp_buffers pb = buffer_layout(align16(2 * ew), align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &pb), FX1_ISP_OK, ctx + " start");
      seq_input.clear();
      queue_input(0, pb, ctx);
      std::vector<std::vector<std::uint8_t>> done;
      service(done, 1, pb, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_profile_state(&dev, &ps), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(ps.active_id, 0x77u, ctx + " active profile");
      FX1_CHECK_EQ_CTX(ps.status & FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK, FX1_ISP_TUNE_STATUS_ACTIVE_VALID_MASK,
                       ctx + " active valid");
      std::uint32_t prof = 0;
      FX1_CHECK_EQ_CTX(fx1_isp_output_meta(&dev, 0, 0, nullptr, nullptr, &prof), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(prof, 0x77u, ctx + " output carries the profile ID");
      FX1_CHECK_EQ_CTX(!done.empty() && done[0] == e_expected[0] ? 1 : 0, 1, ctx + " INIT copy reproduces legacy output");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_unseal(&dev, 0, &err), FX1_ISP_EIO, ctx + " active bank cannot be unsealed");
      FX1_CHECK_EQ_CTX(err & FX1_ISP_TUNE_ERROR_LOCKED_MASK, FX1_ISP_TUNE_ERROR_LOCKED_MASK, ctx + " LOCKED");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_mode(&dev, 0), FX1_ISP_EBUSY, ctx + " mode change while streaming");
      fx1_isp_stop(&dev);

      // Review R5: a profile with Gamma on. INIT clears the bank's payload
      // validity, so SEAL is NOT_READY until the curve is loaded into the bank
      // through the banked port and confirmed; the output then equals legacy.
      ctx = "v107 api profile, Gamma on";
      const fs::path gdir = root / "grbg_gamma22_128x72";
      const std::vector<fx1_test::csr_line> gprof = fx1_test::read_profile(gdir / "profile.csrw");
      std::uint16_t glut[256] = {};
      unsigned gn = 0;
      for (const fx1_test::csr_line &l : gprof) {
         if (l.reg == "GAMMA_LUT_DATA" && gn < 256) {
            glut[gn++] = static_cast<std::uint16_t>(l.value);
         }
      }
      FX1_CHECK_EQ_CTX(gn, 256u, ctx + " curve in the vector");
      reset_and_init();
      apply_profile(gprof);
      FX1_CHECK_EQ_CTX(fx1_isp_profile_mode(&dev, 1), FX1_ISP_OK, ctx + " profile mode");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_begin(&dev, 0, &err), FX1_ISP_OK, ctx + " INIT");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_seal(&dev, 0, 0x22u, &err), FX1_ISP_EIO, ctx + " SEAL before the curve");
      FX1_CHECK_EQ_CTX(err & FX1_ISP_TUNE_ERROR_NOT_READY_MASK, FX1_ISP_TUNE_ERROR_NOT_READY_MASK, ctx + " NOT_READY");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_confirm_tables(&dev, 0, FX1_ISP_TUNE_TABLE_CMD_GAMMA_CONFIRM_MASK, &err),
                       FX1_ISP_EIO, ctx + " confirm alone fails (payload cleared by INIT)");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_load_gamma(&dev, 0, glut, &err), FX1_ISP_OK, ctx + " load + confirm");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_seal(&dev, 0, 0x22u, &err), FX1_ISP_OK, ctx + " SEAL");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_apply(&dev, 0, 0, 0, &err), FX1_ISP_OK, ctx + " APPLY");
      std::uint32_t gw = 0, gh = 0;
      {
         std::ifstream mf(gdir / "meta.txt");  // input W H, output W H, ...
         mf >> gw >> gh;
      }
      const fx1_isp_buffers gb = buffer_layout(align16(2 * gw), align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &gb), FX1_ISP_OK, ctx + " start");
      load_input(fx1_isp_next_input(&dev), read_file(gdir / "input.bin"), gw, gh, align16(2 * gw), gb);
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
      std::vector<std::vector<std::uint8_t>> gdone;
      service(gdone, 1, gb, ctx);
      std::vector<std::uint8_t> gwant = read_file(gdir / "expected_y.bin");
      const std::vector<std::uint8_t> guv = read_file(gdir / "expected_uv.bin");
      gwant.insert(gwant.end(), guv.begin(), guv.end());
      FX1_CHECK_EQ_CTX(!gdone.empty() && gdone[0] == gwant ? 1 : 0, 1, ctx + " output = the vector's expected");
      std::uint32_t gp = 0;
      FX1_CHECK_EQ_CTX(fx1_isp_output_meta(&dev, 0, 0, nullptr, nullptr, &gp) == FX1_ISP_OK && gp == 0x22u ? 1 : 0, 1,
                       ctx + " profile ID 0x22");
      fx1_isp_stop(&dev);
      FX1_CHECK_EQ_CTX(fx1_isp_profile_mode(&dev, 0), FX1_ISP_OK, ctx + " back to legacy");
   }

   // Review V107-7..10 R2 / R6: the driver's bookkeeping across a recovery.
   // (a) A recovery while a completed frame waits: nothing is collected
   //     until it has finished, then the frame is, and the restarted
   //     rotation's next frame is not reported complete before its DMA ran.
   // (b) A recovery that starts between next_done's checks and its DONE
   //     acknowledgement (dropped by the command block): EAGAIN, the frame
   //     stays queued and is collected once the recovery has finished.
   // (c) A reset whose drain outlasts the driver's bounded wait: EBUSY,
   //     the queue kept; the next call completes it without a new request.
   void test_recovery_bookkeeping() {
      const fs::path dir = root / "mandatory_rggb_64x32";
      std::string ctx = "bookkeeping (a) collect during a recovery";
      fx1_isp_buffers b = start_error_case(dir);
      queue_input(0, b, ctx);
      const sc_time lim = sc_time_stamp() + sc_time(5, SC_MS);
      while (!(dut->debug_peek(FX1_ISP_ODMA_BUF_DONE_OFFSET) & 1u) && sc_time_stamp() < lim) {
         wait(1, SC_US);
      }
      wait(20, SC_US);  // the frame has retired
      mem->blocking = true;
      mem->latency = sc_time(20, SC_US);
      queue_input(1, b, ctx);
      while (mem->active_idma == 0 && sc_time_stamp() < lim) {
         wait(100, SC_NS);
      }
      const std::uint32_t seq0 = rd(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
      hal_write(this, FX1_ISP_DMA_CTRL_OFFSET, dev.dma_ctrl | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);  // not the driver's
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK,
                       FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK, ctx + " recovery draining");
      unsigned idx = 99;
      std::uint32_t seq = 99;
      FX1_CHECK_EQ_CTX(fx1_isp_next_done_ex(&dev, &idx, &seq), FX1_ISP_ENODATA, ctx + " nothing collected while busy");
      FX1_CHECK_EQ_CTX(dev.in_flight, 2u, ctx + " queue kept while busy");
      mem->blocking = false;
      mem->latency = SC_ZERO_TIME;
      while ((rd(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK) &&
             sc_time_stamp() < lim + sc_time(5, SC_MS)) {
         wait(1, SC_US);
      }
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET), seq0 + 1u, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_next_done_ex(&dev, &idx, &seq), FX1_ISP_OK, ctx + " the completed frame after the recovery");
      FX1_CHECK_EQ_CTX(idx == 0 && seq == 0 && output(0, b) == e_expected[0] ? 1 : 0, 1, ctx + " frame 0 intact");
      FX1_CHECK_EQ_CTX(fx1_isp_next_lost(&dev, nullptr, nullptr), FX1_ISP_OK, ctx + " frame 1 lost");
      queue_input(2, b, ctx);  // output 0 again
      FX1_CHECK_EQ_CTX(fx1_isp_next_done_ex(&dev, &idx, &seq), FX1_ISP_ENODATA, ctx + " no completion before its DMA");
      std::vector<std::vector<std::uint8_t>> done;
      service(done, 1, b, ctx);
      FX1_CHECK_EQ_CTX(!done.empty() && done[0] == e_expected[2] ? 1 : 0, 1, ctx + " the new frame's own output");
      fx1_isp_stop(&dev);

      ctx = "bookkeeping (b) recovery during the acknowledgement";
      b = start_error_case(dir);
      queue_input(0, b, ctx);
      while (!(dut->debug_peek(FX1_ISP_ODMA_BUF_DONE_OFFSET) & 1u) && sc_time_stamp() < lim + sc_time(10, SC_MS)) {
         wait(1, SC_US);
      }
      wait(20, SC_US);
      mem->blocking = true;
      mem->latency = sc_time(20, SC_US);
      queue_input(1, b, ctx);
      while (mem->active_idma == 0 && sc_time_stamp() < lim + sc_time(10, SC_MS)) {
         wait(100, SC_NS);
      }
      bool fired = false;
      before_write = [&](std::uint32_t off, std::uint32_t) {
         if (off == FX1_ISP_ODMA_BUF_DONE_OFFSET && !fired) {
            fired = true;
            access(tlm::TLM_WRITE_COMMAND, FX1_ISP_DMA_CTRL_OFFSET, dev.dma_ctrl | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
         }
      };
      FX1_CHECK_EQ_CTX(fx1_isp_next_done_ex(&dev, &idx, &seq), FX1_ISP_EAGAIN, ctx + " acknowledgement dropped");
      before_write = nullptr;
      FX1_CHECK_EQ_CTX(fired ? 1 : 0, 1, ctx + " hook fired");
      FX1_CHECK_EQ_CTX(dev.in_flight, 2u, ctx + " frame stays queued");
      FX1_CHECK_EQ_CTX(fx1_isp_next_done_ex(&dev, &idx, &seq), FX1_ISP_ENODATA, ctx + " not while busy");
      mem->blocking = false;
      mem->latency = SC_ZERO_TIME;
      while ((rd(FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK) &&
             sc_time_stamp() < lim + sc_time(20, SC_MS)) {
         wait(1, SC_US);
      }
      FX1_CHECK_EQ_CTX(fx1_isp_next_done_ex(&dev, &idx, &seq), FX1_ISP_OK, ctx + " collected after the recovery");
      FX1_CHECK_EQ_CTX(idx == 0 && seq == 0 && output(0, b) == e_expected[0] ? 1 : 0, 1, ctx + " frame 0");
      FX1_CHECK_EQ_CTX(rd(FX1_ISP_ODMA_BUF_DONE_OFFSET), 0u, ctx + " acknowledged");
      FX1_CHECK_EQ_CTX(fx1_isp_next_lost(&dev, nullptr, nullptr), FX1_ISP_OK, ctx + " frame 1 lost");
      queue_input(3, b, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_next_done_ex(&dev, &idx, &seq), FX1_ISP_ENODATA, ctx + " no stale completion");
      done.clear();
      service(done, 1, b, ctx);
      FX1_CHECK_EQ_CTX(!done.empty() && done[0] == e_expected[3] ? 1 : 0, 1, ctx + " the new frame's own output");
      fx1_isp_stop(&dev);

      // (c) A drain longer than the driver's budget (4096 polls of 16 clocks).
      for (const int variant : {0, 1, 2}) {
         const bool use_stop = variant == 1;
         const bool settled_between = variant == 2;  // a settling call completes the pending reset
         ctx = std::string("bookkeeping (c) slow drain, ") + (use_stop ? "stop" : "dma_reset") +
               (settled_between ? ", settled in between" : "");
         b = start_error_case(dir);
         mem->blocking = true;
         mem->latency = sc_time(1, SC_MS);  // > 4096 x 16 x 2 ns
         queue_input(0, b, ctx);
         while (mem->active_idma == 0 && sc_time_stamp() < lim + sc_time(40, SC_MS)) {
            wait(100, SC_NS);
         }
         const std::uint32_t s0 = rd(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
         const int rc = use_stop ? fx1_isp_stop(&dev) : fx1_isp_dma_reset(&dev);
         FX1_CHECK_EQ_CTX(rc, FX1_ISP_EBUSY, ctx + " timeout reported");
         FX1_CHECK_EQ_CTX(dev.in_flight, 1u, ctx + " queue kept");
         FX1_CHECK_EQ_CTX(dev.state, FX1_ISP_STATE_STREAMING, ctx + " still streaming");
         FX1_CHECK_EQ_CTX(dev.reset_pending != 0 ? 1 : 0, 1, ctx + " reset pending");
         mem->blocking = false;
         mem->latency = SC_ZERO_TIME;
         wait(2, SC_MS);  // the held transfer returns, the pulse runs
         if (settled_between) {
            (void)fx1_isp_input_ready(&dev);  // completes the pending reset (bookkeeping of a DMA reset)
            FX1_CHECK_EQ_CTX(dev.in_flight == 0 && dev.reset_pending == 0 ? 1 : 0, 1, ctx + " completed by settling");
         }
         const int rc2 = use_stop ? fx1_isp_stop(&dev) : fx1_isp_dma_reset(&dev);
         FX1_CHECK_EQ_CTX(rc2, settled_between ? 0 : 1, ctx + " completed: frames dropped");
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET), s0 + 1u, ctx + " no second request");
         FX1_CHECK_EQ_CTX(dev.in_flight == 0 && dev.reset_pending == 0 ? 1 : 0, 1, ctx + " bookkeeping after completion");
         if (use_stop) {
            FX1_CHECK_EQ_CTX(dev.state, FX1_ISP_STATE_CONFIG, ctx + " CONFIG");
         } else {
            done.clear();
            queue_input(1, b, ctx);
            service(done, 1, b, ctx);
            FX1_CHECK_EQ_CTX(!done.empty() && done[0] == e_expected[1] ? 1 : 0, 1, ctx + " stream continues");
            fx1_isp_stop(&dev);
         }
      }
   }

#ifdef FX1_HAVE_FW_FIXTURE
   // Review R5: the CLI's `isp selftest profile` flow (freertos/cmd_isp.c,
   // enter_profile) replayed with the firmware's own basic fixture (Gamma
   // on): legacy profile, profile mode, INIT, the curve into bank 0, SEAL,
   // APPLY; 8 frames whose NV12 CRC-32 must be the fixture's and whose
   // outputs carry profile ID 1.
   static std::uint32_t crc32(std::uint32_t crc, const std::uint8_t *p, std::size_t n) {
      for (std::size_t i = 0; i < n; ++i) {
         crc ^= p[i];
         for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
         }
      }
      return crc;
   }
   static std::uint32_t fw_raw_sample(std::uint32_t frame, std::uint32_t x, std::uint32_t y) {  // isp_common.c
      std::uint32_t v = 400u + 160u * frame;
      v += (x * 37u + y * 91u + frame * 53u) % 512u;
      v += ((x ^ y) & 7u) * 24u;
      if ((x + 2u * frame) % 16u < 3u) v += 900u;
      if ((x * 7u + y * 13u + frame) % 211u == 0u) v = 4095u;
      return v > 4095u ? 4095u : v;
   }
   void test_fw_profile_selftest_replay() {
      const std::string ctx = "fw isp selftest profile replay";
      reset_and_init();
      FX1_CHECK_EQ_CTX(fx1_isp_profile_mode(&dev, 0), FX1_ISP_OK, ctx);
      FX1_CHECK_EQ_CTX(fx1_isp_set_geometry(&dev, ISP_FX_WIDTH, ISP_FX_HEIGHT, ISP_FX_BAYER), FX1_ISP_OK, ctx);
      const std::size_t n = sizeof(isp_fx_profile_basic) / sizeof(isp_fx_profile_basic[0]);
      fx1_isp_apply(&dev, isp_fx_profile_basic, n);
      std::uint16_t lut[256] = {};
      unsigned k = 0;
      for (std::size_t i = 0; i < n; ++i) {
         if (isp_fx_profile_basic[i].offset == FX1_ISP_GAMMA_LUT_DATA_OFFSET && k < 256u) {
            lut[k++] = static_cast<std::uint16_t>(isp_fx_profile_basic[i].value);
         }
      }
      FX1_CHECK_EQ_CTX(k, 256u, ctx + " the basic fixture loads a Gamma curve");
      std::uint32_t err = 0;
      FX1_CHECK_EQ_CTX(fx1_isp_profile_mode(&dev, 1), FX1_ISP_OK, ctx + " profile mode");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_begin(&dev, 0, &err), FX1_ISP_OK, ctx + " INIT");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_load_gamma(&dev, 0, lut, &err), FX1_ISP_OK, ctx + " Gamma into bank 0");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_seal(&dev, 0, 1, &err), FX1_ISP_OK, ctx + " SEAL (err " + fx1_test::hex(err) + ")");
      FX1_CHECK_EQ_CTX(fx1_isp_profile_apply(&dev, 0, 0, 0, &err), FX1_ISP_OK, ctx + " APPLY");
      const fx1_isp_buffers b = buffer_layout(align16(2 * ISP_FX_WIDTH), align16(ISP_FX_WIDTH));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      for (std::uint32_t f = 0; f < ISP_FX_FRAMES; ++f) {
         const unsigned slot = fx1_isp_next_input(&dev);
         for (std::uint32_t y = 0; y < ISP_FX_HEIGHT; ++y) {
            for (std::uint32_t x = 0; x < ISP_FX_WIDTH; ++x) {
               mem->put16(b.in[slot] + std::uint64_t{y} * b.in_stride + 2u * x,
                          static_cast<std::uint16_t>(fw_raw_sample(f, x, y)));
            }
         }
         FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
         std::vector<std::vector<std::uint8_t>> done;
         service(done, 1, b, ctx);
         const std::uint32_t crc = crc32(0xFFFFFFFFu, done.empty() ? nullptr : done[0].data(),
                                         done.empty() ? 0 : done[0].size()) ^ 0xFFFFFFFFu;
         FX1_CHECK_EQ_CTX(crc, isp_fx_crc_basic[f], ctx + " frame " + std::to_string(f) + " CRC");
         std::uint32_t prof = 0;
         fx1_isp_output_meta(&dev, 0, f % FX1_ISP_NUM_BUFFERS, nullptr, nullptr, &prof);  // rotation from 0
         FX1_CHECK_EQ_CTX(prof, 1u, ctx + " profile ID");
      }
      fx1_isp_stop(&dev);
      FX1_CHECK_EQ_CTX(fx1_isp_profile_mode(&dev, 0), FX1_ISP_OK, ctx + " back to legacy");
   }
#endif

   // Review V107-10 completion R1: a legacy Gamma curve loaded in segments,
   // out of address order, with an entry rewritten after it was complete, is
   // carried into the profile bank as the hardware holds it (read back), so
   // the profile-mode output equals the legacy one.
   // over = true: no reset, the profile goes through fx1_isp_apply on top of
   // whatever the previous run configured.
   std::vector<std::uint8_t> run_raw_profile(const fs::path &dir, const std::vector<fx1_isp_reg_write> &writes,
                                             bool profile, const std::string &ctx, bool over = false) {
      std::uint32_t in_w = 0, in_h = 0;
      {
         std::ifstream mf(dir / "meta.txt");
         mf >> in_w >> in_h;
      }
      if (over) {
         FX1_CHECK_EQ_CTX(fx1_isp_apply(&dev, writes.data(), writes.size()), FX1_ISP_OK, ctx + " apply");
      } else {
         reset_and_init();
         for (const fx1_isp_reg_write &w : writes) {
            hal_write(this, w.offset, w.value);
         }
      }
      if (profile) {
         std::uint32_t err = 0;
         FX1_CHECK_EQ_CTX(fx1_isp_profile_from_legacy(&dev, 0, writes.data(), writes.size(), 0x33u, &err), FX1_ISP_OK,
                          ctx + " profile built (TUNE_ERROR " + fx1_test::hex(err) + ")");
      }
      const fx1_isp_buffers b = buffer_layout(align16(2 * in_w), align16(rd(FX1_ISP_RESIZER_OUT_W_OFFSET)));
      FX1_CHECK_EQ_CTX(fx1_isp_start(&dev, &b), FX1_ISP_OK, ctx + " start");
      load_input(fx1_isp_next_input(&dev), read_file(dir / "input.bin"), in_w, in_h, align16(2 * in_w), b);
      FX1_CHECK_EQ_CTX(fx1_isp_queue_frame(&dev), FX1_ISP_OK, ctx + " queue");
      seq_input.clear();
      std::vector<std::vector<std::uint8_t>> done;
      service(done, 1, b, ctx);
      fx1_isp_stop(&dev);
      if (profile) {
         FX1_CHECK_EQ_CTX(fx1_isp_profile_mode(&dev, 0), FX1_ISP_OK, ctx + " back to legacy");
      }
      return done.empty() ? std::vector<std::uint8_t>{} : done[0];
   }
   void test_profile_gamma_order() {
      const fs::path dir = root / "grbg_gamma22_128x72";
      std::vector<fx1_isp_reg_write> head, lut;
      for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
         const csr::reg_desc *d = reg(l.reg);
         if (!d || l.reg == "GAMMA_LUT_ADDR" || l.reg == "GAMMA_CTRL") {
            continue;
         }
         (l.reg == "GAMMA_LUT_DATA" ? lut : head).push_back({d->offset, l.value});
      }
      FX1_CHECK_EQ(lut.size(), 256u);
      for (const unsigned rewrite : {0u, 1u}) {
         const std::string ctx = std::string("profile Gamma carried over, ") +
                                 (rewrite ? "segments 128..255 / 0..127 and entry 5 rewritten" : "16 x i out of order");
         std::vector<fx1_isp_reg_write> w = head;
         const auto value = [&](unsigned i) {
            return rewrite ? lut[i].value : 16u * i;  // the review's probe curve LUT[i] = 16 i
         };
         w.push_back({FX1_ISP_GAMMA_CTRL_OFFSET, 0});  // EN = 0 while loading
         w.push_back({FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 128});
         for (unsigned i = 128; i < 256; ++i) w.push_back({FX1_ISP_GAMMA_LUT_DATA_OFFSET, value(i)});
         w.push_back({FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 0});
         for (unsigned i = 0; i < 128; ++i) w.push_back({FX1_ISP_GAMMA_LUT_DATA_OFFSET, value(i)});
         if (rewrite) {
            w.push_back({FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 5});
            w.push_back({FX1_ISP_GAMMA_LUT_DATA_OFFSET, 0xABC});
         }
         w.push_back({FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK});
         w.push_back({FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK});
         const std::vector<std::uint8_t> legacy = run_raw_profile(dir, w, false, ctx + " legacy");
         const std::vector<std::uint8_t> prof = run_raw_profile(dir, w, true, ctx + " profile");
         FX1_CHECK_EQ_CTX(!legacy.empty() && legacy == prof ? 1 : 0, 1, ctx + ": profile output equals legacy");
         std::vector<std::uint8_t> vec = read_file(dir / "expected_y.bin");
         const std::vector<std::uint8_t> vuv = read_file(dir / "expected_uv.bin");
         vec.insert(vec.end(), vuv.begin(), vuv.end());
         FX1_CHECK_EQ_CTX(legacy != vec ? 1 : 0, 1, ctx + ": the curve differs from the vector's (the case is real)");
      }
   }

   // V107-11 platform CI: a profile applied again over its own configuration
   // (camera stop / configure / start, a second `isp run` or `isp selftest`)
   // finds Gamma / EE / GTM enabled. Its table loads must still be confirmed
   // (fx1_isp_apply disables the block first, MAS §6.14.4, §9.9.1, §6.16.4.2);
   // before the fix the second run bypassed Gamma and EE. Every run on the
   // same device equals the vector's expected image; the third is the
   // profile-mode bring-up of the same profile.
   void test_profile_reapply() {
      for (const char *name : {"grbg_gamma22_128x72", "ee_features_96x64", "gtm_manual_lut_64x32"}) {
         const fs::path dir = root / name;
         std::vector<fx1_isp_reg_write> w;
         for (const fx1_test::csr_line &l : fx1_test::read_profile(dir / "profile.csrw")) {
            if (const csr::reg_desc *d = reg(l.reg)) {
               w.push_back({d->offset, l.value});
            }
         }
         std::vector<std::uint8_t> vec = read_file(dir / "expected_y.bin");
         const std::vector<std::uint8_t> vuv = read_file(dir / "expected_uv.bin");
         vec.insert(vec.end(), vuv.begin(), vuv.end());
         reset_and_init();
         for (unsigned run = 0; run < 3; ++run) {
            const std::string ctx = std::string(name) + (run == 0   ? ", first apply"
                                                         : run == 1 ? ", applied again"
                                                                    : ", applied again, profile mode");
            const std::vector<std::uint8_t> out = run_raw_profile(dir, w, run == 2, ctx, true);
            FX1_CHECK_EQ_CTX(out == vec ? 1 : 0, 1, ctx + ": output equals the vector's");
         }
      }
      // A refused confirmation is reported (legacy mode raises no TUNE_ERROR).
      reset_and_init();
      std::vector<fx1_isp_reg_write> w = {{FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 0}};
      for (unsigned i = 0; i < 255; ++i) w.push_back({FX1_ISP_GAMMA_LUT_DATA_OFFSET, 16u * i});
      w.push_back({FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK});
      FX1_CHECK_EQ(fx1_isp_apply(&dev, w.data(), w.size()), FX1_ISP_EIO);  // 255 of 256 entries
      const fx1_isp_reg_write ee[] = {{FX1_ISP_EE_CTRL_OFFSET, 1u << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT},
                                      {FX1_ISP_EE_LUT_CTRL_OFFSET, FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK}};
      FX1_CHECK_EQ(fx1_isp_apply(&dev, ee, 2), FX1_ISP_EIO);  // resident 1 never loaded
      FX1_CHECK_EQ(fx1_isp_apply(&dev, nullptr, 0), FX1_ISP_OK);
      FX1_CHECK_EQ(fx1_isp_apply(&dev, nullptr, 1), FX1_ISP_EINVAL);
   }

   void run() {
      if (fast_csr) {
         // AWB ownership and every statistics readout with CSR accesses
         // shorter than a core clock: the driver must wait the MAS clocks.
         for (const auto &e : fs::directory_iterator(root)) {
            if (e.is_directory() && e.path().filename().string().rfind("stats_", 0) == 0) {
               run_vector(e.path());
            }
         }
         test_awb_driver();
         test_gtm_driver();  // the GTM hand-over counts frames, not CSR time (review U3-R3)
         test_dma_reset_stats();
         test_dma_reset_gtm();
         test_done_ack_race();
         sc_stop();
         return;
      }
      for (const auto &e : fs::directory_iterator(root)) {
         if (e.is_directory()) {
            run_vector(e.path());
         }
      }
      // The same vectors through the default profile-mode bring-up, where
      // that is the same configuration: one profile for the whole stream
      // (no between-frame writes, which a sealed bank refuses) and no TUNE
      // extension writes (storage in legacy mode, live in profile mode).
      for (const auto &e : fs::directory_iterator(root)) {
         if (!e.is_directory() || fs::exists(e.path() / "frames.csrw")) {
            continue;
         }
         bool tune = false;
         for (const fx1_test::csr_line &l : fx1_test::read_profile(e.path() / "profile.csrw")) {
            tune = tune || l.reg.rfind("TUNE_", 0) == 0;
         }
         if (!tune) {
            run_vector(e.path(), true);
         }
      }
      FX1_CHECK(profile_vectors >= 25);
      FX1_CHECK_EQ(profile_refused, 2u);  // stats_illegal_grids, stats_reset_grids
      std::cout << "profile-mode bring-up: " << profile_vectors << " vectors equal to legacy, " << profile_refused
                << " refused at SEAL\n";
      run_error_cases();
      test_driver_state_machine();
      test_stats_after_abort();
      test_compat_aec_newest();
      test_first_frame_abort_stats();
      test_awb_driver();
      test_ee_driver();
      test_gtm_driver();
      test_resizer_waits();
      test_metadata_driver();
      test_dma_reset_driver();
      test_dma_reset_stats();
      test_dma_reset_gtm();
      test_done_ack_race();
      test_immediate_loss();
      test_queue_after_foreign_settle();
      test_v107_api();
      test_recovery_bookkeeping();
      test_profile_gamma_order();
      test_profile_reapply();
#ifdef FX1_HAVE_FW_FIXTURE
      test_fw_profile_selftest_replay();
#endif
      run_fault_campaign();
      sc_stop();
   }
};

}  // namespace

int sc_main(int argc, char *argv[]) {
   if (argc < 2) {
      std::cerr << "usage: " << argv[0] << " <vectors dir> [--fast-csr]\n";
      return 2;
   }
   const bool fast = argc > 2 && std::string(argv[2]) == "--fast-csr";
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_params params{};
   if (fast) {  // a CSR access is a tenth of a core clock
      params.core_period = sc_time(10, SC_NS);
      params.csr_latency = sc_time(1, SC_NS);
   }
   fx1_isp_tlm dut("isp", params);
   fx1_test::test_memory mem("mem", mem_base, mem_size);
   bench tb("tb");
   tb.root = argv[1];
   tb.fast_csr = fast;
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
   if (fast) {
      FX1_CHECK(tb.vectors >= 6);
      std::cout << "driver (fast CSR): " << tb.vectors << " statistics vectors\n";
      return fx1_test::summary("fx1_isp_test_driver_tlm --fast-csr");
   }
   FX1_CHECK(tb.vectors >= 32);
   std::cout << "driver: " << tb.vectors << " vectors;";
   for (const auto &kv : tb.helpers) {
      std::cout << " " << kv.first << "=" << kv.second;
      FX1_CHECK(kv.second > 0);
   }
   std::cout << "\n";
   FX1_CHECK_EQ(tb.helpers.size(), 11);  // every helper kind exercised (incl. the sub-minimum geometry refusals)
   return fx1_test::summary("fx1_isp_test_driver_tlm");
}
