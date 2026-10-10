// vp_tests.cpp — VP self-check suite mapped to the HAS verification cases.
//
// Spec: §15.3 Verification cases and pass criteria, and the Bảng 6-1
// completion matrix. Each test names the HAS-V-* case it corresponds to.
//
// This suite drives the transaction-level model directly (no SystemC kernel
// needed). This directory contains a functional C++ VP, not a SystemC/RTL TB.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cmb_dma.h"
#include "ddr_model.h"
#include "df_dma.h"
#include "dma_subsystem.h"
#include "nal_dma.h"
#include "sw_dma.h"

using namespace h264;

#ifdef H264_WITH_REVIEW
int run_spec_review_tests();
#endif

static int g_fail = 0;
static int g_pass = 0;

static void check(bool cond, const std::string& what) {
    if (cond) {
        ++g_pass;
        std::printf("    [ok]   %s\n", what.c_str());
    } else {
        ++g_fail;
        std::printf("    [FAIL] %s\n", what.c_str());
    }
}

static void section(const char* s) { std::printf("\n== %s ==\n", s); }

static void drain(H264Arb& arb, DdrModel& mem) {
    AxiMasterBridge bridge;
    while (arb.busy()) {
        arb.acquire(arb.pick());
        const auto r = arb.granted_request();
        if (r.is_write) arb.complete(bridge.write_payload(r, mem, r.data));
        else arb.complete(true, bridge.read(r, mem));
    }
}

// ---------------------------------------------------------------------
// HAS-V-CMB-01 — frame start held high + repeated activation must not
// produce a stale duplicate source frame (§6.1 regression-critical guard).
// ---------------------------------------------------------------------
static void test_cmb_arm_guard() {
    section("HAS-V-CMB-01  CMB frame-start arm guard (§6.1)");
    CodedDims d{176, 144};
    DdrModel  mem(1u << 20);
    H264Arb   arb;
    CmbDma    cmb(mem, arb);
    cmb.configure(d, 0x1000, CmbFrameMode::CodingOrderPicture);

    // Enable rises: one frame start is accepted.
    cmb.update_enable(true);
    check(cmb.frame_start(), "first rising enable accepts a frame start");
    check(!cmb.frame_start(), "the same arm cannot accept a second start");

    // Enable stays HIGH across another frame-start opportunity. A stale high
    // level must NOT re-arm.
    cmb.update_enable(true);
    check(!cmb.frame_start(), "stale high enable does not re-read the old frame");

    // Enable falls, then rises again: rearmed.
    cmb.update_enable(false);
    cmb.update_enable(true);
    check(cmb.frame_start(), "rearms only after enable goes low then high");
}

// ---------------------------------------------------------------------
// §6.1 planar address expansion: Y/U/V offsets and chroma half-grid.
// ---------------------------------------------------------------------
static void test_cmb_planar_addresses() {
    section("§6.1 / §12.3  CMB planar address expansion");
    CodedDims d{176, 144};

    check(y_plane_offset(d) == 0u, "Y plane offset = 0");
    check(u_plane_offset(d) == 176u * 144u, "U plane offset = Wc*Hc");
    check(v_plane_offset(d) == (5u * 176u * 144u) / 4u, "V plane offset = 5*Wc*Hc/4");
    check(frame_bytes(d) == 38016u, "QCIF F = 38,016 bytes (§12.4)");

    // Y(x,y) = base + y*Wc + x
    check(planar_y_addr(0, d, 3, 2) == 2u * 176u + 3u, "Y(x,y) = base + y*Wc + x");
    // U(x,y) = base + Wc*Hc + y*(Wc/2) + x, chroma grid coordinates
    check(planar_u_addr(0, d, 3, 2) == 176u * 144u + 2u * 88u + 3u,
          "U(x,y) = base + Wc*Hc + y*(Wc/2) + x");
    check(uv_stride(d) == 88u, "chroma stride = Wc/2");

    // 1080p coded storage is 1920x1088, NOT 1080 (§12.3 caution).
    CodedDims hd{1920, 1088};
    check(frame_bytes(hd) == 3133440u,
          "cropped 1080p uses Hc=1088 -> F = 3,133,440 (§12.4)");
    check(frame_bytes(hd) != (3u * 1920u * 1080u) / 2u,
          "Hc=1080 would give the wrong DDR size");
}

// ---------------------------------------------------------------------
// §12.3 REFM slot rotation: slot k = REG_REFM + k*F.
// ---------------------------------------------------------------------
static void test_refm_slots() {
    section("§12.3  REFM slot addressing");
    CodedDims d{176, 144};
    const uint64_t reg = 0x400000ull;
    check(refm_slot_base(reg, d, 0) == reg, "slot 0 = REG_REFM");
    check(refm_slot_base(reg, d, 1) == reg + 38016ull, "slot 1 = REG_REFM + F");
    check(refm_slot_base(reg, d, 2) == reg + 2ull * 38016ull, "slot 2 = REG_REFM + 2F");
    check(kRefmSlots == 3, "minimum 3 REFM slots (§12.2)");
}

// ---------------------------------------------------------------------
// HAS-V-SW-01 — B start with List 1 not resident, MB column 0. No early
// FME/MC on a stale cache, and List 0 ready is never a substitute.
// ---------------------------------------------------------------------
static void test_sw_list1_not_resident() {
    section("HAS-V-SW-01  SW List-1 residency gate (§6.2, §8.5)");
    H264Arb arb;
    DdrModel mem(16u << 20);
    CodedDims d{176, 144};
    SwDma sw(arb, d, SwWindow{64, 64});
    sw.set_refm_base(0x400000);

    sw.set_ref_slot(RefList::List0, 0);
    sw.set_ref_slot(RefList::List1, 1);

    // Fill List 0 only.
    sw.fill_window(RefList::List0, 0, 0);
    check(!sw.ready(RefList::List0), "List 0 not resident while reads are queued");
    drain(arb, mem);
    check(sw.ready(RefList::List0), "List 0 resident after fill");
    check(!sw.ready(RefList::List1), "List 1 still not resident");

    check(!sw.fme_mc_may_start(/*is_b_picture=*/true),
          "B-picture FME/MC blocked while List 1 not resident");
    check(sw.fme_mc_may_start(/*is_b_picture=*/false),
          "P-picture may start with List 0 only");

    // List 0 ready must NOT be used as List 1 ready.
    check(!sw.selected_list_resident(RefList::List1),
          "selected-list check is per-list, not inherited from List 0");

    // §6.2: when the selected list is not ready, hold state and do not
    // advance the phase counter.
    const uint32_t before = sw.phase();
    const bool advanced = sw.advance_phase(RefList::List1, 0x1234);
    check(!advanced, "phase does not advance while selected list not ready");
    check(sw.phase() == before, "phase counter held, not dropped-and-incremented");
    check(sw.hold_state().held, "hold state asserted");
    check(sw.hold_state().sw_addr == 0x1234ull, "SW address retained while held");

    // Now make List 1 resident: FME/MC may start and phase advances.
    sw.fill_window(RefList::List1, 0, 0);
    check(!sw.fme_mc_may_start(true), "B-picture blocked before List 1 completion");
    drain(arb, mem);
    check(sw.fme_mc_may_start(true), "B-picture may start once List 1 resident");
    check(sw.advance_phase(RefList::List1, 0x1234), "phase advances when ready");
    check(sw.phase() == before + 1, "phase counter incremented once");
}

// ---------------------------------------------------------------------
// §6.2 row-start: at MB column 0 two columns may be needed.
// ---------------------------------------------------------------------
static void test_sw_row_start() {
    section("§6.2  SW row-start (MB column 0)");
    H264Arb arb;
    DdrModel mem(16u << 20);
    CodedDims d{176, 144};
    SwDma sw(arb, d, SwWindow{64, 64});
    sw.set_refm_base(0x400000);
    sw.set_ref_slot(RefList::List0, 0);

    sw.fill_window(RefList::List0, 0, 0);
    check(!sw.ready(RefList::List0), "row-start waits for all required columns/rows");
    drain(arb, mem);
    check(sw.ready(RefList::List0), "row-start window valid after extra column");

    // Invalidating on picture/list change must drop residency.
    sw.invalidate(RefList::List0);
    check(!sw.ready(RefList::List0), "invalidate clears residency (retag on change)");
}

// ---------------------------------------------------------------------
// HAS-V-DF-01 — delayed done sticky across a new SOF; completion only after
// the final reference write is acknowledged (§6.3).
// ---------------------------------------------------------------------
static void test_df_sticky_done() {
    section("HAS-V-DF-01  DF delayed done + SOF handshake (§6.3)");
    DdrModel mem(1u << 20);
    H264Arb  arb;
    DfDma    df(mem, arb);
    df.set_refm_base(0x400000);
    df.set_dims(CodedDims{176, 144});

    df.on_sofm();
    // Delayed done arrives BEFORE the controller looks at it.
    df.dma_fmdone();
    check(df.done_available(), "delayed dma_fmdone is latched");

    // A NEW SOF must not erase the retained done.
    df.on_sofm();
    check(df.done_available(),
          "new SOF does not clear a delayed done (sticky behavior)");

    // The controller consumes it once.
    check(df.consume_done(), "retained done is consumable after the SOF");
    check(!df.done_available(), "consuming clears the retained flag");
    check(!df.consume_done(), "a consumed done is not delivered twice");
}

// ---------------------------------------------------------------------
// §6.3 filter order is vertical before horizontal, and bypass still writes.
// ---------------------------------------------------------------------
static void test_df_filter_order() {
    section("§6.3  DF filter order and bypass schedule");
    DdrModel mem(1u << 20);
    H264Arb  arb;
    DfDma    df(mem, arb);
    df.set_refm_base(0x400000);
    df.set_dims(CodedDims{176, 144});

    df.latch_dis_idc(1);   // filter enabled
    check(!df.bypass_mode(), "dis_idc=1 selects filter mode");
    df.schedule_macroblock(1, 0, MacroblockPixels{}); // producer supplies a completed filtered tile

    const auto& log = arb.log().entries();
    check(log.size() == 32, "one completed MB issues 16 Y + 8 U + 8 V row writes");
    check(df.pass_log()[0] == FilterPass::Vertical, "vertical pass is scheduled FIRST (§6.3)");
    check(df.pass_log()[1] == FilterPass::Horizontal, "horizontal pass is scheduled second");
    check(log[0].is_write && log[31].is_write, "filtered pixels are REFM writes");

    // §6.3: selection uses the LATCHED dis_idc, not a late config change.
    df.latch_dis_idc(0); // updates next frame's config, not the active frame
    check(!df.bypass_mode(), "late DFCON change does not change active frame selection");
    df.on_sofm();
    check(df.bypass_mode(), "dis_idc=0 selects bypass");
    check(df.latched_dis_idc() == 0, "bypass decision reads the latched value");
}

// ---------------------------------------------------------------------
// HAS-V-NAL-01 / -02 — activation-relative word count, EOS counted, and
// done only after the final B response (§6.4).
// ---------------------------------------------------------------------
static void test_nal_activation_relative() {
    section("HAS-V-NAL-01/02  NAL word count + EOS (§6.4)");
    DdrModel mem(16u << 20);  // 16 MB: must hold REG_NAL = 0x00A00000 (10 MB)
    H264Arb  arb;
    NalDma   nal(mem, arb);
    nal.configure(0x00A00000ull);

    nal.accept_word(0x6742A100u);
    nal.accept_word(0xDEADBEEFu);
    nal.accept_eos();                       // 00 00 01 0B must be counted
    check(nal.words_staged() == 3, "EOS word is counted in the chunk");

    nal.flush_chunk();
    // Drive the arbiter/bridge so the final B response is accepted.
    drain(arb, mem);

    check(nal.final_b_accepted(), "done reported only after final B response");
    check(nal.stm_len() == 3, "STM_LEN counts 32-bit words relative to activation");
    check(nal.byte_count() == 12, "host drains byte_count = STM_LEN * 4");

    // Activation-relative: next chunk starts at offset 0 again.
    nal.on_disable();
    nal.begin_activation();
    check(nal.stm_len() == 0, "STM_LEN is activation-relative");
    check(nal.write_ptr() == 0x00A00000ull, "write pointer reloads from REG_NAL");

    // §6.4: buffer reuse only after disable; pointer reloads on disable.
    nal.accept_word(0x11223344u);
    nal.flush_chunk();
    bool rejected = false;
    try { nal.on_disable(); } catch (const std::logic_error&) { rejected = true; }
    check(rejected, "disable cannot repoint an in-flight NAL buffer");
    drain(arb, mem);
    nal.on_disable();
    check(nal.write_ptr() == 0x00A00000ull, "disable reloads the write pointer");
    check(nal.stm_len() == 1, "disable preserves STM_LEN for host read/drain");
}

// ---------------------------------------------------------------------
// §5.2 / §5.3 — 4 KiB burst safety and the B-response completion boundary.
// ---------------------------------------------------------------------
static void test_axi_4kib_and_bresp() {
    section("HAS-V-AXI-02/04  4 KiB bursts + B-response boundary (§5.2-§5.3)");

    for (uint32_t width : {32u, 64u, 128u}) {
        AxiBridgeConfig cfg;
        cfg.data_width_bits = width;
        AxiMasterBridge axi(cfg);

        DmaRequest r;
        r.client = ClientId::CMB;
        r.size   = TransferSize::B4;
        r.beats  = 4096;                 // deliberately spans many pages
        r.addr   = 0x0FF0ull;            // starts near a 4 KiB boundary
        r.is_write = false;

        const bool ok = axi.all_segments_in_page(r);
        check(ok, "width " + std::to_string(width) +
                  ": no issued burst crosses a 4 KiB page");

        const auto segs = axi.plan_segments(r);
        uint32_t total = 0;
        for (const auto& s : segs) total += s.len + 1u;
        check(static_cast<uint64_t>(total) * axi.bus_bytes() == request_bytes(r),
              "width " + std::to_string(width) + ": segments cover every requested byte exactly once");
    }

    // §5.3 / HAS-V-AXI-04: a failed BRESP must not become a valid completion.
    AxiBridgeConfig bad;
    bad.drop_bresp = true;
    AxiMasterBridge axi_bad(bad);
    DdrModel mem(1u << 20);
    DmaRequest w;
    w.client = ClientId::NAL; w.is_write = true; w.size = TransferSize::B4;
    w.beats = 4; w.addr = 0x1000;
    std::vector<uint8_t> payload(16, 0xAB);
    check(!axi_bad.write_payload(w, mem, payload),
          "failed BRESP returns no completion (§4.5)");
}

// ---------------------------------------------------------------------
// §5.4 — a grant covers the whole transaction; no mid-transaction preempt;
// read data returns to the correct owner.
// ---------------------------------------------------------------------
static void test_arb_no_preempt() {
    section("§5.4  Arbiter grant integrity");
    H264Arb arb;
    struct Sink : CompletionSink {
        ClientId last = ClientId::NONE;
        uint32_t tag = 0;
        void on_done(ClientId c, const DmaResponse& r) override {
            last = c; tag = r.tag;
        }
    } sink;

    DmaRequest r_cmb; r_cmb.client = ClientId::CMB; r_cmb.tag = 111; r_cmb.beats = 2;
    DmaRequest r_nal; r_nal.client = ClientId::NAL; r_nal.tag = 222; r_nal.beats = 1;

    arb.request(r_cmb, &sink);
    arb.request(r_nal, &sink);

    ClientId first = arb.pick();
    check(arb.acquire(first), "grant acquired for the whole transaction");

    // While granted, another client cannot be selected or acquired.
    ClientId other = (first == ClientId::CMB) ? ClientId::NAL : ClientId::CMB;
    check(!arb.acquire(other), "a second client cannot preempt mid-transaction");
    check(arb.granted() == first, "grant is held until completion");

    arb.complete(true);
    check(sink.last == first, "read data returns to the correct owner");
    check(sink.tag == (first == ClientId::CMB ? 111u : 222u),
          "response tag matches the owner's transaction");

    ClientId second = arb.pick();
    check(second == other, "the remaining client is serviced after completion");
    arb.acquire(second);
    arb.complete(true);
    check(sink.last == other, "second owner receives its own response");
    check(!arb.busy(), "arbiter idle after both transactions");
}

// ---------------------------------------------------------------------
// §6.5 / Bảng 6-1 — end-to-end integration through the subsystem.
// ---------------------------------------------------------------------
static void test_integration_activation() {
    section("§6.5  End-to-end activation through DmaSubsystem");
    CodedDims d{176, 144};
    RegisterBases b;
    DmaSubsystem vp(d, b);

    // Stage a source frame pattern at REG_CMB.
    vp.ddr().fill_pattern(b.REG_CMB, frame_bytes(d), 0x0A);

    // CMB: enable rises → arms the DMA and accepts one frame start (§6.1).
    // has_frame_start() is the non-consuming query used to assert the state.
    // consume_frame_start() clears the flag; fetch_macroblock() requires armed_.
    vp.cmb().update_enable(true);
    check(vp.cmb().has_frame_start(), "frame start accepted at activation begin");
    vp.cmb().consume_frame_start();
    vp.cmb().fetch_macroblock(0, 0);
    const size_t serviced = vp.run();
    check(serviced > 0, "CMB requests were serviced through the arbiter");
    check(vp.arb().log().size() == 32, "one MB = 16 Y rows + 8 U + 8 V = 32 reads");

    // Every issued burst stayed within one 4 KiB page.
    bool all_in_page = true;
    for (const auto& e : vp.axi().issued_log()) {
        const uint64_t sp = e.addr / kDdr4KiBPage;
        const uint64_t ep = (e.addr + (e.len + 1ull) * vp.axi().bus_bytes() - 1ull)
                            / kDdr4KiBPage;
        if (sp != ep) all_in_page = false;
    }
    check(all_in_page, "every issued CMB burst is 4 KiB-safe");

    // NAL chunk lands at REG_NAL and is host-drainable by STM_LEN*4.
    vp.nal().begin_activation();
    vp.nal().accept_word(0x00000001u);
    vp.nal().accept_eos();
    vp.nal().flush_chunk();
    vp.run();
    check(vp.nal().final_b_accepted(), "NAL completion after final B response");
    check(vp.nal().byte_count() == 8, "NAL chunk drainable as STM_LEN*4 = 8 bytes");
    check(vp.ddr().read_byte(b.REG_NAL) == 0x01u,
          "NAL payload written from REG_NAL + 0");
}

int main() {
    std::printf("H.264 DMA subsystem VP — spec self-check suite\n");
    std::printf("Spec: SISLAB_H264_AVC_HAS_Detailed v2.1.11, Ch.5-6, Ch.12, Ch.15\n");

    test_cmb_arm_guard();
    test_cmb_planar_addresses();
    test_refm_slots();
    test_sw_list1_not_resident();
    test_sw_row_start();
    test_df_sticky_done();
    test_df_filter_order();
    test_nal_activation_relative();
    test_axi_4kib_and_bresp();
    test_arb_no_preempt();
    test_integration_activation();

    std::printf("\n----------------------------------------\n");
    std::printf("passed: %d   failed: %d\n", g_pass, g_fail);
    int result = g_fail == 0 ? 0 : 1;
#ifdef H264_WITH_REVIEW
    result |= run_spec_review_tests();
#endif
    return result;
}
