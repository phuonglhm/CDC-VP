/* SPDX-License-Identifier: Apache-2.0
 *
 * TFLite-Micro Conv2D registration backed by the FX1 fixed 32x32 NPU GEMM.
 *
 * The accelerator performs signed INT8 MACs only. This adapter owns the
 * framework work around that primitive: im2col, M/N/K tiling, zero padding,
 * input-zero-point correction, bias, per-channel requantization, output
 * offset, and fused activation clamping. Depthwise/transpose convolutions
 * remain on the stock TFLM CPU kernels.
 */

#include <cstdint>

#include "tensorflow/lite/c/builtin_op_data.h"
#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/kernels/internal/common.h"
#include "tensorflow/lite/micro/kernels/conv.h"
#include "tensorflow/lite/micro/kernels/kernel_util.h"
#include "tensorflow/lite/micro/micro_common.h"

extern "C" {
#include "npu_v4.h"
#include "soc/soc_memory_map.h"
#include "uart.h"
}

namespace {

constexpr int kNpuRows = 32;
constexpr int kNpuCols = 32;
constexpr int kNpuDepthMax = 992;
constexpr TickType_t kNpuTimeout = pdMS_TO_TICKS(1000);

volatile std::int8_t *const dma_activations =
    reinterpret_cast<volatile std::int8_t *>(CDC_VPU_OUT0_BASE);
volatile std::int8_t *const dma_weights =
    reinterpret_cast<volatile std::int8_t *>(CDC_NPU_WGT0_BASE);
volatile std::int32_t *const dma_output =
    reinterpret_cast<volatile std::int32_t *>(CDC_NPU_WORK0_BASE);

std::int32_t tile_accum[kNpuRows * kNpuCols];
std::int32_t filter_sums[kNpuCols];

struct ConvStats {
  std::uint32_t ops;
  std::uint32_t jobs;
  std::uint32_t cycles;
  std::uint32_t bytes_read;
  std::uint32_t bytes_written;
};

ConvStats stats;
bool verbose_enabled = true;

TfLiteStatus fail(const char *reason) {
  uart_puts("TFLM_NPU FAIL: Conv2D ");
  uart_puts(reason);
  uart_puts("\n");
  return kTfLiteError;
}

int minimum(int a, int b) { return a < b ? a : b; }

TfLiteStatus Fx1NpuConvEval(TfLiteContext *context, TfLiteNode *node) {
  const TfLiteEvalTensor *const input =
      tflite::micro::GetEvalInput(context, node, tflite::kConvInputTensor);
  const TfLiteEvalTensor *const filter =
      tflite::micro::GetEvalInput(context, node, tflite::kConvWeightsTensor);
  const TfLiteEvalTensor *const bias =
      node->inputs->size == 3
          ? tflite::micro::GetEvalInput(context, node, tflite::kConvBiasTensor)
          : nullptr;
  TfLiteEvalTensor *const output =
      tflite::micro::GetEvalOutput(context, node, tflite::kConvOutputTensor);

  if (input == nullptr || filter == nullptr || output == nullptr ||
      node->builtin_data == nullptr || node->user_data == nullptr) {
    return fail("null tensor/op data");
  }
  if (input->type != kTfLiteInt8 || filter->type != kTfLiteInt8 ||
      output->type != kTfLiteInt8 ||
      (bias != nullptr && bias->type != kTfLiteInt32)) {
    return fail("requires INT8 input/filter/output and INT32 bias");
  }
  if (input->dims == nullptr || input->dims->size != 4 ||
      filter->dims == nullptr || filter->dims->size != 4 ||
      output->dims == nullptr || output->dims->size != 4) {
    return fail("requires NHWC/OHWI rank-4 tensors");
  }

  const auto &params =
      *static_cast<const TfLiteConvParams *>(node->builtin_data);
  const auto &data = *static_cast<const tflite::OpDataConv *>(node->user_data);

  const int batches = input->dims->data[0];
  const int input_height = input->dims->data[1];
  const int input_width = input->dims->data[2];
  const int input_channels = input->dims->data[3];
  const int output_channels = filter->dims->data[0];
  const int filter_height = filter->dims->data[1];
  const int filter_width = filter->dims->data[2];
  const int filter_channels = filter->dims->data[3];
  const int output_height = output->dims->data[1];
  const int output_width = output->dims->data[2];

  if (batches <= 0 || input_height <= 0 || input_width <= 0 ||
      input_channels <= 0 || output_channels <= 0 || filter_height <= 0 ||
      filter_width <= 0 || output_height <= 0 || output_width <= 0) {
    return fail("invalid tensor dimensions");
  }
  if (input_channels != filter_channels) {
    return fail("grouped convolution is not supported");
  }
  if (data.filter_zero_point != 0) {
    return fail("non-zero filter zero-point is not supported");
  }
  if (params.stride_height <= 0 || params.stride_width <= 0 ||
      params.dilation_height_factor <= 0 || params.dilation_width_factor <= 0) {
    return fail("invalid stride/dilation");
  }

  const int gemm_rows = batches * output_height * output_width;
  const int gemm_cols = output_channels;
  const int gemm_depth = filter_height * filter_width * filter_channels;
  if (gemm_depth <= 0) {
    return fail("invalid GEMM depth");
  }

  const std::int8_t *const input_data =
      tflite::micro::GetTensorData<std::int8_t>(input);
  const std::int8_t *const filter_data =
      tflite::micro::GetTensorData<std::int8_t>(filter);
  const std::int32_t *const bias_data =
      tflite::micro::GetOptionalTensorData<std::int32_t>(bias);
  std::int8_t *const output_data =
      tflite::micro::GetTensorData<std::int8_t>(output);

  if (npu_v4_init() != NPU_V4_OK) {
    return fail("driver init");
  }

  ++stats.ops;
  if (verbose_enabled) {
    uart_put_u32("[NPU-CONV] op=", stats.ops, " ");
    uart_put_u32("M=", static_cast<std::uint32_t>(gemm_rows), " ");
    uart_put_u32("N=", static_cast<std::uint32_t>(gemm_cols), " ");
    uart_put_u32("K=", static_cast<std::uint32_t>(gemm_depth), "\n");
  }

  for (int row_base = 0; row_base < gemm_rows; row_base += kNpuRows) {
    const int active_rows = minimum(kNpuRows, gemm_rows - row_base);

    for (int col_base = 0; col_base < gemm_cols; col_base += kNpuCols) {
      const int active_cols = minimum(kNpuCols, gemm_cols - col_base);

      for (int i = 0; i < kNpuRows * kNpuCols; ++i) {
        tile_accum[i] = 0;
      }

      for (int col = 0; col < active_cols; ++col) {
        const int global_col = col_base + col;
        std::int32_t sum = 0;
        for (int depth = 0; depth < gemm_depth; ++depth) {
          sum += filter_data[global_col * gemm_depth + depth];
        }
        filter_sums[col] = sum;
      }

      for (int depth_base = 0; depth_base < gemm_depth;
           depth_base += kNpuDepthMax) {
        const int depth_count = minimum(kNpuDepthMax, gemm_depth - depth_base);

        for (int row = 0; row < kNpuRows; ++row) {
          for (int depth = 0; depth < depth_count; ++depth) {
            std::int8_t value = 0;

            if (row < active_rows) {
              const int global_row = row_base + row;
              int spatial = global_row;
              const int out_x = spatial % output_width;
              spatial /= output_width;
              const int out_y = spatial % output_height;
              const int batch = spatial / output_height;

              const int full_depth = depth_base + depth;
              int kernel = full_depth;
              const int in_channel = kernel % filter_channels;
              kernel /= filter_channels;
              const int filter_x = kernel % filter_width;
              const int filter_y = kernel / filter_width;

              const int in_y = out_y * params.stride_height -
                               data.padding.height +
                               filter_y * params.dilation_height_factor;
              const int in_x = out_x * params.stride_width -
                               data.padding.width +
                               filter_x * params.dilation_width_factor;

              if (in_y >= 0 && in_y < input_height && in_x >= 0 &&
                  in_x < input_width) {
                const int input_offset =
                    ((batch * input_height + in_y) * input_width + in_x) *
                        input_channels +
                    in_channel;
                value = input_data[input_offset];
              } else {
                /* Quantized real zero for SAME padding. */
                value = static_cast<std::int8_t>(data.input_zero_point);
              }
            }
            dma_activations[row * depth_count + depth] = value;
          }
        }

        for (int depth = 0; depth < depth_count; ++depth) {
          for (int col = 0; col < kNpuCols; ++col) {
            std::int8_t value = 0;
            if (col < active_cols) {
              const int global_col = col_base + col;
              value = filter_data[global_col * gemm_depth + depth_base + depth];
            }
            dma_weights[depth * kNpuCols + col] = value;
          }
        }

        const npu_gemm_job_t job = {
            reinterpret_cast<const std::int8_t *>(CDC_VPU_OUT0_BASE),
            reinterpret_cast<const std::int8_t *>(CDC_NPU_WGT0_BASE),
            reinterpret_cast<std::int32_t *>(CDC_NPU_WORK0_BASE),
            static_cast<std::uint32_t>(depth_count),
            static_cast<std::uint32_t>(depth_count),
        };

        const int rc = npu_v4_gemm(&job, kNpuTimeout);
        if (rc != NPU_V4_OK) {
          uart_put_i32("TFLM_NPU FAIL: GEMM rc=", rc, " ");
          uart_put_hex32("status=", npu_v4_last_status(), "\n");
          return kTfLiteError;
        }

        npu_v4_metrics_t metrics = {};
        npu_v4_get_metrics(&metrics);
        ++stats.jobs;
        stats.cycles += metrics.cycle_count;
        stats.bytes_read += metrics.bytes_read;
        stats.bytes_written += metrics.bytes_written;

        for (int row = 0; row < active_rows; ++row) {
          for (int col = 0; col < active_cols; ++col) {
            tile_accum[row * kNpuCols + col] +=
                dma_output[row * kNpuCols + col];
          }
        }
      }

      for (int row = 0; row < active_rows; ++row) {
        const int global_row = row_base + row;
        for (int col = 0; col < active_cols; ++col) {
          const int global_col = col_base + col;
          std::int32_t acc = tile_accum[row * kNpuCols + col];

          /* NPU computes sum(input_q * weight_q). Reference TFLM
           * computes sum((input_q - input_zp) * weight_q). */
          acc -= data.input_zero_point * filter_sums[col];
          if (bias_data != nullptr) {
            acc += bias_data[global_col];
          }
          acc = tflite::MultiplyByQuantizedMultiplier(
              acc, data.per_channel_output_multiplier[global_col],
              data.per_channel_output_shift[global_col]);
          acc += data.output_zero_point;
          if (acc < data.output_activation_min) {
            acc = data.output_activation_min;
          }
          if (acc > data.output_activation_max) {
            acc = data.output_activation_max;
          }
          output_data[global_row * output_channels + global_col] =
              static_cast<std::int8_t>(acc);
        }
      }
    }
  }

  return kTfLiteOk;
}

} // namespace

const TFLMRegistration *GetFx1NpuConvRegistration() {
  static TFLMRegistration registration = tflite::micro::RegisterOp(
      tflite::ConvInit, tflite::ConvPrepare, Fx1NpuConvEval);
  return &registration;
}

extern "C" void fx1_npu_conv_reset_stats(void) { stats = {}; }

extern "C" void fx1_npu_conv_set_verbose(int enabled) {
  verbose_enabled = enabled != 0;
}

extern "C" std::uint32_t fx1_npu_conv_ops(void) { return stats.ops; }

extern "C" std::uint32_t fx1_npu_conv_jobs(void) { return stats.jobs; }

extern "C" std::uint32_t fx1_npu_conv_cycles(void) { return stats.cycles; }

extern "C" std::uint32_t fx1_npu_conv_bytes_read(void) {
  return stats.bytes_read;
}

extern "C" std::uint32_t fx1_npu_conv_bytes_written(void) {
  return stats.bytes_written;
}
