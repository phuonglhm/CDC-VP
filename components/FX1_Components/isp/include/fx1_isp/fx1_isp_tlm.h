// SPDX-License-Identifier: Apache-2.0
// FX1 ISP -- SystemC/TLM-2.0 loosely-timed model of the FTEL ISP IP
// (memory-to-memory wrapper, HAS Figure 4-1).
//
// Interface (see docs/ISP_MODEL_INTERFACE.md):
//   csr_socket   AXI4-Lite register port, offsets relative to the ISP base
//   idma_socket  m_axi_m0 read master (input RAW frames)
//   odma_socket  m_axi_m1 write master (NV12 luma and chroma)
//   rst_n        i_rst_n, active low
//   irq          o_irq, level, OR of the enabled status bits
//
// The register map is in fx1_isp/fx1_isp_csr.h. Base address and interrupt
// number are platform configuration and are not known to the model.
#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components::fx1_isp {

struct fx1_isp_params {
   // Core clock period (i_clk). HAS Table 6-8: ASIC nominal 500 MHz.
   sc_core::sc_time core_period{2, sc_core::SC_NS};
   // Latency annotated on every CSR access (VP parameter, not a silicon figure).
   sc_core::sc_time csr_latency{10, sc_core::SC_NS};
   // Soft-reset pulse length in core cycles (CSR row 451, DEC-14).
   unsigned soft_reset_cycles = 32;
   // PARA_AXI_DATA_WIDTH / 8 and PARA_AXI_ADDR_WIDTH (HAS Table 5-1).
   unsigned axi_data_bytes = 16;
   unsigned axi_addr_bits = 40;
   // Pipeline pacing: one pixel per clock (HAS §6.4) plus this many extra
   // core cycles per line (VP parameter, not a silicon figure).
   unsigned pipeline_line_overhead_cycles = 0;
   // TEST ONLY: replace the image pipeline by the M2 geometry stub (Y = sample
   // >> 4, UV = 128) so DMA lifecycle tests need no image reference.
   bool test_datapath_stub = false;
};

class fx1_isp_tlm : public sc_core::sc_module {
public:
   tlm_utils::simple_target_socket<fx1_isp_tlm> csr_socket;
   tlm_utils::simple_initiator_socket<fx1_isp_tlm> idma_socket;
   tlm_utils::simple_initiator_socket<fx1_isp_tlm> odma_socket;
   sc_core::sc_in<bool> rst_n;
   sc_core::sc_out<bool> irq;

   SC_HAS_PROCESS(fx1_isp_tlm);
   explicit fx1_isp_tlm(sc_core::sc_module_name name, const fx1_isp_params &params = {});
   ~fx1_isp_tlm() override;

   const fx1_isp_params &params() const { return params_; }

   // Current core-clock cycle for a given local-time offset.
   std::uint64_t cycle_at(const sc_core::sc_time &offset = sc_core::SC_ZERO_TIME) const;

   // ---- Debug / testbench backdoor. NOT part of the FW-visible interface. ----
   // Hardware-side register events, used to exercise the CSR, IRQ and error
   // contracts before the DMA and pipeline models exist.
   void debug_hw_set(std::uint32_t offset, std::uint32_t mask);
   void debug_hw_hold(std::uint32_t offset, std::uint32_t mask, bool level);
   void debug_hw_write(std::uint32_t offset, std::uint32_t mask, std::uint32_t value);
   void debug_accepted_sof();
   std::uint32_t debug_peek(std::uint32_t offset) const;
   bool debug_soft_reset_active() const;
   // Commands written by software that no block model has consumed yet.
   std::size_t debug_pending_commands() const;
   // Current rotation positions of the DMA engines (not software visible).
   std::uint32_t debug_idma_head() const;
   std::uint32_t debug_odma_head() const;
   // Sets the statistics frame counter (FRAME_ID source, not software visible)
   // so a fixture can test its 32-bit wrap without 2^32 frames.
   void debug_set_frame_counter(std::uint32_t value);

private:
   void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
   unsigned int transport_dbg(tlm::tlm_generic_payload &trans);
   void reset_method();
   void irq_method();
   void idma_thread();
   void pipeline_thread();
   void odma_thread();
   void state_changed(const sc_core::sc_time &when);

   struct impl;
   std::unique_ptr<impl> impl_;
   fx1_isp_params params_;
   sc_core::sc_event irq_update_ev_;
   bool in_reset_ = false;
};

}  // namespace cdc::components::fx1_isp
