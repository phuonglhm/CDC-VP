// rtl_ref_gvu_fu_r2.h -- latency of the functional units of the vector unit in RTL revision 2 (neo_lite, HAS_NEO_REV = 2).
// The values of every unit are those of rtl_ref_gvu_fu.h (frozen): the oracle on the RTL shows no difference in a value of
// the multipliers or of the Round-Shift between revisions 1 and 2. Only the number of registers between the accepted beat
// and the output valid changed (measured with Verilator: mula_latency / mulb_latency of gvu_pkg.sv, Round-Shift S2_PIPE = 1).
// A table for any revision is given by mula_latency_rev / mulb_latency_rev / lat_vroundshift_rev.
#ifndef RTL_REF_GVU_FU_R2_H
#define RTL_REF_GVU_FU_R2_H

#include "has/rtl_ref_gvu_fu.h"

namespace rtl_ref_gvu
{
    // gvu_pkg.sv, revision 2: MA_32X32 6, MA_32X16 and MA_32X8 4, MA_16X16 2.
    inline int mula_latency_r2(MulaMode m) { return m == MA_32X32 ? 6 : ((m == MA_32X16 || m == MA_32X8) ? 4 : 2); }
    // MB_40X32 7, MB_32X32_16X8 6, MB_32X8 4, MB_3X16X8 2.
    inline int mulb_latency_r2(MulbMode m) { return m == MB_40X32 ? 7 : (m == MB_32X32_16X8 ? 6 : (m == MB_32X8 ? 4 : 2)); }

    inline int mula_latency_rev(MulaMode m, unsigned neo_rev) { return neo_rev >= 2 ? mula_latency_r2(m) : mula_latency(m); }
    inline int mulb_latency_rev(MulbMode m, unsigned neo_rev) { return neo_rev >= 2 ? mulb_latency_r2(m) : mulb_latency(m); }
    // Round-Shift: S2_PIPE = 1 in revision 2 (the register after the rounding stage; FuParams::RS_S2_PIPE of the frozen header).
    inline FuParams fu_params_rev(unsigned neo_rev) { FuParams p; p.RS_S2_PIPE = neo_rev >= 2; return p; }
    inline int lat_vroundshift_rev(unsigned neo_rev) { return lat_vroundshift(fu_params_rev(neo_rev)); }
}

#endif // RTL_REF_GVU_FU_R2_H
