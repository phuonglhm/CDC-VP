//author: linhtk55-fpt

#define UART_TX (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY1_ADDR (*(volatile unsigned int*)(0x0C000004u))
#define PLIC_ENABLE_ADDR (*(volatile unsigned int*)(0x0C002000u))
#define PLIC_THRESHOLD_ADDR (*(volatile unsigned int *)(0x0C200000u))
#define PLIC_CLAIM_ADDR (*(volatile unsigned int*)(0x0C200004u))

#define TIMER_BASE 0x10030000u
#define TIMER_CTRL (*(volatile unsigned int*)(TIMER_BASE + 0x00u))
#define TIMER_VALUE (*(volatile unsigned int*)(TIMER_BASE + 0x04u))
#define TIMER_RELOAD (*(volatile unsigned int*)(TIMER_BASE + 0x08u))
#define TIMER_INTSTATUS (*(volatile unsigned int*)(TIMER_BASE + 0x0Cu))

#define TIMER_CTRL_ENABLE (1u << 0)
#define TIMER_CTRL_INTEN (1u << 3)
#define TIMER_INT_CLEAR (1u << 0)

#define TIMER_PLIC_SOURCE 1u

#define MIE_MEIE (1u << 11)
#define MSTATUS_MIE (1u << 3)

static void uart_putc(const char c)
{
   UART_TX = (unsigned char)c;
}

static void uart_puts(const char *s)
{
   while (*s)
   {
      uart_putc(*s++);
   }
}

static void uart_put_hex32(unsigned value)
{
   static const char hex[] = "0123456789ABCDEF";
   uart_puts("0x");
   for (int shift = 28; shift >= 0; shift -= 4)
   {
      uart_putc(hex[(value >> shift) & 0xFu]);
   }
}

// Global flag to tell the main loop we survived the interrupt
static volatile int timer_fired = 0;

// static void mmio_write32(const char *name, unsigned addr, unsigned value) {
//    unsigned old_mstatus;
//    __asm__ volatile("csrrc %0, mstatus, %1" : "=r"(old_mstatus) : "r"(MSTATUS_MIE) : "memory");

//    MMIO32(addr) = value;
//    uart_puts("WRITE ");
//    uart_puts(name);
//    uart_puts(" [");
//    uart_put_hex32(addr);
//    uart_puts("] <= ");
//    uart_put_hex32(value);
//    uart_puts("\n");

//    if ((old_mstatus & MSTATUS_MIE) != 0u) {
//       unsigned tmp;
//       __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE) : "memory");
//    }
// }

// static unsigned mmio_read32(const char *name, unsigned addr) {
//    const unsigned value = MMIO32(addr);
//    uart_puts("READ  ");
//    uart_puts(name);
//    uart_puts(" [");
//    uart_put_hex32(addr);
//    uart_puts("] => ");
//    uart_put_hex32(value);
//    uart_puts("\n");
//    return value;
// }

void __attribute__((interrupt("machine"))) trap_handler(void)
{
   unsigned int mcause;
   __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
   unsigned int stval = 0u;
   __asm__ volatile("csrr %0, stval" : "=r"(stval));
      unsigned int mepc = 0u;
      __asm__ volatile("csrr %0, mepc" : "=r"(mepc));

   unsigned int claim_id = PLIC_CLAIM_ADDR;

   uart_puts("[TRAP] mcause=");
   uart_put_hex32(mcause);
   uart_puts(" claim=");
   uart_put_hex32(claim_id);
   uart_puts("\n");

   if (((mcause >> 31) & 1u) && ((mcause & 0x7FFFFFFFu) == 11u)) {
      if (claim_id == TIMER_PLIC_SOURCE)
      {
         uart_puts("[TRAP] Timer interrupt received!\n");
         TIMER_INTSTATUS = TIMER_INT_CLEAR;
         timer_fired = 1;
      }
      else
      {
         uart_puts("[TRAP] Unknown interrupt received!\n");
      }

      PLIC_CLAIM_ADDR = claim_id;
   } else {
      uart_puts("[TRAP] Non-external trap\n");
      if ((mcause & 0x7FFFFFFFu) == 15u) {
         uart_puts("[TRAP] Store page fault stval=");
         uart_put_hex32(stval);
         uart_puts(" mepc=");
         uart_put_hex32(mepc);
         uart_puts("\n");
      }
   }
}

int main(void)
{
   uart_puts("Starting generic timer testing...\n");

   __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

   PLIC_PRIORITY1_ADDR = 1u; 
   PLIC_ENABLE_ADDR |= (1u << TIMER_PLIC_SOURCE); // Unmask Source 1
   PLIC_THRESHOLD_ADDR = 0u;// Allow all priorities to pass through

   // Enable CPU interrupts
   unsigned tmp;
   __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
   __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

   uart_puts("Arming timer with 50,000 ticks...\n");

   TIMER_RELOAD = 50000u;

   // Write to CTRL to enable the countdown and allow it to assert the IRQ pin
   TIMER_CTRL = TIMER_CTRL_ENABLE | TIMER_CTRL_INTEN;

   //Wait for the hardware to do its job
   uart_puts("Waiting for interrupt (WFI)...\n");
   while (!timer_fired)
   {
   // Wait For Interrupt instruction puts the CPU to sleep until the trap handler fires
      __asm__ volatile("wfi");
   }

   // 6. Test Passed
   uart_puts("SUCCESS: Timer test passed!\n");
   TIMER_CTRL = 0;               // disable timer
   PLIC_CLAIM_ADDR = TIMER_PLIC_SOURCE; 
   // Park the CPU
   for (;;)
   {
      __asm__ volatile("wfi");
   }
   return 0;
}
