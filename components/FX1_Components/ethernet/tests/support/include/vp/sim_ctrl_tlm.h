// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: vp.common.sim_ctrl_tlm
// Description: Test-status device: firmware ends the simulation with a verdict.
// Changelog:
//   2026-09-28: Initial version.
// =============================================================================
//
//   0x0 EXIT  W   end the simulation; the value is the exit status (0 = pass)
//
// A firmware that parks in a loop cannot tell a regression whether it passed
// or is still spinning in a failure branch; writing its verdict here stops
// the simulation deterministically in both cases.
#pragma once

#include <cstdint>
#include <cstring>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace vp {

class SimCtrlTlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<SimCtrlTlm> socket;

    explicit SimCtrlTlm(sc_core::sc_module_name name) : sc_module(name), socket("socket") {
        socket.register_b_transport(this, &SimCtrlTlm::b_transport);
    }
    bool exited() const { return exited_; }
    int exit_code() const { return code_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
        tx.set_dmi_allowed(false);
        if (tx.get_address() != 0 || !tx.is_write() || !tx.get_data_ptr() ||
            tx.get_data_length() != 4) {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        uint32_t v;
        std::memcpy(&v, tx.get_data_ptr(), 4);
        code_ = int(v);
        exited_ = true;
        sc_core::sc_stop();
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    bool exited_ = false;
    int code_ = -1;
};

}  // namespace vp
