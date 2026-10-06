// SPDX-License-Identifier: Apache-2.0
// Test RAM for the FX1 ISP DMA masters: one target socket per master,
// configurable latency, address-range fault injection and an access log.
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace fx1_test {

struct test_memory : sc_core::sc_module {
   tlm_utils::simple_target_socket<test_memory> idma;
   tlm_utils::simple_target_socket<test_memory> odma;

   struct fault {
      tlm::tlm_command cmd;
      std::uint64_t lo;  // [lo, hi)
      std::uint64_t hi;
      int remaining;  // < 0: every matching access fails
   };
   struct access {
      sc_core::sc_time start;  // when the access takes effect
      sc_core::sc_time end;    // start + latency (response time)
      tlm::tlm_command cmd;
      std::uint64_t addr;
      std::uint32_t len;
      bool from_idma;
      tlm::tlm_response_status status;
   };

   std::uint64_t base;
   std::vector<std::uint8_t> bytes;
   sc_core::sc_time latency = sc_core::SC_ZERO_TIME;
   // Blocking target: b_transport itself waits `latency` (the initiator's
   // thread is suspended inside the target) and completes the transfer at the
   // end; otherwise the latency is annotated. `active_*` count transfers
   // currently inside the target.
   bool blocking = false;
   unsigned active_idma = 0, active_odma = 0;
   std::vector<fault> faults;
   std::vector<access> log;

   test_memory(sc_core::sc_module_name n, std::uint64_t base_addr, std::size_t size)
       : sc_core::sc_module(n), idma("idma"), odma("odma"), base(base_addr), bytes(size, 0) {
      idma.register_b_transport(this, &test_memory::b_idma);
      odma.register_b_transport(this, &test_memory::b_odma);
   }

   std::uint8_t *at(std::uint64_t addr) { return bytes.data() + (addr - base); }
   void fill(std::uint64_t addr, std::size_t len, std::uint8_t v) { std::memset(at(addr), v, len); }
   void put16(std::uint64_t addr, std::uint16_t v) {
      at(addr)[0] = static_cast<std::uint8_t>(v);
      at(addr)[1] = static_cast<std::uint8_t>(v >> 8);
   }

   std::size_t count(tlm::tlm_command cmd, bool from_idma) const {
      std::size_t n = 0;
      for (const access &a : log) {
         n += a.cmd == cmd && a.from_idma == from_idma;
      }
      return n;
   }
   std::uint64_t bytes_written() const {
      std::uint64_t n = 0;
      for (const access &a : log) {
         if (a.cmd == tlm::TLM_WRITE_COMMAND && a.status == tlm::TLM_OK_RESPONSE) {
            n += a.len;
         }
      }
      return n;
   }

private:
   void b_idma(tlm::tlm_generic_payload &t, sc_core::sc_time &d) { serve(t, d, true); }
   void b_odma(tlm::tlm_generic_payload &t, sc_core::sc_time &d) { serve(t, d, false); }

   void serve(tlm::tlm_generic_payload &t, sc_core::sc_time &delay, bool from_idma) {
      const std::uint64_t addr = t.get_address();
      const std::uint32_t len = t.get_data_length();
      const sc_core::sc_time start = sc_core::sc_time_stamp() + delay;
      const bool block = blocking && latency > sc_core::SC_ZERO_TIME;
      if (block) {
         ++(from_idma ? active_idma : active_odma);
         sc_core::wait(latency);
         --(from_idma ? active_idma : active_odma);
      }
      tlm::tlm_response_status st = tlm::TLM_OK_RESPONSE;
      if (addr < base || addr + len > base + bytes.size()) {
         st = tlm::TLM_ADDRESS_ERROR_RESPONSE;
      } else {
         for (fault &f : faults) {
            if (f.cmd == t.get_command() && addr < f.hi && addr + len > f.lo && f.remaining != 0) {
               st = tlm::TLM_GENERIC_ERROR_RESPONSE;  // AXI SLVERR
               if (f.remaining > 0) {
                  --f.remaining;
               }
               break;
            }
         }
      }
      if (st == tlm::TLM_OK_RESPONSE) {
         if (t.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(t.get_data_ptr(), at(addr), len);
         } else if (t.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(at(addr), t.get_data_ptr(), len);
         }
      }
      if (!block) {
         delay += latency;
      }
      log.push_back({start, start + latency, t.get_command(), addr, len, from_idma, st});
      t.set_response_status(st);
   }
};

}  // namespace fx1_test
