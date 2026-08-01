// SystemC Model for SAURIA NPU Core
// Systolic Array 2D Grid Block with configurable hardware parameters

#ifndef SAURIA_SA_ARRAY_H
#define SAURIA_SA_ARRAY_H

#include "sauria_types.h"
#include "debug.h"
#include "sa_processing_element.h"
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

#ifndef FX1_NO_PERF
#include "instrumentation/perf_counters.h"
#endif

static constexpr int CSWITCH_EXTRA_MARGIN = 4;
namespace sauria
{
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
        sc_in<uint32_t> i_context_id{"i_context_id"};

        PeConfig config;

#ifndef FX1_NO_PERF
        // Optional, non-owning pointer to shared perf counters
        // left null in normal runs -> zero overhead, no behavior change
        fx1::PerfCounters *perf{nullptr};
#endif

        void dump_mac_matrix(uint32_t context, const std::string &tag)
        {
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

            SC_METHOD(grid_process);
            sensitive << i_clk.pos();
        }

        void grid_process()
        {
            if (!i_rstn.read() || i_sa_clear.read())
            {
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
                        int delay_len = y + x + 1 + mul_lat + CSWITCH_EXTRA_MARGIN + (config.extra_csreg ? 1 : 0);

                        cs_delay[y][x].assign(delay_len, false);
                        if (std::string(this->name()).find("NpuTop_std") != std::string::npos &&
                            y == 0 && x == 3)
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

            if (!i_pipeline_en.read())
                return;

            act_vector_t<Y_DIM, T_ACT> act_in = i_act_arr.read();
            wei_vector_t<X_DIM, T_WEI> wei_in = i_wei_arr.read();
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

                    // Có thể giữ dump này để debug, nhưng KHÔNG dùng nó để kết luận compute final.
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
                    if (is_std_array && i_context_id.read() == 0 && y == 0 && x == 3)
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

            // ---------------------------------------------------------
            // Debug only when cscan_en is active.
            // This checks the actual scan phase used by PSM.
            // ---------------------------------------------------------
            static uint32_t dbg_cscan_count = 0;

            if (i_cscan_en.read() && dbg_cscan_count < 64)
            {
                // DBG_COUT << "\n[SA CSCAN DEBUG]"
                //           << " inst=" << this->name()
                //           << " cscan_count=" << dbg_cscan_count
                //           << " cscan_en=" << i_cscan_en.read()
                //           << " raw_cswitch=" << i_cswitch_arr.read()
                //           << "\n";

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
            // static uint32_t dbg_sa_total_cycle = 0;
            static uint32_t dbg_sa_event_count = 0;

            bool debug_event =
                any_cs_bit ||
                i_cscan_en.read();

            if (debug_event && dbg_sa_event_count < 128)
            {
                // DBG_COUT << "\n[SA DEBUG DETAIL]"
                //           << " inst=" << this->name()
                //           << " event=" << dbg_sa_event_count
                //           << " raw_cswitch=" << cswitch
                //           << " delayed_cs_row0=" << cs_bit_row0
                //           << " cscan_en=" << i_cscan_en.read()
                //           << "\n";

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

        bool raw_cswitch_q{false};
        bool local_cswitch_q[Y_DIM][X_DIM];
        uint32_t cswitch_context_latched{0};
    };

} // namespace sauria

#endif // SAURIA_SA_ARRAY_H

