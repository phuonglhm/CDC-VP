#ifndef FETCH_FRAME_LOADER_H
#define FETCH_FRAME_LOADER_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "fetch_packet.h"
#include <vector>
#include <mutex>

struct FetchFrameLoader : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<FetchFrameLoader> socket; // sends requests
    tlm_utils::simple_target_socket<FetchFrameLoader> resp_socket; // receives responses

    SC_HAS_PROCESS(FetchFrameLoader);

    // If `start_thread` is true the internal `run()` SC_THREAD will be spawned
    // (useful for the existing fetch unit test). For integration uses where the
    // loader is driven programmatically, pass `false` to avoid auto-running
    // the test stimulus.
    FetchFrameLoader(sc_core::sc_module_name name, bool start_thread = true);
    void run();

    // synchronous load: issues a LOAD and returns the bytes in `out`
    bool load_rect(uint8_t plane, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector<uint8_t>& out);

    // convenience: raw load that returns bytes row-major
    // Use `load_rect()` which returns the raw pixel bytes for callers to copy

    // TLM target entry for receiving fetch responses
    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

    // last received packet (mirrors FetchOutReceiver)
    FetchPacket last;
    bool got{false};
    // preserve the first response seen (so tests can verify the original
    // initiator load even if later calls overwrite `last`).
    FetchPacket first_resp;
    bool have_first_resp{false};

private:
    std::vector<uint8_t> last_resp_;
    bool have_resp_{false};
    std::mutex mtx_;
};

#endif
