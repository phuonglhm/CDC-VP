/*
 * FX1 FreeRTOS CLI application: shared declarations of the command modules.
 * Every command prints its own result; failures are counted in
 * app_failures, which `poweroff` turns into the simulation verdict.
 */
#ifndef RTOS_APP_H
#define RTOS_APP_H

#include <stdint.h>

#include "FreeRTOS.h"

#define APP_VERSION "FX1 FreeRTOS CLI 1.0 (FreeRTOS V11.2.0, RV32IMAFC/ilp32f, AMP hart 0)"

extern volatile uint32_t app_failures;

/* Output helpers (CLI task context). */
void out(const char* s);
void out_hex(uint32_t v);
void out_dec(uint32_t v);
void out_line(const char* s);
/* Report one check: prints "  name: PASS|FAIL", counts failures, returns ok. */
int report(const char* name, int ok);

/* Number parsing: decimal or 0x hex; returns 0 on a malformed token or a
 * value above 0xFFFFFFFF. */
int parse_u32(const char* s, uint32_t* v);

/* Fault-tolerant 32-bit accesses (probe.S): 0 ok, 1 the access faulted. The
 * exception handler resumes after the faulting probe instead of halting. */
int fx1_probe_read32(uint32_t addr, uint32_t* value);
int fx1_probe_write32(uint32_t addr, uint32_t value);

/* Command modules. argv[0] is the command name. Return 0 when the command
 * itself ran (its checks may still fail), nonzero for a usage error. */
int cmd_isp(int argc, char** argv);
int cmd_dma(int argc, char** argv);
int cmd_fpu(int argc, char** argv);
#ifdef FX1_FW_HAS_CAMERA
int cmd_camera(int argc, char** argv);
#endif

/* Device setup after the scheduler starts (IRQ handlers, semaphores). */
void isp_service_init(void);
void dma_service_init(void);

/* ISP ownership between the `isp` commands and the camera (CSI-2 M6b): one
 * owner at a time; the ISP interrupt goes to the owner's handler. Claims are
 * made from the CLI task. */
enum { ISP_OWNER_NONE = 0, ISP_OWNER_ISP = 1, ISP_OWNER_CAMERA = 2 };
int isp_claim(int owner); /* 1: claimed (or already owned by `owner`), 0: busy */
void isp_release(int owner);
#ifdef FX1_FW_HAS_CAMERA
void camera_service_init(void);
BaseType_t camera_isp_isr(void); /* ISP interrupt while the camera owns the ISP */
#endif

#endif /* RTOS_APP_H */
