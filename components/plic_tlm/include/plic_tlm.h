#pragma once

#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/cpu_base.h>

namespace cdc::components {

// Basic RISC-V PLIC (Platform-Level Interrupt Controller), single hart / M-mode.
//
// Aggregates `num_sources` external interrupt lines (irq_in[0..num_sources-1],
// PLIC source id = index + 1) and drives the CPU's external interrupt (MEIP,
// cause 11) via cpu_base::set_irq.
//
// Register map (offsets from the PLIC base, typically 0x0C000000):
//   0x000000 + 4*id   priority[id]        (id 1..N; >threshold = eligible)
//   0x001000          pending bitfield     (read)
//   0x002000          enable bitfield      (context 0 / hart0 M-mode)
//   0x200000          priority threshold   (context 0)
//   0x200004          claim (read) / complete (write)   (context 0)
//
// Simplifications vs a full PLIC: one hart/context, one 32-bit word of sources,
// level-sensitive gateway with claim/complete, priorities compared numerically.
class plic_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<plic_tlm> socket;
    sc_core::sc_vector<sc_core::sc_in<bool>> irq_in;

    plic_tlm(sc_core::sc_module_name name, cdc::cpu::cpu_base& cpu, unsigned num_sources);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void on_sources();          // recompute MEIP when a source line changes
    void update_meip();
    std::uint32_t do_claim();
    bool eligible(unsigned id) const;  // id in 1..N

    cdc::cpu::cpu_base& cpu_;
    unsigned n_;
    std::vector<std::uint32_t> priority_;  // index 0 unused; size n_+1
    std::vector<bool> claimed_;            // gateway: source being serviced
    std::uint32_t enable_ = 0;             // bit id
    std::uint32_t threshold_ = 0;
};

} // namespace cdc::components
