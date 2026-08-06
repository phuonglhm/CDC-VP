//Author: QuanNH107
//Verified: trangmn20

#include "dma_tlm.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace cdc::components {

namespace {

uint32_t load_u32_le(const unsigned char *data) {
   uint32_t value = 0;
   std::memcpy(&value, data, sizeof(value));
   return value;
}

void store_u32_le(unsigned char *data, uint32_t value) {
   std::memcpy(data, &value, sizeof(value));
}

uint32_t load_instr_u32(const std::array<uint8_t, 6> &instr) {
   return static_cast<uint32_t>(instr[2]) | (static_cast<uint32_t>(instr[3]) << 8) |
          (static_cast<uint32_t>(instr[4]) << 16) | (static_cast<uint32_t>(instr[5]) << 24);
}

std::string hex32(uint32_t value) {
   std::ostringstream os;
   os << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
   return os.str();
}

} // namespace

dma_tlm::dma_tlm(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , target_socket("target_socket")
    , master_socket("master_socket")
    , reset_n("reset_n")
    , irq("irq")
    , irq_abort("irq_abort")
    , m_dsr(0)
    , m_dpc(0)
    , m_inten(0)
    , m_int_event_ris(0)
    , m_intmis(0)
    , m_ftrd(0)
    , m_dbginst0(0)
    , m_dbginst1(0)
    , m_cr0(0)
    , m_cr1(0)
    , m_cr2(0)
    , m_cr3(0)
    , m_cr4(0)
    , m_crd(0)
    , m_wd(0)
    , m_irq_level(0)
    , m_pending_channels(0)
    , m_debug_busy(false)
    , m_manager_nonsecure(false)
    , m_irq_abort_level(false) {
   target_socket.register_b_transport(this, &dma_tlm::b_transport);

   reset_state();

   SC_THREAD(worker_thread);

   SC_METHOD(handle_reset);
   sensitive << reset_n.neg();

   SC_METHOD(drive_outputs);
   sensitive << m_output_changed;
   dont_initialize();
}

void dma_tlm::trace(sc_core::sc_trace_file *tf) const {
   if (tf == nullptr) {
      return;
   }

   sc_core::sc_trace(tf, m_dsr, "dma_tlm.dsr");
   sc_core::sc_trace(tf, m_dpc, "dma_tlm.dpc");
   sc_core::sc_trace(tf, m_inten, "dma_tlm.inten");
   sc_core::sc_trace(tf, m_int_event_ris, "dma_tlm.int_event_ris");
   sc_core::sc_trace(tf, m_intmis, "dma_tlm.intmis");
   sc_core::sc_trace(tf, m_ftrd, "dma_tlm.ftrd");
   sc_core::sc_trace(tf, m_debug_busy, "dma_tlm.debug_busy");
   sc_core::sc_trace(tf, m_irq_level, "dma_tlm.irq_level");
   sc_core::sc_trace(tf, m_irq_abort_level, "dma_tlm.irq_abort_level");

   for (unsigned int i = 0; i < NUM_CHANNELS; ++i) {
      const std::string prefix = "dma_tlm.ch" + std::to_string(i);
      sc_core::sc_trace(tf, m_channels[i].sar, prefix + ".sar");
      sc_core::sc_trace(tf, m_channels[i].dar, prefix + ".dar");
      sc_core::sc_trace(tf, m_channels[i].ccr, prefix + ".ccr");
      sc_core::sc_trace(tf, m_channels[i].cpc, prefix + ".cpc");
      sc_core::sc_trace(tf, m_channels[i].ftr, prefix + ".ftr");
      sc_core::sc_trace(tf, m_channels[i].status, prefix + ".status");
   }
}

void dma_tlm::set_channel_start_observer(channel_start_observer observer) {
   m_channel_start_observer = std::move(observer);
}

void dma_tlm::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
   (void)delay;

   if (trans.get_byte_enable_ptr() != nullptr) {
      trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
      return;
   }

   if (trans.get_data_length() < sizeof(uint32_t)) {
      trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
      return;
   }

   unsigned char *data = trans.get_data_ptr();
   if (data == nullptr) {
      trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
      return;
   }

   const uint32_t addr = static_cast<uint32_t>(trans.get_address() & 0xFFFu);

   switch (trans.get_command()) {
   case tlm::TLM_READ_COMMAND:
      store_u32_le(data, read_reg(addr));
      trans.set_response_status(tlm::TLM_OK_RESPONSE);
      break;

   case tlm::TLM_WRITE_COMMAND:
      write_reg(addr, load_u32_le(data));
      trans.set_response_status(tlm::TLM_OK_RESPONSE);
      break;

   default:
      trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
      break;
   }
}

uint32_t dma_tlm::read_reg(uint32_t addr) {
   update_outputs();

   switch (addr) {
   case DSR:
      return (m_manager_nonsecure ? (1u << 9) : 0u) | (m_dsr & 0x1FFu);
   case DPC:
      return m_dpc;
   case INTEN:
      return m_inten;
   case INT_EVENT_RIS:
      return m_int_event_ris;
   case INTMIS:
      return m_intmis;
   case INTCLR:
      return 0;
   case FSRD:
      return ((m_dsr & 0xFu) == STATUS_FAULTING) ? 1u : 0u;
   case FSRC:
      return fault_status_channels();
   case FTRD:
      return m_ftrd;
   case DBGSTATUS:
      return m_debug_busy ? 1u : 0u;
   case DBGCMD:
   case DBGINST0:
   case DBGINST1:
      return 0;
   case CR0:
      return m_cr0;
   case CR1:
      return m_cr1;
   case CR2:
      return m_cr2;
   case CR3:
      return m_cr3;
   case CR4:
      return m_cr4;
   case CRD:
      return m_crd;
   case WD:
      return m_wd & 1u;
   case PERIPH_ID0:
      return PERIPH_ID0_VALUE;
   case PERIPH_ID1:
      return PERIPH_ID1_VALUE;
   case PERIPH_ID2:
      return PERIPH_ID2_VALUE;
   case PERIPH_ID3:
      return PERIPH_ID3_VALUE;
   case PCELL_ID0:
      return PCELL_ID0_VALUE;
   case PCELL_ID1:
      return PCELL_ID1_VALUE;
   case PCELL_ID2:
      return PCELL_ID2_VALUE;
   case PCELL_ID3:
      return PCELL_ID3_VALUE;
   default:
      break;
   }

   if (addr >= FTR0 && addr < FTR0 + NUM_CHANNELS * sizeof(uint32_t) && ((addr - FTR0) % sizeof(uint32_t)) == 0) {
      return m_channels[(addr - FTR0) / sizeof(uint32_t)].ftr;
   }

   if (addr >= CSR0 && addr < CSR0 + NUM_CHANNELS * CHANNEL_STATUS_STRIDE) {
      const uint32_t rel = addr - CSR0;
      const unsigned int channel = rel / CHANNEL_STATUS_STRIDE;
      if ((rel % CHANNEL_STATUS_STRIDE) == 0) {
         return channel_status_value(channel);
      }
      if ((rel % CHANNEL_STATUS_STRIDE) == (CPC0 - CSR0)) {
         return m_channels[channel].cpc;
      }
   }

   if (addr >= SAR0 && addr < SAR0 + NUM_CHANNELS * CHANNEL_AXI_STRIDE) {
      const uint32_t rel = addr - SAR0;
      const unsigned int channel = rel / CHANNEL_AXI_STRIDE;
      switch (rel % CHANNEL_AXI_STRIDE) {
      case SAR0 - SAR0:
         return m_channels[channel].sar;
      case DAR0 - SAR0:
         return m_channels[channel].dar;
      case CCR0 - SAR0:
         return m_channels[channel].ccr;
      case LC0_0 - SAR0:
         return m_channels[channel].lc0;
      case LC1_0 - SAR0:
         return m_channels[channel].lc1;
      default:
         return 0;
      }
   }

   return 0;
}

void dma_tlm::write_reg(uint32_t addr, uint32_t data) {
   switch (addr) {
   case INTEN:
      m_inten = data;
      update_outputs();
      std::cout << sc_core::sc_time_stamp() << " [DMA] INTEN <= " << hex32(m_inten) << '\n';
      return;

   case INTCLR:
      m_int_event_ris &= ~data;
      update_outputs();
      std::cout << sc_core::sc_time_stamp() << " [DMA] INTCLR <= " << hex32(data) << '\n';
      return;

   case DBGINST0:
      if (!m_debug_busy) {
         m_dbginst0 = data;
      }
      std::cout << sc_core::sc_time_stamp() << " [DMA] DBGINST0 <= " << hex32(data) << '\n';
      return;

   case DBGINST1:
      if (!m_debug_busy) {
         m_dbginst1 = data;
      }
      std::cout << sc_core::sc_time_stamp() << " [DMA] DBGINST1 <= " << hex32(data) << '\n';
      return;

   case DBGCMD:
      if (!m_debug_busy && ((data & 0x3u) == 0)) {
         execute_debug_instruction();
      } else if ((data & 0x3u) != 0) {
         std::cout << sc_core::sc_time_stamp() << " [DMA] ignored reserved DBGCMD=" << hex32(data) << '\n';
      }
      return;

   case WD:
      m_wd = data & 1u;
      std::cout << sc_core::sc_time_stamp() << " [DMA] WD <= " << hex32(m_wd) << '\n';
      return;

   default:
      break;
   }

   std::cout << sc_core::sc_time_stamp() << " [DMA] ignored write to read-only/reserved addr=0x" << std::hex
             << std::setw(3) << std::setfill('0') << addr << " data=" << hex32(data) << std::dec << std::setfill(' ')
             << '\n';
}

void dma_tlm::worker_thread() {
   while (true) {
      wait(m_channel_start_event);

      while (m_pending_channels != 0) {
         const uint32_t pending = m_pending_channels;
         for (unsigned int channel = 0; channel < NUM_CHANNELS; ++channel) {
            if ((pending & (1u << channel)) == 0) {
               continue;
            }

            m_pending_channels &= ~(1u << channel);
            execute_channel(channel);
            wait(sc_core::SC_ZERO_TIME);
         }
      }
   }
}

void dma_tlm::handle_reset() {
   reset_state();
   update_outputs();
}

void dma_tlm::drive_outputs() {
   irq.write(m_irq_level);
   irq_abort.write(m_irq_abort_level);
}

void dma_tlm::reset_state() {
   m_dsr = STATUS_STOPPED;
   m_dpc = 0;
   m_inten = 0;
   m_int_event_ris = 0;
   m_intmis = 0;
   m_ftrd = 0;
   m_dbginst0 = 0;
   m_dbginst1 = 0;
   m_cr0 = ((NUM_EVENTS - 1u) << 17) | ((NUM_CHANNELS - 1u) << 4);
   m_cr1 = (3u << 4) | 2u;
   m_cr2 = 0;
   m_cr3 = 0;
   m_cr4 = 0;
   m_crd = ((MFIFO_CAPACITY_BYTES - 1u) << 20) | (3u << 16) | (3u << 12) | (3u << 8) | (3u << 4) | 2u;
   m_wd = 0;
   m_irq_level = 0;
   m_pending_channels = 0;
   m_debug_busy = false;
   m_manager_nonsecure = false;
   m_irq_abort_level = false;

   for (auto &channel : m_channels) {
      channel.sar = 0;
      channel.dar = 0;
      channel.ccr = CCR_RESET_VALUE;
      channel.cpc = 0;
      channel.ftr = 0;
      channel.lc0 = 0;
      channel.lc1 = 0;
      channel.loop_start0 = 0;
      channel.loop_start1 = 0;
      channel.wakeup_number = 0;
      channel.status = STATUS_STOPPED;
      channel.nonsecure = false;
      channel.dmawfp_periph = false;
      channel.dmawfp_burst = false;
      channel.request_type = RequestType::Single;
      channel.mfifo.clear();
   }
}

void dma_tlm::update_outputs() {
   m_intmis = m_int_event_ris & m_inten;
   m_irq_level = m_intmis;
   m_irq_abort_level = (((m_dsr & 0xFu) == STATUS_FAULTING) || fault_status_channels() != 0);
   m_output_changed.notify(sc_core::SC_ZERO_TIME);
}

void dma_tlm::schedule_channel(unsigned int channel) {
   if (!validate_channel(channel)) {
      return;
   }

   m_pending_channels |= (1u << channel);
   m_channel_start_event.notify(sc_core::SC_ZERO_TIME);
}

void dma_tlm::start_channel(unsigned int channel, uint32_t pc, bool nonsecure) {
   if (!validate_channel(channel)) {
      set_manager_fault(FTR_OPERAND_INVALID, true);
      return;
   }

   ChannelState &state = m_channels[channel];
   if (state.status != STATUS_STOPPED) {
      std::cout << sc_core::sc_time_stamp() << " [DMA] DMAGO ignored, channel " << channel << " is not stopped\n";
      return;
   }

   state.cpc = pc;
   state.status = STATUS_EXECUTING;
   state.nonsecure = nonsecure;
   state.ftr = 0;
   state.lc0 = 0;
   state.lc1 = 0;
   state.loop_start0 = 0;
   state.loop_start1 = 0;
   state.mfifo.clear();
   std::cout << sc_core::sc_time_stamp() << " [DMA] DMAGO channel " << channel << " pc=" << hex32(pc)
             << " ns=" << nonsecure << '\n';
   if (m_channel_start_observer) {
      m_channel_start_observer(channel);
   }
   schedule_channel(channel);
}

void dma_tlm::kill_channel(unsigned int channel) {
   if (!validate_channel(channel)) {
      return;
   }

   ChannelState &state = m_channels[channel];
   state.status = STATUS_STOPPED;
   state.ftr = 0;
   state.mfifo.clear();
   m_pending_channels &= ~(1u << channel);
   update_outputs();
   std::cout << sc_core::sc_time_stamp() << " [DMA] DMAKILL channel " << channel << '\n';
}

void dma_tlm::execute_channel(unsigned int channel) {
   if (!validate_channel(channel)) {
      return;
   }

   ChannelState &state = m_channels[channel];
   unsigned int instructions = 0;

   while (reset_n.read() && state.status == STATUS_EXECUTING) {
      if (++instructions > MAX_INSTRUCTIONS_PER_RUN) {
         set_channel_fault(channel, FTR_LOCKUP_ERR, false);
         return;
      }

      std::array<uint8_t, MAX_INSTRUCTION_BYTES> instr{};
      unsigned int length = 0;
      const uint32_t instr_pc = state.cpc;

      if (!fetch_instruction(channel, instr, length)) {
         return;
      }

      if (!execute_instruction(channel, instr, length, instr_pc, false)) {
         return;
      }

      wait(sc_core::SC_ZERO_TIME);
   }
}

bool dma_tlm::execute_instruction(unsigned int channel, const std::array<uint8_t, MAX_INSTRUCTION_BYTES> &instr,
                                  unsigned int length, uint32_t instr_pc, bool debug_instruction) {
   ChannelState &state = m_channels[channel];
   const uint8_t opcode = instr[0];

   switch (opcode) {
   case 0x00:
      state.status = STATUS_STOPPED;
      std::cout << sc_core::sc_time_stamp() << " [DMA] channel " << channel << " DMAEND\n";
      update_outputs();
      return true;

   case 0x01:
      state.status = STATUS_STOPPED;
      state.mfifo.clear();
      update_outputs();
      return true;

   case 0x04:
   case 0x05:
   case 0x07:
      if (!conditional_matches(state, opcode)) {
         return true;
      }
      return execute_load(channel, (opcode & 0x1u) != 0 && ((opcode & 0x2u) == 0));

   case 0x08:
   case 0x09:
   case 0x0B:
      if (!conditional_matches(state, opcode)) {
         return true;
      }
      return execute_store(channel, (opcode & 0x1u) != 0 && ((opcode & 0x2u) == 0));

   case 0x0C:
      return execute_store_zero(channel);

   case 0x30:
   case 0x31:
   case 0x32:
   case 0x33:
      if (length != 2) {
         set_channel_fault(channel, FTR_UNDEF_INSTR, debug_instruction);
         return false;
      }
      state.status = STATUS_WAITING_PERIPHERAL;
      state.wakeup_number = instr[1] >> 3;
      state.dmawfp_periph = (opcode & 0x1u) != 0;
      state.dmawfp_burst = (opcode & 0x2u) != 0;
      state.request_type = state.dmawfp_burst ? RequestType::Burst : RequestType::Single;
      std::cout << sc_core::sc_time_stamp() << " [DMA] channel " << channel << " waiting for peripheral "
                << static_cast<unsigned int>(state.wakeup_number) << '\n';
      return true;

   case 0x54:
   case 0x56: {
      if (length != 3) {
         set_channel_fault(channel, FTR_UNDEF_INSTR, debug_instruction);
         return false;
      }
      const uint32_t imm = static_cast<uint32_t>(instr[1]) | (static_cast<uint32_t>(instr[2]) << 8);
      if ((opcode & 0x2u) != 0) {
         state.dar += imm;
      } else {
         state.sar += imm;
      }
      return true;
   }

   case 0x5C:
   case 0x5E: {
      if (length != 3) {
         set_channel_fault(channel, FTR_UNDEF_INSTR, debug_instruction);
         return false;
      }
      const uint32_t imm = 0xFFFF0000u | static_cast<uint32_t>(instr[1]) |
                           (static_cast<uint32_t>(instr[2]) << 8);
      if ((opcode & 0x2u) != 0) {
         state.dar += imm;
      } else {
         state.sar += imm;
      }
      return true;
   }

   case 0x12:
   case 0x13:
      return true;

   case 0x18:
      return true;

   case 0x20:
      if (length != 2) {
         set_channel_fault(channel, FTR_UNDEF_INSTR, debug_instruction);
         return false;
      }
      state.lc0 = instr[1];
      state.loop_start0 = state.cpc;
      return true;

   case 0x22:
      if (length != 2) {
         set_channel_fault(channel, FTR_UNDEF_INSTR, debug_instruction);
         return false;
      }
      state.lc1 = instr[1];
      state.loop_start1 = state.cpc;
      return true;

   case 0x28:
   case 0x29:
   case 0x2A:
   case 0x2B:
   case 0x2C:
   case 0x2D:
   case 0x2E:
   case 0x2F:
   case 0x38:
   case 0x39:
   case 0x3A:
   case 0x3B:
   case 0x3C:
   case 0x3D:
   case 0x3E:
   case 0x3F:
      if (length != 2) {
         set_channel_fault(channel, FTR_UNDEF_INSTR, debug_instruction);
         return false;
      }
      return execute_loop_end(state, opcode, instr[1], instr_pc);

   case 0x36:
      if ((instr[1] >> 3) >= NUM_EVENTS) {
         set_channel_fault(channel, FTR_OPERAND_INVALID, debug_instruction);
         return false;
      }
      state.status = STATUS_WAITING_EVENT;
      state.wakeup_number = instr[1] >> 3;
      state.dmawfp_periph = false;
      state.dmawfp_burst = false;
      std::cout << sc_core::sc_time_stamp() << " [DMA] channel " << channel << " waiting for event "
                << static_cast<unsigned int>(state.wakeup_number) << '\n';
      return true;

   case 0x34:
      if ((instr[1] >> 3) >= NUM_EVENTS) {
         set_channel_fault(channel, FTR_OPERAND_INVALID, debug_instruction);
         return false;
      }
      send_event(instr[1] >> 3);
      return true;

   case 0xBC: {
      if (length != 6) {
         set_channel_fault(channel, FTR_UNDEF_INSTR, debug_instruction);
         return false;
      }
      const uint8_t reg = instr[1] >> 3;
      const uint32_t imm = load_instr_u32(instr);
      switch (reg) {
      case 0:
         state.sar = imm;
         break;
      case 1:
         if (!validate_ccr(channel, imm)) {
            set_channel_fault(channel, FTR_OPERAND_INVALID, debug_instruction);
            return false;
         }
         state.ccr = imm;
         break;
      case 2:
         state.dar = imm;
         break;
      default:
         set_channel_fault(channel, FTR_OPERAND_INVALID, debug_instruction);
         return false;
      }
      return true;
   }

   default:
      set_channel_fault(channel, FTR_UNDEF_INSTR, debug_instruction);
      std::cout << sc_core::sc_time_stamp() << " [DMA] undefined channel opcode 0x" << std::hex
                << static_cast<unsigned int>(opcode) << std::dec << " at " << hex32(instr_pc) << '\n';
      return false;
   }
}

void dma_tlm::execute_debug_instruction() {
   std::array<uint8_t, MAX_INSTRUCTION_BYTES> instr{};
   instr[0] = static_cast<uint8_t>((m_dbginst0 >> 16) & 0xFFu);
   instr[1] = static_cast<uint8_t>((m_dbginst0 >> 24) & 0xFFu);
   instr[2] = static_cast<uint8_t>(m_dbginst1 & 0xFFu);
   instr[3] = static_cast<uint8_t>((m_dbginst1 >> 8) & 0xFFu);
   instr[4] = static_cast<uint8_t>((m_dbginst1 >> 16) & 0xFFu);
   instr[5] = static_cast<uint8_t>((m_dbginst1 >> 24) & 0xFFu);

   const bool channel_thread = (m_dbginst0 & 0x1u) != 0;
   const unsigned int channel = (m_dbginst0 >> 8) & 0x7u;

   m_debug_busy = true;
   update_outputs();

   if (channel_thread) {
      if (instr[0] == 0x01) {
         kill_channel(channel);
      } else if (instr[0] == 0x18) {
         std::cout << sc_core::sc_time_stamp() << " [DMA] debug DMANOP channel " << channel << '\n';
      } else if (instr[0] == 0x34) {
         if ((instr[1] >> 3) >= NUM_EVENTS) {
            set_channel_fault(channel, FTR_OPERAND_INVALID, true);
         } else {
            send_event(instr[1] >> 3);
         }
      } else {
         set_channel_fault(channel, FTR_OPERAND_INVALID, true);
      }
   } else {
      execute_manager_instruction(instr);
   }

   m_debug_busy = false;
   update_outputs();
}

void dma_tlm::execute_manager_instruction(const std::array<uint8_t, MAX_INSTRUCTION_BYTES> &instr) {
   const uint8_t opcode = instr[0];

   switch (opcode) {
   case 0x00:
      m_dsr = STATUS_STOPPED;
      return;

   case 0x01:
      m_dsr = STATUS_STOPPED;
      m_ftrd = 0;
      update_outputs();
      return;

   case 0xA0:
   case 0xA2: {
      const unsigned int channel = instr[1] & 0x7u;
      const bool nonsecure = (opcode & 0x2u) != 0;
      if (m_manager_nonsecure && !nonsecure) {
         set_manager_fault(1u << 4, true);
         return;
      }
      start_channel(channel, load_instr_u32(instr), nonsecure);
      return;
   }

   case 0x34:
      if ((instr[1] >> 3) >= NUM_EVENTS) {
         set_manager_fault(FTR_OPERAND_INVALID, true);
         return;
      }
      send_event(instr[1] >> 3);
      return;

   case 0x18:
      return;

   default:
      set_manager_fault(FTR_UNDEF_INSTR, true);
      std::cout << sc_core::sc_time_stamp() << " [DMA] undefined manager opcode 0x" << std::hex
                << static_cast<unsigned int>(opcode) << std::dec << '\n';
      return;
   }
}

unsigned int dma_tlm::instruction_length(uint8_t opcode) const {
   switch (opcode) {
   case 0x00:
   case 0x01:
   case 0x04:
   case 0x05:
   case 0x06:
   case 0x07:
   case 0x08:
   case 0x09:
   case 0x0A:
   case 0x0B:
   case 0x0C:
   case 0x12:
   case 0x13:
   case 0x18:
      return 1;

   case 0x20:
   case 0x22:
   case 0x25:
   case 0x27:
   case 0x24:
   case 0x26:
   case 0x28:
   case 0x29:
   case 0x2A:
   case 0x2B:
   case 0x2C:
   case 0x2D:
   case 0x2E:
   case 0x2F:
   case 0x32:
   case 0x30:
   case 0x31:
   case 0x33:
   case 0x34:
   case 0x35:
   case 0x36:
   case 0x37:
   case 0x38:
   case 0x39:
   case 0x3A:
   case 0x3B:
   case 0x3C:
   case 0x3D:
   case 0x3E:
   case 0x3F:
      return 2;

   case 0x54:
   case 0x56:
   case 0x5C:
   case 0x5E:
      return 3;

   case 0xA0:
   case 0xA2:
   case 0xBC:
      return 6;

   default:
      return 1;
   }
}

bool dma_tlm::fetch_instruction(unsigned int channel, std::array<uint8_t, MAX_INSTRUCTION_BYTES> &instr,
                                unsigned int &length) {
   ChannelState &state = m_channels[channel];
   std::vector<uint8_t> first_byte(1, 0);
   if (!master_read(state.cpc, first_byte, 1)) {
      set_channel_fault(channel, FTR_INSTR_FETCH_ERR, false);
      return false;
   }

   instr[0] = first_byte[0];
   length = instruction_length(instr[0]);

   std::vector<uint8_t> bytes(length, 0);
   if (!master_read(state.cpc, bytes, length)) {
      set_channel_fault(channel, FTR_INSTR_FETCH_ERR, false);
      return false;
   }

   std::copy(bytes.begin(), bytes.end(), instr.begin());
   state.cpc += length;
   return true;
}

bool dma_tlm::master_read(uint32_t addr, std::vector<uint8_t> &data, unsigned int length) {
   data.assign(length, 0);
   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   trans.set_command(tlm::TLM_READ_COMMAND);
   trans.set_address(addr);
   trans.set_data_ptr(data.data());
   trans.set_data_length(length);
   trans.set_streaming_width(length);
   trans.set_byte_enable_ptr(nullptr);
   trans.set_dmi_allowed(false);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   master_socket->b_transport(trans, delay);
   wait(delay);

   return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
}

bool dma_tlm::master_write(uint32_t addr, const std::vector<uint8_t> &data) {
   std::vector<uint8_t> local = data;
   tlm::tlm_generic_payload trans;
   sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

   trans.set_command(tlm::TLM_WRITE_COMMAND);
   trans.set_address(addr);
   trans.set_data_ptr(local.data());
   trans.set_data_length(local.size());
   trans.set_streaming_width(local.size());
   trans.set_byte_enable_ptr(nullptr);
   trans.set_dmi_allowed(false);
   trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

   master_socket->b_transport(trans, delay);
   wait(delay);

   return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
}

bool dma_tlm::execute_load(unsigned int channel, bool single_transfer) {
   ChannelState &state = m_channels[channel];
   const uint32_t bytes = source_transfer_bytes(state, single_transfer);
   if (bytes == 0 || state.mfifo.size() + bytes > MFIFO_CAPACITY_BYTES) {
      set_channel_fault(channel, FTR_LOCKUP_ERR, false);
      return false;
   }

   std::vector<uint8_t> data;
   if (!master_read(state.sar, data, bytes)) {
      set_channel_fault(channel, FTR_DATA_READ_ERR, false);
      return false;
   }

   if (!apply_endian_swap(state.ccr, data)) {
      set_channel_fault(channel, FTR_OPERAND_INVALID, false);
      return false;
   }

   for (uint8_t byte : data) {
      state.mfifo.push_back(byte);
   }

   if ((state.ccr & CCR_SRC_INC) != 0) {
      state.sar += bytes;
   }

   return true;
}

bool dma_tlm::execute_store(unsigned int channel, bool single_transfer) {
   ChannelState &state = m_channels[channel];
   const uint32_t bytes = destination_transfer_bytes(state, single_transfer);
   if (bytes == 0 || state.mfifo.size() < bytes) {
      set_channel_fault(channel, FTR_ST_DATA_UNAVAILABLE, false);
      return false;
   }

   std::vector<uint8_t> data(bytes, 0);
   for (uint32_t i = 0; i < bytes; ++i) {
      data[i] = state.mfifo.front();
      state.mfifo.pop_front();
   }

   if (!master_write(state.dar, data)) {
      set_channel_fault(channel, FTR_DATA_WRITE_ERR, false);
      return false;
   }

   if ((state.ccr & CCR_DST_INC) != 0) {
      state.dar += bytes;
   }

   return true;
}

bool dma_tlm::execute_store_zero(unsigned int channel) {
   ChannelState &state = m_channels[channel];
   const uint32_t bytes = destination_transfer_bytes(state, false);
   if (bytes == 0) {
      set_channel_fault(channel, FTR_OPERAND_INVALID, false);
      return false;
   }

   std::vector<uint8_t> data(bytes, 0);
   if (!master_write(state.dar, data)) {
      set_channel_fault(channel, FTR_DATA_WRITE_ERR, false);
      return false;
   }

   if ((state.ccr & CCR_DST_INC) != 0) {
      state.dar += bytes;
   }

   return true;
}

bool dma_tlm::execute_loop_end(ChannelState &state, uint8_t opcode, uint8_t backwards_jump, uint32_t instr_pc) {
   const bool forever = ((opcode >> 4) & 0x1u) == 0;
   const bool use_lc1 = ((opcode >> 2) & 0x1u) != 0;

   if (forever) {
      state.cpc = instr_pc - backwards_jump;
      return true;
   }

   uint8_t &counter = use_lc1 ? state.lc1 : state.lc0;
   if (counter == 0) {
      return true;
   }

   --counter;
   state.cpc = instr_pc - backwards_jump;
   return true;
}

bool dma_tlm::conditional_matches(ChannelState &state, uint8_t opcode) const {
   const bool conditional = (opcode & 0x1u) != 0;
   if (!conditional) {
      return true;
   }

   const bool wants_burst = (opcode & 0x2u) != 0;
   return wants_burst ? state.request_type == RequestType::Burst : state.request_type == RequestType::Single;
}

void dma_tlm::send_event(uint32_t event) {
   if (event >= NUM_EVENTS) {
      set_manager_fault(FTR_OPERAND_INVALID, false);
      return;
   }

   const uint32_t mask = 1u << event;
   m_int_event_ris |= mask;
   resume_waiting_channels(event);
   update_outputs();
   std::cout << sc_core::sc_time_stamp() << " [DMA] DMASEV event " << event << '\n';
}

void dma_tlm::resume_waiting_channels(uint32_t event) {
   for (unsigned int channel = 0; channel < NUM_CHANNELS; ++channel) {
      ChannelState &state = m_channels[channel];
      if (state.status == STATUS_WAITING_EVENT && state.wakeup_number == event) {
         state.status = STATUS_EXECUTING;
         m_int_event_ris &= ~(1u << event);
         schedule_channel(channel);
      }
   }
}

void dma_tlm::set_channel_fault(unsigned int channel, uint32_t fault_bits, bool debug_instruction) {
   if (!validate_channel(channel)) {
      set_manager_fault(FTR_OPERAND_INVALID, debug_instruction);
      return;
   }

   ChannelState &state = m_channels[channel];
   state.status = STATUS_FAULTING;
   state.ftr = fault_bits | (debug_instruction ? FTR_DBG_INSTR : 0u);
   m_pending_channels &= ~(1u << channel);
   update_outputs();
   std::cout << sc_core::sc_time_stamp() << " [DMA] channel " << channel << " fault " << hex32(state.ftr) << '\n';
}

void dma_tlm::set_manager_fault(uint32_t fault_bits, bool debug_instruction) {
   m_dsr = STATUS_FAULTING | (m_manager_nonsecure ? (1u << 9) : 0u);
   m_ftrd = fault_bits | (debug_instruction ? FTR_DBG_INSTR : 0u);
   update_outputs();
   std::cout << sc_core::sc_time_stamp() << " [DMA] manager fault " << hex32(m_ftrd) << '\n';
}

uint32_t dma_tlm::channel_status_value(unsigned int channel) const {
   const ChannelState &state = m_channels[channel];
   return (state.status & 0xFu) | ((static_cast<uint32_t>(state.wakeup_number) & 0x1Fu) << 4) |
          (state.dmawfp_burst ? (1u << 14) : 0u) | (state.dmawfp_periph ? (1u << 15) : 0u) |
          (state.nonsecure ? (1u << 21) : 0u);
}

uint32_t dma_tlm::fault_status_channels() const {
   uint32_t status = 0;
   for (unsigned int channel = 0; channel < NUM_CHANNELS; ++channel) {
      if (m_channels[channel].status == STATUS_FAULTING ||
          m_channels[channel].status == STATUS_FAULTING_COMPLETING) {
         status |= (1u << channel);
      }
   }
   return status;
}

bool dma_tlm::validate_channel(unsigned int channel) const {
   return channel < NUM_CHANNELS;
}

bool dma_tlm::validate_ccr(unsigned int channel, uint32_t ccr) {
   const uint32_t src_size = (ccr >> CCR_SRC_BURST_SIZE_SHIFT) & 0x7u;
   const uint32_t dst_size = (ccr >> CCR_DST_BURST_SIZE_SHIFT) & 0x7u;
   const uint32_t endian_swap = (ccr >> 28) & 0x7u;

   if (!valid_burst_size(src_size) || !valid_burst_size(dst_size) || endian_swap > 4) {
      return false;
   }

   if (m_channels[channel].nonsecure) {
      const uint32_t src_prot = (ccr >> CCR_SRC_PROT_SHIFT) & 0x7u;
      const uint32_t dst_prot = (ccr >> CCR_DST_PROT_SHIFT) & 0x7u;
      if ((src_prot & 0x2u) == 0 || (dst_prot & 0x2u) == 0) {
         return false;
      }
   }

   return true;
}

bool dma_tlm::valid_burst_size(uint32_t encoded_size) const {
   return encoded_size <= 4;
}

bool dma_tlm::apply_endian_swap(uint32_t ccr, std::vector<uint8_t> &data) const {
   const uint32_t encoded = (ccr >> 28) & 0x7u;
   if (encoded == 0) {
      return true;
   }
   if (encoded > 4) {
      return false;
   }

   const uint32_t group_size = 1u << encoded;
   if (group_size == 0 || (data.size() % group_size) != 0) {
      return false;
   }

   for (std::size_t i = 0; i < data.size(); i += group_size) {
      std::reverse(data.begin() + static_cast<std::ptrdiff_t>(i),
                   data.begin() + static_cast<std::ptrdiff_t>(i + group_size));
   }

   return true;
}

uint32_t dma_tlm::source_transfer_bytes(const ChannelState &state, bool single_transfer) const {
   const uint32_t burst_len = single_transfer ? 1u : (((state.ccr >> CCR_SRC_BURST_LEN_SHIFT) & 0xFu) + 1u);
   const uint32_t burst_size = 1u << ((state.ccr >> CCR_SRC_BURST_SIZE_SHIFT) & 0x7u);
   return burst_len * burst_size;
}

uint32_t dma_tlm::destination_transfer_bytes(const ChannelState &state, bool single_transfer) const {
   const uint32_t burst_len = single_transfer ? 1u : (((state.ccr >> CCR_DST_BURST_LEN_SHIFT) & 0xFu) + 1u);
   const uint32_t burst_size = 1u << ((state.ccr >> CCR_DST_BURST_SIZE_SHIFT) & 0x7u);
   return burst_len * burst_size;
}

} // namespace cdc::components
