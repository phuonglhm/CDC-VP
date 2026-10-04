// rtl_ref_eval_counters.h -- verbatim port of sauria_model's instrumentation/eval_counters.h.
//
// Only three things differ from the original; no logic or formula is changed:
//   1. header guard FX1_*_H -> SAURIA_RTL_*_H (instrumentation/perf_counters.h already uses FX1_PERF_COUNTERS_H;
//      keeping the guard would load only one of the two files).
//   2. namespace fx1 -> sauria_rtl (this tree has its own fx1::PerfCounters with a different field set: no
//      ctrl_state_cycles / act|wei_feed|stall_cycles / act|wei_l1_read_words / srama|b|c_*; one name would clash).
//   3. internal include paths -> instrumentation/rtl_ref_*.h.
//
// Kept verbatim because the port is accepted only when its run.log / CSV numbers match sauria_model exactly on
// the same job; any formula change here would break that. Behaviour changes belong in sauria_model first.
// The core's rtl_ref_* files only change includes and type names to point here.
#ifndef SAURIA_RTL_EVAL_COUNTERS_H
#define SAURIA_RTL_EVAL_COUNTERS_H

// DMA timing model + EVAL (NEON Studio) counter contract.
//
// The df_controller emu (tb_evaluate.cpp::run_tiles_and_verify, -DNPU_DF_EMU=1)
// is functional: SRAM loads/stores happen in zero simulated time. This header
// adds an ANALYTIC DMA timing model layered on the REAL per-tile byte counts
// and the REAL per-tile compute cycles (PerfCounters exec delta), producing the
// per-layer counters the EVAL team consumes (VP_RTL_Instrumentation_Spec P0):
//   total/processing/transfer/transfer_overhead cycles, dma_stall, wait_input,
//   mac/dma engine cycles, ddr_read/write_bytes, weight_bytes, l2<->l1 bytes.
//
// Schedule model (double_buffer=1, one shared DMA engine, dual SRAM buffers):
//   during compute of tile t the DMA performs store(t-1) + load(t+1);
//   stall(t) = max(0, store(t-1)+load(t+1) - compute(t)).
//   total = load(0) + sum(compute) + sum(stall) + store(last).
// double_buffer=0 serializes: total = sum(load + compute + store).
//
// Field names match the EVAL "Output CSV Column" names on purpose — do not
// rename without syncing tools/dse_sweep.py and the EVAL spec.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include "instrumentation/rtl_ref_perf_counters.h"   // core-attached measured counters (ingested by finalize)
#include "instrumentation/rtl_ref_axi_timing.h"

namespace sauria_rtl
{
  struct DmaTimingParams
  {
    // ---- Level-1 (external): L2/SRAM <-> DDR ----
    double bw_bytes_per_cycle{20.0}; // external DDR BW: 16 GB/s @ 0.8 GHz = 20 B/cycle
    double ddr_gbps{16.0};           // as configured (for reporting)
    double freq_ghz{0.8};
    uint32_t setup_cycles{20};       // per DMA transfer (descriptor + burst setup)
    bool double_buffer{true};

    // ---- Level-2 (internal): L1/array-feed <-> L2/SRAM ----
    // On-chip SRAM<->array-operand bandwidth. Default 64 B/cycle from the
    // Ambarella npm4k partner template (L1<->L2 = 64 B/cyc); placeholder until
    // the HW team confirms. This is a SUPPLY-side HW port BW distinct from the
    // geometry-derived array DEMAND (X+Y)*ebytes. It does NOT alter total_cycles:
    // the array feed is already inside the measured processing_cycles, so charging
    // it again would double-count. It only sizes internal_transfer_cycles and the
    // internal-bandwidth roofline knee (l2_traffic != ddr_traffic). Env NPU_L1L2_BPC.
    double int_bytes_per_cycle{64.0};

    // Env knobs so a BW sweep needs no rebuild:
    //   NPU_DMA_GBPS (default 16, per Rev2 dual-port DMA diagram — placeholder
    //   until the HW team confirms; see memory rev2-open-questions #1),
    //   NPU_FREQ_GHZ (default 0.8), NPU_DMA_SETUP (default 20), NPU_DMA_DBUF (0/1),
    //   NPU_L1L2_BPC (default 64, internal SRAM<->array B/cycle — partner template).
    static DmaTimingParams from_env()
    {
      DmaTimingParams p;
      if (const char *s = std::getenv("NPU_DMA_GBPS"))
        p.ddr_gbps = std::atof(s);
      if (const char *s = std::getenv("NPU_FREQ_GHZ"))
        p.freq_ghz = std::atof(s);
      if (const char *s = std::getenv("NPU_DMA_SETUP"))
        p.setup_cycles = (uint32_t)std::atoi(s);
      if (const char *s = std::getenv("NPU_DMA_DBUF"))
        p.double_buffer = (std::atoi(s) != 0);
      if (const char *s = std::getenv("NPU_L1L2_BPC"))
        p.int_bytes_per_cycle = std::atof(s);
      if (p.freq_ghz > 0.0)
        p.bw_bytes_per_cycle = p.ddr_gbps / p.freq_ghz;
      return p;
    }

    // Level-1 external transfer (DDR<->SRAM): descriptor/burst setup applies.
    uint64_t xfer_cycles(uint64_t bytes) const
    {
      if (bytes == 0)
        return 0;
      return (uint64_t)std::ceil((double)bytes / bw_bytes_per_cycle) + setup_cycles;
    }

    // Level-2 internal transfer (SRAM<->array feed): on-chip, no DMA descriptor
    // setup. Used for reporting/roofline only (overlapped with compute).
    uint64_t int_xfer_cycles(uint64_t bytes) const
    {
      if (bytes == 0)
        return 0;
      const double bpc = int_bytes_per_cycle > 0.0 ? int_bytes_per_cycle : 1.0;
      return (uint64_t)std::ceil((double)bytes / bpc);
    }
  };

  struct EvalCounters
  {
    // Cycle counters (EVAL cycles.csv)
    uint64_t total_cycles{0};             // modeled end-to-end (compute + non-hidden DMA)
    uint64_t processing_cycles{0};        // real core cycles = span of non-IDLE FSM
    uint64_t core_exec_cycles{0};         // cycles with pipeline_en high (USEFUL), measured
    uint64_t transfer_cycles{0};          // total EXTERNAL DMA busy cycles (hidden or not)
    uint64_t transfer_overhead_cycles{0}; // non-hidden DMA = total - processing
    uint64_t internal_transfer_cycles{0}; // L1<->L2 busy cycles @ int BW (overlapped w/ compute)
    // Engine counters (engine.csv)
    uint64_t mac_engine_cycles{0}; // == processing (array = the MAC engine)
    uint64_t dma_engine_cycles{0}; // == transfer_cycles
    // Stall counters (dma.csv / stall.csv)
    uint64_t dma_stall_cycles{0};   // compute blocked waiting on DMA
    uint64_t wait_input_cycles{0};  // stall share attributable to loads
    uint64_t wait_output_cycles{0}; // stall share attributable to stores
    // Traffic counters (bandwidth.csv)
    uint64_t ddr_read_bytes{0};
    uint64_t ddr_write_bytes{0};
    uint64_t weight_bytes{0};   // subset of ddr_read (B reloads)
    uint64_t l2_to_l1_bytes{0}; // SRAM -> array operand feed
    uint64_t l1_to_l2_bytes{0}; // array -> SRAM output drain

    // ---- Batch A: derived per-run fields (spec-named; filled by finalize()) ----
    double   transfer_overhead_percent{0}; // cycles.csv
    uint64_t theory_min_cycles{0};         // cycles.csv  = ceil(MACs/PEs)
    uint64_t active_cycles{0};             // cycles.csv  = processing
    uint64_t idle_cycles{0};               // cycles.csv  = total - processing
    double   engine_utilization{0};        // engine.csv  = 100*mac_engine/total
    uint64_t dma_read_cycles{0};           // dma.csv     = read bytes / BW
    uint64_t dma_write_cycles{0};          // dma.csv     = write bytes / BW
    uint64_t dma_busy_cycles{0};           // dma.csv     = transfer_cycles
    uint64_t dma_idle_cycles{0};           // dma.csv     = total - transfer
    uint64_t dma_wait_cycles{0};           // dma.csv     = dma_stall (wait DMA done)
    uint64_t mac_active_cycles{0};         // utilization.csv = processing
    uint64_t mac_idle_cycles{0};           // utilization.csv = total - mac_active
    uint64_t pe_active_cycles{0};          // utilization.csv = measured (from perf)
    uint64_t pe_idle_cycles{0};            // utilization.csv = total - pe_active
    uint64_t engine_active_cycles{0};      // utilization.csv = max(mac,dma)
    uint64_t engine_idle_cycles{0};        // utilization.csv = total - engine_active
    uint64_t bias_bytes{0};                // bandwidth.csv
    uint64_t l1_read_bytes{0};             // bandwidth.csv = l2_to_l1
    uint64_t l1_write_bytes{0};            // bandwidth.csv = l1_to_l2
    uint64_t l2_read_bytes{0};             // bandwidth.csv = ddr_read
    uint64_t l2_write_bytes{0};            // bandwidth.csv = ddr_write
    uint64_t l2_footprint_bytes{0};        // memory.csv    = working set
    uint64_t config_bytes{0};              // memory.csv    = ddr_footprint_control

    // Scheduling state (internal)
    uint64_t prev_store_cycles{0};
    uint64_t pending_first_load{0};
    uint32_t tiles_seen{0};

    // Accumulate one tile through the schedule model.
    //   load_b / store_b: bytes actually moved DDR<->SRAM for this tile
    //   compute: real exec-cycle delta for this tile's core run
    void add_tile(const DmaTimingParams &p, uint64_t load_b, uint64_t store_b,
                  uint64_t compute)
    {
      const uint64_t load_c = p.xfer_cycles(load_b);
      const uint64_t store_c = p.xfer_cycles(store_b);
      processing_cycles += compute;
      transfer_cycles += load_c + store_c;

      if (!p.double_buffer)
      {
        // Serial: every transfer is exposed.
        total_cycles += load_c + compute + store_c;
        dma_stall_cycles += load_c + store_c;
        wait_input_cycles += load_c;
        wait_output_cycles += store_c;
      }
      else
      {
        if (tiles_seen == 0)
        {
          // Prologue load of tile 0 cannot be hidden.
          pending_first_load = load_c;
          total_cycles += load_c + compute;
          dma_stall_cycles += load_c;
          wait_input_cycles += load_c;
        }
        else
        {
          // This tile's load was queued during the PREVIOUS tile's compute,
          // but our per-tile loop learns load(t) only now — equivalent model:
          // charge stall against THIS tile's compute for [store(t-1)+load(t)].
          const uint64_t dma_work = prev_store_cycles + load_c;
          const uint64_t stall = (dma_work > compute) ? (dma_work - compute) : 0;
          total_cycles += compute + stall;
          dma_stall_cycles += stall;
          if (stall && dma_work)
          {
            // Prorate the exposed stall between input-load and output-store.
            wait_input_cycles += (uint64_t)((double)stall * (double)load_c / (double)dma_work);
            wait_output_cycles += stall - (uint64_t)((double)stall * (double)load_c / (double)dma_work);
          }
        }
        prev_store_cycles = store_c;
      }

      ddr_read_bytes += load_b;
      ddr_write_bytes += store_b;
      tiles_seen++;
    }

    // Epilogue: last tile's store cannot overlap any compute.
    void finish(const DmaTimingParams &p)
    {
      if (p.double_buffer && tiles_seen > 0)
      {
        total_cycles += prev_store_cycles;
        dma_stall_cycles += prev_store_cycles;
        wait_output_cycles += prev_store_cycles;
        prev_store_cycles = 0;
      }
      transfer_overhead_cycles =
          (total_cycles > processing_cycles) ? (total_cycles - processing_cycles) : 0;
      mac_engine_cycles = processing_cycles;
      dma_engine_cycles = transfer_cycles;
    }

    // Inputs the aggregator needs from the config/workload to derive the rest.
    struct FinalizeIn {
      uint64_t macs{0};          // M*K*N of this layer (algorithmic)
      uint32_t pes{0};           // X*Y
      uint64_t bias_bytes{0};    // Cout*4 (int32 bias)
      uint64_t l2_footprint{0};  // SRAM working set bytes (A+B+C tile)
      uint64_t config_bytes{0};  // controller_args bytes (N_REGS*4)
      const sauria_rtl::PerfCounters *perf{nullptr};  // measured counters (optional)
      DmaTimingParams dma;
    };

    // Compute all Batch-A derived fields from the accumulated + config inputs.
    // Call AFTER finish(). Keeps every value tied to a measured/analytic source.
    void finalize(const FinalizeIn &in)
    {
      const double bpc = in.dma.bw_bytes_per_cycle > 0 ? in.dma.bw_bytes_per_cycle : 1.0;
      transfer_overhead_percent =
          total_cycles ? 100.0 * (double)transfer_overhead_cycles / (double)total_cycles : 0.0;
      theory_min_cycles = in.pes ? (in.macs + in.pes - 1) / in.pes : 0;   // ceil(MACs/PEs)
      // active = measured USEFUL cycles (pipeline_en high), NOT
      // processing_cycles (the core BUSY span, including stall cycles).
      // Falls back to processing_cycles only when the exec counter is not wired.
      const uint64_t act_c = core_exec_cycles ? core_exec_cycles : processing_cycles;
      active_cycles = act_c;
      idle_cycles = (total_cycles > act_c) ? total_cycles - act_c : 0;
      engine_utilization =
          total_cycles ? 100.0 * (double)mac_engine_cycles / (double)total_cycles : 0.0;
      // apply_burst_dma() sets these two fields from the real burst description.
      // Derive them from bytes/bandwidth only when nobody has set them -- otherwise
      // the coarse estimate would overwrite the more accurate number.
      if (!dma_read_cycles)
        dma_read_cycles = (uint64_t)std::ceil((double)ddr_read_bytes / bpc);
      if (!dma_write_cycles)
        dma_write_cycles = (uint64_t)std::ceil((double)ddr_write_bytes / bpc);
      dma_busy_cycles = transfer_cycles;
      dma_idle_cycles = (total_cycles > transfer_cycles) ? total_cycles - transfer_cycles : 0;
      dma_wait_cycles = dma_stall_cycles;
      mac_active_cycles = act_c;   // measured -- cycles in which the PE array really computes
      mac_idle_cycles = (total_cycles > mac_active_cycles) ? total_cycles - mac_active_cycles : 0;
      // pe_active_cycles is a CYCLE count (cycles the PE array is active). In an
      // output-stationary systolic array PE-active == array-active == processing.
      // (in.perf->active_pe_cycles is the AGGREGATE PE·cycles used for MAC-util %,
      //  not a cycle count, so don't use it here.)
      pe_active_cycles = act_c;    // mang output-stationary -> PE-active == array-active
      pe_idle_cycles = (total_cycles > pe_active_cycles) ? total_cycles - pe_active_cycles : 0;
      engine_active_cycles = (mac_engine_cycles > dma_engine_cycles) ? mac_engine_cycles : dma_engine_cycles;
      engine_idle_cycles = (total_cycles > engine_active_cycles) ? total_cycles - engine_active_cycles : 0;
      bias_bytes = in.bias_bytes;
      l1_read_bytes = l2_to_l1_bytes;
      l1_write_bytes = l1_to_l2_bytes;
      l2_read_bytes = ddr_read_bytes;
      l2_write_bytes = ddr_write_bytes;
      l2_footprint_bytes = in.l2_footprint;
      config_bytes = in.config_bytes;
      // Level-2 internal (SRAM<->array) busy cycles at the internal HW port BW.
      // Reporting/roofline only — the array feed is already inside processing_cycles,
      // so this is NOT added to total_cycles (would double-count). Lets the roofline
      // draw a distinct internal knee (l2_traffic != ddr_traffic).
      internal_transfer_cycles = in.dma.int_xfer_cycles(l2_to_l1_bytes + l1_to_l2_bytes);
    }

    // --- I1b (R-C): burst-accurate DMA schedule (C1a/C1b) ---------------------
    // One external tile's DMA burst/beat counts (from tools/rc_dma_model.py, offline
    // config) + its measured compute cycles. Fed to apply_burst_dma().
    struct BurstTile
    {
      uint64_t load_bursts{0}, load_beats{0};    // A/B/C_read inputs
      uint64_t store_bursts{0}, store_beats{0};  // C_write output
      uint64_t compute{0};                       // measured array-active cycles this tile
      uint64_t load_commands{0}, store_commands{0};
      std::vector<uint16_t> load_burst_beats;
      std::vector<uint16_t> store_burst_beats;
    };

    // Replace the byte/BW DMA model (add_tile/finish) with the burst-accurate double-buffer
    // schedule: per-transfer cost = bursts*A + beats*B (A=54 = DRAM
    // latency 50 + turnaround 4; B=2.60 beat throttle -- DramParams::sauria_tb). Schedule:
    //   exposed = load(0) + store(T-1) + sum_t max(0, load(t+1)+store(t-1) - compute(t))
    // This is the SAME schedule instrumentation/dma_dram.h SIMULATES (make dma-c1-gate
    // proves SC-sim == this arithmetic == RTL golden within +/-5%, holdout -2.5%). Fills the
    // DMA cycle fields directly (no add_tile/finish); call finalize() after for derived.
    void apply_burst_dma(const std::vector<BurstTile> &tiles, double A, double B)
    {
      const int n = (int)tiles.size();
      if (n == 0) return;
      // from_env() makes the bandwidth axis sweepable; with no variable set
      // the Verilator testbench configuration is used unchanged.
      const AxiTimingParams axi = AxiTimingParams::from_env();
      auto ld = [&](int t) {
        if (!tiles[t].load_burst_beats.empty())
          return (double)axi_transfer_timing(
              tiles[t].load_commands, tiles[t].load_burst_beats, axi).accel_cycles(axi.sys_per_accel);
        return (double)tiles[t].load_bursts * A + (double)tiles[t].load_beats * B;
      };
      auto st = [&](int t) {
        if (!tiles[t].store_burst_beats.empty())
          return (double)axi_transfer_timing(
              tiles[t].store_commands, tiles[t].store_burst_beats, axi).accel_cycles(axi.sys_per_accel);
        return (double)tiles[t].store_bursts * A + (double)tiles[t].store_beats * B;
      };
      double read_busy = 0, write_busy = 0, comp = 0;
      for (int t = 0; t < n; t++) { read_busy += ld(t); write_busy += st(t); comp += (double)tiles[t].compute; }
      double exposed = ld(0) + st(n - 1);           // prologue load + tail store (always exposed)
      double wait_in = ld(0), wait_out = st(n - 1);
      for (int t = 0; t < n; t++)
      {
        const double l = (t + 1 < n) ? ld(t + 1) : 0.0;   // preload next tile
        const double s = (t - 1 >= 0) ? st(t - 1) : 0.0;  // store previous tile
        const double dma_work = l + s;
        const double stall = (dma_work > (double)tiles[t].compute) ? (dma_work - (double)tiles[t].compute) : 0.0;
        exposed += stall;
        if (stall > 0 && dma_work > 0)
        {
          wait_in += stall * l / dma_work;
          wait_out += stall * s / dma_work;
        }
      }
      processing_cycles = (uint64_t)std::llround(comp);
      dma_read_cycles = (uint64_t)std::llround(read_busy);
      dma_write_cycles = (uint64_t)std::llround(write_busy);
      transfer_cycles = dma_read_cycles + dma_write_cycles;
      dma_busy_cycles = transfer_cycles;
      dma_engine_cycles = transfer_cycles;
      mac_engine_cycles = processing_cycles;
      total_cycles = (uint64_t)std::llround(comp + exposed);
      dma_stall_cycles = (uint64_t)std::llround(exposed);
      dma_wait_cycles = dma_stall_cycles;
      wait_input_cycles = (uint64_t)std::llround(wait_in);
      wait_output_cycles = (uint64_t)std::llround(wait_out);
      transfer_overhead_cycles = (total_cycles > processing_cycles) ? total_cycles - processing_cycles : 0;
      dma_idle_cycles = (total_cycles > transfer_cycles) ? total_cycles - transfer_cycles : 0;
      {   // as finalize() -- active = measured USEFUL beats.
        const uint64_t act_c = core_exec_cycles ? core_exec_cycles : processing_cycles;
        active_cycles = act_c;
        idle_cycles = (total_cycles > act_c) ? total_cycles - act_c : 0;
      }
      tiles_seen = (uint32_t)n;
    }

    // Fill fields the manual/default-build path (tb_evaluate.cpp, NPU_DF_EMU==0) leaves at
    // their zero-initialized default because it hand-builds this struct without calling
    // finalize()/apply_burst_dma() (the only two places that otherwise derive them). Safe to
    // call unconditionally: a no-op wherever finalize()/apply_burst_dma() already ran, since
    // those never leave dma_idle_cycles at 0 when total_cycles is nonzero and transfer < total.
    void derive_missing_defaults()
    {
      if (!dma_idle_cycles && total_cycles)
        dma_idle_cycles = (total_cycles > transfer_cycles) ? total_cycles - transfer_cycles : 0;
    }

    // Machine-readable block parsed by tools/dse_sweep.py (--eval-out).
    void report(const DmaTimingParams &p) const
    {
      printf("\n==== [EVAL] per-layer counters (DMA timing model) ====\n");
      printf("[EVAL] dma_model=ddr_gbps:%.2f,freq_ghz:%.2f,bytes_per_cycle:%.2f,"
             "setup_cycles:%u,double_buffer:%d,int_bytes_per_cycle:%.2f\n",
             p.ddr_gbps, p.freq_ghz, p.bw_bytes_per_cycle, p.setup_cycles,
             p.double_buffer ? 1 : 0, p.int_bytes_per_cycle);
      printf("[EVAL] total_cycles=%llu\n", (unsigned long long)total_cycles);
      printf("[EVAL] processing_cycles=%llu\n", (unsigned long long)processing_cycles);
      printf("[EVAL] core_exec_cycles=%llu\n", (unsigned long long)core_exec_cycles);
      printf("[EVAL] transfer_cycles=%llu\n", (unsigned long long)transfer_cycles);
      printf("[EVAL] transfer_overhead_cycles=%llu\n", (unsigned long long)transfer_overhead_cycles);
      printf("[EVAL] internal_transfer_cycles=%llu\n", (unsigned long long)internal_transfer_cycles);
      printf("[EVAL] mac_engine_cycles=%llu\n", (unsigned long long)mac_engine_cycles);
      printf("[EVAL] dma_engine_cycles=%llu\n", (unsigned long long)dma_engine_cycles);
      printf("[EVAL] dma_stall_cycles=%llu\n", (unsigned long long)dma_stall_cycles);
      printf("[EVAL] wait_input_cycles=%llu\n", (unsigned long long)wait_input_cycles);
      printf("[EVAL] wait_output_cycles=%llu\n", (unsigned long long)wait_output_cycles);
      printf("[EVAL] ddr_read_bytes=%llu\n", (unsigned long long)ddr_read_bytes);
      printf("[EVAL] ddr_write_bytes=%llu\n", (unsigned long long)ddr_write_bytes);
      printf("[EVAL] weight_bytes=%llu\n", (unsigned long long)weight_bytes);
      printf("[EVAL] l2_to_l1_bytes=%llu\n", (unsigned long long)l2_to_l1_bytes);
      printf("[EVAL] l1_to_l2_bytes=%llu\n", (unsigned long long)l1_to_l2_bytes);
      // Batch A derived (spec-named)
      printf("[EVAL] transfer_overhead_percent=%.4f\n", transfer_overhead_percent);
      printf("[EVAL] theory_min_cycles=%llu\n", (unsigned long long)theory_min_cycles);
      printf("[EVAL] active_cycles=%llu\n", (unsigned long long)active_cycles);
      printf("[EVAL] idle_cycles=%llu\n", (unsigned long long)idle_cycles);
      printf("[EVAL] engine_utilization=%.4f\n", engine_utilization);
      printf("[EVAL] dma_read_cycles=%llu\n", (unsigned long long)dma_read_cycles);
      printf("[EVAL] dma_write_cycles=%llu\n", (unsigned long long)dma_write_cycles);
      printf("[EVAL] dma_busy_cycles=%llu\n", (unsigned long long)dma_busy_cycles);
      printf("[EVAL] dma_idle_cycles=%llu\n", (unsigned long long)dma_idle_cycles);
      printf("[EVAL] dma_wait_cycles=%llu\n", (unsigned long long)dma_wait_cycles);
      printf("[EVAL] mac_active_cycles=%llu\n", (unsigned long long)mac_active_cycles);
      printf("[EVAL] mac_idle_cycles=%llu\n", (unsigned long long)mac_idle_cycles);
      printf("[EVAL] pe_active_cycles=%llu\n", (unsigned long long)pe_active_cycles);
      printf("[EVAL] pe_idle_cycles=%llu\n", (unsigned long long)pe_idle_cycles);
      printf("[EVAL] engine_active_cycles=%llu\n", (unsigned long long)engine_active_cycles);
      printf("[EVAL] engine_idle_cycles=%llu\n", (unsigned long long)engine_idle_cycles);
      printf("[EVAL] bias_bytes=%llu\n", (unsigned long long)bias_bytes);
      printf("[EVAL] l1_read_bytes=%llu\n", (unsigned long long)l1_read_bytes);
      printf("[EVAL] l1_write_bytes=%llu\n", (unsigned long long)l1_write_bytes);
      printf("[EVAL] l2_read_bytes=%llu\n", (unsigned long long)l2_read_bytes);
      printf("[EVAL] l2_write_bytes=%llu\n", (unsigned long long)l2_write_bytes);
      printf("[EVAL] l2_footprint_bytes=%llu\n", (unsigned long long)l2_footprint_bytes);
      printf("[EVAL] ddr_footprint_control_bytes=%llu\n", (unsigned long long)config_bytes);
      printf("=========================================================\n");
    }
  };
} // namespace sauria_rtl

#endif // SAURIA_RTL_EVAL_COUNTERS_H
