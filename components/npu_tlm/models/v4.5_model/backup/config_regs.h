// SystemC Model for SAURIA NPU Core
// Configuration Registers Block (config_regs.h)

#ifndef SAURIA_UNIFIED_CONFIG_REGS_H
#define SAURIA_UNIFIED_CONFIG_REGS_H

#include "sauria_types.h"
#include "debug.h"
#include "npu_profile.h"
#include "config_map.h"

namespace sauria
{

    template <
        int IF_W_PARAM = 32,
        int IF_ADR_W_PARAM = 32,
        int X_DIM = 64,
        int Y_DIM = 64,
        int TH_W_PARAM = 2,
        int ACT_IDX_W_PARAM = 15,
        int WEI_IDX_W_PARAM = 15,
        int OUT_IDX_W_PARAM = 15,
        // int PARAMS_W_PAPAM = 8 --> Not used in this model
        int DILP_W_PARAM = 64,
        int SRAMB_N_PARAM = 8>
    class ConfigRegs : public sc_module
    {
    public:
        // Clock & Reset
        sc_in<bool> i_clk{"i_clk"};
        sc_in<bool> i_rstn{"i_rstn"};

        // Host Config/AXI interface
        sc_in<uint32_t> i_host_addr{"i_host_addr"};
        sc_in<bool> i_host_wren{"i_host_wren"};
        sc_in<bool> i_host_rden{"i_host_rden"};
        sc_in<host_data_t> i_host_wdata{"i_host_wdata"};
        sc_in<host_mask_t> i_host_wmask{"i_host_wmask"};
        sc_out<host_data_t> o_host_rdata{"o_host_rdata"};

        // Status inputs from NPU Core
        sc_in<bool> i_done{"i_done"};
        sc_in<bool> i_soft_reset_in{"i_soft_reset_in"};
        sc_in<bool> i_active{"i_active"};

        // Handshake and Reset Control Outputs
        sc_out<bool> o_start{"o_start"};
        sc_out<bool> o_soft_reset{"o_soft_reset"};

        // Runtime PROFILE selector broadcast to the rest of the NPU (feeders/psm/controller
        // pick LINEAR vs SAURIA behavior). Latched from the PROFILE config register.
        sc_out<uint32_t> o_profile{"o_profile"};

        // Ouput to activation feeders
        sc_out<uint32_t> o_act_incntlim{"o_act_incntlim"};
        sc_out<uint32_t> o_act_incntstep{"o_act_incntstep"};
        sc_out<uint32_t> o_act_outcntlim{"o_act_outcntlim"};
        sc_out<uint32_t> o_act_outcntstep{"o_act_outcntstep"};
        // Full SAURIA IFMAP runtime config
        sc_out<uint32_t> o_act_xlim{"o_act_xlim"};
        sc_out<uint32_t> o_act_xstep{"o_act_xstep"};
        sc_out<uint32_t> o_act_ylim{"o_act_ylim"};
        sc_out<uint32_t> o_act_ystep{"o_act_ystep"};
        sc_out<uint32_t> o_act_chlim{"o_act_chlim"};
        sc_out<uint32_t> o_act_chstep{"o_act_chstep"};
        sc_out<uint32_t> o_act_til_xlim{"o_act_til_xlim"};
        sc_out<uint32_t> o_act_til_xstep{"o_act_til_xstep"};
        sc_out<uint32_t> o_act_til_ylim{"o_act_til_ylim"};
        sc_out<uint32_t> o_act_til_ystep{"o_act_til_ystep"};

        // Output to weight feeders
        sc_out<uint32_t> o_wei_incntlim{"o_wei_incntlim"};
        sc_out<uint32_t> o_wei_incntstep{"o_wei_incntstep"};
        // Full SAURIA WEIGHT runtime config
        sc_out<uint32_t> o_wei_wlim{"o_wei_wlim"};
        sc_out<uint32_t> o_wei_wstep{"o_wei_wstep"};
        sc_out<uint32_t> o_wei_klim{"o_wei_klim"};
        sc_out<uint32_t> o_wei_kstep{"o_wei_kstep"};
        sc_out<uint32_t> o_wei_til_klim{"o_wei_til_klim"};
        sc_out<uint32_t> o_wei_til_kstep{"o_wei_til_kstep"};
        sc_out<uint32_t> o_wei_cols_active{"o_wei_cols_active"};
        sc_out<uint32_t> o_wei_waligned{"o_wei_waligned"};

        // Output to PSM feeders
        sc_out<uint32_t> o_cxlim{"o_cxlim"};
        sc_out<uint32_t> o_cxstep{"o_cxstep"};
        sc_out<uint32_t> o_cklim{"o_cklim"};
        sc_out<uint32_t> o_ckstep{"o_ckstep"};
        // Full SAURIA OUTPUT / PSM runtime config
        sc_out<uint32_t> o_out_ncontexts{"o_out_ncontexts"};
        sc_out<uint32_t> o_out_til_cylim{"o_out_til_cylim"};
        sc_out<uint32_t> o_out_til_cystep{"o_out_til_cystep"};
        sc_out<uint32_t> o_out_til_cklim{"o_out_til_cklim"};
        sc_out<uint32_t> o_out_til_ckstep{"o_out_til_ckstep"};
        sc_out<uint32_t> o_out_inactive_cols{"o_out_inactive_cols"};
        sc_out<bool> o_out_preload_en{"o_out_preload_en"};

        // Ouput to Memory (sauria_type)
        sc_out<uint32_t> o_act_base_addr{"o_act_base_addr"};
        sc_out<uint32_t> o_wei_base_addr{"o_wei_base_addr"};
        sc_out<uint32_t> o_out_base_addr{"o_out_base_addr"};

        // Outputs to NPU modules
        sc_out<uint32_t> o_incntlim{"o_incntlim"};
        sc_out<uint32_t> o_act_reps{"o_act_reps"};
        sc_out<uint32_t> o_wei_reps{"o_wei_reps"};
        sc_out<sc_bv<DILP_W_PARAM>> o_dil_pat{"o_dil_pat"};
        sc_out<sramc_mask_t<Y_DIM>> o_rows_active{"o_rows_active"};
        sc_out<uint32_t> o_nsplit{"o_nsplit"};
        sc_out<uint32_t> o_obp_cfg_a{"o_obp_cfg_a"};
        sc_out<uint32_t> o_requant_scale_a{"o_requant_scale_a"};
        sc_out<uint32_t> o_requant_shift_a{"o_requant_shift_a"};
        sc_out<uint32_t> o_obp_cfg_b{"o_obp_cfg_b"};
        sc_out<uint32_t> o_requant_scale_b{"o_requant_scale_b"};
        sc_out<uint32_t> o_requant_shift_b{"o_requant_shift_b"};

        // output run-time
        sc_out<uint32_t> o_in_h{"o_in_h"};
        sc_out<uint32_t> o_in_w{"o_in_w"};
        sc_out<uint32_t> o_in_c{"o_in_c"};
        // sc_out<uint32_t> o_out_h{"o_out_h"};
        // sc_out<uint32_t> o_out_w{"o_out_w"};
        // sc_out<uint32_t> o_out_c{"o_out_c"};

        sc_out<uint32_t> o_kernel_h{"o_kernel_h"};
        sc_out<uint32_t> o_kernel_w{"o_kernel_w"};

        sc_out<uint32_t> o_stride{"o_stride"};
        sc_out<uint32_t> o_padding{"o_padding"};
        sc_out<uint32_t> o_dilation{"o_dilation"};

        sc_out<uint32_t> o_tile_x{"o_tile_x"};
        sc_out<uint32_t> o_tile_y{"o_tile_y"};
        sc_out<uint32_t> o_tile_k{"o_tile_k"};
        sc_out<uint32_t> o_tile_c{"o_tile_c"};

        sc_out<uint32_t> o_x_used{"o_x_used"};
        sc_out<uint32_t> o_y_used{"o_y_used"};

        SC_CTOR(ConfigRegs)
        {
            SC_METHOD(reg_process);
            sensitive << i_clk.pos();
        }

    private:
        // Internal Register State
        uint32_t r_incntlim{96};
        uint32_t r_act_reps{64};
        uint32_t r_wei_reps{64};
        sc_bv<DILP_W_PARAM> r_dil_pat{1};
        sramc_mask_t<Y_DIM> r_rows_active{true};
        uint32_t r_nsplit{Y_DIM / 2};
        uint32_t r_obp_cfg_a{0};
        uint32_t r_requant_scale_a{1};
        uint32_t r_requant_shift_a{0};
        uint32_t r_obp_cfg_b{0};
        uint32_t r_requant_scale_b{1};
        uint32_t r_requant_shift_b{0};

        // Additional registers for activation feeder configuration
        uint32_t r_act_incntlim{96};
        uint32_t r_act_incntstep{1};
        uint32_t r_act_outcntlim{96};
        uint32_t r_act_outcntstep{1};
        uint32_t r_act_xlim{0};
        uint32_t r_act_xstep{1};
        uint32_t r_act_ylim{0};
        uint32_t r_act_ystep{1};
        uint32_t r_act_chlim{0};
        uint32_t r_act_chstep{1};

        uint32_t r_act_til_xlim{0};
        uint32_t r_act_til_xstep{1};
        uint32_t r_act_til_ylim{0};
        uint32_t r_act_til_ystep{1};

        // Additional registers for weight feeder configuration
        uint32_t r_wei_incntlim{96};
        uint32_t r_wei_incntstep{1};
        uint32_t r_wei_wlim{0};
        uint32_t r_wei_wstep{1};
        uint32_t r_wei_klim{0};
        uint32_t r_wei_kstep{1};
        uint32_t r_wei_til_klim{0};
        uint32_t r_wei_til_kstep{1};
        uint32_t r_wei_cols_active{0};
        uint32_t r_wei_waligned{0};

        // Additional registers for PSM configuration
        uint32_t r_cxlim{96};
        uint32_t r_cxstep{1};
        uint32_t r_cklim{96};
        uint32_t r_ckstep{1};
        uint32_t r_out_ncontexts{1};
        uint32_t r_out_til_cklim{0};
        uint32_t r_out_til_cylim{0};
        uint32_t r_out_til_cystep{0};
        uint32_t r_out_til_ckstep{1};
        uint32_t r_out_inactive_cols{0};
        bool r_out_preload_en{false};

        // Additional registers for memory
        uint32_t r_act_base_addr{0};
        uint32_t r_wei_base_addr{0};
        uint32_t r_out_base_addr{0};

        // Additional for layer
        uint32_t r_in_h{0};
        uint32_t r_in_w{0};
        uint32_t r_in_c{0};

        uint32_t r_kernel_h{0};
        uint32_t r_kernel_w{0};

        uint32_t r_stride{0};
        uint32_t r_padding{0};
        uint32_t r_dilation{0};

        uint32_t r_tile_x{0};
        uint32_t r_tile_y{0};
        uint32_t r_tile_k{0};
        uint32_t r_tile_c{0};

        uint32_t r_x_used{0};
        uint32_t r_y_used{0};

        // Control flags
        bool r_start{false};
        bool r_done{false};
        bool r_soft_reset{false};

        // Active runtime profile (default keeps current v1/SAURIA semantics).
        NpuProfile r_profile{SAURIA_DEFAULT_PROFILE};

        // Table-driven decode of a host write under the V4 (LINEAR) profile.
        // Maps (V4 address) -> register via cfg_lookup() so layout lives in config_map.h.
        void decode_v4_write(uint32_t local_addr,
                             const host_data_t &wdata,
                             const host_mask_t &wmask)
        {
            CfgField f = cfg_lookup(PROFILE_V4_LINEAR, local_addr);

            if (f == F_ROWS_ACTIVE)
            {
                for (int i = 0; i < Y_DIM; i++)
                {
                    int byte_idx = i / 8;
                    int bit_idx = i % 8;
                    if (byte_idx < 4 && wmask[byte_idx])
                    {
                        uint32_t val = (uint32_t)wdata[byte_idx];
                        r_rows_active[i] = (val & (1 << bit_idx)) != 0;
                    }
                }
                return;
            }
            if (f == F_DIL_PAT)
            {
                if (wmask[0])
                    r_dil_pat = (uint32_t)wdata[0];
                return;
            }
            if (!wmask[0])
                return;

            uint32_t v = (uint32_t)wdata[0];
            switch (f)
            {
            case F_INCNTLIM:       r_incntlim = v; break;
            case F_ACT_REPS:       r_act_reps = v; break;
            case F_WEI_REPS:       r_wei_reps = v; break;
            case F_NCONTEXTS:      r_out_ncontexts = v; break;
            case F_PRELOAD_EN:     r_out_preload_en = (v != 0); break;
            case F_ACT_INCNTLIM:   r_act_incntlim = v; break;
            case F_ACT_INCNTSTEP:  r_act_incntstep = v; break;
            case F_ACT_OUTCNTLIM:  r_act_outcntlim = v; break;
            case F_ACT_OUTCNTSTEP: r_act_outcntstep = v; break;
            case F_WEI_INCNTLIM:   r_wei_incntlim = v; break;
            case F_WEI_INCNTSTEP:  r_wei_incntstep = v; break;
            case F_CXLIM:          r_cxlim = v; break;
            case F_CXSTEP:         r_cxstep = v; break;
            case F_CKLIM:          r_cklim = v; break;
            case F_CKSTEP:         r_ckstep = v; break;
            case F_TIL_CYLIM:      r_out_til_cylim = v; break;
            case F_TIL_CYSTEP:     r_out_til_cystep = v; break;
            case F_TIL_CKLIM:      r_out_til_cklim = v; break;
            case F_TIL_CKSTEP:     r_out_til_ckstep = v; break;
            case F_NSPLIT:
                if (!i_active.read())
                {
                    r_nsplit = v;
                }
                else
                {
                    DBG_COUT << "[CONFIG_REGS WARNING] Rejected mid-tile N_split write to " << v << " (only allowed at layer boundary)" << std::endl;
                }
                break;
            case F_OBP_CFG_A:        r_obp_cfg_a = v; break;
            case F_REQUANT_SCALE_A:  r_requant_scale_a = v; break;
            case F_REQUANT_SHIFT_A:  r_requant_shift_a = v; break;
            case F_OBP_CFG_B:        r_obp_cfg_b = v; break;
            case F_REQUANT_SCALE_B:  r_requant_scale_b = v; break;
            case F_REQUANT_SHIFT_B:  r_requant_shift_b = v; break;
            default: break; // F_NONE / unmapped -> ignore
            }
        }

        // Current value of a scalar field (for read-back).
        uint32_t field_value(CfgField f)
        {
            switch (f)
            {
            case F_INCNTLIM:       return r_incntlim;
            case F_ACT_REPS:       return r_act_reps;
            case F_WEI_REPS:       return r_wei_reps;
            case F_NCONTEXTS:      return r_out_ncontexts;
            case F_PRELOAD_EN:     return r_out_preload_en ? 1u : 0u;
            case F_ACT_INCNTLIM:   return r_act_incntlim;
            case F_ACT_INCNTSTEP:  return r_act_incntstep;
            case F_ACT_OUTCNTLIM:  return r_act_outcntlim;
            case F_ACT_OUTCNTSTEP: return r_act_outcntstep;
            case F_WEI_INCNTLIM:   return r_wei_incntlim;
            case F_WEI_INCNTSTEP:  return r_wei_incntstep;
            case F_CXLIM:          return r_cxlim;
            case F_CXSTEP:         return r_cxstep;
            case F_CKLIM:          return r_cklim;
            case F_CKSTEP:         return r_ckstep;
            case F_TIL_CYLIM:      return r_out_til_cylim;
            case F_TIL_CYSTEP:     return r_out_til_cystep;
            case F_TIL_CKLIM:      return r_out_til_cklim;
            case F_TIL_CKSTEP:     return r_out_til_ckstep;
            // v1 SAURIA-only fields
            case F_ACT_XLIM:       return r_act_xlim;
            case F_ACT_XSTEP:      return r_act_xstep;
            case F_ACT_YLIM:       return r_act_ylim;
            case F_ACT_YSTEP:      return r_act_ystep;
            case F_ACT_CHLIM:      return r_act_chlim;
            case F_ACT_CHSTEP:     return r_act_chstep;
            case F_ACT_TIL_XLIM:   return r_act_til_xlim;
            case F_ACT_TIL_XSTEP:  return r_act_til_xstep;
            case F_ACT_TIL_YLIM:   return r_act_til_ylim;
            case F_ACT_TIL_YSTEP:  return r_act_til_ystep;
            case F_WEI_WLIM:       return r_wei_wlim;
            case F_WEI_WSTEP:      return r_wei_wstep;
            case F_WEI_KLIM:       return r_wei_klim;
            case F_WEI_KSTEP:      return r_wei_kstep;
            case F_WEI_TIL_KLIM:   return r_wei_til_klim;
            case F_WEI_TIL_KSTEP:  return r_wei_til_kstep;
            case F_WEI_COLS_ACTIVE:return r_wei_cols_active;
            case F_WEI_WALIGNED:   return r_wei_waligned;
            case F_INACTIVE_COLS:  return r_out_inactive_cols;
            case F_ACT_BASE_ADDR:  return r_act_base_addr;
            case F_WEI_BASE_ADDR:  return r_wei_base_addr;
            case F_OUT_BASE_ADDR:  return r_out_base_addr;
            case F_IN_H:           return r_in_h;
            case F_IN_W:           return r_in_w;
            case F_IN_C:           return r_in_c;
            case F_KERNEL_H:       return r_kernel_h;
            case F_KERNEL_W:       return r_kernel_w;
            case F_STRIDE:         return r_stride;
            case F_PADDING:        return r_padding;
            case F_DILATION:       return r_dilation;
            case F_TILE_X:         return r_tile_x;
            case F_TILE_Y:         return r_tile_y;
            case F_TILE_K:         return r_tile_k;
            case F_TILE_C:         return r_tile_c;
            case F_X_USED:         return r_x_used;
            case F_Y_USED:         return r_y_used;
            case F_NSPLIT:         return r_nsplit;
            case F_OBP_CFG_A:        return r_obp_cfg_a;
            case F_REQUANT_SCALE_A:  return r_requant_scale_a;
            case F_REQUANT_SHIFT_A:  return r_requant_shift_a;
            case F_OBP_CFG_B:        return r_obp_cfg_b;
            case F_REQUANT_SCALE_B:  return r_requant_scale_b;
            case F_REQUANT_SHIFT_B:  return r_requant_shift_b;
            default:               return 0u;
            }
        }

        // Table-driven read-back for a given profile.
        host_data_t decode_profile_read(NpuProfile prof, uint32_t local_addr)
        {
            host_data_t rdata;
            CfgField f = cfg_lookup(prof, local_addr);
            if (f == F_DIL_PAT)
            {
                rdata[0] = (float)r_dil_pat.to_uint();
                return rdata;
            }
            if (f == F_ROWS_ACTIVE)
            {
                for (int b = 0; b < 4; b++)
                {
                    uint32_t val = 0;
                    for (int bit = 0; bit < 8; bit++)
                    {
                        int idx = b * 8 + bit;
                        if (idx < Y_DIM && r_rows_active[idx])
                            val |= (1 << bit);
                    }
                    rdata[b] = (float)val;
                }
                return rdata;
            }
            rdata[0] = (float)field_value(f);
            return rdata;
        }

        void reg_process()
        {
            if (!i_rstn.read() || i_soft_reset_in.read())
            {
                r_act_incntlim = 96;
                r_act_incntstep = 1;
                r_act_outcntlim = 96;
                r_act_outcntstep = 1;
                r_wei_incntlim = 96;
                r_wei_incntstep = 1;
                r_cxlim = 96;
                r_cxstep = 1;
                r_cklim = 96;
                r_ckstep = 1;
                r_incntlim = 96;
                r_act_reps = 64;
                r_wei_reps = 64;
                r_dil_pat = 1;
                r_nsplit = Y_DIM / 2;
                r_obp_cfg_a = 0;
                r_requant_scale_a = 1;
                r_requant_shift_a = 0;
                r_obp_cfg_b = 0;
                r_requant_scale_b = 1;
                r_requant_shift_b = 0;
                r_start = false;
                r_done = false;
                r_soft_reset = false;

                o_start.write(false);
                o_soft_reset.write(false);
                o_profile.write((uint32_t)r_profile); // r_profile is sticky across soft reset
                o_incntlim.write(r_incntlim);
                o_act_incntlim.write(r_act_incntlim);
                o_act_incntstep.write(r_act_incntstep);
                o_act_outcntlim.write(r_act_outcntlim);
                o_act_outcntstep.write(r_act_outcntstep);
                o_act_reps.write(r_act_reps);
                o_wei_reps.write(r_wei_reps);
                o_dil_pat.write(r_dil_pat);
                o_rows_active.write(r_rows_active);
                o_nsplit.write(r_nsplit);
                o_obp_cfg_a.write(r_obp_cfg_a);
                o_requant_scale_a.write(r_requant_scale_a);
                o_requant_shift_a.write(r_requant_shift_a);
                o_obp_cfg_b.write(r_obp_cfg_b);
                o_requant_scale_b.write(r_requant_scale_b);
                o_requant_shift_b.write(r_requant_shift_b);
                o_wei_incntlim.write(r_wei_incntlim);
                o_wei_incntstep.write(r_wei_incntstep);

                o_cxlim.write(r_cxlim);
                o_cxstep.write(r_cxstep);
                o_cklim.write(r_cklim);
                o_ckstep.write(r_ckstep);

                o_act_base_addr.write(r_act_base_addr);
                o_wei_base_addr.write(r_wei_base_addr);
                o_out_base_addr.write(r_out_base_addr);
                o_host_rdata.write(host_data_t());
                return;
            }

            // Sync from FSM done status
            if (i_done.read())
            {
                r_done = true;
            }

            // Default auto-clearing signals
            if (r_start)
            {
                r_start = false;
            }
            if (r_soft_reset)
            {
                r_soft_reset = false;
            }

            // Address decoding
            uint32_t addr = i_host_addr.read();
            uint32_t mem_region = addr & SAURIA_MEM_ADDR_MASK;
            uint32_t local_addr = addr & ~SAURIA_MEM_ADDR_MASK;

            if (mem_region == CFG_REGS_OFFSET)
            {
                // Host Writes
                if (i_host_wren.read())
                {
                    host_data_t wdata = i_host_wdata.read();
                    host_mask_t wmask = i_host_wmask.read();

                    if (local_addr == 0x00)
                    {
                        // Control register
                        if (wmask[0])
                        {
                            r_start = (wdata[0] != 0.0f);
                        }
                        // COW on done
                        if (wmask[0] && (wdata[0] == 0.0f))
                        {
                            r_done = false;
                        }
                        // Soft reset
                        if (wmask[2])
                        { // bit 16-23 maps to third byte wmask[2]
                            r_soft_reset = (wdata[2] != 0.0f);
                        }
                    }
                    else if (local_addr == CFG_PROFILE_ADDR)
                    {
                        // Runtime PROFILE select. Driver/testbench writes this FIRST,
                        // before any other config, so the right address map is used.
                        if (wmask[0])
                            r_profile = (NpuProfile)((uint32_t)wdata[0]);
                    }
                    else if (r_profile == PROFILE_V4_LINEAR)
                    {
                        // V4 (LINEAR) profile -> table-driven decode (config_map.h V4 map).
                        decode_v4_write(local_addr, wdata, wmask);
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x00)
                    {
                        if (wmask[0])
                            r_incntlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x04)
                    {
                        if (wmask[0])
                            r_act_reps = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x08)
                    {
                        if (wmask[0])
                            r_wei_reps = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x14)
                    {
                        if (wmask[0])
                        {
                            if (!i_active.read())
                            {
                                r_nsplit = (uint32_t)wdata[0];
                            }
                            else
                            {
                                DBG_COUT << "[CONFIG_REGS WARNING] Rejected mid-tile N_split write to " << (uint32_t)wdata[0] << " (only allowed at layer boundary)" << std::endl;
                            }
                        }
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x20)
                    {
                        if (wmask[0]) r_obp_cfg_a = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x24)
                    {
                        if (wmask[0]) r_requant_scale_a = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x28)
                    {
                        if (wmask[0]) r_requant_shift_a = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x30)
                    {
                        if (wmask[0]) r_obp_cfg_b = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x34)
                    {
                        if (wmask[0]) r_requant_scale_b = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x38)
                    {
                        if (wmask[0]) r_requant_shift_b = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x00)
                    {
                        // Rows active mask
                        for (int i = 0; i < Y_DIM; i++)
                        {
                            int byte_idx = i / 8;
                            int bit_idx = i % 8;
                            if (byte_idx < 4 && wmask[byte_idx])
                            {
                                // Extract byte from wdata[byte_idx]
                                uint32_t val = (uint32_t)wdata[byte_idx];
                                r_rows_active[i] = (val & (1 << bit_idx)) != 0;
                            }
                        }
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x04)
                    { // 0x04 is Reg 9 for act feeder incnt lim
                        if (wmask[0])
                            r_act_incntlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x08)
                    { // 0x08 is Reg 11 for act feeder incnt step
                        if (wmask[0])
                            r_act_incntstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x0C)
                    { // 0x0C is Reg 12 for act feeder outcnt lim
                        if (wmask[0])
                            r_act_outcntlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x10)
                    { // 0x10 is Reg 9 for act feeder incnt step
                        if (wmask[0])
                            r_act_outcntstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x28)
                    { // 0x28 is Reg 10 for dilation
                        if (wmask[0])
                            r_dil_pat = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_XLIM)
                    {
                        if (wmask[0])
                            r_act_xlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_XSTEP)
                    {
                        if (wmask[0])
                            r_act_xstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_YLIM)
                    {
                        if (wmask[0])
                            r_act_ylim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_YSTEP)
                    {
                        if (wmask[0])
                            r_act_ystep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_CHLIM)
                    {
                        if (wmask[0])
                            r_act_chlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_CHSTEP)
                    {
                        if (wmask[0])
                            r_act_chstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_TIL_XLIM)
                    {
                        if (wmask[0])
                            r_act_til_xlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_TIL_XSTEP)
                    {
                        if (wmask[0])
                            r_act_til_xstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_TIL_YLIM)
                    {
                        if (wmask[0])
                            r_act_til_ylim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == ACT_TIL_YSTEP)
                    {
                        if (wmask[0])
                            r_act_til_ystep = (uint32_t)wdata[0];
                    }

                    else if (local_addr == CFG_WEI_OFFSET + 0x04)
                    { // 0x04 is Reg 13 for wei feeder incnt lim
                        if (wmask[0])
                            r_wei_incntlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_WEI_OFFSET + 0x08)
                    { // 0x08 is Reg 14 for wei feeder incnt step
                        if (wmask[0])
                            r_wei_incntstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == WEI_WLIM)
                    {
                        if (wmask[0])
                            r_wei_wlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == WEI_WSTEP)
                    {
                        if (wmask[0])
                            r_wei_wstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == WEI_KLIM)
                    {
                        if (wmask[0])
                            r_wei_klim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == WEI_KSTEP)
                    {
                        if (wmask[0])
                            r_wei_kstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == WEI_TIL_XLIM)
                    {
                        if (wmask[0])
                            r_wei_til_klim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == WEI_TIL_XSTEP)
                    {
                        if (wmask[0])
                            r_wei_til_kstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == WEI_COLS_ACTIVE)
                    {
                        if (wmask[0])
                            r_wei_cols_active = (uint32_t)wdata[0];
                    }
                    else if (local_addr == WEI_WALIGNED)
                    {
                        if (wmask[0])
                            r_wei_waligned = (uint32_t)wdata[0];
                    }

                    else if (local_addr == CFG_OUT_OFFSET + 0x04)
                    { // 0x04 is Reg 15 for PSM cxlim
                        if (wmask[0])
                            r_cxlim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x08)
                    { // 0x08 is Reg 16 for PSM cxstep
                        if (wmask[0])
                            r_cxstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x0C)
                    { // 0x0C is Reg 17 for PSM cklim
                        if (wmask[0])
                            r_cklim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x10)
                    { // 0x10 is Reg 18 for PSM ckstep
                        if (wmask[0])
                            r_ckstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == NCONTEXTS)
                    {
                        if (wmask[0])
                            r_out_ncontexts = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TIL_CYLIM)
                    {
                        if (wmask[0])
                            r_out_til_cylim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TIL_CYSTEP)
                    {
                        if (wmask[0])
                            r_out_til_cystep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TIL_CKLIM)
                    {
                        if (wmask[0])
                            r_out_til_cklim = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TIL_CKSTEP)
                    {
                        if (wmask[0])
                            r_out_til_ckstep = (uint32_t)wdata[0];
                    }
                    else if (local_addr == INACTIVE_COLS)
                    {
                        if (wmask[0])
                            r_out_inactive_cols = (uint32_t)wdata[0];
                    }
                    else if (local_addr == PRELOAD_EN)
                    {
                        if (wmask[0])
                            r_out_preload_en = ((uint32_t)wdata[0] != 0);
                    }

                    else if (local_addr == CFG_ACT_BASE_ADDR)
                    {
                        if (wmask[0])
                            r_act_base_addr = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_WEI_BASE_ADDR)
                    {
                        if (wmask[0])
                            r_wei_base_addr = (uint32_t)wdata[0];
                    }
                    else if (local_addr == CFG_OUT_BASE_ADDR)
                    {
                        if (wmask[0])
                            r_out_base_addr = (uint32_t)wdata[0];
                    }

                    // Layer run-time
                    else if (local_addr == IN_H)
                    {
                        if (wmask[0])
                            r_in_h = (uint32_t)wdata[0];
                    }
                    else if (local_addr == IN_W)
                    {
                        if (wmask[0])
                            r_in_w = (uint32_t)wdata[0];
                    }
                    else if (local_addr == IN_C)
                    {
                        if (wmask[0])
                            r_in_c = (uint32_t)wdata[0];
                    }
                    else if (local_addr == KERNEL_H)
                    {
                        if (wmask[0])
                            r_kernel_h = (uint32_t)wdata[0];
                    }
                    else if (local_addr == KERNEL_W)
                    {
                        if (wmask[0])
                            r_kernel_w = (uint32_t)wdata[0];
                    }
                    else if (local_addr == STRIDE)
                    {
                        if (wmask[0])
                            r_stride = (uint32_t)wdata[0];
                    }
                    else if (local_addr == PADDING)
                    {
                        if (wmask[0])
                            r_padding = (uint32_t)wdata[0];
                    }
                    else if (local_addr == DILATION)
                    {
                        if (wmask[0])
                            r_dilation = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TILE_X)
                    {
                        if (wmask[0])
                            r_tile_x = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TILE_Y)
                    {
                        if (wmask[0])
                            r_tile_y = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TILE_K)
                    {
                        if (wmask[0])
                            r_tile_k = (uint32_t)wdata[0];
                    }
                    else if (local_addr == TILE_C)
                    {
                        if (wmask[0])
                            r_tile_c = (uint32_t)wdata[0];
                    }
                    else if (local_addr == X_USED)
                    {
                        if (wmask[0])
                            r_x_used = (uint32_t)wdata[0];
                    }
                    else if (local_addr == Y_USED)
                    {
                        if (wmask[0])
                            r_y_used = (uint32_t)wdata[0];
                    }
                }

                // Host Reads
                if (i_host_rden.read())
                {
                    host_data_t rdata;
                    if (local_addr == 0x00)
                    {
                        rdata[0] = r_start ? 1.0f : 0.0f;
                        rdata[0] += r_done ? 2.0f : 0.0f;
                        rdata[0] += (!r_done) ? 4.0f : 0.0f; // idle
                        rdata[0] += (!r_done) ? 8.0f : 0.0f; // ready
                        rdata[2] = r_soft_reset ? 128.0f : 0.0f;
                    }
                    else if (local_addr == CFG_PROFILE_ADDR)
                    {
                        rdata[0] = (float)(uint32_t)r_profile;
                    }
                    else if (r_profile == PROFILE_V4_LINEAR)
                    {
                        rdata = decode_profile_read(PROFILE_V4_LINEAR, local_addr);
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x00)
                    {
                        rdata[0] = (float)r_incntlim;
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x04)
                    {
                        rdata[0] = (float)r_act_reps;
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x08)
                    {
                        rdata[0] = (float)r_wei_reps;
                    }
                    else if (local_addr == CFG_CON_OFFSET + 0x14)
                    {
                        rdata[0] = (float)r_nsplit;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x20)
                    {
                        rdata[0] = (float)r_obp_cfg_a;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x24)
                    {
                        rdata[0] = (float)r_requant_scale_a;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x28)
                    {
                        rdata[0] = (float)r_requant_shift_a;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x30)
                    {
                        rdata[0] = (float)r_obp_cfg_b;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x34)
                    {
                        rdata[0] = (float)r_requant_scale_b;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x38)
                    {
                        rdata[0] = (float)r_requant_shift_b;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x00)
                    {
                        for (int b = 0; b < 4; b++)
                        {
                            uint32_t val = 0;
                            for (int bit = 0; bit < 8; bit++)
                            {
                                int idx = b * 8 + bit;
                                if (idx < Y_DIM && r_rows_active[idx])
                                {
                                    val |= (1 << bit);
                                }
                            }
                            rdata[b] = (float)val;
                        }
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x04)
                    { // 0x04 is Reg 9 for act feeder incnt lim
                        rdata[0] = (float)r_act_incntlim;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x08)
                    { // 0x08 is Reg 11 for act feeder outcnt step
                        rdata[0] = (float)r_act_incntstep;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x0C)
                    { // 0x0C is Reg 12 for act feeder outcnt lim
                        rdata[0] = (float)r_act_outcntlim;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x10)
                    { // 0x10 is Reg 9 for act feeder incnt step
                        rdata[0] = (float)r_act_outcntstep;
                    }
                    else if (local_addr == CFG_ACT_OFFSET + 0x28)
                    {
                        rdata[0] = (float)r_dil_pat.to_uint();
                    }
                    else if (local_addr == CFG_WEI_OFFSET + 0x04)
                    { // 0x04 is Reg 13 for wei feeder incnt lim
                        rdata[0] = (float)r_wei_incntlim;
                    }
                    else if (local_addr == CFG_WEI_OFFSET + 0x08)
                    { // 0x08 is Reg 14 for wei feeder incnt step
                        rdata[0] = (float)r_wei_incntstep;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x04)
                    { // 0x04 is Reg 15 for PSM cxlim
                        rdata[0] = (float)r_cxlim;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x08)
                    { // 0x08 is Reg 16 for PSM cxstep
                        rdata[0] = (float)r_cxstep;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x0C)
                    { // 0x0C is Reg 17 for PSM cklim
                        rdata[0] = (float)r_cklim;
                    }
                    else if (local_addr == CFG_OUT_OFFSET + 0x10)
                    { // 0x10 is Reg 18 for PSM ckstep
                        rdata[0] = (float)r_ckstep;
                    }

                    else if (local_addr == CFG_ACT_BASE_ADDR)
                    {
                        rdata[0] = (float)r_act_base_addr;
                    }
                    else if (local_addr == CFG_WEI_BASE_ADDR)
                    {
                        rdata[0] = (float)r_wei_base_addr;
                    }
                    else if (local_addr == CFG_OUT_BASE_ADDR)
                    {
                        rdata[0] = (float)r_out_base_addr;
                    }

                    // Layer run-time
                    else if (local_addr == IN_H)
                    {
                        rdata[0] = (float)r_in_h;
                    }
                    else if (local_addr == IN_W)
                    {
                        rdata[0] = (float)r_in_w;
                    }
                    else if (local_addr == IN_C)
                    {
                        rdata[0] = (float)r_in_c;
                    }
                    else if (local_addr == KERNEL_H)
                    {
                        rdata[0] = (float)r_kernel_h;
                    }
                    else if (local_addr == KERNEL_W)
                    {
                        rdata[0] = (float)r_kernel_w;
                    }
                    else if (local_addr == STRIDE)
                    {
                        rdata[0] = (float)r_stride;
                    }
                    else if (local_addr == PADDING)
                    {
                        rdata[0] = (float)r_padding;
                    }
                    else if (local_addr == DILATION)
                    {
                        rdata[0] = (float)r_dilation;
                    }
                    else if (local_addr == TILE_X)
                    {
                        rdata[0] = (float)r_tile_x;
                    }
                    else if (local_addr == TILE_Y)
                    {
                        rdata[0] = (float)r_tile_y;
                    }
                    else if (local_addr == TILE_K)
                    {
                        rdata[0] = (float)r_tile_k;
                    }
                    else if (local_addr == TILE_C)
                    {
                        rdata[0] = (float)r_tile_c;
                    }
                    else if (local_addr == X_USED)
                    {
                        rdata[0] = (float)r_x_used;
                    }
                    else if (local_addr == Y_USED)
                    {
                        rdata[0] = (float)r_y_used;
                    }

                    else
                    {
                        // V1 profile: anything the legacy chain above did not handle
                        // (e.g. NCONTEXTS at OUT+0x00, til_c*, base addrs, layer regs)
                        // falls back to the table-driven reader so read-back is complete.
                        rdata = decode_profile_read(r_profile, local_addr);
                    }

                    o_host_rdata.write(rdata);
                }
            }

            if (r_start)
            {
                DBG_COUT << "\n=========================================\n";
                DBG_COUT << "NPU RUNTIME CONFIGURATION\n";
                DBG_COUT << "=========================================\n";

                DBG_COUT << "\nCONTROL\n";
                DBG_COUT << "INCNTLIM        : " << r_incntlim << "\n";

                DBG_COUT << "\nACTIVATION\n";
                DBG_COUT << "ACT_INCNTLIM    : " << r_act_incntlim << "\n";
                DBG_COUT << "ACT_INCNTSTEP   : " << r_act_incntstep << "\n";
                DBG_COUT << "ACT_OUTCNTLIM   : " << r_act_outcntlim << "\n";
                DBG_COUT << "ACT_OUTCNTSTEP  : " << r_act_outcntstep << "\n";
                DBG_COUT << "DIL_PAT         : 0x"
                          << std::hex << r_dil_pat.to_uint64()
                          << std::dec << "\n";

                DBG_COUT << "\nWEIGHT\n";
                DBG_COUT << "WEI_INCNTLIM    : " << r_wei_incntlim << "\n";
                DBG_COUT << "WEI_INCNTSTEP   : " << r_wei_incntstep << "\n";

                DBG_COUT << "\nPSUM\n";
                DBG_COUT << "CXLIM           : " << r_cxlim << "\n";
                DBG_COUT << "CXSTEP          : " << r_cxstep << "\n";
                DBG_COUT << "CKLIM           : " << r_cklim << "\n";
                DBG_COUT << "CKSTEP          : " << r_ckstep << "\n";

                DBG_COUT << "\nMEMORY MAP\n";
                DBG_COUT << "ACT_BASE_ADDR   : " << r_act_base_addr << "\n";
                DBG_COUT << "WEI_BASE_ADDR   : " << r_wei_base_addr << "\n";
                DBG_COUT << "OUT_BASE_ADDR   : " << r_out_base_addr << "\n";

                DBG_COUT << "\nREUSE\n";
                DBG_COUT << "ACT_REPS        : " << r_act_reps << "\n";
                DBG_COUT << "WEI_REPS        : " << r_wei_reps << "\n";

                DBG_COUT << "\nARRAY\n";
                DBG_COUT << "ROWS_ACTIVE     : 0x" << std::hex << r_rows_active << std::dec << "\n";

                DBG_COUT << "\nLAYER DESCRIPTOR\n";

                DBG_COUT
                    << "Input  : C="
                    << r_in_c
                    << " H="
                    << r_in_h
                    << " W="
                    << r_in_w
                    << std::endl;

                DBG_COUT
                    << "Kernel : "
                    << r_kernel_h
                    << "x"
                    << r_kernel_w
                    << std::endl;

                DBG_COUT
                    << "Stride : "
                    << r_stride
                    << std::endl;

                DBG_COUT
                    << "Padding: "
                    << r_padding
                    << std::endl;

                DBG_COUT
                    << "Dilation: "
                    << r_dilation
                    << std::endl;
            }

            // Update output ports
            o_start.write(r_start);
            o_soft_reset.write(r_soft_reset);
            o_profile.write((uint32_t)r_profile);
            o_incntlim.write(r_incntlim);
            o_act_reps.write(r_act_reps);
            o_wei_reps.write(r_wei_reps);
            o_dil_pat.write(r_dil_pat);
            o_rows_active.write(r_rows_active);
            o_nsplit.write(r_nsplit);
            o_obp_cfg_a.write(r_obp_cfg_a);
            o_requant_scale_a.write(r_requant_scale_a);
            o_requant_shift_a.write(r_requant_shift_a);
            o_obp_cfg_b.write(r_obp_cfg_b);
            o_requant_scale_b.write(r_requant_scale_b);
            o_requant_shift_b.write(r_requant_shift_b);

            o_act_incntlim.write(r_act_incntlim);
            o_act_incntstep.write(r_act_incntstep);
            o_act_outcntlim.write(r_act_outcntlim);
            o_act_outcntstep.write(r_act_outcntstep);
            o_act_xlim.write(r_act_xlim);
            o_act_xstep.write(r_act_xstep);
            o_act_ylim.write(r_act_ylim);
            o_act_ystep.write(r_act_ystep);
            o_act_chlim.write(r_act_chlim);
            o_act_chstep.write(r_act_chstep);
            o_act_til_xlim.write(r_act_til_xlim);
            o_act_til_xstep.write(r_act_til_xstep);
            o_act_til_ylim.write(r_act_til_ylim);
            o_act_til_ystep.write(r_act_til_ystep);

            o_wei_incntlim.write(r_wei_incntlim);
            o_wei_incntstep.write(r_wei_incntstep);
            o_wei_wlim.write(r_wei_wlim);
            o_wei_wstep.write(r_wei_wstep);
            o_wei_klim.write(r_wei_klim);
            o_wei_kstep.write(r_wei_kstep);
            o_wei_til_klim.write(r_wei_til_klim);
            o_wei_til_kstep.write(r_wei_til_kstep);
            o_wei_cols_active.write(r_wei_cols_active);
            o_wei_waligned.write(r_wei_waligned);

            o_cxlim.write(r_cxlim);
            o_cxstep.write(r_cxstep);
            o_cklim.write(r_cklim);
            o_ckstep.write(r_ckstep);
            o_out_ncontexts.write(r_out_ncontexts);

            o_out_til_cylim.write(r_out_til_cylim);
            o_out_til_cystep.write(r_out_til_cystep);
            o_out_til_cklim.write(r_out_til_cklim);
            o_out_til_ckstep.write(r_out_til_ckstep);

            o_out_inactive_cols.write(r_out_inactive_cols);
            o_out_preload_en.write(r_out_preload_en);

            o_act_base_addr.write(r_act_base_addr);
            o_wei_base_addr.write(r_wei_base_addr);
            o_out_base_addr.write(r_out_base_addr);

            o_in_h.write(r_in_h);
            o_in_w.write(r_in_w);
            o_in_c.write(r_in_c);

            o_kernel_h.write(r_kernel_h);
            o_kernel_w.write(r_kernel_w);

            o_stride.write(r_stride);
            o_padding.write(r_padding);
            o_dilation.write(r_dilation);

            o_tile_x.write(r_tile_x);
            o_tile_y.write(r_tile_y);
            o_tile_k.write(r_tile_k);
            o_tile_c.write(r_tile_c);

            o_x_used.write(r_x_used);
            o_y_used.write(r_y_used);
        }
    };

} // namespace sauria

#endif // SAURIA_UNIFIED_CONFIG_REGS_H
