// SystemC Model for SAURIA NPU Core
// Activation (IFmap) Feeder Block with Parameterized FIFO Depth

#ifndef SAURIA_IFMAP_FEEDER_H
#define SAURIA_IFMAP_FEEDER_H

#include "sauria_types.h"
#include "debug.h"
#include <queue>
#include <vector>
#include <fstream>
#include <string>
#include <fstream>
#include <map>
#include <set>

namespace sauria
{

    template <
        int Y_DIM = 64,
        typename T_ACT = float,
        int SRAMA_CAP = 1024,
        int FIFO_DEPTH = 16,
        bool IS_LANE_B = false>
    class IfmapFeeder : public sc_module
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
        sc_in<bool> i_finalctx{"i_finalctx"};

        // Config Parameters
        sc_in<uint32_t> i_act_incntlim{"i_act_incntlim"};
        sc_in<uint32_t> i_act_incntstep{"i_act_incntstep"};
        sc_in<uint32_t> i_act_outcntlim{"i_act_outcntlim"};
        sc_in<uint32_t> i_act_outcntstep{"i_act_outcntstep"};
        sc_in<sc_bv<DILP_W>> i_act_dil_pat{"i_act_dil_pat"};
        // Full SAURIA IFMAP address-generator runtime config
        sc_in<uint32_t> i_act_xlim{"i_act_xlim"};
        sc_in<uint32_t> i_act_xstep{"i_act_xstep"};
        sc_in<uint32_t> i_act_ylim{"i_act_ylim"};
        sc_in<uint32_t> i_act_ystep{"i_act_ystep"};
        sc_in<uint32_t> i_act_chlim{"i_act_chlim"};
        sc_in<uint32_t> i_act_chstep{"i_act_chstep"};
        sc_in<uint32_t> i_act_til_xlim{"i_act_til_xlim"};
        sc_in<uint32_t> i_act_til_xstep{"i_act_til_xstep"};
        sc_in<uint32_t> i_act_til_ylim{"i_act_til_ylim"};
        sc_in<uint32_t> i_act_til_ystep{"i_act_til_ystep"};
        sc_in<uint32_t> i_context_id{"i_context_id"};
        sc_in<uint32_t> i_ncontexts{"i_ncontexts"};
        sc_in<uint32_t> i_act_reps{"i_act_reps"};
        sc_in<uint32_t> i_mvm_k{"i_mvm_k"};
        sc_in<uint32_t> i_nsplit{"i_nsplit"};
        // Memory Interface to SRAM A
        sc_out<uint32_t> o_srama_addr{"o_srama_addr"};
        sc_out<bool> o_srama_rden{"o_srama_rden"};
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_srama_data{"i_srama_data"};
        sc_in<uint32_t> i_act_base_addr{"i_act_base_addr"};

        // Wavefront Output Vector towards Systolic Array (A ports)
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_act_arr{"o_act_arr"};

        // Feeder Status Outputs
        sc_out<bool> o_act_done{"o_act_done"};
        sc_out<bool> o_act_til_done{"o_act_til_done"};
        sc_out<bool> o_fifo_empty{"o_fifo_empty"};
        sc_out<bool> o_fifo_full{"o_fifo_full"};
        sc_out<bool> o_stall{"o_stall"};

        SC_CTOR(IfmapFeeder)
        {
            SC_METHOD(feeder_process);
            sensitive << i_clk.pos();
        }

    private:
        std::vector<T_ACT> act_flat_buf;
        std::map<uint32_t, act_vector_t<Y_DIM, T_ACT>> act_word_cache;
        std::set<uint32_t> act_requested_words;

        bool act_stream_init{false};
        bool last_appended_word_valid{false};
        uint32_t last_appended_word_addr{0};

        uint32_t act_stream_base_idx{0};
        uint32_t act_word_req_idx{0};
        uint32_t act_last_word_idx{0};

        uint32_t act_emit_abs_idx{0};
        uint32_t act_emit_count{0};
        uint32_t act_pop_count{0};

        std::queue<uint32_t> pending_word_addr_q;
        std::queue<uint32_t> pending_glob_woffs_q;

        static constexpr uint32_t MEMA_N = Y_DIM;

        act_vector_t<Y_DIM, T_ACT> cur_word;
        act_vector_t<Y_DIM, T_ACT> next_word;

        bool cur_word_valid{false};
        bool next_word_valid{false};

        uint32_t pending_glob_woffs{0};
        uint32_t pending_word_addr{0};
        bool pending_read_valid{false};

        uint32_t act_req_idx{0};

        // Internal FIFOs for each of the Y rows
        std::queue<T_ACT> row_fifos[Y_DIM];

        // Dynamic skew delay registers for systolic wavefront (A-side)
        std::vector<T_ACT> skew_regs[Y_DIM];

        // Address tracking register
        uint32_t addr_reg{0};

        // local variable
        uint32_t incnt{0};
        uint32_t dil_idx{0};

        // SAURIA-style IFMAP address generator counters (5-counter, RTL convention)
        uint32_t act_x_cnt{0};
        uint32_t act_y_cnt{0};
        uint32_t act_ch_cnt{0};
        uint32_t act_tx_cnt{0}; // tiling x (output pixel x)
        uint32_t act_ty_cnt{0}; // tiling y (output pixel y)

        // SRAM read latency matching registers
        bool rden_q1{false};
        bool rden_q2{false};
        bool cnt_clear_q{false};

        bool use_sauria_ifmap_addr_gen()
        {
            return (
                i_act_xlim.read() != 0 &&
                i_act_xstep.read() != 0 &&
                i_act_ylim.read() != 0 &&
                i_act_ystep.read() != 0 &&
                i_act_chlim.read() != 0 &&
                i_act_chstep.read() != 0);
        }

        // 5-counter nested advance (RTL convention): x -> y -> ch -> til_x -> til_y.
        // Moi counter tran khi (cnt + step) >= lim -> reset 0, carry sang ngoai.
        // Verified against im2col golden in addr_gen_rtl.py.
        void advance_sauria_ifmap_addr_gen()
        {
            uint32_t next_x = act_x_cnt + i_act_xstep.read();
            if (next_x < i_act_xlim.read())
            {
                act_x_cnt = next_x;
                return;
            }
            act_x_cnt = 0;

            uint32_t next_y = act_y_cnt + i_act_ystep.read();
            if (next_y < i_act_ylim.read())
            {
                act_y_cnt = next_y;
                return;
            }
            act_y_cnt = 0;

            uint32_t next_ch = act_ch_cnt + i_act_chstep.read();
            if (next_ch < i_act_chlim.read())
            {
                act_ch_cnt = next_ch;
                return;
            }
            act_ch_cnt = 0;

            // Context boundary: 3 inner counters wrapped -> move to next output pixel
            uint32_t next_tx = act_tx_cnt + i_act_til_xstep.read();
            if (next_tx < i_act_til_xlim.read())
            {
                act_tx_cnt = next_tx;
                return;
            }
            act_tx_cnt = 0;

            uint32_t next_ty = act_ty_cnt + i_act_til_ystep.read();
            if (next_ty < i_act_til_ylim.read())
            {
                act_ty_cnt = next_ty;
                return;
            }
            act_ty_cnt = 0;
        }

        uint32_t get_effective_k() const
        {
            uint32_t k = i_mvm_k.read();

            if (k == 0)
            {
                k = i_act_incntlim.read();
            }

            if (k == 0)
            {
                k = 1;
            }

            return k;
        }
        // Local context (Ch index) within one output tile. output_tiles are folded
        // into ncontexts (= Ch x n_tiles); the activation for (tile,ch) depends only
        // on ch, so use context_id % ctx_per_tile. Single-tile (act_reps=1) => identity.
        uint32_t local_ch() const
        {
            uint32_t nt = i_act_reps.read(); if (nt == 0) nt = 1;
            uint32_t nc = i_ncontexts.read(); if (nc == 0) nc = 1;
            uint32_t cpt = nc / nt; if (cpt == 0) cpt = 1;
            return i_context_id.read() % cpt;
        }

        void derive_ifmap_mvm_params(
            uint32_t effective_k,
            uint32_t &kernel_elems,
            uint32_t &kernel_w,
            uint32_t &n_channels,
            uint32_t &kx_step) const
        {
            uint32_t chstep = i_act_chstep.read();
            uint32_t ystep = i_act_ystep.read();
            uint32_t til_ystep = i_act_til_ystep.read();

            uint32_t cfg_n_channels = 1;

            if (chstep != 0)
            {
                cfg_n_channels = i_act_chlim.read() / chstep;
            }

            if (cfg_n_channels == 0)
            {
                cfg_n_channels = 1;
            }

            // IMPORTANT:
            // kernel_elems must come from original config limit, not mvm_k.
            // Test 2:
            //   cfg_incntlim = 225
            //   cfg_n_channels = 25
            //   kernel_elems = 9
            kernel_elems = i_act_incntlim.read() / cfg_n_channels;

            if (kernel_elems == 0)
            {
                kernel_elems = 1;
            }

            kernel_w = 1;
            for (uint32_t r = 1; r * r <= kernel_elems; r++)
            {
                if (r * r == kernel_elems)
                {
                    kernel_w = r;
                }
            }

            // Test 2:
            //   ystep = 234
            //   til_ystep = 26
            //   kx_step = 9
            kx_step = 1;

            if (til_ystep != 0 && ystep != 0 && (ystep % til_ystep) == 0)
            {
                kx_step = ystep / til_ystep;
            }

            if (kx_step == 0)
            {
                kx_step = 1;
            }

            // Test 2:
            //   effective_k = 450
            //   kernel_elems = 9
            //   n_channels = 50
            n_channels = effective_k / kernel_elems;

            if (n_channels == 0)
            {
                n_channels = 1;
            }
        }

        bool calc_ifmap_word_range_for_k(
            uint32_t k,
            uint32_t effective_k,
            uint32_t &start_abs_idx,
            uint32_t &first_word,
            uint32_t &last_word,
            uint32_t &ch_id,
            uint32_t &ky,
            uint32_t &kx) const
        {
            if (k >= effective_k)
            {
                return false;
            }

            uint32_t chstep = i_act_chstep.read();
            uint32_t ystep = i_act_ystep.read();

            uint32_t kernel_elems = 1;
            uint32_t kernel_w = 1;
            uint32_t n_channels = 1;
            uint32_t kx_step = 1;

            derive_ifmap_mvm_params(
                effective_k,
                kernel_elems,
                kernel_w,
                n_channels,
                kx_step);

            if (kernel_elems == 0)
            {
                kernel_elems = 1;
            }

            if (kernel_w == 0)
            {
                kernel_w = 1;
            }

            ch_id = k / kernel_elems;

            uint32_t rem = k % kernel_elems;

            ky = rem / kernel_w;
            kx = rem % kernel_w;

            uint32_t context_y_offset =
                local_ch() * i_act_til_ystep.read();

            start_abs_idx =
                context_y_offset + ch_id * chstep + ky * ystep + kx * kx_step;

            first_word = start_abs_idx / Y_DIM;
            last_word = (start_abs_idx + Y_DIM - 1) / Y_DIM;

            return true;
        }

        void feeder_process()
        {
            if (!i_rstn.read() || i_feeder_clear.read() || i_clearfifo.read())
            {
                o_srama_addr.write(0);
                o_srama_rden.write(false);
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                o_act_done.write(false);
                o_act_til_done.write(false);
                o_fifo_empty.write(true);
                o_fifo_full.write(false);
                o_stall.write(false);
                addr_reg = 0;
                incnt = 0;
                dil_idx = 0;

                act_x_cnt = 0;
                act_y_cnt = 0;
                act_ch_cnt = 0;
                act_tx_cnt = 0;
                act_ty_cnt = 0;
                act_req_idx = 0;
                act_pop_count = 0;

                cnt_clear_q = false;
                rden_q1 = false;
                rden_q2 = false;

                last_appended_word_valid = false;
                last_appended_word_addr = 0;

                act_flat_buf.clear();
                act_word_cache.clear();
                act_requested_words.clear();

                cur_word = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                next_word = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                cur_word_valid = false;
                next_word_valid = false;
                pending_glob_woffs = 0;
                pending_word_addr = 0;
                pending_read_valid = false;

                while (!pending_word_addr_q.empty())
                    pending_word_addr_q.pop();

                while (!pending_glob_woffs_q.empty())
                    pending_glob_woffs_q.pop();

                uint32_t nsplit = i_nsplit.read();
                for (int i = 0; i < Y_DIM; i++)
                {
                    while (!row_fifos[i].empty())
                        row_fifos[i].pop();
                    int target_skew = i;
                    if (IS_LANE_B)
                    {
                        target_skew = i - (int)nsplit;
                    }
                    if (target_skew < 0)
                        target_skew = 0;
                    skew_regs[i].assign(target_skew, static_cast<T_ACT>(0)); // Delay length matches row index i
                }
                return;
            }

            if (!i_feeder_en.read())
            {
                o_srama_rden.write(false);
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
                return;
            }

            o_srama_rden.write(false);

            bool cnt_clear_now = i_cnt_clear.read();
            bool cnt_clear_pulse = cnt_clear_now && !cnt_clear_q;
            cnt_clear_q = cnt_clear_now;

            if (cnt_clear_pulse)
            {
                addr_reg = 0;
                incnt = 0;
                dil_idx = 0;

                act_x_cnt = 0;
                act_y_cnt = 0;
                act_ch_cnt = 0;
                act_tx_cnt = 0;
                act_ty_cnt = 0;
                act_req_idx = 0;

                rden_q1 = false;
                rden_q2 = false;
                o_srama_rden.write(false);

                last_appended_word_valid = false;
                last_appended_word_addr = 0;

                cur_word = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                next_word = act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0));
                cur_word_valid = false;
                next_word_valid = false;
                pending_glob_woffs = 0;
                pending_word_addr = 0;
                pending_read_valid = false;

                act_flat_buf.clear();
                act_word_cache.clear();
                act_requested_words.clear();

                act_stream_init = false;

                act_stream_base_idx = 0;
                act_word_req_idx = 0;
                act_last_word_idx = 0;

                act_emit_abs_idx = 0;
                act_emit_count = 0;
                act_pop_count = 0;

                while (!pending_word_addr_q.empty())
                    pending_word_addr_q.pop();

                while (!pending_glob_woffs_q.empty())
                    pending_glob_woffs_q.pop();

                uint32_t nsplit = i_nsplit.read();
                for (int i = 0; i < Y_DIM; i++)
                {
                    while (!row_fifos[i].empty())
                        row_fifos[i].pop();

                    int target_skew = i;
                    if (IS_LANE_B)
                    {
                        target_skew = i - (int)nsplit;
                    }
                    if (target_skew < 0)
                        target_skew = 0;
                    skew_regs[i].assign(target_skew, static_cast<T_ACT>(0));
                }
            }

            bool mem_data_valid = rden_q2;
            rden_q2 = rden_q1;
            rden_q1 = false;
            // 1. Fetch activation vector from SRAM A into FIFOs
            if (mem_data_valid)
            {
                act_vector_t<Y_DIM, T_ACT> mem_data = i_srama_data.read();

                uint32_t use_word_addr = 0;

                if (!pending_word_addr_q.empty())
                {
                    use_word_addr = pending_word_addr_q.front();
                    pending_word_addr_q.pop();
                }

                if (!pending_glob_woffs_q.empty())
                {
                    pending_glob_woffs_q.pop();
                }

                bool duplicate_word = last_appended_word_valid && (use_word_addr == last_appended_word_addr);

                act_word_cache[use_word_addr] = mem_data;
                if (!duplicate_word)
                {
                    for (int y = 0; y < Y_DIM; y++)
                    {
                        act_flat_buf.push_back(mem_data[y]);
                    }

                    // last_appended_word_valid = true;
                    // last_appended_word_addr = use_word_addr;
                }

                static int dbg_mem_word_count = 0;
                // bool dbg_word_interesting =
                //     (use_word_addr < 40) ||
                //     (use_word_addr >= 470 && use_word_addr <= 490) ||
                //     (use_word_addr >= 3350 && use_word_addr <= 3420);

                std::string inst_name = this->name();

                bool dbg_word_interesting = (use_word_addr >= 520 && use_word_addr <= 560);

                if (inst_name.find("NpuTop_std") != std::string::npos &&
                    dbg_word_interesting)
                {
                    DBG_COUT << "[IFMAP MEM WORD]"
                              << " word_addr=" << use_word_addr
                              << " data=[";

                    for (int y = 0; y < Y_DIM; y++)
                    {
                        DBG_COUT << static_cast<int32_t>(mem_data[y]);
                        if (y < Y_DIM - 1)
                            DBG_COUT << ", ";
                    }

                    DBG_COUT << "]" << std::endl;
                }

                dbg_mem_word_count++;

                // Emit all sliding vectors that are now available
                // while (act_emit_count < i_act_incntlim.read())
                // {
                //     uint32_t chstep = i_act_chstep.read();
                //     uint32_t ystep = i_act_ystep.read();

                //     uint32_t n_channels = 1;
                //     if (chstep != 0)
                //     {
                //         n_channels = i_act_chlim.read() / chstep;
                //     }

                //     if (n_channels == 0)
                //     {
                //         n_channels = 1;
                //     }

                //     uint32_t kernel_elems = i_act_incntlim.read() / n_channels;

                //     // For current SAURIA conv tests: kernel_elems = 9 => 3x3.
                //     uint32_t kernel_w = 1;
                //     for (uint32_t r = 1; r * r <= kernel_elems; r++)
                //     {
                //         if (r * r == kernel_elems)
                //         {
                //             kernel_w = r;
                //         }
                //     }

                //     uint32_t k = act_emit_count;

                //     uint32_t ch_id = k / kernel_elems;
                //     uint32_t rem = k % kernel_elems;

                //     uint32_t ky = rem / kernel_w;
                //     uint32_t kx = rem % kernel_w;

                //     uint32_t context_y_offset =
                //         local_ch() * i_act_til_ystep.read();

                //     uint32_t start_abs_idx =
                //         context_y_offset +
                //         ch_id * chstep +
                //         ky * ystep +
                //         kx;

                //     if (start_abs_idx < act_stream_base_idx)
                //     {
                //         break;
                //     }

                //     uint32_t local_start =
                //         start_abs_idx - act_stream_base_idx;

                //     if ((local_start + Y_DIM) > act_flat_buf.size())
                //     {
                //         break;
                //     }

                //     act_vector_t<Y_DIM, T_ACT> feed_vec;

                //     for (int y = 0; y < Y_DIM; y++)
                //     {
                //         T_ACT v = act_flat_buf[local_start + y];
                //         feed_vec[y] = v;
                //         row_fifos[y].push(v);
                //     }

                //     static int dbg_feed_vec_count = 0;
                //     std::string inst_name = this->name();

                //     if (inst_name.find("NpuTop_std") != std::string::npos &&
                //         dbg_feed_vec_count < 64)
                //     {
                //         DBG_COUT << "[IFMAP FEED VEC]"
                //                   << " count=" << dbg_feed_vec_count
                //                   << " ch=" << ch_id
                //                   << " ky=" << ky
                //                   << " kx=" << kx
                //                   << " start_abs_idx=" << start_abs_idx
                //                   << " local_start=" << local_start
                //                   << " feed_vec=[";

                //         for (int y = 0; y < Y_DIM; y++)
                //         {
                //             DBG_COUT << static_cast<int32_t>(feed_vec[y]);
                //             if (y < Y_DIM - 1)
                //                 DBG_COUT << ", ";
                //         }

                //         DBG_COUT << "]" << std::endl;
                //     }

                //     dbg_feed_vec_count++;
                //     act_emit_count++;
                // }
            }

            // ---------------------------------------------------------
            // Emit one IFMAP logical vector per cycle when enough data
            // is available in act_flat_buf.
            // This must be outside mem_data_valid so tail vectors can drain
            // after the final SRAM word has arrived.
            // ---------------------------------------------------------
            uint32_t effective_k = get_effective_k();

            if (act_stream_init && act_emit_count < effective_k)
            {
                uint32_t chstep = i_act_chstep.read();
                uint32_t ystep = i_act_ystep.read();

                uint32_t kernel_elems = 1;
                uint32_t kernel_w = 1;
                uint32_t n_channels = 1;
                uint32_t kx_step = 1;

                derive_ifmap_mvm_params(effective_k, kernel_elems, kernel_w, n_channels, kx_step);

                uint32_t k = act_emit_count;

                uint32_t ch_id = k / kernel_elems;
                uint32_t rem = k % kernel_elems;

                uint32_t ky = rem / kernel_w;
                uint32_t kx = rem % kernel_w;

                uint32_t context_y_offset = local_ch() * i_act_til_ystep.read();
                uint32_t start_abs_idx = context_y_offset + ch_id * chstep + ky * ystep + kx * kx_step;

                if (start_abs_idx >= act_stream_base_idx)
                {
                    // STRIDE FIX: each Y-row is one output-W position; for a strided
                    // conv adjacent output positions are s input-columns apart
                    // (s=1 for GeMM/1x1). Derive s = til_ystep/ystep (holds for d=1).
                    // Only the first active_rows (= til_xstep/s = Y_used) lanes are
                    // real outputs; require words only for that active span.
                    uint32_t s_ys  = i_act_ystep.read();
                    uint32_t s_tys = i_act_til_ystep.read();
                    uint32_t s_row = 1;
                    if (s_ys != 0 && s_tys != 0 && (s_tys % s_ys) == 0)
                        s_row = s_tys / s_ys;
                    if (s_row == 0)
                        s_row = 1;
                    uint32_t s_txstep = i_act_til_xstep.read();
                    uint32_t active_rows = (s_txstep != 0) ? (s_txstep / s_row) : (uint32_t)Y_DIM;
                    if (active_rows == 0 || active_rows > (uint32_t)Y_DIM)
                        active_rows = (uint32_t)Y_DIM;

                    uint32_t first_word = start_abs_idx / Y_DIM;
                    uint32_t last_word = (start_abs_idx + (active_rows - 1) * s_row) / Y_DIM;

                    bool enough_data = true;
                    for (uint32_t w = first_word; w <= last_word; w++)
                    {
                        if (act_word_cache.find(w) == act_word_cache.end())
                        {
                            enough_data = false;
                            break;
                        }
                    }

                    if (enough_data)
                    {
                        act_vector_t<Y_DIM, T_ACT> feed_vec;

                        for (int y = 0; y < Y_DIM; y++)
                        {
                            uint32_t abs_idx = ((uint32_t)y < active_rows) ? (start_abs_idx + y * s_row) : start_abs_idx;
                            uint32_t word_id = abs_idx / Y_DIM;
                            uint32_t lane_id = abs_idx % Y_DIM;

                            T_ACT v = act_word_cache[word_id][lane_id];

                            feed_vec[y] = v;
                            row_fifos[y].push(v);
                        }

                        static int dbg_feed_vec_count = 0;
                        std::string inst_name = this->name();

                        if (inst_name.find("NpuTop_std") != std::string::npos &&
                            act_emit_count >= 60 &&
                            act_emit_count <= 80)
                        {
                            DBG_COUT << "[IFMAP FEED VEC]"
                                      << " context=" << i_context_id.read()
                                      << " k=" << act_emit_count
                                      << " ch=" << ch_id
                                      << " ky=" << ky
                                      << " kx=" << kx
                                      << " start_abs_idx=" << start_abs_idx
                                      << " first_word=" << first_word
                                      << " last_word=" << last_word
                                      << " feed_vec=[";

                            for (int y = 0; y < Y_DIM; y++)
                            {
                                DBG_COUT << static_cast<int32_t>(feed_vec[y]);
                                if (y < Y_DIM - 1)
                                    DBG_COUT << ", ";
                            }

                            DBG_COUT << "]" << std::endl;
                        }

                        dbg_feed_vec_count++;
                        act_emit_count++;
                    }
                }
            }

            if (i_cnt_en.read())
            {
                bool sauria_addr_mode = use_sauria_ifmap_addr_gen();
                bool within_limit = false;
                if (sauria_addr_mode)
                {
                    within_limit = (!act_stream_init) || (act_word_req_idx <= act_last_word_idx);
                }
                else
                {
                    within_limit = (incnt < effective_k);
                }
                bool dil_allow = true;
                if (!sauria_addr_mode)
                {
                    sc_bv<DILP_W> dil = i_act_dil_pat.read();

                    bool dil_all_zero = true;
                    for (int i = 0; i < DILP_W; i++)
                    {
                        if (dil[i] == sc_dt::Log_1)
                        {
                            dil_all_zero = false;
                            break;
                        }
                    }

                    if (!dil_all_zero)
                        dil_allow = (dil[dil_idx % DILP_W] == sc_dt::Log_1);
                    // if(DILP_W >0){
                    //     dil_allow = (dil[dil_idx % DILP_W] == sc_dt::Log_1);
                    // }
                }

                if (within_limit && dil_allow)
                {
                    uint32_t final_addr = 0;

                    if (sauria_addr_mode)
                    {
                        uint32_t context_y_offset =
                            local_ch() * i_act_til_ystep.read();

                        if (!act_stream_init)
                        {
                            act_stream_base_idx = (context_y_offset / Y_DIM) * Y_DIM;

                            act_word_req_idx = act_stream_base_idx / Y_DIM;
                            act_emit_abs_idx = context_y_offset;
                            act_emit_count = 0;
                            act_pop_count = 0;

                            act_word_cache.clear();
                            act_requested_words.clear();

                            uint32_t effective_k_init = get_effective_k();

                            uint32_t dummy_start = 0;
                            uint32_t first_word = 0;
                            uint32_t last_word = 0;
                            uint32_t ch_id = 0;
                            uint32_t ky = 0;
                            uint32_t kx = 0;

                            if (calc_ifmap_word_range_for_k(
                                    effective_k_init - 1,
                                    effective_k_init,
                                    dummy_start,
                                    first_word,
                                    last_word,
                                    ch_id,
                                    ky,
                                    kx))
                            {
                                act_last_word_idx = last_word;
                            }
                            else
                            {
                                act_last_word_idx = act_word_req_idx;
                            }

                            act_stream_init = true;

                            DBG_COUT << "[IFMAP STREAM INIT]"
                                      << " context=" << i_context_id.read()
                                      << " context_y_offset=" << context_y_offset
                                      << " first_word=" << act_word_req_idx
                                      << " last_word=" << act_last_word_idx
                                      << " effective_k=" << effective_k_init
                                      << std::endl;
                        }

                        bool issued_read = false;

                        if (act_emit_count < effective_k)
                        {
                            uint32_t start_abs_idx = 0;
                            uint32_t first_word = 0;
                            uint32_t last_word = 0;
                            uint32_t ch_id = 0;
                            uint32_t ky = 0;
                            uint32_t kx = 0;

                            bool ok = calc_ifmap_word_range_for_k(
                                act_emit_count,
                                effective_k,
                                start_abs_idx,
                                first_word,
                                last_word,
                                ch_id,
                                ky,
                                kx);

                            if (ok)
                            {
                                uint32_t req_word_addr = 0;
                                bool need_request = false;

                                if (act_word_cache.find(first_word) == act_word_cache.end() &&
                                    act_requested_words.find(first_word) == act_requested_words.end())
                                {
                                    req_word_addr = first_word;
                                    need_request = true;
                                }
                                else if (last_word != first_word &&
                                         act_word_cache.find(last_word) == act_word_cache.end() &&
                                         act_requested_words.find(last_word) == act_requested_words.end())
                                {
                                    req_word_addr = last_word;
                                    need_request = true;
                                }

                                if (need_request)
                                {
                                    uint32_t req_final_addr =
                                        i_act_base_addr.read() + req_word_addr;

                                    pending_word_addr_q.push(req_word_addr);
                                    pending_glob_woffs_q.push(0);

                                    act_requested_words.insert(req_word_addr);

                                    o_srama_rden.write(true);
                                    o_srama_addr.write(req_final_addr);

                                    rden_q1 = true;
                                    issued_read = true;

                                    static int dbg_ifmap_demand_req_count = 0;
                                    std::string inst_name = this->name();

                                    if (inst_name.find("NpuTop_std") != std::string::npos &&
                                        dbg_ifmap_demand_req_count < 128)
                                    {
                                        DBG_COUT << "[IFMAP DEMAND REQ]"
                                                  << " req=" << dbg_ifmap_demand_req_count
                                                  << " context=" << i_context_id.read()
                                                  << " k=" << act_emit_count
                                                  << " ch=" << ch_id
                                                  << " ky=" << ky
                                                  << " kx=" << kx
                                                  << " start_abs_idx=" << start_abs_idx
                                                  << " first_word=" << first_word
                                                  << " last_word=" << last_word
                                                  << " req_word_addr=" << req_word_addr
                                                  << " req_final_addr=" << req_final_addr
                                                  << std::endl;

                                        dbg_ifmap_demand_req_count++;
                                    }
                                }
                            }
                        }

                        if (!issued_read)
                        {
                            o_srama_rden.write(false);
                            rden_q1 = false;
                        }
                    }
                    else
                    {
                        final_addr = i_act_base_addr.read() + addr_reg;
                    }
                    static int dbg_word_req_count = 0;
                    std::string inst_name = this->name();

                    if (inst_name.find("NpuTop_std") != std::string::npos &&
                        dbg_word_req_count < 32)
                    {
                        DBG_COUT << "[IFMAP WORD REQ]"
                                  << " req=" << dbg_word_req_count
                                  << " act_word_req_idx=" << act_word_req_idx
                                  << " final_addr=" << final_addr
                                  << " last_word=" << act_last_word_idx
                                  << std::endl;
                    }

                    dbg_word_req_count++;

                    static int dbg_ifmap_read_count = 0;

                    if (dbg_ifmap_read_count < 64)
                    {
                        DBG_COUT << "[DEBUG IFMAP] read_count=" << dbg_ifmap_read_count
                                  << " mode=" << (sauria_addr_mode ? "SAURIA" : "LINEAR")
                                  << "context_id=" << i_context_id.read()
                                  << "context_y_offset" << (local_ch() * i_act_til_ystep.read())
                                  << " feeder_en=" << i_feeder_en.read()
                                  << " cnt_en=" << i_cnt_en.read()
                                  << " cnt_clear=" << i_cnt_clear.read()
                                  << " base=" << i_act_base_addr.read()
                                  << " addr_reg=" << addr_reg
                                  << " x_cnt=" << act_x_cnt
                                  << " y_cnt=" << act_y_cnt
                                  << " ch_cnt=" << act_ch_cnt
                                  << " final_addr=" << final_addr
                                  << " incnt=" << incnt
                                  << " limit=" << i_act_incntlim.read()
                                  << " xlim=" << i_act_xlim.read()
                                  << " xstep=" << i_act_xstep.read()
                                  << " ylim=" << i_act_ylim.read()
                                  << " ystep=" << i_act_ystep.read()
                                  << " chlim=" << i_act_chlim.read()
                                  << " chstep=" << i_act_chstep.read()
                                  << std::endl;
                    }

                    dbg_ifmap_read_count++;

                    if (!sauria_addr_mode)
                    {
                        addr_reg += i_act_incntstep.read();
                        incnt++;
                    }
                }
                else
                {
                    o_srama_rden.write(false);
                    rden_q1 = false;

                    // DBG_COUT << "[DEBUG IFMAP SKIP] incnt=" << incnt
                    //           << " dil_idx=" << dil_idx
                    //           << " within_limit=" << within_limit
                    //           << " dil_allow=" << dil_allow
                    //           << std::endl;
                }
                dil_idx++;
            }
            else
            {
                o_srama_rden.write(false);
                // rden_q = false;
            }

            uint32_t effective_k_for_stall = get_effective_k();

            bool act_next_vec_ready = false;

            if (act_stream_init && act_emit_count < effective_k_for_stall)
            {
                uint32_t chstep = i_act_chstep.read();
                uint32_t ystep = i_act_ystep.read();

                uint32_t kernel_elems = 1;
                uint32_t kernel_w = 1;
                uint32_t n_channels = 1;
                uint32_t kx_step = 1;

                derive_ifmap_mvm_params(
                    effective_k_for_stall,
                    kernel_elems,
                    kernel_w,
                    n_channels,
                    kx_step);

                uint32_t k = act_emit_count;
                uint32_t ch_id = k / kernel_elems;
                uint32_t rem = k % kernel_elems;
                uint32_t ky = rem / kernel_w;
                uint32_t kx = rem % kernel_w;

                uint32_t context_y_offset =
                    local_ch() * i_act_til_ystep.read();

                uint32_t start_abs_idx =
                    context_y_offset +
                    ch_id * chstep +
                    ky * ystep +
                    kx * kx_step;

                uint32_t first_word = start_abs_idx / Y_DIM;
                uint32_t last_word = (start_abs_idx + Y_DIM - 1) / Y_DIM;

                act_next_vec_ready =
                    (act_word_cache.find(first_word) != act_word_cache.end()) &&
                    (last_word == first_word ||
                     act_word_cache.find(last_word) != act_word_cache.end());
            }
            else if (act_emit_count >= effective_k_for_stall)
            {
                act_next_vec_ready = true;
            }

            // ---------------------------------------------------------
            // IFMAP full-prefetch barrier.
            //
            // Current SystemC model uses demand-read for SAURIA/im2col IFMAP.
            // The IFMAP address stream is sparse and can jump far in SRAM,
            // so the feeder may not keep up with one vector/cycle compute.
            //
            // For correctness-first model:
            //   - Stall compute until IFMAP has emitted all effective_k logical vectors.
            //   - Controller still keeps cnt_en=1 while stalled.
            //   - Feeder continues demand-reading and pushing row_fifos.
            //   - Once act_emit_count == effective_k, compute can pop 450 valid vectors.
            //
            // This prevents A/B K misalignment.
            // ---------------------------------------------------------
            bool act_can_pop = true;

            for (int y = 0; y < Y_DIM; y++)
            {
                if (row_fifos[y].empty())
                {
                    act_can_pop = false;
                    break;
                }
            }

            bool act_need_more =
                i_feeder_en.read() &&
                (act_pop_count < effective_k_for_stall);

            // Hold controller until all IFMAP vectors for this context have been emitted.
            bool act_prefetch_not_done =
                act_stream_init &&
                (act_emit_count < effective_k_for_stall);

            // Also stall if compute still needs data but FIFO is empty.
            bool act_should_stall =
                act_need_more &&
                (act_prefetch_not_done ||
                 !act_can_pop);

            o_stall.write(act_should_stall);

            // Optional debug
            static int dbg_ifmap_stall_count = 0;
            std::string inst_name_for_stall = this->name();

            if (inst_name_for_stall.find("NpuTop_std") != std::string::npos &&
                act_should_stall &&
                dbg_ifmap_stall_count < 80)
            {
                DBG_COUT << "[IFMAP STALL STATUS]"
                          << " context=" << i_context_id.read()
                          << " emit_count=" << act_emit_count
                          << " pop_count=" << act_pop_count
                          << " effective_k=" << effective_k_for_stall
                          << " act_can_pop=" << act_can_pop
                          << std::endl;

                dbg_ifmap_stall_count++;
            }

            // 2. Pop and Shift Activations (Wavefront Skew Lines)
            // ---------------------------------------------------------
            // Pop/shift IFMAP physical stream.
            //
            // Important:
            //   After all K logical vectors have been popped, row_fifos become empty,
            //   but skew_regs[y] still contain delayed tail values for y > 0.
            //   During controller DRAIN_FEED, i_pop_en remains true, so we must keep
            //   shifting zeros into skew_regs to flush the physical tail.
            //
            // If we write zero directly when row_fifos are empty, PE(0,0) may be
            // correct but all other PEs lose their tail products.
            // ---------------------------------------------------------
            if (i_pop_en.read())
            {
                act_vector_t<Y_DIM, T_ACT> popped_vec;
                act_vector_t<Y_DIM, T_ACT> act_out;

                uint32_t effective_k = get_effective_k();
                uint32_t nsplit = i_nsplit.read();

                bool has_logical_vector =
                    (act_pop_count < effective_k);

                for (int y = 0; y < Y_DIM; y++)
                {
                    if (row_fifos[y].empty())
                    {
                        has_logical_vector = false;
                        break;
                    }
                }

                bool had_real_vector = has_logical_vector;

                for (int y = 0; y < Y_DIM; y++)
                {
                    T_ACT popped = static_cast<T_ACT>(0);

                    if (has_logical_vector)
                    {
                        popped = row_fifos[y].front();
                        row_fifos[y].pop();
                    }

                    popped_vec[y] = popped;

                    int target_skew = y;
                    if (IS_LANE_B)
                    {
                        target_skew = y - (int)nsplit;
                    }
                    if (target_skew < 0)
                        target_skew = 0;

                    if (target_skew == 0)
                    {
                        act_out[y] = popped;
                    }
                    else
                    {
                        while (skew_regs[y].size() < static_cast<size_t>(target_skew))
                        {
                            skew_regs[y].insert(
                                skew_regs[y].begin(),
                                static_cast<T_ACT>(0));
                        }

                        // This push must happen even during tail flush, with popped=0.
                        skew_regs[y].push_back(popped);

                        act_out[y] = skew_regs[y].front();

                        skew_regs[y].erase(skew_regs[y].begin());
                    }
                }

                o_act_arr.write(act_out);

                // Dump only real logical vectors, not tail flush zeros.
                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    static std::ofstream ifmap_pop_trace("trace_sysc/ifmap_pop_logical.csv");
                    static bool ifmap_pop_header = false;
                    static uint32_t dbg_ifmap_rows = 0;

                    if (!ifmap_pop_header)
                    {
                        ifmap_pop_trace << "context,k";

                        for (int yy = 0; yy < Y_DIM; yy++)
                        {
                            ifmap_pop_trace << ",act" << yy;
                        }

                        ifmap_pop_trace << "\n";
                        ifmap_pop_header = true;
                    }

                    uint32_t trace_k = act_pop_count;

                    if (had_real_vector && dbg_ifmap_rows < 8192)
                    {
                        ifmap_pop_trace << i_context_id.read()
                                        << "," << trace_k;

                        for (int yy = 0; yy < Y_DIM; yy++)
                        {
                            ifmap_pop_trace << ","
                                            << static_cast<int32_t>(popped_vec[yy]);
                        }

                        ifmap_pop_trace << "\n";
                        ifmap_pop_trace.flush();

                        dbg_ifmap_rows++;
                    }
                }

                if (had_real_vector && act_pop_count < effective_k)
                {
                    act_pop_count++;
                }
            }
            else
            {
                o_act_arr.write(act_vector_t<Y_DIM, T_ACT>(static_cast<T_ACT>(0)));
            }

            // uint32_t effective_k = get_effective_k();
            bool act_pop_done = act_stream_init && (act_pop_count >= effective_k);

            o_act_done.write(act_pop_done);
            o_act_til_done.write(act_pop_done);

            static int dbg_act_done_count = 0;
            if (this->name() && dbg_act_done_count < 128)
            {
                std::string inst_name = this->name();

                if (inst_name.find("NpuTop_std") != std::string::npos)
                {
                    DBG_COUT << "[IFMAP DONE STATUS]"
                              << " context=" << i_context_id.read()
                              << " emit_count=" << act_emit_count
                              << " pop_count=" << act_pop_count
                              << " limit=" << effective_k
                              << " cfg_limit=" << i_act_incntlim.read()
                              << " done=" << act_pop_done
                              << std::endl;
                }

                dbg_act_done_count++;
            }

            // 3. Update status flags
            bool empty = row_fifos[0].empty();
            bool full = row_fifos[0].size() >= FIFO_DEPTH;
            o_fifo_empty.write(empty);
            o_fifo_full.write(full);
        }
    };

} // namespace sauria

#endif // SAURIA_IFMAP_FEEDER_H
