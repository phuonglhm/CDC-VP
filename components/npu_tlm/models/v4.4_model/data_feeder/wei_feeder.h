// SystemC Model for SAURIA NPU Core
// Weight Feeder Block with Parameterized FIFO Depth

#ifndef SAURIA_WEI_FEEDER_H
#define SAURIA_WEI_FEEDER_H

#include "sauria_types.h"
#include "debug.h"
#include <queue>
#include <vector>
#include <fstream>
#include <string>

namespace sauria
{

    template <
        int X_DIM = 32,
        typename T_WEI = float,
        int SRAMB_CAP = 1024,
        int FIFO_DEPTH = 16,
        // With the wavefront skew working correctly (triangular fill on both
        // feeders), A and B already enter the array in phase from t=0, so no
        // extra static weight delay is needed. Kept as a tunable knob: if a
        // residual A-vs-B offset is measured, set this to that offset.
        int WEI_POP_DELAY = 0,
        bool IS_LANE_B = false>
    class WeightFeeder : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Control Inputs from Global FSM
        sc_in<bool> i_feeder_en{"i_feeder_en"};
        sc_in<bool> i_feeder_clear{"i_feeder_clear"};
        sc_in<bool> i_start{"i_start"};
        sc_in<bool> i_valid{"i_valid"};
        sc_in<bool> i_finalpush{"i_finalpush"};
        sc_in<bool> i_cnt_en{"i_cnt_en"};
        sc_in<bool> i_cnt_clear{"i_cnt_clear"};
        sc_in<bool> i_clearfifo{"i_clearfifo"};
        sc_in<bool> i_pop_en{"i_pop_en"};
        sc_in<bool> i_cswitch{"i_cswitch"};

        // Config Parameters
        sc_in<uint32_t> i_wei_incntlim{"i_wei_incntlim"};
        sc_in<uint32_t> i_wei_incntstep{"i_wei_incntstep"};
        sc_in<uint32_t> i_wei_base_addr{"i_wei_base_addr"};
        // Full SAURIA WEIGHT address-generator runtime config
        sc_in<uint32_t> i_wei_wlim{"i_wei_wlim"};
        sc_in<uint32_t> i_wei_wstep{"i_wei_wstep"};
        sc_in<uint32_t> i_wei_klim{"i_wei_klim"};
        sc_in<uint32_t> i_wei_kstep{"i_wei_kstep"};
        sc_in<uint32_t> i_wei_til_klim{"i_wei_til_klim"};
        sc_in<uint32_t> i_wei_til_kstep{"i_wei_til_kstep"};
        sc_in<uint32_t> i_wei_cols_active{"i_wei_cols_active"};
        sc_in<uint32_t> i_wei_waligned{"i_wei_waligned"};
        sc_in<uint32_t> i_context_id{"i_context_id"};
        sc_in<uint32_t> i_ncontexts{"i_ncontexts"};
        sc_in<uint32_t> i_out_tile_id{"i_out_tile_id"};
        sc_in<uint32_t> i_mvm_k{"i_mvm_k"};
        sc_in<uint32_t> i_nsplit{"i_nsplit"};

        // Memory Interface to SRAM B
        sc_out<uint32_t> o_sramb_addr{"o_sramb_addr"};
        sc_out<bool> o_sramb_rden{"o_sramb_rden"};
        sc_in<wei_vector_t<X_DIM, T_WEI>> i_sramb_data{"i_sramb_data"};

        // Wavefront Output Vector towards Systolic Array (B ports)
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_wei_arr{"o_wei_arr"};

        // Feeder Status Outputs
        sc_out<bool> o_wei_done{"o_wei_done"};
        sc_out<bool> o_wei_til_done{"o_wei_til_done"};
        sc_out<bool> o_fifo_empty{"o_fifo_empty"};
        sc_out<bool> o_fifo_full{"o_fifo_full"};
        sc_out<bool> o_stall{"o_stall"};

        SC_CTOR(WeightFeeder)
        {
            SC_METHOD(feeder_process);
            sensitive << i_clk.pos();
        }

    private:
        // Weight queues for each of the X columns
        std::queue<T_WEI> col_fifos[X_DIM];
        std::vector<T_WEI> skew_regs[X_DIM];

        // Address tracking register
        uint32_t addr_reg{0};
        uint32_t incnt{0};
        // SAURIA-style WEIGHT address generator counters
        uint32_t wei_w_cnt{0};
        uint32_t wei_k_cnt{0};
        uint32_t wei_til_k_cnt{0};
        uint32_t wei_pop_count{0};

        bool delay_first_weight_pop{false};

        uint32_t wei_out_count{0};
        bool wei_out_started{false};

        // Static A/B phase-compensation counter (see WEI_POP_DELAY).
        // Counts down zero-output cycles before the first real weight pop.
        // Armed once per context when pop starts and the FIFO has real data.
        uint32_t wei_pop_delay_cnt{0};
        bool wei_pop_delay_armed{false};

        // Debug/functional SAURIA weight stream reconstruction.
        // SRAMB flat layout is treated as flat[x*K + k].
        // SA logical stream needs B[k][x].
        std::vector<T_WEI> wei_flat_buf;

        bool wei_stream_init{false};

        uint32_t wei_k_len{0};
        uint32_t wei_total_elems{0};
        uint32_t wei_total_words{0};

        uint32_t wei_word_req_idx{0};
        uint32_t wei_emit_count{0};

        std::queue<uint32_t> pending_word_addr_q;

        // Detect rising edge of cnt_clear
        bool cnt_clear_q{false};

        // SRAM read latency matching registers
        bool rden_q1{false};
        bool rden_q2{false};
        bool rdata_valid{false};

        bool use_sauria_weight_addr_gen()
        {
            return (
                i_wei_wlim.read() != 0 &&
                i_wei_wstep.read() != 0);
        }

        void advance_sauria_weight_addr_gen()
        {
            uint32_t next_w = wei_w_cnt + i_wei_wstep.read();

            if (next_w < i_wei_wlim.read())
            {
                wei_w_cnt = next_w;
                return;
            }

            wei_w_cnt = 0;

            uint32_t next_k = wei_k_cnt + i_wei_kstep.read();

            if (i_wei_klim.read() != 0 && next_k < i_wei_klim.read())
            {
                wei_k_cnt = next_k;
                return;
            }

            wei_k_cnt = 0;

            uint32_t next_til_k = wei_til_k_cnt + i_wei_til_kstep.read();

            if (i_wei_til_klim.read() != 0 && next_til_k < i_wei_til_klim.read())
            {
                wei_til_k_cnt = next_til_k;
            }
            else
            {
                wei_til_k_cnt = 0;
            }
        }
        uint32_t get_effective_k() const
        {
            uint32_t k = i_mvm_k.read();

            if (k != 0)
            {
                return k;
            }

            uint32_t total_elems = i_wei_incntlim.read();

            if (total_elems == 0)
            {
                total_elems = i_wei_wlim.read();
            }

            if (total_elems != 0)
            {
                k = total_elems / X_DIM;
            }

            if (k == 0)
            {
                k = 1;
            }

            return k;
        }
        void feeder_process()
        {
            if (!i_rstn.read() || i_feeder_clear.read() || i_clearfifo.read())
            {
                o_sramb_addr.write(0);
                o_sramb_rden.write(false);
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                o_wei_done.write(false);
                o_wei_til_done.write(false);
                o_fifo_empty.write(true);
                o_fifo_full.write(false);
                o_stall.write(false);
                addr_reg = 0;
                incnt = 0;
                wei_w_cnt = 0;
                wei_k_cnt = 0;
                wei_til_k_cnt = 0;
                wei_pop_count = 0;

                cnt_clear_q = false;
                rden_q1 = false;
                rden_q2 = false;

                delay_first_weight_pop = false;
                wei_pop_delay_armed = false;
                wei_pop_delay_cnt = 0;

                wei_flat_buf.clear();

                wei_stream_init = false;
                wei_k_len = 0;
                wei_total_elems = 0;
                wei_total_words = 0;
                wei_word_req_idx = 0;
                wei_emit_count = 0;

                wei_out_count = 0;
                wei_out_started = false;

                while (!pending_word_addr_q.empty())
                    pending_word_addr_q.pop();
                for (int i = 0; i < X_DIM; i++)
                {
                    while (!col_fifos[i].empty())
                        col_fifos[i].pop();

                    // RTL feed_registers: WEIGHT lane x is delayed by x cycles.
                    skew_regs[i].assign(i, static_cast<T_WEI>(0));
                }
            }

            if (!i_feeder_en.read())
            {
                o_sramb_rden.write(false);
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
                rden_q1 = false;
                rden_q2 = false;
                return;
            }

            // Default SRAM read disable each cycle
            o_sramb_rden.write(false);

            // cnt_clear should behave like a pulse.
            // If controller holds it high for many cycles, feeder only clears once.
            bool cnt_clear_now = i_cnt_clear.read();
            bool cnt_clear_pulse = cnt_clear_now && !cnt_clear_q;
            cnt_clear_q = cnt_clear_now;

            if (cnt_clear_pulse)
            {
                addr_reg = 0;
                incnt = 0;

                wei_w_cnt = 0;
                wei_k_cnt = 0;
                wei_til_k_cnt = 0;

                rden_q1 = false;
                rden_q2 = false;

                wei_flat_buf.clear();
                delay_first_weight_pop = false;
                wei_pop_delay_armed = false;
                wei_pop_delay_cnt = 0;
                wei_stream_init = false;
                wei_k_len = 0;
                wei_total_elems = 0;
                wei_total_words = 0;
                wei_word_req_idx = 0;
                wei_emit_count = 0;
                wei_pop_count = 0;

                wei_out_count = 0;
                wei_out_started = false;

                while (!pending_word_addr_q.empty())
                    pending_word_addr_q.pop();

                for (int i = 0; i < X_DIM; i++)
                {
                    while (!col_fifos[i].empty())
                        col_fifos[i].pop();

                    skew_regs[i].assign(i, static_cast<T_WEI>(0));
                }

                static int dbg_weight_clear_count = 0;
                if (dbg_weight_clear_count < 16)
                {
                    DBG_COUT << "[WEIGHT CLEAR PULSE]"
                              << " feeder_en=" << i_feeder_en.read()
                              << " cnt_en=" << i_cnt_en.read()
                              << " counters reset to 0"
                              << std::endl;
                }
                dbg_weight_clear_count++;

                // return;
            }

            // SRAM read latency matching.
            // Data is valid one cycle after rden.
            bool mem_data_valid = rden_q2;

            // Advance read-valid pipeline.
            // This matches the SRAM latency behavior we saw in IFMAP.
            rden_q2 = rden_q1;
            rden_q1 = false;

            if (mem_data_valid)
            {
                wei_vector_t<X_DIM, T_WEI> mem_data = i_sramb_data.read();

                uint32_t use_word_addr = 0;

                if (!pending_word_addr_q.empty())
                {
                    use_word_addr = pending_word_addr_q.front();
                    pending_word_addr_q.pop();
                }

                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream wei_raw_trace("trace_sysc/weight_sram_flat.csv");
                    static bool wei_raw_header = false;
                    static uint32_t wei_raw_count = 0;

                    if (!wei_raw_header)
                    {
                        wei_raw_trace << "idx,word_addr,lane,value\n";
                        wei_raw_header = true;
                    }

                    for (int x = 0; x < X_DIM; x++)
                    {
                        wei_raw_trace
                            << wei_raw_count << ","
                            << use_word_addr << ","
                            << x << ","
                            << static_cast<int32_t>(mem_data[x])
                            << "\n";

                        wei_raw_count++;
                    }

                    wei_raw_trace.flush();
                }

                // Append one SRAMB word into flat weight buffer.
                for (int x = 0; x < X_DIM; x++)
                {
                    wei_flat_buf.push_back(mem_data[x]);
                }
                static std::ofstream raw_wei_trace("trace_sysc/weight_raw_buf.csv");
                static bool raw_header = false;
                static uint32_t raw_count = 0;

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    if (!raw_header)
                    {
                        raw_wei_trace << "context,out_tile,word_addr,seq_word";
                        for (int x = 0; x < X_DIM; x++)
                        {
                            raw_wei_trace << ",raw" << x;
                        }
                        raw_wei_trace << "\n";
                        raw_header = true;
                    }

                    raw_wei_trace << i_context_id.read()
                                  << "," << i_out_tile_id.read()
                                  << "," << use_word_addr
                                  << "," << raw_count;

                    for (int x = 0; x < X_DIM; x++)
                    {
                        raw_wei_trace << "," << static_cast<int32_t>(mem_data[x]);
                    }

                    raw_wei_trace << "\n";
                    raw_wei_trace.flush();

                    raw_count++;
                }
                static int dbg_wei_mem_word_count = 0;
                if (inst_name.find("NpuTop_std") != std::string::npos &&
                    dbg_wei_mem_word_count < 32)
                {
                    DBG_COUT << "[WEIGHT MEM WORD]"
                              << " count=" << dbg_wei_mem_word_count
                              << " word_addr=" << use_word_addr
                              << " data=[";

                    for (int x = 0; x < X_DIM; x++)
                    {
                        DBG_COUT << static_cast<int32_t>(mem_data[x]);
                        if (x < X_DIM - 1)
                            DBG_COUT << ", ";
                    }

                    DBG_COUT << "]" << std::endl;
                }

                dbg_wei_mem_word_count++;

                // Emit logical B_Mat_mvm vectors once enough flat data is available.
                // Layout weight follow row-major
                //   flat[k * X_DIM + x]
                // if (wei_stream_init && wei_k_len != 0)
                // {
                //     if (wei_emit_count < wei_k_len)
                //     {
                //         bool enough_data = true;

                //         for (int x = 0; x < X_DIM; x++)
                //         {
                //             uint32_t idx = wei_emit_count * X_DIM + x;

                //             if (idx >= wei_flat_buf.size())
                //             {
                //                 enough_data = false;
                //                 break;
                //             }
                //         }

                //         if (enough_data)
                //         {
                //             wei_vector_t<X_DIM, T_WEI> feed_vec;

                //             for (int x = 0; x < X_DIM; x++)
                //             {
                //                 uint32_t idx = wei_emit_count * X_DIM + x;
                //                 T_WEI v = wei_flat_buf[idx];

                //                 feed_vec[x] = v;
                //                 col_fifos[x].push(v);
                //             }

                //             static int dbg_wei_feed_count = 0;

                //             if (inst_name.find("NpuTop_std") != std::string::npos &&
                //                 dbg_wei_feed_count < 64)
                //             {
                //                 DBG_COUT << "[WEIGHT FEED VEC]"
                //                           << " count=" << dbg_wei_feed_count
                //                           << " k=" << wei_emit_count
                //                           << " feed_vec=[";

                //                 for (int x = 0; x < X_DIM; x++)
                //                 {
                //                     DBG_COUT << static_cast<int32_t>(feed_vec[x]);
                //                     if (x < X_DIM - 1)
                //                         DBG_COUT << ", ";
                //                 }

                //                 DBG_COUT << "]" << std::endl;
                //             }

                //             dbg_wei_feed_count++;
                //             wei_emit_count++;
                //         }
                //     }
                // }
            }

            // ---------------------------------------------------------
            // Emit one WEIGHT logical vector per cycle when enough data
            // is available in wei_flat_buf.
            //
            // Verified SAURIA B_Mat_mvm layout:
            //
            //     B[k][x] = flat[k * X_DIM + x]
            //
            // Do NOT use:
            //     flat[x * wei_k_len + k]
            //
            // That transpose layout is wrong for the current SAURIA test.
            // ---------------------------------------------------------
            if (wei_stream_init && wei_k_len != 0)
            {
                if (wei_emit_count < wei_k_len)
                {
                    bool enough_data = true;

                    for (int x = 0; x < X_DIM; x++)
                    {
                        uint32_t idx =
                            wei_emit_count * X_DIM + static_cast<uint32_t>(x);

                        if (idx >= wei_flat_buf.size())
                        {
                            enough_data = false;
                            break;
                        }
                    }

                    if (enough_data)
                    {
                        wei_vector_t<X_DIM, T_WEI> feed_vec;

                        for (int x = 0; x < X_DIM; x++)
                        {
                            uint32_t idx =
                                wei_emit_count * X_DIM + static_cast<uint32_t>(x);

                            T_WEI v = static_cast<T_WEI>(0);

                            if (idx < wei_flat_buf.size())
                            {
                                v = wei_flat_buf[idx];
                            }

                            feed_vec[x] = v;
                            col_fifos[x].push(v);
                        }

                        std::string inst_name = this->name();

                        static int dbg_wei_feed_count = 0;

                        if (inst_name.find("NpuTop_std") != std::string::npos &&
                            dbg_wei_feed_count < 64)
                        {
                            DBG_COUT << "[WEIGHT FEED VEC]"
                                      << " count=" << dbg_wei_feed_count
                                      << " context=" << i_context_id.read()
                                      << " out_tile=" << i_out_tile_id.read()
                                      << " k=" << wei_emit_count
                                      << " flat_base_idx=" << (wei_emit_count * X_DIM)
                                      << " feed_vec=[";

                            for (int x = 0; x < X_DIM; x++)
                            {
                                DBG_COUT << static_cast<int32_t>(feed_vec[x]);

                                if (x < X_DIM - 1)
                                {
                                    DBG_COUT << ", ";
                                }
                            }

                            DBG_COUT << "]" << std::endl;

                            dbg_wei_feed_count++;
                        }

                        wei_emit_count++;
                    }
                }
            }

            if (i_cnt_en.read())
            {
                bool within_limit = (incnt < i_wei_incntlim.read());

                if (within_limit)
                {
                    bool sauria_weight_mode = use_sauria_weight_addr_gen();

                    if (sauria_weight_mode)
                    {
                        if (!wei_stream_init)
                        {
                            // Current test:
                            //   wei_incntlim = 9216 total elements
                            //   X_DIM = 16
                            //   K = 9216 / 16 = 576
                            wei_k_len = get_effective_k();

                            wei_total_elems = wei_k_len * X_DIM;

                            wei_total_words = wei_k_len;

                            wei_word_req_idx = 0;
                            wei_emit_count = 0;
                            wei_flat_buf.clear();

                            while (!pending_word_addr_q.empty())
                                pending_word_addr_q.pop();

                            wei_stream_init = true;

                            // Arm the static A/B phase-compensation delay for
                            // this context. Applied uniformly every context.
                            wei_pop_delay_armed = true;
                            wei_pop_delay_cnt = (uint32_t)WEI_POP_DELAY;

                            DBG_COUT << "[WEIGHT STREAM INIT]"
                                      << "time=" << sc_time_stamp()
                                      << " context=" << i_context_id.read()
                                      << " total_elems=" << wei_total_elems
                                      << " k_len=" << wei_k_len
                                      << " total_words=" << wei_total_words
                                      << std::endl;

                            // NOTE: delay_first_weight_pop removed.
                            // The per-context phase skew between A and B is now
                            // handled by the controller ARRAY_FILL phase, which
                            // pre-fills BOTH feeder FIFOs in lock-step before any
                            // pop. A weight-only delay here would re-introduce a
                            // B-vs-A phase offset (the +3 cycle CTX>0 error).
                        }

                        if (wei_word_req_idx < wei_total_words)
                        {
                            // uint32_t words_per_out_tile = 0;

                            // if (i_wei_incntlim.read() != 0)
                            // {
                            //     words_per_out_tile = i_wei_incntlim.read() / X_DIM;
                            // }

                            // if (words_per_out_tile == 0)
                            // {
                            //     words_per_out_tile = wei_total_words;
                            // }

                            uint32_t words_per_out_tile = wei_k_len;
                            // MULTI-TILE FIX: output_tiles are folded into ncontexts
                            // (= Ch x n_tiles); i_out_tile_id stays 0. Derive the cout
                            // group from the context: n_tiles = til_klim/til_kstep,
                            // ctx_per_tile = ncontexts/n_tiles, cout_group =
                            // context_id/ctx_per_tile. Single-tile (n_tiles=1) => 0.
                            uint32_t wt_ntiles = (i_wei_til_kstep.read() != 0) ? (i_wei_til_klim.read() / i_wei_til_kstep.read()) : 1;
                            if (wt_ntiles == 0) wt_ntiles = 1;
                            uint32_t wt_nc = i_ncontexts.read() ? i_ncontexts.read() : 1;
                            uint32_t wt_cpt = (wt_ntiles != 0) ? (wt_nc / wt_ntiles) : wt_nc;
                            if (wt_cpt == 0) wt_cpt = 1;
                            uint32_t wt_group = i_context_id.read() / wt_cpt;
                            (void)words_per_out_tile;
                            // Weights for the n_tiles cout groups are INTERLEAVED per
                            // k-element in SRAM B (word 0=k0g0, 1=k0g1, 2=k1g0, ...).
                            // So group g reads words g, g+n_tiles, g+2*n_tiles, ...
                            uint32_t req_word_addr = (i_out_tile_id.read() + wei_word_req_idx * wt_ntiles) + wt_group;
                            uint32_t req_final_addr = i_wei_base_addr.read() + req_word_addr;
                            pending_word_addr_q.push(req_word_addr);

                            o_sramb_rden.write(true);
                            o_sramb_addr.write(req_final_addr);

                            rden_q1 = true;

                            static int dbg_wei_req_count = 0;
                            std::string inst_name = this->name();

                            if (inst_name.find("NpuTop_std") != std::string::npos &&
                                dbg_wei_req_count < 64)
                            {
                                DBG_COUT << "[WEIGHT WORD REQ]"
                                          << " req=" << dbg_wei_req_count
                                          << " word_addr=" << req_word_addr
                                          << " final_addr=" << req_final_addr
                                          << " total_words=" << wei_total_words
                                          << std::endl;
                            }

                            dbg_wei_req_count++;

                            wei_word_req_idx++;
                        }
                        else
                        {
                            o_sramb_rden.write(false);
                        }
                    }
                    else
                    {
                        uint32_t final_addr =
                            i_wei_base_addr.read() + addr_reg;

                        o_sramb_rden.write(true);
                        o_sramb_addr.write(final_addr);

                        addr_reg += i_wei_incntstep.read();

                        rden_q1 = true;
                    }

                    incnt++;
                }
                // else
                // {
                //     o_sramb_rden.write(false);
                // }
            }
            else
            {
                o_sramb_rden.write(false);
                rden_q1 = false;
            }

            bool wei_can_pop = true;
            for (int x = 0; x < X_DIM; x++)
            {
                if (col_fifos[x].empty())
                {
                    wei_can_pop = false;
                    break;
                }
            }
            bool wei_need_more = i_feeder_en.read() && (!wei_stream_init || (wei_pop_count < wei_k_len));
            o_stall.write(wei_need_more && !wei_can_pop);

            // ---------------------------------------------------------
            // Pop/shift WEIGHT physical stream.
            //
            // After K logical weight vectors are popped, col_fifos become empty,
            // but skew_regs[x] still contain delayed tail values for x > 0.
            // During DRAIN_FEED, keep shifting zeros into skew_regs to flush tail.
            // ---------------------------------------------------------
            if (i_pop_en.read())
            {
                wei_vector_t<X_DIM, T_WEI> popped_vec;
                wei_vector_t<X_DIM, T_WEI> wei_out;

                bool has_logical_vector =
                    (wei_pop_count < wei_k_len);

                for (int x = 0; x < X_DIM; x++)
                {
                    if (col_fifos[x].empty())
                    {
                        has_logical_vector = false;
                        break;
                    }
                }

                bool had_real_vector = has_logical_vector;

                for (int x = 0; x < X_DIM; x++)
                {
                    T_WEI popped = static_cast<T_WEI>(0);

                    if (has_logical_vector)
                    {
                        popped = col_fifos[x].front();
                        col_fifos[x].pop();
                    }

                    popped_vec[x] = popped;

                    if (x == 0)
                    {
                        wei_out[x] = popped;
                    }
                    else
                    {
                        // Keep exactly x-cycle skew.
                        while (skew_regs[x].size() < static_cast<size_t>(x))
                        {
                            skew_regs[x].insert(
                                skew_regs[x].begin(),
                                static_cast<T_WEI>(0));
                        }

                        // This push must happen even during tail flush, with popped=0.
                        skew_regs[x].push_back(popped);

                        wei_out[x] = skew_regs[x].front();

                        skew_regs[x].erase(skew_regs[x].begin());
                    }
                }

                o_wei_arr.write(wei_out);

                // Dump only real logical vectors, not tail flush zeros.
                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream wei_pop_trace("trace_sysc/weight_pop_logical.csv");
                    static bool wei_pop_header = false;
                    static uint32_t dbg_wei_rows = 0;

                    if (!wei_pop_header)
                    {
                        wei_pop_trace << "context,out_tile,k";

                        for (int xx = 0; xx < X_DIM; xx++)
                        {
                            wei_pop_trace << ",wei" << xx;
                        }

                        wei_pop_trace << "\n";
                        wei_pop_header = true;
                    }

                    uint32_t trace_k = wei_pop_count;

                    if (had_real_vector && dbg_wei_rows < 8192)
                    {
                        wei_pop_trace << i_context_id.read()
                                      << "," << i_out_tile_id.read()
                                      << "," << trace_k;

                        for (int xx = 0; xx < X_DIM; xx++)
                        {
                            wei_pop_trace << ","
                                          << static_cast<int32_t>(popped_vec[xx]);
                        }

                        wei_pop_trace << "\n";
                        wei_pop_trace.flush();

                        dbg_wei_rows++;
                    }
                }

                if (had_real_vector && wei_pop_count < wei_k_len)
                {
                    wei_pop_count++;
                }
            }
            else
            {
                o_wei_arr.write(wei_vector_t<X_DIM, T_WEI>(static_cast<T_WEI>(0)));
            }

            uint32_t wei_tail_limit = wei_k_len + (X_DIM - 1);

            bool wei_pop_done = wei_stream_init && wei_out_started && (wei_out_count >= wei_tail_limit);

            o_wei_done.write(wei_pop_done);
            o_wei_til_done.write(wei_pop_done);

            static int dbg_wei_done_count = 0;
            if (this->name() && dbg_wei_done_count < 128)
            {
                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    DBG_COUT << "[WEIGHT DONE STATUS]"
                              << " context=" << i_context_id.read()
                              << " emit_count=" << wei_emit_count
                              << " pop_count=" << wei_pop_count
                              << " out_count=" << wei_out_count
                              << " k_len=" << wei_k_len
                              << " tail_limit=" << wei_tail_limit
                              << " done=" << wei_pop_done
                              << std::endl;
                }

                dbg_wei_done_count++;
            }

            // 3. Update status flags
            bool empty = col_fifos[0].empty();
            bool full = col_fifos[0].size() >= FIFO_DEPTH;
            o_fifo_empty.write(empty);
            o_fifo_full.write(full);
        }
    };

} // namespace sauria

#endif // SAURIA_WEI_FEEDER_H
