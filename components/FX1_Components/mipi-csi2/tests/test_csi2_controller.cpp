// M2 tests of the CSI-2 RX controller: CSR contract, reset scopes, interrupt
// aggregation and the PHY adapter, with the detailed D-PHY model attached.
//
//   test_fx1_csi2_controller <case>
//
// Oracle: tests/data/csr_v1_4_table.inc, generated from the v1.4 workbook by
// tools/csr_workbook.py (independent of the model, whose access semantics and
// reset values are written by hand) and the decision IDs of
// docs/spec_decisions.md. Datapath events are injected through the hw_*
// interface (unit-test only); PHY status and IRQ 9-11 come from the PHY.
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
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_bfm/csi2_packet.h"
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

enum class Acc { RO, RW, WO, RW1C, RSVD };
struct Field {
    uint32_t off;
    const char* reg;
    const char* field;
    int hi, lo;
    Acc acc;
    long long reset;
    int row;
};
const Field kFields[] = {
#include "data/csr_v1_4_table.inc"
};

constexpr unsigned kMaxLanes = 4;
constexpr uint32_t kMaxLanesEnc = 3;  // 11 = 4 lanes

uint32_t fmask(const Field& f) { return static_cast<uint32_t>(((1ull << (f.hi - f.lo + 1)) - 1) << f.lo); }

// Fields that mirror a live PHY output and therefore read the PHY's value
// after reset rather than the workbook reset value (deviation P11):
// cl_ulpsclknot is active low and the model drives 1 (not in ULPS) also in shutdown.
uint32_t live_phy_mask(uint32_t off) {
    return off == FX1_CSI2_PHY_STATUS ? FX1_CSI2_PHY_STATUS_CL_ULPSCLKNOT_MASK : 0u;
}

// Workbook reset value of a register (build-parameter fields at their built value).
uint32_t wb_reset(uint32_t off) {
    uint32_t v = 0;
    for (const auto& f : kFields)
        if (f.off == off) {
            const uint32_t r = f.reset < 0 ? kMaxLanesEnc : static_cast<uint32_t>(f.reset);
            v |= (r << f.lo) & fmask(f);
        }
    return v;
}
uint32_t wb_mask(uint32_t off, Acc acc) {
    uint32_t m = 0;
    for (const auto& f : kFields)
        if (f.off == off && f.acc == acc) m |= fmask(f);
    return m;
}
std::vector<uint32_t> wb_offsets() {
    std::vector<uint32_t> v;
    for (const auto& f : kFields)
        if (v.empty() || v.back() != f.off) v.push_back(f.off);
    return v;
}

struct Tb : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Tb> socket{"socket"};
    sc_core::sc_signal<bool> resetn{"resetn", false};
    sc_core::sc_signal<bool> irq{"irq"};
    DphyRxWrapper phy{"phy_model"};
    Csi2RxController ctl;
    bfm::DphyTxBfm tx{"tx", phy.clkp, phy.clkn, phy.dp, phy.dn};
    std::string which;

    SC_HAS_PROCESS(Tb);
    Tb(sc_core::sc_module_name nm, std::string w) : sc_core::sc_module(nm), ctl("csi2", phy), which(std::move(w)) {
        socket.bind(ctl.csr_socket);
        ctl.resetn(resetn);
        ctl.irq(irq);
        SC_THREAD(run);
    }

    tlm::tlm_response_status access(tlm::tlm_command cmd, uint64_t addr, unsigned char* data, unsigned len,
                                    const std::vector<unsigned char>& be = {}) {
        tlm::tlm_generic_payload p;
        sc_time d = sc_core::SC_ZERO_TIME;
        p.set_command(cmd);
        p.set_address(addr);
        p.set_data_ptr(data);
        p.set_data_length(len);
        p.set_streaming_width(len);
        std::vector<unsigned char> b = be;
        if (!b.empty()) {
            p.set_byte_enable_ptr(b.data());
            p.set_byte_enable_length(static_cast<unsigned>(b.size()));
        }
        p.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(p, d);
        if (d != sc_core::SC_ZERO_TIME) wait(d);  // consume the access latency the target returned
        return p.get_response_status();
    }
    uint32_t rd(uint32_t off) {
        unsigned char b[4] = {};
        CHECK(access(tlm::TLM_READ_COMMAND, off, b, 4) == tlm::TLM_OK_RESPONSE);
        return b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
    }
    void wr(uint32_t off, uint32_t v) {
        unsigned char b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
        CHECK(access(tlm::TLM_WRITE_COMMAND, off, b, 4) == tlm::TLM_OK_RESPONSE);
    }
    uint32_t dbg(uint32_t off) {
        unsigned char b[4] = {};
        tlm::tlm_generic_payload p;
        p.set_command(tlm::TLM_READ_COMMAND);
        p.set_address(off);
        p.set_data_ptr(b);
        p.set_data_length(4);
        CHECK(socket->transport_dbg(p) == 4);
        return b[0] | b[1] << 8 | b[2] << 16 | uint32_t(b[3]) << 24;
    }
    bool poll(uint32_t off, uint32_t mask, uint32_t want, sc_time step, unsigned max_steps) {
        for (unsigned i = 0; i < max_steps; ++i) {
            if ((rd(off) & mask) == want) return true;
            wait(step);
        }
        return (rd(off) & mask) == want;
    }

    void release_reset() {
        resetn.write(true);
        wait(1, SC_US);  // power-up PHY reset pulse (200 ns) is over
    }

    // ------------------------------------------------------------------ cases
    void csr_reset() {
        release_reset();
        for (uint32_t off : wb_offsets()) {
            const uint32_t want = wb_reset(off), live = live_phy_mask(off);
            CHECK_EQ(rd(off) & ~live, want & ~live);
            CHECK_EQ(dbg(off) & ~live, want & ~live);
        }
        // P11: the PHY in shutdown reports "not in ULPS" on its active-low output.
        CHECK_EQ(rd(FX1_CSI2_PHY_STATUS), FX1_CSI2_PHY_STATUS_CL_ULPSCLKNOT_MASK);
    }

    void csr_access() {
        release_reset();
        // CORE_CONFIG is checked in its own cases (soft/full reset side effects).
        for (uint32_t off : wb_offsets()) {
            if (off == FX1_CSI2_CORE_CONFIG) continue;
            const uint32_t rw = wb_mask(off, Acc::RW), ro = wb_mask(off, Acc::RO);
            const uint32_t before = rd(off);
            wr(off, 0xFFFFFFFFu);
            uint32_t got = rd(off);
            // RW bits take the value; RO keep their value; WO, RW1C (no event) and reserved read 0.
            CHECK_EQ(got & rw, rw);
            CHECK_EQ(got & ro, before & ro);
            CHECK_EQ(got & ~(rw | ro), 0u);
            wr(off, 0);
            got = rd(off);
            CHECK_EQ(got & rw, 0u);
            CHECK_EQ(got & ro, before & ro);
        }
    }

    void csr_partial() {
        release_reset();
        const uint32_t r = FX1_CSI2_EXPECTED_RESOLUTION;
        wr(r, 0x11223344);
        unsigned char b1 = 0xAA;
        CHECK(access(tlm::TLM_WRITE_COMMAND, r + 2, &b1, 1) == tlm::TLM_OK_RESPONSE);
        CHECK_EQ(rd(r), 0x11AA3344u);
        unsigned char h[2] = {0x55, 0x66};
        CHECK(access(tlm::TLM_WRITE_COMMAND, r, h, 2) == tlm::TLM_OK_RESPONSE);
        CHECK_EQ(rd(r), 0x11AA6655u);
        unsigned char w[4] = {1, 2, 3, 4};
        CHECK(access(tlm::TLM_WRITE_COMMAND, r, w, 4, {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED, TLM_BYTE_ENABLED, TLM_BYTE_DISABLED}) ==
              tlm::TLM_OK_RESPONSE);
        CHECK_EQ(rd(r), 0x11036601u);
        unsigned char rb[2] = {};
        CHECK(access(tlm::TLM_READ_COMMAND, r + 1, rb, 2) == tlm::TLM_OK_RESPONSE);
        CHECK(rb[0] == 0x66 && rb[1] == 0x03);
        // Crossing a word boundary, or longer than a word: SLVERR, no effect (C04).
        CHECK(access(tlm::TLM_WRITE_COMMAND, r + 2, w, 4) == tlm::TLM_GENERIC_ERROR_RESPONSE);
        unsigned char w8[8] = {};
        CHECK(access(tlm::TLM_WRITE_COMMAND, r, w8, 8) == tlm::TLM_GENERIC_ERROR_RESPONSE);
        CHECK_EQ(rd(r), 0x11036601u);
        // Length 3 is outside {1, 2, 4}: SLVERR before any side effect (review M2 F2).
        unsigned char w3[3] = {0xEE, 0xEE, 0xEE};
        for (uint32_t lane : {0u, 1u}) {
            CHECK(access(tlm::TLM_WRITE_COMMAND, r + lane, w3, 3) == tlm::TLM_GENERIC_ERROR_RESPONSE);
            CHECK(access(tlm::TLM_READ_COMMAND, r + lane, w3, 3) == tlm::TLM_GENERIC_ERROR_RESPONSE);
        }
        CHECK_EQ(rd(r), 0x11036601u);
        CHECK(ctl.hw_gsp_push(1, 0x09, 0x1234));
        const uint32_t gsp_entry = dbg(FX1_CSI2_GENERIC_SHORT_PACKET);
        unsigned char r3[3] = {};
        for (uint32_t lane : {0u, 1u})
            CHECK(access(tlm::TLM_READ_COMMAND, FX1_CSI2_GENERIC_SHORT_PACKET + lane, r3, 3) == tlm::TLM_GENERIC_ERROR_RESPONSE);
        CHECK_EQ(dbg(FX1_CSI2_GENERIC_SHORT_PACKET), gsp_entry);  // not popped
        CHECK(rd(FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_SP_FIFO_NOT_EMPTY_MASK);
        unsigned char d3[3] = {};
        tlm::tlm_generic_payload p;  // debug transport: size 3 refused too
        p.set_command(tlm::TLM_READ_COMMAND);
        p.set_address(FX1_CSI2_GENERIC_SHORT_PACKET);
        p.set_data_ptr(d3);
        p.set_data_length(3);
        CHECK(socket->transport_dbg(p) == 0);
        // TIMESTAMP_LO snapshot is a side effect: a refused size-3 read must not
        // take it. A latched value with a non-zero high half makes a wrong
        // snapshot visible.
        ctl.hw_latch_timestamp_ns(0x1'0000'0005ull);
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_HI), 0u);  // shadow not taken yet
        CHECK(access(tlm::TLM_READ_COMMAND, FX1_CSI2_TIMESTAMP_LO, r3, 3) == tlm::TLM_GENERIC_ERROR_RESPONSE);
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_HI), 0u);  // still not taken
        (void)rd(FX1_CSI2_TIMESTAMP_LO);
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_HI), 1u);  // a valid LO read takes it
    }

    void csr_reserved() {
        release_reset();
        ctl.hw_raise_irq(IRQ_FRAME_RECEIVED);
        for (uint32_t off : {0x1Cu, 0x3Cu, 0x5Cu, 0x8Cu, 0xB8u, 0xBCu, 0xDCu, 0xF8u, 0xFCu}) {
            wr(off, 0xFFFFFFFFu);
            CHECK_EQ(rd(off), 0u);
        }
        // 0x100..0xFFF: DECERR without side effect; no aliasing onto 0x44.
        unsigned char ones[4] = {0xFF, 0xFF, 0xFF, 0xFF}, rb[4] = {};
        for (uint64_t a : {0x100ull, 0x144ull, 0xFFCull, 0x1000ull, 0x1044ull}) {
            CHECK(access(tlm::TLM_WRITE_COMMAND, a, ones, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
            CHECK(access(tlm::TLM_READ_COMMAND, a, rb, 4) == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 1u << IRQ_FRAME_RECEIVED);
    }

    void irq_case() {
        release_reset();
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);  // flags of the power-up sequence
        ctl.hw_raise_irq(IRQ_CRC_ERROR);
        wait(1, SC_NS);  // irq pin: method + signal update; later clears are at a later timestamp
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 1u << IRQ_CRC_ERROR);  // set whatever the masks (C08)
        CHECK(!irq.read());
        wr(FX1_CSI2_IRQ_ENABLE, 1u << IRQ_CRC_ERROR);
        wait(1, SC_NS);
        CHECK(!irq.read());  // global enable still 0
        wr(FX1_CSI2_GLOBAL_IRQ_ENABLE, 1);
        wait(1, SC_NS);
        CHECK(irq.read());
        wr(FX1_CSI2_IRQ_STATUS, 0);  // writing 0 keeps the flag
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 1u << IRQ_CRC_ERROR);
        wr(FX1_CSI2_IRQ_STATUS, 1u << IRQ_CRC_ERROR);
        wait(1, SC_NS);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 0u);
        CHECK(!irq.read());
        // Byte strobes: only the enabled lane is cleared.
        ctl.hw_raise_irq(IRQ_ECC_1BIT);
        ctl.hw_raise_irq(IRQ_BUF_DONE_OK);
        wait(10, SC_NS);
        unsigned char w[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        CHECK(access(tlm::TLM_WRITE_COMMAND, FX1_CSI2_IRQ_STATUS, w, 4,
                     {TLM_BYTE_ENABLED, TLM_BYTE_DISABLED, TLM_BYTE_DISABLED, TLM_BYTE_DISABLED}) == tlm::TLM_OK_RESPONSE);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 1u << IRQ_BUF_DONE_OK);
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        // W1C race (C02): an event at the same timestamp keeps the flag, before or after the write.
        wait(10, SC_NS);
        ctl.hw_raise_irq(IRQ_FRAME_DROPPED);
        wr(FX1_CSI2_IRQ_STATUS, 1u << IRQ_FRAME_DROPPED);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 1u << IRQ_FRAME_DROPPED);
        wait(10, SC_NS);
        wr(FX1_CSI2_IRQ_STATUS, 1u << IRQ_FRAME_DROPPED);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 0u);
        // Stale epoch (M13) and suppression during soft_reset (C08).
        const uint32_t old_epoch = ctl.epoch();
        wr(FX1_CSI2_CORE_CONFIG, 0x3);  // core_enable | soft_reset
        ctl.hw_raise_irq(IRQ_WC_CORRUPTION);
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        ctl.hw_raise_irq(IRQ_WC_CORRUPTION, old_epoch);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 0u);
        ctl.hw_raise_irq(IRQ_WC_CORRUPTION);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 1u << IRQ_WC_CORRUPTION);
    }

    void counters() {
        release_reset();
        ctl.hw_count(Counter::Ecc1, 0xFFFE);
        ctl.hw_count(Counter::Ecc1, 5);
        ctl.hw_count(Counter::Ecc2, 3);
        CHECK_EQ(rd(FX1_CSI2_ECC_ERROR_COUNT), 0x0003FFFFu);
        ctl.hw_count(Counter::Crc, 0xFFFFFFF0u);
        ctl.hw_count(Counter::Crc, 0x100);
        CHECK_EQ(rd(FX1_CSI2_CRC_ERROR_COUNT), 0xFFFFFFFFu);
        ctl.hw_count(Counter::LaneAbort, 0x20000);
        CHECK_EQ(rd(FX1_CSI2_LANE_ABORT_COUNT), 0xFFFFu);
        ctl.hw_count(Counter::AxiDecerr, 2);
        ctl.hw_count(Counter::AxiSlverr, 1);
        CHECK_EQ(rd(FX1_CSI2_AXI_ERROR_COUNT), 0x00020001u);
        ctl.hw_count(Counter::Packet, 7);
        ctl.hw_count(Counter::Frame, 2);
        ctl.hw_count(Counter::FrameDrop, 1);
        CHECK_EQ(rd(FX1_CSI2_PACKET_COUNT), 7u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_COUNT), 2u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_DROP_COUNT), 1u);
        wr(FX1_CSI2_CORE_CONFIG, 0x3);
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        for (uint32_t off : {FX1_CSI2_ECC_ERROR_COUNT, FX1_CSI2_CRC_ERROR_COUNT, FX1_CSI2_LANE_ABORT_COUNT,
                             FX1_CSI2_AXI_ERROR_COUNT, FX1_CSI2_PACKET_COUNT, FX1_CSI2_FRAME_COUNT, FX1_CSI2_FRAME_DROP_COUNT})
            CHECK_EQ(rd(off), 0u);
    }

    void gsp() {
        release_reset();
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        const uint32_t ne = FX1_CSI2_CORE_STATUS_SP_FIFO_NOT_EMPTY_MASK, full = FX1_CSI2_CORE_STATUS_SP_FIFO_FULL_MASK;
        CHECK_EQ(rd(FX1_CSI2_GENERIC_SHORT_PACKET), 0u);  // empty read returns 0
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & (ne | full), 0u);
        for (unsigned i = 0; i < 8; ++i) CHECK(ctl.hw_gsp_push(i & 3, 0x08 + i, static_cast<uint16_t>(0x1000 + i)));
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & (ne | full), ne | full);
        CHECK(!ctl.hw_gsp_push(0, 0x0F, 0xDEAD));  // ninth entry lost
        CHECK(rd(FX1_CSI2_BUFFER_STATUS) & FX1_CSI2_BUFFER_STATUS_SP_FIFO_OVERFLOW_MASK);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_BUFFER_OVERFLOW));
        const uint32_t first = (0x1000u << 8) | (0u << 6) | 0x08u;
        CHECK_EQ(dbg(FX1_CSI2_GENERIC_SHORT_PACKET), first);  // debug peeks
        CHECK_EQ(dbg(FX1_CSI2_GENERIC_SHORT_PACKET), first);
        unsigned char b = 0;  // a one-byte bus read also pops (D12)
        CHECK(access(tlm::TLM_READ_COMMAND, FX1_CSI2_GENERIC_SHORT_PACKET, &b, 1) == tlm::TLM_OK_RESPONSE);
        CHECK_EQ(b, 0x08u);
        for (unsigned i = 1; i < 8; ++i)
            CHECK_EQ(rd(FX1_CSI2_GENERIC_SHORT_PACKET), ((0x1000u + i) << 8) | ((i & 3u) << 6) | (0x08u + i));
        CHECK_EQ(rd(FX1_CSI2_GENERIC_SHORT_PACKET), 0u);
        CHECK(ctl.hw_gsp_push(1, 0x09, 1));
        wr(FX1_CSI2_CORE_CONFIG, 0x3);
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & ne, 0u);  // flushed by soft_reset
        CHECK_EQ(rd(FX1_CSI2_BUFFER_STATUS) & FX1_CSI2_BUFFER_STATUS_SP_FIFO_OVERFLOW_MASK, 0u);
    }

    void timestamp() {
        release_reset();
        const sc_time t0 = sc_core::sc_time_stamp();  // counter origin = reset release
        wait(1234, SC_NS);
        ctl.hw_latch_timestamp();
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_HI), 0u);  // no LO read yet: last shadow
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_LO), 1234u + 1000u);  // release_reset waited 1 us after release
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_HI), 0u);
        (void)t0;
        wait(10, SC_NS);
        wr(FX1_CSI2_CORE_CONFIG, 0x3);
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_LO), 0u);  // latched context cleared (C10)
        ctl.hw_latch_timestamp();
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_LO), 2244u);  // counter survives soft_reset (C07)
        wr(FX1_CSI2_CORE_CONFIG, FX1_CSI2_CORE_CONFIG_FULL_RESET_MASK);
        wait(1, SC_US);
        ctl.hw_latch_timestamp();
        CHECK_EQ(rd(FX1_CSI2_TIMESTAMP_LO), 1000u);  // full_reset restarts the counter
    }

    void soft_reset_scope() {
        release_reset();
        wr(FX1_CSI2_EXPECTED_RESOLUTION, 0x04380780);
        wr(FX1_CSI2_DMA_BUFFER0_ADDR, 0x80000000);
        wr(FX1_CSI2_IRQ_ENABLE, 0x00FFFFFF);
        ctl.hw_set_ro(FX1_CSI2_CORE_STATUS, FX1_CSI2_CORE_STATUS_NULL_BLANK_SEEN_MASK, ~0u);
        ctl.hw_set_ro(FX1_CSI2_VC0_IMAGE_INFO1, ~0u, 0x00100200);
        ctl.hw_set_ro(FX1_CSI2_ERROR_CAPTURE, ~0u, 0x40010005);
        ctl.hw_set_buffer(0, BufState::SwOwnedValid, 0x123);
        ctl.hw_set_buffer(1, BufState::SwOwnedError, 0x124);
        ctl.hw_set_ro(FX1_CSI2_BUFFER_ERROR_INFO, ~0u, 0x00070000);
        ctl.hw_raise_irq(IRQ_BUF_DONE_OK);
        wr(FX1_CSI2_CORE_CONFIG, 0x3);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK);
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS), 0u);
        CHECK_EQ(rd(FX1_CSI2_VC0_IMAGE_INFO1), 0u);
        CHECK_EQ(rd(FX1_CSI2_ERROR_CAPTURE), 0u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 0u);
        // Kept: configuration, ownership (M15), FRAME_ID, BUFFER_ERROR_INFO.
        CHECK_EQ(rd(FX1_CSI2_EXPECTED_RESOLUTION), 0x04380780u);
        CHECK_EQ(rd(FX1_CSI2_DMA_BUFFER0_ADDR), 0x80000000u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_ENABLE), 0x00FFFFFFu);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0x9u);  // buf1 = 10, buf0 = 01
        CHECK_EQ(rd(FX1_CSI2_FRAME_ID), (0x124u << 16) | 0x123u);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_ERROR_INFO), 0x00070000u);
    }

    void reset_drain() {
        release_reset();
        const uint32_t rip = FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK, busy = FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK;
        // F1: soft_reset pulse while a write of the capture is outstanding.
        ctl.hw_set_capture_in_flight(true);
        CHECK(rd(FX1_CSI2_DMA_STATUS) & busy);
        wr(FX1_CSI2_CORE_CONFIG, 0x3);
        wait(50, SC_NS);
        wr(FX1_CSI2_CORE_CONFIG, 0x1);  // FW lowers soft_reset before the response returns
        CHECK(ctl.reset_drain_pending());
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);
        CHECK(rd(FX1_CSI2_DMA_STATUS) & busy);
        wait(500, SC_NS);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);  // still draining
        ctl.hw_set_capture_in_flight(false);
        ctl.hw_drain_complete();
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & rip, 0u);
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS) & busy, 0u);
        // Disable drain (M16/M17): core_enable 1 -> 0 with a capture in flight;
        // the abort's finalisation event is recorded (not suppressed).
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        ctl.hw_set_capture_in_flight(true);
        wr(FX1_CSI2_CORE_CONFIG, 0x0);
        CHECK(ctl.disable_drain_pending());
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);
        wait(300, SC_NS);
        ctl.hw_raise_irq(IRQ_BUF_DONE_ERR);
        ctl.hw_set_capture_in_flight(false);
        ctl.hw_drain_complete();
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & rip, 0u);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 1u << IRQ_BUF_DONE_ERR);
        // No capture in flight: a disable cause leaves nothing pending.
        wr(FX1_CSI2_DMA_CONTROL, FX1_CSI2_DMA_CONTROL_DMA_ABORT_MASK);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & rip, 0u);
    }

    // Review M2 F1: resetn must not drop the obligation to wait for writes
    // already issued (M13); reset_in_progress / dma_busy stay 1 after the reset
    // pin and the PHY pulse are released, until the DMA reports completion.
    void reset_drain_resetn() {
        release_reset();
        const uint32_t rip = FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK, busy = FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK;
        auto pulse_resetn = [this] {
            resetn.write(false);
            wait(1, SC_US);
            CHECK_EQ(rd(FX1_CSI2_CORE_STATUS), 0u);  // C15: register file held in reset, reads 0
            resetn.write(true);
            wait(1, SC_US);  // the power-up PHY pulse (200 ns) is over
        };
        // (a) capture in flight, resetn, release before the old response.
        ctl.hw_set_capture_in_flight(true);
        uint32_t e0 = ctl.epoch();
        pulse_resetn();
        CHECK(ctl.epoch() != e0);
        CHECK(ctl.reset_drain_pending());
        CHECK(!ctl.phy_adapter().reset_pulse_active());
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);
        CHECK(rd(FX1_CSI2_DMA_STATUS) & busy);
        ctl.hw_raise_irq(IRQ_BUF_DONE_OK, e0);  // completion of the old epoch: never published
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 0u);
        wait(5, SC_US);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);  // still waiting for the old write
        ctl.hw_set_capture_in_flight(false);
        ctl.hw_drain_complete();
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & rip, 0u);
        CHECK_EQ(rd(FX1_CSI2_DMA_STATUS) & busy, 0u);
        // (b) resetn during a soft-reset drain: the drain survives, epoch moves again.
        ctl.hw_set_capture_in_flight(true);
        wr(FX1_CSI2_CORE_CONFIG, 0x3);
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        e0 = ctl.epoch();
        pulse_resetn();
        CHECK(ctl.epoch() != e0);
        CHECK(ctl.reset_drain_pending());
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);
        ctl.hw_set_capture_in_flight(false);
        ctl.hw_drain_complete();
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & rip, 0u);
        // (c) resetn during a disable drain: becomes a reset drain, no abort event.
        ctl.hw_set_capture_in_flight(true);
        wr(FX1_CSI2_CORE_CONFIG, 0x0);
        CHECK(ctl.disable_drain_pending());
        e0 = ctl.epoch();
        pulse_resetn();
        CHECK(!ctl.disable_drain_pending());
        CHECK(ctl.reset_drain_pending());
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);
        ctl.hw_raise_irq(IRQ_BUF_DONE_ERR, e0);
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS), 0u);
        ctl.hw_set_capture_in_flight(false);
        ctl.hw_drain_complete();
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & rip, 0u);
        // (d) full_reset with a capture in flight: busy past the PHY pulse.
        ctl.hw_set_capture_in_flight(true);
        wr(FX1_CSI2_CORE_CONFIG, 0x1 | FX1_CSI2_CORE_CONFIG_FULL_RESET_MASK);
        wait(1, SC_US);
        CHECK(!ctl.phy_adapter().reset_pulse_active());
        CHECK(rd(FX1_CSI2_CORE_STATUS) & rip);
        CHECK(rd(FX1_CSI2_DMA_STATUS) & busy);
        ctl.hw_set_capture_in_flight(false);
        ctl.hw_drain_complete();
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS) & rip, 0u);
    }

    void full_reset() {
        release_reset();
        for (uint32_t off : {FX1_CSI2_EXPECTED_RESOLUTION, FX1_CSI2_IRQ_ENABLE, FX1_CSI2_DMA_ATTR, FX1_CSI2_LINE_PITCH})
            wr(off, 0x00FF00FF);
        wr(FX1_CSI2_PHY_CONFIG, 0x7D);
        ctl.hw_set_buffer(0, BufState::SwOwnedValid, 7);
        ctl.hw_count(Counter::Crc, 3);
        const sc_time t_req = sc_core::sc_time_stamp();
        wr(FX1_CSI2_CORE_CONFIG, 0x1 | FX1_CSI2_CORE_CONFIG_FULL_RESET_MASK);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK);  // PHY pulse
        CHECK(poll(FX1_CSI2_CORE_STATUS, FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK, 0, sc_time(10, SC_NS), 100));
        CHECK(sc_core::sc_time_stamp() - t_req >= sc_time(200, SC_NS));  // P04 minimum width
        for (uint32_t off : wb_offsets()) CHECK_EQ(rd(off) & ~live_phy_mask(off), wb_reset(off) & ~live_phy_mask(off));
    }

    void ownership() {
        release_reset();
        wr(FX1_CSI2_BUFFER_RELEASE, 0x3);  // both HW_OWNED: no effect
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0u);
        ctl.hw_set_buffer(0, BufState::SwOwnedValid, 1);
        ctl.hw_set_buffer(1, BufState::SwOwnedError, 2);
        wr(FX1_CSI2_BUFFER_RELEASE, 0x2);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0x1u);
        wr(FX1_CSI2_BUFFER_RELEASE, 0x1);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_OWNERSHIP), 0u);
        CHECK_EQ(rd(FX1_CSI2_BUFFER_RELEASE), 0u);  // WO reads 0
    }

    void hw_guard() {
        release_reset();
        const auto before = sc_core::sc_report_handler::get_count(sc_core::SC_ERROR);
        ctl.hw_set_ro(FX1_CSI2_CORE_STATUS, FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK, ~0u);  // computed, not owned
        ctl.hw_set_ro(FX1_CSI2_EXPECTED_RESOLUTION, 1, 1);                                     // RW register
        ctl.hw_set_ro(FX1_CSI2_FRAME_ID, 1, 1);                                                // via hw_set_buffer only
        CHECK_EQ(sc_core::sc_report_handler::get_count(sc_core::SC_ERROR) - before, 3);
        CHECK_EQ(rd(FX1_CSI2_CORE_STATUS), 0u);
        CHECK_EQ(rd(FX1_CSI2_EXPECTED_RESOLUTION), 0u);
        CHECK_EQ(rd(FX1_CSI2_FRAME_ID), 0u);
    }

    // PHY bring-up through the CSR (MAS initial sequence steps 2-6), real PHY.
    void phy_bringup() {
        release_reset();
        const uint32_t stop_all = 0x01010101u, cl_stop = FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK,
                       init = FX1_CSI2_PHY_STATUS_INIT_DONE_MASK;
        CHECK_EQ(rd(FX1_CSI2_PHY_STATUS) & (cl_stop | init), 0u);  // phy_enable = 0: PHY held in shutdown
        wr(FX1_CSI2_PHY_CONFIG, FX1_CSI2_PHY_CONFIG_DPHY_RST_REQ_MASK);
        CHECK(rd(FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK);
        CHECK(poll(FX1_CSI2_CORE_STATUS, FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK, 0, sc_time(50, SC_NS), 20));
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        const sc_time t_en = sc_core::sc_time_stamp();
        wr(FX1_CSI2_PHY_CONFIG, 0x7D);  // phy_enable, cl_enable, dl_enable = 0xF
        CHECK(poll(FX1_CSI2_LANE_STATUS, stop_all, stop_all, sc_time(10, SC_US), 60));
        CHECK(poll(FX1_CSI2_PHY_STATUS, cl_stop | init, cl_stop | init, sc_time(10, SC_US), 10));
        std::printf("  stop state on all lanes %.1f us after phy_enable\n", (sc_core::sc_time_stamp() - t_en).to_seconds() * 1e6);
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_STOP_STATE));
        CHECK(ctl.phy_adapter().config_ok());
        // Run-time lane disable applies at once; re-enable needs a PHY reset.
        wr(FX1_CSI2_PHY_CONFIG, 0x3D);  // lane 3 off
        wait(1, SC_US);
        CHECK_EQ(ctl.phy_adapter().applied_lane_mask(), 0x7u);
        wr(FX1_CSI2_PHY_CONFIG, 0x7D);
        wait(1, SC_US);
        CHECK_EQ(ctl.phy_adapter().applied_lane_mask(), 0x7u);
        wr(FX1_CSI2_PHY_CONFIG, 0x7D | FX1_CSI2_PHY_CONFIG_DPHY_RST_REQ_MASK);
        wait(1, SC_US);
        CHECK_EQ(rd(FX1_CSI2_PHY_STATUS) & init, 0u);  // recalibrating
        CHECK(poll(FX1_CSI2_LANE_STATUS, stop_all, stop_all, sc_time(10, SC_US), 60));
        CHECK_EQ(ctl.phy_adapter().applied_lane_mask(), 0xFu);
        // phy_enable = 0: shutdown.
        wr(FX1_CSI2_PHY_CONFIG, 0x7C);
        wait(1, SC_US);
        CHECK_EQ(rd(FX1_CSI2_PHY_STATUS) & init, 0u);
        CHECK_EQ(ctl.phy_adapter().applied_lane_mask(), 0u);
    }

    // SoT errors produced by the real PHY reach LANE_STATUS and IRQ 9/10.
    void phy_events() {
        release_reset();
        const uint32_t stop_all = 0x01010101u;
        wr(FX1_CSI2_PHY_CONFIG, 0x7D);
        CHECK(poll(FX1_CSI2_LANE_STATUS, stop_all, stop_all, sc_time(10, SC_US), 60));
        wr(FX1_CSI2_IRQ_STATUS, 0x00FFFFFF);
        tx.clock_start();
        wait(1, SC_US);
        CHECK(rd(FX1_CSI2_PHY_STATUS) & FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK);
        std::vector<bfm::LaneBurst> lb(4);
        for (auto& b : lb) b.bytes.assign(24, 0x00);
        lb[2].sync = 0xB8 ^ 0x04;  // 1-bit leader error on lane 2 only
        tx.send_burst(lb);
        wait(200, SC_NS);
        CHECK_EQ(rd(FX1_CSI2_LANE_STATUS) & 0x06060606u, 0x00020000u);  // lane2_sot_error
        CHECK_EQ(rd(FX1_CSI2_IRQ_STATUS) & (3u << IRQ_SOT_ERROR), 1u << IRQ_SOT_ERROR);
        for (auto& b : lb) b.sync = 0xBD;  // >= 2-bit error at every offset over a zero payload
        tx.send_burst(lb);
        wait(200, SC_NS);
        CHECK_EQ(rd(FX1_CSI2_LANE_STATUS) & 0x04040404u, 0x04040404u);  // sot_sync_error on all lanes
        CHECK(rd(FX1_CSI2_IRQ_STATUS) & (1u << IRQ_SOT_SYNC_ERROR));
        tx.clock_stop();
        wr(FX1_CSI2_CORE_CONFIG, 0x3);  // sticky bits cleared by soft_reset
        wr(FX1_CSI2_CORE_CONFIG, 0x1);
        CHECK_EQ(rd(FX1_CSI2_LANE_STATUS) & 0x06060606u, 0u);
    }

    void run() {
        const std::map<std::string, std::function<void()>> cases = {
            {"csr_reset", [this] { csr_reset(); }},       {"csr_access", [this] { csr_access(); }},
            {"csr_partial", [this] { csr_partial(); }},   {"csr_reserved", [this] { csr_reserved(); }},
            {"irq", [this] { irq_case(); }},              {"counters", [this] { counters(); }},
            {"gsp", [this] { gsp(); }},                   {"timestamp", [this] { timestamp(); }},
            {"soft_reset_scope", [this] { soft_reset_scope(); }}, {"reset_drain", [this] { reset_drain(); }},
            {"reset_drain_resetn", [this] { reset_drain_resetn(); }},
            {"full_reset", [this] { full_reset(); }},     {"ownership", [this] { ownership(); }},
            {"hw_guard", [this] { hw_guard(); }},         {"phy_bringup", [this] { phy_bringup(); }},
            {"phy_events", [this] { phy_events(); }},
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
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR, sc_core::SC_DISPLAY | sc_core::SC_LOG);
    const std::string which = argc > 1 ? argv[1] : "csr_reset";
    Tb tb("tb", which);
    sc_core::sc_start(20, sc_core::SC_MS);
    if (sc_core::sc_get_status() != sc_core::SC_STOPPED) {
        std::printf("case did not finish\n");
        ++g_fail;
    }
    std::printf("%s: %s\n", which.c_str(), g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
