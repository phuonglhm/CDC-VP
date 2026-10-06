// SPDX-License-Identifier: Apache-2.0
#include "fx1_isp/fx1_isp_tlm.h"

#include "control/control_unit.h"
#include "engine/engines.h"
#include "fx1_isp/fx1_isp_csr.h"

namespace cdc::components::fx1_isp {

using sc_core::sc_time;

struct fx1_isp_tlm::impl {
   impl(const fx1_isp_params &p, fx1_isp_tlm &top)
       : control(p.soft_reset_cycles),
         ctx(control, p),
         in_fifo(ctx, 2),  // PARA_DMA_FIFO_LINES = 2 (HAS Table 6-87)
         y_fifo(ctx, 2),
         uv_fifo(ctx, 2),
         idma(ctx, top.idma_socket, in_fifo),
         pipeline(ctx, in_fifo, y_fifo, uv_fifo),
         odma(ctx, top.odma_socket, y_fifo, uv_fifo) {
      ctx.fifos = {&in_fifo, &y_fifo, &uv_fifo};
   }
   control_unit control;
   engine_context ctx;
   line_fifo in_fifo;
   line_fifo y_fifo;
   line_fifo uv_fifo;
   idma_engine idma;
   pipeline_engine pipeline;
   odma_engine odma;
};

fx1_isp_tlm::fx1_isp_tlm(sc_core::sc_module_name name, const fx1_isp_params &params)
    : sc_core::sc_module(name),
      csr_socket("csr_socket"),
      idma_socket("idma_socket"),
      odma_socket("odma_socket"),
      rst_n("rst_n"),
      irq("irq"),
      params_(params) {
   if (params_.core_period <= sc_core::SC_ZERO_TIME) {
      SC_REPORT_FATAL(this->name(), "core_period must be positive");
   }
   if (params_.axi_data_bytes != 8 && params_.axi_data_bytes != 16 && params_.axi_data_bytes != 32) {
      SC_REPORT_FATAL(this->name(), "axi_data_bytes must be 8, 16 or 32 (PARA_AXI_DATA_WIDTH 64/128/256)");
   }
   impl_ = std::make_unique<impl>(params_, *this);
   impl_->ctx.state_changed = [this] { state_changed(sc_core::SC_ZERO_TIME); };

   // The DMA engines consume these registers (they re-evaluate after every
   // write, see b_transport), so the writes are no longer pending commands.
   control_unit &ctl = impl_->control;
   const std::uint32_t dma_registers[] = {FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_DMA_CTRL_OFFSET,
                                          FX1_ISP_IDMA_BUF_VALID_OFFSET, FX1_ISP_ODMA_BUF_FREE_OFFSET,
                                          FX1_ISP_ODMA_BUF_DONE_OFFSET};
   for (const std::uint32_t off : dma_registers) {
      ctl.set_block_hook(off, [](const register_file::write_effect &, std::uint64_t) {});
   }
   ctl.set_soft_reset_hook([this](std::uint64_t) { impl_->ctx.reset_engines(); });

   csr_socket.register_b_transport(this, &fx1_isp_tlm::b_transport);
   csr_socket.register_transport_dbg(this, &fx1_isp_tlm::transport_dbg);

   SC_METHOD(reset_method);
   sensitive << rst_n;
   SC_METHOD(irq_method);
   sensitive << irq_update_ev_;
   SC_THREAD(idma_thread);
   SC_THREAD(pipeline_thread);
   SC_THREAD(odma_thread);
}

void fx1_isp_tlm::idma_thread() { impl_->idma.run(); }
void fx1_isp_tlm::pipeline_thread() { impl_->pipeline.run(); }
void fx1_isp_tlm::odma_thread() { impl_->odma.run(); }

fx1_isp_tlm::~fx1_isp_tlm() = default;

std::uint64_t fx1_isp_tlm::cycle_at(const sc_time &offset) const {
   const sc_time t = sc_core::sc_time_stamp() + offset;
   return static_cast<std::uint64_t>(t / params_.core_period);
}

void fx1_isp_tlm::state_changed(const sc_time &when) {
   irq_update_ev_.notify(when);
}

void fx1_isp_tlm::reset_method() {
   const bool asserted = !rst_n.read();
   if (asserted) {
      impl_->control.external_reset();
      impl_->ctx.reset_engines();  // frame in flight discarded, engines to idle
   }
   in_reset_ = asserted;
   impl_->ctx.in_reset = asserted;
   impl_->ctx.kick.notify(sc_core::SC_ZERO_TIME);
   state_changed(sc_core::SC_ZERO_TIME);
}

void fx1_isp_tlm::irq_method() {
   const bool level = !in_reset_ && impl_->control.irq_level();
   if (irq.read() != level) {
      irq.write(level);
   }
}

namespace {

// Decodes a TLM access into one AXI4-Lite beat (CSR-23): a 4-byte access
// addresses the word at addr & ~3 (address bits [1:0] ignored, HAS Table
// 7-10); a 1..3 byte access uses the byte lanes starting at addr[1:0] and must
// stay inside the word. Returns false for anything that is not one beat.
struct beat {
   std::uint32_t word = 0;
   unsigned first_lane = 0;
   unsigned length = 0;
};

bool decode(const tlm::tlm_generic_payload &trans, beat &b, tlm::tlm_response_status &err) {
   const sc_dt::uint64 addr = trans.get_address();
   const unsigned len = trans.get_data_length();
   if (addr >= 0x10000u) {
      err = tlm::TLM_ADDRESS_ERROR_RESPONSE;
      return false;
   }
   if (len == 0 || len > 4 || trans.get_streaming_width() < len) {
      err = tlm::TLM_BURST_ERROR_RESPONSE;
      return false;
   }
   if (trans.get_data_ptr() == nullptr) {
      err = tlm::TLM_GENERIC_ERROR_RESPONSE;
      return false;
   }
   b.word = static_cast<std::uint32_t>(addr) & ~3u;
   b.first_lane = len == 4 ? 0u : static_cast<unsigned>(addr & 3u);
   b.length = len;
   if (b.first_lane + len > 4) {
      err = tlm::TLM_BURST_ERROR_RESPONSE;
      return false;
   }
   return true;
}

bool byte_enabled(const tlm::tlm_generic_payload &trans, unsigned i) {
   const unsigned char *be = trans.get_byte_enable_ptr();
   const unsigned be_len = trans.get_byte_enable_length();
   return be == nullptr || be_len == 0 || be[i % be_len] == TLM_BYTE_ENABLED;
}

}  // namespace

void fx1_isp_tlm::b_transport(tlm::tlm_generic_payload &trans, sc_time &delay) {
   beat b;
   tlm::tlm_response_status err = tlm::TLM_OK_RESPONSE;
   const tlm::tlm_command cmd = trans.get_command();
   if (cmd != tlm::TLM_READ_COMMAND && cmd != tlm::TLM_WRITE_COMMAND) {
      trans.set_response_status(cmd == tlm::TLM_IGNORE_COMMAND ? tlm::TLM_OK_RESPONSE
                                                               : tlm::TLM_COMMAND_ERROR_RESPONSE);
      return;
   }
   if (!decode(trans, b, err)) {
      trans.set_response_status(err);
      return;
   }
   if (trans.get_byte_enable_ptr() != nullptr && trans.get_byte_enable_length() == 0) {
      trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
      return;
   }
   // Synchronise on access: a register access has side effects (status,
   // interrupts, commands), so it is applied at its annotated time rather than
   // when the initiator happens to call. The initiator's local time is consumed
   // here; the returned delay carries only the access latency.
   if (delay != sc_core::SC_ZERO_TIME) {
      if (sc_core::sc_get_current_process_handle().proc_kind() != sc_core::SC_THREAD_PROC_) {
         SC_REPORT_ERROR(name(), "CSR access with a non-zero annotated delay from a method "
                                 "process cannot be synchronised; call from a thread or "
                                 "synchronise before the access");
         trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
         return;
      }
      wait(delay);
      delay = sc_core::SC_ZERO_TIME;
   }
   control_unit &ctl = impl_->control;
   const std::uint64_t cycle = cycle_at();
   unsigned char *data = trans.get_data_ptr();

   if (cmd == tlm::TLM_READ_COMMAND) {
      const std::uint32_t value = ctl.csr_read(b.word);
      for (unsigned i = 0; i < b.length; ++i) {
         if (byte_enabled(trans, i)) {
            data[i] = static_cast<unsigned char>(value >> (8u * (b.first_lane + i)));
         }
      }
   } else {
      std::uint32_t value = 0;
      std::uint32_t lanes = 0;
      for (unsigned i = 0; i < b.length; ++i) {
         if (byte_enabled(trans, i)) {
            const unsigned shift = 8u * (b.first_lane + i);
            value |= static_cast<std::uint32_t>(data[i]) << shift;
            lanes |= 0xFFu << shift;
         }
      }
      // While i_rst_n is asserted the register file is held in reset.
      if (!in_reset_) {
         ctl.csr_write(b.word, value, lanes, cycle);
      }
      // Engines re-evaluate their start conditions after every register write.
      impl_->ctx.kick.notify(sc_core::SC_ZERO_TIME);
      state_changed(sc_core::SC_ZERO_TIME);
   }
   delay += params_.csr_latency;
   trans.set_dmi_allowed(false);  // registers have side effects
   trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int fx1_isp_tlm::transport_dbg(tlm::tlm_generic_payload &trans) {
   // Side-effect-free read access for debuggers; writes are not supported.
   beat b;
   tlm::tlm_response_status err = tlm::TLM_OK_RESPONSE;
   if (trans.get_command() != tlm::TLM_READ_COMMAND || !decode(trans, b, err)) {
      return 0;
   }
   const std::uint32_t value = impl_->control.csr_read(b.word);
   unsigned char *data = trans.get_data_ptr();
   for (unsigned i = 0; i < b.length; ++i) {
      data[i] = static_cast<unsigned char>(value >> (8u * (b.first_lane + i)));
   }
   return b.length;
}

std::uint32_t fx1_isp_tlm::debug_idma_head() const { return impl_->ctx.idma_head; }
std::uint32_t fx1_isp_tlm::debug_odma_head() const { return impl_->ctx.odma_head; }

void fx1_isp_tlm::debug_set_frame_counter(std::uint32_t value) { impl_->control.debug_set_frame_counter(value); }

void fx1_isp_tlm::debug_hw_set(std::uint32_t offset, std::uint32_t mask) {
   impl_->control.hw_set(offset, mask, cycle_at());
   state_changed(sc_core::SC_ZERO_TIME);
}

void fx1_isp_tlm::debug_hw_hold(std::uint32_t offset, std::uint32_t mask, bool level) {
   impl_->control.hw_hold(offset, mask, level, cycle_at());
   state_changed(sc_core::SC_ZERO_TIME);
}

void fx1_isp_tlm::debug_hw_write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value) {
   impl_->control.hw_write(offset, mask, value);
   state_changed(sc_core::SC_ZERO_TIME);
}

void fx1_isp_tlm::debug_accepted_sof() {
   impl_->control.accepted_sof(cycle_at());
   state_changed(sc_core::SC_ZERO_TIME);
}

std::uint32_t fx1_isp_tlm::debug_peek(std::uint32_t offset) const {
   return impl_->control.regs().peek(offset);
}

bool fx1_isp_tlm::debug_soft_reset_active() const {
   return impl_->control.soft_reset_active(cycle_at());
}

std::size_t fx1_isp_tlm::debug_pending_commands() const {
   return impl_->control.commands().size();
}

}  // namespace cdc::components::fx1_isp
