/*
 * Ethernet plan M2/M3: the MAC on fx1_soc, bare metal, through the FX1 bus,
 * DDR3, PLIC, write guard and the FX1 Ethernet manager (drivers/fx1_eth.c).
 *
 *   1 CSR through the CPU (local offsets on PERIBUS_1): ID, BUILDCFG, reset
 *     values; access faults: a CSR hole (MAC SLVERR), a byte load and a
 *     misaligned word (FX1 MMIO policy), a store outside its write window
 *   2 probe: MAC identity, the PHY over MDIO (ID 0x0007/0xC0F1)
 *   3 the line peer, told apart by what a broadcast brings back:
 *     loopback, partner (DA = us, SA = 02:00:00:00:00:fe) or none (link down)
 *   4 DMA through DDR3 with the PHY in loopback: 40 frames of 60..1514 bytes
 *     through a 16-entry ring (wraps), byte-exact, at 100 Mbps
 *   5 interrupts: PLIC source 6 on hart 0, claim/ack/complete, the level
 *     falls after the ISR (eth_irq_take), nothing pending after service
 *   6 10 Mbps: speed changed while stopped, frames compared
 *   7 DDR boundary: a transmit buffer crossing the end of the DDR aperture
 *     -> DECERR, TX halts with ERRINFO 0x13, RX keeps working, recovery
 *   8 LR/SC against the Ethernet DMA (write guard, master 0x103): an RX
 *     payload write and an RX DES0 write-back into the reserved granule fail
 *     the SC; a write to another granule does not
 *   9 stop/restart: idle, with traffic; repeat stop; start while running
 *
 * Runs: --eth-peer loopback (default), partner, none (expect tests check the
 * peer line). With --eth-unguarded-dma case 8 must fail (code 0x81).
 */
#include "atomic_common.h"
#include "eth_regs.h"
#include "fx1_eth.h"
#include "fx1_fw.h"

#define ETH(off) (FX1_ETH_CSR_BASE + (off))
#define MIP_MEIP (1u << FX1_IRQ_M_EXT)

static fx1_eth eth;
static volatile uint32_t trap_cause, trap_tval, traps, eth_claims, spurious;
static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t partner[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0xFE};

/* ---- traps: a faulting probe resumes after itself (4-byte instructions) ---- */
static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    if (mcause & 0x80000000u) {                      /* external interrupt */
        const uint32_t id = fx1_plic_claim(0);
        if (id == FX1_IRQ_ETH) {
            fx1_eth_isr(&eth);
            eth_claims++;
        } else if (id) {
            spurious++;
        }
        if (id) fx1_plic_complete(0, id);
        return mepc;
    }
    trap_cause = mcause;
    trap_tval = mtval;
    traps++;
    return mepc + 4;
}

#define PROBE(insn, reg_out, addr)                                                                            \
    __asm__ volatile(".option push\n.option norvc\n" insn "\n.option pop" : "=r"(reg_out) : "r"(addr) : "memory")
static uint32_t probe_lw(uint32_t a) { uint32_t v = 0xDEAD; PROBE("lw %0, 0(%1)", v, a); return v; }
static uint32_t probe_lb(uint32_t a) { uint32_t v = 0xDEAD; PROBE("lbu %0, 0(%1)", v, a); return v; }
static void probe_sw(uint32_t a, uint32_t v)
{
    __asm__ volatile(".option push\n.option norvc\nsw %1, 0(%0)\n.option pop" : : "r"(a), "r"(v) : "memory");
}

static int faulted(uint32_t before, uint32_t cause, uint32_t tval)
{
    return traps == before + 1 && trap_cause == cause && trap_tval == tval;
}

/* ---- helpers -------------------------------------------------------------- */
static uint8_t frame[1514];
static struct {
    uint32_t len, seen, bad, last_len;
    uint8_t last_sa[6], last_da[6];
} rx;

static void rx_cb(void* ctx, const uint8_t* f, uint32_t len, uint32_t des0)
{
    (void)ctx;
    rx.seen++;
    rx.last_len = len;
    for (int i = 0; i < 6; i++) {
        rx.last_da[i] = f[i];
        rx.last_sa[i] = f[6 + i];
    }
    int ok = len == rx.len && !(des0 & 0x00070000u);
    for (uint32_t i = 12; ok && i < len; i++) ok = f[i] == frame[i];
    if (!ok) rx.bad++;
}

static void make(uint32_t len, uint32_t seed, const uint8_t* da)
{
    for (uint32_t i = 0; i < len; i++) frame[i] = (uint8_t)(i * 13u + seed * 7u + 1u);
    for (int i = 0; i < 6; i++) {
        frame[i] = da[i];
        frame[6 + i] = eth.cfg.mac[i];
    }
    frame[12] = 0x88;
    frame[13] = 0xB5;
    rx.len = len;
}

static void delay_us(uint32_t us)
{
    const uint64_t end = fx1_mtime() + (uint64_t)us * (FX1_MTIME_HZ / 1000000u);
    while (fx1_mtime() < end) {
    }
}

/* LR, WFI until MEIP, then ~`spin` loop iterations (DONE may precede the
 * DES0 write-back, MAS 9.3), then SC: one asm block. 0 = SC stored. */
static uint32_t lr_wake_spin_sc(uint32_t addr, uint32_t value, uint32_t spin, uint32_t* lr_value)
{
    uint32_t old, failed, mip, n = spin;
    __asm__ volatile("lr.w   %0, (%4)\n"
                     "1: csrr %2, mip\n"
                     "and    %2, %2, %6\n"
                     "bnez   %2, 2f\n"
                     "wfi\n"
                     "j      1b\n"
                     "2: addi %3, %3, -1\n"
                     "bnez   %3, 2b\n"
                     "sc.w   %1, %5, (%4)\n"
                     : "=&r"(old), "=&r"(failed), "=&r"(mip), "+r"(n)
                     : "r"(addr), "r"(value), "r"(MIP_MEIP)
                     : "memory");
    *lr_value = old;
    return failed;
}

static void drain_irq(void)
{
    while (fx1_read_mip() & MIP_MEIP) {
        const uint32_t id = fx1_plic_claim(0);
        if (id == FX1_IRQ_ETH) fx1_eth_isr(&eth);
        if (id) fx1_plic_complete(0, id);
    }
    fx1_eth_service(&eth);
}

/* service until `n` frames were seen or `us` passed */
static int wait_rx(uint32_t n, uint32_t us)
{
    const uint64_t end = fx1_mtime() + (uint64_t)us * (FX1_MTIME_HZ / 1000000u);
    while (rx.seen < n && fx1_mtime() < end) {
        fx1_eth_service(&eth);
        delay_us(2);
    }
    fx1_eth_service(&eth);
    return rx.seen >= n;
}

static int loop_frames(uint32_t count, uint32_t max_len, uint32_t us_each)
{
    static const uint32_t sizes[] = {60, 61, 64, 127, 128, 255, 256, 511, 512, 1023, 1024, 1514};
    int bad = 0;
    for (uint32_t k = 0; k < count; k++) {
        uint32_t len = sizes[k % (sizeof sizes / sizeof sizes[0])];
        if (len > max_len) len = max_len;
        make(len, k, eth.cfg.mac);
        const uint32_t seen = rx.seen, bad0 = rx.bad;
        if (fx1_eth_send(&eth, frame, len) != ETH_OK || !wait_rx(seen + 1, us_each) || rx.bad != bad0) bad++;
    }
    return bad;
}

static void phy_loopback(int on)
{
    uint16_t bmcr = 0;
    fx1_eth_mdio_read(&eth, FX1_ETH_PHY, 0, &bmcr);
    bmcr = on ? (uint16_t)(bmcr | 0x4000u) : (uint16_t)(bmcr & ~0x4000u);
    fx1_check(fx1_eth_mdio_write(&eth, FX1_ETH_PHY, 0, bmcr) == ETH_OK, 0x0F, "PHY BMCR loopback written while stopped");
}

int main(void)
{
    fx1_set_trap_handler(handler);
    fx1_eth_setup(&eth);
    eth.rx_cb = rx_cb;

    /* 1: CSR through the CPU */
    fx1_check(fx1_read32(ETH(ETH_ID)) == 0x45544801u, 0x10, "1: ETH_ID 'ETH' v0.1 at 0x1103_0000");
    fx1_check(fx1_read32(ETH(ETH_DMA_BUILDCFG)) == ETH_DRV_BUILDCFG && fx1_read32(ETH(ETH_CAP)) == 1u, 0x11,
              "1: BUILDCFG/CAP: 32-bit AXI default build");
    fx1_check(fx1_read32(ETH(ETH_CTRL)) == 0 && fx1_read32(ETH(ETH_RX_MAX_FRAME)) == 1518u, 0x12,
              "1: after power-on reset ETH_CTRL holds every block, RX_MAX_FRAME 1518");
    uint32_t t = traps;
    (void)probe_lw(ETH(0x0024));
    fx1_check(faulted(t, 5, ETH(0x0024)), 0x13, "1: load from a CSR hole: access fault (MAC SLVERR)");
    t = traps;
    probe_sw(ETH(0x9000), 1);
    fx1_check(faulted(t, 7, ETH(0x9000)), 0x14, "1: store to a CSR hole: access fault");
    t = traps;
    (void)probe_lb(ETH(ETH_ID));
    fx1_check(faulted(t, 5, ETH(ETH_ID)), 0x15, "1: byte load: access fault (32-bit MMIO only)");
    t = traps;
    (void)probe_lw(ETH(ETH_ID + 2));
    fx1_check(traps == t + 1 && (trap_cause == 4 || trap_cause == 5), 0x16, "1: misaligned word load traps");
    fx1_log_hex("1: misaligned load mcause", trap_cause);
    fx1_write32(ETH(ETH_CTRL), 0x3FFFu);
    fx1_write32(ETH(ETH_RX_MAC_CFG), 1);
    t = traps;
    probe_sw(ETH(ETH_FILT_CTRL), 7);
    fx1_check(faulted(t, 7, ETH(ETH_FILT_CTRL)) && fx1_read32(ETH(ETH_FILT_CTRL)) != 7u, 0x17,
              "1: store outside its write window (RX_EN=1): access fault, register unchanged");
    fx1_write32(ETH(ETH_RX_MAC_CFG), 0);
    fx1_write32(ETH(ETH_CTRL), 0);
    fx1_log("1: CSR through the CPU, faults propagated");

    /* 2: probe */
    fx1_eth_probe_info pi;
    fx1_check(fx1_eth_probe(&eth, &pi) == ETH_OK && pi.mac_ok && pi.phy_ok, 0x20, "2: MAC and PHY found");
    fx1_check(pi.phy_id1 == 0x0007 && pi.phy_id2 == 0xC0F1, 0x21, "2: PHY ID 0x0007/0xC0F1 over MDIO");
    uint16_t v = 0;
    fx1_check(fx1_eth_mdio_read(&eth, 9, 2, &v) == ETH_E_MDIO, 0x22, "2: no PHY at MDIO 9: EMDIO");

    /* 3: the line peer */
    fx1_check(fx1_eth_start(&eth) == ETH_OK, 0x30, "3: cold start");
    make(100, 1, bcast);
    rx.seen = 0;
    fx1_eth_send(&eth, frame, 100);
    wait_rx(1, 2000);
    if (rx.seen == 0) {
        fx1_check(!pi.link, 0x31, "3: nothing came back: the PHY reports link down");
        fx1_log("3: eth peer: none (link down)");
    } else {
        int from_partner = 1, to_us = 1;
        for (int i = 0; i < 6; i++) {
            from_partner &= rx.last_sa[i] == partner[i];
            to_us &= rx.last_da[i] == eth.cfg.mac[i];
        }
        if (from_partner && to_us) fx1_log("3: eth peer: partner (echo to us)");
        else fx1_log("3: eth peer: loopback");
        fx1_check(pi.link && rx.last_len == 100, 0x32, "3: link up, the frame came back whole");
    }
    fx1_check(fx1_eth_stop(&eth, 2000) == ETH_OK && eth.state == FX1_ETH_STOPPED, 0x33, "3: stop");

    /* 4: DMA through DDR3, PHY loopback, 100 Mbps */
    phy_loopback(1);
    fx1_check(fx1_eth_start(&eth) == ETH_OK, 0x40, "4: restart");
    rx.seen = rx.bad = 0;
    const int bad4 = loop_frames(40, 1514, 2000);
    fx1_check(bad4 == 0 && rx.seen == 40, 0x41, "4: 40 frames 60..1514 bytes through a 16-entry ring, byte-exact");
    fx1_check(fx1_eth_hal_range_errors() == 0, 0x42, "4: every DMA range inside DDR");
    fx1_log("4: DMA through DDR3: 40 frames, ring wrapped");

    /* 5: interrupts on PLIC source 6 */
    fx1_plic_set_priority(FX1_IRQ_ETH, 3);
    fx1_plic_enable(0, FX1_IRQ_ETH, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
    const uint32_t c0 = eth_claims;
    make(200, 5, eth.cfg.mac);
    const uint32_t seen5 = rx.seen;
    fx1_eth_send(&eth, frame, 200);
    wait_rx(seen5 + 1, 2000);
    delay_us(20);
    fx1_eth_service(&eth);
    fx1_check(eth_claims > c0 && spurious == 0, 0x50, "5: source 6 claimed on hart 0");
    fx1_check(!(fx1_read_mip() & MIP_MEIP) && !(fx1_plic_pending() & (1u << FX1_IRQ_ETH)), 0x51,
              "5: after the ISR and complete nothing pends, MEIP low");
    fx1_check(eth.pending == 0, 0x52, "5: the service consumed every pending bit");
    fx1_log_hex("5: ethernet claims", eth_claims);

    /* 6: 10 Mbps */
    fx1_check(fx1_eth_stop(&eth, 2000) == ETH_OK, 0x60, "6: stop");
    fx1_check(fx1_eth_set_speed(&eth, 0) == ETH_OK, 0x61, "6: 10 Mbps set while stopped");
    phy_loopback(1);
    fx1_check(fx1_eth_start(&eth) == ETH_OK, 0x62, "6: start at 10 Mbps");
    rx.seen = rx.bad = 0;
    fx1_check(loop_frames(4, 1514, 4000) == 0, 0x63, "6: 4 frames at 10 Mbps, byte-exact");
    fx1_check(fx1_eth_stop(&eth, 4000) == ETH_OK && fx1_eth_set_speed(&eth, 1) == ETH_OK, 0x64, "6: back to 100 Mbps");
    phy_loopback(1);
    fx1_check(fx1_eth_start(&eth) == ETH_OK, 0x65, "6: restart at 100 Mbps");

    /* 7: a transmit buffer crossing the end of DDR */
    eth_dev_t* d = &eth.dev;
    const uint32_t i7 = d->tx_head % FX1_ETH_RING;
    const uint32_t desc = eth.cfg.tx_ring + 16u * i7;
    const uint32_t des1 = 0xC0000000u | 128u, des2 = FX1_DDR_SIZE - 64u, own = 0x80000000u;
    fx1_eth_hal_mmio()->mem_wr(0, desc + 4, &des1, 4);
    fx1_eth_hal_mmio()->mem_wr(0, desc + 8, &des2, 4);
    fx1_eth_hal_mmio()->mem_wr(0, desc, &own, 4);
    d->tx_head++;
    fx1_write32(ETH(ETH_DMA_TX_POLL), 1);
    delay_us(50);
    fx1_eth_service(&eth);
    fx1_check(eth.err_halted == 2u && eth.last_err[1].errinfo == 0x13u, 0x70,
              "7: TX_DATA_RD_ERR with DECERR past the DDR aperture: TX halted, ERRINFO 0x13");
    make(80, 7, eth.cfg.mac);
    fx1_check(fx1_eth_send(&eth, frame, 80) == ETH_E_STATE, 0x71, "7: no submission on a halted direction");
    fx1_check(fx1_eth_recover(&eth) == ETH_OK && eth.err_halted == 0, 0x72, "7: recovered (SRST, ring rebuilt)");
    rx.seen = rx.bad = 0;
    fx1_check(loop_frames(3, 600, 2000) == 0, 0x73, "7: traffic again after the recovery");

    /* 8: LR/SC against the Ethernet DMA (interrupts masked; WFI wakes on MEIP).
     * Only RX_DONE may wake the hart: TX_DONE comes as soon as the frame is in
     * the TX FIFO, before the loopback copy is written into the RX buffer. */
    fx1_global_irq_disable();
    fx1_eth_service(&eth);
    fx1_write32(ETH(ETH_INT_EN), ETH_INT_RAW_RX_DONE_MASK);
    d->int_en = ETH_INT_RAW_RX_DONE_MASK;
    const uint32_t slot = d->rx_head % FX1_ETH_RING;
    const uint32_t payload = (uint32_t)(uintptr_t)fx1_eth_cpu(eth.cfg.rx_buf + FX1_ETH_STRIDE * slot + 64u);
    const uint32_t other = AT_WORD(5);               /* a granule no device writes */
    at_write(payload, 0x5A5A5A5Au);
    make(1514, 8, eth.cfg.mac);
    fx1_eth_send(&eth, frame, 1514);                 /* 122 us on the wire: the LR comes first */
    uint32_t lr_value = 0;
    const uint32_t sc1 = lr_wake_spin_sc(payload, 0x11111111u, 2000, &lr_value);
    fx1_check(lr_value == 0x5A5A5A5Au, 0x80, "8: the LR ran before the RX payload write");
    fx1_check(sc1 != 0, 0x81, "8: RX payload written into the reserved granule: SC fails");
    drain_irq();
    /* DES0 write-back of the next descriptor */
    const uint32_t slot2 = d->rx_head % FX1_ETH_RING;
    const uint32_t des0 = (uint32_t)(uintptr_t)fx1_eth_cpu(eth.cfg.rx_ring + 16u * slot2);
    make(1514, 9, eth.cfg.mac);
    fx1_eth_send(&eth, frame, 1514);
    const uint32_t sc2 = lr_wake_spin_sc(des0, 0x80000000u, 2000, &lr_value);
    fx1_check(lr_value == 0x80000000u && sc2 != 0, 0x82, "8: RX DES0 write-back into the reserved word: SC fails");
    drain_irq();
    /* control: the DMA writes elsewhere, the reservation holds */
    at_write(other, 0x22222222u);
    make(1514, 10, eth.cfg.mac);
    fx1_eth_send(&eth, frame, 1514);
    const uint32_t sc3 = lr_wake_spin_sc(other, 0x33333333u, 2000, &lr_value);
    fx1_check(sc3 == 0 && at_read(other) == 0x33333333u, 0x83, "8: Ethernet writes to other granules: SC succeeds");
    drain_irq();
    eth_irq_unmask(d, eth.cfg.int_en);
    fx1_global_irq_enable();
    fx1_log("8: LR/SC: Ethernet payload and DES0 writes cancel the reservation, other granules do not");

    /* 9: stop/restart */
    fx1_check(fx1_eth_start(&eth) == ETH_OK && eth.state == FX1_ETH_RUNNING, 0x90, "9: start while running: no-op");
    for (int k = 0; k < 4; k++) {
        make(1514, 20 + k, eth.cfg.mac);
        fx1_eth_send(&eth, frame, 1514);
    }
    fx1_check(fx1_eth_stop(&eth, 0) == ETH_OK && eth.state == FX1_ETH_STOPPED, 0x91, "9: stop with frames queued");
    fx1_log_hex("9: dropped by the stop", eth.last_stop.tx_dropped);
    fx1_check(fx1_eth_stop(&eth, 0) == ETH_OK, 0x92, "9: stop again: OK");
    fx1_check(fx1_eth_start(&eth) == ETH_OK, 0x93, "9: start again");
    rx.seen = rx.bad = 0;
    fx1_check(loop_frames(2, 300, 2000) == 0 && rx.seen == 2, 0x94, "9: only the new session's frames arrive");
    fx1_check(fx1_eth_stop(&eth, 2000) == ETH_OK, 0x95, "9: final stop");
    fx1_log("9: stop/restart");
    return 0;
}
