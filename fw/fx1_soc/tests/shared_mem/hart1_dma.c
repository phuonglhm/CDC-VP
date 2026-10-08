/*
 * G4b / plan C12: shared memory under concurrent masters.
 *
 * Linked into the isp_stream image built with ISP_FX_HART1_DMA=1: while hart 0
 * streams eight ISP frames (RAW -> IDMA -> ODMA -> NV12, checked against the
 * reference CRC), hart 1 runs SYS_DMA M2M copies in a loop in its own DDR
 * region (the test area, disjoint from the RAW/YUV buffer regions):
 *   CPU writes the source -> fx1_dma_prepare (source TO_DEVICE, destination
 *   FROM_DEVICE) -> DMA copy -> completion (polled; hart 1 owns channel 0, no
 *   PLIC enable) -> fx1_dma_complete (destination) -> CPU compares every byte.
 * Both harts, the ISP's two masters and SYS_DMA share the fabric and DDR at
 * the same time. Each IP is driven by one hart only (no ownership hand-over).
 *
 * Overlap is measured, not assumed (review G4b-R2): hart 0 snapshots hart 1's
 * copy counter when the first frame is queued and at every frame completion.
 * Hart 1 must have made progress in every one of the eight intervals, and must
 * still be running when the stream ends. A hart 1 that finished its copies
 * before the first frame (or stalled during the stream) fails 0x62/0x63/0x64.
 */
#include "atomic_common.h"

#define H1_SRC   (FX1_FW_TEST_AREA_BASE + 0x10000u)
#define H1_DST   (FX1_FW_TEST_AREA_BASE + 0x20000u) /* 4 slots of 1 KiB */
#define H1_BYTES 1020u

#define MARKS 9u /* first queue + 8 completions */

static volatile uint32_t h1_stop, h1_copies, h1_errors, h1_exited;
static uint32_t marks[MARKS], marked;

void secondary_main(uint32_t hart)
{
    (void)hart;
    uint32_t n = 0;
    while (!h1_stop) {
        const uint32_t dst = H1_DST + (n & 3u) * 0x400u;
        for (uint32_t i = 0; i < H1_BYTES; ++i) *(volatile uint8_t*)(H1_SRC + i) = (uint8_t)(i * 7u + n);
        at_dma_start(H1_SRC, dst, H1_BYTES, 0); /* includes both fx1_dma_prepare calls */
        const uint64_t start = fx1_mtime();
        while (!at_dma_done())
            if (fx1_mtime() - start > FX1_MTIME_HZ / 1000u) fx1_fail(0x160); /* 1 ms */
        at_dma_ack();
        fx1_dma_complete((const volatile void*)dst, H1_BYTES, FX1_DMA_FROM_DEVICE);
        for (uint32_t i = 0; i < H1_BYTES; ++i)
            if (*(volatile uint8_t*)(dst + i) != (uint8_t)(i * 7u + n)) {
                ++h1_errors;
                break;
            }
        h1_copies = ++n;
    }
    fx1_release();
    h1_exited = 1;
    for (;;) fx1_wfi();
}

/* Hart 0, from isp_stream/main.c: before the stream starts / after it ends. */
void shared_mem_start(void)
{
    fx1_release_secondaries();
}

void shared_mem_mark(uint32_t point)
{
    if (point < MARKS) {
        marks[point] = h1_copies;
        marked |= 1u << point;
    }
}

void shared_mem_finish(void)
{
    const int still_running = !h1_exited;
    h1_stop = 1;
    const uint64_t start = fx1_mtime();
    while (!h1_exited)
        fx1_check(fx1_mtime() - start < FX1_MTIME_HZ / 100u, 0x60, "hart 1 stops within 10 ms");
    fx1_acquire();
    fx1_check(marked == (1u << MARKS) - 1u, 0x65, "stream start and all 8 completions marked");
    uint32_t busy_intervals = 0;
    for (uint32_t k = 1; k < MARKS; ++k) busy_intervals += marks[k] > marks[k - 1];
    const uint32_t in_window = marks[MARKS - 1] - marks[0];
    fx1_log_hex("hart 1 DMA copies before the first frame was queued", marks[0]);
    fx1_log_hex("hart 1 DMA copies between first queue and last completion", in_window);
    fx1_log_hex("frame intervals with hart 1 DMA progress (of 8)", busy_intervals);
    fx1_check(h1_errors == 0, 0x61, "every hart 1 DMA copy matches its source");
    fx1_check(in_window >= 4, 0x62, "hart 1 DMA copies inside the ISP streaming window");
    fx1_check(busy_intervals == MARKS - 1u, 0x63, "hart 1 progressed in every frame interval");
    fx1_check(still_running, 0x64, "hart 1 was still copying when the stream ended");
}
