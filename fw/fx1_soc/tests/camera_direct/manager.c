/*
 * Direct CSI-2 -> ISP through the camera manager (fw/fx1_soc/drivers/fx1_camera,
 * cfg.path = FX1_CAMERA_PATH_DIRECT), plan DIRECT_CSI_ISP D3 (D3.1 / D3.3).
 * fx1_soc --camera, sensor 64x32 through the detailed D-PHY (RAW12 4 lanes /
 * RAW10 2 lanes, golden of camera_isp_golden.h) or the CSI-2 TPG 640x480;
 * interrupts on (cam_isp_irqs: CSI-2 and ISP ISRs to the manager). The sensor
 * frame period is 8 ms (frame_length 800): the NV12 CRC of a frame (~3.5 ms of
 * simulated CPU time) ends before the next one.
 *
 * Every published frame: path direct, csi_side FX1_CAMERA_NO_SIDE, zero_copy
 * 0, isp_in 0 (no RAW side), capture = low bits of the capture sequence,
 * strictly increasing, FS timestamp increasing, ISP frame ID set, NV12 equal
 * to the stand-alone model's golden for its sensor frame; output released.
 *
 *   CAM_CASE=30  nominal: N frames consecutive (sensor frames and FRAME_IDs),
 *                stats direct_ok = N, nothing failed; stop OK, state 0, no CSI-2
 *                soft reset
 *   CAM_CASE=31  path switch: memory (3 frames, zero-copy RAW12) -> stop ->
 *                direct (3) -> stop -> memory (3), each exact; the ISP depth and
 *                ingress source follow the path; the CSI RAW DMA writes only in
 *                the memory phases (host: no traffic check, the memory path reads)
 *   CAM_CASE=32  held outputs: the application keeps all four outputs for 3
 *                frame periods: the ingress drops frames without output credit
 *                (NO_CREDIT, the sensor is never paused), nothing published in
 *                between; stop while held: OK within the deadline + slack, the
 *                held NV12 stay exact, release after stop OK (once); configure
 *                and start again: consecutive exact frames
 *   CAM_CASE=33  TPG 640x480 RAW12 direct (golden tpg640), 2 frames
 *   CAM_CASE=34  ODMA write error (fx1_soc --isp-dma-fault odma-error:106, in
 *                the 3rd frame): that frame ISP_FAILED, not published
 *                (direct_failed[6]), the ISP recovers, the manager arms the
 *                outputs again, the next frames are published exact
 *   CAM_CASE=36  sensor faults (fx1_soc --camera-fault 3:crc:10 --camera-fault
 *                5:no-fe): frames 3 (CSI_DROPPED / INVALID) and 5 (CSI_MISSING_FE,
 *                every line delivered, its output completed) are not published;
 *                the published frames are sensor frames 1, 2, 4, 6, 7, exact
 *   CAM_CASE=37  the sensor stops streaming while running (stream-off written
 *                behind the manager's back): the frames in flight come out, then
 *                dequeue returns FX1_CAMERA_ETIMEOUT within its budget (counted);
 *                stop OK within the deadline; configure and start again, exact
 *   CAM_CASE=38  start fails after configure (I2C host disabled before the
 *                stream-on): FX1_CAMERA_EDEV, state 1 (ISP started, ingress
 *                direct); I2C back: stop cleans up (OK, state 0, SOURCE memory,
 *                ISP off, stream path off, sensor not streaming); configure and
 *                start again, exact
 *   CAM_CASE=39  stop with a capture left without terminal (review D3 F1;
 *                fx1_soc --camera-fault 1:no-fe --camera-fault 5:no-fe): frame
 *                1 has every line, its output DONE, no FE; the sensor stops
 *                (stream-off behind the manager's back) before the next FS, so
 *                its entry waits for a CSI terminal that never comes. stop: OK
 *                (EBUSY retried), one CSI-2 soft reset, nothing published, SOURCE
 *                memory, context table empty; never state 0 / 3 before that;
 *                configure and start again: sensor frames 2-4 exact. Frame 5 the
 *                same with stop(0): no time for the standby, ESENSOR, state 3
 *                only with the pipeline stopped (inside the slack); stop again
 *                OK; configure and start again: frames 6-8 exact
 *   CAM_CASE=40  ODMA error with older outputs DONE and not polled (review D3
 *                F2; fx1_soc --isp-dma-fault odma-error:106, the 3rd frame):
 *                frames 1 and 2 published after the recovery and held; one
 *                released, the ODMA claims it again before the next poll: the
 *                poll does not arm it again (FREE stays clear) and the frame
 *                written there is published; the rotation then waits at the
 *                other held slot (contract §8: the ODMA rotation does not skip a
 *                slot): frames dropped without credit, nothing published, the
 *                held NV12 unchanged, its slot not armed; released: 5 frames
 *                exact through every slot, TAGs increasing
 *   CAM_CASE=41  stop pending (re-review D3 R1; TPG 640x480 as case 33): a
 *                test-only ingress HAL wrapper drops SOURCE=memory writes (the
 *                product driver and model unchanged; readback from the IP), so
 *                the cleanup cannot finish. Configured: stop EBUSY, state 1,
 *                cleanup pending, start / configure refused; without the fault
 *                stop OK. Running, one output held: stop EBUSY within the
 *                deadline + slack, state 2 (never 0 / 3), SOURCE still direct;
 *                while pending the TPG frame, start and configure are refused
 *                (ESTATE), no CSI-2 frame starts, the ingress admits / drops
 *                nothing, the ISP stays stopped, poll publishes nothing,
 *                dequeue times out, the release arms nothing (FREE unchanged);
 *                stop again still EBUSY (no CSI-2 reset: nothing left in the
 *                table); without the fault stop OK, SOURCE memory, table empty;
 *                configure and start again: a frame exact
 *   CAM_CASE=35  stop with the I2C host disabled: ESENSOR within the deadline,
 *                state 3, ISP and ingress stopped (SOURCE memory), configure
 *                refused; I2C back: stop OK; configure and start again, exact
 *
 * The host checks the traffic except for case 31: ISP_IDMA 0, MIPI_DMA 0.
 * Every dequeued frame also checks the output ownership (review D3 F2): a slot
 * the application holds is neither armed (FREE) nor marked unarmed.
 */
#include <stdint.h>

#include "cam_isp.h"
#include "fx1_ccs_sensor.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"
#include "fx1_fw.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_ingress_drv.h"
#include "fx1_isp_hal_mmio.h"

#ifndef CAM_BITS
#define CAM_BITS 12
#endif
#ifndef CAM_CASE
#define CAM_CASE 30
#endif
#ifndef CAM_FRAMES
#define CAM_FRAMES 5u
#endif

#define FRAME_LENGTH 800u
#define FRAME_US (FRAME_LENGTH * 10u)
#define STOP_US 24000u
#define CAM_TPG (CAM_CASE == 33 || CAM_CASE == 41)

#if CAM_TPG
#define W CAM_ISP_TPG640_W
#define H CAM_ISP_TPG640_H
#define Y_BUF(i) (FX1_FW_YUV_BUF_BASE + (i) * 0x80000u)
#define UV_BUF(i) (Y_BUF(i) + 0x50000u)
#else
#define W CAM_ISP_W
#define H CAM_ISP_H
#define Y_BUF(i) CAM_ISP_Y(i)
#define UV_BUF(i) CAM_ISP_UV(i)
#endif

static uint64_t ticks(uint32_t us) { return (uint64_t)us * (FX1_MTIME_HZ / 1000000u); }
static uint32_t us_since(uint64_t t) { return (uint32_t)((fx1_mtime() - t) / ticks(1)); }
__attribute__((unused)) static void wait_us(uint32_t us)
{
    const uint64_t end = fx1_mtime() + ticks(us);
    while (fx1_mtime() < end) (void)fx1_camera_poll(&g_cam);
}

static fx1_camera_cfg direct_cfg(void)
{
#if CAM_TPG
    fx1_camera_cfg c;
    for (unsigned i = 0; i < sizeof c; ++i) ((uint8_t*)&c)[i] = 0;
    c.width = W;
    c.height = H;
    c.bits = 12;
    c.tpg = 1;
    c.profile = cam_isp_profile_tpg640;
    c.profile_len = sizeof cam_isp_profile_tpg640 / sizeof cam_isp_profile_tpg640[0];
#else
    fx1_camera_cfg c = cam_isp_cfg(CAM_BITS);
    c.frame_length = FRAME_LENGTH;
#endif
    c.path = FX1_CAMERA_PATH_DIRECT;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        c.y[i] = Y_BUF(i);
        c.uv[i] = UV_BUF(i);
    }
    c.y_stride = c.uv_stride = W;
    /* No RAW side, no staging on the direct path. */
    c.csi_side[0] = c.csi_side[1] = 0;
    c.csi_side_size = 0;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) c.staging[i] = 0;
    return c;
}

/* Slots the application holds: not given to the ODMA, not to be armed. */
static void ownership_ok(uint32_t code)
{
    const uint32_t free_ = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_FREE_OFFSET);
    for (unsigned k = 0; k < FX1_ISP_NUM_BUFFERS; ++k) {
        if (!g_cam.out_owned[k]) continue;
        if ((free_ | g_cam.unarmed) & (1u << k)) {
            fx1_log_hex("  held slot", k);
            fx1_log_hex("  ODMA FREE", free_);
            fx1_log_hex("  unarmed", g_cam.unarmed);
        }
        fx1_check(!((free_ | g_cam.unarmed) & (1u << k)), code, "manager: a held output is not armed");
    }
}

static uint64_t g_last_seq, g_last_ts;
static uint32_t g_last_n;
static uint16_t g_last_fid;

/* One direct frame: metadata, NV12 against the golden; returns the sensor frame
 * number (1 for TPG). The output is released unless `hold`. */
static uint32_t take(fx1_camera_frame* f, int hold, uint32_t code)
{
    const int rc = fx1_camera_dequeue(&g_cam, f, 4u * FRAME_US + (CAM_TPG ? 400000u : 0u));
    if (rc != FX1_CAMERA_OK) {
        fx1_isp_ingress_counters k;
        fx1_isp_ingress_read_counters(&g_cam.ing, &k);
        fx1_log_hex("  dequeue rc", (uint32_t)rc);
        fx1_log_hex("  ODMA FREE", fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_FREE_OFFSET));
        fx1_log_hex("  ODMA DONE", fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_DONE_OFFSET));
        fx1_log_hex("  ODMA FRAME_COUNT", fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_FRAME_COUNT_OFFSET));
        fx1_log_hex("  ingress CTX_COUNT", fx1_read32(FX1_ISP_INGRESS_BASE + FX1_ISP_INGRESS_CTX_COUNT_OFFSET));
        fx1_log_hex("  ingress admitted", k.frames_admitted);
        fx1_log_hex("  ingress no credit", k.drop_no_credit);
        fx1_log_hex("  ingress invalid", k.frames_invalid);
        fx1_log_hex("  unarmed", g_cam.unarmed);
        fx1_log_hex("  direct ok", g_cam.stats.direct_ok);
        for (unsigned o = 1; o < FX1_CAMERA_OUTCOMES; ++o)
            if (g_cam.stats.direct_failed[o]) fx1_log_hex("  failed outcome", o);
    }
    fx1_check(rc == FX1_CAMERA_OK, code, "manager: frame dequeued");
    ownership_ok(code | 9u);
    fx1_check(f->path == FX1_CAMERA_PATH_DIRECT && f->csi_side == FX1_CAMERA_NO_SIDE && !f->zero_copy && !f->isp_in,
              code | 1u, "manager: direct frame, RAW side / IDMA input N/A");
    fx1_check(f->capture == (uint32_t)f->capture_seq && f->capture_seq > g_last_seq, code | 2u,
              "manager: capture sequence increases");
    fx1_check(!g_last_ts || f->ts_ns > g_last_ts, code | 3u, "manager: FS timestamp increases");
    fx1_check(f->isp_frame != 0 && f->width == W && f->height == H, code | 4u, "manager: ISP frame ID, geometry");
    fx1_check(fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_TS0_OFFSET + 4u * f->isp_output) == f->csi_frame_id &&
                  fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_TAG0_OFFSET + 4u * f->isp_output) == f->isp_seq,
              code | 5u, "manager: output BUF_TS = FRAME_ID, BUF_TAG = context TAG");
    g_last_seq = f->capture_seq;
    g_last_ts = f->ts_ns;
    const uint32_t crc = cam_isp_crc(f, W, W);
#if CAM_TPG
    const uint32_t n = crc == cam_isp_crc_tpg640[0] ? 1u : 0u;
#else
    const uint32_t n = cam_isp_frame_of(CAM_BITS, crc);
#endif
    if (!n) fx1_log_hex("  NV12 CRC", crc);
    fx1_check(n != 0, code | 6u, "manager: NV12 equals the ISP model's");
    if (!hold) fx1_check(fx1_camera_release(&g_cam, f) == FX1_CAMERA_OK, code | 7u, "manager: output released");
    return n;
}

/* Consecutive frames: sensor frame n + 1, FRAME_ID + 1. */
__attribute__((unused)) static void take_consecutive(unsigned count, uint32_t code)
{
    for (unsigned i = 0; i < count; ++i) {
        fx1_camera_frame f;
        const uint32_t n = take(&f, 0, code);
#if !CAM_TPG
        if (g_last_n)
            fx1_check(n == g_last_n + 1u && f.csi_frame_id == ((g_last_fid + 1u) & 0x3FFFu), code | 8u,
                      "manager: consecutive sensor frames and FRAME_IDs");
#endif
        g_last_n = n;
        g_last_fid = f.csi_frame_id;
    }
}

static void bring_up(const fx1_camera_cfg* c, uint32_t code)
{
    const int rc = fx1_camera_configure(&g_cam, c);
    if (rc != FX1_CAMERA_OK) {
        fx1_log_hex("  configure rc", (uint32_t)rc);
        fx1_log_hex("  ISP COMMON_STATUS", fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_COMMON_STATUS_OFFSET));
        fx1_log_hex("  ISP COMMON_CTRL", fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_COMMON_CTRL_OFFSET));
        fx1_log_hex("  ingress STATUS", fx1_isp_ingress_status(&g_cam.ing));
        fx1_log_hex("  ingress CTX_COUNT", fx1_read32(FX1_ISP_INGRESS_BASE + FX1_ISP_INGRESS_CTX_COUNT_OFFSET));
        fx1_log_hex("  isp state", (uint32_t)g_cam.isp.state);
    }
    fx1_check(rc == FX1_CAMERA_OK, code, "manager: configured");
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_OK, code | 1u, "manager: started");
#if CAM_TPG
    fx1_check(fx1_camera_tpg_frame(&g_cam) == FX1_CAMERA_OK, code | 2u, "manager: TPG frame");
#endif
    g_last_n = 0;
}

__attribute__((unused)) static void stop_ok(uint32_t code)
{
    const uint64_t t = fx1_mtime();
    const int rc = fx1_camera_stop(&g_cam, STOP_US);
    const uint32_t el = us_since(t);
    fx1_log_hex("  stop rc", (uint32_t)rc);
    fx1_log_hex("  stop us", el);
    fx1_check(rc == FX1_CAMERA_OK && g_cam.state == 0 && el <= STOP_US + FX1_CAMERA_STOP_SLACK_US, code,
              "manager: stop OK within the deadline");
    fx1_check(fx1_isp_ingress_source(&g_cam.ing) == FX1_ISP_INGRESS_SOURCE_MEMORY, code | 1u,
              "manager: ingress back to SOURCE memory");
    fx1_check(!(fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_STREAM_CONFIG) & FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK),
              code | 2u, "manager: CSI-2 stream path off after the stop");
}

/* Cases 39: the next frame (FRAME_COUNT past `base`) reaches the ODMA, the ISP
 * idle, its entry not READY (no FE); the sensor stops before the next FS. */
__attribute__((unused)) static void no_fe_then_sensor_off(uint32_t base, uint32_t code)
{
    const uint64_t end = fx1_mtime() + ticks(2u * FRAME_US);
    while ((fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_FRAME_COUNT_OFFSET) <= base ||
            (fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK)) &&
           fx1_mtime() < end) {
    }
    const uint32_t done = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_DONE_OFFSET);
    fx1_log_hex("  ODMA DONE", done);
    fx1_check(fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == base + 1u && done, code,
              "manager: the no-FE frame reached the ODMA (output DONE)");
    fx1_check(fx1_isp_ingress_ctx_ready(&g_cam.ing) == 0 && fx1_isp_ingress_ctx_used(&g_cam.ing) == 1u, code | 1u,
              "manager: its entry waits for the CSI terminal");
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 0) == FX1_CCS_OK, code | 2u, "manager: the sensor stops before the next FS");
}

/* Stop until it is no longer EBUSY: never idle (0 / 3) with the ingress not
 * drained; returns the last rc. */
__attribute__((unused)) static int stop_drained(uint32_t us, uint32_t* el_max, uint32_t code)
{
    int rc = FX1_CAMERA_EBUSY;
    for (unsigned i = 0; i < 4u && rc == FX1_CAMERA_EBUSY; ++i) {
        const uint64_t t = fx1_mtime();
        rc = fx1_camera_stop(&g_cam, us);
        const uint32_t el = us_since(t);
        if (el > *el_max) *el_max = el;
        fx1_log_hex("  stop rc", (uint32_t)rc);
        fx1_log_hex("  stop us", el);
        if (g_cam.state == 0 || g_cam.state == 3)
            fx1_check(fx1_isp_ingress_source(&g_cam.ing) == FX1_ISP_INGRESS_SOURCE_MEMORY &&
                          fx1_isp_ingress_ctx_used(&g_cam.ing) == 0,
                      code, "manager: stopped only with SOURCE memory and no context entry");
        else
            fx1_check(rc == FX1_CAMERA_EBUSY, code | 1u, "manager: not stopped: EBUSY");
    }
    return rc;
}

__attribute__((unused)) static void counters(fx1_isp_ingress_counters* k)
{
    fx1_isp_ingress_read_counters(&g_cam.ing, k);
    fx1_log_hex("  frames admitted", k->frames_admitted);
    fx1_log_hex("  dropped no credit", k->drop_no_credit);
    fx1_log_hex("  frames invalid", k->frames_invalid);
}

/* Case 41: test-only fault on the ingress HAL; every other access and every
 * read goes to the IP. */
#if CAM_CASE == 41
static fx1_isp_hal g_ing_hal;
static int g_refuse_memory;
static unsigned g_refused;
static uint32_t gate_read(void* ctx, uint32_t off)
{
    (void)ctx;
    return g_ing_hal.read32(g_ing_hal.ctx, off);
}
static void gate_write(void* ctx, uint32_t off, uint32_t v)
{
    (void)ctx;
    if (g_refuse_memory && off == FX1_ISP_INGRESS_CTRL_OFFSET && !(v & FX1_ISP_INGRESS_CTRL_SOURCE_MASK)) {
        ++g_refused; /* SOURCE = memory not accepted */
        return;
    }
    g_ing_hal.write32(g_ing_hal.ctx, off, v);
}
static void gate(int refuse)
{
    if (!g_ing_hal.write32) g_ing_hal = g_cam.ing.hal;
    g_cam.ing.hal.read32 = gate_read;
    g_cam.ing.hal.write32 = gate_write;
    g_refuse_memory = refuse;
}
static void ungate(void)
{
    g_refuse_memory = 0;
    g_cam.ing.hal = g_ing_hal;
}

/* Nothing may move while the stop is pending. */
typedef struct {
    uint32_t csi_frames, odma_frames, odma_free, unarmed, direct_ok, resets;
    int isp_state;
    fx1_isp_ingress_counters k;
} pending_snap;
static void snap(pending_snap* p)
{
    p->csi_frames = fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_COUNT);
    p->odma_frames = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
    p->odma_free = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_FREE_OFFSET);
    p->unarmed = g_cam.unarmed;
    p->direct_ok = g_cam.stats.direct_ok;
    p->resets = g_cam.stats.stop_csi_resets;
    p->isp_state = g_cam.isp.state;
    fx1_isp_ingress_read_counters(&g_cam.ing, &p->k);
}
static void pending_ok(int state, uint32_t code)
{
    fx1_check(g_cam.state == state && g_cam.stop_cleanup_pending && !g_cam.isp_stop_pending, code,
              "manager: stop pending, state kept");
    fx1_check(fx1_isp_ingress_source(&g_cam.ing) == FX1_ISP_INGRESS_SOURCE_DIRECT && g_refused != 0, code | 1u,
              "manager: SOURCE still direct (memory refused by the fixture)");
    fx1_check(!(fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK), code | 2u,
              "manager: the ISP is stopped");
}
static void stop_completes(uint32_t code)
{
    stop_ok(code);
    fx1_check(fx1_isp_ingress_ctx_used(&g_cam.ing) == 0 && !g_cam.stop_cleanup_pending, code | 3u,
              "manager: context table empty, nothing pending");
}
#endif

int main(void)
{
    fx1_log("camera manager, direct path");
    cam_isp_irqs();
    fx1_check(fx1_camera_probe(&g_cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), CAM_TPG) ==
                  FX1_CAMERA_OK,
              0x10, "manager: probe");
    const fx1_camera_cfg dcfg = direct_cfg();
#if CAM_CASE == 30 || CAM_CASE == 33
    bring_up(&dcfg, 0x20);
#if CAM_CASE == 33
    for (unsigned i = 0; i < 2u; ++i) {
        fx1_camera_frame f;
        take(&f, 0, 0x30);
        if (i == 0) fx1_check(fx1_camera_tpg_frame(&g_cam) == FX1_CAMERA_OK, 0x31, "manager: TPG frame 2");
    }
    const unsigned want = 2u;
#else
    take_consecutive(CAM_FRAMES, 0x30);
    const unsigned want = CAM_FRAMES;
#endif
    fx1_check(g_cam.stats.direct_ok >= want && g_cam.stats.direct_no_output == 0, 0x40, "manager: frames published");
    for (unsigned o = 1; o < FX1_CAMERA_OUTCOMES; ++o)
        if (g_cam.stats.direct_failed[o]) fx1_log_hex("  failed outcome", o);
    stop_ok(0x50);
    unsigned failed = 0;
    for (unsigned o = 0; o < FX1_CAMERA_OUTCOMES; ++o) failed += g_cam.stats.direct_failed[o];
    fx1_check(failed == 0, 0x52, "manager: nothing failed");
    fx1_check(g_cam.stats.stop_csi_resets == 0, 0x53, "manager: a normal stop resets no CSI-2 (P35)");
#elif CAM_CASE == 31
    fx1_camera_cfg mcfg = cam_isp_cfg(12);
    mcfg.frame_length = FRAME_LENGTH;
    for (unsigned phase = 0; phase < 3u; ++phase) {
        if (phase == 1) {
            bring_up(&dcfg, 0x20 + 0x100u * phase);
            take_consecutive(3, 0x30 + 0x100u * phase);
            fx1_check(fx1_isp_ingress_source(&g_cam.ing) == FX1_ISP_INGRESS_SOURCE_DIRECT, 0x3A + 0x100u * phase,
                      "manager: SOURCE direct while running direct");
            stop_ok(0x50 + 0x100u * phase);
        } else {
            fx1_check(fx1_camera_configure(&g_cam, &mcfg) == FX1_CAMERA_OK, 0x20 + 0x100u * phase, "manager: memory configured");
            fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_OK, 0x21 + 0x100u * phase, "manager: memory started");
            if (phase == 2)
                fx1_check(fx1_isp_ingress_source(&g_cam.ing) == FX1_ISP_INGRESS_SOURCE_MEMORY, 0x2A + 0x100u * phase,
                          "manager: SOURCE memory on the memory path");
            uint32_t prev = 0;
            for (unsigned i = 0; i < 3u; ++i) {
                const uint32_t n = cam_isp_take(12, 0x30 + 0x100u * phase);
                fx1_check(!prev || n > prev, 0x38 + 0x100u * phase, "manager: memory frames in order");
                prev = n;
            }
            fx1_check(fx1_camera_stop(&g_cam, STOP_US) == FX1_CAMERA_OK && g_cam.state == 0, 0x50 + 0x100u * phase,
                      "manager: memory stopped");
        }
    }
    fx1_check(g_cam.stats.direct_ok >= 3u, 0x60, "manager: direct frames counted");
#elif CAM_CASE == 32
    bring_up(&dcfg, 0x20);
    take_consecutive(2, 0x30);
    fx1_camera_frame held[FX1_ISP_NUM_BUFFERS];
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        take(&held[i], 1, 0x40 + 0x10u * i);
        fx1_check(held[i].isp_output < FX1_ISP_NUM_BUFFERS, 0x48, "manager: held output");
    }
    const uint32_t ok0 = g_cam.stats.direct_ok;
    fx1_isp_ingress_counters k0;
    counters(&k0);
    wait_us(3u * FRAME_US);
    fx1_isp_ingress_counters k1;
    counters(&k1);
    fx1_check(k1.drop_no_credit >= k0.drop_no_credit + 2u, 0x80, "manager: frames dropped without output credit");
    fx1_check(g_cam.stats.direct_ok == ok0 && g_cam.ready_count == 0, 0x81, "manager: nothing published while held");
    stop_ok(0x90);
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        fx1_dma_complete((const volatile void*)(uintptr_t)held[i].y, W * H, FX1_DMA_FROM_DEVICE);
        fx1_dma_complete((const volatile void*)(uintptr_t)held[i].uv, W * H / 2u, FX1_DMA_FROM_DEVICE);
        fx1_check(cam_isp_frame_of(CAM_BITS, cam_isp_crc(&held[i], W, W)) != 0, 0xA0, "manager: held NV12 still exact");
        fx1_check(fx1_camera_release(&g_cam, &held[i]) == FX1_CAMERA_OK, 0xA1, "manager: release after stop OK");
        fx1_check(fx1_camera_release(&g_cam, &held[i]) == FX1_CAMERA_EINVAL, 0xA2, "manager: a second release refused");
    }
    bring_up(&dcfg, 0xB0);
    take_consecutive(3, 0xC0);
    stop_ok(0xD0);
#elif CAM_CASE == 34
    bring_up(&dcfg, 0x20);
    unsigned published = 0;
    for (unsigned k = 0; published < 5u && k < 12u; ++k) {
        fx1_camera_frame f;
        take(&f, 0, 0x30);
        ++published;
    }
    fx1_log_hex("  isp errors", g_cam.stats.isp_errors);
    fx1_log_hex("  ISP_FAILED", g_cam.stats.direct_failed[FX1_ISP_INGRESS_OUTCOME_ISP_FAILED]);
    fx1_check(g_cam.stats.isp_errors >= 1u && g_cam.stats.direct_failed[FX1_ISP_INGRESS_OUTCOME_ISP_FAILED] >= 1u, 0x40,
              "manager: the ODMA error frame ISP_FAILED, not published");
    fx1_check(published == 5u, 0x41, "manager: frames published after the recovery");
    stop_ok(0x50);
#elif CAM_CASE == 36
    bring_up(&dcfg, 0x20);
    static const uint32_t want[] = {1u, 2u, 4u, 6u, 7u};
    for (unsigned i = 0; i < sizeof want / sizeof want[0]; ++i) {
        fx1_camera_frame f;
        const uint32_t n = take(&f, 0, 0x30 + 0x10u * i);
        if (n != want[i]) fx1_log_hex("  sensor frame", n);
        fx1_check(n == want[i], 0x38 + 0x10u * i, "manager: the faulted frames are not published");
    }
    const fx1_camera_stats* st = &g_cam.stats;
    fx1_log_hex("  csi_dropped", st->direct_failed[FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED]);
    fx1_log_hex("  invalid", st->direct_failed[FX1_ISP_INGRESS_OUTCOME_INVALID]);
    fx1_log_hex("  missing_fe", st->direct_failed[FX1_ISP_INGRESS_OUTCOME_CSI_MISSING_FE]);
    fx1_check(st->direct_failed[FX1_ISP_INGRESS_OUTCOME_CSI_MISSING_FE] == 1u &&
                  st->direct_failed[FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED] + st->direct_failed[FX1_ISP_INGRESS_OUTCOME_INVALID] ==
                      1u,
              0x90, "manager: one CRC drop, one missing FE counted");
    stop_ok(0xA0);
#elif CAM_CASE == 37
    bring_up(&dcfg, 0x20);
    take_consecutive(2, 0x30);
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 0) == FX1_CCS_OK, 0x40, "manager: sensor stream-off behind the manager");
    unsigned extra = 0;
    int rc = FX1_CAMERA_OK;
    uint32_t el = 0;
    for (; extra < 4u; ++extra) {
        fx1_camera_frame f;
        const uint64_t t = fx1_mtime();
        rc = fx1_camera_dequeue(&g_cam, &f, 3u * FRAME_US);
        el = us_since(t);
        if (rc != FX1_CAMERA_OK) break;
        fx1_check(fx1_camera_release(&g_cam, &f) == FX1_CAMERA_OK, 0x41, "manager: frame in flight released");
    }
    fx1_log_hex("  frames after the stream-off", extra);
    fx1_log_hex("  dequeue us", el);
    fx1_check(rc == FX1_CAMERA_ETIMEOUT && extra <= 2u && el <= 3u * FRAME_US + 1000u && g_cam.stats.timeouts == 1u,
              0x42, "manager: ETIMEOUT within the budget once the sensor stopped");
    stop_ok(0x50);
    bring_up(&dcfg, 0x60);
    take_consecutive(3, 0x70);
    stop_ok(0x80);
#elif CAM_CASE == 38
    fx1_check(fx1_camera_configure(&g_cam, &dcfg) == FX1_CAMERA_OK, 0x20, "manager: configured");
    fx1_write32(FX1_I2C0_BASE + 0x10u, 0); /* I2C CTRL.enablehost = 0: the stream-on cannot reach the sensor */
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_EDEV && g_cam.state == 1, 0x21, "manager: start EDEV, state 1");
    fx1_check(fx1_isp_ingress_source(&g_cam.ing) == FX1_ISP_INGRESS_SOURCE_DIRECT, 0x22, "manager: still configured direct");
    fx1_write32(FX1_I2C0_BASE + 0x10u, 1);
    stop_ok(0x30);
    int on = 1;
    fx1_check(!(fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK) &&
                  fx1_ccs_stream_state(&g_cam.sensor, &on) == FX1_CCS_OK && !on,
              0x34, "manager: ISP off, the sensor does not stream");
    bring_up(&dcfg, 0x40);
    take_consecutive(3, 0x50);
    stop_ok(0x60);
#elif CAM_CASE == 39
    /* Frame 1 without FE, the sensor stops: stop(STOP_US). */
    bring_up(&dcfg, 0x20);
    no_fe_then_sensor_off(0, 0x30);
    uint32_t el = 0;
    uint32_t cancelled = g_cam.stats.stop_cancelled;
    int rc = stop_drained(STOP_US, &el, 0x40);
    fx1_log_hex("  stop csi resets", g_cam.stats.stop_csi_resets);
    fx1_check(rc == FX1_CAMERA_OK && g_cam.state == 0 && el <= STOP_US + FX1_CAMERA_STOP_SLACK_US, 0x42,
              "manager: stop OK within the deadline");
    fx1_check(g_cam.stats.stop_csi_resets == 1u && g_cam.stats.direct_ok == 0 &&
                  g_cam.stats.stop_cancelled > cancelled,
              0x43, "manager: one CSI-2 soft reset, the capture cancelled, not published");
    fx1_check(!(fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_STREAM_CONFIG) & FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK), 0x44,
              "manager: CSI-2 stream path off after the stop");
    fx1_check(fx1_camera_stop(&g_cam, STOP_US) == FX1_CAMERA_ESTATE, 0x45, "manager: a second stop: not running");
    /* Configure and start again: sensor frames 2, 3, 4. */
    bring_up(&dcfg, 0x50);
    take_consecutive(3, 0x60);
    fx1_check(g_last_n == 4u, 0x68, "manager: frames 2-4 after the restart");
    /* Frame 5 without FE, the sensor stops: stop(0), no time for the standby. */
    no_fe_then_sensor_off(fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_FRAME_COUNT_OFFSET), 0x70);
    el = 0;
    rc = stop_drained(0, &el, 0x80);
    fx1_check(rc == FX1_CAMERA_ESENSOR && g_cam.state == 3 && el <= FX1_CAMERA_STOP_SLACK_US, 0x82,
              "manager: stop(0) ESENSOR, state 3 within the slack");
    fx1_check(g_cam.stats.stop_csi_resets == 2u && g_cam.stats.direct_ok == 3u, 0x83,
              "manager: a second CSI-2 soft reset, nothing more published");
    fx1_check(fx1_camera_configure(&g_cam, &dcfg) == FX1_CAMERA_ESTATE, 0x84, "manager: configure refused in state 3");
    rc = fx1_camera_stop(&g_cam, STOP_US);
    int on = 1;
    fx1_check(rc == FX1_CAMERA_OK && g_cam.state == 0 && fx1_ccs_stream_state(&g_cam.sensor, &on) == FX1_CCS_OK && !on,
              0x85, "manager: stop again OK, sensor in standby");
    bring_up(&dcfg, 0x90);
    take_consecutive(3, 0xA0);
    fx1_check(g_last_n == 8u, 0xA8, "manager: frames 6-8 after the restart");
    stop_ok(0xB0);
#elif CAM_CASE == 40
    bring_up(&dcfg, 0x20);
    /* Frames 1, 2 DONE and not polled when frame 3's ODMA write fails. */
    uint64_t end = fx1_mtime() + ticks(4u * FRAME_US);
    while (!g_cam.isp_fatal && fx1_mtime() < end) {
    }
    const uint32_t done0 = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_DONE_OFFSET);
    fx1_log_hex("  fatal", g_cam.isp_fatal);
    fx1_log_hex("  DONE before the poll", done0);
    fx1_check(g_cam.isp_fatal != 0 && (done0 & 3u) == 3u, 0x30, "manager: ODMA error with outputs 0, 1 DONE, not polled");
    (void)fx1_camera_poll(&g_cam);
    fx1_log_hex("  unarmed after the recovery", g_cam.unarmed);
    fx1_check(g_cam.ready_count == 2u && g_cam.stats.isp_errors == 1u, 0x31, "manager: frames 1, 2 published after it");
    fx1_camera_frame a, b;
    take(&a, 1, 0x40);
    take(&b, 1, 0x50);
    const uint32_t crc_b = cam_isp_crc(&b, W, W);
    const uint32_t abit = 1u << a.isp_output, bbit = 1u << b.isp_output;
    fx1_check(fx1_camera_release(&g_cam, &a) == FX1_CAMERA_OK && !(g_cam.unarmed & abit), 0x60,
              "manager: released and armed, not marked unarmed");
    end = fx1_mtime() + ticks(2u * FRAME_US);
    while ((fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_FREE_OFFSET) & abit) && fx1_mtime() < end) {
    }
    const uint32_t free1 = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_FREE_OFFSET);
    const uint32_t done1 = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_DONE_OFFSET);
    fx1_log_hex("  FREE, the ODMA writing the released slot", free1);
    fx1_check(!(free1 & abit) && !(done1 & abit), 0x61, "manager: the ODMA claimed the released slot, not DONE yet");
    (void)fx1_camera_poll(&g_cam);
    const uint32_t free2 = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_FREE_OFFSET);
    fx1_log_hex("  FREE after the poll", free2);
    fx1_check(!(free2 & abit), 0x62, "manager: the poll does not arm a slot the ODMA writes");
    /* The frame the ODMA writes in the released slot, then the rotation head
     * reaches the held slot: the ring waits for it (contract §8, ODMA rotation
     * in slot order), new frames are dropped without credit. */
    fx1_camera_frame f;
    take(&f, 0, 0x70);
    fx1_check(f.isp_output == a.isp_output, 0x78, "manager: the released slot published again");
    const uint32_t ok0 = g_cam.stats.direct_ok;
    fx1_isp_ingress_counters k0, k1;
    counters(&k0);
    wait_us(3u * FRAME_US);
    counters(&k1);
    fx1_check(k1.drop_no_credit >= k0.drop_no_credit + 2u && g_cam.stats.direct_ok == ok0 && g_cam.ready_count == 0,
              0x80, "manager: the ring waits at the held slot, nothing published");
    fx1_dma_complete((const volatile void*)(uintptr_t)b.y, W * H, FX1_DMA_FROM_DEVICE);
    fx1_dma_complete((const volatile void*)(uintptr_t)b.uv, W * H / 2u, FX1_DMA_FROM_DEVICE);
    fx1_check(cam_isp_crc(&b, W, W) == crc_b && !(fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_ODMA_BUF_FREE_OFFSET) & bbit),
              0x81, "manager: the held NV12 unchanged, its slot not armed");
    fx1_check(fx1_camera_release(&g_cam, &b) == FX1_CAMERA_OK && !(g_cam.unarmed & bbit), 0x82,
              "manager: held output released and armed");
    /* A full ring through every slot, the held one first. */
    uint32_t seen = 0, last_tag = 0;
    for (unsigned i = 0; i < 5u; ++i) {
        take(&f, 0, 0x90 + 0x10u * i);
        if (i == 0) fx1_check(f.isp_output == b.isp_output, 0x98, "manager: the ring resumes at the released slot");
        fx1_check(f.isp_seq > last_tag, 0x99 + 0x10u * i, "manager: TAGs increase");
        last_tag = f.isp_seq;
        seen |= 1u << f.isp_output;
    }
    fx1_check(seen == (1u << FX1_ISP_NUM_BUFFERS) - 1u, 0xF0, "manager: every slot used after the release");
    fx1_log_hex("  ISP_FAILED", g_cam.stats.direct_failed[FX1_ISP_INGRESS_OUTCOME_ISP_FAILED]);
    fx1_check(g_cam.stats.isp_errors == 1u && g_cam.stats.direct_failed[FX1_ISP_INGRESS_OUTCOME_ISP_FAILED] >= 1u, 0x100,
              "manager: one ODMA error, its frame not published");
    stop_ok(0x110);
#elif CAM_CASE == 41
    /* Configured (state 1). stop(0): no time for the SOURCE switch, cleanup
     * settle only. */
    fx1_check(fx1_camera_configure(&g_cam, &dcfg) == FX1_CAMERA_OK, 0x20, "manager: configured");
    gate(1);
    int rc = fx1_camera_stop(&g_cam, 0);
    fx1_log_hex("  stop rc", (uint32_t)rc);
    fx1_check(rc == FX1_CAMERA_EBUSY, 0x21, "manager: configured stop EBUSY, cleanup not done");
    pending_ok(1, 0x28);
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_ESTATE && fx1_camera_configure(&g_cam, &dcfg) == FX1_CAMERA_ESTATE,
              0x22, "manager: start / configure refused while pending");
    g_refuse_memory = 0;
    stop_completes(0x24);
    ungate();

    /* Running (state 2), one output held. */
    g_last_seq = 0;
    g_last_ts = 0;
    bring_up(&dcfg, 0x30);
    fx1_camera_frame held;
    take(&held, 1, 0x38);
    gate(1);
    g_refused = 0;
    uint64_t t = fx1_mtime();
    rc = fx1_camera_stop(&g_cam, 1000u);
    uint32_t el = us_since(t);
    fx1_log_hex("  stop rc", (uint32_t)rc);
    fx1_log_hex("  stop us", el);
    fx1_check(rc == FX1_CAMERA_EBUSY && el <= 1000u + FX1_CAMERA_STOP_SLACK_US, 0x40,
              "manager: running stop EBUSY within the deadline + slack");
    pending_ok(2, 0x48);
    pending_snap a, b;
    snap(&a);
    fx1_log_hex("  CSI FRAME_COUNT", a.csi_frames);
    fx1_log_hex("  ODMA FREE", a.odma_free);
    rc = fx1_camera_tpg_frame(&g_cam);
    fx1_log_hex("  TPG frame rc", (uint32_t)rc);
    fx1_check(rc == FX1_CAMERA_ESTATE, 0x41, "manager: TPG frame refused while the stop is pending");
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_ESTATE && fx1_camera_configure(&g_cam, &dcfg) == FX1_CAMERA_ESTATE,
              0x42, "manager: start / configure refused while pending");
    fx1_check(fx1_camera_poll(&g_cam) == 0, 0x43, "manager: poll publishes nothing while pending");
    fx1_camera_frame f;
    fx1_check(fx1_camera_dequeue(&g_cam, &f, 2u * FRAME_US) == FX1_CAMERA_ETIMEOUT, 0x44,
              "manager: dequeue times out while pending");
    fx1_check(fx1_camera_release(&g_cam, &held) == FX1_CAMERA_OK && !g_cam.out_owned[held.isp_output], 0x45,
              "manager: release while pending OK (output no longer owned)");
    wait_us(FRAME_US);
    snap(&b);
    fx1_log_hex("  CSI FRAME_COUNT after", b.csi_frames);
    fx1_log_hex("  ingress no credit after", b.k.drop_no_credit);
    fx1_log_hex("  ODMA FREE after", b.odma_free);
    fx1_check(b.csi_frames == a.csi_frames && b.k.frames_admitted == a.k.frames_admitted &&
                  b.k.drop_no_credit == a.k.drop_no_credit && b.k.frames_invalid == a.k.frames_invalid,
              0x46, "manager: no CSI-2 frame started, the ingress admitted / dropped nothing");
    fx1_check(b.odma_frames == a.odma_frames && b.direct_ok == a.direct_ok && g_cam.ready_count == 0 &&
                  b.isp_state == a.isp_state,
              0x47, "manager: the ISP stays stopped, nothing published");
    fx1_check(b.odma_free == a.odma_free && !(b.odma_free & (1u << held.isp_output)) && b.unarmed == a.unarmed, 0x4C,
              "manager: the release armed nothing");
    t = fx1_mtime();
    rc = fx1_camera_stop(&g_cam, 1000u);
    el = us_since(t);
    fx1_log_hex("  stop again rc", (uint32_t)rc);
    fx1_check(rc == FX1_CAMERA_EBUSY && el <= 1000u + FX1_CAMERA_STOP_SLACK_US &&
                  g_cam.stats.stop_csi_resets == a.resets,
              0x50, "manager: stop again still EBUSY, no CSI-2 reset (table empty)");
    pending_ok(2, 0x58);
    g_refuse_memory = 0;
    stop_completes(0x60);
    ungate();
    fx1_check(g_cam.stats.stop_csi_resets == 0 && g_cam.stats.direct_ok == 1u, 0x64,
              "manager: no CSI-2 reset, one frame published");

    /* Configure and start again. */
    g_last_seq = 0;
    g_last_ts = 0;
    bring_up(&dcfg, 0x70);
    take(&f, 0, 0x78); /* one frame: a 640x480 NV12 CRC takes ~0.4 s of simulated CPU time */
    stop_ok(0x90);
    fx1_check(g_cam.stats.direct_ok == 2u && g_cam.stats.stop_csi_resets == 0, 0x94, "manager: 2 frames published");
#elif CAM_CASE == 35
    bring_up(&dcfg, 0x20);
    take_consecutive(2, 0x30);
    fx1_write32(FX1_I2C0_BASE + 0x10u, 0); /* I2C CTRL.enablehost = 0: the standby never reaches the sensor */
    uint64_t t = fx1_mtime();
    int rc = fx1_camera_stop(&g_cam, STOP_US);
    const uint32_t el = us_since(t);
    fx1_log_hex("  stop rc", (uint32_t)rc);
    fx1_check(rc == FX1_CAMERA_ESENSOR && g_cam.state == 3 && el <= STOP_US + FX1_CAMERA_STOP_SLACK_US, 0x40,
              "manager: ESENSOR within the deadline, state 3");
    fx1_check(fx1_isp_ingress_source(&g_cam.ing) == FX1_ISP_INGRESS_SOURCE_MEMORY &&
                  !(fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK),
              0x41, "manager: ingress and ISP stopped anyway");
    fx1_check(fx1_camera_configure(&g_cam, &dcfg) == FX1_CAMERA_ESTATE, 0x42, "manager: configure refused in state 3");
    fx1_write32(FX1_I2C0_BASE + 0x10u, 1);
    t = fx1_mtime();
    rc = fx1_camera_stop(&g_cam, STOP_US);
    int on = 1;
    fx1_check(rc == FX1_CAMERA_OK && g_cam.state == 0 && fx1_ccs_stream_state(&g_cam.sensor, &on) == FX1_CCS_OK && !on,
              0x43, "manager: stop again OK, sensor in standby");
    wait_us(2u * FRAME_US);
    g_last_seq = 0;
    g_last_ts = 0;
    bring_up(&dcfg, 0x50);
    take_consecutive(3, 0x60);
    stop_ok(0x70);
#else
#error "unknown CAM_CASE"
#endif
    fx1_log_hex("  direct ok", g_cam.stats.direct_ok);
    fx1_log_hex("  stop cancelled", g_cam.stats.stop_cancelled);
    fx1_log("camera manager, direct path: PASS");
    return 0;
}
