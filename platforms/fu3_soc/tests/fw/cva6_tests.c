// T05 CPU test on CVA6 (CPU0): ISA (RV32IMFC, no A/D/S/U), traps, WFI wake,
// vectored interrupts and classic PMP. Runs from CVA6_ISRAM at 0x0001_0000.
#include "fw_common.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_CVA6;

static volatile uint32_t sink;
static volatile uint16_t fetch_target[2];  // DSRAM, non-executable

static void isa_checks(void)
{
    const uint32_t misa = CSR_READ(misa);
    const uint32_t want = (1u << ('I' - 'A')) | (1u << ('M' - 'A')) | (1u << ('F' - 'A')) | (1u << ('C' - 'A'));
    const uint32_t absent = (1u << ('A' - 'A')) | (1u << ('D' - 'A')) | (1u << ('S' - 'A')) | (1u << ('U' - 'A')) |
                            (1u << ('N' - 'A'));
    fw_check(1, (misa & want) == want && !(misa & absent) && (misa >> 30) == 1, misa);
    fw_check(2, CSR_READ(mhartid) == 0, CSR_READ(mhartid));

    volatile int32_t a = -7, b = 3;
    fw_check(3, a * b == -21 && a / b == -2 && a % b == -1 && (uint32_t)a / (uint32_t)b == 1431655763u,
             (uint32_t)(a * b));

    // mstatus.MPP is WARL: only M-mode exists.
    CSR_CLEAR(mstatus, MSTATUS_MPP);
    fw_check(8, (CSR_READ(mstatus) & MSTATUS_MPP) == MSTATUS_MPP, CSR_READ(mstatus));

    // Counters: cycle is readable without U-mode; mcycle is writable.
    sink = CSR_READ(cycle);
    CSR_WRITE(mcycle, 1000);
    const uint32_t cyc = CSR_READ(mcycle);
    fw_check(19, cyc >= 1000 && cyc < 1100, cyc);
}

// Kept out of line so no FP instruction is scheduled before FS is enabled.
static void __attribute__((noinline)) fp_ops(void)
{
    float x = 1.5f, y = 2.25f, sum, quot, root;
    __asm__ volatile("fadd.s %0, %1, %2" : "=f"(sum) : "f"(x), "f"(y));
    fw_check(6, sum == 3.75f, *(uint32_t *)&sum);
    CSR_WRITE(fflags, 0);
    float one = 1.0f, three = 3.0f, two = 2.0f;
    __asm__ volatile("fdiv.s %0, %1, %2" : "=f"(quot) : "f"(one), "f"(three));
    fw_check(7, (CSR_READ(fflags) & 1) == 1, CSR_READ(fflags));  // NX
    __asm__ volatile("fsqrt.s %0, %1" : "=f"(root) : "f"(two));
    fw_check(9, *(uint32_t *)&root == 0x3fb504f3u, *(uint32_t *)&root);
    int32_t truncated;
    __asm__ volatile("fcvt.w.s %0, %1, rtz" : "=r"(truncated) : "f"(sum));
    fw_check(10, truncated == 3, (uint32_t)truncated);
    fw_check(11, (CSR_READ(mstatus) & MSTATUS_FS) == MSTATUS_FS, CSR_READ(mstatus));  // Dirty

    // RMM (round to nearest, ties to max magnitude): 2.5 -> 3, -2.5 -> -3.
    float half = 2.5f, neg = -2.5f;
    int32_t r1, r2;
    __asm__ volatile("fcvt.w.s %0, %1, rmm" : "=r"(r1) : "f"(half));
    __asm__ volatile("fcvt.w.s %0, %1, rmm" : "=r"(r2) : "f"(neg));
    fw_check(14, r1 == 3 && r2 == -3, (uint32_t)r1);
    // fcsr/frm/fflags round trip (user-level CSRs on an M-only core).
    CSR_WRITE(fcsr, (2u << 5) | 0x1f);
    fw_check(15, CSR_READ(frm) == 2 && CSR_READ(fflags) == 0x1f && CSR_READ(fcsr) == 0x5f, CSR_READ(fcsr));
    CSR_WRITE(fcsr, 0);
}

static void fp_checks(void)
{
    // FS = Off at reset: any FP instruction or FP CSR access is illegal.
    fw_check(4, (CSR_READ(mstatus) & MSTATUS_FS) == 0, CSR_READ(mstatus));
    EXPECT_TRAP(5, 2, __asm__ volatile("fadd.s f0, f0, f0"));
    EXPECT_TRAP(16, 2, __asm__ volatile("flw f0, 0(sp)"));
    EXPECT_TRAP(17, 2, sink = CSR_READ(fflags));

    CSR_SET(mstatus, 1u << 13);  // FS = Initial
    fp_ops();

    EXPECT_TRAP(12, 2, __asm__ volatile(".word 0x02007053"));  // fadd.d: RVD = 0
    EXPECT_TRAP(13, 2, __asm__ volatile(".word 0x0005202f"));  // amoadd.w x0, x0, (a0): RVA = 0
}

static void trap_checks(void)
{
    EXPECT_TRAP(20, 11, __asm__ volatile("ecall"));
    EXPECT_TRAP(21, 3, __asm__ volatile(".word 0x00100073"));  // ebreak (32-bit)

    // CVA6 has no misaligned access support: lw/sw at an odd address trap (cause 4/6).
    static volatile uint32_t aligned[2];
    const uint32_t odd = (uint32_t)(uintptr_t)aligned + 1;
    uint32_t value;
    EXPECT_TRAP(22, 4, __asm__ volatile("lw %0, 0(%1)" : "=r"(value) : "r"(odd)));
    fw_check(23, fw_trap.tval == odd, fw_trap.tval);
    EXPECT_TRAP(18, 6, __asm__ volatile("sw zero, 0(%0)" : : "r"(odd) : "memory"));

    EXPECT_TRAP(24, 5, sink = *(volatile uint32_t *)0x00002000u);  // reserved: decode error
    fw_check(25, fw_trap.tval == 0x2000u, fw_trap.tval);
    EXPECT_TRAP(26, 5, sink = *(volatile uint32_t *)0x00000080u);  // Ibex-only ROM
    EXPECT_TRAP(27, 7, *(volatile uint32_t *)0x00000080u = 1);

    // Fetch from DSRAM (non-executable) after storing a valid `ret` there.
    fetch_target[0] = 0x8082;
    void (*fn)(void) = (void (*)(void))(uintptr_t)fetch_target;
    EXPECT_TRAP(28, 1, fn());
    fw_check(29, fw_trap.tval == (uint32_t)(uintptr_t)fetch_target, fw_trap.tval);
}

static void interrupt_checks(void)
{
    // WFI wakes on the timer line (time_irq_i, cause 7).
    CSR_SET(mie, 1u << 7);
    CSR_SET(mstatus, MSTATUS_MIE);
    fw_box->req = fw_box->req | (1u << 7);
    __asm__ volatile("wfi");
    fw_check(30, fw_trap.irq_count == 1 && fw_trap.irq_order[0] == 7, fw_trap.irq_count);

    // Vectored mtvec: irq_i[0] (cause 11) enters at BASE + 4 * 11.
    extern char fw_vectors[];
    CSR_WRITE(mtvec, (uint32_t)(uintptr_t)fw_vectors | 1);
    fw_check(31, CSR_READ(mtvec) == ((uint32_t)(uintptr_t)fw_vectors | 1), CSR_READ(mtvec));
    CSR_SET(mie, 1u << 11);
    fw_raise(11);
    while (fw_trap.irq_count < 2)
        ;
    fw_check(32, fw_trap.irq_order[1] == 11 && fw_trap.vector == 11, fw_trap.vector);
    CSR_CLEAR(mstatus, MSTATUS_MIE);
    extern char fw_trap_entry[];
    CSR_WRITE(mtvec, (uint32_t)(uintptr_t)fw_trap_entry);
}

static void pmp_checks(void)
{
    // A locked NAPOT rule (4 KiB of DSRAM, read-only) binds M-mode too.
    const uint32_t page = 0x0103e000u;
    CSR_WRITE(pmpaddr0, (page | 0x7ffu) >> 2);
    CSR_WRITE(pmpcfg0, 0x80u | 0x18u | 0x01u);  // L | NAPOT | R
    const uint32_t before = fw_trap.count;
    sink = *(volatile uint32_t *)page;
    fw_check(40, fw_trap.count == before, fw_trap.cause);
    EXPECT_TRAP(41, 7, *(volatile uint32_t *)page = 1);
    fw_check(42, fw_trap.tval == page, fw_trap.tval);
    CSR_WRITE(pmpcfg0, 0x1fu);  // ignored: entry 0 is locked
    fw_check(43, (CSR_READ(pmpcfg0) & 0xff) == 0x99u, CSR_READ(pmpcfg0));
    // Only 8 entries exist: pmpaddr8 and pmpcfg2 read as zero.
    CSR_WRITE(pmpaddr8, 0x12345678u);
    fw_check(44, CSR_READ(pmpaddr8) == 0 && CSR_READ(pmpcfg2) == 0, CSR_READ(pmpaddr8));
}

int main(void)
{
    isa_checks();
    fp_checks();
    trap_checks();
    interrupt_checks();
    pmp_checks();
    fw_debug_checks(2);
    fw_finish();
    return 0;
}
