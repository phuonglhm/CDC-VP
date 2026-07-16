/**
 * @file hw.h
 * @brief Central include file for ISP hardware architecture support
 *
 * Include this file to get all hardware architecture components:
 *   - Architecture configuration
 *   - Stream types
 *   - Metrics collection
 *   - Memory models
 *   - DMA models
 *   - Tracing utilities
 */

#ifndef ISP_HW_H
#define ISP_HW_H

// Architecture configuration
#include "isp_arch_config.h"

// Stream types
#include "stream_beat.h"

// Timing infrastructure
#include "timed_stream.h"

// Metrics
#include "metrics.h"

// Memory models
#include "local_memory.h"

// DMA models
#include "frame_dma.h"

// Timed block shells
#include "timed_block.h"

// Tracing
#include "trace.h"

#endif  // ISP_HW_H
