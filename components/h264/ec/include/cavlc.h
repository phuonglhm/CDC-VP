#pragma once
#include "ec_types.h"
#include <systemc>

namespace h264::ec {

struct Cavlc : sc_core::sc_module {

    SC_HAS_PROCESS(Cavlc);
    Cavlc(sc_core::sc_module_name name);
    void encode(const EcRequest& req, BitWriter& bw);
};

} // namespace h264::ec