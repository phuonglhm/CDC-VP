/**
 * @file hw.h
 * @brief Umbrella include for the internal ISP line-granular architecture model
 *
 * Includes the seven active model headers:
 *   - Architecture configuration and metrics
 *   - Line channels, stages, and stage runtime
 *   - Frame feedback and architecture sweeps
 */

#ifndef ISP_HW_H
#define ISP_HW_H

#include "isp_arch_config.h"
#include "metrics.h"
#include "line_channel.h"
#include "line_stage.h"
#include "stage_runtime.h"
#include "frame_feedback.h"
#include "sweep.h"

#endif  // ISP_HW_H
