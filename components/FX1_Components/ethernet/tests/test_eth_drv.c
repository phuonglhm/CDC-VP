/* Receive descriptor handling without a simulator: the HAL records payload
 * reads and descriptor-return/doorbell ordering, including undefined LENGTH. */
#include <stdio.h>
#include <string.h>

#include "eth_drv.h"
#include "eth_regs.h"

typedef struct {
    uint32_t des0, returned, payload_reads, bytes, polls;
    int failed;
} memory_t;

static void mem_rd(void *ctx, uint32_t addr, void *dst, uint32_t n)
{
    memory_t *m = ctx;
    if (addr == 0x1000 && n == 4) {
        memcpy(dst, &m->des0, 4);
    } else if (addr == 0x2002) {
        m->payload_reads++;
        m->bytes += n;
        memset(dst, 0x5A, n);
    } else {
        m->failed = 1;
    }
}

static void mem_wr(void *ctx, uint32_t addr, const void *src, uint32_t n)
{
    memory_t *m = ctx;
    uint32_t value = 0;
    if (n == 4) memcpy(&value, src, 4);
    if (addr != 0x1000 || n != 4 || value != 0x80000000u) m->failed = 1;
    m->returned++;
}

static void csr_wr(void *ctx, uint32_t off, uint32_t value)
{
    memory_t *m = ctx;
    if (off != ETH_DMA_RX_POLL || value != ETH_DMA_RX_POLL_POLL_MASK ||
        m->returned != 1) m->failed = 1;
    m->polls++;
}

static int receive_case(uint32_t des0, int expected, uint32_t copies, int error)
{
    memory_t m = {0};
    eth_dev_t d = {0};
    unsigned char buf[16];
    uint32_t status = 0;
    m.des0 = des0;
    d.hal.ctx = &m;
    d.hal.mem_rd = mem_rd;
    d.hal.mem_wr = mem_wr;
    d.hal.wr = csr_wr;
    d.cfg.rx_ring = 0x1000;
    d.cfg.rx_buf = 0x2000;
    d.cfg.rx_lead = 2;
    d.cfg.ring_len = 1;
    memset(buf, 0xA5, sizeof buf);
    const int result = eth_recv(&d, buf, sizeof buf, &status);
    const int owned = (des0 & 0x80000000u) != 0;
    int failed = m.failed || result != expected || m.bytes != copies ||
        m.payload_reads != (copies != 0) || m.returned != (unsigned)!owned ||
        m.polls != (unsigned)!owned || d.rx_head != (unsigned)!owned ||
        d.rx_done != (unsigned)!owned || d.rx_errors != (unsigned)error ||
        (!owned && status != des0);
    for (unsigned i = 0; i < sizeof buf; ++i)
        if (buf[i] != (i < copies ? 0x5A : 0xA5)) failed = 1;
    if (failed) fprintf(stderr, "eth_recv failed for DES0=0x%08x\n", des0);
    return failed;
}

int main(void)
{
    int failures = 0;
    failures += receive_case(0x80000000u, 0, 0, 0); /* still owned by hardware */
    failures += receive_case(0x0002FFFFu, -1, 0, 1); /* undefined length, no copy */
    failures += receive_case(0x00020000u, -1, 0, 1); /* zero length is still error */
    failures += receive_case(8, 8, 8, 0);
    failures += receive_case(64, 16, 16, 0);        /* cap valid data at buffer size */
    failures += receive_case(0x00010008u, 8, 8, 1); /* truncation retains valid data */
    if (!failures) puts("eth_recv: 6 cases passed");
    return failures ? 1 : 0;
}
