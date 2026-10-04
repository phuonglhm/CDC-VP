/*
 * fw_main_example.c -- bare-metal firmware example: runs a HasNpuTop program on the NPU of a system platform through
 * the wrapper's instruction alias, and prints one line per instruction on the UART.
 *
 * It is written against the platform's own firmware headers (memory map with the NPU and UART base addresses) and
 * follows the structure of the platform's existing NPU firmware test. Adapt the three items marked ADAPT.
 *
 * Program and image are linked into the ELF by the file that make_fw_program.py --asm generates:
 *   npu_has_stream : the program stream
 *   npu_has_image  : dram_init.bin, placed by the linker script at a fixed RAM address (linker_example.ld)
 * A whole network needs an image of 86 MB (YOLOv8m) or 114 MB (ViT-B/16); when the platform's ELF loader cannot take
 * that much, load the image into RAM by another platform mechanism and pass its address to npu_has_replay().
 *
 * Expected output: "NPU HAS start", one line per failed step (or per step with NPU_FW_VERBOSE), "NPU HAS PASS".
 */
#include <stdint.h>

#include "soc/soc_memory_map.h" /* ADAPT: provides CDC_NPU0_BASE and CDC_UART0_BASE */

#include "npu_has_fw.h"

extern const uint32_t npu_has_stream[];
extern uint8_t npu_has_image[];

#define UART_TX (*(volatile uint8_t *)(uintptr_t)CDC_UART0_BASE) /* ADAPT: console output */

static void uart_puts(const char *s)
{
    while (*s != '\0')
        UART_TX = (uint8_t)*s++;
}

static void uart_put_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    int shift;
    uart_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4)
        UART_TX = (uint8_t)hex[(v >> shift) & 0xFu];
}

/* ---- hardware abstraction of npu_has_fw.h ---- */
void npu_hal_write32(uint32_t off, uint32_t v) { *(volatile uint32_t *)(uintptr_t)(CDC_NPU0_BASE + off) = v; }
uint32_t npu_hal_read32(uint32_t off) { return *(volatile uint32_t *)(uintptr_t)(CDC_NPU0_BASE + off); }

void npu_hal_ram_read(uint32_t phys, uint8_t *dst, uint32_t n)
{
    const volatile uint8_t *p = (const volatile uint8_t *)(uintptr_t)phys;
    while (n--)
        *dst++ = *p++;
}

void npu_hal_ram_write(uint32_t phys, const uint8_t *src, uint32_t n)
{
    volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)phys;
    while (n--)
        *p++ = *src++;
}

/* ADAPT: polling is used here. With interrupts, enable the wrapper's done/error causes and wait for the NPU
 * interrupt instead, as the platform's NPU firmware test does. */
void npu_hal_idle(void) {}

void npu_hal_report(char kind, uint32_t index, int ok, uint32_t status, uint64_t cycles)
{
#ifndef NPU_FW_VERBOSE
    if (ok)
        return;
#endif
    uart_puts(kind == 'H' ? "host " : "inst ");
    uart_put_hex32(index);
    uart_puts(ok ? " PASS status=" : " FAIL status=");
    uart_put_hex32(status);
    uart_puts(" cycles=");
    uart_put_hex32((uint32_t)cycles);
    uart_puts("\n");
}

int main(void)
{
    npu_has_result res;
    int rc;

    uart_puts("NPU HAS start\n");
    rc = npu_has_replay(npu_has_stream, (uint32_t)(uintptr_t)npu_has_image, 0u, &res);
    if (rc == 0) {
        uart_puts("NPU HAS PASS instructions=");
        uart_put_hex32(res.instructions);
        uart_puts(" cycles=");
        uart_put_hex32((uint32_t)res.exec_cycles);
        uart_puts("\n");
    } else {
        uart_puts("NPU HAS FAIL rc=");
        uart_put_hex32((uint32_t)rc);
        uart_puts(" first=");
        uart_put_hex32(res.first_failed);
        uart_puts(" error=");
        uart_put_hex32(res.last_error);
        uart_puts("\n");
    }
    for (;;) {
    }
}
