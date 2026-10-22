/*
 * MMIO HAL of the reference CSI-2 RX driver (components/FX1_Components/
 * mipi-csi2/driver, fx1_csi2_drv.h) for fx1_soc firmware (CSI-2 M5b):
 * 32-bit accesses at FX1_MIPI_CSR_BASE + offset, delays in simulated time
 * from mtime.
 */
#ifndef FX1_CSI2_HAL_MMIO_H
#define FX1_CSI2_HAL_MMIO_H

#include "fx1_csi2/fx1_csi2_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A HAL for the controller at `base` (FX1_MIPI_CSR_BASE on fx1_soc). The
 * returned structure points at static storage: one controller. */
const fx1_csi2_hal* fx1_csi2_hal_mmio(uintptr_t base);

#ifdef __cplusplus
}
#endif

#endif /* FX1_CSI2_HAL_MMIO_H */
