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
#include "has/gvu_elemwise.h"
#include "has/gvu_vrf.h"
#include "has/has_mmio_compat.h"
#include "has/has_tile_iter.h"
#include "has/has_dma.h"
#include "has/gvu_rce_params.h"
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
    enum class Profile { Legacy, Recommended, Proposals };
    inline const char *profile_name(Profile p)
    {
        return p == Profile::Legacy ? "legacy" : (p == Profile::Proposals ? "proposals" : "recommended");
    }
    // Knobs are fixed at construction (epilogue and element-wise latencies, scratchpad banking).
    inline Knobs profile_knobs(Profile p, Knobs k = Knobs())
    {
        if (p != Profile::Legacy) { k.sp_banked = 1; k.gvu_latencies(); }
        return k;
    }

    class HasNpuTop : public sc_module
    {
    public:
        static constexpr int W32 = 32;
        static constexpr int kSramaBytes = 79 * 1024, kSrambBytes = 81 * 1024;   // HAS: one buffer of each
        typedef sauria::Sram<32, 32, int8_t, int8_t, int32_t, 5056, 5184, 1536> DmaSramT;
        // DMA SRAM side: the v4.5 Sram (sequential schedule) or the host half of the core's ping-pong SRAM (overlapped).
        struct SramPort
        {
            DmaSramT *v45{nullptr};
            sauria_rtl::NpuTop<32, 32, int8_t, int8_t, int32_t, kSramaBytes, kSrambBytes, 1536, 16, 64, 1> *core{nullptr};
            bool core_host_side{false};
            void write(int b, uint32_t o, const uint8_t *p, uint32_t n)
            { if (core_host_side) core->write_sram_host_side(b, o, p, n); else v45->write_bank_data(b, o, p, n); }
            void read(int b, uint32_t o, uint8_t *p, uint32_t n)
            { if (core_host_side) core->read_sram_host_side(b, o, p, n); else v45->read_bank_data(b, o, p, n); }
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
            uint64_t dma_cycles{0}, obp_cycles{0}, cfg_cycles{0}, elem_cycles{0}, vectors{0}, macs{0}, core_passes{0};
            uint64_t weight_loads{0}, weight_reuse_flips{0};
            uint64_t attn{0}, ln{0}, rce_rows{0}, rce_cycles{0}, rce_ovf16{0}, rce_param_errors{0};   // FUSED_ATTN / LAYERNORM
            uint64_t rce_core_passes{0}, rce_core_cycles{0};                                          // attention products on the core
            uint64_t cycles{0};                                                                       // sum of instruction cycles
        } st;
        MmioCompat mmio;
        Knobs knobs;
        int core_max_cycles{2000000}, stall_cycles{5000};
        bool prefetch_a{true};
        bool trace{false};                 // one line per retired instruction on stdout
        std::FILE *metrics_csv{nullptr};   // per-tile core counters, same columns as tb_has_net (FE_METRICS builds)

        const ObpT &obp_block() const { return *obp_; }
        const HasElemwise &elem_block() const { return *ew_; }

        // DRAM image: the DFC appends its private staging area (deviation D3) after `program_bytes`.
        void set_dram(std::vector<uint8_t> *dram)
        {
            dram_ = dram;
            const size_t base = (dram->size() + 4095) & ~size_t(4095);
            stage_a_ = uint32_t(base);
            stage_pre_ = stage_a_ + 128 * 1024;
            stage_out_ = stage_pre_ + 128 * 1024;
            dram->resize(size_t(stage_out_) + 128 * 1024, 0);
            dma_->set_dram(dram);
        }
        uint32_t staging_base() const { return stage_a_; }
        // Place the private staging area at a fixed DRAM offset instead of after the image given to set_dram(). Used when
        // the DRAM vector is filled after set_dram() (a system wrapper that stages data per instruction); the offset must
        // lie above every address the program uses. Call after set_dram().
        void set_staging_base(uint32_t base)
        {
            stage_a_ = (base + 4095u) & ~4095u;
            stage_pre_ = stage_a_ + 128 * 1024;
            stage_out_ = stage_pre_ + 128 * 1024;
            if (dram_ && dram_->size() < size_t(stage_out_) + 128 * 1024) dram_->resize(size_t(stage_out_) + 128 * 1024, 0);
        }

        SC_HAS_PROCESS(HasNpuTop);
        // Default: the Recommended profile. The Knobs constructor leaves every option off (= Legacy) for tools that set
        // options one by one.
        explicit HasNpuTop(sc_module_name nm, Profile p = Profile::Recommended, size_t q_depth = 16)
            : HasNpuTop(nm, profile_knobs(p), q_depth)
        {
            apply_profile(p);
        }
        HasNpuTop(sc_module_name nm, const Knobs &k, size_t q_depth = 16)
            : sc_module(nm), mmio(q_depth), knobs(k)
        {
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

            SC_METHOD(host_port);
            sensitive << i_clk.pos();
            dont_initialize();
            SC_METHOD(obp_writeback);
            sensitive << i_clk.pos();
            dont_initialize();
            SC_THREAD(dfc);
            sensitive << i_clk.pos();
        }
        ~HasNpuTop() { delete core_; delete obp_; delete dma_; delete sram_; delete ew_; }

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
            if (!on) { core_->set_sramc_write_hook(nullptr); return; }
            core_->set_sramc_write_hook([this](uint32_t addr, psum_vector_t<W32, int32_t> &v, const sramc_mask_t<W32> &m)
            {
                if (!inl_active_) return;   // S1: earlier Cin passes keep the int32 psum for the next pass
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
                        const auto r = obp_->apply_inline(v, x, mx);
                        for (uint32_t q = k; q < j; q++) if (mx[q]) out[q] = r[q];
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
            desc3d = tile_order_auto = rce_core = on;
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
        bool inl_active_{true};
        uint64_t halo_copy_done_{0};   // cycle at which the in-SRAM-A halo copy of the prefetched window ends
        SramPort port_;
        // ================================================================ host port
        void host_port()
        {
            if (!i_rstn.read()) { mmio.soft_reset(); o_irq.write(false); o_done.write(false); return; }
            if (i_host_wren.read() && i_host_wmask.read()[0])
                mmio.write(i_host_addr.read(), static_cast<uint32_t>(i_host_wdata.read()[0]));
            sauria::host_data_t r;
            if (i_host_rden.read() && mmio.owns(i_host_addr.read())) r[0] = static_cast<double>(mmio.read(i_host_addr.read()));
            o_host_rdata.write(r);
            o_irq.write(mmio.irq());
            o_done.write(mmio.irq());
            o_deadlock.write(st.core_deadlocks != 0);
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
                // A system wrapper may clear or regrow the DRAM vector between instructions: keep the staging area.
                if (dram_ && dram_->size() < size_t(stage_out_) + 128 * 1024) dram_->resize(size_t(stage_out_) + 128 * 1024, 0);
                const uint64_t c0 = cycles_now();
                const uint64_t tiles0 = st.tiles;
                if (li.opcode == 0x12) exec_gemm(li);
                else if (li.opcode == 0x15) exec_elem(li);
                else if (li.opcode == 0x13) exec_attn(li);
                else if (li.opcode == 0x14) exec_ln(li);
                st.instr++;
                st.cycles += cycles_now() - c0;
                if (trace)
                    std::printf("[DFC] instr %llu op 0x%02x %s tiles %llu cycles %llu\n", (unsigned long long)li.host_seq,
                                li.opcode, li.opcode == 0x12 ? "GEMM_FUSED" : li.opcode == 0x13 ? "FUSED_ATTN" : li.opcode == 0x14 ? "LAYERNORM" : (li.mode == 0 ? "ELEM_ADD" : li.mode == 5 ? "ELEM_AVG" : "ELEM_MAX"),
                                (unsigned long long)(st.tiles - tiles0), (unsigned long long)(cycles_now() - c0));
                mmio.retire(li);
            }
        }

        uint64_t cycles_now() const { return sc_time_stamp().value() / clk_period_.value(); }
        uint32_t dram_u32(uint32_t a) const { uint32_t v; std::memcpy(&v, &(*dram_)[a], 4); return v; }

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

        // ---- GEMM_FUSED: one layer ----
        void exec_gemm(const LayerInstr &li)
        {
            st.gemm++;
            const LayerGeom &g = li.geom;
            const uint32_t cin = uint32_t(li.in_c), K = cin * uint32_t(g.kh) * uint32_t(g.kw);
            // Layer parameters (CH3 in the HW, spec §4.3): LUT, zero point. Scale/shift go per tile (channel range).
            const uint64_t t0 = cycles_now();
            if (li.act != 0)
            {
                uint8_t lut[256];
                if (li.act == 1) for (int x = 0; x < 256; x++) lut[x] = static_cast<uint8_t>(std::max(x - 128, 0));
                else std::memcpy(lut, &(*dram_)[li.lut_addr], 256);
                for (uint32_t lane = 0; lane < W32; lane++)
                    for (uint32_t e = 0; e < 256; e += 4) obp_write4(LUT_BASE + lane * 256 + e, lut + e);
            }
            obp_write(CFG_BASE + ObpT::REG_ZP_OUT, static_cast<uint32_t>(li.zp_out));
            st.cfg_cycles += cycles_now() - t0;
            lut_en_.write(li.act != 0);
            if (overlap_) { exec_tiles_ovl(li, K); return; }

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
                if (!a_ready) dma_->start_read(1, stage_a_, int(cur_a_bank), 0, fill_stage_a(li, tl));
                std::memset(&(*dram_)[stage_pre_], 0, size_t(words) * 128);
                for (uint32_t x = 0; x < nch; x++)
                {
                    const uint32_t b = dram_u32(li.bias_addr + 4 * (uint32_t(tl.c0) + x));
                    for (uint32_t p = 0; p < npos; p++) std::memcpy(&(*dram_)[stage_pre_ + 4 * (x * npos + p)], &b, 4);
                }
                dma_->start_read(0, li.w_addr + uint32_t(tl.c0) * K, 0, 0, nch * K);
                dma_->start_read(3, stage_pre_, 4, 0, words * 128);
                wait();
                st.dma_cycles++;
                wait_dma_excl_ch1();
                for (uint32_t x = 0; x < nch; x++)
                {
                    const uint32_t S = li.per_channel ? dram_u32(li.scale_addr + 4 * (uint32_t(tl.c0) + x)) : li.bcast_S;
                    const uint32_t s = li.per_channel ? dram_u32(li.shift_addr + 4 * (uint32_t(tl.c0) + x)) : uint32_t(li.bcast_s);
                    obp_write(SCALE_BASE + 4 * x, S);
                    obp_write(SHIFT_BASE + 4 * x, s);
                    st.cfg_cycles += 4;
                }
                obp_write(CFG_BASE + ObpT::REG_NCH, nch);
                st.cfg_cycles += 2;
                wait_ch1();
                std::vector<int8_t> a(size_t(cin) * ah * aw), w(size_t(nch) * K);
                std::vector<int32_t> pre(size_t(words) * W32);
                sram_->read_bank_data(int(cur_a_bank), 0, reinterpret_cast<uint8_t *>(a.data()), uint32_t(a.size()));
                sram_->read_bank_data(0, 0, reinterpret_cast<uint8_t *>(w.data()), uint32_t(w.size()));
                sram_->read_bank_data(4, 0, reinterpret_cast<uint8_t *>(pre.data()), uint32_t(pre.size() * 4));
                const uint32_t next_a_bank = cur_a_bank == 2 ? 3 : 2;
                if (prefetch_a && ti + 1 < it.count())
                {
                    dma_->start_read(1, stage_a_, int(next_a_bank), 0, fill_stage_a(li, it.at(ti + 1)));
                    a_ready = true;
                }
                else a_ready = false;
                cur_a_bank = next_a_bank;

                std::vector<int64_t> acc(nel, 0);
#ifdef FE_METRICS
                const sauria_rtl::PerfCounters m0 = perf_;
#endif
                const bool ok = run_core(a, cin, w, nch, uint32_t(g.kh), uint32_t(g.kw), pre, ht, wt, uint32_t(g.sy), nch, yu, acc);
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
                // CH2 write-back; int8 scatter into the destination tensor = DMA descriptor (deviation D3, H19)
                dma_->start_write(stage_out_, 4, 0, words * 128);
                wait();
                st.dma_cycles++;
                wait_dma_excl_ch1();
                for (uint32_t x = 0; x < nch; x++)
                    for (uint32_t p = 0; p < npos; p++)
                    {
                        int32_t val;
                        std::memcpy(&val, &(*dram_)[stage_out_ + 4 * (x * npos + p)], 4);
                        const uint32_t c = uint32_t(tl.c0) + x, y = uint32_t(tl.oy0) + p / wt, xx = uint32_t(tl.ox0) + p % wt;
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
                if (npu_a && desc3d && npu_a->ix0 == t.ix0 && npu_a->ix1 == t.ix1 && t.iy0 > npu_a->iy0 && t.iy0 < npu_a->iy1 &&
                    t.iy1 >= npu_a->iy1)
                {
                    const uint32_t h = inbounds_rows(li, t, t.iy0, npu_a->iy1, ct);
                    timed = std::max(1u, inbounds_rows(li, t, npu_a->iy1, t.iy1, ct));
                    halo_tiles++;
                    halo_bytes += h;
                    if (halo_copy_bpc) halo_copy_done_ = cycles_now() + (h + halo_copy_bpc - 1) / halo_copy_bpc;
                }
                dma_->start_read(1, stage_a_, 2, 0, n, timed);
            }
            if (load_w) dma_->start_read(0, li.w_addr + uint32_t(t.c0) * K + part * uint32_t(t.x_used) * Kt, 0, 0, uint32_t(t.x_used) * Kt);
        }
        void prefetch_c(const LayerInstr &li, const TileGeom &t)
        {
            const uint32_t nch = uint32_t(t.x_used), npos = uint32_t(t.oy1 - t.oy0) * uint32_t(t.ox1 - t.ox0), words = tile_words(t);
            std::memset(&(*dram_)[stage_pre_], 0, size_t(words) * 128);
            for (uint32_t x = 0; x < nch; x++)
            {
                const uint32_t b = (int(t.c0) + int(x) < li.geom.cout) ? dram_u32(li.bias_addr + 4 * (uint32_t(t.c0) + x)) : 0u;
                for (uint32_t p = 0; p < npos; p++) std::memcpy(&(*dram_)[stage_pre_ + 4 * (x * npos + p)], &b, 4);
            }
            // c_bias_bcast (proposal): CH3 moves only the bias vector (4 B per channel); the descriptor
            // replicates it over the tile's positions in SRAM-C. Data is unchanged; only bus bytes/cycles.
            dma_->start_read(3, stage_pre_, 4, 0, words * 128, c_bias_bcast ? 4 * nch : 0);
        }
        void wait_dma_all()
        {
            while (dma_->is_any_read_active() || dma_->is_write_active() || cycles_now() < halo_copy_done_) { wait(); st.dma_cycles++; }
        }

        void core_begin(uint32_t cin, const TileGeom &t, const LayerGeom &g)
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
            desc.B_w = uint32_t(g.kw); desc.B_h = uint32_t(g.kh); desc.d = 1; desc.s = uint32_t(g.sy);
            desc.c_til = cin; desc.k_til = uint32_t(t.x_used); desc.h_til = uint32_t(t.oy1 - t.oy0); desc.w_til = uint32_t(t.ox1 - t.ox0);
            desc.X_used = uint32_t(t.x_used); desc.Y_used = uint32_t(t.y_used); desc.preload_en = 1;
            core_apply_cfg(desc, *tg);
            uint64_t f[F_CFG_COUNT];
            sauria_compute_core_fields(desc, *tg, f);
            core_total_contexts_.write(uint32_t(f[F_CFG_NCONTEXTS]));
            core_mvm_k_.write(1);
            wait();
            core_start_.write(true);
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
            wait(8);
            if (obp_inline_)
            {
                // OBP pipeline tail behind the PSM's last write (latency = HasObp LAT, ESTIMATE until the MAS)
                wait(obp_->latency());
                st.obp_cycles += uint64_t(obp_->latency());
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
            inl_npos_ = uint32_t(t.oy1 - t.oy0) * uint32_t(t.ox1 - t.ox0);
        }
        // CH2 from the C host half, then the int8 scatter into the destination tensor (descriptor emulation, D3).
        void writeback(const LayerInstr &li, const TileGeom &t)
        {
            const uint32_t nch = uint32_t(t.x_used), wt = uint32_t(t.ox1 - t.ox0), npos = uint32_t(t.oy1 - t.oy0) * wt;
            // desc3d: only the REAL elements cross the bus (PAD_TAIL channels / columns are computed, never written)
            const uint32_t real_c = uint32_t(std::max(0, std::min(t.c1, li.geom.cout) - t.c0));
            const uint32_t real_w = uint32_t(std::max(0, std::min(t.ox1, li.geom.ow) - t.ox0));
            const uint32_t real_nel = std::max(1u, real_c * uint32_t(t.oy1 - t.oy0) * real_w);
            dma_->start_write(stage_out_, 4, 0, tile_words(t) * 128, desc3d ? real_nel : 0);
            wait();
            while (dma_->is_write_active()) { wait(); st.dma_cycles++; }
            for (uint32_t x = 0; x < nch; x++)
                for (uint32_t p = 0; p < npos; p++)
                {
                    int32_t val;
                    std::memcpy(&val, &(*dram_)[stage_out_ + 4 * (x * npos + p)], 4);
                    const uint32_t c = uint32_t(t.c0) + x, y = uint32_t(t.oy0) + p / wt, xx = uint32_t(t.ox0) + p % wt;
                    if (c >= uint32_t(li.geom.cout) || xx >= uint32_t(li.geom.ow)) continue;   // PAD_TAIL: computed, not written
                    (*dram_)[li.out_addr + (c * uint32_t(li.geom.oh) + y) * uint32_t(li.geom.ow) + xx] =
                        static_cast<uint8_t>(static_cast<int8_t>(val));
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
                core_begin(cin_t, cur, g);
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
            return halo_reuse && desc3d && nsplit == 1 && li.geom.kh > li.geom.sy && TileIter(li.geom).n_row_tiles() > 1;
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
        // Padded input window of one tile (zero = int8 zero point), C-order [cin][ah][aw] into the staging area.
        uint32_t fill_stage_a(const LayerInstr &li, const TileGeom &t, uint32_t ch0 = 0, uint32_t nc = 0)
        {
            const uint32_t ah = uint32_t(t.iy1 - t.iy0), aw = uint32_t(t.ix1 - t.ix0);
            if (nc == 0) nc = uint32_t(li.in_c);
            for (uint32_t ci = 0; ci < nc; ci++)
                for (uint32_t y = 0; y < ah; y++)
                    for (uint32_t x = 0; x < aw; x++)
                    {
                        const int sy = t.iy0 + int(y), sx = t.ix0 + int(x);
                        uint8_t v = 0;
                        if (sy >= 0 && sx >= 0 && sy < li.in_h && sx < li.in_w)
                            v = (*dram_)[li.in_addr + ((ch0 + ci) * uint32_t(li.in_h) + uint32_t(sy)) * uint32_t(li.in_w) + uint32_t(sx)];
                        (*dram_)[stage_a_ + (ci * ah + y) * aw + x] = v;
                    }
            return nc * ah * aw;
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
        void core_apply_cfg(const sauria::SauriaLayerDesc &desc, const sauria::SauriaTarget &target)
        {
            using namespace sauria;
            uint64_t f[F_CFG_COUNT];
            sauria_compute_core_fields(desc, target, f);
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
                      uint32_t y_used, std::vector<int64_t> &acc)
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
            desc.B_w = kw; desc.B_h = kh; desc.d = 1; desc.s = sy;
            desc.c_til = cin; desc.k_til = nch; desc.h_til = ht; desc.w_til = wt;
            desc.X_used = x_used; desc.Y_used = y_used; desc.preload_en = 1;
            core_apply_cfg(desc, *t);
            {
                uint64_t f[F_CFG_COUNT];
                sauria_compute_core_fields(desc, *t, f);
                core_total_contexts_.write(uint32_t(f[F_CFG_NCONTEXTS]));
            }
            core_mvm_k_.write(1);
            wait();
            // 7. start pulse (2 cycles)
            core_start_.write(true);
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
        void rce_wait(uint64_t c) { wait(clk_period_ * double(c)); st.rce_cycles += c; }

        void exec_attn(const LayerInstr &li)
        {
            const int NQ = li.rows, L = li.seq_len, D = li.head_dim;
            AttnParams p;
            std::vector<int32_t> exp;
            if (!parse_attn_block(*dram_, li.param_addr, p, exp)) { st.rce_param_errors++; st.framing_errors++; return; }
            const bool cm = (li.flags & 2u) != 0;   // mask stays [NQ][L] token-major
            const Mat Q = read_mat(li.a_addr, NQ, D, cm), K = read_mat(li.b_addr, L, D, cm), V = read_mat(li.v_addr, L, D, cm);
            const Mat M = li.mask_addr ? read_mat(li.mask_addr, NQ, L) : Mat(size_t(NQ), std::vector<int>(size_t(L), 0));
            static const LutIndirect recip(14);
            const uint64_t t0 = cycles_now();
            const AttnResult r = rce_core ? fused_attn(Q, K, V, M, p, exp.data(), recip, knobs,
                                                       [this](const Mat &X, const Mat &Y) { return core_dot(X, Y); })
                                          : fused_attn(Q, K, V, M, p, exp.data(), recip, knobs);
            if (rce_core) { st.rce_core_cycles += cycles_now() - t0; st.rce_cycles += cycles_now() - t0; }
            for (int i = 0; i < NQ; i++)
                for (int n = 0; n < D; n++) (*dram_)[li.out_addr + rce_off(cm, i, n, NQ, D)] = static_cast<uint8_t>(static_cast<int8_t>(r.O[i][n]));
            const uint64_t nq = uint64_t(NQ), l = uint64_t(L), d = uint64_t(D);
            uint64_t c = dma_est(kAttnBlockBytes) + dma_est(nq * d) + dma_est(l * d) + dma_est(l * d) + (li.mask_addr ? dma_est(nq * l) : 0);
            if (!rce_core) c += (l + 31) / 32 * core_pass_est(nq, d) + (d + 31) / 32 * core_pass_est(nq, l);
            c += (nq + 31) / 32 * softmax_cycles(l);
            c += dma_est(nq * d);
            rce_wait(c);
            st.attn++;
            st.rce_rows += nq;
            st.rce_ovf16 += r.ovf16;
        }

        // X [npos][cin] . Y [nout][cin]^T on the core as a 1x1 layer: activation = X^T padded to a multiple of 32 positions, weights = Y
        // (SAURIA order c*nch + k for kh = kw = 1), 32 output channels per pass, positions split to fit SRAM-A / SRAM-C.
        // The OBP hook is switched off (raw int32 psums are needed) and the ping-pong select of the GEMM path is restored after.
        Mat64 core_dot(const Mat &X, const Mat &Y)
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
                    std::vector<int64_t> acc(size_t(nch) * wt, 0);
                    if (!run_core(a, cin, w, nch, 1, 1, pre, 1, wt, 1, nch, 32, acc)) st.core_deadlocks++;
                    for (uint32_t k = 0; k < nch; k++)
                        for (uint32_t x = 0; x < np; x++) r[p0 + x][n0 + k] = acc[size_t(k) * wt + x];
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
            static const LutIndirect lut(14);
            const int H = p.H, ob = p.out_int16 ? 2 : 1;
            std::vector<int> x(size_t(H), 0);
            const bool cm = (li.flags & 2u) != 0;
            for (int i = 0; i < li.rows; i++)
            {
                for (int j = 0; j < H; j++) x[j] = int(static_cast<int8_t>((*dram_)[li.in_addr + rce_off(cm, i, j, li.rows, H)]));
                const LnRow o = layernorm_row(x, p, lut, knobs);
                for (int j = 0; j < H; j++)
                {
                    const int16_t y = int16_t(o.y[j]);
                    std::memcpy(&(*dram_)[li.out_addr + rce_off(cm, i, j, li.rows, H) * uint32_t(ob)], &y, size_t(ob));   // little endian
                }
            }
            const uint64_t R = uint64_t(li.rows), h = uint64_t(H);
            rce_wait(dma_est(ln_block_bytes(H)) + dma_est(R * h) + (R + 31) / 32 * ln_cycles(h) + dma_est(R * h * uint64_t(ob)));
            st.ln++;
            st.rce_rows += R;
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

        sc_signal<bool> core_rstn_{"core_rstn"}, core_start_{"core_start"}, core_done_{"core_done"}, core_deadlock_{"core_deadlock"};
        sc_signal<uint32_t> core_mvm_k_{"core_mvm_k"}, core_total_contexts_{"core_total_contexts"}, core_host_addr_{"core_host_addr"};
        sc_signal<bool> core_host_wren_{"core_host_wren"}, core_host_rden_{"core_host_rden"};
        sc_signal<sauria::host_data_t> core_host_wdata_{"core_host_wdata"}, core_host_rdata_{"core_host_rdata"};
        sc_signal<sauria::host_mask_t> core_host_wmask_{"core_host_wmask"};
        sc_signal<float> core_threshold_{"core_threshold"};
        sc_signal<sc_bv<3>> core_select_{"core_select"};
    };
} // namespace has

#endif
