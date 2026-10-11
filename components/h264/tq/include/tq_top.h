#pragma once

#include "tq_types.h"
#include "ftq.h"
#include "itq.h"
#include "transpose_ram.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace h264::tq {

class TqTop : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<TqTop> socket{"socket"};
    SC_HAS_PROCESS(TqTop);
    explicit TqTop(sc_core::sc_module_name name);
    const TqResult& result() const { return result_; }

    void reset(); // Nonblocking protocol reset; configuration must be loaded again.
private:
    unsigned loaded_ = 0;
    bool is_intra_ = true;

    TqRequest request_{};
    TqResult result_{};

    TransposeRam transpose_;
    Ftq ftq_;
    Itq itq_;

    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    void start();
};

} // namespace h264::tq
