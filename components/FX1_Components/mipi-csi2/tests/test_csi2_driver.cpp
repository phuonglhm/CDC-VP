// The C99 reference driver (driver/) running on the controller model with the
// detailed D-PHY attached. The HAL maps read32/write32 onto TLM b_transport
// and delay_us onto wait(), from the bench thread. Datapath events that do
// not exist before M3 are injected through the controller's hw_* interface.
//
//   test_fx1_csi2_driver <case>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_csi2/csi2_rx_controller.h"
#include "fx1_csi2/fx1_csi2_drv.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_bfm/dphy_tx_bfm.h"
#include "fx1_csi2_phy/dphy_rx_wrapper.h"

using namespace fx1::csi2;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;

namespace {
int g_fail = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_fail;                                                                       \
        }                                                                                   \
    } while (0)

Csi2ControllerConfig ctl_cfg(const std::string& which) {
    Csi2ControllerConfig c;
    if (which == "probe_bad_version") c.ip_version = 0x00010200u;  // MAS revision
    if (which == "lanes_max2") c.max_lanes = 2;
    return c;
}

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> socket{"socket"};
    sc_core::sc_signal<bool> resetn{"resetn", false};
    sc_core::sc_signal<bool> irq{"irq"};
    DphyRxWrapper phy{"phy_model"};
    Csi2RxController ctl;
    std::string which;
    // The sensor side holds every lane in LP-11 (Stop State needs a driven
    // LP-11, MAS step 6); absent in case no_sensor.
    std::unique_ptr<bfm::DphyTxBfm> sensor;
    fx1_csi2_dev dev{};
    fx1_csi2_hal hal{};

    SC_HAS_PROCESS(Tb);
    Tb(sc_core::sc_module_name nm, std::string w)
        : sc_core::sc_module(nm), ctl("csi2", phy, ctl_cfg(w)), which(std::move(w)) {
        socket.bind(ctl.csr_socket);
        ctl.resetn(resetn);
        ctl.irq(irq);
        hal.read32 = &Tb::hal_read32;
        hal.write32 = &Tb::hal_write32;
        hal.delay_us = &Tb::hal_delay_us;
        hal.ctx = this;
        if (which != "no_sensor") sensor = std::make_unique<bfm::DphyTxBfm>("sensor", phy.clkp, phy.clkn, phy.dp, phy.dn);
        SC_THREAD(run);
    }

    // ---- HAL
    uint32_t access(tlm::tlm_command cmd, uint32_t off, uint32_t v) {
        unsigned char b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        tlm::tlm_generic_payload p;
        sc_time d = sc_core::SC_ZERO_TIME;
        p.set_command(cmd);
        p.set_address(off);
        p.set_data_ptr(b);
        p.set_data_length(4);
        p.set_streaming_width(4);
        p.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(p, d);
        if (d != sc_core::SC_ZERO_TIME) wait(d);  // consume the access latency the target returned
        CHECK(p.get_response_status() == tlm::TLM_OK_RESPONSE);
        return b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
    }
    static uint32_t hal_read32(void* ctx, uint32_t off) {
        return static_cast<Tb*>(ctx)->access(tlm::TLM_READ_COMMAND, off, 0);
    }
    static void hal_write32(void* ctx, uint32_t off, uint32_t v) {
        static_cast<Tb*>(ctx)->access(tlm::TLM_WRITE_COMMAND, off, v);
    }
    static void hal_delay_us(void*, uint32_t us) { sc_core::wait(sc_time(us, SC_US)); }

    void up() {
        resetn.write(true);
        wait(1, SC_US);
    }

    // ------------------------------------------------------------------ cases
    void probe() {
        up();
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        CHECK(dev.version == 0x00010100u);
        CHECK(dev.max_lanes == 4);
        CHECK(fx1_csi2_phy_start(nullptr, 4, 100) == FX1_CSI2_ESTATE);
    }

    void probe_bad_version() {
        up();
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_ENODEV);
        CHECK(fx1_csi2_soft_reset(&dev, 100) == FX1_CSI2_ESTATE);  // not probed
    }

    void bringup4() {
        up();
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        CHECK(fx1_csi2_irq_enable(&dev, 1u << IRQ_STOP_STATE) == FX1_CSI2_OK);
        const sc_time t0 = sc_core::sc_time_stamp();
        CHECK(fx1_csi2_phy_start(&dev, 4, 2000) == FX1_CSI2_OK);
        std::printf("  phy_start(4) took %.1f us\n", (sc_core::sc_time_stamp() - t0).to_seconds() * 1e6);
        CHECK(dev.lanes == 4);
        fx1_csi2_status st{};
        CHECK(fx1_csi2_get_status(&dev, &st) == FX1_CSI2_OK);
        CHECK((st.lane_status & 0x01010101u) == 0x01010101u);
        CHECK(st.phy_status & FX1_CSI2_PHY_STATUS_INIT_DONE_MASK);
        wait(1, SC_NS);
        CHECK(irq.read());  // stop_state raised by the PHY bring-up
        CHECK(fx1_csi2_irq_handle(&dev) == (1u << IRQ_STOP_STATE));
        wait(1, SC_NS);
        CHECK(!irq.read());
        CHECK(fx1_csi2_irq_handle(&dev) == 0);
        CHECK(fx1_csi2_core_enable(&dev, 1) == FX1_CSI2_OK);
        CHECK(fx1_csi2_soft_reset(&dev, 100) == FX1_CSI2_ENOCLK);  // clock lane not in HS (C20)
        CHECK(fx1_csi2_full_reset(&dev, 100) == FX1_CSI2_OK);
        CHECK(dev.lanes == 0);
        CHECK(fx1_csi2_get_status(&dev, &st) == FX1_CSI2_OK);
        CHECK(!(st.phy_status & FX1_CSI2_PHY_STATUS_INIT_DONE_MASK));  // registers reset: PHY disabled
    }

    void lanes_max2() {
        up();
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        CHECK(dev.max_lanes == 2);
        CHECK(fx1_csi2_phy_start(&dev, 4, 2000) == FX1_CSI2_EINVAL);
        CHECK(fx1_csi2_phy_start(&dev, 3, 2000) == FX1_CSI2_EINVAL);
        CHECK(fx1_csi2_phy_start(&dev, 2, 2000) == FX1_CSI2_OK);
        CHECK(ctl.phy_adapter().applied_lane_mask() == 0x3);
        CHECK(fx1_csi2_phy_start(&dev, 1, 2000) == FX1_CSI2_OK);
        CHECK(ctl.phy_adapter().applied_lane_mask() == 0x1);
        CHECK(fx1_csi2_phy_stop(&dev) == FX1_CSI2_OK);
        wait(1, SC_US);
        CHECK(ctl.phy_adapter().applied_lane_mask() == 0);
    }

    void timeouts() {
        up();
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        // Calibration takes ~340 us: a 20 us budget must expire, bounded.
        sc_time t0 = sc_core::sc_time_stamp();
        CHECK(fx1_csi2_phy_start(&dev, 4, 20) == FX1_CSI2_ETIMEDOUT);
        CHECK(sc_core::sc_time_stamp() - t0 <= sc_time(60, SC_US));
        CHECK(dev.lanes == 0);
        // A drain that never completes (M13): soft reset times out, bounded.
        ctl.hw_set_capture_in_flight(true);
        t0 = sc_core::sc_time_stamp();
        CHECK(fx1_csi2_soft_reset(&dev, 50) == FX1_CSI2_ETIMEDOUT);
        CHECK(sc_core::sc_time_stamp() - t0 <= sc_time(70, SC_US));
        ctl.hw_set_capture_in_flight(false);
        ctl.hw_drain_complete();
        CHECK(fx1_csi2_soft_reset(&dev, 50) == FX1_CSI2_ENOCLK);  // done, but no byte clock (C20)
    }

    // Lane pads not driven: no Stop State, the bring-up must fail bounded.
    void no_sensor() {
        up();
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        const sc_time t0 = sc_core::sc_time_stamp();
        CHECK(fx1_csi2_phy_start(&dev, 4, 600) == FX1_CSI2_ETIMEDOUT);
        CHECK(sc_core::sc_time_stamp() - t0 <= sc_time(700, SC_US));
        fx1_csi2_status st{};
        CHECK(fx1_csi2_get_status(&dev, &st) == FX1_CSI2_OK);
        CHECK(st.phy_status & FX1_CSI2_PHY_STATUS_INIT_DONE_MASK);  // calibrated, but no LP-11 on the pads
        CHECK(!(st.phy_status & FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK));
    }

    void irq_ts_gsp() {
        up();
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        CHECK(fx1_csi2_irq_enable(&dev, 1u << IRQ_FRAME_RECEIVED) == FX1_CSI2_OK);
        CHECK(fx1_csi2_irq_enable(&dev, 1u << 24) == FX1_CSI2_EINVAL);
        ctl.hw_raise_irq(IRQ_FRAME_RECEIVED);
        ctl.hw_raise_irq(IRQ_CRC_ERROR);  // not enabled
        wait(1, SC_NS);
        CHECK(irq.read());
        CHECK(fx1_csi2_irq_handle(&dev) == (1u << IRQ_FRAME_RECEIVED));
        fx1_csi2_status st{};
        CHECK(fx1_csi2_get_status(&dev, &st) == FX1_CSI2_OK);
        CHECK(st.irq_status == (1u << IRQ_CRC_ERROR));  // masked flag left for polling
        wait(5, SC_US);
        ctl.hw_latch_timestamp();
        uint64_t ns = 0;
        CHECK(fx1_csi2_read_timestamp(&dev, &ns) == FX1_CSI2_OK);
        CHECK(ns > 5000 && ns < 7000);
        uint8_t vc = 0, dt = 0;
        uint16_t data = 0;
        CHECK(fx1_csi2_gsp_pop(&dev, &vc, &dt, &data) == FX1_CSI2_ENODATA);
        CHECK(ctl.hw_gsp_push(1, 0x0A, 0xBEEF));
        CHECK(ctl.hw_gsp_push(2, 0x0B, 0x1234));
        CHECK(fx1_csi2_gsp_pop(&dev, &vc, &dt, &data) == FX1_CSI2_OK);
        CHECK(vc == 1 && dt == 0x0A && data == 0xBEEF);
        CHECK(fx1_csi2_gsp_pop(&dev, &vc, &dt, &data) == FX1_CSI2_OK);
        CHECK(vc == 2 && dt == 0x0B && data == 0x1234);
        CHECK(fx1_csi2_gsp_pop(&dev, &vc, &dt, &data) == FX1_CSI2_ENODATA);
    }

    void run() {
        const std::map<std::string, std::function<void()>> cases = {
            {"probe", [this] { probe(); }},           {"probe_bad_version", [this] { probe_bad_version(); }},
            {"bringup4", [this] { bringup4(); }},     {"lanes_max2", [this] { lanes_max2(); }},
            {"timeouts", [this] { timeouts(); }},     {"irq_ts_gsp", [this] { irq_ts_gsp(); }},
            {"no_sensor", [this] { no_sensor(); }},
        };
        const auto it = cases.find(which);
        if (it == cases.end()) {
            std::fprintf(stderr, "unknown case %s\n", which.c_str());
            ++g_fail;
        } else {
            it->second();
        }
        sc_core::sc_stop();
    }
};
}  // namespace

int sc_main(int argc, char** argv) {
    sc_core::sc_set_time_resolution(1, sc_core::SC_FS);
    const std::string which = argc > 1 ? argv[1] : "probe";
    Tb tb("tb", which);
    sc_core::sc_start(20, sc_core::SC_MS);
    if (sc_core::sc_get_status() != sc_core::SC_STOPPED) {
        std::printf("case did not finish\n");
        ++g_fail;
    }
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
