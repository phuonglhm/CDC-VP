// SPDX-License-Identifier: Apache-2.0
#include "engine/engines.h"

#include "fx1_isp/fx1_isp_csr.h"
#include "pipeline/stub_pipeline.h"

namespace cdc::components::fx1_isp {

using sc_core::sc_time;

namespace {

constexpr std::uint32_t dma_irq_idma_start = FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_START_BIT;
constexpr std::uint32_t dma_irq_idma_done = FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_DONE_BIT;
constexpr std::uint32_t dma_irq_idma_underrun = FX1_ISP_DMA_IRQ_STAT_IRQ_IDMA_UNDERRUN_BIT;
constexpr std::uint32_t dma_irq_odma_done = FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_DONE_BIT;
constexpr std::uint32_t dma_irq_odma_overflow = FX1_ISP_DMA_IRQ_STAT_IRQ_ODMA_OVERFLOW_BIT;
constexpr std::uint32_t dma_irq_axi_error = FX1_ISP_DMA_IRQ_STAT_IRQ_AXI_ERROR_BIT;

constexpr std::uint32_t err_align = FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT;
constexpr std::uint32_t err_underrun = FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT;
constexpr std::uint32_t err_overflow = FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT;
constexpr std::uint32_t err_idma_axi = FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT;
constexpr std::uint32_t err_odma_axi = FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT;

std::uint32_t max_beats(std::uint32_t dma_ctrl) {
   return ((dma_ctrl & FX1_ISP_DMA_CTRL_MAX_BURST_M1_MASK) >> FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) + 1u;
}

std::uint64_t reg64(const engine_context &ctx, std::uint32_t lo_offset) {
   return ctx.reg(lo_offset) | (std::uint64_t{ctx.reg(lo_offset + 4u)} << 32);
}

}  // namespace

// ---- engine_context --------------------------------------------------------

void engine_context::wait_out_of_reset(std::uint64_t my_epoch) {
   for (;;) {
      check(my_epoch);
      if (in_reset) {
         wait_event(kick, my_epoch);
         continue;
      }
      const std::uint64_t now = cycle();
      if (ctl.soft_reset_active(now)) {
         wait_time(params.core_period * static_cast<double>(ctl.soft_reset_end_cycle() - now), my_epoch);
         continue;
      }
      return;
   }
}

void engine_context::set(std::uint32_t offset, std::uint32_t mask) {
   ctl.hw_set(offset, mask, cycle());
   state_changed();
}

void engine_context::clear(std::uint32_t offset, std::uint32_t mask) {
   ctl.hw_clear(offset, mask);
   state_changed();
}

void engine_context::write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value) {
   ctl.hw_write(offset, mask, value);
   state_changed();
}

void engine_context::hold(std::uint32_t offset, std::uint32_t mask, bool level) {
   ctl.hw_hold(offset, mask, level, cycle());
   state_changed();
}

void engine_context::accept_frame() {
   ++frames_in_flight;
   set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_BUSY_MASK);
}

void engine_context::retire_frame() {
   sc_assert(frames_in_flight > 0);
   if (--frames_in_flight == 0) {
      clear(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_BUSY_MASK);
   }
}

void engine_context::reset_engines() {
   ++epoch;
   idma_head = 0;
   odma_head = 0;
   frames_in_flight = 0;
   for (line_fifo *f : fifos) {
      f->clear();
   }
   reset_ev.notify(sc_core::SC_ZERO_TIME);
   kick.notify(sc_core::SC_ZERO_TIME);
}

// ---- line_fifo ---------------------------------------------------------------

void line_fifo::push(line l, std::uint64_t my_epoch) {
   while (q_.size() >= depth_) {
      ctx_.wait_event(space_ev_, my_epoch);
   }
   q_.push_back(std::move(l));
   data_ev_.notify(sc_core::SC_ZERO_TIME);
}

line line_fifo::pop(std::uint64_t my_epoch) {
   while (q_.empty()) {
      ctx_.wait_event(data_ev_, my_epoch);
   }
   line l = std::move(q_.front());
   q_.pop_front();
   space_ev_.notify(sc_core::SC_ZERO_TIME);
   return l;
}

void line_fifo::wait_empty(std::uint64_t my_epoch) {
   while (!q_.empty()) {
      ctx_.wait_event(space_ev_, my_epoch);
   }
}

void line_fifo::clear() {
   q_.clear();
   space_ev_.notify(sc_core::SC_ZERO_TIME);
}

// ---- memory access -----------------------------------------------------------

bool memory_access(tlm::tlm_initiator_socket<> &socket, tlm::tlm_command cmd, std::uint64_t addr,
                   std::uint8_t *data, std::uint32_t bytes, engine_context &ctx, std::uint64_t my_epoch) {
   tlm::tlm_generic_payload t;
   t.set_command(cmd);
   t.set_address(dma::port_address(addr, ctx.params.axi_addr_bits));
   t.set_data_ptr(data);
   t.set_data_length(bytes);
   t.set_streaming_width(bytes);
   t.set_byte_enable_ptr(nullptr);
   t.set_byte_enable_length(0);
   t.set_dmi_allowed(false);
   t.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
   sc_time delay = sc_core::SC_ZERO_TIME;
   socket->b_transport(t, delay);
   // The response is observed at its annotated time; a reset in the meantime
   // makes the result irrelevant (HAS §7.1.8: the bus is left clean).
   ctx.wait_time(delay, my_epoch);
   return t.is_response_ok();
}

// ---- input DMA (HAS §6.25.8) -------------------------------------------------

void idma_engine::run() {
   for (;;) {
      const std::uint64_t e = ctx_.epoch;
      try {
         for (;;) {
            wait_for_buffer(e);
            frame(e);
         }
      } catch (const reset_abort &) {
         // State was reset by the reset source; start over in the new epoch.
      }
   }
}

void idma_engine::set_underrun(bool level) {
   const bool held = (ctx_.ctl.regs().held(FX1_ISP_DMA_ERR_OFFSET) & err_underrun) != 0;
   if (level && !held) {
      ctx_.hold(FX1_ISP_DMA_ERR_OFFSET, err_underrun, true);
      ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_idma_underrun);  // rising edge only (CSR-06)
   } else if (!level && held) {
      ctx_.hold(FX1_ISP_DMA_ERR_OFFSET, err_underrun, false);
   }
}

void idma_engine::wait_for_buffer(std::uint64_t e) {
   for (;;) {
      ctx_.wait_out_of_reset(e);
      const std::uint32_t ctrl = ctx_.reg(FX1_ISP_DMA_CTRL_OFFSET);
      const bool enabled = (ctrl & FX1_ISP_DMA_CTRL_IDMA_EN_MASK) != 0;
      const unsigned head = ctx_.idma_head;
      const bool valid = (ctx_.reg(FX1_ISP_IDMA_BUF_VALID_OFFSET) >> head) & 1u;
      // Underrun: enabled, idle between frames, head buffer not valid (HAS §6.25.8.3).
      set_underrun(enabled && !valid);
      if (enabled && valid) {
         // Arm: capture the frame-coherent working set (HAS §6.25.8.2 step 1).
         dma::idma_setup s{};
         s.base = dma::port_address(reg64(ctx_, FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * head),
                                    ctx_.params.axi_addr_bits);
         s.stride = ctx_.reg(FX1_ISP_IDMA_STRIDE_OFFSET);
         s.width = ctx_.reg(FX1_ISP_COMMON_FRAME_WIDTH_OFFSET) & FX1_ISP_COMMON_FRAME_WIDTH_H_ACTIVE_MASK;
         s.height = ctx_.reg(FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET) & FX1_ISP_COMMON_FRAME_HEIGHT_V_ACTIVE_MASK;
         if (dma::idma_setup_ok(s, ctx_.params.axi_data_bytes)) {
            setup_ = s;
            max_beats_ = max_beats(ctrl);
            return;
         }
         // Illegal configuration: the unit does not start (HAS Table 6-100).
         ctx_.set(FX1_ISP_DMA_ERR_OFFSET, err_align);
      }
      ctx_.wait_event(ctx_.kick, e);
   }
}

void idma_engine::frame(std::uint64_t e) {
   const unsigned head = ctx_.idma_head;
   const std::uint32_t bit = 1u << head;
   const std::uint32_t w = setup_.width;
   const std::uint32_t h = setup_.height;
   ++frames_;
   ctx_.set(FX1_ISP_DMA_STAT_OFFSET, FX1_ISP_DMA_STAT_IDMA_BUSY_MASK);
   ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_idma_start);

   std::vector<std::uint8_t> raw(2u * w);
   for (std::uint32_t y = 0; y < h; ++y) {
      const std::uint64_t addr = setup_.base + std::uint64_t{y} * setup_.stride;
      std::uint32_t off = 0;
      for (const dma::burst &b : dma::split_line(addr, 2u * w, max_beats_, ctx_.params.axi_data_bytes)) {
         if (!memory_access(socket_, tlm::TLM_READ_COMMAND, b.addr, raw.data() + off, b.bytes, ctx_, e)) {
            // DEC-19: terminal for the frame; release the buffer, keep the
            // rotation position, discard the frame end to end.
            ctx_.set(FX1_ISP_DMA_ERR_OFFSET, err_idma_axi);
            ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_axi_error);
            ctx_.clear(FX1_ISP_IDMA_BUF_VALID_OFFSET, bit);
            ctx_.clear(FX1_ISP_DMA_STAT_OFFSET, FX1_ISP_DMA_STAT_IDMA_BUSY_MASK);
            if (y > 0) {
               line marker;
               marker.frame = frames_;
               marker.abort = true;
               out_.push(std::move(marker), e);
            }
            return;
         }
         off += b.bytes;
      }
      line l;
      l.frame = frames_;
      l.sof = y == 0;
      l.eof = y + 1 == h;
      l.width = w;
      l.height = h;
      l.samples.resize(w);
      for (std::uint32_t x = 0; x < w; ++x) {  // little-endian halfwords (HAS Table 6-88)
         l.samples[x] = static_cast<std::uint16_t>(raw[2u * x] | (raw[2u * x + 1u] << 8));
      }
      out_.push(std::move(l), e);
   }
   // Retire once the last sample has been accepted downstream.
   out_.wait_empty(e);
   ctx_.clear(FX1_ISP_IDMA_BUF_VALID_OFFSET, bit);
   ctx_.idma_head = (head + 1u) % dma::num_buffers;
   ctx_.write(FX1_ISP_IDMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu, ctx_.reg(FX1_ISP_IDMA_FRAME_COUNT_OFFSET) + 1u);
   ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_idma_done);
   ctx_.clear(FX1_ISP_DMA_STAT_OFFSET, FX1_ISP_DMA_STAT_IDMA_BUSY_MASK);
}

// ---- pipeline thread ------------------------------------------------------------

void pipeline_engine::run() {
   for (;;) {
      const std::uint64_t e = ctx_.epoch;
      try {
         for (;;) {
            frame(e);
         }
      } catch (const reset_abort &) {
      }
   }
}

void pipeline_engine::wait_isp_enabled(std::uint64_t e) {
   // COMMON_CTRL.isp_en gates every block: processing stops while it is clear.
   while (!(ctx_.reg(FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK)) {
      ctx_.wait_event(ctx_.kick, e);
   }
}

sc_time pipeline_engine::line_time(std::uint32_t width) const {
   return ctx_.params.core_period * static_cast<double>(width + ctx_.params.pipeline_line_overhead_cycles);
}

void pipeline_engine::abort_downstream(std::uint64_t f, bool output_started, std::uint64_t e,
                                       bool y_complete, bool uv_complete) {
   ctx_.ctl.blocks_busy(false);
   ctx_.ctl.stats_abort();  // no publication for an abandoned frame (ALG-STAT-09)
   if (output_started) {
      line marker;
      marker.frame = f;
      marker.abort = true;
      // Each plane ends exactly once. UV can reach EOF one luma row before
      // Y; another marker after that EOF would poison the next frame's UV.
      if (!y_complete) y_.push(marker, e);
      if (!uv_complete) uv_.push(std::move(marker), e);
   } else {
      ctx_.retire_frame();
   }
}

void pipeline_engine::frame(std::uint64_t e) {
   // Resynchronise on a start of frame; anything else is discarded.
   for (;;) {
      while (in_.empty()) {
         ctx_.wait_event(in_.data_event(), e);
      }
      if (in_.front()->sof) {
         break;
      }
      in_.pop(e);
   }
   wait_isp_enabled(e);
   line l = in_.pop(e);  // accepted SOF
   // Pending commits first (DEC-26), then the configuration snapshot (HAS §9.4.3).
   ctx_.ctl.accepted_sof(ctx_.cycle());
   ctx_.ctl.blocks_busy(true);  // DEC-38
   ctx_.accept_frame();
   if (ctx_.params.test_datapath_stub) {
      stub_frame(l, ++frames_, e);
   } else {
      isp_frame(l, ++frames_, e);
   }
}

void pipeline_engine::isp_frame(line &l, std::uint64_t f, std::uint64_t e) {
   const pipe::pipeline_config cfg = pipe::snapshot_config(ctx_.ctl);
   pipe::frame_geometry g{0, 0};
   std::uint32_t y_rows = 0, uv_rows = 0;
   auto y_sink = [&](std::vector<std::uint8_t> &&bytes) {
      line o;
      o.frame = f;
      o.sof = y_rows == 0;
      o.eof = y_rows + 1 == g.height;
      o.width = g.width;
      o.height = g.height;
      o.bytes = std::move(bytes);
      ++y_rows;
      y_.push(std::move(o), e);
   };
   auto uv_sink = [&](std::vector<std::uint8_t> &&bytes) {
      line o;
      o.frame = f;
      o.sof = uv_rows == 0;
      o.eof = uv_rows + 1 == g.height / 2u;
      o.width = g.width;
      o.height = g.height;
      o.bytes = std::move(bytes);
      ++uv_rows;
      uv_.push(std::move(o), e);
   };
   auto warn = [](const std::string &msg) { SC_REPORT_WARNING("fx1_isp/pipeline", msg.c_str()); };
   try {
      g = pipeline_.begin(cfg, l.width, l.height, y_sink, uv_sink, warn);
      const std::uint32_t h = l.height;
      for (std::uint32_t y = 0; y < h; ++y) {
         if (y > 0) {
            l = in_.pop(e);
            if (l.abort || l.sof) {
               abort_downstream(f, y_rows > 0, e, y_rows == g.height, uv_rows == g.height / 2u);  // frame abandoned upstream
               return;
            }
         }
         ctx_.wait_time(line_time(l.width), e);  // one pixel per clock
         wait_isp_enabled(e);
         pipeline_.push(l.samples.data());
      }
      pipeline_.finish();
      pipe::end_of_frame(ctx_.ctl, cfg, pipeline_.stats());
      ctx_.ctl.blocks_busy(false);  // the last row has left the pipeline
      ctx_.state_changed();
   } catch (const std::runtime_error &err) {
      // Unsupported geometry for a block (e.g. ALG-DMS-03): drop the frame.
      SC_REPORT_WARNING("fx1_isp/pipeline", err.what());
      while (!l.eof && !l.abort) {
         l = in_.pop(e);
      }
      abort_downstream(f, y_rows > 0, e, y_rows == g.height, uv_rows == g.height / 2u);
   }
}

void pipeline_engine::stub_frame(line &l, std::uint64_t f, std::uint64_t e) {
   const std::uint32_t pattern = ctx_.reg(FX1_ISP_COMMON_BAYER_OFFSET) & FX1_ISP_COMMON_BAYER_PATTERN_MASK;
   const geometry active = control_unit::derive_active(l.width, l.height, pattern);
   const geometry out = control_unit::derive_output(active, ctx_.ctl.committed(FX1_ISP_RESIZER_CTRL_OFFSET));
   const std::uint32_t sh = pattern & 1u;
   const std::uint32_t sv = (pattern >> 1) & 1u;
   std::uint32_t oy = 0;
   const std::uint32_t h = l.height;
   for (std::uint32_t y = 0; y < h; ++y) {
      if (y > 0) {
         l = in_.pop(e);
         if (l.abort || l.sof) {
            abort_downstream(f, oy > 0, e, oy == out.height, (oy + 1u) / 2u == out.height / 2u);
            return;
         }
      }
      ctx_.wait_time(line_time(l.width), e);
      wait_isp_enabled(e);
      if (y < sv || y - sv >= active.height) {
         continue;  // row cropped by the Input Formatter
      }
      const std::uint32_t iy = y - sv;
      while (oy < out.height && stub::source_index(oy, active.height, out.height) == iy) {
         line yl;
         yl.frame = f;
         yl.sof = oy == 0;
         yl.eof = oy + 1 == out.height;
         yl.width = out.width;
         yl.height = out.height;
         yl.bytes.resize(out.width);
         stub::luma_line(l.samples.data(), sh, active.width, yl.bytes.data(), out.width);
         y_.push(std::move(yl), e);
         if (oy % 2u == 0) {
            line ul;
            ul.frame = f;
            ul.sof = oy == 0;
            ul.eof = oy + 2 >= out.height;
            ul.width = out.width;
            ul.height = out.height;
            ul.bytes.assign(out.width, stub::chroma_value);
            uv_.push(std::move(ul), e);
         }
         ++oy;
      }
   }
   ctx_.ctl.stats_abort();  // the test stub measures nothing
   ctx_.ctl.blocks_busy(false);
}

// ---- output DMA (HAS §6.25.9) --------------------------------------------------

void odma_engine::run() {
   for (;;) {
      const std::uint64_t e = ctx_.epoch;
      armed_ = false;
      overflow_flagged_ = false;
      try {
         for (;;) {
            ctx_.wait_out_of_reset(e);
            if (!armed_ && !arm(e)) {
               ctx_.wait_events(ctx_.kick, y_.data_event(), e);
               continue;
            }
            // A new frame starts only while the ODMA is enabled (M2-A2): an armed
            // but disabled ODMA leaves the frame waiting, without overflow (M2-A6).
            const bool enabled = (ctx_.reg(FX1_ISP_DMA_CTRL_OFFSET) & FX1_ISP_DMA_CTRL_ODMA_EN_MASK) != 0;
            if (y_.empty() || !enabled) {
               ctx_.wait_events(ctx_.kick, y_.data_event(), e);
               continue;
            }
            line yl = y_.pop(e);
            if (yl.abort || !yl.sof) {
               continue;  // not the start of a frame: resynchronise
            }
            if (yl.width != setup_.width || yl.height != setup_.height) {
               // The frame does not match the geometry captured at arm.
               ctx_.set(FX1_ISP_DMA_ERR_OFFSET, err_align);
               armed_ = false;
               sink_frame(yl.eof, false, e);
               end_frame();
               continue;
            }
            const unsigned head = ctx_.odma_head;
            const std::uint32_t bit = 1u << head;
            ctx_.set(FX1_ISP_DMA_STAT_OFFSET, FX1_ISP_DMA_STAT_ODMA_BUSY_MASK);
            bool y_end = false;
            bool uv_end = false;
            bool failed = false;
            bool aborted = false;
            for (std::uint32_t oy = 0; oy < setup_.height && !failed && !aborted; ++oy) {
               if (oy > 0) {
                  yl = y_.pop(e);
                  if (yl.abort) {
                     y_end = aborted = true;
                     break;
                  }
               }
               y_end = yl.eof;
               if (!write_line(setup_.y_base, setup_.y_stride, oy, yl, e)) {
                  failed = true;
                  break;
               }
               if (oy % 2u == 0) {
                  const line ul = uv_.pop(e);
                  uv_end = ul.eof || ul.abort;
                  if (ul.abort) {
                     aborted = true;
                     break;
                  }
                  if (!write_line(setup_.uv_base, setup_.uv_stride, oy / 2u, ul, e)) {
                     failed = true;
                     break;
                  }
               }
            }
            if (failed) {
               // DEC-19: no DONE, buffer released (FREE cleared), rotation kept.
               ctx_.set(FX1_ISP_DMA_ERR_OFFSET, err_odma_axi);
               ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_axi_error);
               ctx_.clear(FX1_ISP_ODMA_BUF_FREE_OFFSET, bit);
               armed_ = false;
            }
            if (failed || aborted) {
               // An upstream abort leaves the armed buffer free for the next frame.
               sink_frame(y_end, uv_end, e);
               end_frame();
               continue;
            }
            // Both planes complete: every write response has returned.
            ctx_.clear(FX1_ISP_ODMA_BUF_FREE_OFFSET, bit);  // SPEC-07: cleared when full
            ctx_.set(FX1_ISP_ODMA_BUF_DONE_OFFSET, bit);
            ctx_.odma_head = (head + 1u) % dma::num_buffers;
            ctx_.write(FX1_ISP_ODMA_FRAME_COUNT_OFFSET, 0xFFFFFFFFu,
                       ctx_.reg(FX1_ISP_ODMA_FRAME_COUNT_OFFSET) + 1u);
            ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_odma_done);
            ctx_.set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK);
            ctx_.set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK);  // DEC-17
            armed_ = false;
            end_frame();
         }
      } catch (const reset_abort &) {
      }
   }
}

bool odma_engine::arm(std::uint64_t e) {
   (void)e;
   const std::uint32_t ctrl = ctx_.reg(FX1_ISP_DMA_CTRL_OFFSET);
   const bool enabled = (ctrl & FX1_ISP_DMA_CTRL_ODMA_EN_MASK) != 0;
   const unsigned head = ctx_.odma_head;
   const bool free = (ctx_.reg(FX1_ISP_ODMA_BUF_FREE_OFFSET) >> head) & 1u;
   if (!enabled) {
      return false;
   }
   if (!free) {
      // DEC-18: an output frame is waiting and no buffer is free: report once
      // per stall episode; the pipeline is back-pressured, nothing is lost.
      if (!overflow_flagged_ && !y_.empty() && y_.front()->sof) {
         ctx_.set(FX1_ISP_DMA_ERR_OFFSET, err_overflow);
         ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_odma_overflow);
         overflow_flagged_ = true;
      }
      return false;
   }
   dma::odma_setup s{};
   const unsigned bits = ctx_.params.axi_addr_bits;
   s.y_base = dma::port_address(reg64(ctx_, FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * head), bits);
   s.uv_base = dma::port_address(reg64(ctx_, FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * head), bits);
   s.y_stride = ctx_.reg(FX1_ISP_ODMA_Y_STRIDE_OFFSET);
   s.uv_stride = ctx_.reg(FX1_ISP_ODMA_UV_STRIDE_OFFSET);
   // DEC-20: the output geometry is only driven while ISP_EN is set.
   if (ctx_.reg(FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK) {
      const geometry g = ctx_.ctl.resizer_output_geometry();
      s.width = g.width;
      s.height = g.height;
   }
   if (!dma::odma_setup_ok(s, ctx_.params.axi_data_bytes)) {
      ctx_.set(FX1_ISP_DMA_ERR_OFFSET, err_align);
      return false;
   }
   setup_ = s;
   max_beats_ = max_beats(ctrl);
   armed_ = true;
   overflow_flagged_ = false;
   return true;
}

bool odma_engine::write_line(std::uint64_t base, std::uint32_t stride, std::uint32_t row, const line &l,
                             std::uint64_t e) {
   const std::uint64_t addr = base + std::uint64_t{row} * stride;
   std::vector<std::uint8_t> data = l.bytes;
   std::uint32_t off = 0;
   for (const dma::burst &b :
        dma::split_line(addr, static_cast<std::uint32_t>(data.size()), max_beats_, ctx_.params.axi_data_bytes)) {
      if (!memory_access(socket_, tlm::TLM_WRITE_COMMAND, b.addr, data.data() + off, b.bytes, ctx_, e)) {
         return false;
      }
      off += b.bytes;
   }
   return true;
}

void odma_engine::sink_frame(bool y_done, bool uv_done, std::uint64_t e) {
   // Drain both channels together: the pipeline interleaves luma and chroma
   // lines, so draining one channel to its end first would deadlock on the
   // other channel's full FIFO.
   while (!y_done || !uv_done) {
      if (!y_done && !y_.empty()) {
         const line l = y_.pop(e);
         y_done = l.eof || l.abort;
      } else if (!uv_done && !uv_.empty()) {
         const line l = uv_.pop(e);
         uv_done = l.eof || l.abort;
      } else {
         ctx_.wait_events(y_.data_event(), uv_.data_event(), e);
      }
   }
}

void odma_engine::end_frame() {
   ctx_.clear(FX1_ISP_DMA_STAT_OFFSET, FX1_ISP_DMA_STAT_ODMA_BUSY_MASK);
   ctx_.retire_frame();
}

}  // namespace cdc::components::fx1_isp
