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

void run_tests(cdc::test::tlm_probe& probe,
               sc_core::sc_vector<sc_core::sc_signal<bool>>& idle,
               sc_core::sc_signal<bool>& ast_ack) {
    using namespace sc_core;

    std::cout << "\n[TB] Clkmgr functional tests begin\n";

    // ── Test 1: EXTCLK_CTRL — enable external clock, high speed ──
    write32(probe, 0x8, (0x6 << 4) | 0x6); // SEL=True, HI_SPEED_SEL=True
    ast_ack.write(true);
    sc_core::wait(sc_core::SC_ZERO_TIME);
    uint32_t status = read32(probe, 0xc);
    CDC_CHECK(status == 0x6);

    // ── Test 2: disable external clock ──
    write32(probe, 0x8, (0x6 << 4) | 0x9); // SEL=False
    ast_ack.write(false);
    sc_core::wait(sc_core::SC_ZERO_TIME);
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
    for (unsigned i = 0; i < 4; ++i) {
        idle[i].write(true);
    }
    sc_core::wait(sc_core::sc_time(120, sc_core::SC_NS));
    uint32_t hints_status = read32(probe, 0x20);
    CDC_CHECK(hints_status == 0x0);

    std::cout << "\n[TB] Result: "
              << (cdc::test::failures() == 0 ? "PASS" : "FAIL")
              << " (" << cdc::test::failures() << " error(s))\n";

    sc_stop();
}

}

int sc_main(int, char*[]) {
    Clkmgr clkmgr_model("clkmgr_model");
    clkmgr_model.set_lc_state(Clkmgr::LcState::Dev);
    cdc::test::tlm_probe probe("probe");
    sc_core::sc_vector<sc_core::sc_signal<bool>> idle("idle", 4);
    sc_core::sc_signal<bool> ast_req("ast_req");
    sc_core::sc_signal<bool> ast_ack("ast_ack");

    probe.socket.bind(clkmgr_model.socket);
    for (unsigned i = 0; i < 4; ++i) {
        idle[i].write(false);
        clkmgr_model.idle_i[i](idle[i]);
    }
    clkmgr_model.io_clk_byp_req_o(ast_req);
    clkmgr_model.io_clk_byp_ack_i(ast_ack);
    ast_ack.write(false);

    sc_core::sc_spawn([&probe, &idle, &ast_ack] { run_tests(probe, idle, ast_ack); });
    sc_core::sc_start();

    return cdc::test::failures() == 0 ? 0 : 1;
}
