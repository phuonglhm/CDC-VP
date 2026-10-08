#include "fx1/word_access_guard.h"

namespace fx1 {

WordAccessGuard::WordAccessGuard(sc_core::sc_module_name name) : sc_module(name) {
    target.register_b_transport(this, &WordAccessGuard::b_transport);
    target.register_transport_dbg(this, &WordAccessGuard::transport_dbg);
}

bool WordAccessGuard::admissible(const tlm::tlm_generic_payload& tx) {
    if ((!tx.is_read() && !tx.is_write()) || !tx.get_data_ptr()) return false;
    if (tx.get_data_length() != 4 || tx.get_streaming_width() < 4 || tx.get_address() % 4)
        return false;
    const auto* enables = tx.get_byte_enable_ptr();
    if (!enables) return true;
    const auto length = tx.get_byte_enable_length();
    if (!length) return false;
    for (unsigned i = 0; i < 4; ++i)
        if (enables[i % length] != TLM_BYTE_ENABLED) return false;
    return true;
}

void WordAccessGuard::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    if (!admissible(tx)) {
        ++rejected_;
        tx.set_dmi_allowed(false);
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    out->b_transport(tx, delay);
}

unsigned WordAccessGuard::transport_dbg(tlm::tlm_generic_payload& tx) {
    if (!admissible(tx)) return 0;
    return out->transport_dbg(tx);
}

} // namespace fx1
