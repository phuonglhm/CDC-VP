// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.eth_mac_tlm.tests.test_core_golden

// Changelog:
//   2026-09-29: Initial version.
// =============================================================================
//
// Usage: test_core_golden core.txt
// Replay bundled frame records and compare results field by field.
// Also check counter wrap and overflow properties. See vectors/README.md.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "icsl_eth/eth_core.h"

using namespace icsl_eth::core;

namespace {

int g_fail = 0, g_cases = 0;

Bytes unhex(const std::string& s) {
    Bytes b;
    if (s == "-") return b;
    for (size_t i = 0; i + 1 < s.size(); i += 2) b.push_back(uint8_t(std::stoul(s.substr(i, 2), nullptr, 16)));
    return b;
}

void fail(int line, const std::string& what) {
    if (++g_fail <= 30) std::printf("[FAIL] line %d: %s\n", line, what.c_str());
}

FilterConfig read_filter(std::istringstream& in) {
    FilterConfig c;
    std::string bc, pam, hen, hash;
    in >> bc >> pam >> hen >> hash;
    c.bcast_accept = bc == "1";
    c.pass_all_mcast = pam == "1";
    c.hash_en = hen == "1";
    c.hash_table = std::stoull(hash, nullptr, 16);
    c.uc_num = 4;
    for (int n = 0; n < 4; ++n) {
        std::string a, en;
        in >> a >> en;
        const Bytes b = unhex(a);
        for (int k = 0; k < 6; ++k) c.uc[n][k] = b[k];
        c.uc_en[n] = en == "1";
    }
    return c;
}

void rmon_properties() {
    // Counter32 wrap sets the overflow flag; read-clear clears both; W1C mask
    Rmon r;
    r.en = true;
    RxEvent e;
    e.length = 0xFFFF;
    for (int i = 0; i < 65538; ++i) r.rx_frame(e);           // 65538 x 65535 > 2^32
    const uint64_t want = (uint64_t(65538) * 65535) & 0xFFFFFFFFu;
    ++g_cases;
    if (r.peek(RX_OCTETS) != want || !(r.ovf_rx() & (1u << RX_OCTETS)) || (r.ovf_rx() & (1u << RX_PKTS)))
        fail(0, "RMON octet counter wrap and its overflow flag");
    ++g_cases;
    r.clear_ovf(1u << RX_OCTETS, 0);
    if (r.ovf_rx()) fail(0, "RMON_OVF_STAT_RX W1C");
    ++g_cases;
    r.freeze = true;
    r.rx_frame(e);
    r.tx_frame(64, false, false);
    if (r.peek(RX_PKTS) != 65538 || r.peek(TX_PKTS) != 0) fail(0, "RMON freeze discards events");
    ++g_cases;
    if (r.read(RX_PKTS, true) != 65538 || r.peek(RX_PKTS) != 0) fail(0, "RMON read-clear");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s core.txt\n", argv[0]);
        return 2;
    }
    std::ifstream f(argv[1]);
    if (!f) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::printf("=== core golden: %s ===\n", argv[1]);
    std::string line;
    int ln = 0;
    int counts[7] = {};
    while (std::getline(f, line)) {
        ++ln;
        if (line.empty()) continue;
        std::istringstream in(line);
        std::string kind;
        in >> kind;
        ++g_cases;
        if (kind == "CRC") {
            std::string d, crc;
            int hi;
            in >> d >> crc >> hi;
            const Bytes b = unhex(d);
            if (crc32_raw(b.data(), b.size()) != std::stoul(crc, nullptr, 16)) fail(ln, "crc32_raw");
            if (hi >= 0 && int(mcast_hash_index(b.data())) != hi) fail(ln, "mcast_hash_index");
            ++counts[0];
        } else if (kind == "FILT") {
            std::string da, acc;
            in >> da;
            const FilterConfig c = read_filter(in);
            in >> acc;
            if (filter(unhex(da).data(), c).accept != (acc == "1")) fail(ln, "filter accept");
            ++counts[1];
        } else if (kind == "RX") {
            std::string fr, er, dr, en, l4, drop, vrd;
            unsigned mn, mx, stored;
            in >> fr >> er >> dr >> mn >> mx;
            RxMacConfig c;
            c.filt = read_filter(in);
            c.min_frame = uint16_t(mn);
            c.max_frame = uint16_t(mx);
            in >> en >> l4 >> drop >> vrd >> stored;
            const Bytes w = unhex(fr);
            const RxEvent e = rx_mac_verdict(w, er == "1", dr == "1", c);
            if (e.drop != (drop == "1")) fail(ln, "Rx MAC drop verdict");
            if (!e.drop) {
                const Bytes nf(w.begin(), w.end() - 4);
                const unsigned v = rx_checksum(nf, en == "1", l4 == "1");
                if (v != std::stoul(vrd, nullptr, 16)) fail(ln, "receive checksum verdict");
                if (nf.size() != stored) fail(ln, "stored length");
            }
            Rmon r;
            r.en = true;
            r.rx_frame(e);
            for (unsigned i = 0; i < kRmonRx; ++i) {
                uint32_t d;
                in >> d;
                if (r.peek(i) != d) fail(ln, "RMON receive counter " + std::to_string(i));
            }
            ++counts[2];
        } else if (kind == "RXCS") {
            std::string fr, en, l4, vrd;
            in >> fr >> en >> l4 >> vrd;
            if (rx_checksum(unhex(fr), en == "1", l4 == "1") != std::stoul(vrd, nullptr, 16))
                fail(ln, "rx_checksum");
            ++counts[3];
        } else if (kind == "TXCS") {
            std::string fr, en, l4, exp;
            in >> fr >> en >> l4 >> exp;
            Bytes b = unhex(fr);
            tx_checksum_insert(b, en == "1", l4 == "1");
            if (b != unhex(exp)) fail(ln, "tx_checksum_insert");
            ++counts[4];
        } else if (kind == "TXW") {
            std::string p, exp;
            unsigned mn;
            in >> p >> mn >> exp;
            if (tx_wire_frame(unhex(p), uint16_t(mn)) != unhex(exp)) fail(ln, "tx_wire_frame");
            ++counts[5];
        } else if (kind == "TXRM") {
            std::string wr;
            in >> wr;
            const Bytes w = unhex(wr);
            const bool bc = w[0] == 0xFF && w[1] == 0xFF && w[2] == 0xFF && w[3] == 0xFF &&
                            w[4] == 0xFF && w[5] == 0xFF;
            Rmon r;
            r.en = true;
            r.tx_frame(uint16_t(w.size()), bc, (w[0] & 1) && !bc);
            for (unsigned i = 0; i < kRmonTx; ++i) {
                uint32_t d;
                in >> d;
                if (r.peek(kRmonRx + i) != d) fail(ln, "RMON transmit counter " + std::to_string(i));
            }
            ++counts[6];
        } else {
            fail(ln, "unknown case kind " + kind);
        }
    }
    rmon_properties();
    std::printf("CRC %d, FILT %d, RX %d, RXCS %d, TXCS %d, TXW %d, TXRM %d\n", counts[0], counts[1],
                counts[2], counts[3], counts[4], counts[5], counts[6]);
    std::printf("=== core golden: %d cases, %d failures ===\n", g_cases, g_fail);
    return g_fail == 0 ? 0 : 1;
}
