/* Portable driver of the FTEL Ethernet MAC (MAS v1.0.7 section 9). */

#include "eth_drv.h"

#include "eth_regs.h"

#define RD(off)     d->hal.rd(d->hal.ctx, (off))
#define WR(off, v)  d->hal.wr(d->hal.ctx, (off), (v))

#define DES0_OWN    0x80000000u
#define DES1_LS     0x80000000u
#define DES1_FS     0x40000000u
#define DMA_IDLE    ETH_DMA_TX_STAT_IDLE_MASK
#define DMA_HALTED  ETH_DMA_TX_STAT_HALTED_MASK

/* PHY (Clause 22) */
#define BMCR 0u
#define BMCR_LOOP  0x4000u
#define BMCR_SPEED 0x2000u
#define BMCR_AN    0x1000u
#define BMCR_FDX   0x0100u

static void w32(eth_dev_t *d, uint32_t addr, uint32_t v) { d->hal.mem_wr(d->hal.ctx, addr, &v, 4); }

static uint32_t r32(eth_dev_t *d, uint32_t addr)
{
    uint32_t v;
    d->hal.mem_rd(d->hal.ctx, addr, &v, 4);
    return v;
}

static uint32_t txd(eth_dev_t *d, uint32_t i) { return d->cfg.tx_ring + 16u * (i % d->cfg.ring_len); }
static uint32_t rxd(eth_dev_t *d, uint32_t i) { return d->cfg.rx_ring + 16u * (i % d->cfg.ring_len); }
static uint32_t rxb(eth_dev_t *d, uint32_t i)
{
    return d->cfg.rx_buf + d->cfg.buf_stride * (i % d->cfg.ring_len) + d->cfg.rx_lead;
}

/* poll a register field for up to `us` microseconds */
static int wait_reg(eth_dev_t *d, uint32_t off, uint32_t mask, uint32_t val, uint32_t us)
{
    for (uint32_t t = 0; t <= us; t++) {
        if ((RD(off) & mask) == val) return ETH_OK;
        d->hal.delay_us(d->hal.ctx, 1);
    }
    return ETH_E_TIMEOUT;
}

#define MDIO_STICKY (ETH_MDIO_STAT_DONE_MASK | ETH_MDIO_STAT_NO_RESPONSE_MASK | ETH_MDIO_STAT_START_WHILE_BUSY_MASK)

/* One Clause 22 frame, completion by polling only (the MDIO_DONE interrupt is
 * expected masked). The divider in use (reset value PARA_MDIO_CLK_DIV) is
 * kept; the deadline covers 65 MDC periods of it plus margin. */
static int mdio_xfer(eth_dev_t *d, unsigned phy, unsigned reg, int write, uint16_t wdata, uint16_t *rdata)
{
    uint32_t div = (RD(ETH_MDIO_CTRL) & ETH_MDIO_CTRL_CLK_DIV_MASK) >> ETH_MDIO_CTRL_CLK_DIV_SHIFT;
    if (div < 40u) div = 40u;
    const uint32_t frame_us = (65u * div + 99u) / 100u;      /* i_eth_clk 100 MHz */
    /* a frame still running (an earlier caller timed out): let it end */
    if (wait_reg(d, ETH_MDIO_STAT, ETH_MDIO_STAT_BUSY_MASK, 0, frame_us + 20u)) return ETH_E_MDIO;
    WR(ETH_MDIO_STAT, MDIO_STICKY);                           /* no stale DONE */
    if (write) WR(ETH_MDIO_DATA, wdata);
    WR(ETH_MDIO_CTRL, ETH_MDIO_CTRL_START_MASK | (write ? ETH_MDIO_CTRL_OP_MASK : 0) |
                          ((reg & 31u) << ETH_MDIO_CTRL_REGAD_SHIFT) |
                          ((phy & 31u) << ETH_MDIO_CTRL_PHYAD_SHIFT) |
                          (div << ETH_MDIO_CTRL_CLK_DIV_SHIFT));
    if (wait_reg(d, ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK, ETH_MDIO_STAT_DONE_MASK, 2u * frame_us + 20u))
        return ETH_E_TIMEOUT;
    uint32_t st = RD(ETH_MDIO_STAT);
    WR(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK | ETH_MDIO_STAT_NO_RESPONSE_MASK |
                          ETH_MDIO_STAT_START_WHILE_BUSY_MASK);
    if (st & ETH_MDIO_STAT_NO_RESPONSE_MASK) return ETH_E_MDIO;
    if (rdata) *rdata = (uint16_t)RD(ETH_MDIO_DATA);
    return ETH_OK;
}

int eth_mdio_read(eth_dev_t *d, unsigned phy, unsigned reg, uint16_t *v) { return mdio_xfer(d, phy, reg, 0, 0, v); }

int eth_mdio_write(eth_dev_t *d, unsigned phy, unsigned reg, uint16_t v) { return mdio_xfer(d, phy, reg, 1, v, 0); }

/* ---- configuration check (review F03) --------------------------------------- */
typedef struct region {
    uint64_t lo, hi;                                         /* [lo, hi) */
} region_t;

static int overlap(region_t a, region_t b) { return a.lo < b.hi && b.lo < a.hi; }

int eth_cfg_check(const eth_hal_t *hal, const eth_cfg_t *cfg)
{
    if (!hal || !cfg || !hal->rd || !hal->wr || !hal->mem_wr || !hal->mem_rd || !hal->delay_us) return ETH_E_CFG;
    const uint64_t n = cfg->ring_len;
    if (n == 0 || n > ETH_DRV_MAX_RING) return ETH_E_CFG;
    if ((cfg->tx_ring | cfg->rx_ring) & 15u) return ETH_E_CFG;          /* 16-byte descriptors */
    if ((uint64_t)cfg->rx_lead + 1514u > cfg->buf_stride) return ETH_E_CFG;   /* RX_BUF_SIZE 1514 */
    if (cfg->buf_stride < 1514u) return ETH_E_CFG;                      /* any transmit frame */
    if (cfg->phy_addr < -1 || cfg->phy_addr > 31) return ETH_E_CFG;
    if (cfg->int_en & (ETH_INT_RAW_TX_BUF_UNAVAIL_MASK | 0x40u | 0xFFFFFF00u)) return ETH_E_CFG;
    const uint64_t win_lo = cfg->dma_base;
    const uint64_t win_hi = cfg->dma_size ? win_lo + cfg->dma_size : (1ull << 32);
    if (win_hi > (1ull << 32)) return ETH_E_CFG;
    const region_t r[4] = {
        {cfg->tx_ring, cfg->tx_ring + 16u * n},
        {cfg->rx_ring, cfg->rx_ring + 16u * n},
        {cfg->tx_buf, cfg->tx_buf + (uint64_t)cfg->buf_stride * n},
        {cfg->rx_buf, cfg->rx_buf + (uint64_t)cfg->buf_stride * n},
    };
    for (int i = 0; i < 4; i++) {
        if (r[i].lo < win_lo || r[i].hi > win_hi) return ETH_E_CFG;     /* also no 32-bit wrap */
        for (int j = i + 1; j < 4; j++)
            if (overlap(r[i], r[j])) return ETH_E_CFG;
    }
    return ETH_OK;
}

static void ring_init(eth_dev_t *d, int tx, int rx)
{
    for (uint32_t i = 0; i < d->cfg.ring_len; i++) {
        if (tx) {
            w32(d, txd(d, i) + 4, 0);
            w32(d, txd(d, i) + 8, 0);
            w32(d, txd(d, i), 0);
        }
        if (rx) {
            w32(d, rxd(d, i) + 4, 0);
            w32(d, rxd(d, i) + 8, rxb(d, i));
            w32(d, rxd(d, i), DES0_OWN);
        }
    }
    if (tx) d->tx_head = d->tx_tail = 0;
    if (rx) d->rx_head = 0;
}

int eth_init(eth_dev_t *d, const eth_hal_t *hal, const eth_cfg_t *cfg)
{
    /* before any side effect: nothing below runs on a rejected configuration */
    const int chk = eth_cfg_check(hal, cfg);
    if (chk) return chk;
    d->hal = *hal;
    d->cfg = *cfg;
    d->tx_head = d->tx_tail = d->rx_head = 0;
    d->tx_errors = d->rx_errors = d->tx_done = d->rx_done = 0;
    d->tx_dropped = 0;
    d->tx_unconfirmed = 0;
    d->int_en = cfg->int_en;

    /* status and build configuration before any write */
    const uint32_t st_mask = ETH_DMA_TX_STAT_RUNNING_MASK | ETH_DMA_TX_STAT_HALTED_MASK |
                             ETH_DMA_TX_STAT_IDLE_MASK | ETH_DMA_TX_STAT_SUSPENDED_MASK;
    if ((RD(ETH_DMA_TX_STAT) & st_mask) != DMA_IDLE || (RD(ETH_DMA_RX_STAT) & st_mask) != DMA_IDLE)
        return ETH_E_STATE;
    if (RD(ETH_DMA_BUILDCFG) != ETH_DRV_BUILDCFG) return ETH_E_BUILDCFG;

    /* rings: DES1[29:16] = 0 everywhere; RX DES2 then OWN; TX OWN = 0 */
    ring_init(d, 1, 1);

    /* the grouped writes, in any order, while ETH_CTRL still holds the IP */
    WR(ETH_RMII_CFG, cfg->speed100 ? ETH_RMII_CFG_SPEED_MASK : 0);
    WR(ETH_UC_ADDR_L0, (uint32_t)cfg->mac[0] | (uint32_t)cfg->mac[1] << 8 | (uint32_t)cfg->mac[2] << 16 |
                           (uint32_t)cfg->mac[3] << 24);
    WR(ETH_UC_ADDR_H0, (uint32_t)cfg->mac[4] | (uint32_t)cfg->mac[5] << 8);
    WR(ETH_UC_CTRL0, 1);
    WR(ETH_FILT_CTRL, cfg->filt_ctrl);
    WR(ETH_HASH_L, (uint32_t)cfg->hash);
    WR(ETH_HASH_H, (uint32_t)(cfg->hash >> 32));
    WR(ETH_DMA_TX_DESC_BASE, cfg->tx_ring);
    WR(ETH_DMA_TX_DESC_LEN, cfg->ring_len);
    WR(ETH_DMA_RX_DESC_BASE, cfg->rx_ring);
    WR(ETH_DMA_RX_DESC_LEN, cfg->ring_len);
    WR(ETH_DMA_RX_BUF_SIZE, 1514);              /* >= ETH_RX_MAX_FRAME - 4 */
    WR(ETH_CSUM_RX_CFG, 3);
    WR(ETH_CSUM_TX_CFG, 3);
    WR(ETH_DMA_TX_COAL, cfg->coal);
    WR(ETH_DMA_RX_COAL, cfg->coal);
    WR(ETH_INT_EN, cfg->int_en);
    WR(RMON_CTRL, RMON_CTRL_RMON_EN_MASK);
    WR(ETH_CTRL, 0x3FFFu);                      /* release every block */

    /* the PHY rate must match ETH_RMII_CFG.SPEED */
    if (cfg->phy_addr >= 0) {
        uint16_t id1;
        if (eth_mdio_read(d, (unsigned)cfg->phy_addr, 2, &id1)) return ETH_E_MDIO;
        uint16_t bmcr = BMCR_FDX | (cfg->phy_loopback ? BMCR_LOOP : 0);
        bmcr |= cfg->speed100 ? BMCR_SPEED | BMCR_AN : 0;   /* 10 Mbps: AN off, SPEED 0 */
        if (eth_mdio_write(d, (unsigned)cfg->phy_addr, BMCR, bmcr)) return ETH_E_MDIO;
    }

    /* receive first, so the link partner's first frame finds it running */
    WR(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    WR(ETH_RX_MAC_CFG, ETH_RX_MAC_CFG_RX_EN_MASK);
    WR(ETH_TX_MAC_CFG, ETH_TX_MAC_CFG_TX_EN_MASK);
    WR(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
    return ETH_OK;
}

int eth_send(eth_dev_t *d, const void *frame, uint32_t len)
{
    if (len == 0 || len > 1514 || len > d->cfg.buf_stride) return ETH_E_LEN;
    if (d->tx_head - d->tx_tail >= d->cfg.ring_len) {
        eth_tx_reclaim(d);
        if (d->tx_head - d->tx_tail >= d->cfg.ring_len) return ETH_E_FULL;
    }
    const uint32_t i = d->tx_head;
    const uint32_t buf = d->cfg.tx_buf + d->cfg.buf_stride * (i % d->cfg.ring_len);
    d->hal.mem_wr(d->hal.ctx, buf, frame, len);
    w32(d, txd(d, i) + 8, buf);                                 /* DES2 */
    w32(d, txd(d, i) + 4, DES1_FS | DES1_LS | len);             /* DES1 */
    w32(d, txd(d, i), DES0_OWN);                                /* OWN last */
    d->tx_head++;
    WR(ETH_DMA_TX_POLL, ETH_DMA_TX_POLL_POLL_MASK);
    return ETH_OK;
}

int eth_tx_reclaim(eth_dev_t *d)
{
    int n = 0;
    while (d->tx_tail != d->tx_head) {
        const uint32_t des0 = r32(d, txd(d, d->tx_tail));
        if (des0 & DES0_OWN) break;
        if (des0 & 0x00070000u) d->tx_errors++;                /* DES0[18:16] */
        else d->tx_unconfirmed++;                              /* in the TX FIFO, not yet on the line */
        d->tx_tail++;
        d->tx_done++;
        n++;
    }
    return n;
}

int eth_recv(eth_dev_t *d, void *buf, uint32_t max, uint32_t *des0)
{
    const uint32_t i = d->rx_head;
    const uint32_t w = r32(d, rxd(d, i));
    if (w & DES0_OWN) return 0;
    uint32_t len = w & 0xFFFFu;
    if (des0) *des0 = w;
    if (w & 0x00070000u) d->rx_errors++;
    /* RX_DATA_WR_ERR (DES0[17]): LENGTH is undefined (MAS Table 6-29 row 7); no data */
    const int axi_err = (w & 0x00020000u) != 0;
    if (axi_err) len = 0;
    if (len > max) len = max;
    if (len) d->hal.mem_rd(d->hal.ctx, rxb(d, i), buf, len);
    w32(d, rxd(d, i), DES0_OWN);                                /* return it, DES2 unchanged */
    d->rx_head++;
    d->rx_done++;
    WR(ETH_DMA_RX_POLL, ETH_DMA_RX_POLL_POLL_MASK);             /* mandatory (MAS 9.3.2) */
    return axi_err ? -1 : (int)len;
}

/* Kept for the original examples: clears DONE only. A FreeRTOS ISR uses
 * eth_irq_take(), which also masks the level error sources. */
uint32_t eth_irq_ack(eth_dev_t *d)
{
    const uint32_t st = RD(ETH_INT_STATUS);
    if (st & ETH_INT_RAW_TX_DONE_MASK) WR(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK);
    if (st & ETH_INT_RAW_RX_DONE_MASK) WR(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_DONE_MASK);
    if (st & ETH_INT_RAW_MDIO_DONE_MASK) WR(ETH_MDIO_STAT, ETH_MDIO_STAT_DONE_MASK);
    return st;
}

int eth_stop(eth_dev_t *d) { return eth_quiesce(d, 0, 1000, 0); }

/* ---- lifecycle -------------------------------------------------------------- */
static int both_halted(eth_dev_t *d)
{
    return (RD(ETH_DMA_TX_STAT) & DMA_HALTED) && (RD(ETH_DMA_RX_STAT) & DMA_HALTED);
}

/* The Tx MAC idle on two samples more than 20 us apart (more than the 9.6 us
 * IPG at 10 Mbps): nothing is left in the TX FIFO when nothing enters it. Up
 * to `us` microseconds; adds the time spent to *t. */
static int tx_mac_idle(eth_dev_t *d, uint32_t us, uint32_t *t)
{
    uint32_t idle = 0;
    for (uint32_t k = 0; k < us; k++) {
        const int busy = (RD(ETH_STATUS) & ETH_STATUS_TX_BUSY_MASK) != 0;
        idle = busy ? 0 : idle + 1;
        if (idle > 20u) return 1;
        d->hal.delay_us(d->hal.ctx, 1);
        ++*t;
    }
    return 0;
}

/* Poll both HALTED bits for up to `us`; returns the microseconds spent, or
 * -1 on timeout. */
static int wait_halted(eth_dev_t *d, uint32_t us)
{
    for (uint32_t t = 0; t <= us; t++) {
        if (both_halted(d)) return (int)t;
        d->hal.delay_us(d->hal.ctx, 1);
    }
    return -1;
}

int eth_quiesce(eth_dev_t *d, uint32_t drain_us, uint32_t halt_us, eth_stop_report_t *r)
{
    eth_stop_report_t local;
    if (!r) r = &local;
    *r = (eth_stop_report_t){0};
    uint32_t t = 0;
    /* 1. drain: the queued transmit frames go out (the caller stopped submitting) */
    while (d->tx_tail != d->tx_head && t < drain_us) {
        r->tx_drained += (uint32_t)eth_tx_reclaim(d);
        if (d->tx_tail == d->tx_head) break;
        d->hal.delay_us(d->hal.ctx, 1);
        t++;
    }
    r->tx_drained += (uint32_t)eth_tx_reclaim(d);
    /* ... and the frames already in the TX FIFO leave it */
    int fifo_empty = !d->tx_unconfirmed;
    if (!fifo_empty && d->tx_tail == d->tx_head && t < drain_us) fifo_empty = tx_mac_idle(d, drain_us - t, &t);
    /* 2. HALT_REQ, then a second one aborts the open unit at once (9.3.4) */
    WR(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
    WR(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
    int w = wait_halted(d, halt_us);
    if (w < 0) {
        r->forced = 1;
        t += halt_us;
        if (!(RD(ETH_DMA_TX_STAT) & DMA_HALTED)) WR(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_HALT_REQ_MASK);
        if (!(RD(ETH_DMA_RX_STAT) & DMA_HALTED)) WR(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_HALT_REQ_MASK);
        w = wait_halted(d, halt_us);
    }
    if (w < 0) {
        /* not HALTED: the enables stay, the rings still belong to the DMA */
        r->elapsed_us = t + halt_us;
        r->tx_stat = RD(ETH_DMA_TX_STAT);
        r->rx_stat = RD(ETH_DMA_RX_STAT);
        r->status = RD(ETH_STATUS);
        return ETH_E_TIMEOUT;
    }
    t += (uint32_t)w;
    /* frames written back, whenever they were reclaimed (review ER03), and not
     * seen sent: one more look with the DMA halted, so nothing enters the FIFO.
     * Only TX_EN 1 makes an idle Tx MAC mean an empty FIFO. A frame written
     * back while HALTED was awaited entered the FIFO after any earlier idle
     * sample: that sample no longer counts (review ER03, second round). */
    const int late = eth_tx_reclaim(d);
    r->tx_drained += (uint32_t)late;
    if (late) fifo_empty = 0;
    if (d->tx_unconfirmed && !fifo_empty && (RD(ETH_TX_MAC_CFG) & ETH_TX_MAC_CFG_TX_EN_MASK))
        fifo_empty = tx_mac_idle(d, 25u, &t);
    if (fifo_empty) d->tx_unconfirmed = 0;
    r->tx_fifo_flushed = d->tx_unconfirmed != 0;
    /* 3. only after HALTED: the MAC enables, then the MACs idle (Table 9-7 step 2) */
    WR(ETH_TX_MAC_CFG, 0);
    WR(ETH_RX_MAC_CFG, 0);
    d->hal.delay_us(d->hal.ctx, 1);                          /* >= 13 RMII cycles */
    t++;
    const uint32_t busy = ETH_STATUS_TX_BUSY_MASK | ETH_STATUS_RX_BUSY_MASK;
    const uint32_t frame_us = 1300;                          /* 1518 + 8 bytes at 10 Mbps */
    if (wait_reg(d, ETH_STATUS, busy, 0, frame_us)) {
        r->elapsed_us = t + frame_us;
        r->tx_stat = RD(ETH_DMA_TX_STAT);
        r->rx_stat = RD(ETH_DMA_RX_STAT);
        r->status = RD(ETH_STATUS);
        return ETH_E_TIMEOUT;
    }
    /* 4. nothing of this session may leave later: flush the TX path group and
     * the RX FIFO (Table 6-2 rows 5 and 8, both directions HALTED) */
    const uint32_t ctrl = RD(ETH_CTRL);
    WR(ETH_CTRL, ctrl & ~(ETH_CTRL_TXFIFO_WR_RST_N_MASK | ETH_CTRL_RXFIFO_RD_RST_N_MASK));
    WR(ETH_CTRL, ctrl);
    d->tx_unconfirmed = 0;                                   /* sent or flushed: none left */
    /* 5. no interrupt from this session reaches the next one */
    d->int_en = 0;
    WR(ETH_INT_EN, 0);
    WR(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK);
    WR(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_DONE_MASK);
    /* 6. the halted DMA no longer reads the ring: the descriptors still owned
     * are software's again, their frames were never sent */
    r->tx_drained += (uint32_t)eth_tx_reclaim(d);
    while (d->tx_tail != d->tx_head) {
        w32(d, txd(d, d->tx_tail), 0);
        d->tx_tail++;
        r->tx_dropped++;
    }
    d->tx_dropped += r->tx_dropped;
    r->elapsed_us = t;
    r->tx_stat = RD(ETH_DMA_TX_STAT);
    r->rx_stat = RD(ETH_DMA_RX_STAT);
    r->status = RD(ETH_STATUS);
    return ETH_OK;
}

int eth_restart(eth_dev_t *d)
{
    if (!both_halted(d)) return ETH_E_STATE;
    WR(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_SRST_MASK);          /* accepted while HALTED */
    WR(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_SRST_MASK);
    ring_init(d, 1, 1);
    WR(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK | ETH_DMA_TX_STAT_ERR_MASK);
    WR(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_DONE_MASK | ETH_DMA_RX_STAT_ERR_MASK);
    d->int_en = d->cfg.int_en;
    WR(ETH_INT_EN, d->int_en);
    WR(ETH_DMA_RX_CTRL, ETH_DMA_RX_CTRL_START_MASK);
    WR(ETH_RX_MAC_CFG, ETH_RX_MAC_CFG_RX_EN_MASK);
    WR(ETH_TX_MAC_CFG, ETH_TX_MAC_CFG_TX_EN_MASK);
    WR(ETH_DMA_TX_CTRL, ETH_DMA_TX_CTRL_START_MASK);
    return ETH_OK;
}

int eth_recover(eth_dev_t *d, int tx, eth_err_snap_t *snap, uint32_t *dropped)
{
    const uint32_t stat_off = tx ? ETH_DMA_TX_STAT : ETH_DMA_RX_STAT;
    const uint32_t ctrl_off = tx ? ETH_DMA_TX_CTRL : ETH_DMA_RX_CTRL;
    eth_err_snap_t local;
    if (!snap) snap = &local;
    if (dropped) *dropped = 0;
    /* snapshot before the W1C of ERR clears ERRINFO */
    snap->stat = RD(stat_off);
    snap->errinfo = RD(tx ? ETH_DMA_TX_ERRINFO : ETH_DMA_RX_ERRINFO);
    snap->cur_desc = RD(tx ? ETH_DMA_TX_CUR_DESC : ETH_DMA_RX_CUR_DESC);
    const int err = (snap->stat & ETH_DMA_TX_STAT_ERR_MASK) != 0;
    snap->halted = (snap->stat & DMA_HALTED) || (err && eth_err_halting(snap->errinfo));
    if (!snap->halted) {
        /* TX_TOO_LONG / RX_TRUNC: the direction runs on (Table 9-6) */
        if (err) WR(stat_off, ETH_DMA_TX_STAT_ERR_MASK);
        if (!(RD(stat_off) & DMA_HALTED)) return ETH_OK;
        snap->halted = 1;                                    /* a halting error behind class 4 */
    }
    /* review ER01: ERR (and the interrupt) may precede HALTED by the faulting
     * descriptor's write-back and the drain; SRST is accepted only once HALTED */
    if (wait_reg(d, stat_off, DMA_HALTED, DMA_HALTED, ETH_DRV_HALT_WAIT_US)) return ETH_E_TIMEOUT;
    WR(ctrl_off, ETH_DMA_TX_CTRL_SRST_MASK);
    if (tx) {
        uint32_t n = 0;
        eth_tx_reclaim(d);                                   /* frames written back are done */
        while (d->tx_tail != d->tx_head) {
            d->tx_tail++;
            n++;
        }
        d->tx_dropped += n;
        if (dropped) *dropped = n;
    }
    ring_init(d, tx, !tx);
    WR(stat_off, ETH_DMA_TX_STAT_ERR_MASK);
    WR(ctrl_off, ETH_DMA_TX_CTRL_START_MASK);
    return ETH_OK;
}

uint32_t eth_irq_take(eth_dev_t *d)
{
    const uint32_t st = RD(ETH_INT_STATUS);
    const uint32_t done = st & (ETH_INT_RAW_TX_DONE_MASK | ETH_INT_RAW_RX_DONE_MASK);
    if (done & ETH_INT_RAW_TX_DONE_MASK) WR(ETH_DMA_TX_STAT, ETH_DMA_TX_STAT_DONE_MASK);
    if (done & ETH_INT_RAW_RX_DONE_MASK) WR(ETH_DMA_RX_STAT, ETH_DMA_RX_STAT_DONE_MASK);
    const uint32_t level = st & ~done;                       /* stays high until the owner acts */
    if (level) {
        d->int_en &= ~level;
        WR(ETH_INT_EN, d->int_en);
    }
    return st;
}

void eth_irq_unmask(eth_dev_t *d, uint32_t bits)
{
    d->int_en |= bits & d->cfg.int_en;
    WR(ETH_INT_EN, d->int_en);
}

/* descriptors from `from` up to, not including, the hardware index `cur` */
static uint32_t passed(eth_dev_t *d, uint32_t from, uint32_t cur, uint32_t limit)
{
    const uint32_t n = d->cfg.ring_len;
    const uint32_t k = (cur % n + n - from % n) % n;
    return k < limit ? k : limit;
}

uint32_t eth_tx_inflight(eth_dev_t *d)
{
    eth_tx_reclaim(d);
    if (d->tx_tail == d->tx_head) return 0;
    return passed(d, d->tx_tail, RD(ETH_DMA_TX_CUR_DESC), d->tx_head - d->tx_tail);
}

uint32_t eth_rx_inflight(eth_dev_t *d)
{
    if (!(r32(d, rxd(d, d->rx_head)) & DES0_OWN)) return 0;   /* a frame is ready now */
    return passed(d, d->rx_head, RD(ETH_DMA_RX_CUR_DESC), d->cfg.ring_len);
}
