/*
 * FX1 FreeRTOS CLI firmware (AMP).
 *
 * Hart 0: boot banner -> release hart 1 -> wait for hart 1's IP check ->
 *   print the report; any failure halts the boot (FAIL 0x50) -> FreeRTOS:
 *   trap entry, device services (ISP, SYS_DMA), UART console -> CLI task ->
 *   scheduler.
 * Hart 1: bare-metal hwcheck of every IP, then WFI forever.
 *
 * Console: the UART, over TCP (fx1_soc --uart0-socket) for interactive use or
 * from a file (--uart0-rx-file) in CI. `poweroff` ends the simulation with
 * PASS when no check failed.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "fx1_fw.h"
#include "hwcheck.h"
#include "rtos_app.h"
#include "rtos_bsp.h"

void cli_task(void* arg);

void secondary_main(uint32_t hart)
{
    (void)hart;
    hwcheck_run();
    for (;;) fx1_wfi();
}

int main(void)
{
    fx1_log(APP_VERSION);
    fx1_log("boot: releasing hart 1 for the IP check");
    fx1_release_secondaries();
    const uint64_t t0 = fx1_mtime();
    while (!hwcheck_report.done)
        fx1_check(fx1_mtime() - t0 < FX1_MTIME_HZ / 10u, 0x4F, "hart 1 IP check within 100 ms");
    fx1_acquire();
    hwcheck_print();
    if (hwcheck_report.total_failures) {
        fx1_log("boot halted: hwcheck FAIL");
        fx1_fail(0x50);
    }

    bsp_init();
    isp_service_init();
    dma_service_init();
#ifdef FX1_FW_HAS_CAMERA
    camera_service_init();
#endif
    bsp_console_start();
    fx1_check(xTaskCreate(cli_task, "cli", 2048, NULL, 2, NULL) == pdPASS, 0x51, "CLI task created");
    fx1_log("boot: starting the FreeRTOS scheduler on hart 0");
    vTaskStartScheduler();
    fx1_fail(0x52); /* not reached */
}
