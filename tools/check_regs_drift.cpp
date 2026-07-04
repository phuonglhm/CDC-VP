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
 *     -Icomponents/dma_tlm/include -Ifw/common/include \
 *     tools/check_regs_drift.cpp
 *
 * IPs without symbolic offset constants in their model (e.g. SPI decodes raw
 * cases in the .cpp) are delivered as reference headers only, not clean headers,
 * and are intentionally not checked here.
 */
#include "timer.h"     // model: namespace ADDR / OPS
#include "i2c.h"       // model: I2C_* #defines
#include "dma_tlm.h"   // model: cdc::components::dma_tlm::* static consts

#include "soc/regs/soc_regs_timer.h"
#include "soc/regs/soc_regs_i2c.h"
#include "soc/regs/soc_regs_dma.h"

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
static_assert(CDC_DMA_INTCLR == dma::INTCLR, "dma INTCLR drift");
static_assert(CDC_DMA_CSR0   == dma::CSR0,   "dma CSR0 drift");
static_assert(CDC_DMA_SAR0   == dma::SAR0,   "dma SAR0 drift");
static_assert(CDC_DMA_DAR0   == dma::DAR0,   "dma DAR0 drift");
static_assert(CDC_DMA_CCR0   == dma::CCR0,   "dma CCR0 drift");
static_assert(CDC_DMA_CH_AXI_STRIDE  == dma::CHANNEL_AXI_STRIDE,    "dma AXI stride drift");
static_assert(CDC_DMA_CH_STAT_STRIDE == dma::CHANNEL_STATUS_STRIDE, "dma stat stride drift");
/* CCR_RESET_VALUE is private in the model, so it cannot be static_asserted here;
 * the value in soc_regs_dma.h is copied from the model and left informational. */

int main() { return 0; }
