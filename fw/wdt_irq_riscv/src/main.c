/* WDT SoC verification firmware.
 *
 * Expected path:
 *   CPU -> bus_router -> WDT MMIO
 *   WDT Timeout -> PLIC source 1 -> CPU machine external interrupt
 */

#define UART_TX (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY1_ADDR 0x0C000004u
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

#define MMIO32(addr) (*(volatile unsigned int *)(addr))

#define WDT_CTRL_INTEN (1u << 0)
#define WDT_CTRL_RESEN (1u << 1)
#define WDT_LOCK_UNLOCK 0x1ACCE551u

#define MIE_MEIE (1u << 11)
#define MSTATUS_MIE (1u << 3)

static volatile unsigned wdt_irq_done = 0;

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
         uart_puts("WDT IRQ triggered!\n");
         // Clear the interrupt at the source
         mmio_write32("WDT_INTCLR", WDT_INTCLR_ADDR, 1u);
         wdt_irq_done = 1u;
      }
      mmio_write32("PLIC_CLAIM", PLIC_CLAIM_ADDR, id);
   }
}

int main(void) {
   uart_puts("WDT platform start\n");

   __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

   // Configure PLIC
   mmio_write32("PLIC_PRIORITY1", PLIC_PRIORITY1_ADDR, 1u);
   mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << 1)); /* enable source id 1 */
   mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);

   // Enable CPU interrupts
   unsigned tmp;
   __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
   __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

   // 1. Test Lock Mechanism
   uart_puts("\n--- Testing Lock Mechanism ---\n");
   mmio_write32("WDT_LOCK", WDT_LOCK_ADDR, 0x0); // Ensure it is locked
   mmio_write32("WDT_LOAD", WDT_LOAD_ADDR, 0x55AA55AAu);
   unsigned load_val = mmio_read32("WDT_LOAD", WDT_LOAD_ADDR);
   if (load_val != 0x55AA55AAu) {
      uart_puts("SUCCESS: WDT_LOAD write blocked while locked.\n");
   } else {
      uart_puts("FAILURE: WDT_LOAD write allowed while locked!\n");
   }

   // 2. Unlock and Load
   uart_puts("\n--- Unlocking and Loading ---\n");
   mmio_write32("WDT_LOCK", WDT_LOCK_ADDR, WDT_LOCK_UNLOCK);
   mmio_write32("WDT_LOAD", WDT_LOAD_ADDR, 10000u); // 10000 ticks * 10ns = 100us
   load_val = mmio_read32("WDT_LOAD", WDT_LOAD_ADDR);
   if (load_val == 10000u) {
      uart_puts("SUCCESS: WDT_LOAD write allowed after unlock.\n");
   }

   // 3. Start WDT and wait for IRQ
   uart_puts("\n--- Starting WDT ---\n");
   mmio_write32("WDT_CONTROL", WDT_CONTROL_ADDR, WDT_CTRL_INTEN);

   // Poll current value to see if it is counting down
   for (int i = 0; i < 5; i++) {
      mmio_read32("WDT_VALUE", WDT_VALUE_ADDR);
   }

   uart_puts("Waiting for WDT Interrupt...\n");
   while (!wdt_irq_done) {
      __asm__ volatile("wfi");
   }

   // 4. Final Verification
   unsigned ris = mmio_read32("WDT_RIS", WDT_RIS_ADDR);
   if (wdt_irq_done && (ris == 0)) {
      uart_puts("\nWDT TEST PASS\n");
   } else {
      uart_puts("\nWDT TEST FAIL\n");
   }

   for (;;) {
      __asm__ volatile("wfi");
   }

   return 0;
}
