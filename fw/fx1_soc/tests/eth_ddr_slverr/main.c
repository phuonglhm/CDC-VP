/*
 * Ethernet review gap G1: an AXI SLVERR from the DDR3 controller on an
 * Ethernet DMA write (fx1_soc --ddr3-ecc off --ddr3-fault T:write-result:1).
 * The controller answers the next write after T with SLVERR, whatever the
 * master; T lies inside a burst of 16 frames of 1514 bytes at 10 Mbps
 * through the PHY loopback (about 20 ms of DMA writes: RX payload, RX and TX
 * DES0), while the CPU only reads (polling mtime, no stores, no log: the
 * stacks are in DDR). ECC off: with ECC the fault also locks the granule.
 *
 * Timing (calibrated without the fault, ECC off): CPUs released at 1005 us,
 * main 208 us later. The burst is queued from main + ETH_SLVERR_QUEUE_US;
 * the CPU's copies end at main + 17.8 ms (stores up to then), the line keeps
 * sending until about main + 23.6 ms and the read-only window lasts to
 * main + ETH_SLVERR_END_US. T = 20500 us after the release (main + 20.3 ms)
 * sits in the middle; 18500 and 22500 pass as well. The log prints the
 * times it relies on. Frames still in the RX FIFO when RX halted are
 * delivered after the recovery (MAS Table 6-2 row 15).
 *
 * Checks: exactly one direction halted by the error, ERRINFO class 2 (DES0
 * write-back) or 3 (RX payload), DECERR 0 (SLVERR); the frames before the
 * failing write were received byte-exact; `fx1_eth_recover` brings the
 * direction back and frames go through again; no trap.
 */
#include "eth_regs.h"
#include "fx1_eth.h"
#include "fx1_fw.h"

#define ETH(off) (FX1_ETH_CSR_BASE + (off))

#ifndef ETH_SLVERR_QUEUE_US
#define ETH_SLVERR_QUEUE_US 3000u
#endif
#define ETH_SLVERR_END_US (ETH_SLVERR_QUEUE_US + 26000u)
#define BURST 16u

static fx1_eth eth;
static volatile uint32_t traps;
static uint8_t frame[1514];
static uint32_t rx_seen, rx_bad;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mcause;
    (void)mtval;
    traps++;
    return mepc + 4;
}

static void make(uint32_t seed)
{
    for (uint32_t i = 0; i < sizeof frame; i++) frame[i] = (uint8_t)(i * 13u + seed * 7u + 1u);
    for (int i = 0; i < 6; i++) {
        frame[i] = eth.cfg.mac[i];
        frame[6 + i] = eth.cfg.mac[i];
    }
    frame[12] = 0x88;
    frame[13] = 0xB5;
}

/* a received frame is exact when its payload is the pattern of some seed */
static void rx_cb(void* ctx, const uint8_t* f, uint32_t len, uint32_t des0)
{
    (void)ctx;
    rx_seen++;
    int ok = len == sizeof frame && !(des0 & 0x00070000u);
    const uint32_t seed = (uint32_t)(f[14] - (uint8_t)(14u * 13u + 1u)) * 183u & 0xFFu;   /* 7 * 183 = 1 mod 256 */
    for (uint32_t i = 14; ok && i < len; i++) ok = f[i] == (uint8_t)(i * 13u + seed * 7u + 1u);
    if (!ok) rx_bad++;
}

static uint32_t now_us(uint64_t t0) { return (uint32_t)((fx1_mtime() - t0) / (FX1_MTIME_HZ / 1000000u)); }

int main(void)
{
    const uint64_t t0 = fx1_mtime();
    fx1_set_trap_handler(handler);
    fx1_log_hex("eth slverr: main at mtime (low word)", (uint32_t)t0);
    fx1_eth_setup(&eth);
    eth.rx_cb = rx_cb;
    eth.speed100 = 0;
    eth.cfg.phy_loopback = 1;
    fx1_check(fx1_eth_start(&eth) == ETH_OK, 0x10, "eth slverr: start, 10 Mbps, PHY loopback");
    while (now_us(t0) < ETH_SLVERR_QUEUE_US) {
    }
    uint32_t queued = 0;
    for (uint32_t k = 0; k < BURST; k++) {
        make(k);
        if (fx1_eth_send(&eth, frame, sizeof frame) == ETH_OK) queued++;
    }
    const uint32_t q_done = now_us(t0);
    /* reads only from here to the end of the window */
    while (now_us(t0) < ETH_SLVERR_END_US) {
    }
    fx1_log_hex("eth slverr: burst queued by main + us", q_done);
    fx1_log_hex("eth slverr: window ends at main + us", ETH_SLVERR_END_US);
    fx1_check(queued == BURST, 0x11, "eth slverr: 16 frames queued before the window");
    fx1_eth_service(&eth);
    const uint32_t txs = fx1_read32(ETH(ETH_DMA_TX_STAT)), rxs = fx1_read32(ETH(ETH_DMA_RX_STAT));
    const int tx = (eth.err_halted & 2u) != 0;
    const uint32_t info = eth.last_err[tx].errinfo;
    fx1_log_hex("eth slverr: halted (bit 0 RX, bit 1 TX)", eth.err_halted);
    fx1_log_hex("eth slverr: ERRINFO", info);
    fx1_log_hex("eth slverr: TX STAT", txs);
    fx1_log_hex("eth slverr: RX STAT", rxs);
    fx1_log_hex("eth slverr: frames received before the halt", rx_seen);
    fx1_check(eth.err_halted == 1u || eth.err_halted == 2u, 0x20, "eth slverr: exactly one direction halted");
    fx1_check(((info & 0xFu) == 2u || (info & 0xFu) == 3u) && !(info & 0x10u), 0x21,
              "eth slverr: ERRINFO class 2 or 3, DECERR 0: the SLVERR of the failing write");
    fx1_check(rx_seen < BURST && rx_bad == 0, 0x22, "eth slverr: frames before the failing write exact, the rest not received");
    fx1_check(fx1_eth_recover(&eth) == ETH_OK && eth.err_halted == 0 && eth.state == FX1_ETH_RUNNING, 0x30,
              "eth slverr: recovered");
    /* frames of the burst still in the RX FIFO come after the restart (MAS
     * Table 6-2 row 15, receive SRST): take them first */
    const uint32_t before = rx_seen;
    const uint64_t drain_end = fx1_mtime() + (uint64_t)5000u * (FX1_MTIME_HZ / 1000000u);
    while (fx1_mtime() < drain_end) fx1_eth_service(&eth);
    fx1_log_hex("eth slverr: burst frames delivered from the RX FIFO after the recovery", rx_seen - before);
    fx1_check(rx_seen <= BURST && rx_bad == 0, 0x23, "eth slverr: no frame twice, those from the RX FIFO exact");
    /* frames again, one at a time */
    uint32_t got = 0;
    for (uint32_t k = 0; k < 3; k++) {
        make(0x40u + k);
        const uint32_t seen = rx_seen;
        if (fx1_eth_send(&eth, frame, sizeof frame) != ETH_OK) continue;
        const uint64_t end = fx1_mtime() + (uint64_t)4000u * (FX1_MTIME_HZ / 1000000u);
        while (rx_seen == seen && fx1_mtime() < end) fx1_eth_service(&eth);
        if (rx_seen == seen + 1) got++;
    }
    fx1_check(got == 3u && rx_bad == 0, 0x31, "eth slverr: 3 frames through after the recovery, exact");
    fx1_check(traps == 0, 0x32, "eth slverr: no trap");
    fx1_check(fx1_eth_stop(&eth, 4000) == ETH_OK, 0x33, "eth slverr: stop");
    return 0;
}
