//author: linhtk55-fpt
//verified: hoangv11

#ifndef DMIC_H
#define DMIC_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <queue>
#include "pdm_payload.h"

// offsets
#define DMIC_CTRL_REG 0x00
#define DMIC_STATUS_REG 0x04
#define DMIC_FIFO_DATA_REG 0x08
#define DMIC_FIFO_WM_REG 0x0C
#define DMIC_INT_CLR_REG 0x10

// bit masks
#define DMIC_CTRL_EN (1 << 0)     // Bit 0: 0x01
#define DMIC_CTRL_INT_EN (1 << 1) // Bit 1: 0x02

#define DMIC_CTRL_DEC_SHIFT 8
#define DMIC_CTRL_DEC_MASK (0xFF << DMIC_CTRL_DEC_SHIFT)

#define DMIC_STATUS_FE (1 << 0) // fifo empty
#define DMIC_STATUS_FF (1 << 1) // fifo full
#define DMIC_STATUS_OE (1 << 2) // overrun error
#define DMIC_STATUS_WM (1 << 3) // watermark reached

#define DMIC_INT_CLR_OE (1 << 0) // clear Overrun
#define DMIC_INT_CLR_WM (1 << 1)

namespace cdc::components {
class DmicTLM : public sc_core::sc_module {
public:
   DmicTLM(sc_core::sc_module_name name);
   tlm_utils::simple_target_socket<DmicTLM> pdm_target_socket;
   tlm_utils::simple_target_socket<DmicTLM> bus_target_socket;

   sc_core::sc_in<bool> reset_n;
   sc_core::sc_out<bool> irq_out;

private:
   uint32_t ctrl_reg;
   uint32_t fifo_wm_reg;
   bool overrun_flag;
   bool watermark_flag;

   std::queue<int> pcm_fifo;
   const size_t FIFO_MAX_DEPTH = 64;

   // CIC State
   long integrator;
   long prev_integrator;
   int counter;

   void reset();
   void handle_reset();
   void start_of_simulation() override;
   void evaluate_interrupts();
   uint32_t get_status_reg();
   void pdm_b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
   void bus_b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
};
} // namespace cdc::components
#endif
