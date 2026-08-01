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

        // Buffer Selection (Double Buffering select - kept for compatibility but unused in 8-bank mode)
        sc_in<sc_bv<3>> i_select{"i_select"};

        // Host-side Interface
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"}; // 128-bit host bus (4 elements)
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"}; // Write mask
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Accelerator-side Interface: SRAM A (Activations) - Lane A & B
        sc_in<uint32_t> i_srama_addr_a{"i_srama_addr_a"};
        sc_in<bool> i_srama_rden_a{"i_srama_rden_a"};
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_srama_data_a{"o_srama_data_a"};

        sc_in<uint32_t> i_srama_addr_b{"i_srama_addr_b"};
        sc_in<bool> i_srama_rden_b{"i_srama_rden_b"};
        sc_out<act_vector_t<Y_DIM, T_ACT>> o_srama_data_b{"o_srama_data_b"};

        // Accelerator-side Interface: SRAM B (Weights) - Lane A & B
        sc_in<uint32_t> i_sramb_addr_a{"i_sramb_addr_a"};
        sc_in<bool> i_sramb_rden_a{"i_sramb_rden_a"};
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_sramb_data_a{"o_sramb_data_a"};

        sc_in<uint32_t> i_sramb_addr_b{"i_sramb_addr_b"};
        sc_in<bool> i_sramb_rden_b{"i_sramb_rden_b"};
        sc_out<wei_vector_t<X_DIM, T_WEI>> o_sramb_data_b{"o_sramb_data_b"};

        // Accelerator-side Interface: SRAM C (Partial Sums) - Lane A & B
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_wdata_a{"i_sramc_wdata_a"};
        sc_in<uint32_t> i_sramc_addr_a{"i_sramc_addr_a"};
        sc_in<bool> i_sramc_wren_a{"i_sramc_wren_a"};
        sc_in<bool> i_sramc_rden_a{"i_sramc_rden_a"};
        sc_in<sramc_mask_t<Y_DIM>> i_sramc_wmask_a{"i_sramc_wmask_a"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_rdata_a{"o_sramc_rdata_a"};

        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_sramc_wdata_b{"i_sramc_wdata_b"};
        sc_in<uint32_t> i_sramc_addr_b{"i_sramc_addr_b"};
        sc_in<bool> i_sramc_wren_b{"i_sramc_wren_b"};
        sc_in<bool> i_sramc_rden_b{"i_sramc_rden_b"};
        sc_in<sramc_mask_t<Y_DIM>> i_sramc_wmask_b{"i_sramc_wmask_b"};
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_rdata_b{"o_sramc_rdata_b"};

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

        // Datatype-agnostic capacity constraints based on target byte budgets
        static constexpr int ELEMENT_SIZE_A = sizeof(T_ACT);
        static constexpr int VECTOR_SIZE_A = Y_DIM * ELEMENT_SIZE_A;
        static constexpr int SRAMA_CAP_A = (416 * 1024) / (VECTOR_SIZE_A > 0 ? VECTOR_SIZE_A : 1);
        static constexpr int SRAMA_CAP_B = (408 * 1024) / (VECTOR_SIZE_A > 0 ? VECTOR_SIZE_A : 1);

        static constexpr int ELEMENT_SIZE_B = sizeof(T_WEI);
        static constexpr int VECTOR_SIZE_B = X_DIM * ELEMENT_SIZE_B;
        static constexpr int SRAMB_CAP_A = (320 * 1024) / (VECTOR_SIZE_B > 0 ? VECTOR_SIZE_B : 1);
        static constexpr int SRAMB_CAP_B = (320 * 1024) / (VECTOR_SIZE_B > 0 ? VECTOR_SIZE_B : 1);

        static constexpr int ELEMENT_SIZE_C = sizeof(T_PSUM);
        static constexpr int VECTOR_SIZE_C = Y_DIM * ELEMENT_SIZE_C;
        static constexpr int SRAMC_CAP_A = SRAMC_CAP;
        static constexpr int SRAMC_CAP_B = SRAMC_CAP;

        static constexpr int SCRATCH_SIZE = 24 * 1024; // 24 KB

        // Subword offsets for host addressing (datatype-agnostic)
        static constexpr uint32_t SRAMA_REG_B_OFFSET = (416 * 1024) / (4 * sizeof(T_ACT));
        static constexpr uint32_t SRAMA_TOTAL_SIZE   = (824 * 1024) / (4 * sizeof(T_ACT));

        static constexpr uint32_t SRAMB_BANK_1_OFFSET = (320 * 1024) / (4 * sizeof(T_WEI));
        static constexpr uint32_t SRAMB_TOTAL_SIZE    = (640 * 1024) / (4 * sizeof(T_WEI));

        static constexpr uint32_t SRAMC_BANK_4_PSUMS = (50 * 1024) / (4 * sizeof(T_PSUM));
        static constexpr uint32_t SRAMC_BANK_4_SCRATCH = (50 * 1024) / (4 * sizeof(T_PSUM));
        static constexpr uint32_t SRAMC_BANK_5_PSUMS = (100 * 1024) / (4 * sizeof(T_PSUM));
        static constexpr uint32_t SRAMC_BANK_5_SCRATCH = (100 * 1024) / (4 * sizeof(T_PSUM));


        // --- 8 physical SRAM Banks ---
        // Banks 0 & 1: Weight SRAM (640 KB total)
        wei_vector_t<X_DIM, T_WEI> weight_bank_0[SRAMB_CAP_A]; // Bank 0: Lane A weights (320 KB)
        wei_vector_t<X_DIM, T_WEI> weight_bank_1[SRAMB_CAP_B]; // Bank 1: Lane B weights (320 KB)

        // Banks 2 & 3: IFmap SRAM (824 KB total)
        act_vector_t<Y_DIM, T_ACT> ifmap_bank_0[SRAMA_CAP_A];  // Bank 2: Region A activations (416 KB)
        act_vector_t<Y_DIM, T_ACT> ifmap_bank_1[SRAMA_CAP_B];  // Bank 3: Region B activations (408 KB)

        // Bank 4: PSums_A + Scratch_A (50 KB total)
        psum_vector_t<Y_DIM, T_PSUM> psums_bank_4[SRAMC_CAP_A]; // Lane A PSums (26 KB)
        uint8_t scratch_bank_4[SCRATCH_SIZE];                  // Lane A Scratch (24 KB)

        // Bank 5: PSums_B + Scratch_B (50 KB total)
        psum_vector_t<Y_DIM, T_PSUM> psums_bank_5[SRAMC_CAP_B]; // Lane B PSums (26 KB)
        uint8_t scratch_bank_5[SCRATCH_SIZE];                  // Lane B Scratch (24 KB)

        // Banks 6 & 7: Reserved for future expansion

        void beh_process()
        {
            if (!i_rstn.read())
            {
                o_host_rdata.write(host_data_t());
                o_srama_data_a.write(act_vector_t<Y_DIM, T_ACT>());
                o_srama_data_b.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data_a.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramb_data_b.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata_a.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_sramc_rdata_b.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            if (i_powergate.read())
            {
                // Reset all bank contents when power gated
                for (int d = 0; d < SRAMB_CAP_A; d++) weight_bank_0[d] = wei_vector_t<X_DIM, T_WEI>();
                for (int d = 0; d < SRAMB_CAP_B; d++) weight_bank_1[d] = wei_vector_t<X_DIM, T_WEI>();
                for (int d = 0; d < SRAMA_CAP_A; d++) ifmap_bank_0[d] = act_vector_t<Y_DIM, T_ACT>();
                for (int d = 0; d < SRAMA_CAP_B; d++) ifmap_bank_1[d] = act_vector_t<Y_DIM, T_ACT>();
                for (int d = 0; d < SRAMC_CAP_A; d++) psums_bank_4[d] = psum_vector_t<Y_DIM, T_PSUM>();
                for (int d = 0; d < SRAMC_CAP_B; d++) psums_bank_5[d] = psum_vector_t<Y_DIM, T_PSUM>();
                std::memset(scratch_bank_4, 0, SCRATCH_SIZE);
                std::memset(scratch_bank_5, 0, SCRATCH_SIZE);

                o_host_rdata.write(host_data_t());
                o_srama_data_a.write(act_vector_t<Y_DIM, T_ACT>());
                o_srama_data_b.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data_a.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramb_data_b.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata_a.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_sramc_rdata_b.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            if (i_deepsleep.read())
            {
                o_host_rdata.write(host_data_t());
                o_srama_data_a.write(act_vector_t<Y_DIM, T_ACT>());
                o_srama_data_b.write(act_vector_t<Y_DIM, T_ACT>());
                o_sramb_data_a.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramb_data_b.write(wei_vector_t<X_DIM, T_WEI>());
                o_sramc_rdata_a.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_sramc_rdata_b.write(psum_vector_t<Y_DIM, T_PSUM>());
                return;
            }

            // --------------------------------------------------------
            // 1. Host-Side Accesses (Address-based Bank Routing)
            // --------------------------------------------------------
            uint32_t addr = i_host_addr.read();
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            uint32_t local_addr = addr - mem_region;

            if (i_host_wren.read())
            {
                host_data_t wdata = i_host_wdata.read();
                host_mask_t wmask = i_host_wmask.read();

                if (mem_region == SRAMA_OFFSET)
                {
                    // IFmap SRAM: Bank 2 (Region A) / Bank 3 (Region B)
                    if (local_addr < SRAMA_REG_B_OFFSET) // Region A
                    {
                        uint32_t sub_word = local_addr & MASK_A;
                        uint32_t phys_addr = (local_addr >> SHIFT_A) % SRAMA_CAP_A;
                        for (int i = 0; i < 4; i++)
                        {
                            if (wmask[i] && (sub_word * 4 + i < Y_DIM))
                            {
                                ifmap_bank_0[phys_addr][sub_word * 4 + i] = wdata[i];
                            }
                        }
                    }
                    else if (local_addr < SRAMA_TOTAL_SIZE) // Region B
                    {
                        uint32_t local_offset = local_addr - SRAMA_REG_B_OFFSET;
                        uint32_t sub_word = local_offset & MASK_A;
                        uint32_t phys_addr = (local_offset >> SHIFT_A) % SRAMA_CAP_B;
                        for (int i = 0; i < 4; i++)
                        {
                            if (wmask[i] && (sub_word * 4 + i < Y_DIM))
                            {
                                ifmap_bank_1[phys_addr][sub_word * 4 + i] = wdata[i];
                            }
                        }
                    }
                }
                else if (mem_region == SRAMB_OFFSET)
                {
                    // Weight SRAM: Bank 0 (Lane A weights) / Bank 1 (Lane B weights)
                    if (local_addr < SRAMB_BANK_1_OFFSET) // Bank 0
                    {
                        uint32_t sub_word = local_addr & MASK_B;
                        uint32_t phys_addr = (local_addr >> SHIFT_B) % SRAMB_CAP_A;
                        for (int i = 0; i < 4; i++)
                        {
                            if (wmask[i] && (sub_word * 4 + i < X_DIM))
                            {
                                weight_bank_0[phys_addr][sub_word * 4 + i] = wdata[i];
                            }
                        }
                    }
                    else if (local_addr < SRAMB_TOTAL_SIZE) // Bank 1
                    {
                        uint32_t local_offset = local_addr - SRAMB_BANK_1_OFFSET;
                        uint32_t sub_word = local_offset & MASK_B;
                        uint32_t phys_addr = (local_offset >> SHIFT_B) % SRAMB_CAP_B;
                        for (int i = 0; i < 4; i++)
                        {
                            if (wmask[i] && (sub_word * 4 + i < X_DIM))
                            {
                                weight_bank_1[phys_addr][sub_word * 4 + i] = wdata[i];
                            }
                        }
                    }
                }
                else if (mem_region == SRAMC_OFFSET)
                {
                    // Output SRAM: Bank 4 (Lane A PSums & Scratch) / Bank 5 (Lane B PSums & Scratch)
                    if (local_addr < SRAMC_BANK_4_PSUMS) // Bank 4 PSums
                    {
                        uint32_t sub_word = local_addr & MASK_C;
                        uint32_t phys_addr = (local_addr >> SHIFT_C) % SRAMC_CAP_A;
                        for (int i = 0; i < 4; i++)
                        {
                            if (wmask[i] && (sub_word * 4 + i < Y_DIM))
                            {
                                psums_bank_4[phys_addr][sub_word * 4 + i] = wdata[i];
                            }
                        }
                    }
                    else if (local_addr < SRAMC_BANK_4_SCRATCH) // Bank 4 Scratch
                    {
                        uint32_t scratch_addr = local_addr - SRAMC_BANK_4_PSUMS;
                        for (int i = 0; i < 4; i++)
                        {
                            if (wmask[i] && (scratch_addr + i < SCRATCH_SIZE))
                            {
                                scratch_bank_4[scratch_addr + i] = static_cast<uint8_t>(wdata[i]);
                            }
                        }
                    }
                    else if (local_addr < SRAMC_BANK_5_PSUMS) // Bank 5 PSums
                    {
                        uint32_t local_offset = local_addr - SRAMC_BANK_4_SCRATCH;
                        uint32_t sub_word = local_offset & MASK_C;
                        uint32_t phys_addr = (local_offset >> SHIFT_C) % SRAMC_CAP_B;
                        for (int i = 0; i < 4; i++)
                        {
                            if (wmask[i] && (sub_word * 4 + i < Y_DIM))
                            {
                                psums_bank_5[phys_addr][sub_word * 4 + i] = wdata[i];
                            }
                        }
                    }
                    else if (local_addr < SRAMC_BANK_5_SCRATCH) // Bank 5 Scratch
                    {
                        uint32_t scratch_addr = local_addr - SRAMC_BANK_5_PSUMS;
                        for (int i = 0; i < 4; i++)
                        {
                            if (wmask[i] && (scratch_addr + i < SCRATCH_SIZE))
                            {
                                scratch_bank_5[scratch_addr + i] = static_cast<uint8_t>(wdata[i]);
                            }
                        }
                    }
                }
            }

            if (i_host_rden.read())
            {
                host_data_t rdata;
                if (mem_region == SRAMA_OFFSET)
                {
                    if (local_addr < SRAMA_REG_B_OFFSET)
                    {
                        uint32_t sub_word = local_addr & MASK_A;
                        uint32_t phys_addr = (local_addr >> SHIFT_A) % SRAMA_CAP_A;
                        for (int i = 0; i < 4; i++)
                        {
                            if (sub_word * 4 + i < Y_DIM)
                                rdata[i] = ifmap_bank_0[phys_addr][sub_word * 4 + i];
                        }
                    }
                    else if (local_addr < SRAMA_TOTAL_SIZE)
                    {
                        uint32_t local_offset = local_addr - SRAMA_REG_B_OFFSET;
                        uint32_t sub_word = local_offset & MASK_A;
                        uint32_t phys_addr = (local_offset >> SHIFT_A) % SRAMA_CAP_B;
                        for (int i = 0; i < 4; i++)
                        {
                            if (sub_word * 4 + i < Y_DIM)
                                rdata[i] = ifmap_bank_1[phys_addr][sub_word * 4 + i];
                        }
                    }
                }
                else if (mem_region == SRAMB_OFFSET)
                {
                    if (local_addr < SRAMB_BANK_1_OFFSET)
                    {
                        uint32_t sub_word = local_addr & MASK_B;
                        uint32_t phys_addr = (local_addr >> SHIFT_B) % SRAMB_CAP_A;
                        for (int i = 0; i < 4; i++)
                        {
                            if (sub_word * 4 + i < X_DIM)
                                rdata[i] = weight_bank_0[phys_addr][sub_word * 4 + i];
                        }
                    }
                    else if (local_addr < SRAMB_TOTAL_SIZE)
                    {
                        uint32_t local_offset = local_addr - SRAMB_BANK_1_OFFSET;
                        uint32_t sub_word = local_offset & MASK_B;
                        uint32_t phys_addr = (local_offset >> SHIFT_B) % SRAMB_CAP_B;
                        for (int i = 0; i < 4; i++)
                        {
                            if (sub_word * 4 + i < X_DIM)
                                rdata[i] = weight_bank_1[phys_addr][sub_word * 4 + i];
                        }
                    }
                }
                else if (mem_region == SRAMC_OFFSET)
                {
                    if (local_addr < SRAMC_BANK_4_PSUMS)
                    {
                        uint32_t sub_word = local_addr & MASK_C;
                        uint32_t phys_addr = (local_addr >> SHIFT_C) % SRAMC_CAP_A;
                        for (int i = 0; i < 4; i++)
                        {
                            if (sub_word * 4 + i < Y_DIM)
                                rdata[i] = psums_bank_4[phys_addr][sub_word * 4 + i];
                        }
                    }
                    else if (local_addr < SRAMC_BANK_4_SCRATCH)
                    {
                        uint32_t scratch_addr = local_addr - SRAMC_BANK_4_PSUMS;
                        for (int i = 0; i < 4; i++)
                        {
                            if (scratch_addr + i < SCRATCH_SIZE)
                                rdata[i] = static_cast<double>(scratch_bank_4[scratch_addr + i]);
                        }
                    }
                    else if (local_addr < SRAMC_BANK_5_PSUMS)
                    {
                        uint32_t local_offset = local_addr - SRAMC_BANK_4_SCRATCH;
                        uint32_t sub_word = local_offset & MASK_C;
                        uint32_t phys_addr = (local_offset >> SHIFT_C) % SRAMC_CAP_B;
                        for (int i = 0; i < 4; i++)
                        {
                            if (sub_word * 4 + i < Y_DIM)
                                rdata[i] = psums_bank_5[phys_addr][sub_word * 4 + i];
                        }
                    }
                    else if (local_addr < SRAMC_BANK_5_SCRATCH)
                    {
                        uint32_t scratch_addr = local_addr - SRAMC_BANK_5_PSUMS;
                        for (int i = 0; i < 4; i++)
                        {
                            if (scratch_addr + i < SCRATCH_SIZE)
                                rdata[i] = static_cast<double>(scratch_bank_5[scratch_addr + i]);
                        }
                    }
                }
                o_host_rdata.write(rdata);
            }

            // --------------------------------------------------------
            // 2. Accelerator-Side Accesses (Lane A / Lane B Split Ports)
            // --------------------------------------------------------

            // --- Lane A Accesses ---
            if (i_srama_rden_a.read())
            {
                uint32_t addr_a = i_srama_addr_a.read() % SRAMA_CAP_A;
                o_srama_data_a.write(ifmap_bank_0[addr_a]);
            }
            if (i_sramb_rden_a.read())
            {
                uint32_t addr_b = i_sramb_addr_a.read() % SRAMB_CAP_A;
                o_sramb_data_a.write(weight_bank_0[addr_b]);
            }
            if (i_sramc_wren_a.read())
            {
                uint32_t addr_c = i_sramc_addr_a.read() % SRAMC_CAP_A;
                psum_vector_t<Y_DIM, T_PSUM> wdata_c = i_sramc_wdata_a.read();
                sramc_mask_t<Y_DIM> wmask_c = i_sramc_wmask_a.read();
                for (int i = 0; i < Y_DIM; i++)
                {
                    if (wmask_c[i])
                        psums_bank_4[addr_c][i] = wdata_c[i];
                }
            }
            if (i_sramc_rden_a.read())
            {
                uint32_t addr_c = i_sramc_addr_a.read() % SRAMC_CAP_A;
                o_sramc_rdata_a.write(psums_bank_4[addr_c]);
            }

            // --- Lane B Accesses ---
            if (i_srama_rden_b.read())
            {
                uint32_t addr_a = i_srama_addr_b.read() % SRAMA_CAP_B;
                o_srama_data_b.write(ifmap_bank_1[addr_a]);
            }
            if (i_sramb_rden_b.read())
            {
                uint32_t addr_b = i_sramb_addr_b.read() % SRAMB_CAP_B;
                o_sramb_data_b.write(weight_bank_1[addr_b]);
            }
            if (i_sramc_wren_b.read())
            {
                uint32_t addr_c = i_sramc_addr_b.read() % SRAMC_CAP_B;
                psum_vector_t<Y_DIM, T_PSUM> wdata_c = i_sramc_wdata_b.read();
                sramc_mask_t<Y_DIM> wmask_c = i_sramc_wmask_b.read();
                for (int i = 0; i < Y_DIM; i++)
                {
                    if (wmask_c[i])
                        psums_bank_5[addr_c][i] = wdata_c[i];
                }
            }
            if (i_sramc_rden_b.read())
            {
                uint32_t addr_c = i_sramc_addr_b.read() % SRAMC_CAP_B;
                o_sramc_rdata_b.write(psums_bank_5[addr_c]);
            }
        }

    public:
        // DMA Direct Read/Write Access
        void write_bank_data(int bank_id, uint32_t offset_bytes, const uint8_t* src_data, uint32_t size_bytes)
        {
            uint32_t written = 0;
            while (written < size_bytes)
            {
                uint32_t curr_offset = offset_bytes + written;
                uint8_t byte_val = src_data[written];
                
                if (bank_id == 0) // weight_bank_0
                {
                    uint32_t row_size = X_DIM * sizeof(T_WEI);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (row_idx < SRAMB_CAP_A)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_WEI);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_WEI);
                        uint8_t* ptr = reinterpret_cast<uint8_t*>(&weight_bank_0[row_idx].data[elem_idx]);
                        ptr[byte_in_elem] = byte_val;
                    }
                }
                else if (bank_id == 1) // weight_bank_1
                {
                    uint32_t row_size = X_DIM * sizeof(T_WEI);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (row_idx < SRAMB_CAP_B)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_WEI);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_WEI);
                        uint8_t* ptr = reinterpret_cast<uint8_t*>(&weight_bank_1[row_idx].data[elem_idx]);
                        ptr[byte_in_elem] = byte_val;
                    }
                }
                else if (bank_id == 2) // ifmap_bank_0 (Bank 2 activations)
                {
                    uint32_t row_size = Y_DIM * sizeof(T_ACT);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (row_idx < SRAMA_CAP_A)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_ACT);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_ACT);
                        uint8_t* ptr = reinterpret_cast<uint8_t*>(&ifmap_bank_0[row_idx].data[elem_idx]);
                        ptr[byte_in_elem] = byte_val;
                    }
                }
                else if (bank_id == 3) // ifmap_bank_1 (Bank 3 activations)
                {
                    uint32_t row_size = Y_DIM * sizeof(T_ACT);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (row_idx < SRAMA_CAP_B)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_ACT);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_ACT);
                        uint8_t* ptr = reinterpret_cast<uint8_t*>(&ifmap_bank_1[row_idx].data[elem_idx]);
                        ptr[byte_in_elem] = byte_val;
                    }
                }
                else if (bank_id == 4) // Bank 4: PSums A + Scratch A
                {
                    uint32_t psums_size = SRAMC_CAP_A * Y_DIM * sizeof(T_PSUM);
                    if (curr_offset < psums_size)
                    {
                        uint32_t row_size = Y_DIM * sizeof(T_PSUM);
                        uint32_t row_idx = curr_offset / row_size;
                        uint32_t byte_in_row = curr_offset % row_size;
                        uint32_t elem_idx = byte_in_row / sizeof(T_PSUM);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_PSUM);
                        uint8_t* ptr = reinterpret_cast<uint8_t*>(&psums_bank_4[row_idx].data[elem_idx]);
                        ptr[byte_in_elem] = byte_val;
                    }
                    else
                    {
                        uint32_t scratch_offset = curr_offset - psums_size;
                        if (scratch_offset < SCRATCH_SIZE)
                        {
                            scratch_bank_4[scratch_offset] = byte_val;
                        }
                    }
                }
                else if (bank_id == 5) // Bank 5: PSums B + Scratch B
                {
                    uint32_t psums_size = SRAMC_CAP_B * Y_DIM * sizeof(T_PSUM);
                    if (curr_offset < psums_size)
                    {
                        uint32_t row_size = Y_DIM * sizeof(T_PSUM);
                        uint32_t row_idx = curr_offset / row_size;
                        uint32_t byte_in_row = curr_offset % row_size;
                        uint32_t elem_idx = byte_in_row / sizeof(T_PSUM);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_PSUM);
                        uint8_t* ptr = reinterpret_cast<uint8_t*>(&psums_bank_5[row_idx].data[elem_idx]);
                        ptr[byte_in_elem] = byte_val;
                    }
                    else
                    {
                        uint32_t scratch_offset = curr_offset - psums_size;
                        if (scratch_offset < SCRATCH_SIZE)
                        {
                            scratch_bank_5[scratch_offset] = byte_val;
                        }
                    }
                }
                written++;
            }
        }

        void read_bank_data(int bank_id, uint32_t offset_bytes, uint8_t* dest_data, uint32_t size_bytes)
        {
            uint32_t read_bytes = 0;
            while (read_bytes < size_bytes)
            {
                uint32_t curr_offset = offset_bytes + read_bytes;
                uint8_t byte_val = 0;
                
                if (bank_id == 0)
                {
                    uint32_t row_size = X_DIM * sizeof(T_WEI);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (row_idx < SRAMB_CAP_A)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_WEI);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_WEI);
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&weight_bank_0[row_idx].data[elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                }
                else if (bank_id == 1)
                {
                    uint32_t row_size = X_DIM * sizeof(T_WEI);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (row_idx < SRAMB_CAP_B)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_WEI);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_WEI);
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&weight_bank_1[row_idx].data[elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                }
                else if (bank_id == 2)
                {
                    uint32_t row_size = Y_DIM * sizeof(T_ACT);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (row_idx < SRAMA_CAP_A)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_ACT);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_ACT);
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&ifmap_bank_0[row_idx].data[elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                }
                else if (bank_id == 3)
                {
                    uint32_t row_size = Y_DIM * sizeof(T_ACT);
                    uint32_t row_idx = curr_offset / row_size;
                    uint32_t byte_in_row = curr_offset % row_size;
                    if (row_idx < SRAMA_CAP_B)
                    {
                        uint32_t elem_idx = byte_in_row / sizeof(T_ACT);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_ACT);
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&ifmap_bank_1[row_idx].data[elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                }
                else if (bank_id == 4)
                {
                    uint32_t psums_size = SRAMC_CAP_A * Y_DIM * sizeof(T_PSUM);
                    if (curr_offset < psums_size)
                    {
                        uint32_t row_size = Y_DIM * sizeof(T_PSUM);
                        uint32_t row_idx = curr_offset / row_size;
                        uint32_t byte_in_row = curr_offset % row_size;
                        uint32_t elem_idx = byte_in_row / sizeof(T_PSUM);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_PSUM);
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&psums_bank_4[row_idx].data[elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                    else
                    {
                        uint32_t scratch_offset = curr_offset - psums_size;
                        if (scratch_offset < SCRATCH_SIZE)
                        {
                            byte_val = scratch_bank_4[scratch_offset];
                        }
                    }
                }
                else if (bank_id == 5)
                {
                    uint32_t psums_size = SRAMC_CAP_B * Y_DIM * sizeof(T_PSUM);
                    if (curr_offset < psums_size)
                    {
                        uint32_t row_size = Y_DIM * sizeof(T_PSUM);
                        uint32_t row_idx = curr_offset / row_size;
                        uint32_t byte_in_row = curr_offset % row_size;
                        uint32_t elem_idx = byte_in_row / sizeof(T_PSUM);
                        uint32_t byte_in_elem = byte_in_row % sizeof(T_PSUM);
                        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&psums_bank_5[row_idx].data[elem_idx]);
                        byte_val = ptr[byte_in_elem];
                    }
                    else
                    {
                        uint32_t scratch_offset = curr_offset - psums_size;
                        if (scratch_offset < SCRATCH_SIZE)
                        {
                            byte_val = scratch_bank_5[scratch_offset];
                        }
                    }
                }
                dest_data[read_bytes] = byte_val;
                read_bytes++;
            }
        }
    };

} // namespace sauria

#endif // SAURIA_SRAM_TOP_H

