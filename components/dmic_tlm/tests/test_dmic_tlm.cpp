//author: linhtk55-fpt
//verified: hoangv11

#include <systemc>
#include <iostream>
#include "dmic.h"
#include "testbench.h"
using namespace sc_core;

int sc_main(int argc, char *argv[]) {
   PDM_Source mic("microphone");
   cdc::components::DmicTLM dmic("dmic_peripheral");
   Host_CPU cpu("host_processor");

   sc_signal<bool, SC_MANY_WRITERS> dmic_irq_line("dmic_irq");
   sc_signal<bool> reset_n_line("reset_n");
   mic.initiator_socket.bind(dmic.pdm_target_socket);
   cpu.bus_socket.bind(dmic.bus_target_socket);
   dmic.irq_out.bind(dmic_irq_line);
   cpu.irq_in.bind(dmic_irq_line);
   dmic.reset_n.bind(reset_n_line);
   cpu.reset_n.bind(reset_n_line);

   std::cout << "Starting DMIC Simulation...\n\n";
   sc_start();
   std::cout << "\nSimulation finished.\n";

   int fails = 0;
   auto chk = [&](const char* name, bool ok) {
      std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
      if (!ok) ++fails;
   };
   chk("CTRL readback (EN|INT_EN|DEC=64)", cpu.ctrl_ok);
   chk("at least one IRQ fired", cpu.irq_count > 0);
   chk("watermark event(s) occurred", cpu.wm_events > 0);
   chk("16 samples read per watermark", cpu.samples_read == cpu.wm_events * 16);
   chk("every IRQ had WM/OE cause", cpu.irq_status_ok);

   std::cout << "\n[TB] DMIC failures: " << fails << "\n";
   return fails == 0 ? 0 : 1;
}
