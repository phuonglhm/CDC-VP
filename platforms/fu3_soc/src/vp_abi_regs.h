#pragma once

#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <fu3_plic/intmap.h>

#include "fu3_vp_abi.h"

namespace cdc::platforms::fu3_soc {

// FU3_VP_ABI_1 registers in the SYSCSR aperture (offsets 0x100-0x123, see
// include/fu3_vp_abi.h), placed between the SYSCSR gate and the SYSCSR model:
// those offsets are served here, every other access goes to the model, which
// stays the FU3_SYSCSR_v1 register map.
//   - BOOT_STATUS: straps captured when the SYSCSR reset is released, DEBUG_EN live;
//   - RESET_CAUSE: cumulative bitmap in POR-only state (system, global, watchdog,
//     debug and the SYSCSR's own reset keep it), W1C;
//   - INTMAP_EN0/1: the INTMAP route enables (state kept in the INTMAP model);
//   - SWIRQ: pending bits driving CVA6 ipi_i and the Ibex software source.
// The SYSCSR reset clears the enables, SWIRQ and the captured straps.
class vp_abi_regs : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<vp_abi_regs> socket{"socket"};
    tlm_utils::simple_initiator_socket<vp_abi_regs> out{"out"};   // -> SYSCSR model
    sc_core::sc_in<bool> reset_n{"reset_n"};                        // SYSCSR domain reset
    sc_core::sc_in<bool> boot{"boot"}, mode_0{"mode_0"}, mode_1{"mode_1"}, test_mode{"test_mode"},
        test_select{"test_select"}, debug_en{"debug_en"};
    sc_core::sc_out<bool> cva6_software{"cva6_software"}, ibex_software{"ibex_software"};

    SC_HAS_PROCESS(vp_abi_regs);
    vp_abi_regs(sc_core::sc_module_name name, fu3_plic::Intmap& intmap);

    // Power-on reset: a new reset history (POR only).
    void power_on() { cause_ = FU3_VP_RESET_POR; }
    // A system-level reset taken with these causes (FU3_VP_RESET_* bits).
    void record_reset(unsigned causes) { cause_ |= causes & FU3_VP_RESET_MASK; }
    std::uint32_t reset_cause() const { return cause_; }
    std::uint32_t boot_status() const;

private:
    fu3_plic::Intmap& intmap_;
    std::uint32_t cause_ = 0, captured_ = 0, swirq_ = 0;
    bool valid_ = false;
    sc_core::sc_event changed_;

    void on_reset();
    void drive();
    std::uint32_t read(std::uint32_t offset) const;
    void write(std::uint32_t offset, std::uint32_t value, std::uint32_t lanes);
    bool access(tlm::tlm_generic_payload& tx, bool backdoor);
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
};

} // namespace cdc::platforms::fu3_soc
