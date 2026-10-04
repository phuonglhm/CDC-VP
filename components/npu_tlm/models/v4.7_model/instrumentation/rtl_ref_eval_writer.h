// rtl_ref_eval_writer.h -- verbatim port of sauria_model's instrumentation/eval_writer.h.
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
#ifndef SAURIA_RTL_EVAL_WRITER_H
#define SAURIA_RTL_EVAL_WRITER_H

// I1a (R-C, full-SystemC deliverable) -- schema-driven eval-out CSV writer, in C++.
//
// The deliverable is a SystemC program that runs the model and writes the per-layer
// cycle/metric CSVs the SW EVAL app consumes -- WITHOUT a Python runtime. This mirrors
// tools/dse_sweep.py::write_eval_dir: read tools/eval_schema.csv (metric -> csv_file),
// then emit one CSV per csv_file with the EXACT spec column order. Values come from the
// C++ EvalCounters module (measured) + here-computed derived fields (I1c); a metric with
// no value is left blank (same policy as the Python writer's hybrid-blank cells).
//
// Column parity with the Python eval-out is the I1 gate (tools/eval_csv_parity.py).

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "instrumentation/rtl_ref_eval_counters.h"

namespace sauria_rtl
{
  // eval_schema.csv -> ordered [(csv_file, [metric,...]), ...] preserving first-seen
  // file order and in-file metric order (matches the Python DictReader traversal).
  struct EvalSchema
  {
    std::vector<std::string> files;                              // csv_file order
    std::map<std::string, std::vector<std::string>> metrics;     // csv_file -> metrics

    static EvalSchema load(const std::string &path)
    {
      EvalSchema s;
      std::ifstream fp(path);
      if (!fp) { fprintf(stderr, "[eval_writer] cannot open schema %s\n", path.c_str()); return s; }
      std::string line;
      std::getline(fp, line); // header: metric,csv_file,priority,status,raw_derived,unit,description
      while (std::getline(fp, line))
      {
        if (line.empty()) continue;
        std::stringstream ls(line);
        std::string metric, csvfile;
        std::getline(ls, metric, ',');
        std::getline(ls, csvfile, ',');
        if (metric.empty() || csvfile.empty()) continue;
        if (s.metrics.find(csvfile) == s.metrics.end())
          s.files.push_back(csvfile);
        s.metrics[csvfile].push_back(metric);
      }
      return s;
    }
  };

  // One layer's row: layer identity + metric -> formatted string ("" = blank cell).
  struct EvalRow
  {
    std::string layer_id{"0"};   // string -- the network summary row has id 'network'
    std::string layer_name;
    std::string op_type;   // propagated from the compiler (join key for the SW app)
    std::map<std::string, std::string> vals;
  };

  // I1c: shape/config context for the derived (non-core-measured) metrics. Mirrors the
  // fields tools/dse_sweep.py::pyval reads from the layer row + agg. All per-INFERENCE
  // (pre-reps); reps scales additive metrics at write time (like the Python cell()).
  struct LayerCtx
  {
    std::string op_type;       // compiler op_type (propagated to the row)
    uint64_t M{0}, K{0}, N{0};
    uint32_t reps{1};
    uint32_t X{0}, Y{0};       // array geometry
    uint32_t eb{1};            // element bytes (dtype)
    double freq_ghz{0.8};
    uint64_t tiles{0};         // Mt*Nt per inference (pipeline_bubble uses tiles*reps)
    uint64_t config_bytes{0};  // controller_args bytes (ddr_footprint_control, per inference)
    // MEASURED DRAM footprint (size of the A/B regions in the DRAM image). The
    // formula M*K*eb / K*N*eb is valid only when M/K/N is a real GeMM shape; the core
    // path sets N=1 so that M*K*N == measured MACs, which makes that formula meaningless.
    uint64_t ddr_data_bytes{0}, ddr_weight_bytes{0};
    uint64_t pes{0};           // X*Y
    double ext_gbps{16.0};     // peak external DDR BW (DMA gbps)
    uint64_t rb{0};            // SRAM-A row bytes (for Kfit in l2_footprint); geo['rb']
  };

  // format helpers: integers plain; doubles trimmed (matches Python round()/int()).
  inline std::string fmt_u(uint64_t v) { return std::to_string(v); }
  inline std::string fmt_d(double v, int prec = 3)
  {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", prec, v);
    std::string s(buf);
    // trim trailing zeros / dot to match Python's compact numbers
    if (s.find('.') != std::string::npos)
    {
      size_t last = s.find_last_not_of('0');
      if (s[last] == '.') last--;
      s.erase(last + 1);
    }
    return s;
  }

  // Fill an EvalRow from a measured EvalCounters block (the metrics the core module
  // emits). Derived/analytic metrics (theory_min, bw_gbps, footprints, the 0-by-design
  // set, ...) are added by the I1c derived pass; whatever is absent stays blank.
  inline void eval_row_from_counters(EvalRow &r, const EvalCounters &e)
  {
    auto &v = r.vals;
    // cycles.csv
    v["total_cycles"] = fmt_u(e.total_cycles);
    v["processing_cycles"] = fmt_u(e.processing_cycles);
    v["transfer_cycles"] = fmt_u(e.transfer_cycles);
    v["transfer_overhead_cycles"] = fmt_u(e.transfer_overhead_cycles);
    v["transfer_overhead_percent"] = fmt_d(e.transfer_overhead_percent, 4);
    v["theory_min_cycles"] = fmt_u(e.theory_min_cycles);
    v["active_cycles"] = fmt_u(e.active_cycles);
    v["idle_cycles"] = fmt_u(e.idle_cycles);
    // engine.csv
    v["mac_engine_cycles"] = fmt_u(e.mac_engine_cycles);
    v["dma_engine_cycles"] = fmt_u(e.dma_engine_cycles);
    v["engine_utilization"] = fmt_d(e.engine_utilization, 4);
    // dma.csv
    v["dma_read_cycles"] = fmt_u(e.dma_read_cycles);
    v["dma_write_cycles"] = fmt_u(e.dma_write_cycles);
    v["dma_busy_cycles"] = fmt_u(e.dma_busy_cycles);
    v["dma_idle_cycles"] = fmt_u(e.dma_idle_cycles);
    v["dma_stall_cycles"] = fmt_u(e.dma_stall_cycles);
    v["dma_wait_cycles"] = fmt_u(e.dma_wait_cycles);
    // stall.csv
    v["wait_input_cycles"] = fmt_u(e.wait_input_cycles);
    v["wait_output_cycles"] = fmt_u(e.wait_output_cycles);
    // bandwidth.csv (bytes)
    v["ddr_read_bytes"] = fmt_u(e.ddr_read_bytes);
    v["ddr_write_bytes"] = fmt_u(e.ddr_write_bytes);
    v["weight_bytes"] = fmt_u(e.weight_bytes);
    v["bias_bytes"] = fmt_u(e.bias_bytes);
    v["l1_read_bytes"] = fmt_u(e.l1_read_bytes);
    v["l1_write_bytes"] = fmt_u(e.l1_write_bytes);
    v["l2_read_bytes"] = fmt_u(e.l2_read_bytes);
    v["l2_write_bytes"] = fmt_u(e.l2_write_bytes);
    v["l2_to_l1_bytes"] = fmt_u(e.l2_to_l1_bytes);
    v["l1_to_l2_bytes"] = fmt_u(e.l1_to_l2_bytes);
    // utilization.csv
    v["mac_active_cycles"] = fmt_u(e.mac_active_cycles);
    v["mac_idle_cycles"] = fmt_u(e.mac_idle_cycles);
    v["pe_active_cycles"] = fmt_u(e.pe_active_cycles);
    v["pe_idle_cycles"] = fmt_u(e.pe_idle_cycles);
    v["engine_active_cycles"] = fmt_u(e.engine_active_cycles);
    v["engine_idle_cycles"] = fmt_u(e.engine_idle_cycles);
    // memory.csv
    v["l2_footprint_bytes"] = fmt_u(e.l2_footprint_bytes);
  }

  // I1c: fill a full per-layer row -- measured EvalCounters (reps-scaled, additive) + all
  // the derived/analytic metrics (ports tools/dse_sweep.py::pyval). This makes the C++
  // eval-out column-complete (no Python). Parity gate: tools/eval_csv_parity.py.
  inline void eval_fill_row(EvalRow &r, const EvalCounters &e, const LayerCtx &c)
  {
    r.op_type = c.op_type;
    const uint32_t reps = c.reps ? c.reps : 1;
    auto &v = r.vals;
    auto U = [&](const char *m, uint64_t x, bool additive = true) { v[m] = fmt_u(additive ? x * reps : x); };
    // --- measured (additive -> * reps) ---
    U("total_cycles", e.total_cycles); U("processing_cycles", e.processing_cycles);
    U("transfer_cycles", e.transfer_cycles); U("transfer_overhead_cycles", e.transfer_overhead_cycles);
    U("active_cycles", e.active_cycles); U("idle_cycles", e.idle_cycles);
    U("mac_engine_cycles", e.mac_engine_cycles); U("dma_engine_cycles", e.dma_engine_cycles);
    U("dma_read_cycles", e.dma_read_cycles); U("dma_write_cycles", e.dma_write_cycles);
    U("dma_busy_cycles", e.dma_busy_cycles); U("dma_idle_cycles", e.dma_idle_cycles);
    U("dma_stall_cycles", e.dma_stall_cycles); U("dma_wait_cycles", e.dma_wait_cycles);
    U("wait_input_cycles", e.wait_input_cycles); U("wait_output_cycles", e.wait_output_cycles);
    // utilization (mac/pe/engine active-idle): [M]measured via the bit-exact core tb when the raw
    // counter is populated; else [D]derived from timing here (config-gen path leaves them 0). Per-rep.
    //   mac_active = compute span; mac_idle = total-compute (MAC starved by DMA)
    //   pe_active  = theory_min (useful MAC-work); pe_idle = total-theory_min (under-util + wait)
    //   engine_active = busier engine's span (overlap); engine_idle = total-engine_active
    {
      const uint64_t tot_c = e.total_cycles, proc_c = e.processing_cycles;
      const uint64_t tmin = c.pes ? ((uint64_t)c.M * c.K * c.N + c.pes - 1) / c.pes : 0;
      const uint64_t eng = std::min(tot_c, std::max(proc_c, e.dma_busy_cycles));
      auto UD = [&](const char *m, uint64_t meas, uint64_t der) { v[m] = fmt_u((meas ? meas : der) * reps); };
      UD("mac_active_cycles", e.mac_active_cycles, proc_c);
      UD("mac_idle_cycles",   e.mac_idle_cycles,   tot_c > proc_c ? tot_c - proc_c : 0);
      UD("pe_active_cycles",  e.pe_active_cycles,  tmin);
      UD("pe_idle_cycles",    e.pe_idle_cycles,    tot_c > tmin ? tot_c - tmin : 0);
      UD("engine_active_cycles", e.engine_active_cycles, eng);
      UD("engine_idle_cycles",   e.engine_idle_cycles,   tot_c > eng ? tot_c - eng : 0);
    }
    // --- bytes: measured (EvalCounters, * reps) if present, else shape-derived (pyval B2) ---
    const uint64_t CB = 4;                                   // int32 output element
    const uint64_t Mt = (c.M + c.Y - 1) / c.Y, Nt = (c.N + c.X - 1) / c.X;
    const uint64_t Mp = Mt * c.Y, Np = Nt * c.X, ntl = Mt * Nt;
    const uint64_t d_ddr_r = (c.K * Np + Nt * Mp * c.K) * c.eb * reps;   // weight(keep) + activation
    const uint64_t d_ddr_w = Mp * Np * CB * reps;                       // int32 output store
    const uint64_t d_feed  = c.K * ntl * (c.X + c.Y) * c.eb * reps;     // SRAM->array feed
    const uint64_t d_drain = Mp * Np * CB * reps;                       // array->SRAM drain
    auto BY = [&](const char *m, uint64_t meas, uint64_t derived) { v[m] = fmt_u(meas ? meas * reps : derived); };
    BY("ddr_read_bytes", e.ddr_read_bytes, d_ddr_r); BY("ddr_write_bytes", e.ddr_write_bytes, d_ddr_w);
    BY("weight_bytes", e.weight_bytes, c.K * Np * c.eb * reps);
    BY("l1_read_bytes", e.l1_read_bytes, d_feed); BY("l1_write_bytes", e.l1_write_bytes, d_drain);
    BY("l2_read_bytes", e.l2_read_bytes, d_ddr_r); BY("l2_write_bytes", e.l2_write_bytes, d_ddr_w);
    BY("l2_to_l1_bytes", e.l2_to_l1_bytes, d_feed); BY("l1_to_l2_bytes", e.l1_to_l2_bytes, d_drain);
    // --- ratios (NOT * reps; recompute from e since apply_burst_dma sets cycles not %) ---
    v["transfer_overhead_percent"] = fmt_d(e.total_cycles
        ? 100.0 * (double)e.transfer_overhead_cycles / (double)e.total_cycles : 0.0, 4);
    v["engine_utilization"] = fmt_d(e.total_cycles
        ? 100.0 * (double)e.mac_engine_cycles / (double)e.total_cycles : 0.0, 4);
    // --- sizes / footprints (peak, NOT * reps) ---
    const uint64_t Kf = c.rb ? std::max<uint64_t>(1, c.rb / (c.Y * c.eb)) : c.K;
    const uint64_t Kfit = std::min<uint64_t>(c.K, Kf);
    const uint64_t l2fp = e.l2_footprint_bytes ? e.l2_footprint_bytes
                                               : (c.Y * Kfit + Kfit * c.X) * c.eb + c.Y * c.X * CB;
    v["l2_footprint_bytes"] = fmt_u(l2fp);
    v["max_l2_taken_size_bytes"] = fmt_u(l2fp);
    // --- DERIVED (pyval port) ---
    const double f = c.freq_ghz;
    const uint64_t macs = c.M * c.K * c.N;
    v["theory_min_cycles"] = fmt_u(c.pes ? ((macs + c.pes - 1) / c.pes) * reps : 0);
    U("ddr_wait_cycles", e.dma_stall_cycles);   // share of measured stall
    U("l1_wait_cycles", e.wait_input_cycles);
    U("l2_wait_cycles", e.wait_output_cycles);
    v["sram_conflict_cycles"] = "0"; v["memory_arbitration_cycles"] = "0";   // B3: n/a-by-design
    v["activation_engine_cycles"] = "0"; v["pooling_engine_cycles"] = "0";   // host ops
    v["reshape_engine_cycles"] = "0"; v["reduction_engine_cycles"] = "0";
    v["pipeline_bubble_cycles"] = fmt_u((uint64_t)(c.X + c.Y) * c.tiles * reps);
    v["instruction_stall_cycles"] = fmt_u((c.config_bytes / 4) * reps);
    v["synchronization_cycles"] = "0"; v["dependency_stall_cycles"] = "0";
    v["bias_bytes"] = "0";   // F2: SAURIA has no bias datapath (host op)
    v["ddr_footprint_data_bytes"] = fmt_u(c.ddr_data_bytes ? c.ddr_data_bytes * reps
                                                           : c.M * c.K * c.eb * reps);
    v["ddr_footprint_weight_bytes"] = fmt_u(c.ddr_weight_bytes ? c.ddr_weight_bytes * reps
                                                               : c.K * c.N * c.eb * reps);
    v["ddr_footprint_control_bytes"] = fmt_u(c.config_bytes * reps);
    v["peak_external_bw_gbps"] = fmt_d(c.ext_gbps, 2);
    v["peak_internal_bw_gbps"] = fmt_d((double)(c.X + c.Y) * c.eb * f, 2);
    // bandwidth (per-inference bytes * freq / per-inference latency)
    const double lat = (double)e.total_cycles;
    const double ext_b = e.ddr_read_bytes ? (double)(e.ddr_read_bytes + e.ddr_write_bytes)
                                          : (double)(d_ddr_r + d_ddr_w) / reps;
    const double int_b = e.l2_to_l1_bytes ? (double)(e.l2_to_l1_bytes + e.l1_to_l2_bytes)
                                          : (double)(d_feed + d_drain) / reps;
    v["external_bw_gbps"] = lat ? fmt_d(ext_b * f / lat, 3) : "0";
    v["internal_bw_gbps"] = lat ? fmt_d(int_b * f / lat, 3) : "0";
    v["algorithmic_tops"] = lat ? fmt_d(2.0 * macs / (lat / (f * 1e9)) / 1e12, 3) : "0";
    // ips / ips_bw_limited: network-only in pyval -> blank at layer level (leave unset).
  }

  // I1d: network aggregate row (ports tools/dse_sweep.py::netcell). Additive metrics sum
  // (reps-scaled) across layers; ratios/bw recompute from the sums; footprints take the max;
  // throughput (ips/tops) from network latency. Appended as the 'network' row per CSV.
  inline void eval_network_row(EvalRow &net, const std::vector<const EvalCounters *> &es,
                               const std::vector<const LayerCtx *> &cs)
  {
    net.layer_id = "network"; net.layer_name = "NETWORK"; net.op_type = "scope=network";
    const size_t n = es.size();
    if (n == 0) return;
    auto Sr = [&](uint64_t EvalCounters::*f) {   // sum of a field * reps
      uint64_t s = 0; for (size_t i = 0; i < n; i++) s += es[i]->*f * cs[i]->reps; return s; };
    const double f = cs[0]->freq_ghz;
    // --- additive cycle counters ---
    const uint64_t tot = Sr(&EvalCounters::total_cycles), proc = Sr(&EvalCounters::processing_cycles);
    auto &v = net.vals;
    v["total_cycles"] = fmt_u(tot); v["processing_cycles"] = fmt_u(proc);
    v["transfer_cycles"] = fmt_u(Sr(&EvalCounters::transfer_cycles));
    const uint64_t ovh = Sr(&EvalCounters::transfer_overhead_cycles);
    v["transfer_overhead_cycles"] = fmt_u(ovh);
    v["active_cycles"] = fmt_u(Sr(&EvalCounters::active_cycles));
    v["idle_cycles"] = fmt_u(Sr(&EvalCounters::idle_cycles));
    const uint64_t mace = Sr(&EvalCounters::mac_engine_cycles);
    v["mac_engine_cycles"] = fmt_u(mace); v["dma_engine_cycles"] = fmt_u(Sr(&EvalCounters::dma_engine_cycles));
    v["dma_read_cycles"] = fmt_u(Sr(&EvalCounters::dma_read_cycles));
    v["dma_write_cycles"] = fmt_u(Sr(&EvalCounters::dma_write_cycles));
    v["dma_busy_cycles"] = fmt_u(Sr(&EvalCounters::dma_busy_cycles));
    v["dma_idle_cycles"] = fmt_u(Sr(&EvalCounters::dma_idle_cycles));
    const uint64_t stall = Sr(&EvalCounters::dma_stall_cycles);
    v["dma_stall_cycles"] = fmt_u(stall); v["dma_wait_cycles"] = fmt_u(Sr(&EvalCounters::dma_wait_cycles));
    v["wait_input_cycles"] = fmt_u(Sr(&EvalCounters::wait_input_cycles));
    v["wait_output_cycles"] = fmt_u(Sr(&EvalCounters::wait_output_cycles));
    // utilization (network): measured-sum if populated, else derived-from-sums (mirror per-layer).
    {
      uint64_t tmin = 0;
      for (size_t i = 0; i < n; i++) { const auto *cc = cs[i];
        tmin += (cc->pes ? ((uint64_t)cc->M * cc->K * cc->N + cc->pes - 1) / cc->pes : 0) * cc->reps; }
      const uint64_t dbz = Sr(&EvalCounters::dma_busy_cycles);
      const uint64_t eng = std::min(tot, std::max(proc, dbz));
      auto ND = [&](const char *m, uint64_t meas, uint64_t der) { v[m] = fmt_u(meas ? meas : der); };
      ND("mac_active_cycles", Sr(&EvalCounters::mac_active_cycles), proc);
      ND("mac_idle_cycles",   Sr(&EvalCounters::mac_idle_cycles),   tot > proc ? tot - proc : 0);
      ND("pe_active_cycles",  Sr(&EvalCounters::pe_active_cycles),  tmin);
      ND("pe_idle_cycles",    Sr(&EvalCounters::pe_idle_cycles),    tot > tmin ? tot - tmin : 0);
      ND("engine_active_cycles", Sr(&EvalCounters::engine_active_cycles), eng);
      ND("engine_idle_cycles",   Sr(&EvalCounters::engine_idle_cycles),   tot > eng ? tot - eng : 0);
    }
    // --- bytes (sum measured-or-derived via per-layer row) + derived ---
    // per-layer rows already hold the effective bytes; sum by re-parsing is avoided by
    // recomputing here from the shape sum (matches netcell effS on the derived path).
    double ext_b = 0, int_b = 0, sum_macs = 0, sum_dram = 0;
    uint64_t l2fp_max = 0;
    auto U64 = [&](const std::string &s) -> uint64_t { return s.empty() ? 0ull : std::stoull(s); };
    for (size_t i = 0; i < n; i++)
    {
      const auto &c = *cs[i]; const auto &e = *es[i]; const uint32_t reps = c.reps ? c.reps : 1;
      const uint64_t CB = 4, Mt = (c.M + c.Y - 1) / c.Y, Nt = (c.N + c.X - 1) / c.X;
      const uint64_t Mp = Mt * c.Y, Np = Nt * c.X, ntl = Mt * Nt;
      const uint64_t d_rd = (c.K * Np + Nt * Mp * c.K) * c.eb, d_wr = Mp * Np * CB;
      const uint64_t d_fd = c.K * ntl * (c.X + c.Y) * c.eb, d_dr = Mp * Np * CB;
      ext_b += (double)((e.ddr_read_bytes ? e.ddr_read_bytes : d_rd) + (e.ddr_write_bytes ? e.ddr_write_bytes : d_wr)) * reps;
      int_b += (double)((e.l2_to_l1_bytes ? e.l2_to_l1_bytes : d_fd) + (e.l1_to_l2_bytes ? e.l1_to_l2_bytes : d_dr)) * reps;
      sum_macs += (double)c.M * c.K * c.N * reps;
      sum_dram += (double)((e.ddr_read_bytes ? e.ddr_read_bytes : d_rd) + (e.ddr_write_bytes ? e.ddr_write_bytes : d_wr)) * reps;
      const uint64_t Kf = c.rb ? std::max<uint64_t>(1, c.rb / (c.Y * c.eb)) : c.K;
      const uint64_t Kfit = std::min<uint64_t>(c.K, Kf);
      const uint64_t l2fp = e.l2_footprint_bytes ? e.l2_footprint_bytes
                                                 : (c.Y * Kfit + Kfit * c.X) * c.eb + c.Y * c.X * CB;
      if (l2fp > l2fp_max) l2fp_max = l2fp;
    }
    // sum the byte columns straight from the per-layer rows (already effective+reps-scaled)
    // -- accumulate below when writing; here fill the aggregate-only derived:
    v["l2_footprint_bytes"] = fmt_u(l2fp_max); v["max_l2_taken_size_bytes"] = fmt_u(l2fp_max);
    v["transfer_overhead_percent"] = fmt_d(tot ? 100.0 * ovh / tot : 0.0, 4);
    v["engine_utilization"] = fmt_d(tot ? 100.0 * mace / tot : 0.0, 4);
    v["theory_min_cycles"] = fmt_u([&] { uint64_t s = 0; for (size_t i = 0; i < n; i++) {
      uint64_t mk = cs[i]->M * cs[i]->K * cs[i]->N; s += (cs[i]->pes ? (mk + cs[i]->pes - 1) / cs[i]->pes : 0) * cs[i]->reps; } return s; }());
    v["peak_external_bw_gbps"] = fmt_d(cs[0]->ext_gbps, 2);
    v["peak_internal_bw_gbps"] = fmt_d((double)(cs[0]->X + cs[0]->Y) * cs[0]->eb * f, 2);
    v["external_bw_gbps"] = tot ? fmt_d(ext_b * f / tot, 3) : "0";
    v["internal_bw_gbps"] = tot ? fmt_d(int_b * f / tot, 3) : "0";
    v["algorithmic_tops"] = tot ? fmt_d(2.0 * sum_macs / (tot / (f * 1e9)) / 1e12, 3) : "0";
    v["ips"] = tot ? fmt_d(f * 1e9 / tot, 2) : "0";
    v["ips_bw_limited"] = sum_dram ? fmt_d(cs[0]->ext_gbps * 1e9 / sum_dram, 2) : "0";
    // additive derived + byte columns + 0-metrics: sum the per-layer effective values.
    // (the caller passes the per-layer EvalRows so we can sum their string cells)
    (void)U64;   // (byte/derived additive sums are filled by eval_network_finish below)
  }

  // Sum the additive byte/derived columns straight from the per-layer rows (they already
  // hold effective + reps-scaled values). Call after eval_network_row with the per-layer rows.
  inline void eval_network_finish(EvalRow &net, const std::vector<EvalRow> &layer_rows)
  {
    const char *ADD[] = {"ddr_read_bytes","ddr_write_bytes","weight_bytes","l1_read_bytes",
      "l1_write_bytes","l2_read_bytes","l2_write_bytes","l2_to_l1_bytes","l1_to_l2_bytes",
      "ddr_wait_cycles","l1_wait_cycles","l2_wait_cycles","pipeline_bubble_cycles",
      "instruction_stall_cycles","ddr_footprint_data_bytes","ddr_footprint_weight_bytes",
      "ddr_footprint_control_bytes"};
    for (auto m : ADD)
    {
      unsigned long long s = 0;
      for (const auto &r : layer_rows) { auto it = r.vals.find(m); if (it != r.vals.end() && !it->second.empty()) s += std::stoull(it->second); }
      net.vals[m] = fmt_u(s);
    }
    const char *ZERO[] = {"bias_bytes","sram_conflict_cycles","memory_arbitration_cycles",
      "activation_engine_cycles","pooling_engine_cycles","reshape_engine_cycles",
      "reduction_engine_cycles","synchronization_cycles","dependency_stall_cycles"};
    for (auto m : ZERO) net.vals[m] = "0";
  }


  // -- parameters.json accompanies the CSV set: same run, same directory.
  // Top-level keys keep the contract with the EVAL application (example:
  // results/eval_integration_out/parameters.json).
  struct EvalParams
  {
    std::string model_name{"sauria_core"};
    uint32_t X{32}, Y{32};
    double freq_ghz{0.8};
    std::string precision{"int8"};
    uint64_t l1_size_bytes{0}, l2_size_bytes{0};
    double peak_ext_gbps{16.0}, peak_int_gbps{0.0}, hw_int_gbps{0.0};
    double dma_ddr_gbps{16.0};
    uint32_t dma_setup{20};
    bool dma_dbuf{true};
    double dma_int_bpc{64.0};
    std::string provenance{"FX1 unified SystemC VP (core that, tb_evaluate)"};
    std::string build_config{""};
    std::string stimulus{""};
  };

  // Provenance label of each metric. Six metrics are measured rather than derived:
  //   active_cycles, pe_active_cycles  <- real counters (exec/[STAGE])
  //   l1_read_bytes, l1_write_bytes    <- measured at the SRAM (sram_top.h)
  //   l2_read_bytes, l2_write_bytes    <- DDR<->SRAM bytes counted per tile
  // The remaining "derived" metrics are COMPUTED FROM MEASURED NUMBERS (complements,
  // ratios, theoretical bounds) -- not guesses.
  inline const char *eval_metric_provenance(const std::string &m)
  {
    // ==== GENERATED by tools/gen_provenance.py -- DO NOT EDIT BY HAND ====
    // Source: tools/eval_schema.csv, column `provenance` (NOT column `status`).
    static const std::map<std::string, const char *> P = {
        {"activation_engine_cycles","host"},
        {"active_cycles","measured"},
        {"algorithmic_tops","derived"},
        {"bias_bytes","host"},
        {"ddr_footprint_control_bytes","derived"},
        {"ddr_footprint_data_bytes","derived"},
        {"ddr_footprint_weight_bytes","derived"},
        {"ddr_read_bytes","measured"},
        {"ddr_wait_cycles","analytic"},
        {"ddr_write_bytes","measured"},
        {"dependency_stall_cycles","not-modeled"},
        {"dma_busy_cycles","analytic"},
        {"dma_engine_cycles","analytic"},
        {"dma_idle_cycles","derived"},
        {"dma_read_cycles","analytic"},
        {"dma_stall_cycles","analytic"},
        {"dma_wait_cycles","analytic"},
        {"dma_write_cycles","analytic"},
        {"engine_active_cycles","derived"},
        {"engine_idle_cycles","derived"},
        {"engine_utilization","derived"},
        {"external_bw_gbps","derived"},
        {"idle_cycles","derived"},
        {"instruction_stall_cycles","analytic"},
        {"internal_bw_gbps","derived"},
        {"ips","derived"},
        {"ips_bw_limited","derived"},
        {"l1_read_bytes","derived"},
        {"l1_to_l2_bytes","measured"},
        {"l1_wait_cycles","analytic"},
        {"l1_write_bytes","derived"},
        {"l2_footprint_bytes","derived"},
        {"l2_read_bytes","derived"},
        {"l2_to_l1_bytes","measured"},
        {"l2_wait_cycles","analytic"},
        {"l2_write_bytes","derived"},
        {"mac_active_cycles","derived"},
        {"mac_engine_cycles","derived"},
        {"mac_idle_cycles","derived"},
        {"max_l2_taken_size_bytes","derived"},
        {"memory_arbitration_cycles","n/a-by-design"},
        {"pe_active_cycles","derived"},
        {"pe_idle_cycles","derived"},
        {"peak_external_bw_gbps","host"},
        {"peak_internal_bw_gbps","derived"},
        {"pipeline_bubble_cycles","analytic"},
        {"pooling_engine_cycles","host"},
        {"processing_cycles","measured"},
        {"reduction_engine_cycles","host"},
        {"reshape_engine_cycles","host"},
        {"sram_conflict_cycles","n/a-by-design"},
        {"synchronization_cycles","not-modeled"},
        {"theory_min_cycles","derived"},
        {"total_cycles","derived"},
        {"transfer_cycles","analytic"},
        {"transfer_overhead_cycles","analytic"},
        {"transfer_overhead_percent","derived"},
        {"wait_input_cycles","analytic"},
        {"wait_output_cycles","analytic"},
        {"weight_bytes","measured"},
    };
    // ==== end of generated section ====
    auto it = P.find(m);
    return it != P.end() ? it->second : "derived";
  }

  inline void write_parameters_json(const EvalParams &p, const EvalSchema &schema,
                                    const std::string &outdir)
  {
    std::ofstream f(outdir + "/parameters.json");
    if (!f) return;
    const uint64_t pes = (uint64_t)p.X * p.Y;
    f << "{\n";
    f << "  \"model_name\": \"" << p.model_name << "\",\n";
    f << "  \"geometry\": \"" << p.X << "x" << p.Y << "\",\n";
    f << "  \"mac_count\": " << pes << ",\n";
    f << "  \"pe_count\": " << pes << ",\n";
    f << "  \"frequency_hz\": " << fmt_d(p.freq_ghz * 1e9, 1) << ",\n";
    f << "  \"ops_per_mac\": 2,\n";
    f << "  \"supported_precision\": [\n    \"" << p.precision << "\"\n  ],\n";
    f << "  \"supported_operators\": [\n    \"GeMM\",\n    \"Conv2D\"\n  ],\n";
    f << "  \"l1_size_bytes\": " << p.l1_size_bytes << ",\n";
    f << "  \"l2_size_bytes\": " << p.l2_size_bytes << ",\n";
    f << "  \"peak_external_bw_gbps\": " << fmt_d(p.peak_ext_gbps, 2) << ",\n";
    f << "  \"peak_internal_bw_gbps\": " << fmt_d(p.peak_int_gbps, 2) << ",\n";
    f << "  \"hw_internal_bw_gbps\": " << fmt_d(p.hw_int_gbps, 2) << ",\n";
    f << "  \"dma_timing_model\": {\n";
    f << "    \"ddr_gbps\": " << fmt_d(p.dma_ddr_gbps, 2) << ",\n";
    f << "    \"setup_cycles\": " << p.dma_setup << ",\n";
    f << "    \"double_buffer\": " << (p.dma_dbuf ? "true" : "false") << ",\n";
    f << "    \"int_bytes_per_cycle\": " << fmt_d(p.dma_int_bpc, 2) << ",\n";
    f << "    \"note\": \"compute cycles = measured SystemC core (per-state [STAGE] busy span, "
         "not pipeline-enabled beats); transfer cycles = analytic model over measured byte counts. "
         "External DDR and internal L1<->L2 BW/setup remain PLACEHOLDERS until the HW team confirms.\"\n";
    f << "  },\n";
    f << "  \"provenance\": \"" << p.provenance << "\",\n";
    if (!p.build_config.empty())
      f << "  \"build_config\": \"" << p.build_config << "\",\n";
    if (!p.stimulus.empty())
      f << "  \"stimulus\": \"" << p.stimulus << "\",\n";
    f << "  \"metric_provenance\": {\n";
    bool first = true;
    for (const auto &csvfile : schema.files)
      for (const auto &m : schema.metrics.at(csvfile))
      {
        if (!first) f << ",\n";
        f << "    \"" << m << "\": \"" << eval_metric_provenance(m) << "\"";
        first = false;
      }
    f << "\n  }\n}\n";
  }

  // Write one CSV per csv_file: header "layer_id,layer_name,op_type,<metrics...>", 1 row/layer.
  // Blank cell when a row has no value for that metric (matches the Python hybrid-blank).
  inline void write_eval_dir(const EvalSchema &schema, const std::vector<EvalRow> &rows,
                             const std::string &outdir)
  {
    for (const auto &csvfile : schema.files)
    {
      const auto &mets = schema.metrics.at(csvfile);
      std::ofstream fp(outdir + "/" + csvfile);
      fp << "layer_id,layer_name,op_type";
      for (const auto &m : mets) fp << "," << m;
      fp << "\n";
      for (const auto &r : rows)
      {
        fp << r.layer_id << "," << r.layer_name << "," << r.op_type;
        for (const auto &m : mets)
        {
          auto it = r.vals.find(m);
          fp << "," << (it != r.vals.end() ? it->second : std::string());
        }
        fp << "\n";
      }
    }
  }
} // namespace sauria_rtl

#endif // SAURIA_RTL_EVAL_WRITER_H
