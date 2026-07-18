// SPDX-License-Identifier: Apache-2.0

#include "npu_tlm_v4_model.h"

#include "npu_tlm_v4_regmap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include <npu_top.h>

namespace cdc::components {
namespace {

using namespace npu_v4_reg;

constexpr std::uint64_t kRamBase = 0x8000'0000ULL;
constexpr std::uint64_t kRamSize = 0x1000'0000ULL;
constexpr std::uint32_t kRows = 32;
constexpr std::uint32_t kCols = 32;
constexpr std::uint32_t kMaxK = 1024 - kCols;
constexpr std::uint32_t kCoreTimeoutCycles = 50'000;

using core_type =
    sauria::NpuTop<32, 32, std::int8_t, std::int8_t, std::int32_t,
                   1024, 1024, 2048, 16, 64, 1>;

sauria::PeConfig exact_int8_config()
{
    sauria::PeConfig cfg;
    cfg.arithmetic_type = 1;
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

} // namespace

struct npu_tlm_v4_model::impl : public sc_core::sc_module {
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
        std::uint32_t cycle_count = 0;
        std::uint32_t bytes_read = 0;
        std::uint32_t bytes_written = 0;
        std::uint32_t last_error =
            static_cast<std::uint32_t>(error_code::none);
    };

    npu_tlm_v4_model& owner;
    sc_core::sc_time access_latency;

    sc_core::sc_clock core_clk;
    core_type core;

    sc_core::sc_signal<bool> core_rst_n{"core_rst_n"};
    sc_core::sc_signal<bool> core_soft_reset{"core_soft_reset"};
    sc_core::sc_signal<bool> core_start{"core_start"};
    sc_core::sc_signal<bool> core_done{"core_done"};
    sc_core::sc_signal<bool> core_deadlock{"core_deadlock"};
    sc_core::sc_signal<std::uint32_t> host_addr{"host_addr"};
    sc_core::sc_signal<bool> host_wren{"host_wren"};
    sc_core::sc_signal<bool> host_rden{"host_rden"};
    sc_core::sc_signal<sauria::host_data_t> host_wdata{"host_wdata"};
    sc_core::sc_signal<sauria::host_mask_t> host_wmask{"host_wmask"};
    sc_core::sc_signal<sauria::host_data_t> host_rdata{"host_rdata"};
    sc_core::sc_signal<float> threshold{"threshold"};
    sc_core::sc_signal<sc_dt::sc_bv<3>> buffer_select{"buffer_select"};
    sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS> irq_level{"irq_level"};

    register_file regs;
    sc_core::sc_event work_event;
    bool job_pending = false;
    bool soft_reset_pending = false;

    impl(sc_core::sc_module_name name,
         npu_tlm_v4_model& owner_ref,
         sc_core::sc_time clock_period,
         sc_core::sc_time mmio_latency)
        : sc_core::sc_module(name)
        , owner(owner_ref)
        , access_latency(mmio_latency)
        , core_clk("core_clk", clock_period)
        , core("sauria_core", exact_int8_config())
    {
        core.i_clk(core_clk);
        core.i_rstn(core_rst_n);
        core.i_soft_reset(core_soft_reset);
        core.i_start(core_start);
        core.o_done(core_done);
        core.o_deadlock(core_deadlock);
        core.i_host_addr(host_addr);
        core.i_host_wren(host_wren);
        core.i_host_rden(host_rden);
        core.i_host_wdata(host_wdata);
        core.i_host_wmask(host_wmask);
        core.o_host_rdata(host_rdata);
        core.i_threshold(threshold);
        core.i_select(buffer_select);

        core_rst_n.write(false);
        core_soft_reset.write(false);
        core_start.write(false);
        host_addr.write(0);
        host_wren.write(false);
        host_rden.write(false);
        host_wdata.write(sauria::host_data_t());
        host_wmask.write(sauria::host_mask_t());
        threshold.write(0.0f);
        buffer_select.write(sc_dt::sc_bv<3>("000"));
    }

    bool busy() const { return (regs.status & STATUS_BUSY) != 0u; }

    void update_irq()
    {
        const bool level =
            (regs.ctrl & CTRL_IRQ_EN) != 0u &&
            (regs.irq_enable & regs.irq_status) != 0u;
        irq_level.write(level);
    }

    void reset_registers()
    {
        regs = register_file{};
        job_pending = false;
        soft_reset_pending = false;
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
        case CYCLE_COUNT:
        case BYTES_READ:
        case BYTES_WRITTEN:
        case LAST_ERROR:
        case CORE_ID:
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        default:
            return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }
        return tlm::TLM_OK_RESPONSE;
    }

    bool wait_clock(unsigned count = 1)
    {
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

    bool write_scalar(std::uint32_t address, std::uint32_t value)
    {
        sauria::host_data_t data;
        sauria::host_mask_t mask;
        data[0] = static_cast<float>(value);
        mask[0] = true;
        return core_host_write(address, data, mask);
    }

    bool program_core_registers()
    {
        if (!write_scalar(sauria::CFG_REGS_OFFSET |
                              (sauria::CFG_CON_OFFSET + 0x00),
                          regs.k_dimension + kCols) ||
            !write_scalar(sauria::CFG_REGS_OFFSET |
                              (sauria::CFG_CON_OFFSET + 0x04),
                          1) ||
            !write_scalar(sauria::CFG_REGS_OFFSET |
                              (sauria::CFG_CON_OFFSET + 0x08),
                          1)) {
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
        return write_scalar(sauria::CFG_REGS_OFFSET |
                                (sauria::CFG_ACT_OFFSET + 0x28),
                            regs.dilation_pattern);
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

        if (!program_core_registers()) {
            return error_code::reset_aborted;
        }

        sauria::host_mask_t full_mask;
        full_mask.data.fill(true);

        // SRAM A native layout: one vector of 32 rows for every K index,
        // split into eight 4-element host beats.
        for (std::uint32_t k = 0; k < regs.k_dimension; ++k) {
            for (std::uint32_t sw = 0; sw < 8; ++sw) {
                sauria::host_data_t data;
                for (std::uint32_t lane = 0; lane < 4; ++lane) {
                    const std::uint32_t row = sw * 4 + lane;
                    const std::uint8_t raw = activations[row * stride + k];
                    data[lane] =
                        static_cast<float>(static_cast<std::int8_t>(raw));
                }
                const std::uint32_t address =
                    sauria::SRAMA_OFFSET | ((k << 3) | sw);
                if (!core_host_write(address, data, full_mask)) {
                    return error_code::reset_aborted;
                }
            }
        }

        // The SAURIA weight feeder expects the input pre-skewed by column.
        // Firmware supplies a normal row-major Kx32 matrix; the wrapper models
        // the front-end transformation while staging the private SRAM.
        for (std::uint32_t index = 0;
             index < regs.k_dimension + kCols; ++index) {
            for (std::uint32_t sw = 0; sw < 8; ++sw) {
                sauria::host_data_t data;
                for (std::uint32_t lane = 0; lane < 4; ++lane) {
                    const std::uint32_t col = sw * 4 + lane;
                    const std::int64_t k =
                        static_cast<std::int64_t>(index) - col;
                    std::int8_t value = 0;
                    if (k >= 0 &&
                        k < static_cast<std::int64_t>(regs.k_dimension)) {
                        value = static_cast<std::int8_t>(
                            weights[static_cast<std::size_t>(k) * kCols + col]);
                    }
                    data[lane] = static_cast<float>(value);
                }
                const std::uint32_t address =
                    sauria::SRAMB_OFFSET | ((index << 3) | sw);
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
        for (std::uint32_t col = 0; col < kCols; ++col) {
            for (std::uint32_t sw = 0; sw < 8; ++sw) {
                sauria::host_data_t data;
                const std::uint32_t address =
                    sauria::SRAMC_OFFSET | ((col << 3) | sw);
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

    std::uint32_t value = 0;
    tlm::tlm_response_status status = tlm::TLM_COMMAND_ERROR_RESPONSE;
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        status = impl_->read_reg(static_cast<std::uint32_t>(address), value);
        if (status == tlm::TLM_OK_RESPONSE) {
            std::memcpy(data, &value, sizeof(value));
        }
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(&value, data, sizeof(value));
        status = impl_->write_reg(static_cast<std::uint32_t>(address), value);
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
            if (!impl_->job_pending && !impl_->soft_reset_pending) {
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
            }
        }

        impl_->core_rst_n.write(false);
        impl_->drive_idle_host();
        impl_->reset_registers();
    }
}

} // namespace cdc::components
