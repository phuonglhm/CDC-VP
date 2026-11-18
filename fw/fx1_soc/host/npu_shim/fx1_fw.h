/*
 * Host stand-in for fx1_fw.h when drivers/fx1_npu.c runs against the SystemC
 * clone (host/test_npu_driver_host.cpp): the two MMIO accessors the driver
 * uses, implemented by the test as TLM transactions.
 */
#ifndef FX1_FW_H
#define FX1_FW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t fx1_read32(uintptr_t address);
void fx1_write32(uintptr_t address, uint32_t value);
#ifdef __cplusplus
}
#endif

#endif
