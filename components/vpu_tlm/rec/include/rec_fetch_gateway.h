#ifndef REC_FETCH_GATEWAY_H
#define REC_FETCH_GATEWAY_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "memory_if.h"
#include <vector>
#include <mutex>

// Gateway that implements MemoryIf and forwards requests to a
// `fetch_tlm::FetchWrapper` using TLM. This is intended for tests only
// and performs simple synchronous round-trips using `LOAD` requests.
class RecFetchGateway : public sc_core::sc_module, public MemoryIf {
public:
    tlm_utils::simple_initiator_socket<RecFetchGateway> start_socket;
    tlm_utils::simple_target_socket<RecFetchGateway> out_socket;

    RecFetchGateway(sc_core::sc_module_name name);

    bool getRefBlock(RecPlane plane,
                     uint32_t x,
                     uint32_t y,
                     uint8_t size4x4,
                     PaddingMode pad,
                     RefBlock &out) override;

    void pushRefBlock(RecPlane plane,
                      uint32_t x,
                      uint32_t y,
                      uint8_t size4x4,
                      const RefBlock &block,
                      uint64_t version) override;

    uint64_t regionVersion(RecPlane plane,
                           uint32_t x,
                           uint32_t y,
                           uint8_t size4x4) const override;

    // receive fetch responses here
    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

private:
    std::vector<uint8_t> last_resp_;
    bool have_resp_{false};
    std::mutex mtx_;
};

#endif
