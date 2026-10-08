/*
 * G4a / plan P4 (review G4a-R2): statistics publications carry the right
 * hardware FRAME_ID for the frame they describe.
 *
 * Profile isp_fx_profile_stats = basic + AEC (4x3 zones of 16x16) + AWB
 * (global) + AF, distinct context IDs. Frames are run one at a time (queue,
 * wait for the ISR-signalled completion, read the publication) so the result
 * registers are stable when read and belong to exactly that frame. For every
 * frame k the driver's read-tag-read accessors must return the reference's
 * AEC/AWB/AF FRAME_ID (1..8 after reset), RESULT_CONTEXT_ID, and the AEC
 * global sums and counts per channel; the NV12 must still match basic's CRC.
 */
#include "isp_common.h"

static fx1_isp_dev dev;
static volatile uint32_t notified, dma_errors, last_dma_err;

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
        fx1_isp_irq(&dev, &ev);
        for (unsigned k = 0; k < ev.frames_done && k < dev.in_flight; ++k)
            notified |= 1u << dev.queue[k].seq;
        if (ev.dma_err & ISP_DMA_ERR_FATAL) {
            ++dma_errors;
            last_dma_err = ev.dma_err;
        }
    }
    if (id) fx1_plic_complete(0, id);
    return mepc;
}

static void expect_eq(uint32_t got, uint32_t want, uint32_t code, const char* what)
{
    if (got == want) return;
    fx1_log_hex(what, got);
    fx1_log_hex("  expected", want);
    fx1_fail(code);
}

int main(void)
{
    fx1_isp_buffers bufs;
    isp_buffers(&bufs);
    fx1_check(fx1_isp_init(&dev, &isp_hal) == FX1_ISP_OK, 0x01, "driver init");
    fx1_check(fx1_isp_set_geometry(&dev, ISP_FX_WIDTH, ISP_FX_HEIGHT, ISP_FX_BAYER) == FX1_ISP_OK, 0x02,
              "geometry 64x48 RGGB");
    fx1_isp_apply(&dev, isp_fx_profile_stats, sizeof(isp_fx_profile_stats) / sizeof(isp_fx_profile_stats[0]));

    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_ISP, 1);
    fx1_plic_enable(0, FX1_IRQ_ISP, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
    isp_outputs_to_device(&bufs);
    fx1_check(fx1_isp_start(&dev, &bufs) == FX1_ISP_OK, 0x03, "stream start");
    fx1_log("ISP statistics: AEC 4x3, AWB global, AF; one frame at a time");

    for (uint32_t k = 0; k < ISP_FX_FRAMES; ++k) {
        uint32_t saved = fx1_irq_save();
        fx1_check(fx1_isp_input_ready(&dev), 0x10, "input ready");
        isp_fill_input(bufs.in[fx1_isp_next_input(&dev)], k);
        uint32_t seq = 0;
        fx1_check(fx1_isp_queue_frame_ex(&dev, &seq) == FX1_ISP_OK, 0x11, "frame queued");
        fx1_irq_restore(saved);

        const uint64_t start = fx1_mtime();
        while (!(notified & (1u << seq))) {
            fx1_check(fx1_mtime() - start < FX1_MTIME_HZ / 10u, 0x12, "completion within 100 ms");
            if (dma_errors) {
                fx1_log_hex("ISP DMA_ERR", last_dma_err);
                fx1_fail(0x13);
            }
        }
        unsigned index = 0;
        uint32_t done_seq = 0;
        saved = fx1_irq_save();
        const int got = fx1_isp_next_done_ex(&dev, &index, &done_seq);
        fx1_irq_restore(saved);
        fx1_check(got == FX1_ISP_OK && done_seq == seq, 0x14, "the signalled frame is the one consumed");
        isp_output_to_cpu(bufs.y[index], bufs.uv[index]);
        expect_eq(isp_nv12_crc(bufs.y[index], bufs.uv[index]), isp_fx_crc_basic[k], 0x20 + k,
                  "NV12 CRC (statistics must not change the image)");
        isp_output_to_device(bufs.y[index], bufs.uv[index]);

        /* The publication of frame k: nothing else is queued, so it is stable. */
        const isp_fx_publication* e = &isp_fx_stats_expected[k];
        fx1_isp_aec_global aec;
        fx1_isp_awb_global awb;
        fx1_isp_af_scores af;
        fx1_check(fx1_isp_read_aec_global(&dev, &aec) == FX1_ISP_OK, 0x30, "AEC global read");
        fx1_check(fx1_isp_read_awb_global(&dev, &awb) == FX1_ISP_OK, 0x31, "AWB global read");
        fx1_check(fx1_isp_read_af(&dev, &af) == FX1_ISP_OK, 0x32, "AF read");
        expect_eq(aec.frame_id, e->aec_frame_id, 0x40 + k, "AEC_FRAME_ID");
        expect_eq(awb.frame_id, e->awb_frame_id, 0x50 + k, "AWB_FRAME_ID");
        expect_eq(af.frame_id, e->af_frame_id, 0x60 + k, "AF_FRAME_ID");
        expect_eq(aec.context_id, e->aec_ctx, 0x70, "AEC_RESULT_CONTEXT_ID");
        expect_eq(awb.context_id, e->awb_ctx, 0x71, "AWB_RESULT_CONTEXT_ID");
        expect_eq(af.context_id, e->af_ctx, 0x72, "AF_RESULT_CONTEXT_ID");
        for (unsigned c = 0; c < 4; ++c) {
            expect_eq((uint32_t)aec.sum[c], (uint32_t)e->aec_sum[c], 0x80 + k, "AEC global sum (low)");
            expect_eq((uint32_t)(aec.sum[c] >> 32), (uint32_t)(e->aec_sum[c] >> 32), 0x80 + k,
                      "AEC global sum (high)");
            expect_eq(aec.count[c], e->aec_count[c], 0x90 + k, "AEC global count");
        }
    }
    fx1_log_hex("last AEC/AWB/AF FRAME_ID", isp_fx_stats_expected[ISP_FX_FRAMES - 1].aec_frame_id);
    fx1_check(fx1_isp_stop(&dev) >= 0, 0xA0, "stream stopped");
    fx1_log("8 frames: FRAME_ID 1..8, context tags and AEC global sums match the reference");
    return 0;
}
