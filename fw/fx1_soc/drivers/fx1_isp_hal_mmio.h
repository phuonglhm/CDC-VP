/*
 * MMIO HAL of the reference ISP driver (components/FX1_Components/isp/driver,
 * fx1_isp_drv.h) for fx1_soc firmware (CSI-2 M6a): 32-bit accesses at
 * FX1_ISP_CSR_BASE + offset; delays in ISP core clocks (2 ns) from mtime.
 */
#ifndef FX1_ISP_HAL_MMIO_H
#define FX1_ISP_HAL_MMIO_H

#include "fx1_isp/fx1_isp_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

const fx1_isp_hal* fx1_isp_hal_mmio(void);

#ifdef __cplusplus
}
#endif

#endif /* FX1_ISP_HAL_MMIO_H */
