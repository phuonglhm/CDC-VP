/* SPDX-License-Identifier: Apache-2.0
 *
 * FX1 hardware enumeration and non-destructive register diagnostics.
 *
 * Base addresses come only from soc_memory_map.h. Register offsets without a
 * shared firmware ABI header are the small, read-only/RW subset documented by
 * the corresponding component model. No decode-miss access is attempted:
 * the Bremen CPU would turn that TLM error into a synchronous exception owned
 * by the FreeRTOS trap handler.
 */

#include "hw_diag.h"

#include "FreeRTOS.h"
#include "task.h"

#include "soc/regs/soc_regs_dma.h"
#include "soc/regs/soc_regs_i2c.h"
#include "soc/regs/soc_regs_npu_v4.h"
#include "soc/regs/soc_regs_timer.h"
#include "soc/soc_memory_map.h"
#include "uart.h"

#include <stddef.h>
#include <stdint.h>

/* PrimeCell identification registers shared by SPI, TIMER, WDT, and DMA. */
#define PRIME_PID0 0xFE0u

/* UART (components/uart2_tlm). */
#define UART_FR 0x018u
#define UART_IFLS 0x034u
#define UART_FR_TXFE (1u << 7)
#define UART_FR_RXFE (1u << 4)

/* SPI/PL022 (components/spi_tlm). */
#define SPI_CR0 0x000u

/* WDT/SP805 (components/wdt_tlm). */
#define WDT_PID0_VALUE 0x05u

/* PWM (components/pwm_tlm). */
#define PWM_REGWEN 0x004u
#define PWM_INVERT 0x010u

/* TRNG (components/trng_tlm). */
#define TRNG_IMR 0x100u
#define TRNG_SAMPLE_CNT1 0x130u

/* CMU (components/clkmgr_tlm). */
#define CMU_CLK_ENABLES 0x018u

/* PMU (components/pmu_tlm). */
#define PMU_INTR_STATE 0x000u
#define PMU_INTR_ENABLE 0x004u
#define PMU_INTR_TEST 0x008u
#define PMU_CONTROL 0x014u

/* DMIC (components/dmic_tlm). */
#define DMIC_STATUS 0x004u
#define DMIC_FIFO_WM 0x00Cu
#define DMIC_STATUS_FIFO_EMPTY (1u << 0)

/* OTP (components/otp_tlm). */
#define OTP_STATUS 0x010u
#define OTP_CHECK_TIMEOUT 0x0A0u
#define OTP_MODEL_WORDS 0x200u

/* QSPI (components/qspi_tlm). */
#define QSPI_CTRL 0x000u
#define QSPI_STATUS 0x004u
#define QSPI_CMD 0x008u
#define QSPI_ADDR 0x00Cu
#define QSPI_LEN 0x010u
#define QSPI_CFG 0x014u
#define QSPI_DATA 0x018u
#define QSPI_INT_EN 0x01Cu
#define QSPI_INT_STATUS 0x020u
#define QSPI_START 0x024u
#define QSPI_CTRL_EN (1u << 0)
#define QSPI_INT_DONE (1u << 0)
#define QSPI_CMD_RDID 0x9Fu
#define QSPI_JEDEC_ID 0x00EF4018u

/* RTC (components/rtc_tlm). */
#define RTC_DR 0x000u
#define RTC_MR 0x004u
#define RTC_CR 0x00Cu

/* ADC (components/adc_tlm). */
#define ADC_CONTROL 0x000u
#define ADC_STATUS 0x004u
#define ADC_INTR_ENABLE 0x00Cu
#define ADC_CTRL_START (1u << 0)
#define ADC_CTRL_ENABLE (1u << 1)
#define ADC_STATUS_EOC (1u << 0)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static inline uint32_t mmio_read(uint32_t base, uint32_t offset) {
  const uint32_t value = *(volatile uint32_t *)(uintptr_t)(base + offset);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return value;
}

static inline void mmio_write(uint32_t base, uint32_t offset, uint32_t value) {
  *(volatile uint32_t *)(uintptr_t)(base + offset) = value;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

/* The legacy PL022 model is the one exception in this platform: its APB
 * target explicitly accepts transfers no wider than 16 bits. */
static inline uint32_t mmio_read16(uint32_t base, uint32_t offset) {
  const uint32_t value = *(volatile uint16_t *)(uintptr_t)(base + offset);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return value;
}

static inline void mmio_write16(uint32_t base, uint32_t offset,
                                uint32_t value) {
  *(volatile uint16_t *)(uintptr_t)(base + offset) = (uint16_t)value;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

static size_t text_length(const char *text) {
  size_t length = 0u;

  while (text[length] != '\0') {
    ++length;
  }
  return length;
}

static void print_field(const char *text, size_t width) {
  const size_t length = text_length(text);

  uart_puts(text);
  for (size_t i = length; i < width; ++i) {
    uart_putc(' ');
  }
}

struct scan_summary {
  uint32_t implemented;
  uint32_t reserved;
  uint32_t mismatches;
};

static void scan_probe(struct scan_summary *summary, const char *name,
                       uint32_t base, uint32_t offset, uint32_t expected,
                       uint32_t mask, const char *identity) {
  const uint32_t value = mmio_read(base, offset);
  const int match = mask == 0u || (value & mask) == (expected & mask);

  ++summary->implemented;
  if (!match) {
    ++summary->mismatches;
  }

  uart_puts("  ");
  print_field(name, 9u);
  uart_put_hex32("", base, "  ");
  uart_put_hex32("", offset, "  ");
  uart_put_hex32("", value, "  ");
  uart_puts(match ? identity : "MISMATCH");
  uart_puts("\n");
}

static void scan_value(struct scan_summary *summary, const char *name,
                       uint32_t base, uint32_t offset, uint32_t value,
                       uint32_t expected, uint32_t mask, const char *identity) {
  const int match = (value & mask) == (expected & mask);

  ++summary->implemented;
  if (!match) {
    ++summary->mismatches;
  }

  uart_puts("  ");
  print_field(name, 9u);
  uart_put_hex32("", base, "  ");
  uart_put_hex32("", offset, "  ");
  uart_put_hex32("", value, "  ");
  uart_puts(match ? identity : "MISMATCH");
  uart_puts("\n");
}

static void scan_probe16(struct scan_summary *summary, const char *name,
                         uint32_t base, uint32_t offset, uint32_t expected,
                         uint32_t mask, const char *identity) {
  const uint32_t value = mmio_read16(base, offset);
  const int match = (value & mask) == (expected & mask);

  ++summary->implemented;
  if (!match) {
    ++summary->mismatches;
  }

  uart_puts("  ");
  print_field(name, 9u);
  uart_put_hex32("", base, "  ");
  uart_put_hex32("", offset, "  ");
  uart_put_hex32("", value, "  ");
  uart_puts(match ? identity : "MISMATCH");
  uart_puts("\n");
}

static void scan_reserved(struct scan_summary *summary, const char *name,
                          uint32_t base) {
  ++summary->reserved;
  uart_puts("  ");
  print_field(name, 9u);
  uart_put_hex32("", base, "  ");
  uart_puts("----------  ----------  reserved\n");
}

struct qspi_saved_state {
  uint32_t ctrl;
  uint32_t cmd;
  uint32_t addr;
  uint32_t len;
  uint32_t cfg;
  uint32_t int_en;
};

static uint32_t qspi_read_jedec(int *done_seen, int *done_cleared) {
  struct qspi_saved_state saved;

  saved.ctrl = mmio_read(CDC_QSPI0_BASE, QSPI_CTRL);
  saved.cmd = mmio_read(CDC_QSPI0_BASE, QSPI_CMD);
  saved.addr = mmio_read(CDC_QSPI0_BASE, QSPI_ADDR);
  saved.len = mmio_read(CDC_QSPI0_BASE, QSPI_LEN);
  saved.cfg = mmio_read(CDC_QSPI0_BASE, QSPI_CFG);
  saved.int_en = mmio_read(CDC_QSPI0_BASE, QSPI_INT_EN);

  /* Keep the PLIC line masked; INT_STATUS still records completion. */
  mmio_write(CDC_QSPI0_BASE, QSPI_INT_EN, 0u);
  mmio_write(CDC_QSPI0_BASE, QSPI_INT_STATUS, QSPI_INT_DONE);
  mmio_write(CDC_QSPI0_BASE, QSPI_CTRL, QSPI_CTRL_EN);
  mmio_write(CDC_QSPI0_BASE, QSPI_CMD, QSPI_CMD_RDID);
  mmio_write(CDC_QSPI0_BASE, QSPI_ADDR, 0u);
  mmio_write(CDC_QSPI0_BASE, QSPI_LEN, 3u);
  mmio_write(CDC_QSPI0_BASE, QSPI_CFG, 1u);
  mmio_write(CDC_QSPI0_BASE, QSPI_START, 1u);

  if (done_seen != NULL) {
    *done_seen =
        (mmio_read(CDC_QSPI0_BASE, QSPI_INT_STATUS) & QSPI_INT_DONE) != 0u;
  }

  const uint32_t byte0 = mmio_read(CDC_QSPI0_BASE, QSPI_DATA) & 0xFFu;
  const uint32_t byte1 = mmio_read(CDC_QSPI0_BASE, QSPI_DATA) & 0xFFu;
  const uint32_t byte2 = mmio_read(CDC_QSPI0_BASE, QSPI_DATA) & 0xFFu;
  const uint32_t jedec = (byte0 << 16) | (byte1 << 8) | byte2;

  mmio_write(CDC_QSPI0_BASE, QSPI_INT_STATUS, QSPI_INT_DONE);
  if (done_cleared != NULL) {
    *done_cleared =
        (mmio_read(CDC_QSPI0_BASE, QSPI_INT_STATUS) & QSPI_INT_DONE) == 0u;
  }

  mmio_write(CDC_QSPI0_BASE, QSPI_CMD, saved.cmd);
  mmio_write(CDC_QSPI0_BASE, QSPI_ADDR, saved.addr);
  mmio_write(CDC_QSPI0_BASE, QSPI_LEN, saved.len);
  mmio_write(CDC_QSPI0_BASE, QSPI_CFG, saved.cfg);
  mmio_write(CDC_QSPI0_BASE, QSPI_INT_EN, saved.int_en);
  mmio_write(CDC_QSPI0_BASE, QSPI_CTRL, saved.ctrl);
  return jedec;
}

void hw_scan_run(void) {
  struct scan_summary summary = {0u, 0u, 0u};
  const uint32_t i2c_idle = CDC_I2C_STATUS_FMTEMPTY | CDC_I2C_STATUS_HOSTIDLE |
                            CDC_I2C_STATUS_TARGETIDLE | CDC_I2C_STATUS_RXEMPTY |
                            CDC_I2C_STATUS_TXEMPTY | CDC_I2C_STATUS_ACQEMPTY;
  const uint32_t jedec = qspi_read_jedec(NULL, NULL);

  uart_puts("\n=== FX1 Hardware Scan ===\n");
  uart_puts("  IP       BASE        PROBE       VALUE       RESULT\n");
  uart_puts("  -------- ----------  ----------  ----------  --------\n");

  scan_probe(&summary, "BOOTROM0", CDC_BOOTROM_BASE, 0u, 0u, 0u, "mapped");
  scan_probe(&summary, "IFLASH0", CDC_IFLASH_BASE, 0u, 0u, 0u, "mapped");
  scan_probe(&summary, "CLINT0", CDC_CLINT_BASE, CDC_CLINT_MTIME, 0u, 0u,
             "mtime");
  scan_probe(&summary, "PLIC0", CDC_PLIC_BASE, CDC_PLIC_THRESHOLD, 0u,
             0xFFFFFFFFu, "threshold");
  scan_probe(&summary, "UART0", CDC_UART0_BASE, UART_FR, 0u, 0u, "PL011");
  scan_probe(&summary, "I2C0", CDC_I2C0_BASE, CDC_I2C_STATUS, i2c_idle,
             i2c_idle, "idle");
  scan_probe16(&summary, "SPI0", CDC_SPI0_BASE, PRIME_PID0, 0x22u, 0xFFu,
               "PL022");
  scan_probe(&summary, "TIMER0", CDC_TIMER0_BASE, PRIME_PID0, 0x01u, 0xFFu,
             "timer");
  scan_probe(&summary, "WDT0", CDC_WDT0_BASE, PRIME_PID0, WDT_PID0_VALUE, 0xFFu,
             "SP805");
  scan_probe(&summary, "PWM0", CDC_PWM0_BASE, PWM_REGWEN, 1u, 1u, "enabled");
  scan_probe(&summary, "DMA0", CDC_DMA0_BASE, PRIME_PID0, 0x30u, 0xFFu,
             "PL330");
  scan_probe(&summary, "TRNG0", CDC_TRNG0_BASE, TRNG_SAMPLE_CNT1, 0xFFFFu,
             0xFFFFFFFFu, "idle");
  scan_probe(&summary, "CMU0", CDC_CMU0_BASE, CMU_CLK_ENABLES, 0x0Fu, 0x0Fu,
             "clocks-on");
  scan_probe(&summary, "PMU0", CDC_PMU0_BASE, PMU_CONTROL, 0x180u, 0x1F1u,
             "active");
  scan_probe(&summary, "DMIC0", CDC_DMIC0_BASE, DMIC_STATUS,
             DMIC_STATUS_FIFO_EMPTY, DMIC_STATUS_FIFO_EMPTY, "fifo-empty");
  scan_probe(&summary, "OTP0", CDC_OTP0_BASE, OTP_MODEL_WORDS, 256u,
             0xFFFFFFFFu, "256 words");
  scan_value(&summary, "QSPI0", CDC_QSPI0_BASE, QSPI_CMD, jedec, QSPI_JEDEC_ID,
             0x00FFFFFFu, "JEDEC EF4018");

  scan_reserved(&summary, "ISP0", CDC_ISP0_BASE);
  scan_reserved(&summary, "VPU0", CDC_VPU0_BASE);

#if defined(DEMO_NPU)
  scan_probe(&summary, "NPU0", CDC_NPU0_BASE, CDC_NPU_CORE_ID,
             CDC_NPU_CORE_ID_VALUE, 0xFFFFFFFFu, "SAU4");
#else
  scan_reserved(&summary, "NPU0", CDC_NPU0_BASE);
#endif

  scan_probe(&summary, "UART1", CDC_UART1_BASE, UART_FR,
             UART_FR_TXFE | UART_FR_RXFE, UART_FR_TXFE | UART_FR_RXFE, "PL011");
  scan_probe(&summary, "I2C1", CDC_I2C1_BASE, CDC_I2C_STATUS, i2c_idle,
             i2c_idle, "idle");
  scan_probe16(&summary, "SPI1", CDC_SPI1_BASE, PRIME_PID0, 0x22u, 0xFFu,
               "PL022");
  scan_probe(&summary, "TIMER1", CDC_TIMER1_BASE, PRIME_PID0, 0x01u, 0xFFu,
             "timer");
  scan_probe(&summary, "RTC0", CDC_RTC0_BASE, RTC_CR, 0u, 1u, "disabled");
  scan_probe(&summary, "ADC0", CDC_ADC0_BASE, ADC_STATUS, 0u, ADC_STATUS_EOC,
             "idle");
  scan_probe(&summary, "GPIO0", CDC_GPIO0_BASE, CDC_GPIO_VALUE, 0u, 0u,
             "32-pin");

  if (summary.mismatches == 0u) {
    uart_put_u32("HW_SCAN PASS implemented=", summary.implemented, " ");
    uart_put_u32("reserved=", summary.reserved, "\n");
  } else {
    uart_put_u32("HW_SCAN FAIL mismatches=", summary.mismatches, " ");
    uart_put_u32("implemented=", summary.implemented, " ");
    uart_put_u32("reserved=", summary.reserved, "\n");
  }
}

struct reg_result {
  const char *kind;
  const char *name;
  uint32_t address;
  uint32_t actual;
  uint32_t expected;
  int pass;
};

#define REG_RESULTS_MAX 48u
static struct reg_result reg_results[REG_RESULTS_MAX];
static size_t reg_result_count;

static void reg_record(const char *kind, const char *name, uint32_t address,
                       uint32_t actual, uint32_t expected, int pass) {
  if (reg_result_count < ARRAY_SIZE(reg_results)) {
    struct reg_result *const result = &reg_results[reg_result_count++];
    result->kind = kind;
    result->name = name;
    result->address = address;
    result->actual = actual;
    result->expected = expected;
    result->pass = pass;
  }
}

static void reg_ro(const char *name, uint32_t base, uint32_t offset,
                   uint32_t expected, uint32_t mask) {
  const uint32_t actual = mmio_read(base, offset) & mask;
  const uint32_t wanted = expected & mask;

  reg_record("RO", name, base + offset, actual, wanted, actual == wanted);
}

static void reg_rw(const char *name, uint32_t base, uint32_t offset,
                   uint32_t pattern, uint32_t mask) {
  const uint32_t old_value = mmio_read(base, offset);

  mmio_write(base, offset, pattern);
  const uint32_t actual = mmio_read(base, offset) & mask;
  const uint32_t wanted = pattern & mask;
  mmio_write(base, offset, old_value);
  const uint32_t restored = mmio_read(base, offset) & mask;

  reg_record("RW", name, base + offset, actual, wanted,
             actual == wanted && restored == (old_value & mask));
}

static void reg_ro16(const char *name, uint32_t base, uint32_t offset,
                     uint32_t expected, uint32_t mask) {
  const uint32_t actual = mmio_read16(base, offset) & mask;
  const uint32_t wanted = expected & mask;

  reg_record("RO", name, base + offset, actual, wanted, actual == wanted);
}

static void reg_rw16(const char *name, uint32_t base, uint32_t offset,
                     uint32_t pattern, uint32_t mask) {
  const uint32_t old_value = mmio_read16(base, offset);

  mmio_write16(base, offset, pattern);
  const uint32_t actual = mmio_read16(base, offset) & mask;
  const uint32_t wanted = pattern & mask;
  mmio_write16(base, offset, old_value);
  const uint32_t restored = mmio_read16(base, offset) & mask;

  reg_record("RW", name, base + offset, actual, wanted,
             actual == wanted && restored == (old_value & mask));
}

static void reg_test_clint(void) {
  const uint32_t before = mmio_read(CDC_CLINT_BASE, CDC_CLINT_MTIME);

  vTaskDelay(pdMS_TO_TICKS(1));
  const uint32_t after = mmio_read(CDC_CLINT_BASE, CDC_CLINT_MTIME);
  reg_record("RO", "CLINT0.MTIME", CDC_CLINT_BASE + CDC_CLINT_MTIME, after,
             before, after != before);
}

static void reg_test_timer1(void) {
  const uint32_t old_ctrl = mmio_read(CDC_TIMER1_BASE, CDC_TIMER_CTRL);
  const uint32_t old_value = mmio_read(CDC_TIMER1_BASE, CDC_TIMER_VALUE);
  const uint32_t old_reload = mmio_read(CDC_TIMER1_BASE, CDC_TIMER_RELOAD);
  const uint32_t pattern = 0xA55A1234u;

  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_CTRL, 0u);
  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_INTSTATUS, 1u);
  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_RELOAD, pattern);
  const uint32_t rw_actual = mmio_read(CDC_TIMER1_BASE, CDC_TIMER_RELOAD);

  /* RELOAD=1 makes INTSTATUS assert after two 20 ns model ticks. Keep the
   * interrupt enable bit clear so this cannot reach the PLIC. */
  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_RELOAD, 1u);
  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_CTRL, CDC_TIMER_CTRL_ENABLE);
  vTaskDelay(pdMS_TO_TICKS(1));
  const uint32_t asserted =
      mmio_read(CDC_TIMER1_BASE, CDC_TIMER_INTSTATUS) & 1u;
  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_CTRL, 0u);
  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_INTSTATUS, 1u);
  const uint32_t cleared = mmio_read(CDC_TIMER1_BASE, CDC_TIMER_INTSTATUS) & 1u;

  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_RELOAD, old_reload);
  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_VALUE, old_value);
  mmio_write(CDC_TIMER1_BASE, CDC_TIMER_CTRL, old_ctrl);

  reg_record("RW", "TIMER1.RELOAD", CDC_TIMER1_BASE + CDC_TIMER_RELOAD,
             rw_actual, pattern, rw_actual == pattern);
  reg_record("W1C", "TIMER1.INTSTATUS", CDC_TIMER1_BASE + CDC_TIMER_INTSTATUS,
             cleared, 0u, asserted == 1u && cleared == 0u);
}

static void reg_test_pmu_w1c(void) {
  const uint32_t old_enable = mmio_read(CDC_PMU0_BASE, PMU_INTR_ENABLE);

  mmio_write(CDC_PMU0_BASE, PMU_INTR_ENABLE, 0u);
  mmio_write(CDC_PMU0_BASE, PMU_INTR_STATE, 1u);
  mmio_write(CDC_PMU0_BASE, PMU_INTR_TEST, 1u);
  const uint32_t asserted = mmio_read(CDC_PMU0_BASE, PMU_INTR_STATE) & 1u;
  mmio_write(CDC_PMU0_BASE, PMU_INTR_STATE, 1u);
  const uint32_t cleared = mmio_read(CDC_PMU0_BASE, PMU_INTR_STATE) & 1u;
  mmio_write(CDC_PMU0_BASE, PMU_INTR_ENABLE, old_enable);

  reg_record("W1C", "PMU0.INTR_STATE", CDC_PMU0_BASE + PMU_INTR_STATE, cleared,
             0u, asserted == 1u && cleared == 0u);
}

static void reg_test_adc_w1c(void) {
  const uint32_t old_control = mmio_read(CDC_ADC0_BASE, ADC_CONTROL);
  const uint32_t old_enable = mmio_read(CDC_ADC0_BASE, ADC_INTR_ENABLE);

  mmio_write(CDC_ADC0_BASE, ADC_INTR_ENABLE, 0u);
  mmio_write(CDC_ADC0_BASE, ADC_STATUS, ADC_STATUS_EOC);
  mmio_write(CDC_ADC0_BASE, ADC_CONTROL, ADC_CTRL_ENABLE | ADC_CTRL_START);
  const uint32_t asserted =
      mmio_read(CDC_ADC0_BASE, ADC_STATUS) & ADC_STATUS_EOC;
  mmio_write(CDC_ADC0_BASE, ADC_STATUS, ADC_STATUS_EOC);
  const uint32_t cleared =
      mmio_read(CDC_ADC0_BASE, ADC_STATUS) & ADC_STATUS_EOC;
  mmio_write(CDC_ADC0_BASE, ADC_CONTROL, old_control);
  mmio_write(CDC_ADC0_BASE, ADC_INTR_ENABLE, old_enable);

  reg_record("W1C", "ADC0.STATUS.EOC", CDC_ADC0_BASE + ADC_STATUS, cleared, 0u,
             asserted == ADC_STATUS_EOC && cleared == 0u);
}

static void reg_test_qspi_w1c(void) {
  int done_seen = 0;
  int done_cleared = 0;
  const uint32_t jedec = qspi_read_jedec(&done_seen, &done_cleared);

  reg_record("W1C", "QSPI0.DONE+JEDEC", CDC_QSPI0_BASE + QSPI_INT_STATUS,
             done_cleared ? 0u : 1u, 0u,
             done_seen && done_cleared && jedec == QSPI_JEDEC_ID);
}

static void reg_print_results(void) {
  uint32_t passed = 0u;

  uart_puts("\n=== FX1 Safe Register Test ===\n");
  uart_puts("  TYPE  REGISTER                   WRITE ADDR  WRITE VALUE  "
            "READ ADDR   READ VALUE   RESULT\n");
  uart_puts("  ----  -------------------------  ----------  -----------  "
            "----------  -----------  ------\n");

  for (size_t i = 0u; i < reg_result_count; ++i) {
    const struct reg_result *const result = &reg_results[i];
    const int is_ro = result->kind[0] == 'R' && result->kind[1] == 'O';

    uart_puts("  ");
    print_field(result->kind, 6u);
    print_field(result->name, 27u);
    if (is_ro) {
      uart_puts("----------  ----------  ");
    } else if (result->kind[0] == 'W') {
      /* Every W1C check in this matrix clears bit 0 with a write of one. */
      uart_put_hex32("", result->address, "  ");
      uart_put_hex32("", 1u, "  ");
    } else {
      uart_put_hex32("", result->address, "  ");
      uart_put_hex32("", result->expected, "  ");
    }
    uart_put_hex32("", result->address, "  ");
    uart_put_hex32("", result->actual, "  ");
    if (result->pass) {
      ++passed;
      uart_puts("PASS\n");
    } else {
      uart_puts("FAIL expected=");
      uart_put_hex32("", result->expected, "\n");
    }
  }

  if (passed == reg_result_count) {
    uart_put_u32("REG_TEST PASS ", passed, "/");
    uart_put_u32("", (uint32_t)reg_result_count, "\n");
  } else {
    uart_put_u32("REG_TEST FAIL ", passed, "/");
    uart_put_u32("", (uint32_t)reg_result_count, "\n");
  }
}

void reg_test_run(void) {
  const uint32_t i2c_idle = CDC_I2C_STATUS_FMTEMPTY | CDC_I2C_STATUS_HOSTIDLE |
                            CDC_I2C_STATUS_TARGETIDLE | CDC_I2C_STATUS_RXEMPTY |
                            CDC_I2C_STATUS_TXEMPTY | CDC_I2C_STATUS_ACQEMPTY;

  reg_result_count = 0u;

  /* Read-only/reset/identity checks. */
  reg_test_clint();
  reg_ro("UART1.FR", CDC_UART1_BASE, UART_FR, UART_FR_TXFE | UART_FR_RXFE,
         UART_FR_TXFE | UART_FR_RXFE);
  reg_ro("I2C0.STATUS", CDC_I2C0_BASE, CDC_I2C_STATUS, i2c_idle, i2c_idle);
  reg_ro("I2C1.STATUS", CDC_I2C1_BASE, CDC_I2C_STATUS, i2c_idle, i2c_idle);
  reg_ro16("SPI0.PERIPHID0", CDC_SPI0_BASE, PRIME_PID0, 0x22u, 0xFFu);
  reg_ro16("SPI1.PERIPHID0", CDC_SPI1_BASE, PRIME_PID0, 0x22u, 0xFFu);
  reg_ro("TIMER0.PID0", CDC_TIMER0_BASE, PRIME_PID0, 0x01u, 0xFFu);
  reg_ro("TIMER1.PID0", CDC_TIMER1_BASE, PRIME_PID0, 0x01u, 0xFFu);
  reg_ro("WDT0.PERIPHID0", CDC_WDT0_BASE, PRIME_PID0, WDT_PID0_VALUE, 0xFFu);
  reg_ro("PWM0.REGWEN", CDC_PWM0_BASE, PWM_REGWEN, 1u, 1u);
  reg_ro("DMA0.PERIPHID0", CDC_DMA0_BASE, PRIME_PID0, 0x30u, 0xFFu);
  reg_ro("TRNG0.SAMPLE_CNT1", CDC_TRNG0_BASE, TRNG_SAMPLE_CNT1, 0xFFFFu,
         0xFFFFFFFFu);
  reg_ro("CMU0.CLK_ENABLES", CDC_CMU0_BASE, CMU_CLK_ENABLES, 0x0Fu, 0x0Fu);
  reg_ro("PMU0.CONTROL", CDC_PMU0_BASE, PMU_CONTROL, 0x180u, 0x1F1u);
  reg_ro("DMIC0.STATUS", CDC_DMIC0_BASE, DMIC_STATUS, DMIC_STATUS_FIFO_EMPTY,
         DMIC_STATUS_FIFO_EMPTY);
  reg_ro("OTP0.MODEL_WORDS", CDC_OTP0_BASE, OTP_MODEL_WORDS, 256u, 0xFFFFFFFFu);
  reg_ro("RTC0.CR", CDC_RTC0_BASE, RTC_CR, 0u, 1u);
  reg_ro("ADC0.STATUS", CDC_ADC0_BASE, ADC_STATUS, 0u, ADC_STATUS_EOC);
  reg_ro("GPIO0.DIR", CDC_GPIO0_BASE, CDC_GPIO_DIR, 0u, 0xFFFFFFFFu);
#if defined(DEMO_NPU)
  reg_ro("NPU0.CORE_ID", CDC_NPU0_BASE, CDC_NPU_CORE_ID, CDC_NPU_CORE_ID_VALUE,
         0xFFFFFFFFu);
#endif

  /* Reversible RW checks. Every helper restores and verifies the old value. */
  reg_rw("UART1.IFLS", CDC_UART1_BASE, UART_IFLS, 0x2Du, 0x3Fu);
  reg_rw("I2C0.INTR_ENABLE", CDC_I2C0_BASE, CDC_I2C_INTR_ENABLE, 0x155u,
         0xFFFFFFFFu);
  reg_rw("I2C1.INTR_ENABLE", CDC_I2C1_BASE, CDC_I2C_INTR_ENABLE, 0x2AAu,
         0xFFFFFFFFu);
  reg_rw16("SPI1.CR0", CDC_SPI1_BASE, SPI_CR0, 0x0007u, 0xFFFFu);
  reg_rw("PWM0.INVERT", CDC_PWM0_BASE, PWM_INVERT, 0x15u, 0x3Fu);
  reg_rw("DMA0.INTEN", CDC_DMA0_BASE, CDC_DMA_INTEN, 0x5A5A5A5Au, 0xFFFFFFFFu);
  reg_rw("TRNG0.IMR", CDC_TRNG0_BASE, TRNG_IMR, 0x0Au, 0x0Fu);
  reg_rw("CMU0.CLK_ENABLES", CDC_CMU0_BASE, CMU_CLK_ENABLES, 0x05u, 0x0Fu);
  reg_rw("PMU0.INTR_ENABLE", CDC_PMU0_BASE, PMU_INTR_ENABLE, 1u, 1u);
  reg_rw("DMIC0.FIFO_WM", CDC_DMIC0_BASE, DMIC_FIFO_WM, 8u, 0xFFFFFFFFu);
  reg_rw("OTP0.CHECK_TIMEOUT", CDC_OTP0_BASE, OTP_CHECK_TIMEOUT, 0x1234u,
         0xFFFFFFFFu);
  reg_rw("QSPI0.CFG", CDC_QSPI0_BASE, QSPI_CFG, 4u, 0xFFFFFFFFu);
  reg_rw("RTC0.MR", CDC_RTC0_BASE, RTC_MR, 0x12345678u, 0xFFFFFFFFu);
  reg_rw("ADC0.INTR_ENABLE", CDC_ADC0_BASE, ADC_INTR_ENABLE, 1u, 1u);
  reg_rw("GPIO0.OUT", CDC_GPIO0_BASE, CDC_GPIO_OUT, 0xA5A55A5Au, 0xFFFFFFFFu);
  reg_rw("GPIO0.DIR", CDC_GPIO0_BASE, CDC_GPIO_DIR, 0x80000000u, 0xFFFFFFFFu);
#if defined(DEMO_NPU)
  reg_rw("NPU0.K_DIMENSION", CDC_NPU0_BASE, CDC_NPU_K_DIMENSION, 31u,
         0xFFFFFFFFu);
#endif

  /* Functional sticky-cause checks: observe hardware/software set, W1C,
   * then leave the interrupt source masked and state cleared. */
  reg_test_timer1();
  reg_test_pmu_w1c();
  reg_test_adc_w1c();
  reg_test_qspi_w1c();

  reg_print_results();
}
