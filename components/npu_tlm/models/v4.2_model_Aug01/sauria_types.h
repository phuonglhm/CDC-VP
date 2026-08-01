// SystemC Model for SAURIA NPU Core
// Shared Types and Parameters definition

#ifndef SAURIA_TYPES_H
#define SAURIA_TYPES_H

#include <systemc.h>
#include <array>
#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include "fp16.h" // sauria::fp16_t (SystemC-free; shared with driver libs)

namespace sauria
{

    // Default System Parameters (scaled to 32x32 for advanced verification)
    const int X = 32;      // Systolic array columns
    const int Y = 32;      // Systolic array rows
    const int DILP_W = 64; // Dilation pattern width

    // Compile-time AXI sub-word decode helpers
    const int SUBWORDS_A = Y / 4;
    const int MASK_A = SUBWORDS_A - 1;
    const int SHIFT_A = (SUBWORDS_A == 8) ? 3 : ((SUBWORDS_A == 4) ? 2 : ((SUBWORDS_A == 2) ? 1 : 0));

    const int SUBWORDS_B = X / 4;
    const int MASK_B = SUBWORDS_B - 1;
    const int SHIFT_B = (SUBWORDS_B == 8) ? 3 : ((SUBWORDS_B == 4) ? 2 : ((SUBWORDS_B == 2) ? 1 : 0));

    const int SUBWORDS_C = Y / 4;
    const int MASK_C = SUBWORDS_C - 1;
    const int SHIFT_C = (SUBWORDS_C == 8) ? 3 : ((SUBWORDS_C == 4) ? 2 : ((SUBWORDS_C == 2) ? 1 : 0));

    // Compile-time Zero Gating Config (Optimal defaults)
    const bool ZERO_GATING_MULT = true; // Enable zero detection gating at multiplier
    const bool ZERO_GATING_ADD = false; // Enable zero detection gating at adder

    // Arithmetic Types
    typedef float act_t;  // Activation element data type
    typedef float wei_t;  // Weight element data type
    typedef float psum_t; // Partial Sum (accumulator) data type

    // fp16_t is defined in fp16.h (included above, SystemC-free). Its SystemC
    // sc_trace overload lives here since it needs the SystemC types.
    inline void sc_trace(sc_trace_file *tf, const fp16_t &v, const std::string &name)
    {
        float f = static_cast<float>(v);
        sc_trace(tf, f, name);
    }

    // Memory Dimensions
    const int SRAMA_DEPTH = 1024;
    const int SRAMB_DEPTH = 1024;
    const int SRAMC_DEPTH = 2048;

    // Address Space Mapping (as defined in sauria_addr_pkg.sv)
    const uint32_t SAURIA_MEM_ADDR_MASK = 0x003C0000;
    const uint32_t SRAMA_OFFSET = 0x00040000;
    const uint32_t SRAMB_OFFSET = 0x00080000;
    const uint32_t SRAMC_OFFSET = 0x000C0000;

    // Host Config/Control offsets
    const uint32_t CFG_REGS_OFFSET = 0x00000000;
    const uint32_t CFG_CON_OFFSET = 0x00000200;
    const uint32_t CFG_ACT_OFFSET = 0x00000400;
    const uint32_t CFG_WEI_OFFSET = 0x00000600;
    const uint32_t CFG_OUT_OFFSET = 0x00000800;
    const uint32_t CFG_LAYER_OFFSET = 0x00000A00;

    // Runtime base address
    const uint32_t CFG_ACT_BASE_ADDR = CFG_ACT_OFFSET + 0x80;
    const uint32_t CFG_WEI_BASE_ADDR = CFG_WEI_OFFSET + 0x80;
    const uint32_t CFG_OUT_BASE_ADDR = CFG_OUT_OFFSET + 0x80;

    // Runtime weightfeeder
    const uint32_t WEI_WLIM = CFG_WEI_OFFSET + 0x10;
    const uint32_t WEI_WSTEP = CFG_WEI_OFFSET + 0x14;
    const uint32_t WEI_KLIM = CFG_WEI_OFFSET + 0x18;
    const uint32_t WEI_KSTEP = CFG_WEI_OFFSET + 0x1C;
    const uint32_t WEI_TIL_XLIM = CFG_WEI_OFFSET + 0x20;
    const uint32_t WEI_TIL_XSTEP = CFG_WEI_OFFSET + 0x24;
    const uint32_t WEI_COLS_ACTIVE = CFG_WEI_OFFSET + 0x28;
    const uint32_t WEI_WALIGNED = CFG_WEI_OFFSET + 0x2C;

    // Runtime Ifmapfeeder
    const uint32_t ACT_XLIM = CFG_ACT_OFFSET + 0x14;
    const uint32_t ACT_XSTEP = CFG_ACT_OFFSET + 0x18;
    const uint32_t ACT_YLIM = CFG_ACT_OFFSET + 0x1C;
    const uint32_t ACT_YSTEP = CFG_ACT_OFFSET + 0x20;
    const uint32_t ACT_CHLIM = CFG_ACT_OFFSET + 0x24;
    const uint32_t ACT_CHSTEP = CFG_ACT_OFFSET + 0x2C;
    const uint32_t ACT_TIL_XLIM = CFG_ACT_OFFSET + 0x30;
    const uint32_t ACT_TIL_XSTEP = CFG_ACT_OFFSET + 0x34;
    const uint32_t ACT_TIL_YLIM = CFG_ACT_OFFSET + 0x38;
    const uint32_t ACT_TIL_YSTEP = CFG_ACT_OFFSET + 0x3C;

    // Runtime PSM
    const uint32_t NCONTEXTS = CFG_OUT_OFFSET + 0x00;
    const uint32_t TIL_CYLIM = CFG_OUT_OFFSET + 0x14;
    const uint32_t TIL_CYSTEP = CFG_OUT_OFFSET + 0x18;
    const uint32_t TIL_CKLIM = CFG_OUT_OFFSET + 0x1C;
    const uint32_t TIL_CKSTEP = CFG_OUT_OFFSET + 0x20;
    const uint32_t INACTIVE_COLS = CFG_OUT_OFFSET + 0x24;
    const uint32_t PRELOAD_EN = CFG_OUT_OFFSET + 0x28;

    // Runtime layer address
    const uint32_t IN_H = CFG_LAYER_OFFSET + 0x00;
    const uint32_t IN_W = CFG_LAYER_OFFSET + 0x04;
    const uint32_t IN_C = CFG_LAYER_OFFSET + 0x08;
    const uint32_t OUT_H = CFG_LAYER_OFFSET + 0x0C;
    const uint32_t OUT_W = CFG_LAYER_OFFSET + 0x10;
    const uint32_t OUT_C = CFG_LAYER_OFFSET + 0x14;
    const uint32_t KERNEL_H = CFG_LAYER_OFFSET + 0x18;
    const uint32_t KERNEL_W = CFG_LAYER_OFFSET + 0x1C;
    const uint32_t STRIDE = CFG_LAYER_OFFSET + 0x20;
    const uint32_t PADDING = CFG_LAYER_OFFSET + 0x24;
    const uint32_t DILATION = CFG_LAYER_OFFSET + 0x28;
    const uint32_t DIL_PAT = CFG_LAYER_OFFSET + 0x2C;
    const uint32_t TILE_X = CFG_LAYER_OFFSET + 0x30;
    const uint32_t TILE_Y = CFG_LAYER_OFFSET + 0x34;
    const uint32_t TILE_K = CFG_LAYER_OFFSET + 0x38;
    const uint32_t TILE_C = CFG_LAYER_OFFSET + 0x3C;
    const uint32_t X_USED = CFG_LAYER_OFFSET + 0x40;
    const uint32_t Y_USED = CFG_LAYER_OFFSET + 0x44;
    // ----------------------------------------------------
    // Templated Containers for SystemC Signal Integrity
    // ----------------------------------------------------

    // Activation Vector
    template <int Y_DIM, typename T_ACT = float>
    struct act_vector_t
    {
        std::array<T_ACT, Y_DIM> data;

        act_vector_t() { data.fill(static_cast<T_ACT>(0)); }
        act_vector_t(T_ACT val) { data.fill(val); }

        T_ACT &operator[](size_t idx) { return data[idx]; }
        const T_ACT &operator[](size_t idx) const { return data[idx]; }

        bool operator==(const act_vector_t &other) const
        {
            return data == other.data;
        }

        friend std::ostream &operator<<(std::ostream &os, const act_vector_t &v)
        {
            os << "[";
            for (int i = 0; i < Y_DIM; i++)
            {
                os << v.data[i] << (i == Y_DIM - 1 ? "" : ", ");
            }
            os << "]";
            return os;
        }
    };

    template <int Y_DIM, typename T_ACT>
    inline void sc_trace(sc_trace_file *tf, const act_vector_t<Y_DIM, T_ACT> &v, const std::string &name)
    {
        for (int i = 0; i < Y_DIM; i++)
        {
            sc_trace(tf, v.data[i], name + "(" + std::to_string(i) + ")");
        }
    }

    // Weight Vector
    template <int X_DIM, typename T_WEI = float>
    struct wei_vector_t
    {
        std::array<T_WEI, X_DIM> data;

        wei_vector_t() { data.fill(static_cast<T_WEI>(0)); }
        wei_vector_t(T_WEI val) { data.fill(val); }

        T_WEI &operator[](size_t idx) { return data[idx]; }
        const T_WEI &operator[](size_t idx) const { return data[idx]; }

        bool operator==(const wei_vector_t &other) const
        {
            return data == other.data;
        }

        friend std::ostream &operator<<(std::ostream &os, const wei_vector_t &v)
        {
            os << "[";
            for (int i = 0; i < X_DIM; i++)
            {
                os << v.data[i] << (i == X_DIM - 1 ? "" : ", ");
            }
            os << "]";
            return os;
        }
    };

    template <int X_DIM, typename T_WEI>
    inline void sc_trace(sc_trace_file *tf, const wei_vector_t<X_DIM, T_WEI> &v, const std::string &name)
    {
        for (int i = 0; i < X_DIM; i++)
        {
            sc_trace(tf, v.data[i], name + "(" + std::to_string(i) + ")");
        }
    }

    // Partial Sum Vector
    template <int Y_DIM, typename T_PSUM = float>
    struct psum_vector_t
    {
        std::array<T_PSUM, Y_DIM> data;

        psum_vector_t() { data.fill(static_cast<T_PSUM>(0)); }
        psum_vector_t(T_PSUM val) { data.fill(val); }

        T_PSUM &operator[](size_t idx) { return data[idx]; }
        const T_PSUM &operator[](size_t idx) const { return data[idx]; }

        bool operator==(const psum_vector_t &other) const
        {
            return data == other.data;
        }

        friend std::ostream &operator<<(std::ostream &os, const psum_vector_t &v)
        {
            os << "[";
            for (int i = 0; i < Y_DIM; i++)
            {
                os << v.data[i] << (i == Y_DIM - 1 ? "" : ", ");
            }
            os << "]";
            return os;
        }
    };

    template <int Y_DIM, typename T_PSUM>
    inline void sc_trace(sc_trace_file *tf, const psum_vector_t<Y_DIM, T_PSUM> &v, const std::string &name)
    {
        for (int i = 0; i < Y_DIM; i++)
        {
            sc_trace(tf, v.data[i], name + "(" + std::to_string(i) + ")");
        }
    }

    // Host Data Word Vector (Length 4).
    // Lane storage is `double` (not float): the host readback path carries SRAM-C
    // output element values, and wide-integer datatypes (e.g. int16 -> int64
    // accumulator, values up to ~2^38) exceed float's 24-bit exact-integer range.
    // double is exact for integers up to 2^53, and a superset of float, so int8
    // (<=2^24) and fp16 values are unaffected.
    struct host_data_t
    {
        std::array<double, 4> data;

        host_data_t() { data.fill(0.0); }

        double &operator[](size_t idx) { return data[idx]; }
        const double &operator[](size_t idx) const { return data[idx]; }

        bool operator==(const host_data_t &other) const
        {
            return data == other.data;
        }

        friend std::ostream &operator<<(std::ostream &os, const host_data_t &v)
        {
            os << "[";
            for (int i = 0; i < 4; i++)
            {
                os << v.data[i] << (i == 3 ? "" : ", ");
            }
            os << "]";
            return os;
        }
    };

    inline void sc_trace(sc_trace_file *tf, const host_data_t &v, const std::string &name)
    {
        for (int i = 0; i < 4; i++)
        {
            sc_trace(tf, v.data[i], name + "(" + std::to_string(i) + ")");
        }
    }

    // Host Byte/Word Mask (Length 4)
    struct host_mask_t
    {
        std::array<bool, 4> data;

        host_mask_t() { data.fill(false); }

        bool &operator[](size_t idx) { return data[idx]; }
        const bool &operator[](size_t idx) const { return data[idx]; }

        bool operator==(const host_mask_t &other) const
        {
            return data == other.data;
        }

        friend std::ostream &operator<<(std::ostream &os, const host_mask_t &v)
        {
            os << "[";
            for (int i = 0; i < 4; i++)
            {
                os << (v.data[i] ? "1" : "0") << (i == 3 ? "" : ", ");
            }
            os << "]";
            return os;
        }
    };

    inline void sc_trace(sc_trace_file *tf, const host_mask_t &v, const std::string &name)
    {
        for (int i = 0; i < 4; i++)
        {
            sc_trace(tf, v.data[i], name + "(" + std::to_string(i) + ")");
        }
    }

    // SRAM C Mask Vector
    template <int Y_DIM>
    struct sramc_mask_t
    {
        std::array<bool, Y_DIM> data;

        sramc_mask_t() { data.fill(false); }
        sramc_mask_t(bool val) { data.fill(val); }

        bool &operator[](size_t idx) { return data[idx]; }
        const bool &operator[](size_t idx) const { return data[idx]; }

        bool operator==(const sramc_mask_t &other) const
        {
            return data == other.data;
        }

        friend std::ostream &operator<<(std::ostream &os, const sramc_mask_t &v)
        {
            os << "[";
            for (int i = 0; i < Y_DIM; i++)
            {
                os << (v.data[i] ? "1" : "0") << (i == Y_DIM - 1 ? "" : ", ");
            }
            os << "]";
            return os;
        }
    };

    template <int Y_DIM>
    inline void sc_trace(sc_trace_file *tf, const sramc_mask_t<Y_DIM> &v, const std::string &name)
    {
        for (int i = 0; i < Y_DIM; i++)
        {
            sc_trace(tf, v.data[i], name + "(" + std::to_string(i) + ")");
        }
    }

} // namespace sauria

#endif // SAURIA_TYPES_H
