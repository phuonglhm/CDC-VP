#pragma once
#include <h264/sync/reset_domain.h>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
namespace h264::legacy {
struct SegmentRecord { uint64_t address; unsigned bytes, beats; sc_core::sc_time completed; };
class DmaBridge : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<DmaBridge> input{"input"};
    tlm_utils::simple_initiator_socket<DmaBridge> memory{"memory"};
    std::vector<SegmentRecord> trace;
    const unsigned bus_bytes;
    const unsigned max_beats;
    DmaBridge(sc_core::sc_module_name, unsigned data_width=32, unsigned burst_limit=16);
private:
    void transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
};
}
