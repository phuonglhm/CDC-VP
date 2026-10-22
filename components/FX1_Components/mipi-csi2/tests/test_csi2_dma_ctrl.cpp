// M4b: the DMA of the controller under delayed and failing write responses,
// reset / abort / disable during a write in flight, reset epochs, and the C99
// driver DMA API. Source: the Test Pattern Generator (programmed over the CSR),
// so no lane traffic is needed; the D-PHY model is instantiated but idle.
// Memory: TlmMem with a hook that holds or fails the n-th write.
//
//   test_fx1_csi2_dma_ctrl <case>
//
// Oracles: TPG pattern (MAS 6.5, written out here independently), the layout
// rules M02-M08, ownership / IRQ / counter rules M07, M10-M17, M19.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fx1_csi2/csi2_rx_controller.h"
#include "fx1_csi2/fx1_csi2_drv.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_bfm/csi2_packet.h"
#include "fx1_csi2_phy/dphy_rx_wrapper.h"
#include "tlm_mem.h"

using namespace fx1::csi2;
using sc_core::sc_time;
using sc_core::SC_NS;
using sc_core::SC_US;
using Bytes = std::vector<uint8_t>;

namespace {
int g_fail = 0;
#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);    \
            ++g_fail;                                                                       \
        }                                                                                   \
    } while (0)
#define CHECK_EQ(a, b)                                                                                     \
    do {                                                                                                   \
        const auto va_ = (a);                                                                              \
        const auto vb_ = (b);                                                                              \
        if (va_ != vb_) {                                                                                  \
            std::fprintf(stderr, "%s:%d: CHECK_EQ failed: %s = 0x%llX, expected 0x%llX\n", __FILE__, __LINE__, \
                         #a, (unsigned long long)va_, (unsigned long long)vb_);                            \
            ++g_fail;                                                                                      \
        }                                                                                                  \
    } while (0)

constexpr uint32_t kB0 = 0x80000000u, kB1 = 0x80100000u, kSize = 0x1000;
constexpr unsigned kW = 32, kH = 8, kBpl = 40, kStride = 48;  // TPG 32x8 RAW10, packed

// TPG oracle (MAS 6.5, D11/C35): band = min(7, x / (W >> 3)), colour bars of
// MAS Table 625, RGGB.
Bytes tpg_line(unsigned y) {
    static const bool R[8] = {1, 1, 0, 0, 1, 1, 0, 0}, G[8] = {1, 1, 1, 1, 0, 0, 0, 0}, B[8] = {1, 0, 1, 0, 1, 0, 1, 0};
    std::vector<uint16_t> px(kW);
    for (unsigned x = 0; x < kW; ++x) {
        const unsigned band = std::min(7u, x / (kW >> 3));
        const bool on = (y % 2 == 0 && x % 2 == 0) ? R[band] : (y % 2 == 1 && x % 2 == 1) ? B[band] : G[band];
        px[x] = on ? 0x3FF : 0;
    }
    return bfm::pack_raw10(px);
}

constexpr uint32_t kRip = FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK;
constexpr uint32_t kBusy = FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK;
uint32_t irqbit(unsigned b) { return 1u << b; }

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> socket{"socket"};
    sc_core::sc_signal<bool> resetn{"resetn", false};
    sc_core::sc_signal<bool> irq{"irq"};
    DphyRxWrapper phy{"phy_model"};
    Csi2RxController ctl;
    TlmMem ddr{"ddr", 0x80000000ull, 4u << 20};
    std::string which;
    fx1_csi2_hal hal{};
    fx1_csi2_dev dev{};
    // write hook: hold the n-th write until release(), or fail it
    unsigned writes = 0, hold_nth = 0, fail_nth = 0, fail_partial = 0;
    tlm::tlm_response_status fail_status = tlm::TLM_GENERIC_ERROR_RESPONSE;
    bool held = false;
    sc_core::sc_event gate;
    tlm::tlm_generic_payload* last_ = nullptr;

    SC_HAS_PROCESS(Tb);
    Tb(sc_core::sc_module_name nm, std::string w) : sc_core::sc_module(nm), ctl("csi2", phy), which(std::move(w)) {
        socket.bind(ctl.csr_socket);
        ctl.resetn(resetn);
        ctl.irq(irq);
        ctl.dma_socket.bind(ddr.socket);
        ddr.hook = [this](tlm::tlm_generic_payload& p) {
            last_ = &p;
            ++writes;
            if (writes == hold_nth) {
                held = true;
                wait(gate);
                held = false;
            }
            if (writes == fail_nth && fail_partial) {  // part of the burst lands, then the error
                tlm::tlm_generic_payload& q = *last_;
                std::copy(q.get_data_ptr(), q.get_data_ptr() + std::min<unsigned>(fail_partial, q.get_data_length()),
                          ddr.mem.begin() + (q.get_address() - ddr.base));
            }
            return writes == fail_nth ? fail_status : tlm::TLM_OK_RESPONSE;
        };
        SC_THREAD(run);
    }

    uint32_t xfer(tlm::tlm_command cmd, uint32_t off, uint32_t v) {
        unsigned char b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        tlm::tlm_generic_payload p;
        sc_time d = sc_core::SC_ZERO_TIME;
        p.set_command(cmd);
        p.set_address(off);
        p.set_data_ptr(b);
        p.set_data_length(4);
        p.set_streaming_width(4);
        socket->b_transport(p, d);
        if (d != sc_core::SC_ZERO_TIME) wait(d);
        CHECK(p.get_response_status() == tlm::TLM_OK_RESPONSE);
        return b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
    }
    uint32_t rd(uint32_t off) { return xfer(tlm::TLM_READ_COMMAND, off, 0); }
    void wr(uint32_t off, uint32_t v) { xfer(tlm::TLM_WRITE_COMMAND, off, v); }
    static uint32_t hal_read32(void* c, uint32_t off) { return static_cast<Tb*>(c)->rd(off); }
    static void hal_write32(void* c, uint32_t off, uint32_t v) { static_cast<Tb*>(c)->wr(off, v); }
    static void hal_delay_us(void*, uint32_t us) { sc_core::wait(sc_time(us, SC_US)); }

    fx1_csi2_dma_cfg dma_cfg(bool release_on_error) const {
        fx1_csi2_dma_cfg c{};
        c.base[0] = kB0;
        c.base[1] = kB1;
        c.size = kSize;
        c.bytes_per_line = kBpl;
        c.line_padding = kStride - kBpl;
        c.max_burst_len = 255;
        c.raw16 = 0;
        c.release_on_error = release_on_error;
        return c;
    }
    // Out of reset, driver probe, DMA programmed and started, stream path off,
    // every interrupt source enabled, TPG geometry.
    void setup(bool release_on_error = true) {
        resetn.write(true);
        wait(1, SC_US);
        hal = {&Tb::hal_read32, &Tb::hal_write32, &Tb::hal_delay_us, this};
        dev = fx1_csi2_dev{};
        CHECK(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK);
        CHECK(fx1_csi2_stream_config(&dev, 0, 0) == FX1_CSI2_OK);
        const fx1_csi2_dma_cfg c = dma_cfg(release_on_error);
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_OK);
        CHECK(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK);
        CHECK(fx1_csi2_irq_enable(&dev, 0x00FFFFFF) == FX1_CSI2_OK);
        wr(FX1_CSI2_TPG_RESOLUTION, (kH << 16) | kW);
    }
    // Exactly one TPG frame: enable, then clear; the frame in progress completes.
    void tpg_frame() {
        wr(FX1_CSI2_TPG_CONTROL, FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK | FX1_CSI2_TPG_CONTROL_TPG_PIXEL_FORMAT_MASK);
        wait(40, SC_NS);
        wr(FX1_CSI2_TPG_CONTROL, 0);
    }
    bool wait_held(unsigned max_ns = 20000) {
        for (unsigned t = 0; t < max_ns && !held; t += 10) wait(10, SC_NS);
        return held;
    }
    void release() {
        gate.notify(sc_core::SC_ZERO_TIME);
        wait(1, SC_NS);
    }
    bool side_holds(uint32_t base, unsigned lines) {
        bool ok = true;
        for (unsigned y = 0; y < lines; ++y) {
            ok &= ddr.read(base + y * kStride, kBpl) == tpg_line(y);
            ok &= ddr.untouched(base + y * kStride + kBpl, kStride - kBpl);
        }
        return ok;
    }
    uint32_t own() { return rd(FX1_CSI2_BUFFER_OWNERSHIP); }
    bool poll_frame_count(uint32_t n, unsigned max_us = 20) {
        for (unsigned k = 0; k < max_us * 10 && rd(FX1_CSI2_FRAME_COUNT) != n; ++k) wait(100, SC_NS);
        return rd(FX1_CSI2_FRAME_COUNT) == n;
    }
    // Writes of the DMA made after time t.
    size_t writes_after(sc_time t) const {
        size_t n = 0;
        for (const auto& w : ddr.log) n += w.start > t;
        return n;
    }
    // A clean capture after a recovery: lands whole on `base`.
    void recover_frame(uint32_t base, uint32_t expect_own) {
        const uint32_t fc0 = rd(FX1_CSI2_FRAME_COUNT);
        tpg_frame();
        wait(3, SC_US);
        CHECK(side_holds(base, kH));
        CHECK_EQ(own(), expect_own);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), fc0 + 1);
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS) & kBusy, 0u);
    }

    // ------------------------------------------------------------------ cases

    // M13: soft_reset pulse while a write is outstanding; a second pulse during
    // the drain. reset_in_progress / dma_busy until the old response is back;
    // the side is rewound without event; nothing of the old epoch is published.
    void soft_reset_delayed() {
        setup();
        hold_nth = 3;
        tpg_frame();
        CHECK(wait_held());
        const uint32_t e0 = ctl.epoch();
        wr(FX1_CSI2_CORE_CONFIG, 0x3);
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        CHECK(ctl.epoch() != e0);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & kRip);
        CHECK(rd(FX1_CSI2_DMA_STATUS) & kBusy);
        wait(2, SC_US);
        wr(FX1_CSI2_CORE_CONFIG, 0x3);  // second pulse during the drain
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & kRip);
        const sc_time t_rel = sc_core::sc_time_stamp();
        release();
        wait(200, SC_NS);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & kRip, 0u);
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS) & kBusy, 0u);
        CHECK_EQ(own(), 0u);                       // rewound, HW_OWNED
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 0u);     // no buf_done, no frame event of the old epoch
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 0u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 0u);
        CHECK_EQ(writes_after(t_rel), 0u);         // queued writes were never issued
        recover_frame(kB0, 0x1);
    }

    // M13: full_reset / resetn while a write is outstanding.
    void reset_delayed(bool pin) {
        setup();
        hold_nth = 3;
        tpg_frame();
        CHECK(wait_held());
        if (pin) {
            resetn.write(false);
            wait(1, SC_US);
            resetn.write(true);
            wait(1, SC_US);  // PHY pulse over
        } else {
            wr(FX1_CSI2_CORE_CONFIG, 0x1 | FX1_CSI2_CORE_CONFIG_FULL_RESET_MASK);
            wait(1, SC_US);
        }
        CHECK(!ctl.phy_adapter().reset_pulse_active());
        CHECK(rd(FX1_CSI2_CORE_STATUS) & kRip);  // still waiting for the old write
        CHECK(rd(FX1_CSI2_DMA_STATUS) & kBusy);
        CHECK_EQ(rd(FX1_CSI2_DMA_CONTROL), 0x10u);  // registers at reset
        const sc_time t_rel = sc_core::sc_time_stamp();
        release();
        wait(200, SC_NS);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & kRip, 0u);
        CHECK_EQ(own(), 0u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 0u);
        CHECK_EQ(writes_after(t_rel), 0u);
        setup();  // the configuration is gone: program again
        recover_frame(kB0, 0x1);
    }

    // M16 / M07: dma_abort 0 -> 1 with a write outstanding, both policies;
    // M10: while dma_abort = 1 a starting capture is dropped.
    void abort_delayed(bool release_on_error) {
        setup(release_on_error);
        hold_nth = 3;
        tpg_frame();
        CHECK(wait_held());
        wr(FX1_CSI2_DMA_CONTROL, rd(FX1_CSI2_DMA_CONTROL) | FX1_CSI2_DMA_CONTROL_DMA_ABORT_MASK);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & kRip);  // disable drain (M17)
        CHECK(rd(FX1_CSI2_DMA_STATUS) & kBusy);
        wait(2, SC_US);
        CHECK_EQ(own(), 0u);                       // not finalised before the response
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (irqbit(IRQ_BUF_DONE_ERR) | irqbit(IRQ_FRAME_DROPPED)), 0u);
        const sc_time t_rel = sc_core::sc_time_stamp();
        release();
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & kRip, 0u);  // falls with the finalisation
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS) & kBusy, 0u);
        CHECK_EQ(writes_after(t_rel), 0u);
        fx1_csi2_buf_info bi{};
        CHECK(fx1_csi2_get_buffer(&dev, 0, &bi) == FX1_CSI2_OK);
        if (release_on_error) {
            CHECK(bi.state == FX1_CSI2_BUF_SW_OWNED_ERROR);
            CHECK_EQ(bi.last_valid_line, 2u);  // lines 0, 1 and the held line 2 answered OKAY (M08)
            CHECK_EQ(bi.frame_id, 1u);
            CHECK(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_BUF_DONE_ERR));
            CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);  // delivered with error (D06, M19: DMA cause)
            CHECK(side_holds(kB0, 3));
        } else {
            CHECK(bi.state == FX1_CSI2_BUF_HW_OWNED);
            CHECK(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_FRAME_DROPPED));
            CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
            CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 0u);
        }
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_BUF_DONE_OK), 0u);  // never valid after an abort
        // dma_abort still 1: the next capture is dropped, nothing written.
        const size_t w0 = ddr.log.size();
        const uint32_t d0 = rd(FX1_CSI2_FRAME_DROP_COUNT);
        tpg_frame();
        wait(3, SC_US);
        CHECK_EQ(ddr.log.size(), w0);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), d0 + 1);
        // Driver abort (clears dma_abort) and restart.
        CHECK(fx1_csi2_dma_abort(&dev, 100) == FX1_CSI2_OK);
        CHECK_EQ(rd(FX1_CSI2_DMA_CONTROL) & 0x7u, 0u);
        CHECK(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK);
        if (release_on_error) {
            CHECK(fx1_csi2_buffer_release(&dev, 0) == FX1_CSI2_OK);
            recover_frame(kB1, 0x1u << 2);  // side after the last finalised one
        } else {
            recover_frame(kB1, 0x1u << 2);
        }
    }

    // dma_enable 1 -> 0 with a write outstanding: abort per policy (M16).
    void disable_delayed() {
        setup(true);
        hold_nth = 2;
        tpg_frame();
        CHECK(wait_held());
        wr(FX1_CSI2_DMA_CONTROL, rd(FX1_CSI2_DMA_CONTROL) & ~FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & kRip);
        release();
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & kRip, 0u);
        CHECK_EQ(own(), 0x2u);  // SW_OWNED_ERROR
        CHECK_EQ(rd(FX1_CSI2_BUFFER_ERROR_INFO) & 0xFFFFu, 1u);
        // The capture is still open at the Frame Context (the TPG frame runs on);
        // it is counted at its end, delivered with error (DMA cause, M19).
        CHECK(poll_frame_count(1));
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);
        // A capture while dma_enable = 0: the DMA path does not take part (D06: delivered at its end).
        const size_t w0 = ddr.log.size();
        tpg_frame();
        wait(3, SC_US);
        CHECK_EQ(ddr.log.size(), w0);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 2u);
        CHECK(fx1_csi2_buffer_release(&dev, 0) == FX1_CSI2_OK);
        CHECK(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK);
        recover_frame(kB1, 0x1u << 2);
    }

    // core_enable 1 -> 0 with a write outstanding and the capture still open
    // (a 200-line TPG frame held by the back-pressure of the DMA): Frame Context
    // drops it, counted dropped (M19) although its buffer is released with error.
    void core_disable_open() {
        setup(true);
        CHECK(fx1_csi2_dma_stop(&dev, 100) == FX1_CSI2_OK);
        fx1_csi2_dma_cfg c = dma_cfg(true);
        c.size = 0x10000;
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_OK);
        CHECK(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK);
        wr(FX1_CSI2_TPG_RESOLUTION, (200u << 16) | kW);
        hold_nth = 3;
        tpg_frame();
        CHECK(wait_held());
        wait(5, SC_US);  // the generator is held by the full DMA FIFO, the frame is open
        CHECK(ctl.sys_datapath().frame_context().slot_open(0) || ctl.sys_datapath().frame_context().slot_open(1));
        wr(FX1_CSI2_CORE_CONFIG, 0);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & kRip);
        release();
        wait(2, SC_US);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & kRip, 0u);
        CHECK_EQ(own(), 0x2u);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_ERROR_INFO) & 0xFFFFu, 2u);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_BUF_DONE_ERR));
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 0u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_FRAME_RECEIVED), 0u);
        wait(20, SC_US);  // the generator finishes its frame (discarded) while core_enable = 0
        wr(FX1_CSI2_CORE_CONFIG, 1);
        wr(FX1_CSI2_TPG_RESOLUTION, (kH << 16) | kW);
        CHECK(fx1_csi2_buffer_release(&dev, 0) == FX1_CSI2_OK);
        recover_frame(kB1, 0x1u << 2);
    }

    // The same after the capture's Frame End: Control FSM did not drop it; the
    // DMA abort (core_enable 1 -> 0, M16) releases it with error and it counts
    // as delivered (M19, D06).
    void core_disable_after_fe() {
        setup(true);
        hold_nth = 3;
        tpg_frame();
        CHECK(wait_held());
        wait(2, SC_US);  // the TPG frame has ended at the Frame Context
        wr(FX1_CSI2_CORE_CONFIG, 0);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & kRip);
        release();
        wait(200, SC_NS);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & kRip, 0u);
        CHECK_EQ(own(), 0x2u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 0u);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_BUF_DONE_ERR));
        wr(FX1_CSI2_CORE_CONFIG, 1);
        CHECK(fx1_csi2_buffer_release(&dev, 0) == FX1_CSI2_OK);
        recover_frame(kB1, 0x1u << 2);
    }

    // AXI write error on the n-th write (line n-1): fault trace, counters,
    // IRQ 18, no further write of the capture, policy outcome; recovery.
    void axi_error(unsigned nth, tlm::tlm_response_status st, bool release_on_error, unsigned partial = 0) {
        setup(release_on_error);
        fail_nth = nth;
        fail_partial = partial;
        fail_status = st;
        tpg_frame();
        wait(3, SC_US);
        const bool decerr = st == tlm::TLM_ADDRESS_ERROR_RESPONSE;
        fx1_csi2_dma_fault f{};
        CHECK(fx1_csi2_get_dma_fault(&dev, &f) == FX1_CSI2_OK);
        CHECK_EQ(f.awid, 0u);
        CHECK_EQ(f.resp, decerr ? 3u : 2u);
        CHECK_EQ(f.addr, kB0 + (nth - 1) * kStride);
        CHECK_EQ(f.slverr_count, decerr ? 0u : 1u);
        CHECK_EQ(f.decerr_count, decerr ? 1u : 0u);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_AXI_WRITE_ERROR));
        CHECK_EQ(ddr.log.size(), nth);  // nothing of the capture after the failing write (M16)
        fx1_csi2_buf_info bi{};
        CHECK(fx1_csi2_get_buffer(&dev, 0, &bi) == FX1_CSI2_OK);
        if (release_on_error) {
            CHECK(bi.state == FX1_CSI2_BUF_SW_OWNED_ERROR);
            CHECK_EQ(bi.last_valid_line, nth == 1 ? 0xFFFFu : nth - 2);  // the failing line does not count
            CHECK(side_holds(kB0, nth - 1));  // lines before it are exact; its own bytes are undefined (M24)
            // Nothing beyond the failing burst: its line's padding and every later line untouched.
            CHECK(ddr.untouched(kB0 + (nth - 1) * kStride + kBpl, kSize - (nth - 1) * kStride - kBpl));
            CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 1u);
            CHECK(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_BUF_DONE_ERR));
            CHECK(fx1_csi2_buffer_release(&dev, 0) == FX1_CSI2_OK);
        } else {
            CHECK(bi.state == FX1_CSI2_BUF_HW_OWNED);
            CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        }
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_BUF_DONE_OK), 0u);
        fail_nth = 0;
        recover_frame(kB1, 0x1u << 2);
    }

    // M12: completion waits for the last response, beyond the Frame End.
    void late_response() {
        setup();
        hold_nth = kH;  // the last line's write
        tpg_frame();
        CHECK(wait_held());
        wait(3, SC_US);  // the capture ended long ago at the Frame Context
        CHECK_EQ(own(), 0u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 0u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (irqbit(IRQ_BUF_DONE_OK) | irqbit(IRQ_FRAME_RECEIVED)), 0u);
        CHECK(rd(FX1_CSI2_DMA_STATUS) & kBusy);
        release();
        // Same timestamp as the response: ownership, FRAME_ID, then both events.
        CHECK_EQ(own(), 0x1u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_ID) & 0x3FFFu, 1u);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_BUF_DONE_OK));
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & irqbit(IRQ_FRAME_RECEIVED));
        CHECK(irq.read());
        CHECK(side_holds(kB0, kH));
        CHECK(!ddr.log.empty() && ddr.log.back().end <= sc_core::sc_time_stamp());
    }

    // Driver API rules: argument checks, ESTATE while enabled or busy (M09),
    // graceful stop waiting for the capture, stop timeout then abort.
    void driver_api() {
        setup();
        fx1_csi2_dma_cfg c = dma_cfg(true);
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_ESTATE);  // enabled
        CHECK(fx1_csi2_dma_stop(&dev, 100) == FX1_CSI2_OK);       // idle: immediate
        CHECK_EQ(rd(FX1_CSI2_DMA_CONTROL) & 0x3u, 0u);
        auto bad = [&](std::function<void(fx1_csi2_dma_cfg&)> m) {
            fx1_csi2_dma_cfg b = dma_cfg(true);
            m(b);
            return fx1_csi2_dma_config(&dev, &b);
        };
        CHECK(bad([](auto& b) { b.base[0] += 4; }) == FX1_CSI2_EINVAL);
        CHECK(bad([](auto& b) { b.base[1] = 0xFFFFF008u; }) == FX1_CSI2_EINVAL);  // ends above 4 GiB
        CHECK(bad([](auto& b) { b.line_padding = 4; }) == FX1_CSI2_EINVAL);
        CHECK(bad([](auto& b) { b.bytes_per_line = 0; }) == FX1_CSI2_EINVAL);
        CHECK(bad([](auto& b) { b.size = 8; }) == FX1_CSI2_EINVAL);
        CHECK(fx1_csi2_dma_config(&dev, nullptr) == FX1_CSI2_EINVAL);
        CHECK(fx1_csi2_buffer_release(&dev, 0) == FX1_CSI2_ESTATE);  // HW_OWNED: no effect (M11)
        CHECK(fx1_csi2_buffer_release(&dev, 2) == FX1_CSI2_EINVAL);
        // Stream format: refused while the stream path is enabled.
        CHECK(fx1_csi2_stream_config(&dev, 1, 0) == FX1_CSI2_OK);
        CHECK(fx1_csi2_stream_config(&dev, 1, 1) == FX1_CSI2_ESTATE);
        CHECK(fx1_csi2_stream_config(&dev, 0, 0) == FX1_CSI2_OK);
        CHECK(fx1_csi2_stream_config(&dev, 0, 1) == FX1_CSI2_OK);
        CHECK_EQ(rd(FX1_CSI2_STREAM_CONFIG), 0u);  // disabled, RAW16 (workbook: format 0)
        CHECK(fx1_csi2_stream_config(&dev, 0, 0) == FX1_CSI2_OK);
        // Review M4b F2: from disabled + packed, enable with RAW16 in one call:
        // the format is written while disabled, then the enable: no warning.
        const int w0 = sc_core::sc_report_handler::get_count("fx1/csi2/ctrl");
        CHECK(fx1_csi2_stream_config(&dev, 1, 1) == FX1_CSI2_OK);
        CHECK_EQ(sc_core::sc_report_handler::get_count("fx1/csi2/ctrl"), w0);
        CHECK_EQ(rd(FX1_CSI2_STREAM_CONFIG), FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK);
        CHECK(fx1_csi2_stream_config(&dev, 1, 0) == FX1_CSI2_ESTATE);  // enabled: refused, nothing written
        CHECK_EQ(rd(FX1_CSI2_STREAM_CONFIG), FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK);
        CHECK_EQ(sc_core::sc_report_handler::get_count("fx1/csi2/ctrl"), w0);
        CHECK(fx1_csi2_stream_config(&dev, 0, 1) == FX1_CSI2_OK);
        CHECK(fx1_csi2_stream_config(&dev, 0, 0) == FX1_CSI2_OK);
        CHECK_EQ(sc_core::sc_report_handler::get_count("fx1/csi2/ctrl"), w0);
        // Graceful stop with a write held: waits; the capture completes valid.
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_OK);
        CHECK(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK);
        hold_nth = 4;
        tpg_frame();
        CHECK(wait_held());
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_ESTATE);  // busy
        int rc = 1;
        sc_core::sc_spawn([&] { rc = fx1_csi2_dma_stop(&dev, 1000); });
        wait(5, SC_US);
        CHECK_EQ(rc, 1);  // still waiting
        CHECK_EQ(rd(FX1_CSI2_DMA_CONTROL) & FX1_CSI2_DMA_CONTROL_DMA_START_MASK, 0u);
        release();
        wait(20, SC_US);
        CHECK_EQ(rc, FX1_CSI2_OK);
        CHECK_EQ(own(), 0x1u);
        CHECK(side_holds(kB0, kH));
        CHECK_EQ(rd(FX1_CSI2_DMA_CONTROL) & 0x3u, 0u);
        // Stop timeout while a write is held, then abort after the response.
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_OK);
        CHECK(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK);
        writes = 0;
        hold_nth = 2;
        tpg_frame();
        CHECK(wait_held());
        CHECK(fx1_csi2_dma_stop(&dev, 30) == FX1_CSI2_ETIMEDOUT);
        CHECK(rd(FX1_CSI2_DMA_CONTROL) & FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK);  // left enabled
        // dma_enable cleared with the write still held: busy without enable,
        // the configuration is still refused (M09).
        wr(FX1_CSI2_DMA_CONTROL, rd(FX1_CSI2_DMA_CONTROL) & ~FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK);
        CHECK(rd(FX1_CSI2_DMA_STATUS) & kBusy);
        CHECK(fx1_csi2_dma_config(&dev, &c) == FX1_CSI2_ESTATE);
        rc = 1;
        sc_core::sc_spawn([&] { rc = fx1_csi2_dma_abort(&dev, 1000); });
        wait(5, SC_US);
        CHECK_EQ(rc, 1);
        release();
        wait(20, SC_US);
        CHECK_EQ(rc, FX1_CSI2_OK);
        CHECK_EQ(own(), (0x2u << 2) | 0x1u);  // side 1 released with error, side 0 still valid
        CHECK_EQ(rd(FX1_CSI2_DMA_CONTROL) & 0x7u, 0u);
    }

    void run() {
        const std::map<std::string, std::function<void()>> cases = {
            {"soft_reset_delayed", [this] { soft_reset_delayed(); }},
            {"full_reset_delayed", [this] { reset_delayed(false); }},
            {"resetn_delayed", [this] { reset_delayed(true); }},
            {"abort_delayed_p0", [this] { abort_delayed(false); }},
            {"abort_delayed_p1", [this] { abort_delayed(true); }},
            {"disable_delayed", [this] { disable_delayed(); }},
            {"core_disable_open", [this] { core_disable_open(); }},
            {"core_disable_after_fe", [this] { core_disable_after_fe(); }},
            {"slverr_first", [this] { axi_error(1, tlm::TLM_GENERIC_ERROR_RESPONSE, true); }},
            {"slverr_mid", [this] { axi_error(4, tlm::TLM_GENERIC_ERROR_RESPONSE, true); }},
            {"slverr_last", [this] { axi_error(kH, tlm::TLM_GENERIC_ERROR_RESPONSE, true); }},
            {"decerr_mid_p0", [this] { axi_error(4, tlm::TLM_ADDRESS_ERROR_RESPONSE, false); }},
            {"slverr_partial", [this] { axi_error(4, tlm::TLM_GENERIC_ERROR_RESPONSE, true, 20); }},
            {"late_response", [this] { late_response(); }},
            {"driver_api", [this] { driver_api(); }},
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
    const std::string which = argc > 1 ? argv[1] : "soft_reset_delayed";
    Tb tb("tb", which);
    sc_core::sc_start(20, sc_core::SC_MS);
    if (sc_core::sc_get_status() != sc_core::SC_STOPPED) {
        std::printf("case did not finish\n");
        ++g_fail;
    }
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
