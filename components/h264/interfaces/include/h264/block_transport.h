#pragma once
#include <tlm>
namespace h264 {
// VP block protocol validation, not additional PDF encoder MMIO registers.
inline bool block_payload(tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    if (!tx.is_read() && !tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return false;
    }
    if (!tx.get_data_ptr() || !tx.get_data_length() ||
        tx.get_streaming_width()<tx.get_data_length()) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE); return false;
    }
    if (tx.get_byte_enable_ptr()) {
        tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE); return false;
    }
    return true;
}
}
