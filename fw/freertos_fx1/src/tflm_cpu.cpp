/* SPDX-License-Identifier: Apache-2.0
 *
 * TFLite-Micro CPU baseline for the FX1 FreeRTOS CLI.
 *
 * The model is linked as a binary object generated from the pinned upstream
 * hello_world_int8.tflite. No dynamic allocation is used; the interpreter and
 * all tensors live in the static arena below.
 */

#include <cstddef>
#include <cstdint>

#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "secda_simple_model_model_data.h"

extern "C" {
#include "FreeRTOS.h"
#include "task.h"

#include "uart.h"

extern const unsigned char _binary_hello_world_int8_tflite_start[];
extern const unsigned char _binary_hello_world_int8_tflite_end[];
}

#if defined(DEMO_NPU)
const TFLMRegistration *GetFx1NpuConvRegistration();
extern "C" void fx1_npu_conv_reset_stats(void);
extern "C" void fx1_npu_conv_set_verbose(int enabled);
extern "C" std::uint32_t fx1_npu_conv_ops(void);
extern "C" std::uint32_t fx1_npu_conv_jobs(void);
extern "C" std::uint32_t fx1_npu_conv_cycles(void);
extern "C" std::uint32_t fx1_npu_conv_bytes_read(void);
extern "C" std::uint32_t fx1_npu_conv_bytes_written(void);
#endif

namespace {

constexpr std::size_t kTensorArenaSize = 4096u;
alignas(16) std::uint8_t tensor_arena[kTensorArenaSize];

constexpr std::size_t kSimpleTensorArenaSize = 16u * 1024u;
alignas(16) std::uint8_t simple_tensor_arena[kSimpleTensorArenaSize];

constexpr int kSampleCount = 4;
constexpr std::int8_t kInputQuantized[kSampleCount] = {-96, -63, -34, 0};
constexpr std::int32_t kExpectedMilli[kSampleCount] = {696, 1000, 746, 2};
constexpr std::int32_t kToleranceMilli = 100;

constexpr int kSimpleInputElements = 16;
constexpr int kSimpleOutputElements = 2;
constexpr std::int8_t kSimpleGolden[kSimpleOutputElements] = {-113, 127};

struct SimpleResult {
  std::int8_t output[kSimpleOutputElements];
  std::uint32_t arena_used;
  std::int32_t scale_ppm;
  std::int32_t zero_point;
  TickType_t ticks;
};

std::int32_t abs_i32(std::int32_t value) { return value < 0 ? -value : value; }

const tflite::Model *get_model() {
  return tflite::GetModel(_binary_hello_world_int8_tflite_start);
}

std::size_t model_size() {
  return static_cast<std::size_t>(_binary_hello_world_int8_tflite_end -
                                  _binary_hello_world_int8_tflite_start);
}

int validate_io(TfLiteTensor *input, TfLiteTensor *output, const char *marker) {
  if (input == nullptr || output == nullptr) {
    uart_puts(marker);
    uart_puts(" FAIL: null input/output tensor\n");
    return -1;
  }
  if (input->type != kTfLiteInt8 || output->type != kTfLiteInt8) {
    uart_puts(marker);
    uart_puts(" FAIL: expected INT8 input/output\n");
    return -2;
  }
  return 0;
}

int validate_simple_io(TfLiteTensor *input, TfLiteTensor *output,
                       const char *marker) {
  if (input == nullptr || output == nullptr) {
    uart_puts(marker);
    uart_puts(" FAIL: null input/output tensor\n");
    return -1;
  }
  if (input->type != kTfLiteInt8 || output->type != kTfLiteInt8) {
    uart_puts(marker);
    uart_puts(" FAIL: expected INT8 input/output\n");
    return -2;
  }
  if (input->bytes != kSimpleInputElements ||
      output->bytes != kSimpleOutputElements) {
    uart_puts(marker);
    uart_put_u32(
        " FAIL: input_bytes=", static_cast<std::uint32_t>(input->bytes), " ");
    uart_put_u32("output_bytes=", static_cast<std::uint32_t>(output->bytes),
                 "\n");
    return -3;
  }
  if (input->dims == nullptr || input->dims->size != 4 ||
      input->dims->data[0] != 1 || input->dims->data[1] != 4 ||
      input->dims->data[2] != 4 || input->dims->data[3] != 1 ||
      output->dims == nullptr || output->dims->size != 2 ||
      output->dims->data[0] != 1 || output->dims->data[1] != 2) {
    uart_puts(marker);
    uart_puts(" FAIL: unexpected tensor shape\n");
    return -4;
  }
  return 0;
}

int run_simple_model(bool use_npu, const char *marker, SimpleResult *result) {
  const tflite::Model *const model =
      tflite::GetModel(g_secda_simple_model_model_data);
  if (model == nullptr || model->version() != TFLITE_SCHEMA_VERSION) {
    uart_puts(marker);
    uart_puts(" FAIL: model/schema\n");
    return -1;
  }

  tflite::MicroMutableOpResolver<9> resolver;
  TfLiteStatus status = kTfLiteError;
#if defined(DEMO_NPU)
  status = use_npu ? resolver.AddConv2D(*GetFx1NpuConvRegistration())
                   : resolver.AddConv2D();
#else
  if (use_npu) {
    uart_puts(marker);
    uart_puts(" FAIL: NPU support is not built\n");
    return -2;
  }
  status = resolver.AddConv2D();
#endif
  status = status == kTfLiteOk ? resolver.AddFullyConnected() : status;
  status = status == kTfLiteOk ? resolver.AddSoftmax() : status;
  status = status == kTfLiteOk ? resolver.AddDepthwiseConv2D() : status;
  status = status == kTfLiteOk ? resolver.AddShape() : status;
  status = status == kTfLiteOk ? resolver.AddTransposeConv() : status;
  status = status == kTfLiteOk ? resolver.AddAdd() : status;
  status = status == kTfLiteOk ? resolver.AddPad() : status;
  status = status == kTfLiteOk ? resolver.AddMean() : status;
  if (status != kTfLiteOk) {
    uart_puts(marker);
    uart_puts(" FAIL: resolver\n");
    return -3;
  }

  tflite::MicroInterpreter interpreter(model, resolver, simple_tensor_arena,
                                       kSimpleTensorArenaSize);
  if (interpreter.AllocateTensors() != kTfLiteOk) {
    uart_puts(marker);
    uart_puts(" FAIL: AllocateTensors\n");
    return -4;
  }

  TfLiteTensor *const input = interpreter.input(0);
  TfLiteTensor *const output = interpreter.output(0);
  if (validate_simple_io(input, output, marker) != 0) {
    return -5;
  }

  for (int i = 0; i < kSimpleInputElements; ++i) {
    input->data.int8[i] = static_cast<std::int8_t>(i);
  }

  const TickType_t before = xTaskGetTickCount();
  if (interpreter.Invoke() != kTfLiteOk) {
    uart_puts(marker);
    uart_puts(" FAIL: Invoke\n");
    return -6;
  }

  result->ticks = xTaskGetTickCount() - before;
  result->arena_used =
      static_cast<std::uint32_t>(interpreter.arena_used_bytes());
  result->scale_ppm =
      static_cast<std::int32_t>(output->params.scale * 1000000.0f);
  result->zero_point = output->params.zero_point;
  for (int i = 0; i < kSimpleOutputElements; ++i) {
    result->output[i] = output->data.int8[i];
  }
  return 0;
}

#if defined(DEMO_NPU)
void print_spaces(int count) {
  while (count-- > 0) {
    uart_putc(' ');
  }
}

int decimal_width(std::int32_t value) {
  int width = value < 0 ? 1 : 0;
  std::uint32_t magnitude = value < 0 ? static_cast<std::uint32_t>(-value)
                                      : static_cast<std::uint32_t>(value);
  do {
    ++width;
    magnitude /= 10u;
  } while (magnitude != 0u);
  return width;
}

void print_right_aligned(std::int32_t value, int width) {
  print_spaces(width - decimal_width(value));
  uart_put_i32("", value, "");
}

void print_dequantized(std::int8_t quantized, const SimpleResult &result) {
  const std::int32_t scaled =
      (static_cast<std::int32_t>(quantized) - result.zero_point) *
      result.scale_ppm;
  std::int32_t milli =
      scaled >= 0 ? (scaled + 500) / 1000 : (scaled - 500) / 1000;

  if (milli < 0) {
    uart_putc('-');
    milli = -milli;
  }
  uart_put_i32("", milli / 1000, ".");
  const std::int32_t fraction = milli % 1000;
  uart_putc(static_cast<char>('0' + fraction / 100));
  uart_putc(static_cast<char>('0' + (fraction / 10) % 10));
  uart_putc(static_cast<char>('0' + fraction % 10));
}
#endif

int print_simple_result(const char *tag, const SimpleResult &result) {
  int passes = 0;
  for (int i = 0; i < kSimpleOutputElements; ++i) {
    const std::int32_t actual = result.output[i];
    const bool ok = actual == kSimpleGolden[i];
    if (ok) {
      ++passes;
    }
    uart_puts(tag);
    uart_put_u32(" output[", static_cast<std::uint32_t>(i), "]=");
    uart_put_i32("", actual, " ");
    uart_put_i32("golden=", kSimpleGolden[i], ok ? " OK\n" : " FAIL\n");
  }

  uart_puts(tag);
  uart_put_u32(" model_bytes=",
               static_cast<std::uint32_t>(kSecdaSimpleModelDataSize), " ");
  uart_put_u32("arena_used=", result.arena_used, " ");
  uart_put_i32("scale_ppm=", result.scale_ppm, " ");
  uart_put_i32("zero_point=", result.zero_point, " ");
  uart_put_u32("ticks=", static_cast<std::uint32_t>(result.ticks), "\n");
  return passes;
}

#if defined(DEMO_NPU)
bool print_npu_stats(const char *tag, bool emit = true) {
  const std::uint32_t ops = fx1_npu_conv_ops();
  const std::uint32_t jobs = fx1_npu_conv_jobs();
  const std::uint32_t cycles = fx1_npu_conv_cycles();
  const std::uint32_t bytes_read = fx1_npu_conv_bytes_read();
  const std::uint32_t bytes_written = fx1_npu_conv_bytes_written();

  if (emit) {
    uart_puts(tag);
    uart_put_u32(" ops=", ops, " ");
    uart_put_u32("jobs=", jobs, " ");
    uart_put_u32("cycles=", cycles, " ");
    uart_put_u32("bytes_read=", bytes_read, " ");
    uart_put_u32("bytes_written=", bytes_written, "\n");
  }
  return ops != 0u && jobs != 0u && cycles != 0u && bytes_read != 0u &&
         bytes_written != 0u;
}
#endif

} // namespace

extern "C" int tflm_hello_run(void) {
  uart_puts("[TFLM-HELLO] start\n");

  const tflite::Model *const model = get_model();
  if (model == nullptr || model->version() != TFLITE_SCHEMA_VERSION) {
    uart_puts("TFLM_HELLO FAIL: model/schema\n");
    return -1;
  }

  tflite::MicroMutableOpResolver<1> resolver;
  if (resolver.AddFullyConnected() != kTfLiteOk) {
    uart_puts("TFLM_HELLO FAIL: resolver\n");
    return -2;
  }

  tflite::MicroInterpreter interpreter(model, resolver, tensor_arena,
                                       kTensorArenaSize);
  if (interpreter.AllocateTensors() != kTfLiteOk) {
    uart_puts("TFLM_HELLO FAIL: AllocateTensors\n");
    return -3;
  }

  TfLiteTensor *const input = interpreter.input(0);
  TfLiteTensor *const output = interpreter.output(0);
  if (validate_io(input, output, "TFLM_HELLO") != 0) {
    return -4;
  }

  input->data.int8[0] = kInputQuantized[0];
  if (interpreter.Invoke() != kTfLiteOk) {
    uart_puts("TFLM_HELLO FAIL: Invoke\n");
    return -5;
  }

  uart_put_u32("[TFLM-HELLO] schema=", TFLITE_SCHEMA_VERSION, "\n");
  uart_put_u32("[TFLM-HELLO] model_bytes=",
               static_cast<std::uint32_t>(model_size()), "\n");
  uart_put_u32("[TFLM-HELLO] arena_used=",
               static_cast<std::uint32_t>(interpreter.arena_used_bytes()),
               "\n");
  uart_put_i32("[TFLM-HELLO] output_q=",
               static_cast<std::int32_t>(output->data.int8[0]), "\n");
  uart_puts("TFLM_HELLO PASS\n");
  return 0;
}

extern "C" int tflm_run(void) {
  uart_puts("[TFLM-RUN] start\n");

  const tflite::Model *const model = get_model();
  if (model == nullptr || model->version() != TFLITE_SCHEMA_VERSION) {
    uart_puts("TFLM_RUN FAIL: model/schema\n");
    return -1;
  }

  tflite::MicroMutableOpResolver<1> resolver;
  if (resolver.AddFullyConnected() != kTfLiteOk) {
    uart_puts("TFLM_RUN FAIL: resolver\n");
    return -2;
  }

  tflite::MicroInterpreter interpreter(model, resolver, tensor_arena,
                                       kTensorArenaSize);
  if (interpreter.AllocateTensors() != kTfLiteOk) {
    uart_puts("TFLM_RUN FAIL: AllocateTensors\n");
    return -3;
  }

  TfLiteTensor *const input = interpreter.input(0);
  TfLiteTensor *const output = interpreter.output(0);
  if (validate_io(input, output, "TFLM_RUN") != 0) {
    return -4;
  }

  int passes = 0;
  for (int i = 0; i < kSampleCount; ++i) {
    input->data.int8[0] = kInputQuantized[i];

    const TickType_t before = xTaskGetTickCount();
    if (interpreter.Invoke() != kTfLiteOk) {
      uart_put_u32("TFLM_RUN FAIL: Invoke sample=", i, "\n");
      return -5;
    }
    const TickType_t elapsed = xTaskGetTickCount() - before;

    const std::int32_t predicted_milli = static_cast<std::int32_t>(
        static_cast<float>(static_cast<std::int32_t>(output->data.int8[0]) -
                           output->params.zero_point) *
        output->params.scale * 1000.0f);
    const std::int32_t error_milli =
        abs_i32(kExpectedMilli[i] - predicted_milli);
    const bool ok = error_milli <= kToleranceMilli;
    if (ok) {
      ++passes;
    }

    uart_put_u32("[TFLM-RUN] sample=", static_cast<std::uint32_t>(i), " ");
    uart_put_i32("input_q=", kInputQuantized[i], " ");
    uart_put_i32("output_q=", output->data.int8[0], " ");
    uart_put_i32("pred_milli=", predicted_milli, " ");
    uart_put_i32("expect_milli=", kExpectedMilli[i], " ");
    uart_put_u32("ticks=", static_cast<std::uint32_t>(elapsed),
                 ok ? " OK\n" : " FAIL\n");
  }

  if (passes != kSampleCount) {
    uart_put_u32("TFLM_RUN FAIL: passes=", passes, "/4\n");
    return -6;
  }

  uart_puts("TFLM_RUN PASS 4/4\n");
  return 0;
}

extern "C" int tflm_simple_run(void) {
  uart_puts("[TFLM-SIMPLE] start CPU reference\n");

  SimpleResult result = {};
  if (run_simple_model(false, "TFLM_SIMPLE", &result) != 0) {
    return -1;
  }
  const int passes = print_simple_result("[TFLM-SIMPLE]", result);
  if (passes != kSimpleOutputElements) {
    uart_put_u32("TFLM_SIMPLE FAIL: golden=", passes, "/2\n");
    return -2;
  }

  uart_puts("TFLM_SIMPLE PASS 2/2\n");
  return 0;
}

#if defined(DEMO_NPU)
extern "C" int tflm_npu_run(void) {
  uart_puts("[TFLM-NPU] start Conv2D GEMM offload\n");

  fx1_npu_conv_reset_stats();
  SimpleResult result = {};
  if (run_simple_model(true, "TFLM_NPU", &result) != 0) {
    return -1;
  }

  const int passes = print_simple_result("[TFLM-NPU]", result);
  const bool traffic_ok = print_npu_stats("[TFLM-NPU]");
  if (passes != kSimpleOutputElements) {
    uart_put_u32("TFLM_NPU FAIL: golden=", passes, "/2\n");
    return -2;
  }
  if (!traffic_ok) {
    uart_puts("TFLM_NPU FAIL: no accelerator traffic\n");
    return -3;
  }

  uart_puts("TFLM_NPU PASS 2/2\n");
  return 0;
}

extern "C" int tflm_demo_run(void) {
  SimpleResult cpu = {};
  SimpleResult npu = {};
  if (run_simple_model(false, "TFLM_DEMO", &cpu) != 0) {
    return -1;
  }

  fx1_npu_conv_reset_stats();
  fx1_npu_conv_set_verbose(0);
  const int npu_rc = run_simple_model(true, "TFLM_DEMO", &npu);
  fx1_npu_conv_set_verbose(1);
  if (npu_rc != 0) {
    return -2;
  }

  /*
   * Print only after both runs complete. The external cycle-level NPU model
   * may emit host-side traces while Invoke() is active; delaying the report
   * keeps the UART table contiguous even when those traces are enabled.
   */
  uart_puts("\n=== TFLite Micro on FX1 RV32 FreeRTOS - NPU Offload Demo ===\n");
  uart_puts(
      "Model : SECDA simple_model.tflite (9 ops; Conv2D->NPU, rest->CPU)\n");
  uart_puts("Input : int8 ramp 0..15, shape 1x4x4x1\n\n");

  uart_put_i32("Output: int8, scale=", cpu.scale_ppm, "/1000000 zp=");
  uart_put_i32("", cpu.zero_point, "\n\n");
  uart_puts("  idx | CPU ref | NPU GEMM | match\n");
  uart_puts(" -----+---------+----------+-------\n");

  int matches = 0;
  for (int i = 0; i < kSimpleOutputElements; ++i) {
    const bool match =
        cpu.output[i] == npu.output[i] && cpu.output[i] == kSimpleGolden[i];
    if (match) {
      ++matches;
    }
    uart_puts("  ");
    print_right_aligned(i, 3);
    uart_puts(" | ");
    print_right_aligned(cpu.output[i], 7);
    uart_puts(" | ");
    print_right_aligned(npu.output[i], 8);
    uart_puts(match ? " |  OK\n" : " |  XX\n");
  }
  uart_puts(" -----+---------+----------+-------\n");
  uart_put_u32(" Bit-exact: ", static_cast<std::uint32_t>(matches), "/2");
  uart_puts(matches == kSimpleOutputElements ? "   =>  PASS\n\n"
                                             : "   =>  FAIL\n\n");

  uart_puts("Dequantized output:\n");
  uart_puts("  CPU :");
  for (int i = 0; i < kSimpleOutputElements; ++i) {
    uart_putc(' ');
    print_dequantized(cpu.output[i], cpu);
  }
  uart_puts("\n  NPU :");
  for (int i = 0; i < kSimpleOutputElements; ++i) {
    uart_putc(' ');
    print_dequantized(npu.output[i], npu);
  }
  uart_puts("\n=== Done ===\n");

  const bool traffic_ok = print_npu_stats("[TFLM-DEMO]", false);

  if (matches != kSimpleOutputElements) {
    uart_put_u32("TFLM_DEMO FAIL: bit_exact=", matches, "/2\n");
    return -3;
  }
  if (!traffic_ok) {
    uart_puts("TFLM_DEMO FAIL: no accelerator traffic\n");
    return -4;
  }

  uart_puts("TFLM_DEMO PASS 2/2 bit-exact\n");
  return 0;
}
#endif
