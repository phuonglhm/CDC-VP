/*
 * FX1 NPU N1: the NPU aperture as the CPU sees it (plan FX1_NPU_V47).
 *   absent (fx1_soc --no-npu): a load from the aperture is a load access
 *     fault; the image reports "npu: absent" and passes
 *   present: identification and reset values of the VP bank, a read/write
 *     register, and the access classes: byte / halfword accesses, an unmapped
 *     offset and a write to a read-only register are access faults (the NPU
 *     answers a TLM error, the CPU port maps it), PLIC source 4 low at idle
 * Offsets: components/FX1_Components/npu/include/fx1_npu/fx1_npu_regmap.h
 * (wrapper bank at +0x30000; the firmware driver headers come with N2).
 */
#include "fx1_fw.h"

#define NPU(off) (FX1_NPU_VP_BASE + (off))
#define BANK(off) NPU(0x30000u + (off))
#define NPU_STATUS BANK(0x0004u)
#define NPU_CORE_ID BANK(0x1020u)
#define NPU_VP_ID BANK(0x1800u)
#define NPU_VP_ABI BANK(0x1804u)
#define NPU_VP_MODEL_HASH BANK(0x1808u)
#define NPU_VP_FEATURE BANK(0x180Cu)
#define NPU_VP_QUEUE_DEPTH BANK(0x1810u)
#define NPU_VP_QUEUE_FREE BANK(0x1814u)
#define NPU_VP_STATUS BANK(0x181Cu)
#define NPU_VP_ERROR BANK(0x1820u)
#define NPU_VP_PUSH_WAIT_NS BANK(0x1828u)
#define NPU_RICH_STATUS NPU(0x10318u)
#define NPU_RICH_RETIRED NPU(0x1031Cu)

static volatile uint32_t got_cause, got_tval, traps;
static volatile uint32_t resume_at;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mepc;
    got_cause = mcause;
    got_tval = mtval;
    ++traps;
    return resume_at;
}

/* lw from `addr`; returns 1 and the value when it did not fault. */
static int try_lw(uint32_t addr, uint32_t* value)
{
    const uint32_t before = traps;
    uint32_t v = 0;
    __asm__ volatile("la   t0, 1f\n"
                     "sw   t0, 0(%1)\n"
                     "lw   %0, 0(%2)\n"
                     "1:\n"
                     : "=&r"(v)
                     : "r"(&resume_at), "r"(addr)
                     : "t0", "memory");
    *value = v;
    return traps == before;
}

static void expect_fault(uint32_t before, uint32_t cause, uint32_t addr, uint32_t code, const char* what)
{
    if (traps != before + 1 || got_cause != cause || got_tval != addr) {
        fx1_log_hex("  traps ", traps - before);
        fx1_log_hex("  mcause", got_cause);
        fx1_log_hex("  mtval ", got_tval);
        fx1_check(0, code, what);
    }
    fx1_log(what);
}

int main(void)
{
    fx1_set_trap_handler(handler);
    uint32_t id = 0;
    if (!try_lw(NPU_VP_ID, &id)) {
        fx1_check(got_cause == 5 && got_tval == NPU_VP_ID, 0x01, "aperture load fault (mcause 5)");
        fx1_log("npu: absent (the aperture answers DECERR: load access fault)");
        return 0;
    }
    fx1_log_hex("npu: present, VP_ID", id);
    fx1_check(id == 0x46584E50u, 0x10, "VP_ID = FXNP");
    fx1_check(fx1_read32(NPU_VP_ABI) == 0x00010000u, 0x11, "VP_ABI 1.0");
    const uint32_t hash = fx1_read32(NPU_VP_MODEL_HASH);
    fx1_log_hex("npu: model manifest hash", hash);
    fx1_check(hash != 0, 0x12, "VP_MODEL_HASH set");
    const uint32_t feature = fx1_read32(NPU_VP_FEATURE);
    fx1_log_hex("npu: feature", feature);
    fx1_check((feature & 0x73u) == 0x73u && (feature & 0x88u) == 0, 0x13,
              "FEATURE: native GEMM, rich HasNpuTop, abort, bounded push, queue; no DMA_TENSOR/ATTN");
    fx1_check(fx1_read32(NPU_VP_QUEUE_DEPTH) == 16 && fx1_read32(NPU_VP_QUEUE_FREE) == 16, 0x14, "queue 16, all free");
    fx1_check(fx1_read32(NPU_VP_STATUS) == 1u && fx1_read32(NPU_VP_ERROR) == 0, 0x15, "VP_STATUS quiescent, no error");
    fx1_check(fx1_read32(NPU_CORE_ID) == 0x53413432u, 0x16, "CORE_ID SA42");
    fx1_check(fx1_read32(NPU_STATUS) == 0x8u, 0x17, "STATUS idle");
    fx1_check(fx1_read32(NPU_RICH_STATUS) == 0 && fx1_read32(NPU_RICH_RETIRED) == 0, 0x18, "RICH_STATUS / RETIRED 0");
    fx1_write32(NPU_VP_PUSH_WAIT_NS, 2500);
    fx1_check(fx1_read32(NPU_VP_PUSH_WAIT_NS) == 2500, 0x19, "VP_PUSH_WAIT_NS read/write");
    fx1_write32(NPU_VP_PUSH_WAIT_NS, 100000);
    fx1_log("npu: identification and reset values");

    /* Access classes: each one is an access fault. */
    uint32_t t = traps;
    __asm__ volatile("la t0, 1f\n sw t0, 0(%0)\n lb t1, 0(%1)\n1:\n" : : "r"(&resume_at), "r"(NPU_VP_ID) : "t0", "t1", "memory");
    expect_fault(t, 5, NPU_VP_ID, 0x20, "byte load: load access fault");
    t = traps;
    __asm__ volatile("la t0, 1f\n sw t0, 0(%0)\n sh zero, 0(%1)\n1:\n" : : "r"(&resume_at), "r"(NPU_VP_PUSH_WAIT_NS) : "t0", "memory");
    expect_fault(t, 7, NPU_VP_PUSH_WAIT_NS, 0x21, "halfword store: store access fault");
    fx1_check(fx1_read32(NPU_VP_PUSH_WAIT_NS) == 100000, 0x22, "the faulting store changed nothing");
    t = traps;
    uint32_t v = 0;
    try_lw(BANK(0x1F00u), &v);
    expect_fault(t, 5, BANK(0x1F00u), 0x23, "unmapped bank offset: load access fault");
    t = traps;
    __asm__ volatile("la t0, 1f\n sw t0, 0(%0)\n sw %2, 0(%1)\n1:\n" : : "r"(&resume_at), "r"(NPU_VP_ID), "r"(1u) : "t0", "memory");
    expect_fault(t, 7, NPU_VP_ID, 0x24, "store to read-only VP_ID: store access fault");
    fx1_check(fx1_read32(NPU_VP_ID) == 0x46584E50u, 0x25, "VP_ID unchanged");
    fx1_check((fx1_plic_pending() & (1u << FX1_IRQ_NPU)) == 0, 0x26, "PLIC source 4 low at idle");
    fx1_log("npu: access classes fault as on every FX1 MMIO target");
    return 0;
}
