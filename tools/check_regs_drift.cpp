/*
 * check_regs_drift.cpp - compile-time guard that the clean firmware register
 * headers (fw/common/include/soc/regs/*) match the authoritative TLM models.
 *
 * The TLM model is the SoC's register ground truth (it rejects bad offsets), so
 * every offset shipped to the firmware team is static_asserted against it here.
 * Any drift is a compile error. This is syntax-only (no link, no SystemC lib):
 *
 *   g++ -std=c++17 -fsyntax-only \
 *     -I/opt/systemc-2.3.4/include \
 *     -Icomponents/timer_tlm/include -Icomponents/i2c_tlm/include \
 *     -Icomponents/dma_tlm/include -Icomponents/npu_tlm_v4_model/include \
 *     -Ifw/common/include \
 *     tools/check_regs_drift.cpp
 *
 * IPs without symbolic offset constants in their model (e.g. SPI decodes raw
 * cases in the .cpp) are delivered as reference headers only, not clean headers,
 * and are intentionally not checked here.
 */
#include "timer.h"     // model: namespace ADDR / OPS
#include "i2c.h"       // model: I2C_* #defines
#include "dma_tlm.h"   // model: cdc::components::dma_tlm::* static consts
#include "gpio_tlm.h"  // model: cdc::components::gpio_tlm::k*Offset
#if !defined(CDC_CHECK_REGS_SKIP_NPU)
#include "npu_tlm_v4_regmap.h"
#endif

#include "soc/regs/soc_regs_timer.h"
#include "soc/regs/soc_regs_i2c.h"
#include "soc/regs/soc_regs_dma.h"
#include "soc/regs/soc_regs_npu_v4.h"
#include "soc/soc_memory_map.h"   // GPIO offsets live in the memory-map header

/* ---- Timer ---- */
static_assert(CDC_TIMER_CTRL      == ADDR::CTRL,      "timer CTRL drift");
static_assert(CDC_TIMER_VALUE     == ADDR::VALUE,     "timer VALUE drift");
static_assert(CDC_TIMER_RELOAD    == ADDR::RELOAD,    "timer RELOAD drift");
static_assert(CDC_TIMER_INTSTATUS == ADDR::INTSTATUS, "timer INTSTATUS drift");
static_assert(CDC_TIMER_CTRL_ENABLE  == OPS::ENABLE,  "timer ENABLE drift");
static_assert(CDC_TIMER_CTRL_EX_EN   == OPS::EX_EN,   "timer EX_EN drift");
static_assert(CDC_TIMER_CTRL_EX_CLK  == OPS::EX_CLK,  "timer EX_CLK drift");
static_assert(CDC_TIMER_CTRL_INTR_EN == OPS::INTR_EN, "timer INTR_EN drift");

/* ---- I2C ---- */
static_assert(CDC_I2C_INTR_STATE  == I2C_INTR_STATE,  "i2c INTR_STATE drift");
static_assert(CDC_I2C_INTR_ENABLE == I2C_INTR_ENABLE, "i2c INTR_ENABLE drift");
static_assert(CDC_I2C_CTRL        == I2C_CTRL,        "i2c CTRL drift");
static_assert(CDC_I2C_STATUS      == I2C_STATUS,      "i2c STATUS drift");
static_assert(CDC_I2C_RDATA       == I2C_RDATA,       "i2c RDATA drift");
static_assert(CDC_I2C_FDATA       == I2C_FDATA,       "i2c FDATA drift");
static_assert(CDC_I2C_FIFO_CTRL   == I2C_FIFO_CTRL,   "i2c FIFO_CTRL drift");
static_assert(CDC_I2C_ACQDATA     == I2C_ACQDATA,     "i2c ACQDATA drift");
static_assert(CDC_I2C_TXDATA      == I2C_TXDATA,      "i2c TXDATA drift");
static_assert(CDC_I2C_CTRL_ENABLEHOST   == CTRL_ENABLEHOST,   "i2c host-en drift");
static_assert(CDC_I2C_CTRL_ENABLETARGET == CTRL_ENABLETARGET, "i2c tgt-en drift");
static_assert(CDC_I2C_FDATA_START == FDATA_START, "i2c FDATA_START drift");
static_assert(CDC_I2C_FDATA_STOP  == FDATA_STOP,  "i2c FDATA_STOP drift");
static_assert(CDC_I2C_FDATA_READB == FDATA_READB, "i2c FDATA_READB drift");

/* ---- DMA ---- */
using dma = cdc::components::dma_tlm;
static_assert(CDC_DMA_DSR    == dma::DSR,    "dma DSR drift");
static_assert(CDC_DMA_DPC    == dma::DPC,    "dma DPC drift");
static_assert(CDC_DMA_INTEN  == dma::INTEN,  "dma INTEN drift");
static_assert(CDC_DMA_INT_EVENT_RIS == dma::INT_EVENT_RIS,
              "dma INT_EVENT_RIS drift");
static_assert(CDC_DMA_INTMIS == dma::INTMIS, "dma INTMIS drift");
static_assert(CDC_DMA_INTCLR == dma::INTCLR, "dma INTCLR drift");
static_assert(CDC_DMA_FSRC   == dma::FSRC,   "dma FSRC drift");
static_assert(CDC_DMA_CSR0   == dma::CSR0,   "dma CSR0 drift");
static_assert(CDC_DMA_SAR0   == dma::SAR0,   "dma SAR0 drift");
static_assert(CDC_DMA_DAR0   == dma::DAR0,   "dma DAR0 drift");
static_assert(CDC_DMA_CCR0   == dma::CCR0,   "dma CCR0 drift");
static_assert(CDC_DMA_DBGCMD   == dma::DBGCMD,   "dma DBGCMD drift");
static_assert(CDC_DMA_DBGINST0 == dma::DBGINST0, "dma DBGINST0 drift");
static_assert(CDC_DMA_DBGINST1 == dma::DBGINST1, "dma DBGINST1 drift");
static_assert(CDC_DMA_CH_AXI_STRIDE  == dma::CHANNEL_AXI_STRIDE,    "dma AXI stride drift");
static_assert(CDC_DMA_CH_STAT_STRIDE == dma::CHANNEL_STATUS_STRIDE, "dma stat stride drift");
static_assert(CDC_DMA_CCR_SRC_INC == dma::CCR_SRC_INC,
              "dma CCR SRC_INC drift");
static_assert(CDC_DMA_CCR_SRC_BURST_SIZE_SHIFT ==
                  dma::CCR_SRC_BURST_SIZE_SHIFT,
              "dma CCR SRC_SIZE shift drift");
static_assert(CDC_DMA_CCR_SRC_BURST_LEN_SHIFT ==
                  dma::CCR_SRC_BURST_LEN_SHIFT,
              "dma CCR SRC_LEN shift drift");
static_assert(CDC_DMA_CCR_DST_INC == dma::CCR_DST_INC,
              "dma CCR DST_INC drift");
static_assert(CDC_DMA_CCR_DST_BURST_SIZE_SHIFT ==
                  dma::CCR_DST_BURST_SIZE_SHIFT,
              "dma CCR DST_SIZE shift drift");
static_assert(CDC_DMA_CCR_DST_BURST_LEN_SHIFT ==
                  dma::CCR_DST_BURST_LEN_SHIFT,
              "dma CCR DST_LEN shift drift");
static_assert(CDC_DMA_STATUS_STOPPED == dma::STATUS_STOPPED,
              "dma STOPPED status drift");
/* CCR_RESET_VALUE is private in the model, so it cannot be static_asserted here;
 * the value in soc_regs_dma.h is copied from the model and left informational. */

/* ---- GPIO (ROM-code boot strap block) ---- */
using gpio = cdc::components::gpio_tlm;
static_assert(CDC_GPIO_VALUE == gpio::kValueOffset, "gpio VALUE drift");
static_assert(CDC_GPIO_OUT   == gpio::kOutOffset,   "gpio OUT drift");
static_assert(CDC_GPIO_DIR   == gpio::kDirOffset,   "gpio DIR drift");

/* ---- SAURIA NPU v4 ---- */
#if !defined(CDC_CHECK_REGS_SKIP_NPU)
namespace npu = cdc::components::npu_v4_reg;
static_assert(CDC_NPU_CTRL               == npu::CTRL,               "npu CTRL drift");
static_assert(CDC_NPU_STATUS             == npu::STATUS,             "npu STATUS drift");
static_assert(CDC_NPU_IRQ_ENABLE         == npu::IRQ_ENABLE,         "npu IRQ_ENABLE drift");
static_assert(CDC_NPU_IRQ_STATUS         == npu::IRQ_STATUS,         "npu IRQ_STATUS drift");
static_assert(CDC_NPU_SRC_ADDR           == npu::SRC_ADDR,           "npu SRC_ADDR drift");
static_assert(CDC_NPU_DST_ADDR           == npu::DST_ADDR,           "npu DST_ADDR drift");
static_assert(CDC_NPU_SCRATCH_ADDR       == npu::SCRATCH_ADDR,       "npu SCRATCH_ADDR drift");
static_assert(CDC_NPU_SRC_SIZE_BYTES     == npu::SRC_SIZE_BYTES,     "npu SRC_SIZE drift");
static_assert(CDC_NPU_DST_SIZE_BYTES     == npu::DST_SIZE_BYTES,     "npu DST_SIZE drift");
static_assert(CDC_NPU_WIDTH              == npu::WIDTH,              "npu WIDTH drift");
static_assert(CDC_NPU_HEIGHT             == npu::HEIGHT,             "npu HEIGHT drift");
static_assert(CDC_NPU_SRC_STRIDE_BYTES   == npu::SRC_STRIDE_BYTES,   "npu STRIDE drift");
static_assert(CDC_NPU_FORMAT             == npu::FORMAT,             "npu FORMAT drift");
static_assert(CDC_NPU_OP_MODE            == npu::OP_MODE,            "npu OP_MODE drift");
static_assert(CDC_NPU_WEIGHTS_ADDR       == npu::WEIGHTS_ADDR,       "npu WEIGHTS_ADDR drift");
static_assert(CDC_NPU_PARAM_ADDR         == npu::PARAM_ADDR,         "npu PARAM_ADDR drift");
static_assert(CDC_NPU_WEIGHTS_SIZE_BYTES == npu::WEIGHTS_SIZE_BYTES, "npu WEIGHTS_SIZE drift");
static_assert(CDC_NPU_K_DIMENSION        == npu::K_DIMENSION,        "npu K drift");
static_assert(CDC_NPU_ZERO_THRESHOLD_FP32 == npu::ZERO_THRESHOLD_FP32, "npu THRESHOLD drift");
static_assert(CDC_NPU_ROWS_ACTIVE        == npu::ROWS_ACTIVE,        "npu ROWS_ACTIVE drift");
static_assert(CDC_NPU_DILATION_PATTERN   == npu::DILATION_PATTERN,   "npu DILATION drift");
static_assert(CDC_NPU_CYCLE_COUNT        == npu::CYCLE_COUNT,        "npu CYCLE_COUNT drift");
static_assert(CDC_NPU_BYTES_READ         == npu::BYTES_READ,         "npu BYTES_READ drift");
static_assert(CDC_NPU_BYTES_WRITTEN      == npu::BYTES_WRITTEN,      "npu BYTES_WRITTEN drift");
static_assert(CDC_NPU_LAST_ERROR         == npu::LAST_ERROR,         "npu LAST_ERROR drift");
static_assert(CDC_NPU_CORE_ID            == npu::CORE_ID,            "npu CORE_ID offset drift");
static_assert(CDC_ACCEL_MMIO_SIZE        == npu::MMIO_SIZE,          "npu MMIO size drift");
static_assert(CDC_NPU_CTRL_ENABLE        == npu::CTRL_ENABLE,        "npu ENABLE drift");
static_assert(CDC_NPU_CTRL_START         == npu::CTRL_START,         "npu START drift");
static_assert(CDC_NPU_CTRL_SOFT_RESET    == npu::CTRL_SOFT_RESET,    "npu SOFT_RESET drift");
static_assert(CDC_NPU_CTRL_IRQ_EN        == npu::CTRL_IRQ_EN,        "npu IRQ_EN drift");
static_assert(CDC_NPU_STATUS_BUSY        == npu::STATUS_BUSY,        "npu BUSY drift");
static_assert(CDC_NPU_STATUS_DONE        == npu::STATUS_DONE,        "npu DONE drift");
static_assert(CDC_NPU_STATUS_ERROR       == npu::STATUS_ERROR,       "npu ERROR drift");
static_assert(CDC_NPU_STATUS_IDLE        == npu::STATUS_IDLE,        "npu IDLE drift");
static_assert(CDC_NPU_IRQ_DONE           == npu::IRQ_DONE,           "npu IRQ_DONE drift");
static_assert(CDC_NPU_IRQ_ERROR          == npu::IRQ_ERROR,          "npu IRQ_ERROR drift");
static_assert(CDC_NPU_FORMAT_INT8_INT8_INT32 == npu::FORMAT_INT8_INT8_INT32,
              "npu format value drift");
static_assert(CDC_NPU_OP_GEMM            == npu::OP_GEMM,            "npu op value drift");
static_assert(CDC_NPU_CORE_ID_VALUE      == npu::CORE_ID_VALUE,      "npu CORE_ID drift");
static_assert(CDC_NPU_ERROR_NONE ==
                  static_cast<unsigned>(npu::error_code::none),
              "npu error NONE drift");
static_assert(CDC_NPU_ERROR_DISABLED ==
                  static_cast<unsigned>(npu::error_code::disabled),
              "npu error DISABLED drift");
static_assert(CDC_NPU_ERROR_BUSY ==
                  static_cast<unsigned>(npu::error_code::busy),
              "npu error BUSY drift");
static_assert(CDC_NPU_ERROR_INVALID_DIMENSIONS ==
                  static_cast<unsigned>(npu::error_code::invalid_dimensions),
              "npu error DIMENSIONS drift");
static_assert(CDC_NPU_ERROR_INVALID_FORMAT ==
                  static_cast<unsigned>(npu::error_code::invalid_format),
              "npu error FORMAT drift");
static_assert(CDC_NPU_ERROR_INVALID_OPERATION ==
                  static_cast<unsigned>(npu::error_code::invalid_operation),
              "npu error OPERATION drift");
static_assert(CDC_NPU_ERROR_INVALID_ADDRESS ==
                  static_cast<unsigned>(npu::error_code::invalid_address),
              "npu error ADDRESS drift");
static_assert(CDC_NPU_ERROR_INVALID_SIZE ==
                  static_cast<unsigned>(npu::error_code::invalid_size),
              "npu error SIZE drift");
static_assert(CDC_NPU_ERROR_DMA_READ ==
                  static_cast<unsigned>(npu::error_code::dma_read),
              "npu error DMA_READ drift");
static_assert(CDC_NPU_ERROR_DMA_WRITE ==
                  static_cast<unsigned>(npu::error_code::dma_write),
              "npu error DMA_WRITE drift");
static_assert(CDC_NPU_ERROR_CORE_DEADLOCK ==
                  static_cast<unsigned>(npu::error_code::core_deadlock),
              "npu error DEADLOCK drift");
static_assert(CDC_NPU_ERROR_CORE_TIMEOUT ==
                  static_cast<unsigned>(npu::error_code::core_timeout),
              "npu error TIMEOUT drift");
static_assert(CDC_NPU_ERROR_RESET_ABORTED ==
                  static_cast<unsigned>(npu::error_code::reset_aborted),
              "npu error RESET_ABORTED drift");
#endif

int main() { return 0; }
