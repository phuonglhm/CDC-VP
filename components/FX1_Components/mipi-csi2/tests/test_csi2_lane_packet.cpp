// Unit tests of Lane Management and Packet Analyzer without SystemC or the
// PHY: synthetic PPI traces built from known byte streams, and word streams
// built from packets of the BFM builder. Expected values are the input bytes
// themselves or properties stated by the specifications (SEC-DED, CRC
// examples); nothing is derived from the units under test.
//
//   test_fx1_csi2_lane_packet <case>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "fx1_csi2/csi2_protocol.h"
#include "fx1_csi2/lane_management.h"
#include "fx1_csi2/packet_analyzer.h"
#include "fx1_csi2_bfm/csi2_packet.h"

using namespace fx1::csi2;
namespace bfm = fx1::csi2::bfm;
using Bytes = std::vector<uint8_t>;

namespace {
int g_fail = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_fail;                                                                       \
        }                                                                                   \
    } while (0)

// ---------------------------------------------------------------- Lane Management harness

struct LmRecorder : LmEvents {
    std::vector<MergedWord> words;
    unsigned aborts = 0, deskews = 0;
    bool a_sync = false, a_full = false, a_unarmed = false;
    uint8_t a_mask = 0;
    std::vector<LmState> states;
    int applied = -1;
    void lm_word(const MergedWord& w) override { words.push_back(w); }
    void lm_abort(bool s, bool f, bool u, uint8_t m) override {
        ++aborts;
        a_sync = s;
        a_full = f;
        a_unarmed = u;
        a_mask = m;
    }
    void lm_deskew_burst() override { ++deskews; }
    void lm_state(LmState s) override { states.push_back(s); }
    void lm_lanes_applied(unsigned enc) override { applied = static_cast<int>(enc); }
    // bytes of the data words, in order (abort markers excluded)
    Bytes bytes() const {
        Bytes b;
        for (const auto& w : words)
            for (unsigned i = 0; i < 4; ++i)
                if (w.byte_valid >> i & 1) b.push_back(static_cast<uint8_t>(w.data >> (8 * i)));
        return b;
    }
};

// One lane of a synthetic burst, PPI as this PHY presents it: rxactivehs from
// `start`, rxsynchs with the first byte, bytes with optional gaps, `trail`
// junk bytes, then rxactivehs low `tail` cycles after the last byte.
// PPI conventions: PhyModel = this PHY (rxactivehs and rxsynchs on the same
// edge, first byte with rxvalidhs one cycle later, M1 trace); Mas = MAS v1.3
// timing figure (rxactivehs first, the first byte together with rxsynchs).
enum class Style { PhyModel, Mas };

struct LaneTrace {
    Bytes bytes;
    Style style = Style::PhyModel;
    unsigned start = 2;
    unsigned gap_every = 0;  // insert one invalid cycle after every n bytes (0: none)
    unsigned trail = 0;
    bool active = true;      // lane enters HS at all
    bool sync = true;        // establishes a byte boundary
    unsigned errsync_at = 0; // cycle of an errsotsynchs pulse (0: none)
    unsigned tail = 2;
};

std::vector<PpiSample> make_trace(const std::vector<LaneTrace>& lanes, bool skewcal = false) {
    std::vector<std::vector<PpiSample>> per(lanes.size());
    size_t len = 0;
    for (size_t l = 0; l < lanes.size(); ++l) {
        const auto& t = lanes[l];
        std::vector<PpiSample> v(t.start);
        Bytes all = t.bytes;
        for (unsigned i = 0; i < t.trail; ++i) all.push_back(0xFF);
        if (t.active) {
            unsigned since_gap = 0;
            {
                PpiSample lead;  // PhyModel: sync with active; Mas: active alone
                lead.active[l] = true;
                lead.sync[l] = t.sync && t.style == Style::PhyModel;
                v.push_back(lead);
            }
            for (size_t i = 0; i < all.size(); ++i) {
                if (t.gap_every && since_gap == t.gap_every) {
                    PpiSample g;
                    g.active[l] = true;
                    v.push_back(g);
                    since_gap = 0;
                }
                PpiSample s;
                s.active[l] = true;
                s.valid[l] = t.sync;
                s.sync[l] = t.sync && i == 0 && t.style == Style::Mas;
                s.data[l] = all[i];
                v.push_back(s);
                ++since_gap;
            }
            if (all.empty()) {
                PpiSample s;
                s.active[l] = true;
                v.push_back(s);
            }
            for (unsigned i = 0; i < t.tail; ++i) {
                PpiSample s;
                s.active[l] = true;
                v.push_back(s);
            }
            if (t.errsync_at && t.errsync_at < v.size()) v[t.errsync_at].errsync[l] = true;
        }
        per[l] = v;
        len = std::max(len, v.size());
    }
    std::vector<PpiSample> out(len + 8);
    for (size_t c = 0; c < out.size(); ++c) {
        out[c].skewcal = skewcal;
        for (size_t l = 0; l < lanes.size(); ++l) {
            if (c >= per[l].size()) continue;
            const PpiSample& s = per[l][c];
            out[c].data[l] = s.data[l];
            out[c].valid[l] = s.valid[l];
            out[c].active[l] = s.active[l];
            out[c].sync[l] = s.sync[l];
            out[c].errsync[l] = s.errsync[l];
        }
    }
    return out;
}

void run_lm(LaneManagement& lm, const std::vector<PpiSample>& trace, unsigned enc, LmRecorder& r) {
    for (const auto& s : trace) lm.clock(s, enc, r);
}

unsigned enc_of(unsigned n) { return n == 1 ? 0u : n == 2 ? 1u : 3u; }

Bytes seq_bytes(size_t n, uint8_t seed) {
    Bytes b(n);
    for (size_t i = 0; i < n; ++i) b[i] = static_cast<uint8_t>(seed + 37 * i);
    return b;
}

// ---------------------------------------------------------------- LM cases

void lm_merge() {
    for (Style style : {Style::PhyModel, Style::Mas})
    for (unsigned n : {1u, 2u, 4u})
        for (size_t len = 4; len <= 41; ++len)  // shortest CSI-2 packet: 4 bytes
            for (unsigned skew = 0; skew <= 5; ++skew)
                for (unsigned gaps : {0u, 3u})
                    for (unsigned trail : {0u, 7u}) {
                        LaneManagement lm(4);
                        LmRecorder r;
                        run_lm(lm, make_trace({}), enc_of(n), r);  // adopt the lane count in ST_IDLE
                        const Bytes burst = seq_bytes(len, static_cast<uint8_t>(len));
                        const auto split = bfm::distribute_lanes(burst, n);
                        std::vector<LaneTrace> lanes(n);
                        for (unsigned i = 0; i < n; ++i) {
                            lanes[i].bytes = split[i];
                            lanes[i].start = 2 + (i == n - 1 ? skew : 0);  // last lane late by `skew` cycles
                            lanes[i].gap_every = (i == 0) ? gaps : 0;
                            lanes[i].trail = trail;
                            lanes[i].style = style;
                            lanes[i].tail = 2 + skew;  // lanes of one burst overlap in time
                        }
                        r.words.clear();
                        run_lm(lm, make_trace(lanes), enc_of(n), r);
                        const Bytes got = r.bytes();
                        bool ok = got.size() >= burst.size() && std::equal(burst.begin(), burst.end(), got.begin()) &&
                                  got.size() == burst.size() + trail * n && r.aborts == 0 && !r.words.empty() &&
                                  r.words.front().sob && r.words.back().eob && lm.state() == LmState::Idle;
                        for (size_t i = 0; i + 1 < r.words.size(); ++i)
                            ok = ok && r.words[i].byte_valid == 0xF && !r.words[i].eob && (i == 0 || !r.words[i].sob);
                        if (!ok) {
                            std::fprintf(stderr, "lm_merge style=%d n=%u len=%zu skew=%u gaps=%u trail=%u: got %zu bytes, aborts %u\n",
                                         static_cast<int>(style), n, len, skew, gaps, trail, got.size(), r.aborts);
                            ++g_fail;
                            return;
                        }
                    }
    CHECK(true);
}

void lm_abort_cases() {
    // A lane that enters HS only after the others have closed the burst: unarmed abort.
    {
        LaneManagement lm(4);
        LmRecorder r;
        run_lm(lm, make_trace({}), 1, r);
        std::vector<LaneTrace> l(2);
        for (unsigned i = 0; i < 2; ++i) l[i].bytes = seq_bytes(2, i);
        l[1].start = 12;
        r.words.clear();
        run_lm(lm, make_trace(l), 1, r);
        CHECK(r.aborts >= 1 && r.a_unarmed);
    }
    // FIFO full while unarmed: lane 3 starts after lanes 0-2 have written the usable 6 bytes.
    {
        LaneManagement lm(4);
        LmRecorder r;
        run_lm(lm, make_trace({}), 3, r);
        std::vector<LaneTrace> l(4);
        for (unsigned i = 0; i < 4; ++i) l[i].bytes = seq_bytes(20, i);
        l[3].start = 12;
        r.words.clear();
        run_lm(lm, make_trace(l), 3, r);
        CHECK(r.aborts == 1 && r.a_full && !r.a_sync && r.a_mask == 0x7);
        CHECK(r.words.size() == 1 && r.words[0].is_abort_marker());
    }
    // errsotsynchs wins over armed in the same cycle.
    {
        LaneManagement lm(4);
        LmRecorder r;
        run_lm(lm, make_trace({}), 1, r);
        std::vector<LaneTrace> l(2);
        for (unsigned i = 0; i < 2; ++i) l[i].bytes = seq_bytes(8, i);
        l[1].errsync_at = 3;  // the cycle the FSM would see armed
        r.words.clear();
        run_lm(lm, make_trace(l), 1, r);
        CHECK(r.aborts == 1 && r.a_sync);
        CHECK(r.words.size() == 1 && r.words[0].is_abort_marker());
    }
    // Unarmed: lane 3 never enters HS; the burst closes -> abort_unarmed, lane mask 0111.
    {
        LaneManagement lm(4);
        LmRecorder r;
        run_lm(lm, make_trace({}), 3, r);
        std::vector<LaneTrace> l(4);
        for (unsigned i = 0; i < 4; ++i) l[i].bytes = seq_bytes(3, i);
        l[3].active = false;
        r.words.clear();
        run_lm(lm, make_trace(l), 3, r);
        CHECK(r.aborts == 1 && r.a_unarmed && !r.a_full && r.a_mask == 0x7);
        CHECK(r.words.size() == 1 && r.words[0].is_abort_marker());
    }
    // Deskew burst: calibration pattern, no sync -> silently back to idle, no word.
    {
        LaneManagement lm(4);
        LmRecorder r;
        run_lm(lm, make_trace({}), 1, r);
        std::vector<LaneTrace> l(2);
        for (auto& t : l) {
            t.bytes = Bytes(10, 0x55);
            t.sync = false;
        }
        r.words.clear();
        run_lm(lm, make_trace(l, true), 1, r);
        CHECK(r.deskews == 1 && r.aborts == 0 && r.words.empty() && lm.state() == LmState::Idle);
        // and a normal burst afterwards still works
        for (auto& t : l) {
            t.bytes = seq_bytes(9, 1);
            t.sync = true;
        }
        run_lm(lm, make_trace(l), 1, r);
        CHECK(r.bytes().size() == 18);
    }
}

void lm_lanes_config() {
    LaneManagement lm(4);
    LmRecorder r;
    run_lm(lm, make_trace({}), 1, r);
    CHECK(r.applied == 1 && lm.lanes() == 2);
    // A change during a burst is applied only once ST_IDLE is reached.
    std::vector<LaneTrace> l(2);
    for (unsigned i = 0; i < 2; ++i) l[i].bytes = seq_bytes(16, i);
    auto tr = make_trace(l);
    for (size_t c = 0; c < tr.size(); ++c) {
        lm.clock(tr[c], c < 4 ? 1u : 3u, r);
        if (c > 4 && c + 1 < tr.size() && lm.state() != LmState::Idle) CHECK(lm.lanes() == 2);
    }
    CHECK(lm.lanes() == 4 && r.applied == 3);
    // Reserved 10 is not adopted.
    run_lm(lm, make_trace({}), 2, r);
    CHECK(lm.lanes() == 4 && r.applied == 3);
    // Beyond MAX_LANES (built 2, programmed 4): adopted, never armed -> abort_unarmed.
    LaneManagement lm2(2);
    LmRecorder r2;
    run_lm(lm2, make_trace({}), 3, r2);
    CHECK(lm2.lanes() == 4);
    std::vector<LaneTrace> l2(2);
    for (unsigned i = 0; i < 2; ++i) l2[i].bytes = seq_bytes(4, i);
    run_lm(lm2, make_trace(l2), 3, r2);
    CHECK(r2.aborts == 1 && r2.a_unarmed);
    // reset(): idle, nothing adopted.
    lm.reset();
    CHECK(lm.state() == LmState::Idle && lm.lanes() == 1);
}

// reset() from every Burst FSM state: back to ST_IDLE, nothing of the old
// burst emitted afterwards, the next burst received exactly.
void lm_reset_states() {
    for (LmState target : {LmState::Arming, LmState::Aligned, LmState::Drain, LmState::Abort}) {
        LaneManagement lm(4);
        LmRecorder r;
        run_lm(lm, make_trace({}), 1, r);
        std::vector<LaneTrace> l(2);
        for (unsigned i = 0; i < 2; ++i) l[i].bytes = seq_bytes(40, static_cast<uint8_t>(i));
        if (target == LmState::Abort || target == LmState::Arming) l[1].start = 30;  // lane 1 late: stays unarmed / aborts
        const auto tr = make_trace(l);
        bool hit = false;
        for (const auto& smp : tr) {
            lm.clock(smp, 1, r);
            if (lm.state() == target) {
                hit = true;
                break;
            }
        }
        CHECK(hit);
        const size_t words = r.words.size();
        lm.reset();
        CHECK(lm.state() == LmState::Idle);
        run_lm(lm, make_trace({}), 1, r);  // idle cycles: nothing of the old burst
        CHECK(r.words.size() == words);
        std::vector<LaneTrace> fresh(2);
        for (unsigned i = 0; i < 2; ++i) fresh[i].bytes = seq_bytes(12, static_cast<uint8_t>(9 + i));
        r.words.clear();
        run_lm(lm, make_trace(fresh), 1, r);
        Bytes want;
        for (unsigned k = 0; k < 12; ++k)
            for (unsigned i = 0; i < 2; ++i) want.push_back(fresh[i].bytes[k]);
        CHECK(r.bytes() == want);
    }
}

// ---------------------------------------------------------------- Packet Analyzer harness

struct PaRecorder : PaEvents {
    std::vector<PayloadBeat> beats;
    std::map<PaError, unsigned> errors;
    std::vector<int> error_vc;
    unsigned done = 0, fs = 0, fe = 0, null_blank = 0, vc_unsup = 0;
    std::vector<uint32_t> gsp;
    void pa_beat(const PayloadBeat& b) override { beats.push_back(b); }
    void pa_error(PaError e, int vc, unsigned) override {
        ++errors[e];
        error_vc.push_back(vc);
    }
    void pa_packet_done() override { ++done; }
    void pa_frame_start(unsigned, uint16_t) override { ++fs; }
    void pa_frame_end(unsigned, uint16_t) override { ++fe; }
    void pa_gsp(uint8_t vc, uint8_t dt, uint16_t d) override { gsp.push_back(uint32_t(d) << 8 | vc << 6 | dt); }
    void pa_null_blank() override { ++null_blank; }
    void pa_vc_unsupported() override { ++vc_unsup; }
    unsigned err(PaError e) const {
        auto it = errors.find(e);
        return it == errors.end() ? 0 : it->second;
    }
    unsigned total_errors() const {
        unsigned n = 0;
        for (auto& [e, c] : errors) n += c;
        return n;
    }
    Bytes payload() const {
        Bytes b;
        for (const auto& x : beats)
            for (unsigned i = 0; i < 4; ++i)
                if (x.byte_en >> i & 1) b.push_back(x.bytes[i]);
        return b;
    }
};

// Reference packer: bytes of one burst into words, byte k at word k/4 lane k%4.
std::vector<MergedWord> words_of(const Bytes& burst, size_t trail = 0) {
    Bytes b = burst;
    for (size_t i = 0; i < trail; ++i) b.push_back(0xFF);
    std::vector<MergedWord> w;
    for (size_t i = 0; i < b.size(); i += 4) {
        MergedWord m;
        for (size_t k = 0; k < 4 && i + k < b.size(); ++k) {
            m.data |= uint32_t(b[i + k]) << (8 * k);
            m.byte_valid |= static_cast<uint8_t>(1u << k);
        }
        m.sob = i == 0;
        m.eob = i + 4 >= b.size();
        w.push_back(m);
    }
    return w;
}

void feed(PacketAnalyzer& pa, const Bytes& burst, PaRecorder& r, const PaConfig& cfg = {}, size_t trail = 0) {
    for (const auto& w : words_of(burst, trail)) pa.push(w, cfg, r);
}

Bytes fs(uint8_t vc = 0, uint16_t n = 1) { return bfm::short_packet(vc, bfm::DT_FS, n); }
Bytes fe(uint8_t vc = 0, uint16_t n = 1) { return bfm::short_packet(vc, bfm::DT_FE, n); }

// ---------------------------------------------------------------- PA cases

void pa_lines() {
    // RAW10 WC multiple of 5, RAW12 multiple of 3: every value of WC mod 4.
    for (int raw10 = 0; raw10 <= 1; ++raw10)
        for (unsigned units = 1; units <= 4; ++units) {
            const unsigned wc = units * (raw10 ? 5 : 3);
            PacketAnalyzer pa;
            PaRecorder r;
            feed(pa, fs(), r, {}, 6);
            std::vector<Bytes> lines;
            for (unsigned line = 0; line < 3; ++line) {
                const Bytes p = seq_bytes(wc, static_cast<uint8_t>(line * 11 + wc));
                lines.push_back(p);
                feed(pa, bfm::long_packet(0, raw10 ? bfm::DT_RAW10 : bfm::DT_RAW12, p), r, {}, 7);  // PHY trail
            }
            feed(pa, fe(), r, {}, 6);
            Bytes want;
            for (const auto& l : lines) want.insert(want.end(), l.begin(), l.end());
            CHECK(r.payload() == want);
            CHECK(r.total_errors() == 0 && r.done == 3 && r.fs == 1 && r.fe == 1);
            unsigned sol = 0, eol = 0, sof = 0;
            for (const auto& b : r.beats) {
                sol += b.sol;
                eol += b.eol;
                sof += b.sof;
                CHECK(b.raw10 == (raw10 != 0) && !b.embedded && b.vc == 0);
            }
            CHECK(sol == 3 && eol == 3 && sof == 1 && r.beats.front().sof && r.beats.back().eol);
        }
}

void pa_ecc() {
    const Bytes payload = seq_bytes(10, 3);
    const Bytes pkt = bfm::long_packet(0, bfm::DT_RAW10, payload);
    // Every single-bit error of the 30 protected bits is corrected.
    for (unsigned bit = 0; bit < 30; ++bit) {
        Bytes p = pkt;
        const unsigned pos = bit < 24 ? bit : 24 + (bit - 24);  // data bits 0..23, then ECC bits 0..5
        p[pos / 8] ^= static_cast<uint8_t>(1u << (pos % 8));
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        feed(pa, p, r);
        CHECK(r.err(PaError::Ecc1) == 1 && r.err(PaError::Ecc2) == 0 && r.payload() == payload);
    }
    // ECC bits 7:6 are not checked.
    {
        Bytes p = pkt;
        p[3] ^= 0xC0;
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        feed(pa, p, r);
        CHECK(r.total_errors() == 0 && r.payload() == payload);
    }
    // Every two-bit error is detected and the packet is not routed (SEC-DED).
    unsigned pairs = 0;
    for (unsigned a = 0; a < 30; ++a)
        for (unsigned b = a + 1; b < 30; ++b) {
            Bytes p = pkt;
            p[a / 8] ^= static_cast<uint8_t>(1u << (a % 8));
            p[b / 8] ^= static_cast<uint8_t>(1u << (b % 8));
            PacketAnalyzer pa;
            PaRecorder r;
            feed(pa, fs(), r);
            feed(pa, p, r);
            pairs += r.err(PaError::Ecc2) == 1 && r.beats.empty() && r.done == 0;
            // the next burst is parsed normally
            feed(pa, pkt, r);
            CHECK(r.payload() == payload);
        }
    CHECK(pairs == 435);
    // The receiver CRC matches the MIPI CSI-2 specification example.
    const uint8_t ex1[] = {0xFF, 0x00, 0x00, 0x02, 0xB9, 0xDC, 0xF3, 0x72, 0xBB, 0xD4, 0xB8, 0x5A,
                           0xC8, 0x75, 0xC2, 0x7C, 0x81, 0xF8, 0x05, 0xDF, 0xFF, 0x00, 0x00, 0x01};
    uint16_t crc = 0xFFFF;
    for (uint8_t x : ex1) crc = crc16_update(crc, x);
    CHECK(crc == 0x00F0);
}

void pa_payload_errors() {
    const Bytes payload = seq_bytes(15, 9);
    // CRC: payload delivered (cut-through), error, frame-level error at FE.
    {
        Bytes p = bfm::long_packet(0, bfm::DT_RAW10, payload);
        p[4 + 7] ^= 0x10;
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        feed(pa, p, r);
        feed(pa, fe(), r);
        CHECK(r.err(PaError::Crc) == 1 && r.err(PaError::FrameLevel) == 1 && r.done == 1);
        CHECK(r.payload().size() == payload.size());
    }
    // CRC byte itself corrupted (footer split across words, WC mod 4 = 3).
    {
        Bytes p = bfm::long_packet(0, bfm::DT_RAW10, payload);
        p.back() ^= 0x01;
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        feed(pa, p, r);
        CHECK(r.err(PaError::Crc) == 1 && r.payload() == payload);
    }
    // Truncated burst: WC error, FSM retires the packet, frame-level at FE.
    {
        Bytes p = bfm::long_packet(0, bfm::DT_RAW10, payload);
        p.resize(4 + 9);
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        feed(pa, p, r);
        feed(pa, fe(), r);
        CHECK(r.err(PaError::Wc) == 1 && r.err(PaError::FrameLevel) == 1 && r.done == 1 && r.err(PaError::Crc) == 0);
    }
    // Burst ending on the header word.
    {
        Bytes p = bfm::long_packet(0, bfm::DT_RAW10, payload);
        p.resize(4);
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        feed(pa, p, r);
        CHECK(r.err(PaError::Wc) == 1 && r.done == 1 && r.beats.empty());
    }
}

void pa_routing() {
    const Bytes payload = seq_bytes(10, 4);
    PaConfig cfg;
    // Unsupported DT (RGB888 0x24): error, not routed.
    {
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r, cfg);
        feed(pa, bfm::long_packet(0, 0x24, payload), r, cfg);
        CHECK(r.err(PaError::UnsupportedDt) == 1 && r.beats.empty() && r.done == 1);
    }
    // Null / Blanking: dropped silently.
    {
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r, cfg);
        feed(pa, bfm::long_packet(0, bfm::DT_NULL, payload), r, cfg);
        feed(pa, bfm::long_packet(0, bfm::DT_BLANKING, payload), r, cfg);
        CHECK(r.total_errors() == 0 && r.null_blank == 2 && r.beats.empty() && r.done == 2);
    }
    // Embedded data: routed to the embedded interface only when capture is enabled.
    {
        PaConfig e = cfg;
        e.metadata_capture_en = true;
        e.metadata_dt = bfm::DT_EMBEDDED;
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r, e);
        feed(pa, bfm::long_packet(0, bfm::DT_EMBEDDED, payload), r, e);
        CHECK(r.total_errors() == 0 && r.payload() == payload && r.beats.front().embedded && !r.beats.front().sof);
        feed(pa, bfm::long_packet(0, bfm::DT_RAW10, payload), r, e);
        CHECK(r.beats.back().sof == false);  // sof sits on the first *image* beat
        bool sof_on_image = false;
        for (const auto& b : r.beats) sof_on_image |= b.sof && !b.embedded;
        CHECK(sof_on_image);
        PacketAnalyzer pa2;
        PaRecorder r2;
        feed(pa2, fs(), r2, cfg);
        feed(pa2, bfm::long_packet(0, bfm::DT_EMBEDDED, payload), r2, cfg);  // capture disabled
        CHECK(r2.err(PaError::UnsupportedDt) == 1 && r2.beats.empty());
    }
    // VC1 in the linear profile: dropped, no error, ECC and packet count apply.
    {
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(1), r, cfg);
        feed(pa, bfm::long_packet(1, bfm::DT_RAW10, payload), r, cfg);
        feed(pa, fe(1), r, cfg);
        CHECK(r.total_errors() == 0 && r.vc_unsup == 3 && r.beats.empty() && r.done == 1 && r.fs == 0);
    }
    // Generic short packets 0x08-0x0F to the FIFO; LS/LE and reserved 0x04 ignored.
    {
        PacketAnalyzer pa;
        PaRecorder r;
        for (uint8_t dt : {0x02, 0x03, 0x04, 0x08, 0x0F}) feed(pa, bfm::short_packet(0, dt, 0x1234), r, cfg);
        CHECK(r.gsp.size() == 2 && r.gsp[0] == ((0x1234u << 8) | 0x08) && r.gsp[1] == ((0x1234u << 8) | 0x0F));
        CHECK(r.total_errors() == 0);
    }
}

void pa_frames() {
    const Bytes payload = seq_bytes(10, 5);
    // FS while open, FE with none open, long packet with no frame: frame-sync errors.
    {
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        feed(pa, fs(), r);
        CHECK(r.err(PaError::FrameSync) == 1);
        feed(pa, fe(), r);
        feed(pa, fe(), r);
        CHECK(r.err(PaError::FrameSync) == 2);
        feed(pa, bfm::long_packet(0, bfm::DT_RAW10, payload), r);
        CHECK(r.err(PaError::FrameSync) == 3 && r.beats.empty() && r.done == 1);
    }
    // Abort marker: lane-abort error, open frame marked, frame-level at FE.
    {
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        MergedWord m;
        m.sob = m.eob = true;
        pa.push(m, {}, r);
        CHECK(r.err(PaError::LaneAbort) == 1 && r.error_vc.back() == -1);
        feed(pa, bfm::long_packet(0, bfm::DT_RAW10, payload), r);
        feed(pa, fe(), r);
        CHECK(r.err(PaError::FrameLevel) == 1 && r.payload() == payload);
        // next frame clean
        feed(pa, fs(), r);
        feed(pa, fe(), r);
        CHECK(r.err(PaError::FrameLevel) == 1);
    }
    // Trail words of the PHY after a packet are ignored (no sob).
    {
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r, {}, 8);
        feed(pa, bfm::long_packet(0, bfm::DT_RAW12, seq_bytes(9, 1)), r, {}, 8);
        feed(pa, fe(), r, {}, 8);
        CHECK(r.total_errors() == 0 && r.done == 1 && r.payload() == seq_bytes(9, 1));
    }
    // reset() clears the contexts: an FE afterwards has no frame open.
    {
        PacketAnalyzer pa;
        PaRecorder r;
        feed(pa, fs(), r);
        pa.reset();
        feed(pa, fe(), r);
        CHECK(r.err(PaError::FrameSync) == 1);
    }
}
// Header shorter than 4 bytes (C21): never decoded.
void pa_short_header() {
    const Bytes zero(4, 0);
    const Bytes fs_h = fs();
    const Bytes fe_h = fe();
    const Bytes gsp_h = bfm::short_packet(0, 0x08, 0x1234);
    const Bytes long_h = bfm::long_packet(0, bfm::DT_RAW10, seq_bytes(10, 1));
    for (const Bytes* src : {&zero, &fs_h, &fe_h, &gsp_h, &long_h})
        for (size_t len = 1; len <= 3; ++len)
            for (int in_frame = 0; in_frame <= 1; ++in_frame) {
                PacketAnalyzer pa;
                PaRecorder r;
                if (in_frame) feed(pa, fs(), r);
                const unsigned fs0 = r.fs;
                feed(pa, Bytes(src->begin(), src->begin() + len), r);
                CHECK(r.fs == fs0 && r.fe == 0 && r.gsp.empty() && r.beats.empty() && r.done == 0);
                CHECK(r.err(PaError::Wc) == 1 && r.error_vc.back() == -1);
                CHECK(r.err(PaError::Ecc1) == 0 && r.err(PaError::Ecc2) == 0);
                feed(pa, fe(), r);
                if (in_frame) CHECK(r.err(PaError::FrameLevel) == 1 && r.fe == 1);
                else CHECK(r.err(PaError::FrameSync) == 1 && r.fe == 0);  // no frame was opened
                // recovery
                const unsigned errs = r.total_errors();
                feed(pa, fs(), r);
                feed(pa, bfm::long_packet(0, bfm::DT_RAW10, seq_bytes(10, 2)), r);
                feed(pa, fe(), r);
                CHECK(r.total_errors() == errs && r.payload() == seq_bytes(10, 2));
            }
}

// Payload complete, CRC footer missing (C22): truncated, not a CRC verdict.
void pa_missing_footer() {
    for (unsigned units : {0u, 1u, 2u, 3u, 4u})  // WC 0, 5, 10, 15, 20: every WC mod 4
        for (unsigned missing : {1u, 2u}) {
            const Bytes payload = seq_bytes(units * 5, static_cast<uint8_t>(units));
            Bytes p = bfm::long_packet(0, bfm::DT_RAW10, payload);
            p.resize(p.size() - missing);
            PacketAnalyzer pa;
            PaRecorder r;
            feed(pa, fs(), r);
            feed(pa, p, r);
            CHECK(r.err(PaError::Wc) == 1 && r.err(PaError::Crc) == 0 && r.done == 1);
            CHECK(r.payload() == payload);  // cut-through
            if (!payload.empty()) CHECK(r.beats.back().eol);
            feed(pa, fe(), r);
            CHECK(r.err(PaError::FrameLevel) == 1);
            feed(pa, fs(), r);
            feed(pa, bfm::long_packet(0, bfm::DT_RAW10, seq_bytes(5, 9)), r);
            feed(pa, fe(), r);
            CHECK(r.err(PaError::FrameLevel) == 1 && r.err(PaError::Wc) == 1);
        }
}

// Event order per long packet (C23): beats, the last with EOL, then the
// integrity result, then packet_done; a consumer that flushes on an error
// never sees a beat of that packet afterwards.
struct OrderRecorder : PaRecorder {
    std::vector<std::string> log;
    bool flushed = false;
    unsigned beats_after_flush = 0;
    void pa_beat(const PayloadBeat& b) override {
        PaRecorder::pa_beat(b);
        if (flushed) ++beats_after_flush;
        log.push_back(b.eol ? "EOL" : "BEAT");
    }
    void pa_error(PaError e, int vc, unsigned line) override {
        PaRecorder::pa_error(e, vc, line);
        log.push_back(e == PaError::Crc ? "CRC" : e == PaError::Wc ? "WC" : "ERR");
        flushed = true;  // consumer discards the line
    }
    void pa_packet_done() override {
        PaRecorder::pa_packet_done();
        log.push_back("DONE");
    }
};

void pa_order() {
    for (unsigned units = 1; units <= 4; ++units)  // WC mod 4 = 1, 2, 3, 0
        for (int bad = 0; bad <= 1; ++bad)
            for (int truncate = 0; truncate <= 1; ++truncate) {
                const Bytes payload = seq_bytes(units * 5, static_cast<uint8_t>(units + 3));
                Bytes p = bfm::long_packet(0, bfm::DT_RAW10, payload);
                if (bad) p.back() ^= 0x40;
                if (truncate) p.pop_back();
                PacketAnalyzer pa;
                OrderRecorder r;
                feed(pa, fs(), r);
                r.log.clear();
                feed(pa, p, r);
                const std::string tail = r.log.size() >= 3 ? r.log[r.log.size() - 3] + "," + r.log[r.log.size() - 2] + "," + r.log.back() : "";
                const std::string err = truncate ? "WC" : bad ? "CRC" : "";
                if (err.empty()) CHECK(r.log.size() >= 2 && r.log[r.log.size() - 2] == "EOL" && r.log.back() == "DONE");
                else CHECK(tail == "EOL," + err + ",DONE");
                CHECK(r.beats_after_flush == 0);
            }
}
}  // namespace

int main(int argc, char** argv) {
    const std::string which = argc > 1 ? argv[1] : "lm_merge";
    const std::map<std::string, std::function<void()>> cases = {
        {"lm_merge", lm_merge},   {"lm_abort", lm_abort_cases},       {"lm_lanes", lm_lanes_config},
        {"lm_reset_states", lm_reset_states},
        {"pa_lines", pa_lines},   {"pa_ecc", pa_ecc},                 {"pa_payload_errors", pa_payload_errors},
        {"pa_routing", pa_routing}, {"pa_frames", pa_frames},
        {"pa_short_header", pa_short_header}, {"pa_missing_footer", pa_missing_footer}, {"pa_order", pa_order},
    };
    const auto it = cases.find(which);
    if (it == cases.end()) {
        std::printf("unknown case %s\n", which.c_str());
        return 1;
    }
    it->second();
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
