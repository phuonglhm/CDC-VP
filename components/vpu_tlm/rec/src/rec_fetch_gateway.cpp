#include "../include/rec_fetch_gateway.h"
#include "../../fetch/include/fetch_packet.h"

#include <iostream>
#include <algorithm>

RecFetchGateway::RecFetchGateway(sc_core::sc_module_name name)
    : sc_core::sc_module(name), start_socket("start_socket"), out_socket("out_socket")
{
    out_socket.register_b_transport(this, &RecFetchGateway::b_transport);
}

bool RecFetchGateway::getRefBlock(RecPlane plane,
                                 uint32_t x,
                                 uint32_t y,
                                 uint8_t size4x4,
                                 PaddingMode pad,
                                 RefBlock &out)
{
    (void)pad;
    uint32_t N = recSizeToPixels(size4x4);
    const uint32_t ext = N + 2u;

    // Build a LOAD request that reads the full (N+2)x(N+2) window starting
    // at the supplied anchor (x,y). The Rec code passes anchor coordinates
    // such that this maps one-to-one to the FetchWrapper LOAD semantics.
    FetchPacket req;
    req.cmd = FetchCmd::LOAD;
    req.plane = static_cast<uint8_t>(plane);
    req.x_px = x;
    req.y_px = y;
    req.width = static_cast<uint16_t>(ext);
    req.height = static_cast<uint16_t>(ext);
    req.margin = 0;
    req.downsample = 1;
    req.align_px = 1;
    req.req_id = 0;

    std::vector<uint8_t> buf = packFetchPacket(req);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(buf.data());
    trans.set_data_length(buf.size());
    trans.set_streaming_width(buf.size());
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time d = sc_core::SC_ZERO_TIME;

    // clear any prior response
    {
        std::lock_guard<std::mutex> lk(mtx_);
        have_resp_ = false;
        last_resp_.clear();
    }

    std::cerr << "[RecFetchGateway] SEND LOAD plane=" << static_cast<int>(req.plane)
              << " x=" << req.x_px << " y=" << req.y_px
              << " w=" << req.width << " h=" << req.height
              << " id=" << req.req_id << std::endl;

    try {
        start_socket->b_transport(trans, d);
    } catch (...) {
        return false;
    }

    // After the b_transport returns, the FetchWrapper should have delivered
    // a response into `last_resp_` via `b_transport` above.
    std::vector<uint8_t> resp_buf;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!have_resp_) return false;
        resp_buf = last_resp_;
    }

    FetchPacket resp = unpackFetchPacket(resp_buf.data(), resp_buf.size());
    std::cerr << "[RecFetchGateway] RECV LOAD_RESP id=" << resp.req_id
              << " len=" << resp.data.size() << std::endl;
    size_t to_print = std::min<size_t>(resp.data.size(), 16);
    std::cerr << "[RecFetchGateway] DATA:";
    for (size_t i = 0; i < to_print; ++i) std::cerr << " " << static_cast<int>(resp.data[i]);
    std::cerr << std::endl;
    if (resp.cmd != FetchCmd::LOAD_RESP) return false;
    if (resp.data.size() < static_cast<size_t>(ext) * ext) return false;

    out.width = ext;
    out.height = ext;
    out.stride = ext;
    out.data = resp.data; // row-major (ext x ext)
    return true;
}

void RecFetchGateway::pushRefBlock(RecPlane plane,
                                  uint32_t x,
                                  uint32_t y,
                                  uint8_t size4x4,
                                  const RefBlock &block,
                                  uint64_t version)
{
    (void)plane; (void)x; (void)y; (void)size4x4; (void)block; (void)version;
}

uint64_t RecFetchGateway::regionVersion(RecPlane plane,
                                       uint32_t x,
                                       uint32_t y,
                                       uint8_t size4x4) const
{
    (void)plane; (void)x; (void)y; (void)size4x4; return 0;
}

void RecFetchGateway::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
    // Receive fetch responses (WRITE with packed FetchPacket)
    const uint8_t *p = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
    size_t len = trans.get_data_length();
    if (!p || len == 0) { trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }

    std::lock_guard<std::mutex> lk(mtx_);
    last_resp_.assign(p, p + len);
    have_resp_ = true;
    std::cerr << "[RecFetchGateway] b_transport got response len=" << len << std::endl;
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}
