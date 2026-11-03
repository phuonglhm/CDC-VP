/*
 * FX1 Ethernet manager (plan M3/M4): the portable driver of
 * components/FX1_Components/ethernet/driver (eth_drv.h) on fx1_soc, with
 *
 *   - its MMIO HAL: CSR at FX1_ETH_CSR_BASE + offset (aligned 32-bit only),
 *     DMA memory addressed by IP offset = CPU address - FX1_DDR_BASE, every
 *     range checked in 64-bit arithmetic against the DDR aperture;
 *   - its DMA memory: rings and buffers in the linker section .eth_dma (fx1.ld),
 *     4 KiB aligned, nothing else of the image shares a reservation granule;
 *   - a lifecycle: UNINIT -> RUNNING <-> STOPPED, STOPPING in between, FAULTED
 *     when a stop cannot reach quiescence (the DMA may still own buffers: no
 *     restart, no buffer reuse, until fx1_eth_recover());
 *   - the interrupt split of MAS 9.3: fx1_eth_isr() (never waits) and
 *     fx1_eth_service() (the owner), which also polls descriptors whose DES0
 *     write-back is still in flight after DONE (review F08) and tells its
 *     caller when to run it again (review ER02);
 *   - DMA errors classified by ERRINFO class, not by HALTED at one instant
 *     (review ER01): a halting error stops submissions on that direction at
 *     once, HALTED may follow later.
 *
 * No RTOS dependency. One owner calls everything except fx1_eth_isr(); the
 * FreeRTOS service task is that owner (freertos/cmd_eth.c). MDIO completion is
 * polled (MDIO_DONE stays masked); ETH_INT_EN bit 4 stays 0; RMON counters are
 * read without READ_CLEAR_EN, so a read never clears them.
 */
#ifndef FX1_ETH_H
#define FX1_ETH_H

#include <stdint.h>

#include "eth_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FX1_ETH_RING   16u
#define FX1_ETH_STRIDE 2048u      /* rx_lead 2 + 1514 fits */
#define FX1_ETH_PHY    1u         /* MDIO address of the board PHY */

typedef enum {
    FX1_ETH_UNINIT = 0,
    FX1_ETH_STOPPED,
    FX1_ETH_RUNNING,
    FX1_ETH_STOPPING,
    FX1_ETH_FAULTED,
} fx1_eth_state;

const char* fx1_eth_state_name(fx1_eth_state s);
const char* fx1_eth_rc_name(int rc);

typedef struct fx1_eth_counters {
    uint32_t tx_frames, rx_frames;          /* frames handed to / taken from the rings */
    uint32_t tx_desc_errors, rx_desc_errors;/* DES0[18:16] seen */
    uint32_t rx_trunc;                      /* RX_TRUNC: the frame did not fit, prefix only */
    uint32_t tx_dropped;                    /* abandoned by stop or recovery */
    uint32_t tx_full;                       /* send refused, ring full */
    uint32_t irqs, irq_errors;              /* ISR entries, ERR bits taken */
    uint32_t tx_halts, rx_halts;            /* halting DMA errors */
    uint32_t soft_errors;                   /* TX_TOO_LONG / RX_TRUNC (no halt) */
    uint32_t recoveries;
    uint32_t wb_inflight;                   /* services that found a write-back in flight */
    uint32_t wb_timeouts;                   /* ... still in flight after 2 ms */
    uint32_t starts, stops, forced_stops, failed_stops;
    uint32_t tx_fifo_flushes;               /* stops that flushed written-back frames not seen sent */
    uint32_t recover_timeouts;              /* HALTED not reached by a recovery: FAULTED (stays) */
    uint32_t hal_range_errors;              /* HAL refused an access outside DDR */
} fx1_eth_counters;

typedef struct fx1_eth_hw_stats {
    uint32_t tx_pkts, tx_octets, tx_oversize, tx_drop;
    uint32_t rx_pkts, rx_octets, rx_crc, rx_undersize, rx_oversize, rx_filter_drop, rx_drop_events;
    uint32_t dma_rx_drop_cnt;               /* ETH_DMA_RX_DROP_CNT: no descriptor */
    uint32_t fifo_stat;                     /* ETH_FIFO_STAT */
    uint32_t rmon_ovf_tx, rmon_ovf_rx;
} fx1_eth_hw_stats;

typedef struct fx1_eth_probe_info {
    uint32_t id, cap, buildcfg;
    int mac_ok;                             /* ID and BUILDCFG as the driver expects */
    int phy_ok;                             /* the PHY answered at FX1_ETH_PHY */
    uint16_t phy_id1, phy_id2, bmcr, bmsr;
    int link, speed100;                     /* from BMSR / BMCR */
} fx1_eth_probe_info;

typedef void (*fx1_eth_rx_cb)(void* ctx, const uint8_t* frame, uint32_t len, uint32_t des0);

typedef struct fx1_eth {
    eth_hal_t hal;                          /* fx1_eth_hal_mmio() (a host test substitutes its own) */
    eth_dev_t dev;
    eth_cfg_t cfg;
    fx1_eth_state state;
    int speed100;
    int auto_recover;                       /* service() recovers halting errors itself */
    volatile uint32_t pending;              /* ETH_INT_STATUS bits from the ISR */
    fx1_eth_counters c;
    eth_stop_report_t last_stop;
    int last_stop_rc;
    eth_err_snap_t last_err[2];             /* [0] RX, [1] TX: snapshot of the last halting error */
    uint32_t err_halted;                    /* bit 0 RX, bit 1 TX: halted, waiting for recover */
    uint32_t err_masked;                    /* ERR sources masked until recovered */
    uint32_t err_watch;                     /* bit 0 RX, bit 1 TX: class-4 ERR cleared, watched for HALTED */
    uint64_t watch_since[2];                /* mtime of that clear */
    uint64_t inflight_since;                /* mtime of the first in-flight observation, 0: none */
    int wb_late;                            /* the current in-flight episode passed its deadline */
    fx1_eth_rx_cb rx_cb;
    void* rx_ctx;
    uint8_t rx_buf[1536];
} fx1_eth;

/* HAL of the portable driver on fx1_soc. `yield` (optional) is called about
 * once per millisecond of a driver wait (the RTOS gives the CPU away). */
const eth_hal_t* fx1_eth_hal_mmio(void);
void fx1_eth_hal_set_yield(void (*yield)(void));
uint32_t fx1_eth_hal_range_errors(void);
/* IP offset of a CPU address in DDR (no check) and back. */
uint32_t fx1_eth_offset(const volatile void* cpu);
void* fx1_eth_cpu(uint32_t offset);

/* Reset the manager (UNINIT); no hardware access. */
void fx1_eth_setup(fx1_eth* e);
/* ID/CAP/BUILDCFG and the PHY over MDIO; releases ETH_CTRL[0] and [3] only
 * (the MDIO block) when the IP is still held. ETH_OK, ETH_E_BUILDCFG (MAC not
 * the expected IP) or ETH_E_MDIO (no PHY). */
int fx1_eth_probe(fx1_eth* e, fx1_eth_probe_info* info);
/* UNINIT: cold init (MAS 9.1); STOPPED: restart (SRST, rings rebuilt);
 * RUNNING: nothing to do (ETH_OK); FAULTED: ETH_E_STATE (recover first). */
int fx1_eth_start(fx1_eth* e);
/* RUNNING: drain up to drain_us, then quiesce (forced if needed) -> STOPPED,
 * or FAULTED when not quiescent. STOPPED/UNINIT: ETH_OK, nothing done. */
int fx1_eth_stop(fx1_eth* e, uint32_t drain_us);
/* RUNNING with a halted direction (or an ERR pending): MAS 9.3.3 for that
 * direction only, after HALTED (up to ETH_DRV_HALT_WAIT_US; FAULTED if it
 * does not come).
 * FAULTED: quiesce again with a long deadline. Both directions HALTED: every
 * write of the old session landed, restart -> RUNNING. Otherwise ETH_E_TIMEOUT
 * and still FAULTED, nothing reset or rebuilt (review N2: ETH_CTRL[0] needs
 * both directions HALTED first, MAS 9.1); call it again later.
 * ETH_OK with nothing to recover is fine. */
int fx1_eth_recover(fx1_eth* e);
/* Queue one frame (DA..payload). RUNNING with no halting TX error taken
 * (ETH_E_STATE otherwise, also before HALTED is visible: review ER01), and
 * TX not HALTED (read on each call: a halt no ERR announced). */
int fx1_eth_send(fx1_eth* e, const void* frame, uint32_t len);
/* The owner's half: errors, received frames (to rx_cb), reclaimed transmit
 * descriptors, write-backs in flight. Returns after how many milliseconds it
 * must run again even if no interrupt comes (0 only when not RUNNING):
 *   - a DES0 write-back in flight commits without a new DONE (MAS 9.3, review
 *     F08/ER02): FX1_ETH_POLL_FAST_MS up to the 2 ms deadline, then
 *     FX1_ETH_POLL_SLOW_MS for as long as it stays pending (wb_timeouts counts
 *     the episode once; the descriptor is never reclaimed while OWN reads 1);
 *   - after a class-4 error was cleared, a halting error of the same episode
 *     shows only as HALTED (review ER01): FAST then SLOW for up to
 *     FX1_ETH_WATCH_MS, or until TX is SUSPENDED;
 *   - otherwise FX1_ETH_POLL_IDLE_MS: every pass checks HALTED of both
 *     directions, so a halt nothing announced is found within that time. */
#define FX1_ETH_POLL_FAST_MS 1u
#define FX1_ETH_POLL_SLOW_MS 10u
#define FX1_ETH_POLL_IDLE_MS 100u
#define FX1_ETH_WATCH_MS 100u
int fx1_eth_service(fx1_eth* e);
/* ISR half: take and acknowledge ETH_INT_STATUS (eth_irq_take), add it to
 * e->pending. Returns the status. */
uint32_t fx1_eth_isr(fx1_eth* e);
/* MDIO, validated (phy and reg 0..31). A write to BMCR (reg 0) is refused
 * while RUNNING: speed, loopback, reset and autonegotiation change only
 * through fx1_eth_set_speed / the selftest, with the MAC stopped. */
int fx1_eth_mdio_read(fx1_eth* e, unsigned phy, unsigned reg, uint16_t* v);
int fx1_eth_mdio_write(fx1_eth* e, unsigned phy, unsigned reg, uint16_t v);
/* STOPPED or UNINIT only: line rate of MAC (ETH_RMII_CFG) and PHY (BMCR). */
int fx1_eth_set_speed(fx1_eth* e, int speed100);
void fx1_eth_hw_stats_read(fx1_eth* e, fx1_eth_hw_stats* s);
/* RMON CLEAR_ALL, ETH_DMA_RX_DROP_CNT and the manager's counters. */
void fx1_eth_stats_clear(fx1_eth* e);

/* Loopback selftest through the PHY's own loopback (independent of the line
 * peer): stop, PHY loopback at the speed, start, frames of 60, 61, 512 and
 * 1514 bytes compared byte for byte, stop, PHY loopback off, the previous
 * state restored. Returns the number of failed frames (0: pass), < 0 on a
 * lifecycle error. Uses fx1_eth_service() for reception (polled). */
int fx1_eth_selftest(fx1_eth* e, int speed100, uint32_t timeout_us);

#ifdef __cplusplus
}
#endif

#endif /* FX1_ETH_H */
