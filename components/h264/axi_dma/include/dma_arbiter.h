#pragma once
#include <h264/sync/reset_domain.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <map>
namespace h264 {
struct GrantRecord { int owner; uint64_t address; unsigned length; sc_core::sc_time begin, end; };
class DmaArbiter : public sc_core::sc_module {
public:
    tlm_utils::multi_passthrough_target_socket<DmaArbiter> clients{"clients"};
    tlm_utils::simple_initiator_socket<DmaArbiter> memory{"memory"};
    std::vector<GrantRecord> trace;
    SC_HAS_PROCESS(DmaArbiter);
    explicit DmaArbiter(sc_core::sc_module_name);
    // Smaller value = higher priority. Default FIFO ties; release adapter supplies ranks.
    void set_priority(int owner,unsigned priority) { priorities_[owner]=priority; }
    bool idle() const { return !busy_ && waiting_.empty(); }
    // SC_THREAD only. Stop/reset all producers before using this buffer-reuse fence.
    // A timeout never cancels a submitted transaction or grants buffer ownership.
    bool wait_idle(sc_core::sc_time timeout);
private:
    struct Pending { int owner; unsigned bypasses=0; bool granted=false; sc_core::sc_event grant; };
    std::vector<Pending*> waiting_;
    std::map<int,unsigned> priorities_;
    bool busy_=false;
    sc_core::sc_event changed_;
    void dispatch();
    void transport(int, tlm::tlm_generic_payload&, sc_core::sc_time&);
};
}
