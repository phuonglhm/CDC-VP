// author: linhtk55-fpt

#define UART_TX (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY1_ADDR (*(volatile unsigned int *)(0x0C000004u))
#define PLIC_ENABLE_ADDR (*(volatile unsigned int *)(0x0C002000u))
#define PLIC_THRESHOLD_ADDR (*(volatile unsigned int *)(0x0C200000u))
#define PLIC_CLAIM_ADDR (*(volatile unsigned int *)(0x0C200004u))

#define TRNG_BASE 0x14700000u
#define TRNG_IMR (*(volatile unsigned int *)(TRNG_BASE + 0x100u))
#define TRNG_ICR (*(volatile unsigned int *)(TRNG_BASE + 0x108u))
#define TRNG_VALID (*(volatile unsigned int *)(TRNG_BASE + 0x110u))
#define TRNG_EHR_DATA0 (*(volatile unsigned int *)(TRNG_BASE + 0x114u))
#define TRNG_EHR_DATA1 (*(volatile unsigned int *)(TRNG_BASE + 0x118u))
#define TRNG_EHR_DATA2 (*(volatile unsigned int *)(TRNG_BASE + 0x11Cu))
#define TRNG_EHR_DATA3 (*(volatile unsigned int *)(TRNG_BASE + 0x120u))
#define TRNG_EHR_DATA4 (*(volatile unsigned int *)(TRNG_BASE + 0x124u))
#define TRNG_EHR_DATA5 (*(volatile unsigned int *)(TRNG_BASE + 0x128u))
#define TRNG_SRC_EN (*(volatile unsigned int *)(TRNG_BASE + 0x12Cu))
#define TRNG_SW_RESET (*(volatile unsigned int *)(TRNG_BASE + 0x140u))
#define TRNG_RESET_BITS_COUNTER (*(volatile unsigned int *)(TRNG_BASE + 0x1BCu))

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

   unsigned int claim_id = PLIC_CLAIM_ADDR;

   uart_puts("[TRAP] mcause=");
   uart_put_hex32(mcause);
   uart_puts(" claim=");
   uart_put_hex32(claim_id);
   uart_puts("\n");

   if (((mcause >> 31) & 1u) && ((mcause & 0x7FFFFFFFu) == 11u))
   {
      if (claim_id == TRNG_PLIC_SOURCE)
      {
         uart_puts("[TRAP] TRNG interrupt received!\n");
         TRNG_ICR = TRNG_INT_CLEAR;
         trng_fired = 1;
      }
      else
      {
         uart_puts("[TRAP] Unknown interrupt received!\n");
      }

      PLIC_CLAIM_ADDR = claim_id;
   }
   else
   {
      uart_puts("[TRAP] Non-external trap\n");
   }
}
void test_trng_interrupt(void)
{
   uart_puts("\n--- Running TRNG Interrupt & Generation Test ---\n");
   trng_fired = 0; // Reset flag

   // Unmask TRNG internal interrupts
   TRNG_IMR = 0u;

   uart_puts("Triggering TRNG random number generation...\n");
   TRNG_SRC_EN = TRNG_ENABLE;

   uart_puts("Waiting for TRNG interrupt (WFI)...\n");
   while (!trng_fired)
   {
      __asm__ volatile("wfi");
   }

   uart_puts("SUCCESS: TRNG generated data!\n");
   // Retrieve and print the data
   uart_puts("EHR_DATA0: ");
   uart_put_hex32(TRNG_EHR_DATA0);
   uart_puts("\n");
   uart_puts("EHR_DATA1: ");
   uart_put_hex32(TRNG_EHR_DATA1);
   uart_puts("\n");
   uart_puts("EHR_DATA2: ");
   uart_put_hex32(TRNG_EHR_DATA2);
   uart_puts("\n");
   uart_puts("EHR_DATA3: ");
   uart_put_hex32(TRNG_EHR_DATA3);
   uart_puts("\n");
   uart_puts("EHR_DATA4: ");
   uart_put_hex32(TRNG_EHR_DATA4);
   uart_puts("\n");
   uart_puts("EHR_DATA5: ");
   uart_put_hex32(TRNG_EHR_DATA5);
   uart_puts("\n");
   TRNG_SRC_EN = 0; // Clean up
}

void test_trng_reset(void)
{
   uart_puts("\n--- Running TRNG Software Reset Test ---\n");

   //Dirty the TRNG state safely without firing an interrupt
   TRNG_IMR = 0xFu;  // Mask all interrupts 
   TRNG_SRC_EN = 1u; // Trigger generation

   // Check that data actually generated (VALID should be 1)
   if (TRNG_VALID == 0)
   {
      uart_puts("FAIL: TRNG did not generate data to setup the test.\n");
      return;
   }

   uart_puts("State dirtied. Triggering Software Reset...\n");

   // 2. Trigger the Software Reset
   TRNG_SW_RESET = 1u;

   // 3. Verify the hardware wiped the registers back to defaults
   int passed = 1;

   if (TRNG_IMR != 0xFu)
   {
      uart_puts("FAIL: IMR did not reset to 0xF\n");
      passed = 0;
   }
   if (TRNG_VALID != 0u)
   {
      uart_puts("FAIL: VALID flag did not clear to 0\n");
      passed = 0;
   }
   if (TRNG_EHR_DATA0 != 0u)
   {
      uart_puts("FAIL: EHR_DATA0 did not wipe to 0\n");
      passed = 0;
   }
   if (TRNG_SRC_EN != 0u)
   {
      uart_puts("FAIL: SRC_EN did not clear to 0\n");
      passed = 0;
   }

   if (passed)
   {
      uart_puts("SUCCESS: TRNG Software Reset works correctly!\n");
   }
}

void test_interrupt_masking(void)
{
   uart_puts("\n--- Running TRNG Interrupt Masking Test ---\n");
   TRNG_IMR = 0xFu;  // Mask all interrupts so the CPU isn't disturbed
   //flush out phantom interrupts
   trng_fired = 0;
   for (volatile int i = 0; i < 200; i++) {
      __asm__ volatile("nop");
   }
   TRNG_SRC_EN = TRNG_ENABLE; // Trigger generation
   //Poll the VALID register manually instead of using WFI
   while (TRNG_VALID == 0) {
      __asm__ volatile("nop");
   }
   // Verify that the interrupt DID NOT fire yet
   if (trng_fired != 0) {
      uart_puts("FAIL: Interrupt fired while masked!\n");
      TRNG_SRC_EN = 0;
      return;
   }
   uart_puts("SUCCESS: Data generated successfully while interrupt was masked.\n");
   //Unmask the interrupt. The pending IRQ should fire immediately.
   TRNG_IMR = 0u;
   int timeout = 1000;
   while (!trng_fired && timeout--) {
      __asm__ volatile("nop");
   }

   if (trng_fired == 1) {
      uart_puts("SUCCESS: Trap handler caught the unmasked pending interrupt!\n");
   } else {
      uart_puts("FAIL: Interrupt did not propagate after unmasking.\n");
   }

   TRNG_SRC_EN = 0;
}

void test_register_access(void) {
   uart_puts("\n--- Running Register Access Violations Test---\n");
   
   TRNG_IMR = 0xFu;
   TRNG_SRC_EN = 1u;
   while (TRNG_VALID == 0);

   unsigned int original_valid = TRNG_VALID;
   unsigned int original_data0 = TRNG_EHR_DATA0;
   int passed = 1;

   //Attempt to write garbage values to Read-Only registers
   TRNG_VALID = 0xDEADBEEFu;
   TRNG_EHR_DATA0 = 0xCAFEBABEu;

   // Verify values did not mutate
   if (TRNG_VALID != original_valid) {
      uart_puts("FAIL: Permitted write to Read-Only REG_VALID!\n");
      passed = 0;
   }
   if (TRNG_EHR_DATA0 != original_data0) {
      uart_puts("FAIL: Permitted write to Read-Only REG_EHR_DATA0!\n");
      passed = 0;
   }

   // 2. Attempt to read back Write-Only registers (should safely return 0)
   unsigned int icr_val = TRNG_ICR;
   unsigned int reset_bits_val = TRNG_RESET_BITS_COUNTER;

   if (icr_val != 0u) {
      uart_puts("FAIL: Reading Write-Only REG_ICR did not return 0\n");
      passed = 0;
   }
   if (reset_bits_val != 0u) {
      uart_puts("FAIL: Reading Write-Only REG_RESET_BITS_COUNTER did not return 0\n");
      passed = 0;
   }

   if (passed) {
      uart_puts("SUCCESS: Register RO/WO permissions verified!\n");
   }

   // Clear data out and stop engine
   TRNG_ICR = TRNG_INT_CLEAR;
   TRNG_SRC_EN = 0;
}

void test_reset_bits_counter(void) {
   uart_puts("\n--- Running Reset Bits Counter Test ---\n");
   
   TRNG_IMR = 0xFu;  // Mask interrupts
   TRNG_SRC_EN = 1u; // Trigger generation
   while (TRNG_VALID == 0);

   uart_puts("Data is valid. Disabling SRC_EN and firing bits counter reset...\n");
   
   // Turn source off first (mandatory conditional check in your C++ model)
   TRNG_SRC_EN = 0u; 
   
   // Write any value to trigger the bits counter reset logic
   TRNG_RESET_BITS_COUNTER = 1u;

   // Check if VALID dropped back to 0
   if (TRNG_VALID != 0u) {
      uart_puts("FAIL: VALID flag survived RESET_BITS_COUNTER execution!\n");
   } else {
      uart_puts("SUCCESS: RESET_BITS_COUNTER cleared the valid state cleanly!\n");
   }
}

void test_randomness(void) {
   uart_puts("\n--- Running Uniqueness Test ---\n");
   TRNG_IMR = 0xFu; // Mask interrupts for continuous sequential polling

   unsigned int sample1, sample2, sample3;

   // Capture Sample 1
   TRNG_SRC_EN = 1u;
   while (TRNG_VALID == 0);
   sample1 = TRNG_EHR_DATA0;
   TRNG_ICR = TRNG_INT_CLEAR; // Clear engine state
   TRNG_SRC_EN = 0u;

   // Capture Sample 2
   TRNG_SRC_EN = 1u;
   while (TRNG_VALID == 0);
   sample2 = TRNG_EHR_DATA0;
   TRNG_ICR = TRNG_INT_CLEAR;
   TRNG_SRC_EN = 0u;

   // Capture Sample 3
   TRNG_SRC_EN = 1u;
   while (TRNG_VALID == 0);
   sample3 = TRNG_EHR_DATA0;
   TRNG_ICR = TRNG_INT_CLEAR;
   TRNG_SRC_EN = 0u;

   uart_puts("Sample 1: "); uart_put_hex32(sample1); uart_puts("\n");
   uart_puts("Sample 2: "); uart_put_hex32(sample2); uart_puts("\n");
   uart_puts("Sample 3: "); uart_put_hex32(sample3); uart_puts("\n");

   // Ensure none of the data streams match each other
   if (sample1 == sample2 || sample2 == sample3 || sample1 == sample3) {
      uart_puts("FAIL: Duplicate consecutive entropy detected!\n");
   } else {
      uart_puts("SUCCESS: Dynamic entropy generation confirmed!\n");
   }
}

int main(void)
{
   uart_puts("=== Starting TRNG Test Suite ===\n");

   // Global Setup
   __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

   PLIC_PRIORITY1_ADDR = 1u;
   PLIC_ENABLE_ADDR |= (1u << TRNG_PLIC_SOURCE);
   PLIC_THRESHOLD_ADDR = 0u;

   unsigned tmp;
   __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
   __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));
   test_trng_interrupt();
   test_interrupt_masking();
   test_trng_reset();
   test_register_access();
   test_reset_bits_counter();
   test_randomness();
   uart_puts("\n=== All tests finished. Parking CPU. ===\n");
   for (;;)
   {
      __asm__ volatile("wfi");
   }
   return 0;
}
