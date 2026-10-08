#include "fx1/cpu_port_adapter.h"
#include <sstream>

namespace fx1 {
namespace {
// nullptr when the payload is one the VP++ hart can legitimately produce.
const char* payload_defect(const tlm::tlm_generic_payload& tx) {
    if (!tx.is_read() && !tx.is_write()) return "command is neither read nor write";
    if (!tx.get_data_ptr()) return "null data pointer";
    const auto length = tx.get_data_length();
    if (length != 1 && length != 2 && length != 4 && length != 8)
        return "data length is not 1, 2, 4 or 8";
    if (tx.get_byte_enable_ptr()) return "byte enables are never produced by the VP++ hart";
    const auto width = tx.get_streaming_width();
    if (width != 0 && width < length) return "streaming width below the data length";
    return nullptr;
}
} // namespace

CpuPortAdapter::CpuPortAdapter(sc_core::sc_module_name name) : sc_module(name) {
    target.register_b_transport(this, &CpuPortAdapter::b_transport);
    target.register_transport_dbg(this, &CpuPortAdapter::transport_dbg);
}

void CpuPortAdapter::integration_error(const tlm::tlm_generic_payload& tx, const char* what) {
    ++defects_;
    std::ostringstream message;
    message << "integration error: " << what << " (" << (tx.is_write() ? "write" : tx.is_read() ? "read" : "other")
            << " 0x" << std::hex << tx.get_address() << std::dec << ", " << tx.get_data_length()
            << " bytes, response " << tx.get_response_string() << ")";
    SC_REPORT_ERROR(name(), message.str().c_str());
}

void CpuPortAdapter::b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
    if (const char* defect = payload_defect(tx)) {
        integration_error(tx, defect);
        return;
    }
    if (tx.get_streaming_width() == 0) tx.set_streaming_width(tx.get_data_length());
    tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);  // the target must answer
    out->b_transport(tx, delay);
    switch (tx.get_response_status()) {
    case tlm::TLM_OK_RESPONSE:
    case tlm::TLM_ADDRESS_ERROR_RESPONSE:
    case tlm::TLM_GENERIC_ERROR_RESPONSE:
        break;
    case tlm::TLM_BURST_ERROR_RESPONSE:
    case tlm::TLM_COMMAND_ERROR_RESPONSE:
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        ++mapped_;
        break;
    case tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE:
        integration_error(tx, "target reported a byte-enable error for a payload without byte enables");
        break;
    default:  // INCOMPLETE
        integration_error(tx, "no target set a response");
        break;
    }
}

unsigned CpuPortAdapter::transport_dbg(tlm::tlm_generic_payload& tx) {
    if (tx.get_streaming_width() == 0) tx.set_streaming_width(tx.get_data_length());
    return out->transport_dbg(tx);
}

} // namespace fx1
