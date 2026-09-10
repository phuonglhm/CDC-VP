// SPDX-License-Identifier: Apache-2.0

#include "npu_tlm.h"

#include "npu_tlm_regmap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
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

using namespace npu_tlm_reg;

constexpr std::uint64_t kRamBase = 0x8000'0000ULL;
constexpr std::uint64_t kRamSize = 0x1000'0000ULL;
constexpr std::uint32_t kArrayRows = sauria::Y;
constexpr std::uint32_t kArrayCols = sauria::X;
constexpr std::uint32_t kSoftwareRows = kArrayRows;
constexpr std::uint32_t kSoftwareCols = kArrayCols;
constexpr std::uint32_t kSoftwareRowSubwords = kSoftwareRows / 4;
constexpr std::uint32_t kSoftwareMaxK = 1024 - kSoftwareCols;
static_assert(kSoftwareRows == 64 && kSoftwareCols == 64);
static_assert((kSoftwareRows % 4) == 0);
constexpr std::uint32_t kSramASubwords = kArrayRows / 4;
constexpr std::uint32_t kSramBSubwords = kArrayCols / 4;
constexpr std::uint32_t kSramCSubwords = kArrayRows / 4;

constexpr std::uint32_t log2_power_of_two(std::uint32_t value)
{
    std::uint32_t shift = 0;
    while ((std::uint32_t{1} << shift) < value) {
        ++shift;
    }
    return shift;
}

static_assert((kSramASubwords & (kSramASubwords - 1)) == 0);
static_assert((kSramBSubwords & (kSramBSubwords - 1)) == 0);
static_assert((kSramCSubwords & (kSramCSubwords - 1)) == 0);
constexpr std::uint32_t kSramAShift = log2_power_of_two(kSramASubwords);
constexpr std::uint32_t kSramBShift = log2_power_of_two(kSramBSubwords);
constexpr std::uint32_t kSramCShift = log2_power_of_two(kSramCSubwords);
constexpr std::uint32_t kCoreTimeoutCycles = 50'000;
constexpr std::uint32_t kStatusPollCycles = 8;
constexpr std::uint32_t kInt8Bytes = 1;
constexpr std::uint32_t kInt32Bytes = 4;
constexpr std::uint32_t kConfigRegisterBytes = 42 * sizeof(std::uint32_t);
constexpr std::uint32_t kRichModelBase = 0x4000'0000;
constexpr std::uint32_t kRichElementBytes = sizeof(sauria::act_t);
constexpr std::uint32_t kRichPsumBytes = sizeof(sauria::psum_t);

struct vp_alias_range {
    std::uint32_t alias_base;
    std::uint32_t size;
    std::uint32_t model_base;
};

constexpr std::array<vp_alias_range, 14> kTableAliases{{
    {OBP_A_LUT_BASE, OBP_A_LUT_SIZE, 0x0014'0000},
    {OBP_A_BIAS_BASE, OBP_A_BIAS_SIZE, 0x0015'0000},
    {OBP_A_SCALE_BASE, OBP_A_SCALE_SIZE, 0x0018'0000},
    {OBP_A_SHIFT_BASE, OBP_A_SHIFT_SIZE, 0x0019'0000},
    {OBP_B_LUT_BASE, OBP_B_LUT_SIZE, 0x0016'0000},
    {OBP_B_BIAS_BASE, OBP_B_BIAS_SIZE, 0x0017'0000},
    {OBP_B_SCALE_BASE, OBP_B_SCALE_SIZE, 0x001A'0000},
    {OBP_B_SHIFT_BASE, OBP_B_SHIFT_SIZE, 0x001B'0000},
    {RCE_A_EXP_BASE, RCE_A_EXP_SIZE, 0x0020'0000},
    {RCE_A_RECIP_BASE, RCE_A_RECIP_SIZE, 0x0021'0000},
    {RCE_A_RSQRT_BASE, RCE_A_RSQRT_SIZE, 0x0022'0000},
    {RCE_B_EXP_BASE, RCE_B_EXP_SIZE, 0x0023'0000},
    {RCE_B_RECIP_BASE, RCE_B_RECIP_SIZE, 0x0024'0000},
    {RCE_B_RSQRT_BASE, RCE_B_RSQRT_SIZE, 0x0025'0000},
}};

using core_type =
    sauria::NpuTop<kArrayCols, kArrayRows,
                   sauria::act_t, sauria::wei_t, sauria::psum_t,
                   16, kArrayCols + kArrayRows, 1>;

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
    return sauria::SRAMA_OFFSET | (phys_addr << kSramAShift) |
           (sub_word & (kSramASubwords - 1));
}

std::uint32_t make_sram_b_addr(std::uint32_t phys_addr,
                               std::uint32_t sub_word)
{
    return sauria::SRAMB_OFFSET | (phys_addr << kSramBShift) |
           (sub_word & (kSramBSubwords - 1));
}

std::uint32_t make_sram_c_addr(std::uint32_t phys_addr,
                               std::uint32_t sub_word)
{
    return sauria::SRAMC_OFFSET | (phys_addr << kSramCShift) |
           (sub_word & (kSramCSubwords - 1));
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

struct npu_tlm::impl : public sc_core::sc_module {
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
        std::uint32_t width = kSoftwareCols;
        std::uint32_t height = kSoftwareRows;
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

    struct rich_parameters {
        std::uint32_t in_addr = 0;
        std::uint32_t weight_addr = 0;
        std::uint32_t out_addr = 0;
        std::uint32_t bias_addr = 0;
        std::uint32_t m = kArrayRows;
        std::uint32_t k = kArrayCols;
        std::uint32_t n = kArrayCols;
        std::uint32_t stride = 1;
        std::uint32_t has_skip = 0;
        std::uint32_t skip_addr = 0;
        std::uint32_t q_gamma_a_addr = 0;
        std::uint32_t k_b_addr = 0;
        std::uint32_t v_beta_addr = 0;
        std::uint32_t seq_len = 0;
        std::uint32_t a_len = 0;
        std::uint32_t b_len = 0;
        std::uint32_t heads_dim_mode = 0;
        std::uint32_t head_dim_eps_scale_a = 0;
    };

    struct rich_job {
        std::uint32_t opcode = 0;
        std::uint32_t output_phys = 0;
        std::uint32_t output_offset = 0;
        std::uint32_t output_size = 0;
    };

    npu_tlm& owner;
    sc_core::sc_time access_latency;

    /* Gated core clock: a free-running clock at the configured core period
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
    std::vector<std::uint8_t> rich_dram;
    rich_parameters rich_params;
    std::deque<rich_job> rich_jobs_a;
    std::deque<rich_job> rich_jobs_b;

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
    bool software_run_active = false;
    bool native_run_active = false;
    bool rich_run_active = false;
    std::uint32_t native_run_cycles = 0;
    sc_core::sc_event native_done_event;

    impl(sc_core::sc_module_name name,
         npu_tlm& owner_ref,
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
        core.set_dram(&rich_dram);
        perf.X = kArrayCols;
        perf.Y = kArrayRows;

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

    std::size_t rich_queue_a_size() const
    {
        return core.decoder_inst ? core.decoder_inst->get_queue_a_size() : 0u;
    }

    std::size_t rich_queue_b_size() const
    {
        return core.decoder_inst ? core.decoder_inst->get_queue_b_size() : 0u;
    }

    std::uint32_t rich_state_a() const
    {
        return core.decoder_inst
                   ? static_cast<std::uint32_t>(core.decoder_inst->get_state_a())
                   : 0u;
    }

    std::uint32_t rich_state_b() const
    {
        return core.decoder_inst
                   ? static_cast<std::uint32_t>(core.decoder_inst->get_state_b())
                   : 0u;
    }

    bool rich_active() const
    {
        return !rich_jobs_a.empty() || !rich_jobs_b.empty() ||
               rich_queue_a_size() != 0u || rich_queue_b_size() != 0u ||
               rich_state_a() != 0u || rich_state_b() != 0u;
    }

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
        software_run_active = false;
        native_run_active = false;
        rich_run_active = false;
        native_run_cycles = 0;
        perf.reset();
        native_mvm_k = 0;
        native_total_contexts = 0;
        rich_params = rich_parameters{};
        rich_jobs_a.clear();
        rich_jobs_b.clear();
        rich_dram.clear();
        update_irq();
    }

    void finish_success()
    {
        software_run_active = false;
        regs.status = STATUS_DONE | STATUS_IDLE;
        regs.irq_status |= IRQ_DONE;
        regs.last_error = static_cast<std::uint32_t>(error_code::none);
        update_irq();
    }

    void finish_error(error_code code)
    {
        software_run_active = false;
        regs.status = STATUS_ERROR | STATUS_IDLE;
        regs.irq_status |= IRQ_ERROR;
        regs.last_error = static_cast<std::uint32_t>(code);
        update_irq();
    }

    tlm::tlm_response_status read_reg(std::uint32_t offset, std::uint32_t& value)
    {
        if (read_perf(offset, value)) {
            return tlm::TLM_OK_RESPONSE;
        }
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
        std::uint32_t perf_low_address = 0;
        if (is_perf_low(offset) ||
            perf_low_for_high(offset, perf_low_address)) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }

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
            if (rich_active() && (value & CTRL_START) != 0u) {
                regs.last_error = static_cast<std::uint32_t>(error_code::busy);
                return tlm::TLM_COMMAND_ERROR_RESPONSE;
            }
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
                    software_run_active = true;
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
        case PERF_M:                 return perf.M;
        case PERF_K:                 return perf.K;
        case PERF_N:                 return perf.N;
        case PERF_SA_CYCLES:         return perf.sa_cycles;
        case PERF_OBP_CYCLES:        return perf.obp_cycles;
        case PERF_PROCESSING_CYCLES: return perf.processing_cycles;
        case PERF_TRANSFER_CYCLES:   return perf.transfer_cycles;
        case PERF_MAC_ENGINE_CYCLES: return perf.mac_engine_cycles;
        case PERF_DMA_ENGINE_CYCLES: return perf.dma_engine_cycles;
        case PERF_ACTIVATION_ENGINE_CYCLES:
            return perf.activation_engine_cycles;
        case PERF_POOLING_ENGINE_CYCLES:
            return perf.pooling_engine_cycles;
        case PERF_REDUCTION_ENGINE_CYCLES:
            return perf.reduction_engine_cycles;
        case PERF_DDR_READ_BYTES:    return perf.ddr_read_bytes;
        case PERF_DDR_WRITE_BYTES:   return perf.ddr_write_bytes;
        case PERF_DMA_READ_CYCLES:   return perf.dma_read_cycles_raw;
        case PERF_DMA_WRITE_CYCLES:  return perf.dma_write_cycles_raw;
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
               address == PERF_M ||
               address == PERF_K ||
               address == PERF_N ||
               address == PERF_SA_CYCLES ||
               address == PERF_OBP_CYCLES ||
               address == PERF_PROCESSING_CYCLES ||
               address == PERF_TRANSFER_CYCLES ||
               address == PERF_MAC_ENGINE_CYCLES ||
               address == PERF_DMA_ENGINE_CYCLES ||
               address == PERF_ACTIVATION_ENGINE_CYCLES ||
               address == PERF_POOLING_ENGINE_CYCLES ||
               address == PERF_REDUCTION_ENGINE_CYCLES ||
               address == PERF_DDR_READ_BYTES ||
               address == PERF_DDR_WRITE_BYTES ||
               address == PERF_DMA_READ_CYCLES ||
               address == PERF_DMA_WRITE_CYCLES;
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
        case PERF_M_HI:
            low_address = PERF_M;
            return true;
        case PERF_K_HI:
            low_address = PERF_K;
            return true;
        case PERF_N_HI:
            low_address = PERF_N;
            return true;
        case PERF_SA_CYCLES_HI:
            low_address = PERF_SA_CYCLES;
            return true;
        case PERF_OBP_CYCLES_HI:
            low_address = PERF_OBP_CYCLES;
            return true;
        case PERF_PROCESSING_CYCLES_HI:
            low_address = PERF_PROCESSING_CYCLES;
            return true;
        case PERF_TRANSFER_CYCLES_HI:
            low_address = PERF_TRANSFER_CYCLES;
            return true;
        case PERF_MAC_ENGINE_CYCLES_HI:
            low_address = PERF_MAC_ENGINE_CYCLES;
            return true;
        case PERF_DMA_ENGINE_CYCLES_HI:
            low_address = PERF_DMA_ENGINE_CYCLES;
            return true;
        case PERF_ACTIVATION_ENGINE_CYCLES_HI:
            low_address = PERF_ACTIVATION_ENGINE_CYCLES;
            return true;
        case PERF_POOLING_ENGINE_CYCLES_HI:
            low_address = PERF_POOLING_ENGINE_CYCLES;
            return true;
        case PERF_REDUCTION_ENGINE_CYCLES_HI:
            low_address = PERF_REDUCTION_ENGINE_CYCLES;
            return true;
        case PERF_DDR_READ_BYTES_HI:
            low_address = PERF_DDR_READ_BYTES;
            return true;
        case PERF_DDR_WRITE_BYTES_HI:
            low_address = PERF_DDR_WRITE_BYTES;
            return true;
        case PERF_DMA_READ_CYCLES_HI:
            low_address = PERF_DMA_READ_CYCLES;
            return true;
        case PERF_DMA_WRITE_CYCLES_HI:
            low_address = PERF_DMA_WRITE_CYCLES;
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

    bool rich_address_value(std::uint32_t model_address,
                            std::uint32_t value,
                            std::uint32_t& forwarded)
    {
        std::uint32_t* field = nullptr;
        switch (model_address) {
        case 0x4000'0400u: field = &rich_params.in_addr; break;
        case 0x4000'0404u: field = &rich_params.weight_addr; break;
        case 0x4000'0408u: field = &rich_params.out_addr; break;
        case 0x4000'040Cu: field = &rich_params.bias_addr; break;
        case 0x4000'0434u: field = &rich_params.skip_addr; break;
        case 0x4000'0444u: field = &rich_params.q_gamma_a_addr; break;
        case 0x4000'0448u: field = &rich_params.k_b_addr; break;
        case 0x4000'044Cu: field = &rich_params.v_beta_addr; break;
        default: return true;
        }

        *field = value;
        if (value == 0u) {
            forwarded = 0u;
            return true;
        }
        if (value < kRamBase || value >= kRamBase + kRamSize) {
            return false;
        }
        forwarded = value - static_cast<std::uint32_t>(kRamBase);
        return true;
    }

    void update_rich_parameter(std::uint32_t model_address,
                               std::uint32_t value)
    {
        switch (model_address) {
        case 0x4000'0410u: rich_params.m = value; break;
        case 0x4000'0414u: rich_params.k = value; break;
        case 0x4000'0418u: rich_params.n = value; break;
        case 0x4000'0424u: rich_params.stride = value; break;
        case 0x4000'0430u: rich_params.has_skip = value; break;
        case 0x4000'0450u: rich_params.seq_len = value; break;
        case 0x4000'0454u: rich_params.heads_dim_mode = value; break;
        case 0x4000'0458u: rich_params.head_dim_eps_scale_a = value; break;
        case 0x4000'0464u: rich_params.a_len = value; break;
        case 0x4000'0468u: rich_params.b_len = value; break;
        default: break;
        }
    }

    std::uint32_t rich_num_heads() const
    {
        const std::uint32_t heads =
            (rich_params.heads_dim_mode & 0xFFFF'0000u) != 0u
                ? rich_params.heads_dim_mode & 0xFFFFu
                : rich_params.heads_dim_mode;
        return heads == 0u ? 1u : heads;
    }

    std::uint32_t rich_dimension() const
    {
        return (rich_params.heads_dim_mode & 0xFFFF'0000u) != 0u
                   ? (rich_params.heads_dim_mode >> 16) & 0xFFu
                   : rich_params.heads_dim_mode;
    }

    std::uint32_t rich_elementwise_mode() const
    {
        return (rich_params.heads_dim_mode & 0xFFFF'0000u) != 0u
                   ? (rich_params.heads_dim_mode >> 24) & 0xFFu
                   : rich_params.heads_dim_mode;
    }

    bool checked_bytes(std::uint64_t elements, std::uint32_t element_bytes,
                       std::uint32_t& bytes) const
    {
        if (elements == 0u || element_bytes == 0u ||
            elements > std::numeric_limits<std::uint32_t>::max() /
                           element_bytes) {
            return false;
        }
        const std::uint64_t total = elements * element_bytes;
        bytes = static_cast<std::uint32_t>(total);
        return true;
    }

    bool stage_rich_range(std::uint32_t physical, std::uint32_t size,
                          error_code& error)
    {
        if (!physical_ram_range(physical, size)) {
            error = error_code::invalid_address;
            return false;
        }
        const std::uint32_t offset =
            physical - static_cast<std::uint32_t>(kRamBase);
        const std::uint64_t end = static_cast<std::uint64_t>(offset) + size;
        if (end > rich_dram.size()) {
            rich_dram.resize(static_cast<std::size_t>(end), 0u);
        }

        std::vector<std::uint8_t> data;
        if (!dma_read(physical, data, size)) {
            error = error_code::dma_read;
            return false;
        }
        std::copy(data.begin(), data.end(), rich_dram.begin() + offset);
        return true;
    }

    bool prepare_rich_job(std::uint32_t opcode, rich_job& job,
                          error_code& error)
    {
        error = error_code::none;
        job.opcode = opcode;
        if (opcode == RICH_OPCODE_SET_NSPLIT) {
            return true;
        }

        std::uint32_t input_size = 0;
        std::uint32_t weight_size = 0;
        std::uint32_t output_size = 0;
        if (opcode == RICH_OPCODE_GEMM_FUSED) {
            if (!checked_bytes(static_cast<std::uint64_t>(rich_params.m) *
                                   rich_params.k,
                               kRichElementBytes, input_size) ||
                !checked_bytes(static_cast<std::uint64_t>(rich_params.k) *
                                   rich_params.n,
                               kRichElementBytes, weight_size) ||
                !checked_bytes(static_cast<std::uint64_t>(rich_params.m) *
                                   rich_params.n,
                               kRichElementBytes, output_size)) {
                error = error_code::invalid_dimensions;
                return false;
            }
            if (!stage_rich_range(rich_params.in_addr, input_size, error) ||
                !stage_rich_range(rich_params.weight_addr, weight_size, error)) {
                return false;
            }
            if (rich_params.bias_addr != 0u) {
                std::uint32_t bias_size = 0;
                if (!checked_bytes(rich_params.n, kRichPsumBytes, bias_size) ||
                    !stage_rich_range(rich_params.bias_addr, bias_size, error)) {
                    return false;
                }
            }
            if (rich_params.has_skip != 0u &&
                !stage_rich_range(rich_params.skip_addr, output_size, error)) {
                return false;
            }
        } else if (opcode == RICH_OPCODE_FUSED_ATTN) {
            const std::uint32_t num_heads = rich_num_heads();
            if (!checked_bytes(
                    static_cast<std::uint64_t>(rich_params.seq_len) *
                        num_heads *
                        rich_params.head_dim_eps_scale_a,
                    kRichElementBytes, input_size)) {
                error = error_code::invalid_dimensions;
                return false;
            }
            output_size = input_size;
            if (!stage_rich_range(rich_params.q_gamma_a_addr, input_size, error) ||
                !stage_rich_range(rich_params.k_b_addr, input_size, error) ||
                !stage_rich_range(rich_params.v_beta_addr, input_size, error)) {
                return false;
            }
        } else if (opcode == RICH_OPCODE_LAYERNORM) {
            const std::uint32_t dimension = rich_dimension();
            if (!checked_bytes(
                    static_cast<std::uint64_t>(rich_params.seq_len) *
                        dimension,
                    kRichElementBytes, input_size) ||
                !checked_bytes(dimension, kRichElementBytes,
                               weight_size)) {
                error = error_code::invalid_dimensions;
                return false;
            }
            output_size = input_size;
            if (!stage_rich_range(rich_params.in_addr, input_size, error) ||
                !stage_rich_range(rich_params.q_gamma_a_addr, weight_size, error) ||
                !stage_rich_range(rich_params.v_beta_addr, weight_size, error)) {
                return false;
            }
        } else if (opcode == RICH_OPCODE_ELEM_WISE) {
            const std::uint32_t mode = rich_elementwise_mode();
            const std::uint32_t a_len =
                rich_params.a_len != 0u ? rich_params.a_len
                                        : rich_params.seq_len;
            const std::uint32_t dimension = rich_dimension();
            const std::uint32_t b_len =
                rich_params.b_len != 0u
                    ? rich_params.b_len
                    : (dimension != 0u && dimension < rich_params.seq_len
                           ? dimension
                           : rich_params.seq_len);
            std::uint32_t operand_b_size = 0;
            if (!checked_bytes(a_len, kRichElementBytes, input_size) ||
                (mode != 1u &&
                 !checked_bytes(b_len, kRichElementBytes, operand_b_size))) {
                error = error_code::invalid_dimensions;
                return false;
            }
            if (!checked_bytes(rich_params.seq_len, kRichElementBytes,
                               output_size)) {
                error = error_code::invalid_dimensions;
                return false;
            }
            if (mode == 1u) {
                const std::uint32_t stride =
                    rich_params.stride == 0u ? 2u : rich_params.stride;
                if (!checked_bytes(rich_params.seq_len / stride,
                                   kRichElementBytes, output_size)) {
                    error = error_code::invalid_dimensions;
                    return false;
                }
            }
            if (!stage_rich_range(rich_params.q_gamma_a_addr, input_size, error) ||
                (mode != 1u &&
                 !stage_rich_range(rich_params.k_b_addr, operand_b_size, error))) {
                return false;
            }
        } else {
            error = error_code::invalid_operation;
            return false;
        }

        if (!physical_ram_range(rich_params.out_addr, output_size)) {
            error = error_code::invalid_address;
            return false;
        }
        job.output_phys = rich_params.out_addr;
        job.output_offset =
            rich_params.out_addr - static_cast<std::uint32_t>(kRamBase);
        job.output_size = output_size;
        const std::uint64_t output_end =
            static_cast<std::uint64_t>(job.output_offset) + output_size;
        if (output_end > rich_dram.size()) {
            rich_dram.resize(static_cast<std::size_t>(output_end), 0u);
        }
        return true;
    }

    tlm::tlm_response_status native_write(std::uint32_t address,
                                          std::uint32_t value)
    {
        if (!owner.reset_n.read() || !core_rst_n.read() ||
            software_run_active) {
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
            local == 0u && (value & 0xFFu) != 0u;
        const bool instruction_start =
            model_address == 0x4000'0310u ||
            model_address == 0x4000'0314u;
        if ((native_start && (native_run_active || rich_run_active)) ||
            (instruction_start && native_run_active)) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }
        if (native_start || (instruction_start && !rich_run_active)) {
            perf.reset();
        }

        std::uint32_t forwarded_value = value;
        update_rich_parameter(model_address, value);
        if (!rich_address_value(model_address, value, forwarded_value)) {
            return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }

        rich_job pending_job;
        error_code rich_error = error_code::none;
        if (instruction_start &&
            !prepare_rich_job(value & 0xFFu, pending_job, rich_error)) {
            return rich_error == error_code::invalid_address
                       ? tlm::TLM_ADDRESS_ERROR_RESPONSE
                       : tlm::TLM_GENERIC_ERROR_RESPONSE;
        }

        // Register 0x458 is intentionally shared by integer head_dim/eps_shift
        // and floating-point scale_a. Re-issue its integer interpretation for
        // ATTN/LAYERNORM before the instruction snapshot is pushed.
        if (instruction_start &&
            (pending_job.opcode == RICH_OPCODE_FUSED_ATTN ||
             pending_job.opcode == RICH_OPCODE_LAYERNORM)) {
            sauria::host_data_t integer_data;
            sauria::host_mask_t integer_mask;
            integer_data.data.fill(0.0);
            integer_mask.data.fill(false);
            integer_data[0] = rich_params.head_dim_eps_scale_a;
            integer_mask[0] = true;
            if (!core_host_write(0x4000'0458u, integer_data, integer_mask)) {
                return tlm::TLM_COMMAND_ERROR_RESPONSE;
            }
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
                        (forwarded_value >> (lane * 8)) & 0xFFu));
                mask[lane] = true;
            }
        } else if (is_packed_byte_address(model_address)) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) {
                data[lane] =
                    static_cast<double>((forwarded_value >> (lane * 8)) & 0xFFu);
                mask[lane] = true;
            }
        } else if (is_packed_half_address(model_address)) {
            const std::uint32_t first_lane =
                ((model_address & 0xFFFFu) >> 1) & 3u;
            data[first_lane] = static_cast<double>(forwarded_value & 0xFFFFu);
            mask[first_lane] = true;
            if (first_lane + 1u < 4u) {
                data[first_lane + 1u] =
                    static_cast<double>((forwarded_value >> 16) & 0xFFFFu);
                mask[first_lane + 1u] = true;
            }
        } else if (is_rich_float_address(model_address)) {
            data[0] = static_cast<double>(bits_to_float(forwarded_value));
            mask[0] = true;
        } else if (!is_alias && region == sauria::CFG_REGS_OFFSET &&
                   local == 0u) {
            data[0] = static_cast<double>(forwarded_value & 0xFFu);
            mask[0] = true;
            data[2] = static_cast<double>((forwarded_value >> 16) & 0xFFu);
            mask[2] = ((forwarded_value >> 16) & 0xFFu) != 0u;
        } else {
            data[0] = static_cast<double>(forwarded_value);
            mask[0] = true;
        }

        update_native_sideband(model_address, value);
        if (native_start) {
            buffer_select.write(sc_dt::sc_bv<3>("111"));
            total_contexts.write(native_total_contexts);
            mvm_k.write(native_mvm_k);
        }
        if (!core_host_write(model_address, data, mask)) {
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }
        if (instruction_start) {
            if (model_address == 0x4000'0310u) {
                rich_jobs_a.push_back(pending_job);
            } else {
                rich_jobs_b.push_back(pending_job);
            }
            rich_run_active = true;
            regs.status = STATUS_BUSY;
            regs.irq_status = 0;
            regs.last_error = static_cast<std::uint32_t>(error_code::none);
            update_irq();
        }
        if (native_start) {
            native_run_active = true;
            native_run_cycles = 0;
            regs.status = STATUS_BUSY;
            regs.irq_status = 0;
            regs.last_error = static_cast<std::uint32_t>(error_code::none);
            regs.cycle_count = 0;
            update_irq();
        }
        return tlm::TLM_OK_RESPONSE;
    }

    tlm::tlm_response_status native_read(std::uint32_t address,
                                         std::uint32_t& value)
    {
        if (read_perf(address, value)) {
            return tlm::TLM_OK_RESPONSE;
        }
        if (!owner.reset_n.read() || !core_rst_n.read() ||
            software_run_active) {
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
        } else if (model_address == sauria::CFG_REGS_OFFSET) {
            value = static_cast<std::uint32_t>(std::llround(data[0])) & 0xFFu;
            value |= (static_cast<std::uint32_t>(std::llround(data[2])) &
                      0xFFu)
                     << 16;
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
        if (!owner.reset_n.read() || !core_rst_n.read() ||
            software_run_active ||
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
         * PROFILE_V1_SAURIA uses the SAURIA address generators and
         * PSM schedule. The
         * previous port selected V4_LINEAR but only programmed three control
         * words; that leaves the unified feeders waiting forever on their
         * first synchronized vector.  Program the complete native single-tile
         * schedule used by the reference testbench.
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
            // Activation feeder: software counters plus native address generation.
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

    bool complete_rich_job(const rich_job& job)
    {
        if (job.output_size == 0u) {
            return true;
        }
        const std::uint64_t end =
            static_cast<std::uint64_t>(job.output_offset) + job.output_size;
        if (end > rich_dram.size()) {
            return false;
        }
        std::vector<std::uint8_t> output(
            rich_dram.begin() + job.output_offset,
            rich_dram.begin() + static_cast<std::size_t>(end));
        if (!dma_write(job.output_phys, output)) {
            return false;
        }
        return true;
    }

    bool service_rich_completions()
    {
        const std::size_t queued_a = rich_queue_a_size();
        while (rich_jobs_a.size() > queued_a) {
            const rich_job job = rich_jobs_a.front();
            rich_jobs_a.pop_front();
            if (!complete_rich_job(job)) {
                return false;
            }
        }

        const std::size_t queued_b = rich_queue_b_size();
        while (rich_jobs_b.size() > queued_b) {
            const rich_job job = rich_jobs_b.front();
            rich_jobs_b.pop_front();
            if (!complete_rich_job(job)) {
                return false;
            }
        }
        return true;
    }

    void service_external_runs()
    {
        if (native_run_active) {
            ++native_run_cycles;
            regs.cycle_count = native_run_cycles;
            if (core_deadlock.read()) {
                native_run_active = false;
                finish_error(error_code::core_deadlock);
            } else if (core_done.read()) {
                native_run_active = false;
                finish_success();
            } else if (native_run_cycles >= kCoreTimeoutCycles) {
                native_run_active = false;
                finish_error(error_code::core_timeout);
            }
        }

        if (rich_run_active) {
            if (!service_rich_completions()) {
                rich_run_active = false;
                rich_jobs_a.clear();
                rich_jobs_b.clear();
                finish_error(error_code::dma_write);
            } else if (!rich_active()) {
                rich_run_active = false;
                finish_success();
            }
        }
    }

    error_code validate_job(std::uint32_t& source_span) const
    {
        if (regs.width != kSoftwareCols || regs.height != kSoftwareRows ||
            regs.k_dimension == 0u || regs.k_dimension > kSoftwareMaxK) {
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
            static_cast<std::uint64_t>(stride) * (kSoftwareRows - 1) +
            regs.k_dimension;
        if (span64 > std::numeric_limits<std::uint32_t>::max()) {
            return error_code::invalid_size;
        }
        source_span = static_cast<std::uint32_t>(span64);

        const std::uint32_t weights_need = regs.k_dimension * kSoftwareCols;
        const std::uint32_t dst_need =
            kSoftwareRows * kSoftwareCols * sizeof(std::int32_t);
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
        const std::uint32_t weights_need = regs.k_dimension * kSoftwareCols;

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
            sauria::sauria_find_target("int8_64x64");
        if (target == nullptr) {
            return error_code::invalid_format;
        }
        const sauria::SauriaLayerDesc desc =
            make_gemm_as_conv1x1_desc(kSoftwareRows, kSoftwareCols,
                                      regs.k_dimension,
                                      kSoftwareRows, kSoftwareCols);

        /*
         * Present the firmware's row-major GEMM operands as a 1x1 convolution:
         *   A [Cin][1][W]       = activation[row][k]
         *   B [Cout][Cin][1][1] = weight[k][col]
         * The V4.4 driver then provides the exact native SRAM byte layout.
         */
        std::vector<double> native_a(
            static_cast<std::size_t>(regs.k_dimension) * kSoftwareRows);
        std::vector<double> native_b(
            static_cast<std::size_t>(kSoftwareCols) * regs.k_dimension);
        std::vector<double> native_c(
            static_cast<std::size_t>(kSoftwareCols) * kSoftwareRows, 0.0);
        for (std::uint32_t k = 0; k < regs.k_dimension; ++k) {
            for (std::uint32_t row = 0; row < kSoftwareRows; ++row) {
                native_a[static_cast<std::size_t>(k) * kSoftwareRows + row] =
                    static_cast<std::int8_t>(
                        activations[static_cast<std::size_t>(row) * stride +
                                    k]);
            }
        }
        for (std::uint32_t col = 0; col < kSoftwareCols; ++col) {
            for (std::uint32_t k = 0; k < regs.k_dimension; ++k) {
                native_b[static_cast<std::size_t>(col) *
                             regs.k_dimension +
                         k] =
                    static_cast<std::int8_t>(
                        weights[static_cast<std::size_t>(k) * kSoftwareCols + col]);
            }
        }

        const sauria::SauriaRunInputs native =
            sauria::sauria_prepare(
                *target, desc,
                native_a.data(), regs.k_dimension, 1, kSoftwareRows,
                native_b.data(), kSoftwareCols, regs.k_dimension, 1, 1,
                native_c.data(), kSoftwareCols, 1, kSoftwareRows);

        // One software-facing job is one complete 64x64 output tile.
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
                            sram_offset == sauria::SRAMA_OFFSET
                                ? make_sram_a_addr(phys, sw)
                                : make_sram_b_addr(phys, sw);
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
        for (std::uint32_t col = 0; col < kSoftwareCols; ++col) {
            for (std::uint32_t sw = 0; sw < kSoftwareRowSubwords; ++sw) {
                sauria::host_data_t data;
                data.data.fill(0.0f);
                const std::uint32_t address =
                    make_sram_c_addr(col * kSoftwareRows, sw);
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
            kSoftwareRows * kSoftwareCols * sizeof(std::int32_t), 0);
        // Native C layout is [output channel][spatial row].
        for (std::uint32_t col = 0; col < kSoftwareCols; ++col) {
            for (std::uint32_t sw = 0; sw < kSoftwareRowSubwords; ++sw) {
                sauria::host_data_t data;
                const std::uint32_t address =
                    make_sram_c_addr(col * kSoftwareRows, sw);
                if (!core_host_read(address, data)) {
                    return error_code::reset_aborted;
                }
                for (std::uint32_t lane = 0; lane < 4; ++lane) {
                    const std::uint32_t row = sw * 4 + lane;
                    const std::int32_t value =
                        static_cast<std::int32_t>(std::llround(data[lane]));
                    const std::size_t offset =
                        (static_cast<std::size_t>(row) * kSoftwareCols + col) *
                        sizeof(value);
                    std::memcpy(output.data() + offset, &value, sizeof(value));
                }
            }
        }

        /*
         * Preserve the software-facing exact-output contract. The cycle model
         * still runs above and supplies completion timing and performance
         * counters, while software receives the bit-exact INT32 accumulators
         * expected by the CDC 64x64 GEMM ABI.
         */
        for (std::uint32_t row = 0; row < kSoftwareRows; ++row) {
            for (std::uint32_t col = 0; col < kSoftwareCols; ++col) {
                std::int64_t accumulator = 0;
                for (std::uint32_t k = 0; k < regs.k_dimension; ++k) {
                    const auto activation = static_cast<std::int8_t>(
                        activations[static_cast<std::size_t>(row) * stride +
                                    k]);
                    const auto weight = static_cast<std::int8_t>(
                        weights[static_cast<std::size_t>(k) * kSoftwareCols + col]);
                    accumulator +=
                        static_cast<std::int32_t>(activation) *
                        static_cast<std::int32_t>(weight);
                }

                const std::int32_t value = clamp_i64(accumulator);
                const std::size_t offset =
                    (static_cast<std::size_t>(row) * kSoftwareCols + col) *
                    sizeof(value);
                std::memcpy(output.data() + offset, &value, sizeof(value));
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

npu_tlm::npu_tlm(sc_core::sc_module_name name,
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
    target_socket.register_b_transport(this, &npu_tlm::b_transport);
    target_socket.register_transport_dbg(this,
                                         &npu_tlm::transport_dbg);
    SC_METHOD(drive_irq);
    sensitive << impl_->irq_level;
    SC_THREAD(worker_thread);
}

npu_tlm::~npu_tlm() = default;

void npu_tlm::b_transport(tlm::tlm_generic_payload& trans,
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
    const bool software_access =
        offset >= WRAPPER_BASE &&
        offset < WRAPPER_BASE + WRAPPER_WINDOW_SIZE;
    const std::uint32_t software_offset =
        software_access ? offset - WRAPPER_BASE : 0u;

    std::uint32_t value = 0;
    tlm::tlm_response_status status = tlm::TLM_COMMAND_ERROR_RESPONSE;
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        status = software_access
                     ? impl_->read_reg(software_offset, value)
                     : impl_->submit_native(
                           impl::native_command::read, offset, value);
        if (status == tlm::TLM_OK_RESPONSE) {
            std::memcpy(data, &value, sizeof(value));
        }
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(&value, data, sizeof(value));
        status = software_access
                     ? impl_->write_reg(software_offset, value)
                     : impl_->submit_native(
                           impl::native_command::write, offset, value);
    }
    trans.set_response_status(status);
}

unsigned int
npu_tlm::transport_dbg(tlm::tlm_generic_payload& trans)
{
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    b_transport(trans, delay);
    return trans.get_response_status() == tlm::TLM_OK_RESPONSE
               ? sizeof(std::uint32_t)
               : 0u;
}

void npu_tlm::drive_irq()
{
    irq_out.write(impl_->irq_level.read());
}

void npu_tlm::worker_thread()
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
            impl_->service_external_runs();
            if (!impl_->job_pending && !impl_->soft_reset_pending &&
                !impl_->native_pending && !impl_->native_run_active &&
                !impl_->rich_run_active) {
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
                continue;
            }

            if (impl_->native_run_active || impl_->rich_run_active) {
                if (!impl_->wait_clock()) {
                    break;
                }
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
