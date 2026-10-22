// Unit tests of the sys_clk domain (M3b) without SystemC or the PHY: each block
// alone, then the whole SysDatapath fed with merged words.
//
// Oracles are independent of the units under test: source pixel values, the
// BFM packer (bfm::pack_raw10/12) for packed bytes, explicit tables for the
// TPG colour bars and for the MAS line-end resolution, frame timing formulas
// stated by the MAS.
//
//   test_fx1_csi2_sys_datapath <case>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "fx1_csi2/axis_monitor.h"
#include "fx1_csi2/frame_control.h"
#include "fx1_csi2/pixel_path.h"
#include "fx1_csi2/stream_out.h"
#include "fx1_csi2/sys_datapath.h"
#include "fx1_csi2_bfm/csi2_packet.h"

using namespace fx1::csi2;
namespace bfm = fx1::csi2::bfm;
using Bytes = std::vector<uint8_t>;
using Pixels = std::vector<uint16_t>;

namespace {
int g_fail = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_fail;                                                                       \
        }                                                                                   \
    } while (0)
#define CHECK_EQ(a, b)                                                                                     \
    do {                                                                                                   \
        const auto va_ = (a);                                                                              \
        const auto vb_ = (b);                                                                              \
        if (!(va_ == vb_)) {                                                                               \
            std::fprintf(stderr, "%s:%d: CHECK_EQ failed: %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, \
                         (long long)va_, (long long)vb_);                                                  \
            ++g_fail;                                                                                      \
        }                                                                                                  \
    } while (0)

Pixels px_line(unsigned width, bool raw10, unsigned seed) {
    Pixels p(width);
    for (unsigned x = 0; x < width; ++x) p[x] = static_cast<uint16_t>((x * 37 + seed * 101 + 5) & (raw10 ? 0x3FF : 0xFFF));
    return p;
}
Bytes pack(const Pixels& p, bool raw10) { return raw10 ? bfm::pack_raw10(p) : bfm::pack_raw12(p); }

// Payload beats of one long packet as Packet Analyzer delivers them: four bytes
// per beat from bit 0, sol on the first, eol on the last payload beat.
std::vector<PayloadBeat> beats_of(const Bytes& payload, bool raw10, bool sof, uint8_t vc = 0) {
    std::vector<PayloadBeat> v;
    for (size_t i = 0; i < payload.size(); i += 4) {
        PayloadBeat b;
        const size_t n = std::min<size_t>(4, payload.size() - i);
        for (size_t k = 0; k < n; ++k) b.bytes[k] = payload[i + k];
        b.byte_en = static_cast<uint8_t>((1u << n) - 1);
        b.sol = i == 0;
        b.eol = i + 4 >= payload.size();
        b.sof = sof && i == 0;
        b.vc = vc;
        b.dt = raw10 ? 0x2B : 0x2C;
        b.raw10 = raw10;
        v.push_back(b);
    }
    return v;
}

// ================================================================ Block 4

struct B2pRun {
    Pixels px;  // concatenated output pixels
    std::vector<PxBeat> outs;  // emitting cycles
    unsigned zero_cycles_with_flags = 0;
};

B2pRun run_b2p(ByteToPixel& u, const std::vector<PayloadBeat>& in, unsigned idle_between = 0) {
    B2pRun r;
    auto take = [&] {
        const PxBeat& o = u.out();
        if (o.count == 0) {
            if (o.sof || o.sol || o.eol || o.err_wc_align) ++r.zero_cycles_with_flags;
            return;
        }
        r.outs.push_back(o);
        for (unsigned i = 0; i < o.count; ++i) r.px.push_back(o.px[i]);
        for (unsigned i = o.count; i < 4; ++i)
            if (o.px[i] != 0) ++r.zero_cycles_with_flags;  // lanes above count must be 0
    };
    for (const auto& b : in) {
        u.clock(&b);
        take();
        for (unsigned i = 0; i < idle_between; ++i) {
            u.clock(nullptr);
            take();
        }
    }
    u.clock(nullptr);
    take();
    return r;
}

void b2p_exact(bool raw10) {
    // Several lines of different widths (all WC mod 4 values), back to back and
    // with idle cycles between beats: exact pixels, one sof, sol/eol per line.
    const unsigned step = raw10 ? 4 : 2;
    for (unsigned idle : {0u, 1u, 3u}) {
        ByteToPixel u;
        u.reset();
        std::vector<PayloadBeat> in;
        Pixels expect;
        std::vector<unsigned> widths;
        for (unsigned w = step; w <= 16 * step; w += step) widths.push_back(w);
        widths.push_back(raw10 ? 640 : 642);
        for (size_t li = 0; li < widths.size(); ++li) {
            const Pixels p = px_line(widths[li], raw10, static_cast<unsigned>(li));
            expect.insert(expect.end(), p.begin(), p.end());
            const auto b = beats_of(pack(p, raw10), raw10, li == 0);
            in.insert(in.end(), b.begin(), b.end());
        }
        const B2pRun r = run_b2p(u, in, idle);
        CHECK(r.px == expect);
        CHECK_EQ(r.zero_cycles_with_flags, 0u);
        unsigned sof = 0, sol = 0, eol = 0, err = 0;
        for (const auto& o : r.outs) {
            sof += o.sof;
            sol += o.sol;
            eol += o.eol;
            err += o.err_wc_align;
            CHECK(o.count == (raw10 ? 4 : 2) || (!raw10 && o.count == 4));
            CHECK(o.raw10 == raw10);
        }
        CHECK_EQ(sof, 1u);
        CHECK(r.outs.front().sof && r.outs.front().sol);
        CHECK_EQ(sol, widths.size());
        CHECK_EQ(eol, widths.size());
        CHECK_EQ(err, 0u);
    }
}

void b2p_wc_align() {
    // MAS Table 622, line-end resolution, with the expected output of each case.
    struct Case {
        bool raw10;
        unsigned wc;
        unsigned pixels;  // emitted for the line
        bool err;
    };
    const Case cases[] = {
        {true, 10, 8, false},  // valid line
        {true, 8, 4, true},    // whole unit + 3 bytes dropped
        {true, 4, 4, true},    // no whole unit: one zero-padded unit
        {true, 1, 4, true},
        {false, 6, 4, false},
        {false, 5, 4, true},   // RAW12 WC = 5: unit, then a zero-padded unit (MAS example)
        {false, 2, 2, true},   // zero-padded unit
        {false, 7, 4, true},
    };
    for (const auto& c : cases) {
        ByteToPixel u;
        u.reset();
        Bytes payload(c.wc);
        for (unsigned i = 0; i < c.wc; ++i) payload[i] = static_cast<uint8_t>(0x11 * (i + 1));
        auto in = beats_of(payload, c.raw10, true);
        // A clean line follows: the carry never crosses the line boundary.
        const Pixels next = px_line(c.raw10 ? 8 : 4, c.raw10, 9);
        const auto b2 = beats_of(pack(next, c.raw10), c.raw10, false);
        in.insert(in.end(), b2.begin(), b2.end());
        const B2pRun r = run_b2p(u, in);
        CHECK_EQ(r.px.size(), c.pixels + next.size());
        CHECK(Pixels(r.px.end() - next.size(), r.px.end()) == next);
        unsigned err = 0, eol = 0;
        for (const auto& o : r.outs) {
            err += o.err_wc_align;
            eol += o.eol;
            if (o.err_wc_align) CHECK(o.eol);
        }
        CHECK_EQ(err, c.err ? 1u : 0u);
        CHECK_EQ(eol, 2u);
        if (c.raw10 && c.wc == 4) {
            // zero-padded unit: P0..P3 = byte << 2, 2 LSBs from the missing byte 4 = 0
            for (unsigned i = 0; i < 4; ++i) CHECK_EQ(r.px[i], uint16_t(payload[i] << 2));
        }
    }
    // A line whose end-of-line beat was lost: the next line start clears the
    // carry (line-start safety net), so the next line decodes exactly.
    for (bool raw10 : {true, false}) {
        ByteToPixel u;
        u.reset();
        Bytes cut = pack(px_line(raw10 ? 12 : 6, raw10, 1), raw10);  // 15 or 9 bytes
        auto in = beats_of(cut, raw10, true);
        in.pop_back();  // the end-of-line beat never arrives
        const Pixels next = px_line(raw10 ? 8 : 4, raw10, 2);
        const auto b2 = beats_of(pack(next, raw10), raw10, false);
        in.insert(in.end(), b2.begin(), b2.end());
        const B2pRun r = run_b2p(u, in);
        CHECK(r.px.size() >= next.size());
        CHECK(Pixels(r.px.end() - next.size(), r.px.end()) == next);
        unsigned eol = 0;
        for (const auto& o : r.outs) eol += o.eol;
        CHECK_EQ(eol, 1u);
    }
    // End-of-line beat without payload byte (contract violation): one all-zero unit.
    ByteToPixel u;
    u.reset();
    PayloadBeat b;
    b.raw10 = true;
    b.sol = b.eol = true;
    b.byte_en = 0;
    u.clock(&b);
    CHECK(u.out().count == 4 && u.out().eol && u.out().err_wc_align && u.out().sol);
    CHECK(u.out().px == (std::array<uint16_t, 4>{0, 0, 0, 0}));
}

// ================================================================ Block 7

PxBeat entry(unsigned i, bool eol = false) {
    PxBeat e;
    e.count = 4;
    e.px = {static_cast<uint16_t>(i & 0xFFF), static_cast<uint16_t>(i >> 12 & 0xFFF), 0, 0};
    e.eol = eol;
    return e;
}
unsigned entry_id(const PxBeat& e) { return e.px[0] | unsigned(e.px[1]) << 12; }

void lb_fwft() {
    // Throughput and latency with the consumer always ready.
    {
        LineBuffer lb(64, 56);
        unsigned next = 0, first_out = 0, got = 0;
        for (unsigned c = 0; c < 200; ++c) {
            const bool v = lb.out_valid();
            if (v) {
                CHECK_EQ(entry_id(lb.out()), got);
                if (got == 0) first_out = c;
                ++got;
            }
            const PxBeat e = entry(next);
            lb.clock(c < 100 ? &e : nullptr, true);
            if (c < 100) ++next;
        }
        CHECK_EQ(got, 100u);
        CHECK_EQ(first_out, 3u);  // write at 0, rd_en at 1, fb_px_valid two cycles later
        CHECK(lb.max_fill() <= 3);
    }
    // Random stalls on both sides: order kept, no loss.
    {
        std::mt19937 rng(7);
        LineBuffer lb(64, 56);
        unsigned next = 0, got = 0;
        for (unsigned c = 0; c < 20000; ++c) {
            const bool ready = rng() % 3 != 0;
            if (lb.out_valid() && ready) {
                CHECK_EQ(entry_id(lb.out()), got);
                ++got;
            }
            const bool wr = rng() % 2 == 0 && lb.fill() < 60;
            const PxBeat e = entry(next);
            CHECK(!lb.clock(wr ? &e : nullptr, ready));
            if (wr) ++next;
        }
        CHECK(got > 5000 && got + lb.fill() == next);
    }
    // Overflow: the last slot is reserved for an end-of-line entry.
    {
        LineBuffer lb(16, 12);
        unsigned overflow = 0;
        for (unsigned i = 0; i < 15; ++i) {
            const PxBeat e = entry(i);
            overflow += lb.clock(&e, false);
            CHECK_EQ(lb.full(), lb.fill() >= 12);
        }
        CHECK_EQ(lb.fill(), 15u);
        CHECK_EQ(overflow, 0u);
        PxBeat e = entry(100);
        CHECK(lb.clock(&e, false));  // normal entry refused at depth - 1
        CHECK_EQ(lb.fill(), 15u);
        e = entry(101, true);
        CHECK(!lb.clock(&e, false));  // end of line takes the reserved slot
        CHECK_EQ(lb.fill(), 16u);
        e = entry(102, true);
        CHECK(lb.clock(&e, false));  // and the next one is refused
        PxBeat z;  // px_valid_count = 0: no write, no overflow
        CHECK(!lb.clock(&z, false));
        // Drain: entries 0..14 then 101.
        std::vector<unsigned> out;
        for (unsigned c = 0; c < 40; ++c) {
            if (lb.out_valid()) out.push_back(entry_id(lb.out()));
            lb.clock(nullptr, true);
        }
        CHECK_EQ(out.size(), 16u);
        CHECK_EQ(out.back(), 101u);
        CHECK(lb.empty());
        // Flush (reset) empties in one cycle, including the output stage.
        for (unsigned i = 0; i < 5; ++i) {
            const PxBeat f = entry(i);
            lb.clock(&f, false);
        }
        lb.clock(nullptr, false);
        CHECK(lb.out_valid());
        lb.reset();
        CHECK(lb.empty() && !lb.out_valid() && lb.fill() == 0);
    }
}

// ================================================================ Block 8

struct FbRun {
    std::vector<OutBeat> s, d;
    unsigned overflow = 0, not_ready = 0;
};

// Entries in FWFT order through Format Bridge; consumers ready per their pattern.
FbRun run_fb(const FbConfig& cfg, const std::vector<PxBeat>& in, std::function<bool(unsigned)> sready,
             std::function<bool(unsigned)> dready) {
    FormatBridge fb;
    fb.reset();
    FbRun r;
    size_t i = 0;
    for (unsigned c = 0; c < 200000 && (i < in.size() || fb.stream_valid() || fb.dma_valid() || !fb.quiescent()); ++c) {
        const bool ready = fb.ready(cfg);
        if (!ready) ++r.not_ready;
        const PxBeat* take = (i < in.size() && ready) ? &in[i] : nullptr;
        const bool sp = fb.stream_valid() && sready(c), dp = fb.dma_valid() && dready(c);
        if (sp) r.s.push_back(fb.stream_head());
        if (dp) r.d.push_back(fb.dma_head());
        r.overflow += fb.clock(cfg, take, sp, dp);
        if (take) ++i;
    }
    return r;
}

// Expected 64-bit beats of one line from its bytes (byte 0 in bits 7:0).
std::vector<OutBeat> expect_beats(const Bytes& b, bool sof, bool with_sol) {
    std::vector<OutBeat> v;
    for (size_t i = 0; i < b.size(); i += 8) {
        OutBeat o;
        const size_t n = std::min<size_t>(8, b.size() - i);
        for (size_t k = 0; k < n; ++k) o.data |= uint64_t(b[i + k]) << (8 * k);
        o.keep = static_cast<uint8_t>((1u << n) - 1);
        o.eol = i + 8 >= b.size();
        o.sof = sof && i == 0;
        o.sol = with_sol && i == 0;
        v.push_back(o);
    }
    return v;
}

Bytes raw16_bytes(const Pixels& p) {
    Bytes b;
    for (uint16_t v : p) {
        b.push_back(static_cast<uint8_t>(v & 0xFF));
        b.push_back(static_cast<uint8_t>(v >> 8));
    }
    return b;
}

void fb_formats() {
    for (bool raw10 : {true, false}) {
        // Pixel entries as Byte-to-Pixel Unpacking produces them.
        ByteToPixel u;
        u.reset();
        std::vector<PayloadBeat> in;
        std::vector<Pixels> lines;
        const unsigned widths[] = {8, 12, 16, 20, 36, 64, 100, 640};
        for (unsigned li = 0; li < 8; ++li) {
            lines.push_back(px_line(widths[li], raw10, li));
            const auto b = beats_of(pack(lines.back(), raw10), raw10, li == 0);
            in.insert(in.end(), b.begin(), b.end());
        }
        const B2pRun br = run_b2p(u, in);
        std::vector<OutBeat> exp_packed, exp_raw16;
        for (unsigned li = 0; li < lines.size(); ++li) {
            auto a = expect_beats(pack(lines[li], raw10), li == 0, true);
            auto b = expect_beats(raw16_bytes(lines[li]), li == 0, true);
            exp_packed.insert(exp_packed.end(), a.begin(), a.end());
            exp_raw16.insert(exp_raw16.end(), b.begin(), b.end());
        }
        auto strip_sol = [](std::vector<OutBeat> v) {
            for (auto& o : v) o.sol = false;
            return v;
        };
        // Stream packed + DMA RAW16 at once, both always ready.
        FbConfig cfg{true, false, true, true};
        FbRun r = run_fb(cfg, br.outs, [](unsigned) { return true; }, [](unsigned) { return true; });
        CHECK(r.s == strip_sol(exp_packed));
        CHECK(r.d == exp_raw16);
        CHECK_EQ(r.overflow, 0u);
        // Stream RAW16 + DMA packed, random stalls on both consumers.
        std::mt19937 rng(raw10 ? 3 : 4);
        std::vector<bool> sr(200000), dr(200000);
        for (unsigned i = 0; i < sr.size(); ++i) {
            sr[i] = rng() % 4 != 0;
            dr[i] = rng() % 5 > 1;
        }
        cfg = FbConfig{true, true, true, false};
        r = run_fb(cfg, br.outs, [&](unsigned c) { return sr[c]; }, [&](unsigned c) { return dr[c]; });
        CHECK(r.s == strip_sol(exp_raw16));
        CHECK(r.d == exp_packed);
        CHECK_EQ(r.overflow, 0u);
        // Stream path only; disabled DMA path does not back-pressure.
        cfg = FbConfig{true, false, false, false};
        r = run_fb(cfg, br.outs, [](unsigned) { return true; }, [](unsigned) { return false; });
        CHECK(r.s == strip_sol(exp_packed));
        CHECK(r.d.empty());
        // Both disabled: the bridge keeps draining its input.
        cfg = FbConfig{false, false, false, false};
        r = run_fb(cfg, br.outs, [](unsigned) { return false; }, [](unsigned) { return false; });
        CHECK(r.s.empty() && r.d.empty());
    }
    // Stream consumer stalled: Format Bridge stops at one free entry, so the
    // line-end flush beat written after fb_ready fell is never lost.
    // A first line of another width shifts the FIFO level at which each line
    // ends, so that some line ends with a drain beat and a residual exactly
    // when one entry is free.
    for (unsigned w : {12u, 16u, 20u, 28u, 36u})
    for (unsigned first : {4u, 8u, 12u, 16u, 20u, 24u, 28u}) {
        ByteToPixel u;
        u.reset();
        std::vector<PayloadBeat> in;
        std::vector<OutBeat> exp;
        for (unsigned li = 0; li < 12; ++li) {
            const Pixels p = px_line(li == 0 ? first : w, true, li);
            const auto b = beats_of(pack(p, true), true, li == 0);
            in.insert(in.end(), b.begin(), b.end());
            auto e = expect_beats(pack(p, true), li == 0, false);
            exp.insert(exp.end(), e.begin(), e.end());
        }
        const B2pRun br = run_b2p(u, in);
        FbConfig cfg{true, false, false, false};
        const FbRun r = run_fb(cfg, br.outs, [](unsigned c) { return c > 2000; }, [](unsigned) { return true; });
        CHECK_EQ(r.overflow, 0u);
        CHECK(r.s == exp);
    }
    // err_wc_align rides on the end-of-line beat of the line.
    {
        PxBeat a;
        a.count = 4;
        a.raw10 = true;
        a.sol = a.sof = true;
        PxBeat b = a;
        b.sol = b.sof = false;
        b.eol = b.err_wc_align = true;
        FbConfig cfg{true, false, false, false};
        const FbRun r = run_fb(cfg, {a, b}, [](unsigned) { return true; }, [](unsigned) { return true; });
        CHECK_EQ(r.s.size(), 2u);  // 10 bytes: 8 + 2
        CHECK(!r.s[0].err && r.s[1].err && r.s[1].eol && r.s[1].keep == 0x03);
    }
}

// ================================================================ Block 12

void axis_skid() {
    // Random TREADY: every beat delivered once, in order, protocol kept.
    std::deque<OutBeat> src;
    std::vector<AxisMonitor::Line> expect;
    std::mt19937 rng(11);
    for (unsigned l = 0; l < 50; ++l) {
        AxisMonitor::Line L;
        L.sof = l % 10 == 0;
        const unsigned n = 3 + rng() % 40;
        for (unsigned i = 0; i < n; ++i) L.bytes.push_back(static_cast<uint8_t>(rng()));
        for (const auto& o : expect_beats(L.bytes, L.sof, false)) src.push_back(o);
        expect.push_back(L);
    }
    AxisMaster m;
    m.reset();
    AxisMonitor mon;
    std::vector<bool> rdy(100000);
    for (unsigned i = 0; i < rdy.size(); ++i) rdy[i] = rng() % 3 != 0;
    mon.set_ready([&](uint64_t c) { return rdy[c % rdy.size()]; });
    for (unsigned c = 0; c < 20000; ++c) {
        const bool tready = mon.axis_cycle(m.tvalid(), m.beat());
        const bool v = !src.empty();
        const OutBeat head = v ? src.front() : OutBeat{};
        if (m.clock(tready, v, head, false, 0, true)) src.pop_front();
    }
    CHECK(src.empty());
    CHECK(mon.violations().empty());
    CHECK_EQ(mon.lines().size(), expect.size());
    for (size_t i = 0; i < std::min(expect.size(), mon.lines().size()); ++i) {
        CHECK(mon.lines()[i].bytes == expect[i].bytes);
        CHECK_EQ(mon.lines()[i].sof, expect[i].sof);
        CHECK(!mon.lines()[i].err);
    }
    CHECK(mon.stall_cycles() > 20);
}

void axis_abort() {
    // An abort while a line is open: the real beats already held are delivered,
    // the line is closed by a null beat (TKEEP 0, TLAST, TUSER[1]); the rest of
    // the frame is kept from the ISP until its next Start of Frame.
    AxisMaster m;
    m.reset();
    AxisMonitor mon;
    bool stall = true;
    mon.set_ready([&](uint64_t) { return !stall; });
    std::deque<OutBeat> src;
    Bytes l0(40, 0xA1), l1(40, 0xB2), l2(16, 0xC3);
    for (const auto& o : expect_beats(l0, true, false)) src.push_back(o);
    for (const auto& o : expect_beats(l1, false, false)) src.push_back(o);
    auto step = [&](bool abort) {
        const bool tready = mon.axis_cycle(m.tvalid(), m.beat());
        const bool v = !src.empty();
        const OutBeat head = v ? src.front() : OutBeat{};
        if (m.clock(tready, v, head, abort, 0x1, true)) src.pop_front();
    };
    stall = false;
    for (unsigned c = 0; c < 7; ++c) step(false);  // l0 (5 beats) and part of l1
    stall = true;
    for (unsigned c = 0; c < 4; ++c) step(false);  // out + skid full
    CHECK(m.line_open());
    step(true);  // asm_abort_req
    CHECK(m.state() == AxisMaster::State::Drain);
    for (unsigned c = 0; c < 20; ++c) step(false);  // ISP still stalled: nothing lost, held
    CHECK(m.tvalid());
    // Upstream keeps presenting the rest of l1: taken and discarded (sink mode).
    CHECK(src.empty());
    stall = false;
    for (unsigned c = 0; c < 10; ++c) step(false);
    CHECK(m.state() == AxisMaster::State::Passthrough);
    // A new line without sof (rest of the aborted frame) is gated, then a new frame passes.
    for (const auto& o : expect_beats(l2, false, false)) src.push_back(o);
    for (const auto& o : expect_beats(l2, true, false)) src.push_back(o);
    for (unsigned c = 0; c < 20; ++c) step(false);
    CHECK(mon.violations().empty());
    const auto& L = mon.lines();
    CHECK_EQ(L.size(), 3u);
    if (L.size() == 3) {
        CHECK(L[0].bytes == l0 && L[0].sof && !L[0].err);
        CHECK(L[1].aborted && L[1].err && !L[1].sof);
        CHECK(L[1].bytes.size() < l1.size() && L[1].bytes.size() >= 16);  // the beats held before the abort
        CHECK(L[2].bytes == l2 && L[2].sof);
    }
    CHECK_EQ(mon.abort_beats(), 1u);
    // An abort with no line open still closes the exposure in the mask: one
    // zero-byte beat outside a frame (the monitor does not count it as a line).
    const size_t lines_before = mon.lines().size();
    step(true);
    for (unsigned c = 0; c < 5; ++c) step(false);
    CHECK_EQ(mon.abort_beats(), 2u);
    CHECK_EQ(mon.lines().size(), lines_before);
}

// ================================================================ Block 5

// Independent oracle of the TPG pattern (MAS 6.5, D11): band = min(7, coord / base),
// colour bars of Table 625, RGGB sites.
uint16_t tpg_oracle(unsigned x, unsigned y, unsigned w, unsigned h, bool horizontal, bool raw10) {
    static const bool R[8] = {1, 1, 0, 0, 1, 1, 0, 0}, G[8] = {1, 1, 1, 1, 0, 0, 0, 0}, B[8] = {1, 0, 1, 0, 1, 0, 1, 0};
    const unsigned base = horizontal ? (h >> 3) : (w >> 3);
    const unsigned coord = horizontal ? y : x;
    const unsigned band = std::min(7u, coord / base);
    bool on;
    if (y % 2 == 0 && x % 2 == 0) on = R[band];
    else if (y % 2 == 1 && x % 2 == 1) on = B[band];
    else on = G[band];
    return on ? (raw10 ? 0x3FF : 0xFFF) : 0;
}

struct TpgCapture {
    std::vector<Pixels> lines;
    std::vector<uint64_t> fs_cycles;
    unsigned fe = 0, eof = 0, sof_flags = 0;
};

void tpg_run(TestPatternGenerator& g, TpgConfig& cfg, uint64_t cycles, TpgCapture& cap,
             std::function<bool(uint64_t)> ready = nullptr, uint64_t c0 = 0) {
    for (uint64_t c = c0; c < c0 + cycles; ++c) {
        const bool r = ready ? ready(c) : true;
        if (g.px_valid() && r) {
            const PxBeat p = g.px();
            if (p.sol) cap.lines.emplace_back();
            cap.sof_flags += p.sof;
            for (unsigned i = 0; i < 4; ++i) cap.lines.back().push_back(p.px[i]);
            CHECK_EQ(p.count, 4u);
        }
        const auto ev = g.clock(cfg, true, r, true);
        if (ev.fs) cap.fs_cycles.push_back(c);
        cap.fe += ev.fe;
        cap.eof += ev.eof;
    }
}

void tpg_pattern() {
    for (bool raw10 : {false, true})
        for (bool hor : {false, true})
            for (unsigned w : {8u, 16u, 20u, 64u, 66u})
                for (unsigned h : {8u, 9u, 16u}) {
                    TestPatternGenerator g;
                    g.reset();
                    TpgConfig cfg;
                    cfg.enable = true;
                    cfg.horizontal = hor;
                    cfg.raw10 = raw10;
                    cfg.width = static_cast<uint16_t>(w);
                    cfg.height = static_cast<uint16_t>(h);
                    TpgCapture cap;
                    const unsigned wt = w & ~3u;  // width truncated to a multiple of 4
                    tpg_run(g, cfg, 2 * (wt / 4 * h + 2) + 4, cap);
                    CHECK(cap.lines.size() >= h);
                    for (unsigned y = 0; y < h && y < cap.lines.size(); ++y) {
                        CHECK_EQ(cap.lines[y].size(), wt);
                        bool ok = true;
                        for (unsigned x = 0; x < wt && x < cap.lines[y].size(); ++x)
                            ok &= cap.lines[y][x] == tpg_oracle(x, y, wt, h, hor, raw10);
                        CHECK(ok);
                    }
                }
}

void tpg_timing() {
    // Frame period with TREADY high: (height + vblank) x ((width >> 2) + hblank) + 2.
    struct T {
        unsigned w, h, hb, vb;
    };
    for (const T t : {T{16, 8, 0, 0}, T{32, 8, 5, 0}, T{32, 8, 0, 3}, T{64, 10, 7, 2}}) {
        TestPatternGenerator g;
        g.reset();
        TpgConfig cfg;
        cfg.enable = true;
        cfg.width = static_cast<uint16_t>(t.w);
        cfg.height = static_cast<uint16_t>(t.h);
        cfg.hblank = static_cast<uint16_t>(t.hb);
        cfg.vblank = static_cast<uint16_t>(t.vb);
        TpgCapture cap;
        const uint64_t period = uint64_t(t.h + t.vb) * (t.w / 4 + t.hb) + 2;
        tpg_run(g, cfg, 3 * period + 10, cap);
        CHECK(cap.fs_cycles.size() >= 3);
        for (size_t i = 1; i < cap.fs_cycles.size(); ++i) CHECK_EQ(cap.fs_cycles[i] - cap.fs_cycles[i - 1], period);
        CHECK(cap.fe == cap.eof && cap.fe >= 2);
    }
    // Dimension guard: truncated width < 8 or height < 8 never starts; a
    // corrected value starts without writing tpg_enable again.
    {
        TestPatternGenerator g;
        g.reset();
        TpgConfig cfg;
        cfg.enable = true;
        cfg.width = 11;  // truncated to 8: legal
        cfg.height = 7;
        TpgCapture cap;
        tpg_run(g, cfg, 100, cap);
        CHECK(cap.lines.empty() && g.state() == TestPatternGenerator::State::Idle);
        cfg.width = 7;
        cfg.height = 8;
        tpg_run(g, cfg, 100, cap);
        CHECK(cap.lines.empty());
        cfg.width = 11;
        tpg_run(g, cfg, 100, cap);
        CHECK(!cap.lines.empty() && cap.lines[0].size() == 8u);
    }
    // tpg_enable cleared mid-frame: the frame is completed, then IDLE; a quick
    // de-assert/re-assert does not stop generation.
    {
        TestPatternGenerator g;
        g.reset();
        TpgConfig cfg;
        cfg.enable = true;
        cfg.width = 16;
        cfg.height = 8;
        TpgCapture cap;
        tpg_run(g, cfg, 10, cap);  // frame 0 in progress
        cfg.enable = false;
        tpg_run(g, cfg, 1, cap, nullptr, 10);
        cfg.enable = true;  // quick re-enable
        tpg_run(g, cfg, 1, cap, nullptr, 11);
        CHECK(g.state() == TestPatternGenerator::State::Running);
        cfg.enable = false;
        tpg_run(g, cfg, 200, cap, nullptr, 12);
        CHECK(g.state() == TestPatternGenerator::State::Idle);
        CHECK_EQ(cap.lines.size(), 8u);  // exactly the frame in progress, whole
        CHECK_EQ(cap.eof, 1u);
        // Stall: the counters hold; the frame still has 8 lines of 16 pixels.
        cfg.enable = true;
        TpgCapture c2;
        tpg_run(g, cfg, 200, c2, [](uint64_t c) { return c % 3 == 0; });
        CHECK(c2.lines.size() >= 8);
        for (unsigned y = 0; y < 8; ++y) CHECK_EQ(c2.lines[y].size(), 16u);
    }
}

// ================================================================ whole datapath

struct Harness : SysEvents, PaEvents {
    SysDatapath dp;
    SysConfig cfg;
    AxisMonitor axis;
    uint64_t period_ps = 4000;  // sys_clk 250 MHz
    uint64_t word_ps = 10000;   // merged words at 100 MHz (4 lanes x 800 Mb/s)
    uint64_t now = 0, next_word = 0;
    std::deque<std::pair<uint64_t, MergedWord>> pending;
    // events
    unsigned lb_full = 0, lb_ovf = 0, cdc_ovf = 0, dropped = 0, done = 0, res_err = 0, wc_align = 0, fb_ovf = 0,
             fnum_err = 0, crc = 0, lane_abort = 0, frame_sync = 0, ecc2 = 0;
    std::vector<uint16_t> capture_ids;
    std::vector<uint64_t> capture_ts;
    std::map<unsigned, FrameInfo> published;
    unsigned publish_events = 0;

    explicit Harness(SysParams p = {}) : dp(p) {
        dp.set_sys_events(this);
        dp.set_pa_events(this);
        dp.set_axis_sink(&axis);
        cfg.core_enable = true;
        cfg.fb.axis_enable = true;
        cfg.fb.stream_raw16 = false;
        dp.reset(true);
    }
    // SysEvents
    void sys_line_buffer_full() override { ++lb_full; }
    void sys_line_buffer_overflow() override { ++lb_ovf; }
    void sys_cdc_overflow() override { ++cdc_ovf; }
    void sys_frame_dropped() override { ++dropped; }
    void sys_capture_start(uint16_t id, uint64_t ts) override {
        capture_ids.push_back(id);
        capture_ts.push_back(ts);
    }
    void sys_capture_done() override { ++done; }
    void sys_published(unsigned vc, const FrameInfo& f) override {
        published[vc] = f;
        ++publish_events;
    }
    void sys_resolution_mismatch(unsigned) override { ++res_err; }
    void sys_err_wc_align() override { ++wc_align; }
    void sys_fmtbridge_overflow() override { ++fb_ovf; }
    void sys_frame_num_err(unsigned) override { ++fnum_err; }
    // PaEvents (forwarded by the datapath)
    void pa_beat(const PayloadBeat&) override {}
    unsigned last_err_line = 0;
    void pa_error(PaError e, int, unsigned line) override {
        last_err_line = line;
        crc += e == PaError::Crc;
        lane_abort += e == PaError::LaneAbort;
        frame_sync += e == PaError::FrameSync;
        ecc2 += e == PaError::Ecc2;
    }
    void pa_packet_done() override {}
    void pa_frame_start(unsigned, uint16_t) override {}
    void pa_frame_end(unsigned, uint16_t) override {}
    void pa_gsp(uint8_t, uint8_t, uint16_t) override {}
    void pa_null_blank() override {}
    void pa_vc_unsupported() override {}

    // One packet in one burst, as Lane Management merges it; gap in word periods.
    void burst(const Bytes& pkt, unsigned gap = 8) {
        if (next_word < now) next_word = now;
        for (size_t i = 0; i < pkt.size(); i += 4) {
            MergedWord w;
            const size_t n = std::min<size_t>(4, pkt.size() - i);
            for (size_t k = 0; k < n; ++k) w.data |= uint32_t(pkt[i + k]) << (8 * k);
            w.byte_valid = static_cast<uint8_t>((1u << n) - 1);
            w.sob = i == 0;
            w.eob = i + 4 >= pkt.size();
            pending.push_back({next_word, w});
            next_word += word_ps;
        }
        next_word += gap * word_ps;
    }
    void marker() {  // a burst discarded by Lane Management
        MergedWord m;
        m.sob = m.eob = true;
        if (next_word < now) next_word = now;
        pending.push_back({next_word, m});
        next_word += 8 * word_ps;
    }
    void step() {
        now += period_ps;
        while (!pending.empty() && pending.front().first < now) {
            dp.cdc_push(pending.front().second, pending.front().first);
            pending.pop_front();
        }
        dp.cycle(cfg, now, now / 1000);
    }
    void run(uint64_t n) {
        for (uint64_t i = 0; i < n; ++i) step();
    }
    // Until every scheduled word is consumed and the datapath is quiescent.
    bool settle(uint64_t max = 2000000) {
        for (uint64_t i = 0; i < max; ++i) {
            if (pending.empty() && dp.quiescent(cfg)) return true;
            step();
        }
        return false;
    }
    std::vector<Bytes> frame(unsigned w, unsigned h, bool raw10, uint16_t fnum = 1, unsigned seed = 0) {
        std::vector<Bytes> lines;
        burst(bfm::short_packet(0, bfm::DT_FS, fnum));
        for (unsigned y = 0; y < h; ++y) {
            lines.push_back(pack(px_line(w, raw10, seed + y), raw10));
            burst(bfm::long_packet(0, raw10 ? bfm::DT_RAW10 : bfm::DT_RAW12, lines.back()));
        }
        burst(bfm::short_packet(0, bfm::DT_FE, fnum));
        return lines;
    }
    // Stream lines of the monitor from index `from`.
    std::vector<Bytes> lines_from(size_t from) const {
        std::vector<Bytes> v;
        for (size_t i = from; i < axis.lines().size(); ++i) v.push_back(axis.lines()[i].bytes);
        return v;
    }
};

void pipe_frame(bool raw10) {
    Harness h;
    h.cfg.fc.expected_width = 64;
    h.cfg.fc.expected_height = 6;
    const auto lines = h.frame(64, 6, raw10);
    // Stop after the Frame End: the capture completes without a next Frame Start.
    CHECK(h.settle());
    CHECK(h.lines_from(0) == lines);
    CHECK(h.axis.violations().empty());
    for (size_t i = 0; i < h.axis.lines().size(); ++i) {
        CHECK_EQ(h.axis.lines()[i].sof, i == 0);
        CHECK(!h.axis.lines()[i].err);
        CHECK_EQ(h.axis.lines()[i].tid, 0u);
    }
    CHECK_EQ(h.done, 1u);
    CHECK_EQ(h.dropped, 0u);
    CHECK_EQ(h.res_err, 0u);
    CHECK(h.capture_ids == std::vector<uint16_t>{1});
    const FrameInfo& f = h.published[0];
    CHECK_EQ(f.line_count, 6u);
    CHECK_EQ(f.byte_count, lines[0].size());
    CHECK_EQ(f.data_type, raw10 ? 0x2Bu : 0x2Cu);
    CHECK(!f.width_mismatch && !f.height_mismatch);
    CHECK(h.dp.cdc_max_fill() <= 3);  // sys_clk 250 MHz against 100 Mword/s
    // RAW16 on the stream path, a second frame: frame id 2, exact samples.
    h.cfg.fb.stream_raw16 = true;
    const size_t from = h.axis.lines().size();
    h.frame(64, 6, raw10, 2, 50);
    CHECK(h.settle());
    std::vector<Bytes> expect;
    for (unsigned y = 0; y < 6; ++y) expect.push_back(raw16_bytes(px_line(64, raw10, 50 + y)));
    CHECK(h.lines_from(from) == expect);
    CHECK(h.capture_ids == (std::vector<uint16_t>{1, 2}));
    CHECK_EQ(h.done, 2u);
    CHECK(h.capture_ts[1] > h.capture_ts[0]);
}

// Lines of a dropped frame that reach the ISP are a prefix of the frame, the
// last one cut by an abort beat or every one complete with an abort beat after them.
void check_dropped_prefix(const Harness& h, size_t from, const std::vector<Bytes>& lines) {
    const auto& L = h.axis.lines();
    for (size_t i = from, k = 0; i < L.size() && k < lines.size(); ++i, ++k) {
        if (L[i].aborted) {
            CHECK(L[i].err && L[i].bytes.size() <= lines[k].size());
            CHECK(std::equal(L[i].bytes.begin(), L[i].bytes.end(), lines[k].begin()));
            break;
        }
        CHECK(L[i].bytes == lines[k]);
    }
}

size_t first_sof_after(const Harness& h, size_t from) {
    for (size_t i = from; i < h.axis.lines().size(); ++i)
        if (h.axis.lines()[i].sof) return i;
    return h.axis.lines().size();
}

void pipe_crc_drop() {
    Harness h;
    h.frame(64, 4, true, 1, 0);
    CHECK(h.settle());
    const size_t from = h.axis.lines().size();
    // Frame 2: a payload byte of line 1 corrupted after the CRC was computed.
    std::vector<Bytes> lines;
    h.burst(bfm::short_packet(0, bfm::DT_FS, 2));
    for (unsigned y = 0; y < 6; ++y) {
        lines.push_back(pack(px_line(64, true, 10 + y), true));
        Bytes p = bfm::long_packet(0, bfm::DT_RAW10, lines.back());
        if (y == 1) {
            p[4 + 7] ^= 0x10;
            lines.back()[7] ^= 0x10;  // cut-through: the line leaves with the byte as received
        }
        h.burst(p);
    }
    h.burst(bfm::short_packet(0, bfm::DT_FE, 2));
    CHECK(h.settle());
    CHECK_EQ(h.crc, 1u);
    CHECK_EQ(h.dropped, 1u);
    CHECK_EQ(h.done, 1u);  // frame 1 only
    CHECK(h.axis.abort_beats() >= 1);
    // Nothing of frame 2 after line 1 reaches the ISP.
    const auto& L = h.axis.lines();
    CHECK(L.size() - from <= 2);
    check_dropped_prefix(h, from, lines);
    CHECK(h.dp.control_fsm().state() == ControlFsm::State::Skip);
    // Frame 3 complete and clean.
    const size_t from3 = h.axis.lines().size();
    const auto l3 = h.frame(64, 4, true, 3, 30);
    CHECK(h.settle());
    CHECK(h.lines_from(from3) == l3);
    CHECK_EQ(h.done, 2u);
    CHECK_EQ(h.dropped, 1u);
    CHECK(h.dp.control_fsm().state() == ControlFsm::State::Run);
    CHECK(h.axis.violations().empty());
}

void pipe_lb_full() {
    // The ISP stalls: stream FIFO, then Line Buffer fill; at FULL_WATERMARK the
    // frame is dropped on the sensor path (no pixel lost silently, no overflow),
    // and the next frame is delivered whole.
    Harness h;
    bool stall = false;
    h.axis.set_ready([&](uint64_t) { return !stall; });
    const auto l1 = h.frame(640, 2, true, 1, 0);
    CHECK(h.settle());
    CHECK(h.lines_from(0) == l1);
    const size_t from = h.axis.lines().size();
    stall = true;
    std::vector<Bytes> l2 = h.frame(640, 20, true, 2, 100);  // 20 x 160 entries > 1740
    for (unsigned i = 0; i < 400000 && h.lb_full == 0; ++i) h.step();
    CHECK_EQ(h.lb_full, 1u);
    h.run(8);  // HOLD (2 cycles) then FLUSH: Line Buffer and Format Bridge emptied in one cycle
    CHECK(h.dp.line_buffer().fill() <= 2);
    CHECK_EQ(h.dp.format_bridge().stream_count(), 0u);
    CHECK(h.dp.line_buffer().max_fill() >= 1740 && h.dp.line_buffer().max_fill() < 2048);
    h.run(50000);  // still stalled: the drop has flushed, the rest of the frame is sunk
    stall = false;
    CHECK(h.settle());
    CHECK_EQ(h.lb_ovf, 0u);
    CHECK_EQ(h.dropped, 1u);
    CHECK_EQ(h.done, 1u);
    check_dropped_prefix(h, from, l2);
    const size_t from3 = h.axis.lines().size();
    const auto l3 = h.frame(640, 3, true, 3, 200);
    CHECK(h.settle());
    CHECK(h.lines_from(from3) == l3);
    CHECK_EQ(h.done, 2u);
    CHECK(h.axis.violations().empty());
    CHECK_EQ(h.lb_full, 1u);
}

void pipe_lb_overflow() {
    // A Line Buffer too small for the reaction budget: a write is refused, the
    // overflow is reported and the frame is dropped; the end-of-line slot closes
    // the line in flight.
    SysParams p;
    p.line_buffer_depth = 40;
    p.full_watermark = 39;
    Harness h(p);
    h.word_ps = 4000;  // one word per sys_clk cycle
    bool stall = true;
    h.axis.set_ready([&](uint64_t) { return !stall; });
    h.frame(640, 2, true, 1, 0);
    h.run(20000);
    stall = false;
    CHECK(h.settle());
    CHECK(h.lb_ovf >= 1);
    CHECK_EQ(h.dropped, 1u);
    CHECK(h.axis.violations().empty());
    const size_t from = h.axis.lines().size();
    const auto l2 = h.frame(16, 2, true, 2, 9);
    CHECK(h.settle());
    CHECK(h.lines_from(from) == l2);
}

void pipe_cdc() {
    // sys_clk slower than the merged-word rate: the asynchronous FIFO overflows,
    // the burst is replaced by an abort marker and the frame is dropped (C25).
    Harness h;
    h.period_ps = 20000;  // 50 MHz against 100 Mword/s
    h.frame(640, 2, true, 1, 0);
    CHECK(h.settle());
    CHECK(h.cdc_ovf >= 1);
    CHECK(h.dp.cdc_lost_words() > 0);
    CHECK(h.lane_abort >= 1);
    CHECK_EQ(h.dropped, 1u);
    CHECK_EQ(h.dp.cdc_max_fill(), 16u);
    // Short lines fit the FIFO: delivered.
    const size_t from = h.axis.lines().size();
    const auto l = h.frame(16, 3, true, 2, 5);
    CHECK(h.settle());
    CHECK(h.lines_from(first_sof_after(h, from)) == l);
    CHECK_EQ(h.done, 1u);
}

void pipe_wc_align() {
    // RAW10 line whose WC is not a multiple of 5: err_wc_align, TUSER[1] on the
    // TLAST beat of that line only, and a frame-level drop is not implied.
    Harness h;
    h.burst(bfm::short_packet(0, bfm::DT_FS, 1));
    const Bytes good = pack(px_line(32, true, 1), true);
    Bytes bad = good;
    bad.resize(good.size() - 2);
    h.burst(bfm::long_packet(0, bfm::DT_RAW10, good));
    h.burst(bfm::long_packet(0, bfm::DT_RAW10, bad));
    h.burst(bfm::long_packet(0, bfm::DT_RAW10, good));
    h.burst(bfm::short_packet(0, bfm::DT_FE, 1));
    CHECK(h.settle());
    CHECK_EQ(h.wc_align, 1u);
    const auto& L = h.axis.lines();
    CHECK_EQ(L.size(), 3u);
    if (L.size() == 3) {
        CHECK(!L[0].err && L[1].err && !L[2].err);
        CHECK(L[0].bytes == good && L[2].bytes == good);
        // 38 bytes: 7 whole units, then the 3 bytes on hand at the end of the
        // line zero-padded into an 8th unit (MAS Table 622).
        Bytes exp = bad;
        exp.push_back(0);
        exp.push_back(0);
        CHECK(L[1].bytes == exp);
    }
    CHECK_EQ(h.done, 1u);
}

void pipe_tpg() {
    Harness h;
    // No sensor: enabling the TPG switches at once and its frames reach the ISP.
    h.cfg.tpg.enable = true;
    h.cfg.tpg.width = 32;
    h.cfg.tpg.height = 8;
    h.cfg.tpg.raw10 = true;
    for (unsigned i = 0; i < 20000 && h.done < 2; ++i) h.step();
    h.run(100);  // the stream output trails the frame events
    CHECK(h.dp.tpg_mux().active_sel());
    CHECK(!h.dp.tpg_mux().switch_pending());
    CHECK(h.done >= 2u);
    CHECK_EQ(h.publish_events, 0u);  // no geometry on the TPG path
    CHECK(h.axis.lines().size() >= 16);
    for (unsigned y = 0; y < 16 && y < h.axis.lines().size(); ++y) {
        Pixels p(32);
        for (unsigned x = 0; x < 32; ++x) p[x] = tpg_oracle(x, y % 8, 32, 8, false, true);
        CHECK(h.axis.lines()[y].bytes == pack(p, true));
        CHECK_EQ(h.axis.lines()[y].sof, y % 8 == 0);
    }
    // Disable: the frame in progress completes, then the RX path is selected
    // again once Line Buffer is empty, and a sensor frame is delivered.
    h.cfg.tpg.enable = false;
    CHECK(h.settle());
    CHECK(!h.dp.tpg_mux().active_sel());
    CHECK_EQ(h.axis.lines().size() % 8, 0u);  // whole TPG frames only
    const unsigned tpg_frames = h.done;
    const size_t from = h.axis.lines().size();
    const auto l = h.frame(64, 4, true, 1, 3);
    CHECK(h.settle());
    CHECK(h.lines_from(from) == l);
    CHECK_EQ(h.done, tpg_frames + 1);
    // Enable the TPG while a sensor frame is open: the switch waits for its
    // Frame End (src_switch_pending), the sensor frame is not truncated.
    const size_t from2 = h.axis.lines().size();
    h.burst(bfm::short_packet(0, bfm::DT_FS, 2));
    const Bytes ln = pack(px_line(64, true, 7), true);
    h.burst(bfm::long_packet(0, bfm::DT_RAW10, ln));
    CHECK(h.settle());
    h.cfg.tpg.enable = true;
    h.run(2000);
    CHECK(h.dp.tpg_mux().switch_pending());
    CHECK(!h.dp.tpg_mux().active_sel());
    h.burst(bfm::long_packet(0, bfm::DT_RAW10, ln));
    h.burst(bfm::short_packet(0, bfm::DT_FE, 2));
    for (unsigned i = 0; i < 20000 && !h.dp.tpg_mux().active_sel(); ++i) h.step();
    CHECK(h.dp.tpg_mux().active_sel());
    h.run(3000);
    CHECK(h.axis.lines().size() >= from2 + 2 + 8);
    if (h.axis.lines().size() >= from2 + 3) {
        CHECK(h.axis.lines()[from2].bytes == ln && h.axis.lines()[from2 + 1].bytes == ln);
        CHECK(h.axis.lines()[from2 + 2].sof);  // the first TPG frame starts whole
    }
    CHECK(h.axis.violations().empty());
}

void fc_checks() {
    // Resolution check, frame-number check, missing Frame End, frame ids.
    Harness h;
    h.cfg.fc.expected_width = 32;
    h.cfg.fc.expected_height = 4;
    h.cfg.fc.fnum_check_en = true;
    h.frame(32, 4, true, 1);
    CHECK(h.settle());
    CHECK_EQ(h.res_err, 0u);
    h.frame(32, 3, true, 2);  // height mismatch
    CHECK(h.settle());
    CHECK_EQ(h.res_err, 1u);
    CHECK(h.published[0].height_mismatch && !h.published[0].width_mismatch && h.published[0].line_count == 3);
    h.frame(28, 4, true, 3);  // width mismatch (RAW10: 35 bytes, expected 40)
    CHECK(h.settle());
    CHECK_EQ(h.res_err, 2u);
    CHECK(h.published[0].width_mismatch && !h.published[0].height_mismatch && h.published[0].byte_count == 35);
    h.frame(32, 4, false, 4);  // RAW12 reference (48 bytes): no mismatch
    CHECK(h.settle());
    CHECK_EQ(h.res_err, 2u);
    CHECK_EQ(h.fnum_err, 0u);
    h.frame(32, 4, true, 6);  // 4 -> 6: sequence error
    CHECK(h.settle());
    CHECK_EQ(h.fnum_err, 1u);
    h.frame(32, 4, true, 1);  // 1: numbering restart, exempt
    CHECK(h.settle());
    CHECK_EQ(h.fnum_err, 1u);
    CHECK_EQ(h.done, 6u);
    CHECK(h.capture_ids == (std::vector<uint16_t>{1, 2, 3, 4, 5, 6}));
    // Frame End lost: the next Frame Start closes the frame (sync close): no
    // resolution report, the capture ends, the frame is not dropped.
    h.burst(bfm::short_packet(0, bfm::DT_FS, 2));
    h.burst(bfm::long_packet(0, bfm::DT_RAW10, pack(px_line(32, true, 0), true)));
    const unsigned res0 = h.res_err;
    h.frame(32, 4, true, 3);
    CHECK(h.settle());
    CHECK_EQ(h.frame_sync, 1u);
    CHECK_EQ(h.res_err, res0);
    CHECK_EQ(h.done, 8u);
    CHECK_EQ(h.dropped, 0u);

    // C36: EXPECTED_RESOLUTION left at 0 reports every frame; after reset the
    // previous frame number is 0, so a first FS numbered 5 is a sequence error.
    Harness z;
    z.cfg.fc.fnum_check_en = true;
    z.frame(32, 2, true, 5);
    CHECK(z.settle());
    CHECK_EQ(z.res_err, 1u);
    CHECK_EQ(z.fnum_err, 1u);

    // Embedded data: accounting and extraction (two slots), published at FE.
    Harness e;
    e.cfg.pa.metadata_capture_en = true;
    e.cfg.pa.metadata_dt = 0x12;
    e.cfg.fc.metadata_capture_en = true;
    e.cfg.fc.emb_extract_en = true;
    e.cfg.fc.emb_cfg[0] = 2u | (2u << 8) | (0u << 24);  // line 0, offset 2, 2 bytes
    e.cfg.fc.emb_cfg[1] = 3u | (9u << 8) | (1u << 24);  // line 1, offset 9, 3 bytes
    Bytes emb0(12), emb1(14);
    for (unsigned i = 0; i < emb0.size(); ++i) emb0[i] = static_cast<uint8_t>(0x10 + i);
    for (unsigned i = 0; i < emb1.size(); ++i) emb1[i] = static_cast<uint8_t>(0x80 + i);
    e.burst(bfm::short_packet(0, bfm::DT_FS, 1));
    e.burst(bfm::long_packet(0, bfm::DT_EMBEDDED, emb0));
    e.burst(bfm::long_packet(0, bfm::DT_EMBEDDED, emb1));
    const Bytes img = pack(px_line(32, true, 0), true);
    e.burst(bfm::long_packet(0, bfm::DT_RAW10, img));
    e.burst(bfm::short_packet(0, bfm::DT_FE, 1));
    CHECK(e.settle());
    const FrameInfo& f = e.published[0];
    CHECK_EQ(f.emb_lines, 2u);
    CHECK_EQ(f.emb_words, 3u + 4u);  // payload beats: 12 bytes -> 3, 14 bytes -> 4
    CHECK_EQ(f.extract[0], 0x1312u);
    CHECK_EQ(f.extract[1], 0x8B8A89u);
    CHECK_EQ(f.line_count, 1u);
    CHECK_EQ(e.axis.lines().size(), 1u);  // embedded lines do not enter the pixel path
    // emb_extract_en = 0: counted, values held from the last frame.
    e.cfg.fc.emb_extract_en = false;
    emb0[2] = 0xEE;
    e.burst(bfm::short_packet(0, bfm::DT_FS, 2));
    e.burst(bfm::long_packet(0, bfm::DT_EMBEDDED, emb0));
    e.burst(bfm::short_packet(0, bfm::DT_FE, 2));
    CHECK(e.settle());
    CHECK_EQ(e.published[0].emb_lines, 1u);
    CHECK_EQ(e.published[0].extract[0], 0x1312u);
}

void ctl_core_disable() {
    // core_enable cleared with a capture open: the capture ends as dropped,
    // Control FSM goes to ST_IDLE; after re-enabling, the next frame is delivered.
    Harness h;
    h.burst(bfm::short_packet(0, bfm::DT_FS, 1));
    h.burst(bfm::long_packet(0, bfm::DT_RAW10, pack(px_line(64, true, 0), true)));
    CHECK(h.settle());
    h.cfg.core_enable = false;
    h.dp.core_disable();
    CHECK(h.settle());
    CHECK_EQ(h.dropped, 1u);
    CHECK_EQ(h.done, 0u);
    CHECK(h.dp.control_fsm().state() == ControlFsm::State::Idle);
    CHECK(h.dp.control_fsm().discard() == 3);
    h.cfg.core_enable = true;
    const size_t from = h.axis.lines().size();
    const auto l = h.frame(64, 3, true, 2, 4);
    CHECK(h.settle());
    CHECK(h.lines_from(first_sof_after(h, from)) == l);
    CHECK_EQ(h.done, 1u);
    // Payload before the first Frame Start after reset is discarded (ST_SKIP).
    Harness g;
    g.burst(bfm::long_packet(0, bfm::DT_RAW10, pack(px_line(64, true, 0), true)));
    CHECK(g.settle());
    CHECK(g.axis.lines().empty());
    CHECK_EQ(g.frame_sync, 1u);  // long packet with no frame open
}

unsigned sof_count(const Harness& h, size_t from = 0) {
    unsigned n = 0;
    for (size_t i = from; i < h.axis.lines().size(); ++i) n += h.axis.lines()[i].sof;
    return n;
}

// Review M3b F1 (C37): core_enable 1 -> 0 -> 1 between two sys_clk edges (zero
// time) or for a single cycle, with a capture open and a line in flight, with
// the ISP stalled or not, and while idle. The open capture is dropped once,
// its data never becomes the head of the next frame, the next frame is exact.
void ctl_core_pulse() {
    // variants: bit 0 ISP stalled during the stop, bit 1 one-cycle low instead
    // of zero time, 4/5: no stream sink attached (always ready), 6/7: stalled,
    // stop between two bursts (no line in flight, receive FIFO not full)
    for (unsigned variant = 0; variant < 8; ++variant) {
        const bool no_sink = variant == 4 || variant == 5, between = variant >= 6;
        const bool stalled = ((variant & 1) && !no_sink) || between, one_cycle = variant & 2;
        Harness h;
        if (no_sink) h.dp.set_axis_sink(nullptr);
        bool stall = false;
        h.axis.set_ready([&](uint64_t) { return !stall; });
        h.burst(bfm::short_packet(0, bfm::DT_FS, 1));
        const Bytes l0 = pack(px_line(64, true, 0), true);
        h.burst(bfm::long_packet(0, bfm::DT_RAW10, l0));
        if (between) {
            CHECK(h.settle());
        } else {
            h.burst(bfm::long_packet(0, bfm::DT_RAW10, pack(px_line(640, true, 1), true)));
            while (h.pending.size() > 100) h.step();  // the 640-pixel line is half received
        }
        stall = stalled;
        h.run(20);
        if (one_cycle) {
            h.cfg.core_enable = false;
            h.dp.core_disable();
            h.step();
            h.cfg.core_enable = true;
        } else {
            h.dp.core_disable();  // CSR 1 -> 0 and 0 -> 1 at the same time: the level never seen low
        }
        // The stop stays pending (CORE_STATUS.reset_in_progress) through the
        // drop sequence and the abort of the stream master; it completes only
        // with Control FSM in ST_IDLE and Finalize Sequencer back in PASSTHROUGH.
        bool flushed = false;
        for (unsigned k = 0; k < 50; ++k) {
            const bool was = h.dp.stop_pending();
            h.step();
            flushed |= h.dp.control_fsm().state() == ControlFsm::State::Flush;
            if (was && !h.dp.stop_pending())
                CHECK(h.dp.control_fsm().state() == ControlFsm::State::Idle &&
                      h.dp.axis_master().state() == AxisMaster::State::Passthrough);
        }
        CHECK(flushed || one_cycle);
        if (stalled) {
            // ISP still stalled: the abort cannot complete, the stop stays busy,
            // the held beat is stable (monitor) and no new capture is taken.
            CHECK(h.dp.stop_pending());
            CHECK(h.dp.axis_master().tvalid() && h.dp.axis_master().state() != AxisMaster::State::Passthrough);
            const size_t ids = h.capture_ids.size();
            h.frame(8, 1, true, 5, 3);
            h.run(3000);
            CHECK(h.dp.stop_pending());
            CHECK_EQ(h.capture_ids.size(), ids);
            CHECK_EQ(h.done, 0u);
            stall = false;
            for (unsigned k = 0; k < 1000 && h.dp.stop_pending(); ++k) h.step();
            CHECK(h.axis.abort_beats() >= 1);  // the abort beat was accepted before completion
        }
        CHECK(!h.dp.stop_pending());
        CHECK(h.dp.axis_master().state() == AxisMaster::State::Passthrough);
        stall = false;
        h.burst(bfm::short_packet(0, bfm::DT_FE, 1));  // FE of the interrupted frame
        CHECK(h.settle());
        CHECK_EQ(h.dropped, 1u);
        CHECK_EQ(h.done, 0u);
        if (!no_sink) CHECK(h.axis.abort_beats() >= 1);
        const size_t from = h.axis.lines().size();
        const auto l = h.frame(8, 1, true, 2, 7);
        CHECK(h.settle());
        if (!no_sink) {
            CHECK(h.lines_from(from) == l);
            CHECK_EQ(sof_count(h, from), 1u);
        }
        CHECK_EQ(h.done, 1u);
        CHECK_EQ(h.dropped, 1u);
        CHECK(h.axis.violations().empty());
        // Idle pulse: nothing to drop, no abort, completes at once, next frame exact.
        const uint64_t aborts = h.axis.abort_beats();
        h.dp.core_disable();
        h.run(3);
        CHECK(!h.dp.stop_pending());
        CHECK(h.settle());
        CHECK_EQ(h.dropped, 1u);
        CHECK_EQ(h.axis.abort_beats(), aborts);
        const size_t from2 = h.axis.lines().size();
        const auto l2 = h.frame(8, 2, true, 3, 9);
        CHECK(h.settle());
        if (!no_sink) CHECK(h.lines_from(from2) == l2);
        CHECK_EQ(h.done, 2u);
        CHECK(h.axis.violations().empty());
        // The frame offered while the stop was pending never became a capture.
        CHECK(h.capture_ids == (std::vector<uint16_t>{1, 2, 3}));
    }
}

// Review M3b F2 (C38): an image packet without payload does not consume the
// Start of Frame; SOF rides on the first pixel beat actually delivered, once
// per frame. RAW10 and RAW12, at frame start, after an embedded line, between
// lines, and in a frame made only of empty packets.
void pipe_zero_wc() {
    for (bool raw10 : {true, false}) {
        const uint8_t dt = raw10 ? bfm::DT_RAW10 : bfm::DT_RAW12;
        Harness h;
        h.cfg.pa.metadata_capture_en = h.cfg.fc.metadata_capture_en = true;
        const Bytes empty;
        std::vector<Bytes> expect;
        unsigned frames = 0;
        auto line = [&](unsigned seed) {
            expect.push_back(pack(px_line(32, raw10, seed), raw10));
            h.burst(bfm::long_packet(0, dt, expect.back()));
        };
        const auto pa_lines = [&] { return unsigned(h.dp.packet_analyzer().context(0).line_count); };
        // (1) empty packet at frame start: no line for the Frame Supervisor either
        h.burst(bfm::short_packet(0, bfm::DT_FS, 1));
        h.burst(bfm::long_packet(0, dt, empty));
        CHECK(h.settle());
        CHECK_EQ(pa_lines(), 0u);
        line(1);
        CHECK(h.settle());
        CHECK_EQ(pa_lines(), 1u);
        line(2);
        h.burst(bfm::long_packet(0, dt, empty));  // and at frame end
        CHECK(h.settle());
        CHECK_EQ(pa_lines(), 2u);
        h.burst(bfm::short_packet(0, bfm::DT_FE, 1));
        ++frames;
        CHECK(h.settle());
        CHECK_EQ(h.published[0].line_count, 2u);  // the empty packet is not a line
        // (2) after an embedded line
        h.burst(bfm::short_packet(0, bfm::DT_FS, 2));
        h.burst(bfm::long_packet(0, bfm::DT_EMBEDDED, Bytes(12, 0x5A)));
        h.burst(bfm::long_packet(0, dt, empty));
        line(3);
        h.burst(bfm::short_packet(0, bfm::DT_FE, 2));
        ++frames;
        // (3) between lines
        h.burst(bfm::short_packet(0, bfm::DT_FS, 3));
        line(4);
        h.burst(bfm::long_packet(0, dt, empty));
        line(5);
        CHECK(h.settle());
        CHECK_EQ(pa_lines(), 2u);
        h.burst(bfm::short_packet(0, bfm::DT_FE, 3));
        ++frames;
        // (4) only empty packets, then a clean frame
        h.burst(bfm::short_packet(0, bfm::DT_FS, 4));
        h.burst(bfm::long_packet(0, dt, empty));
        h.burst(bfm::long_packet(0, dt, empty));
        h.burst(bfm::short_packet(0, bfm::DT_FE, 4));
        h.burst(bfm::short_packet(0, bfm::DT_FS, 5));
        line(6);
        h.burst(bfm::short_packet(0, bfm::DT_FE, 5));
        ++frames;
        CHECK(h.settle());
        CHECK(h.lines_from(0) == expect);
        // Scoreboard of frame delimiters: one SOF per frame with pixels, on its first line.
        const auto& L = h.axis.lines();
        const std::vector<bool> first = {true, false, true, true, false, true};
        CHECK_EQ(L.size(), first.size());
        for (size_t i = 0; i < L.size() && i < first.size(); ++i) CHECK_EQ(L[i].sof, bool(first[i]));
        CHECK_EQ(sof_count(h), frames);
        CHECK_EQ(h.done, 5u);  // the empty-only frame is a capture too
        CHECK_EQ(h.dropped, 0u);
        CHECK(h.axis.violations().empty());
        // The line number Packet Analyzer reports with an error ignores empty
        // packets: a CRC error on the 2nd real line, after two empty packets, is line 2.
        Harness e;
        e.burst(bfm::short_packet(0, bfm::DT_FS, 1));
        e.burst(bfm::long_packet(0, dt, empty));
        e.burst(bfm::long_packet(0, dt, pack(px_line(32, raw10, 1), raw10)));
        e.burst(bfm::long_packet(0, dt, empty));
        Bytes bad = bfm::long_packet(0, dt, pack(px_line(32, raw10, 2), raw10));
        bad[bad.size() - 1] ^= 0x01;  // CRC footer
        e.burst(bad);
        CHECK(e.settle());
        CHECK_EQ(e.crc, 1u);
        CHECK_EQ(e.last_err_line, 2u);
        CHECK_EQ(unsigned(e.dp.packet_analyzer().context(0).line_count), 2u);
    }
}

// Block 10 alone: command sequencing per MAS Tables 654/655 and Figure 642.
void ctl_unit() {
    ControlFsm f;
    f.reset();
    CtlIn in;
    auto step = [&](const std::function<void(CtlIn&)>& mod = nullptr) {
        in = CtlIn{};
        in.core_enable = true;
        in.axis_enable = true;
        if (mod) mod(in);
        return f.clock(in);
    };
    CHECK(f.state() == ControlFsm::State::Idle && f.discard() == 3);
    step();
    CHECK(f.state() == ControlFsm::State::Skip && f.outputs(false).force_discard == 3);
    // A packet cause with no capture open: ST_SKIP again, no flush.
    step([](CtlIn& i) { i.capture_start[0] = true; });  // lead Frame Start opens a capture
    CHECK(f.state() == ControlFsm::State::Run);
    CHECK_EQ(f.discard(), 2u);  // lead channel (VC0) resumes, lag channel still discarded
    step([](CtlIn& i) { i.capture_end[0] = true; });
    step([](CtlIn& i) { i.err_ecc2 = true; });
    CHECK(f.state() == ControlFsm::State::Skip);
    CHECK_EQ(f.outputs(false).force_discard, 3u);
    CHECK(!f.outputs(false).lb_flush);
    // Cause with a capture open: discard set at once, capture_drop next cycle,
    // HOLD for 2 cycles (dma_abort_req in the first), FLUSH for one, then SKIP.
    step([](CtlIn& i) { i.capture_start[1] = true; });
    CHECK(f.state() == ControlFsm::State::Run && f.discard() == 2);
    step([](CtlIn& i) { i.err_crc = 1; });
    CHECK_EQ(f.outputs(false).force_discard, 3u);
    CtlOut o = f.outputs(false);
    CHECK(f.state() == ControlFsm::State::Hold && o.capture_drop && o.drop_vc_mask == 3 && o.dma_abort_req);
    step([](CtlIn& i) { i.err_crc = 1; });  // a cause during HOLD is ignored
    o = f.outputs(false);
    CHECK(f.state() == ControlFsm::State::Hold && !o.capture_drop && !o.dma_abort_req);
    step([](CtlIn& i) { i.capture_start[0] = true; });  // capture opened during HOLD: lead_seen
    o = f.outputs(false);
    CHECK(f.state() == ControlFsm::State::Flush && o.lb_flush && o.fb_flush && o.asm_abort_req && o.rx_ctx_clear);
    CHECK_EQ(o.asm_abort_mask, 1u);
    CHECK(!f.outputs(true).rx_ctx_clear);  // TPG selected: the sensor flags are not cleared
    step();
    CHECK(f.state() == ControlFsm::State::Skip);
    step();
    CHECK(f.state() == ControlFsm::State::Run);  // resumes on the capture opened during the drop
    CHECK_EQ(f.discard(), 2u);
    // The dropped capture is counted once at its end; two ending in the same cycle: two pulses.
    CHECK(step([](CtlIn& i) {
        i.capture_end[1] = true;
        i.capture_dropped[1] = true;
    }));
    CHECK(!step());
    CHECK(step([](CtlIn& i) {
        i.capture_end = {true, true};
        i.capture_dropped = {true, true};
    }));
    CHECK(step());
    CHECK(!step());
    // Sensor-path causes ignored while the TPG is selected; Format Bridge overflow is not.
    step([](CtlIn& i) {
        i.active_sel = true;
        i.capture_start[0] = true;
        i.err_ecc2 = i.err_lane_abort = i.lb_full = i.lb_overflow = true;
        i.err_crc = i.err_wc = 1;
    });
    CHECK(f.state() == ControlFsm::State::Run);
    step([](CtlIn& i) {
        i.active_sel = true;
        i.fb_overflow = true;
    });
    CHECK(f.state() == ControlFsm::State::Hold);
    for (unsigned k = 0; k < 4; ++k) step([](CtlIn& i) { i.active_sel = true; });
    CHECK(f.state() == ControlFsm::State::Skip);
    // err_crc on a channel already discarded is ignored; core_enable low with
    // no capture open: ST_IDLE directly, no capture_drop.
    step([](CtlIn& i) { i.capture_start[0] = true; });
    step([](CtlIn& i) { i.capture_end[0] = true; });
    step([](CtlIn& i) { i.err_crc = 2; });  // VC1 is discarded in the linear profile
    CHECK(f.state() == ControlFsm::State::Run);
    step([](CtlIn& i) { i.core_enable = false; });
    CHECK(f.state() == ControlFsm::State::Idle && f.discard() == 3);
    CHECK(!f.outputs(false).capture_drop && !f.outputs(false).lb_flush);
    // A truncated header (channel unknown, C31) is a packet-class cause.
    step();
    step([](CtlIn& i) { i.capture_start[1] = true; });
    CHECK(f.state() == ControlFsm::State::Run);
    step([](CtlIn& i) { i.err_wc_unknown = true; });
    CHECK(f.state() == ControlFsm::State::Hold);
    for (unsigned k = 0; k < 3; ++k) step();
    step([](CtlIn& i) { i.capture_end[1] = true; });
    step([](CtlIn& i) { i.core_enable = false; });
    CHECK(f.state() == ControlFsm::State::Idle);
    // axis_enable falling: asm_abort_req without a drop.
    step();
    step([](CtlIn& i) { i.capture_start[0] = true; });
    step([](CtlIn& i) { i.axis_enable = false; });
    o = f.outputs(false);
    CHECK(o.asm_abort_req && !o.capture_drop && f.state() == ControlFsm::State::Run);
}

// Block 6 alone: switch only at frame boundaries, mask until the incoming SOF.
void mux_unit() {
    TpgMux m;
    m.reset();
    PxBeat rx, tpg;
    rx.count = 4;
    tpg.count = 4;
    auto in_of = [&](bool rx_valid, bool tpg_valid, bool tpg_sof, bool en) {
        TpgMux::In i;
        rx.count = rx_valid ? 4 : 0;
        i.rx = &rx;
        i.tpg_valid = tpg_valid;
        tpg.sof = tpg_sof;
        i.tpg = tpg;
        i.tpg_enable = en;
        i.lb_empty = true;
        return i;
    };
    PxBeat out;
    // RX frame open: the switch waits for its Frame End and for the last beat.
    TpgMux::In i = in_of(false, false, false, false);
    i.fc_fs = true;
    m.clock(i);
    CHECK(m.rx_frame_active());
    i = in_of(true, true, true, true);
    CHECK(!m.tpg_ready(i));  // TPG held at its first beat while RX is selected
    CHECK(m.output(i, out) && out.count == 4);  // RX beat delivered
    m.clock(i);
    CHECK(m.state() == TpgMux::State::Draining);
    for (unsigned k = 0; k < 5; ++k) m.clock(in_of(true, true, true, true));
    CHECK(m.state() == TpgMux::State::Draining && !m.active_sel());
    i = in_of(true, true, true, true);
    i.fc_fe = true;
    m.clock(i);
    CHECK(m.state() == TpgMux::State::Draining);
    m.clock(in_of(true, true, true, true));
    CHECK(m.state() == TpgMux::State::Draining);  // frame closed, but its last beat is still presented
    m.clock(in_of(false, true, true, true));
    CHECK(m.state() == TpgMux::State::Acquiring && m.active_sel() && m.switch_pending());
    // Acquiring: a TPG beat without SOF is masked; the SOF beat is the first delivered.
    i = in_of(false, true, false, true);
    CHECK(!m.output(i, out));
    m.clock(i);
    i = in_of(false, true, true, true);
    CHECK(m.output(i, out) && out.sof);
    m.clock(i);
    CHECK(m.state() == TpgMux::State::Streaming && !m.switch_pending());
    // Back to RX: needs the TPG frame closed and Line Buffer empty.
    i = in_of(false, false, false, false);
    i.tpg_fs = true;
    m.clock(i);  // TPG frame in flight
    i = in_of(false, false, false, false);
    m.clock(i);
    CHECK(m.state() == TpgMux::State::Draining);
    i.tpg_eof = true;
    m.clock(i);
    i = in_of(false, false, false, false);
    i.lb_empty = false;
    m.clock(i);
    CHECK(m.state() == TpgMux::State::Draining);  // Line Buffer still holds TPG entries
    m.clock(in_of(false, false, false, false));
    CHECK(m.state() == TpgMux::State::Acquiring && !m.active_sel());
    // Re-toggle in ACQUIRING: back to DRAINING, still masked (armed survives).
    i = in_of(true, false, false, true);
    CHECK(!m.output(i, out));
    m.clock(i);
    CHECK(m.state() == TpgMux::State::Draining);
    CHECK(!m.output(in_of(true, false, false, true), out));
    // rx_ctx_clear releases a sensor frame that never closes.
    TpgMux n;
    n.reset();
    i = in_of(false, false, false, false);
    i.fc_fs = true;
    n.clock(i);
    i = in_of(false, false, false, false);
    i.rx_ctx_clear = true;
    n.clock(i);
    CHECK(!n.rx_frame_active());
}

// Cost of the sys_clk datapath alone for the M3b benchmark frame (the same
// 640x480 RAW10 frame as fx1_csi2_rxphy_bench_640x480, without the PHY).
void pipe_bench() {
    Harness h;
    h.cfg.fc.expected_width = 640;
    h.cfg.fc.expected_height = 480;
    const auto w0 = std::chrono::steady_clock::now();
    const auto lines = h.frame(640, 480, true);
    CHECK(h.settle(50000000));
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - w0).count();
    std::printf("  datapath 640x480 RAW10: %llu sys_clk cycles in %.3f s wall (%.1f Mcycles/s)\n",
                (unsigned long long)h.dp.cycles(), wall, h.dp.cycles() / wall / 1e6);
    CHECK(h.lines_from(0) == lines);
    CHECK_EQ(h.done, 1u);
    CHECK_EQ(h.res_err, 0u);
}

void pipe_quiescent() {
    // The quiescence predicate never hides a pending cycle: running N extra
    // cycles from a quiescent state changes no observable output.
    Harness h;
    h.frame(64, 3, true);
    CHECK(h.settle());
    const size_t lines = h.axis.lines().size();
    const unsigned done = h.done;
    h.run(10000);
    CHECK_EQ(h.axis.lines().size(), lines);
    CHECK_EQ(h.done, done);
    CHECK(h.dp.quiescent(h.cfg));
}

}  // namespace

int main(int argc, char** argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    const std::map<std::string, std::function<void()>> cases = {
        {"b2p_raw10", [] { b2p_exact(true); }},
        {"b2p_raw12", [] { b2p_exact(false); }},
        {"b2p_wc_align", b2p_wc_align},
        {"lb_fwft", lb_fwft},
        {"fb_formats", fb_formats},
        {"axis_skid", axis_skid},
        {"axis_abort", axis_abort},
        {"tpg_pattern", tpg_pattern},
        {"tpg_timing", tpg_timing},
        {"pipe_frame_raw10", [] { pipe_frame(true); }},
        {"pipe_frame_raw12", [] { pipe_frame(false); }},
        {"pipe_crc_drop", pipe_crc_drop},
        {"pipe_lb_full", pipe_lb_full},
        {"pipe_lb_overflow", pipe_lb_overflow},
        {"pipe_cdc", pipe_cdc},
        {"pipe_wc_align", pipe_wc_align},
        {"pipe_tpg", pipe_tpg},
        {"fc_checks", fc_checks},
        {"ctl_core_disable", ctl_core_disable},
        {"ctl_unit", ctl_unit},
        {"ctl_core_pulse", ctl_core_pulse},
        {"pipe_zero_wc", pipe_zero_wc},
        {"mux_unit", mux_unit},
        {"pipe_quiescent", pipe_quiescent},
        {"pipe_bench", pipe_bench},
    };
    const auto it = cases.find(which);
    if (it == cases.end()) {
        std::fprintf(stderr, "unknown case '%s'\n", which.c_str());
        return 2;
    }
    it->second();
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
