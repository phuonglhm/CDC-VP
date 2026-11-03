

#ifndef ETH_DRV_H
#define ETH_DRV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct eth_hal {
    void *ctx;
    uint32_t (*rd)(void *ctx, uint32_t off);                  /* CSR read, offset */
    void (*wr)(void *ctx, uint32_t off, uint32_t v);          /* CSR write */
    void (*mem_wr)(void *ctx, uint32_t addr, const void *src, uint32_t n);   /* DRAM, IP offset */
    void (*mem_rd)(void *ctx, uint32_t addr, void *dst, uint32_t n);
    /* wait for the interrupt line (level) up to `us`; 1 if it is high */
    int (*wait_irq)(void *ctx, uint32_t us);
    void (*delay_us)(void *ctx, uint32_t us);
} eth_hal_t;

typedef struct eth_cfg {
    uint8_t mac[6];
    int speed100;                   /* 1: 100 Mbps, 0: 10 Mbps (PHY configured to match) */
    int phy_addr;                   /* MDIO address of the PHY, -1: leave the PHY alone */
    int phy_loopback;               /* PHY BMCR loopback */
    uint32_t tx_ring, rx_ring;      /* ring bases (16-byte aligned IP offsets) */
    uint32_t tx_buf, rx_buf;        /* buffer areas */
    uint32_t ring_len;              /* descriptors per ring, <= ETH_DRV_MAX_RING */
    uint32_t buf_stride;            /* bytes between buffers, >= 1536 */
    uint32_t rx_lead;               /* RX buffer offset in its slot (2 aligns the IP header) */
    uint32_t coal;                  /* ETH_DMA_*_COAL */
    uint32_t int_en;                /* ETH_INT_EN */
    uint32_t filt_ctrl;             /* ETH_FILT_CTRL */
    uint64_t hash;                  /* multicast hash table */
    /* DMA window: rings and buffers must lie in [dma_base, dma_base + dma_size)
     * of the IP offset space (the platform adds the DRAM base); 0: whole 4 GiB */
    uint32_t dma_base, dma_size;
} eth_cfg_t;

#define ETH_DRV_MAX_RING 256

typedef struct eth_dev {
    eth_hal_t hal;
    eth_cfg_t cfg;
    uint32_t tx_head, tx_tail, rx_head;         /* free-running software indices */
    uint32_t tx_errors, rx_errors;              /* DES0 status seen */
    uint32_t tx_done, rx_done;
    uint32_t int_en;                            /* ETH_INT_EN as last written (shadow) */
    uint32_t tx_dropped;                        /* queued frames abandoned by stop/recover */
    uint32_t tx_unconfirmed;                    /* frames written back (in the TX FIFO) since the
                                                   Tx MAC was last seen idle */
} eth_dev_t;

/* Result of eth_quiesce(). */
typedef struct eth_stop_report {
    int forced;                     /* a second HALT_REQ was needed */
    uint32_t tx_drained;            /* descriptors written back (the frame reached the TX FIFO) */
    uint32_t tx_dropped;            /* queued frames never read (OWN still 1 at HALTED) */
    int tx_fifo_flushed;            /* the TX FIFO was not seen empty before its flush while
                                       frames written back (this call or earlier) were not
                                       yet confirmed sent: some of them may never leave */
    uint32_t elapsed_us;
    uint32_t tx_stat, rx_stat;      /* ETH_DMA_*_STAT when the sequence ended */
    uint32_t status;                /* ETH_STATUS when the sequence ended */
} eth_stop_report_t;

/* State of a direction, taken before its ERR is cleared (MAS 9.3.3). */
typedef struct eth_err_snap {
    uint32_t stat;                  /* ETH_DMA_*_STAT */
    uint32_t errinfo;               /* ETH_DMA_*_ERRINFO: class [3:0], DECERR [4] */
    uint32_t cur_desc;              /* ETH_DMA_*_CUR_DESC */
    int halted;                     /* the error halts the direction (class, or HALTED seen) */
} eth_err_snap_t;

/* ERRINFO class 4 (TX_TOO_LONG, RX_TRUNC) lets the engine run on; every other
 * class halts it (CSR ETH_DMA_*_ERRINFO, MAS Table 6-32). HALTED may follow
 * ERR by the faulting descriptor's write-back and the drain (MAS 9.3.3 steps
 * 1-5), so the class, not HALTED at one instant, tells a halting error. */
#define ETH_ERR_CLASS_SOFT 4u
static inline int eth_err_halting(uint32_t errinfo) { return (errinfo & 0xFu) != ETH_ERR_CLASS_SOFT; }

/* Upper bound eth_recover() waits for HALTED after a halting error. */
#define ETH_DRV_HALT_WAIT_US 2000u

/* return codes */
#define ETH_OK          0
#define ETH_E_STATE    -1       
#define ETH_E_BUILDCFG -2       /* build configuration differs from ETH_DRV_BUILDCFG */
#define ETH_E_MDIO     -3       /* MDIO timeout or no PHY */
#define ETH_E_FULL     -4       /* transmit ring full */
#define ETH_E_TIMEOUT  -5
#define ETH_E_LEN      -6
#define ETH_E_CFG      -7       /* configuration rejected; nothing was written */

#define ETH_DRV_BUILDCFG 0x29AC2405u

int eth_init(eth_dev_t *d, const eth_hal_t *hal, const eth_cfg_t *cfg);
int eth_mdio_read(eth_dev_t *d, unsigned phy, unsigned reg, uint16_t *v);
int eth_mdio_write(eth_dev_t *d, unsigned phy, unsigned reg, uint16_t v);
/* queue one frame (DA..payload, no FCS); rings the doorbell */
int eth_send(eth_dev_t *d, const void *frame, uint32_t len);
/* reclaim written-back transmit descriptors; returns how many */
int eth_tx_reclaim(eth_dev_t *d);
/* one received frame into buf; returns its length, 0 if none, <0 on error;
 * *des0 gets the write-back word (checksum verdict in [25:20]) */
int eth_recv(eth_dev_t *d, void *buf, uint32_t max, uint32_t *des0);
/* read and acknowledge the DMA completion sources; returns ETH_INT_STATUS */
uint32_t eth_irq_ack(eth_dev_t *d);

/* Cold stop, kept for the original examples: eth_quiesce() without a drain.
 * ETH_OK only when both directions are HALTED and both MACs idle. */
int eth_stop(eth_dev_t *d);

/* ---- lifecycle (FX1 plan M3) ------------------------------------------------
 * Single owner: the caller serializes every call on one eth_dev_t, except
 * eth_irq_take(), which an ISR may call while the owner runs if the owner
 * makes its own ETH_INT_EN updates (eth_irq_unmask) with the interrupt
 * masked. */

/* Every rule eth_init() needs, checked with no hardware access: HAL hooks,
 * ring length and 16-byte alignment, buffer stride (rx_lead + 1514 and every
 * transmit frame fit), regions inside the DMA window without 32-bit wrap,
 * rings and buffer areas pairwise disjoint, PHY address, ETH_INT_EN bit 4
 * (TX_BUF_UNAVAIL, poll only) and bit 6 (reserved) clear. */
int eth_cfg_check(const eth_hal_t *hal, const eth_cfg_t *cfg);

/* Stop both directions (MAS 9.3.4, Figure 9-7 and "Resetting one direction"):
 * drain for up to drain_us: every queued descriptor written back, then the
 * Tx MAC idle on two samples further apart than the inter-packet gap (a
 * write-back only means the frame entered the TX FIFO); HALT_REQ both directions and
 * wait up to halt_us for HALTED, then a second HALT_REQ (forced abort) and
 * another halt_us; clear TX_EN and RX_EN only after HALTED, wait for TX_BUSY
 * and RX_BUSY 0, flush both FIFOs (ETH_CTRL[5], [6]), mask ETH_INT_EN and
 * clear DONE. Before the flush, frames written back but not confirmed sent
 * (tx_unconfirmed, whenever they were reclaimed) are checked once more: with
 * the DMA halted and TX_EN still 1, the Tx MAC idle on two samples 20 us apart
 * means the FIFO is empty; otherwise r->tx_fifo_flushed reports that some of
 * them may have been discarded (their number is not observable).
 * ETH_OK: quiescent, every descriptor back with software.
 * ETH_E_TIMEOUT: not quiescent (a direction not HALTED or a MAC still busy);
 * the MAC enables are left as they were when HALTED was not reached, and the
 * rings still belong to the hardware. r may be NULL. */
int eth_quiesce(eth_dev_t *d, uint32_t drain_us, uint32_t halt_us, eth_stop_report_t *r);

/* Start again after eth_quiesce() returned ETH_OK: SRST both directions,
 * rebuild both rings, restart the software indices, clear DONE and ERR,
 * restore ETH_INT_EN, START. ETH_E_STATE if a direction is not HALTED. */
int eth_restart(eth_dev_t *d);

/* MAS 9.3.3 / Figure 9-6 for one direction (tx 1: transmit). Snapshot first.
 * The class decides (eth_err_halting), not HALTED at that instant. A
 * non-halting error (class 4) with HALTED 0: clear ERR only; if HALTED then
 * reads 1 (a halting error later in the same episode keeps class 4 latched)
 * the halting branch follows. A halting one: wait up to ETH_DRV_HALT_WAIT_US
 * for HALTED (ETH_E_TIMEOUT if it does not come: nothing touched, ERR stays
 * set, the ring still belongs to the DMA), then SRST, rebuild that ring,
 * restart its indices, clear ERR, START; the other direction keeps running.
 * *dropped: transmit frames abandoned.
 * A non-halting return is no proof for the rest of the episode: ERRINFO keeps
 * the first error, so a halting error that came before the clear shows later
 * only as HALTED, with no new ERR or interrupt. After a class-4 clear the
 * owner keeps checking HALTED until the direction has visibly moved on (the
 * FX1 manager's error watch, fw/fx1_soc/drivers/fx1_eth.c). */
int eth_recover(eth_dev_t *d, int tx, eth_err_snap_t *snap, uint32_t *dropped);

/* ISR half: read ETH_INT_STATUS, clear TX/RX DONE (before the descriptors
 * are processed, MAS 9.3), and mask every other active level source (ERR,
 * RX_BUF_UNAVAIL, MDIO_DONE) in ETH_INT_EN so the line falls before the PLIC
 * completion. Returns the status read. Never waits. */
uint32_t eth_irq_take(eth_dev_t *d);
/* Owner half: re-enable `bits` of cfg.int_en that eth_irq_take() masked. */
void eth_irq_unmask(eth_dev_t *d, uint32_t bits);

/* MAS 9.3 / review F08: descriptors the DMA has passed (before
 * ETH_DMA_*_CUR_DESC in ring order) whose OWN still reads 1: their DES0
 * write-back is in flight after DONE. Poll them, do not wait for an
 * interrupt that will not come. A full transmit ring with CUR_DESC at the
 * tail reads 0 (ambiguous: the caller's deadline covers it). */
uint32_t eth_tx_inflight(eth_dev_t *d);
uint32_t eth_rx_inflight(eth_dev_t *d);

#ifdef __cplusplus
}
#endif

#endif /* ETH_DRV_H */
