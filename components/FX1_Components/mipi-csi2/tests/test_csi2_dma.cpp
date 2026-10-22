// M4a unit tests of the DMA RAW path without SystemC: SysDatapath fed with
// merged words, its DMA engine served by a worker model writing into a memory
// image with sentinel bytes. Oracles: the BFM packer and the source pixels for
// the bytes, the layout rules of spec_decisions M01-M12 for the addresses.
//
//   test_fx1_csi2_dma <case>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "fx1_csi2/axis_monitor.h"
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
#define CHECK_EQ(a, b)                                                                                       \
    do {                                                                                                     \
        const auto va_ = (a);                                                                                \
        const auto vb_ = (b);                                                                                \
        if (!(va_ == vb_)) {                                                                                 \
            std::fprintf(stderr, "%s:%d: CHECK_EQ failed: %s = 0x%llx, expected 0x%llx\n", __FILE__, __LINE__, #a, \
                         (unsigned long long)va_, (unsigned long long)vb_);                                  \
            ++g_fail;                                                                                        \
        }                                                                                                    \
    } while (0)

constexpr uint64_t kDdr = 0x80000000ull;
constexpr size_t kDdrSize = 1u << 22;  // 4 MiB window of the memory model
constexpr uint8_t kSentinel = 0xA5;

Pixels px_line(unsigned width, bool raw10, unsigned seed) {
    Pixels p(width);
    for (unsigned x = 0; x < width; ++x) p[x] = static_cast<uint16_t>((x * 29 + seed * 113 + 7) & (raw10 ? 0x3FF : 0xFFF));
    return p;
}
Bytes pack(const Pixels& p, bool raw10) { return raw10 ? bfm::pack_raw10(p) : bfm::pack_raw12(p); }
Bytes raw16(const Pixels& p) {
    Bytes b;
    for (uint16_t v : p) {
        b.push_back(static_cast<uint8_t>(v));
        b.push_back(static_cast<uint8_t>(v >> 8));
    }
    return b;
}

struct Harness : SysEvents, PaEvents, DmaEvents {
    SysDatapath dp;
    SysConfig cfg;
    uint64_t period_ps = 4000, word_ps = 10000, now = 0, next_word = 0;
    std::deque<std::pair<uint64_t, MergedWord>> pending;
    // memory and worker
    Bytes mem = Bytes(kDdrSize, kSentinel);
    unsigned mem_latency = 10;  // sys_clk cycles per burst after its beats
    bool worker_busy = false;
    uint64_t worker_done_at = 0;
    DmaBurst cur;
    std::vector<DmaBurst> log;  // issued bursts
    std::function<uint8_t(const DmaBurst&)> respond;  // BRESP override (M4b)
    // register side
    BufState buf[2] = {BufState::HwOwned, BufState::HwOwned};
    uint16_t frame_id[2] = {0, 0}, last_valid[2] = {0, 0};
    unsigned active = 0;
    bool busy = false;
    std::vector<std::string> trace;  // ordered register / event log
    unsigned done_ok = 0, done_err = 0, ovf = 0, axi_err = 0, delivered = 0, dropped = 0, crc = 0;

    Harness() : dp(SysParams{}) {
        dp.set_sys_events(this);
        dp.set_pa_events(this);
        dp.dma().set_events(this);
        cfg.core_enable = true;
        cfg.fb.axis_enable = false;  // DDR-only milestone (D02)
        cfg.fb.dma_enable = true;
        cfg.dma.enable = cfg.dma.start = true;
        cfg.dma.base = {uint32_t(kDdr), uint32_t(kDdr + 0x100000)};
        cfg.dma.size = 0x80000;
        cfg.dma.max_burst_len = 255;
        dp.reset(true);
    }
    void set_layout(unsigned bytes_per_line, unsigned padding) {
        cfg.dma.bytes_per_line = static_cast<uint16_t>(bytes_per_line);
        cfg.dma.line_padding = static_cast<uint8_t>(padding);
    }
    // SysEvents
    void sys_line_buffer_full() override {}
    void sys_line_buffer_overflow() override {}
    void sys_cdc_overflow() override {}
    void sys_frame_dropped() override {
        ++dropped;
        trace.push_back("dropped");
    }
    void sys_capture_start(uint16_t, uint64_t) override {}
    void sys_capture_done() override {
        ++delivered;
        trace.push_back("delivered");
    }
    void sys_published(unsigned, const FrameInfo&) override {}
    void sys_resolution_mismatch(unsigned) override {}
    void sys_err_wc_align() override {}
    void sys_fmtbridge_overflow() override {}
    void sys_frame_num_err(unsigned) override {}
    // PaEvents
    void pa_beat(const PayloadBeat&) override {}
    void pa_error(PaError e, int, unsigned) override { crc += e == PaError::Crc; }
    void pa_packet_done() override {}
    void pa_frame_start(unsigned, uint16_t) override {}
    void pa_frame_end(unsigned, uint16_t) override {}
    void pa_gsp(uint8_t, uint8_t, uint16_t) override {}
    void pa_null_blank() override {}
    void pa_vc_unsupported() override {}
    // DmaEvents
    BufState dma_buf_state(unsigned s) override { return buf[s & 1]; }
    void dma_set_buffer(unsigned s, BufState st, uint16_t fid) override {
        buf[s & 1] = st;
        if (fid) frame_id[s & 1] = fid;
        trace.push_back("buf" + std::to_string(s) + "=" + std::to_string(int(st)));
    }
    void dma_set_last_valid_line(unsigned s, uint16_t l) override {
        last_valid[s & 1] = l;
        trace.push_back("lvl" + std::to_string(s));
    }
    void dma_set_active_buffer(unsigned s) override { active = s; }
    void dma_busy(bool b) override { busy = b; }
    bool dma_claim_blocked() override { return false; }
    unsigned done_with_write_in_flight = 0;
    void dma_buf_done(bool ok, uint32_t) override {
        (ok ? done_ok : done_err)++;
        if (worker_busy && cur.side == unsigned(active)) ++done_with_write_in_flight;  // M12: responses first
        trace.push_back(ok ? "done_ok" : "done_err");
    }
    void dma_overflow(uint32_t) override { ++ovf; }
    void dma_axi_error(uint8_t, uint8_t, uint32_t, uint32_t) override { ++axi_err; }

    void release(unsigned s) {
        if (buf[s] != BufState::HwOwned) buf[s] = BufState::HwOwned;
    }
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
    // Worker model: one burst at a time, done `beats + mem_latency` cycles
    // after it is taken; writes the memory image on an OKAY response.
    void worker_step(uint64_t cycle) {
        if (worker_busy && cycle >= worker_done_at) {
            worker_busy = false;
            const bool in_window = cur.addr >= kDdr && cur.addr + cur.data.size() <= kDdr + kDdrSize;
            const uint8_t resp = respond ? respond(cur) : (in_window ? 0 : 3);  // DECERR outside the model
            if (resp == 0 && in_window) std::copy(cur.data.begin(), cur.data.end(), mem.begin() + (cur.addr - kDdr));
            dp.dma().burst_done(cur, resp == 0, resp);
            dp.resolve_ledger();
        }
        if (!worker_busy && dp.dma().take_burst(cur)) {
            worker_busy = true;
            worker_done_at = cycle + cur.beats + mem_latency;
            log.push_back(cur);
        }
    }
    uint64_t cycles = 0;
    void step() {
        now += period_ps;
        ++cycles;
        while (!pending.empty() && pending.front().first < now) {
            dp.cdc_push(pending.front().second, pending.front().first);
            pending.pop_front();
        }
        dp.cycle(cfg, now, now / 1000);
        worker_step(cycles);
    }
    void run_idle(uint64_t n) {
        for (uint64_t i = 0; i < n; ++i) step();
    }
    bool settle(uint64_t max = 5000000) {
        for (uint64_t i = 0; i < max; ++i) {
            if (pending.empty() && dp.quiescent(cfg) && !worker_busy && !dp.dma().burst_ready() && !dp.dma().busy())
                return true;
            step();
        }
        return false;
    }
    std::vector<Pixels> frame(unsigned w, unsigned h, bool raw10, uint16_t fnum = 1, unsigned seed = 0,
                              std::function<void(unsigned, Bytes&)> mod = nullptr) {
        std::vector<Pixels> px;
        burst(bfm::short_packet(0, bfm::DT_FS, fnum));
        for (unsigned y = 0; y < h; ++y) {
            px.push_back(px_line(w, raw10, seed + y));
            Bytes pkt = bfm::long_packet(0, raw10 ? bfm::DT_RAW10 : bfm::DT_RAW12, pack(px.back(), raw10));
            if (mod) mod(y, pkt);
            burst(pkt);
        }
        burst(bfm::short_packet(0, bfm::DT_FE, fnum));
        return px;
    }
    Bytes read(uint64_t addr, size_t n) const { return Bytes(mem.begin() + (addr - kDdr), mem.begin() + (addr - kDdr) + n); }
    bool untouched(uint64_t addr, size_t n) const {
        for (size_t i = 0; i < n; ++i)
            if (mem[addr - kDdr + i] != kSentinel) return false;
        return true;
    }
    // The whole side: each line exact at base + n * stride, padding and the
    // rest of the side untouched.
    bool side_holds(unsigned side, const std::vector<Bytes>& lines, unsigned stride) const {
        const uint64_t base = cfg.dma.base[side];
        uint64_t end = base;
        bool ok = true;
        for (size_t n = 0; n < lines.size(); ++n) {
            const uint64_t a = base + n * stride;
            ok &= read(a, lines[n].size()) == lines[n];
            ok &= untouched(a + lines[n].size(), stride - lines[n].size());
            end = a + stride;
        }
        ok &= untouched(end, base + cfg.dma.size - end);
        return ok;
    }
};

std::vector<Bytes> packed_lines(const std::vector<Pixels>& px, bool raw10) {
    std::vector<Bytes> v;
    for (const auto& p : px) v.push_back(pack(p, raw10));
    return v;
}

// ---------------------------------------------------------------- cases

void dma_frame(bool raw10, bool to_raw16) {
    Harness h;
    const unsigned w = 100;  // RAW10: 125 bytes, RAW12: 150 bytes, RAW16: 200 bytes per line
    h.cfg.fb.dma_raw16 = to_raw16;
    const unsigned bpl = to_raw16 ? 2 * w : raw10 ? w * 5 / 4 : w * 3 / 2;
    const unsigned stride = (bpl + 7 + 24) & ~7u;  // padding to a multiple of 8, plus 24 bytes
    h.set_layout(bpl, stride - bpl);
    const auto px = h.frame(w, 6, raw10);
    CHECK(h.settle());  // the sensor stops after FE: completion without a next FS
    std::vector<Bytes> lines;
    for (const auto& p : px) lines.push_back(to_raw16 ? raw16(p) : pack(p, raw10));
    CHECK(h.side_holds(0, lines, stride));
    CHECK(h.untouched(h.cfg.dma.base[1], 0x1000));
    CHECK(h.buf[0] == BufState::SwOwnedValid && h.buf[1] == BufState::HwOwned);
    CHECK_EQ(h.frame_id[0], 1u);
    CHECK_EQ(h.done_ok, 1u);
    CHECK_EQ(h.delivered, 1u);
    CHECK_EQ(h.dropped, 0u);
    CHECK(!h.busy);
    // M12: ownership before the event, frame_received after buf_done.
    const auto it = std::find(h.trace.begin(), h.trace.end(), "buf0=1");
    CHECK(it != h.trace.end() && it + 1 != h.trace.end() && *(it + 1) == "done_ok" && *(it + 2) == "delivered");
    CHECK_EQ(h.done_with_write_in_flight, 0u);
    // Bursts: INCR within one line, never across 4 KiB, 8-byte aligned start.
    for (const auto& b : h.log) {
        const uint64_t off = b.addr - h.cfg.dma.base[0];
        CHECK_EQ(off / stride, (off + b.data.size() - 1) / stride);
        CHECK((off % stride) + b.data.size() <= bpl);
        CHECK_EQ(b.addr % 8, 0u);
        CHECK((b.addr & ~0xFFFull) == ((b.addr + b.data.size() - 1) & ~0xFFFull));
        CHECK(b.data.size() <= 8u * b.beats && b.data.size() > 8u * (b.beats - 1));
        CHECK_EQ(b.side, 0u);
    }
}

void dma_bursts() {
    // max_burst_len = beats - 1 (D03): 0 -> single beats, 1 -> 2 beats, 255 -> one burst per line;
    // a base 16 bytes below a 4 KiB boundary splits the first line there.
    for (unsigned mbl : {0u, 1u, 255u}) {
        Harness h;
        h.mem_latency = 2;  // single-beat bursts need a fast target to keep up with 4 lanes
        h.cfg.dma.max_burst_len = static_cast<uint8_t>(mbl);
        h.cfg.dma.base[0] = uint32_t(kDdr + 0x1000 - 16);
        h.set_layout(800, 0);  // 640 px RAW10
        const auto px = h.frame(640, 2, true);
        CHECK(h.settle());
        CHECK(h.side_holds(0, packed_lines(px, true), 800));
        unsigned max_beats = 0;
        for (const auto& b : h.log) {
            max_beats = std::max(max_beats, b.beats);
            CHECK(b.beats <= mbl + 1);
            CHECK((b.addr & ~0xFFFull) == ((b.addr + b.data.size() - 1) & ~0xFFFull));
        }
        CHECK_EQ(max_beats, mbl == 255 ? 100u : mbl + 1);  // 800 bytes = 100 beats per line
        CHECK_EQ(h.log.front().beats, std::min(mbl + 1, 2u));
        CHECK_EQ(h.log.front().addr, kDdr + 0x1000 - 16);
        if (mbl == 255) CHECK_EQ(h.log.size(), 3u);  // 2 + 98 beats on line 0, 100 on line 1
    }
}

void dma_pingpong() {
    // M11: alternate sides; both SW-owned -> the capture is dropped and nothing
    // is overwritten; a released side is used again.
    Harness h;
    h.set_layout(40, 0);
    std::vector<std::vector<Bytes>> f;
    for (unsigned i = 0; i < 3; ++i) {
        f.push_back(packed_lines(h.frame(32, 4, true, uint16_t(i + 1), 10 * i), true));
        CHECK(h.settle());
    }
    CHECK(h.side_holds(0, f[0], 40));
    CHECK(h.side_holds(1, f[1], 40));
    CHECK(h.buf[0] == BufState::SwOwnedValid && h.buf[1] == BufState::SwOwnedValid);
    CHECK_EQ(h.frame_id[0], 1u);
    CHECK_EQ(h.frame_id[1], 2u);
    CHECK_EQ(h.delivered, 2u);
    CHECK_EQ(h.dropped, 1u);  // third capture: no side
    CHECK_EQ(h.done_ok, 2u);
    h.release(0);
    f.push_back(packed_lines(h.frame(32, 4, true, 4, 30), true));
    CHECK(h.settle());
    CHECK(h.side_holds(0, f[3], 40));
    CHECK(h.side_holds(1, f[1], 40));  // side 1 untouched while software owns it
    CHECK_EQ(h.frame_id[0], 4u);
    CHECK_EQ(h.delivered, 3u);
    // Release both: the next claim takes the side after the last completed (0) -> 1.
    h.release(0);
    h.release(1);
    f.push_back(packed_lines(h.frame(32, 4, true, 5, 40), true));
    CHECK(h.settle());
    CHECK(h.side_holds(1, f[4], 40));
    CHECK_EQ(h.active, 1u);
}

void dma_layout() {
    // M03: a line longer than bytes_per_line -> layout fault; M07 policy 1
    // releases with error, policy 0 rewinds. A shorter line is not a DMA error.
    for (bool release : {true, false}) {
        Harness h;
        h.cfg.dma.err_policy_release = release;
        h.set_layout(40, 0);  // 32 px RAW10 = 40 bytes
        const auto good = packed_lines(h.frame(32, 2, true), true);
        CHECK(h.settle());
        h.release(0);
        h.frame(48, 3, true, 2, 5);  // 60-byte lines: the first line is already too long
        CHECK(h.settle());
        CHECK_EQ(h.ovf, 1u);
        if (release) {
            CHECK(h.buf[1] == BufState::SwOwnedError);
            CHECK_EQ(h.last_valid[1], 0xFFFFu);  // no line completed
            CHECK_EQ(h.done_err, 1u);
            CHECK_EQ(h.delivered, 2u);  // policy 1: delivered with error (D06)
        } else {
            CHECK(h.buf[1] == BufState::HwOwned);
            CHECK_EQ(h.done_err, 0u);
            CHECK_EQ(h.dropped, 1u);
        }
        // Nothing of the aborted capture is written: its data was not issued yet (M16).
        CHECK(h.untouched(h.cfg.dma.base[1], 0x1000));
        // Shorter lines: written, rest of the line untouched, no error.
        Harness s;
        s.set_layout(48, 0);
        const auto px = packed_lines(s.frame(32, 3, true), true);
        CHECK(s.settle());
        CHECK(s.side_holds(0, px, 48));
        CHECK_EQ(s.ovf, 0u);
        CHECK(s.buf[0] == BufState::SwOwnedValid);
        (void)good;
    }
}

void dma_bounds() {
    // M05: a burst that would end beyond DMA_BUFFER_SIZE is not written, the
    // capture ends as a fault; last_valid_line = lines completed before it.
    Harness h;
    h.set_layout(40, 8);  // stride 48
    h.cfg.dma.size = 48 * 2 + 40;  // line 2 ends exactly at the end of the buffer, line 3 does not fit
    const auto px = packed_lines(h.frame(32, 6, true), true);
    CHECK(h.settle());
    CHECK_EQ(h.ovf, 1u);
    CHECK(h.buf[0] == BufState::SwOwnedError);
    CHECK_EQ(h.last_valid[0], 2u);
    for (unsigned n = 0; n < 3; ++n) CHECK(h.read(h.cfg.dma.base[0] + 48 * n, 40) == px[n]);
    CHECK(h.untouched(h.cfg.dma.base[0] + 48 * 3, 0x1000));
    CHECK_EQ(h.delivered, 1u);  // released with error (policy 1, reset value)
    // DMA_BUFFER_SIZE = 0 faults on the first line; address wrap above 4 GiB faults too.
    Harness z;
    z.set_layout(40, 0);
    z.cfg.dma.size = 0;
    z.frame(32, 2, true);
    CHECK(z.settle());
    CHECK(z.buf[0] == BufState::SwOwnedError && z.last_valid[0] == 0xFFFF && z.log.empty());
    Harness w;
    w.set_layout(40, 0);
    w.cfg.dma.base[0] = 0xFFFFFFC0u;  // 64 bytes below 4 GiB, size large
    w.cfg.dma.size = 0x10000;
    w.respond = [](const DmaBurst&) { return uint8_t(0); };  // a target that accepts the top of the space
    w.frame(32, 4, true);
    CHECK(w.settle());
    CHECK(w.buf[0] == BufState::SwOwnedError);
    CHECK_EQ(w.last_valid[0], 0u);  // line 0 (40 bytes) fits, line 1 would wrap
    for (const auto& b : w.log) CHECK(b.addr + b.data.size() <= 0x100000000ull);
}

void dma_claim_refused() {
    // M04 misaligned base / stride, M10 dma_start = 0 or dma_abort = 1: the
    // capture is dropped, nothing written; dma_enable = 0: the DMA does not take part.
    struct V {
        const char* what;
        std::function<void(Harness&)> set;
    };
    const V vs[] = {
        {"base", [](Harness& h) { h.cfg.dma.base[0] += 4; }},
        {"stride", [](Harness& h) { h.set_layout(40, 4); }},
        {"start", [](Harness& h) { h.cfg.dma.start = false; }},
        {"abort", [](Harness& h) { h.cfg.dma.abort = true; }},
    };
    for (const auto& v : vs) {
        Harness h;
        h.set_layout(40, 0);
        v.set(h);
        h.frame(32, 2, true);
        CHECK(h.settle());
        CHECK(h.log.empty());
        CHECK(h.untouched(kDdr, 0x200000));
        CHECK_EQ(h.dropped, 1u);
        CHECK_EQ(h.delivered, 0u);
        CHECK(h.buf[0] == BufState::HwOwned && h.frame_id[0] == 0);
        if (g_fail) std::fprintf(stderr, "  variant %s\n", v.what);
    }
    Harness off;
    off.cfg.dma.enable = false;
    off.cfg.fb.dma_enable = false;
    off.frame(32, 2, true);
    CHECK(off.settle());
    CHECK(off.log.empty());
    CHECK_EQ(off.delivered, 1u);  // D06, dma_enable = 0
}

void dma_cfg_latched() {
    // M09: base, pitch and size latched at the claim; a change during a capture
    // takes effect at the next claim.
    Harness h;
    h.set_layout(40, 0);
    h.frame(32, 6, true);
    for (unsigned i = 0; i < 150; ++i) h.step();
    CHECK(h.busy && h.buf[0] == BufState::HwOwned);  // the capture is being written
    h.cfg.dma.base[1] = uint32_t(kDdr + 0x200000);
    h.set_layout(40, 24);
    h.cfg.dma.size = 64;
    CHECK(h.settle());
    CHECK(h.buf[0] == BufState::SwOwnedValid);
    CHECK_EQ(h.ovf, 0u);
    const auto px = packed_lines(h.frame(32, 2, true, 2, 9), true);
    CHECK(h.settle());
    CHECK(h.read(kDdr + 0x200000, 40) == px[0]);
    CHECK(h.read(kDdr + 0x200000 + 64, 40) != px[1]);  // line 1 beyond size 64: fault
    CHECK(h.buf[1] == BufState::SwOwnedError);
}

void dma_overflow() {
    // A slow memory: the RAW data FIFO fills. Sensor path: beats lost -> the
    // capture is aborted (IRQ 13), the D-PHY side never stalls (M20).
    Harness h;
    h.mem_latency = 2000;
    h.cfg.dma.max_burst_len = 63;  // 8 bursts of 64 beats fill the 512-beat data FIFO before the command FIFO
    h.set_layout(800, 0);
    h.frame(640, 8, true);
    CHECK(h.settle());
    CHECK_EQ(h.ovf, 1u);
    CHECK(h.buf[0] == BufState::SwOwnedError);
    CHECK_EQ(h.dp.dma().max_fifo_beats(), 512u);
    CHECK_EQ(h.delivered, 1u);
    // Single-beat bursts (max_burst_len reset value 0) against a target slower
    // than the beat rate: the command FIFO (16) fills first, same fault.
    Harness c;
    c.mem_latency = 10;
    c.cfg.dma.max_burst_len = 0;
    c.set_layout(800, 0);
    c.frame(640, 2, true);
    CHECK(c.settle());
    CHECK_EQ(c.ovf, 1u);
    CHECK(c.buf[0] == BufState::SwOwnedError);
    // TPG path: the generator is held instead, the frame lands whole.
    Harness t;
    t.mem_latency = 400;
    t.cfg.dma.max_burst_len = 15;  // command FIFO is the limit here
    t.cfg.tpg.enable = true;
    t.cfg.tpg.width = 640;
    t.cfg.tpg.height = 8;
    t.cfg.tpg.raw10 = true;
    t.set_layout(800, 0);
    for (unsigned i = 0; i < 400000 && t.done_ok == 0; ++i) t.step();
    t.cfg.tpg.enable = false;
    CHECK(t.settle());
    CHECK_EQ(t.ovf, 0u);
    CHECK(t.done_ok >= 1u);
    CHECK(t.buf[0] == BufState::SwOwnedValid);
}

void dma_back_to_back() {
    // FS of frame N+1 decoded while the tail of frame N is still in Line Buffer:
    // every beat goes to the buffer of its own capture (M18).
    Harness h;
    h.word_ps = 4000;  // one word per sys_clk cycle
    h.mem_latency = 0;
    h.set_layout(800, 0);
    std::vector<std::vector<Bytes>> f;
    for (unsigned i = 0; i < 2; ++i) {
        h.burst(bfm::short_packet(0, bfm::DT_FS, uint16_t(i + 1)), 0);
        std::vector<Pixels> px;
        for (unsigned y = 0; y < 4; ++y) {
            px.push_back(px_line(640, true, 50 * i + y));
            h.burst(bfm::long_packet(0, bfm::DT_RAW10, pack(px.back(), true)), 0);
        }
        h.burst(bfm::short_packet(0, bfm::DT_FE, uint16_t(i + 1)), 0);
        f.push_back(packed_lines(px, true));
    }
    CHECK(h.settle());
    CHECK(h.side_holds(0, f[0], 800));
    CHECK(h.side_holds(1, f[1], 800));
    CHECK_EQ(h.delivered, 2u);
    CHECK_EQ(h.done_ok, 2u);
}

void dma_ctl_drop() {
    // Control FSM drops a capture (CRC error) with a buffer claimed: counted
    // dropped once (M19); the buffer per err_frame_policy, no frame_received.
    for (bool release : {true, false}) {
        Harness h;
        h.cfg.dma.err_policy_release = release;
        h.set_layout(40, 0);
        h.frame(32, 6, true, 1, 0, [](unsigned y, Bytes& p) {
            if (y == 2) p[4 + 3] ^= 0x01;
        });
        CHECK(h.settle());
        CHECK_EQ(h.crc, 1u);
        CHECK_EQ(h.dropped, 1u);
        CHECK_EQ(h.delivered, 0u);
        if (release) {
            CHECK(h.buf[0] == BufState::SwOwnedError);
            CHECK_EQ(h.done_err, 1u);
        } else {
            CHECK(h.buf[0] == BufState::HwOwned);
            CHECK_EQ(h.done_err, 0u);
        }
        CHECK_EQ(h.done_ok, 0u);
        // The next frame lands whole on the next free side.
        if (release) h.release(0);
        const auto px = packed_lines(h.frame(32, 4, true, 2, 7), true);
        CHECK(h.settle());
        const unsigned side = h.active;
        CHECK(h.side_holds(side, px, 40));
        CHECK_EQ(h.delivered, 1u);
        CHECK_EQ(h.dropped + h.delivered, 2u);  // D06: ended = FRAME_COUNT + FRAME_DROP_COUNT
    }
}

// Review M4a F1: the ISP stalls with the stream and DMA paths both on; two
// frames reach their Frame End while the first one's data is still queued in
// Line Buffer. No capture completes before its data; both land whole.
// Variant: the second capture is refused (its side misaligned), so the third
// one is too while the first still holds side 0.
void dma_stream_backlog() {
    for (bool refused : {false, true}) {
        Harness h;
        AxisMonitor axis;
        bool stall = true;
        axis.set_ready([&](uint64_t) { return !stall; });
        h.dp.set_axis_sink(&axis);
        h.cfg.fb.axis_enable = true;
        h.cfg.fb.stream_raw16 = false;
        h.set_layout(160, 0);  // 128 px RAW10
        if (refused) h.cfg.dma.base[1] += 4;
        std::vector<std::vector<Bytes>> f;
        const unsigned frames = refused ? 3 : 2;
        for (unsigned i = 0; i < frames; ++i) f.push_back(packed_lines(h.frame(128, 4, true, uint16_t(i + 1), 20 * i), true));
        for (unsigned i = 0; i < 20000 && !h.pending.empty(); ++i) h.step();
        h.run_idle(2000);
        // Every Frame End decoded, data still queued: nothing complete, nothing written for frame 2.
        CHECK(h.pending.empty());
        CHECK_EQ(h.done_ok, 0u);
        CHECK(h.buf[0] == BufState::HwOwned && h.buf[1] == BufState::HwOwned);
        CHECK_EQ(h.delivered, 0u);
        CHECK(h.dp.line_buffer().fill() > 0);
        stall = false;
        CHECK(h.settle());
        CHECK(h.side_holds(0, f[0], 160));
        CHECK(axis.violations().empty());
        if (!refused) {
            CHECK(h.side_holds(1, f[1], 160));
            CHECK(h.buf[0] == BufState::SwOwnedValid && h.buf[1] == BufState::SwOwnedValid);
            CHECK_EQ(h.done_ok, 2u);
            CHECK_EQ(h.delivered, 2u);
            CHECK_EQ(h.dropped, 0u);
        } else {
            CHECK(h.untouched(h.cfg.dma.base[1] - 4, 0x1000));
            CHECK_EQ(h.done_ok, 1u);
            CHECK_EQ(h.delivered, 1u);
            CHECK_EQ(h.dropped, 2u);
        }
        // Stream output: all frames whole.
        CHECK_EQ(axis.lines().size(), 4u * frames);
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    const std::map<std::string, std::function<void()>> cases = {
        {"frame_raw10", [] { dma_frame(true, false); }},
        {"frame_raw12", [] { dma_frame(false, false); }},
        {"frame_raw10_to16", [] { dma_frame(true, true); }},
        {"frame_raw12_to16", [] { dma_frame(false, true); }},
        {"bursts", dma_bursts},
        {"pingpong", dma_pingpong},
        {"layout", dma_layout},
        {"bounds", dma_bounds},
        {"claim_refused", dma_claim_refused},
        {"cfg_latched", dma_cfg_latched},
        {"overflow", dma_overflow},
        {"back_to_back", dma_back_to_back},
        {"ctl_drop", dma_ctl_drop},
        {"stream_backlog", dma_stream_backlog},
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
