// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.eth_core
// Description: Bit-exact frame functions of the Ethernet MAC (see eth_core.h).
// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
#include "icsl_eth/eth_core.h"

#include <algorithm>

namespace icsl_eth {
namespace core {

// =============================================================================
// CRC-32
// =============================================================================
namespace {
struct CrcTable {
    std::array<uint32_t, 256> t{};
    CrcTable() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
            t[i] = c;
        }
    }
};
const CrcTable kCrc;
}  // namespace

uint32_t crc32_raw(const uint8_t* p, size_t n, uint32_t init) {
    uint32_t c = init;
    for (size_t i = 0; i < n; ++i) c = kCrc.t[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}

std::array<uint8_t, 4> fcs(const uint8_t* p, size_t n) {
    const uint32_t v = ~crc32_raw(p, n);
    return {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
}

bool fcs_ok(const uint8_t* p, size_t n) {
    return n >= 4 && crc32_raw(p, n) == kCrcResidue;
}

unsigned mcast_hash_index(const uint8_t* da) { return (crc32_raw(da, 6) >> 26) & 0x3F; }

// =============================================================================
// address filter: broadcast, multicast, unicast; first match wins
// =============================================================================
FilterResult filter(const uint8_t* da, const FilterConfig& c) {
    FilterResult r;
    r.bcast = std::all_of(da, da + 6, [](uint8_t b) { return b == 0xFF; });
    const bool group = da[0] & 1;
    r.mcast = group && !r.bcast;
    if (r.bcast) {
        r.accept = c.bcast_accept;
    } else if (group) {
        r.accept = c.pass_all_mcast ||
                   (c.hash_en && ((c.hash_table >> mcast_hash_index(da)) & 1));
    } else {
        for (unsigned n = 0; n < c.uc_num && n < 4; ++n)
            if (c.uc_en[n] && std::equal(da, da + 6, c.uc[n].begin())) r.accept = true;
    }
    return r;
}

// =============================================================================
// Rx MAC end-of-frame verdict
// =============================================================================
namespace {
// ETH_RX_MAX_FRAME already holds the limit in use (the CSR stores at most
// PARA_MAX_FRAME_LEN).
uint16_t eff_max(const RxMacConfig& c) { return c.max_frame; }
}  // namespace

RxEvent rx_mac_verdict(const Bytes& f, bool rx_er, bool dribble, const RxMacConfig& c) {
    RxEvent e;
    // LEN_COUNTER saturates at 0xFFFF and that value cuts the frame (branch f)
    const bool cut = f.size() >= 0xFFFF;
    const size_t n = cut ? 0xFFFF : f.size();
    e.length = uint16_t(n);
    e.crc_error = cut || n < 4 || crc32_raw(f.data(), n) != kCrcResidue;
    e.undersize = n < c.min_frame;
    e.oversize = n > eff_max(c);
    e.fragment = e.undersize && e.crc_error;
    e.jabber = e.oversize && e.crc_error;
    e.align_error = e.crc_error && dribble && !cut;
    e.rx_er = rx_er;
    const bool wire_err = e.crc_error || e.undersize || e.oversize || rx_er;
    // The DA is complete after six bytes; a shorter frame is undersize at any
    // sane ETH_RX_MIN_FRAME, so the filter result of a partial DA never counts.
    FilterResult fr;
    const bool da_done = n >= 6;
    if (da_done) fr = filter(f.data(), c.filt);
    const bool clean_miss = !fr.accept && !wire_err;
    e.filter_drop = clean_miss;
    e.bcast = da_done && fr.bcast && !clean_miss;
    e.mcast = da_done && fr.mcast && !clean_miss;
    e.drop = wire_err || !fr.accept;
    return e;
}

size_t rx_forwarded_bytes(size_t frame_len, const RxMacConfig& c) {
    // capture stops once the length passes the effective maximum; the four
    // bytes left in the FCS delay line never reach the FIFO
    const size_t captured = std::min<size_t>(frame_len, eff_max(c));
    return captured > 4 ? captured - 4 : 0;
}

// =============================================================================

// =============================================================================
namespace {
constexpr uint8_t kTcp = 6, kUdp = 17;

uint32_t sum16(const uint8_t* p, size_t n, uint32_t s = 0) {
    for (size_t i = 0; i + 1 < n; i += 2) s += uint32_t(p[i]) << 8 | p[i + 1];
    if (n & 1) s += uint32_t(p[n - 1]) << 8;
    return s;
}
uint16_t fold(uint32_t s) {
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return uint16_t(s);
}

struct Ipv4 {
    bool ok = false;                            // conditions 1 and 2
    size_t off = 14, ihl = 0;
    uint8_t proto = 0;
    unsigned totlen = 0;
    bool frag = false;                          // MF set or fragment offset != 0
};

Ipv4 parse(const Bytes& f) {
    Ipv4 p;
    if (f.size() < 14 + 20) return p;
    if ((unsigned(f[12]) << 8 | f[13]) != 0x0800) return p;       // cond 1
    const uint8_t vi = f[14];
    if ((vi >> 4) != 4 || (vi & 0xF) < 5) return p;                // cond 2
    p.ok = true;
    p.ihl = size_t(vi & 0xF) * 4;
    p.proto = f[14 + 9];
    p.totlen = unsigned(f[14 + 2]) << 8 | f[14 + 3];
    const unsigned ff = unsigned(f[14 + 6]) << 8 | f[14 + 7];
    p.frag = (ff & 0x2000) || (ff & 0x1FFF);
    return p;
}

// condition 4: the L4 half is checked
bool cond4(const Ipv4& p, bool l4_en) {
    return l4_en && !p.frag && (p.proto == kTcp || p.proto == kUdp);
}

// conditions 3 and 5 on a parsed header
bool cond35(const Ipv4& p, bool c4, size_t frame_bytes) {
    const unsigned min_l4 = c4 ? (p.proto == kTcp ? 20u : 8u) : 0u;
    if (p.totlen < p.ihl + min_l4) return false;                   // cond 3
    return 14 + size_t(p.totlen) <= frame_bytes;                    // cond 5
}

// one's-complement sum of the pseudo header and the L4 segment
uint32_t l4_sum(const Bytes& f, const Ipv4& p) {
    const size_t l4 = p.off + p.ihl, l4len = p.totlen - p.ihl;
    uint32_t s = sum16(&f[p.off + 12], 8);                        // source + destination
    s += p.proto;
    s += uint32_t(l4len);
    return sum16(&f[l4], l4len, s);
}
}  // namespace

uint8_t rx_checksum(const Bytes& f, bool en, bool l4_en) {
    if (!en) return 0;
    const Ipv4 p = parse(f);
    if (!p.ok) return 0;
    const bool c4 = cond4(p, l4_en);
    if (!cond35(p, c4, f.size())) return 0;                        // every *_checked clear
    uint8_t v = 1u << 5;                                            // IPV4_CHECKED
    if (fold(sum16(&f[p.off], p.ihl)) != 0xFFFF) v |= 1u << 4;     // IPV4_ERR
    v |= p.proto == kTcp ? 1 : p.proto == kUdp ? 2 : 0;            // L4_PROTO
    if (c4) {
        const size_t fpos = p.off + p.ihl + (p.proto == kUdp ? 6 : 16);
        if (p.proto == kUdp && f[fpos] == 0 && f[fpos + 1] == 0) return v;   // RFC 768
        v |= 1u << 3;                                               // L4_CHECKED
        if (fold(l4_sum(f, p)) != 0xFFFF) v |= 1u << 2;            // L4_ERR
    }
    return v;
}

void tx_checksum_insert(Bytes& f, bool en, bool l4_en) {
    if (!en) return;
    const Ipv4 p = parse(f);
    if (!p.ok) return;
    const bool c4 = cond4(p, l4_en);
    if (!cond35(p, c4, f.size())) return;                          // sent as built
    f[p.off + 10] = f[p.off + 11] = 0;
    const uint16_t hc = uint16_t(~fold(sum16(&f[p.off], p.ihl)));
    f[p.off + 10] = uint8_t(hc >> 8);
    f[p.off + 11] = uint8_t(hc);
    if (!c4) return;
    const size_t fpos = p.off + p.ihl + (p.proto == kUdp ? 6 : 16);
    f[fpos] = f[fpos + 1] = 0;
    uint16_t c = uint16_t(~fold(l4_sum(f, p)));
    if (p.proto == kUdp && c == 0) c = 0xFFFF;                      // RFC 768
    f[fpos] = uint8_t(c >> 8);
    f[fpos + 1] = uint8_t(c);
}

// =============================================================================
// Tx framing
// =============================================================================
Bytes tx_wire_frame(const Bytes& payload, uint16_t tx_min_frame) {
    Bytes w = payload;
    if (w.size() + 4 < tx_min_frame) w.resize(tx_min_frame - 4u, 0);
    const auto c = fcs(w.data(), w.size());
    w.insert(w.end(), c.begin(), c.end());
    return w;
}

// =============================================================================

// =============================================================================
namespace {
// RFC 2819 fixed bins, none outside 64..1518; returns the bin 0..5 or -1
int size_bin(unsigned len) {
    if (len == 64) return 0;
    if (len >= 65 && len <= 127) return 1;
    if (len >= 128 && len <= 255) return 2;
    if (len >= 256 && len <= 511) return 3;
    if (len >= 512 && len <= 1023) return 4;
    if (len >= 1024 && len <= 1518) return 5;
    return -1;
}
}  // namespace

void Rmon::reset() {
    cnt_.fill(0);
    ovf_ = 0;
    edge_ = ~uint64_t(0);
    inc_.fill(0);
    wrap_ = 0;
}

void Rmon::add(unsigned idx, uint32_t delta) {
    if (!delta) return;
    if (now && edge_ != now()) {
        edge_ = now();
        inc_.fill(0);
        wrap_ = 0;
    }
    const uint64_t s = uint64_t(cnt_[idx]) + delta;
    if (s > 0xFFFFFFFFull) {
        ovf_ |= 1u << idx;
        wrap_ |= 1u << idx;
    }
    cnt_[idx] = uint32_t(s);
    inc_[idx] += delta;
}

void Rmon::rx_frame(const RxEvent& e) {
    if (!live()) return;
    const bool len_under = e.undersize || e.fragment;
    const bool len_over = e.oversize || e.jabber;
    const bool fcs_bad = e.crc_error || e.align_error || e.fragment || e.jabber;
    const bool wire_err = fcs_bad || len_under || len_over || e.rx_er;
    const bool full = !(e.filter_drop && !wire_err);
    if (!full) {
        add(RX_FILTER_DROP, 1);
        return;
    }
    add(RX_OCTETS, e.length);
    add(RX_PKTS, 1);
    if (!wire_err && e.bcast) add(RX_BCAST_PKTS, 1);
    if (!wire_err && e.mcast && !e.bcast) add(RX_MCAST_PKTS, 1);
    // one error class per frame: fragment, jabber, undersize, oversize, CRC
    if (len_under && fcs_bad) add(RX_FRAGMENTS, 1);
    else if (len_over && fcs_bad) add(RX_JABBERS, 1);
    else if (len_under) add(RX_UNDERSIZE, 1);
    else if (len_over) add(RX_OVERSIZE, 1);
    else if (fcs_bad) add(RX_CRC_ALIGN_ERR, 1);
    const int b = size_bin(e.length);
    if (b >= 0) add(RX_PKTS_64 + unsigned(b), 1);
}

void Rmon::tx_frame(uint16_t length, bool bcast, bool mcast) {
    if (!live()) return;
    add(TX_PKTS, 1);
    add(TX_OCTETS, length);
    if (bcast) add(TX_BCAST, 1);
    if (mcast && !bcast) add(TX_MCAST, 1);
    const int b = size_bin(length);
    if (b >= 0) add(TX_PKTS_64 + unsigned(b), 1);
}

void Rmon::rx_drop() {
    if (live()) add(RX_DROP_EVENTS, 1);
}
void Rmon::tx_oversize() {
    if (live()) add(TX_OVERSIZE, 1);
}
void Rmon::tx_drop() {
    if (live()) add(TX_DROP, 1);
}

// Table 6-37 rows 2 and 3: the sample is the value before this edge's update;
// a read-clear restarts the counter at this edge's increment.
uint32_t Rmon::read(unsigned idx, bool read_clear) {
    if (idx >= RMON_NUM) return 0;
    const uint32_t v = cnt_[idx] - inc(idx);
    if (read_clear) {
        cnt_[idx] = inc(idx);
        ovf_ &= ~(1u << idx);
    }
    return v;
}

// Table 6-37 row 1: every counter restarts at its increment of this edge.
void Rmon::clear_all() {
    for (unsigned i = 0; i < RMON_NUM; ++i) cnt_[i] = inc(i);
    ovf_ = 0;
}

// A wrap sets its flag even against a clear of the same flag on that edge.
void Rmon::clear_ovf(uint32_t rx_mask, uint32_t tx_mask) {
    const uint32_t m = (rx_mask & ((1u << kRmonRx) - 1)) | ((tx_mask & ((1u << kRmonTx) - 1)) << kRmonRx);
    ovf_ &= ~(m & ~(this_edge() ? wrap_ : 0u));
}

}  // namespace core
}  // namespace icsl_eth
