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
constexpr std::uint32_t dma_irq_cfg_error = FX1_ISP_DMA_IRQ_STAT_IRQ_CFG_ERROR_BIT;

constexpr std::uint32_t err_align = FX1_ISP_DMA_ERR_ERR_ALIGN_BIT;
constexpr std::uint32_t err_underrun = FX1_ISP_DMA_ERR_ERR_IDMA_UNDERRUN_BIT;
constexpr std::uint32_t err_overflow = FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT;
constexpr std::uint32_t err_idma_axi = FX1_ISP_DMA_ERR_ERR_AXI_RD_BIT;
constexpr std::uint32_t err_odma_axi = FX1_ISP_DMA_ERR_ERR_AXI_WR_BIT;

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
         if (ctl.soft_reset_end_cycle() == control_unit::never) {  // draining: woken when it ends
            wait_event(kick, my_epoch);
            continue;
         }
         wait_time(params.core_period * static_cast<double>(ctl.soft_reset_end_cycle() - now), my_epoch);
         ctl.advance_to(cycle());  // the recovery's final retirement (MAS v1.07 §9.9.8)
         state_changed();
         continue;
      }
      return;
   }
}

// An AXI error response (MAS v1.07 §6.25.4.1, §9.9.8): the sticky DMA_ERR
// bit and the interrupt, then the automatic DMA-scope recovery; during a
// running recovery the request joins it (no new SEQUENCE or record).
void engine_context::axi_response_error(bool read) {
   set(FX1_ISP_DMA_ERR_OFFSET, read ? err_idma_axi : err_odma_axi);
   set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_axi_error);
   ctl.recovery_request(control_unit::reset_scope::dma, read ? control_unit::rc_read : control_unit::rc_write, cycle());
   state_changed();
}

void engine_context::set(std::uint32_t offset, std::uint32_t mask) {
   ctl.hw_set(offset, mask, cycle());
   state_changed();
}

void engine_context::clear(std::uint32_t offset, std::uint32_t mask) {
   ctl.hw_clear(offset, mask, cycle());
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

void engine_context::retire_frame(bool completed) {
   sc_assert(frames_in_flight > 0);
   ctl.frame_retired();  // the frame's profile reference (V107-3)
   if (completed) {
      // The frame has retired at every enabled output with DONE (DEC-17; MAS
      // v1.07: FRAME_DONE is the retirement of the frame, not one channel).
      set(FX1_ISP_COMMON_IRQ_STATUS_OFFSET, FX1_ISP_COMMON_IRQ_STATUS_FRAME_DONE_IRQ_MASK);
      set(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_FRAME_DONE_MASK);
   }
   if (--frames_in_flight == 0) {
      clear(FX1_ISP_COMMON_STATUS_OFFSET, FX1_ISP_COMMON_STATUS_BUSY_MASK);
   }
   retired.notify(sc_core::SC_ZERO_TIME);
}

void engine_context::consumer_done(std::uint64_t f, bool ok) {
   const auto it = consumers.find(f);
   if (it == consumers.end()) {
      return;  // abandoned by a reset
   }
   it->second.ok = it->second.ok && ok;
   if (--it->second.pending == 0) {
      const bool completed = it->second.ok;
      consumers.erase(it);
      retire_frame(completed);
   }
}

// Output channel DMA registers (CSR v1.07).
const odma_channel_regs odma_channels[num_channels] = {
   {FX1_ISP_ODMA_BUF_FREE_OFFSET, FX1_ISP_ODMA_BUF_DONE_OFFSET, FX1_ISP_ODMA_FRAME_COUNT_OFFSET,
    FX1_ISP_ODMA_Y_ADDR_L0_OFFSET, FX1_ISP_ODMA_UV_ADDR_L0_OFFSET, FX1_ISP_ODMA_Y_STRIDE_OFFSET,
    FX1_ISP_ODMA_UV_STRIDE_OFFSET, FX1_ISP_ODMA_BUF_TAG0_OFFSET, FX1_ISP_ODMA_BUF_TS0_OFFSET,
    FX1_ISP_ODMA_BUF_SOF_TS0_OFFSET, FX1_ISP_ODMA_BUF_EOF_TS0_OFFSET, FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET,
    FX1_ISP_TUNE_CH0_BUF0_PROFILE_OFFSET},
   {FX1_ISP_CH1_ODMA_BUF_FREE_OFFSET, FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET, FX1_ISP_CH1_ODMA_FRAME_COUNT_OFFSET,
    FX1_ISP_CH1_ODMA_Y_ADDR_L0_OFFSET, FX1_ISP_CH1_ODMA_UV_ADDR_L0_OFFSET, FX1_ISP_CH1_ODMA_Y_STRIDE_OFFSET,
    FX1_ISP_CH1_ODMA_UV_STRIDE_OFFSET, FX1_ISP_CH1_ODMA_BUF_TAG0_OFFSET, FX1_ISP_CH1_ODMA_BUF_TS0_OFFSET,
    FX1_ISP_CH1_ODMA_BUF_SOF_TS0_OFFSET, FX1_ISP_CH1_ODMA_BUF_EOF_TS0_OFFSET, FX1_ISP_TUNE_CH1_BUF0_FRAME_OFFSET,
    FX1_ISP_TUNE_CH1_BUF0_PROFILE_OFFSET},
   {FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET, FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET, FX1_ISP_CH2_ODMA_FRAME_COUNT_OFFSET,
    FX1_ISP_CH2_ODMA_Y_ADDR_L0_OFFSET, 0, FX1_ISP_CH2_ODMA_Y_STRIDE_OFFSET, 0, FX1_ISP_CH2_ODMA_BUF_TAG0_OFFSET,
    FX1_ISP_CH2_ODMA_BUF_TS0_OFFSET, FX1_ISP_CH2_ODMA_BUF_SOF_TS0_OFFSET, FX1_ISP_CH2_ODMA_BUF_EOF_TS0_OFFSET,
    FX1_ISP_TUNE_CH2_BUF0_FRAME_OFFSET, FX1_ISP_TUNE_CH2_BUF0_PROFILE_OFFSET},
};
static_assert(FX1_ISP_CH1_ODMA_Y_ADDR_L1_OFFSET - FX1_ISP_CH1_ODMA_Y_ADDR_L0_OFFSET == 16u &&
                  FX1_ISP_CH2_ODMA_Y_ADDR_L1_OFFSET - FX1_ISP_CH2_ODMA_Y_ADDR_L0_OFFSET == 16u &&
                  FX1_ISP_ODMA_Y_ADDR_L1_OFFSET - FX1_ISP_ODMA_Y_ADDR_L0_OFFSET == 16u,
              "buffer address registers are 16 bytes apart in every channel");

void engine_context::reset_engines() {
   ++epoch;
   idma_head = 0;
   odma_head = {};
   frames_in_flight = 0;
   consumers.clear();
   odma_busy = 0;
   odma_overflowing = 0;
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
   // An accepted transaction is outstanding until its response, however the
   // target times it: a blocking target holds this thread inside b_transport,
   // an annotated one returns the response time in `delay`. The response is
   // received whatever happens meanwhile (MAS v1.07 §9.9.8): the drain of a
   // recovery ends only when every accepted transaction has its response, and
   // an error response in the drain sets its sticky bit and joins the
   // running recovery. Only then is aborted work abandoned.
   // A hard reset wins over everything accepted before it: such a response
   // only ends the transaction's lifetime (no DMA_ERR, no recovery).
   const std::uint64_t gen = ctx.hard_reset_gen;
   ++ctx.axi_outstanding;
   socket->b_transport(t, delay);
   if (delay > sc_core::SC_ZERO_TIME) {
      sc_core::wait(delay);
   }
   --ctx.axi_outstanding;
   const bool ok = t.is_response_ok();
   if (!ok && gen == ctx.hard_reset_gen && !ctx.in_reset) {
      ctx.axi_response_error(cmd == tlm::TLM_READ_COMMAND);
   }
   if (ctx.axi_outstanding == 0) {
      ctx.ctl.recovery_drained(ctx.edge_at_or_after_now());
      ctx.kick.notify(sc_core::SC_ZERO_TIME);
   }
   ctx.check(my_epoch);
   return ok;
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
      const bool valid = (ctx_.request(FX1_ISP_IDMA_BUF_VALID_OFFSET) >> head) & 1u;
      const bool valid_next_edge = !valid && ((ctx_.reg(FX1_ISP_IDMA_BUF_VALID_OFFSET) >> head) & 1u);
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
         s.depth = ctx_.reg(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET) & FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_MASK;
         if (dma::idma_setup_ok(s, ctx_.params.axi_data_bytes)) {
            setup_ = s;
            // The buffer's tag and capture timestamp join the working set; a
            // later write to them does not reach this frame (MAS Table 160).
            tag_ = ctx_.reg(FX1_ISP_IDMA_BUF_TAG0_OFFSET + 4u * head);
            ts_ = ctx_.reg(FX1_ISP_IDMA_BUF_TS0_OFFSET + 4u * head);
            max_beats_ = max_beats(ctrl);
            return;
         }
         // Illegal configuration: the unit does not start; DMA_ERR.ALIGN and
         // IRQ_CFG_ERROR. The rejected buffer is not read: its VALID bit is
         // cleared and the rotation stays (MAS v1.07 §6.25.3.4), so the unit
         // waits as if the buffer were not available until software corrects
         // the configuration and sets VALID again.
         ctx_.set(FX1_ISP_DMA_ERR_OFFSET, err_align);
         ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_cfg_error);
         ctx_.clear(FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << head);
         continue;
      }
      if (enabled && valid_next_edge) {
         ctx_.wait_next_edge(e);  // armed on this edge: seen on the next
      } else {
         ctx_.wait_event(ctx_.kick, e);
      }
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
            // Terminal (MAS v1.07 §6.25.4.1, §9.9.8): the sticky error, then
            // automatic DMA-scope recovery of every unretired frame. The
            // engines stop at once; the recovery's final retirement clears
            // the ownership bits and restarts the rotations (DEC-19 superseded).
            // memory_access latched the error and requested the recovery.
            (void)bit;
            ctx_.check(e);  // aborted by the recovery
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
      l.tag = tag_;
      l.ts = ts_;
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

void pipeline_engine::abort_downstream(std::uint64_t f, std::uint64_t e) {
   ctx_.ctl.blocks_busy(false);
   ctx_.ctl.advance_to(ctx_.cycle());
   ctx_.ctl.stats_abort();  // no publication for an abandoned frame (ALG-STAT-09)
   ctx_.consumer_done(f, false);  // the pipeline
   for (unsigned c = 0; c < num_channels; ++c) {
      channel_progress &p = prog_[c];
      if (!p.on) {
         continue;
      }
      p.on = false;
      if (p.y_rows == 0) {
         ctx_.consumer_done(f, false);  // this output never saw the frame
         continue;
      }
      // Each plane ends exactly once. UV can reach EOF one luma row before
      // Y; another marker after that EOF would poison the next frame's UV.
      line marker;
      marker.frame = f;
      marker.abort = true;
      if (p.y_rows < p.height) {
         out_[c].y->push(marker, e);
      }
      if (out_[c].uv && p.uv_rows < p.height / 2u) {
         out_[c].uv->push(std::move(marker), e);
      }
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
   // Admission (MAS v1.07 §9.7, §9.9.7): context credits, and in profile mode
   // an active or due profile, an enabled output and the old-epoch drain.
   for (;;) {
      const control_unit::admission a = ctx_.ctl.admission_state(ctx_.cycle(), ctx_.frames_in_flight);
      if (a == control_unit::admission::go) {
         break;
      }
      if (a == control_unit::admission::wait_edge) {
         ctx_.wait_next_edge(e);
      } else {
         ctx_.wait_events(ctx_.kick, ctx_.retired, e);
      }
   }
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
   const std::uint32_t ingress = ctx_.ctl.frame_id(), profile = ctx_.ctl.frame_profile_id();
   // The distributor presents every pixel to each channel enabled by the
   // OUTPUT_ENABLE mask of this SOF (MAS v1.07 §9.5.1); each has its FIFOs.
   std::array<pipe::isp_pipeline::channel_sinks, num_channels> sinks{};
   prog_ = {};
   unsigned outputs = 0;
   for (unsigned c = 0; c < num_channels; ++c) {
      if (!(cfg.outputs & (1u << c))) {
         continue;
      }
      ++outputs;
      prog_[c].on = true;
      auto make = [&, c](bool chroma) {
         return [this, c, chroma, f, e, &l, ingress, profile](std::vector<std::uint8_t> &&bytes) {
            channel_progress &p = prog_[c];
            std::uint32_t &rows = chroma ? p.uv_rows : p.y_rows;
            const std::uint32_t total = chroma ? p.height / 2u : p.height;
            line o;
            o.frame = f;
            o.sof = rows == 0;
            o.eof = rows + 1 == total;
            o.width = p.width;
            o.height = p.height;
            o.tag = l.tag;  // the frame's metadata follows it to the output (MAS Table 160)
            o.ts = l.ts;
            o.ingress = ingress;
            o.profile = profile;
            o.bytes = std::move(bytes);
            ++rows;
            (chroma ? out_[c].uv : out_[c].y)->push(std::move(o), e);
         };
      };
      sinks[c].y = make(false);
      if (out_[c].uv) {
         sinks[c].uv = make(true);
      }
   }
   ctx_.frame_begin(f, outputs);
   auto warn = [](const std::string &msg) { SC_REPORT_WARNING("fx1_isp/pipeline", msg.c_str()); };
   try {
      pipeline_.begin(cfg, l.width, l.height, sinks, warn);
      for (unsigned c = 0; c < num_channels; ++c) {
         const pipe::frame_geometry g = pipeline_.output(c);
         prog_[c].width = g.width;
         prog_[c].height = g.height;
      }
      const std::uint32_t h = l.height;
      for (std::uint32_t y = 0; y < h; ++y) {
         if (y > 0) {
            l = in_.pop(e);
            if (l.abort || l.sof) {
               abort_downstream(f, e);  // frame abandoned upstream
               return;
            }
         }
         ctx_.wait_time(line_time(l.width), e);  // one pixel per clock
         wait_isp_enabled(e);
         pipeline_.push(l.samples.data());
      }
      pipeline_.finish();
      ctx_.ctl.advance_to(ctx_.cycle());  // publication edge (MAS §6.22.4.3)
      pipe::end_of_frame(ctx_.ctl, cfg, pipeline_.stats());
      ctx_.ctl.blocks_busy(false);  // the last row has left the pipeline
      for (channel_progress &p : prog_) {
         p.on = false;
      }
      ctx_.consumer_done(f, true);
      ctx_.state_changed();
   } catch (const std::runtime_error &err) {
      // Unsupported geometry for a block (e.g. ALG-DMS-03): drop the frame.
      SC_REPORT_WARNING("fx1_isp/pipeline", err.what());
      while (!l.eof && !l.abort) {
         l = in_.pop(e);
      }
      abort_downstream(f, e);
   }
}

void pipeline_engine::stub_frame(line &l, std::uint64_t f, std::uint64_t e) {
   // The datapath stub (DMA tests) drives CH0 only.
   ctx_.frame_begin(f, 1);
   const std::uint32_t pattern = ctx_.reg(FX1_ISP_COMMON_BAYER_OFFSET) & FX1_ISP_COMMON_BAYER_PATTERN_MASK;
   const std::uint32_t depth = ctx_.reg(FX1_ISP_COMMON_INPUT_FORMAT_OFFSET) & FX1_ISP_COMMON_INPUT_FORMAT_DEPTH_MASK;
   const geometry active = control_unit::derive_active(l.width, l.height, pattern);
   // The frame's published Resizer set (MAS §6.20.4.3), if derived for this raster.
   const resizer_set &rz = ctx_.ctl.resizer_frame();
   const geometry out = rz.valid && rz.key.width == active.width && rz.key.height == active.height ? rz.out : active;
   prog_ = {};
   prog_[0] = channel_progress{true, out.width, out.height, 0, 0};
   const std::uint32_t ingress = ctx_.ctl.frame_id(), profile = ctx_.ctl.frame_profile_id();
   const std::uint32_t sh = pattern & 1u;
   const std::uint32_t sv = (pattern >> 1) & 1u;
   std::uint32_t oy = 0;
   const std::uint32_t h = l.height;
   for (std::uint32_t y = 0; y < h; ++y) {
      if (y > 0) {
         l = in_.pop(e);
         if (l.abort || l.sof) {
            abort_downstream(f, e);
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
         yl.tag = l.tag;
         yl.ts = l.ts;
         yl.ingress = ingress;
         yl.profile = profile;
         yl.bytes.resize(out.width);
         stub::luma_line(l.samples.data(), sh, active.width, yl.bytes.data(), out.width, depth);
         out_[0].y->push(std::move(yl), e);
         ++prog_[0].y_rows;
         if (oy % 2u == 0) {
            line ul;
            ul.frame = f;
            ul.sof = oy == 0;
            ul.eof = oy + 2 >= out.height;
            ul.width = out.width;
            ul.height = out.height;
            ul.tag = l.tag;
            ul.ts = l.ts;
            ul.ingress = ingress;
            ul.profile = profile;
            ul.bytes.assign(out.width, stub::chroma_value);
            out_[0].uv->push(std::move(ul), e);
            ++prog_[0].uv_rows;
         }
         ++oy;
      }
   }
   ctx_.ctl.stats_abort();  // the test stub measures nothing
   ctx_.ctl.blocks_busy(false);
   prog_[0].on = false;
   ctx_.consumer_done(f, true);
}

// ---- output DMA (HAS §6.25.9) --------------------------------------------------

void odma_engine::run() {
   for (;;) {
      const std::uint64_t e = ctx_.epoch;
      busy_ = overflow_ = false;
      try {
         for (;;) {
            ctx_.wait_out_of_reset(e);
            // A new frame starts only while the ODMA is enabled (M2-A2): a
            // disabled ODMA leaves the frame waiting, without overflow (M2-A6).
            const bool enabled = (ctx_.reg(FX1_ISP_DMA_CTRL_OFFSET) & FX1_ISP_DMA_CTRL_ODMA_EN_MASK) != 0;
            const line *first = y_.front();
            if (!enabled || !first) {
               set_overflow(false);
               ctx_.wait_events(ctx_.kick, y_.data_event(), e);
               continue;
            }
            if (first->abort || !first->sof) {
               y_.pop(e);  // not the start of a frame: resynchronise
               continue;
            }
            if (!claim(*first, e)) {
               if (retry_next_edge_) {
                  ctx_.wait_next_edge(e);  // freed on this edge: seen on the next
               } else {
                  ctx_.wait_events(ctx_.kick, y_.data_event(), e);
               }
               continue;
            }
            line yl = y_.pop(e);
            frame_ = yl.frame;
            const unsigned head = ctx_.odma_head[ch_];
            const std::uint32_t bit = 1u << head;
            const std::uint32_t fh = yl.height;
            const std::uint32_t tag = yl.tag, ts = yl.ts, ingress = yl.ingress, profile = yl.profile;
            const std::uint32_t sof_ts = ctx_.ctl.frame_counter(ctx_.cycle());
            std::uint32_t eof_ts = 0;
            bool y_end = false;
            bool uv_end = uv_ == nullptr;  // CH2: one plane
            bool failed = false;
            bool aborted = false;
            for (std::uint32_t oy = 0; oy < fh && !failed && !aborted; ++oy) {
               if (oy > 0) {
                  yl = y_.pop(e);
                  if (yl.abort) {
                     y_end = aborted = true;
                     break;
                  }
               }
               y_end = yl.eof;
               if (y_end && uv_end) {
                  eof_ts = ctx_.ctl.frame_counter(ctx_.cycle());  // last line accepted
               }
               if (!write_line(setup_.y_base, setup_.y_stride, oy, yl, e)) {
                  failed = true;
                  break;
               }
               if (uv_ && oy % 2u == 0) {
                  const line ul = uv_->pop(e);
                  uv_end = ul.eof || ul.abort;
                  if (ul.abort) {
                     aborted = true;
                     break;
                  }
                  if (y_end && uv_end) {
                     eof_ts = ctx_.ctl.frame_counter(ctx_.cycle());
                  }
                  if (!write_line(setup_.uv_base, setup_.uv_stride, oy / 2u, ul, e)) {
                     failed = true;
                     break;
                  }
               }
            }
            if (failed) {
               // Terminal: memory_access latched the sticky error and requested
               // the automatic DMA-scope recovery (MAS v1.07 §9.9.8); no DONE,
               // no metadata (DEC-19 superseded).
               ctx_.check(e);  // aborted by the recovery
            } else if (aborted) {
               // Frame abandoned upstream: it is processed as if it had never
               // existed (MAS §9.1.1), so the buffer it took is handed back
               // free for the next frame, rotation kept, no metadata
               // published (VP decision U5-A3).
               ctx_.set(r_.free, bit);
            }
            if (failed || aborted) {
               sink_frame(y_end, uv_end, e);
               end_frame(false);
               continue;
            }
            // Every plane complete: every write response has returned. The
            // frame's metadata is published with the buffer (MAS Table 6-159,
            // §9.9.9: ingress frame ID and profile ID with it).
            ctx_.write(r_.tag0 + 4u * head, 0xFFFFFFFFu, tag);
            ctx_.write(r_.ts0 + 4u * head, 0xFFFFFFFFu, ts);
            ctx_.write(r_.sof_ts0 + 4u * head, 0xFFFFFFFFu, sof_ts);
            ctx_.write(r_.eof_ts0 + 4u * head, 0xFFFFFFFFu, eof_ts);
            ctx_.write(r_.frame0 + 4u * head, 0xFFFFFFFFu, ingress);
            ctx_.write(r_.profile0 + 4u * head, 0xFFFFFFFFu, profile);
            ctx_.set(r_.done, bit);
            ctx_.ctl.output_done(ingress, ch_);  // not a failed output of a later recovery
            ctx_.odma_head[ch_] = (head + 1u) % dma::num_buffers;
            ctx_.write(r_.count, 0xFFFFFFFFu, ctx_.reg(r_.count) + 1u);
            ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_odma_done);
            ctx_.set(FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET, 1u << (FX1_ISP_OUTPUT_IRQ_STATUS_DONE_SHIFT + ch_));
            end_frame(true);
         }
      } catch (const reset_abort &) {
      }
   }
}

bool odma_engine::claim(const line &first, std::uint64_t e) {
   (void)e;
   const std::uint32_t ctrl = ctx_.reg(FX1_ISP_DMA_CTRL_OFFSET);
   const unsigned head = ctx_.odma_head[ch_];
   const std::uint32_t bit = 1u << head;
   const bool free = (ctx_.request(r_.free) >> head) & 1u;
   retry_next_edge_ = !free && ((ctx_.reg(r_.free) >> head) & 1u);
   // The channel's frame is waiting and the head buffer is not free: the
   // pipeline is back-pressured, nothing is lost (MAS §6.25.3.9, DEC-18).
   set_overflow(!free);
   if (!free) {
      return false;
   }
   // The claim captures both plane bases and strides and the frame's own
   // output geometry, then checks Table 6-153 (MAS §6.25.3.8): a rejected
   // buffer is neither read nor written, its FREE bit is cleared, the rotation
   // does not advance and the frame waits for software (U4-A2 superseded).
   dma::odma_setup s{};
   const unsigned bits = ctx_.params.axi_addr_bits;
   s.rgb = uv_ == nullptr;
   s.y_base = dma::port_address(reg64(ctx_, r_.y_addr0 + 16u * head), bits);
   s.y_stride = ctx_.reg(r_.y_stride);
   if (!s.rgb) {
      s.uv_base = dma::port_address(reg64(ctx_, r_.uv_addr0 + 16u * head), bits);
      s.uv_stride = ctx_.reg(r_.uv_stride);
   }
   s.width = first.width;
   s.height = first.height;
   if (!dma::odma_setup_ok(s, ctx_.params.axi_data_bytes)) {
      ctx_.clear(r_.free, bit);
      ctx_.set(FX1_ISP_DMA_ERR_OFFSET, err_align);
      ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_cfg_error);  // MAS Table 6-160
      set_overflow(true);  // the frame now waits for a free buffer
      retry_next_edge_ = false;
      return false;
   }
   setup_ = s;
   max_beats_ = max_beats(ctrl);
   ctx_.clear(r_.free, bit);  // the frame takes the head buffer (MAS Table 6-158)
   busy_ = true;
   if (ctx_.odma_busy++ == 0) {
      ctx_.set(FX1_ISP_DMA_STAT_OFFSET, FX1_ISP_DMA_STAT_ODMA_BUSY_MASK);
   }
   return true;
}

void odma_engine::set_overflow(bool level) {
   // DMA_ERR.ODMA_OVERFLOW stays set while any channel is stalled (MAS Table
   // 6-160, DEC-18 status part superseded); the interrupt is raised once per stall.
   if (level == overflow_) {
      return;
   }
   overflow_ = level;
   if (level) {
      if (ctx_.odma_overflowing++ == 0) {
         ctx_.hold(FX1_ISP_DMA_ERR_OFFSET, err_overflow, true);
         ctx_.set(FX1_ISP_DMA_IRQ_STAT_OFFSET, dma_irq_odma_overflow);
      }
   } else if (--ctx_.odma_overflowing == 0) {
      ctx_.hold(FX1_ISP_DMA_ERR_OFFSET, err_overflow, false);
   }
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
   // Drain both planes together: the pipeline interleaves luma and chroma
   // lines, so draining one plane to its end first would deadlock on the
   // other plane's full FIFO.
   while (!y_done || !uv_done) {
      if (!y_done && !y_.empty()) {
         const line l = y_.pop(e);
         y_done = l.eof || l.abort;
      } else if (!uv_done && uv_ && !uv_->empty()) {
         const line l = uv_->pop(e);
         uv_done = l.eof || l.abort;
      } else if (uv_) {
         ctx_.wait_events(y_.data_event(), uv_->data_event(), e);
      } else {
         ctx_.wait_event(y_.data_event(), e);
      }
   }
}

void odma_engine::end_frame(bool ok) {
   busy_ = false;
   if (--ctx_.odma_busy == 0) {
      ctx_.clear(FX1_ISP_DMA_STAT_OFFSET, FX1_ISP_DMA_STAT_ODMA_BUSY_MASK);
   }
   ctx_.consumer_done(frame_, ok);
}

}  // namespace cdc::components::fx1_isp
