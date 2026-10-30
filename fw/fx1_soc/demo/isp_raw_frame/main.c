/*
 * ISP RAW-frame demo: the whole FX1 platform processes one host-provided RAW
 * frame. Driven by platforms/fx1_soc/demo/run_isp_demo.py, which loads the
 * RAW frame, a register profile and a job descriptor into DDR (fx1_soc --load)
 * and reads the NV12 result back (--dump). See isp_demo_job.h.
 *
 * Hart 0, through the reference C99 driver on an MMIO HAL:
 *   job descriptor -> fx1_isp_init -> set_geometry -> apply(profile) ->
 *   start -> queue -> ISP interrupt (PLIC source 3) -> next_done -> statistics
 *   -> result block -> PASS
 * while the ISP's IDMA reads the RAW from DDR and its ODMA writes the NV12 back
 * over the FX1 bus. The firmware does not touch the pixels.
 */
#include "fx1_fw.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "isp_demo_job.h"
#include "isp_demo_setup.h"

#define FATAL_DMA_ERR (FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT | FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | \
                       FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT)

static fx1_isp_dev dev;
static volatile uint32_t isr_done, isr_fatal;

static uint32_t hal_read32(void* ctx, uint32_t offset)
{
    (void)ctx;
    return fx1_read32(FX1_ISP_CSR_BASE + offset);
}
static void hal_write32(void* ctx, uint32_t offset, uint32_t value)
{
    (void)ctx;
    fx1_write32(FX1_ISP_CSR_BASE + offset, value);
}
static void hal_delay_cycles(void* ctx, uint32_t cycles)
{
    (void)ctx;
    const uint64_t ticks = ((uint64_t)cycles * 2u + 49u) / 50u + 1u; /* 2 ns ISP clock, 50 ns tick */
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < ticks) {
    }
}
static const fx1_isp_hal hal = {hal_read32, hal_write32, hal_delay_cycles, 0};

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) {
        fx1_log_hex("unexpected mcause", mcause);
        fx1_fail(0x0E0);
    }
    const uint32_t id = fx1_plic_claim(0);
    if (id == FX1_IRQ_ISP) {
        fx1_isp_events ev;
        fx1_isp_irq(&dev, &ev); /* acknowledges at the ISP */
        if (ev.frames_done) isr_done = 1;
        isr_fatal |= ev.dma_err & FATAL_DMA_ERR;
    }
    if (id) fx1_plic_complete(0, id);
    return mepc;
}

int main(void)
{
    const volatile isp_demo_job* job = (const volatile isp_demo_job*)(FX1_FW_SHARED_BASE + ISP_DEMO_JOB_OFFSET);
    volatile isp_demo_result* res = (volatile isp_demo_result*)(FX1_FW_SHARED_BASE + ISP_DEMO_RESULT_OFFSET);
    const fx1_isp_reg_write* profile = (const fx1_isp_reg_write*)(FX1_FW_SHARED_BASE + ISP_DEMO_PROFILE_OFFSET);
    res->magic = 0;
    fx1_check(isp_demo_job_ok(job), 0x01, "job descriptor present (run through run_isp_demo.py)");
    const uint32_t mode = isp_demo_mode(job);
    fx1_log_hex("ISP demo: width", job->width);
    fx1_log_hex("          height", job->height);
    fx1_log_hex("          bayer", job->bayer);
    fx1_log_hex("          profile writes", job->profile_count);
    fx1_log_hex("          mode (0 profile, 1 legacy)", mode);
    fx1_log_hex("          outputs", isp_demo_outputs(job));

    /* Bring-up in HAS section 9.1 order: geometry, profile (and in profile
     * mode the bank: INIT, payloads, SEAL, APPLY), outputs, then start. */
    fx1_check(fx1_isp_init(&dev, &hal) == FX1_ISP_OK, 0x10, "driver init");
    uint32_t tune_error = 0;
    if (isp_demo_configure(&dev, job, profile, mode, &tune_error) != FX1_ISP_OK) {
        fx1_log_hex("ISP profile / geometry refused, TUNE_ERROR", tune_error);
        res->status = 2;
        res->mode = mode;
        res->tune_error = tune_error;
        res->magic = ISP_DEMO_RESULT_MAGIC;
        fx1_mb();
        fx1_fail(0x11);
    }

    fx1_isp_buffers bufs;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) { /* one frame at a time: one slot is enough */
        bufs.in[i] = job->in_addr;
        bufs.y[i] = job->y_addr;
        bufs.uv[i] = job->uv_addr;
    }
    bufs.in_stride = job->in_stride;
    bufs.y_stride = bufs.uv_stride = job->out_stride;
    bufs.max_burst_beats = 16;

    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_ISP, 1);
    fx1_plic_enable(0, FX1_IRQ_ISP, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();

    const uint32_t y_bytes = job->out_stride * job->height, uv_bytes = job->out_stride * (job->height / 2u);
    fx1_dma_prepare((const volatile void*)(uintptr_t)job->y_addr, y_bytes, FX1_DMA_FROM_DEVICE);
    fx1_dma_prepare((const volatile void*)(uintptr_t)job->uv_addr, uv_bytes, FX1_DMA_FROM_DEVICE);
    fx1_check(fx1_isp_start(&dev, &bufs) == FX1_ISP_OK, 0x12, "stream start (output stride fits)");
    fx1_log_hex("ISP output width", dev.out_width);
    fx1_log_hex("           height", dev.out_height);

    const uint64_t t0 = fx1_mtime();
    for (uint32_t k = 0; k < job->frames; ++k) {
        /* The RAW was written by the host before reset; hand it to the IDMA. */
        fx1_dma_prepare((const volatile void*)(uintptr_t)job->in_addr, job->in_stride * job->height,
                        FX1_DMA_TO_DEVICE);
        uint32_t saved = fx1_irq_save();
        isr_done = 0;
        fx1_check(fx1_isp_input_ready(&dev) && fx1_isp_queue_frame(&dev) == FX1_ISP_OK, 0x20, "frame queued");
        fx1_irq_restore(saved);
        const uint64_t start = fx1_mtime();
        unsigned index = 0;
        int got = FX1_ISP_ENODATA;
        while (got != FX1_ISP_OK) {
            if (isr_fatal) {
                fx1_log_hex("ISP DMA_ERR", isr_fatal);
                fx1_fail(0x21);
            }
            fx1_check(fx1_mtime() - start < FX1_MTIME_HZ, 0x22, "frame within 1 s of simulated time");
            if (!isr_done) continue;
            saved = fx1_irq_save();
            got = fx1_isp_next_done(&dev, &index);
            fx1_irq_restore(saved);
        }
        fx1_dma_complete((const volatile void*)(uintptr_t)job->y_addr, y_bytes, FX1_DMA_FROM_DEVICE);
        fx1_dma_complete((const volatile void*)(uintptr_t)job->uv_addr, uv_bytes, FX1_DMA_FROM_DEVICE);
        fx1_log_hex("frame done", k);
        if (k + 1u < job->frames) { /* the next frame writes the same output again */
            fx1_dma_prepare((const volatile void*)(uintptr_t)job->y_addr, y_bytes, FX1_DMA_FROM_DEVICE);
            fx1_dma_prepare((const volatile void*)(uintptr_t)job->uv_addr, uv_bytes, FX1_DMA_FROM_DEVICE);
        }
    }
    const uint32_t ticks = (uint32_t)(fx1_mtime() - t0);

    fx1_isp_progress p;
    fx1_check(fx1_isp_get_progress(&dev, &p) == FX1_ISP_OK, 0x30, "progress");
    fx1_isp_aec_global aec;
    fx1_isp_awb_global awb;
    fx1_isp_af_scores af;
    const int aec_ok = fx1_isp_read_aec_global(&dev, &aec) == FX1_ISP_OK;
    const int awb_ok = fx1_isp_read_awb_global(&dev, &awb) == FX1_ISP_OK;
    const int af_ok = fx1_isp_read_af(&dev, &af) == FX1_ISP_OK;

    res->status = 0;
    res->out_width = dev.out_width;
    res->out_height = dev.out_height;
    res->out_stride = job->out_stride;
    res->frames_done = job->frames;
    res->idma_frames = p.idma_frames;
    res->odma_frames = p.odma_frames;
    res->dma_err = p.dma_err;
    res->aec_frame_id = aec_ok ? aec.frame_id : 0;
    res->awb_frame_id = awb_ok ? awb.frame_id : 0;
    res->af_frame_id = af_ok ? af.frame_id : 0;
    res->frame_ticks = ticks;
    uint32_t prof = 0, w1 = 0, h1 = 0, w2 = 0, h2 = 0;
    const uint32_t outputs = isp_demo_outputs(job);
    /* the last frame's buffer index: the rotation advances once per frame */
    if (mode == ISP_DEMO_MODE_PROFILE) fx1_isp_output_meta(&dev, 0, (job->frames - 1u) % FX1_ISP_NUM_BUFFERS, NULL, NULL, &prof);
    if (outputs & 2u) isp_demo_channel_size(&dev, 1, &w1, &h1);
    if (outputs & 4u) isp_demo_channel_size(&dev, 2, &w2, &h2);
    res->mode = mode;
    res->outputs = outputs;
    res->profile_id = prof;
    res->tune_error = 0;
    res->ch1_width = w1;
    res->ch1_height = h1;
    res->ch2_width = w2;
    res->ch2_height = h2;
    fx1_release();
    res->magic = ISP_DEMO_RESULT_MAGIC;
    fx1_mb();

    fx1_log_hex("frames processed", job->frames);
    fx1_log_hex("simulated us, first queue to last completion", ticks / 20u);
    fx1_log_hex("AEC/AWB/AF FRAME_ID", (res->aec_frame_id << 16) | (res->awb_frame_id << 8) | res->af_frame_id);
    fx1_log_hex("DMA_ERR (non-fatal bits only)", p.dma_err);
    fx1_check(fx1_isp_stop(&dev) >= 0, 0x31, "stream stopped");
    fx1_log("ISP demo done: NV12 in DDR, result block written");
    return 0;
}
