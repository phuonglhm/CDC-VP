#pragma once
//
// 1:1 port of RTL sauria_core/psm/psm_shift_register.sv.
//
// Real RTL: reg_q[0] = i_din (a WIRE, not a register -- combinational
// passthrough of this cycle's input); reg_q[k] for k=1..X are real
// registers, each shifting from reg_q[k-1] when i_shift (reg_q[k] <=
// reg_q[k-1]). o_dout = reg_q[X] -- i.e. data entering i_din takes exactly
// X real register-delay cycles to reach o_dout.
//
// VecT is the data type carried per stage (this project always instantiates
// with VecT = psum_vector_t<Y_DIM, T_PSUM>, i.e. one full Y-wide element
// bus per RTL's BUFF_W = OC_W*Y bit packing -- kept as a real vector type
// here rather than manually bit-packed, matching this codebase's existing
// convention elsewhere).
//
#include <cstdint>

namespace sauria_rtl
{
using namespace sauria; // A1 port: shared vector/mask types stay in ::sauria, class bodies move to ::sauria_rtl.

    template <int X_DIM, typename VecT>
    class PsmShiftRegister
    {
    public:
        void reset()
        {
            for (int k = 0; k < X_DIM; k++)
                stages_[k] = VecT();
        }

        // in.din=i_din, in.shift=i_shift, in.clear=i_clear. Returns o_dout
        // (the PRE-tick value of reg_q[X], i.e. stages_[X_DIM-1] before this
        // cycle's shift is applied -- matches RTL's o_dout being a plain
        // `assign` off the register, not a function of this cycle's shift).
        VecT tick(const VecT &din, bool shift, bool clear)
        {
            VecT dout = stages_[X_DIM - 1];

            if (clear)
            {
                for (int k = 0; k < X_DIM; k++)
                    stages_[k] = VecT();
            }
            else if (shift)
            {
                // Shift from the END down to 1, THEN write stage 0 from din
                // -- matches reg_q[k]<=reg_q[k-1] for k=X..1 with reg_q[0]=
                // i_din (this cycle's NEW input), all applied simultaneously
                // on the same clock edge in real hardware.
                for (int k = X_DIM - 1; k > 0; k--)
                    stages_[k] = stages_[k - 1];
                stages_[0] = din;
            }

            return dout;
        }

        // o_dout AFTER this cycle's shift has been applied. Used by FX1_A3_CARR_POSTTICK: the model passes o_c_arr to
        // the array through an sc_signal, so the array reads it one cycle later than the RTL; taking the post-shift
        // value compensates exactly that cycle.
        VecT out_now() const { return stages_[X_DIM - 1]; }

    private:
        VecT stages_[X_DIM]{};
    };

} // namespace sauria_rtl
