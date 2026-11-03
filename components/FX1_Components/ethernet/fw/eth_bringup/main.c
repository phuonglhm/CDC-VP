

#include <stdint.h>

#include "eth_drv.h"
#include "eth_regs.h"

#define ETH_BASE   0x40000000u
#define UART_BASE  0x40010000u
#define PLIC_BASE  0x4C000000u
#define UNMAPPED   0x08000000u          /* past the 64 MiB DDR: DECERR */

#define REG(off)   (*(volatile uint32_t *)(ETH_BASE + (off)))
#define PLIC_PRIO(id)  (*(volatile uint32_t *)(PLIC_BASE + 4u * (id)))
#define PLIC_PENDING   (*(volatile uint32_t *)(PLIC_BASE + 0x1000u))
#define PLIC_ENABLE    (*(volatile uint32_t *)(PLIC_BASE + 0x2000u))
#define PLIC_THRESHOLD (*(volatile uint32_t *)(PLIC_BASE + 0x200000u))
#define PLIC_CLAIM     (*(volatile uint32_t *)(PLIC_BASE + 0x200004u))
#define ETH_SOURCE 1u

/* ---- tiny runtime: no libc --------------------------------------------------- */
void *memset(void *d, int c, unsigned long n)
{
    unsigned char *p = d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, unsigned long n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    while (n--) *p++ = *q++;
    return d;
}

static void uart_putc(char c) { *(volatile uint32_t *)UART_BASE = (uint32_t)(unsigned char)c; }

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

static void uart_hex(uint32_t v)
{
    static const char h[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int i = 28; i >= 0; i -= 4) uart_putc(h[(v >> i) & 0xF]);
}

static int failures;

static void check(int ok, const char *what, uint32_t got, uint32_t want)
{
    if (ok) return;
    failures++;
    uart_puts("FW: FAIL ");
    uart_puts(what);
    uart_puts(" got ");
    uart_hex(got);
    uart_puts(" want ");
    uart_hex(want);
    uart_puts("\n");
}

static void marker(const char *pass, int before)
{
    if (failures == before) uart_puts(pass);
}

/* ---- interrupt: PLIC claim, acknowledge at the source, complete ---------------- */
static volatile uint32_t irq_count, irq_status, irq_errs;

/* mtvec holds the mode in bits 1:0, so the handler must be 4-byte aligned. */
void __attribute__((interrupt("machine"), aligned(4))) trap_handler(void)
{
    uint32_t mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
    if (mcause != 0x8000000Bu) {           /* only machine external interrupts */
        uart_puts("FW: unexpected trap ");
        uart_hex(mcause);
        uart_puts("\n");
        for (;;) {
        }
    }
    const uint32_t id = PLIC_CLAIM;
    if (id == ETH_SOURCE) {
        const uint32_t st = REG(ETH_INT_STATUS);
        irq_status |= st;
        irq_count++;

        if (st & ETH_INT_RAW_TX_DONE_MASK) REG(ETH_DMA_TX_STAT) = ETH_DMA_TX_STAT_DONE_MASK;
        if (st & ETH_INT_RAW_RX_DONE_MASK) REG(ETH_DMA_RX_STAT) = ETH_DMA_RX_STAT_DONE_MASK;
        if (st & (ETH_INT_RAW_TX_ERR_MASK | ETH_INT_RAW_RX_ERR_MASK)) {
            irq_errs |= st & (ETH_INT_RAW_TX_ERR_MASK | ETH_INT_RAW_RX_ERR_MASK);
            REG(ETH_INT_EN) &= ~(ETH_INT_RAW_TX_ERR_MASK | ETH_INT_RAW_RX_ERR_MASK);
        }
    }
    PLIC_CLAIM = id;                       /* complete */
}

/* ---- the driver's HAL: MMIO and the CPU's view of DRAM (base 0) ----------------- */
static uint32_t hal_rd(void *c, uint32_t off)
{
    (void)c;
    return REG(off);
}
static void hal_wr(void *c, uint32_t off, uint32_t v)
{
    (void)c;
    REG(off) = v;
}
static void hal_mem_wr(void *c, uint32_t a, const void *s, uint32_t n)
{
    (void)c;
    volatile uint8_t *p = (volatile uint8_t *)a;
    const uint8_t *q = s;
    while (n--) *p++ = *q++;
}
static void hal_mem_rd(void *c, uint32_t a, void *d, uint32_t n)
{
    (void)c;
    const volatile uint8_t *p = (const volatile uint8_t *)a;
    uint8_t *q = d;
    while (n--) *q++ = *p++;
}
static uint32_t irq_seen;
static int hal_wait_irq(void *c, uint32_t us)
{
    (void)c;
    (void)us;                              /* bounded by the platform's --sim-ms */
    while (irq_count == irq_seen) __asm__ volatile("wfi");
    irq_seen = irq_count;
    return 1;
}
static void hal_delay_us(void *c, uint32_t us)
{
    (void)c;
    for (volatile uint32_t i = 0; i < us * 25u; i++) {
    }
}

/* ---- frames ------------------------------------------------------------------------ */
static const uint8_t mac_self[6] = {0x02, 0x00, 0x5E, 0x10, 0x00, 0x01};
static const uint8_t mac_peer[6] = {0x02, 0x00, 0x5E, 0x10, 0x00, 0x02};

/* IPv4/UDP, both checksum fields zero: the IP inserts them */
static uint32_t udp_frame(uint8_t *f, const uint8_t *da, uint32_t seq, uint32_t payload)
{
    const uint32_t tot = 20 + 8 + payload, ul = 8 + payload;
    for (int i = 0; i < 6; i++) {
        f[i] = da[i];
        f[6 + i] = mac_self[i];
    }
    const uint8_t hdr[30] = {0x08, 0x00, 0x45, 0, (uint8_t)(tot >> 8), (uint8_t)tot, 0, (uint8_t)seq,
                             0x40, 0, 64, 17, 0, 0, 10, 1, 0, 1, 10, 1, 0, 2,
                             0x13, 0x88, 0x13, 0x89, (uint8_t)(ul >> 8), (uint8_t)ul, 0, 0};
    for (int i = 0; i < 30; i++) f[12 + i] = hdr[i];
    for (uint32_t i = 0; i < payload; i++) f[42 + i] = (uint8_t)(seq * 13u + i * 3u);
    return 42 + payload;
}

static uint8_t txf[1600], rxf[1600];

int main(void)
{
    int before;
    uart_puts("FW: start\n");

    /* ---- identity and reset values ---------------------------------------------- */
    before = failures;
    check(REG(ETH_ID) == 0x45544801u, "ETH_ID", REG(ETH_ID), 0x45544801u);
    check(REG(ETH_DMA_BUILDCFG) == ETH_DRV_BUILDCFG, "BUILDCFG", REG(ETH_DMA_BUILDCFG), ETH_DRV_BUILDCFG);
    check(REG(ETH_CAP) == 1, "ETH_CAP", REG(ETH_CAP), 1);
    check(REG(ETH_MDIO_CTRL) == ETH_MDIO_CTRL_RESET, "MDIO_CTRL reset", REG(ETH_MDIO_CTRL), ETH_MDIO_CTRL_RESET);
    check((REG(ETH_DMA_TX_STAT) & 0x3F) == 0x4, "DMA_TX_STAT IDLE", REG(ETH_DMA_TX_STAT), 0x4);
    REG(ETH_HASH_L) = 0x12345678u;
    check(REG(ETH_HASH_L) == 0x12345678u, "HASH_L read/write", REG(ETH_HASH_L), 0x12345678u);
    REG(ETH_HASH_L) = 0;
    marker("FW: ID PASS\n", before);

    /* ---- interrupt path: PLIC source 1 -> MEIP -------------------------------------- */
    __asm__ volatile("csrw mtvec, %0" ::"r"((uint32_t)trap_handler));
    PLIC_PRIO(ETH_SOURCE) = 1;
    PLIC_THRESHOLD = 0;
    PLIC_ENABLE = 1u << ETH_SOURCE;
    __asm__ volatile("csrs mie, %0" ::"r"(1u << 11));
    __asm__ volatile("csrs mstatus, %0" ::"r"(1u << 3));

    before = failures;
    eth_dev_t d;
    eth_hal_t h = {0, hal_rd, hal_wr, hal_mem_wr, hal_mem_rd, hal_wait_irq, hal_delay_us};
    eth_cfg_t cfg;
    memset(&cfg, 0, sizeof cfg);
    memcpy(cfg.mac, mac_self, 6);
    cfg.speed100 = 1;
    cfg.phy_addr = 1;
    cfg.tx_ring = 0x00010000u;
    cfg.rx_ring = 0x00020000u;
    cfg.tx_buf = 0x00100000u;
    cfg.rx_buf = 0x00200000u;
    cfg.ring_len = 16;
    cfg.buf_stride = 0x800;
    cfg.rx_lead = 2;
    cfg.int_en = ETH_INT_RAW_TX_DONE_MASK | ETH_INT_RAW_RX_DONE_MASK | ETH_INT_RAW_TX_ERR_MASK |
                 ETH_INT_RAW_RX_ERR_MASK;
    cfg.filt_ctrl = ETH_FILT_CTRL_BCAST_ACCEPT_MASK;
    int rc = eth_init(&d, &h, &cfg);
    check(rc == ETH_OK, "eth_init", (uint32_t)rc, 0);
    uint16_t id1 = 0, id2 = 0, bmsr = 0, none = 0;
    rc = eth_mdio_read(&d, 1, 2, &id1) | eth_mdio_read(&d, 1, 3, &id2) | eth_mdio_read(&d, 1, 1, &bmsr);
    check(rc == ETH_OK && id1 == 0x0007 && id2 == 0xC0F1, "PHY identifier", ((uint32_t)id1 << 16) | id2, 0x0007C0F1u);
    check(bmsr & 4, "PHY link up (BMSR)", bmsr, 4);
    rc = eth_mdio_read(&d, 9, 2, &none);
    check(rc == ETH_E_MDIO && (REG(ETH_MDIO_DATA) & 0xFFFF) == 0xFFFF, "NO_RESPONSE at PHYAD 9", (uint32_t)rc,
          (uint32_t)ETH_E_MDIO);
    marker("FW: MDIO PASS\n", before);

    /* ---- to the line --------------------------------------------------------------------- */
    before = failures;
    for (uint32_t k = 0; k < 4; k++) {
        const uint32_t n = udp_frame(txf, mac_peer, k, 18 + 100 * k);
        rc = eth_send(&d, txf, n);
        check(rc == ETH_OK, "eth_send (line)", (uint32_t)rc, 0);
    }
    for (int t = 0; t < 400 && REG(RMON_TX_PKTS) < 4; t++) hal_delay_us(0, 10);
    eth_tx_reclaim(&d);
    check(REG(RMON_TX_PKTS) == 4 && d.tx_done == 4 && d.tx_errors == 0, "RMON_TX_PKTS / reclaim",
          REG(RMON_TX_PKTS), 4);
    marker("FW: LINE PASS\n", before);

    /* ---- PHY loopback: out and back, interrupt-driven ------------------------------------ */
    before = failures;
    rc = eth_mdio_write(&d, 1, 0, 0x6100);         /* loopback, 100 Mbps, full duplex */
    check(rc == ETH_OK, "BMCR loopback", (uint32_t)rc, 0);
    const uint32_t nloop = 12;
    uint32_t got = 0, sent = 0;
    while (got < nloop) {
        while (sent < nloop && sent - got < 4) {
            const uint32_t n = udp_frame(txf, mac_self, sent, 18 + 97 * sent);
            if (eth_send(&d, txf, n) != ETH_OK) break;
            sent++;
        }
        hal_wait_irq(0, 0);
        eth_tx_reclaim(&d);
        uint32_t des0;
        int n;
        while ((n = eth_recv(&d, rxf, sizeof rxf, &des0)) > 0) {
            const uint32_t want = udp_frame(txf, mac_self, got, 18 + 97 * got);
            check((uint32_t)n == want, "loopback length", (uint32_t)n, want);
            int same = 1;
            for (uint32_t i = 42; i < want; i++) same &= rxf[i] == txf[i];
            check(same, "loopback payload", got, 0);
            check(((des0 >> 20) & 0x3F) == 0x2A, "checksum verdict (inserted, then verified)",
                  (des0 >> 20) & 0x3F, 0x2A);
            check(rxf[24] != 0 || rxf[25] != 0, "IPv4 checksum inserted", 0, 1);
            got++;
        }
    }
    check(REG(RMON_RX_PKTS) == nloop, "RMON_RX_PKTS", REG(RMON_RX_PKTS), nloop);
    check(REG(RMON_RX_PKTS_64) == 1, "RMON_RX_PKTS_64 (the padded 60-byte frame)", REG(RMON_RX_PKTS_64), 1);
    check(irq_count > 0 && (irq_status & ETH_INT_RAW_RX_DONE_MASK), "RX_DONE interrupts", irq_count, 1);
    marker("FW: LOOP PASS\n", before);

    /* ---- a payload read error: halt, report, recover ---------------------------------------- */
    before = failures;
    const uint32_t bad = d.tx_head % cfg.ring_len;
    const uint32_t dsc = cfg.tx_ring + 16u * bad;
    *(volatile uint32_t *)(dsc + 8) = UNMAPPED;                        /* DES2 */
    *(volatile uint32_t *)(dsc + 4) = 0xC0000000u | 100u;              /* FS | LS | 100 */
    *(volatile uint32_t *)dsc = 0x80000000u;
    REG(ETH_DMA_TX_POLL) = 1;
    while (!(irq_errs & ETH_INT_RAW_TX_ERR_MASK)) __asm__ volatile("wfi");
    for (int t = 0; t < 100 && !(REG(ETH_DMA_TX_STAT) & ETH_DMA_TX_STAT_HALTED_MASK); t++) hal_delay_us(0, 1);
    check((REG(ETH_DMA_TX_STAT) & 0x13) == 0x12, "TX halted with ERR", REG(ETH_DMA_TX_STAT) & 0x13, 0x12);
    check(REG(ETH_DMA_TX_ERRINFO) == 0x13, "ERRINFO = DECERR | TX_DATA_RD_ERR", REG(ETH_DMA_TX_ERRINFO), 0x13);
    check(REG(ETH_DMA_TX_CUR_DESC) == bad, "CUR_DESC at the fault", REG(ETH_DMA_TX_CUR_DESC), bad);
    check(*(volatile uint32_t *)dsc == 0x00060000u, "DES0[18:17]", *(volatile uint32_t *)dsc, 0x00060000u);
    check(REG(RMON_TX_DROP) == 1, "RMON_TX_DROP", REG(RMON_TX_DROP), 1);
    
    REG(ETH_DMA_TX_STAT) = ETH_DMA_TX_STAT_ERR_MASK;
    REG(ETH_DMA_TX_CTRL) = ETH_DMA_TX_CTRL_SRST_MASK;
    check(REG(ETH_DMA_TX_CUR_DESC) == 0 && REG(ETH_DMA_TX_ERRINFO) == 0, "SRST index / ERRINFO cleared",
          REG(ETH_DMA_TX_CUR_DESC), 0);
    for (uint32_t i = 0; i < cfg.ring_len; i++) *(volatile uint32_t *)(cfg.tx_ring + 16u * i) = 0;
    d.tx_head = d.tx_tail = 0;
    REG(ETH_INT_EN) |= ETH_INT_RAW_TX_ERR_MASK | ETH_INT_RAW_RX_ERR_MASK;
    REG(ETH_DMA_TX_CTRL) = ETH_DMA_TX_CTRL_START_MASK;
    const uint32_t n = udp_frame(txf, mac_self, 99, 60);
    eth_send(&d, txf, n);
    hal_wait_irq(0, 0);
    for (int t = 0; t < 200 && eth_recv(&d, rxf, sizeof rxf, 0) == 0; t++) hal_delay_us(0, 5);
    check(REG(RMON_RX_PKTS) == nloop + 1, "a frame after recovery", REG(RMON_RX_PKTS), nloop + 1);
    marker("FW: ERR PASS\n", before);

    /* ---- the interrupt line is low once every source is acknowledged ---------------------- */
    eth_tx_reclaim(&d);
    for (int t = 0; t < 50; t++) hal_delay_us(0, 1);
    check(REG(ETH_INT_STATUS) == 0 && !(PLIC_PENDING & (1u << ETH_SOURCE)), "interrupt line low",
          REG(ETH_INT_STATUS), 0);
    rc = eth_stop(&d);
    check(rc == ETH_OK, "HALT both directions", (uint32_t)rc, 0);
    if (failures == 0) uart_puts("FW: ALL PASS\n");
    return failures;
}
