// Independent HAS contract checks for the 2026-10-10 review.
// Regression assertions for defects found in the reviewed implementation.
// No RTL timing, endian convention, or arbiter priority is inferred here.
#include <algorithm>
#include <cstdio>
#include <vector>
#include "dma_subsystem.h"

using namespace h264;
static int passed = 0, failed = 0;
static void check(bool ok, const char* label) {
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", label);
    ok ? ++passed : ++failed;
}
static RegisterBases small_bases() { return {0x1000, 0x10000, 0x40000}; }
static DmaRequest request(ClientId c, uint64_t addr, uint32_t count,
                          TransferSize size, bool write) {
    DmaRequest r;
    r.client = c; r.addr = addr; r.beats = count; r.size = size; r.is_write = write;
    return r;
}

static void bridge_contracts() {
    DdrModel mem(1u << 20);
    for (uint32_t width : {32u, 64u, 128u}) {
        AxiBridgeConfig cfg;
        cfg.data_width_bits = width;
        AxiMasterBridge axi(cfg);
        mem.fill_pattern(0x1000, 64, 0xA0);
        // All clients express row/chunk counts as 32-bit internal words.
        auto r = request(ClientId::CMB, 0x1000, 4, TransferSize::B4, false);
        const auto bytes = axi.read(r, mem);
        char label[128];
        std::snprintf(label, sizeof(label), "AXI%u: four internal words return 16 bytes", width);
        check(bytes.size() == 16, label);
        check(!axi.issued_log().empty(), "actual read populates accepted-address log");

        mem.fill_pattern(0x1000, 64, 0xA0);
        r.is_write = true;
        const uint8_t next = mem.read_byte(0x1010);
        check(axi.write_payload(r, mem, std::vector<uint8_t>(16, 0x11)),
              "word payload write succeeds");
        std::snprintf(label, sizeof(label), "AXI%u: word write preserves byte after payload", width);
        check(mem.read_byte(0x1010) == next, label);

        for (TransferSize size : {TransferSize::B1, TransferSize::B2}) {
            const size_t count = static_cast<size_t>(size);
            mem.fill_pattern(0x2000, 64, 0xA0);
            const uint8_t untouched = mem.read_byte(0x2000 + count);
            r = request(ClientId::NAL, 0x2000, 1, size, true);
            axi.write_payload(r, mem, std::vector<uint8_t>(count, 0x55));
            std::snprintf(label, sizeof(label), "AXI%u: %zu-byte write preserves unselected bytes", width, count);
            check(mem.read_byte(0x2000 + count) == untouched, label);
        }
    }
    AxiBridgeConfig cfg;
    cfg.data_width_bits = 128;
    AxiMasterBridge wide(cfg);
    // +8 is a legal chroma-row offset emitted by CMB on the wider bus.
    auto r = request(ClientId::CMB, 0xFF8, 1, TransferSize::B4, false);
    check(wide.all_segments_in_page(r), "AXI128: addressed word at 0xFF8 has page-safe external burst");

    cfg = {}; cfg.delay_bvalid = true;
    DmaSubsystem vp({176, 144}, small_bases(), cfg, 1u << 20);
    vp.nal().accept_word(0x12345678);
    vp.nal().flush_chunk();
    check(!vp.nal().final_b_accepted(), "NAL is incomplete before service");
    // One service call cannot fabricate an immediate delayed B response.
    vp.service_one();
    check(!vp.nal().final_b_accepted(), "delay_bvalid leaves completion pending for a later B response");
}

static void client_completion_contracts() {
    DmaSubsystem cmbvp({176, 144}, small_bases(), {}, 1u << 20);
    cmbvp.cmb().update_enable(true);
    cmbvp.cmb().fetch_macroblock(0, 0);
    cmbvp.service_one();
    check(!cmbvp.cmb().done(), "CMB tile is incomplete after only one of its 32 row reads");
    cmbvp.run();
    check(cmbvp.cmb().done(), "CMB tile completes after all row reads");
    cmbvp.cmb().fetch_macroblock(1, 0);
    check(!cmbvp.cmb().done(), "next CMB tile starts incomplete");

    DdrModel mem(1u << 20); H264Arb arb;
    CmbDma cmb(mem, arb);
    DmaResponse bad; bad.ok = false;
    cmb.on_done(ClientId::CMB, bad);
    check(!cmb.done(), "failed CMB read never produces successful done");

    SwDma sw(arb, {176, 144}, {64, 64});
    sw.set_refm_base(0x10000);
    sw.set_ref_slot(RefList::List0, 0);
    sw.set_ref_slot(RefList::List1, 1);
    sw.fill_window(RefList::List0, 0, 0);
    sw.fill_window(RefList::List1, 0, 0);
    check(!sw.ready(RefList::List1), "SW List1 stays nonresident until reads complete");
    check(!sw.fme_mc_may_start(true), "B-picture FME/MC stays blocked while both lists are queued");
    AxiMasterBridge bridge;
    while (arb.pick() != ClientId::NONE) {
        arb.acquire(arb.pick());
        arb.complete(true, bridge.read(arb.granted_request(), mem));
    }
    const size_t old_log = arb.log().size();
    sw.fill_window(RefList::List0, 1, 0);
    check(arb.log().size() > old_log, "moving SW to next MB requests the newly exposed region");
    sw.fill_window(RefList::List0, 0, 0);
    const size_t before_row = arb.log().size();
    sw.fill_window(RefList::List0, 0, 1);
    check(arb.log().size() > before_row, "moving SW to next row requests the new row region");

    DmaSubsystem dfvp({176, 144}, small_bases(), {}, 1u << 20);
    dfvp.df().on_sofm();
    dfvp.df().schedule_macroblock(0, 0, MacroblockPixels{});
    dfvp.df().dma_fmdone(); dfvp.df().consume_done();
    dfvp.service_one();
    check(!dfvp.df().final_write_acked(), "DF first response is not the final write response");
    check(!dfvp.df().frame_complete(), "DF frame is incomplete with another write pending");
    dfvp.run();
    check(dfvp.df().frame_complete(), "DF frame completes after all writes and consumed done");
    dfvp.df().on_sofm();
    check(!dfvp.df().frame_complete(), "new DF frame does not inherit old completion");

    DmaSubsystem dfbad({176, 144}, small_bases(), {}, 1u << 20);
    dfbad.df().schedule_macroblock(0, 0, MacroblockPixels{});
    dfbad.df().dma_fmdone(); dfbad.df().consume_done();
    while (dfbad.df().pending_writes() > 1) dfbad.service_one();
    dfbad.arb().acquire(ClientId::DF); dfbad.arb().complete(false);
    check(!dfbad.df().frame_complete(), "failed final DF write invalidates reference completion");
}

static void nal_contracts() {
    DmaSubsystem vp({176, 144}, small_bases(), {}, 1u << 20);
    vp.nal().accept_word(0x11111111); vp.nal().flush_chunk(); vp.run();
    vp.nal().accept_word(0x22222222); vp.nal().flush_chunk(); vp.run();
    check(vp.nal().stm_len() == 2, "two NAL flushes count two words in one activation");
    check(vp.ddr().read_byte(0x40000) == 0x11 && vp.ddr().read_byte(0x40004) == 0x22,
          "NAL flushes append within the activation without overwriting word one");
    vp.nal().on_disable();
    vp.nal().begin_activation();
    vp.nal().accept_eos(); vp.nal().flush_chunk(); vp.run();
    check(vp.ddr().read_byte(0x40000) == 0x00 && vp.ddr().read_byte(0x40001) == 0x00 &&
          vp.ddr().read_byte(0x40002) == 0x01 && vp.ddr().read_byte(0x40003) == 0x0B,
          "accept_eos emits the documented bytes 00 00 01 0B");

    DmaSubsystem inflight({176, 144}, small_bases(), {}, 1u << 20);
    inflight.nal().accept_word(0x11111111); inflight.nal().flush_chunk();
    inflight.nal().accept_word(0x22222222); inflight.nal().flush_chunk();
    inflight.service_one();
    check(inflight.ddr().read_byte(0x40004) == 0,
          "queued one-word NAL request does not commit bytes belonging to the next request");
    check(inflight.nal().stm_len() == 1,
          "first one-word NAL completion counts exactly one committed word");
    inflight.run();
    check(inflight.ddr().read_byte(0x40000) == 0x11 && inflight.ddr().read_byte(0x40004) == 0x22,
          "multiple queued NAL writes preserve both accepted words");
}

static void integration_contracts() {
    DmaSubsystem vp({176, 144}, small_bases(), {}, 1u << 20);
    vp.nal().accept_word(0x12345678); vp.nal().flush_chunk();
    vp.arb().acquire(ClientId::NAL);
    check(vp.service_one() == ClientId::NAL, "service_one reports the owner of a pre-acquired grant");

    DmaSubsystem bounded({176, 144}, small_bases(), {}, 1u << 20);
    bounded.nal().accept_word(0x12345678); bounded.nal().flush_chunk();
    bool converged = true;
    try { bounded.run(1); } catch (const std::runtime_error&) { converged = false; }
    check(converged, "run(1) succeeds when the sole transaction finishes within its bound");

    const CodedDims hd{1920, 1088};
    const RegisterBases defaults;
    check(defaults.REG_NAL >= defaults.REG_REFM + 3ull * frame_bytes(hd),
          "default NAL base follows the three full-HD REFM slots without overlap");
}

static MacroblockPixels tile_pattern(uint8_t seed = 1) {
    MacroblockPixels tile;
    for (size_t i = 0; i < tile.y.size(); ++i) tile.y[i] = static_cast<uint8_t>(seed + i);
    for (size_t i = 0; i < tile.u.size(); ++i) {
        tile.u[i] = static_cast<uint8_t>(seed + 17 + i);
        tile.v[i] = static_cast<uint8_t>(seed + 71 + i);
    }
    return tile;
}

static bool tile_matches(const MacroblockPixels& tile, const DdrModel& mem,
                         uint64_t base, CodedDims dims, uint32_t mb_x, uint32_t mb_y) {
    for (uint32_t row = 0; row < 16; ++row)
        for (uint32_t col = 0; col < 16; ++col)
            if (tile.y[row * 16 + col] != mem.read_byte(planar_y_addr(base, dims, mb_x * 16 + col, mb_y * 16 + row)))
                return false;
    for (uint32_t row = 0; row < 8; ++row)
        for (uint32_t col = 0; col < 8; ++col)
            if (tile.u[row * 8 + col] != mem.read_byte(planar_u_addr(base, dims, mb_x * 8 + col, mb_y * 8 + row)) ||
                tile.v[row * 8 + col] != mem.read_byte(planar_v_addr(base, dims, mb_x * 8 + col, mb_y * 8 + row)))
                return false;
    return true;
}

static bool window_matches(const SwDma& sw, RefList list, const DdrModel& mem,
                           uint64_t base, CodedDims dims) {
    if (!sw.ready(list)) return false;
    for (uint32_t p = 0; p < 3; ++p) {
        const auto& plane = sw.plane(list, p);
        for (uint32_t row = 0; row < plane.height; ++row)
            for (uint32_t col = 0; col < plane.width; ++col) {
                const auto x = plane.x + col, y = plane.y + row;
                const auto addr = p == 0 ? planar_y_addr(base, dims, x, y) :
                                  p == 1 ? planar_u_addr(base, dims, x, y) : planar_v_addr(base, dims, x, y);
                if (plane.pixels[row * plane.width + col] != mem.read_byte(addr)) return false;
            }
    }
    return true;
}

static void pixel_and_stall_contracts() {
    const CodedDims dims{176,144};
    for (uint32_t width : {32u,64u,128u}) {
        for (uint32_t delay : {0u,20u,50u}) {
            AxiBridgeConfig cfg;
            cfg.data_width_bits = width;
            cfg.arready_delay = cfg.rvalid_delay = delay;
            cfg.r_beat_gap = delay ? 1 : 0;
            cfg.consumer_delay = delay;
            cfg.awready_delay = cfg.wready_delay = cfg.bvalid_delay = delay ? 1 : 0;
            DmaSubsystem vp(dims, small_bases(), cfg, 1u << 20);
            vp.ddr().fill_pattern(vp.bases().REG_CMB, frame_bytes(dims), 0x31);
            vp.cmb().update_enable(true);
            vp.cmb().fetch_macroblock(1,1); // chroma starts in an upper 128-bit lane
            vp.run();
            check(tile_matches(vp.cmb().pixels(), vp.ddr(), vp.bases().REG_CMB, dims, 1,1),
                  "CMB Y/U/V pixels equal source at every width and read delay 0/20/50");
            vp.df().set_ref_slot(2);
            vp.df().on_sofm();
            const auto tile = vp.cmb().pixels();
            vp.df().schedule_macroblock(1,1,tile,false); // bypass accepts reconstructed pixels
            vp.df().dma_fmdone(); vp.df().consume_done();
            vp.run();
            const auto ref = refm_slot_base(vp.bases().REG_REFM,dims,2);
            check(vp.df().frame_complete() && tile_matches(tile,vp.ddr(),ref,dims,1,1),
                  "DF bypass writes real Y/U/V to selected slot after all delayed responses");
            check(vp.axi().max_fifo_words() <= kReadFifoDepth, "read FIFO stays within 16 internal words");
            bool safe = !vp.axi().issued_log().empty();
            for (const auto& s : vp.axi().issued_log())
                safe = safe && s.addr % vp.axi().bus_bytes() == 0 && s.size_bytes == vp.axi().bus_bytes() &&
                       s.addr / 4096 == (s.addr + (s.len + 1ull) * s.size_bytes - 1) / 4096;
            check(safe, "accepted read/write bursts are aligned, correctly sized and page-safe");
        }
    }

    DmaSubsystem vp(dims,small_bases(),{},1u << 20);
    vp.ddr().fill_pattern(vp.bases().REG_REFM,3ull * frame_bytes(dims),0x27);
    vp.sw().set_ref_slot(RefList::List0,0);
    vp.sw().fill_window(RefList::List0,0,0); vp.run();
    check(window_matches(vp.sw(),RefList::List0,vp.ddr(),vp.bases().REG_REFM,dims),
          "SW fills all Y/U/V rows rather than two contiguous fragments");
    vp.arb().log().clear();
    vp.sw().fill_window(RefList::List0,1,0); vp.run();
    size_t fetched = 0;
    for (const auto& r : vp.arb().log().entries()) fetched += static_cast<size_t>(r.beats) * r.size_bytes;
    check(fetched == 1536, "horizontal SW movement fetches only new 16-pixel strip with chroma");
    check(window_matches(vp.sw(),RefList::List0,vp.ddr(),vp.bases().REG_REFM,dims), "overlapping SW pixels remain exact");
    vp.sw().fill_window(RefList::List0,1,1); vp.run();
    check(window_matches(vp.sw(),RefList::List0,vp.ddr(),vp.bases().REG_REFM,dims), "new-row SW pixels use picture stride");
    vp.sw().fill_window(RefList::List0,10,8); vp.run();
    check(window_matches(vp.sw(),RefList::List0,vp.ddr(),vp.bases().REG_REFM,dims) &&
          vp.sw().plane(RefList::List0,0).width == 16 && vp.sw().plane(RefList::List0,0).height == 16,
          "SW bottom/right clipping stays within the coded frame");
    vp.sw().fill_window(RefList::List0,0,0);
    vp.sw().set_ref_slot(RefList::List0,1,42); // invalidate in-flight old-generation reads
    vp.sw().fill_window(RefList::List0,0,0);
    vp.service_one();
    check(!vp.sw().ready(RefList::List0), "old-generation response cannot make retagged SW ready");
    vp.run();
    check(window_matches(vp.sw(),RefList::List0,vp.ddr(),refm_slot_base(vp.bases().REG_REFM,dims,1),dims),
          "retagged SW returns the new slot's pixels");

    auto working_bases = small_bases();
    working_bases.REG_REFM = 0x20000;
    working_bases.cmb_frames = 2;
    DmaSubsystem working(dims, working_bases, {}, 1u << 20);
    working.cmb().configure(dims,working.bases().REG_CMB,CmbFrameMode::WorkingSet);
    working.cmb().set_working_set_frames(2);
    working.cmb().select_working_frame(1);
    const auto frame1 = working.bases().REG_CMB + frame_bytes(dims);
    working.ddr().fill_pattern(frame1,frame_bytes(dims),0x62);
    working.cmb().update_enable(true); working.cmb().fetch_macroblock(0,0); working.run();
    check(tile_matches(working.cmb().pixels(),working.ddr(),frame1,dims,0,0), "CMB working-set mode applies selected frame offset");

    DmaSubsystem filtered(dims, small_bases(), {}, 1u << 20);
    filtered.df().latch_df_enable(true);
    std::vector<FilterPass> order;
    filtered.df().set_filter([&](FilterPass pass, MacroblockPixels& tile) {
        order.push_back(pass);
        tile.y[0] = pass == FilterPass::Vertical ? 10 : static_cast<uint8_t>(tile.y[0] + 3);
    });
    filtered.df().schedule_macroblock(0,0,tile_pattern(),false);
    filtered.df().dma_fmdone(); filtered.df().consume_done(); filtered.run();
    check(order == std::vector<FilterPass>{FilterPass::Vertical,FilterPass::Horizontal} &&
          filtered.ddr().read_byte(filtered.bases().REG_REFM) == 13,
          "DF invokes vertical then horizontal producer before exporting filtered samples");
    const auto old_frame = filtered.df().frame_id();
    filtered.df().on_sofm();
    filtered.df().schedule_macroblock(0,0,tile_pattern(9));
    filtered.df().dma_fmdone();
    const auto pending_frame = filtered.df().frame_id();
    filtered.df().on_sofm();
    filtered.run();
    check(filtered.df().retained_frame() == pending_frame && !filtered.df().frame_complete(),
          "new SOF keeps pending writes and sticky done associated with their original frame");
    filtered.df().consume_done();
    check(filtered.df().frame_complete(old_frame) && filtered.df().frame_complete(pending_frame) &&
          !filtered.df().frame_complete(), "consuming old DF done cannot complete the new frame");
}

static void boundary_and_error_contracts() {
    for (uint32_t width : {32u,64u,128u}) {
        AxiBridgeConfig cfg;
        cfg.data_width_bits = width; cfg.burst_limit = 2;
        AxiMasterBridge bridge(cfg);
        DdrModel mem(1u << 20);
        for (TransferSize size : {TransferSize::B1,TransferSize::B2,TransferSize::B4}) {
            for (uint32_t lane = 0; lane < width / 8; ++lane) {
                mem.fill_pattern(0xF00,512,0x27);
                const uint64_t addr = 0xFF0 + lane;
                const size_t n = 23 * static_cast<size_t>(size);
                const uint8_t before = mem.read_byte(addr - 1), after = mem.read_byte(addr + n);
                std::vector<uint8_t> payload(n);
                for (size_t i=0; i<n; ++i) payload[i] = static_cast<uint8_t>(i + 0x42);
                auto r = request(ClientId::NAL,addr,23,size,true);
                const auto segments = bridge.plan_segments(r);
                const uint32_t responses_before = bridge.b_responses();
                bridge.clear_log();
                bool ok = bridge.write_payload(r,mem,payload);
                auto read_req = r; read_req.is_write = false;
                ok = ok && bridge.read(read_req,mem) == payload && mem.read_byte(addr-1) == before &&
                     mem.read_byte(addr+n) == after &&
                     bridge.b_responses() - responses_before == segments.size();
                size_t strobed = 0;
                for (const auto& beat : bridge.write_log())
                    for (uint32_t b = 0; b < width/8; ++b)
                        if (beat.strobe & (1u << b)) ++strobed;
                check(ok && strobed == n, "all bus lanes/sizes survive multi-burst 4KiB read/write with one B per segment");
            }
        }
    }
    AxiBridgeConfig cfg;
    cfg.drop_rresp = true;
    DmaSubsystem cmbbad({176,144},small_bases(),cfg,1u << 20);
    cmbbad.cmb().update_enable(true); cmbbad.cmb().fetch_macroblock(0,0); cmbbad.run();
    check(cmbbad.error() && cmbbad.cmb().failed() && !cmbbad.cmb().done(), "RRESP failure is delivered to CMB and subsystem error state");
    cmbbad.sw().set_ref_slot(RefList::List0,0); cmbbad.sw().fill_window(RefList::List0,0,0); cmbbad.run();
    check(cmbbad.sw().failed(RefList::List0) && !cmbbad.sw().ready(RefList::List0), "RRESP failure never validates SW cache");
    cfg = {}; cfg.drop_bresp = true;
    DmaSubsystem bad({176,144},small_bases(),cfg,1u << 20);
    bad.nal().accept_word(0x12345678); bad.nal().flush_chunk(); bad.run();
    check(bad.error() && bad.nal().b_failed() && !bad.nal().final_b_accepted() && bad.nal().stm_len() == 0,
          "failed B response produces error without successful NAL length/completion");
    bad.df().schedule_macroblock(0,0,tile_pattern()); bad.df().dma_fmdone(); bad.df().consume_done(); bad.run();
    check(bad.df().failed() && !bad.df().frame_complete(), "failed DF writes never validate reference frame");

    DmaSubsystem vp({176,144},small_bases(),{},1u << 20);
    bool allocation_rejected = false;
    try { vp.cmb().set_working_set_frames(2); }
    catch (const std::out_of_range&) { allocation_rejected = true; }
    check(allocation_rejected, "CMB cannot grow beyond its host-allocated source region");
    vp.nal().accept_word(0x01020304); vp.nal().flush_chunk();
    bool rejected = false;
    try { vp.nal().begin_activation(); } catch (const std::logic_error&) { rejected = true; }
    check(rejected, "reactivation cannot erase queued NAL payload");
    vp.run();
    rejected = false;
    try { vp.nal().begin_activation(); } catch (const std::logic_error&) { rejected = true; }
    check(rejected, "completed activation still requires disable before reactivation");
    vp.nal().on_disable();
    check(vp.nal().stm_len() == 1, "STM_LEN survives disable until host drains output");
    vp.nal().begin_activation(); vp.nal().accept_eos(); vp.nal().flush_chunk(); vp.run();
    check(vp.nal().stm_len() == 1 && vp.ddr().read_byte(vp.bases().REG_NAL + 3) == 0x0B,
          "reactivation resets offset/count and keeps EOS byte order");
    NalDma big(vp.ddr(), vp.arb());
    big.configure(vp.bases().REG_NAL,8,WordByteOrder::BigEndian);
    big.accept_eos(); big.flush_chunk(); vp.run();
    check(vp.ddr().read_byte(vp.bases().REG_NAL + 3) == 0x0B, "explicit big-endian model convention also emits documented EOS bytes");
    big.accept_word(0x12345678);
    rejected = false;
    try { big.accept_word(0); } catch (const std::out_of_range&) { rejected = true; }
    check(rejected, "NAL ingestion enforces activation capacity before enqueue");
    big.flush_chunk(); vp.run();
    rejected = false;
    try { DmaSubsystem overlap({1920,1088},{0x100000,0x400000,0xA00000}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "overlapping full-HD CMB/REFM/NAL allocation is rejected");
    rejected = false;
    try { DmaSubsystem invalid({1920,1080},RegisterBases{}); }
    catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "presentation height 1080 cannot be used as coded storage height");

    cfg = {}; cfg.consumer_delay = 5; cfg.data_width_bits = 128; cfg.burst_limit = 256;
    AxiMasterBridge fifo(cfg);
    auto r = request(ClientId::CMB,0x1000,128,TransferSize::B4,false);
    vp.ddr().fill_pattern(r.addr,512,0x38);
    const auto data = fifo.read(r,vp.ddr());
    check(data.size() == 512 && fifo.max_fifo_words() == 16,
          "continuous wide R data with delayed consumer fills but never overflows 16-word FIFO");
    cfg.bad_rlast = true;
    AxiMasterBridge wrong_last(cfg);
    rejected = false;
    try { wrong_last.read(r,vp.ddr()); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "malformed RLAST becomes failed read rather than successful data");
}

static void external_memory_contracts() {
    struct SharedMemory : MemoryIf {
        DdrModel storage{1u<<20};
        static constexpr uint64_t base() { return 0x80000000ull; }
        void write(uint64_t a,const uint8_t* p,size_t n) override { storage.write(a-base(),p,n); }
        void read(uint64_t a,uint8_t* p,size_t n) const override { storage.read(a-base(),p,n); }
        uint8_t read_byte(uint64_t a) const override { return storage.read_byte(a-base()); }
    } memory;
    RegisterBases bases{0x80001000,0x80010000,0x80030000,1,4096};
    {
        DmaSubsystem vp({32,16},bases,memory,SharedMemory::base(),1u<<20);
        check(&vp.memory()==&memory,"subsystem uses borrowed physical memory interface");
        bool rejected=false;
        try { (void)vp.ddr(); } catch(const std::logic_error&) { rejected=true; }
        check(rejected,"external-memory subsystem does not expose private DDR");
        vp.nal().accept_word(0x67452301);vp.nal().flush_chunk();vp.run();
        check(vp.nal().stm_len()==1 && memory.read_byte(bases.REG_NAL)==1 &&
              memory.read_byte(bases.REG_NAL+3)==0x67,"NAL commits into physical shared-memory window");
    }
    check(memory.read_byte(bases.REG_NAL)==1,"destroying DMA does not own or destroy shared memory");
    bool rejected=false;bases.REG_CMB=0x1000;
    try { DmaSubsystem invalid({32,16},bases,memory,SharedMemory::base(),1u<<20); }
    catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"physical base below shared-memory window is rejected");
}

int run_spec_review_tests() {
    passed = failed = 0;
    bridge_contracts();
    client_completion_contracts();
    nal_contracts();
    integration_contracts();
    pixel_and_stall_contracts();
    boundary_and_error_contracts();
    external_memory_contracts();
    std::printf("Review contracts: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}

#ifndef H264_COMBINED_TESTS
int main() { return run_spec_review_tests(); }
#endif
