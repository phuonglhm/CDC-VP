/*
 * Ethernet gap G3 (GAPS_REVIEW GR01): CPU atomics against the Ethernet DMA's
 * writes. Every Ethernet DMA write goes through a WriteGuard (master 0x103):
 * it waits while an AMO or SC bracket is open on its 64-byte granule, and on
 * completion cancels the LR reservations it overlaps.
 *
 * One image per cell, chosen at build time:
 *   ETH_AMO_SC      0: amoor.w loop; 1: lr.w / or / sc.w loop (one asm block)
 *   ETH_AMO_TARGET  0: a word of the RX payload write;
 *                   1: a word in the granule of the RX DES0 write-back;
 *                   2: a word in the granule of the TX DES0 write-back.
 *
 * Each trial sends a frame to itself (PHY loopback, 100 Mbps) and meanwhile
 * runs the atomic loop on one word until the DES0 of interest reads OWN = 0.
 * Interrupts are masked, so nothing but an Ethernet write can cancel a
 * reservation.
 *
 * RX payload (target 0): the frame's bytes at the word are 00 00 00 80 and
 * every atomic only sets low bits, so once the payload write has landed bit
 * 31 can never be cleared: a final value without it is a DMA write lost
 * inside an atomic (0x20). The word's offset moves from trial to trial,
 * which moves the write by up to about 115 us of line time.
 *
 * DES0 (targets 1, 2): the CPU must not write a descriptor the DMA owns, so
 * the word is a free word of the descriptor's granule. Both rings are 2
 * descriptors at +32 of their own granule (fixture below); words 0..7 of the
 * granule are free, one is the atomic's, the others hold a canary. Checks per
 * trial: the DES0 write-back is complete (OWN 0, no error, RX length), the
 * atomic word is exactly what the CPU's successful atomics set (no lost
 * atomic, no DMA bytes outside DES0), the canaries are intact. The frame
 * length moves from trial to trial, which moves the write-back. The TX
 * write-back follows the doorbell by a few microseconds, before the loop has
 * started, so in the TX cell the driver's doorbell (a CSR write through its
 * HAL) is held back: the TX DMA's own poll tick, or the doorbell rung from
 * inside the loop after a number of atomics that also moves, starts it.
 *
 * The data checks alone do not prove an overlap. The ctest registration
 * requires the platform's "device writes held N" counter to be non-zero: in
 * one image only one kind of bracket is opened and only one kind of Ethernet
 * write reaches its granule, so N counts that write held by that bracket. An
 * SC image also requires SC failures (0x24): Ethernet writes in the granule
 * between LR and SC. Negative controls run the images with
 * --eth-unguarded-dma.
 */
#include "atomic_common.h"
#include "eth_regs.h"
#include "fx1_eth.h"
#include "fx1_fw.h"

#ifndef ETH_AMO_SC
#define ETH_AMO_SC 0
#endif
#ifndef ETH_AMO_TARGET
#define ETH_AMO_TARGET 0
#endif

#define TRIALS       64u
#define AMO_PER_POLL 8u
#define DMA_BIT      0x80000000u
#define FIX_RING     2u           /* descriptors per ring in the DES0 cells */
#define FIX_DESC     32u          /* ring offset in its granule */
#define FIX_FREE     8u           /* free words before the ring */
#define CANARY       0xC0DE0000u

#if ETH_AMO_SC
#define OP_NAME "lr/sc"
#else
#define OP_NAME "amoor.w"
#endif
#if ETH_AMO_TARGET == 0
#define TARGET_NAME "the RX payload"
#elif ETH_AMO_TARGET == 1
#define TARGET_NAME "the RX DES0 granule"
#else
#define TARGET_NAME "the TX DES0 granule"
#endif

static fx1_eth eth;
static uint8_t frame[1514];
#if ETH_AMO_TARGET != 0
/* [0]: RX ring granule, [1]: TX ring granule */
static uint32_t fixture[2][AT_GRANULE / 4] __attribute__((section(".eth_dma"), aligned(AT_GRANULE)));
#endif

#if ETH_AMO_TARGET == 2
static void (*csr_wr)(void* ctx, uint32_t off, uint32_t v);
static int doorbell_held;
static void wr_hold_doorbell(void* ctx, uint32_t off, uint32_t v)
{
    if (off == ETH_DMA_TX_POLL)
        doorbell_held = 1;
    else
        csr_wr(ctx, off, v);
}
#endif

/* 0 when the SC stored (old | bits) */
static inline uint32_t lr_or_sc(uint32_t addr, uint32_t bits)
{
    uint32_t old, failed;
    __asm__ volatile("lr.w %0, (%2)\n"
                     "or   %1, %0, %3\n"
                     "sc.w %1, %1, (%2)\n"
                     : "=&r"(old), "=&r"(failed)
                     : "r"(addr), "r"(bits)
                     : "memory");
    return failed;
}

int main(void)
{
    fx1_global_irq_disable();
    fx1_eth_setup(&eth);
    eth.cfg.phy_loopback = 1;
#if ETH_AMO_TARGET != 0
    eth.cfg.ring_len = FIX_RING;
    eth.cfg.rx_ring = fx1_eth_offset(&fixture[0][FIX_DESC / 4]);
    eth.cfg.tx_ring = fx1_eth_offset(&fixture[1][FIX_DESC / 4]);
    for (uint32_t g = 0; g < 2; ++g)
        for (uint32_t w = 0; w < FIX_FREE; ++w) fixture[g][w] = CANARY | (g << 8) | w;
#endif
    const uint32_t ring = eth.cfg.ring_len;
    fx1_check(fx1_eth_start(&eth) == ETH_OK, 0x10, "eth atomic: start, PHY loopback");
#if ETH_AMO_TARGET == 2
    csr_wr = eth.dev.hal.wr;
    eth.dev.hal.wr = wr_hold_doorbell;                              /* eth_send's doorbell */
#endif
    for (uint32_t i = 0; i < sizeof frame; i++) frame[i] = (uint8_t)(i * 13u + 1u) & 0x7Fu;   /* no bit 7 */
    for (int i = 0; i < 6; i++) {
        frame[i] = eth.cfg.mac[i];
        frame[6 + i] = eth.cfg.mac[i];
    }
    frame[12] = 0x88;
    frame[13] = 0xB5;
    uint32_t ops = 0, sc_failed = 0;
    for (uint32_t t = 0; t < TRIALS; ++t) {
        const uint32_t rslot = eth.dev.rx_head % ring, tslot = eth.dev.tx_head % ring;
        const volatile uint32_t* rdes0 = (const volatile uint32_t*)fx1_eth_cpu(eth.cfg.rx_ring + 16u * rslot);
        const volatile uint32_t* tdes0 = (const volatile uint32_t*)fx1_eth_cpu(eth.cfg.tx_ring + 16u * tslot);
#if ETH_AMO_TARGET == 0
        const uint32_t len = sizeof frame;
        const uint32_t j = 18u + 4u * ((t * 29u) % 360u);          /* buffer + rx_lead 2 + j: word aligned */
        const uint32_t word =
            (uint32_t)(uintptr_t)fx1_eth_cpu(eth.cfg.rx_buf + FX1_ETH_STRIDE * rslot + eth.cfg.rx_lead + j);
        const volatile uint32_t* until = rdes0;
        (void)tdes0;
        frame[j] = 0;
        frame[j + 1] = 0;
        frame[j + 2] = 0;
        frame[j + 3] = 0x80;
#else
        const uint32_t len = 64u + (t * 97u) % (sizeof frame - 63u);
        const uint32_t g = ETH_AMO_TARGET - 1u, w = t % FIX_FREE;
        const uint32_t word = (uint32_t)(uintptr_t)&fixture[g][w];
        const volatile uint32_t* until = g ? tdes0 : rdes0;
#endif
        at_write(word, 0);
        fx1_check(fx1_eth_send(&eth, frame, len) == ETH_OK, 0x11, "eth atomic: frame queued");
        uint32_t k = 0, expect = 0, fails = 0;
        do {
#if ETH_AMO_TARGET == 2
            if (doorbell_held && k >= AMO_PER_POLL * (1u + t % 8u)) {
                csr_wr(eth.dev.hal.ctx, ETH_DMA_TX_POLL, ETH_DMA_TX_POLL_POLL_MASK);
                doorbell_held = 0;
            }
#endif
            for (uint32_t i = 0; i < AMO_PER_POLL; ++i, ++k) {
                const uint32_t bit = 1u << (k % 31u);
#if ETH_AMO_SC
                if (lr_or_sc(word, bit)) {
                    ++fails;
                    continue;
                }
#else
                (void)at_amoor(word, bit);
#endif
                expect |= bit;
            }
        } while (*until & DMA_BIT);
        ops += k;
        sc_failed += fails;
        const uint32_t final = at_read(word);
#if ETH_AMO_TARGET == 0
        fx1_dma_complete((const volatile void*)word, 4, FX1_DMA_FROM_DEVICE);
        if (!(final & DMA_BIT)) {
            fx1_log_hex("trial", t);
            fx1_log_hex("  final value (bit 31 = the Ethernet DMA write)", final);
            fx1_fail(0x20);
        }
        fx1_check((final & ~DMA_BIT) != 0, 0x21, "eth atomic: the atomics ran while the frame was received");
        frame[j] = frame[j + 1] = frame[j + 2] = frame[j + 3] = 0x11;
#else
        const uint32_t d = *until;
        const int bad_des0 = (d & 0x00070000u) || (!g && (d & 0xFFFFu) != len);
        uint32_t canaries = 0;
        for (uint32_t c = 0; c < FIX_FREE; ++c)
            if (c != w && fixture[g][c] != (CANARY | (g << 8) | c)) ++canaries;
        if (final != expect || bad_des0 || canaries) {
            fx1_log_hex("trial", t);
            fx1_log_hex("  atomic word", final);
            fx1_log_hex("  expected (successful atomics)", expect);
            fx1_log_hex("  DES0", d);
            fx1_log_hex("  frame length", len);
            fx1_log_hex("  canaries changed", canaries);
            fx1_fail(0x20);
        }
        fixture[g][w] = CANARY | (g << 8) | w;
#endif
        if (ETH_AMO_TARGET == 2) {                                 /* the frame still comes back */
            const uint64_t end = fx1_mtime() + (uint64_t)2000u * (FX1_MTIME_HZ / 1000000u);
            while ((*rdes0 & DMA_BIT) && fx1_mtime() < end) {
            }
        }
        fx1_eth_service(&eth);                                    /* RX descriptor back, TX reclaimed */
    }
#if ETH_AMO_TARGET == 2
    eth.dev.hal.wr = csr_wr;
#endif
    fx1_log_hex("trials", TRIALS);
    fx1_log_hex(OP_NAME " executed", ops);
    fx1_log_hex("SC failed", sc_failed);
    fx1_check(eth.c.rx_frames == TRIALS && eth.c.rx_desc_errors == 0 && eth.c.tx_desc_errors == 0, 0x22,
              "eth atomic: every frame received, no descriptor error");
    fx1_check(fx1_eth_stop(&eth, 2000) == ETH_OK, 0x23, "eth atomic: stop");
    if (ETH_AMO_SC)
        fx1_check(sc_failed != 0, 0x24, "eth atomic: Ethernet writes in the granule cancelled reservations");
    fx1_log("eth atomic: " OP_NAME " on " TARGET_NAME ": no Ethernet DMA write lost, every atomic kept");
    return 0;
}
