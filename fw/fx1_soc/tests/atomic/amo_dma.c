/*
 * G4b / plan C7: an AMO's read-modify-write is atomic against SYS_DMA.
 *
 * Each trial clears a word, starts a 4-byte DMA copy of 0x80000000 into it and
 * meanwhile hammers the same word with amoor.w of the low bits until the DMA
 * reports completion. Every amoor only sets bits, so once the DMA's write has
 * landed bit 31 can never be cleared again: a final value without bit 31 is a
 * DMA write lost inside an AMO (load before the write, store after it).
 *
 * Where the DMA write lands is decided by simulated timing, not by the test.
 * An AMO's load-to-store window is a few tens of ns of a much longer loop, so
 * the trials sweep the phase: each one varies the size of the DMA's padding
 * copy, which moves the real write by a few bus slots. A pass alone still does
 * not prove an overlap happened. The ctest registration
 * therefore also requires the platform's "device writes held N" counter to be
 * non-zero (a DMA write really arrived inside an AMO bracket and waited), and
 * a negative control runs the image under --no-exclusive-monitor, where it
 * must FAIL with code 0x20.
 */
#include "atomic_common.h"

#define TRIALS       128u
#define AMO_PER_POLL 8u
#define DMA_VALUE    0x80000000u

int main(void)
{
    at_write(AT_SRC, DMA_VALUE);
    uint32_t amos = 0;
    for (uint32_t t = 0; t < TRIALS; ++t) {
        const uint32_t word = AT_WORD(16 + t);
        at_write(word, 0);
        at_dma_start(AT_SRC, word, 4, AT_PAD_BYTES - (t * 37u) % 700u);
        uint32_t k = 0;
        do {
            for (uint32_t i = 0; i < AMO_PER_POLL; ++i, ++k) (void)at_amoor(word, 1u << (k % 31u));
        } while (!at_dma_done());
        at_dma_ack();
        fx1_dma_complete((const volatile void*)word, 4, FX1_DMA_FROM_DEVICE);
        amos += k;
        const uint32_t final = at_read(word);
        if (!(final & DMA_VALUE)) {
            fx1_log_hex("trial", t);
            fx1_log_hex("  final value (bit 31 = the DMA write)", final);
            fx1_fail(0x20);
        }
        fx1_check((final & ~DMA_VALUE) != 0, 0x21, "the AMOs ran while the DMA was busy");
    }
    fx1_log_hex("trials", TRIALS);
    fx1_log_hex("amoor.w executed", amos);
    fx1_log("no DMA write lost inside an AMO");
    return 0;
}
