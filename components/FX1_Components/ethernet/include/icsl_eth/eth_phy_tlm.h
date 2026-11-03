// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_phy_tlm
// Description: 10/100 PHY with Clause 22 registers, and a scripted link partner.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//

// part): the MAC reaches its registers over MDIO and its line through the
// frame transport of eth_frame.h.
//
//     mdio      target     <- MAC mdio      (PHYAD = phy_addr answers, others do not)
//     mac_tx    target     <- MAC phy_tx
//     mac_rx    initiator  -> MAC phy_rx
//     line_tx   initiator  -> peer line_rx  (another PHY, or an EthLinkPartner)
//     line_rx   target     <- peer line_tx
//
// Registers: BMCR(0) reset / loopback / speed / AN enable / restart AN /
// duplex; BMSR(1) capabilities, AN complete, link status; PHYID1/2(2,3);
// ANAR(4); ANLPAR(5); the rest read-write scratch.  With auto-negotiation on
// the link resolves to 100 Mbps full duplex; off, BMCR.SPEED selects the rate.
// Loopback (BMCR[14]) returns the MAC's frames to it and keeps them off the
// line.  A frame at a rate other than the PHY's is garbage to it and dropped,
// in both directions -- the "SPEED must match the PHY" rule of ETH_RMII_CFG.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "icsl_eth/eth_frame.h"

namespace icsl_eth {

class EthPhyTlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<EthPhyTlm> mdio;
    tlm_utils::simple_target_socket<EthPhyTlm> mac_tx;
    tlm_utils::simple_initiator_socket<EthPhyTlm> mac_rx;
    tlm_utils::simple_initiator_socket<EthPhyTlm> line_tx;
    tlm_utils::simple_target_socket<EthPhyTlm> line_rx;

    static constexpr uint16_t kBmcrReset = 1u << 15, kBmcrLoop = 1u << 14, kBmcrSpeed = 1u << 13,
                              kBmcrAnEn = 1u << 12, kBmcrAnRestart = 1u << 9, kBmcrFdx = 1u << 8;
    static constexpr uint16_t kBmsrLink = 1u << 2, kBmsrAnDone = 1u << 5;

    EthPhyTlm(sc_core::sc_module_name n, unsigned phy_addr = 1, uint16_t id1 = 0x0007,
              uint16_t id2 = 0xC0F1)
        : sc_module(n), mdio("mdio"), mac_tx("mac_tx"), mac_rx("mac_rx"), line_tx("line_tx"),
          line_rx("line_rx"), addr_(phy_addr & 31), id1_(id1), id2_(id2) {
        mdio.register_b_transport(this, &EthPhyTlm::mdio_b_transport);
        mac_tx.register_b_transport(this, &EthPhyTlm::mac_tx_b_transport);
        line_rx.register_b_transport(this, &EthPhyTlm::line_rx_b_transport);
        reset_regs();
    }

    void set_link(bool up) { link_ = up; }
    bool link() const { return link_; }
    bool speed100() const { return (reg_[0] & kBmcrAnEn) ? true : (reg_[0] & kBmcrSpeed) != 0; }
    bool loopback() const { return reg_[0] & kBmcrLoop; }
    uint16_t reg(unsigned r) const { return reg_[r & 31]; }
    uint64_t to_line() const { return to_line_; }
    uint64_t from_line() const { return from_line_; }
    uint64_t dropped() const { return dropped_; }

private:
    void reset_regs() {
        reg_.fill(0);
        reg_[0] = kBmcrSpeed | kBmcrAnEn | kBmcrFdx;
        reg_[2] = id1_;
        reg_[3] = id2_;
        reg_[4] = 0x01E1;                       // 100FD 100HD 10FD 10HD, 802.3
    }
    uint16_t bmsr() const {
        uint16_t v = 0x7809;                    // 100FD 100HD 10FD 10HD, extended regs
        if (link_) v |= kBmsrLink;
        if (link_ && (reg_[0] & kBmcrAnEn)) v |= kBmsrAnDone;
        return v;
    }

    void mdio_b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time&) {
        const unsigned phyad = unsigned(t.get_address() >> 5) & 31, r = unsigned(t.get_address()) & 31;
        unsigned char* d = t.get_data_ptr();
        if (phyad != addr_ || !d || t.get_data_length() != 2) {
            t.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);   // no turnaround
            return;
        }
        if (t.is_read()) {
            const uint16_t v = r == 1 ? bmsr() : r == 5 ? (link_ ? 0x45E1 : 0) : reg_[r];
            d[0] = uint8_t(v);
            d[1] = uint8_t(v >> 8);
        } else {
            const uint16_t v = uint16_t(d[0] | d[1] << 8);
            if (r == 0 && (v & kBmcrReset)) {
                reset_regs();                   // self-clearing soft reset
            } else if (r == 0) {
                reg_[0] = uint16_t(v & ~(kBmcrReset | kBmcrAnRestart));
            } else if (r != 1 && r != 2 && r != 3 && r != 5) {
                reg_[r] = v;
            }
        }
        t.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    void mac_tx_b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time&) {
        std::vector<uint8_t> b;
        EthFrameExt x;
        t.set_response_status(tlm::TLM_OK_RESPONSE);
        if (!recv_frame(t, b, x)) return;
        if (x.speed100 != speed100()) {         // wrong rate on TXD: garbage
            ++dropped_;
            return;
        }
        if (loopback()) {
            send_frame(mac_rx, b, x);
        } else if (link_) {
            ++to_line_;
            send_frame(line_tx, b, x);
        } else {
            ++dropped_;
        }
    }

    void line_rx_b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time&) {
        std::vector<uint8_t> b;
        EthFrameExt x;
        t.set_response_status(tlm::TLM_OK_RESPONSE);
        if (!recv_frame(t, b, x)) return;
        if (!link_ || loopback() || x.speed100 != speed100()) {
            ++dropped_;
            return;
        }
        ++from_line_;
        send_frame(mac_rx, b, x);
    }

    unsigned addr_;
    uint16_t id1_, id2_;
    std::array<uint16_t, 32> reg_{};
    bool link_ = true;
    uint64_t to_line_ = 0, from_line_ = 0, dropped_ = 0;
};

// A scripted station on the line: records what it receives, sends what the
// test gives it.  Frames it sends start at the time send() is called.
class EthLinkPartner : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<EthLinkPartner> line_tx;
    tlm_utils::simple_target_socket<EthLinkPartner> line_rx;

    struct Rx {
        std::vector<uint8_t> bytes;
        EthFrameExt ext;
        sc_core::sc_time start;
    };

    explicit EthLinkPartner(sc_core::sc_module_name n) : sc_module(n), line_tx("line_tx"), line_rx("line_rx") {
        line_rx.register_b_transport(this, &EthLinkPartner::rx_b_transport);
    }
    
    void send(const std::vector<uint8_t>& da_thru_fcs, const EthFrameExt& x) {
        send_frame(line_tx, da_thru_fcs, x);
        sc_core::wait(frame_time(da_thru_fcs.size(), x.speed100, x.preamble));
    }
    std::deque<Rx>& received() { return rx_; }
    sc_core::sc_event& rx_event() { return ev_; }

private:
    void rx_b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time&) {
        Rx r;
        t.set_response_status(tlm::TLM_OK_RESPONSE);
        if (!recv_frame(t, r.bytes, r.ext)) return;
        r.start = sc_core::sc_time_stamp();
        rx_.push_back(std::move(r));
        ev_.notify(sc_core::SC_ZERO_TIME);
    }
    std::deque<Rx> rx_;
    sc_core::sc_event ev_;
};

}  // namespace icsl_eth
