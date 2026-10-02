// rtl_ref_metrics_report.h -- verbatim port of sauria_model's instrumentation/metrics_report.h.
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
#ifndef SAURIA_RTL_METRICS_REPORT_H
#define SAURIA_RTL_METRICS_REPORT_H

// -- EVALUATION METRICS REPORT: one `metrics.log` file in the same output directory.
//
// Three rules:
//   1. Every metric prints its UNIT and FORMULA next to the value.
//   2. Every MEASURED number states which COUNTER and which WINDOW it comes from.
//   3. The header records all INPUTS (test, run-time configuration, frequency, bandwidth,
//      SRAM) so that sweep runs can be compared with each other.
//
// WARNING: THE ENERGY MODEL IS AN ORDER-OF-MAGNITUDE ESTIMATE. SAURIA provides no power
// data (test_type='power_estimation' only generates two convolutions for an EXTERNAL
// analysis flow, without pJ coefficients). Default coefficients are from Horowitz,
// "Computing's Energy Problem", ISSCC 2014 -- 45 nm node. No technology-node scaling
// is applied (SAURIA has not published its node). Override with environment variables.

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <ctime>
#include <string>
#include <cstdlib>
#include "instrumentation/rtl_ref_eval_counters.h"
#include "instrumentation/rtl_ref_perf_counters.h"

namespace sauria_rtl
{
  // One "key : value" line. Must be a plain function -- a lambda cannot take `...`.
  inline void mx_line(FILE *f, const char *key, const char *fmt, ...)
  {
    std::fprintf(f, "   %-26s : ", key);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(f, fmt, ap);
    va_end(ap);
    std::fprintf(f, "\n");
  }

  // ---------------------------------------------------------------------------
  // Energy coefficients. Default = Horowitz ISSCC'14, 45 nm.
  // ---------------------------------------------------------------------------
  struct EnergyParams
  {
    double mac_pj{0.3};        // int8 multiply (0.2) + int32 add (0.1)
    double sram_rd_pj32{10.0}; // ~32 KB SRAM read, per 32-bit word
    double sram_wr_pj32{10.0}; // SRAM write, per 32-bit word
    double dram_pj32{640.0};   // DRAM access, per 32-bit word
    double node_scale{1.0};    // technology-node scale factor. 1.0 = no scaling.

    static EnergyParams from_env()
    {
      EnergyParams e;
      if (const char *s = std::getenv("NPU_E_MAC_PJ"))     e.mac_pj = std::atof(s);
      if (const char *s = std::getenv("NPU_E_SRAM_RD_PJ32")) e.sram_rd_pj32 = std::atof(s);
      if (const char *s = std::getenv("NPU_E_SRAM_WR_PJ32")) e.sram_wr_pj32 = std::atof(s);
      if (const char *s = std::getenv("NPU_E_DRAM_PJ32"))  e.dram_pj32 = std::atof(s);
      if (const char *s = std::getenv("NPU_E_NODE_SCALE")) e.node_scale = std::atof(s);
      return e;
    }
  };

  // Run description -- section "1. INPUTS" of the report.
  struct MetricsInputs
  {
    std::string test_name{"(NPU_TEST_NAME not set)"};
    std::string build_config, dma_model, dtype{"int8"};
    // Do NOT print "M x K x N" from lc.M/K/N -- that is an internal trick (M = total output
    // elements, N = 1) only to make the product equal the MAC count; printing it would mislead.
    std::string shape_note;             // real shape, declared by the user (NPU_TEST_SHAPE)
    uint64_t out_elems{0};              // actual number of output elements (measured)
    uint64_t macs{0};                   // algorithmic MAC count (measured)
    uint32_t n_tiles{0}, ncontexts{0}, mvm_k{0};
    uint32_t loop_order{0};
    bool preload_en{false};
    uint32_t X{0}, Y{0};
    uint32_t act_bytes{1}, wei_bytes{1}, psum_bytes{4};
    double freq_ghz{0.8};
    uint64_t srama_cap{0}, sramb_cap{0}, sramc_cap{0}, l2_working_set{0};
    double ddr_gbps{16.0}, ddr_bytes_per_cycle{20.0};
    // bandwidth the burst/AXI model actually applies (derived from the throttle).
    double axi_eff_gbps{0.0};
    uint32_t axi_bw_up{0}, axi_bw_down{0}, axi_dram_lat_sys{0};
    bool dma_is_burst{true};
    uint32_t dma_setup{0};
    bool dma_double_buffer{true};
    // DMA counts (compared with the RTL golden measured with a Verilator probe)
    uint64_t dma_commands{0}, dma_bursts{0}, dma_beats{0};
    // Core counters
    uint64_t stage_total{0}, stage_idle{0}, stage_busy{0};
    uint64_t exec_cycles{0}, stall_cycles{0}, mac_ops{0};
    uint64_t srama_rd_bytes{0}, sramb_rd_bytes{0};
    uint64_t sramc_rd_bytes{0}, sramc_wr_bytes{0};
  };

  inline void write_metrics_log(const std::string &outdir,
                                const EvalCounters &e,
                                const MetricsInputs &in,
                                const EnergyParams &ep)
  {
    FILE *f = std::fopen((outdir + "/metrics.log").c_str(), "w");
    if (!f) return;

    const char *LOOP[] = {"keep_B", "keep_C", "keep_A", "?"};
    const double f_hz = in.freq_ghz * 1e9;
    const uint64_t pes = (uint64_t)in.X * in.Y;
    const uint64_t macs = in.macs;
    const double T_s = e.total_cycles / f_hz;   // latency of one inference [s]
    const double T_ms = T_s * 1e3;

    std::time_t now = std::time(nullptr);
    char ts[64]; std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::fprintf(f,
"================================================================================\n"
" EVALUATION METRICS REPORT -- SAURIA NPU core, SystemC model\n"
"================================================================================\n"
" Created  : %s\n"
" Directory: %s\n"
"\n"
" How to read: every metric in section 3 has a UNIT, a FORMULA and a SOURCE.\n"
" \"measured\" means read directly from a simulation counter -- the counter and the\n"
" window are named. \"derived\" means computed from measured numbers. \"estimate\"\n"
" means coefficients from outside the model -- NOT for conclusions.\n", ts, outdir.c_str());

    // ---------------------------------------------------------------- 1
    std::fprintf(f,
"\n--------------------------------------------------------------------------------\n"
" 1. INPUTS -- configuration of this run (sweep axes)\n"
"--------------------------------------------------------------------------------\n"
"\n 1.1 Test\n");
    mx_line(f, "test name", "%s", in.test_name.c_str());
    if (!in.shape_note.empty())
      mx_line(f, "shape (declared)", "%s   [NPU_TEST_SHAPE]", in.shape_note.c_str());
    else
      mx_line(f, "shape", "%s",
              "not declared -- set NPU_TEST_SHAPE=\"MxKxN\" to record it here");
    mx_line(f, "output elements", "%llu  [measured: summed over tiles]",
         (unsigned long long)in.out_elems);
    mx_line(f, "algorithmic MACs", "%llu  [measured: output elements * K per context]",
         (unsigned long long)macs);
    mx_line(f, "outer tiles", "%u", in.n_tiles);
    mx_line(f, "contexts per tile", "%u", in.ncontexts);
    mx_line(f, "K per context (mvm_k)", "%u", in.mvm_k);
    mx_line(f, "preload_en", "%s", in.preload_en ? "1 (C reloaded from DRAM)" : "0");
    mx_line(f, "loop_order", "%u (%s)", in.loop_order, LOOP[in.loop_order & 3]);

    std::fprintf(f, "\n 1.2 Simulated hardware\n");
    mx_line(f, "PE array geometry", "%u x %u = %llu PE", in.X, in.Y, (unsigned long long)pes);
    mx_line(f, "data type", "%s  (A %u B, B %u B, psum %u B)",
         in.dtype.c_str(), in.act_bytes, in.wei_bytes, in.psum_bytes);
    mx_line(f, "frequency", "%.3f GHz   [model ASSUMPTION -- not published by SAURIA]",
         in.freq_ghz);
    mx_line(f, "SRAM-A capacity", "%llu B", (unsigned long long)in.srama_cap);
    mx_line(f, "SRAM-B capacity", "%llu B", (unsigned long long)in.sramb_cap);
    mx_line(f, "SRAM-C capacity", "%llu B", (unsigned long long)in.sramc_cap);
    mx_line(f, "SRAM total", "%llu B", (unsigned long long)(in.srama_cap + in.sramb_cap + in.sramc_cap));
    mx_line(f, "working set used", "%llu B  [A+B+C of ONE tile]",
         (unsigned long long)in.l2_working_set);
    if (in.srama_cap + in.sramb_cap + in.sramc_cap)
      mx_line(f, "SRAM usage", "%.2f %%  [= working set / SRAM total]",
           100.0 * (double)in.l2_working_set
                 / (double)(in.srama_cap + in.sramb_cap + in.sramc_cap));

    std::fprintf(f, "\n 1.3 DMA / external memory\n");
    if (in.dma_is_burst)
    {
      mx_line(f, "EFFECTIVE DDR bandwidth", "%.2f GB/s   <-- ACTUAL SWEEP AXIS",
           in.axi_eff_gbps);
      mx_line(f, "  from the AXI throttle", "%u beats run / %u sys cycles idle, sys = %ux core",
           in.axi_bw_up, in.axi_bw_down, 2u);
      mx_line(f, "  DRAM latency per burst", "%u sys cycles   [PLACEHOLDER]", in.axi_dram_lat_sys);
      mx_line(f, "NPU_DMA_GBPS", "%.2f GB/s   [NO effect in the burst model --",
           in.ddr_gbps);
      mx_line(f, "", "%s", "feeds the byte model only. Sweep NPU_AXI_BW_UP/DOWN]");
    }
    else
      mx_line(f, "nominal DDR bandwidth", "%.2f GB/s = %.2f B per cycle   [PLACEHOLDER]",
           in.ddr_gbps, in.ddr_bytes_per_cycle);
    mx_line(f, "setup cost per command", "%u cycles   [PLACEHOLDER]", in.dma_setup);
    mx_line(f, "double buffer", "%s", in.dma_double_buffer ? "yes" : "no");
    mx_line(f, "DMA model", "%s", in.dma_model.c_str());
    mx_line(f, "DMA commands/bursts/beats", "%llu / %llu / %llu   [measured -- RTL burst split]",
         (unsigned long long)in.dma_commands, (unsigned long long)in.dma_bursts,
         (unsigned long long)in.dma_beats);

    std::fprintf(f, "\n 1.4 Build configuration\n");
    mx_line(f, "build flags", "%s", in.build_config.c_str());

    // ---------------------------------------------------------------- 2
    std::fprintf(f,
"\n--------------------------------------------------------------------------------\n"
" 2. RAW MEASUREMENTS -- which counter, which window\n"
"--------------------------------------------------------------------------------\n");
    std::fprintf(f,
"   %-28s %14s  %-8s %s\n", "quantity", "value", "unit", "measured where");
    std::fprintf(f, "   %s\n", std::string(104, '-').c_str());
    auto meas = [&](const char *k, unsigned long long v, const char *u, const char *src) {
      std::fprintf(f, "   %-28s %14llu  %-8s %s\n", k, v, u, src);
    };
    meas("FSM cycles total", in.stage_total, "cycles",
         "PerfCounters::ctrl_state_cycles[] summed over all states");
    meas("FSM cycles in IDLE", in.stage_idle, "cycles", "ctrl_state_cycles[0] (s00 IDLE)");
    meas("core BUSY span", in.stage_busy, "cycles",
         "total minus IDLE -- the quantity comparable with RTL");
    meas("ENABLED cycles", in.exec_cycles, "cycles",
         "sa_array.h: counts every cycle the array computes");
    meas("pipeline stall cycles", in.stall_cycles, "cycles", "busy span minus enabled cycles");
    meas("non-gated MACs", in.mac_ops, "ops", "sa_array.h: both operands non-zero");
    meas("SRAM-A read", in.srama_rd_bytes, "B",
         "sram_top.h rden port -- windowed o_start..i_done");
    meas("SRAM-B read", in.sramb_rd_bytes, "B", "sram_top.h rden port, same window");
    meas("SRAM-C write", in.sramc_wr_bytes, "B",
         "sram_top.h wren port, only mask-enabled elements");
    meas("SRAM-C read", in.sramc_rd_bytes, "B", "sram_top.h rden port");
    meas("DDR read", e.ddr_read_bytes, "B", "bytes actually loaded per tile (A + B + C reload)");
    meas("DDR write", e.ddr_write_bytes, "B", "bytes actually written per tile");
    meas("total cycles", e.total_cycles, "cycles",
         "busy span + unhidden DMA (double-buffer schedule)");

    // ---------------------------------------------------------------- 3
    std::fprintf(f,
"\n--------------------------------------------------------------------------------\n"
" 3. EVALUATION METRICS\n"
"--------------------------------------------------------------------------------\n");
    auto metric = [&](const char *name, double val, const char *unit,
                      const char *formula, const char *prov) {
      std::fprintf(f, "\n * %s\n", name);
      std::fprintf(f, "     value    : %.6g %s\n", val, unit);
      std::fprintf(f, "     formula  : %s\n", formula);
      std::fprintf(f, "     source   : %s\n", prov);
    };

    std::fprintf(f, "\n 3.1 Time and throughput\n");
    metric("Latency of one inference", T_ms, "ms",
           "T = total_cycles / f",
           "measured (total_cycles) + assumption (f = frequency)");
    metric("Throughput (FPS)", T_s > 0 ? 1.0 / T_s : 0.0, "inferences/s",
           "FPS = 1 / T = f / total_cycles",
           "derived from T");
    const double tops = T_s > 0 ? 2.0 * (double)macs / T_s / 1e12 : 0.0;
    const double tops_peak = 2.0 * (double)pes * f_hz / 1e12;
    metric("Achieved TOPS", tops, "TOPS",
           "TOPS = 2 * MACs / T / 1e12   (1 MAC = 2 ops: 1 multiply + 1 add)",
           "derived from algorithmic MACs and T");
    metric("Theoretical peak TOPS", tops_peak, "TOPS",
           "TOPS_peak = 2 * PEs * f / 1e12",
           "derived from geometry and frequency");
    metric("Efficiency vs peak", tops_peak > 0 ? 100.0 * tops / tops_peak : 0.0, "%",
           "= achieved TOPS / theoretical peak TOPS",
           "derived");

    std::fprintf(f, "\n 3.2 Utilisation\n");
    metric("PE array utilisation", e.processing_cycles > 0
             ? 100.0 * (double)e.theory_min_cycles / (double)e.processing_cycles : 0.0, "%",
           "= theoretical lower bound / core busy span",
           "both measured (lower bound = ceil(MACs / PEs))");
    metric("Useful cycle ratio", e.processing_cycles > 0
             ? 100.0 * (double)e.active_cycles / (double)e.processing_cycles : 0.0, "%",
           "= enabled cycles / core busy span",
           "both measured -- the rest is pipeline stall");
    metric("Stall ratio", in.stage_busy > 0
             ? 100.0 * (double)in.stall_cycles / (double)in.stage_busy : 0.0, "%",
           "= stall cycles / busy span",
           "measured");
    metric("Exposed DMA ratio", e.total_cycles > 0
             ? 100.0 * (double)e.transfer_overhead_cycles / (double)e.total_cycles : 0.0, "%",
           "= unhidden DMA cycles / total cycles",
           "derived from the double-buffer schedule over measured bursts");

    std::fprintf(f, "\n 3.3 Bandwidth and arithmetic intensity\n");
    const double ddr_bytes = (double)(e.ddr_read_bytes + e.ddr_write_bytes);
    const double int_bytes = (double)(e.l2_to_l1_bytes + e.l1_to_l2_bytes);
    metric("Achieved DDR bandwidth", T_s > 0 ? ddr_bytes / T_s / 1e9 : 0.0, "GB/s",
           "= (DDR bytes read + DDR bytes written) / T / 1e9",
           "measured (bytes) + assumption (f in T)");
    {
      const double bw_ref = in.dma_is_burst && in.axi_eff_gbps > 0
                              ? in.axi_eff_gbps : in.ddr_gbps;
      metric("DDR bandwidth utilisation", bw_ref > 0 && T_s > 0
               ? 100.0 * (ddr_bytes / T_s / 1e9) / bw_ref : 0.0, "%",
             "= achieved bandwidth / EFFECTIVE bandwidth of the active model",
             "denominator from the AXI throttle in the burst model -- see 1.3");
    }
    metric("SRAM<->array bandwidth", T_s > 0 ? int_bytes / T_s / 1e9 : 0.0, "GB/s",
           "= (SRAM-to-array bytes + array-to-SRAM bytes) / T / 1e9",
           "measured at the memories, windowed to the core run");
    metric("Arithmetic intensity", ddr_bytes > 0 ? 2.0 * (double)macs / ddr_bytes : 0.0, "ops/byte",
           "= 2 * MACs / DDR bytes   (x axis of the roofline plot)",
           "derived");
    metric("Beat alignment amplification", ddr_bytes > 0
             ? (double)in.dma_beats * 128.0 / ddr_bytes : 0.0, "x",
           "= beats * 128 B / useful bytes   (1.0 = no waste)",
           "measured (beats counted as the RTL splits bursts)");

    // ---------------------------------------------------------------- 3.4 energy
    const double e_mac_pj  = (double)in.mac_ops * ep.mac_pj;
    const double sram_rd_w = (double)(in.srama_rd_bytes + in.sramb_rd_bytes
                                      + in.sramc_rd_bytes) / 4.0;
    const double sram_wr_w = (double)in.sramc_wr_bytes / 4.0;
    const double e_sram_pj = sram_rd_w * ep.sram_rd_pj32 + sram_wr_w * ep.sram_wr_pj32;
    const double e_dram_pj = (ddr_bytes / 4.0) * ep.dram_pj32;
    const double e_tot_pj  = (e_mac_pj + e_sram_pj + e_dram_pj) * ep.node_scale;
    const double e_tot_mj  = e_tot_pj * 1e-12 * 1e3;          // pJ -> mJ
    const double p_mw      = T_s > 0 ? (e_tot_pj * 1e-12) / T_s * 1e3 : 0.0;
    const double tops_w    = e_tot_pj > 0
                             ? (2.0 * (double)macs) / (e_tot_pj * 1e-12) / 1e12 : 0.0;

    std::fprintf(f,
"\n 3.4 Energy and power\n"
"\n"
"     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
"     !!  ORDER-OF-MAGNITUDE ESTIMATE - NOT A MEASUREMENT. NOT FOR CONCLUSIONS !!\n"
"     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
"\n"
"     SAURIA provides no power data. The coefficients below come from published\n"
"     data for the 45 nm node:\n"
"       Horowitz, \"Computing's Energy Problem (and what we can do about it)\",\n"
"       ISSCC 2014, Figure 1.1.4 -- energy per operation at 45 nm.\n"
"     EVENT COUNTS are real measurements; only the COEFFICIENTS are external. The\n"
"     coefficient error can reach several x, and no technology-node scaling is applied\n"
"     (SAURIA has not published its node). Override with environment variables:\n"
"       NPU_E_MAC_PJ  NPU_E_SRAM_RD_PJ32  NPU_E_SRAM_WR_PJ32  NPU_E_DRAM_PJ32\n"
"       NPU_E_NODE_SCALE\n"
"\n"
"     Coefficients in use:\n"
"       MAC int8 (mul+add)     = %.3f pJ\n"
"       SRAM read (per 32 bit) = %.3f pJ\n"
"       SRAM write(per 32 bit) = %.3f pJ\n"
"       DRAM access (32 bit)   = %.3f pJ\n"
"       node scale factor      = %.3f  (1.0 = no scaling)\n"
"\n"
"     Breakdown:\n"
"       PE array = %12.3f uJ  (%5.1f %%)\n"
"       SRAM     = %12.3f uJ  (%5.1f %%)\n"
"       DRAM     = %12.3f uJ  (%5.1f %%)\n",
      ep.mac_pj, ep.sram_rd_pj32, ep.sram_wr_pj32, ep.dram_pj32, ep.node_scale,
      e_mac_pj * 1e-6, e_tot_pj > 0 ? 100.0 * e_mac_pj * ep.node_scale / e_tot_pj : 0.0,
      e_sram_pj * 1e-6, e_tot_pj > 0 ? 100.0 * e_sram_pj * ep.node_scale / e_tot_pj : 0.0,
      e_dram_pj * 1e-6, e_tot_pj > 0 ? 100.0 * e_dram_pj * ep.node_scale / e_tot_pj : 0.0);

    metric("Energy per inference", e_tot_mj, "mJ  [ESTIMATE]",
           "E = MACs * e_MAC + (SRAM bytes / 4) * e_SRAM + (DRAM bytes / 4) * e_DRAM",
           "event counts = MEASURED; coefficients = Horowitz ISSCC'14 45 nm (external)");
    metric("Average power", p_mw, "mW  [ESTIMATE]",
           "P = E / T",
           "derived from E (estimate) and T (measured)");
    metric("Energy efficiency", tops_w, "TOPS/W  [ESTIMATE]",
           "TOPS/W = 2 * MACs / E",
           "derived from E (estimate)");

    std::fprintf(f,
"\n--------------------------------------------------------------------------------\n"
" 4. RELIABILITY NOTES\n"
"--------------------------------------------------------------------------------\n"
"   * Compute core   : bit- and cycle-exact with the RTL.\n"
"   * DMA            : COMMAND/BURST/BEAT counts match the golden measured on the RTL;\n"
"                      timing uses a transaction-level AXI model.\n"
"   * DDR bandwidth and setup cost: PLACEHOLDERS, awaiting hardware confirmation.\n"
"     => ABSOLUTE bandwidth numbers are not for conclusions; compare only between\n"
"        runs with the SAME parameters.\n"
"   * Energy/power: ESTIMATE, see the warning in section 3.4.\n"
"   * The 0.8 GHz frequency is a model assumption.\n"
"================================================================================\n");
    std::fclose(f);
  }
} // namespace sauria_rtl

#endif // SAURIA_RTL_METRICS_REPORT_H
