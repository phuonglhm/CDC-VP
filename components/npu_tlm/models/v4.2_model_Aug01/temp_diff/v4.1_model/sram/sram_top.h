// SystemC Model for SAURIA NPU Core
// Double-Buffered SRAM System (A, B, C)

#ifndef SAURIA_SRAM_TOP_H
#define SAURIA_SRAM_TOP_H

#include "sauria_types.h"
#include "debug.h"

namespace sauria
{

    template <
        int X_DIM = 32,
        int Y_DIM = 32,
        typename T_ACT = float,
        typename T_WEI = float,
        typename T_PSUM = float,
        int SRAMA_CAP = 1024,
        int SRAMB_CAP = 1024,
        int SRAMC_CAP = 2048>
    class Sram : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Power Gating / Deep Sleep (Global)
        sc_in<bool> i_deepsleep{"i_deepsleep"};
        sc_in<bool> i_powergate{"i_powergate"};

        // Buffer Selection (Double Buffering)
        // bit 0: SRAM A, bit 1: SRAM B, bit 2: SRAM C
        sc_in<sc_bv<3>> i_select{"i_select"};

        // Host-side Interface
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"}; // 128-bit host bus (4 floats)
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"}; // Write byte/word mask
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Accelerator-side Interface: SRAM A (Activations)
        sc_in<uint32_t> i_srama_addr{"i_srama_addr"};
        sc_in<bool> i_srama_rden{"i_srama_rden"};
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_srama_data{"o_srama_data"};

        // Accelerator-side Interface: SRAM B (Weights)
        sc_in<uint32_t> i_sramb_addr{"i_sramb_addr"};
        sc_in<bool> i_sramb_rden{"i_sramb_rden"};
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_sramb_data{"o_sramb_data"};

        // Accelerator-side Interface: SRAM C (Partial Sums / Outputs)
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_wdata{"i_sramc_wdata"};
        sc_in<uint32_t> i_sramc_addr{"i_sramc_addr"};
        sc_in<bool> i_sramc_wren{"i_sramc_wren"};
        sc_in<bool> i_sramc_rden{"i_sramc_rden"};
        sc_in<sramc_mask_t<Y_DIM>> i_sramc_wmask{"i_sramc_wmask"}; // Element-wise write mask (Y bits)
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_rdata{"o_sramc_rdata"};

        SC_CTOR(Sram)
        {
            SC_METHOD(beh_process);
            sensitive << i_clk.pos();
        }

    private:
        static constexpr int log2_const(int n)
        {
            return (n <= 1) ? 0 : 1 + log2_const(n / 2);
        }

        static const int SUBWORDS_A = Y_DIM / 4;
        static const int MASK_A = SUBWORDS_A - 1;
        static const int SHIFT_A = log2_const(SUBWORDS_A);

        static const int SUBWORDS_B = X_DIM / 4;
        static const int MASK_B = SUBWORDS_B - 1;
        static const int SHIFT_B = log2_const(SUBWORDS_B);

        static const int SUBWORDS_C = Y_DIM / 4;
        static const int MASK_C = SUBWORDS_C - 1;
        static const int SHIFT_C = log2_const(SUBWORDS_C);

        // Double-buffered physical memories
        act_vector_t<Y_DIM, T_ACT> mem_a[2][SRAMA_CAP];
        wei_vector_t<X_DIM, T_WEI> mem_b[2][SRAMB_CAP];
        psum_vector_t<Y_DIM, T_PSUM> mem_c[2][SRAMC_CAP];

        // SystemC clock-edge behavioral process
        void beh_process()
        {
            if (!i_rstn.read())
            {
                // Reset latency outputs
                o_host_rdata.write(host_data_t());
                o_srama_data.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            // Power Gating / Deep Sleep simulation behavior
            if (i_powergate.read())
            {
                // If power-gated, all memory state is lost and output is zero
                for (int i = 0; i < 2; i++)
                {
                    for (int d = 0; d < SRAMA_CAP; d++)
                        mem_a[i][d] = act_vector_t<Y_DIM, T_ACT>();
                    for (int d = 0; d < SRAMB_CAP; d++)
                        mem_b[i][d] = wei_vector_t<X_DIM, T_WEI>();
                    for (int d = 0; d < SRAMC_CAP; d++)
                        mem_c[i][d] = psum_vector_t<Y_DIM, T_PSUM>();
                }
                o_host_rdata.write(host_data_t());
                o_srama_data.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            if (i_deepsleep.read())
            {
                // In deepsleep, state is preserved, but outputs are inactive
                o_host_rdata.write(host_data_t());
                o_srama_data.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            // Extract select bits
            sc_bv<3> sel = i_select.read();
            bool sel_a = sel[0].to_bool();
            bool sel_b = sel[1].to_bool();
            bool sel_c = sel[2].to_bool();

            // Double Buffering buffer indexes
            int npu_a_idx = sel_a ? 0 : 1;
            int host_a_idx = sel_a ? 1 : 0;

            int npu_b_idx = sel_b ? 0 : 1;
            int host_b_idx = sel_b ? 1 : 0;

            int npu_c_idx = sel_c ? 0 : 1;
            int host_c_idx = sel_c ? 1 : 0;

            // --------------------------------------------------------
            // 1. Host-Side Accesses (Reads and Writes)
            // --------------------------------------------------------
            uint32_t addr = i_host_addr.read();
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            uint32_t local_addr = addr & ~SAURIA_MEM_ADDR_MASK;

            if (i_host_wren.read())
            {
                host_data_t wdata = i_host_wdata.read();
                host_mask_t wmask = i_host_wmask.read();

                if (mem_region == SRAMA_OFFSET)
                {
                    uint32_t sub_word = local_addr & MASK_A;
                    uint32_t phys_addr = (local_addr >> SHIFT_A) % SRAMA_CAP;
                    for (int i = 0; i < 4; i++)
                    {
                        if (wmask[i] && (sub_word * 4 + i < Y_DIM))
                        {
                            mem_a[host_a_idx][phys_addr][sub_word * 4 + i] = wdata[i];
                        }
                    }
                }
                else if (mem_region == SRAMB_OFFSET)
                {
                    uint32_t sub_word = local_addr & MASK_B;
                    uint32_t phys_addr = (local_addr >> SHIFT_B) % SRAMB_CAP;
                    for (int i = 0; i < 4; i++)
                    {
                        if (wmask[i] && (sub_word * 4 + i < X_DIM))
                        {
                            mem_b[host_b_idx][phys_addr][sub_word * 4 + i] = wdata[i];
                        }
                    }
                }
                else if (mem_region == SRAMC_OFFSET)
                {
                    uint32_t sub_word = local_addr & MASK_C;
                    uint32_t phys_addr = (local_addr >> SHIFT_C) % SRAMC_CAP;
                    for (int i = 0; i < 4; i++)
                    {
                        if (wmask[i] && (sub_word * 4 + i < Y_DIM))
                        {
                            mem_c[host_c_idx][phys_addr][sub_word * 4 + i] = wdata[i];
                        }
                    }
                }
            }

            if (i_host_rden.read())
            {
                host_data_t rdata;
                if (mem_region == SRAMA_OFFSET)
                {
                    uint32_t sub_word = local_addr & MASK_A;
                    uint32_t phys_addr = (local_addr >> SHIFT_A) & (SRAMA_CAP - 1);
                    for (int i = 0; i < 4; i++)
                    {
                        if (sub_word * 4 + i < Y_DIM)
                        {
                            rdata[i] = mem_a[host_a_idx][phys_addr][sub_word * 4 + i];
                        }
                    }
                }
                else if (mem_region == SRAMB_OFFSET)
                {
                    uint32_t sub_word = local_addr & MASK_B;
                    uint32_t phys_addr = (local_addr >> SHIFT_B) & (SRAMB_CAP - 1);
                    for (int i = 0; i < 4; i++)
                    {
                        if (sub_word * 4 + i < X_DIM)
                        {
                            rdata[i] = mem_b[host_b_idx][phys_addr][sub_word * 4 + i];
                        }
                    }
                }
                else if (mem_region == SRAMC_OFFSET)
                {
                    uint32_t sub_word = local_addr & MASK_C;
                    uint32_t phys_addr = (local_addr >> SHIFT_C) & (SRAMC_CAP - 1);
                    for (int i = 0; i < 4; i++)
                    {
                        if (sub_word * 4 + i < Y_DIM)
                        {
                            rdata[i] = mem_c[host_c_idx][phys_addr][sub_word * 4 + i];
                        }
                    }
                }
                o_host_rdata.write(rdata);
            }

            // --------------------------------------------------------
            // 2. Accelerator-Side Accesses (NPU Reads and Writes)
            // --------------------------------------------------------

            // SRAM A Reads (Activations)
            if (i_srama_rden.read())
            {
                uint32_t addr_a = i_srama_addr.read() % SRAMA_CAP;
                auto data_a = mem_a[npu_a_idx][addr_a];

                o_srama_data.write(data_a);

                static int dbg_srama_read_count = 0;

                if (dbg_srama_read_count < 32)
                {
                    DBG_COUT << "[SRAM A NPU READ] count=" << dbg_srama_read_count
                              << " npu_a_idx=" << npu_a_idx
                              << " host_a_idx=" << host_a_idx
                              << " sel_a=" << sel_a
                              << " addr=" << addr_a
                              << " data=[";

                    for (int i = 0; i < Y_DIM; i++)
                    {
                        DBG_COUT << static_cast<int>(data_a[i]);
                        if (i < Y_DIM - 1)
                            DBG_COUT << ", ";
                    }

                    DBG_COUT << "]" << std::endl;
                }

                dbg_srama_read_count++;
            }

            // SRAM B Reads (Weights)
            if (i_sramb_rden.read())
            {
                uint32_t addr_b = i_sramb_addr.read() % SRAMB_CAP;
                auto data_b = mem_b[npu_b_idx][addr_b];

                o_sramb_data.write(data_b);

                static int dbg_sramb_read_count = 0;

                if (dbg_sramb_read_count < 32)
                {
                    DBG_COUT << "[SRAM B NPU READ] count=" << dbg_sramb_read_count
                              << " npu_b_idx=" << npu_b_idx
                              << " host_b_idx=" << host_b_idx
                              << " sel_b=" << sel_b
                              << " addr=" << addr_b
                              << " data=[";

                    for (int i = 0; i < X_DIM; i++)
                    {
                        DBG_COUT << static_cast<int>(data_b[i]);
                        if (i < X_DIM - 1)
                            DBG_COUT << ", ";
                    }

                    DBG_COUT << "]" << std::endl;
                }

                dbg_sramb_read_count++;
            }

            // SRAM C Reads/Writes (Partial Sums)
            if (i_sramc_wren.read())
            {
                uint32_t addr_c = i_sramc_addr.read() % SRAMC_CAP;
                psum_vector_t<Y_DIM, T_PSUM> wdata_c = i_sramc_wdata.read();
                sramc_mask_t<Y_DIM> wmask_c = i_sramc_wmask.read();

                for (int i = 0; i < Y_DIM; i++)
                {
                    if (wmask_c[i])
                    {
                        mem_c[npu_c_idx][addr_c][i] = wdata_c[i];
                    }
                }

                static int dbg_sramc_write_count = 0;

                if (dbg_sramc_write_count < 64)
                {
                    DBG_COUT << "[SRAM C NPU WRITE] count="
                              << dbg_sramc_write_count
                              << " npu_c_idx=" << npu_c_idx
                              << " host_c_idx=" << host_c_idx
                              << " sel_c=" << sel_c
                              << " addr=" << addr_c
                              << " wmask=" << wmask_c
                              << " data=[";

                    for (int i = 0; i < Y_DIM; i++)
                    {
                        DBG_COUT << wdata_c[i];
                        if (i < Y_DIM - 1)
                            DBG_COUT << ", ";
                    }

                    DBG_COUT << "]" << std::endl;
                }

                dbg_sramc_write_count++;
            }

            if (i_sramc_rden.read())
            {
                uint32_t addr_c = i_sramc_addr.read() & (SRAMC_CAP - 1);
                o_sramc_rdata.write(mem_c[npu_c_idx][addr_c]);
            }
        }
    };

} // namespace sauria

#endif // SAURIA_SRAM_TOP_H

