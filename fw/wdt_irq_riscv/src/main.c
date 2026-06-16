/* WDT SoC verification firmware.
 *
 * Expected path:
 *   CPU -> bus_router -> WDT MMIO
 *   WDT Timeout -> PLIC source 5 -> CPU machine external interrupt
 *   WDT Second Timeout -> System Reset -> Reboot
 */

#include <stdint.h>

#define UART_TX (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY5_ADDR 0x0C000014u
#define PLIC_ENABLE_ADDR 0x0C002000u
#define PLIC_THRESHOLD_ADDR 0x0C200000u
#define PLIC_CLAIM_ADDR 0x0C200004u

#define WDT_BASE 0x10040000u
#define WDT_LOAD_ADDR (WDT_BASE + 0x000u)
#define WDT_VALUE_ADDR (WDT_BASE + 0x004u)
#define WDT_CONTROL_ADDR (WDT_BASE + 0x008u)
#define WDT_INTCLR_ADDR (WDT_BASE + 0x00Cu)
#define WDT_RIS_ADDR (WDT_BASE + 0x010u)
#define WDT_MIS_ADDR (WDT_BASE + 0x014u)
#define WDT_LOCK_ADDR (WDT_BASE + 0xC00u)

#define MMIO32(addr) (*(volatile uint32_t *)(addr))

#define WDT_CTRL_INTEN (1u << 0)
#define WDT_CTRL_RESEN (1u << 1)
#define WDT_LOCK_UNLOCK 0x1ACCE551u

#define MIE_MEIE (1u << 11)
#define MSTATUS_MIE (1u << 3)

/* RAM base is 0x80000000. Put magic flag at the end of 1MB RAM */
#define MAGIC_FLAG_ADDR 0x800FFF00u
#define MAGIC_VAL 0xDEADBEEFu

static volatile unsigned wdt_irq_count = 0;

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

static void mmio_write32(const char *name, uint32_t addr, uint32_t value) {
   MMIO32(addr) = value;
   uart_puts("WRITE ");
   uart_puts(name);
   uart_puts(" [");
   uart_put_hex32(addr);
   uart_puts("] <= ");
   uart_put_hex32(value);
   uart_puts("\n");
}

static uint32_t mmio_read32(const char *name, uint32_t addr) {
   const uint32_t value = MMIO32(addr);
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
   uint32_t mcause;
   __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

   if ((mcause & 0x7FFFFFFFu) == 11u) {
      const uint32_t id = MMIO32(PLIC_CLAIM_ADDR);
      if (id == 5u) {
         wdt_irq_count++;
         uart_puts("WDT IRQ Triggered (");
         uart_putc((char)('0' + wdt_irq_count));
         uart_puts(")\n");

         /* SABOTAGE: We do NOT clear the WDT interrupt here.
          * This will force the hardware to trigger a System Reset
          * on the NEXT timeout. */
         uart_puts("Intentionally leaving WDT active for RESET test...\n");
      }
      MMIO32(PLIC_CLAIM_ADDR) = id;
   }
}

int main(void) {
   uart_puts("\nWDT platform boot\n");

   /* 1. Detect if this is a COLD boot or a WARM (Watchdog) boot */
   if (MMIO32(MAGIC_FLAG_ADDR) == MAGIC_VAL) {
      uart_puts("Detected Magic Flag! Watchdog Reset successful.\n");
      uart_puts("WDT RESET TEST PASS\n");
      /* Clear flag for next manual run */
      MMIO32(MAGIC_FLAG_ADDR) = 0;
      for (;;) {
         __asm__ volatile("wfi");
      }
   }

   /* 2. Cold Boot Setup */
   uart_puts("Starting Cold Boot Sequence...\n");
   MMIO32(MAGIC_FLAG_ADDR) = MAGIC_VAL;

   __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

   // Configure PLIC
   mmio_write32("PLIC_PRIORITY5", PLIC_PRIORITY5_ADDR, 1u);
   mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << 5));
   mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);

   // Enable CPU interrupts
   uint32_t tmp;
   __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
   __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

   // 3. Unlock and Start WDT with Reset Enabled
   mmio_write32("WDT_LOCK", WDT_LOCK_ADDR, WDT_LOCK_UNLOCK);
   mmio_write32("WDT_LOAD", WDT_LOAD_ADDR, 5000u); // 50us timeout

   uart_puts("Enabling WDT stages: IRQ then RESET\n");
   mmio_write32("WDT_CONTROL", WDT_CONTROL_ADDR, WDT_CTRL_INTEN | WDT_CTRL_RESEN);

   uart_puts("Waiting for system destruction...\n");
   while (1) {
      __asm__ volatile("wfi");
   }

   return 0;
}
