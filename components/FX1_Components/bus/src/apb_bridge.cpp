#include "bus/apb_bridge.h"
#include "bus/config.h"
#include "bus/transaction.h"

namespace bus {
ApbBridge::ApbBridge(sc_core::sc_module_name name,unsigned cycle_ns) : sc_module(name),cycle_ns_(cycle_ns) {
    target.register_b_transport(this, &ApbBridge::b_transport);
    target.register_transport_dbg(this, &ApbBridge::transport_dbg);
}
void ApbBridge::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    consume_delay(delay);
    if (!validate(tx)) return;
    const auto n = tx.get_data_length();
    // Current behavioral contract: one naturally aligned 8/16/32-bit APB access.
    // TODO: split AXI bursts into APB accesses when required by the real design.
    if (n != 1 && n != 2 && n != 4) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (tx.get_address() % n) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    Lock lock(lock_);
    sc_core::wait(2.0 * cycle_ns_, sc_core::SC_NS); // setup + access; no unsigned overflow
    out->b_transport(tx, delay);
    consume_delay(delay);
}
unsigned ApbBridge::transport_dbg(tlm::tlm_generic_payload& tx) {
    // Backdoor access, not an APB transfer: no beat/alignment/timing restrictions.
    const auto n = out->transport_dbg(tx);
    tx.set_dmi_allowed(false);
    return n;
}
} // namespace bus
