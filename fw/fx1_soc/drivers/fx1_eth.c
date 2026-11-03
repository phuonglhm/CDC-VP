/*
 * FX1 Ethernet manager: HAL, DMA memory, lifecycle and service of the
 * portable Ethernet driver on fx1_soc (fx1_eth.h).
 */
#include "fx1_eth.h"

#include "eth_regs.h"
#include "fx1_fw.h"

/* ---- DMA memory (.eth_dma, fx1.ld) ------------------------------------------ */
typedef struct {
    uint8_t tx_ring[16u * FX1_ETH_RING];
    uint8_t rx_ring[16u * FX1_ETH_RING];
    uint8_t pad[4096u - 32u * FX1_ETH_RING];
    uint8_t tx_buf[FX1_ETH_STRIDE * FX1_ETH_RING];
    uint8_t rx_buf[FX1_ETH_STRIDE * FX1_ETH_RING];
} eth_pool;
static eth_pool pool __attribute__((section(".eth_dma"), aligned(4096)));

/* ---- HAL --------------------------------------------------------------------- */
static void (*hal_yield)(void);
static uint32_t hal_range_errors;
static uint32_t hal_waited_us;

uint32_t fx1_eth_offset(const volatile void* cpu) { return (uint32_t)(uintptr_t)cpu - FX1_DDR_BASE; }
void* fx1_eth_cpu(uint32_t offset) { return (void*)(uintptr_t)(FX1_DDR_BASE + offset); }

/* [offset, offset + n) inside the DDR aperture, in 64-bit arithmetic */
static int in_ddr(uint32_t offset, uint32_t n)
{
    return (uint64_t)offset + n <= (uint64_t)FX1_DDR_SIZE;
}

static uint32_t hal_rd(void* ctx, uint32_t off)
{
    (void)ctx;
    return fx1_read32(FX1_ETH_CSR_BASE + (off & 0xFFFCu));
}

static void hal_wr(void* ctx, uint32_t off, uint32_t v)
{
    (void)ctx;
    fx1_write32(FX1_ETH_CSR_BASE + (off & 0xFFFCu), v);
}

/* CPU writes to memory the DMA will read: the data, then a fence, so a later
 * OWN=1 store is ordered after it (fx1_dma_prepare). */
static void hal_mem_wr(void* ctx, uint32_t off, const void* src, uint32_t n)
{
    (void)ctx;
    if (!in_ddr(off, n)) {
        hal_range_errors++;
        return;
    }
    volatile uint8_t* d = (volatile uint8_t*)fx1_eth_cpu(off);
    const uint8_t* s = (const uint8_t*)src;
    if (n == 4 && !(off & 3u) && !((uintptr_t)src & 3u))
        *(volatile uint32_t*)d = *(const uint32_t*)src;
    else
        for (uint32_t i = 0; i < n; i++) d[i] = s[i];
    fx1_dma_prepare(d, n, FX1_DMA_TO_DEVICE);
}

/* CPU reads of memory the DMA wrote (fx1_dma_complete first). */
static void hal_mem_rd(void* ctx, uint32_t off, void* dst, uint32_t n)
{
    (void)ctx;
    uint8_t* d = (uint8_t*)dst;
    if (!in_ddr(off, n)) {
        hal_range_errors++;
        for (uint32_t i = 0; i < n; i++) d[i] = 0xFF;    /* reads as hardware-owned */
        return;
    }
    const volatile uint8_t* s = (const volatile uint8_t*)fx1_eth_cpu(off);
    fx1_dma_complete(s, n, FX1_DMA_FROM_DEVICE);
    if (n == 4 && !(off & 3u) && !((uintptr_t)dst & 3u))
        *(uint32_t*)dst = *(const volatile uint32_t*)s;
    else
        for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

static int hal_wait_irq(void* ctx, uint32_t us)
{
    (void)ctx;
    (void)us;
    return 0;                                         /* the service polls; no blocking wait */
}

static void hal_delay_us(void* ctx, uint32_t us)
{
    (void)ctx;
    const uint64_t end = fx1_mtime() + (uint64_t)us * (FX1_MTIME_HZ / 1000000u);
    while (fx1_mtime() < end) {
    }
    hal_waited_us += us;
    if (hal_waited_us >= 1000u) {
        hal_waited_us = 0;
        if (hal_yield) hal_yield();
    }
}

static const eth_hal_t mmio_hal = {0, hal_rd, hal_wr, hal_mem_wr, hal_mem_rd, hal_wait_irq, hal_delay_us};

const eth_hal_t* fx1_eth_hal_mmio(void) { return &mmio_hal; }
void fx1_eth_hal_set_yield(void (*yield)(void)) { hal_yield = yield; }
uint32_t fx1_eth_hal_range_errors(void) { return hal_range_errors; }

/* ---- names ------------------------------------------------------------------- */
const char* fx1_eth_state_name(fx1_eth_state s)
{
    switch (s) {
    case FX1_ETH_UNINIT: return "uninitialized";
    case FX1_ETH_STOPPED: return "stopped";
    case FX1_ETH_RUNNING: return "running";
    case FX1_ETH_STOPPING: return "stopping";
    case FX1_ETH_FAULTED: return "FAULTED";
    }
    return "?";
}

const char* fx1_eth_rc_name(int rc)
{
    switch (rc) {
    case ETH_OK: return "OK";
    case ETH_E_STATE: return "ESTATE";
    case ETH_E_BUILDCFG: return "EBUILDCFG";
    case ETH_E_MDIO: return "EMDIO";
    case ETH_E_FULL: return "EFULL";
    case ETH_E_TIMEOUT: return "ETIMEOUT";
    case ETH_E_LEN: return "ELEN";
    case ETH_E_CFG: return "ECFG";
    }
    return "E?";
}

/* ---- lifecycle --------------------------------------------------------------- */
#define ETH_INT_USED (ETH_INT_RAW_TX_DONE_MASK | ETH_INT_RAW_RX_DONE_MASK | ETH_INT_RAW_TX_ERR_MASK | \
                      ETH_INT_RAW_RX_ERR_MASK)
static const uint8_t board_mac[6] = {0x02, 0x00, 0x46, 0x58, 0x31, 0x01};    /* locally administered */

/* every access of the manager through its HAL (e in scope) */
#define RD(off) e->hal.rd(e->hal.ctx, (off))
#define WR(off, v) e->hal.wr(e->hal.ctx, (off), (v))

void fx1_eth_setup(fx1_eth* e)
{
    uint8_t* p = (uint8_t*)e;
    for (uint32_t i = 0; i < sizeof *e; i++) p[i] = 0;
    e->state = FX1_ETH_UNINIT;
    e->speed100 = 1;
    e->hal = mmio_hal;
    e->dev.hal = mmio_hal;
    eth_cfg_t* c = &e->cfg;
    for (int i = 0; i < 6; i++) c->mac[i] = board_mac[i];
    c->phy_addr = (int)FX1_ETH_PHY;
    c->tx_ring = fx1_eth_offset(pool.tx_ring);
    c->rx_ring = fx1_eth_offset(pool.rx_ring);
    c->tx_buf = fx1_eth_offset(pool.tx_buf);
    c->rx_buf = fx1_eth_offset(pool.rx_buf);
    c->ring_len = FX1_ETH_RING;
    c->buf_stride = FX1_ETH_STRIDE;
    c->rx_lead = 2;
    c->coal = 0;
    c->int_en = ETH_INT_USED;
    c->filt_ctrl = ETH_FILT_CTRL_BCAST_ACCEPT_MASK;
    c->dma_base = 0;
    c->dma_size = FX1_DDR_SIZE;
}

int fx1_eth_probe(fx1_eth* e, fx1_eth_probe_info* info)
{
    fx1_eth_probe_info local;
    if (!info) info = &local;
    uint8_t* p = (uint8_t*)info;
    for (uint32_t i = 0; i < sizeof *info; i++) p[i] = 0;
    info->id = RD(ETH_ID);
    info->cap = RD(ETH_CAP);
    info->buildcfg = RD(ETH_DMA_BUILDCFG);
    info->mac_ok = info->id == 0x45544801u && info->buildcfg == ETH_DRV_BUILDCFG;
    if (!info->mac_ok) return ETH_E_BUILDCFG;
    const uint32_t mdio_up = ETH_CTRL_RST_N_MASK | ETH_CTRL_MDIO_RST_N_MASK;
    const uint32_t ctrl = RD(ETH_CTRL);
    if ((ctrl & mdio_up) != mdio_up) WR(ETH_CTRL, ctrl | mdio_up);   /* only the MDIO block */
    eth_dev_t* d = &e->dev;
    d->hal = e->hal;
    if (eth_mdio_read(d, FX1_ETH_PHY, 2, &info->phy_id1) != ETH_OK) return ETH_E_MDIO;
    info->phy_ok = 1;
    if (eth_mdio_read(d, FX1_ETH_PHY, 3, &info->phy_id2) || eth_mdio_read(d, FX1_ETH_PHY, 0, &info->bmcr) ||
        eth_mdio_read(d, FX1_ETH_PHY, 1, &info->bmsr))
        return ETH_E_MDIO;
    info->link = (info->bmsr & 0x0004u) != 0;
    info->speed100 = (info->bmcr & 0x1000u) || (info->bmcr & 0x2000u);   /* AN resolves 100FD */
    return ETH_OK;
}

static void unmask_all(fx1_eth* e)
{
    e->err_halted = 0;
    e->err_masked = 0;
    e->pending = 0;
    e->inflight_since = 0;
    e->wb_late = 0;
    e->err_watch = 0;
}

int fx1_eth_start(fx1_eth* e)
{
    int rc;
    switch (e->state) {
    case FX1_ETH_RUNNING:
        return ETH_OK;
    case FX1_ETH_FAULTED:
    case FX1_ETH_STOPPING:
        return ETH_E_STATE;
    case FX1_ETH_UNINIT:
        e->cfg.speed100 = e->speed100;
        WR(RMON_CTRL, RMON_CTRL_RMON_EN_MASK);           /* no READ_CLEAR_EN, no FREEZE */
        rc = eth_init(&e->dev, &e->hal, &e->cfg);
        break;
    case FX1_ETH_STOPPED:
    default:
        rc = eth_restart(&e->dev);
        break;
    }
    if (rc != ETH_OK) return rc;
    WR(RMON_CTRL, RMON_CTRL_RMON_EN_MASK);
    unmask_all(e);
    e->state = FX1_ETH_RUNNING;
    e->c.starts++;
    return ETH_OK;
}

int fx1_eth_stop(fx1_eth* e, uint32_t drain_us)
{
    if (e->state != FX1_ETH_RUNNING) return e->state == FX1_ETH_FAULTED ? ETH_E_STATE : ETH_OK;
    e->state = FX1_ETH_STOPPING;                     /* no more submissions from here */
    const int rc = eth_quiesce(&e->dev, drain_us, 200, &e->last_stop);
    e->last_stop_rc = rc;
    e->c.stops++;
    if (e->last_stop.forced) e->c.forced_stops++;
    e->c.tx_dropped += e->last_stop.tx_dropped;
    if (e->last_stop.tx_fifo_flushed) e->c.tx_fifo_flushes++;
    if (rc != ETH_OK) {
        e->c.failed_stops++;
        e->state = FX1_ETH_FAULTED;                  /* the DMA may still own the rings */
        return rc;
    }
    unmask_all(e);
    e->state = FX1_ETH_STOPPED;
    return ETH_OK;
}

/* ETH_INT_EN is shared with fx1_eth_isr(): its read-modify-write runs with
 * interrupts masked on this hart. */
static void unmask_irq(fx1_eth* e, uint32_t bit)
{
    const uint32_t saved = fx1_irq_save();
    eth_irq_unmask(&e->dev, bit);
    fx1_irq_restore(saved);
}

/* MAS 9.3.3 for one direction. eth_recover() waits for HALTED first; when it
 * does not come the ring still belongs to the DMA: FAULTED, recovered only by
 * fx1_eth_recover() from there (review ER01). */
static int recover_dir(fx1_eth* e, int tx)
{
    const uint32_t dir = 1u << tx;
    uint32_t dropped = 0;
    eth_err_snap_t s;
    const int rc = eth_recover(&e->dev, tx, &s, &dropped);
    if (!(e->err_halted & dir)) e->last_err[tx] = s;   /* else keep the snapshot taken at the error */
    if (rc != ETH_OK) {
        e->c.recover_timeouts++;
        e->err_halted |= dir;
        e->state = FX1_ETH_FAULTED;
        return rc;
    }
    e->c.tx_dropped += dropped;
    if (s.halted) e->c.recoveries++;
    else if (s.stat & ETH_DMA_TX_STAT_ERR_MASK) e->c.soft_errors++;
    e->err_halted &= ~dir;
    const uint32_t bit = tx ? ETH_INT_RAW_TX_ERR_MASK : ETH_INT_RAW_RX_ERR_MASK;
    e->err_masked &= ~bit;
    unmask_irq(e, bit);
    return ETH_OK;
}

int fx1_eth_recover(fx1_eth* e)
{
    if (e->state == FX1_ETH_RUNNING) {
        for (int tx = 0; tx < 2; tx++) {
            const uint32_t stat = RD(tx ? ETH_DMA_TX_STAT : ETH_DMA_RX_STAT);
            if ((e->err_halted & (1u << tx)) || (stat & (ETH_DMA_TX_STAT_HALTED_MASK | ETH_DMA_TX_STAT_ERR_MASK))) {
                const int rc = recover_dir(e, tx);
                if (rc != ETH_OK) return rc;
            }
        }
        return ETH_OK;
    }
    if (e->state != FX1_ETH_FAULTED) return ETH_OK;
    /* a stop that did not reach quiescence: try again, longer. HALTED on both
     * directions means every issued AXI transaction was answered (the drain),
     * so no write of the old session can land in the rings any more. There
     * is no other way out: clearing ETH_CTRL[0] needs both directions HALTED
     * first (MAS 9.1, Table 6-2 row 3), and a write already issued would
     * still land after a cold init rebuilt the rings (review N2). Until then
     * the device stays FAULTED and its memory untouched. */
    const int rc = eth_quiesce(&e->dev, 0, 5000, &e->last_stop);
    e->last_stop_rc = rc;
    if (rc != ETH_OK) {
        e->c.recover_timeouts++;
        return rc;
    }
    e->state = FX1_ETH_STOPPED;
    e->c.recoveries++;
    return fx1_eth_start(e);
}

static void service_error(fx1_eth* e, int tx);

int fx1_eth_send(fx1_eth* e, const void* frame, uint32_t len)
{
    if (e->state != FX1_ETH_RUNNING) return ETH_E_STATE;
    if (e->err_halted & 2u) return ETH_E_STATE;
    /* a TX halt no ERR announced (a halting error behind a class-4 one,
     * review ER01 second round): one CSR read per frame */
    if (RD(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_HALTED_MASK) {
        service_error(e, 1);
        if ((e->err_halted & 2u) || e->state != FX1_ETH_RUNNING) return ETH_E_STATE;
    }
    const int rc = eth_send(&e->dev, frame, len);
    if (rc == ETH_OK) e->c.tx_frames++;
    else if (rc == ETH_E_FULL) e->c.tx_full++;
    return rc;
}

uint32_t fx1_eth_isr(fx1_eth* e)
{
    const uint32_t st = eth_irq_take(&e->dev);
    e->c.irqs++;
    if (st & (ETH_INT_RAW_TX_ERR_MASK | ETH_INT_RAW_RX_ERR_MASK)) e->c.irq_errors++;
    __atomic_fetch_or(&e->pending, st, __ATOMIC_RELAXED);
    return st;
}

/* A DMA error of one direction (review ER01). The ERRINFO class tells a
 * halting error; HALTED may come later, after the faulting descriptor's
 * write-back and the drain (MAS 9.3.3), so submissions stop at once. HALTED
 * without ERR is a halting error whose class stayed hidden behind the first
 * error of the episode: ERRINFO latches only while ERR is 0, so a halting
 * error before the clear of a class-4 one raises no new ERR and no
 * interrupt. After such a clear the direction is watched (err_watch) until
 * it shows it moved on, HALTED or not (fx1_eth_service). */
static void service_error(fx1_eth* e, int tx)
{
    const uint32_t dir = 1u << tx;
    const uint32_t bit = tx ? ETH_INT_RAW_TX_ERR_MASK : ETH_INT_RAW_RX_ERR_MASK;
    const uint32_t stat_off = tx ? ETH_DMA_TX_STAT : ETH_DMA_RX_STAT;
    if (!(e->err_halted & dir)) {
        uint32_t stat = RD(stat_off);
        if (!(stat & (ETH_DMA_TX_STAT_ERR_MASK | ETH_DMA_TX_STAT_HALTED_MASK))) {
            unmask_irq(e, bit);
            return;
        }
        eth_err_snap_t s = {stat, RD(tx ? ETH_DMA_TX_ERRINFO : ETH_DMA_RX_ERRINFO),
                            RD(tx ? ETH_DMA_TX_CUR_DESC : ETH_DMA_RX_CUR_DESC), 1};
        if (!(stat & ETH_DMA_TX_STAT_HALTED_MASK) && !eth_err_halting(s.errinfo)) {
            /* TX_TOO_LONG / RX_TRUNC: snapshot, clear, carry on (Table 9-6) */
            s.halted = 0;
            e->last_err[tx] = s;
            WR(stat_off, ETH_DMA_TX_STAT_ERR_MASK);
            e->c.soft_errors++;
            if (!(RD(stat_off) & ETH_DMA_TX_STAT_HALTED_MASK)) {
                e->err_watch |= dir;
                e->watch_since[tx] = fx1_mtime();
                unmask_irq(e, bit);
                return;
            }
            s.halted = 1;
        }
        e->err_watch &= ~dir;
        e->err_halted |= dir;                        /* fx1_eth_send refuses TX from here */
        if (tx) e->c.tx_halts++;
        else e->c.rx_halts++;
        e->last_err[tx] = s;
    }
    e->err_masked |= bit;                            /* stays masked until recovered */
    if (e->auto_recover) recover_dir(e, tx);
}

int fx1_eth_service(fx1_eth* e)
{
    if (e->state != FX1_ETH_RUNNING) return 0;
    const uint32_t st = __atomic_exchange_n(&e->pending, 0u, __ATOMIC_RELAXED);
    const uint64_t now0 = fx1_mtime();
    for (int tx = 1; tx >= 0; tx--) {
        const uint32_t dir = 1u << tx;
        const uint32_t bit = tx ? ETH_INT_RAW_TX_ERR_MASK : ETH_INT_RAW_RX_ERR_MASK;
        if (e->err_halted & dir) {
            if (st & bit) service_error(e, tx);
            continue;
        }
        /* every pass: HALTED that no ERR announced */
        const uint32_t stat = RD(tx ? ETH_DMA_TX_STAT : ETH_DMA_RX_STAT);
        if ((st & bit) || (stat & ETH_DMA_TX_STAT_HALTED_MASK)) {
            service_error(e, tx);
            continue;
        }
        /* the watch after a class-4 clear ends when the direction moved on:
         * TX suspended (it fetched past every queued descriptor), or after
         * FX1_ETH_WATCH_MS; the idle poll below still checks HALTED */
        if ((e->err_watch & dir) && ((tx && (stat & ETH_DMA_TX_STAT_SUSPENDED_MASK)) ||
                                     now0 - e->watch_since[tx] > (uint64_t)FX1_MTIME_HZ / 1000u * FX1_ETH_WATCH_MS))
            e->err_watch &= ~dir;
    }
    if (e->state != FX1_ETH_RUNNING) return 0;       /* HALTED never came: FAULTED */
    /* receive everything the DMA has returned; the counters advance by what
     * this pass saw (review ER04: they share the epoch of fx1_eth_stats_clear) */
    const uint32_t rx_err0 = e->dev.rx_errors, tx_err0 = e->dev.tx_errors;
    uint32_t des0, progress = 0;
    int n;
    while ((n = eth_recv(&e->dev, e->rx_buf, sizeof e->rx_buf, &des0)) != 0) {
        progress++;
        if (n < 0) continue;                         /* RX_DATA_WR_ERR: no data */
        e->c.rx_frames++;
        if (des0 & 0x00010000u) e->c.rx_trunc++;     /* a prefix of the frame only */
        if (e->rx_cb) e->rx_cb(e->rx_ctx, e->rx_buf, (uint32_t)n, des0);
    }
    progress += (uint32_t)eth_tx_reclaim(&e->dev);
    e->c.rx_desc_errors += e->dev.rx_errors - rx_err0;
    e->c.tx_desc_errors += e->dev.tx_errors - tx_err0;
    /* DONE may precede the DES0 write-back (MAS 9.3, review F08): the
     * descriptors in flight commit with no further interrupt, so the caller
     * runs this again until they do (review ER02), never idle meanwhile */
    /* not on a direction halted by an error: CUR_DESC stays at the faulting
     * descriptor, behind what software took, and the recovery waits for
     * HALTED (the end of that descriptor's write-back) itself */
    const uint32_t inflight = ((e->err_halted & 1u) ? 0 : eth_rx_inflight(&e->dev)) +
                              ((e->err_halted & 2u) ? 0 : eth_tx_inflight(&e->dev));
    if (!inflight) {
        e->inflight_since = 0;
        e->wb_late = 0;
        if (e->err_watch) {                          /* a class-4 clear under watch */
            uint64_t since = e->watch_since[(e->err_watch & 1u) ? 0 : 1];
            if ((e->err_watch & 3u) == 3u && e->watch_since[1] < since) since = e->watch_since[1];
            return now0 - since <= FX1_MTIME_HZ / 500u ? (int)FX1_ETH_POLL_FAST_MS : (int)FX1_ETH_POLL_SLOW_MS;
        }
        return (int)FX1_ETH_POLL_IDLE_MS;
    }
    const uint64_t now = fx1_mtime();
    if (!e->inflight_since || progress) {            /* deadline from the last progress */
        if (!e->inflight_since) e->c.wb_inflight++;
        e->inflight_since = now;
        e->wb_late = 0;
    }
    if (now - e->inflight_since <= FX1_MTIME_HZ / 500u) return (int)FX1_ETH_POLL_FAST_MS;
    if (!e->wb_late) {
        e->wb_late = 1;
        e->c.wb_timeouts++;
    }
    return (int)FX1_ETH_POLL_SLOW_MS;
}

int fx1_eth_mdio_read(fx1_eth* e, unsigned phy, unsigned reg, uint16_t* v)
{
    if (phy > 31u || reg > 31u || !v) return ETH_E_CFG;
    e->dev.hal = e->hal;
    return eth_mdio_read(&e->dev, phy, reg, v);
}

int fx1_eth_mdio_write(fx1_eth* e, unsigned phy, unsigned reg, uint16_t v)
{
    if (phy > 31u || reg > 31u) return ETH_E_CFG;
    if (reg == 0 && (e->state == FX1_ETH_RUNNING || e->state == FX1_ETH_STOPPING)) return ETH_E_STATE;
    e->dev.hal = e->hal;
    return eth_mdio_write(&e->dev, phy, reg, v);
}

int fx1_eth_set_speed(fx1_eth* e, int speed100)
{
    if (e->state != FX1_ETH_STOPPED && e->state != FX1_ETH_UNINIT) return ETH_E_STATE;
    e->speed100 = speed100 ? 1 : 0;
    e->cfg.speed100 = e->speed100;
    e->dev.cfg.speed100 = e->speed100;
    if (e->state == FX1_ETH_STOPPED) {
        /* both MACs are disabled and idle: the RMII_CFG window is open (Table 9-7) */
        WR(ETH_RMII_CFG, e->speed100 ? ETH_RMII_CFG_SPEED_MASK : 0);
        const uint16_t bmcr = e->speed100 ? 0x3100u : 0x0100u;    /* AN+100 or 10FD */
        return fx1_eth_mdio_write(e, FX1_ETH_PHY, 0, bmcr);
    }
    return ETH_OK;
}

void fx1_eth_hw_stats_read(fx1_eth* e, fx1_eth_hw_stats* s)
{
    s->tx_pkts = RD(RMON_TX_PKTS);
    s->tx_octets = RD(RMON_TX_OCTETS);
    s->tx_oversize = RD(RMON_TX_OVERSIZE);
    s->tx_drop = RD(RMON_TX_DROP);
    s->rx_pkts = RD(RMON_RX_PKTS);
    s->rx_octets = RD(RMON_RX_OCTETS);
    s->rx_crc = RD(RMON_RX_CRC_ALIGN_ERR);
    s->rx_undersize = RD(RMON_RX_UNDERSIZE);
    s->rx_oversize = RD(RMON_RX_OVERSIZE);
    s->rx_filter_drop = RD(RMON_RX_FILTER_DROP);
    s->rx_drop_events = RD(RMON_RX_DROP_EVENTS);
    s->dma_rx_drop_cnt = RD(ETH_DMA_RX_DROP_CNT);
    s->fifo_stat = RD(ETH_FIFO_STAT);
    s->rmon_ovf_tx = RD(RMON_OVF_STAT_TX);
    s->rmon_ovf_rx = RD(RMON_OVF_STAT_RX);
}

void fx1_eth_stats_clear(fx1_eth* e)
{
    WR(RMON_CTRL, RMON_CTRL_RMON_EN_MASK | RMON_CTRL_CLEAR_ALL_MASK);
    WR(ETH_DMA_RX_DROP_CNT, 0);
    WR(ETH_FIFO_STAT, 0x3Fu);
    WR(RMON_OVF_STAT_TX, 0xFFFFFFFFu);
    WR(RMON_OVF_STAT_RX, 0xFFFFFFFFu);
    /* every manager counter counts from here (no absolute driver total is
     * copied into one, review ER04); the ISR's own counters too, so with it
     * masked on this hart */
    const uint32_t saved = fx1_irq_save();
    uint8_t* p = (uint8_t*)&e->c;
    for (uint32_t i = 0; i < sizeof e->c; i++) p[i] = 0;
    fx1_irq_restore(saved);
}

/* ---- selftest ---------------------------------------------------------------- */
typedef struct {
    const uint8_t* want;
    uint32_t len;
    int seen, bad;
} st_rx;

static void st_cb(void* ctx, const uint8_t* f, uint32_t len, uint32_t des0)
{
    st_rx* r = (st_rx*)ctx;
    r->seen++;
    int ok = len == r->len && !(des0 & 0x00070000u);
    for (uint32_t i = 0; ok && i < len; i++) ok = f[i] == r->want[i];
    if (!ok) r->bad++;
}

static uint8_t st_frame[1514];

int fx1_eth_selftest(fx1_eth* e, int speed100, uint32_t timeout_us)
{
    static const uint32_t sizes[] = {60, 61, 512, 1514};
    const fx1_eth_state before = e->state;
    const int speed_before = e->speed100;
    if (before == FX1_ETH_FAULTED || before == FX1_ETH_STOPPING) return ETH_E_STATE;
    int rc = fx1_eth_stop(e, 2000);
    if (rc) return rc;
    if (e->state == FX1_ETH_UNINIT) {
        /* cold init first, then stop: the configuration is in place */
        if ((rc = fx1_eth_start(e)) || (rc = fx1_eth_stop(e, 0))) return rc;
    }
    if ((rc = fx1_eth_set_speed(e, speed100))) return rc;
    const uint16_t bmcr = (speed100 ? 0x3100u : 0x0100u) | 0x4000u;        /* PHY loopback */
    if ((rc = fx1_eth_mdio_write(e, FX1_ETH_PHY, 0, bmcr))) return rc;
    if ((rc = fx1_eth_start(e))) return rc;
    const fx1_eth_rx_cb cb0 = e->rx_cb;
    void* ctx0 = e->rx_ctx;
    st_rx r = {st_frame, 0, 0, 0};
    e->rx_cb = st_cb;
    e->rx_ctx = &r;
    int failed = 0;
    for (uint32_t k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        const uint32_t len = sizes[k];
        for (uint32_t i = 0; i < len; i++) st_frame[i] = (uint8_t)(i * 7u + k * 29u + 3u);
        for (int i = 0; i < 6; i++) {
            st_frame[i] = e->cfg.mac[i];                 /* to ourselves */
            st_frame[6 + i] = e->cfg.mac[i];
        }
        st_frame[12] = 0x88;
        st_frame[13] = 0xB5;
        r.len = len;
        const int seen0 = r.seen, bad0 = r.bad;
        if (fx1_eth_send(e, st_frame, len) != ETH_OK) {
            failed++;
            continue;
        }
        const uint64_t end = fx1_mtime() + (uint64_t)timeout_us * (FX1_MTIME_HZ / 1000000u);
        while (r.seen == seen0 && fx1_mtime() < end) {
            fx1_eth_service(e);
            e->hal.delay_us(e->hal.ctx, 5);
        }
        if (r.seen != seen0 + 1 || r.bad != bad0) failed++;
    }
    e->rx_cb = cb0;
    e->rx_ctx = ctx0;
    /* back to the line, the previous speed and state */
    if ((rc = fx1_eth_stop(e, 2000))) return rc;
    if ((rc = fx1_eth_set_speed(e, speed_before))) return rc;
    if (before == FX1_ETH_RUNNING && (rc = fx1_eth_start(e))) return rc;
    return failed;
}
