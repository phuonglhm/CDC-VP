//Author: trangnm20
#include "tlm_probe.h"
#include "clkmgr.h"
#include <cstdint>
#include <systemc>

namespace {

uint32_t read32(cdc::test::tlm_probe& probe, uint64_t addr) {
    uint32_t value = 0;
    probe.read(addr, &value, sizeof(value));
    return value;
}

void write32(cdc::test::tlm_probe& probe, uint64_t addr, uint32_t data) {
    probe.write(addr, &data, sizeof(data));
}

void run_tests(cdc::test::tlm_probe& probe) {
    using namespace sc_core;

    std::cout << "\n[TB] Clkmgr functional tests begin\n";

    // ── Test 1: EXTCLK_CTRL — enable external clock, high speed ──
    write32(probe, 0x8, (0x6 << 4) | 0x6); // SEL=True, HI_SPEED_SEL=True
    uint32_t status = read32(probe, 0xc);
    CDC_CHECK(status == 0x6);

    // ── Test 2: disable external clock ──
    write32(probe, 0x8, (0x6 << 4) | 0x9); // SEL=False
    status = read32(probe, 0xc);
    CDC_CHECK(status == 0x9);

    // ── Test 3: EXTCLK_CTRL_REGWEN lock ──
    uint32_t regwen = read32(probe, 0x4);
    CDC_CHECK(regwen == 0x1); // unlocked at reset

    write32(probe, 0x4, 0x0); // lock it
    regwen = read32(probe, 0x4);
    CDC_CHECK(regwen == 0x0);

    write32(probe, 0x8, (0x6 << 4) | 0x6); // attempt write while locked
    status = read32(probe, 0xc);
    CDC_CHECK(status == 0x9); // unchanged — still disabled from Test 2

    write32(probe, 0x4, 0x1); // attempt to unlock — rw0c, should not work
    regwen = read32(probe, 0x4);
    CDC_CHECK(regwen == 0x0);

    // ── Test 4: CLK_ENABLES read/write ──
    write32(probe, 0x18, 0x5);
    uint32_t enables = read32(probe, 0x18);
    CDC_CHECK(enables == 0x5);

    // ── Test 5: CLK_HINTS / CLK_HINTS_STATUS with idle tracking ──
    write32(probe, 0x1c, 0x0); // request shutoff for all 4 blocks
    uint32_t hints_status = read32(probe, 0x20);
    // all blocks default idle_ = true in clkmgr.h, so all should clear
    CDC_CHECK(hints_status == 0x0);

    std::cout << "\n[TB] Result: "
              << (cdc::test::failures() == 0 ? "PASS" : "FAIL")
              << " (" << cdc::test::failures() << " error(s))\n";

    sc_stop();
}

}

int sc_main(int, char*[]) {
    Clkmgr clkmgr_model("clkmgr_model");
    cdc::test::tlm_probe probe("probe");

    probe.socket.bind(clkmgr_model.socket);

    sc_core::sc_spawn([&probe] { run_tests(probe); });
    sc_core::sc_start();

    return cdc::test::failures() == 0 ? 0 : 1;
}