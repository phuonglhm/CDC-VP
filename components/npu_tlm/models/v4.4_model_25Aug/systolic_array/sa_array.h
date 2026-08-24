// SystemC Model for SAURIA NPU Core
// Systolic Array 2D Grid Block with configurable hardware parameters
// Rev2: Split-lane execution aware (Lane A: rows 0 to N_split-1; Lane B: rows N_split to Y_DIM-1)

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
        int X_DIM = 64,
        int Y_DIM = 64, // Rev2 supports up to 64 rows
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

        // Dynamic lane split row boundary
        sc_in<uint32_t> i_nsplit{"i_nsplit"};

        // Wavefront inputs from feeders (Lane A and Lane B)
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_act_arr_a{"i_act_arr_a"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_act_arr_b{"i_act_arr_b"};
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_wei_arr_a{"i_wei_arr_a"};
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_wei_arr_b{"i_wei_arr_b"};

        // Scan-chain input/output connections with PSM (Lane A and Lane B)
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr_a{"i_c_arr_a"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr_a{"o_c_arr_a"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr_b{"i_c_arr_b"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr_b{"o_c_arr_b"};

        // Control Inputs (Lane A and Lane B)
        sc_in<bool> i_pipeline_en_a{"i_pipeline_en_a"};
        sc_in<bool> i_pipeline_en_b{"i_pipeline_en_b"};
        sc_in<bool> i_cscan_en_a{"i_cscan_en_a"};
        sc_in<bool> i_cscan_en_b{"i_cscan_en_b"};
        sc_in<sc_bv<X_DIM>> i_cswitch_arr_a{"i_cswitch_arr_a"};
        sc_in<sc_bv<X_DIM>> i_cswitch_arr_b{"i_cswitch_arr_b"};
        sc_in<bool> i_sa_clear_a{"i_sa_clear_a"};
        sc_in<bool> i_sa_clear_b{"i_sa_clear_b"};
        sc_in<uint32_t> i_context_id_a{"i_context_id_a"};
        sc_in<uint32_t> i_context_id_b{"i_context_id_b"};

        PeConfig config;

#ifndef FX1_NO_PERF
        fx1::PerfCounters *perf{nullptr};
#endif

        void dump_mac_matrix(uint32_t context, const std::string &tag, uint32_t start_y, uint32_t end_y)
        {
            static std::ofstream f("trace_sysc/sa_macq_dump.csv");
            static bool header_written = false;
            if (!header_written)
            {
                f << "tag,context,y,x,mac_q,mac_sc_q\n";
                header_written = true;
            }

            for (uint32_t y = start_y; y < end_y && y < Y_DIM; y++)
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
            f.flush();
        }

        void dump_mac_cell(uint32_t context, int y, int x, const std::string &tag)
        {
            static std::ofstream f("trace_sysc/sa_macq_cell_dump.csv");
            static bool header_written = false;
            if (!header_written)
            {
                f << "tag,context,y,x,mac_q,mac_sc_q,time\n";
                header_written = true;
            }

            f << tag << ","
              << context << ","
              << y << ","
              << x << ","
              << static_cast<int64_t>(grid[y][x].mac_q) << ","
              << static_cast<int64_t>(grid[y][x].mac_sc_q) << ","
              << sc_time_stamp()
              << "\n";
            f.flush();
        }

        SC_HAS_PROCESS(SystolicArray);
        SystolicArray(sc_module_name nm, const PeConfig &cfg = PeConfig())
            : sc_module(nm), config(cfg)
        {
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

        void reset_lane_a(uint32_t nsplit)
        {
            raw_cswitch_q_a = false;
            cswitch_context_latched_a = 0;

            int mul_lat = config.stages_mul + (config.intermediate_pipeline_stage ? 1 : 0);

            for (uint32_t y = 0; y < nsplit; y++)
            {
                for (int x = 0; x < X_DIM; x++)
                {
                    local_cswitch_q[y][x] = false;
                    grid[y][x].reset();
                    int delay_len = y + x + 1 + mul_lat + CSWITCH_EXTRA_MARGIN + (config.extra_csreg ? 1 : 0);
                    cs_delay[y][x].assign(delay_len, false);
                }
            }
        }

        void reset_lane_b(uint32_t nsplit)
        {
            raw_cswitch_q_b = false;
            cswitch_context_latched_b = 0;

            int mul_lat = config.stages_mul + (config.intermediate_pipeline_stage ? 1 : 0);

            for (uint32_t y = nsplit; y < Y_DIM; y++)
            {
                for (int x = 0; x < X_DIM; x++)
                {
                    local_cswitch_q[y][x] = false;
                    grid[y][x].reset();
                    int delay_len = (y - nsplit) + x + 1 + mul_lat + CSWITCH_EXTRA_MARGIN + (config.extra_csreg ? 1 : 0);
                    cs_delay[y][x].assign(delay_len, false);
                }
            }
        }

        void grid_process()
        {
            uint32_t nsplit = i_nsplit.read();
            if (nsplit > Y_DIM) nsplit = Y_DIM;

            if (!i_rstn.read())
            {
                reset_lane_a(nsplit);
                reset_lane_b(nsplit);
                o_c_arr_a.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_c_arr_b.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            if (i_sa_clear_a.read())
            {
                reset_lane_a(nsplit);
            }
            if (i_sa_clear_b.read())
            {
                reset_lane_b(nsplit);
            }

            act_vector_t<Y_DIM, T_ACT> act_in_a = i_act_arr_a.read();
            act_vector_t<Y_DIM, T_ACT> act_in_b = i_act_arr_b.read();
            wei_vector_t<X_DIM, T_WEI> wei_in_a = i_wei_arr_a.read();
            wei_vector_t<X_DIM, T_WEI> wei_in_b = i_wei_arr_b.read();
            psum_vector_t<Y_DIM, T_PSUM> scan_in_a = i_c_arr_a.read();
            psum_vector_t<Y_DIM, T_PSUM> scan_in_b = i_c_arr_b.read();
            sc_bv<X_DIM> cswitch_a = i_cswitch_arr_a.read();
            sc_bv<X_DIM> cswitch_b = i_cswitch_arr_b.read();

            // Detect cswitch edge
            bool raw_cswitch_any_a = false;
            for (int x = 0; x < X_DIM; x++)
            {
                if (cswitch_a[x].to_bool())
                {
                    raw_cswitch_any_a = true;
                    break;
                }
            }
            if (raw_cswitch_any_a && !raw_cswitch_q_a)
            {
                cswitch_context_latched_a = i_context_id_a.read();
                dump_mac_matrix(cswitch_context_latched_a, "before_raw_cswitch_A", 0, nsplit);
            }
            raw_cswitch_q_a = raw_cswitch_any_a;

            bool raw_cswitch_any_b = false;
            for (int x = 0; x < X_DIM; x++)
            {
                if (cswitch_b[x].to_bool())
                {
                    raw_cswitch_any_b = true;
                    break;
                }
            }
            if (raw_cswitch_any_b && !raw_cswitch_q_b)
            {
                cswitch_context_latched_b = i_context_id_b.read();
                dump_mac_matrix(cswitch_context_latched_b, "before_raw_cswitch_B", nsplit, Y_DIM);
            }
            raw_cswitch_q_b = raw_cswitch_any_b;

            // Capture pipeline state snapshot
            for (int y = 0; y < Y_DIM; y++)
            {
                for (int x = 0; x < X_DIM; x++)
                {
                    prev_a[y][x] = grid[y][x].a_q;
                    prev_b[y][x] = grid[y][x].b_q;
                    prev_sc[y][x] = grid[y][x].mac_sc_q;
                }
            }

            psum_vector_t<Y_DIM, T_PSUM> scan_out_a;
            psum_vector_t<Y_DIM, T_PSUM> scan_out_b;

            // Step PEs individually
            for (int y = 0; y < Y_DIM; y++)
            {
                bool pe_pipeline_en = (y < (int)nsplit) ? i_pipeline_en_a.read() : i_pipeline_en_b.read();
                
                // If this row's lane pipeline is disabled, hold state and do not step
                if (!pe_pipeline_en)
                {
                    if (y < (int)nsplit)
                    {
                        scan_out_a[y] = grid[y][0].mac_sc_q;
                    }
                    else
                    {
                        scan_out_b[y] = grid[y][0].mac_sc_q;
                    }
                    continue;
                }

                for (int x = 0; x < X_DIM; x++)
                {
                    // Data propagation
                    T_ACT a_val = (x == 0) ? ((y < (int)nsplit) ? act_in_a[y] : act_in_b[y]) : prev_a[y][x - 1];
                    
                    T_WEI b_val;
                    if (y < (int)nsplit)
                    {
                        if (y == 0)
                        {
                            b_val = wei_in_a[x];
                        }
                        else
                        {
                            b_val = prev_b[y - 1][x];
                        }
                    }
                    else
                    {
                        if (y == (int)nsplit)
                        {
                            b_val = wei_in_b[x];
                        }
                        else
                        {
                            b_val = prev_b[y - 1][x];
                        }
                    }

                    T_PSUM c_val = (x == X_DIM - 1) ? ((y < (int)nsplit) ? scan_in_a[y] : scan_in_b[y]) : prev_sc[y][x + 1];

                    // Context switch scheduling
                    bool cs_in = (y < (int)nsplit) ? cswitch_a[x].to_bool() : cswitch_b[x].to_bool();
                    cs_delay[y][x].push_back(cs_in);
                    bool cs_bit = cs_delay[y][x].front();
                    cs_delay[y][x].erase(cs_delay[y][x].begin());

                    bool local_cswitch_rise = cs_bit && !local_cswitch_q[y][x];
                    local_cswitch_q[y][x] = cs_bit;
                    
                    bool pe_cscan_en = (y < (int)nsplit) ? 
                        (i_cscan_en_a.read() && !local_cswitch_rise) : 
                        (i_cscan_en_b.read() && !local_cswitch_rise);

                    // Execute PE execution step
                    grid[y][x].step(a_val, b_val, c_val, local_cswitch_rise,
                                    pe_cscan_en, pe_pipeline_en, i_threshold.read());

                    if (local_cswitch_rise)
                    {
                        uint32_t ctx = (y < (int)nsplit) ? cswitch_context_latched_a : cswitch_context_latched_b;
                        dump_mac_cell(ctx, y, x, "after_local_cswitch");
                    }

                    if (x == 0)
                    {
                        if (y < (int)nsplit)
                        {
                            scan_out_a[y] = grid[y][0].mac_sc_q;
                        }
                        else
                        {
                            scan_out_b[y] = grid[y][0].mac_sc_q;
                        }
                    }

#ifndef FX1_NO_PERF
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
            if (perf && (i_pipeline_en_a.read() || i_pipeline_en_b.read()))
            {
                perf->exec_cycles++;
                perf->sa_cycles++;
                perf->processing_cycles++;
                perf->mac_engine_cycles++;
            }
#endif

            o_c_arr_a.write(scan_out_a);
            o_c_arr_b.write(scan_out_b);
        }

        T_PSUM get_pe_mac(int y, int x) const
        {
            return grid[y][x].mac_q;
        }

        T_PSUM get_pe_mac_sc(int y, int x) const
        {
            return grid[y][x].mac_sc_q;
        }

    private:
        ProcessingElement<T_ACT, T_WEI, T_PSUM> grid[Y_DIM][X_DIM];
        std::vector<bool> cs_delay[Y_DIM][X_DIM];

        T_ACT prev_a[Y_DIM][X_DIM];
        T_WEI prev_b[Y_DIM][X_DIM];
        T_PSUM prev_sc[Y_DIM][X_DIM];

        bool raw_cswitch_q_a{false};
        bool raw_cswitch_q_b{false};
        bool local_cswitch_q[Y_DIM][X_DIM];
        uint32_t cswitch_context_latched_a{0};
        uint32_t cswitch_context_latched_b{0};
    };
} // namespace sauria

#endif // SAURIA_SA_ARRAY_H
