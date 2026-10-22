/*
 * Boot-time IP check run by hart 1 (bare metal) before hart 0 uses any IP.
 *
 * Hart 0 releases hart 1, waits for hwcheck_report.done (with a timeout) and
 * halts the boot if any IP failed. Hart 1 then parks in WFI. Only hart 1
 * touches the IPs while the check runs, so the "one owner per IP" rule of the
 * platform contract holds.
 *
 * Per IP: identity / reset values, write-readback-restore on registers without
 * side effects, and the bus-error contract (bad access -> access fault). DDR:
 * pattern, address-in-address and sub-word tests on a 4 KiB window.
 * Camera build (FX1_FW_HAS_CAMERA, CSI-2 M6b): CSI-2 RX identity, reset values
 * and read/write registers; I2C0 status and CTRL; the sensor's model_id over
 * I2C0. Without --camera the CSI-2 RX faults on the bus: reported as absent
 * (skipped), not as a failure; I2C0 is checked, the sensor is not.
 */
#ifndef HWCHECK_H
#define HWCHECK_H

#include <stdint.h>

#define HWCHECK_MAX_IPS 10u

typedef struct {
    const char* name;
    const char* skipped; /* not checked, and why (camera absent); 0: checked */
    uint32_t checks, failures;
    /* first failure */
    const char* what;
    uint32_t addr, got, expected;
} hwcheck_ip;

typedef struct {
    volatile uint32_t done; /* written last by hart 1 (release order) */
    uint32_t ips;
    uint32_t total_checks, total_failures;
    hwcheck_ip ip[HWCHECK_MAX_IPS];
} hwcheck_result;

extern hwcheck_result hwcheck_report;

/* Hart 1 entry (secondary_main). */
void hwcheck_run(void);
/* Hart 0: print the report on the console. */
void hwcheck_print(void);

#endif /* HWCHECK_H */
