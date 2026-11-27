/* SPDX-License-Identifier: Apache-2.0 */
/*
 * FX1 ISP_INGRESS: VP-only register block of the direct CSI-2 -> ISP path.
 *
 * NOT part of the FTEL ISP v1.07 or CSI-2 RX register maps: neither IP has a
 * pixel-source select (direct CSI->ISP contract, P20). The block selects the
 * ISP's pixel source (memory = IDMA, the reset value; direct = the CSI-2 RX
 * AXI4-Stream) and reports the per-capture context of direct frames.
 * Offsets are relative to the block base (fx1_soc: FX1_ISP_INGRESS_BASE,
 * 0x1105_0000, PERIBUS_1, 64 KiB). 32-bit accesses only.
 *
 * Contract: components/FX1_Components/mipi-csi2/docs/DIRECT_CSI_ISP_CONTRACT.md
 * (§6 register map, §7 frame acceptance, §9 context entries).
 */
#ifndef FX1_ISP_INGRESS_REGS_H
#define FX1_ISP_INGRESS_REGS_H

#define FX1_ISP_INGRESS_ID_OFFSET 0x000u
#define FX1_ISP_INGRESS_ID_VALUE 0x49535049u /* 'ISPI' */

#define FX1_ISP_INGRESS_CAPABILITY_OFFSET 0x004u
#define FX1_ISP_INGRESS_CAPABILITY_BOUND_MASK 0x00000001u /* a CSI stream is bound */
#define FX1_ISP_INGRESS_CAPABILITY_VERSION_SHIFT 4u
#define FX1_ISP_INGRESS_CAPABILITY_VERSION_MASK 0x000000F0u
#define FX1_ISP_INGRESS_CAPABILITY_FIFO_BEATS_SHIFT 8u
#define FX1_ISP_INGRESS_CAPABILITY_FIFO_BEATS_MASK 0x0000FF00u
#define FX1_ISP_INGRESS_CAPABILITY_CTX_DEPTH_SHIFT 16u
#define FX1_ISP_INGRESS_CAPABILITY_CTX_DEPTH_MASK 0x000F0000u
#define FX1_ISP_INGRESS_CAPABILITY_MAX_LINE_SHIFT 20u
#define FX1_ISP_INGRESS_CAPABILITY_MAX_LINE_MASK 0xFFF00000u

/* CTRL: SOURCE changes only while idle and drained (STATUS.SWITCH_REFUSED
 * otherwise); direct also needs DMA_CTRL.IDMA_EN = 0. */
#define FX1_ISP_INGRESS_CTRL_OFFSET 0x008u
#define FX1_ISP_INGRESS_CTRL_SOURCE_MASK 0x00000001u /* 0 = memory (reset), 1 = direct */
#define FX1_ISP_INGRESS_CTRL_EXPECTED_TID_SHIFT 4u
#define FX1_ISP_INGRESS_CTRL_EXPECTED_TID_MASK 0x00000030u

#define FX1_ISP_INGRESS_STATUS_OFFSET 0x00Cu
#define FX1_ISP_INGRESS_STATUS_IDLE_MASK 0x00000001u          /* RO */
#define FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK 0x00000002u      /* RO */
#define FX1_ISP_INGRESS_STATUS_DIRECT_MASK 0x00000004u        /* RO: SOURCE = direct in effect */
#define FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK 0x00000100u /* W1C */
#define FX1_ISP_INGRESS_STATUS_CTX_FULL_MASK 0x00000200u      /* W1C */
#define FX1_ISP_INGRESS_STATUS_NOT_BOUND_MASK 0x00000400u     /* W1C */
#define FX1_ISP_INGRESS_STATUS_STICKY_MASK 0x00000700u

/* Counters: RO, 32-bit, wrap; cleared by reset only. */
#define FX1_ISP_INGRESS_BEATS_ACCEPTED_OFFSET 0x010u   /* direct beats taken for an admitted frame */
#define FX1_ISP_INGRESS_FRAMES_ADMITTED_OFFSET 0x014u
#define FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET 0x018u   /* ISP admission or ODMA buffer refused at the SOF */
#define FX1_ISP_INGRESS_DROP_NO_CONTEXT_OFFSET 0x01Cu  /* SOF of an untracked capture */
#define FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET 0x020u   /* tracked capture that never sent an SOF */
#define FX1_ISP_INGRESS_DROP_CONFIG_OFFSET 0x024u      /* input geometry / depth not legal at the SOF */
#define FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET 0x028u   /* admitted, then invalid (cause counters 0x080..) */
#define FX1_ISP_INGRESS_CAPTURES_UNTRACKED_OFFSET 0x02Cu
#define FX1_ISP_INGRESS_BEATS_DISCARDED_OFFSET 0x030u  /* memory mode, TID, dropped / invalid frames */
#define FX1_ISP_INGRESS_STALL_CYCLES_OFFSET 0x034u     /* calls with TVALID = 1 answered TREADY = 0 */
#define FX1_ISP_INGRESS_LAST_DROP_OFFSET 0x038u        /* newest frame not admitted */
#define FX1_ISP_INGRESS_LAST_DROP_REASON_MASK 0x0000000Fu
#define FX1_ISP_INGRESS_LAST_DROP_FRAME_ID_SHIFT 16u
#define FX1_ISP_INGRESS_LAST_DROP_FRAME_ID_MASK 0x3FFF0000u

/* Not-admitted reasons (LAST_DROP.REASON). */
#define FX1_ISP_INGRESS_DROP_REASON_NO_CREDIT 1u
#define FX1_ISP_INGRESS_DROP_REASON_NO_CONTEXT 2u
#define FX1_ISP_INGRESS_DROP_REASON_NO_STREAM 3u
#define FX1_ISP_INGRESS_DROP_REASON_CONFIG 4u

/* Head of the READY context entries (admission order); valid while
 * CTX_COUNT.READY != 0. POP frees it. */
#define FX1_ISP_INGRESS_CTX_TAG_OFFSET 0x040u    /* = ODMA BUF_TAG of the frame */
#define FX1_ISP_INGRESS_CTX_SEQ_LO_OFFSET 0x044u /* 64-bit CSI capture sequence */
#define FX1_ISP_INGRESS_CTX_SEQ_HI_OFFSET 0x048u
#define FX1_ISP_INGRESS_CTX_FRAME_OFFSET 0x04Cu
#define FX1_ISP_INGRESS_CTX_FRAME_FRAME_ID_MASK 0x00003FFFu /* CSI FRAME_ID (= ODMA BUF_TS) */
#define FX1_ISP_INGRESS_CTX_FRAME_EPOCH_SHIFT 16u
#define FX1_ISP_INGRESS_CTX_FRAME_EPOCH_MASK 0xFFFF0000u
#define FX1_ISP_INGRESS_CTX_TS_LO_OFFSET 0x050u /* 64-bit FS timestamp, ns */
#define FX1_ISP_INGRESS_CTX_TS_HI_OFFSET 0x054u
#define FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET 0x058u
#define FX1_ISP_INGRESS_CTX_OUTCOME_CODE_MASK 0x0000000Fu
#define FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_SHIFT 8u
#define FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_MASK 0x0000FF00u
#define FX1_ISP_INGRESS_CTX_LINES_OFFSET 0x05Cu   /* complete lines received */
#define FX1_ISP_INGRESS_CTX_SAMPLES_OFFSET 0x060u /* samples received */
#define FX1_ISP_INGRESS_CTX_COUNT_OFFSET 0x064u
#define FX1_ISP_INGRESS_CTX_COUNT_READY_MASK 0x0000000Fu
#define FX1_ISP_INGRESS_CTX_COUNT_USED_SHIFT 8u
#define FX1_ISP_INGRESS_CTX_COUNT_USED_MASK 0x00000F00u
#define FX1_ISP_INGRESS_CTX_POP_OFFSET 0x068u /* W1 bit 0 */
/* ISP frame ID of the frame (assigned at the pipeline's accepted SOF; the ID
 * its statistics (AEC/AWB/AF_FRAME_ID) and TUNE_CHx_BUFn_FRAME carry); 0: the
 * frame never reached the ISP. Firmware drops statistics of a non-OK capture. */
#define FX1_ISP_INGRESS_CTX_ISP_FRAME_OFFSET 0x06Cu

/* Outcome codes (CTX_OUTCOME.CODE), in precedence order when several apply. */
#define FX1_ISP_INGRESS_OUTCOME_OK 1u
#define FX1_ISP_INGRESS_OUTCOME_RESET 2u          /* CSI or ISP reset while admitted */
#define FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED 3u    /* capture_end(DROPPED) */
#define FX1_ISP_INGRESS_OUTCOME_CSI_MISSING_FE 4u /* capture_end(MISSING_FE) */
#define FX1_ISP_INGRESS_OUTCOME_INVALID 5u        /* stream validation failed: CAUSE */
#define FX1_ISP_INGRESS_OUTCOME_ISP_FAILED 6u     /* an enabled output did not complete */

/* Invalid-frame causes (CTX_OUTCOME.CAUSE; counter at 0x080 + 4 * (cause - 1)). */
#define FX1_ISP_INGRESS_CAUSE_LINE_LENGTH 1u   /* a line not exactly WIDTH samples */
#define FX1_ISP_INGRESS_CAUSE_EXTRA_LINES 2u   /* data after HEIGHT lines */
#define FX1_ISP_INGRESS_CAUSE_MISSING_LINES 3u /* SOF before HEIGHT lines */
#define FX1_ISP_INGRESS_CAUSE_KEEP 4u          /* TKEEP odd byte count or not a prefix */
#define FX1_ISP_INGRESS_CAUSE_DEPTH 5u         /* a sample above the configured depth */
#define FX1_ISP_INGRESS_CAUSE_TUSER_ERR 6u     /* TUSER[1] on a data beat */
#define FX1_ISP_INGRESS_CAUSE_ABORT 7u         /* null abort beat */
#define FX1_ISP_INGRESS_CAUSE_TAG_MISMATCH 8u  /* beat of another capture inside the frame */
#define FX1_ISP_INGRESS_NUM_CAUSES 8u
#define FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(cause) (0x080u + 4u * ((cause) - 1u))

#define FX1_ISP_INGRESS_LAST_DROP_SEQ_LO_OFFSET 0x0A0u
#define FX1_ISP_INGRESS_LAST_DROP_SEQ_HI_OFFSET 0x0A4u

#define FX1_ISP_INGRESS_FIFO_BEATS 16u
#define FX1_ISP_INGRESS_CTX_DEPTH 8u
#define FX1_ISP_INGRESS_MAX_LINE_SAMPLES 3842u

#endif /* FX1_ISP_INGRESS_REGS_H */
