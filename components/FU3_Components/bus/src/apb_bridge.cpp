#include "fu3_bus/apb_bridge.h"
#include "fu3_bus/config.h"
#include "fu3_bus/transaction.h"

namespace fu3_bus {
ApbBridge::ApbBridge(sc_core::sc_module_name name,unsigned cycle_ns) : sc_module(name),cycle_(cycle_ns,sc_core::SC_NS) {
    target.register_b_transport(this, &ApbBridge::b_transport);
    target.register_transport_dbg(this, &ApbBridge::transport_dbg);
}
void ApbBridge::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    consume_delay(delay);
    if (!validate(tx)) return;
    const auto n = tx.get_data_length();
    // VP contract: aligned subwords or up to 16 consecutive 32-bit words.
    // This is a functional decomposition, not a claim about the RTL converter.
    if (n != 1 && n != 2 && (n == 0 || n > 64 || n % 4)) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return;
    }
    if (tx.get_address() % (n < 4 ? n : 4)) {
        tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    Fifo::Lease lease(queue_);
    struct Restore {
        tlm::tlm_generic_payload& tx;
        sc_dt::uint64 address;
        unsigned char *data, *enables;
        unsigned length, streaming, enable_length;
        explicit Restore(tlm::tlm_generic_payload& t) : tx(t), address(t.get_address()),
            data(t.get_data_ptr()), enables(t.get_byte_enable_ptr()), length(t.get_data_length()),
            streaming(t.get_streaming_width()), enable_length(t.get_byte_enable_length()) {}
        ~Restore() {
            tx.set_address(address); tx.set_data_ptr(data); tx.set_data_length(length);
            tx.set_streaming_width(streaming); tx.set_byte_enable_ptr(enables);
            tx.set_byte_enable_length(enable_length); tx.set_dmi_allowed(false);
        }
    } restore(tx);
    for (unsigned offset = 0; offset < n; offset += 4) {
        const unsigned beat = n < 4 ? n : 4;
        unsigned char enables[4] = {};
        tx.set_address(restore.address + offset);
        tx.set_data_ptr(restore.data + offset);
        tx.set_data_length(beat); tx.set_streaming_width(beat);
        if (restore.enables) {
            for (unsigned i = 0; i < beat; ++i)
                enables[i] = restore.enables[(offset+i) % restore.enable_length];
            tx.set_byte_enable_ptr(enables); tx.set_byte_enable_length(beat);
            // A complete enabled word is the canonical unmasked TLM transfer.
            // Preserve every sparse/zero mask; never emulate a CSR RMW.
            if(beat==4 && enables[0]==0xff && enables[1]==0xff &&
               enables[2]==0xff && enables[3]==0xff) {
                tx.set_byte_enable_ptr(nullptr);tx.set_byte_enable_length(0);
            }
        }
        tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        // Even zero configured latency yields to other SystemC threads.
        sc_core::wait(2.0 * cycle_);
        out->b_transport(tx, delay);
        consume_delay(delay);
        if (!tx.is_response_ok()) break; // Earlier beats are not rolled back.
    }
}
unsigned ApbBridge::transport_dbg(tlm::tlm_generic_payload& tx) {
    // Backdoor access, not an APB transfer: no beat/alignment/timing restrictions.
    const auto n = out->transport_dbg(tx);
    tx.set_dmi_allowed(false);
    return n;
}
} // namespace fu3_bus
