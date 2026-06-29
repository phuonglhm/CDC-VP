//Author: trangmn20
//Verified by: QuanNH107
/* DMA SoC verification firmware.
 *
 * Builds a DMA channel program in RAM, launches it via the debug
 * register interface (DBGINST0/DBGINST1/DBGCMD), and verifies the
 * transfer completed and copied the expected bytes.
 *
 * ADDRESSING NOTE:
 *   The DMA's master_socket shares the same bus_router as the CPU, so
 *   the DMA sees the SAME address map as the CPU. SAR/DAR/CPC values
 *   must be full addresses (RAM_BASE + offset), not raw offsets.
 */

#define UART_TX           (*(volatile unsigned char *)0x10000000u)
#define RAM_BASE          0x80000000u
#define DMA_BASE          0x10070000u

/* DMA register offsets (from dma_tlm.h) */
#define DMA_DSR           (DMA_BASE + 0x000u)
#define DMA_INTEN         (DMA_BASE + 0x020u)
#define DMA_INT_EVENT_RIS (DMA_BASE + 0x024u)
#define DMA_INTMIS        (DMA_BASE + 0x028u)
#define DMA_INTCLR        (DMA_BASE + 0x02Cu)
#define DMA_FSRC          (DMA_BASE + 0x034u)
#define DMA_CSR0          (DMA_BASE + 0x100u)
#define DMA_SAR0          (DMA_BASE + 0x400u)
#define DMA_DAR0          (DMA_BASE + 0x404u)
#define DMA_CCR0          (DMA_BASE + 0x408u)
#define DMA_DBGSTATUS     (DMA_BASE + 0xD00u)
#define DMA_DBGCMD        (DMA_BASE + 0xD04u)
#define DMA_DBGINST0      (DMA_BASE + 0xD08u)
#define DMA_DBGINST1      (DMA_BASE + 0xD0Cu)

#define STATUS_STOPPED    0x0u
#define EVENT_DONE        3u

/* Full RAM addresses for the DMA program/data */
#define PROGRAM_ADDR      (RAM_BASE + 0x80000u)
#define SRC_ADDR          (RAM_BASE + 0x80100u)
#define DST_ADDR          (RAM_BASE + 0x80200u)
#define COPY_LEN          32u

#define MMIO32(addr)      (*(volatile unsigned int *)(addr))
#define MMIO8(addr)       (*(volatile unsigned char *)(addr))

static void uart_putc(char c) { UART_TX = (unsigned char)c; }
static void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
static void uart_put_hex32(unsigned v) {
    static const char h[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int s = 28; s >= 0; s -= 4) uart_putc(h[(v >> s) & 0xF]);
}

static unsigned check(const char *name, unsigned got, unsigned expected) {
    if (got == expected) {
        uart_puts("  PASS "); uart_puts(name); uart_puts("\n");
        return 1;
    }
    uart_puts("  FAIL "); uart_puts(name);
    uart_puts(" expected="); uart_put_hex32(expected);
    uart_puts(" got="); uart_put_hex32(got); uart_puts("\n");
    return 0;
}

/* DMAMOV reg, imm  -- opcode 0xBC, reg in bits[7:3] of byte1, 4-byte little-endian imm */
static unsigned emit_dmamov(unsigned pc, unsigned char reg, unsigned imm) {
    MMIO8(pc + 0) = 0xBCu;
    MMIO8(pc + 1) = (unsigned char)(reg << 3);
    MMIO8(pc + 2) = (unsigned char)(imm & 0xFFu);
    MMIO8(pc + 3) = (unsigned char)((imm >> 8) & 0xFFu);
    MMIO8(pc + 4) = (unsigned char)((imm >> 16) & 0xFFu);
    MMIO8(pc + 5) = (unsigned char)((imm >> 24) & 0xFFu);
    return pc + 6;
}

int main(void)
{
    uart_puts("DMA platform start\n");
    unsigned pass = 1;

    /* ── Prepare source data and clear destination ── */
    uart_puts("\n[1] Prepare source/destination buffers\n");
    for (unsigned i = 0; i < COPY_LEN; i++) {
        MMIO8(SRC_ADDR + i) = (unsigned char)(0x40u + i);
        MMIO8(DST_ADDR + i) = 0u;
    }
    pass &= check("src[0]==0x40", MMIO8(SRC_ADDR), 0x40u);
    pass &= check("dst[0]==0",    MMIO8(DST_ADDR), 0u);

    /* ── Build a minimal DMA program ── */
    uart_puts("\n[2] Build DMA channel program in RAM\n");
    unsigned pc = PROGRAM_ADDR;
    /* CCR: src/dst increment, burst size = 4 bytes (encoded 2), burst len = 4 beats (encoded 3) */
    unsigned ccr = (1u << 0) | (2u << 1) | (3u << 4) |     /* src: inc, size=4B, len=4 */
                   (1u << 14) | (2u << 15) | (3u << 18);   /* dst: inc, size=4B, len=4 */
    pc = emit_dmamov(pc, 1, ccr);          /* DMAMOV CCR, ccr */
    pc = emit_dmamov(pc, 0, SRC_ADDR);     /* DMAMOV SAR, src */
    pc = emit_dmamov(pc, 2, DST_ADDR);     /* DMAMOV DAR, dst */
    MMIO8(pc) = 0x04u; pc += 1;            /* DMALD  (load one burst = 16 bytes) */
    MMIO8(pc) = 0x08u; pc += 1;            /* DMAST  (store one burst = 16 bytes) */
    MMIO8(pc) = 0x04u; pc += 1;            /* DMALD  second burst */
    MMIO8(pc) = 0x08u; pc += 1;            /* DMAST  second burst (16+16=32 bytes total) */
    MMIO8(pc) = 0x34u; pc += 1;            /* DMASEV event 3 */
    MMIO8(pc) = (unsigned char)(EVENT_DONE << 3); pc += 1;
    MMIO8(pc) = 0x00u; pc += 1;            /* DMAEND */
    uart_puts("  program length=");
    uart_put_hex32(pc - PROGRAM_ADDR);
    uart_puts("\n");

    /* ── Launch via debug interface ── */
    uart_puts("\n[3] Launch channel 0 via DBGINST/DBGCMD\n");
    MMIO32(DMA_INTEN) = (1u << EVENT_DONE);
    pass &= check("INTEN", MMIO32(DMA_INTEN), (1u << EVENT_DONE));

    /* DBGINST0: byte0=0xA0 (DMAGO, secure), byte1=0x00 (channel 0), channel_thread=0 (manager) */
    MMIO32(DMA_DBGINST0) = (0xA0u << 16) | (0x00u << 24);
    MMIO32(DMA_DBGINST1) = PROGRAM_ADDR;
    MMIO32(DMA_DBGCMD)   = 0u;             /* dispatch */

    /* ── Wait for completion ── */
    uart_puts("\n[4] Wait for channel completion\n");
    unsigned timeout = 100000u;
    unsigned csr0 = MMIO32(DMA_CSR0);
    while ((csr0 & 0xFu) != STATUS_STOPPED && timeout > 0) {
        csr0 = MMIO32(DMA_CSR0);
        timeout--;
    }
    pass &= check("CSR0 stopped after DMAEND", csr0 & 0xFu, STATUS_STOPPED);
    if (timeout == 0) {
        uart_puts("  WARNING: timed out waiting for channel\n");
    }

    /* ── Verify results ── */
    uart_puts("\n[5] Verify transfer results\n");
    unsigned data_ok = 1;
    for (unsigned i = 0; i < COPY_LEN; i++) {
        unsigned char s = MMIO8(SRC_ADDR + i);
        unsigned char d = MMIO8(DST_ADDR + i);
        if (s != d) {
            data_ok = 0;
            uart_puts("  MISMATCH at byte ");
            uart_put_hex32(i);
            uart_puts(" src=");
            uart_put_hex32(s);
            uart_puts(" dst=");
            uart_put_hex32(d);
            uart_puts("\n");
        }
    }
    if (data_ok) {
        uart_puts("  PASS all 32 bytes match\n");
    } else {
        pass = 0;
    }

    pass &= check("SAR0 final", MMIO32(DMA_SAR0), SRC_ADDR + COPY_LEN);
    pass &= check("DAR0 final", MMIO32(DMA_DAR0), DST_ADDR + COPY_LEN);
    pass &= check("FSRC clean", MMIO32(DMA_FSRC), 0u);
    pass &= check("INT_EVENT_RIS done bit", MMIO32(DMA_INT_EVENT_RIS), (1u << EVENT_DONE));

    /* Clear interrupt */
    MMIO32(DMA_INTCLR) = (1u << EVENT_DONE);
    pass &= check("INTMIS clear after INTCLR", MMIO32(DMA_INTMIS), 0u);

    /* ── DMAKILL stops a running channel ── */
    uart_puts("\n[6] DMAKILL stops a running channel\n");
    {
        /* Build a slow program on channel 0: infinite loop (DMALPEND forever)
         * so the channel stays EXECUTING long enough for us to kill it. */
        unsigned kill_pc = RAM_BASE + 0x80300u;
        unsigned p = kill_pc;
        MMIO8(p) = 0x18u; p += 1;             /* DMANOP */
        unsigned loop_body = p;
        MMIO8(p) = 0x18u; p += 1;             /* DMANOP (loop body) */
        unsigned loop_end = p;
        MMIO8(p) = 0x28u;                     /* DMALPEND forever, backwards jump */
        MMIO8(p + 1) = (unsigned char)(loop_end - loop_body + 1u);
        p += 2;

        /* Launch channel 0 again (it's STOPPED from test 3-5, so this is OK) */
        MMIO32(DMA_DBGINST0) = (0xA0u << 16) | (0x00u << 24);
        MMIO32(DMA_DBGINST1) = kill_pc;
        MMIO32(DMA_DBGCMD)   = 0u;

        unsigned csr_running = MMIO32(DMA_CSR0);
        pass &= check("channel running before kill", csr_running & 0xFu, 0x1u /* STATUS_EXECUTING */);

        /* DBGINST0 for DMAKILL: channel_thread=1, channel=0, opcode=0x01 in byte0 */
        MMIO32(DMA_DBGINST0) = (0x01u << 16) | (0x00u << 24) | (0u << 8) | 1u;
        MMIO32(DMA_DBGINST1) = 0u;
        MMIO32(DMA_DBGCMD)   = 0u;

        unsigned csr_after_kill = MMIO32(DMA_CSR0);
        pass &= check("channel stopped after DMAKILL", csr_after_kill & 0xFu, STATUS_STOPPED);
    }

    /* ── undefined opcode triggers a fault ── */
    uart_puts("\n[7] Undefined opcode triggers a fault on channel 1\n");
    {
        const unsigned DMA_CSR1 = DMA_BASE + 0x108u;  /* CSR0 + 1*stride(0x008) */
        const unsigned DMA_FTR1 = DMA_BASE + 0x044u;  /* FTR0 + 1*4 */
        const unsigned FTR_UNDEF_INSTR = (1u << 0);

        unsigned bad_pc = RAM_BASE + 0x80400u;
        MMIO8(bad_pc) = 0xFFu; /* not a valid DMA opcode */

        /* Launch channel 1 (byte1 channel field, DBGINST0 bits[10:8]) */
        MMIO32(DMA_DBGINST0) = (0xA0u << 16) | (0x01u << 24);
        MMIO32(DMA_DBGINST1) = bad_pc;
        MMIO32(DMA_DBGCMD)   = 0u;

        for (volatile int i = 0; i < 1000; i++) { }  
        unsigned csr1 = MMIO32(DMA_CSR1);
        pass &= check("channel 1 faulting", csr1 & 0xFu, 0xFu /* STATUS_FAULTING */);

        unsigned ftr1 = MMIO32(DMA_FTR1);
        pass &= check("FTR1 has UNDEF_INSTR bit", ftr1 & FTR_UNDEF_INSTR, FTR_UNDEF_INSTR);
    }

    /* ── Result ── */
    uart_puts("\n");
    uart_puts(pass ? "DMA PASS\n" : "DMA FAIL\n");
    for (;;) __asm__ volatile("wfi");
    return 0;
}