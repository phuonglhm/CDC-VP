/*
 * ISP commands, through the reference C99 driver from a task:
 *   isp selftest  the 8-frame 64x48 fixture (basic profile); every NV12 must
 *                 match the reference CRC-32 (tests/isp_stream/isp_fixture.h)
 *   isp run       the host-loaded RAW job of the ISP demo (fx1_soc --load, see
 *                 fw/fx1_soc/demo/isp_raw_frame/isp_demo_job.h): fixed frames,
 *                 NV12 left in DDR for --dump, result block written
 *   isp run auto  one AWB probe + at least two render frames, using a shared
 *                 base profile and a bounded demo neutral-zone policy
 *   isp stats     AEC/AWB/AF publications of the last frame, captured before
 *                 the stream is stopped (fx1_isp_stop soft-resets the ISP,
 *                 which clears the statistics results; FRAME_IDs survive)
 * The ISR (PLIC source 3) acknowledges at the ISP and gives a semaphore; the
 * driver is called from the task inside critical sections so the ISR never
 * runs in the middle of a driver call.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "isp_common.h"
#include "isp_auto_policy.h"
#include "isp_demo_job.h"
#include "rtos_app.h"
#include "rtos_bsp.h"

static fx1_isp_dev dev;
static SemaphoreHandle_t done;
static volatile uint32_t isr_fatal;
static int dev_ready;

static struct {
    int valid, aec_ok, awb_ok, af_ok;
    fx1_isp_aec_global aec;
    fx1_isp_awb_global awb;
    fx1_isp_af_scores af;
} last_stats;

static volatile int isp_owner; /* ISP_OWNER_*: written by the CLI task, read by the ISR */

int isp_claim(int owner)
{
    taskENTER_CRITICAL();
    const int ok = isp_owner == ISP_OWNER_NONE || isp_owner == owner;
    if (ok) isp_owner = owner;
    taskEXIT_CRITICAL();
    return ok;
}

void isp_release(int owner)
{
    taskENTER_CRITICAL();
    if (isp_owner == owner) isp_owner = ISP_OWNER_NONE;
    taskEXIT_CRITICAL();
}

static BaseType_t isp_isr(void)
{
#ifdef FX1_FW_HAS_CAMERA
    if (isp_owner == ISP_OWNER_CAMERA) return camera_isp_isr();
#endif
    BaseType_t woken = pdFALSE;
    fx1_isp_events ev;
    fx1_isp_irq(&dev, &ev); /* reads and acknowledges at the ISP: the line drops */
    isr_fatal |= ev.dma_err & ISP_DMA_ERR_FATAL;
    if (ev.frames_done || (ev.dma_err & ISP_DMA_ERR_FATAL)) xSemaphoreGiveFromISR(done, &woken);
    return woken;
}

void isp_service_init(void)
{
    done = xSemaphoreCreateBinary();
    configASSERT(done);
    bsp_irq_register(FX1_IRQ_ISP, isp_isr);
}

/* Queue one frame and wait for its output; returns the buffer index or -1. */
static int one_frame(TickType_t timeout)
{
    taskENTER_CRITICAL();
    const int queued = fx1_isp_input_ready(&dev) && fx1_isp_queue_frame(&dev) == FX1_ISP_OK;
    taskEXIT_CRITICAL();
    if (!queued) return -1;
    for (;;) {
        unsigned index = 0;
        taskENTER_CRITICAL();
        const int got = fx1_isp_next_done(&dev, &index);
        taskEXIT_CRITICAL();
        if (got == FX1_ISP_OK) return (int)index;
        if (isr_fatal || xSemaphoreTake(done, timeout) != pdTRUE) return -1;
    }
}

static void stop(void)
{
    last_stats.aec_ok = fx1_isp_read_aec_global(&dev, &last_stats.aec) == FX1_ISP_OK;
    last_stats.awb_ok = fx1_isp_read_awb_global(&dev, &last_stats.awb) == FX1_ISP_OK;
    last_stats.af_ok = fx1_isp_read_af(&dev, &last_stats.af) == FX1_ISP_OK;
    last_stats.valid = 1;
    taskENTER_CRITICAL();
    (void)fx1_isp_stop(&dev);
    taskEXIT_CRITICAL();
}

static int selftest(void)
{
    fx1_isp_buffers bufs;
    isp_buffers(&bufs);
    isr_fatal = 0;
    xSemaphoreTake(done, 0);
    int ok = fx1_isp_init(&dev, &isp_hal) == FX1_ISP_OK &&
             fx1_isp_set_geometry(&dev, ISP_FX_WIDTH, ISP_FX_HEIGHT, ISP_FX_BAYER) == FX1_ISP_OK;
    if (ok) {
        fx1_isp_apply(&dev, isp_fx_profile_basic, sizeof(isp_fx_profile_basic) / sizeof(isp_fx_profile_basic[0]));
        isp_outputs_to_device(&bufs);
        ok = fx1_isp_start(&dev, &bufs) == FX1_ISP_OK;
    }
    dev_ready = ok;
    uint32_t matched = 0;
    for (uint32_t k = 0; ok && k < ISP_FX_FRAMES; ++k) {
        isp_fill_input(bufs.in[fx1_isp_next_input(&dev)], k);
        const int index = one_frame(pdMS_TO_TICKS(100));
        if (index < 0) {
            ok = 0;
            break;
        }
        isp_output_to_cpu(bufs.y[index], bufs.uv[index]);
        const uint32_t crc = isp_nv12_crc(bufs.y[index], bufs.uv[index]);
        if (crc == isp_fx_crc_basic[k]) ++matched;
        isp_output_to_device(bufs.y[index], bufs.uv[index]);
    }
    if (dev_ready) stop();
    out("isp: ");
    out_dec(matched);
    out("/");
    out_dec(ISP_FX_FRAMES);
    out(" frames 64x48 match the reference CRC, DMA_ERR fatal ");
    out_hex(isr_fatal);
    out_line("");
    return report("isp 8-frame fixture", ok && matched == ISP_FX_FRAMES && !isr_fatal);
}


/* Called between completed frames, with no frame in flight. Release the AWB
 * bank on every path. One result is measured, then a reset/restart renders
 * the same RAW with the chosen controls, so fixed-reference comparison is
 * independent of the probe's GTM/NR state. */
static isp_auto_choice auto_meter(void)
{
    isp_auto_meter meter = {0};
    fx1_isp_awb_lease lease;
    isp_auto_choice c;
    if (fx1_isp_awb_acquire(&dev, &lease) == FX1_ISP_OK) {
        for (unsigned i=0; i<lease.nx*lease.ny; ++i) {
            fx1_isp_awb_zone z;
            if (fx1_isp_awb_read_zones(&dev, i, 1, &z) != FX1_ISP_OK) {
                memset(&meter, 0, sizeof(meter));
                break;
            }
            isp_auto_add(&meter, z.sum, z.count);
        }
        if (fx1_isp_awb_release(&dev) != FX1_ISP_OK) memset(&meter, 0, sizeof(meter));
    }
#define AUTO_RD(name) dev.hal.read32(dev.hal.ctx, FX1_ISP_##name##_OFFSET)
    c=isp_auto_decide(&meter, AUTO_RD(WB_GAIN_R), AUTO_RD(WB_GAIN_G), AUTO_RD(WB_GAIN_B),
                      AUTO_RD(GTM_KEY), AUTO_RD(GTM_LWHITE), AUTO_RD(EE_BETA));
#undef AUTO_RD
    out("ISP_AUTO " ISP_AUTO_POLICY_ID " valid="); out_dec(c.valid);
    out(" zones="); out_dec(c.zones); out(" blend="); out_dec(c.blend); out_line("");
    /* Exact applied CSR writes, used by the host's independent reference. */
    out("ISP_AUTO_CSR WB_GAIN_R "); out_hex(c.r); out_line("");
    out("ISP_AUTO_CSR WB_GAIN_G "); out_hex(c.g); out_line("");
    out("ISP_AUTO_CSR WB_GAIN_B "); out_hex(c.b); out_line("");
    out("ISP_AUTO_CSR GTM_KEY "); out_hex(c.key); out_line("");
    out("ISP_AUTO_CSR GTM_LWHITE "); out_hex(c.white); out_line("");
    out("ISP_AUTO_CSR EE_BETA "); out_hex(c.beta); out_line("");
    return c;
}

static int run_job(int adaptive)
{
    const volatile isp_demo_job* job = (const volatile isp_demo_job*)(FX1_FW_SHARED_BASE + ISP_DEMO_JOB_OFFSET);
    volatile isp_demo_result* res = (volatile isp_demo_result*)(FX1_FW_SHARED_BASE + ISP_DEMO_RESULT_OFFSET);
    if (job->magic != ISP_DEMO_JOB_MAGIC || job->version != ISP_DEMO_VERSION) {
        out_line("isp run: no job descriptor in DDR (load one with run_isp_demo.py / fx1_soc --load)");
        return 0;
    }
    if (adaptive && job->frames < 3u) {
        out_line("isp run auto: job needs a probe plus at least 2 render frames");
        return report("isp run auto", 0);
    }
    fx1_isp_buffers bufs;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        bufs.in[i] = job->in_addr;
        bufs.y[i] = job->y_addr;
        bufs.uv[i] = job->uv_addr;
    }
    bufs.in_stride = job->in_stride;
    bufs.y_stride = bufs.uv_stride = job->out_stride;
    bufs.max_burst_beats = 16;
    isr_fatal = 0;
    xSemaphoreTake(done, 0);
    res->magic = 0;
    int ok = fx1_isp_init(&dev, &isp_hal) == FX1_ISP_OK &&
             fx1_isp_set_geometry(&dev, job->width, job->height, job->bayer) == FX1_ISP_OK;
    if (ok) {
        fx1_isp_apply(&dev, (const fx1_isp_reg_write*)(FX1_FW_SHARED_BASE + ISP_DEMO_PROFILE_OFFSET),
                      job->profile_count);
        if (adaptive) {
            const uint32_t ctrl=dev.hal.read32(dev.hal.ctx,FX1_ISP_GTM_CTRL_OFFSET);
            const uint32_t wb=dev.hal.read32(dev.hal.ctx,FX1_ISP_WB_CTRL_OFFSET);
            const uint32_t ee=dev.hal.read32(dev.hal.ctx,FX1_ISP_EE_CTRL_OFFSET);
            if (!(wb & FX1_ISP_WB_CTRL_EN_MASK) || !(ctrl & FX1_ISP_GTM_CTRL_EN_MASK) ||
                (ctrl & FX1_ISP_GTM_CTRL_MANUAL_MASK) ||
                (ee & (FX1_ISP_EE_CTRL_PROFILE_SEL_MASK | FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK))) {
                out_line("isp run auto: requires WB, GTM auto, EE profile 0 (shared base)");
                return report("isp run auto",0);
            }
        }
        fx1_dma_prepare((const volatile void*)(uintptr_t)job->y_addr, job->out_stride * job->height,
                        FX1_DMA_FROM_DEVICE);
        fx1_dma_prepare((const volatile void*)(uintptr_t)job->uv_addr, job->out_stride * (job->height / 2u),
                        FX1_DMA_FROM_DEVICE);
        ok = fx1_isp_start(&dev, &bufs) == FX1_ISP_OK;
    }
    dev_ready = ok;
    fx1_isp_progress initial = {0};
    if (dev_ready) fx1_isp_get_progress(&dev, &initial);
    const TickType_t t0 = xTaskGetTickCount();
    uint32_t gtm_enable = 0;
    if (ok && adaptive) {
        fx1_dma_prepare((const volatile void*)(uintptr_t)job->in_addr, job->in_stride*job->height, FX1_DMA_TO_DEVICE);
        ok = one_frame(pdMS_TO_TICKS(1000)) >= 0;
        if (ok) {
            const isp_auto_choice c=auto_meter();
            const uint32_t ctrl=dev.hal.read32(dev.hal.ctx, FX1_ISP_GTM_CTRL_OFFSET);
            /* Policy only supports enabled WB and automatic GTM. */
            if ((ctrl & FX1_ISP_GTM_CTRL_MANUAL_MASK) || !(ctrl & FX1_ISP_GTM_CTRL_EN_MASK)) ok=0;
            taskENTER_CRITICAL();
            (void)fx1_isp_stop(&dev);
            const fx1_isp_reg_write writes[]={
                {FX1_ISP_WB_GAIN_R_OFFSET,c.r},{FX1_ISP_WB_GAIN_G_OFFSET,c.g},{FX1_ISP_WB_GAIN_B_OFFSET,c.b},
                {FX1_ISP_GTM_KEY_OFFSET,c.key},{FX1_ISP_GTM_LWHITE_OFFSET,c.white},{FX1_ISP_EE_BETA_OFFSET,c.beta},
                {FX1_ISP_GTM_CTRL_OFFSET,ctrl & ~FX1_ISP_GTM_CTRL_EN_MASK}};
            fx1_isp_apply(&dev,writes,sizeof(writes)/sizeof(writes[0]));
            gtm_enable=ctrl;
            if (ok) ok=fx1_isp_start(&dev,&bufs)==FX1_ISP_OK;
            taskEXIT_CRITICAL();
            xSemaphoreTake(done,0);
            out_line("ISP_AUTO probe=1; render warm-up GTM off, then GTM auto on");
        }
    }
    for (uint32_t k = 0; ok && k < job->frames - (adaptive ? 1u : 0u); ++k) {
        if (adaptive && k==1u) {
            taskENTER_CRITICAL();
            const fx1_isp_reg_write en={FX1_ISP_GTM_CTRL_OFFSET,gtm_enable};
            fx1_isp_apply(&dev,&en,1);
            taskEXIT_CRITICAL();
        }
        fx1_dma_prepare((const volatile void*)(uintptr_t)job->in_addr, job->in_stride * job->height,
                        FX1_DMA_TO_DEVICE);
        ok = one_frame(pdMS_TO_TICKS(1000)) >= 0;
    }
    fx1_dma_complete((const volatile void*)(uintptr_t)job->y_addr, job->out_stride * job->height,
                     FX1_DMA_FROM_DEVICE);
    fx1_dma_complete((const volatile void*)(uintptr_t)job->uv_addr, job->out_stride * (job->height / 2u),
                     FX1_DMA_FROM_DEVICE);
    fx1_isp_progress p;
    fx1_isp_get_progress(&dev, &p);
    res->status = ok ? 0u : 1u;
    res->out_width = dev.out_width;
    res->out_height = dev.out_height;
    res->out_stride = job->out_stride;
    res->frames_done = ok ? job->frames : 0u;
    res->idma_frames = p.idma_frames - initial.idma_frames;
    res->odma_frames = p.odma_frames - initial.odma_frames;
    res->dma_err = p.dma_err;
    res->frame_ticks = (uint32_t)(xTaskGetTickCount() - t0) * (FX1_MTIME_HZ / configTICK_RATE_HZ);
    if (dev_ready) stop();
    res->aec_frame_id = dev_ready && last_stats.aec_ok ? last_stats.aec.frame_id : 0u;
    res->awb_frame_id = dev_ready && last_stats.awb_ok ? last_stats.awb.frame_id : 0u;
    res->af_frame_id = dev_ready && last_stats.af_ok ? last_stats.af.frame_id : 0u;
    fx1_release();
    res->magic = ISP_DEMO_RESULT_MAGIC;
    out("isp run: ");
    out_dec(job->width);
    out("x");
    out_dec(job->height);
    out(" RAW -> ");
    out_dec(dev.out_width);
    out("x");
    out_dec(dev.out_height);
    out(" NV12 at Y ");
    out_hex(job->y_addr);
    out(" UV ");
    out_hex(job->uv_addr);
    out(", ");
    out_dec(job->frames);
    out(" frame(s), DMA_ERR ");
    out_hex(p.dma_err);
    out_line("");
    return report("isp run", ok && !isr_fatal);
}

static void stats(void)
{
    if (!last_stats.valid) {
        out_line("isp stats: no frame processed yet (isp selftest / isp run)");
        return;
    }
    const fx1_isp_aec_global aec = last_stats.aec;
    const fx1_isp_awb_global awb = last_stats.awb;
    const fx1_isp_af_scores af = last_stats.af;
    if (last_stats.aec_ok) {
        out("AEC FRAME_ID ");
        out_dec(aec.frame_id);
        out(", count R/Gr/Gb/B ");
        for (int c = 0; c < 4; ++c) {
            out_dec(aec.count[c]);
            out(c < 3 ? "/" : "");
        }
        out_line("");
    }
    if (last_stats.awb_ok) {
        out("AWB FRAME_ID ");
        out_dec(awb.frame_id);
        out(", context ");
        out_hex(awb.context_id);
        out_line("");
    }
    if (last_stats.af_ok) {
        out("AF  FRAME_ID ");
        out_dec(af.frame_id);
        out(", context ");
        out_hex(af.context_id);
        out_line("");
    }
}

/* selftest / run use the ISP: refused while the camera owns it. */
static int claim_for(const char* what)
{
    if (isp_claim(ISP_OWNER_ISP)) return 1;
    out(what);
    out_line(": the ISP is in use by the camera (camera stop first)");
    return 0;
}

int cmd_isp(int argc, char** argv)
{
    if (argc == 3 && !strcmp(argv[1], "run") && !strcmp(argv[2], "auto")) {
        if (claim_for("isp run auto")) {
            run_job(1);
            isp_release(ISP_OWNER_ISP);
        }
        return 0;
    }
    if (argc != 2) return 1;
    if (!strcmp(argv[1], "selftest")) {
        if (claim_for("isp selftest")) {
            selftest();
            isp_release(ISP_OWNER_ISP);
        }
    } else if (!strcmp(argv[1], "run")) {
        if (claim_for("isp run")) {
            run_job(0);
            isp_release(ISP_OWNER_ISP);
        }
    } else if (!strcmp(argv[1], "stats")) {
        stats();
    } else {
        return 1;
    }
    return 0;
}
