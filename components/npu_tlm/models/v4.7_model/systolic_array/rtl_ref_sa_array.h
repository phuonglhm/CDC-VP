// 1:1 copy of sauria_model's systolic_array/sa_array.h.
// Changes from the reference file (both file-scope, not namespace-scope -- namespace rename alone
// does not resolve these):
//   1. namespace `sauria` -> `sauria_rtl` (avoid collision with this tree's own sauria::SystolicArray).
//   2. `CSWITCH_EXTRA_MARGIN` (file-scope constexpr, outside any namespace) -> `RTL_REF_CSWITCH_EXTRA_MARGIN`
//      -- both this tree's NATIVE sa_array.h and sauria_model's sa_array.h declare this same name at
//      file scope; including both headers in one translation unit would otherwise be a redefinition.
//   3. `#include "sa_processing_element.h"` -> `#include "rtl_ref_sa_processing_element.h"` (the
//      ported ProcessingElement/PeConfig, not this tree's native ones).
// Everything else is byte-for-byte identical to the reference file -- do not "clean up" or
// reinterpret without re-checking against sauria_model's actual source.
//
// This array's cs_delay[y][x] staggering (below) works together with control/rtl_ref_context_switch_controller.h's
// staggered cswitch_arr timing (PE_LAT+i per column) -- ported as a matched pair, not mixed with this tree's native
// array (the native array with the ported control does not accumulate correctly for K > 1).
#ifndef RTL_REF_SA_ARRAY_H
#define RTL_REF_SA_ARRAY_H

#include "sauria_types.h"
#include "debug.h"
#include "rtl_ref_sa_processing_element.h"
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

#ifndef FX1_NO_PERF
#include "instrumentation/rtl_ref_perf_counters.h"
#endif

static constexpr int RTL_REF_CSWITCH_EXTRA_MARGIN = 4;
namespace sauria_rtl
{
    using namespace sauria;

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = float,
        typename T_WEI = float,
        typename T_PSUM = float>
    class SystolicArray : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Dynamic threshold for zero-detection/negligence
        sc_in<float> i_threshold{"i_threshold"};

        // Wavefront inputs from feeders
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_act_arr{"i_act_arr"}; // Size Y
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_wei_arr{"i_wei_arr"}; // Size X

        // Scan-chain input/output connections with PSM (Right-to-Left chain)
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr{"i_c_arr"};  // Size Y
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr{"o_c_arr"}; // Size Y

        // Control Inputs
        sc_in<bool> i_pipeline_en{"i_pipeline_en"};
        sc_in<bool> i_cscan_en{"i_cscan_en"};
        sc_in<sc_bv<X_DIM>> i_cswitch_arr{"i_cswitch_arr"}; // Precise delays per row
        sc_in<bool> i_sa_clear{"i_sa_clear"};
        sc_in<bool> i_softstall{"i_softstall"};
        sc_in<bool> i_pop_en_dbg{"i_pop_en_dbg"};  // only for trace labels
        sc_in<uint32_t> i_context_id{"i_context_id"};

        PeConfig config;

#ifndef FX1_NO_PERF
        // Optional, non-owning pointer to shared perf counters
        // left null in normal runs -> zero overhead, no behavior change
        sauria_rtl::PerfCounters *perf{nullptr};
#endif

        void dump_mac_matrix(uint32_t context, const std::string &tag)
        {
            if (!FX1_DUMPS) return;
            static bool header_written = false;

            std::ofstream f;
            if (!header_written)
            {
                f.open("trace_sysc/sa_macq_dump.csv", std::ios::out);
                f << "tag,context,y,x,mac_q,mac_sc_q\n";
                header_written = true;
            }
            else
            {
                f.open("trace_sysc/sa_macq_dump.csv", std::ios::app);
            }

            for (int y = 0; y < Y_DIM; y++)
            {
                for (int x = 0; x < X_DIM; x++)
                {
                    f << tag << ","
                      << context << ","
                      << y << ","
                      << x << ","
                      << static_cast<int64_t>(grid[y][x].mac_q) << ","
                      << static_cast<int64_t>(grid[y][x].mac_sc_q)
                      << "\n";
                }
            }

            f.close();

            DBG_COUT << "[SA MAC DUMP]"
                      << " inst=" << this->name()
                      << " tag=" << tag
                      << " context=" << context
                      << " time=" << sc_time_stamp()
                      << std::endl;
        }

        void dump_mac_cell(uint32_t context, int y, int x, const std::string &tag)
        {
            if (!FX1_DUMPS) return;
            static bool header_written = false;

            std::ofstream f;
            if (!header_written)
            {
                f.open("trace_sysc/sa_macq_cell_dump.csv", std::ios::out);
                f << "tag,context,y,x,mac_q,mac_sc_q,time\n";
                header_written = true;
            }
            else
            {
                f.open("trace_sysc/sa_macq_cell_dump.csv", std::ios::app);
            }

            f << tag << ","
              << context << ","
              << y << ","
              << x << ","
              << static_cast<int64_t>(grid[y][x].mac_q) << ","
              << static_cast<int64_t>(grid[y][x].mac_sc_q) << ","
              << sc_time_stamp()
              << "\n";

            f.close();
        }

        // Custom Constructor
        SC_HAS_PROCESS(SystolicArray);
        SystolicArray(sc_module_name nm, const PeConfig &cfg = PeConfig())
            : sc_module(nm), config(cfg)
        {

            // Apply configuration to all processing elements in grid
            for (int y = 0; y < Y_DIM; y++)
            {
                for (int x = 0; x < X_DIM; x++)
                {
                    grid[y][x].config = config;
                }
            }

            // PERF: evaluated once instead of 1024x per cycle
            // inside grid_process()'s reset loop. Instance name never changes.
            dbg_is_std_inst_ =
                (std::string(this->name()).find("NpuTop_std") != std::string::npos);

            SC_METHOD(grid_process);
            sensitive << i_clk.pos();
        }

        void grid_process()
        {
            if (!i_rstn.read() || i_sa_clear.read())
            {
                // PERF: this body is IDEMPOTENT. It used to re-run
                // every cycle for as long as i_sa_clear stayed high -- 1024 vector
                // re-allocs + 1024 std::string constructions per cycle per array.
                // ContextFsm holds sa_clear=1 throughout IDLE (the old
                // switch(state) drove it 0), so guard-on builds paid it for the
                // entire idle period. Applying it once per reset EPISODE is
                // bit-identical: nothing below depends on being repeated.
                if (sa_reset_applied_)
                    return;
                sa_reset_applied_ = true;

                raw_cswitch_q = false;
                cswitch_context_latched = 0;

                for (int yy = 0; yy < Y_DIM; yy++)
                {
                    for (int xx = 0; xx < X_DIM; xx++)
                    {
                        local_cswitch_q[yy][xx] = false;
                    }
                }
                o_c_arr.write(psum_vector_t<Y_DIM, T_PSUM>());
                for (int y = 0; y < Y_DIM; y++)
                {
                    for (int x = 0; x < X_DIM; x++)
                    {
                        grid[y][x].reset();
                        int mul_lat = config.stages_mul + (config.intermediate_pipeline_stage ? 1 : 0);

                        // Context switch must wait until the last product has passed
                        // through the PE multiplier pipeline and reached mac_q.
#ifdef FX1_A3_CONTEXT_FSM
                        // Under the RTL-faithful ContextSwitchController (staggered o_cswitch_arr, cscnt_q == PE_LAT+i per
                        // column), a "+x" term would double-count the column-propagation delay: in the RTL (sa_array.sv +
                        // sa_processing_element.sv) i_cswitch_arr[x] enters column x at row 0 with ZERO internal x-delay;
                        // only the Y (row) delay comes from the array's register chain (mat_cswitch[j] -> mat_cswitch[j+1]).
                        // PE_LAT is the RTL value (STAGES_MUL + INTERMEDIATE_PIPELINE_STAGE + ZERO_GATING_MULT = 2).
                        //
                        // "+2" (not "+1") comes from a cycle-by-cycle reference model that re-derives BOTH: (a) mac_q's true settle
                        // time vs cswitch arrival -- uniform "+1" gap at all
                        // 64 tested (y,x) positions in a 32x32/K=64 sim,
                        // confirmed unchanged even with zero-gating (zero_det_
                        // neg.sv) properly modeled; AND (b) the scan-chain
                        // TRANSIT itself (mac_sc_q doubles as the C-chain scan
                        // register -- a swapped value must shift X-1-x MORE
                        // hops through other columns to reach o_c_arr, where
                        // PSM actually reads it). For the farthest corner
                        // specifically, its own swap coincides with the exact
                        // cycle scanning starts, costing it ONE EXTRA transit
                        // cycle beyond the naive hop-count (its value isn't
                        // "ready to shift" until the cycle after the swap,
                        // unlike every other column whose swap already
                        // happened earlier and was already sitting still).
                        // Combined: "+1" (settle) + "+1" (scan-start transit
                        // coincidence, worst case only) = "+2".
                        //
                        // the previous version of this
                        // line applied "+2" to EVERY column x (delay_len
                        // depended on y only) -- that does NOT match what
                        // the reference model actually found. Re-reading
                        // ref_sa_array.cpp's own scan-start-coincidence
                        // mechanism: scan_start_cyc is defined as
                        // the cycle the SINGLE farthest PE (y=Y_DIM-1,
                        // x=X_DIM-1) swaps -- the "+1 extra transit cycle"
                        // this causes is therefore intrinsic to THAT ONE
                        // (y,x) pair only, not to "column X_DIM-1 for every
                        // row" or "row Y_DIM-1 for every column". Every
                        // other position (including other rows at column
                        // X_DIM-1, and other columns at row Y_DIM-1) only
                        // needs the uniform "+1" settle margin. Also
                        // verified that ref_sa_array.cpp's
                        // own swap formula (mac_sc_d=mac_q, mac_d=mac_sc_q+
                        // mul_q) matches real RTL exactly -- these "+1"/"+2"
                        // values are trustworthy, not tainted by the swap-
                        // arithmetic bug fixed in sa_processing_element.h.
                        //
                        // Changed ONLY under this guard; guard-off keeps the original formula below unchanged.
                        //
                        // FX1_A3_CSDELAY_MODE selects the formula (rtl_ref_defaults.h sets 3 = RTL; the fallback below is the
                        // old hand-tuned mode 0, kept for comparison).
#ifndef FX1_A3_CSDELAY_MODE
#define FX1_A3_CSDELAY_MODE 0
#endif
                        // RTL sa_array.sv propagates cswitch DOWN one PE register per row => delay = y, depends on y only.
                        // Mode 1 uses exactly that; mode 2 borrows the guard-off formula; mode 0 keeps the old hand-tuned one.
                        int delay_len;
#ifndef FX1_A3_EXTRA_CSREG
#define FX1_A3_EXTRA_CSREG 0
#endif
#if FX1_A3_CSDELAY_MODE == 3
                        // With EXTRA_CSREG = 1 the RTL has ONE MORE cswitch register in EVERY PE (sa_processing_element.sv:196
                        // `cswitch_q_ext <= i_cswitch` when pipeline_ff_en). Mode 1 has only the row chain (y);
                        // mode 3 = full RTL: y + EXTRA_CSREG.
                        delay_len = y + (FX1_A3_EXTRA_CSREG ? 1 : 0);
#elif FX1_A3_CSDELAY_MODE == 1
                        delay_len = y;
#elif FX1_A3_CSDELAY_MODE == 2
                        delay_len = y + x + 1 + mul_lat + RTL_REF_CSWITCH_EXTRA_MARGIN +
                                    (config.extra_csreg ? 1 : 0);
#else
                        delay_len = (y == Y_DIM - 1 && x == X_DIM - 1) ? (y + 2) : (y + 1);
#endif
#else
                        int delay_len = y + x + 1 + mul_lat + RTL_REF_CSWITCH_EXTRA_MARGIN + (config.extra_csreg ? 1 : 0);
#endif

                        cs_delay[y][x].assign(delay_len, false);
                        if (dbg_is_std_inst_ && y == 0 && x == 3)
                        {
                            DBG_COUT << "[CS_DELAY_INIT]"
                                      << " inst=" << this->name()
                                      << " y=" << y
                                      << " x=" << x
                                      << " mul_lat=" << mul_lat
                                      << " extra_csreg=" << config.extra_csreg
                                      << " delay_len=" << delay_len
                                      << std::endl;
                        }
                    }
                }
                return;
            }
            sa_reset_applied_ = false;

            if (!i_pipeline_en.read())
                return;

            act_vector_t<Y_DIM, T_ACT> act_in = i_act_arr.read();
            wei_vector_t<X_DIM, T_WEI> wei_in = i_wei_arr.read();
#ifdef FX1_A3_SOFTSTALL_ZERO2
            // OBUF_BUSY_SHIFT = "Array shifting ZEROS (soft-stall)"
            // (context_fsm.sv:386). Holding the old operand when there is no pop would make the PE add one product TWICE; the
            // RTL feeds zeros.
            if (i_softstall.read())
            {
                act_in = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                wei_in = wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0));
            }
#endif
            psum_vector_t<Y_DIM, T_PSUM> scan_in = i_c_arr.read();
            sc_bv<X_DIM> cswitch = i_cswitch_arr.read();

            bool raw_cswitch_any = false;
            for (int x = 0; x < X_DIM; x++)
            {
                if (cswitch[x].to_bool())
                {
                    raw_cswitch_any = true;
                    break;
                }
            }

            // Dump MAC_Q exactly before raw cswitch enters the delayed cswitch pipeline.
            // This is the best point to check whether SA compute result is already correct.
            std::string inst_name = this->name();

            bool is_std_array = inst_name.find("NpuTop_std") != std::string::npos;

            if (raw_cswitch_any && !raw_cswitch_q)
            {
                cswitch_context_latched = i_context_id.read();

                if (is_std_array)
                {
                    DBG_COUT << "[SA RAW CSWITCH]"
                              << " inst=" << this->name()
                              << " context_latched=" << cswitch_context_latched
                              << " time=" << sc_time_stamp()
                              << std::endl;

                    // Debug dump only; do NOT use it to judge the final compute result.
                    dump_mac_matrix(cswitch_context_latched, "before_raw_cswitch");
                }
            }

            raw_cswitch_q = raw_cswitch_any;

            // 1. Capture current pipeline state (from previous cycle)
            T_ACT prev_a[Y_DIM][X_DIM];
            T_WEI prev_b[Y_DIM][X_DIM];
            T_PSUM prev_sc[Y_DIM][X_DIM];

            for (int y = 0; y < Y_DIM; y++)
            {
                for (int x = 0; x < X_DIM; x++)
                {
                    prev_a[y][x] = grid[y][x].a_q;
                    prev_b[y][x] = grid[y][x].b_q;
                    prev_sc[y][x] = grid[y][x].mac_sc_q;
                }
            }

            psum_vector_t<Y_DIM, T_PSUM> scan_out;
            // debug delayed context-switch actually used by PE
            bool any_cs_bit = false;
            sc_bv<X_DIM> cs_bit_row0;
            cs_bit_row0 = 0;

            // 2. Step execution with pipeline registered values
            for (int y = 0; y < Y_DIM; y++)
            {
                for (int x = 0; x < X_DIM; x++)
                {
                    // Determine activation input (A-port, registered horizontally)
                    T_ACT a_val = (x == 0) ? act_in[y] : prev_a[y][x - 1];

                    // Determine weight input (B-port, registered vertically)
                    T_WEI b_val = (y == 0) ? wei_in[x] : prev_b[y - 1][x];

                    // Determine scan-chain input (C-port, registered right-to-left)
                    T_PSUM c_val = (x == X_DIM - 1) ? scan_in[y] : prev_sc[y][x + 1];

                    // Cycle-accurate context-switch staggering delay
                    bool cs_in = cswitch[x].to_bool();
                    cs_delay[y][x].push_back(cs_in);

                    bool cs_bit = cs_delay[y][x].front();
                    cs_delay[y][x].erase(cs_delay[y][x].begin());

                    if (cs_bit)
                    {
                        any_cs_bit = true;
                    }

                    if (y == 0)
                        cs_bit_row0[x] = cs_bit ? sc_dt::Log_1 : sc_dt::Log_0;

                    // ---------------------------------------------------------
                    // Dump final MAC_Q of each PE exactly before its local cswitch.
                    // This is the correct value that will be swapped into scan-chain.
                    // ---------------------------------------------------------
                    bool local_cswitch_rise = cs_bit && !local_cswitch_q[y][x];
                    local_cswitch_q[y][x] = cs_bit;
                    bool pe_cscan_en = i_cscan_en.read() && !local_cswitch_rise;
                    // Step execution
                    if (FX1_DUMPS && is_std_array && i_context_id.read() == 0 && y == 0 && x == 3)
                    {
                        static std::ofstream f("trace_sysc/pe03_trace.csv");
                        static bool header = false;
                        static uint32_t pe_dbg_cycle = 0;

                        if (!header)
                        {
                            f << "cycle,context,a,b,cs_bit,local_cswitch,cscan_en,pipeline_en,"
                              << "mac_q_before,mac_sc_before,mac_q_after,mac_sc_after\n";
                            header = true;
                        }

                        T_PSUM mac_q_before = grid[y][x].mac_q;
                        T_PSUM mac_sc_before = grid[y][x].mac_sc_q;

                        // Step PE with pulse, not level.
                        grid[y][x].step(a_val, b_val, c_val, local_cswitch_rise,
                                        pe_cscan_en, i_pipeline_en.read(),
                                        i_threshold.read());

                        f << pe_dbg_cycle << ","
                          << i_context_id.read() << ","
                          << static_cast<int32_t>(a_val) << ","
                          << static_cast<int32_t>(b_val) << ","
                          << cs_bit << ","
                          << local_cswitch_rise << ","
                          << pe_cscan_en << ","
                          << i_pipeline_en.read() << ","
                          << static_cast<int64_t>(mac_q_before) << ","
                          << static_cast<int64_t>(mac_sc_before) << ","
                          << static_cast<int64_t>(grid[y][x].mac_q) << ","
                          << static_cast<int64_t>(grid[y][x].mac_sc_q)
                          << "\n";

                        pe_dbg_cycle++;
                    }
#ifdef FX1_A3_PE01_ALLCTX
                    // Debug hook (default off): drops the hard-coded "context == 0" filter to cover all contexts (the header
                    // already has a `context` column).
                    else if (FX1_DUMPS && is_std_array && y == 0 && (x == 0 || x == 1))
#else
                    else if (FX1_DUMPS && is_std_array && i_context_id.read() == 0 && y == 0 && (x == 0 || x == 1))
#endif
                    {
                        // pinpoint the swap<->shift boundary
                        // between column 0 and column 1 during PREWRITE_SHIFT
                        // (col0=array_out0, feeds PSM directly). Records the
                        // exact shift-in source value (c_val) alongside
                        // before/after mac_sc_q so the cyc381/382 duplicate
                        // can be explained directly instead of inferred.
#ifdef FX1_A3_SELFCHECK
                        if (x == 0) grid[y][x].selfchk_log_ = true;
#endif
                        static std::ofstream f01("trace_sysc/pe01_trace.csv");
                        static bool header01 = false;
                        static uint32_t pe01_dbg_cycle0 = 0;
                        static uint32_t pe01_dbg_cycle1 = 0;

                        if (!header01)
                        {
                            f01 << "tsim,cycle,x,context,a,b,cs_bit,local_cswitch,cscan_en,pipeline_en,"
                                << "mac_q_before,mac_sc_before,mac_q_after,mac_sc_after,c_val,pop_en\n";
                            header01 = true;
                        }

                        T_PSUM mac_q_before = grid[y][x].mac_q;
                        T_PSUM mac_sc_before = grid[y][x].mac_sc_q;

                        grid[y][x].step(a_val, b_val, c_val, local_cswitch_rise,
                                        pe_cscan_en, i_pipeline_en.read(),
                                        i_threshold.read());

                        uint32_t &ctr = (x == 0) ? pe01_dbg_cycle0 : pe01_dbg_cycle1;
                        f01 << sc_core::sc_time_stamp().value() << ","
                            << ctr << ","
                            << x << ","
                            << i_context_id.read() << ","
                            << static_cast<int32_t>(a_val) << ","
                            << static_cast<int32_t>(b_val) << ","
                            << cs_bit << ","
                            << local_cswitch_rise << ","
                            << pe_cscan_en << ","
                            << i_pipeline_en.read() << ","
                            << static_cast<int64_t>(mac_q_before) << ","
                            << static_cast<int64_t>(mac_sc_before) << ","
                            << static_cast<int64_t>(grid[y][x].mac_q) << ","
                            << static_cast<int64_t>(grid[y][x].mac_sc_q) << ","
                            << static_cast<int64_t>(c_val)
                            << "," << (int)i_pop_en_dbg.read()
                            << "\n";
                        ctr++;
                    }
                    else
                    {
                        // Step PE with rise, not level.
                        grid[y][x].step(a_val, b_val, c_val, local_cswitch_rise,
                                        pe_cscan_en, i_pipeline_en.read(),
                                        i_threshold.read());
                    }

                    // Dump AFTER local cswitch step.
                    // At this point, final accumulated mac_q_next has been swapped into mac_sc_q.
                    if (is_std_array && local_cswitch_rise)
                    {
                        dump_mac_cell(cswitch_context_latched, y, x, "after_local_cswitch");
                    }

#ifdef FX1_A3_PE_DUMP
                    // Additive, off-by-default: full (cycle,y,x) mac_q/mac_sc_q
                    // dump for cycle-by-cycle diff against a reference model /
                    // RTL. Only active on the "_std" instance (matches the
                    // convention of every other trace dump in this file) so a
                    // 3-instance (std/approx/gated) build does not triple the
                    // output. See note/K_tiling.../plan (Option A resumed).
                    if (is_std_array)
                    {
                        static std::ofstream pf("trace_sysc/pe_all_trace.csv");
                        static bool pf_header = false;
                        static uint32_t pf_cycle = 0;
                        if (!pf_header)
                        {
                            pf << "time,cycle,y,x,a,b,c,pipeline_en,thres,cswitch_rise,cscan_en,mac_q,mac_sc_q\n";
                            pf_header = true;
                        }
                        pf << sc_core::sc_time_stamp() << "," << pf_cycle << "," << y << "," << x << ","
                           << static_cast<int32_t>(a_val) << ","
                           << static_cast<int32_t>(b_val) << ","
                           << static_cast<int64_t>(c_val) << ","
                           << i_pipeline_en.read() << ","
                           << i_threshold.read() << ","
                           << local_cswitch_rise << "," << pe_cscan_en << ","
                           << static_cast<int64_t>(grid[y][x].mac_q) << ","
                           << static_cast<int64_t>(grid[y][x].mac_sc_q) << "\n";
                        // Advance once per full (y,x) sweep, not once per PE:
                        // increment on the last cell of the grid.
                        if (y == Y_DIM - 1 && x == X_DIM - 1) pf_cycle++;
                    }
#endif

#ifdef FX1_A3_PE_DUMP_ROW7
                    // Debug hook (default off): targeted dump of the last row (y = Y_DIM-1), cswitch_rise events only, across all
                    // columns and contexts (the full FX1_A3_PE_DUMP is too large for long runs). At a swap, mac_sc_q (the
                    // value just shelved) is 0 if the swap arrived before any real accumulate, nonzero if it arrived late.
                    if (is_std_array && y == (Y_DIM - 1) && local_cswitch_rise)
                    {
                        static std::ofstream pfr("trace_sysc/pe_row7_trace.csv");
                        static bool pfr_header = false;
                        if (!pfr_header)
                        {
                            pfr << "time,ctx,y,x,a,b,cswitch_rise,mac_q,mac_sc_q\n";
                            pfr_header = true;
                        }
                        pfr << sc_core::sc_time_stamp() << ","
                            << i_context_id.read() << ","
                            << y << "," << x << ","
                            << static_cast<int32_t>(a_val) << ","
                            << static_cast<int32_t>(b_val) << ","
                            << local_cswitch_rise << ","
                            << static_cast<int64_t>(grid[y][x].mac_q) << ","
                            << static_cast<int64_t>(grid[y][x].mac_sc_q) << "\n";
                        // No per-line flush (was causing severe I/O-bound
                        // slowdown, >40 min for a run that should take ~2-3
                        // min) -- rely on normal ofstream buffering + the
                        // destructor's flush at program exit.
                    }
#endif
                    // Output of leftmost column shifts out to PSM/Outputs
                    if (x == 0)
                    {
                        scan_out[y] = grid[y][0].mac_sc_q;
                    }
#ifndef FX1_NO_PERF
                    // Non-instrusive active-PE accounting: a PE counts as avtive
                    // when both operands are non-negligible (not zero-gated)
                    if (perf)
                    {
                        bool a_nz = (std::abs((double)a_val) > i_threshold.read());
                        bool b_nz = (std::abs((double)b_val) > i_threshold.read());
                        if (a_nz && b_nz)
                        {
                            perf->active_pe_cycles++;
                            perf->mac_ops++;
                        }
                        perf->total_pe_cycles++;
                    }
#endif
                }
            }
#ifndef FX1_NO_PERF
            if (perf)
                perf->exec_cycles++; // This clock did real comput work
#endif

            o_c_arr.write(scan_out);

#ifdef FX1_A3_ROW0_DUMP
            // unconditional (every cycle, not
            // gated by cscan_en) dump of ALL X_DIM columns' mac_sc_q for row
            // y=0 -- to empirically determine the TRUE shift_cnt<->column
            // mapping instead of hand-deriving it (the hand-derivation
            // attempt was wrong and made layer-#2 worse, 112->128/128).
            // Only the "_std" instance (same convention as every other dump
            // in this file). Written unconditionally so both the pre-scan
            // "quiet hold" snapshot (every column's true final value, before
            // any shifting starts) AND the full shift sequence are captured
            // in one file, directly comparable against psm_write_trace.csv's
            // "time" column.
            {
                std::string inst_name3 = this->name();
                if (inst_name3.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream row0_trace("trace_sysc/row0_full_trace.csv");
                    static bool row0_header = false;
                    if (!row0_header)
                    {
                        row0_trace << "time,row,cscan_en";
                        for (int x = 0; x < X_DIM; x++)
                            row0_trace << ",col" << x;
                        row0_trace << "\n";
                        row0_header = true;
                    }
                    for (int yy = 0; yy < Y_DIM; yy++)
                    {
                        row0_trace << sc_core::sc_time_stamp() << ","
                                   << yy << ","
                                   << i_cscan_en.read();
                        for (int x = 0; x < X_DIM; x++)
                            row0_trace << "," << static_cast<int32_t>(grid[yy][x].mac_sc_q);
                        row0_trace << "\n";
                    }
                    row0_trace.flush();
                }
            }
#endif

            // ---------------------------------------------------------
            // Debug only when cscan_en is active.
            // This checks the actual scan phase used by PSM.
            // ---------------------------------------------------------
            static uint32_t dbg_cscan_count = 0;

#ifdef FX1_A3_SCANOUT_TRACE
            if (i_cscan_en.read())
            {
                std::string inst_name2 = this->name();
                if (inst_name2.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream scanout_trace("trace_sysc/scanout_trace.csv");
                    static bool scanout_header = false;
                    static uint32_t scanout_count = 0;
                    if (!scanout_header)
                    {
                        scanout_trace << "time,scanout_count";
                        for (int y = 0; y < Y_DIM; y++)
                            scanout_trace << ",scan_out" << y;
                        scanout_trace << "\n";
                        scanout_header = true;
                    }
                    scanout_trace << sc_core::sc_time_stamp() << "," << scanout_count;
                    for (int y = 0; y < Y_DIM; y++)
                        scanout_trace << "," << static_cast<int32_t>(scan_out[y]);
                    scanout_trace << "\n";
                    scanout_trace.flush();
                    scanout_count++;
                }
            }
#endif

            if (i_cscan_en.read() && dbg_cscan_count < 64)
            {
                DBG_COUT << "  MAC_SC_Q row0 first8 = [";
                for (int x = 0; x < 8 && x < X_DIM; x++)
                {
                    DBG_COUT << grid[0][x].mac_sc_q;
                    if (x < 7 && x < X_DIM - 1)
                        DBG_COUT << ", ";
                }
                DBG_COUT << "]\n";

                DBG_COUT << "  scan_out = [";
                for (int y = 0; y < Y_DIM; y++)
                {
                    DBG_COUT << scan_out[y];
                    if (y < Y_DIM - 1)
                        DBG_COUT << ", ";
                }
                DBG_COUT << "]\n";

                dbg_cscan_count++;
            }

            // ---------------------------------------------------------
            // SA internal debug: event-based, not cycle-limited.
            // Print when cswitch or cscan_en is active.
            // ---------------------------------------------------------
            static uint32_t dbg_sa_event_count = 0;

            bool debug_event =
                any_cs_bit ||
                i_cscan_en.read();

            if (debug_event && dbg_sa_event_count < 128)
            {
                DBG_COUT << "  MAC_Q row0 first8 = [";
                for (int x = 0; x < 8 && x < X_DIM; x++)
                {
                    DBG_COUT << grid[0][x].mac_q;
                    if (x < 7 && x < X_DIM - 1)
                        DBG_COUT << ", ";
                }
                DBG_COUT << "]\n";

                DBG_COUT << "  MAC_SC_Q row0 first8 = [";
                for (int x = 0; x < 8 && x < X_DIM; x++)
                {
                    DBG_COUT << grid[0][x].mac_sc_q;
                    if (x < 7 && x < X_DIM - 1)
                        DBG_COUT << ", ";
                }
                DBG_COUT << "]\n";

                DBG_COUT << "  scan_out = [";
                for (int y = 0; y < Y_DIM; y++)
                {
                    DBG_COUT << scan_out[y];
                    if (y < Y_DIM - 1)
                        DBG_COUT << ", ";
                }
                DBG_COUT << "]\n";

                dbg_sa_event_count++;
            }
        }

        // Direct read access to PE accumulator state for standalone testing verification
        T_PSUM get_pe_mac(int y, int x) const
        {
            return grid[y][x].mac_q;
        }

        T_PSUM get_pe_mac_sc(int y, int x) const
        {
            return grid[y][x].mac_sc_q;
        }

    private:
        // 2D matrix of processing elements
        ProcessingElement<T_ACT, T_WEI, T_PSUM> grid[Y_DIM][X_DIM];

        // 2D context switch delay buffers
        std::vector<bool> cs_delay[Y_DIM][X_DIM];

        // Per-instance pipeline-state snapshot (was wrongly 'static' inside
        // grid_process, which shared state across all array instances).
        T_ACT prev_a[Y_DIM][X_DIM];
        T_WEI prev_b[Y_DIM][X_DIM];
        T_PSUM prev_sc[Y_DIM][X_DIM];

        // PERF -- see grid_process()'s reset branch.
        bool sa_reset_applied_{false};
        bool dbg_is_std_inst_{false};

        bool raw_cswitch_q{false};
        bool local_cswitch_q[Y_DIM][X_DIM];
        uint32_t cswitch_context_latched{0};
    };

} // namespace sauria_rtl

#endif // RTL_REF_SA_ARRAY_H
