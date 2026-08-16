// SystemC Model for SAURIA NPU Core
// Output Boundary Pipeline (OBP) with Parameterized Dimensions and Datatypes
// Supports Bias Addition, Requantization, LUT-based Activations, and Residual Addition
// Encapsulates 3-cycle control pipeline (address, mask, wren) for synchronous SRAM writeback

#ifndef SAURIA_OBP_TOP_H
#define SAURIA_OBP_TOP_H

#include <systemc.h>
#include <type_traits>
#include "sauria_types.h"
#include "debug.h"

#ifndef FX1_NO_PERF
#include "instrumentation/perf_counters.h"
#endif

namespace sauria
{
    template <
        int Y_DIM = 32, // Width of the boundary processing array
        uint32_t LUT_OFFSET = 0x00140000,
        uint32_t BIAS_OFFSET = 0x00150000,
        typename T_PSUM = float,
        typename T_ACT = float>
    class Obp : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Data & Control Inputs from PSM / Systolic Array
        sc_in<psum_vector_t<Y_DIM, T_PSUM>> i_data{"i_data"};        // Input from PSM
        sc_in<uint32_t> i_addr{"i_addr"};                            // SRAM C target address
        sc_in<sramc_mask_t<Y_DIM>> i_wmask{"i_wmask"};              // Write mask
        sc_in<bool> i_valid{"i_valid"};

        // Residual skip input
        sc_in<act_vector_t<Y_DIM, T_ACT>> i_residual{"i_residual"};  // Residual skip input

        // Data & Control Outputs to SRAM C (pipelined)
        sc_out<psum_vector_t<Y_DIM, T_PSUM>> o_sramc_wdata{"o_sramc_wdata"};
        sc_out<uint32_t> o_sramc_addr{"o_sramc_addr"};
        sc_out<bool> o_sramc_wren{"o_sramc_wren"};
        sc_out<sramc_mask_t<Y_DIM>> o_sramc_wmask{"o_sramc_wmask"};
        sc_out<bool> o_valid{"o_valid"};

        // Config Controls
        sc_in<bool> i_bias_en{"i_bias_en"};
        sc_in<bool> i_requant_en{"i_requant_en"};
        sc_in<bool> i_lut_en{"i_lut_en"};
        sc_in<bool> i_residual_en{"i_residual_en"};
        sc_in<uint32_t> i_requant_scale{"i_requant_scale"}; // Requantization scale/multiplier
        sc_in<uint32_t> i_requant_shift{"i_requant_shift"}; // Requantization right shift bits

        // Host Programming Interface (for LUT RAM and Bias RAM)
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"};
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"};
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

#ifndef FX1_NO_PERF
        fx1::PerfCounters *perf{nullptr};
#endif

        SC_CTOR(Obp)
        {
            SC_METHOD(pipeline_process);
            sensitive << i_clk.pos();

            // Initialize memories
            for (int l = 0; l < Y_DIM; l++)
            {
                std::memset(lut_ram[l], 0, 256);
                bias_ram[l] = 0;
                scale_ram[l] = 0;
                scale_ram_valid[l] = false;
                shift_ram[l] = 0;
                shift_ram_valid[l] = false;
            }
        }

    private:
        // Internal Storage: 
        // 64-lane pipelined LUT RAM (each lane has 256 entries of 8-bit data)
        uint8_t lut_ram[Y_DIM][256];
        // Bias RAM (one 32-bit entry per lane)
        int32_t bias_ram[Y_DIM];
        // 64-lane scale RAM (one 32-bit scale multiplier per lane)
        uint32_t scale_ram[Y_DIM];
        bool scale_ram_valid[Y_DIM];
        // 64-lane shift RAM (one 32-bit shift value per lane)
        uint32_t shift_ram[Y_DIM];
        bool shift_ram_valid[Y_DIM];

        // Pipeline stage registers (storing controls together with data for timing alignment)
        struct Stage1Reg
        {
            bool valid{false};
            psum_vector_t<Y_DIM, T_PSUM> biased_data;
            act_vector_t<Y_DIM, T_ACT> residual;
            uint32_t addr{0};
            sramc_mask_t<Y_DIM> wmask;
        } stage1_reg;

        struct Stage2Reg
        {
            bool valid{false};
            psum_vector_t<Y_DIM, T_PSUM> requant_data;
            act_vector_t<Y_DIM, T_ACT> residual;
            uint32_t addr{0};
            sramc_mask_t<Y_DIM> wmask;
        } stage2_reg;

        struct Stage3Reg
        {
            bool valid{false};
            psum_vector_t<Y_DIM, T_PSUM> activated_data;
            act_vector_t<Y_DIM, T_ACT> residual;
            uint32_t addr{0};
            sramc_mask_t<Y_DIM> wmask;
        } stage3_reg;

        // Saturate/clamp helper for integer & floating-point types
        template <typename T>
        static T clamp_val(double val)
        {
            if (std::is_integral<T>::value)
            {
                if (std::is_signed<T>::value)
                {
                    if (val > 127.0) return 127;
                    if (val < -128.0) return -128;
                }
                else
                {
                    if (val > 255.0) return 255;
                    if (val < 0.0) return 0;
                }
                return static_cast<T>(val);
            }
            else
            {
                return static_cast<T>(val);
            }
        }

        void pipeline_process()
        {
            if (!i_rstn.read())
            {
                // Reset memories
                for (int l = 0; l < Y_DIM; l++)
                {
                    std::memset(lut_ram[l], 0, 256);
                    bias_ram[l] = 0;
                    scale_ram[l] = 0;
                    scale_ram_valid[l] = false;
                    shift_ram[l] = 0;
                    shift_ram_valid[l] = false;
                }

                // Reset pipeline registers
                stage1_reg = Stage1Reg();
                stage2_reg = Stage2Reg();
                stage3_reg = Stage3Reg();

                o_sramc_wdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_sramc_addr.write(0);
                o_sramc_wren.write(false);
                o_sramc_wmask.write(sramc_mask_t<Y_DIM>());
                o_valid.write(false);
                o_host_rdata.write(host_data_t());
                return;
            }

            // --------------------------------------------------------
            // 0. Host Interface: Program / Read LUT & Bias RAMs
            // --------------------------------------------------------
            uint32_t addr = i_host_addr.read();
            uint32_t region = addr & 0x00FF0000;
            uint32_t offset = addr & 0x0000FFFF;

            if (i_host_wren.read())
            {
                host_data_t wdata = i_host_wdata.read();
                host_mask_t wmask = i_host_wmask.read();

                if (region == LUT_OFFSET)
                {
                    // offset = lane_idx * 256 + entry_idx
                    uint32_t lane_idx = (offset >> 8) % Y_DIM;
                    uint32_t entry_idx = offset & 0xFF;
                    uint32_t base_entry = entry_idx & ~3; // 4-byte aligned write grouping
                    
                    for (int i = 0; i < 4; i++)
                    {
                        if (wmask[i] && (base_entry + i < 256))
                        {
                            lut_ram[lane_idx][base_entry + i] = static_cast<uint8_t>(wdata[i]);
                        }
                    }
                }
                else if (region == BIAS_OFFSET)
                {
                    // offset = lane_idx * 4
                    uint32_t lane_idx = (offset >> 2) % Y_DIM;
                    if (wmask[0])
                    {
                        bias_ram[lane_idx] = static_cast<int32_t>(wdata[0]);
                    }
                }
                else if (region == (LUT_OFFSET + 0x00040000))
                {
                    // SCALE_OFFSET: offset = lane_idx * 4
                    uint32_t lane_idx = (offset >> 2) % Y_DIM;
                    if (wmask[0])
                    {
                        scale_ram[lane_idx] = static_cast<uint32_t>(wdata[0]);
                        scale_ram_valid[lane_idx] = true;
                    }
                }
                else if (region == (LUT_OFFSET + 0x00050000))
                {
                    // SHIFT_OFFSET: offset = lane_idx * 4
                    uint32_t lane_idx = (offset >> 2) % Y_DIM;
                    if (wmask[0])
                    {
                        shift_ram[lane_idx] = static_cast<uint32_t>(wdata[0]);
                        shift_ram_valid[lane_idx] = true;
                    }
                }
            }

            if (i_host_rden.read())
            {
                host_data_t rdata;
                if (region == LUT_OFFSET)
                {
                    uint32_t lane_idx = (offset >> 8) % Y_DIM;
                    uint32_t entry_idx = offset & 0xFF;
                    uint32_t base_entry = entry_idx & ~3;
                    for (int i = 0; i < 4; i++)
                    {
                        if (base_entry + i < 256)
                        {
                            rdata[i] = static_cast<double>(lut_ram[lane_idx][base_entry + i]);
                        }
                    }
                }
                else if (region == BIAS_OFFSET)
                {
                    uint32_t lane_idx = (offset >> 2) % Y_DIM;
                    rdata[0] = static_cast<double>(bias_ram[lane_idx]);
                }
                else if (region == (LUT_OFFSET + 0x00040000))
                {
                    uint32_t lane_idx = (offset >> 2) % Y_DIM;
                    rdata[0] = static_cast<double>(scale_ram[lane_idx]);
                }
                else if (region == (LUT_OFFSET + 0x00050000))
                {
                    uint32_t lane_idx = (offset >> 2) % Y_DIM;
                    rdata[0] = static_cast<double>(shift_ram[lane_idx]);
                }
                o_host_rdata.write(rdata);
            }

            // --------------------------------------------------------
            // 1. Stage 1: Bias Addition (INT32 + INT32)
            // --------------------------------------------------------
            Stage1Reg next_stage1;
            next_stage1.valid = i_valid.read();
            if (i_valid.read())
            {
                psum_vector_t<Y_DIM, T_PSUM> in_val = i_data.read();
                next_stage1.residual = i_residual.read();
                next_stage1.addr = i_addr.read();
                next_stage1.wmask = i_wmask.read();

                for (int l = 0; l < Y_DIM; l++)
                {
                    int32_t bias_val = i_bias_en.read() ? bias_ram[l] : 0;
                    next_stage1.biased_data[l] = in_val[l] + bias_val;
                }
            }

            // --------------------------------------------------------
            // 2. Stage 2: Requantization (INT32 -> INT8)
            // --------------------------------------------------------
            Stage2Reg next_stage2;
            next_stage2.valid = stage1_reg.valid;
            if (stage1_reg.valid)
            {
                next_stage2.residual = stage1_reg.residual;
                next_stage2.addr = stage1_reg.addr;
                next_stage2.wmask = stage1_reg.wmask;
                
                uint64_t scale_default = i_requant_scale.read();
                uint32_t shift_default = i_requant_shift.read();
                bool requant_en = i_requant_en.read();

                for (int l = 0; l < Y_DIM; l++)
                {
                    double val = static_cast<double>(stage1_reg.biased_data[l]);
                    if (requant_en)
                    {
                        uint64_t scale = scale_ram_valid[l] ? scale_ram[l] : scale_default;
                        uint32_t shift = shift_ram_valid[l] ? shift_ram[l] : shift_default;

                        if (std::is_integral<T_PSUM>::value)
                        {
                            int64_t product = static_cast<int64_t>(stage1_reg.biased_data[l]) * scale;
                            val = static_cast<double>(product >> shift);
                        }
                        else
                        {
                            // Float path uses multiplier scaling directly
                            val = val * static_cast<double>(scale) / static_cast<double>(1ULL << shift);
                        }
                        next_stage2.requant_data[l] = static_cast<T_PSUM>(clamp_val<T_ACT>(val));
                    }
                    else
                    {
                        next_stage2.requant_data[l] = static_cast<T_PSUM>(val);
                    }
                }
            }

            // --------------------------------------------------------
            // 3. Stage 3: Pipelined LUT Lookup / Activation
            // --------------------------------------------------------
            Stage3Reg next_stage3;
            next_stage3.valid = stage2_reg.valid;
            if (stage2_reg.valid)
            {
                next_stage3.residual = stage2_reg.residual;
                next_stage3.addr = stage2_reg.addr;
                next_stage3.wmask = stage2_reg.wmask;
                
                bool lut_en = i_lut_en.read();

                for (int l = 0; l < Y_DIM; l++)
                {
                    T_PSUM val = stage2_reg.requant_data[l];
                    if (lut_en)
                    {
                        // Map signed INT8 (-128 to 127) to LUT index (0 to 255)
                        int32_t val_int = static_cast<int32_t>(clamp_val<T_ACT>(val));
                        uint8_t index = static_cast<uint8_t>(val_int + 128);
                        uint8_t lut_val = lut_ram[l][index];
                        // Convert unsigned 8-bit back to signed INT8 (-128 to 127)
                        next_stage3.activated_data[l] = static_cast<T_PSUM>(static_cast<int8_t>(lut_val));
                    }
                    else
                    {
                        next_stage3.activated_data[l] = val;
                    }
                }
            }

            // --------------------------------------------------------
            // 4. Stage 4: Residual Addition & Final Writeback
            // --------------------------------------------------------
            if (stage3_reg.valid)
            {
                psum_vector_t<Y_DIM, T_PSUM> out_val;
                bool residual_en = i_residual_en.read();
                bool requant_en = i_requant_en.read();
                bool lut_en = i_lut_en.read();
                for (int l = 0; l < Y_DIM; l++)
                {
                    double val = static_cast<double>(stage3_reg.activated_data[l]);
                    if (residual_en)
                    {
                        val += static_cast<double>(stage3_reg.residual[l]);
                    }
                    if (requant_en || lut_en)
                    {
                        out_val[l] = static_cast<T_PSUM>(clamp_val<T_ACT>(val));
                    }
                    else
                    {
                        out_val[l] = static_cast<T_PSUM>(val);
                    }
                }
                o_sramc_wdata.write(out_val);
                o_sramc_addr.write(stage3_reg.addr);
                o_sramc_wmask.write(stage3_reg.wmask);
                o_sramc_wren.write(true);
                o_valid.write(true);
            }
            else
            {
                o_sramc_wdata.write(psum_vector_t<Y_DIM, T_PSUM>());
                o_sramc_addr.write(0);
                o_sramc_wmask.write(sramc_mask_t<Y_DIM>());
                o_sramc_wren.write(false);
                o_valid.write(false);
            }

            // Advance pipeline registers
            stage1_reg = next_stage1;
            stage2_reg = next_stage2;
            stage3_reg = next_stage3;

#ifndef FX1_NO_PERF
            if (perf && (i_valid.read() || stage1_reg.valid || stage2_reg.valid || stage3_reg.valid))
            {
                perf->obp_cycles++;
            }
#endif
        }
    };
} // namespace sauria

#endif // SAURIA_OBP_TOP_H
