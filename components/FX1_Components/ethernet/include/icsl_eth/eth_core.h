// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.eth_core
// Description: Bit-exact frame functions of the Ethernet MAC: FCS, address
//              filter, Rx MAC verdict, checksum offload, Tx framing, RMON.
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
// Plain C++17 frame functions, independent of SystemC. Frames start at the
// first destination-address byte; preamble and SFD are not stored.
#pragma once

#include <array>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace icsl_eth {
namespace core {

using Bytes = std::vector<uint8_t>;

constexpr unsigned kMinFrame = 64;              // DA..FCS, IEEE 802.3
constexpr unsigned kMinIpg = 12;                // byte times
constexpr unsigned kPreambleBytes = 8;          // 7 x 0x55 + SFD
constexpr uint32_t kCrcResidue = 0xDEBB20E3u;   // LSB-first engine, good frame

// ---- CRC-32 (IEEE 802.3, reflected, preset ~0) --------------------------------
// The engine register after `n` bytes, without the final complement.
uint32_t crc32_raw(const uint8_t* p, size_t n, uint32_t init = 0xFFFFFFFFu);
// The four FCS bytes of DA..payload, in wire order.
std::array<uint8_t, 4> fcs(const uint8_t* p, size_t n);

bool fcs_ok(const uint8_t* p, size_t n);
// Multicast hash index: crc_reg[31:26] after the six DA bytes.
unsigned mcast_hash_index(const uint8_t* da);

struct FilterConfig {
    bool bcast_accept = true;                   // ETH_FILT_CTRL.BCAST_ACCEPT
    bool pass_all_mcast = false;                // ETH_FILT_CTRL.PASS_ALL_MCAST
    bool hash_en = false;                       // ETH_FILT_CTRL.HASH_EN
    uint64_t hash_table = 0;                    // {ETH_HASH_H, ETH_HASH_L}
    unsigned uc_num = 4;                        // PARA_RX_UNICAST_NUM
    std::array<std::array<uint8_t, 6>, 4> uc{}; // octet 0 = first DA byte
    std::array<bool, 4> uc_en{};                // ETH_UC_CTRL<n>.ENABLE
};
struct FilterResult {
    bool accept = false;
    bool bcast = false;                         // FF:FF:FF:FF:FF:FF
    bool mcast = false;                         // group address, not broadcast
};
FilterResult filter(const uint8_t* da, const FilterConfig& c);

struct RxMacConfig {
    uint16_t min_frame = 64;                    // ETH_RX_MIN_FRAME
    uint16_t max_frame = 1518;                  // ETH_RX_MAX_FRAME, at most PARA_MAX_FRAME_LEN
    FilterConfig filt;
};
// The RMON event of one frame and the commit/drop decision (o_rxmac_m_tuser[1]).
struct RxEvent {
    bool crc_error = false, align_error = false;
    bool undersize = false, oversize = false, fragment = false, jabber = false;
    bool filter_drop = false;                   // clean filter miss only
    bool rx_er = false;
    bool bcast = false, mcast = false;          // DA class, 0 on a clean miss
    uint16_t length = 0;                        // DA..FCS, saturating
    bool drop = false;                          // any wire error or filter reject
};
// `f` is DA..FCS as received; `rx_er` a PHY error anywhere in the frame;
// `dribble` a non-octet frame end.  A frame of 65535 bytes or more is the
// length cut of branch (f): forced CRC error, so it classifies jabber.
RxEvent rx_mac_verdict(const Bytes& f, bool rx_er, bool dribble, const RxMacConfig& c);
// Bytes the Rx MAC forwards into the RX FIFO: DA..payload, capped where
// capture stops (length above the effective maximum).
size_t rx_forwarded_bytes(size_t frame_len, const RxMacConfig& c);

// Receive verdict record, DES0[25:20] layout: [5] IPV4_CHECKED, [4] IPV4_ERR,
// [3] L4_CHECKED, [2] L4_ERR, [1:0] L4_PROTO (1 TCP, 2 UDP).  `f` is the frame
// the DMA receives: DA..payload, FCS stripped.  Zero when `en` is clear.
uint8_t rx_checksum(const Bytes& f, bool en, bool l4_en);
// Transmit insertion on DA..payload (no pad, no FCS), in place.
void tx_checksum_insert(Bytes& f, bool en, bool l4_en);

// DA..payload -> DA..pad..FCS: zero pad until the frame with its FCS reaches
// ETH_TX_MIN_FRAME (at least 64, the CSR stores the floor), then the FCS.
Bytes tx_wire_frame(const Bytes& payload, uint16_t tx_min_frame);

// 0x7800 order, 18..30 transmit in 0x7010 order.
enum RmonIdx : unsigned {
    RX_DROP_EVENTS = 0, RX_OCTETS, RX_PKTS, RX_BCAST_PKTS, RX_MCAST_PKTS, RX_CRC_ALIGN_ERR,
    RX_UNDERSIZE, RX_OVERSIZE, RX_FRAGMENTS, RX_JABBERS, RX_COLLISIONS, RX_PKTS_64,
    RX_PKTS_65_127, RX_PKTS_128_255, RX_PKTS_256_511, RX_PKTS_512_1023, RX_PKTS_1024_1518,
    RX_FILTER_DROP,
    TX_PKTS = 18, TX_OCTETS, TX_BCAST, TX_MCAST, TX_UNDERSIZE, TX_OVERSIZE, TX_DROP, TX_PKTS_64,
    TX_PKTS_65_127, TX_PKTS_128_255, TX_PKTS_256_511, TX_PKTS_512_1023, TX_PKTS_1024_1518,
    RMON_NUM = 31
};
constexpr unsigned kRmonRx = 18, kRmonTx = 13;

class Rmon {
public:
    bool en = false;                            // RMON_CTRL.RMON_EN
    bool freeze = false;                        // RMON_CTRL.FREEZE
    // Current counting edge (e.g. the simulation time).  When set, a clear on
    // the edge of an increment follows Table 6-37 in either call order: the
    // counter restarts at that edge's increments, a read samples the value
    // before them, and a wrap of that edge keeps its overflow flag.
    std::function<uint64_t()> now;

    void reset();
    // events; each is dropped unless en && !freeze
    void rx_frame(const RxEvent& e);
    void tx_frame(uint16_t length, bool bcast, bool mcast);
    void rx_drop();                             // RX FIFO overflow or no descriptor
    void tx_oversize();                         // DMA TX_TOO_LONG
    void tx_drop();                             // DMA TX_DATA_RD_ERR
    // bus access; a read-clear read also clears the counter's overflow flag
    uint32_t read(unsigned idx, bool read_clear);
    uint32_t peek(unsigned idx) const { return idx < RMON_NUM ? cnt_[idx] : 0; }
    void clear_all();
    void clear_ovf(uint32_t rx_mask, uint32_t tx_mask);
    uint32_t ovf_rx() const { return uint32_t(ovf_ & ((1u << kRmonRx) - 1)); }
    uint32_t ovf_tx() const { return uint32_t(ovf_ >> kRmonRx); }

private:
    bool live() const { return en && !freeze; }
    void add(unsigned idx, uint32_t delta);
    bool this_edge() const { return now && edge_ == now(); }
    uint32_t inc(unsigned idx) const { return this_edge() ? inc_[idx] : 0; }
    std::array<uint32_t, RMON_NUM> cnt_{};
    uint32_t ovf_ = 0;                          // bit i = counter i wrapped
    uint64_t edge_ = ~uint64_t(0);              // the edge inc_ and wrap_ belong to
    std::array<uint32_t, RMON_NUM> inc_{};
    uint32_t wrap_ = 0;
};

}  // namespace core
}  // namespace icsl_eth
