// sw_eval.cpp -- standalone SW-EVAL (MODE B): writes the 9 CSVs + parameters.json from the SHAPE table. No SystemC, no testbench.
//
// Includes only the two arithmetic headers rtl_ref_eval_counters.h / rtl_ref_eval_writer.h (no SystemC), so the verbatim-ported
// writer is reused and no formula is rewritten. The glue (filling EvalCounters / LayerCtx / EvalParams) lives here.
//
// Basis: tile cycles depend only on the tile SHAPE (measuring two tiles of the same shape gives identical busy/exec; only mac_nz
// changes). Every number is therefore taken from `shape_table.csv` (one row per shape, measured with sauria_model) and multiplied
// by the tile count from `layer_tiles.csv`. Every generated number carries the MODE B label in `metric_provenance`
// (parameters.json) -- never a measurement.
//
//   sw_eval --shapes shape_table.csv --tiles layer_tiles.csv --schema eval_schema.csv --out DIR [--compat]
//   --compat : reproduce sauria_model's per-job output verbatim (op_type=GeMM, measured pe_active, labels unchanged) for a
//              cell-by-cell cross-check.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "instrumentation/rtl_ref_eval_writer.h"

using namespace sauria_rtl;

static std::vector<std::string> split(const std::string &s, char d = ',')
{
  std::vector<std::string> o; std::string t; std::stringstream ss(s);
  while (std::getline(ss, t, d)) o.push_back(t);
  if (!s.empty() && s.back() == d) o.push_back("");
  return o;
}
static std::string trimcr(std::string s) { while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back(); return s; }

struct Csv
{
  std::vector<std::string> hdr; std::vector<std::vector<std::string>> rows; std::map<std::string, size_t> col;
  bool load(const std::string &p)
  {
    std::ifstream f(p); if (!f) return false;
    std::string l; if (!std::getline(f, l)) return false;
    hdr = split(trimcr(l));
    for (size_t i = 0; i < hdr.size(); i++) col[hdr[i]] = i;
    while (std::getline(f, l)) { l = trimcr(l); if (l.empty()) continue; rows.push_back(split(l)); }
    return true;
  }
  const std::string &at(size_t r, const char *c) const
  {
    auto it = col.find(c);
    if (it == col.end()) { fprintf(stderr, "[sw_eval] missing column '%s'\n", c); exit(2); }
    return rows[r][it->second];
  }
  uint64_t u(size_t r, const char *c) const { return std::strtoull(at(r, c).c_str(), nullptr, 10); }
};

// [EVAL] column name -> EvalCounters member (summed per tile). l2_footprint takes the MAX, not the sum.
struct Fld { const char *name; uint64_t EvalCounters::*p; };
static const Fld FLD[] = {
  {"total_cycles", &EvalCounters::total_cycles}, {"processing_cycles", &EvalCounters::processing_cycles},
  {"core_exec_cycles", &EvalCounters::core_exec_cycles}, {"transfer_cycles", &EvalCounters::transfer_cycles},
  {"transfer_overhead_cycles", &EvalCounters::transfer_overhead_cycles},
  {"internal_transfer_cycles", &EvalCounters::internal_transfer_cycles}, {"mac_engine_cycles", &EvalCounters::mac_engine_cycles},
  {"dma_engine_cycles", &EvalCounters::dma_engine_cycles}, {"dma_stall_cycles", &EvalCounters::dma_stall_cycles},
  {"wait_input_cycles", &EvalCounters::wait_input_cycles}, {"wait_output_cycles", &EvalCounters::wait_output_cycles},
  {"ddr_read_bytes", &EvalCounters::ddr_read_bytes}, {"ddr_write_bytes", &EvalCounters::ddr_write_bytes},
  {"weight_bytes", &EvalCounters::weight_bytes}, {"l2_to_l1_bytes", &EvalCounters::l2_to_l1_bytes},
  {"l1_to_l2_bytes", &EvalCounters::l1_to_l2_bytes}, {"theory_min_cycles", &EvalCounters::theory_min_cycles},
  {"active_cycles", &EvalCounters::active_cycles}, {"idle_cycles", &EvalCounters::idle_cycles},
  {"dma_read_cycles", &EvalCounters::dma_read_cycles}, {"dma_write_cycles", &EvalCounters::dma_write_cycles},
  {"dma_busy_cycles", &EvalCounters::dma_busy_cycles}, {"dma_idle_cycles", &EvalCounters::dma_idle_cycles},
  {"dma_wait_cycles", &EvalCounters::dma_wait_cycles}, {"mac_active_cycles", &EvalCounters::mac_active_cycles},
  {"mac_idle_cycles", &EvalCounters::mac_idle_cycles}, {"pe_active_cycles", &EvalCounters::pe_active_cycles},
  {"pe_idle_cycles", &EvalCounters::pe_idle_cycles}, {"engine_active_cycles", &EvalCounters::engine_active_cycles},
  {"engine_idle_cycles", &EvalCounters::engine_idle_cycles}, {"bias_bytes", &EvalCounters::bias_bytes},
  {"l1_read_bytes", &EvalCounters::l1_read_bytes}, {"l1_write_bytes", &EvalCounters::l1_write_bytes},
  {"l2_read_bytes", &EvalCounters::l2_read_bytes}, {"l2_write_bytes", &EvalCounters::l2_write_bytes},
  {"ddr_footprint_control_bytes", &EvalCounters::config_bytes},
};

// ---- MODE B labels: source (provenance column of eval_schema.csv) + known limits of this path ----
static const char *UPPER[] = {"total_cycles", "transfer_overhead_cycles", "transfer_overhead_percent", "idle_cycles", "dma_stall_cycles",
  "dma_wait_cycles", "wait_input_cycles", "wait_output_cycles", "ddr_wait_cycles", "l1_wait_cycles", "l2_wait_cycles",
  "mac_idle_cycles", "pe_idle_cycles", "engine_idle_cycles", "dma_idle_cycles", nullptr};
static const char *LOWER[] = {"engine_utilization", "ips", "algorithmic_tops", "external_bw_gbps", "internal_bw_gbps", nullptr};
static const char *FOOT[] = {"ddr_footprint_data_bytes", "ddr_footprint_weight_bytes", nullptr};
static const char *CORE[] = {"total_cycles", "ips", "ips_bw_limited", "algorithmic_tops", "engine_utilization", nullptr};
static const char *WIN[] = {"l2_to_l1_bytes", "l1_to_l2_bytes", "l1_read_bytes", "l1_write_bytes", nullptr};
static bool in(const char *const *L, const std::string &m) { for (; *L; ++L) if (m == *L) return true; return false; }

// columns that depend on the REAL run (busy/exec/SRAM bytes) -- found by perturbation (sensitivity.json): 8 direct + 13 derived
static const char *MEAS_DIRECT[] = {"processing_cycles", "active_cycles", "mac_engine_cycles", "mac_active_cycles", "l2_to_l1_bytes", "l1_to_l2_bytes",
  "l1_read_bytes", "l1_write_bytes", nullptr};
static const char *MEAS_DERIVED[] = {"total_cycles", "transfer_overhead_percent", "idle_cycles", "engine_utilization", "dma_idle_cycles", "mac_idle_cycles",
  "pe_idle_cycles", "engine_active_cycles", "engine_idle_cycles", "internal_bw_gbps", "external_bw_gbps", "ips", "algorithmic_tops", nullptr};
static double g_meas_pct = 0.0;   // % of tiles measured through the core port (0 = pure MODE B)
static bool g_footprint_est = false;
static std::string label_modeb(const std::string &m, const char *orig)
{
  std::string s = std::string(orig) + "|MODE-B";
  if (g_meas_pct > 0.0 && (in(MEAS_DIRECT, m) || in(MEAS_DERIVED, m)))
  {
    char b[200];
    if (in(MEAS_DIRECT, m))
      snprintf(b, sizeof(b), "%s|MODE-A%s(%.2f%% of tiles measured through the core port, remainder MODE-B from the shape table)", orig, g_meas_pct >= 99.995 ? "" : "+B", g_meas_pct);
    else
      snprintf(b, sizeof(b), "%s|MODE-A+B(derived: busy/exec measured on %.2f%% of tiles through the core port, DMA/aggregate still MODE-B)", orig, g_meas_pct);
    s = b;
  }
  if (in(UPPER, m)) s += "|UPPER-BOUND(n=1: DMA summed sequentially, no overlap between tiles)";
  if (in(LOWER, m)) s += "|LOWER-BOUND(derived from total_cycles being an upper bound)";
  if (in(FOOT, m)) s += g_footprint_est ? "|ESTIMATE(input=cin*Hmax*Wmax from the tile plan, weight=cout*K; trailing padding may be over-counted)"
                                          : "|UPPER-BOUND(sum of per-tile footprints; halo/reuse across tiles double-counted)";
  if (in(CORE, m)) s += "|CORE-ONLY(83 conv layers; excludes OBP, OBP config load, host ops, inter-layer DMA)";
  if (in(WIN, m)) s += "|UNVERIFIED-WINDOW(SRAM bytes in sauria_model's per-tile window; the port counts per tile call, 2.3% off: not yet validated)";
  if (m == "ddr_read_bytes" || m == "ddr_write_bytes" || m == "weight_bytes")
    s += "|NOTE(original label is 'measured' but the value is computed from the tile plan/DMA description -- tb_evaluate.cpp:3333-3334,3429 -- not a hardware counter)";
  if (m == "pe_active_cycles" ) s += "|DEF(useful-work = theoretical MACs/1024, project definition)";
  if (m == "mac_active_cycles") s += "|DEF(PE array exec cycles)";
  if (m == "instruction_stall_cycles" || m == "ddr_footprint_control_bytes") s += "|ASSUME(1 KB of config load per tile)";
  return s;
}

struct Layer { int id; std::string name; std::map<int, uint64_t> shapes; std::vector<int> seq; std::vector<long long> key; };

int main(int argc, char **argv)
{
  std::string shapes_p, tiles_p, schema_p, out, layers_p, meas_p;
  bool compat = false;
  uint64_t A_bytes = 161792, B_bytes = 165888, C_cells = 1536, C_cell_bytes = 128;   // tb_fe_core_net.cpp:57,58,64 (C = O, 1 O = 128 B)
  for (int i = 1; i < argc; i++)
  {
    std::string a = argv[i];
    auto nx = [&]() { if (i + 1 >= argc) { fprintf(stderr, "missing value after %s\n", a.c_str()); exit(2); } return std::string(argv[++i]); };
    if (a == "--shapes") shapes_p = nx(); else if (a == "--tiles") tiles_p = nx(); else if (a == "--schema") schema_p = nx();
    else if (a == "--out") out = nx(); else if (a == "--layers") layers_p = nx(); else if (a == "--measured") meas_p = nx(); else if (a == "--compat") compat = true;
    else if (a == "--sram-a-bytes") A_bytes = std::strtoull(nx().c_str(), nullptr, 10);
    else if (a == "--sram-b-bytes") B_bytes = std::strtoull(nx().c_str(), nullptr, 10);
    else if (a == "--sram-c-cells") C_cells = std::strtoull(nx().c_str(), nullptr, 10);
    else { fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
  }
  if (shapes_p.empty() || tiles_p.empty() || schema_p.empty() || out.empty())
  { fprintf(stderr, "usage: sw_eval --shapes shape_table.csv --tiles layer_tiles.csv --schema eval_schema.csv --out DIR [--compat]\n"); return 2; }

  Csv S, T, LT; bool have_lt = false;
  if (!S.load(shapes_p) || !T.load(tiles_p)) { fprintf(stderr, "could not read input files\n"); return 2; }
  if (!layers_p.empty()) { if (!LT.load(layers_p)) { fprintf(stderr, "could not read --layers\n"); return 2; } have_lt = true; }
  std::map<int, size_t> ltrow;
  for (size_t r = 0; have_lt && r < LT.rows.size(); r++) ltrow[(int)LT.u(r, "layer_id")] = r;
  EvalSchema es = EvalSchema::load(schema_p);
  if (es.files.empty()) return 2;
  {  // label source check: the C++ table (generated by gen_provenance) must MATCH the `provenance` column of this schema file
    Csv sc; sc.load(schema_p); int bad = 0;
    for (size_t r = 0; r < sc.rows.size(); r++)
      if (sc.at(r, "provenance") != eval_metric_provenance(sc.at(r, "metric")))
      { fprintf(stderr, "[sw_eval] label mismatch: %s schema=%s C++ table=%s\n", sc.at(r, "metric").c_str(), sc.at(r, "provenance").c_str(), eval_metric_provenance(sc.at(r, "metric"))); bad++; }
    if (bad) return 3;
  }

  // ---- shape table ----
  std::map<int, size_t> srow;   // shape_id -> row
  for (size_t r = 0; r < S.rows.size(); r++) srow[(int)S.u(r, "shape_id")] = r;

  // ---- lop ----
  std::vector<Layer> L; std::map<int, size_t> lidx; uint64_t dropped_tiles = 0;
  std::vector<std::pair<std::string, uint64_t>> dropped;
  for (size_t r = 0; r < T.rows.size(); r++)
  {
    int lid = (int)T.u(r, "layer_id"); int sid = std::atoi(T.at(r, "shape_id").c_str());
    if (!lidx.count(lid)) { lidx[lid] = L.size(); Layer l; l.id = lid; l.name = T.at(r, "layer_name"); L.push_back(l); }
    Layer &l = L[lidx[lid]];
    if (sid < 0 || !srow.count(sid)) { dropped_tiles++; continue; }
    l.shapes[sid]++; l.seq.push_back(sid);
    l.key.push_back(T.col.count("step") ? ((long long)T.u(r, "step") << 32) | (long long)T.u(r, "tile") : -1);
  }


  // ---- --measured metrics_tiles.csv (MEASURED through the core port, MODE A). Overrides busy/exec/SRAM bytes per tile; the rest stays MODE B ----
  struct Meas { uint64_t busy, exec, sram_rd, sram_wr; };
  std::map<long long, Meas> M;
  uint64_t meas_bad = 0, meas_dup = 0, meas_notok = 0, meas_rows = 0; std::string meas_meta;
  if (!meas_p.empty())
  {
    std::ifstream mf(meas_p); if (!mf) { fprintf(stderr, "could not read --measured\n"); return 2; }
    std::string ln; std::vector<std::string> hdr; std::map<std::string, size_t> hc;
    while (std::getline(mf, ln))
    {
      ln = trimcr(ln); if (ln.empty()) continue;
      if (ln[0] == '#') { meas_meta = ln; continue; }
      if (hdr.empty()) { hdr = split(ln); for (size_t i = 0; i < hdr.size(); i++) hc[hdr[i]] = i; continue; }
      std::vector<std::string> f = split(ln);
      if (f.size() != hdr.size()) { meas_bad++; continue; }       // truncated line (run interrupted)
      auto U = [&](const char *k) { return std::strtoull(f[hc.at(k)].c_str(), nullptr, 10); };
      meas_rows++;
      if (U("ok") != 1) { meas_notok++; continue; }
      const long long key = ((long long)U("step") << 32) | (long long)U("tile");
      if (M.count(key)) meas_dup++;
      M[key] = Meas{U("busy"), U("exec"), U("a_rd_bytes") + U("b_rd_bytes"), U("c_wr_bytes")};
    }
    uint64_t bs = 0; for (auto &kv : M) bs += kv.second.busy;
    if (M.empty() || bs == 0)
    { fprintf(stderr, "!! --measured: no valid tile, or busy = 0 for ALL tiles (FSM histogram disabled: apply rtl_ref_defaults_perf.patch). ABORTING.\n"); return 2; }
  }
  std::vector<std::vector<uint64_t>> cov;   // [layer] = {tiles, measured, d_busy(+), d_busy(-), d_exec(+), d_exec(-)}
  const DmaTimingParams dma = DmaTimingParams::from_env();
  const uint64_t PES = 32 * 32;
  std::vector<EvalCounters> ecs; std::vector<LayerCtx> lcs; std::vector<EvalRow> rows; std::vector<std::string> names;
  std::vector<EvalCounters> ovs; std::vector<uint64_t> ntiles;
  ecs.reserve(L.size()); lcs.reserve(L.size()); rows.reserve(L.size() + 1); ovs.reserve(L.size());
  std::vector<std::string> omitted;
  uint64_t tiles_total = 0; bool footprint_est = false;

  for (const Layer &l : L)
  {
    if (l.seq.empty()) { omitted.push_back(l.name); continue; }
    EvalCounters e; LayerCtx c;
    uint64_t out_tot = 0, macs = 0, ctx = 0, data_b = 0, wei_b = 0; uint64_t kk = 0; bool k_same = true;
    for (const auto &kv : l.shapes)
    {
      const size_t r = srow[kv.first]; const uint64_t n = kv.second;
      for (const Fld &f : FLD) e.*f.p += n * S.u(r, f.name);
      e.l2_footprint_bytes = std::max<uint64_t>(e.l2_footprint_bytes, S.u(r, "l2_footprint_bytes"));
      const uint64_t k = S.u(r, "mvm_k"); if (kk && kk != k) k_same = false; kk = k;
      out_tot += n * S.u(r, "out_elems"); macs += n * S.u(r, "out_elems") * k; ctx += n * S.u(r, "ncontexts");
      data_b += n * S.u(r, "A_bytes"); wei_b += n * S.u(r, "B_bytes");
    }
    // ---- per-tile override with MEASURED numbers (MODE A): add the DIFFERENCE from the shape table to the fields that depend on busy/exec ----
    uint64_t n_meas = 0, dbp = 0, dbn = 0, dxp = 0, dxn = 0;
    if (!meas_p.empty() && !compat)
    {
      long long d_busy = 0, d_exec = 0, d_rd = 0, d_wr = 0;
      for (size_t i = 0; i < l.seq.size(); i++)
      {
        auto it = M.find(l.key[i]); if (it == M.end()) continue;
        const size_t r = srow[l.seq[i]]; n_meas++;
        const long long db = (long long)it->second.busy - (long long)S.u(r, "processing_cycles");
        const long long dx = (long long)it->second.exec - (long long)S.u(r, "core_exec_cycles");
        d_busy += db; d_exec += dx;
        (db >= 0 ? dbp : dbn) += (uint64_t)(db >= 0 ? db : -db); (dx >= 0 ? dxp : dxn) += (uint64_t)(dx >= 0 ? dx : -dx);
        d_rd += (long long)it->second.sram_rd - (long long)S.u(r, "l2_to_l1_bytes");
        d_wr += (long long)it->second.sram_wr - (long long)S.u(r, "l1_to_l2_bytes");
      }
      auto add = [](uint64_t &f, long long d) { f = (uint64_t)((long long)f + d); };
      add(e.total_cycles, d_busy); add(e.processing_cycles, d_busy); add(e.mac_engine_cycles, d_busy);
      add(e.core_exec_cycles, d_exec); add(e.active_cycles, d_exec); add(e.mac_active_cycles, d_exec);
      add(e.idle_cycles, d_busy - d_exec); add(e.mac_idle_cycles, d_busy - d_exec);
      add(e.dma_idle_cycles, d_busy);                       // dma_idle = total - transfer, transfer unchanged
      add(e.l2_to_l1_bytes, d_rd); add(e.l1_read_bytes, d_rd);
      add(e.l1_to_l2_bytes, d_wr); add(e.l1_write_bytes, d_wr);
    }
    cov.push_back({(uint64_t)l.seq.size(), n_meas, dbp, dbn, dxp, dxn});
    // DMA with the ideal overlap schedule (only for the LOWER bound of total; the main number is the sequential sum = UPPER bound)
    EvalCounters ov;
    {
      std::vector<EvalCounters::BurstTile> bt;
      for (size_t i = 0; i < l.seq.size(); i++)
      {
        const size_t r = srow[l.seq[i]]; EvalCounters::BurstTile t;
        t.load_bursts = S.u(r, "wait_input_cycles"); t.store_bursts = S.u(r, "wait_output_cycles"); t.compute = S.u(r, "processing_cycles");
        if (!compat && !meas_p.empty()) { auto it = M.find(l.key[i]); if (it != M.end()) t.compute = it->second.busy; }
        bt.push_back(t);
      }
      ov.apply_burst_dma(bt, 1.0, 0.0);   // A=1,B=0: cost = measured cycles (no new numbers), schedule = the ported function
    }
    c.op_type = compat ? "GeMM" : "Conv2D";
    c.K = k_same ? kk : 1; c.M = k_same ? out_tot : macs; c.N = 1;   // M*K*N == theoretical MACs (convention of the reference model)
    c.reps = 1; c.X = 32; c.Y = 32; c.eb = 1; c.pes = PES; c.tiles = ctx; c.config_bytes = e.config_bytes;
    c.freq_ghz = dma.freq_ghz; c.ext_gbps = 16.0; c.rb = 65536;
    c.ddr_data_bytes = data_b; c.ddr_weight_bytes = wei_b;   // default (and --compat): sum of per-tile images, as sauria_model does for one job
    const bool lt_ok = !compat && have_lt && ltrow.count(l.id) && !LT.at(ltrow[l.id], "data_bytes").empty();
    if (lt_ok) { c.ddr_data_bytes = LT.u(ltrow[l.id], "data_bytes"); c.ddr_weight_bytes = LT.u(ltrow[l.id], "weight_bytes"); }
    footprint_est = footprint_est || lt_ok;
    // engine_* is NOT additive per tile: it is defined (eval_counters.h) as max(mac,dma) over the layer TOTAL, not the sum of maxima.
    e.engine_active_cycles = std::max(e.mac_engine_cycles, e.dma_engine_cycles);
    e.engine_idle_cycles = e.total_cycles > e.engine_active_cycles ? e.total_cycles - e.engine_active_cycles : 0;
    // pe_active/pe_idle -> "derived" branch of eval_fill_row: active = ceil(MAC/PE) (useful work), idle = total - active.
    // exec is NOT used (see parameters.json: utilization_definition). Clear BOTH; clearing only one breaks active+idle == total.
    if (!compat) { e.pe_active_cycles = 0; e.pe_idle_cycles = 0; }
    EvalRow er; er.layer_id = std::to_string(l.id); er.layer_name = l.name;
    ecs.push_back(e); lcs.push_back(c); ovs.push_back(ov); names.push_back(l.name); ntiles.push_back(l.seq.size());
    eval_fill_row(er, ecs.back(), lcs.back());
    rows.push_back(er); tiles_total += l.seq.size();
  }
  std::vector<EvalRow> layer_rows = rows;
  EvalRow netr;
  {
    std::vector<const EvalCounters *> pe; std::vector<const LayerCtx *> pc;
    for (size_t i = 0; i < ecs.size(); i++) { pe.push_back(&ecs[i]); pc.push_back(&lcs[i]); }
    eval_network_row(netr, pe, pc); eval_network_finish(netr, layer_rows);
  }
  rows.push_back(netr);
  {  // try writing first; create the directory only if that fails (avoids an unnecessary shell call)
    std::ofstream probe(out + "/.probe"); 
    if (!probe) {
#ifdef _WIN32
      std::string mk = "mkdir \"" + out + "\" >NUL 2>&1";
#else
      std::string mk = "mkdir -p \"" + out + "\"";
#endif
      if (std::system(mk.c_str())) {}
    } else { probe.close(); std::remove((out + "/.probe").c_str()); }
  }
  write_eval_dir(es, rows, out);
  uint64_t cov_tiles = 0, cov_meas = 0;
  {
    std::ofstream f(out + "/coverage.csv");
    f << "layer_id,layer_name,tiles_with_shape,tiles_measured_core_port,measured_pct,mode,abs_dbusy_up,abs_dbusy_down,abs_dexec_up,abs_dexec_down\n";
    for (size_t i = 0; i < ecs.size(); i++)
    {
      const auto &v = cov[i]; cov_tiles += v[0]; cov_meas += v[1];
      f << rows[i].layer_id << "," << names[i] << "," << v[0] << "," << v[1] << "," << fmt_d(v[0] ? 100.0 * v[1] / v[0] : 0, 2) << ","
        << (v[1] == 0 ? "B" : (v[1] == v[0] ? "A" : "A+B")) << "," << v[2] << "," << v[3] << "," << v[4] << "," << v[5] << "\n";
    }
  }

  // ---- project_metrics.csv: names DIFFER from the schema names, project definitions ----
  {
    std::ofstream f(out + "/project_metrics.csv");
    f << "layer_id,layer_name,op_type,core_busy_cycles,exec_cycles,pipeline_stall_cycles,total_macs,pe_mac_utilization_pct,"
         "array_exec_share_of_busy_pct,core_gops,total_cycles_no_overlap_UPPER,total_cycles_ideal_overlap_LOWER,tiles\n";
    uint64_t sb = 0, se = 0, sm = 0, su = 0, sl = 0, st = 0;
    auto line = [&](const std::string &id, const std::string &nm, const std::string &op, uint64_t b, uint64_t x, uint64_t m,
                    uint64_t up, uint64_t lo, uint64_t nt) {
      f << id << "," << nm << "," << op << "," << b << "," << x << "," << (b > x ? b - x : 0) << "," << m << ","
        << fmt_d(b ? 100.0 * (double)m / ((double)b * PES) : 0, 3) << "," << fmt_d(b ? 100.0 * (double)x / (double)b : 0, 3) << ","
        << fmt_d(b ? 2.0 * (double)m * dma.freq_ghz / (double)b : 0, 2) << "," << up << "," << lo << "," << nt << "\n"; };
    for (size_t i = 0; i < ecs.size(); i++)
    {
      const uint64_t m = lcs[i].M * lcs[i].K * lcs[i].N;
      const uint64_t nt = ntiles[i];
      line(rows[i].layer_id, names[i], lcs[i].op_type, ecs[i].processing_cycles, ecs[i].core_exec_cycles, m,
           ecs[i].total_cycles, ovs[i].total_cycles, nt);
      sb += ecs[i].processing_cycles; se += ecs[i].core_exec_cycles; sm += m; su += ecs[i].total_cycles; sl += ovs[i].total_cycles; st += nt;
    }
    line("network", "NETWORK", "scope=network", sb, se, sm, su, sl, st);
  }

  // ---- parameters.json (MODE B) ----
  g_footprint_est = footprint_est;
  {
    const uint64_t C_bytes = C_cells * C_cell_bytes;   // C is declared in cells (128 B/cell); convert to BYTES before adding (unit error in tb_evaluate.cpp)
    const uint64_t l2 = A_bytes + B_bytes + C_bytes;
    std::ofstream f(out + "/parameters.json");
    g_meas_pct = cov_tiles ? 100.0 * cov_meas / cov_tiles : 0.0;
    f << "{\n  \"model_name\": \"sauria_core\",\n  \"mode\": \"" << (cov_meas ? "A+B" : "B") << "\",\n";
    f << "  \"mode_b_definition\": \"MODE B = built up from the SHAPE TABLE (each tile shape measured once on sauria_model, then multiplied by the tile count from the program) -- "
         "NOT measured through the core port over a network run. The formulas are the port's writer verbatim (rtl_ref_eval_writer.h).\",\n";
    f << "  \"geometry\": \"32x32\",\n  \"mac_count\": 1024,\n  \"pe_count\": 1024,\n  \"frequency_hz\": " << fmt_d(dma.freq_ghz * 1e9, 1) << ",\n";
    f << "  \"ops_per_mac\": 2,\n  \"supported_precision\": [\n    \"int8\"\n  ],\n  \"supported_operators\": [\n    \"GeMM\",\n    \"Conv2D\"\n  ],\n";
    f << "  \"l1_size_bytes\": 4096,\n  \"l1_size_verified\": false,\n"
         "  \"l1_size_note\": \"NOT VERIFIED: 4096 = X*Y*C_BYTES = 32*32*4 (the PE array's accumulator registers) per sauria_model's formula in tb_evaluate.cpp:3698; "
         "this number has not been seen in the HAS/RTL.\",\n";
    f << "  \"l2_size_bytes\": " << l2 << ",\n";
    f << "  \"l2_size_breakdown_bytes\": {\"A\": " << A_bytes << ", \"B\": " << B_bytes << ", \"C\": " << C_bytes << ", \"C_cells\": " << C_cells
      << ", \"C_bytes_per_cell\": " << C_cell_bytes << "},\n";
    f << "  \"l2_size_note\": \"A+B+C computed in BYTES (C: 1,536 cells x 128 B = 196,608). sauria_model's tb_evaluate.cpp:3699 adds the CELL COUNT as if it were bytes => 329,216 (wrong).\",\n";
    f << "  \"l2_usable_per_tile_bytes\": 262144,\n  \"l2_usable_per_tile_note\": \"the tile splitter uses ONE bank per operand: 80,896 + 82,944 + 98,304 (fe_work/step6/program.json limits)\",\n";
    f << "  \"scratch\": \"not modeled\",\n  \"scratch_note\": \"the core-port path has no scratch (rtl_ref_sram_top.h); skip going to bank 3 of v4.5's Sram is a PROJECT ASSUMPTION, not confirmed hardware\",\n";
    f << "  \"peak_external_bw_gbps\": 16.00,\n  \"peak_internal_bw_gbps\": " << fmt_d((double)(32 + 32) * 1 * dma.freq_ghz, 2)
      << ",\n  \"hw_internal_bw_gbps\": " << fmt_d(dma.int_bytes_per_cycle * dma.freq_ghz, 2) << ",\n";
    f << "  \"dma_timing_model\": {\n    \"ddr_gbps\": " << fmt_d(dma.ddr_gbps, 2) << ",\n    \"setup_cycles\": " << dma.setup_cycles << ",\n"
      << "    \"double_buffer\": false,\n"
         "    \"double_buffer_note\": \"there ARE two banks per operand, but the tile splitter uses only ONE bank/tile and the second act bank already holds skip; 96.9% of tiles CANNOT prefetch "
         "(the second act bank holds the skip tensor). The schedule here is a SEQUENTIAL SUM (every tile's DMA is fully exposed) => an upper bound; total_cycles_ideal_overlap_LOWER in project_metrics.csv is the lower bound.\",\n"
      << "    \"int_bytes_per_cycle\": " << fmt_d(dma.int_bytes_per_cycle, 2) << ",\n"
      << "    \"note\": \"compute cycles = measured by sauria_model per SHAPE (MODE B); transfer cycles = analytic model. DDR/L1-L2 numbers are still PLACEHOLDERS until HW confirms.\"\n  },\n";
    f << "  \"provenance\": \"MODE B: sauria_model's shape table + the rtl_ref_eval_writer.h writer (standalone SW-EVAL, no SystemC)\",\n";
    f << "  \"build_config\": \"" << (compat ? "compat (reproduces sauria_model verbatim, NOT for reporting)" : "sw_eval MODE B") << "\",\n";
    f << "  \"stimulus\": \"layers=" << ecs.size() << " tiles=" << tiles_total << " tiles_without_shape=" << dropped_tiles << " layers_dropped=" << omitted.size() << "\",\n";
    f << "  \"tile_counts\": {\"program_frame\": " << (tiles_total + dropped_tiles) << ", \"with_measurement_basis\": " << tiles_total
      << ", \"gated_no_pass_shape\": " << dropped_tiles << ", \"measured_core_port\": " << cov_meas
      << ", \"note\": \"program_frame = program.json (83 conv layers); with_measurement_basis = tiles whose shape PASSed (feeds into core busy 209,651,588); gated = shapes that FAILed on sauria_model (det.p3.box_out)\"},\n";
    f << "  \"utilization_definition\": {\"chosen\": \"pe_mac_utilization_pct = Total MACs / (core_busy * 1024)\", "
         "\"pe_active_cycles\": \"= ceil(theoretical MACs / 1024) (useful-work, matches the schema description); NO LONGER = exec_cycles\", "
         "\"engine_utilization\": \"core_busy / total_cycles (share of wall-clock time the core is busy) -- NOT PE utilization\", \"array_exec_share_of_busy_pct\": \"exec / busy -- a DIFFERENT quantity, different name (array actually computing vs. merely occupied)\", \"mac_active_cycles\": \"= exec_cycles\"},\n";
    f << "  \"unverified_parameters\": [\"l1_size_bytes\", \"scratch\", \"skip_storage_bank3\", \"sram_window_tile_call\", \"dram_bandwidth_16gbps_placeholder\"],\n";
    f << "  \"metric_provenance\": {\n";
    bool first = true;
    for (const auto &cf : es.files) for (const auto &m : es.metrics.at(cf))
    { if (!first) f << ",\n"; f << "    \"" << m << "\": \"" << label_modeb(m, eval_metric_provenance(m)) << "\""; first = false; }
    f << "\n  }\n}\n";
  }
  {
    std::ofstream f(out + "/MODE_B.txt");
    f << "MODE B: every number in this directory is BUILT UP from the shape table (sauria_model, 1 tile per shape) x the tile count from the program.\n"
         "NONE of it was measured through the core port over a network run. See parameters.json -> metric_provenance / utilization_definition.\n"
         "layers=" << ecs.size() << " tiles=" << tiles_total << " tiles_without_shape=" << dropped_tiles << "\n";
    for (auto &o : omitted) f << "layer dropped (no tile has a shape): " << o << "\n";
  }
  fprintf(stderr, "[sw_eval] %s: %zu layers, %llu tiles, %llu tiles without a shape, %zu layers dropped -> %s\n", compat ? "compat" : (cov_meas ? "MODE A+B" : "MODE B"),
          ecs.size(), (unsigned long long)tiles_total, (unsigned long long)dropped_tiles, omitted.size(), out.c_str());
  return 0;
}
