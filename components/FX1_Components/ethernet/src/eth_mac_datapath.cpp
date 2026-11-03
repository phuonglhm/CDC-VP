// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.eth_mac_datapath
// Description: Frame FIFOs, Tx MAC and Rx MAC at frame level.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
// Frame-level transmit and receive processing, including FIFO admission,
// checksum handling, inter-packet gap, line occupancy and completion events.
#include <algorithm>

#include "eth_regs.h"
#include "icsl_eth/eth_mac_tlm.h"

using namespace sc_core;

namespace icsl_eth {

unsigned EthMacTlm::words_of(size_t bytes) const {
    const unsigned B = p_.axi_data_bytes;
    return unsigned((bytes + B - 1) / B);
}

// ---- TX FIFO (DROP_ON_FULL = 0: the DMA stream stalls) ------------------------
bool EthMacTlm::tx_fifo_space(unsigned bytes) const {
    const unsigned B = p_.axi_data_bytes;
    return tx_fifo_used_ + words_of(tx_open_.size() + bytes) * B <= p_.tx_fifo_bytes;
}

// least PARA_FIFO_RSV_WORDS free records; a record leaves at the Tx MAC claim.
bool EthMacTlm::tx_meta_space() const {
    const size_t records = tx_fifo_.size() - (tx_claimed_ ? 1 : 0);
    return p_.tx_meta_depth - records >= p_.tx_meta_reserve;
}

void EthMacTlm::tx_fifo_flush() {
    tx_fifo_.clear();
    tx_claimed_ = false;
    tx_fifo_used_ = 0;
    tx_open_.clear();
    tx_.frame_open = false;
    tx_fifo_ev_.notify(SC_ZERO_TIME);
}

void EthMacTlm::rx_fifo_flush() {
    rx_fifo_.clear();
    rx_fifo_used_ = 0;
    ++rx_fifo_gen_;
    rx_fifo_ev_.notify(SC_ZERO_TIME);
}

// ---- Tx MAC --------------------------------------------------------------------
void EthMacTlm::tx_mac_thread() {
    while (true) {
        const bool tx_en = regs_.value(ETH_TX_MAC_CFG) & ETH_TX_MAC_CFG_TX_EN_MASK;
        if (!(tx_en && tx_line_ok() && !tx_fifo_.empty() && !tx_claimed_)) {
            wait(tx_fifo_ev_ | txmac_reset_ev_);
            continue;
        }
        const uint64_t gen = txmac_gen_;
        // the claim: TX_EN gates only this; a claimed frame runs to its end
        tx_claimed_ = true;
        tx_busy_ = true;
        tx_fifo_ev_.notify(SC_ZERO_TIME);               // its metadata record is out
        const bool s100 = speed100();
        const sc_time bt = byte_time(s100);
        const sc_time sof = sc_time_stamp() + bt * double(regs_.value(ETH_IPG) & 0xFF);   // at least 12: the CSR stores the floor
        while (sc_time_stamp() < sof && gen == txmac_gen_) wait(sof - sc_time_stamp(), txmac_reset_ev_);
        if (gen != txmac_gen_) continue;                // reset: the FIFO was flushed
        const core::Bytes wire =
            core::tx_wire_frame(tx_fifo_.front().bytes, uint16_t(regs_.value(ETH_TX_MIN_FRAME)));
        EthFrameExt x;
        x.speed100 = s100;
        send_frame(phy_tx, wire, x);
        const sc_time end = sc_time_stamp() + frame_time(wire.size(), s100);
        while (sc_time_stamp() < end && gen == txmac_gen_) wait(end - sc_time_stamp(), txmac_reset_ev_);
        if (gen != txmac_gen_) continue;
        // the RMON event at the last FCS byte: length DA..FCS, DA class
        const bool bcast = std::all_of(wire.begin(), wire.begin() + 6, [](uint8_t b) { return b == 0xFF; });
        rmon_.tx_frame(uint16_t(std::min<size_t>(wire.size(), 0xFFFF)), bcast, (wire[0] & 1) && !bcast);
        sync_rmon();
        ++tx_frames_;
        tx_fifo_used_ -= std::min(tx_fifo_used_, words_of(tx_fifo_.front().bytes.size()) * p_.axi_data_bytes);
        tx_fifo_.pop_front();
        tx_claimed_ = false;
        tx_busy_ = false;
        tx_last_end_ = sc_time_stamp();
        tx_fifo_ev_.notify(SC_ZERO_TIME);
    }
}

// ---- Rx MAC --------------------------------------------------------------------
void EthMacTlm::phy_rx_b_transport(tlm::tlm_generic_payload& tx, sc_time& delay) {
    LineFrame f;
    if (!recv_frame(tx, f.bytes, f.ext)) {
        tx.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    tx.set_response_status(tlm::TLM_OK_RESPONSE);
    // Rx RMII / Rx MAC held in reset, or RX_EN clear at the SFD: the line is
    // ignored.  A frame at the other line rate yields no SFD at this one.
    if (!rx_line_ok()) return;
    if (!(regs_.value(ETH_RX_MAC_CFG) & ETH_RX_MAC_CFG_RX_EN_MASK)) return;
    if (f.ext.speed100 != speed100()) return;
    f.end = sc_time_stamp() + delay + frame_time(f.bytes.size(), f.ext.speed100, f.ext.preamble);
    f.gen = rxmac_gen_;
    f.fifo_gen = rx_fifo_gen_;
    f.fifo_open = rx_fifo_writable();
    rx_line_.push_back(std::move(f));
    rx_line_ev_.notify(SC_ZERO_TIME);
}

void EthMacTlm::rx_mac_thread() {
    const unsigned fifo_words = p_.rx_fifo_bytes / p_.axi_data_bytes;
    while (true) {
        if (rx_line_.empty()) {
            wait(rx_line_ev_);
            continue;
        }
        if (sc_time_stamp() < rx_line_.front().end) {
            wait(rx_line_.front().end - sc_time_stamp(), rx_line_ev_);
            continue;                                   // an abort may have emptied the line
        }
        LineFrame lf = std::move(rx_line_.front());
        rx_line_.pop_front();
        if (lf.gen != rxmac_gen_) continue;
        const core::RxMacConfig cfg = rx_mac_config();
        const core::RxEvent ev = core::rx_mac_verdict(lf.bytes, lf.ext.rx_er, lf.ext.dribble, cfg);
        rmon_.rx_frame(ev);
        // An RX FIFO reset met by the frame (held at its SOF, or asserted since,
        // released or not) loses it: the FIFO drops words up to the next SOF.
        // No overflow and no drop event (MAS Table 6-2 row 8).
        const bool fifo_ok = lf.fifo_open && lf.fifo_gen == rx_fifo_gen_ && rx_fifo_writable();
        // the Rx MAC writes the frame speculatively; the FIFO rewinds it if it
        // does not fit (overflow even for a bad verdict) or on a drop verdict
        const size_t fwd = core::rx_forwarded_bytes(lf.bytes.size(), cfg);
        const unsigned words = std::max(1u, words_of(fwd));
        if (!fifo_ok) {
            // lost to the reset
        } else if (rx_fifo_used_ + words > fifo_words) {
            regs_.hw_set(ETH_FIFO_STAT, ETH_FIFO_STAT_RXFIFO_OVF_MASK);
            rmon_.rx_drop();
        } else if (!ev.drop) {
            RxFrame f;
            f.bytes.assign(lf.bytes.begin(), lf.bytes.end() - 4);   // FCS stripped
            f.words = words;
            f.ready = sc_time_stamp() + p_.clk_period * double(p_.rx_fifo_latency_cycles);
            rx_fifo_used_ += words;
            rx_fifo_.push_back(std::move(f));
            rx_fifo_ev_.notify(SC_ZERO_TIME);
        }
        sync_rmon();
    }
}

}  // namespace icsl_eth
