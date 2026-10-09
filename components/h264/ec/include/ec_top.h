#pragma once

#include "ec_types.h"
#include "cavlc.h"
#include "cabac.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace h264::ec {
struct EcTop : sc_core::sc_module {
    tlm_utils::simple_target_socket<EcTop> socket{"socket"};

    SC_HAS_PROCESS(EcTop);
    EcTop(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    void start();

    Cavlc cavlc_;
    Cabac cabac_;
    EcRequest request_{};
    EcResult result_{};

};

}