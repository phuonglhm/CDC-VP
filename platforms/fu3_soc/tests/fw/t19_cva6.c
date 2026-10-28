// Plan T19 GDB test, CVA6 side (VP verification firmware), preloaded into
// CVA6_ISRAM and released by t19_ibex.c. Single-precision values in fa0-fa4,
// an inexact division (fflags.NX), then the label t19_fp_stop where the GDB
// test stops, reads them and writes fa2; fa2 as it is afterwards becomes the
// result the Ibex prints ("cva6 result").
//   fa0 = 1.5 (0x3fc00000), fa1 = 2.25 (0x40100000), fa2 = fa0 * fa1 = 3.375
//   (0x40580000), fa3 = 3.0, fa4 = 1.0 / 3.0 (inexact)
#include "fw_common.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_CVA6;

#define T19_CVA6_DONE (*(volatile uint32_t *)0x0103f208u)
#define T19_CVA6_RESULT (*(volatile uint32_t *)0x0103f20cu)

int main(void)
{
    uint32_t result;
    __asm__ volatile("li t0, 0x2000\n"   // mstatus.FS = Initial
                     "csrs mstatus, t0\n"
                     "csrw fflags, zero\n"
                     "li t0, 0x3fc00000\n"
                     "fmv.w.x fa0, t0\n"
                     "li t0, 0x40100000\n"
                     "fmv.w.x fa1, t0\n"
                     "fmul.s fa2, fa0, fa1\n"
                     "li t0, 0x40400000\n"
                     "fmv.w.x fa3, t0\n"
                     "li t0, 0x3f800000\n"
                     "fmv.w.x fa4, t0\n"
                     "fdiv.s fa4, fa4, fa3\n"
                     ".globl t19_fp_stop\n"
                     "t19_fp_stop:\n"
                     "nop\n"
                     "fmv.x.w %0, fa2\n"
                     : "=r"(result)
                     :
                     : "t0", "fa0", "fa1", "fa2", "fa3", "fa4", "memory");
    T19_CVA6_RESULT = result;
    T19_CVA6_DONE = 1;
    for (;;)
        __asm__ volatile("wfi");
    return 0;
}
