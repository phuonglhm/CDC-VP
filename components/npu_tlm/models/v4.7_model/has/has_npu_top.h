// has_npu_top.h -- HasNpuTop: the delivery top level of the NPU (docs/ARCHITECTURE.md, section 7).
//
// Host side = the v4.5 MMIO protocol (has/has_mmio_compat.h: gather registers 0x40000400.., push 0x40000310,
// STATUS 0x40000318, RETIRED 0x4000031C) + o_irq. Memory side = one DRAM (set_dram(), like NpuTop / SauriaDma).
// Inside, the Data Flow Controller (SC_THREAD dfc) pops one LayerInstr per pushed instruction and runs it:
//   GEMM_FUSED  -> tile iterator (has/has_tile_iter.h) -> per tile: DMA CH1 input window, CH0 weights (already in
//                  SAURIA order in DRAM), CH3 bias preload; OBP parameters; real core (sauria_rtl::NpuTop);
//                  OBP (has::HasObp) over the tile's contexts; DMA CH2 write-back into the destination tensor
//   ELEM_WISE   -> has::HasElemwise (ADD 2 stages / MAX_POOL / AVG_POOL mode 5 through the Scratchpad)
//   FUSED_ATTN, LAYERNORM -> vector-unit models (gvu_fused_attn.h, gvu_layernorm.h), operands and parameters from DRAM
//   DMA_TENSOR  -> COPY of one contiguous DRAM block by the DMA read and write ports (exec_dma_tensor; on by default, HAS_DMA_TENSOR=0 refuses it)
//
// Two schedules. Sequential (set_mode(false, ...)): the per-tile sequence of the network testbench
// tools/fe/sysc/tb_has_net.cpp, one tile after the other with the next input window prefetched. Overlapped
// (set_mode(true, ...)): true ping-pong in the core SRAM, see exec_tiles_ovl(). The run profiles (Profile) select one.
// Deviations from the hardware drawings, kept on purpose:
//   D1  sequential schedule / epilogue not inline: the OBP re-reads SRAM-C after the core instead of sitting on the
//       PSM -> SRAM-C path (removed by set_obp_inline(true))
//   D2  two SRAM models (the v4.5 Sram the DMA writes in the sequential schedule, the core's sauria_rtl::Sram) joined
//       by the backdoor
//   D3  the 3-D input window with zero padding, the bias broadcast and the int8 write-back scatter are DMA-descriptor
//       features (questions H17 to H19) emulated with a private staging area appended after the program's DRAM image
//   D4  v4.5 DMA timing (32 B/cycle, no DRAM latency; about 2x optimistic against the HAS AXI-128) unless
//       DmaParams::has_axi128() is selected
#ifndef HAS_NPU_TOP_H
#define HAS_NPU_TOP_H

#include <systemc.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sauria_types.h"
#include "sram/sram_top.h"
#include "control/sauria_dma.h"
#include "rtl_ref_npu_top.h"
#include "driver/libsauria_cfg.h"
#include "sauria_targets.h"
#include "has/gvu_obp.h"
#include "has/gvu_psm_probe.h"
#include "has/gvu_elemwise.h"
#include "has/gvu_top_stream.h"
#include "has/gvu_vrf.h"
#include "has/gvu_sp_map.h"
#include "has/has_mmio_compat.h"
#include "has/has_host_neo_port.h"
#include "has/has_tile_iter.h"
#include "has/has_pad_gen.h"
#include "has/has_dma.h"
#include "has/dma_dfc_1d.h"
#include "has/dfc_core_dma.h"
#include "has/gvu_out_transpose.h"
#include "has/gvu_rce_params.h"
#include "has/has_profile_neo.h"
#include "has/hw_capability.h"
#ifdef FE_METRICS
#include "instrumentation/rtl_ref_perf_counters.h"
#endif
#ifndef FX1_A3_SRAM_BACKDOOR_LOAD
#error "has_npu_top.h v1 loads the core SRAM through NpuTop::load_sram_backdoor: build with -DFX1_A3_SRAM_BACKDOOR_LOAD"
#endif

namespace has
{
    using sauria::act_vector_t;
    using sauria::wei_vector_t;
    using sauria::psum_vector_t;
    using sauria::sramc_mask_t;

    // Run profiles. Recommended = the configuration of the reference results (overlapped ping-pong schedule, HAS AXI-128
    // DMA, epilogue inline on the PSM -> SRAM-C path, banked scratchpad, vector-unit latencies, 3-D DMA descriptors,
    // per-layer tile order, attention products on the core). Proposals = Recommended + the two proposed hardware options
    // (bias broadcast descriptor, halo reuse). Legacy = sequential schedule, v4.5 DMA timing, epilogue after the core.
    // Neo / NeoAsRtl (has/has_profile_neo.h): Recommended without the 3-D DMA descriptors (the DMA of the RTL is 1-D), the knobs of the
    // model that follow neo_lite RTL revision 1 on and the proposals the RTL does not have off; NeoAsRtl adds the knobs that imitate the RTL.
    enum class Profile { Legacy, Recommended, Proposals, Neo, NeoAsRtl };
    inline const char *profile_name(Profile p)
    {
        switch (p)
        {
        case Profile::Legacy: return "legacy";
        case Profile::Proposals: return "proposals";
        case Profile::Neo: return "neo";
        case Profile::NeoAsRtl: return "neo_as_rtl";
        default: return "recommended";
        }
    }
    inline bool profile_is_neo(Profile p) { return p == Profile::Neo || p == Profile::NeoAsRtl; }
    // The profile a constructor with the default argument (Recommended) really takes: HAS_PROFILE=neo|neo_as_rtl in the environment
    // selects a neo profile; unset, Recommended stays. The other profiles are never changed.
    inline Profile resolve_profile(Profile p)
    {
        if (p != Profile::Recommended) return p;
        switch (profile_neo::from_env())
        {
        case 1: return Profile::Neo;
        case 2: return Profile::NeoAsRtl;
        default: return p;
        }
    }
    // Knobs are fixed at construction (epilogue and element-wise latencies, scratchpad banking).
    inline Knobs profile_knobs(Profile p, Knobs k)
    {
        if (p != Profile::Legacy) { k.sp_banked = 1; k.gvu_latencies(); }
        return k;
    }
    // Knobs of a profile when the caller gives none: the default configuration (Knobs::spec_default()) for every profile
    // but Legacy, which keeps the earlier values.
    inline Knobs profile_knobs(Profile p)
    {
        if (profile_is_neo(p))
        {
            // the variables of the profile first (before any part reads its knob), then the knobs from the environment
            apply_env_profile(profile_name(p), stderr);
            return profile_knobs(p, Knobs::from_env(true));
        }
        Knobs k;
        if (p != Profile::Legacy) k.spec_default();
        return profile_knobs(p, k);
    }

    class HasNpuTop : public sc_module
    {
    public:
        static constexpr int W32 = 32;
        static constexpr int kSramaBytes = 79 * 1024, kSrambBytes = 81 * 1024;   // HAS: one buffer of each
        // Private staging areas in the DRAM vector: input window of a tile; PSUM preload and PSUM read-back, each as
        // large as the modelled SRAM-C (1,536 rows of 128 bytes), so that every tile the SRAM model holds is staged whole.
        static constexpr uint32_t kStageABytes = 128 * 1024, kStageCBytes = 1536 * 128;
        typedef sauria::Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> DmaSramT;
        // Bank of a DMA transfer whose bytes are not kept (HAS_BUS_STRICT): it stands for a vector-unit bank the model fills on its own
        // (the channel parameters reach the epilogue by set_channel_params, the table by load_lut); only the bus cycles count.
        static constexpr int kSinkBank = -1;
        // DMA SRAM side: the v4.5 Sram (sequential schedule) or the host half of the core's ping-pong SRAM (overlapped).
        struct SramPort
        {
            DmaSramT *v45{nullptr};
            sauria_rtl::NpuTop<32, 32, int8_t, int8_t, int32_t, kSramaBytes, kSrambBytes, 1536, 16, 64, 1> *core{nullptr};
            bool core_host_side{false};
            void write(int b, uint32_t o, const uint8_t *p, uint32_t n)
            { if (b == kSinkBank) return; if (core_host_side) core->write_sram_host_side(b, o, p, n); else v45->write_bank_data(b, o, p, n); }
            void read(int b, uint32_t o, uint8_t *p, uint32_t n)
            { if (b == kSinkBank) return; if (core_host_side) core->read_sram_host_side(b, o, p, n); else v45->read_bank_data(b, o, p, n); }
        };
        typedef HasDma<SramPort> DmaT;
        typedef HasObp<W32> ObpT;
        typedef sauria_rtl::NpuTop<32, 32, int8_t, int8_t, int32_t, kSramaBytes, kSrambBytes, 1536, 16, 64, 1> CoreT;
        static constexpr uint32_t LUT_BASE = 0x00140000, SCALE_BASE = 0x00180000, SHIFT_BASE = 0x00190000, CFG_BASE = 0x001A0000;

        // ---- ports (host side of NpuTop, docs/SW_INTEGRATION_GUIDE.md, section 3) ----
        sc_in<bool> i_clk{"i_clk"}, i_rstn{"i_rstn"};
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"}, i_host_rden{"i_host_rden"};
        sc_in<sauria::host_data_t> i_host_wdata{"i_host_wdata"};
        sc_in<sauria::host_mask_t> i_host_wmask{"i_host_wmask"};
        sc_out<sauria::host_data_t> o_host_rdata{"o_host_rdata"};
        sc_out<bool> o_irq{"o_irq"}, o_done{"o_done"}, o_deadlock{"o_deadlock"};

        // ---- statistics (read by the testbench after the run; not part of the MMIO interface) ----
        struct Stats
        {
            uint64_t instr{0}, gemm{0}, elem{0}, tiles{0}, core_deadlocks{0}, framing_errors{0}, wide_weight_rejected{0};
            uint64_t stage_overflow{0};   // tiles larger than a staging area (they do not fit the SRAMs either); also a framing error
            uint64_t dma_cycles{0}, obp_cycles{0}, cfg_cycles{0}, elem_cycles{0}, vectors{0}, macs{0}, core_passes{0};
            uint64_t weight_loads{0}, weight_reuse_flips{0};
            uint64_t attn{0}, ln{0}, rce_rows{0}, rce_cycles{0}, rce_ovf16{0}, rce_param_errors{0};   // FUSED_ATTN / LAYERNORM
            uint64_t rce_core_passes{0}, rce_core_cycles{0};                                          // attention products on the core
            uint64_t rce_groups{0}, rce_sp_short{0};   // row groups run; instructions whose scalar bank is too small (sp_limit_rows)
            uint64_t rce_smx_model{0};                 // HAS_GVU_SOFTMAX: softmax row groups that stayed on the model (masked or not expressible)
            uint64_t cycles{0};                                                                       // sum of instruction cycles
            // PE utilization = real_macs / (core_busy * X * Y). core_busy: cycles from the start pulse of a core pass to its
            // o_done (close to the controller's not-IDLE span that FE_METRICS builds measure). real_macs: MACs the layer needs
            // (output elements x K), without the padding of PAD_TAIL / position rounding that `macs` includes.
            uint64_t core_busy{0}, real_macs{0};
            uint64_t feeder_pad_passes{0};   // core passes whose padding was generated by the IFMAP feeder (feeder_pad)
            uint64_t feeder_pad_fallback{0}; // feeder_pad on, window reaches outside the image, padding staged all the same
                                             // (1x1 stride-1 layer, window entirely outside, value too wide for its register)
            uint64_t dma_tensor{0}, dma_tensor_bytes{0}, dma_tensor_cycles{0};   // DMA_TENSOR instructions, bytes, cycles
            uint64_t dma_upsample{0}, dma_reshape{0};                            // DMA upsample / reshape instructions executed
        } st;
        MmioCompat mmio;
        NeoPort neo;   // HAS_HOST=neo: the register file of neo_lite in the host port (has/has_host_neo_port.h); off by default
        Knobs knobs;
        int core_max_cycles{2000000}, stall_cycles{5000};
        bool prefetch_a{true};
        bool trace{false};                 // one line per retired instruction on stdout
        // Padding by the IFMAP feeder (default off; environment HAS_FEEDER_PAD=1). Off: the DFC stages the input window of
        // the tile WITH its padding and the core runs a no-padding convolution on it. On: SRAM-A receives only the real
        // part of the window and the feeder inserts the padding value while it gathers (IfmapFeederRtl, not in the SAURIA
        // RTL). Tiles without padding are identical in both settings.
        bool feeder_pad{false};
        // DMA_TENSOR COPY chunk in bytes (environment HAS_DMA_TENSOR_CHUNK, 16 .. 98304): chunk k + 1 is read while chunk k is written.
        uint32_t dma_tensor_chunk{1024};
        // One line per COPY on stdout (environment HAS_TRACE_DMA_TENSOR=1; off by default, no effect on the run): bytes, chunk and the
        // cycles from the start of the copy (all DMA transfers idle) to its retire, for tools/has/verify_flows.py.
        bool trace_dma_tensor{false};
        // 4-D DMA descriptors (default on; environment HAS_DMA_ND=0 turns them off; model proposal, not in the RTL DMA): the tile
        // input window gathered and the int8 write-back scattered by HasDma descriptors (same bytes, same bus cycles), and
        // ELEM_WISE modes 6 (slice) / 7 (concat) / 8 (upsample) / 9 (reshape) accepted and run by the DMA in pieces of
        // dma_tensor_chunk bytes. Off: modes 6..9 refused, the tile window and write-back as before (same bytes, same cycles).
        bool dma_nd{true};
        // Slice / concat pieces over merged dimensions (default on; environment HAS_DMA_ND_MERGE=0 turns it off): before the
        // strided side is cut into pieces, dimensions j and j + 1 with s[j + 1] = n[j] * s[j] are merged, so a piece is no longer cut
        // at a row or channel boundary that is contiguous in memory (same bytes in the same order, fewer and fuller pieces).
        bool dma_nd_merge{true};
        // Corner turn on the int8 output of the epilogue (default on; environment HAS_OUT_TRANSPOSE=0 turns it off; proposal of the model, not in
        // the RTL, has/gvu_out_transpose.h): tiles of the channels from GEMM_FUSED X_TMAJ_C0 on are written token-major. Off: X_TMAJ_C0
        // is ignored and every tile is written [C][H][W] as before.
        bool out_transpose{true};
        // Bus cost of the data the DFC moves outside the DMA model (environment HAS_BUS_STRICT=1; default off, nothing changes): the
        // per-channel scale / shift of a tile (8 bytes a channel; the hardware brings them into the vector-unit sbank with a DMA step of
        // its data flow controller) cross the DMA read port with the prefetch of the tile's bias; FUSED_ATTN adds the softmax output
        // A (rows x seq_len int8) written out of the vector unit and read into SRAM-A for A.V (no direct path from the vector unit to
        // the core SRAM). HAS_BUS_STRICT_LUT=1 (test only): the activation table load as a DMA transfer of 256 bytes instead of its
        // estimate, to compare the two.
        bool bus_strict{false};
        bool bus_strict_lut{false};
        // HAS_GVU_FA_OVERLAP=1 (default 0; needs HAS_GVU_FA=1 on the clock and the products on the core): the requant of the attention
        // products runs WHILE the core computes them (proposal P-FA1 / P-FA2): the zero-point corrections of a phase go into the preload of
        // the partial sums, and the requant of the sums of one core pass (32 output channels) is a job on GvuTop that runs during the next
        // pass; only the requant of the last pass and the waits for the unit stay in the time of the instruction.
        bool fa_overlap{false};
        uint64_t fa_ovl_phases_{0}, fa_ovl_serial_{0}, fa_ovl_jobs_{0}, fa_ovl_wait_{0};
        // Padding value of GEMM_FUSED input windows (int8): taken from the instruction (PAD_VALUE, 0 when not written).
        // HAS_PAD_VALUE forces one value for every convolution instead (experiments on existing programs only).
        int pad_value{0};
        bool pad_value_forced{false};
        int pad_val_of(const LayerInstr &li) const { return pad_value_forced ? pad_value : li.pad_val; }
        // Padding of one tile under feeder_pad: geometry and register values from the padding generator (has/has_pad_gen.h).
        typedef PadGen PadTile;
        std::FILE *metrics_csv{nullptr};   // per-tile core counters, same columns as tb_has_net (FE_METRICS builds)

        const ObpT &obp_block() const { return *obp_; }
        const HasElemwise &elem_block() const { return *ew_; }


        // DRAM image: the DFC appends its private staging area (deviation D3) after `program_bytes`.
        void set_dram(std::vector<uint8_t> *dram)
        {
            dram_ = dram;
            const size_t base = (dram->size() + 4095) & ~size_t(4095);
            stage_a_ = uint32_t(base);
            stage_pre_ = stage_a_ + kStageABytes;
            stage_out_ = stage_pre_ + kStageCBytes;
            dram->resize(size_t(stage_out_) + kStageCBytes, 0);
            dma_->set_dram(dram);
        }
        uint32_t staging_base() const { return stage_a_; }
        // Place the private staging area at a fixed DRAM offset instead of after the image given to set_dram(). Used when
        // the DRAM vector is filled after set_dram() (a system wrapper that stages data per instruction); the offset must
        // lie above every address the program uses. Call after set_dram().
        void set_staging_base(uint32_t base)
        {
            stage_a_ = (base + 4095u) & ~4095u;
            stage_pre_ = stage_a_ + kStageABytes;
            stage_out_ = stage_pre_ + kStageCBytes;
            if (dram_ && dram_->size() < size_t(stage_out_) + kStageCBytes) dram_->resize(size_t(stage_out_) + kStageCBytes, 0);
        }

        SC_HAS_PROCESS(HasNpuTop);
        // Default: the Recommended profile. The Knobs constructor leaves every option off (= Legacy) for tools that set
        // options one by one.
        explicit HasNpuTop(sc_module_name nm, Profile p = Profile::Recommended, size_t q_depth = 16)
            : HasNpuTop(nm, profile_knobs(resolve_profile(p)), q_depth)
        {
            apply_profile(resolve_profile(p));
            if (profile_is_neo(resolve_profile(p))) verify_env_profile(stderr);
        }
        HasNpuTop(sc_module_name nm, const Knobs &k, size_t q_depth = 16)
            : sc_module(nm), mmio(q_depth), knobs(k)
        {
            if (const char *v = std::getenv("HAS_FEEDER_PAD")) feeder_pad = std::atoi(v) != 0;
            if (const char *v = std::getenv("HAS_PAD_VALUE")) { pad_value = std::max(-128, std::min(127, std::atoi(v))); pad_value_forced = true; }
            if (const char *v = std::getenv("HAS_DMA_TENSOR_CHUNK")) dma_tensor_chunk = uint32_t(std::max(16, std::min(98304, std::atoi(v))));
            if (const char *v = std::getenv("HAS_DMA_ND")) dma_nd = std::atoi(v) != 0;
            if (const char *v = std::getenv("HAS_DMA_ND_MERGE")) dma_nd_merge = std::atoi(v) != 0;
            if (const char *v = std::getenv("HAS_OUT_TRANSPOSE")) out_transpose = std::atoi(v) != 0;
            if (const char *v = std::getenv("HAS_TRACE_DMA_TENSOR")) trace_dma_tensor = std::atoi(v) != 0;
            if (const char *v = std::getenv("HAS_BUS_STRICT")) bus_strict = std::atoi(v) != 0;
            if (const char *v = std::getenv("HAS_BUS_STRICT_LUT")) bus_strict_lut = std::atoi(v) != 0;
            if (const char *v = std::getenv("HAS_GVU_FA_OVERLAP")) fa_overlap = std::atoi(v) != 0;
            ew_ = new HasElemwise(knobs);

            sram_ = new DmaSramT("dma_sram");
            sram_->i_clk(i_clk); sram_->i_rstn(i_rstn); sram_->i_deepsleep(s_false_); sram_->i_powergate(s_false_);
            sram_->i_select(sram_select_);
            sram_->i_host_addr(s_host_addr_); sram_->i_host_wren(s_false_); sram_->i_host_rden(s_false_);
            sram_->i_host_wdata(s_host_wdata_); sram_->i_host_wmask(s_host_wmask_); sram_->o_host_rdata(s_host_rdata_);
            sram_->i_srama_addr_a(z_addr_[0]); sram_->i_srama_rden_a(s_false_); sram_->o_srama_data_a(a_data_[0]);
            sram_->i_srama_addr_b(z_addr_[1]); sram_->i_srama_rden_b(s_false_); sram_->o_srama_data_b(a_data_[1]);
            sram_->i_sramb_addr_a(z_addr_[2]); sram_->i_sramb_rden_a(s_false_); sram_->o_sramb_data_a(b_data_[0]);
            sram_->i_sramb_addr_b(z_addr_[3]); sram_->i_sramb_rden_b(s_false_); sram_->o_sramb_data_b(b_data_[1]);
            sram_->i_sramc_wdata_a(c_zero_[0]); sram_->i_sramc_addr_a(z_addr_[4]); sram_->i_sramc_wren_a(s_false_);
            sram_->i_sramc_rden_a(s_false_); sram_->i_sramc_wmask_a(c_mask_[0]); sram_->o_sramc_rdata_a(c_rd_[0]);
            sram_->i_sramc_wdata_b(c_zero_[1]); sram_->i_sramc_addr_b(z_addr_[5]); sram_->i_sramc_wren_b(s_false_);
            sram_->i_sramc_rden_b(s_false_); sram_->i_sramc_wmask_b(c_mask_[1]); sram_->o_sramc_rdata_b(c_rd_[1]);

            dma_ = new DmaT("dma", DmaParams::v45());
            dma_->i_clk(i_clk); dma_->i_rstn(i_rstn);
            port_.v45 = sram_;
            dma_->set_port(&port_);

            obp_ = new ObpT("obp", knobs, knobs.lat_obp);
            obp_->i_clk(i_clk); obp_->i_rstn(i_rstn);
            obp_->i_data(o_in_); obp_->i_addr(o_in_addr_); obp_->i_wmask(o_in_mask_); obp_->i_valid(o_in_valid_);
            obp_->i_residual(o_residual_);
            obp_->o_sramc_wdata(o_out_); obp_->o_sramc_addr(o_out_addr_); obp_->o_sramc_wren(o_out_wren_);
            obp_->o_sramc_wmask(o_out_mask_); obp_->o_valid(o_out_valid_);
            obp_->i_bias_en(s_false_); obp_->i_requant_en(s_true_); obp_->i_lut_en(lut_en_); obp_->i_residual_en(s_false_);
            obp_->i_vec_channel_mode(s_true_); obp_->i_requant_scale(def_scale_); obp_->i_requant_shift(def_shift_);
            obp_->i_host_addr(obp_host_addr_); obp_->i_host_wren(obp_host_wren_); obp_->i_host_rden(s_false_);
            obp_->i_host_wdata(obp_host_wdata_); obp_->i_host_wmask(obp_host_wmask_); obp_->o_host_rdata(obp_host_rdata_);

            // Instance name must contain "NpuTop_std" (selects the feeder tape in rtl_ref_ifmap_feeder.h).
            core_ = new CoreT("NpuTop_std");
#ifdef FE_METRICS
            core_->attach_perf(&perf_);
#endif
            core_->i_clk(i_clk); core_->i_rstn(core_rstn_); core_->i_soft_reset(s_false_); core_->i_start(core_start_);
            core_->o_done(core_done_); core_->o_deadlock(core_deadlock_); core_->i_mvm_k(core_mvm_k_);
            core_->i_host_addr(core_host_addr_); core_->i_host_wren(core_host_wren_); core_->i_host_rden(core_host_rden_);
            core_->i_host_wdata(core_host_wdata_); core_->i_host_wmask(core_host_wmask_); core_->o_host_rdata(core_host_rdata_);
            core_->i_threshold(core_threshold_); core_->i_select(core_select_); core_->i_total_contexts(core_total_contexts_);
            port_.core = core_;
            if (GvuTopBridge::level() == 3) build_stream();

            SC_METHOD(host_port);
            sensitive << i_clk.pos();
            dont_initialize();
            SC_METHOD(obp_writeback);
            sensitive << i_clk.pos();
            dont_initialize();
            SC_METHOD(on_core_done);
            sensitive << core_done_.posedge_event();
            dont_initialize();
            SC_THREAD(dfc);
            sensitive << i_clk.pos();
            if (PsmProbe::on())
            {
                SC_METHOD(psm_probe_tick);
                sensitive << i_clk.pos();
                dont_initialize();
            }
        }
        ~HasNpuTop()
        {
            if (bus_strict)
                std::fprintf(stderr, "[HasNpuTop] HAS_BUS_STRICT: vector-unit writes into SRAM-C %llu, in a step with a PSM write %llu\n",
                             (unsigned long long)bs_pokes_, (unsigned long long)bs_conflicts_);
            if (fa_overlap)
                std::fprintf(stderr, "[HasNpuTop] HAS_GVU_FA_OVERLAP: phases overlapped %llu (left serial %llu), requant jobs during the core passes %llu, "
                             "cycles the DFC waited for the unit %llu\n", (unsigned long long)fa_ovl_phases_, (unsigned long long)fa_ovl_serial_,
                             (unsigned long long)fa_ovl_jobs_, (unsigned long long)fa_ovl_wait_);
            delete gvs_; delete tap_; delete core_; delete obp_; delete dma_; delete sram_; delete ew_;
        }

        // Call before sc_start(). overlap=false: sequential schedule. overlap=true: overlapped schedule -- the DMA
        // works on the host half of the core's ping-pong SRAM, and while the core computes tile i the DFC
        // runs OBP + write-back of tile i-1 and prefetches tile i+1. `p` selects the DMA timing (DmaParams::v45() or
        // DmaParams::has_axi128(latency)).
        void set_mode(bool overlap, const DmaParams &p)
        {
            overlap_ = overlap;
            port_.core_host_side = overlap;
            dma_->prm = p;
        }
        bool overlap() const { return overlap_; }
        // Period of i_clk. Cycle counts (statistics, halo copy, vector-unit waits) are derived from simulated time, so
        // this must match the clock that drives i_clk (default 10 ns). The result in cycles does not depend on it.
        void set_clock_period(const sc_time &p) { clk_period_ = p; }
        const sc_time &clock_period() const { return clk_period_; }
        // Epilogue on the PSM -> SRAM-C write path (vector-unit drawing, figure 5; removes deviation D1) instead of
        // re-reading SRAM-C after the core. Overlapped schedule only: the tile's C half already holds the int8 results when it is
        // flipped to the host side, so the DFC goes straight to the CH2 write-back.
        void set_obp_inline(bool on)
        {
            obp_inline_ = on;
            if (!on) { core_->set_sramc_write_hook(nullptr); if (obp_) obp_->stream_sink = nullptr; return; }
            // HAS_GVU_TOP=3: the beats of the last Cin pass go into the clocked stream (PsmStreamTap); the write itself is not changed
            // (SRAM-C keeps the int32 psum as in the RTL) and the int8 result is written back by GvuTopStream one edge plus the stream depth later
            if (gvs_) obp_->stream_sink = [this](const psum_vector_t<W32, int32_t> &in, uint32_t mask, uint32_t ch, int64_t S, int s, bool bypass,
                                                 const psum_vector_t<W32, int32_t> &model)
            {
                StreamPacket p;
                for (int l = 0; l < W32; l++) p.data[size_t(l)] = ((mask >> l) & 1u) ? in[size_t(l)] : 0;   // lanes without a mask are 0 in the packet
                p.mask = mask; p.addr = obp_->inline_word_addr; p.ch = ch; p.S = S; p.s = s; p.bypass = bypass || !stream_zp_fits_; p.model = model;
                p.oc_last = (ch + 1 == inl_nch_); p.oc_tile_last = inl_oc_tile_last_;
                p.call_last = inl_seg_last_;
                tap_->push(p);
            };
            core_->set_sramc_write_hook([this](uint32_t addr, psum_vector_t<W32, int32_t> &v, const sramc_mask_t<W32> &m)
            {
                if (!inl_active_) return;   // S1: earlier Cin passes keep the int32 psum for the next pass
                const bool strm = gvs_ != nullptr;
                const uint32_t e0 = (addr % 1536u) * W32;
                psum_vector_t<W32, int32_t> out = v;
                uint32_t k = 0;
                while (k < uint32_t(W32))
                {
                    if (!m[k]) { k++; continue; }
                    const uint32_t x = (e0 + k) / inl_npos_;
                    sramc_mask_t<W32> mx(false);
                    uint32_t j = k;
                    for (; j < uint32_t(W32); j++)
                    {
                        if (!m[j]) continue;
                        if ((e0 + j) / inl_npos_ != x) break;
                        mx[j] = true;
                    }
                    if (x >= inl_nch_) inline_out_of_tile++;
                    else
                    {
                        if (strm)
                        {
                            // the write that holds the last element of the tile (last channel, last position) is the last of the call
                            uint32_t ql = k;
                            for (uint32_t q = k; q < j; q++) if (mx[q]) ql = q;
                            inl_seg_last_ = (x + 1 == inl_nch_) && (e0 + ql + 1 == inl_nch_ * inl_npos_);
                            obp_->inline_word_addr = addr;
                        }
                        const auto r = obp_->apply_inline(v, x, mx);
                        if (!strm) for (uint32_t q = k; q < j; q++) if (mx[q]) out[q] = r[q];
                        st.vectors++;
                    }
                    k = j;
                }
                v = out;
            });
        }
        bool obp_inline() const { return obp_inline_; }
        // Options of a profile (call before sc_start; the knobs part comes from profile_knobs() at construction).
        void apply_profile(Profile p, uint32_t dram_latency = 0)
        {
            const bool on = p != Profile::Legacy;
            set_mode(on, on ? DmaParams::has_axi128(dram_latency) : DmaParams::v45());
            set_obp_inline(on);
            // Neo profiles: no 3-D descriptors (1-D DMA, DFC loops over at most 2-D); S4 tile order (loop_order 0 / 2 of the DFC) and the
            // attention products on the core stay
            desc3d = on && !profile_is_neo(p);
            tile_order_auto = rce_core = on;
            c_bias_bcast = halo_reuse = p == Profile::Proposals;
        }
        // 3-D DMA descriptors (proposals H18, H19): CH1 moves only the in-bounds part of the input
        // window (the DMA fills the padding), CH2 writes the tile as packed int8. Data is unchanged; only bus bytes/cycles.
        bool desc3d{false};
        // Per-layer tile order: walk the tiles spatial-outermost when that moves fewer DMA bytes (the input window
        // then stays in SRAM-A while the output-channel tiles cycle; weights reload instead). Results are unchanged.
        bool tile_order_auto{false};
        uint64_t s4_layers{0};
        // Halo reuse (proposal; needs desc3d, no input-channel split, no spatial-outer order, kh > stride): tiles are walked
        // column by column (TileIter order 2), so the next tile's input window overlaps the one in the NPU half of SRAM-A by
        // kh - stride rows. CH1 moves only the new rows over the bus; the overlap rows are copied inside SRAM-A (NPU half ->
        // host half) at halo_copy_bpc bytes/cycle, off the AXI port, overlapped with the core (0 = free, e.g. a ring buffer).
        // The copy engine and its SRAM-A port are ESTIMATES (no HW drawing yet). Data is unchanged; only bus bytes/cycles.
        // FUSED_ATTN's Q.K^T and A.V on the core (run_core, 1x1 layers, positions padded to 32) instead of the tile-rule
        // estimate. DMA and softmax timing stay estimates.
        bool rce_core{false};
        // Proposal: CH3 bias preload as a broadcast descriptor (bias vector only on the bus). Timing ESTIMATED.
        bool c_bias_bcast{false};
        bool halo_reuse{false};
        uint32_t halo_copy_bpc{32};
        uint64_t halo_layers{0}, halo_tiles{0}, halo_bytes{0};
        uint64_t tmaj_tiles{0}, tmaj_tail_cycles{0}, tmaj_straddle{0};   // HAS_OUT_TRANSPOSE: tiles written token-major, corner-turn tails
        uint64_t inline_out_of_tile{0};
        const DmaStats &dma_stats() const { return dma_->st; }
        const GvuLsu &lsu() const { return ew_->lsu; }
        // Vector-unit structure report: Scratchpad transfers per bank / address mode (ELEM_WISE) and whether every
        // pipeline stage of the specification's table 2 fits the pipeline registers (specification layout, aliased 1 KB).
        std::string gvu_report() const
        {
            std::string s = ew_->lsu.report();
            int fit_spec = 0, fit_1k = 0;
            std::string miss_1k;
            for (const RegNeed &n : pipeline_reg_needs())
            {
                fit_spec += vrf_fits(n, VrfLayout::Spec) ? 1 : 0;
                if (vrf_fits(n, VrfLayout::Aliased1K)) fit_1k++;
                else miss_1k += std::string(miss_1k.empty() ? "" : ", ") + n.pipeline;
            }
            const int total = int(pipeline_reg_needs().size());
            s += "pipeline registers: " + std::to_string(fit_spec) + "/" + std::to_string(total) + " stages fit the specification layout, " +
                 std::to_string(fit_1k) + "/" + std::to_string(total) + " fit an aliased 1 KB" +
                 (miss_1k.empty() ? std::string() : " (not: " + miss_1k + ")") + "\n";
            return s;
        }

    private:
        bool overlap_{false};
        sc_time clk_period_{10, SC_NS};
        bool obp_inline_{false};
        uint32_t inl_npos_{1}, inl_nch_{1};
        bool inl_seg_last_{false};     // the segment of the write being streamed holds the last element of the tile
        int stall_mode_{0};   // HAS_GVU_STALL, see build_stream
        bool inl_active_{true};
        // HAS_GVU_TOP=3: GvuTop on the clock of the core (has/gvu_top_stream.h)
        PsmStreamTap *tap_{nullptr};
        GvuTopStream *gvs_{nullptr};
        bool gvu_clk_{false};   // HAS_GVU_ELEM_CLK: the vector-unit operations of the DFC run as jobs on the clock of GvuTopStream
        bool inl_oc_tile_last_{false};
        uint8_t stream_lut_[256];
        bool stream_zp_fits_{true};    // the zero point of the layer fits the 8 bits of the RTL register (stream_begin)
        bool regs_on_{false};          // HAS_GVU_REGS=1: the DFC drives GvuTop through the register file (has/gvu_regs_dfc.h)
        bool inl_call_last_{false};    // this core call is the last of the layer: its packets of the last context carry oc_tile_last (per-context flag)
        uint64_t halo_copy_done_{0};   // cycle at which the in-SRAM-A halo copy of the prefetched window ends
        SramPort port_;
        // ================================================================ host port
        void host_port()
        {
            if (!i_rstn.read()) { mmio.soft_reset(); neo.soft_reset(); o_irq.write(false); o_done.write(false); return; }
            if (i_host_wren.read() && i_host_wmask.read()[0] && !neo.handle_write(mmio, i_host_addr.read(), static_cast<uint32_t>(i_host_wdata.read()[0])))
                mmio.write(i_host_addr.read(), static_cast<uint32_t>(i_host_wdata.read()[0]));
            sauria::host_data_t r;
            if (i_host_rden.read() && neo.owns(i_host_addr.read())) r[0] = static_cast<double>(neo.read(mmio, i_host_addr.read()));
            else if (i_host_rden.read() && mmio.owns(i_host_addr.read())) r[0] = static_cast<double>(mmio.read(i_host_addr.read()));
            o_host_rdata.write(r);
            o_irq.write(mmio.irq());
            o_done.write(mmio.irq());
            o_deadlock.write(st.core_deadlocks != 0);
        }

        // HAS_BUS_STRICT, count only: results of the vector unit written back into SRAM-C (poke_sramc) in a clock step in which the PSM
        // also writes SRAM-C (one write port in the RTL). Nothing is delayed; the count and the writes are reported at exit.
        sc_signal<bool> *bs_wren_{nullptr};
        bool bs_looked_up_{false};
        uint64_t bs_pokes_{0}, bs_conflicts_{0};
        void count_sramc_conflict()
        {
            if (!bs_looked_up_)
            {
                bs_wren_ = dynamic_cast<sc_signal<bool> *>(sc_find_object((std::string(core_->name()) + ".s_sramc_wren").c_str()));
                bs_looked_up_ = true;
            }
            bs_pokes_++;
            if (bs_wren_ && bs_wren_->read()) bs_conflicts_++;
        }
        // HAS_PSM_PROBE: start / done of the core and the SRAM-C port of its PSM, sampled once per clock (observation only).
        // The port signals are private to the core top level, so they are looked up by name.
        sc_signal<bool> *pp_wren_{nullptr}, *pp_rden_{nullptr};
        sc_signal<uint32_t> *pp_addr_{nullptr};
        sc_signal<sramc_mask_t<W32>> *pp_mask_{nullptr};
        bool pp_looked_up_{false};
        void psm_probe_tick()
        {
            if (!pp_looked_up_)
            {
                const std::string c = std::string(core_->name()) + ".";
                pp_wren_ = dynamic_cast<sc_signal<bool> *>(sc_find_object((c + "s_sramc_wren").c_str()));
                pp_rden_ = dynamic_cast<sc_signal<bool> *>(sc_find_object((c + "s_sramc_rden").c_str()));
                pp_addr_ = dynamic_cast<sc_signal<uint32_t> *>(sc_find_object((c + "s_sramc_addr").c_str()));
                pp_mask_ = dynamic_cast<sc_signal<sramc_mask_t<W32>> *>(sc_find_object((c + "s_sramc_wmask").c_str()));
                pp_looked_up_ = true;
                if (!pp_wren_ || !pp_rden_ || !pp_addr_ || !pp_mask_)
                    std::fprintf(stderr, "[psm-probe] WARNING: the SRAM-C port signals of %s were not found; nothing is observed\n", core_->name());
            }
            if (!pp_wren_ || !pp_rden_ || !pp_addr_ || !pp_mask_) return;
            uint32_t mask = 0;
            if (pp_wren_->read())
            {
                const sramc_mask_t<W32> &m = pp_mask_->read();
                for (int i = 0; i < W32; i++) if (m[i]) mask |= 1u << i;
            }
            PsmProbe::tick(core_start_.read(), core_done_.read(), pp_wren_->read(), pp_rden_->read(), pp_addr_->read(), mask,
                           obp_inline_ && inl_active_, inl_npos_, inl_nch_);
        }

        // OBP output vectors -> PSUM bank of the DMA-side SRAM in C-order [k, h, w] (same as tb_has_net).
        void obp_writeback()
        {
            if (!o_out_wren_.read()) return;
            const uint32_t id = o_out_addr_.read(), ctx = id / cur_nch_, x = id % cur_nch_;
            const uint32_t oy = ctx / (cur_wt_ / cur_yu_), cx0 = (ctx % (cur_wt_ / cur_yu_)) * cur_yu_;
            const auto &v = o_out_.read();
            const auto &m = o_out_mask_.read();
            for (uint32_t k = 0; k < cur_yu_; k++)
                if (m[k])
                {
                    const uint32_t e = (x * cur_ht_ + oy) * cur_wt_ + cx0 + k;
                    const int32_t val = v[k];
                    if (overlap_) core_->write_sram_host_side(4, e * 4, reinterpret_cast<const uint8_t *>(&val), 4);
                    else sram_->write_bank_data(4, e * 4, reinterpret_cast<const uint8_t *>(&val), 4);
                }
            obp_rows_written_++;
        }

        // ================================================================ Data Flow Controller
        void dfc()
        {
            core_rstn_.write(false); core_start_.write(false); core_host_wren_.write(false); core_host_rden_.write(false);
            core_threshold_.write(0.5f); core_mvm_k_.write(1); core_total_contexts_.write(1); core_select_.write(sc_bv<3>("000"));
            obp_host_wren_.write(false); o_in_valid_.write(false); lut_en_.write(false);
            def_scale_.write(1); def_shift_.write(0); sram_select_.write(sc_bv<3>("000"));
            for (;;)
            {
                wait();
                if (!i_rstn.read()) continue;
                LayerInstr li;
                if (!mmio.pop(li)) continue;
                if (HwCheck::on()) HwCheck::instr(li);   // HAS_HW_CHECK=1: what the hardware (neo_lite) can run (has/hw_capability.h)
                // A system wrapper may clear or regrow the DRAM vector between instructions: keep the staging area.
                if (dram_ && dram_->size() < size_t(stage_out_) + kStageCBytes) dram_->resize(size_t(stage_out_) + kStageCBytes, 0);
                const uint64_t c0 = cycles_now();
                const uint64_t tiles0 = st.tiles;
                if (li.opcode == 0x12) exec_gemm(li);
                else if (li.opcode == 0x15) exec_elem(li);
                else if (li.opcode == 0x13) exec_attn(li);
                else if (li.opcode == 0x14) exec_ln(li);
                else if (li.opcode == 0x16) exec_dma_tensor(li);
                st.instr++;
                st.cycles += cycles_now() - c0;
                if (trace)
                    std::printf("[DFC] instr %llu op 0x%02x %s tiles %llu cycles %llu\n", (unsigned long long)li.host_seq,
                                li.opcode,
                                li.opcode == 0x12 ? "GEMM_FUSED" :
                                li.opcode == 0x13 ? "FUSED_ATTN" :
                                li.opcode == 0x14 ? "LAYERNORM" :
                                li.opcode == 0x16 ? (li.mode == 0 ? "DMA_COPY" : (li.mode == 2 || li.mode == 8) ? "DMA_UPSAMPLE" : "DMA_RESHAPE") :
                                (li.mode == 0 ? "ELEM_ADD" : li.mode == 5 ? "ELEM_AVG" : li.mode == 6 ? "ELEM_SLICE" :
                                 li.mode == 7 ? "ELEM_CONCAT" : li.mode == 8 ? "ELEM_UPSAMPLE" : li.mode == 9 ? "ELEM_RESHAPE" : "ELEM_MAX"),
                                (unsigned long long)(st.tiles - tiles0), (unsigned long long)(cycles_now() - c0));
                mmio.retire(li);
            }
        }

        uint64_t cycles_now() const { return sc_time_stamp().value() / clk_period_.value(); }

        // Core busy span of one pass (Stats::core_busy): opened at the start pulse, closed by o_done. A pass that never reports
        // done (deadlock, cycle limit) is closed where the DFC gives up. Counting only; no signal is written.
        void core_busy_open() { core_t0_ = cycles_now(); core_running_ = true; }
        void core_busy_close()
        {
            if (!core_running_) return;
            st.core_busy += cycles_now() - core_t0_;
            core_running_ = false;
        }
        void on_core_done() { core_busy_close(); }
        uint32_t dram_u32(uint32_t a) const { uint32_t v; std::memcpy(&v, &(*dram_)[a], 4); return v; }
        // A tile whose PSUM words exceed the staging area (= the 1,536 rows of the modelled SRAM-C; a legal tiling uses at
        // most 768) is counted, and its part beyond the area is neither written nor read: nothing is touched outside
        // the DRAM vector.
        uint32_t stage_words(uint32_t words)
        {
            if (words <= kStageCBytes / 128u) return words;
            st.stage_overflow++;
            st.framing_errors++;
            return kStageCBytes / 128u;
        }

        void obp_write(uint32_t addr, uint32_t v)
        {
            sauria::host_data_t d; sauria::host_mask_t m;
            d[0] = static_cast<double>(v); m.data.fill(true);
            obp_host_addr_.write(addr); obp_host_wdata_.write(d); obp_host_wmask_.write(m); obp_host_wren_.write(true);
            wait();
            obp_host_wren_.write(false);
            wait();
        }
        void obp_write4(uint32_t addr, const uint8_t *b)
        {
            sauria::host_data_t d; sauria::host_mask_t m;
            for (int q = 0; q < 4; q++) d[q] = static_cast<double>(b[q]);   // raw byte, as in tb_has_net
            m.data.fill(true);
            obp_host_addr_.write(addr); obp_host_wdata_.write(d); obp_host_wmask_.write(m); obp_host_wren_.write(true);
            wait();
            obp_host_wren_.write(false);
            wait();
        }

        void wait_dma_excl_ch1()
        {
            while (dma_->is_write_active() || dma_->is_read_active(0) || dma_->is_read_active(2) || dma_->is_read_active(3))
            { wait(); st.dma_cycles++; }
        }
        void wait_ch1() { while (dma_->is_read_active(1)) { wait(); st.dma_cycles++; } }

        // ---- HAS_GVU_TOP=3 ----
        bool stream_on() const { return gvs_ != nullptr && obp_inline_; }
        void build_stream()
        {
            tap_ = new PsmStreamTap("psm_stream_tap");
            gvs_ = new GvuTopStream("gvu_top_stream");
            tap_->i_clk(i_clk); tap_->i_rstn(i_rstn); tap_->i_psum_ready(st_ready_);
            tap_->o_psum_valid(st_valid_); tap_->o_psum_data(st_data_); tap_->o_oc_last(st_oc_last_); tap_->o_oc_tile_last(st_oc_tile_last_);
            tap_->o_psum_empty(st_empty_); tap_->o_stream_stall(st_stall_);
            tap_->o_side_addr(st_addr_); tap_->o_side_mask(st_mask_); tap_->o_side_ch(st_ch_); tap_->o_side_scale(st_scale_);
            tap_->o_side_shift(st_shift_); tap_->o_side_bypass(st_bypass_); tap_->o_side_model(st_model_); tap_->o_side_push(st_push_);
            gvs_->i_clk(i_clk); gvs_->i_rstn(i_rstn); gvs_->i_psum_valid(st_valid_); gvs_->i_psum_data(st_data_); gvs_->i_oc_last(st_oc_last_);
            gvs_->i_oc_tile_last(st_oc_tile_last_); gvs_->i_psum_empty(st_empty_); gvs_->i_stream_stall(st_stall_); gvs_->o_psum_ready(st_ready_);
            gvs_->i_side_addr(st_addr_); gvs_->i_side_mask(st_mask_); gvs_->i_side_ch(st_ch_); gvs_->i_side_scale(st_scale_);
            gvs_->i_side_shift(st_shift_); gvs_->i_side_bypass(st_bypass_); gvs_->i_side_model(st_model_); gvs_->i_side_push(st_push_);
            gvs_->result_sink = [this](uint32_t addr, uint32_t mask, const psum_vector_t<W32, int32_t> &v)
            {
                if (bus_strict) count_sramc_conflict();
                core_->poke_sramc(addr, v, mask);
            };
            gvs_->dump = GvuTopBridge::dump_file();
            GvuTopBridge::attach_elem(
                [this](const int8_t *A, const int8_t *B, int8_t *out, size_t n, int zpA, int zpB, int zpO, int64_t SA, int64_t SB, int64_t SO, int sA,
                       int sB, int sO, int rm, bool sub_before, bool sat16, uint64_t mc, uint64_t *cyc)
                { return gvs_->add(A, B, out, n, zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, rm, sub_before, sat16, mc, cyc); },
                [this](const int8_t *in, int8_t *out, int g, int H, int W, int kk, int s, int p, uint64_t mc, uint64_t *cyc)
                { return gvs_->maxpool(in, out, g, H, W, kk, s, p, mc, cyc); });
            // HAS_GVU_AVG=1: ELEM_WISE AVG_POOL on GvuTop (POOLING_AVG of the model), the channel groups of HasElemwise::avgpool in the call style
            if (GvuTopStream::avg_on())
                ew_->gvu_avg = [this](const int8_t *in, int8_t *out, int g, int H, int W, int kk, int s, int64_t S, int sh, int rm, uint64_t mc, uint64_t *cyc)
                { return gvs_->avgpool(in, out, g, H, W, kk, s, S, sh, rm, mc, cyc); };
            // HAS_GVU_REGS=1: the DFC configures and starts the unit through the register file of gvu_core (Reg 0..8), the register writes cost
            // HAS_GVU_REG_LAT clock edges each (default 4, an ESTIMATE of AXI-Lite into gvu_core); the layer ends by the flags of the PSM stream
            // (per-context oc_tile_last flag in the tap), max pool and residual add run by registers (residual add: register proposal Reg 9..15, HAS_GVU_REGS_ADD=0 keeps it outside).
            // It needs the jobs on the clock of the next knob, which it switches on.
            // default on; HAS_GVU_REGS=0 drives GvuTop by assignments as before the register file
            const char *regs_env = std::getenv("HAS_GVU_REGS");
            if ((regs_env ? std::atoi(regs_env) : 1) == 1)
                {
                    unsigned lat = 4;
                    if (const char *l = std::getenv("HAS_GVU_REG_LAT")) lat = static_cast<unsigned>(std::max(2, std::atoi(l)));
                    regs_on_ = true;
                    bool add_regs = true;     // HAS_GVU_REGS_ADD=0: residual add stays outside the register file (as before the add sequence existed)
                    if (const char *a = std::getenv("HAS_GVU_REGS_ADD")) add_regs = std::atoi(a) != 0;
                    gvs_->enable_regs(lat, add_regs);
                    tap_->enable_f1();
                }
            // HAS_GVU_ELEM_CLK=1: residual add and max pool as jobs on the clock of the unit instead of the call style above; the DFC thread
            // waits for each job (HasElemwise keeps its overlap of DMA and compute, now along the clock)
            bool elem_clk = regs_on_;
            const char *clk_env = std::getenv("HAS_GVU_ELEM_CLK");   // default on; HAS_GVU_ELEM_CLK=0 keeps the call style (unless HAS_GVU_REGS=1)
            if ((clk_env ? std::atoi(clk_env) : 1) == 1) elem_clk = true;
            if (elem_clk)
                {
                    ElemClock &c = ew_->clk;
                    c.fits_add = &GvuTopStream::add_fits;
                    c.start_add = [this](const int8_t *A, const int8_t *B, int8_t *out, size_t n, int zpA, int zpB, int zpO, int64_t SA, int64_t SB,
                                         int64_t SO, int sA, int sB, int sO, int rm, uint64_t mc)
                    { gvs_->start_add(A, B, out, n, zpA, zpB, zpO, SA, SB, SO, sA, sB, sO, rm, mc); };
                    c.fits_maxpool = [this](int g, int H, int W, int kk, int s, int p) { return gvs_->maxpool_fits_now(g, H, W, kk, s, p); };
                    c.start_maxpool = [this](const int8_t *in, int8_t *out, int g, int H, int W, int kk, int s, int p, uint64_t mc)
                    { gvs_->start_maxpool(in, out, g, H, W, kk, s, p, mc); };
                    if (GvuTopStream::avg_on())
                    {
                        c.fits_avgpool = [this](int g, int H, int W, int kk, int s, int64_t S, int sh, int rm) { return gvs_->avgpool_fits_now(g, H, W, kk, s, S, sh, rm); };
                        c.start_avgpool = [this](const int8_t *in, int8_t *out, int g, int H, int W, int kk, int s, int64_t S, int sh, int rm, uint64_t mc)
                        { gvs_->start_avgpool(in, out, g, H, W, kk, s, S, sh, rm, mc); };
                    }
                    gvu_clk_ = true;
                    c.finish = [this]()
                    {
                        while (!gvs_->job_done()) sc_core::wait(gvs_->job_event());
                        return gvs_->job_cycles();
                    };
                }
            // HAS_GVU_STALL=1: the core stops on o_stream_stall as in psm_top.sv (FIFO of 4 packets full and none leaving: the SRAM-C
            // write is blocked and the whole core holds; the write goes through, once, in the first cycle with room). Off (HAS_GVU_STALL=0):
            // the packet is taken anyway and counted (STALL NEEDED). HAS_GVU_STALL=2 is for tests only: the stall is computed and
            // its edges counted, but not applied (the core never stops).
            const char *stall_env = std::getenv("HAS_GVU_STALL");   // default 1 (the core stops on o_stream_stall); HAS_GVU_STALL=0 switches it off
            stall_mode_ = stall_env ? std::atoi(stall_env) : 1;
            if (stall_mode_ != 1 && stall_mode_ != 2) stall_mode_ = 0;
            stream_stats().stall_mode = stall_mode_;
            if (stall_mode_)
                core_->set_stream_backpressure([this](uint32_t addr, const sramc_mask_t<W32> &m) { return stream_room(addr, m); });
            std::atexit(&GvuTopStream::report);
        }
        // Packets the write hook pushes for an SRAM-C write (one per output channel of its lanes; the same walk as the hook in
        // set_obp_inline); 0 when the write does not go into the stream.
        uint32_t stream_packets_of(uint32_t addr, const sramc_mask_t<W32> &m) const
        {
            if (!obp_inline_ || !inl_active_) return 0;
            const uint32_t e0 = (addr % 1536u) * W32;
            uint32_t n = 0, k = 0;
            while (k < uint32_t(W32))
            {
                if (!m[k]) { k++; continue; }
                const uint32_t x = (e0 + k) / inl_npos_;
                uint32_t j = k;
                for (; j < uint32_t(W32); j++)
                {
                    if (!m[j]) continue;
                    if ((e0 + j) / inl_npos_ != x) break;
                }
                if (x < inl_nch_) n++;
                k = j;
            }
            return n;
        }
        // Backpressure of the core (rtl_ref_npu_top.h, set_stream_backpressure): true when the FIFO of the tap takes the packets of
        // the write the PSM makes in this cycle.
        bool stream_room(uint32_t addr, const sramc_mask_t<W32> &m)
        {
            StreamStats &ss = stream_stats();
            ss.peek_checked = core_->stream_write_peek_checked();
            ss.peek_mismatch = core_->stream_write_peek_mismatch();
            const uint32_t n = stream_packets_of(addr, m);
            if (n == 0 || tap_->in_ready_now(n)) return true;
            if (stall_mode_ == 2)
            {
                ss.predicted_edges++;
                ss.predicted_hash = StreamStats::hash_step(ss.predicted_hash, sc_core::sc_time_stamp().value());
                return true;
            }
            ss.core_stall_cycles++;
            if (n > 1) ss.stall_multi++;
            return false;
        }
        // Start of a GEMM_FUSED layer: the unit takes the table and the parameters of the layer and enters its stream state on its own
        // clock; the DFC waits for o_psum_ready (table load: LUT_ACT_INIT, a few cycles). The unit holds 8 bits of zero point (RTL
        // register GEMM_FUSED [15:8], sign-extended): the beats of a layer whose zero point is outside int8 go as bypassed.
        void stream_begin(const LayerInstr &li)
        {
            if (!stream_on()) return;
            stream_zp_fits_ = GvuTopBridge::zp_fits(li.zp_out);
            if (regs_on_)
            {
                // the layer configuration and the start are register writes; the DFC waits until the last one has been answered
                gvs_->begin_layer_regs(li.act != 0 ? reinterpret_cast<const int8_t *>(stream_lut_) : nullptr, li.zp_out, knobs.round_mode, li.act != 0, knobs.req_narrow);
                gvs_->reg_wait();
            }
            else
            gvs_->begin_layer(li.act != 0 ? reinterpret_cast<const int8_t *>(stream_lut_) : nullptr, li.zp_out, knobs.round_mode, li.act != 0, knobs.req_narrow);
            int g = 0;
            while (!gvs_->ready_for_beats() && g++ < 1000) wait();
            if (g >= 1000) st.framing_errors++;
        }
        void stream_end()
        {
            if (!stream_on()) return;
            if (regs_on_) { gvs_->end_layer_regs(); return; }     // the unit has left GEMM_FUSED by itself; clear the done flag
            gvs_->end_layer();
            int g = 0;
            while (!gvs_->layer_closed() && g++ < 1000) wait();
        }

        // ---- GEMM_FUSED: one layer ----
        void exec_gemm(const LayerInstr &li)
        {
            st.gemm++;
            const LayerGeom &g = li.geom;
            const uint32_t cin = uint32_t(li.in_c), K = cin * uint32_t(g.kh) * uint32_t(g.kw);
            st.real_macs += uint64_t(g.cout) * uint64_t(g.oh) * uint64_t(g.ow) * K;
            // Layer parameters (CH3 in the HW, spec §4.3): LUT, zero point. Scale/shift go per tile (channel range).
            const uint64_t t0 = cycles_now();
            if (li.act != 0)
            {
                uint8_t lut[256];
                if (li.act == 1) for (int x = 0; x < 256; x++) lut[x] = static_cast<uint8_t>(std::max(x - 128, 0));
                else std::memcpy(lut, &(*dram_)[li.lut_addr], 256);
                if (GvuTopBridge::level() >= 2 && obp_inline_)
                {
                    // HAS_GVU_TOP=2: the table takes GvuTop's path, a DMA of 256 bytes into vbank1 and LUT_ACT_INIT, instead
                    // of 32 copies over the host port; the epilogue model gets the same table without bus cycles
                    obp_->load_lut(lut);
                    if (stream_on() && bus_strict_lut)
                    {
                        // HAS_BUS_STRICT_LUT: the 256 bytes as a DMA read (read channel 2, not kept), waited until the channel is idle
                        std::memcpy(stream_lut_, lut, 256);
                        dma_->start_read(2, li.lut_addr, kSinkBank, 0, 256);
                        wait();
                        while (dma_->is_read_active(2)) wait();
                    }
                    else if (stream_on()) { std::memcpy(stream_lut_, lut, 256); wait(static_cast<int>(dma_est(256))); }   // the unit loads it on its own clock (stream_begin)
                    else wait(static_cast<int>(GvuTopBridge::table_cycles(dma_est(256))));
                }
                else
                    for (uint32_t lane = 0; lane < W32; lane++)
                        for (uint32_t e = 0; e < 256; e += 4) obp_write4(LUT_BASE + lane * 256 + e, lut + e);
            }
            obp_write(CFG_BASE + ObpT::REG_ZP_OUT, static_cast<uint32_t>(li.zp_out));
            stream_begin(li);
            st.cfg_cycles += cycles_now() - t0;
            lut_en_.write(li.act != 0);
            if (overlap_) { exec_tiles_ovl(li, K); stream_end(); return; }

            TileIter it(g);
            uint32_t cur_a_bank = 2;
            bool a_ready = false;
            for (long ti = 0; ti < it.count(); ti++)
            {
                const TileGeom tl = it.at(ti);
                const uint32_t nch = uint32_t(tl.x_used), ht = uint32_t(tl.oy1 - tl.oy0), wt = uint32_t(tl.ox1 - tl.ox0);
                const uint32_t npos = ht * wt, ah = uint32_t(tl.iy1 - tl.iy0), aw = uint32_t(tl.ix1 - tl.ix0);
                const uint32_t yu = uint32_t(tl.y_used), nel = nch * npos, words = (nel + W32 - 1) / W32;
                const uint32_t n_ctx = ht * (wt / yu);
                if (uint64_t(nch) * K > uint64_t(kSrambBytes))
                {
                    // cfg-b (weights wider than one buffer) is off in every plan so far; not supported by v1.
                    st.wide_weight_rejected++;
                    st.framing_errors++;
                    continue;
                }
                // feeder padding: the lead in front of the real part is not data, it does not cross the bus
                auto stage_a_read = [&](int bank, const TileGeom &t)
                {
                    const uint32_t n = fill_stage_a(li, t);
                    const PadTile p = pad_tile(li, t);
                    dma_->start_read(1, stage_a_, bank, 0, n, (p.on && n > p.lead) ? n - p.lead : 0);
                };
                if (!a_ready) stage_a_read(int(cur_a_bank), tl);
                const uint32_t swords = stage_words(words);
                std::memset(&(*dram_)[stage_pre_], 0, size_t(swords) * 128);
                for (uint32_t x = 0; x < nch; x++)
                {
                    // padded channels (PAD_TAIL) are computed, never written: no bias, broadcast scale
                    const uint32_t b = (int(tl.c0) + int(x) < g.cout) ? dram_u32(li.bias_addr + 4 * (uint32_t(tl.c0) + x)) : 0u;
                    for (uint32_t p = 0; p < npos; p++)
                        if (uint64_t(x) * npos + p < uint64_t(swords) * W32) std::memcpy(&(*dram_)[stage_pre_ + 4 * (x * npos + p)], &b, 4);
                }
                dma_->start_read(0, li.w_addr + uint32_t(tl.c0) * K, 0, 0, nch * K);
                dma_->start_read(3, stage_pre_, 4, 0, swords * 128);
                wait();
                st.dma_cycles++;
                wait_dma_excl_ch1();
                for (uint32_t x = 0; x < nch; x++)
                {
                    const bool real = int(tl.c0) + int(x) < g.cout;
                    const uint32_t S = (li.per_channel && real) ? dram_u32(li.scale_addr + 4 * (uint32_t(tl.c0) + x)) : li.bcast_S;
                    const uint32_t s = (li.per_channel && real) ? dram_u32(li.shift_addr + 4 * (uint32_t(tl.c0) + x)) : uint32_t(li.bcast_s);
                    obp_write(SCALE_BASE + 4 * x, S);
                    obp_write(SHIFT_BASE + 4 * x, s);
                    st.cfg_cycles += 4;
                }
                obp_write(CFG_BASE + ObpT::REG_NCH, nch);
                st.cfg_cycles += 2;
                wait_ch1();
                const PadTile pd = pad_tile(li, tl);
                std::vector<int8_t> a(pd.on ? size_t(pd.lead) + size_t(cin) * pd.hr * pd.wr : size_t(cin) * ah * aw), w(size_t(nch) * K);
                std::vector<int32_t> pre(size_t(words) * W32);
                sram_->read_bank_data(int(cur_a_bank), 0, reinterpret_cast<uint8_t *>(a.data()), uint32_t(a.size()));
                sram_->read_bank_data(0, 0, reinterpret_cast<uint8_t *>(w.data()), uint32_t(w.size()));
                sram_->read_bank_data(4, 0, reinterpret_cast<uint8_t *>(pre.data()), uint32_t(pre.size() * 4));
                const uint32_t next_a_bank = cur_a_bank == 2 ? 3 : 2;
                if (prefetch_a && ti + 1 < it.count())
                {
                    stage_a_read(int(next_a_bank), it.at(ti + 1));
                    a_ready = true;
                }
                else a_ready = false;
                cur_a_bank = next_a_bank;

                std::vector<int64_t> acc(nel, 0);
#ifdef FE_METRICS
                const sauria_rtl::PerfCounters m0 = perf_;
#endif
                const bool ok = run_core(a, cin, w, nch, uint32_t(g.kh), uint32_t(g.kw), pre, ht, wt, uint32_t(g.sy), nch, yu, acc, &pd,
                                         uint32_t(g.dil));
#ifdef FE_METRICS
                write_metrics(m0, li, ti, cin, uint32_t(g.kh), uint32_t(g.kw), uint32_t(g.sy), nch, ht, wt, yu, n_ctx, ok,
                              uint64_t(nch) * npos * K);
#endif
                if (!ok) st.core_deadlocks++;
                st.tiles++;
                st.macs += uint64_t(nch) * npos * K;

                // OBP over the tile's contexts (as tb_has_net: one vector = one channel x Y_used positions)
                cur_ht_ = ht; cur_wt_ = wt; cur_nch_ = nch; cur_yu_ = yu;
                for (uint32_t ctx = 0; ctx < n_ctx; ctx++)
                {
                    const uint32_t oy = ctx / (wt / yu), cx0 = (ctx % (wt / yu)) * yu;
                    const uint64_t before = obp_rows_written_;
                    for (uint32_t x = 0; x < nch; x++)
                    {
                        psum_vector_t<W32, int32_t> v(0);
                        act_vector_t<W32, int8_t> r(0);
                        sramc_mask_t<W32> m(false);
                        for (uint32_t k = 0; k < yu; k++)
                        {
                            const int64_t av = acc[x * npos + oy * wt + cx0 + k];
                            if (av > INT32_MAX || av < INT32_MIN) st.framing_errors++;
                            v[k] = static_cast<int32_t>(av);
                            m[k] = true;
                        }
                        o_in_.write(v); o_residual_.write(r); o_in_mask_.write(m); o_in_addr_.write(ctx * nch + x);
                        o_in_valid_.write(true);
                        wait();
                        st.obp_cycles++;
                    }
                    o_in_valid_.write(false);
                    int drain = 0;
                    while ((obp_->vectors_out != obp_->vectors_in || obp_rows_written_ - before != nch) && drain < 1000)
                    { wait(); drain++; }
                    st.obp_cycles += uint64_t(drain);
                    if (obp_rows_written_ - before != nch) st.framing_errors++;
                    st.vectors += nch;
                }
                if (tmaj_tile(li, tl))
                {
                    writeback_tmaj(li, tl, nch, ht, wt, swords, 0, true);   // the serial write-back times the whole staging area
                    continue;
                }
                if (dma_nd)
                {
                    // CH2 write-back; int8 scatter/concat into the destination tensor via HasDma 4-D descriptor (HAS_DMA_ND)
                    const uint32_t real_c = uint32_t(std::max(0, std::min(tl.c1, g.cout) - tl.c0));
                    const uint32_t real_w = uint32_t(std::max(0, std::min(tl.ox1, g.ow) - tl.ox0));
                    const DmaNdDesc wb_desc = DmaNdDesc::tile_writeback_3d(
                        li.out_addr, uint32_t(g.cout), uint32_t(g.oh), uint32_t(g.ow),
                        uint32_t(tl.c0), real_c, uint32_t(tl.oy0), ht, uint32_t(tl.ox0), real_w);
                    dma_->start_write_sramc_nd(stage_out_, 4, 0, swords * 128, wb_desc, nch, ht, wt, 0);
                    wait();
                    st.dma_cycles++;
                    wait_dma_excl_ch1();
                    continue;
                }
                // CH2 write-back; int8 scatter into the destination tensor = DMA descriptor (deviation D3, H19)
                dma_->start_write(stage_out_, 4, 0, swords * 128);
                wait();
                st.dma_cycles++;
                wait_dma_excl_ch1();
                for (uint32_t x = 0; x < nch; x++)
                    for (uint32_t p = 0; p < npos; p++)
                    {
                        if (uint64_t(x) * npos + p >= uint64_t(swords) * W32) continue;   // beyond the staging area: not written
                        int32_t val;
                        std::memcpy(&val, &(*dram_)[stage_out_ + 4 * (x * npos + p)], 4);
                        const uint32_t c = uint32_t(tl.c0) + x, y = uint32_t(tl.oy0) + p / wt, xx = uint32_t(tl.ox0) + p % wt;
                        if (c >= uint32_t(g.cout) || xx >= uint32_t(g.ow)) continue;   // PAD_TAIL: computed, not written
                        (*dram_)[li.out_addr + (c * uint32_t(g.oh) + y) * uint32_t(g.ow) + xx] = static_cast<uint8_t>(static_cast<int8_t>(val));
                    }
            }
        }

        // ================================================================ overlapped schedule
        // Ping-pong per category (HAS §6.12, §9.6): i_select bit 0 = A, 1 = B, 2 = C; the DMA fills the HOST half, a flip
        // hands it to the core. Per tile i:  flip -> start core(i) -> [prefetch A (and B if the channel range changes) of
        // tile i+1 | OBP + CH2 write-back of tile i-1 from the C host half | CH3 preload of tile i+1 into that half]
        // -> wait core(i) -> read its PSUM (NPU half, deviation D1) -> wait DMA. Weight blocks are reused when possible (see below).
        sc_bv<3> sel_{"000"};

        void flip(bool a, bool b, bool c)
        {
            if (a) sel_[0] = !sel_[0].to_bool();
            if (b) sel_[1] = !sel_[1].to_bool();
            if (c) sel_[2] = !sel_[2].to_bool();
            core_select_.write(sel_);
            wait(2);
        }
        static uint32_t tile_nel(const TileGeom &t) { return uint32_t(t.x_used) * uint32_t(t.oy1 - t.oy0) * uint32_t(t.ox1 - t.ox0); }
        static uint32_t tile_words(const TileGeom &t) { return (tile_nel(t) + W32 - 1) / W32; }

        // part / cin_t (S1): input channels [part*cin_t, (part+1)*cin_t); weights of a channel block are stored as its Cin
        // parts one after the other (compiler), each part in SAURIA order: W_ADDR + c0*K + part*x_used*Kt.
        // npu_a (halo reuse): the window now in the NPU half of SRAM-A; rows shared with t are copied in SRAM, not re-read.
        void prefetch_ab(const LayerInstr &li, const TileGeom &t, uint32_t K, bool load_w, uint32_t part = 0, uint32_t cin_t = 0,
                         bool load_a = true, const TileGeom *npu_a = nullptr)
        {
            const uint32_t ct = cin_t ? cin_t : uint32_t(li.in_c), Kt = K / uint32_t(li.in_c) * ct;
            if (load_a)
            {
                const uint32_t n = fill_stage_a(li, t, part * ct, ct);
                uint32_t timed = desc3d ? inbounds_a(li, t, ct) : 0;
                if (!desc3d)
                {
                    const PadTile p = pad_tile(li, t);   // feeder padding: the lead is not data, it does not cross the bus
                    if (p.on && n > p.lead) timed = n - p.lead;
                }
                if (npu_a && desc3d && npu_a->ix0 == t.ix0 && npu_a->ix1 == t.ix1 && t.iy0 > npu_a->iy0 && t.iy0 < npu_a->iy1 &&
                    t.iy1 >= npu_a->iy1)
                {
                    const uint32_t h = inbounds_rows(li, t, t.iy0, npu_a->iy1, ct);
                    timed = std::max(1u, inbounds_rows(li, t, npu_a->iy1, t.iy1, ct));
                    halo_tiles++;
                    halo_bytes += h;
                    if (halo_copy_bpc) halo_copy_done_ = cycles_now() + (h + halo_copy_bpc - 1) / halo_copy_bpc;
                }
                dma_->start_read(1, stage_a_, 2, 0, n, timed, DfcCoreDma::extra_a(li.in_h, li.in_w, t.iy0, t.iy1, t.ix0, t.ix1, part * ct, ct));   // HAS_DFC_CORE_DMA
            }
            if (load_w) dma_->start_read(0, li.w_addr + uint32_t(t.c0) * K + part * uint32_t(t.x_used) * Kt, 0, 0, uint32_t(t.x_used) * Kt, 0, DfcCoreDma::extra_b());
        }
        void prefetch_c(const LayerInstr &li, const TileGeom &t)
        {
            const uint32_t nch = uint32_t(t.x_used), npos = uint32_t(t.oy1 - t.oy0) * uint32_t(t.ox1 - t.ox0);
            const uint32_t words = stage_words(tile_words(t));
            std::memset(&(*dram_)[stage_pre_], 0, size_t(words) * 128);
            for (uint32_t x = 0; x < nch; x++)
            {
                const uint32_t b = (int(t.c0) + int(x) < li.geom.cout) ? dram_u32(li.bias_addr + 4 * (uint32_t(t.c0) + x)) : 0u;
                for (uint32_t p = 0; p < npos; p++)
                    if (uint64_t(x) * npos + p < uint64_t(words) * W32) std::memcpy(&(*dram_)[stage_pre_ + 4 * (x * npos + p)], &b, 4);
            }
            // c_bias_bcast (proposal): CH3 moves only the bias vector (4 B per channel); the descriptor
            // replicates it over the tile's positions in SRAM-C. Data is unchanged; only bus bytes/cycles.
            dma_->start_read(3, stage_pre_, 4, 0, words * 128, c_bias_bcast ? 4 * nch : 0, DfcCoreDma::extra_c());
            // HAS_BUS_STRICT: the scale / shift of the tile (inline_params) as one DMA read of 8 bytes a channel on read channel 2
            if (bus_strict && obp_inline_ && li.per_channel) dma_->start_read(2, li.scale_addr + 4 * uint32_t(t.c0), kSinkBank, 0, 8 * nch);
        }
        void wait_dma_all()
        {
            while (dma_->is_any_read_active() || dma_->is_write_active() || cycles_now() < halo_copy_done_) { wait(); st.dma_cycles++; }
        }

        void core_begin(uint32_t cin, const TileGeom &t, const LayerGeom &g, const PadTile &pd)
        {
            using namespace sauria;
            const SauriaTarget *tg = sauria_find_target("int8_32x32");
            core_rstn_.write(false); core_start_.write(false); core_host_wren_.write(false); core_host_rden_.write(false);
            core_threshold_.write(0.5f); core_mvm_k_.write(1); core_total_contexts_.write(1);
            wait(5);
            core_rstn_.write(true);
            wait(2);
            core_wr(CFG_PROFILE_ADDR, uint32_t(PROFILE_V1_SAURIA));
            SauriaLayerDesc desc{};
            desc.B_w = uint32_t(g.kw); desc.B_h = uint32_t(g.kh); desc.d = uint32_t(g.dil); desc.s = uint32_t(g.sy);
            desc.c_til = cin; desc.k_til = uint32_t(t.x_used); desc.h_til = uint32_t(t.oy1 - t.oy0); desc.w_til = uint32_t(t.ox1 - t.ox0);
            desc.X_used = uint32_t(t.x_used); desc.Y_used = uint32_t(t.y_used); desc.preload_en = 1;
            core_apply_cfg(desc, *tg, &pd);
            core_apply_pad(&pd);
            uint64_t f[F_CFG_COUNT];
            sauria_compute_core_fields(desc, *tg, f);
            core_total_contexts_.write(uint32_t(f[F_CFG_NCONTEXTS]));
            // 0 marks CON.INCNTLIM = 0 as a real value (one MAC per context); the core honours it with
            // FX1_A3_INCNTLIM_ZERO_RTL, otherwise it reads 0 as 1 like before
            core_mvm_k_.write(f[F_CFG_INCNTLIM] ? 1 : 0);
            if (tap_ && tap_->f1_on()) tap_->begin_call(uint32_t(f[F_CFG_NCONTEXTS]), inl_call_last_);   // per-context flag: the invocation starts
            wait();
            core_start_.write(true);
            core_busy_open();
            wait(2);
            core_start_.write(false);
        }
        bool core_end(uint32_t nel, std::vector<int64_t> &acc)
        {
            int cyc = 0, consec_deadlock = 0, since_progress = 0;
            uint32_t last_wc = core_->get_psm_recorded_write_count();
            bool got_done = core_done_.read(), got_stuck = false;   // o_done is latched until the next start / reset
            while (!got_done && cyc < core_max_cycles)
            {
                wait();
                cyc++;
                const uint32_t wc = core_->get_psm_recorded_write_count();
                if (wc != last_wc) { since_progress = 0; last_wc = wc; }
                else since_progress++;
                if (core_deadlock_.read()) consec_deadlock++;
                else consec_deadlock = 0;
                if (consec_deadlock >= stall_cycles && since_progress >= stall_cycles) { got_stuck = true; break; }
                got_done = core_done_.read();
            }
            core_busy_close();
            wait(8);
            if (obp_inline_)
            {
                // OBP pipeline tail behind the PSM's last write (latency = HasObp LAT, ESTIMATE until the MAS)
                if (obp_->inline_latency() > 0) wait(obp_->inline_latency());
                st.obp_cycles += uint64_t(obp_->inline_latency());
                if (stream_on())
                {
                    // HAS_GVU_TOP=3: no fixed tail; wait until nothing is left between the PSM and SRAM-C (usually 0 cycles: the last
                    // result is written back the stream depth plus one edge after the last PSM write, o_done comes four edges after it, plus wait(8) above)
                    uint64_t extra = 0;
                    while (!gvs_->drained(*tap_) && extra < 10000) { wait(); extra++; }
                    GvuTopStream::count_drain(extra);
                    st.obp_cycles += extra;
                    if (tap_->f1_on()) tap_->end_call();
                }
                core_rstn_.write(false);
                return got_done && !got_stuck;
            }
            std::vector<int32_t> cbuf(nel, 0);
            core_->read_sram_backdoor(4, 0, reinterpret_cast<uint8_t *>(cbuf.data()), uint32_t(cbuf.size() * sizeof(int32_t)));
            for (uint32_t e = 0; e < nel; e++) acc[e] = int64_t(cbuf[e]);
            core_rstn_.write(false);
            return got_done && !got_stuck;
        }

        // OBP over one finished tile; results land in the C host half (obp_writeback, overlap_ = true).
        void obp_pass(const LayerInstr &li, const TileGeom &t, const std::vector<int64_t> &acc)
        {
            const uint32_t nch = uint32_t(t.x_used), ht = uint32_t(t.oy1 - t.oy0), wt = uint32_t(t.ox1 - t.ox0), npos = ht * wt;
            const uint32_t yu = uint32_t(t.y_used), n_ctx = ht * (wt / yu);
            for (uint32_t x = 0; x < nch; x++)
            {
                obp_write(SCALE_BASE + 4 * x, li.per_channel ? dram_u32(li.scale_addr + 4 * (uint32_t(t.c0) + x)) : li.bcast_S);
                obp_write(SHIFT_BASE + 4 * x, li.per_channel ? dram_u32(li.shift_addr + 4 * (uint32_t(t.c0) + x)) : uint32_t(li.bcast_s));
                st.cfg_cycles += 4;
            }
            obp_write(CFG_BASE + ObpT::REG_NCH, nch);
            st.cfg_cycles += 2;
            cur_ht_ = ht; cur_wt_ = wt; cur_nch_ = nch; cur_yu_ = yu;
            for (uint32_t ctx = 0; ctx < n_ctx; ctx++)
            {
                const uint32_t oy = ctx / (wt / yu), cx0 = (ctx % (wt / yu)) * yu;
                const uint64_t before = obp_rows_written_;
                for (uint32_t x = 0; x < nch; x++)
                {
                    psum_vector_t<W32, int32_t> v(0);
                    act_vector_t<W32, int8_t> r(0);
                    sramc_mask_t<W32> m(false);
                    for (uint32_t k = 0; k < yu; k++)
                    {
                        const int64_t av = acc[x * npos + oy * wt + cx0 + k];
                        if (av > INT32_MAX || av < INT32_MIN) st.framing_errors++;
                        v[k] = static_cast<int32_t>(av);
                        m[k] = true;
                    }
                    o_in_.write(v); o_residual_.write(r); o_in_mask_.write(m); o_in_addr_.write(ctx * nch + x);
                    o_in_valid_.write(true);
                    wait();
                    st.obp_cycles++;
                }
                o_in_valid_.write(false);
                int drain = 0;
                while ((obp_->vectors_out != obp_->vectors_in || obp_rows_written_ - before != nch) && drain < 1000) { wait(); drain++; }
                st.obp_cycles += uint64_t(drain);
                if (obp_rows_written_ - before != nch) st.framing_errors++;
                st.vectors += nch;
            }
        }
        // A1: channel parameters of tile t into the OBP (Scratchpad sbank, see HasObp::set_channel_params).
        void inline_params(const LayerInstr &li, const TileGeom &t)
        {
            const uint32_t nch = uint32_t(t.x_used);
            for (uint32_t x = 0; x < nch; x++)
            {
                const bool real = int(t.c0) + int(x) < li.geom.cout;   // padded channels (PAD_TAIL) are computed, never written
                obp_->set_channel_params(x, (li.per_channel && real) ? dram_u32(li.scale_addr + 4 * (uint32_t(t.c0) + x)) : li.bcast_S,
                                         int((li.per_channel && real) ? dram_u32(li.shift_addr + 4 * (uint32_t(t.c0) + x)) : uint32_t(li.bcast_s)));
            }
            obp_->set_nch(nch);
            inl_nch_ = nch;
            inl_oc_tile_last_ = int(t.c0) + int(t.x_used) >= li.geom.cout;
            inl_npos_ = uint32_t(t.oy1 - t.oy0) * uint32_t(t.ox1 - t.ox0);
        }
        // CH2 from the C host half, then the int8 scatter/concat into the destination tensor via HasDma 4-D descriptor.
        void writeback(const LayerInstr &li, const TileGeom &t)
        {
            const uint32_t nch = uint32_t(t.x_used), ht = uint32_t(t.oy1 - t.oy0), wt = uint32_t(t.ox1 - t.ox0);
            // desc3d: only the REAL elements cross the bus (PAD_TAIL channels / columns are computed, never written)
            const uint32_t real_c = uint32_t(std::max(0, std::min(t.c1, li.geom.cout) - t.c0));
            const uint32_t real_w = uint32_t(std::max(0, std::min(t.ox1, li.geom.ow) - t.ox0));
            const uint32_t real_nel = std::max(1u, real_c * ht * real_w);
            // the overflow of this tile was counted when its preload was staged (prefetch_c)
            const uint32_t swords = std::min(tile_words(t), kStageCBytes / 128u);
            if (tmaj_tile(li, t)) { writeback_tmaj(li, t, nch, ht, wt, swords, desc3d ? real_nel : 0, false); return; }
            if (dma_nd)
            {
                // HAS_DMA_ND: the scatter is done by HasDma at the end of the write, with a 4-D descriptor
                const DmaNdDesc wb_desc = DmaNdDesc::tile_writeback_3d(
                    li.out_addr, uint32_t(li.geom.cout), uint32_t(li.geom.oh), uint32_t(li.geom.ow),
                    uint32_t(t.c0), real_c, uint32_t(t.oy0), ht, uint32_t(t.ox0), real_w);
                // HAS_GVU_OUT_VBANK2: the result goes vbank2 -> DRAM with 1-D commands of the DFC (neo_dma_controller DMA_SEND_VBANK2); the data moves as before, the time is the measured one
                dma_->start_write_sramc_nd(stage_out_, 4, 0, swords * 128, wb_desc, nch, ht, wt, DmaDfc1d::out_vbank2() ? 1u : (desc3d ? real_nel : 0));
                wait();
                while (dma_->is_write_active()) { wait(); st.dma_cycles++; }
                if (DmaDfc1d::out_vbank2())
                {
                    const uint64_t cyc = DmaDfc1d::write_cost(wb_desc);
                    for (uint64_t i = 0; i < cyc; i++) { wait(); st.dma_cycles++; }
                }
                return;
            }
            const uint32_t npos = ht * wt;
            // HAS_GVU_OUT_VBANK2 without the 4-D descriptors (profile neo): the same measured cost of the vbank2 -> DRAM commands as above; the
            // descriptor only describes the destination runs
            const bool vb2 = DmaDfc1d::out_vbank2();
            dma_->start_write(stage_out_, 4, 0, swords * 128, vb2 ? 1u : (desc3d ? real_nel : 0));
            wait();
            while (dma_->is_write_active()) { wait(); st.dma_cycles++; }
            if (vb2)
            {
                const DmaNdDesc wb_desc = DmaNdDesc::tile_writeback_3d(
                    li.out_addr, uint32_t(li.geom.cout), uint32_t(li.geom.oh), uint32_t(li.geom.ow),
                    uint32_t(t.c0), real_c, uint32_t(t.oy0), ht, uint32_t(t.ox0), real_w);
                const uint64_t cyc = DmaDfc1d::write_cost(wb_desc);
                for (uint64_t i = 0; i < cyc; i++) { wait(); st.dma_cycles++; }
            }
            for (uint32_t x = 0; x < nch; x++)
                for (uint32_t p = 0; p < npos; p++)
                {
                    if (uint64_t(x) * npos + p >= uint64_t(swords) * W32) continue;   // beyond the staging area: not written
                    int32_t val;
                    std::memcpy(&val, &(*dram_)[stage_out_ + 4 * (x * npos + p)], 4);
                    const uint32_t c = uint32_t(t.c0) + x, y = uint32_t(t.oy0) + p / wt, xx = uint32_t(t.ox0) + p % wt;
                    if (c >= uint32_t(li.geom.cout) || xx >= uint32_t(li.geom.ow)) continue;   // PAD_TAIL: computed, not written
                    (*dram_)[li.out_addr + (c * uint32_t(li.geom.oh) + y) * uint32_t(li.geom.ow) + xx] =
                        static_cast<uint8_t>(static_cast<int8_t>(val));
                }
        }

        // HAS_OUT_TRANSPOSE: a tile is token-major when GEMM_FUSED wrote X_TMAJ_C0 and the tile starts at or above it. A tile across
        // X_TMAJ_C0 (the compiler did not align it to the channel tile) is counted (tmaj_straddle) and written [C][H][W].
        bool tmaj_tile(const LayerInstr &li, const TileGeom &t)
        {
            if (!out_transpose || li.opcode != 0x12 || li.tmaj_c0 < 0) return false;
            if (t.c0 < li.tmaj_c0 && t.c1 > li.tmaj_c0) { tmaj_straddle++; return false; }
            return t.c0 >= li.tmaj_c0;
        }
        // Write-back of a token-major tile: the corner turn sits between the epilogue and SRAM-C, so the write-back waits for the words
        // of its last context (GvuOutTranspose::tail_edges), then moves the same bytes as the [C][H][W] write-back (CH2, `timed` bus
        // bytes) and stores position p of channel c of the layer at out + c0*OH*OW + p*(OUT_C - c0) + (c - c0), c0 = X_TMAJ_C0.
        // serial: the CH2 wait of the serial schedule (wait_dma_excl_ch1), else the one of the overlapped schedule.
        void writeback_tmaj(const LayerInstr &li, const TileGeom &t, uint32_t nch, uint32_t ht, uint32_t wt, uint32_t swords,
                            uint32_t timed, bool serial)
        {
            const LayerGeom &g = li.geom;
            const uint32_t npos = ht * wt, c0 = uint32_t(li.tmaj_c0), rowc = uint32_t(g.cout) - c0;
            const uint32_t tail = GvuOutTranspose<W32>::tail_edges(npos, uint32_t(t.y_used));
            wait(clk_period_ * double(tail));
            tmaj_tiles++;
            tmaj_tail_cycles += tail;
            dma_->start_write(stage_out_, 4, 0, swords * 128, timed);
            wait();
            if (serial) { st.dma_cycles++; wait_dma_excl_ch1(); }
            else while (dma_->is_write_active()) { wait(); st.dma_cycles++; }
            std::vector<int8_t> cm(size_t(nch) * npos, 0);
            for (uint32_t x = 0; x < nch; x++)
                for (uint32_t p = 0; p < npos; p++)
                {
                    if (uint64_t(x) * npos + p >= uint64_t(swords) * W32) continue;   // beyond the staging area: not written
                    int32_t val;
                    std::memcpy(&val, &(*dram_)[stage_out_ + 4 * (x * npos + p)], 4);
                    cm[size_t(x) * npos + p] = static_cast<int8_t>(val);
                }
            const std::vector<int8_t> tm = GvuOutTranspose<W32>::run(cm, nch, npos, uint32_t(t.y_used));
            for (uint32_t p = 0; p < npos; p++)
                for (uint32_t x = 0; x < nch; x++)
                {
                    if (uint64_t(x) * npos + p >= uint64_t(swords) * W32) continue;
                    const uint32_t c = uint32_t(t.c0) + x, y = uint32_t(t.oy0) + p / wt, xx = uint32_t(t.ox0) + p % wt;
                    if (c >= uint32_t(g.cout) || xx >= uint32_t(g.ow)) continue;   // PAD_TAIL: computed, not written
                    (*dram_)[li.out_addr + c0 * uint32_t(g.oh) * uint32_t(g.ow) + (y * uint32_t(g.ow) + xx) * rowc + (c - c0)] =
                        static_cast<uint8_t>(tm[size_t(p) * nch + x]);
                }
        }

        // Input-channel split: with li.cin_t < in_c every tile runs nsplit core passes over Cin parts. Between passes only A
        // and B flip; C stays on the NPU side, so the psum a pass writes is the preload the next pass reads (no DMA). The
        // epilogue (inline only) is active on the last pass. With nsplit = 1 this is the plain overlapped sequence.
        void exec_tiles_ovl(const LayerInstr &li, uint32_t K)
        {
            const LayerGeom &g = li.geom;
            const uint32_t cin = uint32_t(li.in_c);
            const uint32_t cin_t = (li.cin_t > 0 && li.cin_t < li.in_c) ? uint32_t(li.cin_t) : cin, nsplit = cin / cin_t;
            const uint32_t Kt = K / cin * cin_t;
            LayerGeom go = g;
            if (tile_order_auto && nsplit == 1 && pick_spatial_outer(li, K)) { go.order = 1; s4_layers++; }
            const bool halo = halo_applies(li, nsplit) && go.order != 1;
            if (halo) { go.order = 2; halo_layers++; }
            TileIter it(go);
            const long n = it.count();
            if (uint64_t(std::min(g.cout_t, g.cout)) * Kt > uint64_t(kSrambBytes))
            { st.wide_weight_rejected += uint64_t(n); st.framing_errors++; return; }   // cfg-b: not in v1
            if (nsplit > 1 && !obp_inline_) { st.framing_errors++; return; }             // S1 needs the OBP on the write path
            const long steps = n * long(nsplit);
            TileGeom cur = it.at(0), prev{};
            prefetch_ab(li, cur, K, true, 0, cin_t);
            prefetch_c(li, cur);
            wait_dma_all();
            bool w_new = true, a_new = true, have_prev = false;
            // B ping-pong keeps up to two weight blocks, keyed (c0, Cin part): a block already in the host half is
            // reused by flipping, without a DMA reload (S1: part 0 / part 1 of one channel range alternate every step).
            auto wkey = [nsplit](const TileGeom &t, uint32_t pt) { return int64_t(t.c0) * int64_t(nsplit) + int64_t(pt); };
            int64_t key_npu = -1, key_host = wkey(cur, 0);
            std::vector<int64_t> prev_acc;
            for (long s = 0; s < steps; s++)
            {
                const long i = s / long(nsplit);
                const uint32_t part = uint32_t(s % long(nsplit));
                const bool first = part == 0, last = part + 1 == nsplit;
                cur = it.at(i);
                if (w_new) std::swap(key_npu, key_host);
                flip(a_new, w_new, first);
#ifdef FE_METRICS
                const sauria_rtl::PerfCounters m0 = perf_;
#endif
                if (obp_inline_) { if (first) inline_params(li, cur); inl_active_ = last; }
                inl_call_last_ = last && (i + 1 == n);       // oc_tile_last only in the last call of the layer
                core_begin(cin_t, cur, g, pad_tile(li, cur));
                // ---- overlapped with core(i, part)
                TileGeom nxt{};
                bool next_w = false, next_a = true;
                if (s + 1 < steps)
                {
                    const long ni = (s + 1) / long(nsplit);
                    const uint32_t np = uint32_t((s + 1) % long(nsplit));
                    nxt = it.at(ni);
                    const int64_t nk = wkey(nxt, np);
                    bool load_w = false;
                    if (nk == key_npu) next_w = false;                          // same weights: no flip, no load
                    else if (nk == key_host) next_w = true;                     // already in the other half: flip only
                    else { next_w = true; load_w = true; key_host = nk; }       // DMA into the host half, then flip
                    if (load_w) st.weight_loads++; else if (next_w) st.weight_reuse_flips++;
                    next_a = nsplit > 1 || !tile_order_auto || nxt.iy0 != cur.iy0 || nxt.iy1 != cur.iy1 || nxt.ix0 != cur.ix0 ||
                             nxt.ix1 != cur.ix1;
                    prefetch_ab(li, nxt, K, load_w, np, cin_t, next_a, halo ? &cur : nullptr);
                }
                if (first && have_prev) { if (!obp_inline_) obp_pass(li, prev, prev_acc); writeback(li, prev); }
                if (first && i + 1 < n) prefetch_c(li, it.at(i + 1));
                // ----
                const uint32_t nel = tile_nel(cur);
                std::vector<int64_t> acc(nel, 0);
                const bool ok = core_end(nel, acc);
#ifdef FE_METRICS
                write_metrics(m0, li, s, cin_t, uint32_t(g.kh), uint32_t(g.kw), uint32_t(g.sy), uint32_t(cur.x_used), uint32_t(cur.oy1 - cur.oy0),
                              uint32_t(cur.ox1 - cur.ox0), uint32_t(cur.y_used),
                              uint32_t(cur.oy1 - cur.oy0) * (uint32_t(cur.ox1 - cur.ox0) / uint32_t(cur.y_used)), ok, uint64_t(nel) * Kt);
#endif
                if (!ok) st.core_deadlocks++;
                st.macs += uint64_t(nel) * Kt;
                st.core_passes++;
                wait_dma_all();
                w_new = next_w;
                a_new = next_a;
                if (!last) continue;
                st.tiles++;
                prev = cur;
                prev_acc.swap(acc);
                have_prev = true;
            }
            inl_active_ = true;
            flip(false, false, true);   // the last tile's C half goes to the host side
            if (!obp_inline_) obp_pass(li, prev, prev_acc);
            writeback(li, prev);
        }

        // S4: DMA bytes of the two walk orders (input windows and weight blocks; C traffic is the same in both).
        bool pick_spatial_outer(const LayerInstr &li, uint32_t K) const
        {
            TileIter it(li.geom);
            const uint64_t nc = uint64_t(it.n_cout_tiles()), ns = uint64_t(it.n_spatial_tiles());
            if (nc <= 1) return false;
            uint64_t a_bytes = 0;
            for (uint64_t j = 0; j < ns; j++) a_bytes += inbounds_a(li, it.at(long(j)));   // one channel tile row = all windows
            uint64_t b_bytes = 0;
            for (uint64_t c = 0; c < nc; c++) b_bytes += uint64_t(it.at(long(c * ns)).x_used) * K;
            // channel-outer with halo reuse reads each window's overlap rows once per column instead of once per tile
            uint64_t a_c = a_bytes;
            if (halo_applies(li, 1))
            {
                LayerGeom g2 = li.geom;
                g2.order = 2;
                TileIter i2(g2);
                a_c = 0;
                for (uint64_t j = 0; j < ns; j++)
                {
                    const TileGeom t = i2.at(long(j));
                    const bool cont = j % uint64_t(i2.n_row_tiles()) != 0;
                    a_c += cont ? inbounds_rows(li, t, i2.at(long(j) - 1).iy1, t.iy1, uint32_t(li.in_c)) : inbounds_a(li, t);
                }
            }
            const uint64_t c_outer = a_c * nc + b_bytes, s_outer = a_bytes + b_bytes * ns;
            return s_outer < c_outer;
        }
        bool halo_applies(const LayerInstr &li, uint32_t nsplit) const
        {
            // not with dilation: the overlap of two row tiles is not a plain band of kh - sy rows there (not modelled)
            return halo_reuse && desc3d && nsplit == 1 && li.geom.dil == 1 && li.geom.kh > li.geom.sy &&
                   TileIter(li.geom).n_row_tiles() > 1;
        }
        // In-bounds bytes of rows [ya, yb) of a tile's input window (nc channels).
        static uint32_t inbounds_rows(const LayerInstr &li, const TileGeom &t, int ya, int yb, uint32_t nc)
        {
            const int y0 = std::max(ya, 0), y1 = std::min(yb, li.in_h), x0 = std::max(t.ix0, 0), x1 = std::min(t.ix1, li.in_w);
            if (y1 <= y0 || x1 <= x0) return 0;
            return nc * uint32_t(y1 - y0) * uint32_t(x1 - x0);
        }
        // In-bounds bytes of a tile's input window (what a 3-D descriptor with zero fill actually reads).
        static uint32_t inbounds_a(const LayerInstr &li, const TileGeom &t, uint32_t nc = 0)
        {
            const int y0 = std::max(t.iy0, 0), y1 = std::min(t.iy1, li.in_h), x0 = std::max(t.ix0, 0), x1 = std::min(t.ix1, li.in_w);
            if (y1 <= y0 || x1 <= x0) return 1;
            return (nc ? nc : uint32_t(li.in_c)) * uint32_t(y1 - y0) * uint32_t(x1 - x0);
        }
        // Padding of the input window of one tile when the IFMAP feeder generates it (feeder_pad). on = false: the knob is
        // off, the tile has no padding, the layer is 1x1 stride 1 (aligned address mode), nothing of the window lies inside
        // the image, or a value does not fit its register -- the staged flow is used for that tile.
        static constexpr uint8_t kPadLeadFill = 0x5A;   // content of the lead: never read as data; non-zero so that a masking fault shows
        PadTile pad_tile(const LayerInstr &li, const TileGeom &t) const
        {
            if (!feeder_pad) return PadTile();
            return PadGen::make(li.in_h, li.in_w, t.iy0, t.iy1, t.ix0, t.ix1, li.geom.kh, li.geom.kw, li.geom.sy, pad_val_of(li),
                                SAURIA_ACT_IDX_W, li.geom.dil);
        }
        // Padding registers of the feeder (ACT_PAD_*, rtl_ref_config_map.h) for the next core pass. The core reset before
        // every pass clears them, so a pass without feeder padding writes nothing (and costs no configuration cycle).
        void core_apply_pad(const PadTile *pd)
        {
            if (pd && feeder_pad && !pd->on && pd->outside) st.feeder_pad_fallback++;
            if (!pd || !pd->on) return;
            core_wr(sauria_rtl::ACT_PAD_COL, pd->reg_col());
            core_wr(sauria_rtl::ACT_PAD_ROW_LO, pd->reg_row_lo());
            core_wr(sauria_rtl::ACT_PAD_ROW_HI, pd->reg_row_hi());
            core_wr(sauria_rtl::ACT_PAD_CTRL, pd->reg_ctrl());
            st.feeder_pad_passes++;
        }
        // Padded input window of one tile (padding value of the instruction; 0 = int8 zero point of the present networks),
        // C-order [cin][ah][aw] into the staging area via HasDma 4-D descriptor (DmaNdDesc::tile_window_3d). With feeder_pad on
        // a padded tile: lead + only the real part [cin][hr][wr].
        uint32_t fill_stage_a(const LayerInstr &li, const TileGeom &t, uint32_t ch0 = 0, uint32_t nc = 0)
        {
            const uint32_t ah = uint32_t(t.iy1 - t.iy0), aw = uint32_t(t.ix1 - t.ix0);
            if (nc == 0) nc = uint32_t(li.in_c);
            const PadTile pd = pad_tile(li, t);
            if (pd.on)
            {
                const uint64_t need_r = uint64_t(pd.lead) + uint64_t(nc) * pd.hr * pd.wr;
                if (need_r > kStageABytes) { st.stage_overflow++; st.framing_errors++; }
                for (uint32_t o = 0; o < pd.lead && o < kStageABytes; o++) (*dram_)[stage_a_ + o] = kPadLeadFill;
                const uint32_t sy0 = uint32_t(t.iy0 + int(pd.pt)), sx0 = uint32_t(t.ix0 + int(pd.pl));
                if (dma_nd && need_r <= kStageABytes && pd.hr > 0 && pd.wr > 0)   // HAS_DMA_ND: 4-D descriptor gather
                {
                    const DmaNdDesc src_desc = DmaNdDesc::tile_window_3d(
                        li.in_addr, uint32_t(li.in_c), uint32_t(li.in_h), uint32_t(li.in_w),
                        ch0, nc, sy0, pd.hr, sx0, pd.wr);
                    const DmaNdDesc dst_desc = DmaNdDesc::linear(stage_a_ + pd.lead, nc * pd.hr * pd.wr, 1);
                    dma_->gather_dram_nd(src_desc, dst_desc);
                }
                else
                {
                    for (uint32_t ci = 0; ci < nc; ci++)
                        for (uint32_t y = 0; y < pd.hr; y++)
                            for (uint32_t x = 0; x < pd.wr; x++)
                            {
                                const uint64_t o = uint64_t(pd.lead) + (uint64_t(ci) * pd.hr + y) * pd.wr + x;
                                if (o >= kStageABytes) continue;
                                (*dram_)[stage_a_ + uint32_t(o)] =
                                    (*dram_)[li.in_addr + ((ch0 + ci) * uint32_t(li.in_h) + sy0 + y) * uint32_t(li.in_w) + sx0 + x];
                            }
                }
                return uint32_t(std::min<uint64_t>(need_r, kStageABytes));
            }
            const uint8_t pv = static_cast<uint8_t>(static_cast<int8_t>(pad_val_of(li)));
            const uint64_t need = uint64_t(nc) * ah * aw;
            if (need > kStageABytes) { st.stage_overflow++; st.framing_errors++; }   // the window does not fit SRAM-A either
            if (!dma_nd)
            {
                for (uint32_t ci = 0; ci < nc; ci++)
                    for (uint32_t y = 0; y < ah; y++)
                        for (uint32_t x = 0; x < aw; x++)
                        {
                            const uint64_t o = (uint64_t(ci) * ah + y) * aw + x;
                            if (o >= kStageABytes) continue;
                            const int sy = t.iy0 + int(y), sx = t.ix0 + int(x);
                            uint8_t v = pv;
                            if (sy >= 0 && sx >= 0 && sy < li.in_h && sx < li.in_w)
                                v = (*dram_)[li.in_addr + ((ch0 + ci) * uint32_t(li.in_h) + uint32_t(sy)) * uint32_t(li.in_w) + uint32_t(sx)];
                            (*dram_)[stage_a_ + uint32_t(o)] = v;
                        }
                return uint32_t(std::min<uint64_t>(need, kStageABytes));
            }
            // HAS_DMA_ND: pad value everywhere, then the real part by a 4-D descriptor gather
            const uint64_t cap = std::min<uint64_t>(need, kStageABytes);
            std::memset(dram_->data() + stage_a_, pv, static_cast<size_t>(cap));
            const int sy0 = std::max(0, t.iy0), sy1 = std::min(t.iy1, li.in_h);
            const int sx0 = std::max(0, t.ix0), sx1 = std::min(t.ix1, li.in_w);
            if (need <= kStageABytes && sy1 > sy0 && sx1 > sx0)
            {
                const uint32_t hr = uint32_t(sy1 - sy0), wr = uint32_t(sx1 - sx0);
                const uint32_t dy0 = uint32_t(sy0 - t.iy0), dx0 = uint32_t(sx0 - t.ix0);
                const DmaNdDesc src_desc = DmaNdDesc::tile_window_3d(
                    li.in_addr, uint32_t(li.in_c), uint32_t(li.in_h), uint32_t(li.in_w),
                    ch0, nc, uint32_t(sy0), hr, uint32_t(sx0), wr);
                const DmaNdDesc dst_desc = DmaNdDesc::tile_writeback_3d(
                    stage_a_, nc, ah, aw, 0u, nc, dy0, hr, dx0, wr);
                dma_->gather_dram_nd(src_desc, dst_desc);
            }
            else if (need > kStageABytes)
            {
                for (uint32_t ci = 0; ci < nc; ci++)
                    for (uint32_t y = 0; y < ah; y++)
                        for (uint32_t x = 0; x < aw; x++)
                        {
                            const uint64_t o = (uint64_t(ci) * ah + y) * aw + x;
                            if (o >= kStageABytes) continue;
                            const int sy = t.iy0 + int(y), sx = t.ix0 + int(x);
                            if (sy >= 0 && sx >= 0 && sy < li.in_h && sx < li.in_w)
                                (*dram_)[stage_a_ + uint32_t(o)] =
                                    (*dram_)[li.in_addr + ((ch0 + ci) * uint32_t(li.in_h) + uint32_t(sy)) * uint32_t(li.in_w) + uint32_t(sx)];
                        }
            }
            return uint32_t(cap);
        }

        // ---- one tile through the core (sequential schedule): same steps as tb_has_net::run_tile_via_npu_top, weights already in
        // SAURIA order (compiler), backdoor SRAM load, read-back of SRAM-C while select is still "111" ----
        void core_wr(uint32_t addr, uint32_t val)
        {
            sauria::host_data_t d; sauria::host_mask_t m;
            d[0] = double(val); m[0] = true;
            core_host_addr_.write(addr); core_host_wdata_.write(d); core_host_wmask_.write(m);
            core_host_wren_.write(true); core_host_rden_.write(false);
            wait();
            core_host_wren_.write(false);
            wait();
        }
        void core_wr_bytes(uint32_t addr, uint64_t bits, int lanes, int bits_per_lane)
        {
            sauria::host_data_t d; sauria::host_mask_t m;
            for (int b = 0; b < lanes; b++)
            {
                d[b] = double((bits >> (b * bits_per_lane)) & ((bits_per_lane == 32) ? 0xFFFFFFFFull : 0xFFull));
                m[b] = true;
            }
            core_host_addr_.write(addr); core_host_wdata_.write(d); core_host_wmask_.write(m);
            core_host_wren_.write(true); core_host_rden_.write(false);
            wait();
            core_host_wren_.write(false);
            wait();
        }
        void core_apply_cfg(const sauria::SauriaLayerDesc &desc, const sauria::SauriaTarget &target, const PadTile *pd = nullptr)
        {
            using namespace sauria;
            uint64_t f[F_CFG_COUNT];
            sauria_compute_core_fields(desc, target, f);
            if (pd && pd->on)
            {
                // feeder padding: SRAM-A holds the real part only, so rows and channels step with the real width / height
                // (the kernel rows still step DILATION rows apart)
                f[F_CFG_YLIM] = pd->ylim(desc.B_h);
                f[F_CFG_YSTEP] = pd->ystep();
                f[F_CFG_CHLIM] = pd->chlim(desc.c_til);
                f[F_CFG_CHSTEP] = pd->chstep();
                f[F_CFG_TIL_YLIM] = pd->til_ylim(desc.s, desc.h_til);
                f[F_CFG_TIL_YSTEP] = pd->til_ystep(desc.s);
            }
            core_wr(CFG_CON_OFFSET + 0x00, uint32_t(f[F_CFG_INCNTLIM]));
            core_wr(CFG_CON_OFFSET + 0x04, uint32_t(f[F_CFG_ACT_REPS]));
            core_wr(CFG_CON_OFFSET + 0x08, uint32_t(f[F_CFG_WEI_REPS]));
            core_wr_bytes(CFG_ACT_OFFSET + 0x00, desc.Y_used >= 32 ? 0xFFFFFFFFull : ((1ull << desc.Y_used) - 1), 4, 8);
            core_wr(CFG_ACT_OFFSET + 0x04, uint32_t(f[F_CFG_INCNTLIM]) + 1);
            core_wr(CFG_ACT_OFFSET + 0x08, uint32_t(target.Y));
            core_wr(CFG_ACT_OFFSET + 0x0C, uint32_t(f[F_CFG_XLIM]));
            core_wr(CFG_ACT_OFFSET + 0x10, uint32_t(target.Y));
            core_wr(CFG_ACT_OFFSET + 0x28, uint32_t(f[F_CFG_DIL_PAT] & 0xFFFFFFFFu));
            core_wr(CFG_ACT_OFFSET + 0x40, uint32_t(f[F_CFG_DIL_PAT] >> 32));
            core_wr(CFG_ACT_OFFSET + 0x44, uint32_t(desc.s));
            core_wr(CFG_ACT_OFFSET + 0x14, uint32_t(f[F_CFG_XLIM]));
            core_wr(CFG_ACT_OFFSET + 0x18, uint32_t(f[F_CFG_XSTEP]));
            core_wr(CFG_ACT_OFFSET + 0x1C, uint32_t(f[F_CFG_YLIM]));
            core_wr(CFG_ACT_OFFSET + 0x20, uint32_t(f[F_CFG_YSTEP]));
            core_wr(CFG_ACT_OFFSET + 0x24, uint32_t(f[F_CFG_CHLIM]));
            core_wr(CFG_ACT_OFFSET + 0x2C, uint32_t(f[F_CFG_CHSTEP]));
            core_wr(CFG_ACT_OFFSET + 0x30, uint32_t(f[F_CFG_TIL_XLIM]));
            core_wr(CFG_ACT_OFFSET + 0x34, uint32_t(f[F_CFG_TIL_XSTEP]));
            core_wr(CFG_ACT_OFFSET + 0x38, uint32_t(f[F_CFG_TIL_YLIM]));
            core_wr(CFG_ACT_OFFSET + 0x3C, uint32_t(f[F_CFG_TIL_YSTEP]));
            core_wr(CFG_WEI_OFFSET + 0x04, uint32_t(f[F_CFG_WLIM]));
            core_wr(CFG_WEI_OFFSET + 0x08, uint32_t(target.X));
            core_wr(CFG_WEI_OFFSET + 0x10, uint32_t(f[F_CFG_WLIM]));
            core_wr(CFG_WEI_OFFSET + 0x14, uint32_t(f[F_CFG_WSTEP]));
            core_wr(CFG_WEI_OFFSET + 0x18, uint32_t(f[F_CFG_KLIM]));
            core_wr(CFG_WEI_OFFSET + 0x1C, uint32_t(f[F_CFG_KSTEP]));
            core_wr(CFG_WEI_OFFSET + 0x20, uint32_t(f[F_CFG_TIL_KLIM]));
            core_wr(CFG_WEI_OFFSET + 0x24, uint32_t(f[F_CFG_TIL_KSTEP]));
            core_wr_bytes(WEI_COLS_ACTIVE, f[F_CFG_COLS_ACTIVE], 2, 32);
            core_wr(CFG_WEI_OFFSET + 0x2C, uint32_t(f[F_CFG_WALIGNED]));
            core_wr(NCONTEXTS, uint32_t(f[F_CFG_NCONTEXTS]));
            core_wr(CFG_OUT_OFFSET + 0x04, uint32_t(f[F_CFG_CXLIM]));
            core_wr(CFG_OUT_OFFSET + 0x08, uint32_t(f[F_CFG_CXSTEP]));
            core_wr(CFG_OUT_OFFSET + 0x0C, uint32_t(f[F_CFG_CKLIM]));
            core_wr(CFG_OUT_OFFSET + 0x10, uint32_t(f[F_CFG_CKSTEP]));
            core_wr(TIL_CYLIM, uint32_t(f[F_CFG_TIL_CYLIM]));
            core_wr(TIL_CYSTEP, uint32_t(f[F_CFG_TIL_CYSTEP]));
            core_wr(TIL_CKLIM, uint32_t(f[F_CFG_TIL_CKLIM]));
            core_wr(TIL_CKSTEP, uint32_t(f[F_CFG_TIL_CKSTEP]));
            core_wr(INACTIVE_COLS, uint32_t(f[F_CFG_INACTIVE_COLS]));
            core_wr(PRELOAD_EN, uint32_t(f[F_CFG_PRELOAD_EN]));
            core_wr(CFG_ACT_BASE_ADDR, 0);
            core_wr(CFG_WEI_BASE_ADDR, 0);
            core_wr(CFG_OUT_BASE_ADDR, 0);
        }

        bool run_core(const std::vector<int8_t> &a, uint32_t cin, const std::vector<int8_t> &wgt_sauria, uint32_t nch, uint32_t kh,
                      uint32_t kw, const std::vector<int32_t> &pre, uint32_t ht, uint32_t wt, uint32_t sy, uint32_t x_used,
                      uint32_t y_used, std::vector<int64_t> &acc, const PadTile *pd = nullptr, uint32_t dil = 1)
        {
            using namespace sauria;
            const SauriaTarget *t = sauria_find_target("int8_32x32");
            const uint32_t nel = nch * ht * wt;
            // 1. reset, select 000 (host side = buffer 0)
            core_rstn_.write(false); core_start_.write(false); core_host_wren_.write(false); core_host_rden_.write(false);
            core_threshold_.write(0.5f); core_mvm_k_.write(1); core_total_contexts_.write(1); core_select_.write(sc_bv<3>("000"));
            wait(5);
            core_rstn_.write(true);
            wait(2);
            core_wr(CFG_PROFILE_ADDR, uint32_t(PROFILE_V1_SAURIA));
            // 2. SRAM A / C(preload) / B through the backdoor (deviation D2)
            core_->load_sram_backdoor(2, 0, reinterpret_cast<const uint8_t *>(a.data()), uint32_t(a.size()));
            core_->load_sram_backdoor(4, 0, reinterpret_cast<const uint8_t *>(pre.data()), uint32_t(pre.size() * sizeof(int32_t)));
            core_->load_sram_backdoor(0, 0, reinterpret_cast<const uint8_t *>(wgt_sauria.data()), uint32_t(wgt_sauria.size()));
            // 3. swap to the NPU side
            core_select_.write(sc_bv<3>("111"));
            wait(2);
            // 4-6. configuration, contexts, mvm_k before start
            SauriaLayerDesc desc{};
            desc.B_w = kw; desc.B_h = kh; desc.d = dil; desc.s = sy;
            desc.c_til = cin; desc.k_til = nch; desc.h_til = ht; desc.w_til = wt;
            desc.X_used = x_used; desc.Y_used = y_used; desc.preload_en = 1;
            core_apply_cfg(desc, *t, pd);
            core_apply_pad(pd);
            {
                uint64_t f[F_CFG_COUNT];
                sauria_compute_core_fields(desc, *t, f);
                core_total_contexts_.write(uint32_t(f[F_CFG_NCONTEXTS]));
                core_mvm_k_.write(f[F_CFG_INCNTLIM] ? 1 : 0);   // see core_begin
            }
            wait();
            // 7. start pulse (2 cycles)
            core_start_.write(true);
            core_busy_open();
            wait(2);
            core_start_.write(false);
            // 8. wait for done; stuck = deadlock held AND no PSM progress for stall_cycles
            int cyc = 0, consec_deadlock = 0, since_progress = 0;
            uint32_t last_wc = core_->get_psm_recorded_write_count();
            bool got_done = false, got_stuck = false;
            while (cyc < core_max_cycles)
            {
                wait();
                cyc++;
                const uint32_t wc = core_->get_psm_recorded_write_count();
                if (wc != last_wc) { since_progress = 0; last_wc = wc; }
                else since_progress++;
                if (core_deadlock_.read()) consec_deadlock++;
                else consec_deadlock = 0;
                if (consec_deadlock >= stall_cycles && since_progress >= stall_cycles) { got_stuck = true; break; }
                if (core_done_.read()) { got_done = true; break; }
            }
            core_busy_close();
            wait(8);
            // 9. read SRAM-C while select is still 111 (NPU-side buffer holds the result)
            std::vector<int32_t> cbuf(nel, 0);
            core_->read_sram_backdoor(4, 0, reinterpret_cast<uint8_t *>(cbuf.data()), uint32_t(cbuf.size() * sizeof(int32_t)));
            for (uint32_t e = 0; e < nel; e++) acc[e] = int64_t(cbuf[e]);
            core_select_.write(sc_bv<3>("000"));
            wait(2);
            core_rstn_.write(false);   // keep the idle core in reset (simulation speed, S4)
            return got_done && !got_stuck;
        }

#ifdef FE_METRICS
        void write_metrics(const sauria_rtl::PerfCounters &m0, const LayerInstr &li, long ti, uint32_t cin, uint32_t kh, uint32_t kw,
                           uint32_t sy, uint32_t nch, uint32_t ht, uint32_t wt, uint32_t yu, uint32_t n_ctx, bool ok, uint64_t macs)
        {
            if (!metrics_csv) return;
            if (!metrics_header_)
            {
                std::fprintf(metrics_csv, "step,tile,cin,kh,kw,sy,nch,ht,wt,yu,n_ctx,ok,ticks_all,busy,exec,pe_cycles,mac_nz,macs_theory,"
                                          "a_rd_beats,a_rd_bytes,b_rd_beats,b_rd_bytes,c_rd_beats,c_rd_bytes,c_wr_beats,c_wr_bytes");
                for (int q = 1; q <= 24; q++) std::fprintf(metrics_csv, ",st%02d", q);
                std::fprintf(metrics_csv, "\n");
                metrics_header_ = true;
            }
            const sauria_rtl::PerfCounters &p1 = perf_;
            uint64_t d_all = 0, d_busy = 0;
            for (int q = 0; q < sauria_rtl::PerfCounters::NUM_CTRL_STATES; q++)
            {
                const uint64_t d = p1.ctrl_state_cycles[q] - m0.ctrl_state_cycles[q];
                d_all += d;
                if (q != 0) d_busy += d;
            }
            auto U = [](uint64_t v) { return (unsigned long long)v; };
            std::fprintf(metrics_csv, "%u,%ld,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu",
                         li.host_seq - 1, ti, cin, kh, kw, sy, nch, ht, wt, yu, n_ctx, ok ? 1 : 0, U(d_all), U(d_busy),
                         U(p1.exec_cycles - m0.exec_cycles), U(p1.total_pe_cycles - m0.total_pe_cycles), U(p1.mac_ops - m0.mac_ops), U(macs),
                         U(p1.srama_read_beats - m0.srama_read_beats), U(p1.srama_read_bytes - m0.srama_read_bytes),
                         U(p1.sramb_read_beats - m0.sramb_read_beats), U(p1.sramb_read_bytes - m0.sramb_read_bytes),
                         U(p1.sramc_read_beats - m0.sramc_read_beats), U(p1.sramc_read_bytes - m0.sramc_read_bytes),
                         U(p1.sramc_write_beats - m0.sramc_write_beats), U(p1.sramc_write_bytes - m0.sramc_write_bytes));
            for (int q = 1; q <= 24; q++) std::fprintf(metrics_csv, ",%llu", U(p1.ctrl_state_cycles[q] - m0.ctrl_state_cycles[q]));
            std::fprintf(metrics_csv, "\n");
            std::fflush(metrics_csv);
        }
        sauria_rtl::PerfCounters perf_;
        bool metrics_header_{false};
#endif

        // ================================================================ FUSED_ATTN and LAYERNORM through the vector unit
        // Data path bit-exact (gvu_fused_attn.h / gvu_layernorm.h, validated at unit level) fed from DRAM by the DFC;
        // timing = ESTIMATE, serial: DMA in (CH3 parameter block + operands) -> compute -> DMA out, nothing overlapped.
        //   DMA      ceil(bytes / burst) * burst_beats + dram_latency per transfer (the HasDma formula, AXI params of the run)
        //   Q.K^T / A.V (without rce_core): the measured per-pass tile rule, per 32-channel pass
        //            n_ctx * max(K, 230) + 281 with n_ctx = ceil(NQ / 32); with rce_core the products run on the core
        //   SOFTMAX  softmax_cycles(L) per group of 32 rows (gvu_softmax.h)      LAYERNORM ln_cycles(H) per group of 32 rows
        uint64_t dma_est(uint64_t bytes) const
        {
            const DmaParams &p = dma_->prm;
            return bytes ? (bytes + p.burst_bytes() - 1) / p.burst_bytes() * p.burst_beats + p.dram_latency : 0;
        }
        static uint64_t core_pass_est(uint64_t npos, uint64_t K) { return (npos + 31) / 32 * std::max<uint64_t>(K, 230) + 281; }
        static uint64_t ln_cycles(uint64_t H) { return (H + 3) + (H + 5) + 20 + (H + 10) + (H + 10); }   // stages 1, 2, 3-4, 5
        // FLAGS bit 1 CHANNEL_MAJOR: element (token i, channel j) at addr + j*rows + i -- the [C][tokens] layout a
        // GEMM_FUSED (1x1, pixel = token) writes, read by a strided 3-D descriptor. Otherwise token-major addr + i*cols + j.
        static uint32_t rce_off(bool cm, int i, int j, int rows, int cols) { return uint32_t(cm ? j * rows + i : i * cols + j); }
        Mat read_mat(uint32_t addr, int rows, int cols, bool cm = false) const
        {
            Mat m(static_cast<size_t>(rows), std::vector<int>(static_cast<size_t>(cols), 0));
            for (int i = 0; i < rows; i++)
                for (int j = 0; j < cols; j++) m[i][j] = int(static_cast<int8_t>((*dram_)[addr + rce_off(cm, i, j, rows, cols)]));
            return m;
        }
        // rows x cols int8, row i at addr + i * pitch (token-major operand inside a wider tensor)
        Mat read_rows(uint32_t addr, int rows, int cols, int pitch) const
        {
            Mat m(static_cast<size_t>(rows), std::vector<int>(static_cast<size_t>(cols), 0));
            for (int i = 0; i < rows; i++)
                for (int j = 0; j < cols; j++) m[i][j] = int(static_cast<int8_t>((*dram_)[addr + uint32_t(i) * uint32_t(pitch) + uint32_t(j)]));
            return m;
        }
        void rce_wait(uint64_t c) { wait(clk_period_ * double(c)); st.rce_cycles += c; }
        // Rows of `n` elements one group of the 32 lanes takes: 32, or with Knobs::sp_limit_rows what the banked Scratchpad
        // holds. A scalar bank too small for the instruction is counted (rce_sp_short) and then left out of the limit.
        uint64_t rce_group_rows(SpPipeline pl, uint64_t n)
        {
            if (!knobs.sp_limit_rows) return 32;
            int r = sp_max_rows(pl, n, knobs);
            if (r == 0)
            {
                Knobs k2 = knobs;
                k2.sp_s = 0;
                if (sp_max_rows(pl, n, k2) > 0) st.rce_sp_short++;
                r = std::max(1, sp_max_rows(pl, n, k2));
            }
            return uint64_t(r);
        }

        void exec_attn(const LayerInstr &li)
        {
            const int NQ = li.rows, L = li.seq_len, D = li.head_dim;
            AttnParams p;
            std::vector<int32_t> exp;
            if (!parse_attn_block(*dram_, li.param_addr, p, exp)) { st.rce_param_errors++; st.framing_errors++; return; }
            const bool cm = (li.flags & 2u) != 0;   // mask stays [NQ][L] token-major
            const Mat Q = read_mat(li.a_addr, NQ, D, cm), K = read_mat(li.b_addr, L, D, cm);
            // FLAGS bit 2: V token-major, rows li.v_stride apart (a head of a GEMM_FUSED output written with X_TMAJ_C0) -- the layout
            // the weights of A.V need; otherwise V as the other operands (reordered for the weights here, no cycles)
            const Mat V = li.v_stride > 0 ? read_rows(li.v_addr, L, D, li.v_stride) : read_mat(li.v_addr, L, D, cm);
            const Mat M = li.mask_addr ? read_mat(li.mask_addr, NQ, L) : Mat(size_t(NQ), std::vector<int>(size_t(L), 0));
            const LutIndirect recip(14, knobs.lut_mas_depth != 0, knobs.lut_zero0 != 0);
            const uint64_t t0 = cycles_now();
            // HAS_GVU_SOFTMAX=1 (with HAS_GVU_TOP=3): the softmax rows run on GvuTop in groups of 32 rows (one per lane); GvuTop's rows go
            // into A.V. A group with a masked position stays on the model (its estimate is added). With HAS_GVU_ELEM_CLK=1 (default) the
            // groups are one job on the clock (the simulated time passes while the DFC waits); otherwise the call style, whose cycles are added.
            uint64_t smx_cyc = 0, smx_st1 = 0, smx_model = 0, job_time = 0;
            bool smx_on = false, smx_all = false;
            SoftmaxFn smx;
            if (gvs_ && GvuTopStream::softmax_on())
                smx = [&](const Mat &X, Mat &A, const std::vector<std::vector<uint8_t>> &zeros)
                {
                    const SoftmaxCfg sc;
                    const std::vector<int16_t> ri = recip.mas_recip_image();
                    gvs_->softmax_begin(exp.data(), std::vector<int32_t>(ri.begin(), ri.end()));
                    smx_on = true; smx_all = true;
                    const uint64_t mc = softmax_cycles(uint64_t(L), knobs.lut_stages);
                    std::vector<Mat> xg;
                    std::vector<std::vector<std::vector<int> *>> ag;
                    for (size_t i0 = 0; i0 < X.size(); i0 += 32)
                    {
                        const size_t n = std::min<size_t>(32, X.size() - i0);
                        bool masked = false;
                        std::vector<const std::vector<int> *> xs;
                        std::vector<std::vector<int> *> as;
                        for (size_t i = i0; i < i0 + n; i++)
                        {
                            xs.push_back(&X[i]); as.push_back(&A[i]);
                            if (!zeros.empty()) for (uint8_t z : zeros[i]) masked = masked || z;
                        }
                        if (masked || (gvu_clk_ && !gvs_->softmax_fits_now(L, sc.S, sc.s, sc.zp))) { smx_all = false; st.rce_smx_model++; smx_model += mc; continue; }
                        if (gvu_clk_)
                        {
                            xg.emplace_back();
                            for (const auto *r : xs) xg.back().push_back(*r);
                            ag.push_back(as);
                            continue;
                        }
                        uint64_t gc = 0;
                        if (!gvs_->softmax_group(xs, as, L, sc.S, sc.s, sc.zp, mc, &gc)) { smx_all = false; st.rce_smx_model++; smx_model += mc; }
                        else { smx_cyc += gc; smx_st1 += gvs_->softmax_last_st1(); }
                    }
                    if (gvu_clk_ && !xg.empty())
                    {
                        gvs_->start_softmax(xg, ag, L, sc.S, sc.s, sc.zp, mc);
                        while (!gvs_->job_done()) wait(gvs_->job_event());
                        job_time += gvs_->job_cycles();
                        if (gvs_->job_ok()) smx_st1 += gvs_->softmax_job_st1();
                        else { smx_all = false; smx_model += mc * xg.size(); st.rce_smx_model += xg.size(); }
                    }
                };
            // HAS_GVU_FA=1 (with HAS_GVU_SOFTMAX=1): the requant of phase 1 (Q.K^T) and of phase 3 (A.V, -Za.Vcol by the preload of the partial
            // sums) as GEMM_FUSED stream beats on GvuTop, GvuTop's values kept. Call style: the beats are taken to stream behind the products
            // (inline, as GEMM_FUSED), so each phase adds its stream tail and its register writes, and the softmax row maximum (ST1) rides on
            // the beats of phase 1 (proposal FA_ST1, its ticks leave the softmax cycles). On the clock (HAS_GVU_ELEM_CLK=1): the phase is a job
            // AFTER the products of the core (every beat costs an edge: the inline stream of the proposal is not modelled), no ST1 credit.
            // The mask is applied to the scores after the requant (counted).
            uint64_t fa_tail = 0;
            bool fa_all = false;
            RequantFn req;
            if (gvs_ && GvuTopStream::fa_on())
            {
                fa_all = true;
                req = [&](int, const Mat64 &acc, Mat &out, int64_t Mr, int TSr, int Zr)
                {
                    if (gvu_clk_)
                    {
                        gvs_->start_requant(acc, &out, Mr, TSr, Zr, knobs.round_mode, knobs.req_narrow);
                        while (!gvs_->job_done()) wait(gvs_->job_event());
                        job_time += gvs_->job_cycles();
                        if (!gvs_->job_ok()) fa_all = false;
                        return;
                    }
                    uint64_t gc = 0;
                    // the phase adds its stream tail and, through the register file, the writes that start it (gc minus the beats)
                    if (gvs_->requant_rows(acc, out, Mr, TSr, Zr, knobs.round_mode, knobs.req_narrow, &gc)) fa_tail += GvuTopStream::kDepth + 1 + gvs_->last_reg_cycles();
                    else fa_all = false;
                };
                if (li.mask_addr) for (const auto &row : M) for (int v : row) stream_stats().fa_masked += v == -128;
            }
            // HAS_GVU_FA_OVERLAP=1: phase q (0 = Q.K^T, 1 = A.V) runs on the core with its zero-point corrections in the preload of the
            // partial sums; after each pass the DFC waits for the job of the previous pass (exposed), then starts the requant of this
            // pass on GvuTop and lets the core go on. The req hook then takes GvuTop's values when the sums it gets are the ones the
            // jobs had; otherwise (a phase whose corrections or sums do not fit int32) the phase runs serially as without the knob.
            const bool ovl = fa_overlap && rce_core && gvu_clk_ && gvs_ && GvuTopStream::fa_on();
            int ovl_q = 0, ovl_jq = 0;
            bool ovl_busy = false, ovl_ok[2] = {false, false};
            Mat64 ovl_acc[2], ovl_blk;
            Mat ovl_val[2], ovl_out;
            uint32_t ovl_p0 = 0, ovl_n0 = 0;
            auto ovl_wait = [&]()
            {
                if (!ovl_busy) return;
                const uint64_t w0 = cycles_now();
                while (!gvs_->job_done()) wait(gvs_->job_event());
                const uint64_t w = cycles_now() - w0;
                job_time += w; fa_ovl_wait_ += w;
                ovl_busy = false;
                if (!gvs_->job_ok()) { ovl_ok[ovl_jq] = false; return; }
                for (size_t x = 0; x < ovl_out.size(); x++)
                    for (size_t k = 0; k < ovl_out[x].size(); k++) ovl_val[ovl_jq][ovl_p0 + x][ovl_n0 + k] = ovl_out[x][k];
            };
            DotFn dot_ovl = [&](const Mat &X, const Mat &Y)
            {
                const int q = std::min(ovl_q++, 1);
                const size_t n = X.size(), m = Y.size(), cin = X[0].size();
                const int Zq = p.asym_zp ? p.Zq : 0, Zk = p.asym_zp ? p.Zk : 0, Zv = p.asym_zp ? p.Zv : 0, Za = SoftmaxCfg().zp;
                std::vector<int64_t> xr(n, 0), yr(m, 0);
                for (size_t i = 0; i < n; i++) for (int v : X[i]) xr[i] += v;
                for (size_t j = 0; j < m; j++) for (int v : Y[j]) yr[j] += v;
                Mat64 C(n, std::vector<int64_t>(m, 0));
                const int64_t lim = (int64_t(1) << 31) - 1 - int64_t(cin) * 16384;   // |corr| + |product| inside int32
                bool fits = lim > 0;
                for (size_t i = 0; i < n && fits; i++)
                    for (size_t j = 0; j < m; j++)
                    {
                        const int64_t c = q == 0 ? -int64_t(Zk) * xr[i] - int64_t(Zq) * yr[j] + int64_t(cin) * Zq * Zk
                                                 : -int64_t(Za) * yr[j] + (p.av_zv_corr ? -int64_t(Zv) * xr[i] + int64_t(cin) * Za * Zv : 0);
                        if (c > lim || c < -lim) { fits = false; break; }
                        C[i][j] = c;
                    }
                ovl_ok[q] = fits;
                if (!fits) return core_dot(X, Y);
                ovl_acc[q].assign(n, std::vector<int64_t>(m, 0));
                ovl_val[q].assign(n, std::vector<int>(m, 0));
                const int64_t Mr = q == 0 ? p.Mqk : p.Mav;
                const int TSr = q == 0 ? p.TSqk : p.TSav, Zr = q == 0 ? p.Zqk : p.Zav;
                const Mat64 r = core_dot(X, Y, &C, [&, q, Mr, TSr, Zr](uint32_t p0, uint32_t np, uint32_t n0, uint32_t nch, uint32_t wt,
                                                                      const std::vector<int64_t> &a)
                {
                    ovl_wait();
                    ovl_blk.assign(np, std::vector<int64_t>(nch, 0));
                    ovl_out.assign(np, std::vector<int>(nch, 0));
                    for (uint32_t x = 0; x < np; x++)
                        for (uint32_t k = 0; k < nch; k++)
                        {
                            const int64_t v = a[size_t(k) * wt + x];
                            ovl_blk[x][k] = v; ovl_acc[q][p0 + x][n0 + k] = v;
                            ovl_out[x][k] = requant(v, Mr, TSr, Zr, knobs);
                        }
                    ovl_p0 = p0; ovl_n0 = n0; ovl_jq = q; ovl_busy = true;
                    gvs_->start_requant(ovl_blk, &ovl_out, Mr, TSr, Zr, knobs.round_mode, knobs.req_narrow);
                    fa_ovl_jobs_++;
                });
                ovl_wait();
                return r;
            };
            if (ovl)
            {
                const RequantFn req_serial = req;
                req = [&, req_serial](int ph, const Mat64 &acc, Mat &out, int64_t Mr, int TSr, int Zr)
                {
                    const int q = ph == 1 ? 0 : 1;
                    if (ovl_ok[q] && acc == ovl_acc[q]) { out = ovl_val[q]; fa_ovl_phases_++; return; }
                    fa_ovl_serial_++;
                    req_serial(ph, acc, out, Mr, TSr, Zr);
                };
            }
            const AttnResult r = rce_core ? fused_attn(Q, K, V, M, p, exp.data(), recip, knobs,
                                                       ovl ? dot_ovl : DotFn([this](const Mat &X, const Mat &Y) { return core_dot(X, Y); }), smx, req)
                                          : fused_attn(Q, K, V, M, p, exp.data(), recip, knobs, dot_cpu, smx, req);
            if (rce_core)
            {
                const uint64_t core_t = cycles_now() - t0 - job_time;   // the jobs of the vector unit are counted below, not as core time
                st.rce_core_cycles += core_t; st.rce_cycles += core_t;
                st.real_macs += 2 * uint64_t(NQ) * uint64_t(L) * uint64_t(D);   // Q.K^T and A.V
            }
            st.rce_cycles += job_time;
            for (int i = 0; i < NQ; i++)
                for (int n = 0; n < D; n++) (*dram_)[li.out_addr + rce_off(cm, i, n, NQ, D)] = static_cast<uint8_t>(static_cast<int8_t>(r.O[i][n]));
            const uint64_t nq = uint64_t(NQ), l = uint64_t(L), d = uint64_t(D);
            uint64_t c = dma_est(kAttnBlockBytes) + dma_est(nq * d) + dma_est(l * d) + dma_est(l * d) + (li.mask_addr ? dma_est(nq * l) : 0);
            if (!rce_core) c += (l + 31) / 32 * core_pass_est(nq, d) + (d + 31) / 32 * core_pass_est(nq, l);
            const uint64_t ga = rce_group_rows(SpPipeline::Softmax, l), na = (nq + ga - 1) / ga;
            // GvuTop softmax: its cycles (call style; on the clock they have passed already) plus the DMA of the two tables into vbank1
            // (6 words of 128 bytes, once per instruction), and the estimate of the groups left on the model
            if (!smx_on) c += na * softmax_cycles(l, knobs.lut_stages);
            else if (!gvu_clk_ && !smx_all) c += na * softmax_cycles(l, knobs.lut_stages);
            else c += smx_cyc + smx_model + dma_est(6 * 128);
            if (!gvu_clk_ && smx_all && fa_all) { c = c - smx_st1 + fa_tail; stream_stats().fa_st1_hidden += smx_st1; }
            st.rce_groups += na;
            c += dma_est(nq * d);
            if (bus_strict) c += 2 * dma_est(nq * l);   // HAS_BUS_STRICT: A of A.V out of the vector unit, then into SRAM-A
            rce_wait(c);
            st.attn++;
            st.rce_rows += nq;
            st.rce_ovf16 += r.ovf16;
        }

        // X [npos][cin] . Y [nout][cin]^T on the core as a 1x1 layer: activation = X^T padded to a multiple of 32 positions, weights = Y
        // (SAURIA order c*nch + k for kh = kw = 1), 32 output channels per pass, positions split to fit SRAM-A / SRAM-C.
        // The OBP hook is switched off (raw int32 psums are needed) and the ping-pong select of the GEMM path is restored after.
        // HAS_GVU_FA_OVERLAP: `pre_m` [npos][nout] goes into the preload of the partial sums (the products returned without it) and
        // `pass` sees the sums of every pass (preload included, [k * wt + x]) as soon as the pass ends.
        typedef std::function<void(uint32_t p0, uint32_t np, uint32_t n0, uint32_t nch, uint32_t wt, const std::vector<int64_t> &acc)> PassFn;
        Mat64 core_dot(const Mat &X, const Mat &Y, const Mat64 *pre_m = nullptr, const PassFn &pass = PassFn())
        {
            const uint32_t npos = uint32_t(X.size()), cin = uint32_t(X[0].size()), nout = uint32_t(Y.size());
            Mat64 r(npos, std::vector<int64_t>(nout, 0));
            const bool inl = inl_active_;
            inl_active_ = false;
            uint32_t wmax = std::min<uint32_t>(uint32_t(kSramaBytes) / cin, 1536u * uint32_t(W32) / 32u) / 32u * 32u;
            if (wmax == 0 || cin * 32u > uint32_t(kSrambBytes)) { st.framing_errors++; inl_active_ = inl; return r; }
            for (uint32_t p0 = 0; p0 < npos; p0 += wmax)
            {
                const uint32_t np = std::min(wmax, npos - p0), wt = (np + 31) / 32 * 32;
                std::vector<int8_t> a(size_t(cin) * wt, 0);
                for (uint32_t c = 0; c < cin; c++)
                    for (uint32_t x = 0; x < np; x++) a[size_t(c) * wt + x] = int8_t(X[p0 + x][c]);
                for (uint32_t n0 = 0; n0 < nout; n0 += 32)
                {
                    const uint32_t nch = std::min(32u, nout - n0);
                    std::vector<int8_t> w(size_t(cin) * nch);
                    for (uint32_t c = 0; c < cin; c++)
                        for (uint32_t k = 0; k < nch; k++) w[size_t(c) * nch + k] = int8_t(Y[n0 + k][c]);
                    std::vector<int32_t> pre(size_t(nch) * wt, 0);
                    if (pre_m)
                        for (uint32_t k = 0; k < nch; k++)
                            for (uint32_t x = 0; x < np; x++) pre[size_t(k) * wt + x] = int32_t((*pre_m)[p0 + x][n0 + k]);
                    std::vector<int64_t> acc(size_t(nch) * wt, 0);
                    if (!run_core(a, cin, w, nch, 1, 1, pre, 1, wt, 1, nch, 32, acc)) st.core_deadlocks++;
                    if (pass) pass(p0, np, n0, nch, wt, acc);
                    for (uint32_t k = 0; k < nch; k++)
                        for (uint32_t x = 0; x < np; x++) r[p0 + x][n0 + k] = acc[size_t(k) * wt + x] - int64_t(pre[size_t(k) * wt + x]);
                    st.rce_core_passes++;
                    st.macs += uint64_t(np) * nch * cin;
                }
            }
            core_select_.write(sel_);
            wait(2);
            inl_active_ = inl;
            return r;
        }

        void exec_ln(const LayerInstr &li)
        {
            LnParams p;
            if (!parse_ln_block(*dram_, li.param_addr, p) || p.H != li.seq_len) { st.rce_param_errors++; st.framing_errors++; return; }
            const LutIndirect lut(14, knobs.lut_mas_depth != 0, knobs.lut_zero0 != 0);
            const int H = p.H, ob = p.out_int16 ? 2 : 1;
            std::vector<int> x(size_t(H), 0);
            const bool cm = (li.flags & 2u) != 0;
            std::vector<std::vector<int>> X(size_t(li.rows)), Y(size_t(li.rows));
            for (int i = 0; i < li.rows; i++)
            {
                for (int j = 0; j < H; j++) x[j] = int(static_cast<int8_t>((*dram_)[li.in_addr + rce_off(cm, i, j, li.rows, H)]));
                const LnRow o = layernorm_row(x, p, lut, knobs);
                X[size_t(i)] = x;
                Y[size_t(i)] = o.y;
            }
            // HAS_GVU_LN=1 (with HAS_GVU_TOP=3): the rows in groups of 32 on the lane LayerNorm of the vector unit (has/rtl_ref_gvu_ln_lanes.h,
            // the model's proposal); its values are written and its cycles replace the estimate when every group ran there
            uint64_t ln_cyc = 0;
            bool ln_all = false, ln_clk = false;
            std::vector<Mat> xg;
            std::vector<std::vector<std::vector<int> *>> yg;
            if (gvs_ && GvuTopStream::ln_on())
            {
                ln_all = true;
                for (int i0 = 0; i0 < li.rows; i0 += 32)
                {
                    std::vector<const std::vector<int> *> xs;
                    std::vector<std::vector<int> *> ys;
                    for (int i = i0; i < std::min(li.rows, i0 + 32); i++) { xs.push_back(&X[size_t(i)]); ys.push_back(&Y[size_t(i)]); }
                    if (gvu_clk_)
                    {
                        xg.emplace_back();
                        for (const auto *r : xs) xg.back().push_back(*r);
                        yg.push_back(ys);
                        continue;
                    }
                    uint64_t gc = 0;
                    if (gvs_->layernorm_group(xs, ys, p, lut, knobs.round_mode, ln_cycles(uint64_t(H)), &gc)) ln_cyc += gc;
                    else ln_all = false;
                }
                // HAS_GVU_ELEM_CLK=1 (default): the groups are one job on the clock; its time passes here, so nothing is added below
                if (gvu_clk_ && !xg.empty())
                {
                    gvs_->start_layernorm(xg, yg, p, lut, knobs.round_mode, ln_cycles(uint64_t(H)));
                    while (!gvs_->job_done()) wait(gvs_->job_event());
                    st.rce_cycles += gvs_->job_cycles();
                    ln_clk = true;
                    if (!gvs_->job_ok()) ln_all = false;
                }
            }
            for (int i = 0; i < li.rows; i++)
                for (int j = 0; j < H; j++)
                {
                    const int16_t y = int16_t(Y[size_t(i)][size_t(j)]);
                    std::memcpy(&(*dram_)[li.out_addr + rce_off(cm, i, j, li.rows, H) * uint32_t(ob)], &y, size_t(ob));   // little endian
                }
            const uint64_t R = uint64_t(li.rows), h = uint64_t(H);
            const uint64_t gl = rce_group_rows(SpPipeline::LayerNorm, h), nl = (R + gl - 1) / gl;
            rce_wait(dma_est(ln_block_bytes(H)) + dma_est(R * h) + (ln_clk ? (ln_all ? 0 : nl * ln_cycles(h)) : (ln_all ? ln_cyc : nl * ln_cycles(h))) +
                     dma_est(R * h * uint64_t(ob)));
            st.rce_groups += nl;
            st.ln++;
            st.rce_rows += R;
        }

        // ---- DMA_TENSOR (0x16): COPY (mode 0), UPSAMPLE (mode 2 / 8), RESHAPE (mode 3 / 9) ----
        // LEN bytes DRAM -> DRAM through the read port (CH0) and the write port of the DMA, in chunks: chunk k + 1 is read while
        // chunk k is written. The bytes pass through bank 4 of the v4.5 SRAM, two halves used in turn: a stand-in for the FIFO of
        // the RTL DMA, unused by the overlapped schedule and holding nothing across instructions in the sequential one, so the
        // core SRAM is not touched. A region that reaches the DFC staging area (beyond the program image) is reported, nothing copied.
        void exec_dma_chunked_copy(uint32_t src_addr, uint32_t dst_addr, uint32_t n)
        {
            wait_dma_all();
            const bool host_side = port_.core_host_side;
            port_.core_host_side = false;
            const uint32_t C = dma_tensor_chunk, nk = (n + C - 1) / C;
            for (uint32_t k = 0; k <= nk; k++)
            {
                if (k < nk) dma_->start_read(0, src_addr + k * C, 4, (k % 2) * C, std::min(C, n - k * C));
                if (k > 0) dma_->start_write(dst_addr + (k - 1) * C, 4, ((k - 1) % 2) * C, std::min(C, n - (k - 1) * C));
                wait();
                while (dma_->is_read_active(0) || dma_->is_write_active()) { wait(); st.dma_cycles++; }
            }
            port_.core_host_side = host_side;
        }

        bool exec_dma_upsample_op(const LayerInstr &li)
        {
            const uint64_t in_bytes = uint64_t(li.c) * uint64_t(li.h) * uint64_t(li.w);
            const uint64_t out_bytes = li.n;
            if (uint64_t(li.a_addr) + in_bytes > stage_a_ || uint64_t(li.out_addr) + out_bytes > stage_a_)
            {
                std::fprintf(stderr, "[HasNpuTop] WARNING: DMA_UPSAMPLE (push %u): 0x%x (%llu B) -> 0x%x (%llu B) reaches beyond "
                                     "the program image (staging from 0x%x); nothing copied\n",
                             li.host_seq, li.a_addr, (unsigned long long)in_bytes, li.out_addr, (unsigned long long)out_bytes, stage_a_);
                st.framing_errors++;
                return false;
            }
            const DmaNdDesc sd = DmaNdDesc::upsample_chw(
                li.a_addr, uint32_t(li.c), uint32_t(li.h), uint32_t(li.w),
                uint32_t(li.pool_k), uint32_t(li.pool_s), 1);
            if (uint64_t(sd.length()) != out_bytes)
            {
                std::fprintf(stderr, "[HasNpuTop] WARNING: DMA_UPSAMPLE (push %u): descriptor length %llu B differs from the %llu B of the instruction; nothing copied\n",
                             li.host_seq, (unsigned long long)sd.length(), (unsigned long long)out_bytes);
                st.framing_errors++;
                return false;
            }
            const DmaNdDesc dd = DmaNdDesc::linear(li.out_addr, li.n, 1);
            run_dma_nd_op(sd, dd);
            dma_->st.upsample_ops++;
            st.dma_upsample++;
            return true;
        }

        bool exec_dma_reshape_op(const LayerInstr &li)
        {
            const uint32_t n = li.n;
            if (uint64_t(li.a_addr) + n > stage_a_ || uint64_t(li.out_addr) + n > stage_a_)
            {
                std::fprintf(stderr, "[HasNpuTop] WARNING: DMA_RESHAPE (push %u): 0x%x -> 0x%x, %u bytes reaches beyond "
                                     "the program image (staging from 0x%x); nothing copied\n",
                             li.host_seq, li.a_addr, li.out_addr, n, stage_a_);
                st.framing_errors++;
                return false;
            }
            if ((li.flags & 1u) != 0)
            {
                // Optional [C][H*W] -> [H*W][C] corner-turn transpose during reshape
                const DmaNdDesc sd = DmaNdDesc::tensor_chw(
                    li.a_addr, uint32_t(li.c), 1u, uint32_t(li.h) * uint32_t(li.w), 1).permuted({{ 2, 0, 1, 3 }});
                const DmaNdDesc dd = DmaNdDesc::linear(li.out_addr, n, 1);
                run_dma_nd_op(sd, dd);
            }
            else if (li.a_addr != li.out_addr)
            {
                exec_dma_chunked_copy(li.a_addr, li.out_addr, n);
            }
            else
            {
                // In-place C-order shape rebind: metadata-only (zero bus bytes)
                wait_dma_all();
                wait();
            }
            dma_->st.reshape_ops++;
            st.dma_reshape++;
            return true;
        }

        void exec_dma_tensor(const LayerInstr &li)
        {
            st.dma_tensor++;
            const uint64_t t0 = cycles_now();
            if (li.mode == 2 || li.mode == 8)
            {
                wait_dma_all();
                const uint64_t t1 = cycles_now();
                if (!exec_dma_upsample_op(li)) return;
                st.dma_tensor_bytes += li.n;
                st.dma_tensor_cycles += cycles_now() - t0;
                if (trace_dma_tensor)
                    std::printf("[dma-tensor] push %u (UPSAMPLE): bytes %u, chunk %u, cycles waiting for idle DMA %llu, cycles %llu\n",
                                li.host_seq, li.n, dma_tensor_chunk, (unsigned long long)(t1 - t0), (unsigned long long)(cycles_now() - t1));
                return;
            }
            if (li.mode == 3 || li.mode == 9)
            {
                wait_dma_all();
                const uint64_t t1 = cycles_now();
                if (!exec_dma_reshape_op(li)) return;
                const uint32_t moved = (li.a_addr == li.out_addr && (li.flags & 1u) == 0) ? 0u : li.n;
                st.dma_tensor_bytes += moved;
                st.dma_tensor_cycles += cycles_now() - t0;
                if (trace_dma_tensor)
                    std::printf("[dma-tensor] push %u (RESHAPE): bytes %u, chunk %u, cycles waiting for idle DMA %llu, cycles %llu\n",
                                li.host_seq, moved, dma_tensor_chunk, (unsigned long long)(t1 - t0), (unsigned long long)(cycles_now() - t1));
                return;
            }
            if (uint64_t(li.a_addr) + li.n > stage_a_ || uint64_t(li.out_addr) + li.n > stage_a_)
            {
                std::fprintf(stderr, "[HasNpuTop] WARNING: DMA_TENSOR (push %u): COPY 0x%x -> 0x%x, %u bytes reaches beyond the program "
                                     "image (staging from 0x%x); nothing copied\n", li.host_seq, li.a_addr, li.out_addr, li.n, stage_a_);
                st.framing_errors++;
                return;
            }
            wait_dma_all();
            const uint64_t t1 = cycles_now();
            exec_dma_chunked_copy(li.a_addr, li.out_addr, li.n);
            st.dma_tensor_bytes += li.n;
            st.dma_tensor_cycles += cycles_now() - t0;
            if (trace_dma_tensor)
                std::printf("[dma-tensor] push %u: bytes %u, chunk %u, cycles waiting for idle DMA %llu, cycles of the copy %llu\n", li.host_seq, li.n, dma_tensor_chunk,
                            (unsigned long long)(t1 - t0), (unsigned long long)(cycles_now() - t1));
        }

        // ---- HAS_DMA_ND: slice / concat / upsample / reshape (ELEM_WISE modes 6..9, model proposal) ----
        // Items [k0, k0 + cnt) of descriptor d as one descriptor: the range starts on a block of the dimensions below `lv` and
        // stays inside one run of dimension lv, so the strides of d are kept.
        static DmaNdDesc nd_sub(const DmaNdDesc &d, uint64_t k0, uint64_t cnt, int lv)
        {
            DmaNdDesc r = d;
            uint64_t below = 1, q = k0;
            int64_t off = 0;
            for (int j = 0; j < lv; j++) below *= std::max(1u, d.n[j]);
            for (int j = 0; j < 4; j++) { const uint64_t nj = std::max(1u, d.n[j]); off += int64_t(q % nj) * d.s[j]; q /= nj; }
            r.base = uint32_t(int64_t(d.base) + off * int64_t(std::max(1u, d.elem_bytes)));
            r.n[lv] = unsigned(cnt / below);
            for (int j = lv + 1; j < 4; j++) r.n[j] = 1;
            return r;
        }
        // HAS_DMA_ND_MERGE: the same item sequence with dimensions of one item dropped and contiguous neighbours merged
        // (dimension j + 1 continues dimension j when s[j + 1] = n[j] * s[j]).
        static DmaNdDesc nd_merge(const DmaNdDesc &d)
        {
            DmaNdDesc r = d;
            int m = 0;
            for (int j = 0; j < 4; j++)
            {
                const unsigned nj = std::max(1u, d.n[j]);
                if (nj == 1) continue;
                if (m > 0 && d.s[j] == int64_t(r.n[m - 1]) * r.s[m - 1]) { r.n[m - 1] *= nj; continue; }
                r.n[m] = nj; r.s[m] = d.s[j]; m++;
            }
            if (m == 0) { r.n[0] = 1; r.s[0] = 1; m = 1; }
            for (int j = m; j < 4; j++) { r.n[j] = 1; r.s[j] = 0; }
            return r;
        }
        // src -> dst (byte elements; one side 1-D) in pieces of at most dma_tensor_chunk bytes through bank 4 of the v4.5 SRAM,
        // as DMA_TENSOR COPY: piece k + 1 is read (CH0) while piece k is written; the core SRAM is not touched.
        void run_dma_nd_op(const DmaNdDesc &src_desc, const DmaNdDesc &dst_desc)
        {
            auto one_d = [](const DmaNdDesc &d) { return d.n[1] <= 1 && d.n[2] <= 1 && d.n[3] <= 1 && d.s[0] == 1; };
            const bool src_lin = one_d(src_desc);
            if (src_desc.elem_bytes != 1 || dst_desc.elem_bytes != 1 || !(src_lin || one_d(dst_desc)) || src_desc.length() != dst_desc.length())
            { st.framing_errors++; return; }
            if (DmaDfc1d::on())   // HAS_DMA_DFC_1D: the hardware's one-dimensional commands, measured cost; the data is moved directly
            {
                wait_dma_all();
                dma_->gather_dram_nd(src_desc, dst_desc, false);
                const uint64_t cyc = DmaDfc1d::cost(src_desc, dst_desc);
                for (uint64_t i = 0; i < cyc; i++) { wait(); st.dma_cycles++; }
                return;
            }
            // HAS_DMA_ND_MERGE: the strided side is cut over its merged dimensions (off: sm / dm are the descriptors as given)
            const DmaNdDesc sm = (dma_nd_merge && !src_lin) ? nd_merge(src_desc) : src_desc;
            const DmaNdDesc dm = (dma_nd_merge && src_lin) ? nd_merge(dst_desc) : dst_desc;
            const DmaNdDesc &gm = src_lin ? dm : sm;
            const uint64_t C = dma_tensor_chunk, L = gm.length();
            int lv = 0;
            uint64_t per = 1;
            while (lv < 3 && per * std::max(1u, gm.n[lv]) <= C) { per *= std::max(1u, gm.n[lv]); lv++; }
            const uint64_t nlv = std::max(1u, gm.n[lv]), m = std::max<uint64_t>(1, C / per);
            std::vector<std::pair<DmaNdDesc, DmaNdDesc>> pc;
            for (uint64_t k = 0; k < L;)
            {
                const uint64_t cnt = std::min(m, nlv - (k / per) % nlv) * per;
                const DmaNdDesc a = nd_sub(sm, k, cnt, src_lin ? 0 : lv), b = nd_sub(dm, k, cnt, src_lin ? lv : 0);
                pc.push_back({a, b});
                k += cnt;
            }
            wait_dma_all();
            const bool host_side = port_.core_host_side;
            port_.core_host_side = false;
            const uint32_t nk = uint32_t(pc.size());
            for (uint32_t k = 0; k <= nk; k++)
            {
                if (k < nk) dma_->start_read_nd(0, pc[k].first, 4, DmaNdDesc::linear((k % 2) * uint32_t(C), pc[k].first.total_bytes(), 1));
                if (k > 0)
                    dma_->start_write_nd(pc[k - 1].second, 4, DmaNdDesc::linear(((k - 1) % 2) * uint32_t(C), pc[k - 1].second.total_bytes(), 1));
                wait();
                while (dma_->is_read_active(0) || dma_->is_write_active()) { wait(); st.dma_cycles++; }
            }
            port_.core_host_side = host_side;
        }

        // ---- ELEM_WISE: one instruction ----
        void exec_elem(const LayerInstr &li)
        {
            st.elem++;
            const uint64_t t0 = cycles_now();
            uint8_t *d = dram_->data();
            if (li.mode == 0)
            {
                AddParams p;
                p.zpA = li.add.zpA; p.zpB = li.add.zpB; p.zpO = li.add.zpO;
                p.SA = li.add.SA; p.SB = li.add.SB; p.SO = li.add.SO;
                p.sA = li.add.sA; p.sB = li.add.sB; p.sO = li.add.sO;
                ew_->add(reinterpret_cast<const int8_t *>(d + li.a_addr), reinterpret_cast<const int8_t *>(d + li.b_addr),
                         reinterpret_cast<int8_t *>(d + li.out_addr), li.n, p);
            }
            else if (li.mode == 5)
                ew_->avgpool(reinterpret_cast<const int8_t *>(d + li.a_addr), reinterpret_cast<int8_t *>(d + li.out_addr),
                             li.c, li.h, li.w, li.pool_k, li.pool_s, li.avg_scale, li.avg_shift);
            else if (li.mode == 6 && dma_nd)
            {
                // DMA_SLICE: slice [c][h][w] at a_addr along axis pool_mode (0=C, 1=H, 2=W) into out_addr
                const DmaNdDesc sd = DmaNdDesc::tensor_chw(li.a_addr, uint32_t(li.c), uint32_t(li.h), uint32_t(li.w), 1)
                                         .sliced_chw(li.pool_mode, uint32_t(li.pool_p), uint32_t(li.pool_k), uint32_t(li.pool_s));
                const DmaNdDesc dd = DmaNdDesc::linear(li.out_addr, sd.total_bytes(), 1);
                run_dma_nd_op(sd, dd);
            }
            else if (li.mode == 7 && dma_nd)
            {
                // DMA_CONCAT: place part A (and optional part B when b_addr != 0) into [c][h][w] at out_addr along axis pool_mode
                const uint32_t off_a = (li.b_addr != 0) ? 0u : uint32_t(li.pool_p);
                const DmaNdDesc dd_a = DmaNdDesc::concat_chw(li.out_addr, uint32_t(li.c), uint32_t(li.h), uint32_t(li.w),
                                                             li.pool_mode, off_a, uint32_t(li.pool_k), 1);
                const DmaNdDesc sd_a = DmaNdDesc::linear(li.a_addr, dd_a.total_bytes(), 1);
                run_dma_nd_op(sd_a, dd_a);
                if (li.b_addr != 0)
                {
                    const int dim = (li.pool_mode == 0) ? li.c : (li.pool_mode == 1 ? li.h : li.w);
                    const uint32_t len_b = uint32_t(std::max(1, dim - li.pool_k));
                    const DmaNdDesc dd_b = DmaNdDesc::concat_chw(li.out_addr, uint32_t(li.c), uint32_t(li.h), uint32_t(li.w),
                                                                 li.pool_mode, uint32_t(li.pool_k), len_b, 1);
                    const DmaNdDesc sd_b = DmaNdDesc::linear(li.b_addr, dd_b.total_bytes(), 1);
                    run_dma_nd_op(sd_b, dd_b);
                }
            }
            else if (li.mode == 8 && dma_nd)
            {
                // DMA_UPSAMPLE: nearest-neighbor [c][h][w] -> [c][h*pool_k][w*pool_s]
                exec_dma_upsample_op(li);
            }
            else if (li.mode == 9 && dma_nd)
            {
                // DMA_RESHAPE: [c][h][w] -> [geom.cout][geom.oh][geom.ow]
                exec_dma_reshape_op(li);
            }
            else
                ew_->maxpool(reinterpret_cast<const int8_t *>(d + li.a_addr), reinterpret_cast<int8_t *>(d + li.out_addr),
                             li.c, li.h, li.w, li.pool_k, li.pool_s, li.pool_p);
            st.elem_cycles += cycles_now() - t0;
        }

        // ---- sub-blocks and their wires ----
        HasElemwise *ew_{nullptr};
        DmaSramT *sram_{nullptr};
        DmaT *dma_{nullptr};
        ObpT *obp_{nullptr};
        CoreT *core_{nullptr};
        std::vector<uint8_t> *dram_{nullptr};
        uint32_t stage_a_{0}, stage_pre_{0}, stage_out_{0};
        uint32_t cur_ht_{1}, cur_wt_{1}, cur_nch_{1}, cur_yu_{1};
        uint64_t obp_rows_written_{0};

        sc_signal<bool> s_false_{"s_false"}, s_true_{"s_true", true};
        sc_signal<sc_bv<3>> sram_select_{"sram_select"};
        sc_signal<uint32_t> s_host_addr_{"s_host_addr"};
        sc_signal<sauria::host_data_t> s_host_wdata_{"s_host_wdata"}, s_host_rdata_{"s_host_rdata"};
        sc_signal<sauria::host_mask_t> s_host_wmask_{"s_host_wmask"};
        sc_signal<uint32_t> z_addr_[6];
        sc_signal<act_vector_t<W32, int8_t>> a_data_[2];
        sc_signal<wei_vector_t<W32, int8_t>> b_data_[2];
        sc_signal<psum_vector_t<W32, int32_t>> c_zero_[2], c_rd_[2];
        sc_signal<sramc_mask_t<W32>> c_mask_[2];

        sc_signal<psum_vector_t<W32, int32_t>> o_in_{"o_in"}, o_out_{"o_out"};
        sc_signal<uint32_t> o_in_addr_{"o_in_addr"}, o_out_addr_{"o_out_addr"};
        sc_signal<sramc_mask_t<W32>> o_in_mask_{"o_in_mask"}, o_out_mask_{"o_out_mask"};
        sc_signal<bool> o_in_valid_{"o_in_valid"}, o_out_wren_{"o_out_wren"}, o_out_valid_{"o_out_valid"}, lut_en_{"lut_en"};
        sc_signal<act_vector_t<W32, int8_t>> o_residual_{"o_residual"};
        sc_signal<uint32_t> def_scale_{"def_scale"}, def_shift_{"def_shift"};
        sc_signal<uint32_t> obp_host_addr_{"obp_host_addr"};
        sc_signal<bool> obp_host_wren_{"obp_host_wren"};
        sc_signal<sauria::host_data_t> obp_host_wdata_{"obp_host_wdata"}, obp_host_rdata_{"obp_host_rdata"};
        sc_signal<sauria::host_mask_t> obp_host_wmask_{"obp_host_wmask"};

        sc_signal<bool> st_ready_{"st_ready"}, st_valid_{"st_valid"}, st_oc_last_{"st_oc_last"}, st_oc_tile_last_{"st_oc_tile_last"},
                        st_empty_{"st_empty"}, st_stall_{"st_stall"}, st_bypass_{"st_bypass"};
        sc_signal<psum_vector_t<W32, int32_t>> st_data_{"st_data"}, st_model_{"st_model"};
        sc_signal<uint32_t> st_addr_{"st_addr"}, st_mask_{"st_mask"}, st_ch_{"st_ch"};
        sc_signal<int64_t> st_scale_{"st_scale"};
        sc_signal<int32_t> st_shift_{"st_shift"};
        sc_signal<uint64_t> st_push_{"st_push"};
        sc_signal<bool> core_rstn_{"core_rstn"}, core_start_{"core_start"}, core_done_{"core_done"}, core_deadlock_{"core_deadlock"};
        uint64_t core_t0_{0};
        bool core_running_{false};
        sc_signal<uint32_t> core_mvm_k_{"core_mvm_k"}, core_total_contexts_{"core_total_contexts"}, core_host_addr_{"core_host_addr"};
        sc_signal<bool> core_host_wren_{"core_host_wren"}, core_host_rden_{"core_host_rden"};
        sc_signal<sauria::host_data_t> core_host_wdata_{"core_host_wdata"}, core_host_rdata_{"core_host_rdata"};
        sc_signal<sauria::host_mask_t> core_host_wmask_{"core_host_wmask"};
        sc_signal<float> core_threshold_{"core_threshold"};
        sc_signal<sc_bv<3>> core_select_{"core_select"};
    };
} // namespace has

#endif
