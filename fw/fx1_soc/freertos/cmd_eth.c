/*
 * `eth` commands and the Ethernet service (plan M4) on the FX1 manager
 * (drivers/fx1_eth.c) and the portable driver.
 *
 * Ownership: one mutex. The service task holds it to run fx1_eth_service();
 * a command holds it for its whole operation. The ISR (PLIC source 6) only
 * takes and acknowledges the status (fx1_eth_isr: DONE cleared, level error
 * sources masked so the line falls before the PLIC completion) and notifies
 * the service task; it never waits and never touches the rings. During a
 * lifecycle operation (start/stop/recover/selftest) source 6 is also masked
 * at the PLIC, so no ISR runs while ETH_INT_EN is rewritten.
 *
 * DONE may come before the DES0 write-back (MAS 9.3, review F08): while the
 * service finds a descriptor in flight it runs again after the delay it
 * returns (1 ms, 10 ms once past its 2 ms deadline; review ER02) instead of
 * sleeping until an interrupt that will not come.
 */
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "eth_regs.h"
#include "fx1_eth.h"
#include "fx1_fw.h"
#include "rtos_app.h"
#include "rtos_bsp.h"

#define ETH(off) (FX1_ETH_CSR_BASE + (off))

static fx1_eth eth;
static SemaphoreHandle_t eth_lock;
static TaskHandle_t eth_task;

static BaseType_t eth_irq(void)
{
    BaseType_t woken = pdFALSE;
    fx1_eth_isr(&eth);
    if (eth_task) vTaskNotifyGiveFromISR(eth_task, &woken);
    return woken;
}

static void eth_yield(void) { taskYIELD(); }

static void eth_service_task(void* arg)
{
    (void)arg;
    TickType_t wait = portMAX_DELAY;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, wait);
        xSemaphoreTake(eth_lock, portMAX_DELAY);
        const int again_ms = fx1_eth_service(&eth);
        xSemaphoreGive(eth_lock);
        wait = portMAX_DELAY;
        if (again_ms > 0) {
            wait = pdMS_TO_TICKS((uint32_t)again_ms);
            if (wait == 0) wait = 1;
        }
    }
}

void eth_service_init(void)
{
    fx1_eth_setup(&eth);
    eth_lock = xSemaphoreCreateMutex();
    configASSERT(eth_lock);
    fx1_eth_hal_set_yield(eth_yield);
    configASSERT(xTaskCreate(eth_service_task, "eth", 1024, NULL, 3, &eth_task) == pdPASS);
    bsp_irq_register(FX1_IRQ_ETH, eth_irq);
}

/* ---- command plumbing ---------------------------------------------------------- */
static int lock(void)
{
    if (xSemaphoreTake(eth_lock, pdMS_TO_TICKS(2000)) != pdTRUE) {
        out_line("eth: busy (the service did not release the device in 2 s)");
        return 0;
    }
    return 1;
}

static void unlock(void)
{
    xSemaphoreGive(eth_lock);
    xTaskNotifyGive(eth_task);                       /* pick up what arrived meanwhile */
}

/* lifecycle operations run with source 6 masked at the PLIC */
static void irq_off(void) { fx1_plic_enable(0, FX1_IRQ_ETH, 0); }
static void irq_on(void) { fx1_plic_enable(0, FX1_IRQ_ETH, 1); }

static uint32_t us_since(uint64_t t0) { return (uint32_t)((fx1_mtime() - t0) / (FX1_MTIME_HZ / 1000000u)); }

static void out_rc(int rc)
{
    out(fx1_eth_rc_name(rc));
}

static void out_state(void)
{
    out("state ");
    out(fx1_eth_state_name(eth.state));
}

static uint8_t tx_frame[1514];

/* ---- subcommands ---------------------------------------------------------------- */
static void cmd_eth_probe(void)
{
    fx1_eth_probe_info pi;
    if (!lock()) return;
    const int rc = fx1_eth_probe(&eth, &pi);
    unlock();
    out("eth probe: ID ");
    out_hex(pi.id);
    out(" CAP ");
    out_hex(pi.cap);
    out(" BUILDCFG ");
    out_hex(pi.buildcfg);
    out_line(pi.mac_ok ? " (MAC ok)" : " (MAC not the expected IP)");
    if (pi.mac_ok) {
        if (pi.phy_ok) {
            out("eth probe: PHY at MDIO ");
            out_dec(FX1_ETH_PHY);
            out(" ID ");
            out_hex(((uint32_t)pi.phy_id1 << 16) | pi.phy_id2);
            out(", BMCR ");
            out_hex(pi.bmcr);
            out(", BMSR ");
            out_hex(pi.bmsr);
            out(pi.link ? ", link up" : ", link down");
            out_line(pi.speed100 ? ", 100 Mbps" : ", 10 Mbps");
        } else {
            out("eth probe: PHY not responding at MDIO ");
            out_dec(FX1_ETH_PHY);
            out_line(" (the MAC is fine)");
        }
    }
    report("eth probe", rc == ETH_OK);
}

static void cmd_eth_status(void)
{
    if (!lock()) return;
    eth_dev_t* d = &eth.dev;
    const uint32_t txc = fx1_read32(ETH(ETH_TX_MAC_CFG)), rxc = fx1_read32(ETH(ETH_RX_MAC_CFG));
    const uint32_t st = fx1_read32(ETH(ETH_STATUS)), ien = fx1_read32(ETH(ETH_INT_EN)),
                   ist = fx1_read32(ETH(ETH_INT_STATUS)), raw = fx1_read32(ETH(ETH_INT_RAW));
    const uint32_t txs = fx1_read32(ETH(ETH_DMA_TX_STAT)), rxs = fx1_read32(ETH(ETH_DMA_RX_STAT));
    const uint32_t txcur = fx1_read32(ETH(ETH_DMA_TX_CUR_DESC)), rxcur = fx1_read32(ETH(ETH_DMA_RX_CUR_DESC));
    unlock();
    out("eth: ");
    out(fx1_eth_state_name(eth.state));
    out(eth.speed100 ? ", 100 Mbps" : ", 10 Mbps");
    out(", TX_EN ");
    out_dec(txc & 1u);
    out(" RX_EN ");
    out_dec(rxc & 1u);
    out(", ETH_STATUS ");
    out_hex(st);
    out_line("");
    out("  irq: INT_EN ");
    out_hex(ien);
    out(" INT_RAW ");
    out_hex(raw);
    out(" INT_STATUS ");
    out_hex(ist);
    out(" pending ");
    out_hex(eth.pending);
    out_line("");
    out("  tx: head ");
    out_dec(d->tx_head);
    out(" tail ");
    out_dec(d->tx_tail);
    out(" CUR_DESC ");
    out_dec(txcur);
    out(" STAT ");
    out_hex(txs);
    out(" | rx: head ");
    out_dec(d->rx_head);
    out(" CUR_DESC ");
    out_dec(rxcur);
    out(" STAT ");
    out_hex(rxs);
    out_line("");
    out("  errors: halted ");
    out(eth.err_halted ? ((eth.err_halted & 2u) ? ((eth.err_halted & 1u) ? "tx+rx" : "tx") : "rx") : "none");
    out(", last TX ERRINFO ");
    out_hex(eth.last_err[1].errinfo);
    out(" CUR ");
    out_dec(eth.last_err[1].cur_desc);
    out(", last RX ERRINFO ");
    out_hex(eth.last_err[0].errinfo);
    out(" CUR ");
    out_dec(eth.last_err[0].cur_desc);
    out_line("");
    out("  last stop: ");
    out_rc(eth.last_stop_rc);
    out(" in ");
    out_dec(eth.last_stop.elapsed_us);
    out(" us, forced ");
    out_dec((uint32_t)eth.last_stop.forced);
    out(", dropped ");
    out_dec(eth.last_stop.tx_dropped);
    out(", fifo flushed ");
    out_dec((uint32_t)eth.last_stop.tx_fifo_flushed);
    out_line("");
}

static void cmd_eth_stats(int clear)
{
    fx1_eth_hw_stats hw;
    if (!lock()) return;
    if (clear) fx1_eth_stats_clear(&eth);
    fx1_eth_hw_stats_read(&eth, &hw);
    const fx1_eth_counters c = eth.c;
    unlock();
    if (clear) out_line("eth stats: cleared (RMON CLEAR_ALL, RX_DROP_CNT, driver counters)");
    out("eth stats (RMON reads do not clear: READ_CLEAR_EN 0): tx pkts ");
    out_dec(hw.tx_pkts);
    out(" octets ");
    out_dec(hw.tx_octets);
    out(" oversize ");
    out_dec(hw.tx_oversize);
    out(" drop ");
    out_dec(hw.tx_drop);
    out_line("");
    out("  rx pkts ");
    out_dec(hw.rx_pkts);
    out(" octets ");
    out_dec(hw.rx_octets);
    out(" crc ");
    out_dec(hw.rx_crc);
    out(" undersize ");
    out_dec(hw.rx_undersize);
    out(" oversize ");
    out_dec(hw.rx_oversize);
    out(" filter-drop ");
    out_dec(hw.rx_filter_drop);
    out(" drop-events ");
    out_dec(hw.rx_drop_events);
    out_line("");
    out("  dma: no-descriptor drops ");
    out_dec(hw.dma_rx_drop_cnt);
    out(", FIFO_STAT ");
    out_hex(hw.fifo_stat);
    out(", RMON overflow tx ");
    out_hex(hw.rmon_ovf_tx);
    out(" rx ");
    out_hex(hw.rmon_ovf_rx);
    out_line("");
    out("  driver: tx ");
    out_dec(c.tx_frames);
    out(" rx ");
    out_dec(c.rx_frames);
    out(" rx-trunc ");
    out_dec(c.rx_trunc);
    out(" desc-errors tx ");
    out_dec(c.tx_desc_errors);
    out(" rx ");
    out_dec(c.rx_desc_errors);
    out(" dropped ");
    out_dec(c.tx_dropped);
    out(" ring-full ");
    out_dec(c.tx_full);
    out_line("");
    out("  driver: irqs ");
    out_dec(c.irqs);
    out(" (errors ");
    out_dec(c.irq_errors);
    out(") halts tx ");
    out_dec(c.tx_halts);
    out(" rx ");
    out_dec(c.rx_halts);
    out(" soft-errors ");
    out_dec(c.soft_errors);
    out(" recoveries ");
    out_dec(c.recoveries);
    out(" recover-timeouts ");
    out_dec(c.recover_timeouts);
    out(" wb-inflight ");
    out_dec(c.wb_inflight);
    out(" wb-timeouts ");
    out_dec(c.wb_timeouts);
    out(" starts ");
    out_dec(c.starts);
    out(" stops ");
    out_dec(c.stops);
    out(" (forced ");
    out_dec(c.forced_stops);
    out(", failed ");
    out_dec(c.failed_stops);
    out(", fifo-flushed ");
    out_dec(c.tx_fifo_flushes);
    out_line(")");
}

static void cmd_eth_start(void)
{
    if (!lock()) return;
    const fx1_eth_state before = eth.state;
    irq_off();
    const uint64_t t0 = fx1_mtime();
    const int rc = fx1_eth_start(&eth);
    const uint32_t us = us_since(t0);
    irq_on();
    unlock();
    out("eth start: ");
    out_rc(rc);
    if (rc == ETH_OK && before == FX1_ETH_RUNNING) out(" (already running, nothing done)");
    out(" in ");
    out_dec(us);
    out(" us, ");
    out_state();
    out_line("");
    report("eth start", rc == ETH_OK);
}

static void cmd_eth_stop(uint32_t drain_us)
{
    if (!lock()) return;
    const fx1_eth_state before = eth.state;
    irq_off();
    const uint64_t t0 = fx1_mtime();
    const int rc = fx1_eth_stop(&eth, drain_us);
    const uint32_t us = us_since(t0);
    irq_on();
    unlock();
    out("eth stop: ");
    out_rc(rc);
    if (before != FX1_ETH_RUNNING && rc == ETH_OK) out(" (not running, nothing done)");
    out(" in ");
    out_dec(us);
    out(" us, ");
    out_state();
    if (before == FX1_ETH_RUNNING) {
        out(", dropped ");
        out_dec(eth.last_stop.tx_dropped);
        out(", forced ");
        out_dec((uint32_t)eth.last_stop.forced);
        if (eth.last_stop.tx_fifo_flushed) out(", TX FIFO flushed before it drained");
    }
    if (rc != ETH_OK) out(" (DMA may still own the rings: eth recover)");
    out_line("");
    report("eth stop", rc == ETH_OK);
}

static void cmd_eth_recover(void)
{
    if (!lock()) return;
    const fx1_eth_state before = eth.state;
    const uint32_t halted = eth.err_halted;
    irq_off();
    const uint64_t t0 = fx1_mtime();
    const int rc = fx1_eth_recover(&eth);
    const uint32_t us = us_since(t0);
    irq_on();
    unlock();
    out("eth recover: ");
    out_rc(rc);
    out(" in ");
    out_dec(us);
    out(" us, ");
    if (before == FX1_ETH_FAULTED) out("from FAULTED, ");
    else if (halted) out(halted == 3u ? "TX and RX " : (halted & 2u) ? "TX " : "RX ");
    else if (before == FX1_ETH_RUNNING) out("nothing to recover, ");
    out_state();
    out_line("");
    report("eth recover", rc == ETH_OK);
}

static int cmd_eth_mdio(int argc, char** argv)
{
    uint32_t phy, reg, val = 0;
    const int write = argc >= 3 && !strcmp(argv[2], "write");
    if (argc < 5 || (!write && strcmp(argv[2], "read")) || !parse_u32(argv[3], &phy) || !parse_u32(argv[4], &reg) ||
        (write && (argc != 6 || !parse_u32(argv[5], &val))) || (!write && argc != 5))
        return 1;
    if (phy > 31u || reg > 31u || val > 0xFFFFu) {
        out_line("eth mdio: PHY and register are 0..31, the value 0..0xffff");
        return 0;
    }
    if (!lock()) return 0;
    uint16_t v = (uint16_t)val;
    const int rc = write ? fx1_eth_mdio_write(&eth, phy, reg, v) : fx1_eth_mdio_read(&eth, phy, reg, &v);
    unlock();
    out("eth mdio ");
    out(write ? "write " : "read ");
    out_dec(phy);
    out(".");
    out_dec(reg);
    out(": ");
    if (rc == ETH_OK) {
        out_hex(v);
        out_line("");
    } else if (rc == ETH_E_STATE) {
        out_line("refused: BMCR changes speed/loopback/reset only with the MAC stopped (eth stop first)");
    } else {
        out_rc(rc);
        out_line(rc == ETH_E_MDIO ? " (no PHY answered)" : "");
    }
    return 0;
}

static int cmd_eth_send(int argc, char** argv)
{
    uint32_t n, len = 128, pat = 0xA5;
    if (argc < 3 || !parse_u32(argv[2], &n) || !n || n > 1000u || (argc > 3 && !parse_u32(argv[3], &len)) ||
        (argc > 4 && !parse_u32(argv[4], &pat)) || argc > 5)
        return 1;
    if (len < 60u || len > 1514u || pat > 0xFFu) {
        out_line("eth send: LEN 60..1514, PATTERN 0..0xff");
        return 0;
    }
    const uint32_t rx0 = eth.c.rx_frames;
    for (int i = 0; i < 6; i++) {
        tx_frame[i] = eth.cfg.mac[i];                /* to us: loopback or the partner's echo */
        tx_frame[6 + i] = eth.cfg.mac[i];
    }
    tx_frame[12] = 0x88;
    tx_frame[13] = 0xB5;
    for (uint32_t i = 14; i < len; i++) tx_frame[i] = (uint8_t)pat;
    uint32_t queued = 0, refused = 0;
    int rc = ETH_OK;
    const uint64_t t0 = fx1_mtime();
    /* one frame per hold of the lock: between two, the service task (higher
     * priority) returns the receive descriptors the loopback copies fill */
    for (uint32_t k = 0; k < n; k++) {
        for (int tries = 0; tries < 200; tries++) {
            if (!lock()) return 0;
            rc = fx1_eth_send(&eth, tx_frame, len);
            unlock();
            if (rc != ETH_E_FULL) break;
            vTaskDelay(1);
        }
        if (rc != ETH_OK) {
            refused++;
            if (rc != ETH_E_FULL) break;
        } else {
            queued++;
        }
    }
    /* what came back (loopback / partner), for up to 2 ms per frame */
    const TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(2u * n + 5u);
    while (eth.c.rx_frames - rx0 < queued && xTaskGetTickCount() < end) vTaskDelay(1);
    out("eth send: ");
    out_dec(queued);
    out(" x ");
    out_dec(len);
    out(" bytes queued, ");
    out_dec(refused);
    out(" refused");
    if (rc != ETH_OK) {
        out(" (");
        out_rc(rc);
        out(")");
    }
    out(", ");
    out_dec(eth.c.rx_frames - rx0);
    out(" received back in ");
    out_dec(us_since(t0));
    out_line(" us");
    return 0;
}

static int cmd_eth_dump(int argc, char** argv)
{
    if (argc != 3 || (strcmp(argv[2], "rx") && strcmp(argv[2], "tx"))) return 1;
    const int tx = argv[2][0] == 't';
    if (!lock()) return 0;
    const uint32_t ring = tx ? eth.cfg.tx_ring : eth.cfg.rx_ring;
    const uint32_t cur = fx1_read32(ETH(tx ? ETH_DMA_TX_CUR_DESC : ETH_DMA_RX_CUR_DESC));
    const uint32_t head = (tx ? eth.dev.tx_head : eth.dev.rx_head) % FX1_ETH_RING;
    const uint32_t tail = eth.dev.tx_tail % FX1_ETH_RING;
    out(tx ? "eth tx ring at " : "eth rx ring at ");
    out_hex((uint32_t)(uintptr_t)fx1_eth_cpu(ring));
    out(" (IP offset ");
    out_hex(ring);
    out("), ");
    out(fx1_eth_state_name(eth.state));
    out_line(eth.state == FX1_ETH_UNINIT ? ": not initialized" : "");
    for (uint32_t i = 0; i < FX1_ETH_RING && eth.state != FX1_ETH_UNINIT; i++) {
        const volatile uint32_t* w = (const volatile uint32_t*)fx1_eth_cpu(ring + 16u * i);
        out(i == cur ? " >" : "  ");
        out(i == head ? "h" : " ");
        out(tx && i == tail ? "t" : " ");
        out_dec(i);
        out(i < 10 ? "  " : " ");
        for (int k = 0; k < 4; k++) {
            out(" ");
            out_hex(w[k]);
        }
        out_line(w[0] & 0x80000000u ? "  OWN" : "");
    }
    unlock();
    out_line("  (> CUR_DESC, h software head, t transmit tail)");
    return 0;
}

/* ---- eth cycle N: stop/start with one frame per session ------------------------- */
static void cmd_eth_cycle(uint32_t n)
{
    const size_t heap0 = xPortGetFreeHeapSize();
    uint32_t ok = 0, stale = 0;
    for (int i = 0; i < 6; i++) {
        tx_frame[i] = eth.cfg.mac[i];
        tx_frame[6 + i] = eth.cfg.mac[i];
    }
    tx_frame[12] = 0x88;
    tx_frame[13] = 0xB5;
    const uint64_t t0 = fx1_mtime();
    for (uint32_t k = 0; k < n; k++) {
        if (!lock()) break;
        irq_off();
        int rc = fx1_eth_start(&eth);
        irq_on();
        const uint32_t rx0 = eth.c.rx_frames;
        unlock();
        if (rc != ETH_OK) break;
        /* the frame goes out and (loopback/partner) comes back */
        if (!lock()) break;
        rc = fx1_eth_send(&eth, tx_frame, 100);
        unlock();
        const TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(5);
        while (eth.c.rx_frames == rx0 && xTaskGetTickCount() < end) vTaskDelay(1);
        if (!lock()) break;
        if (eth.c.rx_frames > rx0 + 1u) stale++;
        irq_off();
        const int src = fx1_eth_stop(&eth, 2000);
        irq_on();
        unlock();
        if (rc == ETH_OK && src == ETH_OK && eth.state == FX1_ETH_STOPPED) ok++;
        else break;
    }
    const size_t heap1 = xPortGetFreeHeapSize();
    out("eth cycle: ");
    out_dec(ok);
    out("/");
    out_dec(n);
    out(" stop/start cycles in ");
    out_dec(us_since(t0) / 1000u);
    out(" ms, stale frames ");
    out_dec(stale);
    out(", heap free ");
    out_dec((uint32_t)heap0);
    out(" -> ");
    out_dec((uint32_t)heap1);
    out(", service stack spare ");
    out_dec((uint32_t)uxTaskGetStackHighWaterMark(eth_task));
    out_line(" words");
    report("eth cycle", ok == n && stale == 0 && heap0 == heap1);
}

/* ---- eth load start|stop: background traffic with a payload check -------------- */
static TaskHandle_t load_task;
static volatile int load_run;
static volatile uint32_t load_sent, load_rx, load_bad, load_refused;
static uint8_t load_frame[1024];

static void load_rx_cb(void* ctx, const uint8_t* f, uint32_t len, uint32_t des0)
{
    (void)ctx;
    if (len < 18 || f[12] != 0x88 || f[13] != 0xB6) return;     /* not a load frame */
    const uint32_t seq = f[14] | f[15] << 8 | f[16] << 16 | (uint32_t)f[17] << 24;
    int ok = len == sizeof load_frame && !(des0 & 0x00070000u);
    for (uint32_t i = 18; ok && i < len; i++) ok = f[i] == (uint8_t)(i + seq);
    load_rx++;
    if (!ok) load_bad++;
}

static void eth_load_task(void* arg)
{
    (void)arg;
    uint32_t seq = 0;
    while (load_run) {
        for (int i = 0; i < 6; i++) {
            load_frame[i] = eth.cfg.mac[i];
            load_frame[6 + i] = eth.cfg.mac[i];
        }
        load_frame[12] = 0x88;
        load_frame[13] = 0xB6;
        for (int i = 0; i < 4; i++) load_frame[14 + i] = (uint8_t)(seq >> (8 * i));
        for (uint32_t i = 18; i < sizeof load_frame; i++) load_frame[i] = (uint8_t)(i + seq);
        if (xSemaphoreTake(eth_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            const int rc = fx1_eth_send(&eth, load_frame, sizeof load_frame);
            xSemaphoreGive(eth_lock);
            if (rc == ETH_OK) {
                load_sent++;
                seq++;
            } else {
                load_refused++;
            }
        }
        vTaskDelay(1);
    }
    load_task = NULL;
    vTaskDelete(NULL);
}

static void cmd_eth_load(int start)
{
    if (start) {
        if (load_task) {
            out_line("eth load: already running");
            return;
        }
        if (!lock()) return;
        if (eth.state != FX1_ETH_RUNNING) {
            irq_off();
            fx1_eth_start(&eth);
            irq_on();
        }
        eth.rx_cb = load_rx_cb;
        unlock();
        load_sent = load_rx = load_bad = load_refused = 0;
        load_run = 1;
        /* the CLI's priority: time-sliced with a command that busy-polls */
        if (xTaskCreate(eth_load_task, "ethload", 512, NULL, 2, &load_task) != pdPASS) {
            load_run = 0;
            out_line("eth load: no task");
            return;
        }
        out_line("eth load: started (1 KiB frames to ourselves, one per tick)");
        return;
    }
    if (!load_task) {
        out_line("eth load: not running");
        return;
    }
    load_run = 0;
    while (load_task) vTaskDelay(1);
    vTaskDelay(pdMS_TO_TICKS(5));                    /* the last frames come back */
    if (lock()) {
        eth.rx_cb = NULL;
        unlock();
    }
    out("eth load: sent ");
    out_dec(load_sent);
    out(", received ");
    out_dec(load_rx);
    out(", payload errors ");
    out_dec(load_bad);
    out(", refused ");
    out_dec(load_refused);
    out_line("");
    report("eth load", load_sent > 0 && load_bad == 0 && load_rx == load_sent);
}

/* ---- MDIO from a second task (Ethernet gap G5) ---------------------------------
 * PHY ID1/ID2 read again and again, each read under the owner's mutex, while
 * the CLI runs the lifecycle (stop, start, cycle, recover, selftest): MDIO and
 * lifecycle serialize on the one owner, so every read returns the PHY's ID. */
static TaskHandle_t mdio_task;
static volatile int mdio_run;
static volatile uint32_t mdio_reads, mdio_errors, mdio_not_running, mdio_halted;

static void eth_mdio_task(void* arg)
{
    (void)arg;
    while (mdio_run) {
        if (xSemaphoreTake(eth_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint16_t id1 = 0, id2 = 0;
            const int rc1 = fx1_eth_mdio_read(&eth, FX1_ETH_PHY, 2, &id1);
            const int rc2 = fx1_eth_mdio_read(&eth, FX1_ETH_PHY, 3, &id2);
            if (eth.state != FX1_ETH_RUNNING) mdio_not_running++;   /* between stop and start */
            if (eth.err_halted) mdio_halted++;                       /* a direction waits for recover */
            xSemaphoreGive(eth_lock);
            mdio_reads++;
            if (rc1 != ETH_OK || rc2 != ETH_OK || id1 != 0x0007u || id2 != 0xC0F1u) mdio_errors++;
        }
        vTaskDelay(1);
    }
    mdio_task = NULL;
    vTaskDelete(NULL);
}

static void cmd_eth_mdiopoll(int start)
{
    if (start) {
        if (mdio_task) {
            out_line("eth mdiopoll: already running");
            return;
        }
        mdio_reads = mdio_errors = mdio_not_running = mdio_halted = 0;
        mdio_run = 1;
        if (xTaskCreate(eth_mdio_task, "ethmdio", 384, NULL, 2, &mdio_task) != pdPASS) {
            mdio_run = 0;
            out_line("eth mdiopoll: no task");
            return;
        }
        out_line("eth mdiopoll: started (PHY ID1/ID2 every tick, from a second task)");
        return;
    }
    if (!mdio_task) {
        out_line("eth mdiopoll: not running");
        return;
    }
    mdio_run = 0;
    while (mdio_task) vTaskDelay(1);
    out("eth mdiopoll: ");
    out_dec(mdio_reads);
    out(" reads, ");
    out_dec(mdio_errors);
    out(" wrong or failed; ");
    out_dec(mdio_not_running);
    out(" while not running, ");
    out_dec(mdio_halted);
    out_line(" with a direction halted");
    report("eth mdiopoll", mdio_reads > 0 && mdio_errors == 0);
}

#ifdef FX1_ETH_TEST_FAULTS
/* Test images only: make the next DMA access of a direction fail with DECERR
 * (a buffer that crosses the end of the DDR aperture). `soft`: two transmit
 * descriptors, a 1600-byte frame (TX_TOO_LONG, class 4) then the bad buffer:
 * the halting error comes while ERR still holds class 4 (review ER01). */
static void cmd_eth_fault(int tx, int soft)
{
    if (!lock()) return;
    eth_dev_t* d = &eth.dev;
    const eth_hal_t* h = fx1_eth_hal_mmio();
    const uint32_t bad = FX1_DDR_SIZE - 64u, own = 0x80000000u;
    if (soft) {
        const uint32_t i = d->tx_head % FX1_ETH_RING, j = (d->tx_head + 1u) % FX1_ETH_RING;
        const uint32_t da = eth.cfg.tx_ring + 16u * i, db = eth.cfg.tx_ring + 16u * j;
        const uint32_t la = 0xC0000000u | 1600u, lb = 0xC0000000u | 128u;
        const uint32_t ba = eth.cfg.tx_buf + FX1_ETH_STRIDE * i;
        h->mem_wr(0, db + 4, &lb, 4);
        h->mem_wr(0, db + 8, &bad, 4);
        h->mem_wr(0, db, &own, 4);
        h->mem_wr(0, da + 4, &la, 4);
        h->mem_wr(0, da + 8, &ba, 4);
        h->mem_wr(0, da, &own, 4);                    /* the first one last */
        d->tx_head += 2u;
        fx1_write32(ETH(ETH_DMA_TX_POLL), 1);
    } else if (tx) {
        const uint32_t desc = eth.cfg.tx_ring + 16u * (d->tx_head % FX1_ETH_RING), des1 = 0xC0000000u | 128u;
        h->mem_wr(0, desc + 4, &des1, 4);
        h->mem_wr(0, desc + 8, &bad, 4);
        h->mem_wr(0, desc, &own, 4);
        d->tx_head++;
        fx1_write32(ETH(ETH_DMA_TX_POLL), 1);
    } else {
        /* the held descriptor is re-fetched after a halt and START */
        fx1_write32(ETH(ETH_DMA_RX_CTRL), ETH_DMA_RX_CTRL_HALT_REQ_MASK);
        while (!(fx1_read32(ETH(ETH_DMA_RX_STAT)) & ETH_DMA_RX_STAT_HALTED_MASK)) {
        }
        const uint32_t desc = eth.cfg.rx_ring + 16u * (d->rx_head % FX1_ETH_RING);
        h->mem_wr(0, desc + 8, &bad, 4);
        fx1_write32(ETH(ETH_DMA_RX_CTRL), ETH_DMA_RX_CTRL_START_MASK);
    }
    unlock();
    out(soft ? "eth fault: TX_TOO_LONG frame, then a TX descriptor past the DDR aperture, queued"
        : tx ? "eth fault: TX descriptor past the DDR aperture queued" : "eth fault: RX buffer past the DDR aperture");
    out_line("");
}
#endif

static void cmd_eth_selftest(void)
{
    if (!lock()) return;
    const uint32_t irq0 = eth.c.irqs;
    irq_off();
    int r100 = fx1_eth_selftest(&eth, 1, 5000);
    int r10 = r100 < 0 ? r100 : fx1_eth_selftest(&eth, 0, 20000);
    irq_on();
    /* the interrupt path: one frame through the running service */
    int irq_ok = 0;
    if (r100 >= 0 && r10 >= 0) {
        const fx1_eth_state before = eth.state;
        if (before != FX1_ETH_RUNNING) {
            irq_off();
            fx1_eth_start(&eth);
            irq_on();
        }
        unlock();
        const uint32_t rx0 = eth.c.rx_frames;
        static char* send1[] = {"eth", "send", "1", "64"};
        cmd_eth_send(4, send1);
        irq_ok = eth.c.irqs > irq0 && eth.c.rx_frames >= rx0;
        if (!lock()) return;
        if (before != FX1_ETH_RUNNING) {
            irq_off();
            fx1_eth_stop(&eth, 2000);
            irq_on();
        }
    }
    unlock();
    out("eth selftest: PHY loopback, 60/61/512/1514 bytes: 100 Mbps ");
    out(r100 == 0 ? "ok" : r100 > 0 ? "BAD" : fx1_eth_rc_name(r100));
    out(", 10 Mbps ");
    out(r10 == 0 ? "ok" : r10 > 0 ? "BAD" : fx1_eth_rc_name(r10));
    out(", interrupt path ");
    out_line(irq_ok ? "ok" : "BAD");
    report("eth selftest", r100 == 0 && r10 == 0 && irq_ok);
}

int cmd_eth(int argc, char** argv)
{
    if (argc < 2) return 1;
    const char* sub = argv[1];
    if (!strcmp(sub, "probe") && argc == 2) cmd_eth_probe();
    else if (!strcmp(sub, "status") && argc == 2) cmd_eth_status();
    else if (!strcmp(sub, "stats") && (argc == 2 || (argc == 3 && !strcmp(argv[2], "clear")))) cmd_eth_stats(argc == 3);
    else if (!strcmp(sub, "start") && argc == 2) cmd_eth_start();
    else if (!strcmp(sub, "stop") && argc <= 3) {
        uint32_t drain = 20000;                      /* us: queued frames go out first */
        if (argc == 3 && (!parse_u32(argv[2], &drain) || drain > 1000000u)) return 1;
        cmd_eth_stop(drain);
    } else if (!strcmp(sub, "recover") && argc == 2) cmd_eth_recover();
    else if (!strcmp(sub, "selftest") && argc == 2) cmd_eth_selftest();
    else if (!strcmp(sub, "mdio")) return cmd_eth_mdio(argc, argv);
    else if (!strcmp(sub, "send")) return cmd_eth_send(argc, argv);
    else if (!strcmp(sub, "dump")) return cmd_eth_dump(argc, argv);
    else if (!strcmp(sub, "cycle") && argc == 3) {
        uint32_t n;
        if (!parse_u32(argv[2], &n) || !n || n > 1000u) return 1;
        cmd_eth_cycle(n);
    } else if (!strcmp(sub, "load") && argc == 3 && (!strcmp(argv[2], "start") || !strcmp(argv[2], "stop")))
        cmd_eth_load(argv[2][2] == 'a');
    else if (!strcmp(sub, "mdiopoll") && argc == 3 && (!strcmp(argv[2], "start") || !strcmp(argv[2], "stop")))
        cmd_eth_mdiopoll(argv[2][2] == 'a');
    else if (!strcmp(sub, "wait") && argc == 3) {
        /* scripts: let MS milliseconds pass, the device left to the service */
        uint32_t ms;
        if (!parse_u32(argv[2], &ms) || ms > 10000u) return 1;
        vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1);
        out("eth wait: ");
        out_dec(ms);
        out_line(" ms");
    }
#ifdef FX1_ETH_TEST_FAULTS
    else if (!strcmp(sub, "fault") && argc == 3 &&
             (!strcmp(argv[2], "tx") || !strcmp(argv[2], "rx") || !strcmp(argv[2], "softtx")))
        cmd_eth_fault(argv[2][0] != 'r', argv[2][0] == 's');
#endif
    else return 1;
    return 0;
}
