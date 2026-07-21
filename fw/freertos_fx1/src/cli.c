/* SPDX-License-Identifier: Apache-2.0
 *
 * FreeRTOS CLI foundation for the FX1 NPU/TFLite-Micro milestone.
 *
 * Command implementations live behind stable table entries so the shell
 * parser remains independent of TFLM and platform diagnostic details.
 */

#include "cli.h"

#include "FreeRTOS.h"

#include "hw_diag.h"
#include "uart.h"

#include <stddef.h>

#define CLI_LINE_MAX 96u

typedef void (*cli_handler_t)(void);

typedef struct {
  const char *name;
  const char *help;
  cli_handler_t handler;
} cli_command_t;

static void cmd_help(void);
static void cmd_tflm_hello(void);
static void cmd_tflm_run(void);
static void cmd_tflm_simple(void);
static void cmd_tflm_npu(void);
static void cmd_tflm_demo(void);
static void cmd_hw_scan(void);
static void cmd_reg_test(void);

#if defined(DEMO_TFLM)
extern int tflm_hello_run(void);
extern int tflm_run(void);
extern int tflm_simple_run(void);
#if defined(DEMO_NPU)
extern int tflm_npu_run(void);
extern int tflm_demo_run(void);
#endif
#endif

/* Keep this table aligned with docs/NPU_CLI_TFLM_PLAN.md Part B. No unrelated
 * Corstone-320 commands belong in this milestone. */
static const cli_command_t commands[] = {
    {"help", "List available commands", cmd_help},
    {"tflm_hello", "TFLite-Micro link and interpreter smoke test",
     cmd_tflm_hello},
    {"tflm_run", "Run the hello_world INT8 sine model on the CPU",
     cmd_tflm_run},
    {"tflm_simple", "Run the SECDA simple model CPU baseline", cmd_tflm_simple},
    {"tflm_npu", "Run the simple model with FX1 NPU offload", cmd_tflm_npu},
    {"tflm_demo", "Compare CPU and NPU outputs bit-exactly", cmd_tflm_demo},
    {"hw_scan", "Enumerate the implemented FX1 platform IP", cmd_hw_scan},
    {"reg_test", "Run safe RW, RO, and W1C register checks", cmd_reg_test},
};

static int str_eq(const char *a, const char *b) {
  while (*a != '\0' && *a == *b) {
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

static void cmd_help(void) {
  uart_puts("Commands:\n");
  for (size_t i = 0u; i < sizeof(commands) / sizeof(commands[0]); ++i) {
    uart_puts("  ");
    uart_puts(commands[i].name);
    uart_puts(" : ");
    uart_puts(commands[i].help);
    uart_puts("\n");
  }
}

static void cmd_tflm_hello(void) {
#if defined(DEMO_TFLM)
  (void)tflm_hello_run();
#else
  uart_puts("TFLM_HELLO unavailable: TFLite-Micro is not integrated yet\n");
#endif
}

static void cmd_tflm_run(void) {
#if defined(DEMO_TFLM)
  (void)tflm_run();
#else
  uart_puts("TFLM_RUN unavailable: CPU inference is not integrated yet\n");
#endif
}

static void cmd_tflm_simple(void) {
#if defined(DEMO_TFLM)
  (void)tflm_simple_run();
#else
  uart_puts("TFLM_SIMPLE unavailable: simple_model is not integrated yet\n");
#endif
}

static void cmd_tflm_npu(void) {
#if !defined(DEMO_NPU)
  uart_puts("TFLM_NPU unavailable: NPU absent\n");
#elif !defined(DEMO_TFLM)
  uart_puts("TFLM_NPU unavailable: TFLite-Micro is not integrated\n");
#else
  (void)tflm_npu_run();
#endif
}

static void cmd_tflm_demo(void) {
#if !defined(DEMO_NPU)
  uart_puts("TFLM_DEMO unavailable: NPU absent\n");
#elif !defined(DEMO_TFLM)
  uart_puts("TFLM_DEMO unavailable: TFLite-Micro is not integrated\n");
#else
  (void)tflm_demo_run();
#endif
}

static void cmd_hw_scan(void) { hw_scan_run(); }

static void cmd_reg_test(void) { reg_test_run(); }

static void dispatch(char *command) {
  size_t first = 0u;
  size_t end = 0u;

  while (command[first] == ' ' || command[first] == '\t') {
    ++first;
  }
  while (command[end] != '\0') {
    ++end;
  }
  while (end > first &&
         (command[end - 1u] == ' ' || command[end - 1u] == '\t')) {
    --end;
  }
  command[end] = '\0';

  if (command[first] == '\0') {
    return;
  }

  for (size_t i = 0u; i < sizeof(commands) / sizeof(commands[0]); ++i) {
    if (str_eq(&command[first], commands[i].name)) {
      commands[i].handler();
      return;
    }
  }

  uart_puts("sh: command not found: ");
  uart_puts(&command[first]);
  uart_puts(" (try 'help')\n");
}

static void dispatch_line(char *line) {
  char *command = line;

  /* Semicolon support keeps deterministic --uart0-rx-file scripts compact.
   * Empty segments are ignored. There are intentionally no arguments yet. */
  for (char *p = line;; ++p) {
    if (*p == ';' || *p == '\0') {
      const char terminator = *p;

      *p = '\0';
      dispatch(command);
      if (terminator == '\0') {
        return;
      }
      command = p + 1;
    }
  }
}

static void print_banner(void) {
  uart_puts("\n");
  uart_puts("========================================\n");
  uart_puts("  FX1 FreeRTOS NPU Console\n");
  uart_puts("  User: root | OS: FreeRTOS | Arch: RV32IMAC\n");
  uart_puts("========================================\n");
  uart_puts("Type 'help' to list available commands.\n");
}

void cli_run(void) {
  char line[CLI_LINE_MAX];
  int skip_lf_after_cr = 0;

  print_banner();

  for (;;) {
    size_t length = 0u;
    int overflow = 0;

    line[0] = '\0';
    uart_puts("root@fx1:~# ");

    for (;;) {
      char c;

      if (!uart_getc(&c, portMAX_DELAY)) {
        continue;
      }

      if (skip_lf_after_cr && c == '\n') {
        skip_lf_after_cr = 0;
        continue;
      }
      skip_lf_after_cr = 0;

      if (c == '\r' || c == '\n') {
        if (c == '\r') {
          skip_lf_after_cr = 1;
        }
        uart_puts("\n");
        break;
      }

      if (c == '\b' || c == 0x7F) {
        if (length > 0u) {
          --length;
          line[length] = '\0';
          uart_puts("\b \b");
        }
        continue;
      }

      if (c >= 0x20 && c < 0x7F) {
        if (length < sizeof(line) - 1u) {
          line[length++] = c;
          line[length] = '\0';
          uart_putc(c);
        } else {
          overflow = 1;
        }
      }
    }

    if (overflow) {
      uart_puts("sh: input line too long\n");
    } else {
      dispatch_line(line);
    }

    const uint32_t dropped = uart_rx_dropped();
    if (dropped != 0u) {
      uart_put_u32("UART RX warning: dropped ", dropped, " byte(s)\n");
    }
  }
}
