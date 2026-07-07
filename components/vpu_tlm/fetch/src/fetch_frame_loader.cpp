#include "../include/fetch_frame_loader.h"
#include <iostream>

FetchFrameLoader::FetchFrameLoader(sc_core::sc_module_name name, bool start_thread)
    : sc_core::sc_module(name), socket("socket"), resp_socket("resp_socket")
{
    resp_socket.register_b_transport(this, &FetchFrameLoader::b_transport);
    if (start_thread) SC_THREAD(run);
}

void FetchFrameLoader::run() {
    wait(sc_core::sc_time(1, sc_core::SC_NS));

    // 1) LOAD request (8x4 rectangle)
    FetchPacket load;
    load.cmd = FetchCmd::LOAD;
    load.plane = 0; load.x_px = 10; load.y_px = 20; load.width = 8; load.height = 4; load.req_id = 0xA1;
    auto buf = packFetchPacket(load);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(const_cast<uint8_t*>(buf.data()));
    trans.set_data_length(buf.size());
    trans.set_streaming_width(buf.size());
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    std::cerr << "FetchFrameLoader: issuing LOAD 8x4\n";
    socket->b_transport(trans, delay);
    std::cerr << "FetchFrameLoader: LOAD returned status=" << trans.get_response_status() << "\n";
    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) std::cerr << "FetchFrameLoader: LOAD failed\n";

    wait(sc_core::sc_time(50, sc_core::SC_NS));

    // 2) WRITE_4x4 request
    FetchPacket w;
    w.cmd = FetchCmd::WRITE_4x4; w.plane = 0; w.x_px = 12; w.y_px = 24; w.req_id = 0xB2;
    w.data.resize(16);
    for (int i = 0; i < 16; ++i) w.data[i] = static_cast<uint8_t>(200 + i);
    auto wbuf = packFetchPacket(w);

    tlm::tlm_generic_payload wtrans;
    wtrans.set_command(tlm::TLM_WRITE_COMMAND);
    wtrans.set_address(0);
    wtrans.set_data_ptr(const_cast<uint8_t*>(wbuf.data()));
    wtrans.set_data_length(wbuf.size());
    wtrans.set_streaming_width(wbuf.size());
    wtrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    std::cerr << "FetchFrameLoader: issuing WRITE_4x4\n";
    socket->b_transport(wtrans, delay);
    std::cerr << "FetchFrameLoader: WRITE_4x4 returned status=" << wtrans.get_response_status() << "\n";
    if (wtrans.get_response_status() != tlm::TLM_OK_RESPONSE) std::cerr << "FetchFrameLoader: WRITE_4x4 failed\n";

    wait(sc_core::sc_time(100, sc_core::SC_NS));
}

bool FetchFrameLoader::load_rect(uint8_t plane, uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector<uint8_t>& out) {
    FetchPacket req;
    req.cmd = FetchCmd::LOAD;
    req.plane = plane;
    req.x_px = x;
    req.y_px = y;
    req.width = static_cast<uint16_t>(width);
    req.height = static_cast<uint16_t>(height);
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

    // clear prior response
    {
        std::lock_guard<std::mutex> lk(mtx_);
        have_resp_ = false;
        last_resp_.clear();
        got = false;
    }

    try {
        socket->b_transport(trans, d);
    } catch (...) {
        return false;
    }

    std::vector<uint8_t> resp_buf;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!have_resp_) return false;
        resp_buf = last_resp_;
    }

    FetchPacket resp = unpackFetchPacket(resp_buf.data(), resp_buf.size());
    if (resp.cmd != FetchCmd::LOAD_RESP) return false;
    if (resp.data.size() < static_cast<size_t>(width) * height) return false;
    out = resp.data;
    return true;
}

// frame-copy helper removed to avoid linking common/frame into this
// test binary. Callers can use `load_rect()` and copy bytes into their
// frame containers as needed.

void FetchFrameLoader::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
    const uint8_t *p = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
    size_t len = trans.get_data_length();
    if (!p || len == 0) { trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE); return; }

    std::lock_guard<std::mutex> lk(mtx_);
    last_resp_.assign(p, p + len);
    have_resp_ = true;
    last = unpackFetchPacket(p, len);
    got = true;
    if (!have_first_resp) {
        first_resp = last;
        have_first_resp = true;
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}
