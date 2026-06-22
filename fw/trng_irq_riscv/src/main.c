//author: linhtk55-fpt

#define UART_TX (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY1_ADDR (*(volatile unsigned int*)(0x0C000004u))
#define PLIC_ENABLE_ADDR (*(volatile unsigned int*)(0x0C002000u))
#define PLIC_THRESHOLD_ADDR (*(volatile unsigned int *)(0x0C200000u))
#define PLIC_CLAIM_ADDR (*(volatile unsigned int*)(0x0C200004u))

#define TRNG_BASE 0x14700000u
#define TRNG_IMR (*(volatile unsigned int*)(TRNG_BASE + 0x100u))
#define TRNG_ICR (*(volatile unsigned int*)(TRNG_BASE + 0x108u))
#define TRNG_VALID (*(volatile unsigned int*)(TRNG_BASE + 0x110u))
#define TRNG_EHR_DATA0 (*(volatile unsigned int*)(TRNG_BASE + 0x114u))
#define TRNG_EHR_DATA1 (*(volatile unsigned int*)(TRNG_BASE + 0x118u))
#define TRNG_EHR_DATA2 (*(volatile unsigned int*)(TRNG_BASE + 0x11Cu))
#define TRNG_EHR_DATA3 (*(volatile unsigned int*)(TRNG_BASE + 0x120u))
#define TRNG_EHR_DATA4 (*(volatile unsigned int*)(TRNG_BASE + 0x124u))
#define TRNG_EHR_DATA5 (*(volatile unsigned int*)(TRNG_BASE + 0x128u))
#define TRNG_SRC_EN (*(volatile unsigned int*)(TRNG_BASE + 0x12Cu))

#define TRNG_PLIC_SOURCE 1u
#define TRNG_ENABLE 1u
#define TRNG_INT_CLEAR 1u

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
static volatile int trng_fired = 0;

void __attribute__((interrupt("machine"))) trap_handler(void)
{
   unsigned int mcause;
   __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
   // unsigned int stval = 0u;
   // __asm__ volatile("csrr %0, stval" : "=r"(stval));
   //    unsigned int mepc = 0u;
   //    __asm__ volatile("csrr %0, mepc" : "=r"(mepc));

   unsigned int claim_id = PLIC_CLAIM_ADDR;

   uart_puts("[TRAP] mcause=");
   uart_put_hex32(mcause);
   uart_puts(" claim=");
   uart_put_hex32(claim_id);
   uart_puts("\n");

   if (((mcause >> 31) & 1u) && ((mcause & 0x7FFFFFFFu) == 11u)) {
      if (claim_id == TRNG_PLIC_SOURCE)
      {
         uart_puts("[TRAP] TRNG interrupt received!\n");
         TRNG_ICR = TRNG_INT_CLEAR;
      }
      else
      {
         uart_puts("[TRAP] Unknown interrupt received!\n");
      }

      PLIC_CLAIM_ADDR = claim_id;
   } else {
      uart_puts("[TRAP] Non-external trap\n");
      // if ((mcause & 0x7FFFFFFFu) == 15u) {
      //    uart_puts("[TRAP] Store page fault stval=");
      //    uart_put_hex32(stval);
      //    uart_puts(" mepc=");
      //    uart_put_hex32(mepc);
      //    uart_puts("\n");
      // }
   }
}
int main(void) {
   uart_puts("Starting TRNG generation testing... \n");
   __asm__ volatile("csrw mtvec, %0"::"r"(trap_handler));
   PLIC_PRIORITY1_ADDR = 1u;
   PLIC_ENABLE_ADDR |= (1u << TRNG_PLIC_SOURCE); 
   PLIC_THRESHOLD_ADDR = 0u;

   unsigned tmp;
   __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
   __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));
   TRNG_IMR = 0u;

   uart_puts("Triggering TRNG random number generation...\n");

   // Write 1 to Source Enable to generate the 6 random numbers
   TRNG_SRC_EN = TRNG_ENABLE;

   uart_puts("Waiting for TRNG interrupt (WFI)...\n");
   while (!trng_fired) {
      // Sleep until the trap handler fires
      __asm__ volatile("wfi");
   }
   // Retrieve and print the data
   uart_puts("SUCCESS: TRNG generated data!\n");
   uart_puts("EHR_DATA0: "); uart_put_hex32(TRNG_EHR_DATA0); uart_puts("\n");
   uart_puts("EHR_DATA1: "); uart_put_hex32(TRNG_EHR_DATA1); uart_puts("\n");
   uart_puts("EHR_DATA2: "); uart_put_hex32(TRNG_EHR_DATA2); uart_puts("\n");
   uart_puts("EHR_DATA3: "); uart_put_hex32(TRNG_EHR_DATA3); uart_puts("\n");
   uart_puts("EHR_DATA4: "); uart_put_hex32(TRNG_EHR_DATA4); uart_puts("\n");
   uart_puts("EHR_DATA5: "); uart_put_hex32(TRNG_EHR_DATA5); uart_puts("\n");

   // Clean up and park the CPU
   TRNG_SRC_EN = 0; 
   
   for (;;) {
      __asm__ volatile("wfi");
   }
   return 0;
}
