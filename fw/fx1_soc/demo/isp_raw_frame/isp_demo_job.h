/*
 * Host <-> firmware contract of the ISP RAW-frame demo
 * (platforms/fx1_soc/demo/run_isp_demo.py writes the job and the profile with
 * fx1_soc --load, and reads the result with --dump). All little-endian.
 *
 *   FX1_FW_SHARED_BASE + 0x0000  isp_demo_job      (host -> firmware)
 *   FX1_FW_SHARED_BASE + 0x0100  isp_demo_result   (firmware -> host)
 *   FX1_FW_SHARED_BASE + 0x1000  profile: profile_count x {u32 offset, u32 value},
 *                                ISP CSR writes applied in order (a .csrw file)
 *   in_addr / y_addr / uv_addr   RAW input (ISP 16-bit container) and NV12 output
 *
 * Version 2 (V107-10): the job selects the ISP mode and the output channels.
 * Profile mode is the default (CSR v1.07, MAS §9.7-9.9: the profile is
 * applied, then built into bank 0, sealed and applied); legacy mode is kept
 * for comparison. A version-1 job (64 bytes) still runs, in legacy mode,
 * CH0 only. The result block is version 2 in both cases.
 */
#ifndef ISP_DEMO_JOB_H
#define ISP_DEMO_JOB_H

#include <stdint.h>

#define ISP_DEMO_JOB_OFFSET     0x0000u
#define ISP_DEMO_RESULT_OFFSET  0x0100u
#define ISP_DEMO_PROFILE_OFFSET 0x1000u
#define ISP_DEMO_JOB_MAGIC      0x4A315846u /* "FX1J" */
#define ISP_DEMO_RESULT_MAGIC   0x52315846u /* "FX1R" */
#define ISP_DEMO_VERSION        2u
#define ISP_DEMO_VERSION_V1     1u /* still accepted: legacy mode, CH0 */
#define ISP_DEMO_MODE_PROFILE   0u
#define ISP_DEMO_MODE_LEGACY    1u

typedef struct isp_demo_job {
    uint32_t magic, version;
    uint32_t width, height, bayer; /* bayer: COMMON_BAYER code (RGGB 0, GRBG 1, GBRG 2, BGGR 3) */
    uint32_t in_addr, in_stride;   /* RAW input, 16-bit samples, stride in bytes */
    uint32_t y_addr, uv_addr, out_stride;
    uint32_t profile_count;        /* {offset, value} pairs at ISP_DEMO_PROFILE_OFFSET */
    uint32_t frames;               /* total physical frames; auto CLI includes one AWB probe */
    /* version 2 (a version-1 job ends here, with four zero words) */
    uint32_t mode;                 /* ISP_DEMO_MODE_PROFILE (0, default) or ISP_DEMO_MODE_LEGACY */
    uint32_t outputs;              /* OUTPUT_ENABLE: bit c = CHc; CH0 required (0 reads as CH0) */
    uint32_t profile_id;           /* profile mode: ID sealed into bank 0 (0 reads as 1) */
    uint32_t ch1_y_addr, ch1_uv_addr, ch1_stride; /* CH1 NV12, when enabled */
    uint32_t ch2_addr, ch2_stride;  /* CH2 packed RGB888 (stride >= 3 x width), when enabled */
    uint32_t reserved[4];
} isp_demo_job;                    /* 96 bytes (version 1: the first 64) */

typedef struct isp_demo_result {
    uint32_t magic, status; /* status 0 = frames completed */
    uint32_t out_width, out_height, out_stride;
    /* Completion/counter deltas for this job, including an auto probe. */
    uint32_t frames_done, idma_frames, odma_frames, dma_err;
    uint32_t aec_frame_id, awb_frame_id, af_frame_id;
    uint32_t frame_ticks;   /* mtime ticks (50 ns) from the first queue to the last completion */
    /* version 2 */
    uint32_t mode;          /* the mode the job ran in (ISP_DEMO_MODE_*) */
    uint32_t outputs;       /* the channels written */
    uint32_t profile_id;    /* profile mode: the active profile ID of the outputs (0 legacy) */
    uint32_t tune_error;    /* profile build failure: the TUNE_ERROR bits (status 2) */
    uint32_t ch1_width, ch1_height, ch2_width, ch2_height;
    uint32_t reserved[3];
} isp_demo_result;          /* status 0 done, 1 frames failed, 2 profile not built */

#endif /* ISP_DEMO_JOB_H */
