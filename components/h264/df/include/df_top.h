#pragma once

#include "df_types.h"
#include "df_filter.h"
#include "df_line_mem.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace h264::df {
struct DfTop : sc_core::sc_module {
    tlm_utils::simple_target_socket<DfTop> socket{"socket"};

    SC_HAS_PROCESS(DfTop);
    DfTop(sc_core::sc_module_name name);

    void reset(); // Nonblocking protocol reset; configuration must be loaded again.
private:
    unsigned loaded_ = 0;

    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    void start();

    DfLineMemory line_mem_;
    DfFilter filter_;

    DfRequest request_{};
    DfResult result_{};
};
} // namespace h264::df
