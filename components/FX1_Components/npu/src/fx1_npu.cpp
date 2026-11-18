// SPDX-License-Identifier: Apache-2.0
//
// FX1 NPU (SAURIA v4.7) TLM integration. See fx1_npu.h and README.md.
//
// The native engine part (GEMM bank, native MMIO through the native host port,
// table aliases, counters) is cloned from components/npu_tlm/src/npu_tlm.cpp at
// CDC-VP 915b6ad5. The rich part is new: an accept queue in front of
// has::HasNpuTop, decoded at acceptance by a copy of the model's own decoder.

// sc_spawn / sc_bind
#define SC_INCLUDE_DYNAMIC_PROCESSES
#include "fx1_npu/fx1_npu.h"
#include "fx1_npu/fx1_npu_regmap.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

// Same build switches as the drop's core_rtl top level (core_rtl/npu_top.h).
#ifndef SAURIA_CORE_BACKEND_RTL_REF
#define SAURIA_CORE_BACKEND_RTL_REF
#endif
#ifndef FX1_A3_SRAM_BACKDOOR_LOAD
#define FX1_A3_SRAM_BACKDOOR_LOAD
#endif
#include <has/has_npu_top.h>
#include <npu_top.h>
#include <libsauria_cfg.h>
#include <npu_profile.h>
#include <perf_counters.h>
#include <sauria_run.h>
#include <sauria_targets.h>

#ifndef FX1_NPU_MODEL_HASH
#define FX1_NPU_MODEL_HASH 0u
#endif

extern char** environ;

namespace fx1::npu {
namespace {

using namespace reg;

constexpr std::uint32_t kArrayRows = 32;
constexpr std::uint32_t kArrayCols = 32;
constexpr std::uint32_t kSoftwareRows = kArrayRows;
constexpr std::uint32_t kSoftwareCols = kArrayCols;
constexpr std::uint32_t kSoftwareRowSubwords = kSoftwareRows / 4;
constexpr std::uint32_t kSoftwareMaxK = 1024 - kSoftwareCols;
constexpr std::uint32_t kSramASubwords = kArrayRows / 4;
constexpr std::uint32_t kSramBSubwords = kArrayCols / 4;
constexpr std::uint32_t kSramCSubwords = kArrayRows / 4;
constexpr std::uint32_t log2_power_of_two(std::uint32_t value)
{
    std::uint32_t shift = 0;
    while ((std::uint32_t{1} << shift) < value) ++shift;
    return shift;
}
constexpr std::uint32_t kSramAShift = log2_power_of_two(kSramASubwords);
constexpr std::uint32_t kSramBShift = log2_power_of_two(kSramBSubwords);
constexpr std::uint32_t kSramCShift = log2_power_of_two(kSramCSubwords);
constexpr std::uint32_t kCoreTimeoutCycles = 50'000;
// HasNpuTop: model staging area after the image (has/has_npu_top.h kStageABytes + 2 x kStageCBytes).
constexpr std::uint32_t kModelStagingBytes = 128 * 1024 + 2 * 1536 * 128;
// Offset given to an address register that is null or outside the window: any
// region it would start beyond every window, so the bounds check refuses an
// instruction that uses it, and an instruction that does not use it runs.
constexpr std::uint32_t kInvalidOffset = 0xFFFF'0000u;
// Rich worker: polling period (cycles) while an instruction runs on the model.
constexpr unsigned kPollCycles = 16;

struct vp_alias_range {
    std::uint32_t alias_base, size, model_base;
};
constexpr std::array<vp_alias_range, 14> kTableAliases{{
    {OBP_A_LUT_BASE, OBP_A_LUT_SIZE, 0x0014'0000},   {OBP_A_BIAS_BASE, OBP_A_BIAS_SIZE, 0x0015'0000},
    {OBP_A_SCALE_BASE, OBP_A_SCALE_SIZE, 0x0018'0000}, {OBP_A_SHIFT_BASE, OBP_A_SHIFT_SIZE, 0x0019'0000},
    {OBP_B_LUT_BASE, OBP_B_LUT_SIZE, 0x0016'0000},   {OBP_B_BIAS_BASE, OBP_B_BIAS_SIZE, 0x0017'0000},
    {OBP_B_SCALE_BASE, OBP_B_SCALE_SIZE, 0x001A'0000}, {OBP_B_SHIFT_BASE, OBP_B_SHIFT_SIZE, 0x001B'0000},
    {RCE_A_EXP_BASE, RCE_A_EXP_SIZE, 0x0020'0000},   {RCE_A_RECIP_BASE, RCE_A_RECIP_SIZE, 0x0021'0000},
    {RCE_A_RSQRT_BASE, RCE_A_RSQRT_SIZE, 0x0022'0000}, {RCE_B_EXP_BASE, RCE_B_EXP_SIZE, 0x0023'0000},
    {RCE_B_RECIP_BASE, RCE_B_RECIP_SIZE, 0x0024'0000}, {RCE_B_RSQRT_BASE, RCE_B_RSQRT_SIZE, 0x0025'0000},
}};

using native_top =
    sauria::NpuTop<kArrayCols, kArrayRows, sauria::act_t, sauria::wei_t, sauria::psum_t, 16, kArrayCols + kArrayRows, 1,
                   5056, 5184, 1536>;

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

float bits_to_float(std::uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
std::uint32_t low32(std::uint64_t v) { return static_cast<std::uint32_t>(v & 0xFFFF'FFFFULL); }
std::uint32_t high32(std::uint64_t v) { return static_cast<std::uint32_t>(v >> 32); }
std::int32_t clamp_i64(std::int64_t value)
{
    if (value > std::numeric_limits<std::int32_t>::max()) return std::numeric_limits<std::int32_t>::max();
    if (value < std::numeric_limits<std::int32_t>::min()) return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
std::uint32_t make_sram_a_addr(std::uint32_t phys, std::uint32_t sw)
{
    return sauria::SRAMA_OFFSET | (phys << kSramAShift) | (sw & (kSramASubwords - 1));
}
std::uint32_t make_sram_b_addr(std::uint32_t phys, std::uint32_t sw)
{
    return sauria::SRAMB_OFFSET | (phys << kSramBShift) | (sw & (kSramBSubwords - 1));
}
std::uint32_t make_sram_c_addr(std::uint32_t phys, std::uint32_t sw)
{
    return sauria::SRAMC_OFFSET | (phys << kSramCShift) | (sw & (kSramCSubwords - 1));
}
sauria::SauriaLayerDesc make_gemm_as_conv1x1_desc(std::uint32_t rows, std::uint32_t cols, std::uint32_t depth,
                                                  std::uint32_t tile_rows, std::uint32_t tile_cols)
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

// Rich parameter / extension registers that hold DRAM addresses (the lists of
// the drop's core_rtl/npu_top.h: param_is_address, ext_is_address).
constexpr std::array<std::uint32_t, 8> kParamAddressRegs{0x4000'0400u, 0x4000'0404u, 0x4000'0408u, 0x4000'040Cu,
                                                         0x4000'0434u, 0x4000'0444u, 0x4000'0448u, 0x4000'044Cu};
constexpr std::array<std::uint32_t, 5> kExtAddressRegs{0x4000'0490u, 0x4000'0494u, 0x4000'0498u, 0x4000'04E0u,
                                                       0x4000'04E4u};
// Words of the neo_lite register file that hold DRAM addresses (core_rtl/npu_top.h neo_is_address_word).
constexpr std::array<unsigned, 7> kNeoAddressWords{18, 19, 20, 34, 35, 36, 37};

bool env_flag(const char* name)
{
    const char* v = std::getenv(name);
    return v && v[0] == '1';
}
has::Profile env_profile()
{
    const char* v = std::getenv("SAURIA_CORE_RTL_PROFILE");
    if (v && std::strcmp(v, "legacy") == 0) return has::Profile::Legacy;
    if (v && std::strcmp(v, "proposals") == 0) return has::Profile::Proposals;
    return has::Profile::Recommended;
}

struct Region {
    std::uint64_t off = 0, bytes = 0;
};

// Model internal error counters that make a retired instruction suspect
// (the list of core_rtl/npu_top.h err_snap()).
struct ModelErrors {
    std::uint64_t framing = 0, wide = 0, deadlock = 0, param = 0, cfg = 0, lut = 0, oot = 0;
    bool operator!=(const ModelErrors& o) const
    {
        return framing != o.framing || wide != o.wide || deadlock != o.deadlock || param != o.param || cfg != o.cfg ||
               lut != o.lut || oot != o.oot;
    }
};

struct ModelCounters {
    std::uint64_t cycles = 0, dma = 0, obp = 0, rce = 0, elem = 0, rd = 0, wr = 0;
    std::uint64_t busy = 0, real = 0, macs = 0, rd_cyc = 0, wr_cyc = 0;
};

} // namespace

// ---------------------------------------------------------------------------
struct Fx1Npu::impl : public sc_core::sc_module {
    enum class native_command { none, read, write };

    struct register_file {
        std::uint32_t ctrl = 0;
        std::uint32_t status = STATUS_IDLE;
        std::uint32_t irq_enable = 0;
        std::uint32_t irq_status = 0;
        std::uint32_t src_addr = 0, dst_addr = 0, scratch_addr = 0, src_size = 0, dst_size = 0;
        std::uint32_t width = kSoftwareCols, height = kSoftwareRows, src_stride = 0;
        std::uint32_t format = FORMAT_INT8_INT8_INT32, op_mode = OP_GEMM;
        std::uint32_t weights_addr = 0, param_addr = 0, weights_size = 0;
        std::uint32_t k_dimension = 64, threshold_bits = 0, rows_active = 0xFFFF'FFFFu, dilation_pattern = 1;
        std::uint32_t input_offset = 0, weight_offset = 0, output_offset = 0;
        std::uint32_t activation_min = static_cast<std::uint32_t>(-128), activation_max = 127;
        std::uint32_t bias_addr = 0, bias_size = 0, multiplier_addr = 0, multiplier_size = 0, shift_addr = 0, shift_size = 0;
        std::uint32_t rich_window_base = 0, rich_window_size = 0;
        std::uint32_t cycle_count = 0, bytes_read = 0, bytes_written = 0;
        std::uint32_t last_error = static_cast<std::uint32_t>(error_code::none);
    };

    // One accepted rich push.
    struct Entry {
        std::uint64_t seq = 0;
        bool neo = false;
        std::uint32_t opcode = 0;
        std::uint32_t win_base = 0, win_size = 0;
        std::array<std::uint32_t, RICH_PARAM_COUNT> par{};
        std::array<std::uint32_t, RICH_EXT_COUNT> ext{};
        std::array<bool, RICH_EXT_COUNT> ext_set{};
        std::array<std::uint32_t, NEO_REG_COUNT> neo_reg{};
        std::uint32_t neo_ien = 0;
        std::vector<Region> reads, writes;  // offsets in the window
    };

    struct gated_clock {
        sc_core::sc_signal<bool>* sig = nullptr;
        sc_core::sc_time half;
        int users = 0;
        sc_core::sc_event on;
    };

    Fx1Npu& owner;
    NpuOptions opt;
    std::uint32_t model_hash;

    // ---- clocks (gated: no edge while the engine has nothing to do) -------
    sc_core::sc_signal<bool> native_clk{"native_clk"};
    sc_core::sc_signal<bool> rich_clk{"rich_clk"};
    gated_clock native_gclk, rich_gclk;

    // ---- native engine (cloned from npu_tlm) -------------------------------
    native_top native;
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
    std::vector<std::uint8_t> native_dram;  // the native decoder never receives a push in FX1
    std::uint32_t native_mvm_k = 0, native_total_contexts = 0;

    // ---- rich engine: has::HasNpuTop ---------------------------------------
    has::Profile profile = has::Profile::Recommended;
    // Written by the rich worker and by a CTRL.SOFT_RESET in the native worker.
    sc_core::sc_signal<bool, sc_core::SC_MANY_WRITERS> rich_rstn{"rich_rstn"};
    sc_core::sc_signal<std::uint32_t> r_addr{"r_addr"};
    sc_core::sc_signal<bool> r_wren{"r_wren"}, r_rden{"r_rden"};
    sc_core::sc_signal<sauria::host_data_t> r_wdata{"r_wdata"}, r_rdata{"r_rdata"};
    sc_core::sc_signal<sauria::host_mask_t> r_wmask{"r_wmask"};
    sc_core::sc_signal<bool> r_irq{"r_irq"}, r_done{"r_done"}, r_deadlock{"r_deadlock"};
    std::unique_ptr<has::HasNpuTop> rtl;
    std::vector<std::uint8_t> image;  // offset 0 = window base of the running entry
    std::uint32_t staging_base = 0;   // current HasNpuTop staging base (0: not set yet)
    std::size_t image_peak = 0;
    // Decoder copies used at acceptance (pure C++; never connected to SystemC).
    std::unique_ptr<has::MmioCompat> probe;
    std::unique_ptr<has::NeoPort> probe_neo;
    bool neo_on = false;

    fx1::PerfCounters perf;
    register_file regs;

    // ---- rich shadow registers (what the CPU wrote) ------------------------
    std::array<std::uint32_t, RICH_PARAM_COUNT> rich_par{};
    std::array<std::uint32_t, RICH_EXT_COUNT> rich_ext{};
    std::array<bool, RICH_EXT_COUNT> rich_ext_set{};
    std::array<std::uint32_t, NEO_REG_COUNT> neo_reg{};
    std::uint32_t neo_ien = 0;
    bool neo_armed = false;
    std::uint64_t neo_done = 0, neo_cleared_ctrl = 0, neo_cleared_ist = 0;

    // ---- rich queue ---------------------------------------------------------
    std::deque<Entry> queue;
    std::optional<Entry> exec;  // popped, not finished
    bool exec_aborted = false;
    bool copyback_active = false;
    bool rich_error_in_batch = false;
    std::uint64_t accepted = 0, retired = 0;
    std::uint32_t rich_err_code = 0;
    bool rich_sticky = false;
    sc_core::sc_event queue_event, slot_event, rich_idle_event;

    // ---- VP bank ------------------------------------------------------------
    std::uint32_t vp_error = 0, vp_error_info = 0;
    std::uint32_t push_wait_ns;
    std::uint64_t abort_dropped = 0;
    bool abort_report_pending = false;  // finish_error(reset_aborted) once the aborted work stopped
    // Incremented by every reset: work that started before it reports nothing.
    std::uint64_t reset_generation = 0;

    // ---- native worker state (npu_tlm) ---------------------------------------
    sc_core::sc_event work_event;
    bool job_pending = false;
    bool soft_reset_pending = false;
    native_command native_cmd = native_command::none;
    std::uint32_t native_address = 0, native_write_value = 0, native_read_value = 0;
    tlm::tlm_response_status native_status = tlm::TLM_INCOMPLETE_RESPONSE;
    bool native_pending = false;
    bool native_active = false;
    bool software_run_active = false;
    bool native_run_active = false;
    bool native_abort = false;
    std::uint32_t native_run_cycles = 0;
    sc_core::sc_event native_done_event;

    // ---- DMA ----------------------------------------------------------------
    bool dma_active = false;
    sc_core::sc_event dma_free_event;

    NpuStats st;
    // The interrupt level, computed by update_irq() in whichever process changed
    // the registers (transport callers: one per CPU; both workers). Only
    // drive_irq() (one method) writes irq_out: a signal written by two
    // processes in one delta cycle stops the simulation (E115), even with
    // SC_MANY_WRITERS (review N2: two initiators acknowledging while a worker
    // completes).
    bool irq_level = false;
    sc_core::sc_event irq_event;

    // ---- host wall-time watchdog -----------------------------------------
    std::atomic<std::uint64_t> heartbeat{0};
    std::atomic<int> armed{0};
    std::atomic<bool> watchdog_stop{false};
    std::mutex watchdog_mutex;
    std::condition_variable watchdog_cv;
    std::thread watchdog;

    std::string config_text;

    impl(sc_core::sc_module_name name, Fx1Npu& owner_ref, const NpuOptions& options)
        : sc_core::sc_module(name)
        , owner(owner_ref)
        , opt(options)
        , model_hash(options.model_hash ? options.model_hash : static_cast<std::uint32_t>(FX1_NPU_MODEL_HASH))
        , native("native", exact_int8_config())
        , push_wait_ns(options.push_wait_ns)
    {
        if (opt.queue_depth == 0 || opt.queue_depth > 255) throw std::invalid_argument("fx1_npu: queue depth 1..255");
        if (opt.max_window == 0 || opt.max_window > 0x8000'0000u) throw std::invalid_argument("fx1_npu: max window");
        if (opt.dma_chunk_bytes < 4 || (opt.dma_chunk_bytes & (opt.dma_chunk_bytes - 1)) != 0)
            throw std::invalid_argument("fx1_npu: DMA chunk must be a power of two >= 4");
        if (opt.ram_size == 0 || opt.ram_base > std::numeric_limits<std::uint32_t>::max() ||
            opt.ram_base + opt.ram_size > 0x1'0000'0000ULL)
            throw std::invalid_argument("fx1_npu: RAM range must lie in the 32-bit address space");

        native_gclk.sig = &native_clk;
        native_gclk.half = opt.clock_period / 2;
        rich_gclk.sig = &rich_clk;
        rich_gclk.half = opt.clock_period / 2;
        sc_core::sc_spawn(sc_bind(&impl::clock_loop, this, &native_gclk), "native_clk_gen");
        sc_core::sc_spawn(sc_bind(&impl::clock_loop, this, &rich_gclk), "rich_clk_gen");

        native.i_clk(native_clk);
        native.i_rstn(core_rst_n);
        native.i_soft_reset(core_soft_reset);
        native.i_start(core_start);
        native.o_done(core_done);
        native.o_deadlock(core_deadlock);
        native.i_mvm_k(mvm_k);
        native.i_host_addr(host_addr);
        native.i_host_wren(host_wren);
        native.i_host_rden(host_rden);
        native.i_host_wdata(host_wdata);
        native.i_host_wmask(host_wmask);
        native.o_host_rdata(host_rdata);
        native.i_threshold(threshold);
        native.i_select(buffer_select);
        native.i_total_contexts(total_contexts);
        native.attach_perf(&perf);
        native.set_dram(&native_dram);
        perf.X = kArrayCols;
        perf.Y = kArrayRows;

        // HasNpuTop exactly as the drop's core_rtl top level builds it.
        profile = env_profile();
        rtl = std::make_unique<has::HasNpuTop>(
            "rtl", has::profile_knobs(profile, has::Knobs::from_env(profile != has::Profile::Legacy)));
        rtl->apply_profile(profile);
        rtl->c_bias_bcast = rtl->c_bias_bcast || env_flag("SAURIA_CORE_RTL_C_BCAST");
        rtl->trace = env_flag("SAURIA_CORE_RTL_TRACE");
        rtl->set_clock_period(opt.clock_period);
        rtl->i_clk(rich_clk);
        rtl->i_rstn(rich_rstn);
        rtl->i_host_addr(r_addr);
        rtl->i_host_wren(r_wren);
        rtl->i_host_rden(r_rden);
        rtl->i_host_wdata(r_wdata);
        rtl->i_host_wmask(r_wmask);
        rtl->o_host_rdata(r_rdata);
        rtl->o_irq(r_irq);
        rtl->o_done(r_done);
        rtl->o_deadlock(r_deadlock);
        // The image starts empty: set_dram() puts the model staging area at 0
        // for now; it is moved above the window of the first entry (set_staging).
        rtl->set_dram(&image);
        probe = std::make_unique<has::MmioCompat>(rtl->mmio);
        probe_neo = std::make_unique<has::NeoPort>();
        neo_on = rtl->neo.on();

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
        rich_rstn.write(false);
        r_addr.write(0);
        r_wren.write(false);
        r_rden.write(false);

        config_text = make_config_text();
        if (opt.host_watchdog_s) watchdog = std::thread([this] { watchdog_loop(); });
    }

    ~impl() override
    {
        {
            std::lock_guard<std::mutex> lock(watchdog_mutex);
            watchdog_stop = true;
        }
        watchdog_cv.notify_all();
        if (watchdog.joinable()) watchdog.join();
    }

    std::string make_config_text() const
    {
        std::ostringstream out;
        out << "profile=" << has::profile_name(profile) << " host=" << (neo_on ? "neo" : "off");
        std::vector<std::string> vars;
        for (char** e = environ; e && *e; ++e) {
            const std::string kv = *e;
            if (kv.rfind("HAS_", 0) == 0 || kv.rfind("SAURIA_CORE_RTL_", 0) == 0) vars.push_back(kv);
        }
        std::sort(vars.begin(), vars.end());
        out << " env=" << (vars.empty() ? std::string("none") : std::string());
        for (std::size_t i = 0; i < vars.size(); ++i) out << (i ? "," : "") << vars[i];
        return out.str();
    }

    // =========================================================== clocks ====
    void clock_loop(gated_clock* c)
    {
        c->sig->write(false);
        for (;;) {
            while (c->users == 0) sc_core::wait(c->on);
            c->sig->write(true);
            heartbeat.fetch_add(1, std::memory_order_relaxed);
            sc_core::wait(c->half);
            c->sig->write(false);
            sc_core::wait(c->half);
        }
    }
    void clock_use(gated_clock& c, bool on)
    {
        if (on) {
            if (c.users++ == 0) c.on.notify(sc_core::SC_ZERO_TIME);
            armed.fetch_add(1);
        } else if (c.users > 0) {
            --c.users;
            armed.fetch_sub(1);
        }
    }
    struct clock_hold {
        impl& self;
        gated_clock& clock;
        clock_hold(impl& s, gated_clock& c) : self(s), clock(c) { self.clock_use(clock, true); }
        ~clock_hold() { self.clock_use(clock, false); }
    };

    void watchdog_loop()
    {
        std::uint64_t last = heartbeat.load();
        auto since = std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> lock(watchdog_mutex);
        while (!watchdog_stop) {
            watchdog_cv.wait_for(lock, std::chrono::seconds(1));
            if (watchdog_stop) break;
            const std::uint64_t now_beat = heartbeat.load();
            const auto now = std::chrono::steady_clock::now();
            if (armed.load() <= 0 || now_beat != last) {
                last = now_beat;
                since = now;
                continue;
            }
            const auto stalled = std::chrono::duration_cast<std::chrono::seconds>(now - since).count();
            if (stalled >= static_cast<long long>(opt.host_watchdog_s)) {
                std::fprintf(stderr,
                             "[fx1_npu] FATAL: host watchdog: the NPU has work but simulated time did not advance for %lld s of "
                             "wall time (a model step that never yields). Exit %d.\n",
                             static_cast<long long>(stalled), opt.host_watchdog_exit);
                std::fflush(stderr);
                std::fflush(stdout);
                std::_Exit(opt.host_watchdog_exit);
            }
        }
    }

    bool wait_native_clock(unsigned count = 1)
    {
        clock_hold hold(*this, native_gclk);
        for (unsigned i = 0; i < count; ++i) {
            sc_core::wait(native_clk.posedge_event());
            sc_core::wait(sc_core::SC_ZERO_TIME);
            if (!owner.reset_n.read()) return false;
        }
        return true;
    }

    // ===================================================== status / IRQ ====
    bool native_busy() const { return software_run_active || native_run_active; }
    bool rich_busy() const { return !queue.empty() || exec.has_value(); }
    bool gemm_bank_busy() const { return (regs.status & STATUS_BUSY) != 0u; }

    void update_irq()
    {
        const bool level = (regs.ctrl & CTRL_IRQ_EN) != 0u && (regs.irq_enable & regs.irq_status) != 0u;
        irq_level = level;
        irq_event.notify(sc_core::SC_ZERO_TIME);
    }
    void finish_success()
    {
        software_run_active = false;
        regs.status = (regs.status & STATUS_ERROR) | STATUS_DONE | STATUS_IDLE;
        regs.irq_status |= IRQ_DONE;
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
    // The rich side reports through the same bank (D17): a sticky ERROR + IRQ
    // that firmware clears by W1C, without hiding BUSY while work remains.
    void rich_error(std::uint32_t rich_code, std::uint32_t vp_bits, std::uint32_t info, error_code bank_code)
    {
        rich_err_code = rich_code;
        rich_sticky = true;
        vp_error |= vp_bits;
        vp_error_info = info;
        rich_error_in_batch = true;
        regs.status |= STATUS_ERROR;
        regs.irq_status |= IRQ_ERROR;
        regs.last_error = static_cast<std::uint32_t>(bank_code);
        update_irq();
    }
    void rich_batch_end()
    {
        regs.status = (regs.status & ~STATUS_BUSY) | STATUS_IDLE;
        if (!rich_error_in_batch) {
            regs.status |= STATUS_DONE;
            regs.irq_status |= IRQ_DONE;
        }
        rich_error_in_batch = false;
        update_irq();
        rich_idle_event.notify(sc_core::SC_ZERO_TIME);
    }

    bool quiescent() const
    {
        const bool will_dma = !queue.empty() || (exec.has_value() && !exec_aborted) || dma_active || software_run_active;
        return !will_dma;
    }
    std::uint32_t vp_status() const
    {
        std::uint32_t s = 0;
        if (quiescent()) s |= VP_STATUS_QUIESCENT;
        if (exec.has_value() || rtl->mmio.busy()) s |= VP_STATUS_ENGINE_BUSY;
        if (exec.has_value() && exec_aborted) s |= VP_STATUS_ABORT_ACTIVE;
        if (copyback_active) s |= VP_STATUS_COPYBACK_ACTIVE;
        if (native_busy()) s |= VP_STATUS_NATIVE_BUSY;
        s |= (static_cast<std::uint32_t>(std::min<std::size_t>(queue.size(), 255)) << VP_STATUS_QUEUED_SHIFT);
        return s;
    }
    std::uint32_t feature_bits() const
    {
        std::uint32_t f = VP_FEATURE_NATIVE_GEMM | VP_FEATURE_RICH_HAS | VP_FEATURE_ABORT | VP_FEATURE_BOUNDED_PUSH |
                          VP_FEATURE_QUEUE;
        if (neo_on) f |= VP_FEATURE_DFC_WINDOW;
        return f;
    }
    std::uint32_t rich_status() const
    {
        const std::uint32_t q = static_cast<std::uint32_t>(std::min<std::size_t>(queue.size(), 255));
        return (rich_busy() ? RICH_STATUS_BUSY : 0u) | (queue.size() >= opt.queue_depth ? RICH_STATUS_QUEUE_FULL : 0u) |
               (rich_sticky ? RICH_STATUS_ERROR : 0u) | (q << RICH_STATUS_QUEUED_SHIFT) |
               ((rich_err_code & 0xFFFFu) << RICH_STATUS_ERROR_SHIFT);
    }

    // ============================================== counters (npu_tlm) ====
    std::uint64_t perf_value(std::uint32_t low_address) const
    {
        switch (low_address) {
        case PERF_EXEC_CYCLES: return perf.exec_cycles;
        case PERF_STALL_CYCLES: return perf.stall_cycles;
        case PERF_MAC_OPS: return perf.mac_ops;
        case PERF_ACTIVE_PE_CYCLES: return perf.active_pe_cycles;
        case PERF_TOTAL_PE_CYCLES: return perf.total_pe_cycles;
        case PERF_TOTAL_CYCLES: return perf.total_cycles;
        case PERF_M: return perf.M;
        case PERF_K: return perf.K;
        case PERF_N: return perf.N;
        case PERF_SA_CYCLES: return perf.sa_cycles;
        case PERF_OBP_CYCLES: return perf.obp_cycles;
        case PERF_PROCESSING_CYCLES: return perf.processing_cycles;
        case PERF_TRANSFER_CYCLES: return perf.transfer_cycles;
        case PERF_MAC_ENGINE_CYCLES: return perf.mac_engine_cycles;
        case PERF_DMA_ENGINE_CYCLES: return perf.dma_engine_cycles;
        case PERF_ACTIVATION_ENGINE_CYCLES: return perf.activation_engine_cycles;
        case PERF_POOLING_ENGINE_CYCLES: return perf.pooling_engine_cycles;
        case PERF_REDUCTION_ENGINE_CYCLES: return perf.reduction_engine_cycles;
        case PERF_DDR_READ_BYTES: return perf.ddr_read_bytes;
        case PERF_DDR_WRITE_BYTES: return perf.ddr_write_bytes;
        case PERF_DMA_READ_CYCLES: return perf.dma_read_cycles_raw;
        case PERF_DMA_WRITE_CYCLES: return perf.dma_write_cycles_raw;
        default: return 0;
        }
    }
    static bool is_perf_low(std::uint32_t a)
    {
        switch (a) {
        case PERF_EXEC_CYCLES: case PERF_STALL_CYCLES: case PERF_MAC_OPS: case PERF_ACTIVE_PE_CYCLES:
        case PERF_TOTAL_PE_CYCLES: case PERF_TOTAL_CYCLES: case PERF_M: case PERF_K: case PERF_N:
        case PERF_SA_CYCLES: case PERF_OBP_CYCLES: case PERF_PROCESSING_CYCLES: case PERF_TRANSFER_CYCLES:
        case PERF_MAC_ENGINE_CYCLES: case PERF_DMA_ENGINE_CYCLES: case PERF_ACTIVATION_ENGINE_CYCLES:
        case PERF_POOLING_ENGINE_CYCLES: case PERF_REDUCTION_ENGINE_CYCLES: case PERF_DDR_READ_BYTES:
        case PERF_DDR_WRITE_BYTES: case PERF_DMA_READ_CYCLES: case PERF_DMA_WRITE_CYCLES:
            return true;
        default:
            return false;
        }
    }
    static bool perf_low_for_high(std::uint32_t a, std::uint32_t& low)
    {
        static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 22> kPairs{{
            {PERF_EXEC_CYCLES_HI, PERF_EXEC_CYCLES},
            {PERF_STALL_CYCLES_HI, PERF_STALL_CYCLES},
            {PERF_MAC_OPS_HI, PERF_MAC_OPS},
            {PERF_ACTIVE_PE_CYCLES_HI, PERF_ACTIVE_PE_CYCLES},
            {PERF_TOTAL_PE_CYCLES_HI, PERF_TOTAL_PE_CYCLES},
            {PERF_TOTAL_CYCLES_HI, PERF_TOTAL_CYCLES},
            {PERF_M_HI, PERF_M},
            {PERF_K_HI, PERF_K},
            {PERF_N_HI, PERF_N},
            {PERF_SA_CYCLES_HI, PERF_SA_CYCLES},
            {PERF_OBP_CYCLES_HI, PERF_OBP_CYCLES},
            {PERF_PROCESSING_CYCLES_HI, PERF_PROCESSING_CYCLES},
            {PERF_TRANSFER_CYCLES_HI, PERF_TRANSFER_CYCLES},
            {PERF_MAC_ENGINE_CYCLES_HI, PERF_MAC_ENGINE_CYCLES},
            {PERF_DMA_ENGINE_CYCLES_HI, PERF_DMA_ENGINE_CYCLES},
            {PERF_ACTIVATION_ENGINE_CYCLES_HI, PERF_ACTIVATION_ENGINE_CYCLES},
            {PERF_POOLING_ENGINE_CYCLES_HI, PERF_POOLING_ENGINE_CYCLES},
            {PERF_REDUCTION_ENGINE_CYCLES_HI, PERF_REDUCTION_ENGINE_CYCLES},
            {PERF_DDR_READ_BYTES_HI, PERF_DDR_READ_BYTES},
            {PERF_DDR_WRITE_BYTES_HI, PERF_DDR_WRITE_BYTES},
            {PERF_DMA_READ_CYCLES_HI, PERF_DMA_READ_CYCLES},
            {PERF_DMA_WRITE_CYCLES_HI, PERF_DMA_WRITE_CYCLES},
        }};
        for (const auto& p : kPairs)
            if (p.first == a) {
                low = p.second;
                return true;
            }
        return false;
    }
    bool read_perf(std::uint32_t address, std::uint32_t& value) const
    {
        if (is_perf_low(address)) {
            value = low32(perf_value(address));
            return true;
        }
        std::uint32_t low = 0;
        if (perf_low_for_high(address, low)) {
            value = high32(perf_value(low));
            return true;
        }
        return false;
    }

    ModelCounters model_counters() const
    {
        ModelCounters s;
        s.cycles = rtl->st.cycles;
        s.dma = rtl->st.dma_cycles;
        s.obp = rtl->st.obp_cycles;
        s.rce = rtl->st.rce_cycles;
        s.elem = rtl->st.elem_cycles;
        s.rd = rtl->dma_stats().read_bytes;
        s.wr = rtl->dma_stats().write_bytes;
        s.busy = rtl->st.core_busy;
        s.real = rtl->st.real_macs;
        s.macs = rtl->st.macs;
        s.rd_cyc = rtl->dma_stats().read_port_cycles;
        s.wr_cyc = rtl->dma_stats().write_port_cycles;
        return s;
    }
    // core_rtl/npu_top.h add_rtl_counters(): what one HasNpuTop instruction adds.
    void add_model_counters(const ModelCounters& a, const ModelCounters& b)
    {
        perf.exec_cycles += b.cycles - a.cycles;
        perf.processing_cycles += b.cycles - a.cycles;
        perf.dma_engine_cycles += b.dma - a.dma;
        perf.obp_cycles += b.obp - a.obp;
        perf.reduction_engine_cycles += b.rce - a.rce;
        perf.pooling_engine_cycles += b.elem - a.elem;
        perf.ddr_read_bytes += b.rd - a.rd;
        perf.ddr_write_bytes += b.wr - a.wr;
        perf.dma_read_cycles_raw += b.rd_cyc - a.rd_cyc;
        perf.dma_write_cycles_raw += b.wr_cyc - a.wr_cyc;
        perf.active_pe_cycles += b.real - a.real;
        perf.total_pe_cycles += (b.busy - a.busy) * std::uint64_t{kArrayCols} * kArrayRows;
        perf.sa_cycles += b.busy - a.busy;
        perf.mac_engine_cycles += b.busy - a.busy;
        perf.mac_ops += b.macs - a.macs;
    }
    ModelErrors model_errors() const
    {
        ModelErrors e;
        e.framing = rtl->st.framing_errors;
        e.wide = rtl->st.wide_weight_rejected;
        e.deadlock = rtl->st.core_deadlocks;
        e.param = rtl->st.rce_param_errors;
        e.cfg = rtl->obp_block().cfg_errors;
        e.lut = rtl->obp_block().lut_lane_mismatch;
        e.oot = rtl->inline_out_of_tile;
        return e;
    }

    // ===================================================== GEMM bank ======
    bool in_ram(std::uint64_t addr, std::uint64_t size) const
    {
        return size != 0 && addr >= opt.ram_base && size <= opt.ram_size && addr - opt.ram_base <= opt.ram_size - size;
    }

    tlm::tlm_response_status read_reg(std::uint32_t offset, std::uint32_t& value) const
    {
        if (read_perf(offset, value)) return tlm::TLM_OK_RESPONSE;
        switch (offset) {
        case CTRL: value = regs.ctrl; break;
        case STATUS: value = regs.status; break;
        case IRQ_ENABLE: value = regs.irq_enable; break;
        case IRQ_STATUS: value = regs.irq_status; break;
        case SRC_ADDR: value = regs.src_addr; break;
        case DST_ADDR: value = regs.dst_addr; break;
        case SCRATCH_ADDR: value = regs.scratch_addr; break;
        case SRC_SIZE_BYTES: value = regs.src_size; break;
        case DST_SIZE_BYTES: value = regs.dst_size; break;
        case WIDTH: value = regs.width; break;
        case HEIGHT: value = regs.height; break;
        case SRC_STRIDE_BYTES: value = regs.src_stride; break;
        case FORMAT: value = regs.format; break;
        case OP_MODE: value = regs.op_mode; break;
        case WEIGHTS_ADDR: value = regs.weights_addr; break;
        case PARAM_ADDR: value = regs.param_addr; break;
        case WEIGHTS_SIZE_BYTES: value = regs.weights_size; break;
        case K_DIMENSION: value = regs.k_dimension; break;
        case ZERO_THRESHOLD_FP32: value = regs.threshold_bits; break;
        case ROWS_ACTIVE: value = regs.rows_active; break;
        case DILATION_PATTERN: value = regs.dilation_pattern; break;
        case CYCLE_COUNT: value = regs.cycle_count; break;
        case BYTES_READ: value = regs.bytes_read; break;
        case BYTES_WRITTEN: value = regs.bytes_written; break;
        case LAST_ERROR: value = regs.last_error; break;
        case CORE_ID: value = CORE_ID_VALUE; break;
        case INPUT_OFFSET: value = regs.input_offset; break;
        case WEIGHT_OFFSET: value = regs.weight_offset; break;
        case OUTPUT_OFFSET: value = regs.output_offset; break;
        case ACTIVATION_MIN: value = regs.activation_min; break;
        case ACTIVATION_MAX: value = regs.activation_max; break;
        case BIAS_ADDR: value = regs.bias_addr; break;
        case BIAS_SIZE_BYTES: value = regs.bias_size; break;
        case MULTIPLIER_ADDR: value = regs.multiplier_addr; break;
        case MULTIPLIER_SIZE_BYTES: value = regs.multiplier_size; break;
        case SHIFT_ADDR: value = regs.shift_addr; break;
        case SHIFT_SIZE_BYTES: value = regs.shift_size; break;
        case RICH_WINDOW_BASE: value = regs.rich_window_base; break;
        case RICH_WINDOW_SIZE: value = regs.rich_window_size; break;
        // VP bank
        case VP_ID: value = VP_ID_VALUE; break;
        case VP_ABI: value = VP_ABI_VALUE; break;
        case VP_MODEL_HASH: value = model_hash; break;
        case VP_FEATURE: value = feature_bits(); break;
        case VP_QUEUE_DEPTH: value = opt.queue_depth; break;
        case VP_QUEUE_FREE:
            value = static_cast<std::uint32_t>(queue.size() >= opt.queue_depth ? 0 : opt.queue_depth - queue.size());
            break;
        case VP_CMD: value = 0; break;
        case VP_STATUS: value = vp_status(); break;
        case VP_ERROR: value = vp_error; break;
        case VP_ERROR_INFO: value = vp_error_info; break;
        case VP_PUSH_WAIT_NS: value = push_wait_ns; break;
        case VP_ACCEPTED: value = low32(accepted); break;
        case VP_ABORT_DROPPED: value = low32(abort_dropped); break;
        case VP_WINDOW_MAX: value = opt.max_window; break;
        default: return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }
        return tlm::TLM_OK_RESPONSE;
    }

    tlm::tlm_response_status write_reg(std::uint32_t offset, std::uint32_t value)
    {
        std::uint32_t low = 0;
        if (is_perf_low(offset) || perf_low_for_high(offset, low)) return tlm::TLM_COMMAND_ERROR_RESPONSE;

        // VP bank (writable at any time: it never waits).
        switch (offset) {
        case VP_CMD:
            if (value & VP_CMD_CLEAR_ERROR) {
                vp_error = 0;
                vp_error_info = 0;
            }
            if (value & VP_CMD_ABORT) abort_all();
            return tlm::TLM_OK_RESPONSE;
        case VP_ERROR:
            vp_error &= ~value;
            if (vp_error == 0) vp_error_info = 0;
            return tlm::TLM_OK_RESPONSE;
        case VP_PUSH_WAIT_NS:
            push_wait_ns = value;
            return tlm::TLM_OK_RESPONSE;
        case VP_ID: case VP_ABI: case VP_MODEL_HASH: case VP_FEATURE: case VP_QUEUE_DEPTH: case VP_QUEUE_FREE:
        case VP_STATUS: case VP_ERROR_INFO: case VP_ACCEPTED: case VP_ABORT_DROPPED: case VP_WINDOW_MAX:
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        default: break;
        }

        if (offset == STATUS) {
            if (value & STATUS_DONE) {
                regs.status &= ~STATUS_DONE;
                regs.irq_status &= ~IRQ_DONE;
            }
            if (value & STATUS_ERROR) {
                regs.status &= ~STATUS_ERROR;
                regs.irq_status &= ~IRQ_ERROR;
                regs.last_error = static_cast<std::uint32_t>(error_code::none);
            }
            if (!gemm_bank_busy()) regs.status |= STATUS_IDLE;
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
            if (rich_busy() && (value & (CTRL_START | CTRL_SOFT_RESET)) != 0u) {
                regs.last_error = static_cast<std::uint32_t>(error_code::busy);
                return tlm::TLM_COMMAND_ERROR_RESPONSE;
            }
            if (gemm_bank_busy() && (value & (CTRL_START | CTRL_SOFT_RESET)) != 0u) {
                regs.last_error = static_cast<std::uint32_t>(error_code::busy);
                return tlm::TLM_COMMAND_ERROR_RESPONSE;
            }
            regs.ctrl = value & (CTRL_ENABLE | CTRL_IRQ_EN);
            if (value & CTRL_SOFT_RESET) {
                soft_reset_pending = true;
                work_event.notify(sc_core::SC_ZERO_TIME);
            }
            if (value & CTRL_START) {
                if ((regs.ctrl & CTRL_ENABLE) == 0u) {
                    finish_error(error_code::disabled);
                } else {
                    regs.status = STATUS_BUSY;
                    regs.irq_status = 0;
                    regs.last_error = static_cast<std::uint32_t>(error_code::none);
                    regs.cycle_count = 0;
                    regs.bytes_read = 0;
                    regs.bytes_written = 0;
                    software_run_active = true;
                    native_abort = false;
                    job_pending = true;
                    work_event.notify(sc_core::SC_ZERO_TIME);
                }
            }
            update_irq();
            return tlm::TLM_OK_RESPONSE;
        }
        if (gemm_bank_busy()) return tlm::TLM_COMMAND_ERROR_RESPONSE;

        switch (offset) {
        case SRC_ADDR: regs.src_addr = value; break;
        case DST_ADDR: regs.dst_addr = value; break;
        case SCRATCH_ADDR: regs.scratch_addr = value; break;
        case SRC_SIZE_BYTES: regs.src_size = value; break;
        case DST_SIZE_BYTES: regs.dst_size = value; break;
        case WIDTH: regs.width = value; break;
        case HEIGHT: regs.height = value; break;
        case SRC_STRIDE_BYTES: regs.src_stride = value; break;
        case FORMAT: regs.format = value; break;
        case OP_MODE: regs.op_mode = value; break;
        case WEIGHTS_ADDR: regs.weights_addr = value; break;
        case PARAM_ADDR: regs.param_addr = value; break;
        case WEIGHTS_SIZE_BYTES: regs.weights_size = value; break;
        case K_DIMENSION: regs.k_dimension = value; break;
        case ZERO_THRESHOLD_FP32: regs.threshold_bits = value; break;
        case ROWS_ACTIVE: regs.rows_active = value; break;
        case DILATION_PATTERN: regs.dilation_pattern = value; break;
        case INPUT_OFFSET: regs.input_offset = value; break;
        case WEIGHT_OFFSET: regs.weight_offset = value; break;
        case OUTPUT_OFFSET: regs.output_offset = value; break;
        case ACTIVATION_MIN: regs.activation_min = value; break;
        case ACTIVATION_MAX: regs.activation_max = value; break;
        case BIAS_ADDR: regs.bias_addr = value; break;
        case BIAS_SIZE_BYTES: regs.bias_size = value; break;
        case MULTIPLIER_ADDR: regs.multiplier_addr = value; break;
        case MULTIPLIER_SIZE_BYTES: regs.multiplier_size = value; break;
        case SHIFT_ADDR: regs.shift_addr = value; break;
        case SHIFT_SIZE_BYTES: regs.shift_size = value; break;
        case RICH_WINDOW_BASE: regs.rich_window_base = value; break;
        case RICH_WINDOW_SIZE: regs.rich_window_size = value; break;
        case CYCLE_COUNT: case BYTES_READ: case BYTES_WRITTEN: case LAST_ERROR: case CORE_ID:
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        default:
            return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }
        return tlm::TLM_OK_RESPONSE;
    }

    // ================================================== rich registers ====
    static bool is_param(std::uint32_t m) { return m >= MODEL_PARAM_LO && m <= MODEL_PARAM_HI && (m & 3u) == 0; }
    static bool is_ext(std::uint32_t m) { return m >= MODEL_EXT_LO && m < MODEL_EXT_END && (m & 3u) == 0; }
    bool is_neo(std::uint32_t m) const { return neo_on && m >= MODEL_NEO_BASE && m < MODEL_NEO_BASE + MODEL_NEO_SIZE; }

    tlm::tlm_response_status rich_read(std::uint32_t model, std::uint32_t& value) const
    {
        value = 0;
        if (model == MODEL_STATUS) {
            value = rich_status();
            return tlm::TLM_OK_RESPONSE;
        }
        if (model == MODEL_RETIRED) {
            value = low32(retired);
            return tlm::TLM_OK_RESPONSE;
        }
        if (is_neo(model)) {
            const std::uint32_t off = model - MODEL_NEO_BASE;
            const bool idle = !rich_busy();
            if (off == NEO_CTRL_OFF)
                value = (neo_armed ? 1u : 0u) | ((neo_done > neo_cleared_ctrl && idle) ? 2u : 0u) | 0xC000'0000u;
            else if (off == NEO_IEN_OFF)
                value = neo_ien;
            else if (off == NEO_ISTATUS_OFF)
                value = (neo_done > neo_cleared_ist && idle) ? 1u : 0u;
            else if (off >= NEO_REG_OFF && off < NEO_REG_OFF + 4 * NEO_REG_COUNT && (off & 3u) == 0)
                value = neo_reg[(off - NEO_REG_OFF) / 4];
            else
                value = 0x1BAD'ADD2u;
            return tlm::TLM_OK_RESPONSE;
        }
        // Parameters, extension and push registers are write-only: read as 0.
        if ((model >= MODEL_LEGACY_LO_A && model <= MODEL_RETIRED) || is_param(model) || is_ext(model))
            return tlm::TLM_OK_RESPONSE;
        return tlm::TLM_ADDRESS_ERROR_RESPONSE;
    }

    tlm::tlm_response_status rich_write(std::uint32_t model, std::uint32_t value, sc_core::sc_time& delay)
    {
        if (is_neo(model)) {
            const std::uint32_t off = model - MODEL_NEO_BASE;
            if (off == NEO_CTRL_OFF) {
                neo_armed = (value & 1u) != 0;
                if (value & 2u) neo_cleared_ctrl = neo_done;
            } else if (off == NEO_IEN_OFF) {
                neo_ien = value & 1u;
            } else if (off == NEO_ISTATUS_OFF) {
                if (value & 1u) neo_cleared_ist = neo_done;
            } else if (off >= NEO_REG_OFF && off < NEO_REG_OFF + 4 * NEO_REG_COUNT && (off & 3u) == 0) {
                neo_reg[(off - NEO_REG_OFF) / 4] = value;
            }
            return tlm::TLM_OK_RESPONSE;
        }
        if (neo_on && model == MODEL_NEO_MARKER && value == 0) return tlm::TLM_OK_RESPONSE;  // staging flag (model ignores it)
        if (is_param(model)) {
            rich_par[(model - MODEL_PARAM_LO) / 4] = value;
            return tlm::TLM_OK_RESPONSE;
        }
        if (is_ext(model)) {
            const std::uint32_t i = (model - MODEL_EXT_LO) / 4;
            rich_ext[i] = value;
            rich_ext_set[i] = true;
            return tlm::TLM_OK_RESPONSE;
        }
        switch (model) {
        case MODEL_PUSH_A: return rich_push(value & 0xFFu, delay);
        case MODEL_PUSH_B:
            clear_ext();
            refuse(static_cast<std::uint32_t>(rich_error_code::lane_b), VP_ERROR_REFUSED, 0, error_code::invalid_operation);
            return tlm::TLM_OK_RESPONSE;
        case MODEL_LEGACY_HI_A:
            refuse(static_cast<std::uint32_t>(rich_error_code::legacy_64bit), VP_ERROR_REFUSED, 0,
                   error_code::invalid_operation);
            return tlm::TLM_OK_RESPONSE;
        case MODEL_LEGACY_LO_A: case MODEL_LEGACY_LO_B: case MODEL_LEGACY_HI_B: case MODEL_RETIRED:
            return tlm::TLM_OK_RESPONSE;
        case MODEL_STATUS:
            rich_sticky = false;
            rich_err_code = 0;
            return tlm::TLM_OK_RESPONSE;
        default:
            return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        }
    }

    void clear_ext()
    {
        rich_ext.fill(0);
        rich_ext_set.fill(false);
    }
    void refuse(std::uint32_t rich_code, std::uint32_t vp_bits, std::uint32_t info, error_code bank_code)
    {
        ++st.pushes_refused;
        rich_error(rich_code, vp_bits, info, bank_code);
    }

    // Window offset of a CPU address register value (see kInvalidOffset).
    static std::uint32_t rebase(std::uint32_t value, std::uint32_t base, std::uint32_t size)
    {
        if (value == 0u || value < base || value - base >= size) return kInvalidOffset;
        return value - base;
    }

    // Regions of one decoded instruction (the rules of the drop's
    // core_rtl/npu_top.h bounds_check, from the model's own LayerInstr).
    // False: an operation outside the FX1 ABI.
    static bool regions_of(const has::LayerInstr& li, std::vector<Region>& reads, std::vector<Region>& writes)
    {
        auto add = [](std::vector<Region>& v, std::uint64_t off, std::uint64_t bytes) {
            if (bytes) v.push_back({off, bytes});
        };
        if (li.opcode == 0x12) {
            const auto& g = li.geom;
            const std::uint64_t in_c = static_cast<std::uint64_t>(std::max(li.in_c, 0));
            const std::uint64_t cout = static_cast<std::uint64_t>(std::max(g.cout, 0));
            const std::uint64_t K = in_c * static_cast<std::uint64_t>(std::max(g.kh, 0)) * static_cast<std::uint64_t>(std::max(g.kw, 0));
            const std::uint64_t tc = static_cast<std::uint64_t>(std::max(g.cout_t, 0));
            const std::uint64_t wch = (g.pad_tail && tc) ? (cout + tc - 1) / tc * tc : cout;
            const std::uint64_t out = cout * static_cast<std::uint64_t>(std::max(g.oh, 0)) * static_cast<std::uint64_t>(std::max(g.ow, 0));
            add(reads, li.in_addr, in_c * static_cast<std::uint64_t>(std::max(li.in_h, 0)) * static_cast<std::uint64_t>(std::max(li.in_w, 0)));
            add(reads, li.w_addr, wch * K);
            add(reads, li.bias_addr, 4 * cout);
            if (li.per_channel) {
                add(reads, li.scale_addr, 4 * cout);
                add(reads, li.shift_addr, 4 * cout);
            }
            if (li.act == 2 || li.act == 3) add(reads, li.lut_addr, 256);
            add(reads, li.out_addr, out);   // staged too: a partial write keeps the other bytes
            add(writes, li.out_addr, out);
            return true;
        }
        if (li.opcode == 0x15 && li.mode == 0) {
            add(reads, li.a_addr, li.n);
            add(reads, li.b_addr, li.n);
            add(reads, li.out_addr, li.n);
            add(writes, li.out_addr, li.n);
            return true;
        }
        if (li.opcode == 0x15 && (li.mode == 1 || li.mode == 5)) {
            const std::uint64_t c = static_cast<std::uint64_t>(std::max(li.c, 0));
            const std::int64_t h = li.h, w = li.w, k = li.pool_k, p = li.pool_p, s = li.pool_s > 0 ? li.pool_s : 1;
            add(reads, li.a_addr, c * static_cast<std::uint64_t>(std::max<std::int64_t>(h, 0)) * static_cast<std::uint64_t>(std::max<std::int64_t>(w, 0)));
            if (k > 0 && h + 2 * p >= k && w + 2 * p >= k) {
                const std::uint64_t oh = static_cast<std::uint64_t>((h + 2 * p - k) / s + 1);
                const std::uint64_t ow = static_cast<std::uint64_t>((w + 2 * p - k) / s + 1);
                add(reads, li.out_addr, c * oh * ow);
                add(writes, li.out_addr, c * oh * ow);
            }
            return true;
        }
        return false;
    }

    // Merge byte ranges and round them out to whole DMA chunks inside the window.
    std::vector<Region> merge_reads(std::vector<Region> v, std::uint64_t window) const
    {
        const std::uint64_t c = opt.dma_chunk_bytes;
        for (auto& r : v) {
            const std::uint64_t lo = r.off / c * c, hi = std::min(window, (r.off + r.bytes + c - 1) / c * c);
            r.off = lo;
            r.bytes = hi - lo;
        }
        std::sort(v.begin(), v.end(), [](const Region& a, const Region& b) { return a.off < b.off; });
        std::vector<Region> out;
        for (const auto& r : v) {
            if (!out.empty() && r.off <= out.back().off + out.back().bytes)
                out.back().bytes = std::max(out.back().off + out.back().bytes, r.off + r.bytes) - out.back().off;
            else
                out.push_back(r);
        }
        return out;
    }

    // Lane-A push (or doorbell). Validation first: a refused push never waits.
    tlm::tlm_response_status rich_push(std::uint32_t opcode, sc_core::sc_time& delay)
    {
        const bool bell = neo_on && opcode == NEO_DOORBELL_OPCODE;
        auto done_push = [&] {
            if (!bell) clear_ext();  // one-shot extension bank (the model clears it at every lane-A push)
        };
        if (!owner.reset_n.read()) {
            done_push();
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        }
        if (native_busy()) {
            done_push();
            refuse(RICH_ERR_VP_BUSY, VP_ERROR_BUSY, opcode, error_code::busy);
            return tlm::TLM_OK_RESPONSE;
        }
        Entry e;
        e.neo = bell;
        e.opcode = opcode;
        e.win_base = regs.rich_window_base;
        e.win_size = regs.rich_window_size;
        if (e.win_size == 0 || e.win_size > opt.max_window || !in_ram(e.win_base, e.win_size)) {
            done_push();
            refuse(RICH_ERR_VP_BAD_ADDR, VP_ERROR_BAD_ADDR, e.win_base, error_code::invalid_address);
            return tlm::TLM_OK_RESPONSE;
        }
        if (!bell && opcode != RICH_OPCODE_GEMM_FUSED && opcode != RICH_OPCODE_ELEM_WISE) {
            done_push();  // SET_NSPLIT, FUSED_ATTN, LAYERNORM, DMA_TENSOR: outside the FX1 ABI
            refuse(RICH_ERR_VP_UNSUPPORTED, VP_ERROR_UNSUPPORTED, opcode, error_code::invalid_operation);
            return tlm::TLM_OK_RESPONSE;
        }
        e.par = rich_par;
        e.ext = rich_ext;
        e.ext_set = rich_ext_set;
        for (std::uint32_t a : kParamAddressRegs) {
            auto& v = e.par[(a - MODEL_PARAM_LO) / 4];
            v = rebase(v, e.win_base, e.win_size);
        }
        for (std::uint32_t a : kExtAddressRegs) {
            const std::uint32_t i = (a - MODEL_EXT_LO) / 4;
            if (e.ext_set[i]) e.ext[i] = rebase(e.ext[i], e.win_base, e.win_size);
        }
        if (bell) {
            e.neo_reg = neo_reg;
            e.neo_ien = neo_ien;
            for (unsigned w : kNeoAddressWords) e.neo_reg[w] = rebase(e.neo_reg[w], e.win_base, e.win_size);
            if (!neo_armed) {
                refuse(static_cast<std::uint32_t>(rich_error_code::unsupported_opcode), VP_ERROR_REFUSED, opcode,
                       error_code::invalid_operation);  // the model refuses a doorbell without START
                return tlm::TLM_OK_RESPONSE;
            }
        }
        // Decode with a copy of the model's decoder: same refusal, same geometry.
        std::vector<has::LayerInstr> decoded;
        std::uint32_t model_code = 0;
        if (!decode(e, decoded, model_code)) {
            done_push();
            if (bell) neo_armed = false;
            refuse(model_code, VP_ERROR_REFUSED, opcode, error_code::invalid_operation);
            return tlm::TLM_OK_RESPONSE;
        }
        for (const auto& li : decoded) {
            if (!regions_of(li, e.reads, e.writes)) {
                done_push();
                if (bell) neo_armed = false;
                refuse(RICH_ERR_VP_UNSUPPORTED, VP_ERROR_UNSUPPORTED, (static_cast<std::uint32_t>(li.mode) << 8) | li.opcode,
                       error_code::invalid_operation);
                return tlm::TLM_OK_RESPONSE;
            }
        }
        for (const auto& r : e.reads) {
            if (r.off + r.bytes > e.win_size) {
                done_push();
                if (bell) neo_armed = false;
                refuse(RICH_ERR_VP_BAD_ADDR, VP_ERROR_BAD_ADDR,
                       r.off >= kInvalidOffset ? 0xFFFF'FFFFu : static_cast<std::uint32_t>(r.off), error_code::invalid_address);
                return tlm::TLM_OK_RESPONSE;
            }
        }
        e.reads = merge_reads(e.reads, e.win_size);
        done_push();
        if (bell) neo_armed = false;  // the doorbell consumes START (has_host_neo_port.h)

        // Bounded wait for a slot (review N0 R1): simulated time inside this
        // transport, independent of what the CPU does; never a late enqueue.
        if (queue.size() >= opt.queue_depth) {
            const sc_core::sc_time limit(static_cast<double>(push_wait_ns), sc_core::SC_NS);
            const sc_core::sc_time deadline = sc_core::sc_time_stamp() + delay + limit;
            sc_core::wait(delay);
            delay = sc_core::SC_ZERO_TIME;
            while (queue.size() >= opt.queue_depth && owner.reset_n.read() && sc_core::sc_time_stamp() < deadline) {
                sc_core::wait(deadline - sc_core::sc_time_stamp(), slot_event | owner.reset_n.negedge_event());
            }
            if (!owner.reset_n.read()) return tlm::TLM_COMMAND_ERROR_RESPONSE;
            if (queue.size() >= opt.queue_depth) {
                ++st.push_timeouts;
                --st.pushes_refused;  // counted as a timeout, not as a refusal
                refuse(static_cast<std::uint32_t>(rich_error_code::queue_overflow), VP_ERROR_PUSH_TIMEOUT,
                       low32(accepted + 1), error_code::busy);
                return tlm::TLM_GENERIC_ERROR_RESPONSE;
            }
        }
        if (!rich_busy()) {
            perf.reset();  // a new batch: counters of this batch only (npu_tlm behaviour)
            rich_error_in_batch = false;
        }
        e.seq = ++accepted;
        ++st.pushes_accepted;
        queue.push_back(std::move(e));
        regs.status = (regs.status & STATUS_ERROR) | STATUS_BUSY;
        regs.irq_status &= ~IRQ_DONE;
        update_irq();
        queue_event.notify(sc_core::SC_ZERO_TIME);
        return tlm::TLM_OK_RESPONSE;
    }

    // Replays entry `e` into a decoder (the probe at acceptance, the model at
    // execution): the same order as a CPU writes it.
    template <typename Sink>
    void replay(const Entry& e, Sink&& write) const
    {
        for (std::uint32_t i = 0; i < RICH_PARAM_COUNT; ++i) write(MODEL_PARAM_LO + 4 * i, e.par[i]);
        if (e.neo) {
            for (std::uint32_t i = 0; i < NEO_REG_COUNT; ++i) write(MODEL_NEO_BASE + NEO_REG_OFF + 4 * i, e.neo_reg[i]);
            write(MODEL_NEO_BASE + NEO_IEN_OFF, e.neo_ien);
            write(MODEL_NEO_BASE + NEO_CTRL_OFF, 1u);  // START (accepted only when the CPU had armed it)
            write(MODEL_PUSH_A, NEO_DOORBELL_OPCODE);
        } else {
            for (std::uint32_t i = 0; i < RICH_EXT_COUNT; ++i)
                if (e.ext_set[i]) write(MODEL_EXT_LO + 4 * i, e.ext[i]);
            write(MODEL_PUSH_A, e.opcode);
        }
    }

    bool decode(const Entry& e, std::vector<has::LayerInstr>& out, std::uint32_t& code)
    {
        has::MmioCompat& m = *probe;
        has::NeoPort& n = *probe_neo;
        m.soft_reset();
        n.soft_reset();
        const std::uint64_t errors = m.n_errors;
        replay(e, [&](std::uint32_t a, std::uint32_t v) {
            if (!n.handle_write(m, a, v)) m.write(a, v);
        });
        if (m.n_errors != errors) {
            code = static_cast<std::uint32_t>(m.last_error());
            return false;
        }
        has::LayerInstr li;
        while (m.pop(li)) {
            out.push_back(li);
            m.retire(li);
        }
        if (out.empty()) {
            code = static_cast<std::uint32_t>(rich_error_code::unsupported_opcode);
            return false;
        }
        return true;
    }

    // ABORT (VP_CMD bit 0): reachable only once the bus is free (review R1).
    void abort_all()
    {
        ++st.aborts;
        const std::size_t dropped = queue.size();
        // Native work: a GEMM-bank job (software_run_active) or a raw native
        // START (native_run_active). Both stop through native_abort; the native
        // worker soft-resets the core before it reports (review N1 R1).
        const bool native_work = software_run_active || native_run_active;
        const bool running = exec.has_value() || native_work || dma_active;
        abort_dropped += dropped;
        st.abort_dropped += dropped;
        queue.clear();
        slot_event.notify(sc_core::SC_ZERO_TIME);
        if (exec.has_value()) exec_aborted = true;
        if (native_work) native_abort = true;
        if (dropped || running) {
            vp_error |= VP_ERROR_ABORTED;
            vp_error_info = static_cast<std::uint32_t>(dropped);
            rich_err_code = RICH_ERR_VP_ABORTED;
            rich_sticky = true;
            abort_report_pending = true;
            // Reported by the worker that stops the work, at once when nothing runs.
            if (!exec.has_value() && !native_work) report_abort();
        }
        work_event.notify(sc_core::SC_ZERO_TIME);
        queue_event.notify(sc_core::SC_ZERO_TIME);
    }
    void report_abort()
    {
        if (!abort_report_pending) return;
        abort_report_pending = false;
        regs.status = (regs.status & ~(STATUS_BUSY | STATUS_DONE)) | STATUS_ERROR | STATUS_IDLE;
        regs.irq_status |= IRQ_ERROR;
        regs.last_error = static_cast<std::uint32_t>(error_code::reset_aborted);
        rich_error_in_batch = false;
        update_irq();
        rich_idle_event.notify(sc_core::SC_ZERO_TIME);
    }

    // ========================================================== DMA ========
    // One transaction per chunk aligned to the chunk size, so the DDR port is
    // never held for a whole window and the CPU's accesses interleave.
    template <typename Stop>
    bool dma(bool write, std::uint64_t address, std::uint8_t* data, std::uint64_t size, Stop&& stop,
             std::uint64_t* done_bytes = nullptr)
    {
        while (dma_active) sc_core::wait(dma_free_event);
        dma_active = true;
        bool ok = true;
        std::uint64_t pos = 0;
        while (pos < size) {
            if (stop()) {
                ok = false;
                break;
            }
            const std::uint64_t a = address + pos;
            const std::uint64_t chunk = std::min<std::uint64_t>(size - pos, opt.dma_chunk_bytes - (a % opt.dma_chunk_bytes));
            tlm::tlm_generic_payload trans;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            trans.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
            trans.set_address(a);
            trans.set_data_ptr(data + pos);
            trans.set_data_length(static_cast<unsigned>(chunk));
            trans.set_streaming_width(static_cast<unsigned>(chunk));
            trans.set_byte_enable_ptr(nullptr);
            trans.set_dmi_allowed(false);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            owner.master_socket->b_transport(trans, delay);
            if (delay != sc_core::SC_ZERO_TIME) sc_core::wait(delay);
            ++st.dma_transactions;
            if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
                ok = false;
                break;
            }
            (write ? st.dma_write_bytes : st.dma_read_bytes) += chunk;
            pos += chunk;
        }
        if (done_bytes) *done_bytes = pos;
        dma_active = false;
        dma_free_event.notify(sc_core::SC_ZERO_TIME);
        return ok;
    }

    // ===================================================== rich worker =====
    // HasNpuTop staging area right above the window (moved only while the
    // model is idle; the vector grows to the largest window seen).
    void set_staging(std::uint32_t window)
    {
        const std::uint32_t base = (window + 4095u) & ~4095u;
        if (staging_base == base && image.size() >= std::size_t{base} + kModelStagingBytes) return;
        // One allocation of the final size (window + model staging), no doubling.
        if (image.capacity() < std::size_t{base} + kModelStagingBytes) image.reserve(std::size_t{base} + kModelStagingBytes);
        if (image.size() < base) image.resize(base, 0);
        rtl->set_staging_base(base);
        staging_base = base;
        image_peak = std::max(image_peak, image.size());
    }

    enum class run_result { ok, refused, model_error, aborted, reset };

    run_result run_on_model(const Entry& e, std::uint64_t generation)
    {
        clock_hold hold(*this, rich_gclk);
        ++st.model_dispatches;
        const std::uint64_t errors0 = rtl->mmio.n_errors;
        const std::uint32_t retired0 = rtl->mmio.read(MODEL_RETIRED);
        const ModelErrors me0 = model_errors();
        const ModelCounters mc0 = model_counters();
        // Writes as the model's host port applies them (has_npu_top.h host_port).
        replay(e, [&](std::uint32_t a, std::uint32_t v) {
            if (!rtl->neo.handle_write(rtl->mmio, a, v)) rtl->mmio.write(a, v);
        });
        const sc_core::sc_time poll = opt.clock_period * kPollCycles;
        for (;;) {
            if (rtl->mmio.n_errors != errors0) {
                ++st.model_refusals;
                rtl->mmio.write(MODEL_STATUS, 0);  // clear the model's sticky error; the clone reports it
                return run_result::refused;
            }
            if (rtl->mmio.read(MODEL_RETIRED) != retired0 && !rtl->mmio.busy()) break;
            sc_core::wait(poll);
        }
        // A reset while the model ran: its counters belong to the old epoch.
        if (generation != reset_generation) return run_result::reset;
        add_model_counters(mc0, model_counters());
        if (model_errors() != me0) return run_result::model_error;
        if (!owner.reset_n.read()) return run_result::reset;
        return exec_aborted ? run_result::aborted : run_result::ok;
    }

    void rich_thread()
    {
        for (;;) {
            // Reset: hold the model in reset while the platform reset is low.
            if (!owner.reset_n.read()) {
                rich_rstn.write(false);
                {
                    clock_hold hold(*this, rich_gclk);
                    sc_core::wait(opt.clock_period * 4);
                }
                sc_core::wait(owner.reset_n.posedge_event());
            }
            if (!rich_rstn.read()) {
                clock_hold hold(*this, rich_gclk);
                sc_core::wait(opt.clock_period * 2);
                rich_rstn.write(true);
                sc_core::wait(opt.clock_period * 2);
            }
            while (queue.empty() && owner.reset_n.read() && rich_rstn.read())
                sc_core::wait(queue_event | owner.reset_n.negedge_event());
            if (!owner.reset_n.read() || !rich_rstn.read()) continue;

            exec = std::move(queue.front());
            queue.pop_front();
            const std::uint64_t generation = reset_generation;
            exec_aborted = false;
            slot_event.notify(sc_core::SC_ZERO_TIME);  // credit returned when staging starts
            Entry& e = *exec;

            set_staging(e.win_size);
            auto stop = [this, generation] { return exec_aborted || !owner.reset_n.read() || generation != reset_generation; };
            bool ok = true;
            for (const auto& r : e.reads) {
                if (!dma(false, std::uint64_t{e.win_base} + r.off, image.data() + r.off, r.bytes, stop)) {
                    ok = false;
                    break;
                }
            }
            // The last staging transaction may return after an ABORT or a reset
            // (dma() yields): such an entry is never dispatched to the model.
            if (ok && stop()) ok = false;
            run_result result = run_result::aborted;
            if (!ok) {
                if (!stop()) {
                    rich_error(RICH_ERR_VP_DMA, VP_ERROR_DMA, low32(e.seq), error_code::dma_read);
                    result = run_result::model_error;  // reported already
                }
            } else {
                result = run_on_model(e, generation);
            }
            if (generation != reset_generation) {  // reset during the entry: no copy-back, no report
                exec.reset();
                exec_aborted = false;
                continue;
            }
            switch (result) {
            case run_result::ok: {
                copyback_active = true;
                std::uint64_t written = 0;
                bool wrote = true;
                for (const auto& r : e.writes) {
                    std::uint64_t n = 0;
                    if (!dma(true, std::uint64_t{e.win_base} + r.off, image.data() + r.off, r.bytes, stop, &n)) {
                        written += n;
                        wrote = false;
                        break;
                    }
                    written += n;
                }
                copyback_active = false;
                // dma() yields: a reset while a copy-back transaction was in
                // flight leaves the new epoch's RETIRED, neo DONE, error and info
                // untouched (review N1 R2). The transaction itself still
                // completed: one issued before the reset is drained, not recalled.
                if (generation != reset_generation) {
                    exec.reset();
                    exec_aborted = false;
                    continue;
                }
                if (wrote) {
                    ++retired;
                    ++st.retired;
                    if (e.neo) ++neo_done;
                } else if (stop()) {
                    vp_error_info = low32(written);  // bytes of the aborted copy-back that reached DDR
                } else {
                    rich_error(RICH_ERR_VP_DMA, VP_ERROR_DMA, low32(e.seq), error_code::dma_write);
                }
                break;
            }
            case run_result::refused:
                rich_error(static_cast<std::uint32_t>(rtl->mmio.last_error()) ? static_cast<std::uint32_t>(rtl->mmio.last_error())
                                                                              : RICH_ERR_VP_MODEL,
                           VP_ERROR_REFUSED, low32(e.seq), error_code::invalid_operation);
                break;
            case run_result::model_error:
                if (ok) rich_error(RICH_ERR_VP_MODEL, VP_ERROR_MODEL, low32(e.seq), error_code::core_deadlock);
                break;
            case run_result::aborted:
            case run_result::reset:
                break;
            }
            exec.reset();
            exec_aborted = false;
            if (generation != reset_generation) continue;  // a reset came in between: its state stands
            if (queue.empty()) {
                if (abort_report_pending) report_abort();
                else rich_batch_end();
            }
        }
    }

    // ============================================ native path (npu_tlm) ====
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
        if (!wait_native_clock(2)) return false;
        core_soft_reset.write(false);
        return wait_native_clock(2);
    }
    bool core_host_write(std::uint32_t address, const sauria::host_data_t& data, const sauria::host_mask_t& mask)
    {
        if (!wait_native_clock()) return false;
        host_addr.write(address);
        host_wdata.write(data);
        host_wmask.write(mask);
        host_rden.write(false);
        host_wren.write(true);
        if (!wait_native_clock()) return false;
        host_wren.write(false);
        return wait_native_clock();
    }
    bool core_host_read(std::uint32_t address, sauria::host_data_t& data)
    {
        if (!wait_native_clock()) return false;
        host_addr.write(address);
        host_wren.write(false);
        host_rden.write(true);
        if (!wait_native_clock(2)) return false;
        host_rden.write(false);
        if (!wait_native_clock()) return false;
        data = host_rdata.read();
        return true;
    }
    static bool is_sram_address(std::uint32_t a)
    {
        const std::uint32_t r = a & sauria::SAURIA_MEM_ADDR_MASK;
        return r == sauria::SRAMA_OFFSET || r == sauria::SRAMB_OFFSET || r == sauria::SRAMC_OFFSET;
    }
    static bool is_rows_active_address(std::uint32_t a)
    {
        return (a & sauria::SAURIA_MEM_ADDR_MASK) == sauria::CFG_REGS_OFFSET &&
               (a & ~sauria::SAURIA_MEM_ADDR_MASK) == sauria::CFG_ACT_OFFSET;
    }
    static bool translate_table_alias(std::uint32_t vp, std::uint32_t& model)
    {
        for (const auto& r : kTableAliases)
            if (vp >= r.alias_base && vp < r.alias_base + r.size) {
                model = r.model_base + (vp - r.alias_base);
                return true;
            }
        model = vp;
        return false;
    }
    static bool is_packed_byte_address(std::uint32_t a)
    {
        const std::uint32_t r = a & 0x00FF'0000u;
        return r == 0x0014'0000u || r == 0x0016'0000u || r == 0x0020'0000u || r == 0x0021'0000u || r == 0x0023'0000u ||
               r == 0x0024'0000u;
    }
    static bool is_packed_half_address(std::uint32_t a)
    {
        const std::uint32_t r = a & 0x00FF'0000u;
        return r == 0x0022'0000u || r == 0x0025'0000u;
    }
    void update_native_sideband(std::uint32_t address, std::uint32_t value)
    {
        const std::uint32_t region = address & sauria::SAURIA_MEM_ADDR_MASK;
        const std::uint32_t local = address & ~sauria::SAURIA_MEM_ADDR_MASK;
        if (region != sauria::CFG_REGS_OFFSET) return;
        if (local == sauria::NCONTEXTS || local == sauria::CFG_CON_OFFSET + 0x0C) {
            native_total_contexts = value;
            total_contexts.write(value);
        }
        if (local == sauria::TILE_C || local == sauria::IN_C || local == sauria::WEI_KSTEP) {
            native_mvm_k = value;
            mvm_k.write(value);
        }
    }

    tlm::tlm_response_status native_write(std::uint32_t address, std::uint32_t value)
    {
        if (!owner.reset_n.read() || !core_rst_n.read() || software_run_active) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        std::uint32_t model_address = address;
        const bool is_alias = translate_table_alias(address, model_address);
        const std::uint32_t local = model_address & ~sauria::SAURIA_MEM_ADDR_MASK;
        const std::uint32_t region = model_address & sauria::SAURIA_MEM_ADDR_MASK;
        const bool native_start = !is_alias && region == sauria::CFG_REGS_OFFSET && local == 0u && (value & 0xFFu) != 0u;
        if (native_start && (native_run_active || rich_busy())) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        if (native_start) perf.reset();

        sauria::host_data_t data;
        sauria::host_mask_t mask;
        data.data.fill(0.0);
        mask.data.fill(false);
        if (is_sram_address(model_address) || is_rows_active_address(model_address)) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) {
                data[lane] = static_cast<double>(static_cast<std::int8_t>((value >> (lane * 8)) & 0xFFu));
                mask[lane] = true;
            }
        } else if (is_packed_byte_address(model_address)) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) {
                data[lane] = static_cast<double>((value >> (lane * 8)) & 0xFFu);
                mask[lane] = true;
            }
        } else if (is_packed_half_address(model_address)) {
            const std::uint32_t first = ((model_address & 0xFFFFu) >> 1) & 3u;
            data[first] = static_cast<double>(value & 0xFFFFu);
            mask[first] = true;
            if (first + 1u < 4u) {
                data[first + 1u] = static_cast<double>((value >> 16) & 0xFFFFu);
                mask[first + 1u] = true;
            }
        } else if (!is_alias && region == sauria::CFG_REGS_OFFSET && local == 0u) {
            data[0] = static_cast<double>(value & 0xFFu);
            mask[0] = true;
            data[2] = static_cast<double>((value >> 16) & 0xFFu);
            mask[2] = ((value >> 16) & 0xFFu) != 0u;
        } else {
            data[0] = static_cast<double>(value);
            mask[0] = true;
        }
        update_native_sideband(model_address, value);
        if (native_start) {
            buffer_select.write(sc_dt::sc_bv<3>("111"));
            total_contexts.write(native_total_contexts);
            mvm_k.write(native_mvm_k);
        }
        if (!core_host_write(model_address, data, mask)) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        if (native_start) {
            native_run_active = true;
            native_abort = false;
            native_run_cycles = 0;
            regs.status = STATUS_BUSY;
            regs.irq_status = 0;
            regs.last_error = static_cast<std::uint32_t>(error_code::none);
            regs.cycle_count = 0;
            update_irq();
        }
        return tlm::TLM_OK_RESPONSE;
    }

    tlm::tlm_response_status native_read(std::uint32_t address, std::uint32_t& value)
    {
        if (read_perf(address, value)) return tlm::TLM_OK_RESPONSE;
        if (!owner.reset_n.read() || !core_rst_n.read() || software_run_active) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        std::uint32_t model_address = address;
        translate_table_alias(address, model_address);
        sauria::host_data_t data;
        if (!core_host_read(model_address, data)) return tlm::TLM_COMMAND_ERROR_RESPONSE;
        value = 0;
        if (is_sram_address(model_address) || is_rows_active_address(model_address) || is_packed_byte_address(model_address)) {
            for (std::uint32_t lane = 0; lane < 4; ++lane) {
                const auto v = static_cast<std::int32_t>(std::llround(data[lane]));
                value |= (static_cast<std::uint32_t>(v) & 0xFFu) << (lane * 8);
            }
        } else if (is_packed_half_address(model_address)) {
            const std::uint32_t first = ((model_address & 0xFFFFu) >> 1) & 3u;
            value = static_cast<std::uint32_t>(std::llround(data[first])) & 0xFFFFu;
            if (first + 1u < 4u) value |= (static_cast<std::uint32_t>(std::llround(data[first + 1u])) & 0xFFFFu) << 16;
        } else if (model_address == sauria::CFG_REGS_OFFSET) {
            value = static_cast<std::uint32_t>(std::llround(data[0])) & 0xFFu;
            value |= (static_cast<std::uint32_t>(std::llround(data[2])) & 0xFFu) << 16;
        } else {
            value = static_cast<std::uint32_t>(std::llround(data[0]));
        }
        return tlm::TLM_OK_RESPONSE;
    }

    tlm::tlm_response_status submit_native(native_command command, std::uint32_t address, std::uint32_t& value)
    {
        if (command == native_command::read && read_perf(address, value)) return tlm::TLM_OK_RESPONSE;
        if (!owner.reset_n.read() || !core_rst_n.read() || software_run_active || native_active)
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
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
        if (command == native_command::read && native_status == tlm::TLM_OK_RESPONSE) value = native_read_value;
        native_active = false;
        return native_status;
    }
    void abort_native_request()
    {
        if (!native_pending) return;
        native_status = tlm::TLM_COMMAND_ERROR_RESPONSE;
        native_cmd = native_command::none;
        native_pending = false;
        native_done_event.notify(sc_core::SC_ZERO_TIME);
    }
    void service_native_request()
    {
        if (!native_pending) return;
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
    void service_native_run()
    {
        // An ABORT accepted while the run was active wins over a completion or
        // a timeout the worker has not seen yet: the abort branch reports.
        if (!native_run_active || native_abort) return;
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

    bool write_scalar(std::uint32_t address, std::uint32_t value)
    {
        sauria::host_data_t data;
        sauria::host_mask_t mask;
        data[0] = static_cast<float>(value);
        mask[0] = true;
        return core_host_write(address, data, mask);
    }

    bool program_core_registers(const sauria::SauriaLayerDesc& desc, const sauria::SauriaTarget& target)
    {
        std::uint64_t f[sauria::F_CFG_COUNT];
        sauria::sauria_compute_core_fields(desc, target, f);
        if (!write_scalar(sauria::CFG_REGS_OFFSET | sauria::CFG_PROFILE_ADDR, sauria::PROFILE_V1_SAURIA) ||
            !write_scalar(sauria::CFG_REGS_OFFSET | (sauria::CFG_CON_OFFSET + 0x00), regs.k_dimension) ||
            !write_scalar(sauria::CFG_REGS_OFFSET | (sauria::CFG_CON_OFFSET + 0x04),
                          static_cast<std::uint32_t>(f[sauria::F_CFG_ACT_REPS])) ||
            !write_scalar(sauria::CFG_REGS_OFFSET | (sauria::CFG_CON_OFFSET + 0x08),
                          static_cast<std::uint32_t>(f[sauria::F_CFG_WEI_REPS])))
            return false;
        sauria::host_data_t rows;
        sauria::host_mask_t full_mask;
        full_mask.data.fill(true);
        for (unsigned i = 0; i < 4; ++i) rows[i] = static_cast<float>((regs.rows_active >> (i * 8)) & 0xFFu);
        if (!core_host_write(sauria::CFG_REGS_OFFSET | (sauria::CFG_ACT_OFFSET + 0x00), rows, full_mask)) return false;
        const auto wr = [this](std::uint32_t a, std::uint64_t v) {
            return write_scalar(sauria::CFG_REGS_OFFSET | a, static_cast<std::uint32_t>(v));
        };
        return wr(sauria::CFG_ACT_OFFSET + 0x04, regs.k_dimension) && wr(sauria::CFG_ACT_OFFSET + 0x08, f[sauria::F_CFG_XSTEP]) &&
               wr(sauria::CFG_ACT_OFFSET + 0x0C, f[sauria::F_CFG_XLIM]) && wr(sauria::CFG_ACT_OFFSET + 0x10, f[sauria::F_CFG_XSTEP]) &&
               wr(sauria::CFG_ACT_OFFSET + 0x28, f[sauria::F_CFG_DIL_PAT]) && wr(sauria::ACT_XLIM, f[sauria::F_CFG_XLIM]) &&
               wr(sauria::ACT_XSTEP, f[sauria::F_CFG_XSTEP]) && wr(sauria::ACT_YLIM, f[sauria::F_CFG_YLIM]) &&
               wr(sauria::ACT_YSTEP, f[sauria::F_CFG_YSTEP]) && wr(sauria::ACT_CHLIM, f[sauria::F_CFG_CHLIM]) &&
               wr(sauria::ACT_CHSTEP, f[sauria::F_CFG_CHSTEP]) && wr(sauria::ACT_TIL_XLIM, f[sauria::F_CFG_TIL_XLIM]) &&
               wr(sauria::ACT_TIL_XSTEP, f[sauria::F_CFG_TIL_XSTEP]) && wr(sauria::ACT_TIL_YLIM, f[sauria::F_CFG_TIL_YLIM]) &&
               wr(sauria::ACT_TIL_YSTEP, f[sauria::F_CFG_TIL_YSTEP]) && wr(sauria::CFG_WEI_OFFSET + 0x04, f[sauria::F_CFG_WLIM]) &&
               wr(sauria::CFG_WEI_OFFSET + 0x08, f[sauria::F_CFG_WSTEP]) && wr(sauria::WEI_WLIM, f[sauria::F_CFG_WLIM]) &&
               wr(sauria::WEI_WSTEP, f[sauria::F_CFG_WSTEP]) && wr(sauria::WEI_KLIM, f[sauria::F_CFG_KLIM]) &&
               wr(sauria::WEI_KSTEP, f[sauria::F_CFG_KSTEP]) && wr(sauria::WEI_TIL_XLIM, f[sauria::F_CFG_TIL_KLIM]) &&
               wr(sauria::WEI_TIL_XSTEP, f[sauria::F_CFG_TIL_KSTEP]) && wr(sauria::WEI_COLS_ACTIVE, f[sauria::F_CFG_COLS_ACTIVE]) &&
               wr(sauria::WEI_WALIGNED, f[sauria::F_CFG_WALIGNED]) && wr(sauria::NCONTEXTS, f[sauria::F_CFG_NCONTEXTS]) &&
               wr(sauria::CFG_OUT_OFFSET + 0x04, f[sauria::F_CFG_CXLIM]) && wr(sauria::CFG_OUT_OFFSET + 0x08, f[sauria::F_CFG_CXSTEP]) &&
               wr(sauria::CFG_OUT_OFFSET + 0x0C, f[sauria::F_CFG_CKLIM]) && wr(sauria::CFG_OUT_OFFSET + 0x10, f[sauria::F_CFG_CKSTEP]) &&
               wr(sauria::TIL_CYLIM, f[sauria::F_CFG_TIL_CYLIM]) && wr(sauria::TIL_CYSTEP, f[sauria::F_CFG_TIL_CYSTEP]) &&
               wr(sauria::TIL_CKLIM, f[sauria::F_CFG_TIL_CKLIM]) && wr(sauria::TIL_CKSTEP, f[sauria::F_CFG_TIL_CKSTEP]) &&
               wr(sauria::INACTIVE_COLS, f[sauria::F_CFG_INACTIVE_COLS]) && wr(sauria::PRELOAD_EN, 0u) &&
               wr(sauria::CFG_ACT_BASE_ADDR, 0u) && wr(sauria::CFG_WEI_BASE_ADDR, 0u) && wr(sauria::CFG_OUT_BASE_ADDR, 0u);
    }

    error_code validate_job(std::uint32_t& source_span) const
    {
        if (regs.width != kSoftwareCols || regs.height != kSoftwareRows || regs.k_dimension == 0u ||
            regs.k_dimension > kSoftwareMaxK)
            return error_code::invalid_dimensions;
        if (regs.format != FORMAT_INT8_INT8_INT32) return error_code::invalid_format;
        if (regs.op_mode != OP_GEMM) return error_code::invalid_operation;
        const std::uint32_t stride = regs.src_stride == 0u ? regs.k_dimension : regs.src_stride;
        if (stride < regs.k_dimension) return error_code::invalid_size;
        const std::uint64_t span64 = static_cast<std::uint64_t>(stride) * (kSoftwareRows - 1) + regs.k_dimension;
        if (span64 > std::numeric_limits<std::uint32_t>::max()) return error_code::invalid_size;
        source_span = static_cast<std::uint32_t>(span64);
        const std::uint32_t weights_need = regs.k_dimension * kSoftwareCols;
        const std::uint32_t dst_need = kSoftwareRows * kSoftwareCols * sizeof(std::int32_t);
        if (regs.src_size < source_span || regs.weights_size < weights_need || regs.dst_size < dst_need)
            return error_code::invalid_size;
        if ((regs.dst_addr & 0x3u) != 0u || !in_ram(regs.src_addr, source_span) || !in_ram(regs.weights_addr, weights_need) ||
            !in_ram(regs.dst_addr, dst_need))
            return error_code::invalid_address;
        return error_code::none;
    }

    // CDC 32x32 GEMM bank job (npu_tlm execute_job), with ABORT checks between steps.
    error_code execute_job()
    {
        perf.reset();
        std::uint32_t source_span = 0;
        error_code result = validate_job(source_span);
        if (result != error_code::none) return result;
        const std::uint32_t stride = regs.src_stride == 0u ? regs.k_dimension : regs.src_stride;
        const std::uint32_t weights_need = regs.k_dimension * kSoftwareCols;
        auto stop = [this] { return native_abort || !owner.reset_n.read(); };
        auto aborted = [&] { return error_code::reset_aborted; };
        std::vector<std::uint8_t> activations(source_span), weights(weights_need);
        if (!dma(false, regs.src_addr, activations.data(), source_span, stop)) return stop() ? aborted() : error_code::dma_read;
        if (!dma(false, regs.weights_addr, weights.data(), weights_need, stop)) return stop() ? aborted() : error_code::dma_read;
        regs.bytes_read = source_span + weights_need;
        if (!pulse_core_soft_reset() || stop()) return aborted();
        threshold.write(bits_to_float(regs.threshold_bits));
        buffer_select.write(sc_dt::sc_bv<3>("000"));
        const sauria::SauriaTarget* target = sauria::sauria_find_target("int8_32x32");
        if (target == nullptr) return error_code::invalid_format;
        const sauria::SauriaLayerDesc desc =
            make_gemm_as_conv1x1_desc(kSoftwareRows, kSoftwareCols, regs.k_dimension, kSoftwareRows, kSoftwareCols);
        std::vector<double> native_a(static_cast<std::size_t>(regs.k_dimension) * kSoftwareRows);
        std::vector<double> native_b(static_cast<std::size_t>(kSoftwareCols) * regs.k_dimension);
        std::vector<double> native_c(static_cast<std::size_t>(kSoftwareCols) * kSoftwareRows, 0.0);
        for (std::uint32_t k = 0; k < regs.k_dimension; ++k)
            for (std::uint32_t row = 0; row < kSoftwareRows; ++row)
                native_a[static_cast<std::size_t>(k) * kSoftwareRows + row] =
                    static_cast<std::int8_t>(activations[static_cast<std::size_t>(row) * stride + k]);
        for (std::uint32_t col = 0; col < kSoftwareCols; ++col)
            for (std::uint32_t k = 0; k < regs.k_dimension; ++k)
                native_b[static_cast<std::size_t>(col) * regs.k_dimension + k] =
                    static_cast<std::int8_t>(weights[static_cast<std::size_t>(k) * kSoftwareCols + col]);
        const sauria::SauriaRunInputs inputs =
            sauria::sauria_prepare(*target, desc, native_a.data(), regs.k_dimension, 1, kSoftwareRows, native_b.data(),
                                   kSoftwareCols, regs.k_dimension, 1, 1, native_c.data(), kSoftwareCols, 1, kSoftwareRows);
        mvm_k.write(regs.k_dimension);
        total_contexts.write(1u);
        if (!program_core_registers(desc, *target) || stop()) return aborted();
        sauria::host_mask_t full_mask;
        full_mask.data.fill(true);
        const auto preload_int8 = [&](std::uint32_t sram_offset, std::uint32_t begin, std::uint32_t size) {
            std::uint32_t loaded = 0;
            for (std::uint32_t phys = 0; loaded < size; ++phys) {
                for (std::uint32_t sw = 0; sw < kSoftwareRowSubwords && loaded < size; ++sw) {
                    sauria::host_data_t data;
                    data.data.fill(0.0f);
                    for (std::uint32_t lane = 0; lane < 4 && loaded < size; ++lane, ++loaded)
                        data[lane] = static_cast<float>(static_cast<std::int8_t>(inputs.initial_dram[begin + loaded]));
                    const std::uint32_t address =
                        sram_offset == sauria::SRAMA_OFFSET ? make_sram_a_addr(phys, sw) : make_sram_b_addr(phys, sw);
                    if (!core_host_write(address, data, full_mask)) return false;
                }
            }
            return true;
        };
        if (!preload_int8(sauria::SRAMA_OFFSET, inputs.A_off, inputs.B_off - inputs.A_off) ||
            !preload_int8(sauria::SRAMB_OFFSET, inputs.B_off, inputs.C_off - inputs.B_off) || stop())
            return aborted();
        for (std::uint32_t col = 0; col < kSoftwareCols; ++col)
            for (std::uint32_t sw = 0; sw < kSoftwareRowSubwords; ++sw) {
                sauria::host_data_t data;
                data.data.fill(0.0f);
                if (!core_host_write(make_sram_c_addr(col * kSoftwareRows, sw), data, full_mask)) return aborted();
            }
        buffer_select.write(sc_dt::sc_bv<3>("111"));
        if (!wait_native_clock(2)) return aborted();
        core_start.write(true);
        if (!wait_native_clock()) return aborted();
        core_start.write(false);
        bool completed = false;
        for (std::uint32_t cycle = 0; cycle < kCoreTimeoutCycles; ++cycle) {
            if (!wait_native_clock() || native_abort) {
                pulse_core_soft_reset();
                return aborted();
            }
            regs.cycle_count = cycle + 1;
            if (core_deadlock.read()) return error_code::core_deadlock;
            if (core_done.read()) {
                completed = true;
                break;
            }
        }
        if (!completed) return error_code::core_timeout;
        buffer_select.write(sc_dt::sc_bv<3>("000"));
        if (!wait_native_clock(2)) return aborted();
        // The software-facing exact INT32 result (npu_tlm contract); the model
        // run above gives the timing and the counters.
        std::vector<std::uint8_t> output(kSoftwareRows * kSoftwareCols * sizeof(std::int32_t), 0);
        for (std::uint32_t row = 0; row < kSoftwareRows; ++row)
            for (std::uint32_t col = 0; col < kSoftwareCols; ++col) {
                std::int64_t acc = 0;
                for (std::uint32_t k = 0; k < regs.k_dimension; ++k)
                    acc += static_cast<std::int32_t>(static_cast<std::int8_t>(activations[static_cast<std::size_t>(row) * stride + k])) *
                           static_cast<std::int32_t>(static_cast<std::int8_t>(weights[static_cast<std::size_t>(k) * kSoftwareCols + col]));
                const std::int32_t v = clamp_i64(acc);
                std::memcpy(output.data() + (static_cast<std::size_t>(row) * kSoftwareCols + col) * sizeof(v), &v, sizeof(v));
            }
        if (stop()) return aborted();
        if (!dma(true, regs.dst_addr, output.data(), output.size(), stop)) return stop() ? aborted() : error_code::dma_write;
        regs.bytes_written = static_cast<std::uint32_t>(output.size());
        ++st.native_jobs;
        return error_code::none;
    }

    void reset_native_state()
    {
        abort_native_request();
        regs = register_file{};  // a reset returns every register to its reset value
        job_pending = false;
        soft_reset_pending = false;
        software_run_active = false;
        native_run_active = false;
        native_abort = false;
        native_run_cycles = 0;
        perf.reset();
        native_mvm_k = 0;
        native_total_contexts = 0;
        update_irq();
    }
    void reset_rich_state()
    {
        ++reset_generation;
        rich_par.fill(0);
        clear_ext();
        neo_reg.fill(0);
        neo_ien = 0;
        neo_armed = false;
        neo_done = neo_cleared_ctrl = neo_cleared_ist = 0;
        queue.clear();
        if (exec.has_value()) exec_aborted = true;
        retired = 0;
        accepted = 0;
        abort_dropped = 0;
        push_wait_ns = opt.push_wait_ns;
        rich_err_code = 0;
        rich_sticky = false;
        rich_error_in_batch = false;
        abort_report_pending = false;
        vp_error = 0;
        vp_error_info = 0;
        slot_event.notify(sc_core::SC_ZERO_TIME);
    }

    void native_thread()
    {
        core_rst_n.write(false);
        drive_idle_host();
        buffer_select.write(sc_dt::sc_bv<3>("000"));
        for (;;) {
            while (!owner.reset_n.read()) sc_core::wait(owner.reset_n.posedge_event());
            if (!wait_native_clock(2)) continue;
            core_rst_n.write(true);
            if (!wait_native_clock(2)) {
                core_rst_n.write(false);
                continue;
            }
            while (owner.reset_n.read()) {
                service_native_run();
                if (!job_pending && !soft_reset_pending && !native_pending && !native_run_active) {
                    sc_core::wait(work_event | owner.reset_n.negedge_event());
                    if (!owner.reset_n.read()) break;
                }
                if (soft_reset_pending) {
                    soft_reset_pending = false;
                    if (!pulse_core_soft_reset()) break;
                    reset_native_state();
                    reset_rich_state();
                    rich_rstn.write(false);  // the rich worker pulses the model reset
                    queue_event.notify(sc_core::SC_ZERO_TIME);
                    continue;
                }
                if (job_pending) {
                    job_pending = false;
                    const error_code result = execute_job();
                    if (!owner.reset_n.read()) break;
                    if (result == error_code::none) finish_success();
                    else finish_error(result);
                    native_abort = false;
                    if (result == error_code::reset_aborted) abort_report_pending = false;
                    continue;
                }
                if (native_pending) {
                    service_native_request();
                    continue;
                }
                if (native_run_active && native_abort) {
                    // Raw native START aborted: soft-reset the core, then report
                    // (STATUS ERROR, LAST_ERROR RESET_ABORTED, IRQ_STATUS ERROR).
                    // The abort is reported here only, never into the next job.
                    if (!pulse_core_soft_reset()) break;
                    native_run_active = false;
                    native_abort = false;
                    abort_report_pending = false;
                    ++st.native_aborts;
                    finish_error(error_code::reset_aborted);
                    continue;
                }
                if (native_run_active && !wait_native_clock()) break;
            }
            abort_native_request();
            core_rst_n.write(false);
            drive_idle_host();
            reset_native_state();
            reset_rich_state();
        }
    }
};

// ===========================================================================
Fx1Npu::Fx1Npu(sc_core::sc_module_name name, const NpuOptions& options)
    : sc_core::sc_module(name)
    , target_socket("target_socket")
    , master_socket("master_socket")
    , reset_n("reset_n")
    , irq_out("irq_out")
    , impl_(std::make_unique<impl>("impl", *this, options))
{
    target_socket.register_b_transport(this, &Fx1Npu::b_transport);
    target_socket.register_transport_dbg(this, &Fx1Npu::transport_dbg);
    SC_METHOD(drive_irq);
    sensitive << impl_->irq_event;
    sc_core::sc_spawn(sc_bind(&impl::native_thread, impl_.get()), "native_worker");
    sc_core::sc_spawn(sc_bind(&impl::rich_thread, impl_.get()), "rich_worker");
}

Fx1Npu::~Fx1Npu() = default;

void Fx1Npu::drive_irq() { irq_out.write(impl_->irq_level); }

void Fx1Npu::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    delay += impl_->opt.access_latency;
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
    if (trans.get_data_length() != sizeof(std::uint32_t) || trans.get_streaming_width() < sizeof(std::uint32_t) ||
        (address & 0x3u) != 0u || address >= MMIO_SIZE) {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    const std::uint32_t offset = static_cast<std::uint32_t>(address);
    const bool bank = offset >= WRAPPER_BASE && offset < WRAPPER_BASE + WRAPPER_WINDOW_SIZE;
    const bool rich = offset >= RICH_ALIAS_BASE && offset < RICH_ALIAS_BASE + RICH_ALIAS_SIZE;
    std::uint32_t value = 0;
    tlm::tlm_response_status status = tlm::TLM_COMMAND_ERROR_RESPONSE;
    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        if (bank) status = impl_->read_reg(offset - WRAPPER_BASE, value);
        else if (rich) status = impl_->rich_read(MODEL_RICH_BASE + (offset - RICH_ALIAS_BASE), value);
        else status = impl_->submit_native(impl::native_command::read, offset, value);
        if (status == tlm::TLM_OK_RESPONSE) std::memcpy(data, &value, sizeof(value));
    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
        std::memcpy(&value, data, sizeof(value));
        if (bank) status = impl_->write_reg(offset - WRAPPER_BASE, value);
        else if (rich) status = impl_->rich_write(MODEL_RICH_BASE + (offset - RICH_ALIAS_BASE), value, delay);
        else status = impl_->submit_native(impl::native_command::write, offset, value);
    }
    trans.set_response_status(status);
}

// Debug access: reads of the wrapper bank and of the rich status/shadow only;
// never waits, never changes state. Anything else returns 0 bytes.
unsigned int Fx1Npu::transport_dbg(tlm::tlm_generic_payload& trans)
{
    if (trans.get_command() != tlm::TLM_READ_COMMAND || trans.get_data_length() != 4 || trans.get_data_ptr() == nullptr ||
        (trans.get_address() & 3u) != 0u)
        return 0;
    const std::uint64_t offset = trans.get_address();
    std::uint32_t value = 0;
    tlm::tlm_response_status status = tlm::TLM_ADDRESS_ERROR_RESPONSE;
    if (offset >= WRAPPER_BASE && offset < WRAPPER_BASE + WRAPPER_WINDOW_SIZE)
        status = impl_->read_reg(static_cast<std::uint32_t>(offset - WRAPPER_BASE), value);
    else if (offset >= RICH_ALIAS_BASE && offset < RICH_ALIAS_BASE + RICH_ALIAS_SIZE)
        status = impl_->rich_read(MODEL_RICH_BASE + static_cast<std::uint32_t>(offset - RICH_ALIAS_BASE), value);
    if (status != tlm::TLM_OK_RESPONSE) return 0;
    std::memcpy(trans.get_data_ptr(), &value, 4);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
    return 4;
}

NpuStats Fx1Npu::stats() const
{
    NpuStats s = impl_->st;
    s.image_size = impl_->image.size();
    s.image_capacity = impl_->image.capacity();
    s.image_peak = impl_->image_peak;
    return s;
}

std::string Fx1Npu::model_config() const { return impl_->config_text; }

std::string Fx1Npu::banner() const
{
    std::ostringstream out;
    char hash[16];
    std::snprintf(hash, sizeof hash, "%08x", impl_->model_hash);
    out << "SAURIA v4.7 (model manifest " << hash << "), " << impl_->config_text << ", queue " << impl_->opt.queue_depth
        << ", window max " << (impl_->opt.max_window >> 20) << " MiB, push wait " << impl_->push_wait_ns << " ns, DMA chunk "
        << impl_->opt.dma_chunk_bytes << " B";
    return out.str();
}

std::string Fx1Npu::report() const
{
    const NpuStats s = stats();
    std::ostringstream out;
    out << "pushes " << s.pushes_accepted << " accepted " << s.pushes_refused << " refused " << s.push_timeouts
        << " timed out, retired " << s.retired << ", model refusals " << s.model_refusals << ", aborts " << s.aborts
        << " (dropped " << s.abort_dropped << "), GEMM-bank jobs " << s.native_jobs << ", DMA rd " << s.dma_read_bytes
        << " B wr " << s.dma_write_bytes << " B in " << s.dma_transactions << " transactions, image " << s.image_size
        << " B (peak " << s.image_peak << ")";
    return out.str();
}

} // namespace fx1::npu
