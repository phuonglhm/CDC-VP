/*
 * G5 / plan P5: SYS_DMA error and pause recovery on the platform (DMA
 * programming guide, "Errors" and "Channel enable"). Interrupts through PLIC
 * source 2, cleared at the DMA before the PLIC complete.
 *
 *   1  destination unmapped: WRITE_DECERR, no completion, the channel goes
 *      inactive; the same channel is re-armed and a copy then succeeds
 *   2  source unmapped: READ_DECERR, then the same recovery
 *   3  a chain whose second command has an unmapped destination: the first
 *      command completes (data checked), the second reports WRITE_DECERR and
 *      no completion; re-arm, the next copy succeeds
 *   4  isolation: channel 0 fails while channel 1 runs a copy; channel 1
 *      completes with correct data and no error
 *   5  pause/resume: a 16-command chain is paused with CH_ENABLE = 0 once at
 *      least 2 commands have completed; its completed-command count
 *      (CH_TRANSFER_COUNT) stops moving and no final completion arrives;
 *      CH_ENABLE = 1 resumes it, the count grows by exactly 16 in total and
 *      every command's data is correct
 *
 * The DMA has no channel abort: CH_ENABLE = 0 pauses (accepted transactions
 * drain, re-enabling continues), and only the IP reset, for which the FX1
 * memory map has no controller yet, cancels a command.
 */
#include "fx1_fw.h"
#include "dma/fx1_dma_regs.h"

#define DMA(offset)    (FX1_SYS_DMA_CSR_BASE + (offset))
#define CH(n, offset)  DMA(FX1_DMA_CH((n), (offset)))
#define UNMAPPED       0x60000000u
#define AREA           (FX1_FW_TEST_AREA_BASE + 0x40000u)
#define SRC            (AREA + 0x00000u)
#define DST            (AREA + 0x10000u)
#define SRC1           (AREA + 0x20000u)
#define DST1           (AREA + 0x30000u)
#define DESC           (AREA + 0x3F000u) /* 16-byte descriptors */
#define CHAIN          16u
#define CFG_M2M        (FX1_DMA_ADDRESS_INCREMENT | FX1_DMA_CFG_TOKENS(1) | FX1_DMA_CFG_BURST_BYTES(32))
#define ERRORS         (FX1_DMA_INTERRUPT_MASK & ~FX1_DMA_INT_COMMAND_COMPLETE)

static volatile uint32_t status_seen[2], completions[2], irqs;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) {
        fx1_log_hex("unexpected mcause", mcause);
        fx1_fail(0x0E0);
    }
    const uint32_t id = fx1_plic_claim(0);
    if (id == FX1_IRQ_SYS_DMA) {
        ++irqs;
        const uint32_t core = fx1_read32(DMA(FX1_DMA_CORE_STATUS));
        for (uint32_t ch = 0; ch < 2; ++ch) {
            if (!(core & (1u << ch))) continue;
            const uint32_t st = fx1_read32(CH(ch, FX1_DMA_CH_INTERRUPT_STATUS));
            status_seen[ch] |= st;
            if (st & FX1_DMA_INT_COMMAND_COMPLETE) ++completions[ch]; /* one W1C acknowledges one */
            fx1_write32(CH(ch, FX1_DMA_CH_INTERRUPT_CLEAR), st);   /* clear at the device first */
        }
    }
    if (id) fx1_plic_complete(0, id);
    return mepc;
}

static void fill(uint32_t base, uint32_t bytes, uint32_t seed)
{
    for (uint32_t i = 0; i < bytes; ++i) *(volatile uint8_t*)(base + i) = (uint8_t)(i * 29u + seed);
}

static int same(uint32_t a, uint32_t b, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; ++i)
        if (*(volatile uint8_t*)(a + i) != *(volatile uint8_t*)(b + i)) return 0;
    return 1;
}

static void reset_counts(uint32_t ch)
{
    const uint32_t saved = fx1_irq_save();
    status_seen[ch] = 0;
    completions[ch] = 0;
    fx1_irq_restore(saved);
}

/* Program one command (or the head of a chain: `control` = next descriptor). */
static void arm(uint32_t ch, uint32_t src, uint32_t dst, uint32_t bytes, uint32_t control)
{
    fx1_write32(CH(ch, FX1_DMA_CH_ENABLE), 0);
    fx1_write32(CH(ch, FX1_DMA_CH_INTERRUPT_CLEAR), FX1_DMA_INTERRUPT_MASK);
    fx1_write32(CH(ch, FX1_DMA_CH_CMD_READ_ADDR), src);
    fx1_write32(CH(ch, FX1_DMA_CH_CMD_WRITE_ADDR), dst);
    fx1_write32(CH(ch, FX1_DMA_CH_CMD_TRANSFER_SIZE), bytes);
    fx1_write32(CH(ch, FX1_DMA_CH_CMD_CONTROL), control);
    fx1_write32(CH(ch, FX1_DMA_CH_READ_CONFIG), CFG_M2M);
    fx1_write32(CH(ch, FX1_DMA_CH_WRITE_CONFIG), CFG_M2M);
    fx1_write32(CH(ch, FX1_DMA_CH_PERIPHERAL_CONFIG), 0);
    fx1_write32(CH(ch, FX1_DMA_CH_MODE_CONFIG), FX1_DMA_MODE_SWAP_NONE);
    fx1_write32(CH(ch, FX1_DMA_CH_ENABLE), 1);
}

static void start(uint32_t ch) { fx1_write32(CH(ch, FX1_DMA_CH_START), 1); }

static int idle(uint32_t ch)
{
    return fx1_read32(CH(ch, FX1_DMA_CH_ACTIVE_STATUS)) == 0 &&
           fx1_read32(CH(ch, FX1_DMA_CH_OUTSTANDING_STATUS)) == 0;
}

/* Wait until channel `ch` has reported something (completion or error) and is
 * idle. Ownership is the caller's: it completes exactly what it prepared. */
static void wait_done(uint32_t ch, uint32_t want_completions, uint32_t code)
{
    const uint64_t t0 = fx1_mtime();
    while (!((status_seen[ch] & ERRORS) || completions[ch] >= want_completions) || !idle(ch))
        fx1_check(fx1_mtime() - t0 < FX1_MTIME_HZ / 1000u, code, "DMA channel settles within 1 ms");
}

static void wait_us(uint32_t us)
{
    const uint64_t t0 = fx1_mtime();
    while (fx1_mtime() - t0 < (uint64_t)us * (FX1_MTIME_HZ / 1000000u)) {
    }
}

/* A good 1023-byte copy on channel `ch` after an error: re-arm and verify. */
static void rearm_and_copy(uint32_t ch, uint32_t seed, uint32_t code)
{
    reset_counts(ch);
    fill(SRC, FX1_DMA_MAX_TRANSFER, seed);
    fx1_dma_prepare((const volatile void*)SRC, FX1_DMA_MAX_TRANSFER, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)DST, FX1_DMA_MAX_TRANSFER, FX1_DMA_FROM_DEVICE);
    arm(ch, SRC, DST, FX1_DMA_MAX_TRANSFER, FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    start(ch);
    wait_done(ch, 1, code);
    fx1_dma_complete((const volatile void*)DST, FX1_DMA_MAX_TRANSFER, FX1_DMA_FROM_DEVICE);
    fx1_check(status_seen[ch] == FX1_DMA_INT_COMMAND_COMPLETE && completions[ch] == 1, code + 1,
              "re-armed channel: exactly one completion, no error");
    fx1_check(same(SRC, DST, FX1_DMA_MAX_TRANSFER), code + 2, "re-armed channel: data copied");
}

static void expect_failed(uint32_t ch, uint32_t error_bit, uint32_t code, const char* what)
{
    fx1_check((status_seen[ch] & ERRORS) == error_bit, code, what);
    fx1_check(idle(ch), code + 1, "failed command: channel inactive, nothing outstanding");
}

int main(void)
{
    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_SYS_DMA, 1);
    fx1_plic_enable(0, FX1_IRQ_SYS_DMA, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();

    /* 1 */
    reset_counts(0);
    fx1_dma_prepare((const volatile void*)SRC, 256, FX1_DMA_TO_DEVICE);
    arm(0, SRC, UNMAPPED, 256, FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    start(0);
    wait_done(0, 1, 0x10); /* the source is the CPU's again; nothing was written */
    expect_failed(0, FX1_DMA_INT_WRITE_DECERR, 0x11, "1: WRITE_DECERR only");
    fx1_check(completions[0] == 0, 0x13, "1: a failed command is no completion");
    rearm_and_copy(0, 1, 0x14);
    fx1_log("1: write error -> WRITE_DECERR, re-armed channel copies correctly");

    /* 2 */
    reset_counts(0);
    fx1_dma_prepare((const volatile void*)DST, 256, FX1_DMA_FROM_DEVICE);
    arm(0, UNMAPPED, DST, 256, FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    start(0);
    wait_done(0, 1, 0x20);
    fx1_dma_complete((const volatile void*)DST, 256, FX1_DMA_FROM_DEVICE); /* back to the CPU */
    expect_failed(0, FX1_DMA_INT_READ_DECERR, 0x21, "2: READ_DECERR only");
    fx1_check(completions[0] == 0, 0x23, "2: no completion");
    rearm_and_copy(0, 2, 0x24);
    fx1_log("2: read error -> READ_DECERR, re-armed channel copies correctly");

    /* 3: command 1 good, command 2 (descriptor) to an unmapped destination */
    reset_counts(0);
    fill(SRC, 512, 3);
    volatile uint32_t* d = (volatile uint32_t*)DESC;
    d[0] = SRC + 512;
    d[1] = UNMAPPED;
    d[2] = 256;
    d[3] = FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST;
    fx1_dma_prepare((const volatile void*)DESC, 16, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)SRC, 512, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)DST, 256, FX1_DMA_FROM_DEVICE);
    arm(0, SRC, DST, 256, DESC | FX1_DMA_CMD_SET_INT); /* not last: next = DESC */
    start(0);
    const uint64_t t3 = fx1_mtime();
    while (!(status_seen[0] & ERRORS) || !idle(0))
        fx1_check(fx1_mtime() - t3 < FX1_MTIME_HZ / 1000u, 0x30, "3: chain settles within 1 ms");
    fx1_dma_complete((const volatile void*)DST, 256, FX1_DMA_FROM_DEVICE);
    fx1_check(completions[0] == 1, 0x31, "3: the first command completed, the failed one did not");
    expect_failed(0, FX1_DMA_INT_WRITE_DECERR, 0x32, "3: WRITE_DECERR for the second command");
    fx1_check(same(SRC, DST, 256), 0x34, "3: the first command's data");
    rearm_and_copy(0, 4, 0x35);
    fx1_log("3: chain with a failing second command: first completes, error reported, re-arm works");

    /* 4: channel 1 copies while channel 0 fails */
    reset_counts(0);
    reset_counts(1);
    fill(SRC1, FX1_DMA_MAX_TRANSFER, 5);
    fx1_dma_prepare((const volatile void*)SRC1, FX1_DMA_MAX_TRANSFER, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)DST1, FX1_DMA_MAX_TRANSFER, FX1_DMA_FROM_DEVICE);
    arm(1, SRC1, DST1, FX1_DMA_MAX_TRANSFER, FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    fx1_dma_prepare((const volatile void*)SRC, FX1_DMA_MAX_TRANSFER, FX1_DMA_TO_DEVICE);
    arm(0, SRC, UNMAPPED, FX1_DMA_MAX_TRANSFER, FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    fx1_write32(DMA(FX1_DMA_CORE_CHANNEL_START), 0x3u); /* both at once */
    wait_done(1, 1, 0x40);
    wait_done(0, 1, 0x41);
    fx1_dma_complete((const volatile void*)DST1, FX1_DMA_MAX_TRANSFER, FX1_DMA_FROM_DEVICE);
    expect_failed(0, FX1_DMA_INT_WRITE_DECERR, 0x42, "4: channel 0 failed");
    fx1_check(status_seen[1] == FX1_DMA_INT_COMMAND_COMPLETE && completions[1] == 1, 0x44,
              "4: channel 1 completed without error");
    fx1_check(same(SRC1, DST1, FX1_DMA_MAX_TRANSFER), 0x45, "4: channel 1 data");
    fx1_log("4: an error on channel 0 leaves channel 1's concurrent copy intact");

    /* 5: pause and resume a 16-command chain */
    reset_counts(0);
    fill(SRC, CHAIN * 1024u, 6);
    for (uint32_t i = 0; i < CHAIN * 1024u; i += 4) *(volatile uint32_t*)(DST + i) = 0xDEADBEEFu;
    for (uint32_t k = 1; k < CHAIN; ++k) { /* descriptor k-1 describes command k */
        volatile uint32_t* e = (volatile uint32_t*)(DESC + 16u * (k - 1));
        e[0] = SRC + 1024u * k;
        e[1] = DST + 1024u * k;
        e[2] = 1024u - 4u; /* within the 1023-byte limit, word multiple */
        e[3] = (k == CHAIN - 1) ? (FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST) : (DESC + 16u * k);
    }
    fx1_dma_prepare((const volatile void*)DESC, 16u * CHAIN, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)SRC, CHAIN * 1024u, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)DST, CHAIN * 1024u, FX1_DMA_FROM_DEVICE);
    arm(0, SRC, DST, 1024u - 4u, DESC);
    const uint32_t base = fx1_read32(CH(0, FX1_DMA_CH_TRANSFER_COUNT)) & 0xFFFu;
    start(0);
    const uint64_t t5 = fx1_mtime();
    while (((fx1_read32(CH(0, FX1_DMA_CH_TRANSFER_COUNT)) & 0xFFFu) - base) < 2u)
        fx1_check(fx1_mtime() - t5 < FX1_MTIME_HZ / 1000u, 0x56, "5: two commands within 1 ms");
    fx1_write32(CH(0, FX1_DMA_CH_ENABLE), 0); /* pause: accepted transactions drain */
    wait_us(20);
    const uint32_t paused = (fx1_read32(CH(0, FX1_DMA_CH_TRANSFER_COUNT)) & 0xFFFu) - base;
    wait_us(50);
    const uint32_t still = (fx1_read32(CH(0, FX1_DMA_CH_TRANSFER_COUNT)) & 0xFFFu) - base;
    fx1_log_hex("5: commands completed when paused", paused);
    fx1_check(paused >= 2 && paused < CHAIN && still == paused, 0x50, "5: a paused chain makes no progress");
    fx1_check(completions[0] == 0 && status_seen[0] == 0, 0x51, "5: no completion or error while paused");
    fx1_write32(CH(0, FX1_DMA_CH_ENABLE), 1); /* resume */
    wait_done(0, 1, 0x52);
    fx1_dma_complete((const volatile void*)DST, CHAIN * 1024u, FX1_DMA_FROM_DEVICE);
    fx1_check(status_seen[0] == FX1_DMA_INT_COMMAND_COMPLETE && completions[0] == 1, 0x53,
              "5: one completion (only the last command sets an interrupt), no error");
    fx1_check(((fx1_read32(CH(0, FX1_DMA_CH_TRANSFER_COUNT)) & 0xFFFu) - base) == CHAIN, 0x54,
              "5: exactly the 16 commands ran, the rest after the resume");
    for (uint32_t k = 0; k < CHAIN; ++k)
        fx1_check(same(SRC + 1024u * k, DST + 1024u * k, 1024u - 4u), 0x55, "5: every command's data");
    fx1_log("5: CH_ENABLE pauses a chain and resumes it, all 16 commands correct");

    fx1_check(!(fx1_plic_pending() & (1u << FX1_IRQ_SYS_DMA)), 0x60, "SYS_DMA interrupt line low at the end");
    fx1_log_hex("SYS_DMA interrupts taken", irqs);
    return 0;
}
