/* SPDX-License-Identifier: Apache-2.0
 *
 * Step 12.8 interactive console for the FlooNoC-backed noc_soc platform.
 *
 * RX is not a firmware backdoor: host bytes enter UartTLM's pin-side RX FIFO,
 * assert PLIC source 1, and are drained by the BSP ISR through UART MMIO over
 * FlooNoC. The CLI task arms that path early but does not consume commands
 * until the complete Step 12.4 workload has reported PASS.
 */

#include "cli.h"

#include "FreeRTOS.h"
#include "task.h"

#include "hw_diag.h"
#include "soc/noc_dashboard_protocol.h"
#include "uart.h"

#include <stddef.h>
#include <stdint.h>

#define CLI_LINE_MAX 96u

typedef int (*cli_handler_t)(void);

typedef struct {
    const char *name;
    const char *help;
    cli_handler_t handler;
} cli_command_t;

static int cmd_soc(void);
static int cmd_help(void);
static int cmd_noc_dashboard(void);
static int cmd_hw_scan(void);
static int cmd_reg_test(void);

static const cli_command_t commands[] = {
    {"help", "Show available commands", cmd_help},
    {"soc", "Show the FlooNoC platform topology and address map", cmd_soc},
    {"noc_dashboard", "Request the full host-rendered NoC dashboard",
     cmd_noc_dashboard},
    {"hw_scan", "Scan mapped FlooNoC SoC hardware", cmd_hw_scan},
    {"reg_test", "Run safe restoring RO, RW and W1C checks", cmd_reg_test},
};

static void print_commands(void)
{
    uart_puts("Commands:\n");
    for (size_t index = 0u;
         index < sizeof(commands) / sizeof(commands[0]); ++index) {
        uart_puts("  ");
        uart_puts(commands[index].name);
        uart_puts(" : ");
        uart_puts(commands[index].help);
        uart_puts("\n");
    }
}

static int str_eq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static int cmd_help(void)
{
    print_commands();
    return 0;
}

static int cmd_soc(void)
{
    uart_puts("CLI soc mesh=4x4 managers=cpu,dma,probe\n");
    uart_puts("CLI soc cpu=0,0 dma=0,3 ram=0,1 plic=3,0\n");
    uart_puts("CLI soc RAM=0x80000000 UART0=0x10000000 PLIC=0x0C000000\n");
    return 0;
}

static int cmd_noc_dashboard(void)
{
    uart_puts("CLI noc_dashboard host request\n");
    uart_puts(CDC_NOC_DASHBOARD_REQUEST);
    return 0;
}

static int cmd_hw_scan(void)
{
    hw_scan_run();
    return 0;
}

static int cmd_reg_test(void)
{
    reg_test_run();
    return 0;
}

static int dispatch(char *line)
{
    size_t first = 0u;
    size_t end = 0u;

    while (line[first] == ' ' || line[first] == '\t') {
        ++first;
    }
    while (line[end] != '\0') {
        ++end;
    }
    while (end > first &&
           (line[end - 1u] == ' ' || line[end - 1u] == '\t')) {
        --end;
    }
    line[end] = '\0';

    if (line[first] == '\0') {
        return -1;
    }

    for (size_t index = 0u;
         index < sizeof(commands) / sizeof(commands[0]); ++index) {
        if (str_eq(&line[first], commands[index].name)) {
            return commands[index].handler();
        }
    }

    uart_puts("command not found: ");
    uart_puts(&line[first]);
    uart_puts(" (valid: ");
    for (size_t index = 0u;
         index < sizeof(commands) / sizeof(commands[0]); ++index) {
        if (index != 0u) {
            uart_puts(", ");
        }
        uart_puts(commands[index].name);
    }
    uart_puts(")\n");
    return 0;
}

static void print_banner(void)
{
    uart_puts("\n========================================\n");
    uart_puts("  FlooNoC Virtual SoC FreeRTOS Console\n");
    uart_puts("  UART0 RX -> PLIC source 1 -> CPU/NoC\n");
    uart_puts("========================================\n");
    print_commands();
}

void noc_cli_task(void *params)
{
    char line[CLI_LINE_MAX];
    int skip_lf_after_cr = 0;

    (void)params;

    uart_rx_start();
    uart_puts("FreeRTOS NoC CLI RX armed source=1\n");

    if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 1u) {
        uart_puts("FreeRTOS NoC CLI FAIL: release notification\n");
        vTaskDelete(NULL);
    }

    print_banner();
    uart_puts("FreeRTOS NoC CLI ready\n");

    for (;;) {
        size_t length = 0u;
        int overflow = 0;

        line[0] = '\0';
        uart_puts("noc@vp:~# ");

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
            uart_puts("input line too long\n");
        } else {
            (void)dispatch(line);
        }
    }
}
