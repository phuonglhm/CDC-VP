/*
 * Direct CSI-2 -> ISP on fx1_soc --camera, plan DIRECT_CSI_ISP D2b: direct +
 * concurrent CSI RAW dump, CSI faults, resets and recovery (contract
 * components/FX1_Components/mipi-csi2/docs/DIRECT_CSI_ISP_CONTRACT.md §9-§11).
 *
 * Sensor 64x32 RAW12 (4 lanes) through the detailed D-PHY, 8 ms frame period
 * (CAM_FRAME_LENGTH 800 lines): the checks of one frame (NV12 CRC, RAW dump
 * compare, about 4.4 ms of simulated CPU time) end inside its blanking. At
 * the 3 ms period the firmware falls behind by ~1.4 ms per frame until a
 * capture finds no free side (Refused: CSI_DROPPED with every line, first D2b
 * run; contract §11).
 * Frames are identified by their 64-bit capture sequence (seq = the sensor's
 * frame index while nothing is lost before the CSI, the platform's
 * --camera-fault numbering). A published frame is checked as in direct.c
 * (context, BUF_TAG / BUF_TS / ISP frame, NV12 against the golden; the
 * golden index is the sensor frame number). A frame that must not be
 * published is checked for its outcome; an output buffer it completed is
 * released unread.
 *
 * RAW dump (CAM_CASE 10..13): the CSI RAW DMA also writes every capture
 * (RAW16, stride 2W) to two sides at CAM_ISP_SIDE(0/1), baseline coupling
 * (contract §11): a DMA refusal / rewind gives capture_end DROPPED (the
 * stream still delivers the whole frame: CSI_DROPPED with H lines, never
 * published, D2a mutation D06); a release with error (policy 1) gives
 * capture_end OK (published, P33). The dump of a capture is found by its
 * DMA buffer FRAME_ID = the context FRAME_ID and checked against the sensor
 * pattern. ISP IDMA reads nothing.
 *
 *   CAM_CASE=10  dump + FRAME_ID wrap (fx1_soc --camera-frame-id-preset 16381):
 *                5 frames, FRAME_IDs 16382, 16383, 1, 2, 3, all published, every
 *                dump exact and associated by FRAME_ID
 *   CAM_CASE=11  dump faults, err_frame_policy 0 (--csi-dma-fault write-error:106):
 *                no free side (seq 4), AXI write error (seq 5, line 9), DMA abort
 *                inside a frame (seq 7), layout error (seq 9: bytes_per_line
 *                below the line): each CSI_DROPPED with H lines, not published;
 *                the frames between and after published with exact dumps
 *   CAM_CASE=12  dump faults, err_frame_policy 1 (--csi-dma-fault write-error:42):
 *                AXI write error (seq 2, line 9), layout error (seq 4), DMA abort
 *                (seq 6): the side released with error, the frame published (P33)
 *   CAM_CASE=13  dump with stream stall / disable: ISP paused (TREADY 0) inside
 *                seq 3 for 5 frame periods (frames dropped, not published, the
 *                DMA stalls with the stream), then the stream path disabled inside
 *                a frame for 2 frame periods: the cut frame INVALID, not published,
 *                its dump and the dumps sent while disabled exact; frames after
 *                the re-enable published
 *   CAM_CASE=20  sensor faults (--camera-fault 3:crc:10 --camera-fault 5:crc:31
 *                --camera-fault 7:no-fe), no dump: seq 3 and 5 (CRC on a middle /
 *                the last line) CSI_DROPPED or INVALID, seq 7 CSI_MISSING_FE with
 *                H lines, none published; the others published in order
 *   CAM_CASE=21  CSI soft reset: compliant (ingress SOURCE memory, ISP_EN 0, CSI
 *                soft reset in the blanking, ISP_EN 1, SOURCE direct): no frame
 *                lost or failed after it, epoch + 1; violation (CSI soft reset
 *                inside a frame while the ISP streams): that frame RESET, not
 *                published, the next ones published, epoch + 1
 *   CAM_CASE=22  core disable inside a frame, enable again: that frame CSI_DROPPED
 *                or INVALID, not published; the next ones published
 *
 * The host checks the traffic (fx1_soc --traffic-report): ISP_IDMA reads 0, with
 * the dump MIPI_DMA writes only inside the two sides.
 */
#include <stdint.h>

/* Set for every translation unit (direct_common.c programs the sensor mode):
 * fw/fx1_soc/CMakeLists.txt builds with -DCAM_FRAME_LENGTH=800u. */
#ifndef CAM_FRAME_LENGTH
#error "recover.c: build with -DCAM_FRAME_LENGTH=<lines> for every source"
#endif
#include "direct_common.h"
#include "fx1_ccs_sensor.h"

#ifndef CAM_CASE
#define CAM_CASE 10
#endif

#define DUMP (CAM_CASE >= 10 && CAM_CASE <= 13)
#define HELPER static __attribute__((unused)) /* not every case uses every helper */
#define LINE_BYTES (2u * W)
#define BAD_LINE_BYTES (LINE_BYTES - 8u) /* M03: a line longer than bytes_per_line: layout error */

static unsigned g_published, g_failed;
static unsigned g_dumps __attribute__((unused));
static uint64_t g_last_seq, g_last_ts;
static uint32_t g_last_epoch;
static uint32_t g_last_n;    /* sensor frame number of the last published frame */
static uint64_t g_last_n_seq; /* its seq: the frame number of seq s is g_last_n + s - g_last_n_seq */
static uint16_t g_last_n_fid; /* its FRAME_ID */

/* ------------------------------------------------------------------ context */

HELPER void next_ctx(fx1_isp_ingress_ctx* c, uint32_t code)
{
    fx1_check(direct_wait_ctx(4u * DIRECT_FRAME_US), code, "recover: a context entry READY");
    fx1_check(fx1_isp_ingress_ctx_pop(&g_ing, c) == FX1_ISP_OK, code | 1u, "recover: context popped");
    fx1_check(c->seq > g_last_seq, code | 2u, "recover: capture sequence increases");
    fx1_check(!g_last_seq || c->ts_ns > g_last_ts, code | 3u, "recover: FS timestamp increases");
    fx1_check(c->epoch >= g_last_epoch, code | 4u, "recover: epoch never decreases");
    fx1_log_hex("  ctx seq", (uint32_t)c->seq);
    g_last_seq = c->seq;
    g_last_ts = c->ts_ns;
    g_last_epoch = c->epoch;
}

/* ------------------------------------------------------------------ RAW dump */

#if DUMP
static void dma_program(uint16_t bytes_per_line)
{
    const fx1_csi2_dma_cfg d = {{CAM_ISP_SIDE(0), CAM_ISP_SIDE(1)},
                                0x1000u,
                                bytes_per_line,
                                (uint8_t)(LINE_BYTES - bytes_per_line), /* stride stays 2W (M02, M04) */
                                255u,
                                0u,
                                0u,
                                1u,
                                1u, /* RAW16, as the stream */
                                (uint8_t)(CAM_CASE == 12 ? 1u : 0u)};
    fx1_check(fx1_csi2_dma_config(&g_cam.csi, &d) == FX1_CSI2_OK, 0x50, "recover: CSI RAW DMA configured");
    fx1_check(fx1_csi2_dma_start(&g_cam.csi) == FX1_CSI2_OK, 0x51, "recover: CSI RAW DMA started");
}
HELPER void dma_reprogram(uint16_t bytes_per_line)
{
    fx1_check(fx1_csi2_dma_stop(&g_cam.csi, 2000) == FX1_CSI2_OK, 0x52, "recover: CSI RAW DMA stopped");
    dma_program(bytes_per_line);
}

/* The side whose FRAME_ID is `fid` and whose state is `state`; -1 if none. */
HELPER int side_of(uint16_t fid, fx1_csi2_buf_state state)
{
    for (unsigned s = 0; s < 2; ++s) {
        fx1_csi2_buf_info bi;
        if (fx1_csi2_get_buffer(&g_cam.csi, s, &bi) == FX1_CSI2_OK && bi.state == state && bi.frame_id == fid)
            return (int)s;
    }
    return -1;
}
HELPER fx1_csi2_buf_info side_info(unsigned s)
{
    fx1_csi2_buf_info bi = {FX1_CSI2_BUF_HW_OWNED, 0, 0};
    (void)fx1_csi2_get_buffer(&g_cam.csi, s, &bi);
    return bi;
}

/* Lines [0, lines) of side s against the sensor pattern of frame n. */
HELPER int side_exact(unsigned s, uint32_t n, unsigned lines)
{
    const uintptr_t base = CAM_ISP_SIDE(s);
    fx1_dma_complete((const volatile void*)base, LINE_BYTES * H, FX1_DMA_FROM_DEVICE);
    for (unsigned y = 0; y < lines; ++y)
        for (unsigned x = 0; x < W; ++x) {
            const volatile uint16_t* p = (const volatile uint16_t*)(base + y * LINE_BYTES + 2u * x);
            if (*p != fx1_ccs_pattern(n, x, y, CAM_BITS)) {
                fx1_log_hex("  RAW mismatch at line", y);
                fx1_log_hex("  column", x);
                return 0;
            }
        }
    return 1;
}
HELPER void side_release(unsigned s)
{
    fx1_dma_prepare((const volatile void*)(uintptr_t)CAM_ISP_SIDE(s), LINE_BYTES * H, FX1_DMA_FROM_DEVICE);
    fx1_check(fx1_csi2_buffer_release(&g_cam.csi, s) == FX1_CSI2_OK, 0x53, "recover: side released");
}

/* The dump of capture c (FRAME_ID association), frame n, whole and valid; released unless held. */
HELPER int dump_valid(const fx1_isp_ingress_ctx* c, uint32_t n, int hold, uint32_t code)
{
    const int s = side_of(c->frame_id, FX1_CSI2_BUF_SW_OWNED_VALID);
    fx1_check(s >= 0, code, "recover: dump side SW_OWNED_VALID with the context FRAME_ID");
    fx1_check(side_exact((unsigned)s, n, H), code | 1u, "recover: dump equals the sensor pattern");
    ++g_dumps;
    if (!hold) side_release((unsigned)s);
    return s;
}
/* No side holds capture c's data (refused, or rewound to HW_OWNED). */
HELPER void dump_none(const fx1_isp_ingress_ctx* c, uint32_t code)
{
    fx1_check(side_of(c->frame_id, FX1_CSI2_BUF_SW_OWNED_VALID) < 0 &&
                  side_of(c->frame_id, FX1_CSI2_BUF_SW_OWNED_ERROR) < 0,
              code, "recover: no dump of a capture the DMA dropped");
}
/* Policy 1: the side of capture c released with error, lines up to its last valid one exact. */
HELPER void dump_error(const fx1_isp_ingress_ctx* c, uint32_t n, int last_valid, uint32_t code)
{
    const int s = side_of(c->frame_id, FX1_CSI2_BUF_SW_OWNED_ERROR);
    fx1_check(s >= 0, code, "recover: side SW_OWNED_ERROR with the context FRAME_ID");
    if (s < 0) return;
    const fx1_csi2_buf_info bi = side_info((unsigned)s);
    fx1_log_hex("  last valid line", bi.last_valid_line);
    if (last_valid != -2) fx1_check(bi.last_valid_line == (uint16_t)last_valid, code | 1u, "recover: BUFFER_ERROR_INFO");
    const unsigned lines = bi.last_valid_line == 0xFFFFu ? 0u : bi.last_valid_line + 1u;
    fx1_check(lines <= H && side_exact((unsigned)s, n, lines), code | 2u, "recover: valid lines equal the pattern");
    side_release((unsigned)s);
}
/* Releases every SW-owned side (dumps sent while the stream is off); returns how many were valid and exact. */
HELPER unsigned dump_poll(uint32_t code)
{
    unsigned k = 0;
    for (unsigned s = 0; s < 2; ++s) {
        const fx1_csi2_buf_info bi = side_info(s);
        if (bi.state == FX1_CSI2_BUF_HW_OWNED) continue;
        fx1_check(bi.state == FX1_CSI2_BUF_SW_OWNED_VALID, code, "recover: dump valid while the stream is off");
        /* FRAME_ID -> frame number: consecutive captures, no wrap in this case. */
        const uint32_t n = g_last_n + (uint16_t)(bi.frame_id - g_last_n_fid);
        fx1_check(side_exact(s, n, H), code | 1u, "recover: dump while the stream is off equals the pattern");
        side_release(s);
        ++k;
        ++g_dumps;
    }
    return k;
}
/* Dumps of captures that have no context entry: older than `fid` (dropped at
 * the ingress before their SOF, NO_STREAM, while the DMA completed them).
 * Checked against the pattern (frame number from the last published frame:
 * one capture per sensor frame here) and released; returns how many. */
HELPER unsigned dump_orphans(uint16_t fid, uint32_t code)
{
    unsigned k = 0;
    for (unsigned s = 0; s < 2; ++s) {
        const fx1_csi2_buf_info bi = side_info(s);
        const uint16_t age = (uint16_t)(fid - bi.frame_id);
        if (bi.state == FX1_CSI2_BUF_HW_OWNED || age == 0 || age >= 0x2000u) continue;
        fx1_log_hex("  dump without a context entry, FRAME_ID", bi.frame_id);
        if (bi.state == FX1_CSI2_BUF_SW_OWNED_VALID) {
            const uint32_t n = g_last_n + (uint16_t)(bi.frame_id - g_last_n_fid);
            fx1_check(side_exact(s, n, H), code, "recover: dump of a capture without context equals the pattern");
            ++g_dumps;
        }
        side_release(s);
        ++k;
    }
    return k;
}
#endif

/* ------------------------------------------------------------------ outcomes */

/* CSI causes seen since the last call (raw IRQ_STATUS, W1C) and, with the
 * dump, both sides and the DMA state: the reason a capture was dropped. */
HELPER void log_csi_state(void)
{
    const uint32_t irq = fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_IRQ_STATUS);
    fx1_log_hex("  CSI IRQ_STATUS", irq);
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_IRQ_STATUS, irq);
#if DUMP
    for (unsigned s = 0; s < 2; ++s) {
        const fx1_csi2_buf_info bi = side_info(s);
        fx1_log_hex("  side state | frame_id << 8", (uint32_t)bi.state | ((uint32_t)bi.frame_id << 8) | (s << 31));
    }
    fx1_log_hex("  DMA_CONTROL", fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_DMA_CONTROL));
    fx1_log_hex("  DMA_STATUS", fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_DMA_STATUS));
    fx1_log_hex("  CORE_STATUS", fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_CORE_STATUS));
#endif
}

/* Publishes c (must be OK); returns the ODMA buffer, re-armed. */
HELPER uint32_t publish(const fx1_isp_ingress_ctx* c, uint32_t code)
{
    if (c->outcome != FX1_ISP_INGRESS_OUTCOME_OK) {
        direct_log_failed(c);
        log_csi_state();
    }
    const int out = direct_find_done(c->tag);
    fx1_check(out >= 0, code, "recover: ODMA DONE with BUF_TAG = context TAG");
    const uint32_t n = direct_check_frame(c, (unsigned)out, code | 0x8u);
    fx1_check(!g_last_n || n > g_last_n, code | 1u, "recover: sensor frames in order");
    direct_arm((unsigned)out);
    g_last_n = n;
    g_last_n_seq = c->seq;
    g_last_n_fid = c->frame_id;
    ++g_published;
    return n;
}

#define OUT(o) (1u << (o))
/* c must not be published: its outcome in `allowed`; with `full`, the stream
 * delivered every line (a CSI drop with a complete stream). */
HELPER void refuse(const fx1_isp_ingress_ctx* c, uint32_t allowed, int full, uint32_t code)
{
    direct_log_failed(c);
    log_csi_state();
    fx1_check(c->outcome != FX1_ISP_INGRESS_OUTCOME_OK && (allowed & OUT(c->outcome)), code,
              "recover: expected outcome, not published");
    if (full) fx1_check(c->lines == H, code | 1u, "recover: the stream delivered the whole frame");
    const int out = direct_find_done(c->tag); /* an output it completed: released unread */
    if (out >= 0) direct_arm((unsigned)out);
    ++g_failed;
}

/* Waits until the ingress has admitted the next frame (IN_FRAME) and a few lines are in. */
HELPER void inside_next_frame(uint32_t code)
{
    /* IN_FRAME rising: not a frame already under way. */
    const uint64_t end = fx1_mtime() + direct_ticks(2u * DIRECT_FRAME_US);
    while ((fx1_isp_ingress_status(&g_ing) & FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK) && fx1_mtime() < end) {
    }
    while (!(fx1_isp_ingress_status(&g_ing) & FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK) && fx1_mtime() < end) {
    }
    fx1_check(fx1_isp_ingress_status(&g_ing) & FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK, code,
              "recover: inside a frame");
    direct_spin_us(60); /* about 6 of the 32 lines (10 us each) */
}

HELPER void expect_ok(uint32_t code)
{
    fx1_isp_ingress_ctx c;
    next_ctx(&c, code);
    const uint32_t n = publish(&c, code | 0x10u);
#if DUMP
    dump_valid(&c, n, 0, code | 0x20u);
#endif
    (void)n;
}

/* ------------------------------------------------------------------ cases */

#if CAM_CASE == 10
static void run_case(void)
{
    uint16_t prev = 0;
    int wrapped = 0;
    for (unsigned i = 0; i < 5u; ++i) {
        fx1_isp_ingress_ctx c;
        next_ctx(&c, 0x100u);
        const uint32_t n = publish(&c, 0x110u);
        dump_valid(&c, n, 0, 0x120u);
        if (i == 0) fx1_check(c.frame_id == 16382u, 0x130, "recover: preset FRAME_ID 16381 -> first capture 16382");
        if (i) {
            fx1_check(c.frame_id == (prev == 0x3FFFu ? 1u : prev + 1u), 0x131,
                      "recover: FRAME_ID + 1, 16383 -> 1 (0 never assigned)");
            wrapped |= prev == 0x3FFFu && c.frame_id == 1u;
        }
        prev = c.frame_id;
    }
    fx1_check(wrapped, 0x132, "recover: FRAME_ID wrapped through the platform");
}
#elif CAM_CASE == 11
static void run_case(void)
{
    fx1_isp_ingress_ctx c;
    expect_ok(0x100u); /* seq 1 */
    /* seq 2, 3: published, sides held: seq 4 finds no free side (Refused) */
    next_ctx(&c, 0x140u);
    const int s2 = dump_valid(&c, publish(&c, 0x141u), 1, 0x142u);
    next_ctx(&c, 0x150u);
    const int s3 = dump_valid(&c, publish(&c, 0x151u), 1, 0x152u);
    next_ctx(&c, 0x160u);
    fx1_check(c.seq == 4u, 0x161, "recover: seq 4");
    refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED), 1, 0x162u);
    dump_none(&c, 0x164u);
    fx1_check(side_info((unsigned)s2).state == FX1_CSI2_BUF_SW_OWNED_VALID &&
                  side_info((unsigned)s3).state == FX1_CSI2_BUF_SW_OWNED_VALID,
              0x165, "recover: held sides untouched by the refused capture");
    side_release((unsigned)s2);
    side_release((unsigned)s3);
    /* seq 5: AXI write error (line 9) -> rewound */
    next_ctx(&c, 0x170u);
    fx1_check(c.seq == 5u, 0x171, "recover: seq 5");
    refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED), 1, 0x172u);
    dump_none(&c, 0x174u);
    fx1_csi2_dma_fault f;
    (void)fx1_csi2_get_dma_fault(&g_cam.csi, &f);
    fx1_log_hex("  fault addr", f.addr);
    fx1_log_hex("  fault resp | slverr << 8", f.resp | ((uint32_t)f.slverr_count << 8));
    fx1_check(fx1_csi2_get_dma_fault(&g_cam.csi, &f) == FX1_CSI2_OK && f.slverr_count == 1u && f.resp == 2u &&
                  (f.addr & 0xFFFFu) == 9u * LINE_BYTES,
              0x175, "recover: AXI fault trace: one SLVERR at line 9");
    expect_ok(0x180u); /* seq 6 */
    /* seq 7: DMA abort inside the frame -> rewound (policy 0) */
    inside_next_frame(0x190u);
    fx1_check(fx1_csi2_dma_abort(&g_cam.csi, 2000) == FX1_CSI2_OK, 0x191, "recover: DMA abort inside a frame");
    next_ctx(&c, 0x1A0u);
    fx1_check(c.seq == 7u, 0x1A1, "recover: seq 7");
    refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED), 1, 0x1A2u);
    dump_none(&c, 0x1A4u);
    dma_program(LINE_BYTES);
    expect_ok(0x1B0u); /* seq 8 */
    /* seq 9: layout error */
    dma_reprogram(BAD_LINE_BYTES);
    next_ctx(&c, 0x1C0u);
    fx1_check(c.seq == 9u, 0x1C1, "recover: seq 9");
    refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED), 1, 0x1C2u);
    dump_none(&c, 0x1C4u);
    dma_reprogram(LINE_BYTES);
    expect_ok(0x1D0u); /* seq 10 */
    expect_ok(0x1E0u); /* seq 11 */
}
#elif CAM_CASE == 12
static void run_case(void)
{
    fx1_isp_ingress_ctx c;
    expect_ok(0x100u); /* seq 1 */
    /* seq 2: AXI write error at line 9 -> released with error, published (P33) */
    next_ctx(&c, 0x140u);
    fx1_check(c.seq == 2u, 0x141, "recover: seq 2");
    dump_error(&c, publish(&c, 0x142u), 8, 0x144u);
    expect_ok(0x150u); /* seq 3 */
    /* seq 4: layout error -> released with error (no line complete), published */
    dma_reprogram(BAD_LINE_BYTES);
    next_ctx(&c, 0x160u);
    fx1_check(c.seq == 4u, 0x161, "recover: seq 4");
    dump_error(&c, publish(&c, 0x162u), 0xFFFF, 0x164u);
    dma_reprogram(LINE_BYTES);
    expect_ok(0x170u); /* seq 5 */
    /* seq 6: DMA abort inside the frame -> released with error, published */
    inside_next_frame(0x180u);
    fx1_check(fx1_csi2_dma_abort(&g_cam.csi, 2000) == FX1_CSI2_OK, 0x181, "recover: DMA abort inside a frame");
    next_ctx(&c, 0x190u);
    fx1_check(c.seq == 6u, 0x191, "recover: seq 6");
    dump_error(&c, publish(&c, 0x192u), -2, 0x194u);
    dma_program(LINE_BYTES);
    expect_ok(0x1A0u); /* seq 7 */
}
#elif CAM_CASE == 13
static void run_case(void)
{
    fx1_isp_ingress_ctx c;
    expect_ok(0x100u); /* seq 1 */
    expect_ok(0x110u); /* seq 2 */
    /* ISP paused inside seq 3 for 5 frame periods: TREADY 0, the CSI drops captures */
    inside_next_frame(0x120u);
    direct_isp_wr(FX1_ISP_COMMON_CTRL_OFFSET, 0);
    direct_spin_us(5u * DIRECT_FRAME_US);
    direct_isp_wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
    unsigned after = 0, failed = 0, orphans = 0;
    for (unsigned k = 0; after < 2u && k < 16u; ++k) {
        next_ctx(&c, 0x130u);
        orphans += dump_orphans(c.frame_id, 0x13Au);
        if (c.outcome == FX1_ISP_INGRESS_OUTCOME_OK) {
            dump_valid(&c, publish(&c, 0x131u), 0, 0x132u);
            ++after;
            continue;
        }
        refuse(&c,
               OUT(FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED) |
                   (c.cause == FX1_ISP_INGRESS_CAUSE_ABORT || c.cause == FX1_ISP_INGRESS_CAUSE_MISSING_LINES
                        ? OUT(FX1_ISP_INGRESS_OUTCOME_INVALID)
                        : 0u),
               0, 0x134u);
        ++failed;
        /* A capture the CSI dropped has no dump (policy 0); one the flush cut after an OK end has one. */
        if (c.outcome == FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED) dump_none(&c, 0x136u);
        else {
            const int s = side_of(c.frame_id, FX1_CSI2_BUF_SW_OWNED_VALID);
            if (s >= 0) side_release((unsigned)s);
        }
    }
    fx1_log_hex("  dumps without a context entry", orphans);
    fx1_check(after == 2u && failed >= 1u, 0x138, "recover: back-pressure drop, then frames published");
    /* Stream path disabled inside a frame for 2 frame periods; the DMA keeps dumping. */
    inside_next_frame(0x140u);
    fx1_check(fx1_csi2_stream_config(&g_cam.csi, 0, 1) == FX1_CSI2_OK, 0x141, "recover: stream off inside a frame");
    unsigned off_dumps = 0;
    const uint64_t end = fx1_mtime() + direct_ticks(2u * DIRECT_FRAME_US);
    while (fx1_mtime() < end) off_dumps += dump_poll(0x142u);
    /* Re-enable in the blanking: right after a dump, the FE has passed. */
    const uint64_t end2 = fx1_mtime() + direct_ticks(2u * DIRECT_FRAME_US);
    unsigned k2 = 0;
    while (!(k2 = dump_poll(0x144u)) && fx1_mtime() < end2) {
    }
    off_dumps += k2;
    fx1_check(fx1_csi2_stream_config(&g_cam.csi, 1, 1) == FX1_CSI2_OK, 0x146, "recover: stream on again");
    fx1_log_hex("  dumps while the stream was off", off_dumps);
    fx1_check(off_dumps >= 2u, 0x147, "recover: the DMA dumped the cut frame and the frames sent while off");
    /* The cut frame: INVALID (abort beat at the stop), its CSI capture ended OK. */
    next_ctx(&c, 0x150u);
    refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_INVALID), 0, 0x151u);
    fx1_check(c.lines < H, 0x153, "recover: the stop cut the frame");
    /* Frames sent while off never reached the ingress (no context entry). */
    next_ctx(&c, 0x160u);
    const uint64_t skipped = c.seq - g_last_n_seq;
    fx1_log_hex("  captures between", (uint32_t)skipped);
    const uint32_t n = publish(&c, 0x161u);
    dump_valid(&c, n, 0, 0x162u);
    expect_ok(0x170u);
}
#elif CAM_CASE == 20
static void run_case(void)
{
    fx1_isp_ingress_ctx c;
    for (unsigned seq = 1; seq <= 9u; ++seq) {
        next_ctx(&c, 0x100u);
        fx1_check(c.seq == seq, 0x101, "recover: one context entry per capture");
        if (seq == 3u || seq == 5u) {
            refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED) | OUT(FX1_ISP_INGRESS_OUTCOME_INVALID), 0, 0x110u);
        } else if (seq == 7u) {
            refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_CSI_MISSING_FE), 1, 0x120u);
        } else {
            const uint32_t n = publish(&c, 0x130u);
            fx1_check(n == seq, 0x131, "recover: sensor frame = capture sequence");
        }
    }
    fx1_csi2_status st;
    fx1_check(fx1_csi2_get_status(&g_cam.csi, &st) == FX1_CSI2_OK && st.crc_error_count == 2u, 0x140,
              "recover: two CRC errors at the CSI");
}
#elif CAM_CASE == 21
static void run_case(void)
{
    fx1_isp_ingress_ctx c;
    expect_ok(0x100u);
    expect_ok(0x110u);
    const uint32_t e0 = g_last_epoch;
    /* Compliant: stop the ingress and the ISP, reset the CSI in the blanking, restart. */
    fx1_check((fx1_isp_ingress_status(&g_ing) &
               (FX1_ISP_INGRESS_STATUS_IDLE_MASK | FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK)) ==
                  FX1_ISP_INGRESS_STATUS_IDLE_MASK,
              0x11F, "recover: in the blanking, ingress idle");
    fx1_check(fx1_isp_ingress_set_source(&g_ing, FX1_ISP_INGRESS_SOURCE_MEMORY) == FX1_ISP_OK, 0x120,
              "recover: SOURCE memory before the CSI reset");
    direct_isp_wr(FX1_ISP_COMMON_CTRL_OFFSET, 0);
    const int rc = fx1_csi2_soft_reset(&g_cam.csi, 2000);
    fx1_log_hex("  compliant soft reset rc", (uint32_t)rc);
    /* In the blanking the clock lane is in LP-11 (P16): ENOCLK, the Lane Management
     * part is applied at the next byte clock (C20); the datapath reset is done. */
    fx1_check(rc == FX1_CSI2_OK || rc == FX1_CSI2_ENOCLK, 0x121, "recover: CSI soft reset");
    direct_isp_wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
    fx1_check(fx1_isp_ingress_set_source(&g_ing, FX1_ISP_INGRESS_SOURCE_DIRECT) == FX1_ISP_OK, 0x122,
              "recover: SOURCE direct again");
    next_ctx(&c, 0x130u);
    fx1_log_hex("  epoch before the reset", e0);
    fx1_log_hex("  epoch after", c.epoch);
    fx1_log_hex("  seq after", (uint32_t)c.seq);
    fx1_check(c.epoch == e0 + 1u, 0x131, "recover: epoch + 1 after the compliant reset");
    publish(&c, 0x132u);
    expect_ok(0x140u);
    const uint32_t e1 = g_last_epoch;
    /* Violation: CSI soft reset inside a frame while the ISP streams. */
    inside_next_frame(0x150u);
    fx1_check(fx1_csi2_soft_reset(&g_cam.csi, 2000) == FX1_CSI2_OK, 0x151, "recover: CSI soft reset inside a frame");
    next_ctx(&c, 0x160u);
    fx1_log_hex("  epoch of the cut frame", c.epoch);
    fx1_check(c.epoch == e1, 0x161, "recover: the cut frame started before the reset");
    refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_RESET), 0, 0x162u);
    next_ctx(&c, 0x170u);
    fx1_check(c.epoch == e1 + 1u, 0x171, "recover: epoch + 1 after the reset");
    publish(&c, 0x172u);
    expect_ok(0x180u);
}
#elif CAM_CASE == 22
static void run_case(void)
{
    fx1_isp_ingress_ctx c;
    expect_ok(0x100u);
    expect_ok(0x110u);
    inside_next_frame(0x120u);
    fx1_check(fx1_csi2_core_enable(&g_cam.csi, 0) == FX1_CSI2_OK, 0x121, "recover: core disabled inside a frame");
    direct_spin_us(50);
    fx1_check(fx1_csi2_core_enable(&g_cam.csi, 1) == FX1_CSI2_OK, 0x122, "recover: core enabled again");
    next_ctx(&c, 0x130u);
    refuse(&c, OUT(FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED) | OUT(FX1_ISP_INGRESS_OUTCOME_INVALID), 0, 0x131u);
    fx1_check(c.lines < H, 0x133, "recover: the disable cut the frame");
    expect_ok(0x140u);
    expect_ok(0x150u);
}
#else
#error "unknown CAM_CASE"
#endif

int main(void)
{
    fx1_log("direct CSI-2 -> ISP recovery");
    direct_setup(FX1_ISP_NUM_BUFFERS);
#if DUMP
    dma_program(LINE_BYTES);
#endif
    direct_start();
    run_case();
    fx1_log_hex("  published", g_published);
    fx1_log_hex("  not published", g_failed);
#if DUMP
    fx1_log_hex("  RAW dumps checked", g_dumps);
#endif

    fx1_check(fx1_ccs_stream(&g_cam.sensor, 0) == FX1_CCS_OK, 0x41, "recover: sensor stopped");
    fx1_isp_ingress_counters k;
    direct_drain(&k, 8u * DIRECT_FRAME_US);
#if DUMP
    for (unsigned s = 0; s < 2; ++s)
        if (side_info(s).state != FX1_CSI2_BUF_HW_OWNED) side_release(s);
    fx1_check(fx1_csi2_dma_stop(&g_cam.csi, 4000) == FX1_CSI2_OK, 0x42, "recover: CSI RAW DMA stopped");
#endif
    fx1_check(k.captures_untracked == 0 && k.drop_no_context == 0, 0x43, "recover: every capture tracked");
    fx1_check(fx1_isp_ingress_set_source(&g_ing, FX1_ISP_INGRESS_SOURCE_MEMORY) == FX1_ISP_OK, 0x47,
              "recover: SOURCE back to memory");
    fx1_log("direct CSI-2 -> ISP recovery: PASS");
    return 0;
}
