// rtl_ref_perf_counters.h -- verbatim port of sauria_model's instrumentation/perf_counters.h.
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
#ifndef SAURIA_RTL_PERF_COUNTERS_H
#define SAURIA_RTL_PERF_COUNTERS_H

#include <cstdint>
#include <cstdio>
#include <string>

namespace sauria_rtl
{
  struct PerfCounters
  {
    // From the Systolia array (per-cycle active PE accounting)
    uint64_t active_pe_cycles{0}; // Sum over cycles of (#PEs doing a real MAC)
    uint64_t total_pe_cycles{0};  // Sum over cycles of (X*Y) while pipeline on
    uint64_t mac_ops{0};          // count of non-gated MACs actually performed

    // From the NPU top / control (timing)
    uint64_t exec_cycles{0};  // cycles with pipeline_en high (compute)
    uint64_t stall_cycles{0}; // cycles stalled (feeder empty / fifo full)
    uint64_t total_cycles{0}; // all clcoked cycles from start to done

    // Pha B -- per-controller-FSM-state cycle histogram. The controller adds 1 to
    // ctrl_state_cycles[state] every tick; accumulates across all tiles. RAW (no
    // bucketing) on purpose -- tools/ Python maps state index -> stage bucket so the
    // bucket taxonomy can change without a rebuild (Rev2-friendly). Index = the
    // ctrl_state_t enum in control/main_controller.h; names below MUST mirror it.
    static const int NUM_CTRL_STATES = 32; // >= enum size (IDLE..DONE = 25)
    uint64_t ctrl_state_cycles[NUM_CTRL_STATES]{};

    static const char *ctrl_state_name(int i)
    {
      static const char *N[] = {
          "IDLE", "START_FLAGS", "ARRAY_PREP", "ARRAY_FILL", "FIRST_SHIFT",
          "START_COMP", "DRAIN_FEED", "ARRAY_FLUSH", "WAIT_CSWITCH",
          "WAIT_CSWITCH_STALL", "WAIT_OBUF", "WAIT_OBUF_STALL", "SCND_SHIFT",
          "SCND_SHIFT_STALL", "ALL_BUSY_SHIFT", "ALL_BUSY", "ARRAY_BUSY",
          "OBUF_BUSY_SHIFT", "FORCE_STALL", "OBUF_BUSY", "ARRAY_CSWITCH",
          "ARRAY_CSWITCH_STALL", "LAST_SHIFT", "LAST_WAIT", "DONE"};
      return (i >= 0 && i < 25) ? N[i] : "?";
    }

    int X{0}, Y{0};

    // A1 -- per-feeder cycle/byte counters (A = activation/SRAM-A, B = weight/SRAM-B).
    // RAW, no bucketing (tools/ Python maps to load-A / load-B stage + reuse_ratio).
    // SAURIA is OUTPUT-STATIONARY: both feeders stream every context, so *_feed_cycles
    // are ~equal (lockstep); the A-vs-B signal lives in *_stall_cycles (independent SRAM
    // backpressure) and in wei_l1_read_words vs weight_bytes(DRAM) = weight reuse from
    // SRAM-B. Written by the feeders via their own guarded `if (perf)`; the FSM-state
    // window (exec) still bounds them. Only emitted when nonzero.
    uint64_t wei_feed_cycles{0}, wei_stall_cycles{0}, wei_l1_read_words{0};
    uint64_t act_feed_cycles{0}, act_stall_cycles{0}, act_l1_read_words{0};

    // -- SRAM traffic measured DIRECTLY at the memories (sram/sram_top.h), not
    // derived from the problem shape. Every accepted rden/wren cycle = one REAL
    // access, including re-reads and overwrites -- the traffic that actually
    // happens on the ports, which is what bandwidth.csv needs.
    //   A/B read  -> SRAM(L2) to PE array(L1)  = l2_to_l1_bytes / l1_read_bytes
    //   C write   -> PE array back to SRAM     = l1_to_l2_bytes / l1_write_bytes
    //   C read    -> psum read-back for accumulation (reported separately)
    uint64_t srama_read_beats{0}, srama_read_bytes{0};
    uint64_t sramb_read_beats{0}, sramb_read_bytes{0};
    uint64_t sramc_write_beats{0}, sramc_write_bytes{0};
    uint64_t sramc_read_beats{0}, sramc_read_bytes{0};

    // Derived metrics
    double pe_utilization() const
    {
      return total_pe_cycles ? (double)active_pe_cycles / total_pe_cycles : 0.0;
    }

    // -- the TIME quantity comparable with hardware. `exec_cycles` counts only
    // pipeline_en-high cycles (USEFUL cycles); the core actually occupies the span in
    // which the control FSM is NOT in IDLE. On test3 the two differ by ~1.8x (26156 vs
    // 46896) -- using the wrong one under-reports by 1.8x against the core RTL.
    // s00 = IDLE in BOTH enums (ctrl_state_t; ContextFsm::MainState after its
    // kMainToCtrl mapping), so the subtraction is valid with the guard on or off.
    uint64_t stage_total_cycles() const
    {
      uint64_t s = 0;
      for (int i = 0; i < NUM_CTRL_STATES; i++)
        s += ctrl_state_cycles[i];
      return s;
    }
    uint64_t stage_busy_cycles() const
    {
      const uint64_t t = stage_total_cycles();
      return (t > ctrl_state_cycles[0]) ? t - ctrl_state_cycles[0] : 0;
    }

    // -- fills two fields that are otherwise not wired: total = every clocked cycle;
    // stall = cycles the core is busy but the pipeline does NOT run (feeder empty /
    // FIFO full). Call after the run.
    void finalize_from_stage()
    {
      const uint64_t busy = stage_busy_cycles();
      total_cycles = stage_total_cycles();
      stall_cycles = (busy > exec_cycles) ? busy - exec_cycles : 0;
    }

    // Stall fraction of the core BUSY span, not of the total including IDLE.
    double stall_fraction() const
    {
      const uint64_t b = stage_busy_cycles();
      if (b)
        return (double)stall_cycles / (double)b;
      return total_cycles ? (double)stall_cycles / (double)total_cycles : 0.0;
    }

    void reset()
    {
      active_pe_cycles = total_pe_cycles = mac_ops = 0;
      exec_cycles = stall_cycles = total_cycles = 0;
      for (int i = 0; i < NUM_CTRL_STATES; i++)
        ctrl_state_cycles[i] = 0;
      wei_feed_cycles = wei_stall_cycles = wei_l1_read_words = 0;
      act_feed_cycles = act_stall_cycles = act_l1_read_words = 0;
      srama_read_beats = srama_read_bytes = 0;
      sramb_read_beats = sramb_read_bytes = 0;
      sramc_write_beats = sramc_write_bytes = 0;
      sramc_read_beats = sramc_read_bytes = 0;
    }

    void report(const std::string &tag = "") const
    {
      printf("\n==== [PERF] %s (array %dx%d) ====\n", tag.c_str(), X, Y);
      printf("  total cycles      : %llu\n", (unsigned long long)total_cycles);
      printf("  core busy cycles  : %llu  <- compare with the RTL core\n",
             (unsigned long long)stage_busy_cycles());
      printf("  exec cycles       : %llu\n", (unsigned long long)exec_cycles);
      printf("  stall cycles      : %llu  (%.1f%%)\n",
             (unsigned long long)stall_cycles, stall_fraction() * 100.0);
      printf("  MAC ops performed : %llu\n", (unsigned long long)mac_ops);
      printf("  PE utilization    : %.1f%%\n", pe_utilization() * 100.0);
      printf("=========================================\n");
      report_stage();
      report_feed();
      report_sram();
    }

    // A1 -- emit RAW per-feeder cycle/byte counters. Parsed by tools/ (feed_report /
    // dse_sweep) into load-A / load-B stage + reuse_ratio. Only printed when nonzero.
    void report_feed() const
    {
      if ((wei_feed_cycles | act_feed_cycles | wei_l1_read_words | act_l1_read_words) == 0)
        return; // feeders not instrumented / not run
      printf("\n==== [FEED] per-feeder cycles / L1 reads (A=act/SRAM-A, B=wei/SRAM-B) ====\n");
      printf("[FEED] act_feed_cycles   = %llu\n", (unsigned long long)act_feed_cycles);
      printf("[FEED] act_stall_cycles  = %llu\n", (unsigned long long)act_stall_cycles);
      printf("[FEED] act_l1_read_words = %llu\n", (unsigned long long)act_l1_read_words);
      printf("[FEED] wei_feed_cycles   = %llu\n", (unsigned long long)wei_feed_cycles);
      printf("[FEED] wei_stall_cycles  = %llu\n", (unsigned long long)wei_stall_cycles);
      printf("[FEED] wei_l1_read_words = %llu\n", (unsigned long long)wei_l1_read_words);
    }

    // -- measured SRAM traffic. Printed only when there is data.
    void report_sram() const
    {
      if ((srama_read_beats | sramb_read_beats | sramc_write_beats | sramc_read_beats) == 0)
        return;
      printf("\n==== [SRAM] RAW memory traffic ====\n");
      printf("[SRAM] (RAW = every rden/wren cycle of the WHOLE simulation, including\n"
             "[SRAM]  when the core is idle and host-port SRAM loads. The number used in\n"
             "[SRAM]  bandwidth.csv is the per-tile-window [SRAM] line.)\n");
      printf("[SRAM] srama_read_beats  = %llu\n", (unsigned long long)srama_read_beats);
      printf("[SRAM] srama_read_bytes  = %llu\n", (unsigned long long)srama_read_bytes);
      printf("[SRAM] sramb_read_beats  = %llu\n", (unsigned long long)sramb_read_beats);
      printf("[SRAM] sramb_read_bytes  = %llu\n", (unsigned long long)sramb_read_bytes);
      printf("[SRAM] sramc_write_beats = %llu\n", (unsigned long long)sramc_write_beats);
      printf("[SRAM] sramc_write_bytes = %llu\n", (unsigned long long)sramc_write_bytes);
      printf("[SRAM] sramc_read_beats  = %llu\n", (unsigned long long)sramc_read_beats);
      printf("[SRAM] sramc_read_bytes  = %llu\n", (unsigned long long)sramc_read_bytes);
    }

    // Pha B -- emit the RAW per-controller-state cycle histogram. Parsed by
    // tools/ (stage_report / dse_sweep). Only nonzero states are printed.
    // Invariant: sum over states == controller run cycles.
    void report_stage() const
    {
      uint64_t sum = 0;
      for (int i = 0; i < NUM_CTRL_STATES; i++)
        sum += ctrl_state_cycles[i];
      if (sum == 0)
        return; // controller not instrumented / not run
      printf("\n==== [STAGE] controller per-state cycles ====\n");
      for (int i = 0; i < NUM_CTRL_STATES; i++)
        if (ctrl_state_cycles[i])
          printf("[STAGE] s%02d %-20s = %llu\n", i, ctrl_state_name(i),
                 (unsigned long long)ctrl_state_cycles[i]);
      printf("[STAGE] total = %llu\n", (unsigned long long)sum);
      printf("[STAGE] busy  = %llu   (total - s00 IDLE)\n",
             (unsigned long long)(sum > ctrl_state_cycles[0] ? sum - ctrl_state_cycles[0] : 0));
    }
  };
} // namespace sauria_rtl

#endif // SAURIA_RTL_PERF_COUNTERS_H