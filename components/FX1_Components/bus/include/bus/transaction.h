#pragma once
#include <systemc>
#include <tlm>

namespace bus {
// Model uses blocking transport and consumes all annotated delay.
// Call b_transport from SC_THREAD, never SC_METHOD.
inline void consume_delay(sc_core::sc_time& delay) {
    if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
    delay = sc_core::SC_ZERO_TIME;
}
inline bool validate(tlm::tlm_generic_payload& tx) {
    tx.set_dmi_allowed(false);
    if (!tx.is_read() && !tx.is_write()) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return false;
    }
    if (!tx.get_data_ptr() || !tx.get_data_length()) {
        tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return false;
    }
    if (tx.get_streaming_width() < tx.get_data_length()) {
        tx.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
        return false;
    }
    if (tx.get_byte_enable_ptr()) {
        if (!tx.get_byte_enable_length()) {
            tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return false;
        }
        for (unsigned i = 0; i < tx.get_byte_enable_length(); ++i) {
            const auto value = tx.get_byte_enable_ptr()[i];
            if (value != TLM_BYTE_ENABLED && value != TLM_BYTE_DISABLED) {
                tx.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
                return false;
            }
        }
    }
    return true;
}
class Lock {
public:
    explicit Lock(sc_core::sc_mutex& mutex) : mutex_(mutex) { mutex_.lock(); }
    ~Lock() { mutex_.unlock(); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
private:
    sc_core::sc_mutex& mutex_;
};
class RestoreAddress {
public:
    explicit RestoreAddress(tlm::tlm_generic_payload& tx) : tx_(tx), address_(tx.get_address()) {}
    ~RestoreAddress() { tx_.set_address(address_); }
private:
    tlm::tlm_generic_payload& tx_;
    sc_dt::uint64 address_;
};
} // namespace bus
