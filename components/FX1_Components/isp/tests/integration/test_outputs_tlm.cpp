// SPDX-License-Identifier: Apache-2.0
// The three output channels over TLM (MAS v1.07 §6.25.3.8, §9.3, §9.5.1;
// plan V107-8 gate): every enabled-channel mask, lossless back-pressure from
// one channel without a free buffer, independent rotations sharing the ingress
// frame ID, the output interrupt group (masked events latch), the CH2 RGB888
// stride rule at the claim, and FRAME_DONE at retirement. The expected bytes
// come from the streaming kernels (isp_pipeline) on the same configuration;
// those are compared with the Python reference by unit.pipeline_vectors.

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "control/control_unit.h"
#include "fx1_check.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_tlm.h"
#include "pipeline/isp_pipeline.h"
#include "test_memory.h"

using namespace sc_core;
using cdc::components::fx1_isp::control_unit;
using cdc::components::fx1_isp::fx1_isp_params;
using cdc::components::fx1_isp::fx1_isp_tlm;
using cdc::components::fx1_isp::num_channels;
namespace pipe = cdc::components::fx1_isp::pipe;

namespace {

constexpr std::uint64_t mem_base = 0x8000'0000ull;
constexpr std::size_t mem_size = 16u << 20;
constexpr std::uint32_t W = 64, H = 48;
constexpr std::uint8_t guard = 0xA5;

// Per channel: output size, plane bases (buffer k at + k * 0x40000) and strides.
struct chan {
   std::uint32_t w, h, stride;
   std::uint64_t y, uv;  // uv = 0: one RGB888 plane
   std::uint32_t free, done, count, y_stride, uv_stride, y_addr0, uv_addr0, frame0, tag0;
};
const std::array<chan, num_channels> chans = {{
   {W, H, W, mem_base + 0x100000ull, mem_base + 0x200000ull, FX1_ISP_ODMA_BUF_FREE_OFFSET,
    FX1_ISP_ODMA_BUF_DONE_OFFSET, FX1_ISP_ODMA_FRAME_COUNT_OFFSET, FX1_ISP_ODMA_Y_STRIDE_OFFSET,
    FX1_ISP_ODMA_UV_STRIDE_OFFSET, FX1_ISP_ODMA_Y_ADDR_L0_OFFSET, FX1_ISP_ODMA_UV_ADDR_L0_OFFSET,
    FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET, FX1_ISP_ODMA_BUF_TAG0_OFFSET},
   {32, 24, 32, mem_base + 0x300000ull, mem_base + 0x400000ull, FX1_ISP_CH1_ODMA_BUF_FREE_OFFSET,
    FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET, FX1_ISP_CH1_ODMA_FRAME_COUNT_OFFSET, FX1_ISP_CH1_ODMA_Y_STRIDE_OFFSET,
    FX1_ISP_CH1_ODMA_UV_STRIDE_OFFSET, FX1_ISP_CH1_ODMA_Y_ADDR_L0_OFFSET, FX1_ISP_CH1_ODMA_UV_ADDR_L0_OFFSET,
    FX1_ISP_TUNE_CH1_BUF0_FRAME_OFFSET, FX1_ISP_CH1_ODMA_BUF_TAG0_OFFSET},
   {32, 32, 96, mem_base + 0x500000ull, 0, FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET, FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET,
    FX1_ISP_CH2_ODMA_FRAME_COUNT_OFFSET, FX1_ISP_CH2_ODMA_Y_STRIDE_OFFSET, 0, FX1_ISP_CH2_ODMA_Y_ADDR_L0_OFFSET, 0,
    FX1_ISP_TUNE_CH2_BUF0_FRAME_OFFSET, FX1_ISP_CH2_ODMA_BUF_TAG0_OFFSET},
}};
std::uint64_t plane(std::uint64_t base, unsigned k) { return base + 0x40000ull * k; }

// The configuration: CH0 full image, CH1 crop 32 x 24 at (8, 4), CH2 crop
// 32 x 32 at (16, 8), all FullRes; CSC BT.709 for a non-default CH2 matrix.
const std::vector<std::pair<std::uint32_t, std::uint32_t>> config = {
   {FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, W},           {FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, H},
   {FX1_ISP_CSC_CTRL_OFFSET, 1},                     {FX1_ISP_CH1_CROP_ORIGIN_OFFSET, 8 | (4u << 16)},
   {FX1_ISP_CH1_CROP_SIZE_OFFSET, 32 | (24u << 16)}, {FX1_ISP_CH1_CROP_CTRL_OFFSET, 1},
   {FX1_ISP_CH2_CROP_ORIGIN_OFFSET, 16 | (8u << 16)}, {FX1_ISP_CH2_CROP_SIZE_OFFSET, 32 | (32u << 16)},
   {FX1_ISP_CH2_CROP_CTRL_OFFSET, 1},
};

std::uint16_t sample(unsigned tag, std::uint32_t x, std::uint32_t y) {
   return static_cast<std::uint16_t>((x * 37u + y * 91u + tag * 513u) & 0xFFFu);
}

// Expected bytes of each channel for input `tag`: the streaming kernels on a
// control unit with the same configuration.
std::array<std::array<std::vector<std::uint8_t>, 2>, num_channels> expected(unsigned tag) {
   control_unit cu;
   std::uint64_t t = 1;
   for (const auto &w : config) {
      cu.csr_write(w.first, w.second, 0xFFFFFFFFu, t++);
   }
   cu.csr_write(FX1_ISP_OUTPUT_ENABLE_OFFSET, 7, 0xFFFFFFFFu, t++);
   cu.accepted_sof(1000);
   const pipe::pipeline_config cfg = pipe::snapshot_config(cu);
   std::array<std::array<std::vector<std::uint8_t>, 2>, num_channels> out;
   std::array<pipe::isp_pipeline::channel_sinks, num_channels> sinks;
   for (unsigned c = 0; c < num_channels; ++c) {
      sinks[c].y = [&out, c](std::vector<std::uint8_t> &&r) { out[c][0].insert(out[c][0].end(), r.begin(), r.end()); };
      if (c < 2) {
         sinks[c].uv = [&out, c](std::vector<std::uint8_t> &&r) { out[c][1].insert(out[c][1].end(), r.begin(), r.end()); };
      }
   }
   pipe::isp_pipeline p;
   p.begin(cfg, W, H, sinks);
   std::vector<std::uint16_t> line(W);
   for (std::uint32_t y = 0; y < H; ++y) {
      for (std::uint32_t x = 0; x < W; ++x) {
         line[x] = sample(tag, x, y);
      }
      p.push(line.data());
   }
   p.finish();
   return out;
}

struct bench : sc_module {
   tlm_utils::simple_initiator_socket<bench> socket;
   sc_out<bool> rst_n;
   sc_in<bool> irq;
   fx1_isp_tlm *dut = nullptr;
   fx1_test::test_memory *mem = nullptr;

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

   void reset_and_program(std::uint32_t outputs) {
      rst_n.write(false);
      wait(10, SC_NS);
      rst_n.write(true);
      wait(10, SC_NS);
      for (const auto &w : config) {
         wr(w.first, w.second);
      }
      wr(FX1_ISP_OUTPUT_ENABLE_OFFSET, outputs);
      wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
      wait(dut->params().core_period * 80.0);
      for (unsigned k = 0; k < 4; ++k) {
         wr(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, static_cast<std::uint32_t>(mem_base + 0x10000ull * k));
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
         for (unsigned k = 0; k < 4; ++k) {
            mem->fill(plane(chans[c].y, k), std::size_t{chans[c].stride} * chans[c].h, guard);
            if (chans[c].uv) {
               mem->fill(plane(chans[c].uv, k), std::size_t{chans[c].stride} * chans[c].h / 2u, guard);
            }
         }
      }
      wr(FX1_ISP_DMA_CTRL_OFFSET, 0x3F03);
   }
   void load(unsigned k, unsigned tag) {
      for (std::uint32_t y = 0; y < H; ++y) {
         for (std::uint32_t x = 0; x < W; ++x) {
            mem->put16(mem_base + 0x10000ull * k + 2u * (y * W + x), sample(tag, x, y));
         }
      }
      wr(FX1_ISP_IDMA_BUF_TAG0_OFFSET + 4u * k, 0x7A600 + tag);
   }
   // Mismatches of channel c's buffer k against the kernel bytes (padding
   // beyond the row must stay untouched).
   std::size_t compare(unsigned c, unsigned k, unsigned tag) {
      const auto want = expected(tag)[c];
      const chan &ch = chans[c];
      const std::uint32_t row = c == 2 ? 3 * ch.w : ch.w;
      std::size_t bad = 0;
      for (unsigned p = 0; p < (ch.uv ? 2u : 1u); ++p) {
         const std::uint32_t rows = p ? ch.h / 2u : ch.h;
         FX1_CHECK_EQ_CTX(want[p].size(), std::size_t{rows} * row, "kernel plane size");
         for (std::uint32_t y = 0; y < rows; ++y) {
            const std::uint8_t *got = mem->at(plane(p ? ch.uv : ch.y, k) + std::uint64_t{y} * ch.stride);
            for (std::uint32_t x = 0; x < ch.stride; ++x) {
               bad += got[x] != (x < row ? want[p][std::size_t{y} * row + x] : guard);
            }
         }
      }
      return bad;
   }

   // Every mask: enabled channels publish DONE with their bytes, the others
   // are untouched; FRAME_DONE once the frame retired at every output.
   void test_masks() {
      for (std::uint32_t mask = 1; mask < 8; ++mask) {
         const std::string ctx = "mask " + std::to_string(mask);
         reset_and_program(mask);
         for (unsigned c = 0; c < num_channels; ++c) {
            wr(chans[c].free, 0x1);
         }
         load(0, mask);
         wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
         FX1_CHECK_EQ_CTX(idle() ? 1 : 0, 1, ctx + " retired");
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK,
                          FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK, ctx + " FRAME_DONE");
         FX1_CHECK_EQ_CTX(rd(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET), mask, ctx + " OUTPUT_IRQ_STATUS.DONE");
         for (unsigned c = 0; c < num_channels; ++c) {
            const std::string cc = ctx + " CH" + std::to_string(c);
            const bool on = mask & (1u << c);
            FX1_CHECK_EQ_CTX(rd(chans[c].done), on ? 1u : 0u, cc + " DONE");
            FX1_CHECK_EQ_CTX(rd(chans[c].count), on ? 1u : 0u, cc + " FRAME_COUNT");
            FX1_CHECK_EQ_CTX(rd(chans[c].free), on ? 0u : 1u, cc + " FREE");
            if (on) {
               FX1_CHECK_EQ_CTX(compare(c, 0, mask), 0, cc + " bytes");
               FX1_CHECK_EQ_CTX(rd(chans[c].frame0), 1u, cc + " ingress frame ID");
               FX1_CHECK_EQ_CTX(rd(chans[c].tag0), 0x7A600 + mask, cc + " tag");
            } else {
               FX1_CHECK_EQ_CTX(*mem->at(chans[c].y), guard, cc + " untouched");
            }
         }
      }
   }

   // CH2 without a free buffer back-pressures the shared pipeline: CH0 cannot
   // finish, nothing is lost; freeing CH2 completes both with their bytes.
   void test_backpressure() {
      reset_and_program(5);
      wr(chans[0].free, 0x1);
      load(0, 9);
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      wait(1, SC_MS);
      FX1_CHECK_EQ(rd(chans[0].count), 0u);
      FX1_CHECK_EQ(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT, FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT);
      FX1_CHECK_EQ(rd(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK, FX1_ISP_COMMON_STATUS_BUSY_MASK);
      wr(chans[2].free, 0x1);
      FX1_CHECK(idle());
      FX1_CHECK_EQ(rd(chans[0].count), 1u);
      FX1_CHECK_EQ(rd(chans[2].count), 1u);
      FX1_CHECK_EQ(compare(0, 0, 9), 0);
      FX1_CHECK_EQ(compare(2, 0, 9), 0);
      // Sticky W1C, held while the stall lasted: clearable once it is over.
      wr(FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT);
      FX1_CHECK_EQ(rd(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT, 0u);
   }

   // Different rotations: frame A on CH0 only, then (idle) frame B on CH0 and
   // CH1. B lands in CH0 buffer 1 and CH1 buffer 0 with one ingress frame ID.
   void test_rotations() {
      reset_and_program(1);
      wr(chans[0].free, 0x3);
      wr(chans[1].free, 0x1);
      load(0, 3);
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(idle());
      wr(FX1_ISP_DMA_CTRL_OFFSET, 0x3F02);  // input stopped: OUTPUT_ENABLE is IDLE class
      FX1_CHECK(idle());
      wr(FX1_ISP_OUTPUT_ENABLE_OFFSET, 3);
      wait(dut->params().core_period * 80.0);
      wr(FX1_ISP_DMA_CTRL_OFFSET, 0x3F03);
      load(1, 4);
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x2);
      FX1_CHECK(idle());
      FX1_CHECK_EQ(rd(chans[0].done), 0x3u);
      FX1_CHECK_EQ(rd(chans[1].done), 0x1u);
      FX1_CHECK_EQ(rd(chans[0].frame0 + 4), rd(chans[1].frame0));  // one ingress frame ID
      FX1_CHECK_EQ(rd(chans[1].frame0), 2u);
      FX1_CHECK_EQ(rd(chans[0].tag0 + 4), rd(chans[1].tag0));
      FX1_CHECK_EQ(compare(0, 1, 4), 0);
      FX1_CHECK_EQ(compare(1, 0, 4), 0);
      FX1_CHECK_EQ(dut->debug_odma_head(0), 2u);
      FX1_CHECK_EQ(dut->debug_odma_head(1), 1u);
   }

   // The output group (MAS Table 9-188): a masked DONE still latches; the
   // pin follows OUTPUT_IRQ_EN; W1C clears per bit.
   void test_output_irq() {
      reset_and_program(6);
      wr(chans[1].free, 0x1);
      wr(chans[2].free, 0x1);
      load(0, 5);
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(idle());
      FX1_CHECK_EQ(rd(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET), 0x6u);
      FX1_CHECK(!irq.read());  // every group masked
      wr(FX1_ISP_OUTPUT_IRQ_EN_OFFSET, 0x4);
      wait(10, SC_NS);
      FX1_CHECK(irq.read());
      wr(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET, 0x2);  // CH1 only: CH2 keeps the pin high
      wait(10, SC_NS);
      FX1_CHECK(irq.read());
      FX1_CHECK_EQ(rd(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET), 0x4u);
      wr(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET, 0x4);
      wait(10, SC_NS);
      FX1_CHECK(!irq.read());
   }

   // CH2 stride below 3W (Table 6-153): the claim rejects the buffer (ALIGN,
   // CFG_ERROR, FREE cleared) and the frame waits; corrected and freed again,
   // it completes.
   void test_ch2_stride() {
      reset_and_program(4);
      wr(chans[2].y_stride, 64);  // < 96 = 3 x 32
      wr(chans[2].free, 0x1);
      load(0, 6);
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT) != 0; }));
      FX1_CHECK_EQ(rd(FX1_ISP_DMA_IRQ_STAT_OFFSET) & FX1_ISP_DMA_IRQ_STAT_IRQ_CFG_ERROR_BIT,
                   FX1_ISP_DMA_IRQ_STAT_IRQ_CFG_ERROR_BIT);
      FX1_CHECK_EQ(rd(chans[2].free), 0u);
      FX1_CHECK_EQ(rd(chans[2].count), 0u);
      FX1_CHECK_EQ(*mem->at(chans[2].y), guard);
      wr(chans[2].y_stride, 96);
      wr(FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT);
      wr(chans[2].free, 0x1);
      FX1_CHECK(idle());
      FX1_CHECK_EQ(rd(chans[2].count), 1u);
      FX1_CHECK_EQ(compare(2, 0, 6), 0);
   }

   // Published metadata of every channel's buffer k (MAS v1.07 Table 6-159,
   // §9.9.9), addressed as base + 4 * k: capture TS, SOF/EOF times, the
   // ingress frame ID and the profile ID (0 in legacy mode); the CH1 / CH2
   // Resizer busy bits follow the frame's mask.
   void test_metadata() {
      reset_and_program(5);
      wr(chans[0].free, 0x1);
      load(0, 7);
      wr(FX1_ISP_IDMA_BUF_TS0_OFFSET, 0x1234);
      wr(FX1_ISP_IDMA_BUF_VALID_OFFSET, 0x1);
      FX1_CHECK(wait_until([&] { return (peek(FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT) != 0; }));
      FX1_CHECK_EQ(rd(FX1_ISP_CH2_RESIZER_STATUS_OFFSET) & 1u, 1u);  // stalled: CH2 holds pixels
      FX1_CHECK_EQ(rd(FX1_ISP_CH1_RESIZER_STATUS_OFFSET) & 1u, 0u);  // not enabled by the frame
      wr(chans[2].free, 0x1);
      FX1_CHECK(idle());
      FX1_CHECK_EQ(rd(FX1_ISP_CH2_RESIZER_STATUS_OFFSET) & 1u, 0u);
      const unsigned k = 0;
      FX1_CHECK_EQ(rd(FX1_ISP_ODMA_BUF_TS0_OFFSET + 4u * k), 0x1234u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH2_ODMA_BUF_TS0_OFFSET + 4u * k), 0x1234u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH2_ODMA_BUF_TAG0_OFFSET + 4u * k), 0x7A607u);
      FX1_CHECK(rd(FX1_ISP_CH2_ODMA_BUF_EOF_TS0_OFFSET + 4u * k) > rd(FX1_ISP_CH2_ODMA_BUF_SOF_TS0_OFFSET + 4u * k));
      FX1_CHECK_EQ(rd(FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET + 4u * k), rd(FX1_ISP_TUNE_CH2_BUF0_FRAME_OFFSET + 4u * k));
      FX1_CHECK_EQ(rd(FX1_ISP_TUNE_CH0_BUF0_PROFILE_OFFSET + 4u * k), 0u);
      FX1_CHECK_EQ(rd(FX1_ISP_TUNE_CH2_BUF0_PROFILE_OFFSET + 4u * k), 0u);
      FX1_CHECK_EQ(rd(FX1_ISP_TUNE_CH1_BUF0_FRAME_OFFSET + 4u * k), 0u);  // CH1 published nothing
      FX1_CHECK_EQ(rd(FX1_ISP_TUNE_CH1_BUF0_PROFILE_OFFSET + 4u * k), 0u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH1_ODMA_BUF_TS0_OFFSET + 4u * k), 0u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH1_ODMA_BUF_SOF_TS0_OFFSET + 4u * k), 0u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH1_ODMA_BUF_EOF_TS0_OFFSET + 4u * k), 0u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH1_ODMA_BUF_TAG0_OFFSET + 4u * k), 0u);
      // The address registers keep all 32 bits of each word.
      wr(FX1_ISP_CH1_ODMA_Y_ADDR_L0_OFFSET + 16u * 3, 0xDEADBEE0u);
      wr(FX1_ISP_CH1_ODMA_UV_ADDR_L0_OFFSET + 16u * 3 + 4, 0x12u);
      wr(FX1_ISP_CH2_ODMA_Y_ADDR_L0_OFFSET + 16u * 2 + 4, 0x34u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH1_ODMA_Y_ADDR_L0_OFFSET + 16u * 3), 0xDEADBEE0u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH1_ODMA_UV_ADDR_L0_OFFSET + 16u * 3 + 4), 0x12u);
      FX1_CHECK_EQ(rd(FX1_ISP_CH2_ODMA_Y_ADDR_L0_OFFSET + 16u * 2 + 4), 0x34u);
   }

   void run() {
      test_metadata();
      test_masks();
      test_backpressure();
      test_rotations();
      test_output_irq();
      test_ch2_stride();
      sc_stop();
   }
};

}  // namespace

int sc_main(int, char *[]) {
   sc_signal<bool> rst_n("rst_n");
   sc_signal<bool> irq("irq");
   fx1_isp_tlm dut("isp", fx1_isp_params{});
   fx1_test::test_memory mem("mem", mem_base, mem_size);
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
   sc_start();
   return fx1_test::summary("fx1_isp_test_outputs_tlm");
}
