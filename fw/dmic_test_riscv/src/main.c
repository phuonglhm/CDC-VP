// Author: hoangv11

/* DMIC SoC verification firmware.
 *
 * Expected path:
 *   CPU -> bus_router -> DMIC MMIO
 *   DMIC Watermark -> PLIC source 1 -> CPU machine external interrupt
 */

#define UART_TX (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY1_ADDR 0x0C000004u
#define PLIC_ENABLE_ADDR 0x0C002000u
#define PLIC_THRESHOLD_ADDR 0x0C200000u
#define PLIC_CLAIM_ADDR 0x0C200004u

#define DMIC_BASE_ADDR 0x10060000u
#define DMIC_CTRL_ADDR (DMIC_BASE_ADDR + 0x00u)
#define DMIC_STATUS_ADDR (DMIC_BASE_ADDR + 0x04u)
#define DMIC_DATA_ADDR (DMIC_BASE_ADDR + 0x08u)
#define DMIC_FIFO_WM_ADDR (DMIC_BASE_ADDR + 0x0Cu)
#define DMIC_INT_CLR_ADDR (DMIC_BASE_ADDR + 0x10u)

#define MMIO32(addr) (*(volatile unsigned int *)(addr))

#define DMIC_CTRL_EN (1u << 0)
#define DMIC_CTRL_INT_EN (1u << 1)
#define DMIC_CTRL_DEC_SHIFT 8

#define DMIC_STATUS_FE (1u << 0)
#define DMIC_STATUS_FF (1u << 1)
#define DMIC_STATUS_OE (1u << 2)
#define DMIC_STATUS_WM (1u << 3)

#define DMIC_INT_CLR_OE (1u << 0)
#define DMIC_INT_CLR_WM (1u << 1)

#define MIE_MEIE (1u << 11)
#define MSTATUS_MIE (1u << 3)

static volatile unsigned dmic_irq_done = 0;
static volatile unsigned dmic_sample = 0;

static void uart_putc(char c) {
   UART_TX = (unsigned char)c;
}

static void uart_puts(const char *s) {
   while (*s) {
      uart_putc(*s++);
   }
}

static void uart_put_hex32(unsigned value) {
   static const char hex[] = "0123456789ABCDEF";
   uart_puts("0x");
   for (int shift = 28; shift >= 0; shift -= 4) {
      uart_putc(hex[(value >> shift) & 0xFu]);
   }
}

static void mmio_write32(const char *name, unsigned addr, unsigned value) {
   unsigned old_mstatus;
   __asm__ volatile("csrrc %0, mstatus, %1" : "=r"(old_mstatus) : "r"(MSTATUS_MIE) : "memory");

   MMIO32(addr) = value;
   uart_puts("WRITE ");
   uart_puts(name);
   uart_puts(" [");
   uart_put_hex32(addr);
   uart_puts("] <= ");
   uart_put_hex32(value);
   uart_puts("\n");

   if ((old_mstatus & MSTATUS_MIE) != 0u) {
      unsigned tmp;
      __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE) : "memory");
   }
}

static unsigned mmio_read32(const char *name, unsigned addr) {
   const unsigned value = MMIO32(addr);
   uart_puts("READ  ");
   uart_puts(name);
   uart_puts(" [");
   uart_put_hex32(addr);
   uart_puts("] => ");
   uart_put_hex32(value);
   uart_puts("\n");
   return value;
}

void __attribute__((interrupt("machine"))) trap_handler(void) {
   unsigned mcause;
   __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

   if ((mcause & 0x7FFFFFFFu) == 11u) {
      const unsigned id = mmio_read32("PLIC_CLAIM", PLIC_CLAIM_ADDR);
      if (id == 1u) {
         dmic_sample = mmio_read32("DMIC_DATA", DMIC_DATA_ADDR);
         dmic_irq_done = 1u;
         uart_puts("DMIC IRQ triggered\n");
         uart_puts("DMIC sample=");
         uart_put_hex32(dmic_sample);
         uart_puts("\n");

         mmio_write32("DMIC_INT_CLR", DMIC_INT_CLR_ADDR, DMIC_INT_CLR_WM);

         unsigned int ctrl = mmio_read32("DMIC_CONTROL", DMIC_CTRL_ADDR);
         mmio_write32("DMIC_CONTROL", DMIC_CTRL_ADDR, ctrl & ~DMIC_CTRL_INT_EN);
      }
      mmio_write32("PLIC_CLAIM", PLIC_CLAIM_ADDR, id);
   }
}

int main(void) {
   uart_puts("DMIC platform start\n");

   __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

   mmio_write32("PLIC_PRIORITY1", PLIC_PRIORITY1_ADDR, 1u);
   mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << 1));
   mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);

   unsigned tmp;
   __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
   __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

   mmio_write32("DMIC_FIFO_WM", DMIC_FIFO_WM_ADDR, 1u);

   unsigned int ctrl_val = DMIC_CTRL_EN | DMIC_CTRL_INT_EN | (64u << DMIC_CTRL_DEC_SHIFT);
   mmio_write32("DMIC_CONTROL", DMIC_CTRL_ADDR, ctrl_val);

   while (!dmic_irq_done) {
      __asm__ volatile("wfi");
   }

   const unsigned dmic_status = mmio_read32("DMIC_STATUS", DMIC_STATUS_ADDR);
   if ((dmic_status & DMIC_STATUS_OE) == 0u) {
      uart_puts("DMIC TEST PASS\n");
   } else {
      uart_puts("DMIC TEST FAIL\n");
   }

   for (;;) {
      __asm__ volatile("wfi");
   }

   return 0;
}
