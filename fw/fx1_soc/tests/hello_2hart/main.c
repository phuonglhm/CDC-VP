/*
 * G1 smoke: both harts boot from one image.
 * Hart 0 prints, releases hart 1 through MSIP and waits for its report.
 * Checks: mhartid values, separate stacks inside the stack region, hart 1
 * released only after .bss was cleared, shared data seen by both harts.
 */
#include "fx1_fw.h"

static volatile uint32_t hart1_alive;   /* .bss: cleared by hart 0 */
static volatile uint32_t hart1_sp;
static volatile uint32_t shared_token = 0x1234; /* .data */

static uint32_t current_sp(void)
{
    uint32_t sp;
    __asm__ volatile("mv %0, sp" : "=r"(sp));
    return sp;
}

void secondary_main(uint32_t hart)
{
    fx1_check(hart == 1 && fx1_hartid() == 1, 0x101, "hart 1 has mhartid 1");
    hart1_sp = current_sp();
    fx1_check(shared_token == 0xCAFE, 0x102, "hart 1 sees the value hart 0 wrote");
    fx1_log("hart 1 up");
    hart1_alive = 1;
    for (;;) fx1_wfi();
}

int main(void)
{
    fx1_check(fx1_hartid() == 0, 0x001, "main runs on hart 0");
    fx1_log("Hello from FX1 hart 0");
    shared_token = 0xCAFE;
    fx1_release_secondaries();

    const uint64_t start = fx1_mtime();
    while (!hart1_alive) {
        if (fx1_mtime() - start > FX1_MTIME_HZ / 100) { /* 10 ms */
            fx1_log("hart 1 did not report");
            return 0x002;
        }
    }

    const uint32_t sp0 = current_sp();
    const uint32_t stack_lo = FX1_FW_STACK_BASE;
    const uint32_t stack_hi = FX1_FW_STACK_BASE + FX1_NUM_HARTS * FX1_FW_STACK_SIZE;
    fx1_check(sp0 > stack_lo && sp0 <= stack_lo + FX1_FW_STACK_SIZE, 0x003, "hart 0 stack in slot 0");
    fx1_check(hart1_sp > stack_lo + FX1_FW_STACK_SIZE && hart1_sp <= stack_hi, 0x004,
              "hart 1 stack in slot 1");
    fx1_log_hex("hart 1 sp", hart1_sp);
    return 0;
}
