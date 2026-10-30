/*
 * Camera commands (CSI-2 M6b), through the camera manager
 * (fw/fx1_soc/drivers/fx1_camera) from the CLI task:
 *   camera probe                    CSI-2 RX identity, sensor model_id over I2C0
 *   camera mode [W H BITS LANES]    set (default 64 32 12 4) or show the mode
 *   camera source [sensor|tpg]      set or show the source (TPG: no D-PHY)
 *   camera start                    configure (profile basic) and start
 *   camera capture N [crc]          N frames: metadata, NV12 CRC, golden match
 *   camera status                   state, counters, drops, sensor frame_count
 *   camera stop [US]                stop within US (default 24000) + slack
 *   camera recover                  stop, sensor soft reset, configure, start
 *
 * The commands run in the CLI task: the manager is polled with a one-tick
 * delay between polls, so the other tasks keep running while a capture waits.
 * The CSI-2 interrupt (PLIC source 7) goes to the manager; the ISP interrupt
 * is shared with the `isp` commands and goes to the manager while the camera
 * owns the ISP (configured or running, isp_claim). Frames of a capture are
 * collected first and printed afterwards: console output (TX polled at the
 * UART's character time) would otherwise hold the CSI-2 sides and make the
 * receiver drop frames.
 *
 * Verification: NV12 CRC-32 against the golden of the stand-alone ISP model
 * (fw/fx1_soc/tests/camera_isp/camera_isp_golden.h) where one exists: sensor
 * 64x32 RAW12 / RAW10 frames 1..32 (any lane count), TPG 640x480 and
 * 3840x2160 RAW12. The CRC is computed for NV12 up to 1 MiB, or always with
 * `crc`: the simulated CPU runs about 0.3 MIPS from DDR, so a 12 MiB CRC at 4K
 * takes about ten minutes of host time (the bare-metal tpg4k regression checks
 * the 4K NV12 on the host instead). Every frame's RAW is spot-checked at
 * hand-over (capture hook): sensor pattern (frame number from pixel (0,0)) or
 * TPG colour bars. When the CRC is skipped, a golden mode's NV12 is
 * spot-checked instead: 32 Y and 32 UV bytes of the golden (TPG 4K); those
 * positions are poisoned before configure and in every frame of that mode
 * before its release, CRC or not, so a slot the ISP did not write cannot pass
 * with an earlier frame's data (M6b review F2, re-review).
 *
 * Failures counted for poweroff: probe and capture checks, start and recover
 * failures. stop prints its result (OK / ESENSOR / ETIMEOUT) without counting:
 * a sensor that does not confirm standby is a state the operator handles.
 * Commands refused for the state (not probed, running, ISP busy, camera
 * absent) print why and are not counted either.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "camera_isp_golden.h"
#include "fx1_camera.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"
#include "fx1_fw.h"
#include "fx1_isp_hal_mmio.h"
#include "rtos_app.h"
#include "rtos_bsp.h"

/* DDR layout, clear of the `isp` commands' buffers (first 16 MiB of the RAW
 * and YUV regions): sizes for frames up to 3840x2160. */
#define CAM_SIDE(i) (FX1_FW_RAW_BUF_BASE + 0x1000000u + (i) * 0x1000000u)  /* 2 x 16 MiB RAW16 */
#define CAM_STAGING(i) (FX1_FW_RAW_BUF_BASE + 0x3000000u + (i) * 0x1000000u) /* 4 x 16 MiB (RAW10) */
#define CAM_Y(i) (FX1_FW_YUV_BUF_BASE + 0x1000000u + (i) * 0x1400000u)     /* 4 NV12 slots */
#define CAM_UV(i) (CAM_Y(i) + 0xC00000u)
#define CAM_SIDE_SIZE 0x1000000u
#define CAM_MAX_W 3840u
#define CAM_MAX_H 2160u
#define CAM_MAX_FRAMES 32u
#define CAM_CRC_LIMIT 0x100000u   /* NV12 bytes checked by default */
#define CAM_STOP_US 24000u
#define CAM_PIXEL_HZ 200000000u   /* CcsSensorConfig::pixel_rate_hz */
#define CAM_UI_PS 1250u           /* D-PHY TX BFM: 800 Mb/s per lane */

static fx1_camera cam;
static struct {
    uint32_t w, h, bits, lanes;
    int tpg;
} mode = {64u, 32u, 12u, 4u, 0};  /* 32 lines: the ISP v1.07 minimum active size */
static fx1_camera_cfg cfg;
static int probed;                /* probe done, CSI-2 RX present */
static int csi_ready;             /* the CSI-2 driver instance is valid (its ISR may run) */
static const char* last_op = "none";
static int last_rc;

/* ---- interrupt services ------------------------------------------------------ */

/* Acknowledges at the controller whatever the state (an interrupt after a
 * stop must not keep the level line high); source 7 cannot pend before the
 * first probe (interrupt enables reset to 0). */
static BaseType_t csi_isr(void)
{
    if (csi_ready) fx1_camera_csi_isr(&cam);
    return pdFALSE; /* the CLI task polls */
}

BaseType_t camera_isp_isr(void)
{
    fx1_camera_isp_isr(&cam);
    return pdFALSE;
}

void camera_service_init(void) { bsp_irq_register(FX1_IRQ_MIPI, csi_isr); }

/* ---- helpers ------------------------------------------------------------------- */

static const char* rc_name(int rc)
{
    switch (rc) {
    case FX1_CAMERA_OK: return "OK";
    case FX1_CAMERA_EINVAL: return "EINVAL";
    case FX1_CAMERA_ESTATE: return "ESTATE";
    case FX1_CAMERA_ETIMEOUT: return "ETIMEOUT";
    case FX1_CAMERA_EDEV: return "EDEV";
    case FX1_CAMERA_ESENSOR: return "ESENSOR";
    case FX1_CAMERA_EBUSY: return "EBUSY";
    default: return "?";
    }
}

static const char* state_name(int s)
{
    static const char* const n[] = {"idle", "configured", "running", "stopped, sensor not confirmed"};
    return s >= 0 && s <= 3 ? n[s] : "?";
}

static void note(const char* op, int rc)
{
    last_op = op;
    last_rc = rc;
}

static uint32_t stride16(uint32_t w) { return (w + 15u) & ~15u; }
static uint32_t line_length(void) { return mode.w + 160u > 2000u ? mode.w + 160u : 2000u; }
static uint32_t frame_length(void) { return mode.h + 284u; }

/* Expected frame period: the sensor's line timing, or the line payload on
 * the lanes when it is longer (the sensor then stretches its lines). */
static uint32_t frame_us(void)
{
    if (mode.tpg) return mode.w * mode.h / 50u + 1000u; /* ~250 MHz pixel path, generous */
    const uint32_t line_ns = line_length() * 1000u / (CAM_PIXEL_HZ / 1000000u);
    const uint32_t payload_ns = (mode.w * mode.bits / 8u + 6u) * 8u * CAM_UI_PS / 1000u / mode.lanes + 2000u;
    const uint32_t ns = line_ns > payload_ns ? line_ns : payload_ns;
    return (uint32_t)((uint64_t)ns * frame_length() / 1000u);
}

/* 12 / 10: sensor 64x32 tables (frames 1..32); 4: TPG 4K, 6: TPG 640x480
 * (one image each); 0: no golden. */
static int golden_case(void)
{
    if (!mode.tpg && mode.w == CAM_ISP_W && mode.h == CAM_ISP_H) return (int)mode.bits;
    if (mode.tpg && mode.w == CAM_ISP_TPG_W && mode.h == CAM_ISP_TPG_H && mode.bits == 12u) return 4;
    if (mode.tpg && mode.w == CAM_ISP_TPG640_W && mode.h == CAM_ISP_TPG640_H && mode.bits == 12u) return 6;
    return 0;
}

static int tpg_golden(int gold) { return gold == 4 || gold == 6; }

static uint32_t crc_table[256];
static uint32_t crc_bytes(uint32_t crc, uint32_t addr, uint32_t n)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256u; ++i) {
            uint32_t c = i;
            for (unsigned k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
            crc_table[i] = c;
        }
    const volatile uint32_t* p = (const volatile uint32_t*)(uintptr_t)addr;
    for (uint32_t i = 0; i < n / 4u; ++i) {
        const uint32_t w = p[i];
        for (unsigned b = 0; b < 4; ++b) crc = (crc >> 8) ^ crc_table[(crc ^ (w >> (8u * b))) & 0xFFu];
    }
    return crc;
}

static uint32_t nv12_crc(const fx1_camera_frame* f)
{
#ifdef CAMERA_TEST_FAST_CRC
    /* Test image (freertos_cli_camfault): a TPG 4K frame's CRC is taken as
     * the golden one, which takes minutes on the simulated CPU; the session
     * using it checks the ring's poisoning, not the CRC (computed for real at
     * 64x32 and 640x480 in the other sessions). */
    if (golden_case() == 4) return cam_isp_crc_tpg4k[0];
#endif
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t y = 0; y < f->height; ++y) crc = crc_bytes(crc, f->y + y * cfg.y_stride, f->width);
    for (uint32_t y = 0; y < f->height / 2u; ++y) crc = crc_bytes(crc, f->uv + y * cfg.uv_stride, f->width);
    return ~crc;
}

/* ---- NV12 spot check when the CRC is skipped (TPG 4K) ------------------------ */

static int spot_mode(void) { return golden_case() == 4; }

static volatile uint8_t* nv12_byte(uint32_t plane, uint32_t stride, const cam_isp_spot* p)
{
    return (volatile uint8_t*)(uintptr_t)(plane + (uint32_t)p->y * stride + p->x);
}

/* Bytes of Y and UV that differ from the golden. */
static void spot_check(const fx1_camera_frame* f, uint32_t* bad_y, uint32_t* bad_uv)
{
    *bad_y = *bad_uv = 0;
    for (uint32_t k = 0; k < 32u; ++k) {
        *bad_y += *nv12_byte(f->y, cfg.y_stride, &cam_isp_spot_tpg4k_y[k]) != cam_isp_spot_tpg4k_y[k].v;
        *bad_uv += *nv12_byte(f->uv, cfg.uv_stride, &cam_isp_spot_tpg4k_uv[k]) != cam_isp_spot_tpg4k_uv[k].v;
    }
}

/* Each sample position gets a value it must not have, until the ISP writes it. */
static void spot_poison(uint32_t y, uint32_t uv)
{
    for (uint32_t k = 0; k < 32u; ++k) {
        *nv12_byte(y, cfg.y_stride, &cam_isp_spot_tpg4k_y[k]) = (uint8_t)~cam_isp_spot_tpg4k_y[k].v;
        *nv12_byte(uv, cfg.uv_stride, &cam_isp_spot_tpg4k_uv[k]) = (uint8_t)~cam_isp_spot_tpg4k_uv[k].v;
    }
    fx1_dma_prepare((const volatile void*)(uintptr_t)y, 1, FX1_DMA_FROM_DEVICE); /* fence before the ISP */
}

/* ---- RAW spot check (capture hook, runs in poll before the hand-over) ------- */

static struct {
    uint32_t capture, frame, bad;
} raw_seen[8];

static uint32_t tpg_bar(uint32_t x, uint32_t y)
{
    static const uint8_t R[8] = {1, 1, 0, 0, 1, 1, 0, 0}, G[8] = {1, 1, 1, 1, 0, 0, 0, 0}, B[8] = {1, 0, 1, 0, 1, 0, 1, 0};
    const uint32_t wt = mode.w & ~3u;
    uint32_t b = wt >= 8u ? x / (wt >> 3) : 0u;
    if (b > 7u) b = 7u;
    const uint8_t on = (!(y & 1u) && !(x & 1u)) ? R[b] : ((y & 1u) && (x & 1u)) ? B[b] : G[b];
    return on ? (mode.bits == 12u ? 0x0FFFu : 0x03FFu) : 0u;
}

static void raw_hook(void* ctx, uint32_t raw16, uint32_t capture, uint16_t csi_frame_id)
{
    (void)ctx;
    (void)csi_frame_id;
    const volatile uint16_t* px = (const volatile uint16_t*)(uintptr_t)raw16;
    const uint32_t mask = (1u << mode.bits) - 1u;
    uint32_t f = 0, bad = 0;
    if (!mode.tpg) { /* pattern (x*37 + y*101 + f*53 + 5) & mask: f from pixel (0,0), modulo 2^bits */
        const uint32_t s0 = px[0];
        while (f <= mask && ((f * 53u + 5u) & mask) != s0) ++f;
    }
    for (uint32_t k = 0; k < 16u; ++k) {
        const uint32_t x = (k * (mode.w / 16u) + k) % mode.w, y = (k * 7u) % mode.h;
        const uint32_t want = mode.tpg ? tpg_bar(x, y) : ((x * 37u + y * 101u + f * 53u + 5u) & mask);
        bad += px[y * mode.w + x] != want;
    }
    raw_seen[capture & 7u].capture = capture;
    raw_seen[capture & 7u].frame = f;
    raw_seen[capture & 7u].bad = bad;
}

/* ---- commands -------------------------------------------------------------------- */

static void print_mode(void)
{
    out("camera mode: ");
    out_dec(mode.w);
    out("x");
    out_dec(mode.h);
    out(" RAW");
    out_dec(mode.bits);
    if (!mode.tpg) {
        out(", ");
        out_dec(mode.lanes);
        out(" lane(s)");
    }
    out(", source ");
    out(mode.tpg ? "tpg" : "sensor");
    out(", golden ");
    out_line(golden_case() ? "yes" : "no");
}

static int need_idle(const char* cmd)
{
    if (cam.state == 0) return 1;
    out(cmd);
    out(": camera is ");
    out(state_name(cam.state));
    out_line(" (camera stop first)");
    return 0;
}

static void probe(void)
{
    if (!need_idle("camera probe")) return;
    uint32_t ver = 0;
    probed = 0;
    if (fx1_probe_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_IP_VERSION, &ver)) {
        out_line("camera: absent (no CSI-2 RX: fx1_soc runs without --camera)");
        note("probe", FX1_CAMERA_EDEV);
        return;
    }
    csi_ready = 0;
    const int rc = fx1_camera_probe(&cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), mode.tpg);
    csi_ready = cam.csi.magic != 0; /* the CSI-2 part probed, whatever the sensor answered */
    note("probe", rc);
    out("camera: CSI-2 RX ip_version ");
    out_hex(ver);
    if (!mode.tpg) {
        uint16_t id = 0;
        const int ok = rc == FX1_CAMERA_OK && fx1_ccs_read16(&cam.sensor, 0x0000u, &id) == FX1_CCS_OK;
        out(", sensor model_id ");
        out_hex(ok ? id : 0u);
        out(" at I2C0 ");
        out_hex(FX1_CCS_ADDR);
    }
    out_line("");
    probed = rc == FX1_CAMERA_OK;
    report("camera probe", probed);
}

static int set_mode(int argc, char** argv)
{
    if (argc == 2) {
        print_mode();
        return 0;
    }
    if (argc != 6) return 1;
    uint32_t w, h, bits, lanes;
    if (!parse_u32(argv[2], &w) || !parse_u32(argv[3], &h) || !parse_u32(argv[4], &bits) || !parse_u32(argv[5], &lanes))
        return 1;
    if (!w || w > CAM_MAX_W || (w & 7u) || !h || h > CAM_MAX_H || (h & 1u) || (bits != 10u && bits != 12u) ||
        (lanes != 1u && lanes != 2u && lanes != 4u)) {
        out_line("camera mode: W 8..3840 (multiple of 8), H 2..2160 (even), BITS 10|12, LANES 1|2|4");
        return 0;
    }
    if (!need_idle("camera mode")) return 0;
    mode.w = w;
    mode.h = h;
    mode.bits = bits;
    mode.lanes = lanes;
    print_mode();
    if (!mode.tpg && w * h > 64u * 64u)
        out_line("  note: sensor frames go through the detailed D-PHY model: large frames take minutes of host time each (camera source tpg is fast)");
    return 0;
}

static int set_source(int argc, char** argv)
{
    if (argc == 3 && (!strcmp(argv[2], "sensor") || !strcmp(argv[2], "tpg"))) {
        if (!need_idle("camera source")) return 0;
        const int tpg = !strcmp(argv[2], "tpg");
        if (tpg != mode.tpg) probed = 0; /* probe again: the sensor is probed only for source sensor */
        mode.tpg = tpg;
    } else if (argc != 2) {
        return 1;
    }
    print_mode();
    if (!probed) out_line("  (camera probe before camera start)");
    return 0;
}

static void build_cfg(void)
{
    memset(&cfg, 0, sizeof cfg);
    cfg.width = mode.w;
    cfg.height = mode.h;
    cfg.bits = mode.bits;
    cfg.tpg = mode.tpg;
    cfg.lanes = mode.lanes;
    cfg.line_length = (uint16_t)line_length();
    cfg.frame_length = (uint16_t)frame_length();
    cfg.csi_side[0] = CAM_SIDE(0);
    cfg.csi_side[1] = CAM_SIDE(1);
    cfg.csi_side_size = CAM_SIDE_SIZE;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        cfg.staging[i] = CAM_STAGING(i);
        cfg.y[i] = CAM_Y(i);
        cfg.uv[i] = CAM_UV(i);
    }
    cfg.y_stride = cfg.uv_stride = stride16(mode.w);
    if (mode.tpg && golden_case() == 6) {
        cfg.profile = cam_isp_profile_tpg640;
        cfg.profile_len = sizeof cam_isp_profile_tpg640 / sizeof cam_isp_profile_tpg640[0];
    } else if (mode.tpg) {
        cfg.profile = cam_isp_profile_tpg4k;
        cfg.profile_len = sizeof cam_isp_profile_tpg4k / sizeof cam_isp_profile_tpg4k[0];
    } else if (mode.bits == 12u) {
        cfg.profile = cam_isp_profile_raw12;
        cfg.profile_len = sizeof cam_isp_profile_raw12 / sizeof cam_isp_profile_raw12[0];
    } else {
        cfg.profile = cam_isp_profile_raw10;
        cfg.profile_len = sizeof cam_isp_profile_raw10 / sizeof cam_isp_profile_raw10[0];
    }
    cfg.capture_hook = raw_hook;
}

/* Configure and start with the current mode; the ISP is claimed for the camera. */
static int start_stream(void)
{
    if (!isp_claim(ISP_OWNER_CAMERA)) {
        out_line("camera start: the ISP is in use by the isp commands");
        return FX1_CAMERA_ESTATE;
    }
    build_cfg();
    if (spot_mode())
        for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) spot_poison(cfg.y[i], cfg.uv[i]);
    int rc = fx1_camera_configure(&cam, &cfg);
#ifdef CAMERA_TEST_STREAM_ON_FAULT
    /* Test image (freertos_cli_camfault): the I2C host is lost once between
     * configure and stream-on (M6b review F1 regression). */
    static int stream_on_fault = 1;
    if (rc == FX1_CAMERA_OK && stream_on_fault && !mode.tpg) {
        stream_on_fault = 0;
        fx1_write32(FX1_I2C0_BASE + 0x10u, 0); /* I2C CTRL.enablehost = 0 */
        out_line("camera start: test fault: I2C host disabled before stream-on");
    }
#endif
    if (rc == FX1_CAMERA_OK) rc = fx1_camera_start(&cam);
    if (rc != FX1_CAMERA_OK && cam.state == 1) {
        /* Configured, the ISP started, the CSI-2 DMA maybe too: cleaned up
         * (M6b review F1); ESENSOR leaves state 3 (stop again later). */
        const int c = fx1_camera_stop(&cam, CAM_STOP_US);
        out("camera start: cleanup stop ");
        out(rc_name(c));
        out(", state ");
        out_line(state_name(cam.state));
    }
    if (cam.state == 0 || cam.state == 3) isp_release(ISP_OWNER_CAMERA); /* the ISP is stopped */
    return rc;
}

static void start(void)
{
    if (!probed) { /* an operator error, like the usage errors: not counted */
        out_line("camera start: camera probe first");
        note("start", FX1_CAMERA_ESTATE);
        return;
    }
    if (!need_idle("camera start")) return;
    const int rc = start_stream();
    note("start", rc);
    out("camera start: ");
    out_line(rc_name(rc));
    report("camera start", rc == FX1_CAMERA_OK);
}

typedef struct {
    int rc;
    fx1_camera_frame f;
    uint32_t crc, number, raw_frame, raw_bad, bad_y, bad_uv;
    int crc_done, raw_known, spot_done;
} cap_rec;
static cap_rec recs[CAM_MAX_FRAMES];

static int wait_frame(fx1_camera_frame* f, uint32_t timeout_us)
{
    const uint64_t t0 = fx1_mtime();
    for (;;) {
        if (fx1_camera_poll(&cam)) return fx1_camera_dequeue(&cam, f, 0);
        if (fx1_mtime() - t0 > (uint64_t)timeout_us * (FX1_MTIME_HZ / 1000000u)) {
            ++cam.stats.timeouts;
            return FX1_CAMERA_ETIMEOUT;
        }
        vTaskDelay(1);
    }
}

static int capture(int argc, char** argv)
{
    uint32_t n = 0;
    int force_crc = 0;
    if (argc < 3 || argc > 4 || !parse_u32(argv[2], &n) || !n || n > CAM_MAX_FRAMES) return 1;
    if (argc == 4) {
        if (strcmp(argv[3], "crc")) return 1;
        force_crc = 1;
    }
    if (cam.state != 2) {
        out_line("camera capture: not running (camera start)");
        return 0;
    }
    const int gold = golden_case();
    const uint32_t bytes = mode.w * mode.h * 3u / 2u;
    const int do_crc = force_crc || bytes <= CAM_CRC_LIMIT;
    const uint32_t timeout = 4u * frame_us() + 50000u;
    const uint32_t drops0 = fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT);
    uint32_t got = 0;
    for (; got < n; ++got) { /* collect first, print afterwards */
        cap_rec* r = &recs[got];
        memset(r, 0, sizeof *r);
        if (mode.tpg && (r->rc = fx1_camera_tpg_frame(&cam)) != FX1_CAMERA_OK) break;
        r->rc = wait_frame(&r->f, timeout);
        if (r->rc != FX1_CAMERA_OK) break;
        const uint32_t slot = r->f.capture & 7u;
        r->raw_known = raw_seen[slot].capture == r->f.capture;
        r->raw_frame = raw_seen[slot].frame;
        r->raw_bad = raw_seen[slot].bad;
        if (!do_crc && spot_mode()) {
            spot_check(&r->f, &r->bad_y, &r->bad_uv);
            r->spot_done = 1;
        }
        if (do_crc) {
            r->crc = nv12_crc(&r->f);
            r->crc_done = 1;
            if (tpg_golden(gold)) r->number = r->crc == (gold == 4 ? cam_isp_crc_tpg4k : cam_isp_crc_tpg640)[0] ? 1u : 0u;
            else if (gold)
                for (uint32_t k = 0; k < CAM_ISP_FRAMES; ++k)
                    if ((gold == 12 ? cam_isp_crc_raw12 : cam_isp_crc_raw10)[k] == r->crc) r->number = k + 1u;
        }
        /* Every frame of a spot-checked mode, CRC or not: the slot goes back
         * to the ring poisoned (M6b re-review F2). */
        if (spot_mode()) spot_poison(r->f.y, r->f.uv);
        fx1_camera_release(&cam, &r->f);
    }
    const uint32_t drops = fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT) - drops0;
    int ok = got == n;
    uint32_t prev = 0;
    for (uint32_t k = 0; k < got; ++k) {
        const cap_rec* r = &recs[k];
        out("  capture ");
        out_dec(r->f.capture);
        out(" FRAME_ID ");
        out_dec(r->f.csi_frame_id);
        out(" side ");
        out_dec(r->f.csi_side);
        out(" isp seq ");
        out_dec(r->f.isp_seq);
        out(r->f.zero_copy ? " zero-copy" : " staged");
        out(" out ");
        out_dec(r->f.isp_output);
        out(" ");
        out_dec(r->f.width);
        out("x");
        out_dec(r->f.height);
        if (!r->raw_known) {
            out(" raw ?");
            ok = 0;
        } else {
            if (!mode.tpg) {
                out(" raw frame ");
                out_dec(r->raw_frame);
            }
            out(r->raw_bad ? " raw BAD" : " raw ok");
            if (r->raw_bad) ok = 0;
        }
        if (!r->crc_done) {
            out(" crc skipped (size)");
            if (!r->spot_done) {
                out(", NV12 not checked (no golden)");
            } else {
                out(": full golden not compared, Y spot ");
                out(r->bad_y ? "BAD" : "ok");
                out(" UV spot ");
                out(r->bad_uv ? "BAD" : "ok");
                out(" (64 bytes)");
                if (r->bad_y || r->bad_uv) ok = 0;
            }
        } else {
            out(" crc ");
            out_hex(r->crc);
            /* "no golden" for a pattern frame beyond the table; a frame whose
             * RAW is not the pattern is compared anyway (and fails). */
            if (!gold || (!tpg_golden(gold) && r->raw_known && !r->raw_bad && r->raw_frame > CAM_ISP_FRAMES)) {
                out(" no golden");
            } else if (r->number && (tpg_golden(gold) || r->number == r->raw_frame)) {
                out(tpg_golden(gold) ? " = golden" : " = golden frame ");
                if (!tpg_golden(gold)) out_dec(r->number);
                if (!tpg_golden(gold) && r->number <= prev) ok = 0; /* frames in order */
                prev = r->number;
            } else {
                out(" GOLDEN MISMATCH");
                ok = 0;
            }
        }
        out_line("");
    }
    if (got < n) {
        out("  frame ");
        out_dec(got + 1u);
        out(": ");
        out_line(rc_name(recs[got].rc));
    }
    note("capture", got == n ? FX1_CAMERA_OK : recs[got].rc);
    out("camera capture: ");
    out_dec(got);
    out("/");
    out_dec(n);
    out(" frames, drops ");
    out_dec(drops);
    out_line("");
    report("camera capture", ok);
    return 0;
}

static void status(void)
{
    const fx1_camera_stats* s = &cam.stats;
    out("camera: ");
    out(state_name(cam.state));
    out(", last ");
    out(last_op);
    out(" ");
    out_line(rc_name(last_rc));
    print_mode();
    out("  csi ok ");
    out_dec(s->csi_ok);
    out(" err ");
    out_dec(s->csi_err);
    out(" irqs ");
    out_dec(s->csi_irqs);
    out(", isp queued ");
    out_dec(s->isp_queued);
    out(" done ");
    out_dec(s->isp_done);
    out(" lost ");
    out_dec(s->isp_lost);
    out(" errors ");
    out_dec(s->isp_errors);
    out(" irqs ");
    out_dec(s->isp_irqs);
    out_line("");
    out("  held for isp ");
    out_dec(s->held_for_isp);
    out(", timeouts ");
    out_dec(s->timeouts);
    out(", stop cancelled ");
    out_dec(s->stop_cancelled);
    out(", sensor errors ");
    out_dec(s->sensor_errors);
    if (probed) {
        out(", drops ");
        out_dec(fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT));
        uint8_t fc = 0;
        if (!mode.tpg && fx1_ccs_frame_count(&cam.sensor, &fc) == FX1_CCS_OK) {
            out(", sensor frame_count ");
            out_dec(fc);
        }
    }
    out_line("");
}

static int stop_stream(uint32_t us)
{
    const int rc = fx1_camera_stop(&cam, us);
    if (cam.state == 0 || cam.state == 3) isp_release(ISP_OWNER_CAMERA); /* the ISP is stopped */
    return rc;
}

static int stop(int argc, char** argv)
{
    uint32_t us = CAM_STOP_US;
    if (argc > 3 || (argc == 3 && !parse_u32(argv[2], &us))) return 1;
    if (cam.state != 1 && cam.state != 2 && cam.state != 3) {
        out_line("camera stop: not running");
        return 0;
    }
    const uint64_t t0 = fx1_mtime();
    const int rc = stop_stream(us);
    const uint32_t el = (uint32_t)((fx1_mtime() - t0) / (FX1_MTIME_HZ / 1000000u)); /* the call only, not the print */
    note("stop", rc);
    out("camera stop: ");
    out(rc_name(rc));
    out(" in ");
    out_dec(el);
    out(" us, state ");
    out_line(state_name(cam.state));
    if (rc == FX1_CAMERA_ESENSOR) out_line("  the sensor did not confirm standby: camera stop again once I2C answers");
    if (rc == FX1_CAMERA_EBUSY) out_line("  the ISP is still draining its reset: camera stop again (buffers kept)");
    return 0;
}

static void recover(void)
{
    int rc = FX1_CAMERA_OK;
    const char* step = "stop";
    if (!probed) {
        rc = FX1_CAMERA_ESTATE;
        step = "probe";
    }
    if (rc == FX1_CAMERA_OK && cam.state != 0) rc = stop_stream(CAM_STOP_US);
    if (rc == FX1_CAMERA_OK && !mode.tpg) {
        step = "sensor soft reset";
        rc = fx1_ccs_soft_reset(&cam.sensor) == FX1_CCS_OK ? FX1_CAMERA_OK : FX1_CAMERA_ESENSOR;
    }
    if (rc == FX1_CAMERA_OK) {
        step = "configure and start";
        rc = start_stream();
    }
    note("recover", rc);
    out("camera recover: ");
    if (rc != FX1_CAMERA_OK) {
        out(step);
        out(" -> ");
    }
    out(rc_name(rc));
    out(", state ");
    out_line(state_name(cam.state));
    report("camera recover", rc == FX1_CAMERA_OK);
}

int cmd_camera(int argc, char** argv)
{
    if (argc < 2) return 1;
    if (!strcmp(argv[1], "probe") && argc == 2) probe();
    else if (!strcmp(argv[1], "mode")) return set_mode(argc, argv);
    else if (!strcmp(argv[1], "source")) return set_source(argc, argv);
    else if (!strcmp(argv[1], "start") && argc == 2) start();
    else if (!strcmp(argv[1], "capture")) return capture(argc, argv);
    else if (!strcmp(argv[1], "status") && argc == 2) status();
    else if (!strcmp(argv[1], "stop")) return stop(argc, argv);
    else if (!strcmp(argv[1], "recover") && argc == 2) recover();
    else return 1;
    return 0;
}
