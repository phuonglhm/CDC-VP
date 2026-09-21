// SystemC Model for SAURIA NPU Core
// Partial Sum Memory (PSM) Block with Parameterized Dimensions

// i_fsm_start pulse
// → phase 5: wait PSM_START_DELAY cycle, cscan=0, write=0
// → phase 1: write x0, cscan=0
// → phase 2: cscan=1, no write
// → phase 3: cscan=1, no write
// → phase 4: write x1..x15, cscan=1

#ifndef SAURIA_PSM_TOP_H
#define SAURIA_PSM_TOP_H

#include <fstream>
#include <string>
#include "sauria_types.h"
#include "debug.h"

static constexpr uint32_t PSM_START_DELAY = 2;

namespace sauria
{

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_PSUM = float,
        int SRAMC_CAP = 1536>
    class Psm : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Data Inputs from Array Scan Chain
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_c_arr{"i_c_arr"}; // Data entering from leftmost PE column

        // Memory Interface to SRAM C
        sc_in<uint32_t> i_out_base_addr{"i_out_base_addr"};
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_rdata{"i_sramc_rdata"};
        sc_out<uint32_t> o_sramc_addr{"o_sramc_addr"};
        sc_out<bool> o_sramc_wren{"o_sramc_wren"};
        sc_out<bool> o_sramc_rden{"o_sramc_rden"};
        sc_out<sramc_mask_t<Y_DIM>> o_sramc_wmask{"o_sramc_wmask"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_wdata{"o_sramc_wdata"};

        // Config Parameters
        sc_in<uint32_t> i_cxlim{"i_cxlim"};
        sc_in<uint32_t> i_cxstep{"i_cxstep"};
        sc_in<uint32_t> i_cklim{"i_cklim"};
        sc_in<uint32_t> i_ckstep{"i_ckstep"};
        sc_in<uint32_t> i_til_cylim{"i_til_cylim"};
        sc_in<uint32_t> i_til_cystep{"i_til_cystep"};
        sc_in<uint32_t> i_til_cklim{"i_til_cklim"};
        sc_in<uint32_t> i_til_ckstep{"i_til_ckstep"};
        sc_in<uint32_t> i_ncontexts{"i_ncontexts"};
        sc_in<uint32_t> i_total_contexts{"i_total_contexts"};
        sc_in<uint32_t> i_context_id{"i_context_id"};
        sc_in<bool> i_preload_en{"i_preload_en"};
        sc_in<sramc_mask_t<Y_DIM>> i_rows_active{"i_rows_active"}; // Active Rows config (Y bits)

        // Control Inputs from global FSM
        sc_in<bool> i_fsm_start{"i_fsm_start"};
        sc_in<bool> i_fsm_reset{"i_fsm_reset"};
        sc_in<bool> i_pipeline_en{"i_pipeline_en"};

        // Control Outputs to Global FSM / Array
        sc_out<bool> o_done{"o_done"};
        sc_out<bool> o_finalwrite{"o_finalwrite"};
        sc_out<bool> o_shift_done{"o_shift_done"};
        sc_out<bool> o_cscan_en{"o_cscan_en"}; // Directs array to shift out C chain

        // Data Outputs to Array (Preload values sent right-to-left)
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_c_arr{"o_c_arr"};

        SC_CTOR(Psm)
        {
            SC_METHOD(psm_process);
            sensitive << i_clk.pos();
        }

    private:
        uint32_t start_delay_cnt{0};
        bool start_q{false};

        uint32_t addr_reg{0};
        uint32_t shift_cnt{0};
        uint32_t delay_cnt{0};
        uint32_t write_limit_vectors{0};

        uint32_t psm_context_cnt{0};
        uint32_t active_context_id{0};
        uint32_t context_addr_base{0};

        // PSM scan timing phase
        // 0 = idle
        // 1 = write current scan_out as x0
        // 2 = enable scan, no write
        // 3 = wait scan_out update, no write
        // 4 = normal scan write x1..x15
        uint32_t psm_scan_phase{0};

        bool shifting{false};
        bool preload_mode{false};

        void dump_psm_trace(
            uint32_t context,
            uint32_t addr,
            uint32_t shift,
            const psum_vector_t<Y_DIM, T_PSUM> &data)
        {
            // Only dump STD instance, avoid std/approx/gated duplicated traces.
            std::string inst_name = this->name();
            if (inst_name.find("NpuTop_std") == std::string::npos)
            {
                return;
            }

            static std::ofstream psm_trace("trace_sysc/psm_write_trace.csv");
            static bool psm_trace_header = false;
            static uint32_t psm_trace_count = 0;

            if (!psm_trace_header)
            {
                psm_trace << "write_count,context,addr,shift_cnt";
                for (int y = 0; y < Y_DIM; y++)
                {
                    psm_trace << ",lane" << y;
                }
                psm_trace << "\n";
                psm_trace_header = true;
            }

            psm_trace
                << psm_trace_count << ","
                << context << ","
                << addr << ","
                << shift;

            for (int y = 0; y < Y_DIM; y++)
            {
                psm_trace << "," << static_cast<int32_t>(data[y]);
            }

            psm_trace << "\n";
            psm_trace.flush();

            psm_trace_count++;
        }

        uint32_t get_write_limit_vectors()
        {
            if (i_cxlim.read() != 0)
            {
                return i_cxlim.read();
            }
            return 0;
        }

        uint32_t calc_c_addr(uint32_t global_context, uint32_t x)
        {
            uint32_t nctx = i_ncontexts.read();
            if (nctx == 0)
            {
                nctx = 1;
            }

            uint32_t cxlim = i_cxlim.read();
            if (cxlim == 0)
            {
                cxlim = X_DIM;
            }

            uint32_t out_tile = global_context / nctx;
            uint32_t local_ctx = global_context % nctx;

            uint32_t one_output_tile_elements =
                nctx * cxlim * Y_DIM;

            // SAURIA C/output layout:
            //
            //   C[out_tile][x][local_context][y]
            //
            // element base address for one Y-vector:
            //
            //   addr =
            //       out_tile  * (nctx * cxlim * Y_DIM)
            //     + x         * (nctx * Y_DIM)
            //     + local_ctx * Y_DIM
            //
            return (out_tile * one_output_tile_elements + x * (nctx * Y_DIM) + local_ctx * Y_DIM) / Y_DIM;
        }

        void psm_process()
        {
            if (!i_rstn.read() || i_fsm_reset.read())
            {
                // o_sramc_addr.write(i_out_base_addr.read());
                o_sramc_addr.write(0);
                o_sramc_wren.write(false);
                o_sramc_rden.write(false);
                o_sramc_wmask.write(sramc_mask_t<Y_DIM>());
                o_sramc_wdata.write(psum_vector_t<Y_DIM, T_PSUM>());

                o_done.write(false);
                o_finalwrite.write(false);
                o_shift_done.write(false);
                o_cscan_en.write(false);
                o_c_arr.write(psum_vector_t<Y_DIM, T_PSUM>());

                addr_reg = 0;
                shift_cnt = 0;
                delay_cnt = 0;
                write_limit_vectors = 0;
                psm_scan_phase = 0;

                start_delay_cnt = 0;
                start_q = false;

                shifting = false;
                preload_mode = false;

                psm_context_cnt = 0;
                context_addr_base = 0;
                active_context_id = 0;
                return;
            }

            // Default outputs each cycle
            o_sramc_wren.write(false);
            o_sramc_rden.write(false);
            o_done.write(false);
            o_shift_done.write(false);
            o_finalwrite.write(false);
            o_cscan_en.write(false);

            bool start_pulse = i_fsm_start.read() && !start_q;
            start_q = i_fsm_start.read();

            if (start_pulse && !shifting)
            {
                shifting = true;
                shift_cnt = 0;
                delay_cnt = 0;
                start_delay_cnt = 0;

                preload_mode = false;

                write_limit_vectors = get_write_limit_vectors();

                // Latch global context id from controller.
                // For multi-output-tile test this must be 0..5.
                active_context_id = i_context_id.read();

                context_addr_base = 0;
                addr_reg = 0;

                // Do not scan or write immediately.
                o_cscan_en.write(false);
                o_sramc_wren.write(false);

                psm_scan_phase = 5;

                DBG_COUT << "[PSM START]"
                          << " global_context = " << active_context_id
                          << " / total_contexts = " << i_total_contexts.read()
                          << " ncontexts = " << i_ncontexts.read()
                          << std::endl;

                return;
            }

            if (!shifting)
            {
                o_cscan_en.write(false);
                return;
            }

            // ---------------------------------------------------------
            // Phase 5: wait after FSM start before reading scan_out.
            // This gives local cswitch/swap time to complete across SA.
            // No scan, no write in this phase.
            // ---------------------------------------------------------
            if (psm_scan_phase == 5)
            {
                o_cscan_en.write(false);
                o_sramc_wren.write(false);

                if (start_delay_cnt >= PSM_START_DELAY)
                {
                    psm_scan_phase = 1;

                    DBG_COUT << "[PSM START DELAY DONE]"
                              << " context=" << psm_context_cnt
                              << " delay_cnt=" << start_delay_cnt
                              << std::endl;
                }
                else
                {
                    start_delay_cnt++;
                }

                return;
            }

            // ---------------------------------------------------------
            // Phase 1: write current i_c_arr as x0, no scan advance
            // ---------------------------------------------------------
            if (psm_scan_phase == 1)
            {
                psum_vector_t<Y_DIM, T_PSUM> array_out = i_c_arr.read();

                // uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);

                // o_cscan_en.write(false);

                // o_sramc_wren.write(true);
                // o_sramc_addr.write(i_out_base_addr.read() + wr_addr);
                // o_sramc_wmask.write(i_rows_active.read());
                // // write_data[y] = i_sramc_r_data.read()[y] + array_out[y];
                // o_sramc_wdata.write(array_out);
                // dump_psm_trace(active_context_id, i_out_base_addr.read() + wr_addr, shift_cnt, array_out);

                uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);
                o_cscan_en.write(false);
                o_sramc_wren.write(true);
                // SRAMC uses local address. Do not add DRAM out_base here
                o_sramc_addr.write(wr_addr);
                o_sramc_wmask.write(i_rows_active.read());
                o_sramc_wdata.write(array_out);
                dump_psm_trace(active_context_id, wr_addr, shift_cnt, array_out);
                shift_cnt++;

                psm_scan_phase = 2;
                return;
            }

            // ---------------------------------------------------------
            // Phase 2: enable scan, no write
            // SA sees this on the next clock.
            // ---------------------------------------------------------
            if (psm_scan_phase == 2)
            {
                o_cscan_en.write(true);
                o_sramc_wren.write(false);

                psm_scan_phase = 3;
                return;
            }

            // ---------------------------------------------------------
            // Phase 3: keep scan enabled, wait one cycle
            // for scan_out/i_c_arr to update.
            // ---------------------------------------------------------
            if (psm_scan_phase == 3)
            {
                o_cscan_en.write(true);
                o_sramc_wren.write(false);

                psm_scan_phase = 4;
                return;
            }

            // ---------------------------------------------------------
            // Phase 4: normal scan writes x1..x15
            // ---------------------------------------------------------
            if (shift_cnt < write_limit_vectors)
            {
                psum_vector_t<Y_DIM, T_PSUM> array_out = i_c_arr.read();

                // uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);

                // o_cscan_en.write(true);

                // o_sramc_wren.write(true);
                // o_sramc_addr.write(i_out_base_addr.read() + wr_addr);
                // o_sramc_wmask.write(i_rows_active.read());
                // // write_data[y] = i_sramc_r_data.read()[y] + array_out[y];
                // o_sramc_wdata.write(array_out);

                // dump_psm_trace(active_context_id, i_out_base_addr.read() + wr_addr, shift_cnt, array_out);

                uint32_t wr_addr = calc_c_addr(active_context_id, shift_cnt);

                o_cscan_en.write(true);

                o_sramc_wren.write(true);
                o_sramc_addr.write(wr_addr);
                o_sramc_wmask.write(i_rows_active.read());
                o_sramc_wdata.write(array_out);

                dump_psm_trace(active_context_id, i_out_base_addr.read() + wr_addr, shift_cnt, array_out);

                shift_cnt++;
                return;
            }

            // ---------------------------------------------------------
            // Done current context
            // ---------------------------------------------------------
            shifting = false;
            psm_scan_phase = 0;

            o_cscan_en.write(false);
            o_sramc_wren.write(false);
            o_sramc_rden.write(false);
            o_shift_done.write(true);
            o_done.write(true);

            uint32_t nctx = i_ncontexts.read();
            if (nctx == 0)
            {
                nctx = 1;
            }

            if ((psm_context_cnt + 1) < nctx)
            {
                psm_context_cnt++;
            }
            else
            {
                psm_context_cnt = 0;
            }

            DBG_COUT << "[PSM DONE]"
                      << " global_context = " << active_context_id
                      << std::endl;
        }
        // else
        // {
        //     o_sramc_wren.write(false);
        //     o_sramc_rden.write(false);
        // }
    };
} // namespace sauria

#endif // SAURIA_PSM_TOP_H

