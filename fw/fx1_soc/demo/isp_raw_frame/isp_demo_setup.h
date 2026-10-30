/*
 * ISP RAW-frame demo: the job's ISP bring-up, shared by the bare-metal demo
 * (main.c) and the FreeRTOS CLI (`isp run`). See isp_demo_job.h.
 *
 * Profile mode (the default, CSR v1.07): the job's profile is applied in
 * legacy mode, then fx1_isp_profile_from_legacy builds it into bank 0
 * (INIT, payloads reloaded, SEAL, APPLY). A failure is reported (status 2,
 * the TUNE_ERROR bits), never replaced by a legacy run.
 */
#ifndef ISP_DEMO_SETUP_H
#define ISP_DEMO_SETUP_H

#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "isp_demo_job.h"

/* 0 if the descriptor is not a job this firmware runs. */
static inline int isp_demo_job_ok(const volatile isp_demo_job* job)
{
    if (job->magic != ISP_DEMO_JOB_MAGIC || job->frames < 1u) return 0;
    if (job->version == ISP_DEMO_VERSION_V1) return 1;
    if (job->version != ISP_DEMO_VERSION || job->mode > ISP_DEMO_MODE_LEGACY) return 0;
    return job->outputs == 0u || ((job->outputs & 1u) && job->outputs <= 7u);
}

static inline uint32_t isp_demo_mode(const volatile isp_demo_job* job)
{
    return job->version == ISP_DEMO_VERSION_V1 ? ISP_DEMO_MODE_LEGACY : job->mode;
}

static inline uint32_t isp_demo_outputs(const volatile isp_demo_job* job)
{
    return job->version == ISP_DEMO_VERSION_V1 || job->outputs == 0u ? 1u : job->outputs;
}

static inline uint32_t isp_demo_profile_id(const volatile isp_demo_job* job)
{
    return job->version == ISP_DEMO_VERSION_V1 || job->profile_id == 0u ? 1u : job->profile_id;
}

/* Geometry, profile, outputs and (profile mode) the bank; before
 * fx1_isp_start. Returns FX1_ISP_OK, or the failing driver result with the
 * TUNE_ERROR bits in *tune_error. `mode`: the mode to run (the job's, or
 * legacy where the caller needs it). */
static inline int isp_demo_configure(fx1_isp_dev* dev, const volatile isp_demo_job* job,
                                     const fx1_isp_reg_write* profile, uint32_t mode, uint32_t* tune_error)
{
    *tune_error = 0;
    int rc = fx1_isp_profile_mode(dev, 0); /* a previous profile-mode run left it on */
    if (rc == FX1_ISP_OK) rc = fx1_isp_set_geometry(dev, job->width, job->height, job->bayer);
    if (rc == FX1_ISP_OK) rc = fx1_isp_apply(dev, profile, job->profile_count); /* EIO: a table not confirmed */
    if (rc != FX1_ISP_OK) return rc;
    const uint32_t outputs = isp_demo_outputs(job);
    const fx1_isp_reg_write enable = {FX1_ISP_OUTPUT_ENABLE_OFFSET, outputs};
    fx1_isp_apply(dev, &enable, 1);
    for (unsigned ch = 1; ch < 3u; ++ch) {
        if (!(outputs & (1u << ch))) continue;
        fx1_isp_channel_buffers cb;
        for (unsigned k = 0; k < FX1_ISP_NUM_BUFFERS; ++k) { /* one frame at a time: one slot is enough */
            cb.y[k] = ch == 1u ? job->ch1_y_addr : job->ch2_addr;
            cb.uv[k] = job->ch1_uv_addr;
        }
        cb.y_stride = ch == 1u ? job->ch1_stride : job->ch2_stride;
        cb.uv_stride = ch == 1u ? job->ch1_stride : 0u;
        rc = fx1_isp_set_channel_buffers(dev, ch, &cb);
        if (rc != FX1_ISP_OK) return rc;
    }
    if (mode == ISP_DEMO_MODE_PROFILE)
        rc = fx1_isp_profile_from_legacy(dev, 0, profile, job->profile_count, isp_demo_profile_id(job), tune_error);
    return rc;
}

/* Output geometry of channel ch (after start). */
static inline void isp_demo_channel_size(fx1_isp_dev* dev, unsigned ch, uint32_t* w, uint32_t* h)
{
    *w = dev->hal.read32(dev->hal.ctx, ch == 1u ? FX1_ISP_CH1_RESIZER_OUT_W_OFFSET : FX1_ISP_CH2_RESIZER_OUT_W_OFFSET);
    *h = dev->hal.read32(dev->hal.ctx, ch == 1u ? FX1_ISP_CH1_RESIZER_OUT_H_OFFSET : FX1_ISP_CH2_RESIZER_OUT_H_OFFSET);
}

#endif /* ISP_DEMO_SETUP_H */
