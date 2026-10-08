/*
 * G2 / plan C11 throughput probe (fx1_regression, not a smoke gate).
 * Both harts run an integer + memory loop over their own 4 KiB buffer in DDR;
 * the simulator prints retired instructions and host MIPS at the end. Each
 * checksum is compared with a value computed independently on the host
 * (review G2), so both quantum runs gate the data, not only the speed.
 */
#include "fx1_fw.h"

#define ITERATIONS 20000u
#define WORDS      1024u
/* Independent reference (Python model of work() below, review G2). */
#define EXPECTED_HART0 0x1762a79au
#define EXPECTED_HART1 0x124d31b2u

static uint32_t buffer[FX1_NUM_HARTS][WORDS];
static volatile uint32_t done[FX1_NUM_HARTS];
static volatile uint32_t checksum[FX1_NUM_HARTS];

static void work(uint32_t hart)
{
    uint32_t* b = buffer[hart];
    uint32_t sum = hart + 1;
    for (uint32_t i = 0; i < ITERATIONS; ++i) {
        const uint32_t k = i % WORDS;
        b[k] = b[k] * 1664525u + sum;
        sum ^= b[k] >> 3;
    }
    checksum[hart] = sum;
    fx1_release();
    done[hart] = 1;
}

void secondary_main(uint32_t hart)
{
    work(hart);
    for (;;) fx1_wfi();
}

int main(void)
{
    fx1_release_secondaries();
    const uint64_t start = fx1_mtime();
    work(0);
    while (!done[1]) {
    }
    fx1_acquire();
    const uint64_t ticks = fx1_mtime() - start;
    fx1_log_hex("checksum hart0", checksum[0]);
    fx1_log_hex("checksum hart1", checksum[1]);
    fx1_log_hex("mtime ticks", (uint32_t)ticks);
    fx1_check(checksum[0] == EXPECTED_HART0, 0x01, "hart 0 checksum matches the reference");
    fx1_check(checksum[1] == EXPECTED_HART1, 0x02, "hart 1 checksum matches the reference");
    return 0;
}
