// Bounded waits of the C99 driver against a fake HAL (no SystemC): the
// hardware never finishes its reset, so every wait must end with ETIMEDOUT
// after delays that add up to exactly the timeout (never more), whatever the
// timeout and polling interval (review M2 F3).
//
//   test_fx1_csi2_driver_timeout <case>
//     zero | below_poll | max_poll10 | max_poll_big | poll_zero
//     abort_in_budget | abort_phase2_late | abort_zero | abort_below_poll
//     phy_start_late | phy_start_in_budget
//
// The abort_* / phy_start_* cases (review M4b F1) use a fake whose time is the
// sum of the delays requested so far: several conditions waited for in turn
// by one call share one budget.
//
// Guard: once more delays than expected have been requested the fake reports
// "reset done", so a looping driver returns OK (a FAIL here) instead of
// hanging the test.
#include <cstdint>
#include <cstdio>
#include <string>

#include "fx1_csi2/fx1_csi2_drv.h"
#include "fx1_csi2/fx1_csi2_regs.h"

namespace {
struct Fake {
    uint64_t total_us = 0;
    uint64_t calls = 0;
    uint64_t max_calls = 0;
    bool guard_hit = false;
};

uint32_t fake_read32(void* ctx, uint32_t off) {
    auto* f = static_cast<Fake*>(ctx);
    switch (off) {
    case FX1_CSI2_IP_VERSION: return FX1_CSI2_SUPPORTED_VERSION;
    case FX1_CSI2_PROTOCOL_CONFIG: return 3u << FX1_CSI2_PROTOCOL_CONFIG_MAX_LANES_SHIFT;
    case FX1_CSI2_CORE_STATUS:
        return f->guard_hit ? 0u : FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK;  // never finishes
    default: return 0;
    }
}
void fake_write32(void*, uint32_t, uint32_t) {}
void fake_delay_us(void* ctx, uint32_t us) {
    auto* f = static_cast<Fake*>(ctx);
    f->total_us += us;
    if (++f->calls > f->max_calls) f->guard_hit = true;
}

struct Case {
    uint32_t poll_us, timeout_us;
    uint64_t want_calls;
};

int run(const Case& c) {
    Fake f;
    f.max_calls = c.want_calls;
    fx1_csi2_hal hal{&fake_read32, &fake_write32, &fake_delay_us, &f};
    fx1_csi2_dev dev{};
    if (fx1_csi2_probe(&dev, &hal) != FX1_CSI2_OK) {
        std::printf("probe failed\n");
        return 1;
    }
    dev.poll_us = c.poll_us;
    const int rc = fx1_csi2_soft_reset(&dev, c.timeout_us);
    std::printf("poll=%u timeout=%u: rc=%d delays=%llu total=%llu us guard=%d\n", c.poll_us, c.timeout_us, rc,
                (unsigned long long)f.calls, (unsigned long long)f.total_us, f.guard_hit);
    bool ok = rc == FX1_CSI2_ETIMEDOUT && !f.guard_hit && f.total_us == c.timeout_us && f.calls == c.want_calls;
    return ok ? 0 : 1;
}
// ---- timeline fake: register values as a function of the time spent in delays
struct Timed {
    uint64_t t = 0;  // us
    uint64_t busy_until = 0, rip_until = 0, cl_stop_at = 0, lane_stop_at = 0;
    uint32_t dma_control = 0;
    uint64_t guard = 100000;
};
uint32_t timed_read32(void* ctx, uint32_t off) {
    auto* f = static_cast<Timed*>(ctx);
    switch (off) {
    case FX1_CSI2_IP_VERSION: return FX1_CSI2_SUPPORTED_VERSION;
    case FX1_CSI2_PROTOCOL_CONFIG: return 3u << FX1_CSI2_PROTOCOL_CONFIG_MAX_LANES_SHIFT;
    case FX1_CSI2_DMA_STATUS: return f->t < f->busy_until ? FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK : 0u;
    case FX1_CSI2_CORE_STATUS: return f->t < f->rip_until ? FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK : 0u;
    case FX1_CSI2_PHY_STATUS: return f->t >= f->cl_stop_at ? FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK : 0u;
    case FX1_CSI2_LANE_STATUS: return f->t >= f->lane_stop_at ? 0x01010101u : 0u;
    case FX1_CSI2_DMA_CONTROL: return f->dma_control;
    default: return 0;
    }
}
void timed_write32(void* ctx, uint32_t off, uint32_t v) {
    if (off == FX1_CSI2_DMA_CONTROL) static_cast<Timed*>(ctx)->dma_control = v;
}
void timed_delay_us(void* ctx, uint32_t us) {
    auto* f = static_cast<Timed*>(ctx);
    f->t += us;
    if (f->t > f->guard) f->busy_until = f->rip_until = f->cl_stop_at = f->lane_stop_at = 0;  // never hang
}

int run_timed(const std::string& which) {
    Timed f;
    fx1_csi2_hal hal{&timed_read32, &timed_write32, &timed_delay_us, &f};
    fx1_csi2_dev dev{};
    if (fx1_csi2_probe(&dev, &hal) != FX1_CSI2_OK) return 1;
    dev.poll_us = 1;
    int rc = 0, want_rc = FX1_CSI2_OK;
    uint64_t want_t = 0;
    bool abort_left = false, check_abort = false;
    const uint32_t abort = FX1_CSI2_DMA_CONTROL_DMA_ABORT_MASK;
    f.dma_control = FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK | FX1_CSI2_DMA_CONTROL_DMA_START_MASK;
    if (which == "abort_in_budget") {  // busy clears at 4, reset_in_progress at 8, budget 10
        f.busy_until = 4;
        f.rip_until = 8;
        rc = fx1_csi2_dma_abort(&dev, 10);
        want_t = 8;
        check_abort = true;
    } else if (which == "abort_phase2_late") {  // busy at 8, reset still pending at 10
        f.busy_until = 8;
        f.rip_until = 14;
        rc = fx1_csi2_dma_abort(&dev, 10);
        want_rc = FX1_CSI2_ETIMEDOUT;
        want_t = 10;
        abort_left = true;
        check_abort = true;
    } else if (which == "abort_zero") {
        f.busy_until = 5;
        rc = fx1_csi2_dma_abort(&dev, 0);
        want_rc = FX1_CSI2_ETIMEDOUT;
        abort_left = true;
        check_abort = true;
    } else if (which == "abort_below_poll") {
        dev.poll_us = 10;
        f.busy_until = 100;
        rc = fx1_csi2_dma_abort(&dev, 5);
        want_rc = FX1_CSI2_ETIMEDOUT;
        want_t = 5;
        abort_left = true;
        check_abort = true;
    } else if (which == "phy_start_late") {  // PHY reset done at 3, clock lane at 7, lanes at 12
        f.rip_until = 3;
        f.cl_stop_at = 7;
        f.lane_stop_at = 12;
        rc = fx1_csi2_phy_start(&dev, 4, 10);
        want_rc = FX1_CSI2_ETIMEDOUT;
        want_t = 10;
    } else if (which == "phy_start_in_budget") {
        f.rip_until = 3;
        f.cl_stop_at = 7;
        f.lane_stop_at = 9;
        rc = fx1_csi2_phy_start(&dev, 4, 10);
        want_t = 9;
    } else {
        std::printf("unknown case %s\n", which.c_str());
        return 1;
    }
    std::printf("%s: rc=%d time=%llu us dma_control=0x%x\n", which.c_str(), rc, (unsigned long long)f.t, f.dma_control);
    bool ok = rc == want_rc && f.t == want_t;
    if (check_abort) ok &= bool(f.dma_control & abort) == abort_left;
    if (check_abort && !abort_left) ok &= (f.dma_control & 0x7u) == 0;  // abort, enable and start cleared
    return ok ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
    const std::string which = argc > 1 ? argv[1] : "zero";
    if (which.rfind("abort_", 0) == 0 || which.rfind("phy_start_", 0) == 0) {
        const int rc = run_timed(which);
        std::printf("%s: %s\n", which.c_str(), rc ? "FAIL" : "PASS");
        return rc;
    }
    Case c{};
    if (which == "zero") c = {10, 0, 0};
    else if (which == "below_poll") c = {10, 5, 1};
    else if (which == "max_poll10") c = {10, UINT32_MAX, (uint64_t(UINT32_MAX) + 9) / 10};
    else if (which == "max_poll_big") c = {0x80000000u, UINT32_MAX, 2};
    else if (which == "poll_zero") c = {0, 100, 100};  // 0 is taken as 1 us
    else {
        std::printf("unknown case %s\n", which.c_str());
        return 1;
    }
    const int rc = run(c);
    std::printf("%s: %s\n", which.c_str(), rc ? "FAIL" : "PASS");
    return rc;
}
