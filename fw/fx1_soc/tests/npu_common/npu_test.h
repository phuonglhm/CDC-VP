/*
 * Helpers shared by the FX1 NPU firmware tests (header only, hart side).
 */
#ifndef NPU_TEST_H
#define NPU_TEST_H

#include "fx1_fw.h"
#include "fx1_npu.h"

#define NPU_MS(n) ((uint64_t)FX1_MTIME_HZ / 1000u * (n))

/* Prints the error registers of the last failure. */
static inline void npu_log_last(const fx1_npu_t* npu)
{
    fx1_log_hex("  STATUS     ", npu->last.status);
    fx1_log_hex("  LAST_ERROR ", npu->last.last_error);
    fx1_log_hex("  RICH_STATUS", npu->last.rich_status);
    fx1_log_hex("  VP_ERROR   ", npu->last.vp_error);
    fx1_log_hex("  VP_INFO    ", npu->last.vp_error_info);
    fx1_log_hex("  VP_STATUS  ", npu->last.vp_status);
}

/* Fails with `code` unless `s` is `want`; logs the error registers. */
static inline void npu_expect(const fx1_npu_t* npu, fx1_npu_status_t s, fx1_npu_status_t want, uint32_t code, const char* what)
{
    if (s != want) {
        fx1_log(fx1_npu_status_name(s));
        npu_log_last(npu);
    }
    fx1_check(s == want, code, what);
}

/* Bytes of [a, a + n) that differ from `b`. */
static inline uint32_t npu_diff(const volatile uint8_t* a, const uint8_t* b, uint32_t n)
{
    uint32_t bad = 0;
    for (uint32_t i = 0; i < n; ++i) bad += a[i] != b[i];
    return bad;
}

/* Routes PLIC source FX1_IRQ_NPU to `context` (priority 2, threshold 0) with
 * interrupts still masked, and drops a request the PLIC latched before (a
 * level the driver already acked while the source was routed or not). Returns
 * the number of stale claims. */
static inline uint32_t npu_plic_route(uint32_t context)
{
    fx1_plic_set_priority(FX1_IRQ_NPU, 2);
    fx1_plic_set_threshold(context, 0);
    fx1_plic_enable(context, FX1_IRQ_NPU, 1);
    uint32_t stale = 0, id;
    while ((id = fx1_plic_claim(context)) != 0) {
        fx1_plic_complete(context, id);
        ++stale;
    }
    return stale;
}

static inline fx1_npu_status_t npu_test_init(fx1_npu_t* npu, uint32_t required, int irq)
{
    return fx1_npu_init(npu, FX1_NPU_VP_BASE, required, irq, fx1_mtime, FX1_MTIME_HZ);
}

#endif
