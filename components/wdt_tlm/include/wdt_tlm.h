#ifndef WDT_TLM_H
#define WDT_TLM_H

#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

class wdt_tlm : public sc_core::sc_module {
public:
   tlm_utils::simple_target_socket<wdt_tlm> target_socket;
   sc_core::sc_in<bool> reset_n;
   sc_core::sc_out<bool> irq;
   sc_core::sc_out<bool> reset_o;

   static const uint32_t WDOG_LOAD = 0x000;
   static const uint32_t WDOG_VALUE = 0x004;
   static const uint32_t WDOG_CONTROL = 0x008;
   static const uint32_t WDOG_INTCLR = 0x00C;
   static const uint32_t WDOG_RIS = 0x010;
   static const uint32_t WDOG_MIS = 0x014;
   static const uint32_t WDOG_LOCK = 0xC00;

   static const uint32_t WDOG_PERIPHID0 = 0xFE0;
   static const uint32_t WDOG_PERIPHID1 = 0xFE4;
   static const uint32_t WDOG_PERIPHID2 = 0xFE8;
   static const uint32_t WDOG_PERIPHID3 = 0xFEC;
   static const uint32_t WDOG_PCELLID0 = 0xFF0;
   static const uint32_t WDOG_PCELLID1 = 0xFF4;
   static const uint32_t WDOG_PCELLID2 = 0xFF8;
   static const uint32_t WDOG_PCELLID3 = 0xFFC;

   static const uint32_t CTRL_INTEN = 1u << 0;
   static const uint32_t CTRL_RESEN = 1u << 1;

   static const uint32_t LOCK_UNLOCK_VALUE = 0x1ACCE551u;

   static const uint32_t PERIPHID0_VALUE = 0x05;
   static const uint32_t PERIPHID1_VALUE = 0x18;
   static const uint32_t PERIPHID2_VALUE = 0x14;
   static const uint32_t PERIPHID3_VALUE = 0x00;
   static const uint32_t PCELLID0_VALUE = 0x0D;
   static const uint32_t PCELLID1_VALUE = 0xF0;
   static const uint32_t PCELLID2_VALUE = 0x05;
   static const uint32_t PCELLID3_VALUE = 0xB1;

   SC_HAS_PROCESS(wdt_tlm);

   wdt_tlm(sc_core::sc_module_name name, sc_core::sc_time tick_period);
   void trace(sc_core::sc_trace_file *tf) const;

private:
   static const uint32_t CONTROL_WRITABLE_MASK = CTRL_INTEN | CTRL_RESEN;
   static const uint32_t RIS_ASSERTED = 1u;

   uint32_t m_load;
   uint32_t m_counter;
   uint32_t m_control;
   uint32_t m_ris;
   uint32_t m_mis;
   bool m_locked;
   bool m_reset_asserted;
   bool m_irq_level;
   bool m_reset_level;
   sc_core::sc_time m_tick_period;
   sc_core::sc_event m_output_changed;

   void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

   uint32_t read_reg(uint32_t addr);
   void write_reg(uint32_t addr, uint32_t data);

   void counter_thread();
   void handle_timeout();
   void drive_outputs();

   void reload_counter();
   void clear_interrupt();
   void update_outputs();

   bool interrupt_enabled() const;
   bool reset_enabled() const;
};

} // namespace cdc::components

#endif
