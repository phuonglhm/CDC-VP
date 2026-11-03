// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.eth_frame
// Description: Frame-level PHY and MDIO transport conventions of the VP.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
// The RMII di-bits and the MDIO bit stream are not modelled; what crosses them
// is modelled as one transaction per frame or per management frame.
//
// Frame transport (MAC <-> PHY <-> line), tlm_generic_payload:
//   command   TLM_WRITE_COMMAND
//   address   0
//   data      DA..FCS as on the wire after the SFD (no preamble, no SFD)
//   extension EthFrameExt: line rate of the sender, PHY receive error,
//             dribble nibble, preamble length (bytes, SFD included)
//   timing    b_transport is called at the start of the frame; the receiver
//             derives the end of the frame from the length and the line rate
//             and never calls wait().  The sender waits out the wire time.
//
// Management transport (MAC MDIO -> PHY), tlm_generic_payload:
//   command   TLM_READ_COMMAND / TLM_WRITE_COMMAND
//   address   (PHYAD << 5) | REGAD
//   data      2 bytes, little-endian register value
//   response  TLM_OK_RESPONSE when a PHY at PHYAD drives the turnaround;
//             TLM_ADDRESS_ERROR_RESPONSE when nothing answers (the pulled-up
//             bus reads 1: NO_RESPONSE, data 0xFFFF)
//   timing    called at the end of the management frame
#pragma once

#include <cstdint>
#include <vector>

#include <systemc>
#include <tlm>

namespace icsl_eth {

struct EthFrameExt : tlm::tlm_extension<EthFrameExt> {
    bool speed100 = true;           // line rate of the sender
    bool rx_er = false;             // PHY receive error during the frame
    bool dribble = false;           // non-octet-aligned frame end
    unsigned preamble = 8;          // preamble + SFD bytes on the wire

    tlm::tlm_extension_base* clone() const override { return new EthFrameExt(*this); }
    void copy_from(const tlm::tlm_extension_base& e) override {
        *this = static_cast<const EthFrameExt&>(e);
    }
};

// Time one octet takes on the line: 80 ns at 100 Mbps, 800 ns at 10 Mbps.
inline sc_core::sc_time byte_time(bool speed100) {
    return sc_core::sc_time(speed100 ? 80.0 : 800.0, sc_core::SC_NS);
}
// Wire time of a frame of `len` bytes (DA..FCS) with its preamble.
inline sc_core::sc_time frame_time(size_t len, bool speed100, unsigned preamble = 8) {
    return byte_time(speed100) * double(len + preamble);
}

// Send one frame through an initiator socket; the payload lives on the stack
// of the caller for the duration of the call.
template <typename Socket>
tlm::tlm_response_status send_frame(Socket& s, std::vector<uint8_t> bytes, const EthFrameExt& x) {
    tlm::tlm_generic_payload tx;
    EthFrameExt ext = x;
    tx.set_command(tlm::TLM_WRITE_COMMAND);
    tx.set_address(0);
    tx.set_data_ptr(bytes.data());
    tx.set_data_length(unsigned(bytes.size()));
    tx.set_streaming_width(unsigned(bytes.size()));
    tx.set_byte_enable_ptr(nullptr);
    tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    tx.set_extension(&ext);
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
    s->b_transport(tx, d);
    tx.clear_extension(&ext);
    return tx.get_response_status();
}

// Decode a received frame payload; false if the payload is not a frame.
inline bool recv_frame(tlm::tlm_generic_payload& tx, std::vector<uint8_t>& bytes, EthFrameExt& x) {
    if (tx.get_command() != tlm::TLM_WRITE_COMMAND || !tx.get_data_ptr()) return false;
    bytes.assign(tx.get_data_ptr(), tx.get_data_ptr() + tx.get_data_length());
    EthFrameExt* e = nullptr;
    tx.get_extension(e);
    x = e ? *e : EthFrameExt();
    return true;
}

}  // namespace icsl_eth
