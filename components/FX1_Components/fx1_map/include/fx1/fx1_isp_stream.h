/*
 * FX1 VP: host <-> firmware contract of the ISP RAW-sequence stream
 * (`run_vp.sh isp-stream`; platforms/fx1_soc/docs/ISP_STREAM_CONTRACT.md).
 *
 * VP only, not an ISP or SoC interface: the host prepares the session in DDR
 * before the CPUs run (fx1_soc --load), the firmware (FreeRTOS `isp stream`)
 * runs it, and the platform's host bridge (fx1_soc --isp-stream-spool) captures
 * every completed frame while the simulation runs. Nothing here is an ISP CSR
 * or interrupt. C (the firmware builds C11), shared by the firmware, the platform (C++) and, as a
 * mirror checked by test, platforms/fx1_soc/demo/isp_stream_prep.py.
 *
 * All words are little-endian uint32; the 64-bit counts are {lo, hi} pairs.
 *
 * Ownership of the shared blocks (FX1_FW_SHARED_BASE + 0x100000 ...):
 *   desc     host writes it before boot; read-only afterwards
 *   status   firmware writes; the bridge reads it on every doorbell
 *   control  the bridge writes (stop request); the firmware reads
 *   record i firmware publishes output slot i's frame; the bridge reads it and
 *            writes only `ack_seq`; the firmware reuses output slot i only
 *            once ack_seq == seq
 * Publication order: the firmware writes every field of a record, fences,
 * writes `seq`, then `state` = PUBLISHED, fences, then rings the doorbell
 * (FX1_SIM_CTRL_DOORBELL_OFF, any value). Doorbells may coalesce: the bridge
 * drains records by `seq`, never one per event.
 */
#ifndef FX1_ISP_STREAM_H
#define FX1_ISP_STREAM_H

#include <stdint.h>

#include "fx1/fx1_memory_map.h"

/* ---- DDR layout -------------------------------------------------------------- */
#define FX1_ISP_STREAM_DESC_ADDR     (FX1_FW_SHARED_BASE + 0x100000u) /* 128 B, host -> FW      */
#define FX1_ISP_STREAM_STATUS_ADDR   (FX1_FW_SHARED_BASE + 0x100100u) /* 128 B, FW -> host      */
#define FX1_ISP_STREAM_CONTROL_ADDR  (FX1_FW_SHARED_BASE + 0x100200u) /*  64 B, host -> FW      */
#define FX1_ISP_STREAM_RECORD_ADDR   (FX1_FW_SHARED_BASE + 0x100400u) /* 4 x 128 B, FW <-> host */
#define FX1_ISP_STREAM_RECORD_BYTES  128u
#define FX1_ISP_STREAM_PROFILE_ADDR  (FX1_FW_SHARED_BASE + 0x110000u) /* {offset, value} pairs  */
#define FX1_ISP_STREAM_PROFILE_MAX   32768u                           /* writes: 256 KiB        */
/* SYS_DMA command chains, one per input slot (16-byte descriptors). */
#define FX1_ISP_STREAM_CHAIN_ADDR    (FX1_FW_SHARED_BASE + 0x200000u)
#define FX1_ISP_STREAM_CHAIN_PITCH   0x50000u                         /* 20480 descriptors      */
/* Four input and four output slots of 16 MiB: a 3840x2160 RAW16 frame or NV12. */
#define FX1_ISP_STREAM_SLOTS         4u
#define FX1_ISP_STREAM_SLOT_BYTES    0x1000000u
#define FX1_ISP_STREAM_IN_ADDR(i)    (FX1_FW_RAW_BUF_BASE + (i) * FX1_ISP_STREAM_SLOT_BYTES)
#define FX1_ISP_STREAM_OUT_ADDR(i)   (FX1_FW_YUV_BUF_BASE + (i) * FX1_ISP_STREAM_SLOT_BYTES)
/* UV at the first 1 MiB boundary after Y (as the ISP demo). */
#define FX1_ISP_STREAM_UV_OFFSET(y_bytes) ((((y_bytes) + 0xFFFFFu) / 0x100000u) * 0x100000u)
/* Source pool: the preloaded, immutable RAW frames. Starts 2 MiB into the
 * test area, after the buffers of `dma test` (TEST_AREA + 0x100000 ...). */
#define FX1_ISP_STREAM_POOL_BASE     (FX1_FW_TEST_AREA_BASE + 0x200000u)
#define FX1_ISP_STREAM_POOL_END      0xA0000000u                      /* end exclusive          */
#define FX1_ISP_STREAM_POOL_ALIGN    0x1000u
/* SYS_DMA: channel 1 belongs to the stream (`dma test` uses channel 0). One
 * command moves at most FX1_DMA_MAX_TRANSFER (1023) bytes: a frame is a chain
 * of 1008-byte commands (16-byte multiple). */
#define FX1_ISP_STREAM_DMA_CHANNEL   1u
#define FX1_ISP_STREAM_DMA_CHUNK     1008u

/* ---- identities and codes ---------------------------------------------------- */
#define FX1_ISP_STREAM_DESC_MAGIC    0x53315846u /* "FX1S" */
#define FX1_ISP_STREAM_STATUS_MAGIC  0x54315846u /* "FX1T" */
#define FX1_ISP_STREAM_CONTROL_MAGIC 0x43315846u /* "FX1C" */
#define FX1_ISP_STREAM_RECORD_MAGIC  0x46315846u /* "FX1F" */
#define FX1_ISP_STREAM_VERSION       1u

#define FX1_ISP_STREAM_FLAG_LOOP     0x1u        /* run until a stop request; frames = 0 */

#define FX1_ISP_STREAM_MODE_PROFILE  0u
#define FX1_ISP_STREAM_MODE_LEGACY   1u

/* status.state */
#define FX1_ISP_STREAM_IDLE          0u
#define FX1_ISP_STREAM_PREPARED      1u
#define FX1_ISP_STREAM_RUNNING       2u
#define FX1_ISP_STREAM_DRAINING      3u
#define FX1_ISP_STREAM_STOPPED       4u
#define FX1_ISP_STREAM_ERROR         5u

/* status.stop_reason (STOPPED; ERROR has stop_reason ERROR) */
#define FX1_ISP_STREAM_STOP_NONE     0u
#define FX1_ISP_STREAM_STOP_DONE     1u /* every frame of the schedule completed */
#define FX1_ISP_STREAM_STOP_USER     2u /* control stop request, reason USER     */
#define FX1_ISP_STREAM_STOP_LIMIT    3u /* control stop request, reason LIMIT    */
#define FX1_ISP_STREAM_STOP_ERROR    4u

/* status.error (ERROR); status.detail carries the raw bits or return code */
#define FX1_ISP_STREAM_ERR_NONE      0u
#define FX1_ISP_STREAM_ERR_DESC      1u /* descriptor invalid (detail: field index) */
#define FX1_ISP_STREAM_ERR_BUSY      2u /* ISP or SYS_DMA channel owned elsewhere   */
#define FX1_ISP_STREAM_ERR_PROFILE   3u /* profile not applied / built (TUNE_ERROR) */
#define FX1_ISP_STREAM_ERR_CONFIG    4u /* geometry / start refused (driver code)   */
#define FX1_ISP_STREAM_ERR_COPY      5u /* SYS_DMA error or timeout (int status)    */
#define FX1_ISP_STREAM_ERR_ISP       6u /* DMA_ERR other than end-of-input underrun */
#define FX1_ISP_STREAM_ERR_TIMEOUT   7u /* no completion within the frame budget    */
#define FX1_ISP_STREAM_ERR_HOST      8u /* bridge absent or ACK protocol violated   */

/* control.request / control.reason */
#define FX1_ISP_STREAM_REQ_NONE      0u
#define FX1_ISP_STREAM_REQ_STOP      1u
#define FX1_ISP_STREAM_REASON_USER   1u
#define FX1_ISP_STREAM_REASON_LIMIT  2u

/* record.state */
#define FX1_ISP_STREAM_REC_FREE      0u
#define FX1_ISP_STREAM_REC_PUBLISHED 1u

/* colorimetry words: bit 0 full range, bits [2:1] CSC_CTRL.std, bit 31 valid
 * (as isp_demo_result.colorimetry, ISP-UPD-005). */
#define FX1_ISP_STREAM_COLORIMETRY_VALID 0x80000000u

/* ---- blocks -------------------------------------------------------------------- */
typedef struct fx1_isp_stream_desc {          /* host -> firmware, before boot */
    uint32_t magic, version, size, flags;     /* size = sizeof(fx1_isp_stream_desc) */
    uint32_t session_lo, session_hi;          /* random per run; echoed everywhere */
    uint32_t width, height;                   /* input geometry (pixels) */
    uint32_t bayer;                           /* COMMON_BAYER code; the profile's write must agree */
    uint32_t depth;                           /* COMMON_INPUT_FORMAT.DEPTH: 0 RAW12, 1 RAW10 native */
    uint32_t in_stride, frame_bytes;          /* pool and slot layout: frame_bytes = in_stride * height */
    uint32_t source_count, source_base, source_pitch; /* source k at source_base + k * source_pitch */
    uint32_t repeat;                          /* K >= 1: ordinal n uses source (n / K) mod source_count */
    uint32_t frames_lo, frames_hi;            /* physical frames; 0 with FLAG_LOOP */
    uint32_t queue;                           /* frames in flight: 1, 2 or 4 */
    uint32_t mode;                            /* FX1_ISP_STREAM_MODE_* */
    uint32_t profile_count, profile_id;       /* writes at PROFILE_ADDR; ID sealed (0 reads as 1) */
    uint32_t out_stride;                      /* CH0 Y and UV stride (bytes) */
    uint32_t frame_budget_us;                 /* simulated time a frame may take to complete */
    uint32_t reserved[8];                     /* 0 */
} fx1_isp_stream_desc;                        /* 128 bytes */

typedef struct fx1_isp_stream_status {        /* firmware -> host */
    uint32_t magic, version, state, stop_reason;
    uint32_t error, detail;
    uint32_t session_lo, session_hi;
    uint32_t submitted_lo, submitted_hi;      /* frames queued to the ISP */
    uint32_t completed_lo, completed_hi;      /* frames published (every completion is published) */
    uint32_t cancelled;                       /* scheduled but never submitted after a stop */
    uint32_t failed;                          /* submitted, lost in a recovery or an error */
    uint32_t out_width, out_height, out_stride;
    uint32_t colorimetry;                     /* committed before start, fixed for the session */
    uint32_t dma_err;                         /* COMMON DMA_ERR seen at the end (underrun included) */
    uint32_t idma_frames, odma_frames;        /* ISP counter deltas of the session */
    uint32_t publish_seq;                     /* seq of the last published record */
    uint32_t elapsed_ticks;                   /* mtime ticks from start to the last completion */
    uint32_t reserved[9];
} fx1_isp_stream_status;                      /* 128 bytes */

typedef struct fx1_isp_stream_control {       /* host bridge -> firmware */
    uint32_t magic, version;
    uint32_t request, reason;                 /* FX1_ISP_STREAM_REQ_*, FX1_ISP_STREAM_REASON_* */
    uint32_t session_lo, session_hi;          /* must match the descriptor's */
    uint32_t reserved[10];
} fx1_isp_stream_control;                     /* 64 bytes */

typedef struct fx1_isp_stream_record {        /* firmware -> host, one per output slot */
    uint32_t magic, state, seq, ack_seq;      /* ack_seq: written by the host only */
    uint32_t session_lo, session_hi;
    uint32_t ordinal_lo, ordinal_hi;          /* 0-based physical frame number */
    uint32_t source_index, repeat_index;      /* source k, repeat r (0..K-1) */
    uint32_t driver_seq, tag;                 /* fx1_isp_queue_frame_meta seq and tag */
    uint32_t frame_id, profile_id;            /* TUNE_CH0_BUFn_FRAME / _PROFILE */
    uint32_t ts, sof_ts, eof_ts;              /* FRAME_COUNTER values (fx1_isp_frame_meta) */
    uint32_t slot;                            /* output slot index (= record index) */
    uint32_t y_addr, uv_addr;
    uint32_t width, height, stride;           /* CH0 active geometry and stride */
    uint32_t colorimetry;
    uint32_t status;                          /* 0: a completed, valid frame */
    uint32_t reserved[7];
} fx1_isp_stream_record;                      /* 128 bytes */

#if defined(__cplusplus)
static_assert(sizeof(fx1_isp_stream_desc) == 128, "fx1_isp_stream_desc");
static_assert(sizeof(fx1_isp_stream_status) == 128, "fx1_isp_stream_status");
static_assert(sizeof(fx1_isp_stream_control) == 64, "fx1_isp_stream_control");
static_assert(sizeof(fx1_isp_stream_record) == FX1_ISP_STREAM_RECORD_BYTES, "fx1_isp_stream_record");
#else
typedef char fx1_isp_stream_desc_size[sizeof(fx1_isp_stream_desc) == 128 ? 1 : -1];
typedef char fx1_isp_stream_status_size[sizeof(fx1_isp_stream_status) == 128 ? 1 : -1];
typedef char fx1_isp_stream_control_size[sizeof(fx1_isp_stream_control) == 64 ? 1 : -1];
typedef char fx1_isp_stream_record_size[sizeof(fx1_isp_stream_record) == FX1_ISP_STREAM_RECORD_BYTES ? 1 : -1];
#endif

#endif /* FX1_ISP_STREAM_H */
