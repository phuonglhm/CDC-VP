/*
 * G4b / plan C7: LR/SC reservations against every master that can write DDR.
 *
 *   1  SC without a reservation fails and does not write
 *   2  LR/SC succeeds once; a second SC fails; SC to another granule fails
 *   3  LR, SYS_DMA writes the reserved word, SC fails; the DMA data stays
 *   4  ... the DMA writes another word of the same 64-byte granule: SC fails
 *   5  ... the DMA writes the next granule: SC succeeds
 *   6  LR, hart 1 stores to the reserved word, SC fails; hart 1's data stays
 *   7  ... hart 1 stores to the next granule: SC succeeds
 *   8  LR on an ISP output word, the ISP writes the frame, SC fails; the NV12
 *      is still the reference's
 *
 * Interrupts stay globally masked; a hart waits in WFI for the device's
 * interrupt (or hart 1's IPI) between LR and SC, so the LR..SC distance is a
 * few instructions (at_lr_wait_sc). The DMA's real write is chained behind a
 * padding copy, so it lands after the LR (checked, 0x0D2).
 *
 * No --no-exclusive-monitor control: upstream VP++ drops an LR reservation
 * after 17 instructions, so the SCs here fail there for an unrelated reason.
 */
#include "atomic_common.h"
#include "isp_common.h"

#define MIP_MSIP (1u << FX1_IRQ_M_SOFT)
#define MIP_MEIP (1u << FX1_IRQ_M_EXT)

/* Hart 1 command block (.bss, in the image, far from the words under test). */
static volatile uint32_t h1_cmd, h1_addr, h1_value, h1_done;
static volatile uint64_t h1_at;

void secondary_main(uint32_t hart)
{
    (void)hart;
    uint32_t seen = 0;
    for (;;) {
        while (h1_cmd == seen) {
        }
        seen = h1_cmd;
        fx1_acquire();
        while (fx1_mtime() < h1_at) {
        }
        at_write(h1_addr, h1_value);
        fx1_mb();
        fx1_set_msip(0, 1); /* wake hart 0 out of its WFI */
        h1_done = seen;
    }
}

static void expect(int ok, uint32_t code, const char* what)
{
    fx1_check(ok, code, what);
}

/* LR at `word`, the DMA copies `value` to `target` meanwhile, SC `sc_value`. */
static uint32_t lr_dma_sc(uint32_t word, uint32_t target, uint32_t value, uint32_t sc_value)
{
    at_write(AT_SRC, value);
    at_dma_start(AT_SRC, target, 4, AT_PAD_BYTES);
    uint32_t lr_value = 0;
    const uint32_t failed = at_lr_wait_sc(word, sc_value, MIP_MEIP, &lr_value);
    expect(at_dma_done(), 0x0D1, "DMA completed when the hart woke");
    at_dma_ack();
    at_plic_drain(0);
    fx1_dma_complete((const volatile void*)target, 4, FX1_DMA_FROM_DEVICE);
    expect(lr_value != value || target != word, 0x0D2, "the LR ran before the DMA wrote");
    return failed;
}

static uint32_t lr_hart1_sc(uint32_t word, uint32_t target, uint32_t value, uint32_t sc_value)
{
    static uint32_t cmd;
    h1_addr = target;
    h1_value = value;
    h1_at = fx1_mtime() + FX1_MTIME_HZ / 100000u; /* 10 us from now: after the LR */
    fx1_release();
    h1_cmd = ++cmd;
    uint32_t lr_value = 0;
    const uint32_t failed = at_lr_wait_sc(word, sc_value, MIP_MSIP, &lr_value);
    fx1_set_msip(0, 0);
    while (h1_done != cmd) {
    }
    fx1_acquire();
    return failed;
}

static void isp_case(uint32_t sc_value)
{
    static fx1_isp_dev dev;
    fx1_isp_buffers bufs;
    isp_buffers(&bufs);
    expect(fx1_isp_init(&dev, &isp_hal) == FX1_ISP_OK, 0x80, "8: ISP init");
    expect(fx1_isp_set_geometry(&dev, ISP_FX_WIDTH, ISP_FX_HEIGHT, ISP_FX_BAYER) == FX1_ISP_OK, 0x81,
           "8: geometry");
    fx1_isp_apply(&dev, isp_fx_profile_basic, sizeof(isp_fx_profile_basic) / sizeof(isp_fx_profile_basic[0]));
    fx1_plic_set_priority(FX1_IRQ_ISP, 1);
    fx1_plic_enable(0, FX1_IRQ_ISP, 1);
    const uint32_t word = (uint32_t)bufs.y[0]; /* first output buffer, first Y word */
    at_write(word, 0);
    isp_outputs_to_device(&bufs);
    expect(fx1_isp_start(&dev, &bufs) == FX1_ISP_OK, 0x82, "8: stream start");

    const unsigned slot = fx1_isp_next_input(&dev);
    isp_fill_input(bufs.in[slot], 0);
    /* Deliberately outside the C12 ownership contract: the CPU reserves a word
     * of a buffer the ODMA owns, which is exactly the conflict under test. */
    (void)at_lr(word);
    expect(fx1_isp_queue_frame(&dev) == FX1_ISP_OK, 0x83, "8: frame queued");
    /* Interrupts are masked: poll the ISP's line and acknowledge at the device. */
    unsigned index = 0;
    const uint64_t start = fx1_mtime();
    while (fx1_isp_next_done(&dev, &index) != FX1_ISP_OK) {
        if (fx1_read_mip() & MIP_MEIP) {
            fx1_isp_events ev;
            fx1_isp_irq(&dev, &ev);
            expect(!(ev.dma_err & ISP_DMA_ERR_FATAL), 0x84, "8: no fatal ISP DMA error");
            at_plic_drain(0);
        }
        expect(fx1_mtime() - start < FX1_MTIME_HZ / 100u, 0x85, "8: frame within 10 ms");
    }
    expect(index == 0, 0x86, "8: output buffer 0");
    expect(at_sc(word, sc_value) != 0, 0x87, "8: SC after the ISP wrote the reserved word fails");
    isp_output_to_cpu(bufs.y[0], bufs.uv[0]);
    expect(at_read(word) != sc_value, 0x88, "8: the failed SC did not write");
    expect(isp_nv12_crc(bufs.y[0], bufs.uv[0]) == isp_fx_crc_basic[0], 0x89, "8: NV12 matches the reference");
    expect(fx1_isp_stop(&dev) >= 0, 0x8A, "8: stream stopped");
    fx1_plic_enable(0, FX1_IRQ_ISP, 0);
    at_plic_drain(0);
}

int main(void)
{
    fx1_global_irq_disable();
    fx1_irq_enable(FX1_MIE_MEIE | FX1_MIE_MSIE); /* WFI wake-ups only: MIE stays 0 */
    fx1_plic_set_priority(FX1_IRQ_SYS_DMA, 1);
    fx1_plic_enable(0, FX1_IRQ_SYS_DMA, 1);
    fx1_release_secondaries();

    /* 1 */
    at_write(AT_WORD(0), 0x11111111u);
    expect(at_sc(AT_WORD(0), 0xBADu) != 0, 0x10, "1: SC without a reservation fails");
    expect(at_read(AT_WORD(0)) == 0x11111111u, 0x11, "1: ... and does not write");
    fx1_log("1: SC without a reservation fails, memory unchanged");

    /* 2 */
    expect(at_lr(AT_WORD(0)) == 0x11111111u, 0x20, "2: LR reads the word");
    expect(at_sc(AT_WORD(0), 0x22222222u) == 0, 0x21, "2: SC after LR succeeds");
    expect(at_read(AT_WORD(0)) == 0x22222222u, 0x22, "2: ... and writes");
    expect(at_sc(AT_WORD(0), 0xBADu) != 0, 0x23, "2: a second SC fails (reservation consumed)");
    (void)at_lr(AT_WORD(0));
    expect(at_sc(AT_WORD(1), 0xBADu) != 0, 0x24, "2: SC to another granule than the LR fails");
    expect(at_read(AT_WORD(1)) != 0xBADu && at_sc(AT_WORD(0), 0xBADu) != 0, 0x25,
           "2: ... writes nothing and consumed the reservation");
    fx1_log("2: LR/SC pairs, consumed and mismatched reservations");

    /* 3 */
    at_write(AT_WORD(2), 0x33333333u);
    expect(lr_dma_sc(AT_WORD(2), AT_WORD(2), 0xD3A00003u, 0xC0FFEE03u) != 0, 0x31,
           "3: SYS_DMA wrote the reserved word between LR and SC: SC fails");
    expect(at_read(AT_WORD(2)) == 0xD3A00003u, 0x32, "3: the DMA data survives");
    fx1_log("3: SYS_DMA write to the reserved word cancels the reservation");

    /* 4 */
    at_write(AT_WORD(3), 0x44444444u);
    expect(lr_dma_sc(AT_WORD(3), AT_WORD(3) + AT_GRANULE - 4u, 0xD3A00004u, 0xC0FFEE04u) != 0, 0x41,
           "4: SYS_DMA wrote the same granule: SC fails");
    expect(at_read(AT_WORD(3)) == 0x44444444u && at_read(AT_WORD(3) + AT_GRANULE - 4u) == 0xD3A00004u,
           0x42, "4: reserved word untouched, DMA word written");
    fx1_log("4: SYS_DMA write elsewhere in the 64-byte granule cancels it");

    /* 5 */
    at_write(AT_WORD(4), 0x55555555u);
    expect(lr_dma_sc(AT_WORD(4), AT_WORD(5), 0xD3A00005u, 0xC0FFEE05u) == 0, 0x51,
           "5: SYS_DMA wrote the next granule: SC succeeds");
    expect(at_read(AT_WORD(4)) == 0xC0FFEE05u && at_read(AT_WORD(5)) == 0xD3A00005u, 0x52,
           "5: both writes landed");
    fx1_log("5: SYS_DMA write to the next granule keeps the reservation");
    fx1_plic_enable(0, FX1_IRQ_SYS_DMA, 0);

    /* 6 */
    at_write(AT_WORD(6), 0x66666666u);
    expect(lr_hart1_sc(AT_WORD(6), AT_WORD(6), 0x4A170006u, 0xC0FFEE06u) != 0, 0x61,
           "6: hart 1 stored to the reserved word between LR and SC: SC fails");
    expect(at_read(AT_WORD(6)) == 0x4A170006u, 0x62, "6: hart 1's data survives");
    fx1_log("6: the other hart's store cancels the reservation");

    /* 7 */
    at_write(AT_WORD(7), 0x77777777u);
    expect(lr_hart1_sc(AT_WORD(7), AT_WORD(8), 0x4A170007u, 0xC0FFEE07u) == 0, 0x71,
           "7: hart 1 stored to the next granule: SC succeeds");
    expect(at_read(AT_WORD(7)) == 0xC0FFEE07u && at_read(AT_WORD(8)) == 0x4A170007u, 0x72,
           "7: both writes landed");
    fx1_log("7: the other hart's store to the next granule keeps it");

    /* 8 */
    isp_case(0xC0FFEE08u);
    fx1_log("8: ISP ODMA output over the reserved word cancels the reservation");
    return 0;
}
