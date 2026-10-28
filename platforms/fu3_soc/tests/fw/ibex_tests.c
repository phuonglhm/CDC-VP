// T05 CPU test on Ibex (CPU1): reset state, ISA (RV32IMC + Zcb, U-mode, no
// A/F/D/S), vectored-only mtvec, fast interrupts and their priority, WFI wake,
// PMP/Smepmp from the reset profile (RLB, MMWP, MML) and U-mode isolation.
#include "fw_common.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_IBEX;

static volatile uint32_t sink;
static volatile uint8_t zbuf[8];
static volatile uint16_t fetch_target[2];

#define MSECCFG_READ() CSR_READ(0x747)
#define MSECCFG_WRITE(v) CSR_WRITE(0x747, v)

static void reset_checks(void)
{
    fw_check(1, fw_box->reset_info[3] == 0x80, fw_box->reset_info[3]);         // first fetch
    fw_check(2, fw_box->reset_info[0] == 0x1, fw_box->reset_info[0]);          // mtvec
    fw_check(3, fw_box->reset_info[1] == 0x6, fw_box->reset_info[1]);          // RLB | MMWP
    fw_check(4, fw_box->reset_info[2] == 0x008b009du, fw_box->reset_info[2]);  // entries 0-3
}

static void isa_checks(void)
{
    const uint32_t misa = CSR_READ(misa);
    const uint32_t want = (1u << ('I' - 'A')) | (1u << ('M' - 'A')) | (1u << ('C' - 'A')) | (1u << ('U' - 'A'));
    const uint32_t absent = (1u << ('A' - 'A')) | (1u << ('F' - 'A')) | (1u << ('D' - 'A')) | (1u << ('S' - 'A')) |
                            (1u << ('N' - 'A'));
    fw_check(10, (misa & want) == want && !(misa & absent), misa);
    fw_check(11, CSR_READ(mhartid) == 1, CSR_READ(mhartid));

    // mtvec: MODE stays vectored, BASE is 256-byte aligned.
    const uint32_t saved = CSR_READ(mtvec);
    CSR_WRITE(mtvec, 0x00050123u);
    fw_check(12, CSR_READ(mtvec) == 0x00050101u, CSR_READ(mtvec));
    CSR_WRITE(mtvec, saved);

    CSR_WRITE(mcycle, 5000);
    const uint32_t cyc = CSR_READ(mcycle);
    fw_check(14, cyc >= 5000 && cyc < 5100, cyc);

    EXPECT_TRAP(13, 2, __asm__ volatile(".word 0x00052007"));  // flw: RVF = 0
}

static void zcb_checks(void)
{
    for (unsigned i = 0; i < 8; ++i)
        zbuf[i] = 0x81 + i;
    // One asm block: the compressed forms need x8-x15, and no call may run
    // between setting those registers and using them.
    volatile uint32_t r[6];
    __asm__ volatile(
        "mv a0, %[buf]\n\t"
        "addi a4, a0, 4\n\t"
        "c.lbu a1, 1(a0)\n\t"
        "c.lhu a2, 2(a0)\n\t"
        "c.lh a3, 2(a0)\n\t"
        "c.sb a1, 0(a0)\n\t"
        "c.sh a2, 2(a4)\n\t"
        "sw a1, 0(%[out])\n\t"
        "sw a2, 4(%[out])\n\t"
        "sw a3, 8(%[out])\n\t"
        "li a5, 0x1234\n\t"
        "c.zext.b a5\n\t"
        "sw a5, 12(%[out])\n\t"
        "c.not a5\n\t"
        "sw a5, 16(%[out])\n\t"
        "li a5, 7\n\t"
        "li a4, 6\n\t"
        "c.mul a5, a4\n\t"
        "sw a5, 20(%[out])"
        :
        : [buf] "r"(zbuf), [out] "r"(r)
        : "a0", "a1", "a2", "a3", "a4", "a5", "memory");
    fw_check(20, r[0] == 0x82, r[0]);
    fw_check(21, r[1] == 0x8483, r[1]);
    fw_check(22, r[2] == 0xffff8483u, r[2]);
    fw_check(23, zbuf[0] == 0x82 && zbuf[6] == 0x83 && zbuf[7] == 0x84, zbuf[0] | zbuf[6] << 8 | zbuf[7] << 16);
    fw_check(24, r[3] == 0x34, r[3]);
    fw_check(25, r[4] == 0xffffffcbu, r[4]);
    fw_check(26, r[5] == 42, r[5]);

    // Zbb-dependent Zcb instructions are reserved without Zbb (decision Q01-e).
    EXPECT_TRAP(27, 2, __asm__ volatile(".2byte 0x9d65"));  // c.sext.b a0
    EXPECT_TRAP(28, 2, __asm__ volatile(".2byte 0x9d69"));  // c.zext.h a0
}

static void interrupt_checks(void)
{
    // Three lines at once: fast[0] (16), fast[3] (19), external (11).
    const uint32_t lines = (1u << 16) | (1u << 19) | (1u << 11);
    CSR_SET(mie, lines);
    fw_box->req = fw_box->req | lines;
    while ((fw_box->ack & lines) != lines)
        ;
    fw_check(30, (CSR_READ(mip) & lines) == lines, CSR_READ(mip));
    CSR_SET(mstatus, MSTATUS_MIE);
    while (fw_trap.irq_count < 3)
        ;
    fw_check(31, fw_trap.irq_order[0] == 16 && fw_trap.irq_order[1] == 19 && fw_trap.irq_order[2] == 11,
             fw_trap.irq_order[0] | fw_trap.irq_order[1] << 8 | fw_trap.irq_order[2] << 16);

    // WFI wakes on a fast line (fast[5], cause 21).
    CSR_SET(mie, 1u << 21);
    fw_box->req = fw_box->req | (1u << 21);
    __asm__ volatile("wfi");
    fw_check(32, fw_trap.irq_count == 4 && fw_trap.irq_order[3] == 21, fw_trap.irq_count);
    CSR_CLEAR(mstatus, MSTATUS_MIE);
}

static void pmp_machine_checks(void)
{
    EXPECT_TRAP(40, 7, *(volatile uint32_t *)0x00000100u = 1);  // ROM rule is R/X
    EXPECT_TRAP(41, 5, sink = *(volatile uint32_t *)0x00002000u);  // no rule + MMWP
    // The bus lets Ibex fetch the CVA6 image while staging; the PMP staging
    // rule (R/W, no X) does not.
    void (*cva6_image)(void) = (void (*)(void))0x00010000u;
    EXPECT_TRAP(42, 1, cva6_image());
    // Misaligned loads and stores are split by the LSU, not trapped.
    static volatile uint32_t words[2] = {0x44332211u, 0x88776655u};
    const uint32_t odd = (uint32_t)(uintptr_t)words + 1;
    uint32_t value;
    const uint32_t traps = fw_trap.count;
    __asm__ volatile("lw %0, 0(%1)" : "=r"(value) : "r"(odd));
    __asm__ volatile("sw %0, 0(%1)" : : "r"(0xa5a5a5a5u), "r"((uint32_t)(uintptr_t)words + 2) : "memory");
    fw_check(44, fw_trap.count == traps && value == 0x55443322u, value);
    fw_check(45, words[0] == 0xa5a52211u && words[1] == 0x8877a5a5u, words[0]);

    fetch_target[0] = 0x8082;  // ret, in DSRAM (R/W rule)
    EXPECT_TRAP(43, 1, ((void (*)(void))(uintptr_t)fetch_target)());
}

static volatile uint32_t user_load;

static void user_body(void)
{
    // U-mode: locked legacy rules apply to U as written (DSRAM R/W).
    user_load = *(volatile uint32_t *)0x01000000u;
    *(volatile uint32_t *)0x01020100u = 0x5a5a5a5au;
    EXPECT_TRAP(50, 2, sink = CSR_READ(mstatus));                 // M-mode CSR
    EXPECT_TRAP(51, 7, *(volatile uint32_t *)0x00000100u = 1);   // ROM
    EXPECT_TRAP(55, 2, __asm__ volatile("mret"));                  // M-mode instruction
    EXPECT_TRAP(56, 2, __asm__ volatile("wfi"));                   // mstatus.TW = 1 (set by caller)
    __asm__ volatile("ecall");
}

static void user_checks(void)
{
    // MPP is WARL: values for modes Ibex lacks (S, reserved) read back as M.
    for (uint32_t mpp = 1; mpp <= 2; ++mpp) {
        CSR_CLEAR(mstatus, MSTATUS_MPP);
        CSR_SET(mstatus, mpp << 11);
        fw_check(57, (CSR_READ(mstatus) & MSTATUS_MPP) == MSTATUS_MPP, CSR_READ(mstatus));
    }
    // MRET into U-mode clears MPRV; TW makes U-mode WFI illegal.
    CSR_SET(mstatus, MSTATUS_MPRV | (1u << 21));
    RUN_USER(8, user_body);
    fw_check(58, (CSR_READ(mstatus) & MSTATUS_MPRV) == 0, CSR_READ(mstatus));
    CSR_CLEAR(mstatus, 1u << 21);
    fw_check(52, fw_trap.cause == 8, fw_trap.cause);             // left U-mode by ecall
    fw_check(53, *(volatile uint32_t *)0x01020100u == 0x5a5a5a5au, 0);
    // Back in M-mode: an M-mode CSR is accessible again (MPP is U after mret).
    const uint32_t before = fw_trap.count;
    sink = CSR_READ(mscratch);
    fw_check(54, fw_trap.count == before, fw_trap.cause);
}

static void user_debug_body(void)
{
    // U-mode: ebreak with dcsr.ebreaku, then a halt request.
    uint32_t at;
    fw_dbg.phase = FW_DBG_EBREAK;
    FW_EBREAK(at);
    fw_check(100, fw_dbg.cause == 1 && fw_dbg.prv == 0 && fw_dbg.dpc == at, fw_dbg.prv);
    fw_dbg.phase = FW_DBG_HALT;
    fw_debug_request();
    fw_check(101, fw_dbg.cause == 3 && fw_dbg.prv == 0, fw_dbg.cause);
    __asm__ volatile("ecall");
}

static void debug_checks(void)
{
    fw_debug_checks(4);
    // Arm ebreaku as well, then enter Debug Mode from U-mode and return there.
    fw_dbg.phase = FW_DBG_ARM_EBREAKM;
    fw_dbg.arg = 1;
    fw_debug_request();
    RUN_USER(8, user_debug_body);
    fw_check(102, fw_trap.cause == 8, fw_trap.cause);
}

static void smepmp_checks(void)
{
    // RLB = 1 at reset: locked entries can still be rewritten.
    CSR_WRITE(pmpaddr11, (0x0103d000u | 0x7ffu) >> 2);
    CSR_SET(pmpcfg2, 0x99u << 24);  // entry 11: L | NAPOT | R
    fw_check(60, (CSR_READ(pmpcfg2) >> 24) == 0x99u, CSR_READ(pmpcfg2));

    // Clearing RLB locks them; RLB cannot be set again while entries are locked.
    MSECCFG_WRITE(0x2);
    fw_check(61, MSECCFG_READ() == 0x2, MSECCFG_READ());
    CSR_CLEAR(pmpcfg2, 0x99u << 24);
    CSR_WRITE(pmpaddr11, 0);
    fw_check(62, (CSR_READ(pmpcfg2) >> 24) == 0x99u && CSR_READ(pmpaddr11) == ((0x0103d000u | 0x7ffu) >> 2),
             CSR_READ(pmpcfg2));
    MSECCFG_WRITE(0x6);
    fw_check(63, MSECCFG_READ() == 0x2, MSECCFG_READ());
    MSECCFG_WRITE(0x0);  // MMWP is sticky
    fw_check(64, MSECCFG_READ() == 0x2, MSECCFG_READ());

    // MML: locked rules become M-mode-only; new M-only executable rules are refused.
    MSECCFG_WRITE(0x3);
    fw_check(65, MSECCFG_READ() == 0x3, MSECCFG_READ());
    CSR_SET(pmpcfg3, 0x9cu);  // entry 12: L | NAPOT | X
    fw_check(66, (CSR_READ(pmpcfg3) & 0xff) == 0, CSR_READ(pmpcfg3));
    sink = *(volatile uint32_t *)0x01000000u;  // M-mode keeps its R/W data rule
    RUN_USER(1, user_body);                    // the image rule is M-only now
    fw_check(67, fw_trap.cause == 1, fw_trap.cause);
}

int main(void)
{
    reset_checks();
    isa_checks();
    zcb_checks();
    interrupt_checks();
    pmp_machine_checks();
    user_checks();
    debug_checks();
    smepmp_checks();
    fw_finish();
    return 0;
}
