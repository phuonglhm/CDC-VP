//Author: QuanNH107

#ifndef DMA_TLM_H
#define DMA_TLM_H

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

class dma_tlm : public sc_core::sc_module {
public:
   tlm_utils::simple_target_socket<dma_tlm> target_socket;
   tlm_utils::simple_initiator_socket<dma_tlm> master_socket;
   sc_core::sc_in<bool> reset_n;
   sc_core::sc_out<uint32_t> irq;
   sc_core::sc_out<bool> irq_abort;

   static const uint32_t DSR = 0x000;
   static const uint32_t DPC = 0x004;
   static const uint32_t INTEN = 0x020;
   static const uint32_t INT_EVENT_RIS = 0x024;
   static const uint32_t INTMIS = 0x028;
   static const uint32_t INTCLR = 0x02C;
   static const uint32_t FSRD = 0x030;
   static const uint32_t FSRC = 0x034;
   static const uint32_t FTRD = 0x038;

   static const uint32_t FTR0 = 0x040;
   static const uint32_t CSR0 = 0x100;
   static const uint32_t CPC0 = 0x104;
   static const uint32_t CHANNEL_STATUS_STRIDE = 0x008;

   static const uint32_t SAR0 = 0x400;
   static const uint32_t DAR0 = 0x404;
   static const uint32_t CCR0 = 0x408;
   static const uint32_t LC0_0 = 0x40C;
   static const uint32_t LC1_0 = 0x410;
   static const uint32_t CHANNEL_AXI_STRIDE = 0x020;

   static const uint32_t DBGSTATUS = 0xD00;
   static const uint32_t DBGCMD = 0xD04;
   static const uint32_t DBGINST0 = 0xD08;
   static const uint32_t DBGINST1 = 0xD0C;

   static const uint32_t CR0 = 0xE00;
   static const uint32_t CR1 = 0xE04;
   static const uint32_t CR2 = 0xE08;
   static const uint32_t CR3 = 0xE0C;
   static const uint32_t CR4 = 0xE10;
   static const uint32_t CRD = 0xE14;
   static const uint32_t WD = 0xE80;

   static const uint32_t PERIPH_ID0 = 0xFE0;
   static const uint32_t PERIPH_ID1 = 0xFE4;
   static const uint32_t PERIPH_ID2 = 0xFE8;
   static const uint32_t PERIPH_ID3 = 0xFEC;
   static const uint32_t PCELL_ID0 = 0xFF0;
   static const uint32_t PCELL_ID1 = 0xFF4;
   static const uint32_t PCELL_ID2 = 0xFF8;
   static const uint32_t PCELL_ID3 = 0xFFC;

   static const uint32_t CCR_SRC_INC = 1u << 0;
   static const uint32_t CCR_SRC_BURST_SIZE_SHIFT = 1;
   static const uint32_t CCR_SRC_BURST_LEN_SHIFT = 4;
   static const uint32_t CCR_SRC_PROT_SHIFT = 8;
   static const uint32_t CCR_DST_INC = 1u << 14;
   static const uint32_t CCR_DST_BURST_SIZE_SHIFT = 15;
   static const uint32_t CCR_DST_BURST_LEN_SHIFT = 18;
   static const uint32_t CCR_DST_PROT_SHIFT = 22;

   static const uint32_t STATUS_STOPPED = 0x0;
   static const uint32_t STATUS_EXECUTING = 0x1;
   static const uint32_t STATUS_WAITING_EVENT = 0x4;
   static const uint32_t STATUS_AT_BARRIER = 0x5;
   static const uint32_t STATUS_WAITING_PERIPHERAL = 0x7;
   static const uint32_t STATUS_KILLING = 0x8;
   static const uint32_t STATUS_COMPLETING = 0x9;
   static const uint32_t STATUS_FAULTING_COMPLETING = 0xE;
   static const uint32_t STATUS_FAULTING = 0xF;

   static const uint32_t PERIPH_ID0_VALUE = 0x30;
   static const uint32_t PERIPH_ID1_VALUE = 0x13;
   static const uint32_t PERIPH_ID2_VALUE = 0x34;
   static const uint32_t PERIPH_ID3_VALUE = 0x00;
   static const uint32_t PCELL_ID0_VALUE = 0x0D;
   static const uint32_t PCELL_ID1_VALUE = 0xF0;
   static const uint32_t PCELL_ID2_VALUE = 0x05;
   static const uint32_t PCELL_ID3_VALUE = 0xB1;

   SC_HAS_PROCESS(dma_tlm);

   explicit dma_tlm(sc_core::sc_module_name name);
   void trace(sc_core::sc_trace_file *tf) const;

private:
   static const unsigned int NUM_CHANNELS = 8;
   static const unsigned int NUM_EVENTS = 32;
   static const unsigned int MAX_INSTRUCTION_BYTES = 6;
   static const unsigned int MAX_INSTRUCTIONS_PER_RUN = 4096;
   static const unsigned int MFIFO_CAPACITY_BYTES = 1024;

   static const uint32_t CCR_RESET_VALUE = 0x00800200u;

   static const uint32_t FTR_UNDEF_INSTR = 1u << 0;
   static const uint32_t FTR_OPERAND_INVALID = 1u << 1;
   static const uint32_t FTR_CH_EVENT_ERR = 1u << 5;
   static const uint32_t FTR_CH_RDWR_ERR = 1u << 7;
   static const uint32_t FTR_ST_DATA_UNAVAILABLE = 1u << 13;
   static const uint32_t FTR_INSTR_FETCH_ERR = 1u << 16;
   static const uint32_t FTR_DATA_WRITE_ERR = 1u << 17;
   static const uint32_t FTR_DATA_READ_ERR = 1u << 18;
   static const uint32_t FTR_DBG_INSTR = 1u << 30;
   static const uint32_t FTR_LOCKUP_ERR = 1u << 31;

   enum class RequestType {
      Single,
      Burst
   };

   struct ChannelState {
      uint32_t sar;
      uint32_t dar;
      uint32_t ccr;
      uint32_t cpc;
      uint32_t ftr;
      uint8_t lc0;
      uint8_t lc1;
      uint32_t loop_start0;
      uint32_t loop_start1;
      uint8_t wakeup_number;
      uint32_t status;
      bool nonsecure;
      bool dmawfp_periph;
      bool dmawfp_burst;
      RequestType request_type;
      std::deque<uint8_t> mfifo;
   };

   std::array<ChannelState, NUM_CHANNELS> m_channels;
   uint32_t m_dsr;
   uint32_t m_dpc;
   uint32_t m_inten;
   uint32_t m_int_event_ris;
   uint32_t m_intmis;
   uint32_t m_ftrd;
   uint32_t m_dbginst0;
   uint32_t m_dbginst1;
   uint32_t m_cr0;
   uint32_t m_cr1;
   uint32_t m_cr2;
   uint32_t m_cr3;
   uint32_t m_cr4;
   uint32_t m_crd;
   uint32_t m_wd;
   uint32_t m_irq_level;
   uint32_t m_pending_channels;
   bool m_debug_busy;
   bool m_manager_nonsecure;
   bool m_irq_abort_level;

   sc_core::sc_event m_channel_start_event;
   sc_core::sc_event m_output_changed;

   void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

   uint32_t read_reg(uint32_t addr);
   void write_reg(uint32_t addr, uint32_t data);

   void worker_thread();
   void handle_reset();
   void drive_outputs();

   void reset_state();
   void update_outputs();
   void schedule_channel(unsigned int channel);
   void start_channel(unsigned int channel, uint32_t pc, bool nonsecure);
   void kill_channel(unsigned int channel);
   void execute_channel(unsigned int channel);
   bool execute_instruction(unsigned int channel, const std::array<uint8_t, MAX_INSTRUCTION_BYTES> &instr,
                            unsigned int length, uint32_t instr_pc, bool debug_instruction);
   void execute_debug_instruction();
   void execute_manager_instruction(const std::array<uint8_t, MAX_INSTRUCTION_BYTES> &instr);

   unsigned int instruction_length(uint8_t opcode) const;
   bool fetch_instruction(unsigned int channel, std::array<uint8_t, MAX_INSTRUCTION_BYTES> &instr,
                          unsigned int &length);
   bool master_read(uint32_t addr, std::vector<uint8_t> &data, unsigned int length);
   bool master_write(uint32_t addr, const std::vector<uint8_t> &data);

   bool execute_load(unsigned int channel, bool single_transfer);
   bool execute_store(unsigned int channel, bool single_transfer);
   bool execute_store_zero(unsigned int channel);
   bool execute_loop_end(ChannelState &state, uint8_t opcode, uint8_t backwards_jump, uint32_t instr_pc);
   bool conditional_matches(ChannelState &state, uint8_t opcode) const;

   void send_event(uint32_t event);
   void resume_waiting_channels(uint32_t event);
   void set_channel_fault(unsigned int channel, uint32_t fault_bits, bool debug_instruction);
   void set_manager_fault(uint32_t fault_bits, bool debug_instruction);

   uint32_t channel_status_value(unsigned int channel) const;
   uint32_t fault_status_channels() const;
   bool validate_channel(unsigned int channel) const;
   bool validate_ccr(unsigned int channel, uint32_t ccr);
   bool valid_burst_size(uint32_t encoded_size) const;
   bool apply_endian_swap(uint32_t ccr, std::vector<uint8_t> &data) const;

   uint32_t source_transfer_bytes(const ChannelState &state, bool single_transfer) const;
   uint32_t destination_transfer_bytes(const ChannelState &state, bool single_transfer) const;
};

} // namespace cdc::components

#endif
