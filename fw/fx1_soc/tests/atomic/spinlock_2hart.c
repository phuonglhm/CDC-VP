/*
 * G4b / plan C7: both harts take an LR/SC spin lock 50 000 times each and
 * increment a plain counter inside it (lw / addi / sw, not atomic by itself),
 * and an AMO counter outside it. Both must end at exactly 100 000: a lost
 * increment of the plain counter is two harts inside the lock at once, i.e. a
 * reservation that survived the other hart's SC or store.
 *
 * With the exclusive monitor the LR takes no bus lock, so the harts really
 * race between LR and SC. The simulator's schedule is deterministic, and
 * without help the two harts never both see the lock free before one of them
 * stores (a mutation that lets a hart's SC leave the other's reservation
 * intact still counted 100 000). So each hart waits a varying few
 * instructions between a successful-looking LR and its SC, which opens that
 * window; then a reservation that survives the other hart's SC lets both in
 * and the plain counter loses increments. The ctest registration also
 * requires the platform's "cancelled by hart N" counter to be non-zero.
 */
#include "atomic_common.h"

#define ITERATIONS 50000u
#define LOCK       AT_WORD(400)
#define COUNTER    AT_WORD(401)
#define AMO_COUNT  AT_WORD(402)

static volatile uint32_t h1_finished, sc_retries[FX1_NUM_HARTS];

static void lock(uint32_t round)
{
    uint32_t seen, failed, retries = 0;
    const uint32_t hart = fx1_hartid();
    for (;;) {
        seen = at_lr(LOCK);
        if (seen) continue;
        for (uint32_t k = (round * 3u + hart * 5u + retries) & 7u; k; --k) __asm__ volatile("nop");
        failed = at_sc(LOCK, 1u);
        if (!failed) break;
        ++retries;
    }
    fx1_acquire();
    sc_retries[hart] += retries;
}

static void unlock(void)
{
    fx1_release();
    at_write(LOCK, 0);
}

static void work(void)
{
    for (uint32_t i = 0; i < ITERATIONS; ++i) {
        lock(i);
        at_write(COUNTER, at_read(COUNTER) + 1u);
        unlock();
        __asm__ volatile("amoadd.w zero, %1, (%0)" ::"r"(AMO_COUNT), "r"(1u) : "memory");
    }
}

void secondary_main(uint32_t hart)
{
    (void)hart;
    work();
    fx1_release();
    h1_finished = 1;
    for (;;) fx1_wfi();
}

int main(void)
{
    at_write(LOCK, 0);
    at_write(COUNTER, 0);
    at_write(AMO_COUNT, 0);
    fx1_release_secondaries();
    work();
    while (!h1_finished) {
    }
    fx1_acquire();
    const uint32_t counter = at_read(COUNTER), amo = at_read(AMO_COUNT);
    fx1_log_hex("plain counter under the LR/SC lock", counter);
    fx1_log_hex("amoadd.w counter", amo);
    fx1_log_hex("hart 0 failed SC", sc_retries[0]);
    fx1_log_hex("hart 1 failed SC", sc_retries[1]);
    fx1_check(counter == 2u * ITERATIONS, 0x10, "mutual exclusion: no increment lost");
    fx1_check(amo == 2u * ITERATIONS, 0x11, "AMO: no increment lost");
    fx1_check(at_read(LOCK) == 0, 0x12, "lock released");
    return 0;
}
