#pragma once
#include "ec_types.h"
#include <systemc>
namespace h264::ec {
// Frame zigzag, natural row-major coefficient indices.
inline constexpr unsigned zigzag4[16]={0,1,4,8,5,2,3,6,9,12,13,10,7,11,14,15};
struct Cavlc : sc_core::sc_module {
    SC_HAS_PROCESS(Cavlc);
    Cavlc(sc_core::sc_module_name name);
    void encode(const EcRequest& req, BitWriter& bw);
    // Input is already in scan order. nC=-1 is required for 4:2:0 chroma DC.
    // No output is committed if validation or capacity fails.
    static unsigned encode_block(const std::array<std::int16_t,16>& scan,
                                 unsigned max_coeff,int nc,BitWriter& bw);
};
} // namespace h264::ec
