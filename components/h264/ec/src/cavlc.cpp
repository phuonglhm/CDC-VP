#include "cavlc.h"
#include "exp_golomb.h"

namespace h264::ec {

Cavlc::Cavlc(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

void Cavlc::encode(const EcRequest& req, BitWriter& bw) {
    for(int i = 0; i < 16; ++i) {
        if(req.levels[i] != 0) ExpGolomb::encode_se(req.levels[i], bw);
    }
}

} // namespace h264::ec