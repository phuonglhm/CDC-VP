// Author: hoangv11
// Verified by: quannh107

/* WDT SoC verification firmware — unified test suite
 * Hardware wiring (from wdt_platform_top.cpp):
 *   WDT irq   → plic.irq_in[4]  → PLIC source 5
 *   WDT reset → platform handle_wdt_reset() → cpu.reset_cpu()
 *   tick period = 10 ns
 */

#include <stdint.h>

#define UART_TX (*(volatile unsigned char *)0x10000000u)
#define MMIO32(addr) (*(volatile uint32_t *)(addr))

#define PLIC_BASE 0x0C000000u
#define PLIC_PRIORITY5_ADDR (PLIC_BASE + 0x000014u)
#define PLIC_ENABLE_ADDR (PLIC_BASE + 0x02000u)
#define PLIC_THRESHOLD_ADDR (PLIC_BASE + 0x200000u)
#define PLIC_CLAIM_ADDR (PLIC_BASE + 0x200004u)

#define WDT_BASE 0x10040000u
#define WDT_LOAD_ADDR (WDT_BASE + 0x000u)
#define WDT_VALUE_ADDR (WDT_BASE + 0x004u)
#define WDT_CTRL_ADDR (WDT_BASE + 0x008u)
#define WDT_INTCLR_ADDR (WDT_BASE + 0x00Cu)
#define WDT_RIS_ADDR (WDT_BASE + 0x010u)
#define WDT_MIS_ADDR (WDT_BASE + 0x014u)
#define WDT_LOCK_ADDR (WDT_BASE + 0xC00u)

#define WDT_CTRL_INTEN (1u << 0)
#define WDT_CTRL_RESEN (1u << 1)
#define WDT_LOCK_UNLOCK 0x1ACCE551u

#define MIE_MEIE (1u << 11)
#define MSTATUS_MIE (1u << 3)

#define MAGIC_FLAG_ADDR 0x800FFF00u
#define MAGIC_VAL 0xDEADBEEFu

static volatile uint32_t wdt_irq_count = 0u;
static volatile uint32_t wdt_ris_in_isr = 0u;
static volatile uint32_t wdt_irq_cleared = 0u;

static void uart_putc(char c) {
   UART_TX = (unsigned char)c;
}

static void uart_puts(const char *s) {
   while (*s)
      uart_putc(*s++);
}

static void uart_put_hex32(uint32_t v) {
   static const char hex[] = "0123456789ABCDEF";
   uart_puts("0x");
   for (int sh = 28; sh >= 0; sh -= 4)
      uart_putc(hex[(v >> sh) & 0xFu]);
}

static void uart_put_uint(uint32_t v) {
   if (v == 0) {
      uart_putc('0');
      return;
   }
   char buf[10];
   int i = 0;
   while (v) {
      buf[i++] = (char)('0' + v % 10);
      v /= 10;
   }
   while (i--)
      uart_putc(buf[i + 1]);
}

static void mmio_write32(const char *name, uint32_t addr, uint32_t value) {
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

static uint32_t mmio_read32(const char *name, uint32_t addr) {
   uint32_t v = MMIO32(addr);
   uart_puts("READ  ");
   uart_puts(name);
   uart_puts(" [");
   uart_put_hex32(addr);
   uart_puts("] => ");
   uart_put_hex32(v);
   uart_putc('\n');
   return v;
}

void __attribute__((interrupt("machine"))) trap_handler(void) {
   uint32_t mcause;
   __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

   if (((mcause >> 31) & 1u) && ((mcause & 0x7FFFFFFFu) == 11u)) {
      const uint32_t id = MMIO32(PLIC_CLAIM_ADDR);

      if (id == 5u) {
         wdt_irq_count++;

         uart_puts("WDT IRQ triggered (count=");
         uart_put_uint(wdt_irq_count);
         uart_puts(")\n");

         if (wdt_irq_count == 1u) {
            wdt_ris_in_isr = MMIO32(WDT_RIS_ADDR);
            MMIO32(WDT_INTCLR_ADDR) = 1u;
            wdt_irq_cleared = 1u;
            uart_puts("IRQ cleared — counter reloaded\n");
         } else {
            uart_puts("Leaving WDT active — waiting for reset...\n");
         }
      }

      MMIO32(PLIC_CLAIM_ADDR) = id;
   }
}

int main(void) {
   uint32_t test_failed = 0u;

   if (MMIO32(MAGIC_FLAG_ADDR) == MAGIC_VAL) {
      uart_puts("\n--- Warm Boot Detected ---\n");
      uart_puts("Magic flag found: Watchdog reset successful.\n");
      uart_puts("\nWDT TEST PASS\n");
      MMIO32(MAGIC_FLAG_ADDR) = 0u;
      for (;;)
         __asm__ volatile("wfi");
   }

   uart_puts("WDT platform start (cold boot)\n");

   MMIO32(MAGIC_FLAG_ADDR) = MAGIC_VAL;

   __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

   mmio_write32("PLIC_PRIORITY5", PLIC_PRIORITY5_ADDR, 1u);
   mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << 5));
   mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);

   uint32_t tmp;
   __asm__ volatile("csrrs %0, mie,     %1" : "=r"(tmp) : "r"(MIE_MEIE));
   __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

   uart_puts("\n--- Phase 1: Lock Mechanism ---\n");

   mmio_write32("WDT_LOCK", WDT_LOCK_ADDR, 0x00000000u);

   uint32_t initial_load = mmio_read32("WDT_LOAD", WDT_LOAD_ADDR);

   mmio_write32("WDT_LOAD", WDT_LOAD_ADDR, initial_load ^ 0x55AA55AAu);
   uint32_t locked_readback = mmio_read32("WDT_LOAD", WDT_LOAD_ADDR);

   if (locked_readback == initial_load) {
      uart_puts("PASS: Write blocked while locked.\n");
   } else {
      uart_puts("FAIL: Write accepted while locked!\n");
      test_failed = 1u;
   }

   mmio_write32("WDT_LOCK", WDT_LOCK_ADDR, WDT_LOCK_UNLOCK);
   mmio_write32("WDT_LOAD", WDT_LOAD_ADDR, 10000u);
   uint32_t unlocked_readback = mmio_read32("WDT_LOAD", WDT_LOAD_ADDR);

   if (unlocked_readback == 10000u) {
      uart_puts("PASS: Write accepted after unlock.\n");
   } else {
      uart_puts("FAIL: Write rejected after unlock!\n");
      test_failed = 1u;
   }

   uart_puts("\n--- Phase 2: IRQ Test (INTEN=1, RESEN=0) ---\n");

   mmio_write32("WDT_CONTROL", WDT_CTRL_ADDR, WDT_CTRL_INTEN);

   for (int i = 0; i < 3; i++)
      mmio_read32("WDT_VALUE", WDT_VALUE_ADDR);

   uart_puts("Waiting for WDT IRQ...\n");
   while (!wdt_irq_cleared) {
      __asm__ volatile("wfi");
   }

   uint32_t ris_after = mmio_read32("WDT_RIS", WDT_RIS_ADDR);

   if (wdt_ris_in_isr != 0u) {
      uart_puts("PASS: RIS was set when IRQ fired.\n");
   } else {
      uart_puts("FAIL: RIS was 0 inside ISR!\n");
      test_failed = 1u;
   }

   if (ris_after == 0u) {
      uart_puts("PASS: RIS cleared after INTCLR write.\n");
   } else {
      uart_puts("FAIL: RIS still set after INTCLR!\n");
      test_failed = 1u;
   }

   if (test_failed) {
      uart_puts("\nWDT TEST FAIL (phases 1-2)\n");
      for (;;)
         __asm__ volatile("wfi");
   }

   uart_puts("\n--- Phase 3: Reset Test (INTEN=1, RESEN=1) ---\n");

   mmio_write32("WDT_CONTROL", WDT_CTRL_ADDR, 0u);
   mmio_write32("WDT_LOAD", WDT_LOAD_ADDR, 5000u);
   mmio_write32("WDT_CONTROL", WDT_CTRL_ADDR, WDT_CTRL_INTEN | WDT_CTRL_RESEN);

   uart_puts("WDT armed with RESEN. Awaiting system reset...\n");

   for (;;)
      __asm__ volatile("wfi");

   return 0;
}
