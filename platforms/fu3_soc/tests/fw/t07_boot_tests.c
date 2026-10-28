// Plan T07 boot test on Ibex: what the boot firmware sees of the CRM, SYSCSR
// and the handoff. Three boots, told apart by a counter in DSRAM (the VP keeps
// SRAM content across a system reset):
//   1 (power-on):  CRM startup state, captured straps (FU3_VP_ABI_1
//                  BOOT_STATUS) and RESET_CAUSE = POR, a gated SYSCSR faults,
//                  PLL to 400 MHz, CVA6 image copied during staging, CVA6
//                  released through CRM FCLK_SW, CVA6_ISRAM closed afterwards
//                  (also across a local CVA6 reset); then a software reset.
//   2 (software):  request cleared, CVA6 held, staging reopened, new straps,
//                  CRM clock setup kept; then a global reset.
//   3 (global):    CRM back to its reset values; verdict.
// The harness (test_fu3_soc_boot) sets the straps, changes them while boot 1
// runs, and checks the reset causes and the CPU clock.
#include "fw_common.h"
#include "fu3_vp_abi.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_IBEX;

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define CRM 0x50010000u
#define CRM_PLL (CRM + 0x10)
#define CRM_PLL_CTRL (CRM + 0x18)
#define CRM_CLK_0 (CRM + 0x1c)
#define CRM_CLK_1 (CRM + 0x20)
#define CRM_CLK (CRM + 0x24)
#define CRM_CLK_GATE_1 (CRM + 0x2c)
#define CRM_IBEX_SW (CRM + 0x30)
#define CRM_FCLK_SW (CRM + 0x34)
#define CRM_SYSTEM_CLK_SW (CRM + 0x54)
#define GATE1_SYSCSR (1u << 5)
#define SYSCSR 0x44010000u
#define SYSCSR_STATUS (SYSCSR + 0x70)
#define SYSCSR_SYS_CTRL (SYSCSR + 0x74)
#define SYSCSR_OSC_PAD (SYSCSR + 0x80)
#define CVA6_ISRAM 0x00010000u

#define T07_CVA6_STARTS REG(0x0103fe00u)   // incremented by the CVA6 payload
#define T07_MAGIC REG(0x0103fe10u)
#define T07_BOOT REG(0x0103fe14u)
#define MAGIC 0x7007b007u

extern const uint32_t t07_cva6_payload[], t07_cva6_payload_end[];
static volatile uint32_t sink;

static int wait_for(volatile uint32_t *word, uint32_t value)
{
    for (unsigned i = 0; i < 20000; ++i)
        if (*word == value)
            return 1;
    return 0;
}

static void reset_and_wait(uint32_t sys_ctrl)
{
    REG(SYSCSR_SYS_CTRL) = sys_ctrl;
    for (;;)
        ;
}

static void boot_1(void)
{
    // CRM startup (spec 4.1): system and Ibex released, CVA6 held.
    fw_check(101, REG(CRM_SYSTEM_CLK_SW) == 1 && REG(CRM_IBEX_SW) == 1 && REG(CRM_FCLK_SW) == 0, REG(CRM_FCLK_SW));
    // Straps captured at the SYSCSR reset release (boot = mode_0 = 1); the
    // reset history starts with POR and is cleared here (W1C).
    fw_check(102, REG(SYSCSR + FU3_VP_BOOT_STATUS_OFFSET) ==
                      (FU3_VP_BOOT_STRAPS_VALID | FU3_VP_BOOT_BOOT | FU3_VP_BOOT_MODE_0),
             REG(SYSCSR + FU3_VP_BOOT_STATUS_OFFSET));
    fw_check(107, REG(SYSCSR + FU3_VP_RESET_CAUSE_OFFSET) == FU3_VP_RESET_POR, REG(SYSCSR + FU3_VP_RESET_CAUSE_OFFSET));
    REG(SYSCSR + FU3_VP_RESET_CAUSE_OFFSET) = FU3_VP_RESET_MASK;
    fw_check(103, REG(SYSCSR_OSC_PAD) == 0x13, REG(SYSCSR_OSC_PAD));

    // A gated SYSCSR answers with an error: load access fault (decision Q02-i).
    REG(CRM_CLK_GATE_1) = REG(CRM_CLK_GATE_1) & ~GATE1_SYSCSR;
    EXPECT_TRAP(104, 5, sink = REG(SYSCSR_OSC_PAD));
    fw_check(105, fw_trap.tval == SYSCSR_OSC_PAD, fw_trap.tval);
    REG(CRM_CLK_GATE_1) = REG(CRM_CLK_GATE_1) | GATE1_SYSCSR;
    fw_check(106, REG(SYSCSR_OSC_PAD) == 0x13, REG(SYSCSR_OSC_PAD));

    // PLL 20 MHz x 20 / 1 = 400 MHz; bus divider set while both selectors are
    // on the OSC (CRM spec 3.7.4), then branch selector, then final selector.
    REG(CRM_PLL) = 0x150;
    REG(CRM_PLL_CTRL) = 0x5948;
    fw_check(107, wait_for(&REG(CRM_PLL), 0x150 | (1u << 14)), REG(CRM_PLL));
    REG(CRM_CLK) = 1;
    REG(CRM_CLK_0) = 1;
    REG(CRM_CLK_1) = 1;

    // Staging: Ibex writes the CVA6 image, then releases CVA6 (decision Q02-b).
    const uint32_t words = (uint32_t)(t07_cva6_payload_end - t07_cva6_payload);
    for (uint32_t i = 0; i < words; ++i)
        REG(CVA6_ISRAM + 4 * i) = t07_cva6_payload[i];
    int same = 1;
    for (uint32_t i = 0; i < words; ++i)
        same = same && REG(CVA6_ISRAM + 4 * i) == t07_cva6_payload[i];
    fw_check(108, same, words);
    REG(CRM_FCLK_SW) = 1;
    fw_check(109, wait_for(&T07_CVA6_STARTS, 1), T07_CVA6_STARTS);

    // Staging closed by the release: Ibex can no longer write or read CVA6_ISRAM.
    EXPECT_TRAP(110, 7, REG(CVA6_ISRAM) = 0);
    EXPECT_TRAP(111, 5, sink = REG(CVA6_ISRAM));

    // A local CVA6 reset restarts CVA6 and does not reopen staging.
    REG(CRM_FCLK_SW) = 0;
    REG(CRM_FCLK_SW) = 1;
    fw_check(112, wait_for(&T07_CVA6_STARTS, 2), T07_CVA6_STARTS);
    EXPECT_TRAP(113, 7, REG(CVA6_ISRAM) = 0);

    reset_and_wait(1u << 0);   // sys_ctrl.sw_reset_req
}

static void boot_2(void)
{
    fw_check(121, REG(SYSCSR_SYS_CTRL) == 0, REG(SYSCSR_SYS_CTRL));   // cleared by the reset it caused
    fw_check(122, REG(CRM_FCLK_SW) == 0 && REG(CRM_IBEX_SW) == 1, REG(CRM_FCLK_SW));
    for (volatile unsigned i = 0; i < 200; ++i)
        ;
    fw_check(123, T07_CVA6_STARTS == 2, T07_CVA6_STARTS);   // CVA6 held since the reset
    // New boot epoch: staging open again.
    REG(CVA6_ISRAM) = t07_cva6_payload[0];
    fw_check(124, REG(CVA6_ISRAM) == t07_cva6_payload[0], REG(CVA6_ISRAM));
    // Straps changed during boot 1: captured anew at this reset's release.
    fw_check(125, REG(SYSCSR + FU3_VP_BOOT_STATUS_OFFSET) == (FU3_VP_BOOT_STRAPS_VALID | FU3_VP_BOOT_MODE_1),
             REG(SYSCSR + FU3_VP_BOOT_STATUS_OFFSET));
    fw_check(127, REG(SYSCSR + FU3_VP_RESET_CAUSE_OFFSET) == FU3_VP_RESET_SOFTWARE,
             REG(SYSCSR + FU3_VP_RESET_CAUSE_OFFSET));
    REG(SYSCSR + FU3_VP_RESET_CAUSE_OFFSET) = FU3_VP_RESET_SOFTWARE;
    fw_check(126, REG(CRM_CLK_1) == 1 && REG(CRM_PLL) == (0x150 | (1u << 14)), REG(CRM_CLK_1));   // CRM kept

    reset_and_wait(1u << 4);   // sys_ctrl.sw_global_rst_req
}

static void boot_3(void)
{
    fw_check(131, REG(CRM_CLK_0) == 0 && REG(CRM_CLK_1) == 0 && REG(CRM_PLL_CTRL) == 0x5949, REG(CRM_CLK_1));
    fw_check(132, REG(SYSCSR_SYS_CTRL) == 0 && REG(CRM_FCLK_SW) == 0, REG(SYSCSR_SYS_CTRL));
    // RESET_CAUSE lives in POR-only state: the global reset keeps it and adds itself.
    fw_check(133, REG(SYSCSR + FU3_VP_RESET_CAUSE_OFFSET) == FU3_VP_RESET_GLOBAL, REG(SYSCSR + FU3_VP_RESET_CAUSE_OFFSET));
    fw_finish();
}

int main(void)
{
    if (T07_MAGIC != MAGIC) {
        T07_MAGIC = MAGIC;
        T07_BOOT = 0;
    }
    T07_BOOT = T07_BOOT + 1;
    fw_check(100, fw_box->reset_info[3] == 0x80, fw_box->reset_info[3]);   // every boot from ROM 0x80
    switch (T07_BOOT) {
    case 1:
        boot_1();
        break;
    case 2:
        boot_2();
        break;
    case 3:
        boot_3();
        break;
    default:
        fw_check(199, 0, T07_BOOT);
        fw_finish();
    }
    return 0;
}
