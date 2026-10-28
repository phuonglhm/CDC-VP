// FU3_VP_ABI_1 boot smoke on Ibex, from a real reset (VP verification
// firmware, not a product Boot ROM). The ROM stub (ibex_rom.S) makes the
// IBEX_ISRAM image executable and jumps here; this image then follows the
// Boot ROM sequence of the approved proposal (section 4):
//   1. identify the ABI, read the captured straps / DEBUG_EN and the reset cause;
//   2. stage the CVA6 image and the debug blob in both debug partitions while
//      the staging PMP rules are RW/NX (Ibex debug partition: entry 11);
//   3. apply the final W^X PMP: Ibex image and debug partition RX, CVA6_ISRAM
//      no access; then clear mseccfg.RLB so the locked entries are final;
//   4. wait while DEBUG_EN = 1 (BOOT_STATUS[8], live), enable the INTMAP
//      software route and the Ibex software interrupt (SWIRQ), release CVA6
//      (FCLK_SW, closes staging). With `test_fu3_soc_cpu -d <us>` the board
//      holds DEBUG_EN high until then and the harness checks that CVA6 is
//      released only afterwards;
//   5. CVA6 runs its image and sends an IPI to Ibex through SWIRQ.
#include "fw_common.h"
#include "fu3_vp_abi.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_IBEX;

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define SYS FU3_VP_ABI_SYSCSR_BASE
#define CRM_FCLK_SW 0x50010034u
#define CVA6_ISRAM 0x00010000u
#define CVA6_DEBUG 0x00040000u
#define IBEX_IMAGE 0x00050000u
#define IBEX_DEBUG 0x00060000u
#define ABI1_CVA6_STARTS REG(0x0103fe20u)
#define ABI1_DEBUG_EN_AT_BOOT REG(0x0103fe24u)
#define MSECCFG_RLB (1u << 2)
#define MIE_MSIE (1u << 3)
// PMP cfg bytes: L | TOR / NAPOT | R | W | X
#define PMP_L 0x80u
#define PMP_TOR 0x08u
#define PMP_NAPOT 0x18u
#define PMP_R 0x01u
#define PMP_W 0x02u
#define PMP_X 0x04u

extern const uint32_t abi1_cva6_payload[], abi1_cva6_payload_end[];
extern const uint32_t vp_debug_blob[], vp_debug_blob_end[];
static volatile uint32_t sw_irqs;
static volatile uint32_t sink;

static void isr(uint32_t cause)
{
    if (cause == 3) {
        sw_irqs = sw_irqs + 1;
        REG(SYS + FU3_VP_SWIRQ_CLEAR_OFFSET) = FU3_VP_SWIRQ_IBEX;   // acknowledge
        while (CSR_READ(mip) & MIE_MSIE)
            ;
    }
}

static void copy(uint32_t dst, const uint32_t *begin, const uint32_t *end)
{
    for (const uint32_t *p = begin; p < end; ++p, dst += 4)
        REG(dst) = *p;
}

static int same(uint32_t dst, const uint32_t *begin, const uint32_t *end)
{
    for (const uint32_t *p = begin; p < end; ++p, dst += 4)
        if (REG(dst) != *p)
            return 0;
    return 1;
}

static int wait_for(volatile uint32_t *word, uint32_t value)
{
    for (unsigned i = 0; i < 200000; ++i)
        if (*word == value)
            return 1;
    return 0;
}

int main(void)
{
    // 1. ABI identification, boot status and reset cause.
    fw_check(200, REG(SYS + FU3_VP_ABI_VERSION_OFFSET) == FU3_VP_ABI_VERSION_VALUE, REG(SYS + FU3_VP_ABI_VERSION_OFFSET));
    const uint32_t boot = REG(SYS + FU3_VP_BOOT_STATUS_OFFSET);
    fw_check(201, (boot & FU3_VP_BOOT_STRAPS_VALID) != 0, boot);
    ABI1_DEBUG_EN_AT_BOOT = (boot & FU3_VP_BOOT_DEBUG_EN) != 0;   // board choice; waited for in step 4
    fw_check(203, REG(SYS + FU3_VP_RESET_CAUSE_OFFSET) == FU3_VP_RESET_POR, REG(SYS + FU3_VP_RESET_CAUSE_OFFSET));
    REG(SYS + FU3_VP_RESET_CAUSE_OFFSET) = FU3_VP_RESET_POR;
    fw_check(204, REG(SYS + FU3_VP_RESET_CAUSE_OFFSET) == 0, REG(SYS + FU3_VP_RESET_CAUSE_OFFSET));

    // 2. Staging. Entry 4 (IBEX_ISRAM, RX since the ROM stub) now ends at the
    // debug partition, which entry 11 covers RW/NX while it is written.
    CSR_WRITE(pmpaddr4, IBEX_DEBUG >> 2);
    CSR_WRITE(pmpaddr11, (IBEX_DEBUG | (0x1000 / 2 - 1)) >> 2);
    CSR_SET(pmpcfg2, (PMP_L | PMP_NAPOT | PMP_R | PMP_W) << 24);
    copy(CVA6_ISRAM, abi1_cva6_payload, abi1_cva6_payload_end);
    copy(CVA6_DEBUG, vp_debug_blob, vp_debug_blob_end);
    copy(IBEX_DEBUG, vp_debug_blob, vp_debug_blob_end);
    fw_check(210, same(CVA6_ISRAM, abi1_cva6_payload, abi1_cva6_payload_end), REG(CVA6_ISRAM));
    fw_check(211, same(CVA6_DEBUG, vp_debug_blob, vp_debug_blob_end), REG(CVA6_DEBUG));
    fw_check(212, same(IBEX_DEBUG, vp_debug_blob, vp_debug_blob_end), REG(IBEX_DEBUG));

    // 3. Final W^X: Ibex debug partition RX (entry 11), CVA6_ISRAM no access
    // for Ibex (entry 2), Ibex image already RX (entry 4); then lock (RLB = 0).
    CSR_CLEAR(pmpcfg2, 0xffu << 24);
    CSR_SET(pmpcfg2, (PMP_L | PMP_NAPOT | PMP_R | PMP_X) << 24);
    CSR_CLEAR(pmpcfg0, 0xffu << 16);
    CSR_SET(pmpcfg0, (PMP_L | PMP_TOR) << 16);
    EXPECT_TRAP(220, 7, REG(CVA6_ISRAM) = 0);    // store access fault
    EXPECT_TRAP(221, 5, sink = REG(CVA6_ISRAM));  // load access fault
    EXPECT_TRAP(222, 7, REG(IBEX_DEBUG) = 0);
    EXPECT_TRAP(223, 7, REG(IBEX_IMAGE + 0x8000) = 0);   // own image: W^X
    fw_check(224, REG(IBEX_DEBUG) == vp_debug_blob[0], REG(IBEX_DEBUG));   // still readable
    CSR_CLEAR(0x747, MSECCFG_RLB);
    fw_check(225, (CSR_READ(0x747) & MSECCFG_RLB) == 0, CSR_READ(0x747));
    const uint32_t cfg1 = CSR_READ(pmpcfg1);
    CSR_SET(pmpcfg1, PMP_W);   // a locked entry no longer changes
    fw_check(226, CSR_READ(pmpcfg1) == cfg1, CSR_READ(pmpcfg1));
    EXPECT_TRAP(227, 7, REG(IBEX_IMAGE + 0x8000) = 0);

    // 4. Hand off only once DEBUG_EN is low. Software interrupt to Ibex first (self test).
    unsigned polls = 0;
    while ((REG(SYS + FU3_VP_BOOT_STATUS_OFFSET) & FU3_VP_BOOT_DEBUG_EN) && polls < 1000000)
        ++polls;
    fw_check(230, (REG(SYS + FU3_VP_BOOT_STATUS_OFFSET) & FU3_VP_BOOT_DEBUG_EN) == 0, polls);
    fw_irq_hook = isr;
    REG(SYS + FU3_VP_INTMAP_EN1_OFFSET) = 1u << (FU3_VP_INTMAP_SRC_SW_IBEX - 32);
    CSR_SET(mie, MIE_MSIE);
    CSR_SET(mstatus, MSTATUS_MIE);
    REG(SYS + FU3_VP_SWIRQ_SET_OFFSET) = FU3_VP_SWIRQ_IBEX;
    fw_check(231, wait_for(&sw_irqs, 1), sw_irqs);
    fw_check(232, REG(SYS + FU3_VP_SWIRQ_PENDING_OFFSET) == 0, REG(SYS + FU3_VP_SWIRQ_PENDING_OFFSET));
    ABI1_CVA6_STARTS = 0;
    REG(CRM_FCLK_SW) = 1;

    // 5. CVA6 runs its image and interrupts Ibex (IPI).
    fw_check(240, wait_for(&ABI1_CVA6_STARTS, 1), ABI1_CVA6_STARTS);
    fw_check(241, wait_for(&sw_irqs, 2), sw_irqs);
    CSR_CLEAR(mstatus, MSTATUS_MIE);
    fw_irq_hook = 0;
    fw_finish();
    return 0;
}
