#pragma once

#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/cpu_base.h>

namespace cdc::components {

// RISC-V CLINT (Core-Local Interruptor), single hart.
//
// Memory-mapped registers (offsets from the CLINT base, typically 0x02000000):
//   0x0000  msip      (32-bit) software interrupt pending (bit0)
//   0x4000  mtimecmp  (64-bit) timer compare
//   0xBFF8  mtime     (64-bit) monotonic time (read; derived from sim time)
//
// Drives the CPU's timer (MTIP, cause 7) and software (MSIP, cause 3) interrupts
// through cpu_base::set_irq. mtime/mtimecmp are in microseconds.
class clint_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<clint_tlm> socket;

    clint_tlm(sc_core::sc_module_name name, cdc::cpu::cpu_base& cpu);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void timer_proc();
    std::uint64_t now_mtime() const;
    void reschedule();

    cdc::cpu::cpu_base& cpu_;
    std::uint64_t mtimecmp_ = 0;
    std::uint32_t msip_ = 0;
    sc_core::sc_event cmp_event_;
};

} // namespace cdc::components
