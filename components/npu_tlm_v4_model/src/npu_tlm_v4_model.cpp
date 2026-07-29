// SPDX-License-Identifier: Apache-2.0

#include "npu_tlm_v4_model.h"

#include "npu_tlm_v4_regmap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <vector>

#include <libsauria_cfg.h>
#include <npu_top.h>
#include <npu_profile.h>
#include <perf_counters.h>
#include <sauria_run.h>
#include <sauria_targets.h>

namespace cdc::components {
namespace {

using namespace npu_v4_reg;

constexpr std::uint64_t kRamBase = 0x8000'0000ULL;
constexpr std::uint64_t kRamSize = 0x1000'0000ULL;
constexpr std::uint32_t kRows = 32;
constexpr std::uint32_t kCols = 32;
constexpr std::uint32_t kMaxK = 1024 - kCols;
constexpr std::uint32_t kCoreTimeoutCycles = 50'000;
constexpr std::uint32_t kStatusPollCycles = 8;
constexpr std::uint32_t kInt8Bytes = 1;
constexpr std::uint32_t kInt32Bytes = 4;
constexpr std::uint32_t kConfigRegisterBytes = 42 * sizeof(std::uint32_t);
constexpr std::uint32_t kRichModelBase = 0x4000'0000;

struct vp_alias_range {
    std::uint32_t alias_base;
    std::uint32_t size;
    std::uint32_t model_base;
};

constexpr std::array<vp_alias_range, 14> kTableAliases{{
    {0x0002'0000, 0x2000, 0x0014'0000},
    {0x0002'2000, 0x1000, 0x0015'0000},
    {0x0002'3000, 0x1000, 0x0018'0000},
    {0x0002'4000, 0x1000, 0x0019'0000},
    {0x0002'5000, 0x2000, 0x0016'0000},
    {0x0002'7000, 0x1000, 0x0017'0000},
    {0x0002'8000, 0x1000, 0x001A'0000},
    {0x0002'9000, 0x1000, 0x001B'0000},
    {0x0002'A000, 0x1000, 0x0020'0000},
    {0x0002'B000, 0x1000, 0x0021'0000},
    {0x0002'C000, 0x1000, 0x0022'0000},
    {0x0002'D000, 0x1000, 0x0023'0000},
    {0x0002'E000, 0x1000, 0x0024'0000},
    {0x0002'F000, 0x1000, 0x0025'0000},
}};

// Match the V4.2 FVP bridge build. INT8 jobs are staged as exact integer-valued
// floats, while the float core also supports V4.2 rich-instruction scale fields.
using core_type =
    sauria::NpuTop<32, 32, float, float, float,
                   1024, 1024, 2048, 16, 64, 1>;

sauria::PeConfig exact_int8_config()
{
    sauria::PeConfig cfg;
    cfg.arithmetic_type = 0;
    cfg.mul_type = 0;
    cfg.add_type = 0;
    cfg.stages_mul = 1;
    cfg.intermediate_pipeline_stage = true;
    cfg.zero_gating_mult = false;
    return cfg;
}

bool physical_ram_range(std::uint32_t addr, std::uint32_t size)
{
    if (size == 0u || addr < kRamBase) {
        return false;
    }
    const std::uint64_t start = addr;
    const std::uint64_t end = start + size;
    return end >= start && end <= kRamBase + kRamSize;
}

float bits_to_float(std::uint32_t bits)
{
    float value = 0.0f;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t low32(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value & 0xFFFF'FFFFULL);
}

std::uint32_t high32(std::uint64_t value)
{
    return static_cast<std::uint32_t>(value >> 32);
}

std::int32_t clamp_i64(std::int64_t value)
{
    if (value > std::numeric_limits<std::int32_t>::max()) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (value < std::numeric_limits<std::int32_t>::min()) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(value);
}

std::int32_t rounding_divide_by_pot(std::int64_t value, int shift)
{
    if (shift <= 0) {
        return clamp_i64(value);
    }

    const std::int64_t mask = (std::int64_t{1} << shift) - 1;
    const std::int64_t remainder = value & mask;
    const std::int64_t threshold = (mask >> 1) + ((value < 0) ? 1 : 0);
    return clamp_i64((value >> shift) +
                     ((remainder > threshold) ? 1 : 0));
}

std::int32_t multiply_by_quantized_multiplier(std::int32_t value,
                                              std::int32_t multiplier,
                                              std::int8_t shift)
{
    std::int64_t scaled = static_cast<std::int64_t>(value) * multiplier;
    scaled = (scaled + (std::int64_t{1} << 30)) >> 31;

    if (shift > 0) {
        return clamp_i64(scaled << shift);
    }
    return rounding_divide_by_pot(scaled, -shift);
}

std::uint32_t make_sram_a_addr(std::uint32_t phys_addr,
                               std::uint32_t sub_word)
{
    return sauria::SRAMA_OFFSET | (phys_addr << sauria::SHIFT_A) |
           (sub_word & sauria::MASK_A);
}

std::uint32_t make_sram_b_addr(std::uint32_t phys_addr,
                               std::uint32_t sub_word)
{
    return sauria::SRAMB_OFFSET | (phys_addr << sauria::SHIFT_B) |
           (sub_word & sauria::MASK_B);
}

std::uint32_t make_sram_c_addr(std::uint32_t phys_addr,
                               std::uint32_t sub_word)
{
    return sauria::SRAMC_OFFSET | (phys_addr << sauria::SHIFT_C) |
           (sub_word & sauria::MASK_C);
}

sauria::SauriaLayerDesc make_gemm_as_conv1x1_desc(std::uint32_t rows,
                                                   std::uint32_t cols,
                                                   std::uint32_t depth,
                                                   std::uint32_t tile_rows,
                                                   std::uint32_t tile_cols)
{
    sauria::SauriaLayerDesc desc;
    desc.B_w = 1;
    desc.B_h = 1;
    desc.d = 1;
    desc.s = 1;
    desc.c_til = static_cast<int>(depth);
    desc.k_til = static_cast<int>(tile_cols);
    desc.h_til = 1;
    desc.w_til = static_cast<int>(tile_rows);
    desc.X_used = static_cast<int>(tile_cols);
    desc.Y_used = static_cast<int>(tile_rows);
    desc.preload_en = 0;
    desc.C_w = static_cast<int>(rows);
    desc.C_h = 1;
    desc.C_c = static_cast<int>(cols);
    desc.A_c = static_cast<int>(depth);
    return desc;
}

} // namespace

struct npu_tlm_v4_model::impl : public sc_core::sc_module {
    enum class native_command {
        none,
        read,
        write,
    };

    struct register_file {
        std::uint32_t ctrl = 0;
        std::uint32_t status = STATUS_IDLE;
        std::uint32_t irq_enable = 0;
        std::uint32_t irq_status = 0;
        std::uint32_t src_addr = 0;
        std::uint32_t dst_addr = 0;
        std::uint32_t scratch_addr = 0;
        std::uint32_t src_size = 0;
        std::uint32_t dst_size = 0;
        std::uint32_t width = kCols;
        std::uint32_t height = kRows;
        std::uint32_t src_stride = 0;
        std::uint32_t format = FORMAT_INT8_INT8_INT32;
        std::uint32_t op_mode = OP_GEMM;
        std::uint32_t weights_addr = 0;
        std::uint32_t param_addr = 0;
        std::uint32_t weights_size = 0;
        std::uint32_t k_dimension = 64;
        std::uint32_t threshold_bits = 0;
        std::uint32_t rows_active = 0xFFFF'FFFFu;
        std::uint32_t dilation_pattern = 1;
        std::uint32_t input_offset = 0;
        std::uint32_t weight_offset = 0;
        std::uint32_t output_offset = 0;
        std::uint32_t activation_min = static_cast<std::uint32_t>(-128);
        std::uint32_t activation_max = 127;
        std::uint32_t bias_addr = 0;
        std::uint32_t bias_size = 0;
        std::uint32_t multiplier_addr = 0;
        std::uint32_t multiplier_size = 0;
        std::uint32_t shift_addr = 0;
        std::uint32_t shift_size = 0;
        std::uint32_t cycle_count = 0;
        std::uint32_t bytes_read = 0;
        std::uint32_t bytes_written = 0;
        std::uint32_t last_error =
            static_cast<std::uint32_t>(error_code::none);
    };

    npu_tlm_v4_model& owner;
    sc_core::sc_time access_latency;

    /* Gated core clock: a free-running sc_clock at the 2 ns core period
     * dominates simulation cost for the whole platform even while the NPU
     * is idle (~5e8 edges per simulated second). The clock thread below only
     * toggles while the worker consumes edges through wait_clock(). */
    sc_core::sc_signal<bool> core_clk{"core_clk"};
    sc_core::sc_time clk_half_period;
    bool clk_enabled = false;
    sc_core::sc_event clk_enable_event;
    core_type core;

    sc_core::sc_signal<bool> core_rst_n{"core_rst_n"};
    sc_core::sc_signal<bool> core_soft_reset{"core_soft_reset"};
    sc_core::sc_signal<bool> core_start{"core_start"};
    sc_core::sc_signal<bool> core_done{"core_done"};
    sc_core::sc_signal<bool> core_deadlock{"core_deadlock"};
    sc_core::sc_signal<std::uint32_t> mvm_k{"mvm_k"};
    sc_core::sc_signal<std::uint32_t> host_addr{"host_addr"};
    sc_core::sc_signal<bool> host_wren{"host_wren"};
    sc_core::sc_signal<bool> host_rden{"host_rden"};
    sc_core::sc_signal<sauria::host_data_t> host_wdata{"host_wdata"};
    sc_core::sc_signal<sauria::host_mask_t> host_wmask{"host_wmask"};
    sc_core::sc_signal<sauria::host_data_t> host_rdata{"host_rdata"};
    sc_core::sc_signal<float> threshold{"threshold"};
    sc_core::sc_signal<sc_dt::sc_bv<3>> buffer_select{"buffer_select"};
    sc_core::sc_signal<std::uint32_t> total_contexts{"total_contexts"};
    sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS> irq_level{"irq_level"};
    fx1::PerfCounters perf;
    std::uint32_t native_mvm_k = 0;
    std::uint32_t native_total_contexts = 0;

    register_file regs;
    sc_core::sc_event work_event;
    bool job_pending = false;
    bool soft_reset_pending = false;
    native_command native_cmd = native_command::none;
    std::uint32_t native_address = 0;
    std::uint32_t native_write_value = 0;
    std::uint32_t native_read_value = 0;
    tlm::tlm_response_status native_status =
        tlm::TLM_INCOMPLETE_RESPONSE;
    bool native_pending = false;
    bool native_active = false;
    sc_core::sc_event native_done_event;

    impl(sc_core::sc_module_name name,
         npu_tlm_v4_model& owner_ref,
         sc_core::sc_time clock_period,
         sc_core::sc_time mmio_latency)
        : sc_core::sc_module(name)
        , owner(owner_ref)
        , access_latency(mmio_latency)
        , clk_half_period(clock_period / 2)
        , core("sauria_core", exact_int8_config())
    {
        sc_core::sc_spawn(sc_bind(&impl::clock_thread, this),
                          "core_clk_gen");
        core.i_clk(core_clk);
        core.i_rstn(core_rst_n);
        core.i_soft_reset(core_soft_reset);
        core.i_start(core_start);
        core.o_done(core_done);
        core.o_deadlock(core_deadlock);
        core.i_mvm_k(mvm_k);
        core.i_host_addr(host_addr);
        core.i_host_wren(host_wren);
        core.i_host_rden(host_rden);
        core.i_host_wdata(host_wdata);
        core.i_host_wmask(host_wmask);
        core.o_host_rdata(host_rdata);
        core.i_threshold(threshold);
        core.i_select(buffer_select);
        core.i_total_contexts(total_contexts);
        core.attach_perf(&perf);
        perf.X = kCols;
        perf.Y = kRows;

        core_rst_n.write(false);
        core_soft_reset.write(false);
        core_start.write(false);
        host_addr.write(0);
        host_wren.write(false);
        host_rden.write(false);
        host_wdata.write(sauria::host_data_t());
        host_wmask.write(sauria::host_mask_t());
        mvm_k.write(0);
        threshold.write(0.0f);
        buffer_select.write(sc_dt::sc_bv<3>("000"));
        total_contexts.write(0);
    }

    bool busy() const { return (regs.status & STATUS_BUSY) != 0u; }

    void abort_native_request()
    {
        if (!native_pending) {
            return;
        }
        native_status = tlm::TLM_COMMAND_ERROR_RESPONSE;
        native_cmd = native_command::none;
        native_pending = false;
        native_done_event.notify(sc_core::SC_ZERO_TIME);
    }

    void update_irq()
    {
        const bool level =
            (regs.ctrl & CTRL_IRQ_EN) != 0u &&
            (regs.irq_enable & regs.irq_status) != 0u;
        irq_level.write(level);
    }

    void reset_registers()
    {
        abort_native_request();
        regs = register_file{};
        job_pending = false;
        soft_reset_pending = false;
        perf.reset();
        native_mvm_k = 0;
        native_total_contexts = 0;
        update_irq();
    }

    void finish_success()
    {
        regs.status = STATUS_DONE | STATUS_IDLE;
        regs.irq_status |= IRQ_DONE;
        regs.last_error = static_cast<std::uint32_t>(error_code::none);
        update_irq();
    }

    void finish_error(error_code code)
    {
        regs.status = STATUS_ERROR | STATUS_IDLE;
        regs.irq_status |= IRQ_ERROR;
        regs.last_error = static_cast<std::uint32_t>(code);
        update_irq();
    }

    tlm::tlm_response_status read_reg(std::uint32_t offset, std::uint32_t& value)
    {
        switch (offset) {
        case CTRL:               value = regs.ctrl; break;
        case STATUS:             value = regs.status; break;
        case IRQ_ENABLE:         value = regs.irq_enable; break;
        case IRQ_STATUS:         value = regs.irq_status; break;
        case SRC_ADDR:           value = regs.src_addr; break;
        case DST_ADDR:           value = regs.dst_addr; break;
        case SCRATCH_ADDR:       value = regs.scratch_addr; break;
        case SRC_SIZE_BYTES:     value = regs.src_size; break;
        case DST_SIZE_BYTES:     value = regs.dst_size; break;
        case WIDTH:              value = regs.width; break;
        case HEIGHT:             value = regs.height; break;
        case SRC_STRIDE_BYTES:   value = regs.src_stride; break;
        case FORMAT:             value = regs.format; break;
        case OP_MODE:            value = regs.op_mode; break;
        case WEIGHTS_ADDR:       value = regs.weights_addr; break;
        case PARAM_ADDR:         value = regs.param_addr; break;
        case WEIGHTS_SIZE_BYTES: value = regs.weights_size; break;
        case K_DIMENSION:        value = regs.k_dimension; break;
        case ZERO_THRESHOLD_FP32:value = regs.threshold_bits; break;
        case ROWS_ACTIVE:        value = regs.rows_active; break;
        case DILATION_PATTERN:   value = regs.dilation_pattern; break;
        case CYCLE_COUNT:        value = regs.cycle_count; break;
        case BYTES_READ:         value = regs.bytes_read; break;
        case BYTES_WRITTEN:      value = regs.bytes_written; break;
        case LAST_ERROR:         value = regs.last_error; break;
        case CORE_ID:            value = CORE_ID_VALUE; break;
        case INPUT_OFFSET:       value = regs.input_offset; break;
        case WEIGHT_OFFSET:      value = regs.weight_offset; break;
        case OUTPUT_OFFSET:      value = regs.output_offset; break;
        case ACTIVATION_MIN:     value = regs.activation_min; break;
        case ACTIVATION_MAX:     value = regs.activation_max; break;
        case BIAS_ADDR:          value = regs.bias_addr; break;
        case BIAS_SIZE_BYTES:    value = regs.bias_size; break;
        case MULTIPLIER_ADDR:    value = regs.multiplier_addr; break;
        case MULTIPLIER_SIZE_BYTES: value = regs.multiplier_size; break;
        case SHIFT_ADDR:         value = regs.shift_addr; break;
        case SHIFT_SIZE_BYTES:   value = regs.shift_size; break;
        case PERF_EXEC_CYCLES:   value = low32(perf.exec_cycles); break;
        case PERF_STALL_CYCLES:  value = low32(perf.stall_cycles); break;
        case PERF_MAC_OPS:       value = low32(perf.mac_ops); break;
        case PERF_ACTIVE_PE_CYCLES:
            value = low32(perf.active_pe_cycles);
            break;
        case PERF_TOTAL_PE_CYCLES:
            value = low32(perf.total_pe_cycles);
            break;
        case PERF_TOTAL_CYCLES:  value = low32(perf.total_cycles); break;
        case PERF_SA_CYCLES:
            value = low32(perf.sa_cycles);
            break;
        case PERF_OBP_CYCLES:
            value = low32(perf.obp_cycles);
            break;
        default:
            return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }
        return tlm::TLM_OK_RESPONSE;
    }

    tlm::tlm_response_status write_reg(std::uint32_t offset, std::uint32_t value)
    {
        if (offset == STATUS) {
            if ((value & STATUS_DONE) != 0u) {
                regs.status &= ~STATUS_DONE;
                regs.irq_status &= ~IRQ_DONE;
            }
            if ((value & STATUS_ERROR) != 0u) {
                regs.status &= ~STATUS_ERROR;
                regs.irq_status &= ~IRQ_ERROR;
                regs.last_error =
                    static_cast<std::uint32_t>(error_code::none);
            }
            if (!busy()) {
                regs.status |= STATUS_IDLE;
            }
            update_irq();
            return tlm::TLM_OK_RESPONSE;
        }

        if (offset == IRQ_STATUS) {
            regs.irq_status &= ~(value & (IRQ_DONE | IRQ_ERROR));
            update_irq();
            return tlm::TLM_OK_RESPONSE;
        }

        if (offset == IRQ_ENABLE) {
            regs.irq_enable = value & (IRQ_DONE | IRQ_ERROR);
            update_irq();
            return tlm::TLM_OK_RESPONSE;
        }

        if (offset == CTRL) {
            if (busy() && (value & (CTRL_START | CTRL_SOFT_RESET)) != 0u) {
                regs.last_error = static_cast<std::uint32_t>(error_code::busy);
                return tlm::TLM_COMMAND_ERROR_RESPONSE;
            }

            regs.ctrl = value & (CTRL_ENABLE | CTRL_IRQ_EN);
            if ((value & CTRL_SOFT_RESET) != 0u) {
                soft_reset_pending = true;
                work_event.notify(sc_core::SC_ZERO_TIME);
            }
            if ((value & CTRL_START) != 0u) {
                if ((regs.ctrl & CTRL_ENABLE) == 0u) {
                    finish_error(error_code::disabled);
                } else {
                    regs.status = STATUS_BUSY;
                    regs.irq_status = 0;
                    regs.last_error =
                        static_cast<std::uint32_t>(error_code::none);
                    regs.cycle_count = 0;
                    regs.bytes_read = 0;
                    regs.bytes_written = 0;
                    job_pending = true;
                    work_event.notify(sc_core::SC_ZERO_TIME);
                    update_irq();
                }
            }
            update_irq();
            return tlm::TLM_OK_RESPONSE;
        }

        if (busy()) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }

        switch (offset) {
        case SRC_ADDR:           regs.src_addr = value; break;
        case DST_ADDR:           regs.dst_addr = value; break;
        case SCRATCH_ADDR:       regs.scratch_addr = value; break;
        case SRC_SIZE_BYTES:     regs.src_size = value; break;
        case DST_SIZE_BYTES:     regs.dst_size = value; break;
        case WIDTH:              regs.width = value; break;
        case HEIGHT:             regs.height = value; break;
        case SRC_STRIDE_BYTES:   regs.src_stride = value; break;
        case FORMAT:             regs.format = value; break;
        case OP_MODE:            regs.op_mode = value; break;
        case WEIGHTS_ADDR:       regs.weights_addr = value; break;
        case PARAM_ADDR:         regs.param_addr = value; break;
        case WEIGHTS_SIZE_BYTES: regs.weights_size = value; break;
        case K_DIMENSION:        regs.k_dimension = value; break;
        case ZERO_THRESHOLD_FP32:regs.threshold_bits = value; break;
        case ROWS_ACTIVE:        regs.rows_active = value; break;
        case DILATION_PATTERN:   regs.dilation_pattern = value; break;
        case INPUT_OFFSET:       regs.input_offset = value; break;
        case WEIGHT_OFFSET:      regs.weight_offset = value; break;
        case OUTPUT_OFFSET:      regs.output_offset = value; break;
        case ACTIVATION_MIN:     regs.activation_min = value; break;
        case ACTIVATION_MAX:     regs.activation_max = value; break;
        case BIAS_ADDR:          regs.bias_addr = value; break;
        case BIAS_SIZE_BYTES:    regs.bias_size = value; break;
        case MULTIPLIER_ADDR:    regs.multiplier_addr = value; break;
        case MULTIPLIER_SIZE_BYTES: regs.multiplier_size = value; break;
        case SHIFT_ADDR:         regs.shift_addr = value; break;
        case SHIFT_SIZE_BYTES:   regs.shift_size = value; break;
        case CYCLE_COUNT:
        case BYTES_READ:
        case BYTES_WRITTEN:
        case LAST_ERROR:
        case CORE_ID:
        case PERF_EXEC_CYCLES:
        case PERF_STALL_CYCLES:
        case PERF_MAC_OPS:
        case PERF_ACTIVE_PE_CYCLES:
        case PERF_TOTAL_PE_CYCLES:
        case PERF_TOTAL_CYCLES:
        case PERF_SA_CYCLES:
        case PERF_OBP_CYCLES:
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        default:
            return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }
        return tlm::TLM_OK_RESPONSE;
    }

    void clock_thread()
    {
        core_clk.write(false);
        for (;;) {
            while (!clk_enabled) {
                sc_core::wait(clk_enable_event);
            }
            core_clk.write(true);
            sc_core::wait(clk_half_period);
            core_clk.write(false);
            sc_core::wait(clk_half_period);
        }
    }

    void set_clock_running(bool on)
    {
        if (on && !clk_enabled) {
            clk_enabled = true;
            clk_enable_event.notify(sc_core::SC_ZERO_TIME);
        } else if (!on) {
            clk_enabled = false;
        }
    }

    bool wait_clock(unsigned count = 1)
    {
        set_clock_running(true);
        for (unsigned i = 0; i < count; ++i) {
            sc_core::wait(core_clk.posedge_event());
            sc_core::wait(sc_core::SC_ZERO_TIME);
            if (!owner.reset_n.read()) {
                return false;
            }
        }
        return true;
    }

    void drive_idle_host()
    {
        core_start.write(false);
        core_soft_reset.write(false);
        host_wren.write(false);
        host_rden.write(false);
        host_addr.write(0);
    }

    bool pulse_core_soft_reset()
    {
        drive_idle_host();
        buffer_select.write(sc_dt::sc_bv<3>("000"));
        core_soft_reset.write(true);
        if (!wait_clock(2)) {
            return false;
        }
        core_soft_reset.write(false);
        return wait_clock(2);
    }

    bool core_host_write(std::uint32_t address,
                         const sauria::host_data_t& data,
                         const sauria::host_mask_t& mask)
    {
        if (!wait_clock()) {
            return false;
        }
        host_addr.write(address);
        host_wdata.write(data);
        host_wmask.write(mask);
        host_rden.write(false);
        host_wren.write(true);
        if (!wait_clock()) {
            return false;
        }
        host_wren.write(false);
        return wait_clock();
    }

    bool core_host_read(std::uint32_t address, sauria::host_data_t& data)
    {
        if (!wait_clock()) {
            return false;
        }
        host_addr.write(address);
        host_wren.write(false);
        host_rden.write(true);
        if (!wait_clock(2)) {
            return false;
        }
        host_rden.write(false);
        if (!wait_clock()) {
            return false;
        }
        data = host_rdata.read();
        return true;
    }

    bool is_sram_address(std::uint32_t address) const
    {
        const std::uint32_t region =
            address & sauria::SAURIA_MEM_ADDR_MASK;
        return region == sauria::SRAMA_OFFSET ||
               region == sauria::SRAMB_OFFSET ||
               region == sauria::SRAMC_OFFSET;
    }

    bool is_rows_active_address(std::uint32_t address) const
    {
        const std::uint32_t region =
            address & sauria::SAURIA_MEM_ADDR_MASK;
        const std::uint32_t local =
            address & ~sauria::SAURIA_MEM_ADDR_MASK;
        return region == sauria::CFG_REGS_OFFSET &&
               local == sauria::CFG_ACT_OFFSET;
    }

    bool translate_vp_alias(std::uint32_t vp_address,
                            std::uint32_t& model_address) const
    {
        if (vp_address >= RICH_ALIAS_BASE &&
            vp_address < RICH_ALIAS_BASE + RICH_ALIAS_SIZE) {
            model_address =
                kRichModelBase + (vp_address - RICH_ALIAS_BASE);
            return true;
        }
        for (const auto& range : kTableAliases) {
            if (vp_address >= range.alias_base &&
                vp_address < range.alias_base + range.size) {
                model_address =
                    range.model_base + (vp_address - range.alias_base);
                return true;
            }
        }
        model_address = vp_address;
        return false;
    }

    bool is_packed_byte_address(std::uint32_t address) const
    {
        const std::uint32_t region = address & 0x00FF'0000u;
        return region == 0x0014'0000u || region == 0x0016'0000u ||
               region == 0x0020'0000u || region == 0x0021'0000u ||
               region == 0x0023'0000u || region == 0x0024'0000u;
    }

    bool is_packed_half_address(std::uint32_t address) const
    {
        const std::uint32_t region = address & 0x00FF'0000u;
        return region == 0x0022'0000u || region == 0x0025'0000u;
    }

    bool is_rich_float_address(std::uint32_t address) const
    {
        return address == 0x4000'0438u || address == 0x4000'043Cu ||
               address == 0x4000'0440u || address == 0x4000'0458u ||
               address == 0x4000'045Cu || address == 0x4000'0460u;
    }

    std::uint64_t perf_value(std::uint32_t low_address) const
    {
        switch (low_address) {
        case PERF_EXEC_CYCLES:       return perf.exec_cycles;
        case PERF_STALL_CYCLES:      return perf.stall_cycles;
        case PERF_MAC_OPS:           return perf.mac_ops;
        case PERF_ACTIVE_PE_CYCLES:  return perf.active_pe_cycles;
        case PERF_TOTAL_PE_CYCLES:   return perf.total_pe_cycles;
        case PERF_TOTAL_CYCLES:      return perf.total_cycles;
        case PERF_SA_CYCLES:         return perf.sa_cycles;
        case PERF_OBP_CYCLES:        return perf.obp_cycles;
        default:                     return 0;
        }
    }

    bool is_perf_low(std::uint32_t address) const
    {
        return address == PERF_EXEC_CYCLES ||
               address == PERF_STALL_CYCLES ||
               address == PERF_MAC_OPS ||
               address == PERF_ACTIVE_PE_CYCLES ||
               address == PERF_TOTAL_PE_CYCLES ||
               address == PERF_TOTAL_CYCLES ||
               address == PERF_SA_CYCLES ||
               address == PERF_OBP_CYCLES;
    }

    bool perf_low_for_high(std::uint32_t address,
                           std::uint32_t& low_address) const
    {
        switch (address) {
        case PERF_EXEC_CYCLES_HI:
            low_address = PERF_EXEC_CYCLES;
            return true;
        case PERF_STALL_CYCLES_HI:
            low_address = PERF_STALL_CYCLES;
            return true;
        case PERF_MAC_OPS_HI:
            low_address = PERF_MAC_OPS;
            return true;
        case PERF_ACTIVE_PE_CYCLES_HI:
            low_address = PERF_ACTIVE_PE_CYCLES;
            return true;
        case PERF_TOTAL_PE_CYCLES_HI:
            low_address = PERF_TOTAL_PE_CYCLES;
            return true;
        case PERF_TOTAL_CYCLES_HI:
            low_address = PERF_TOTAL_CYCLES;
            return true;
        case PERF_SA_CYCLES_HI:
            low_address = PERF_SA_CYCLES;
            return true;
        case PERF_OBP_CYCLES_HI:
            low_address = PERF_OBP_CYCLES;
            return true;
        default:
            return false;
        }
    }

    bool read_perf(std::uint32_t address, std::uint32_t& value) const
    {
        if (is_perf_low(address)) {
            value = low32(perf_value(address));
            return true;
        }
        std::uint32_t low_address = 0;
        if (perf_low_for_high(address, low_address)) {
            value = high32(perf_value(low_address));
            return true;
        }
        return false;
    }

    void update_native_sideband(std::uint32_t address, std::uint32_t value)
    {
        const std::uint32_t region =
            address & sauria::SAURIA_MEM_ADDR_MASK;
        const std::uint32_t local =
            address & ~sauria::SAURIA_MEM_ADDR_MASK;
        if (region != sauria::CFG_REGS_OFFSET) {
            return;
        }
        if (local == sauria::NCONTEXTS ||
            local == sauria::CFG_CON_OFFSET + 0x0C) {
            native_total_contexts = value;
            total_contexts.write(value);
        }
        if (local == sauria::TILE_C || local == sauria::IN_C ||
            local == sauria::WEI_KSTEP) {
            native_mvm_k = value;
            mvm_k.write(value);
        }
    }

    tlm::tlm_response_status native_write(std::uint32_t address,
                                          std::uint32_t value)
    {
        if (!owner.reset_n.read() || !core_rst_n.read() || busy()) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }

        std::uint32_t model_address = address;
        const bool is_alias =
            translate_vp_alias(address, model_address);

        const std::uint32_t local =
            model_address & ~sauria::SAURIA_MEM_ADDR_MASK;
        const std::uint32_t region =
            model_address & sauria::SAURIA_MEM_ADDR_MASK;
        const bool native_start =
            !is_alias && region == sauria::CFG_REGS_OFFSET &&
            local == 0u && value != 0u;
        const bool instruction_start =
            model_address == 0x4000'0310u ||
            model_address == 0x4000'0314u;
        if (native_start || instruction_start) {
            perf.reset();
        }

        sauria::host_data_t data;
        sauria::host_mask_t mask;
        data.data.fill(0.0);
        mask.data.fill(false);

        if (is_sram_address(model_address) ||
            is_rows_active_address(model_address)) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) {
                data[lane] = static_cast<double>(
                    static_cast<std::int8_t>(
                        (value >> (lane * 8)) & 0xFFu));
                mask[lane] = true;
            }
        } else if (is_packed_byte_address(model_address)) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) {
                data[lane] =
                    static_cast<double>((value >> (lane * 8)) & 0xFFu);
                mask[lane] = true;
            }
        } else if (is_packed_half_address(model_address)) {
            const std::uint32_t first_lane =
                ((model_address & 0xFFFFu) >> 1) & 3u;
            data[first_lane] = static_cast<double>(value & 0xFFFFu);
            mask[first_lane] = true;
            if (first_lane + 1u < 4u) {
                data[first_lane + 1u] =
                    static_cast<double>((value >> 16) & 0xFFFFu);
                mask[first_lane + 1u] = true;
            }
        } else if (is_rich_float_address(model_address)) {
            data[0] = static_cast<double>(bits_to_float(value));
            mask[0] = true;
        } else {
            data[0] = static_cast<double>(value);
            mask[0] = true;
            if (local == 0u) {
                const std::uint32_t upper = (value >> 16) & 0xFFu;
                data[2] = static_cast<double>(upper);
                mask[2] = upper != 0u;
            }
        }

        update_native_sideband(model_address, value);
        if (native_start) {
            buffer_select.write(sc_dt::sc_bv<3>("111"));
            total_contexts.write(native_total_contexts);
            mvm_k.write(native_mvm_k);
        }
        return core_host_write(model_address, data, mask)
                   ? tlm::TLM_OK_RESPONSE
                   : tlm::TLM_COMMAND_ERROR_RESPONSE;
    }

    tlm::tlm_response_status native_read(std::uint32_t address,
                                         std::uint32_t& value)
    {
        if (read_perf(address, value)) {
            return tlm::TLM_OK_RESPONSE;
        }
        if (!owner.reset_n.read() || !core_rst_n.read() || busy()) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }

        std::uint32_t model_address = address;
        translate_vp_alias(address, model_address);

        sauria::host_data_t data;
        if (!core_host_read(model_address, data)) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }

        value = 0;
        if (is_sram_address(model_address) ||
            is_rows_active_address(model_address) ||
            is_packed_byte_address(model_address)) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) {
                const auto lane_value =
                    static_cast<std::int32_t>(std::llround(data[lane]));
                value |= (static_cast<std::uint32_t>(lane_value) & 0xFFu)
                         << (lane * 8);
            }
        } else if (is_packed_half_address(model_address)) {
            const std::uint32_t first_lane =
                ((model_address & 0xFFFFu) >> 1) & 3u;
            value = static_cast<std::uint32_t>(
                        std::llround(data[first_lane])) &
                    0xFFFFu;
            if (first_lane + 1u < 4u) {
                value |=
                    (static_cast<std::uint32_t>(
                         std::llround(data[first_lane + 1u])) &
                     0xFFFFu)
                    << 16;
            }
        } else {
            value = static_cast<std::uint32_t>(std::llround(data[0]));
        }
        return tlm::TLM_OK_RESPONSE;
    }

    tlm::tlm_response_status submit_native(
        native_command command,
        std::uint32_t address,
        std::uint32_t& value)
    {
        if (command == native_command::read && read_perf(address, value)) {
            return tlm::TLM_OK_RESPONSE;
        }
        if (!owner.reset_n.read() || !core_rst_n.read() || busy() ||
            native_active) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }

        native_active = true;
        native_cmd = command;
        native_address = address;
        native_write_value = value;
        native_read_value = 0;
        native_status = tlm::TLM_INCOMPLETE_RESPONSE;
        native_pending = true;
        work_event.notify(sc_core::SC_ZERO_TIME);

        sc_core::wait(native_done_event | owner.reset_n.negedge_event());
        if (native_pending) {
            native_cmd = native_command::none;
            native_pending = false;
            native_active = false;
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }
        if (command == native_command::read &&
            native_status == tlm::TLM_OK_RESPONSE) {
            value = native_read_value;
        }
        native_active = false;
        return native_status;
    }

    void service_native_request()
    {
        if (!native_pending) {
            return;
        }

        std::uint32_t value = native_write_value;
        if (native_cmd == native_command::read) {
            native_status = native_read(native_address, value);
            native_read_value = value;
        } else if (native_cmd == native_command::write) {
            native_status = native_write(native_address, value);
        } else {
            native_status = tlm::TLM_COMMAND_ERROR_RESPONSE;
        }

        native_cmd = native_command::none;
        native_pending = false;
        native_done_event.notify(sc_core::SC_ZERO_TIME);
    }

    bool write_scalar(std::uint32_t address, std::uint32_t value)
    {
        sauria::host_data_t data;
        sauria::host_mask_t mask;
        data[0] = static_cast<float>(value);
        mask[0] = true;
        return core_host_write(address, data, mask);
    }

    bool program_core_registers(const sauria::SauriaLayerDesc& desc,
                                const sauria::SauriaTarget& target)
    {
        std::uint64_t fields[sauria::F_CFG_COUNT];
        sauria::sauria_compute_core_fields(desc, target, fields);

        /*
         * V4.2 PROFILE_V1_SAURIA path uses the SAURIA address generators and
         * PSM schedule. The
         * previous port selected V4_LINEAR but only programmed three control
         * words; that leaves the unified feeders waiting forever on their
         * first synchronized vector.  Program the complete native single-tile
         * schedule used by the V4.2 reference testbench.
         */
        if (!write_scalar(sauria::CFG_REGS_OFFSET |
                              sauria::CFG_PROFILE_ADDR,
                          sauria::PROFILE_V1_SAURIA) ||
            !write_scalar(sauria::CFG_REGS_OFFSET |
                              (sauria::CFG_CON_OFFSET + 0x00),
                          regs.k_dimension) ||
            !write_scalar(sauria::CFG_REGS_OFFSET |
                              (sauria::CFG_CON_OFFSET + 0x04),
                          static_cast<std::uint32_t>(
                              fields[sauria::F_CFG_ACT_REPS])) ||
            !write_scalar(sauria::CFG_REGS_OFFSET |
                              (sauria::CFG_CON_OFFSET + 0x08),
                          static_cast<std::uint32_t>(
                              fields[sauria::F_CFG_WEI_REPS]))) {
            return false;
        }

        sauria::host_data_t rows;
        sauria::host_mask_t full_mask;
        full_mask.data.fill(true);
        for (unsigned i = 0; i < 4; ++i) {
            rows[i] = static_cast<float>(
                (regs.rows_active >> (i * 8)) & 0xFFu);
        }
        if (!core_host_write(sauria::CFG_REGS_OFFSET |
                                 (sauria::CFG_ACT_OFFSET + 0x00),
                             rows, full_mask)) {
            return false;
        }

        const auto wr = [this](std::uint32_t address,
                               std::uint64_t value) {
            return write_scalar(sauria::CFG_REGS_OFFSET | address,
                                static_cast<std::uint32_t>(value));
        };

        return
            // Activation feeder: compatibility counters plus native addr-gen.
            wr(sauria::CFG_ACT_OFFSET + 0x04, regs.k_dimension) &&
            wr(sauria::CFG_ACT_OFFSET + 0x08,
               fields[sauria::F_CFG_XSTEP]) &&
            wr(sauria::CFG_ACT_OFFSET + 0x0C,
               fields[sauria::F_CFG_XLIM]) &&
            wr(sauria::CFG_ACT_OFFSET + 0x10,
               fields[sauria::F_CFG_XSTEP]) &&
            wr(sauria::CFG_ACT_OFFSET + 0x28,
               fields[sauria::F_CFG_DIL_PAT]) &&
            wr(sauria::ACT_XLIM, fields[sauria::F_CFG_XLIM]) &&
            wr(sauria::ACT_XSTEP, fields[sauria::F_CFG_XSTEP]) &&
            wr(sauria::ACT_YLIM, fields[sauria::F_CFG_YLIM]) &&
            wr(sauria::ACT_YSTEP, fields[sauria::F_CFG_YSTEP]) &&
            wr(sauria::ACT_CHLIM, fields[sauria::F_CFG_CHLIM]) &&
            wr(sauria::ACT_CHSTEP, fields[sauria::F_CFG_CHSTEP]) &&
            wr(sauria::ACT_TIL_XLIM,
               fields[sauria::F_CFG_TIL_XLIM]) &&
            wr(sauria::ACT_TIL_XSTEP,
               fields[sauria::F_CFG_TIL_XSTEP]) &&
            wr(sauria::ACT_TIL_YLIM,
               fields[sauria::F_CFG_TIL_YLIM]) &&
            wr(sauria::ACT_TIL_YSTEP,
               fields[sauria::F_CFG_TIL_YSTEP]) &&

            // Weight feeder.
            wr(sauria::CFG_WEI_OFFSET + 0x04,
               fields[sauria::F_CFG_WLIM]) &&
            wr(sauria::CFG_WEI_OFFSET + 0x08,
               fields[sauria::F_CFG_WSTEP]) &&
            wr(sauria::WEI_WLIM, fields[sauria::F_CFG_WLIM]) &&
            wr(sauria::WEI_WSTEP, fields[sauria::F_CFG_WSTEP]) &&
            wr(sauria::WEI_KLIM, fields[sauria::F_CFG_KLIM]) &&
            wr(sauria::WEI_KSTEP, fields[sauria::F_CFG_KSTEP]) &&
            wr(sauria::WEI_TIL_XLIM,
               fields[sauria::F_CFG_TIL_KLIM]) &&
            wr(sauria::WEI_TIL_XSTEP,
               fields[sauria::F_CFG_TIL_KSTEP]) &&
            wr(sauria::WEI_COLS_ACTIVE,
               fields[sauria::F_CFG_COLS_ACTIVE]) &&
            wr(sauria::WEI_WALIGNED,
               fields[sauria::F_CFG_WALIGNED]) &&

            // PSM/output schedule.
            wr(sauria::NCONTEXTS,
               fields[sauria::F_CFG_NCONTEXTS]) &&
            wr(sauria::CFG_OUT_OFFSET + 0x04,
               fields[sauria::F_CFG_CXLIM]) &&
            wr(sauria::CFG_OUT_OFFSET + 0x08,
               fields[sauria::F_CFG_CXSTEP]) &&
            wr(sauria::CFG_OUT_OFFSET + 0x0C,
               fields[sauria::F_CFG_CKLIM]) &&
            wr(sauria::CFG_OUT_OFFSET + 0x10,
               fields[sauria::F_CFG_CKSTEP]) &&
            wr(sauria::TIL_CYLIM,
               fields[sauria::F_CFG_TIL_CYLIM]) &&
            wr(sauria::TIL_CYSTEP,
               fields[sauria::F_CFG_TIL_CYSTEP]) &&
            wr(sauria::TIL_CKLIM,
               fields[sauria::F_CFG_TIL_CKLIM]) &&
            wr(sauria::TIL_CKSTEP,
               fields[sauria::F_CFG_TIL_CKSTEP]) &&
            wr(sauria::INACTIVE_COLS,
               fields[sauria::F_CFG_INACTIVE_COLS]) &&
            wr(sauria::PRELOAD_EN, 0u) &&

            // The wrapper preloads the private SRAMs from offset zero.
            wr(sauria::CFG_ACT_BASE_ADDR, 0u) &&
            wr(sauria::CFG_WEI_BASE_ADDR, 0u) &&
            wr(sauria::CFG_OUT_BASE_ADDR, 0u);
    }

    bool dma_read(std::uint32_t address, std::vector<std::uint8_t>& data,
                  std::uint32_t size)
    {
        data.assign(size, 0);
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data.data());
        trans.set_data_length(size);
        trans.set_streaming_width(size);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        owner.master_socket->b_transport(trans, delay);
        if (delay != sc_core::SC_ZERO_TIME) {
            sc_core::wait(delay);
        }
        return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    }

    bool dma_write(std::uint32_t address, const std::vector<std::uint8_t>& data)
    {
        std::vector<std::uint8_t> local = data;
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(local.data());
        trans.set_data_length(static_cast<unsigned>(local.size()));
        trans.set_streaming_width(static_cast<unsigned>(local.size()));
        trans.set_byte_enable_ptr(nullptr);
        trans.set_dmi_allowed(false);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        owner.master_socket->b_transport(trans, delay);
        if (delay != sc_core::SC_ZERO_TIME) {
            sc_core::wait(delay);
        }
        return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    }

    error_code validate_job(std::uint32_t& source_span) const
    {
        if (regs.width != kCols || regs.height != kRows ||
            regs.k_dimension == 0u || regs.k_dimension > kMaxK) {
            return error_code::invalid_dimensions;
        }
        if (regs.format != FORMAT_INT8_INT8_INT32) {
            return error_code::invalid_format;
        }
        if (regs.op_mode != OP_GEMM) {
            return error_code::invalid_operation;
        }

        const std::uint32_t stride =
            regs.src_stride == 0u ? regs.k_dimension : regs.src_stride;
        if (stride < regs.k_dimension) {
            return error_code::invalid_size;
        }

        const std::uint64_t span64 =
            static_cast<std::uint64_t>(stride) * (kRows - 1) +
            regs.k_dimension;
        if (span64 > std::numeric_limits<std::uint32_t>::max()) {
            return error_code::invalid_size;
        }
        source_span = static_cast<std::uint32_t>(span64);

        const std::uint32_t weights_need = regs.k_dimension * kCols;
        const std::uint32_t dst_need = kRows * kCols * sizeof(std::int32_t);
        if (regs.src_size < source_span ||
            regs.weights_size < weights_need ||
            regs.dst_size < dst_need) {
            return error_code::invalid_size;
        }
        if ((regs.dst_addr & 0x3u) != 0u ||
            !physical_ram_range(regs.src_addr, source_span) ||
            !physical_ram_range(regs.weights_addr, weights_need) ||
            !physical_ram_range(regs.dst_addr, dst_need)) {
            return error_code::invalid_address;
        }
        return error_code::none;
    }

    error_code execute_job()
    {
        perf.reset();

        std::uint32_t source_span = 0;
        error_code result = validate_job(source_span);
        if (result != error_code::none) {
            return result;
        }

        const std::uint32_t stride =
            regs.src_stride == 0u ? regs.k_dimension : regs.src_stride;
        const std::uint32_t weights_need = regs.k_dimension * kCols;

        std::vector<std::uint8_t> activations;
        std::vector<std::uint8_t> weights;
        if (!dma_read(regs.src_addr, activations, source_span) ||
            !owner.reset_n.read()) {
            return owner.reset_n.read() ? error_code::dma_read
                                        : error_code::reset_aborted;
        }
        if (!dma_read(regs.weights_addr, weights, weights_need) ||
            !owner.reset_n.read()) {
            return owner.reset_n.read() ? error_code::dma_read
                                        : error_code::reset_aborted;
        }
        regs.bytes_read = source_span + weights_need;

        if (!pulse_core_soft_reset()) {
            return error_code::reset_aborted;
        }
        threshold.write(bits_to_float(regs.threshold_bits));
        buffer_select.write(sc_dt::sc_bv<3>("000"));

        const sauria::SauriaTarget* target =
            sauria::sauria_find_target("int8_32x32");
        if (target == nullptr) {
            return error_code::invalid_format;
        }
        const sauria::SauriaLayerDesc desc =
            make_gemm_as_conv1x1_desc(kRows, kCols, regs.k_dimension,
                                      kRows, kCols);

        /*
         * Present the firmware's row-major GEMM operands as a 1x1 convolution:
         *   A [Cin][1][W]       = activation[row][k]
         *   B [Cout][Cin][1][1] = weight[k][col]
         * The V4.2 driver then provides the exact native SRAM byte layout.
         */
        std::vector<double> native_a(
            static_cast<std::size_t>(regs.k_dimension) * kRows);
        std::vector<double> native_b(
            static_cast<std::size_t>(kCols) * regs.k_dimension);
        std::vector<double> native_c(
            static_cast<std::size_t>(kCols) * kRows, 0.0);
        for (std::uint32_t k = 0; k < regs.k_dimension; ++k) {
            for (std::uint32_t row = 0; row < kRows; ++row) {
                native_a[static_cast<std::size_t>(k) * kRows + row] =
                    static_cast<std::int8_t>(
                        activations[static_cast<std::size_t>(row) * stride +
                                    k]);
            }
        }
        for (std::uint32_t col = 0; col < kCols; ++col) {
            for (std::uint32_t k = 0; k < regs.k_dimension; ++k) {
                native_b[static_cast<std::size_t>(col) *
                             regs.k_dimension +
                         k] =
                    static_cast<std::int8_t>(
                        weights[static_cast<std::size_t>(k) * kCols + col]);
            }
        }

        const sauria::SauriaRunInputs native =
            sauria::sauria_prepare(
                *target, desc,
                native_a.data(), regs.k_dimension, 1, kRows,
                native_b.data(), kCols, regs.k_dimension, 1, 1,
                native_c.data(), kCols, 1, kRows);

        // One public wrapper job is one complete 32x32 output tile.
        mvm_k.write(regs.k_dimension);
        total_contexts.write(1u);

        if (!program_core_registers(desc, *target)) {
            return error_code::reset_aborted;
        }

        sauria::host_mask_t full_mask;
        full_mask.data.fill(true);

        const auto preload_int8 =
            [this, &native, &full_mask](std::uint32_t sram_offset,
                                        std::uint32_t begin,
                                        std::uint32_t size) {
                std::uint32_t loaded = 0;
                for (std::uint32_t phys = 0; loaded < size; ++phys) {
                    for (std::uint32_t sw = 0; sw < 8 && loaded < size;
                         ++sw) {
                        sauria::host_data_t data;
                        data.data.fill(0.0f);
                        for (std::uint32_t lane = 0;
                             lane < 4 && loaded < size; ++lane, ++loaded) {
                            data[lane] = static_cast<float>(
                                static_cast<std::int8_t>(
                                    native.initial_dram[begin + loaded]));
                        }
                        const std::uint32_t address =
                            sram_offset | ((phys << 3) | sw);
                        if (!core_host_write(address, data, full_mask)) {
                            return false;
                        }
                    }
                }
                return true;
            };

        if (!preload_int8(sauria::SRAMA_OFFSET, native.A_off,
                          native.B_off - native.A_off) ||
            !preload_int8(sauria::SRAMB_OFFSET, native.B_off,
                          native.C_off - native.B_off)) {
            return error_code::reset_aborted;
        }

        /*
         * Clear the output SRAM because preload is disabled.  This also makes
         * repeated jobs deterministic if a future model version stops clearing
         * SRAM C as part of soft reset.
         */
        for (std::uint32_t col = 0; col < kCols; ++col) {
            for (std::uint32_t sw = 0; sw < 8; ++sw) {
                sauria::host_data_t data;
                data.data.fill(0.0f);
                const std::uint32_t address =
                    make_sram_c_addr(col * kRows, sw);
                if (!core_host_write(address, data, full_mask)) {
                    return error_code::reset_aborted;
                }
            }
        }

        buffer_select.write(sc_dt::sc_bv<3>("111"));
        if (!wait_clock(2)) {
            return error_code::reset_aborted;
        }
        core_start.write(true);
        if (!wait_clock()) {
            return error_code::reset_aborted;
        }
        core_start.write(false);

        bool completed = false;
        for (std::uint32_t cycle = 0; cycle < kCoreTimeoutCycles; ++cycle) {
            if (!wait_clock()) {
                return error_code::reset_aborted;
            }
            regs.cycle_count = cycle + 1;
            if (core_deadlock.read()) {
                return error_code::core_deadlock;
            }
            if (core_done.read()) {
                completed = true;
                break;
            }
        }
        if (!completed) {
            return error_code::core_timeout;
        }

        buffer_select.write(sc_dt::sc_bv<3>("000"));
        if (!wait_clock(2)) {
            return error_code::reset_aborted;
        }

        std::vector<std::uint8_t> output(
            kRows * kCols * sizeof(std::int32_t), 0);
        // Native C layout is [output channel][spatial row].
        for (std::uint32_t col = 0; col < kCols; ++col) {
            for (std::uint32_t sw = 0; sw < 8; ++sw) {
                sauria::host_data_t data;
                const std::uint32_t address =
                    make_sram_c_addr(col * kRows, sw);
                if (!core_host_read(address, data)) {
                    return error_code::reset_aborted;
                }
                for (std::uint32_t lane = 0; lane < 4; ++lane) {
                    const std::uint32_t row = sw * 4 + lane;
                    const std::int32_t value =
                        static_cast<std::int32_t>(std::llround(data[lane]));
                    const std::size_t offset =
                        (static_cast<std::size_t>(row) * kCols + col) *
                        sizeof(value);
                    std::memcpy(output.data() + offset, &value, sizeof(value));
                }
            }
        }

        if (!dma_write(regs.dst_addr, output) || !owner.reset_n.read()) {
            return owner.reset_n.read() ? error_code::dma_write
                                        : error_code::reset_aborted;
        }
        regs.bytes_written = static_cast<std::uint32_t>(output.size());
        return error_code::none;
    }
};

npu_tlm_v4_model::npu_tlm_v4_model(sc_core::sc_module_name name,
                                   sc_core::sc_time core_clock_period,
                                   sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , target_socket("target_socket")
    , master_socket("master_socket")
    , reset_n("reset_n")
    , irq_out("irq_out")
    , impl_(std::make_unique<impl>("impl", *this, core_clock_period,
                                   access_latency))
{
    target_socket.register_b_transport(this, &npu_tlm_v4_model::b_transport);
    target_socket.register_transport_dbg(this,
                                         &npu_tlm_v4_model::transport_dbg);
    SC_METHOD(drive_irq);
    sensitive << impl_->irq_level;
    SC_THREAD(worker_thread);
}

npu_tlm_v4_model::~npu_tlm_v4_model() = default;

void npu_tlm_v4_model::b_transport(tlm::tlm_generic_payload& trans,
                                   sc_core::sc_time& delay)
{
    delay += impl_->access_latency;

    const std::uint64_t address = trans.get_address();
    unsigned char* data = trans.get_data_ptr();
    if (trans.get_byte_enable_ptr() != nullptr) {
        trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
        return;
    }
    if (data == nullptr) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }
    if (trans.get_data_length() != sizeof(std::uint32_t) ||
        (address & 0x3u) != 0u || address >= MMIO_SIZE) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }

    const std::uint32_t offset = static_cast<std::uint32_t>(address);
    const bool compatibility_access =
        offset >= WRAPPER_BASE &&
        offset < WRAPPER_BASE + WRAPPER_WINDOW_SIZE;
    const std::uint32_t compatibility_offset =
        compatibility_access ? offset - WRAPPER_BASE : 0u;

    std::uint32_t value = 0;
    tlm::tlm_response_status status = tlm::TLM_COMMAND_ERROR_RESPONSE;
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        status = compatibility_access
                     ? impl_->read_reg(compatibility_offset, value)
                     : impl_->submit_native(
                           impl::native_command::read, offset, value);
        if (status == tlm::TLM_OK_RESPONSE) {
            std::memcpy(data, &value, sizeof(value));
        }
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(&value, data, sizeof(value));
        status = compatibility_access
                     ? impl_->write_reg(compatibility_offset, value)
                     : impl_->submit_native(
                           impl::native_command::write, offset, value);
    }
    trans.set_response_status(status);
}

unsigned int
npu_tlm_v4_model::transport_dbg(tlm::tlm_generic_payload& trans)
{
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    b_transport(trans, delay);
    return trans.get_response_status() == tlm::TLM_OK_RESPONSE
               ? sizeof(std::uint32_t)
               : 0u;
}

void npu_tlm_v4_model::drive_irq()
{
    irq_out.write(impl_->irq_level.read());
}

void npu_tlm_v4_model::worker_thread()
{
    impl_->core_rst_n.write(false);
    impl_->drive_idle_host();
    impl_->buffer_select.write(sc_dt::sc_bv<3>("000"));

    for (;;) {
        while (!reset_n.read()) {
            impl_->set_clock_running(false);
            wait(reset_n.posedge_event());
        }
        if (!impl_->wait_clock(2)) {
            continue;
        }
        impl_->core_rst_n.write(true);
        if (!impl_->wait_clock(2)) {
            impl_->core_rst_n.write(false);
            continue;
        }

        while (reset_n.read()) {
            if (!impl_->job_pending && !impl_->soft_reset_pending &&
                !impl_->native_pending) {
                impl_->set_clock_running(false);
                wait(impl_->work_event | reset_n.negedge_event());
                if (!reset_n.read()) {
                    break;
                }
            }

            if (impl_->soft_reset_pending) {
                impl_->soft_reset_pending = false;
                if (!impl_->pulse_core_soft_reset()) {
                    break;
                }
                impl_->reset_registers();
                continue;
            }

            if (impl_->job_pending) {
                impl_->job_pending = false;
                const error_code result = impl_->execute_job();
                if (!reset_n.read()) {
                    break;
                }
                if (result == error_code::none) {
                    impl_->finish_success();
                } else {
                    impl_->finish_error(result);
                }
                continue;
            }

            if (impl_->native_pending) {
                impl_->service_native_request();
            }
        }

        impl_->abort_native_request();
        impl_->core_rst_n.write(false);
        impl_->drive_idle_host();
        impl_->reset_registers();
        impl_->set_clock_running(false);
    }
}

} // namespace cdc::components
