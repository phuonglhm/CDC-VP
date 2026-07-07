#include "rec_mc.h"
#include <iostream>

RecMc::RecMc(sc_core::sc_module_name name) : 
    sc_module(name), start_socket("start_socket"), buffer_socket("buffer_socket") {
    start_socket.register_b_transport(this, &RecMc::b_transport);
}

void RecMc::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    RecPacket pkt = unpackRecPacket(trans);

    switch (pkt.cmd) {
        case RecCmd::PRE:
            handle_pre(pkt);
            break;
        case RecCmd::READ_REQ:
            handle_read_req(pkt, trans);
            break;
        default:
            break;
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}



void RecMc::handle_pre(const RecPacket &pkt) {
    std::vector<uint8_t> outbuf = packRecPacket(pkt);

    tlm::tlm_generic_payload new_trans;
    new_trans.set_command(tlm::TLM_WRITE_COMMAND);
    new_trans.set_address(0);
    new_trans.set_data_ptr(outbuf.data());
    new_trans.set_data_length(outbuf.size());
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;

    buffer_socket->b_transport(new_trans, d);
}


void RecMc::handle_read_req(const RecPacket &pkt, tlm::tlm_generic_payload &trans) {
    // Convert 4x4 block coords to pixel coords
    uint32_t px = static_cast<uint32_t>(pkt.x) * 4u;
    uint32_t py = static_cast<uint32_t>(pkt.y) * 4u;

    RecPlane plane = (pkt.sel <= 2) ? static_cast<RecPlane>(pkt.sel) : RecPlane::Y;

    std::vector<uint8_t> pred(16, 0);
    bool filled = false;

    // If this is an MC request and we have an MV provider, try to read MV
    if (static_cast<PredType>(pkt.pred_type) == PredType::MC && mvd_if) {
        MotionVector mv;
        if (mvd_if->readMV(static_cast<uint32_t>(pkt.block_idx), mv)) {
            // Treat MV components as integer pixel offsets for this TLM model
            int32_t ref_px = static_cast<int32_t>(px) + mv.x;
            int32_t ref_py = static_cast<int32_t>(py) + mv.y;

            uint32_t ax = (ref_px > 0) ? static_cast<uint32_t>(ref_px - 1) : 0u;
            uint32_t ay = (ref_py > 0) ? static_cast<uint32_t>(ref_py - 1) : 0u;

            RefBlock win;
            if (getRefBlock(plane, ax, ay, pkt.size, PaddingMode::EDGE, win)) {
                int N = static_cast<int>(recSizeToPixels(pkt.size));
                uint32_t blocks = static_cast<uint32_t>(N / 4);
                uint32_t subX = (blocks > 1) ? (pkt.i4x4_x & (blocks - 1)) : 0u;
                uint32_t subY = (blocks > 1) ? (pkt.i4x4_y & (blocks - 1)) : 0u;
                uint32_t startX = subX * 4;
                uint32_t startY = subY * 4;
                auto idx = [&](int r, int c) -> size_t { return static_cast<size_t>(r) * win.stride + static_cast<size_t>(c); };

                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        size_t pos = idx(1 + static_cast<int>(startY + r), 1 + static_cast<int>(startX + c));
                        uint8_t v = 0;
                        if (pos < win.data.size()) v = win.data[pos];
                        pred[static_cast<size_t>(r) * 4 + c] = v;
                    }
                }
                filled = true;
            }
        }
    }

    // Fallback: read the reference block anchored at px-1,py-1 (no MV)
    if (!filled && mem_if) {
        uint32_t ax = (px > 0) ? px - 1 : 0u;
        uint32_t ay = (py > 0) ? py - 1 : 0u;
        RefBlock win;
        if (getRefBlock(plane, ax, ay, pkt.size, PaddingMode::EDGE, win)) {
            int N = static_cast<int>(recSizeToPixels(pkt.size));
            uint32_t blocks = static_cast<uint32_t>(N / 4);
            uint32_t subX = (blocks > 1) ? (pkt.i4x4_x & (blocks - 1)) : 0u;
            uint32_t subY = (blocks > 1) ? (pkt.i4x4_y & (blocks - 1)) : 0u;
            uint32_t startX = subX * 4;
            uint32_t startY = subY * 4;
            auto idx = [&](int r, int c) -> size_t { return static_cast<size_t>(r) * win.stride + static_cast<size_t>(c); };

            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    size_t pos = idx(1 + static_cast<int>(startY + r), 1 + static_cast<int>(startX + c));
                    uint8_t v = 0;
                    if (pos < win.data.size()) v = win.data[pos];
                    pred[static_cast<size_t>(r) * 4 + c] = v;
                }
            }
            filled = true;
        }
    }

    // Build PRE packet and forward to downstream buffer (RecTQ)
    RecPacket outpkt;
    outpkt.cmd = RecCmd::PRE;
    outpkt.block_idx = pkt.block_idx;
    outpkt.x = pkt.x;
    outpkt.y = pkt.y;
    outpkt.size = pkt.size;
    outpkt.sel = pkt.sel;
    outpkt.qp = pkt.qp;
    outpkt.pred_type = pkt.pred_type;
    outpkt.mode = pkt.mode;
    outpkt.pre_sel = pkt.pre_sel;
    outpkt.i4x4_x = pkt.i4x4_x;
    outpkt.i4x4_y = pkt.i4x4_y;
    outpkt.data = std::move(pred);

    std::vector<uint8_t> outbuf = packRecPacket(outpkt);
    tlm::tlm_generic_payload new_trans;
    new_trans.set_command(tlm::TLM_WRITE_COMMAND);
    new_trans.set_address(0);
    new_trans.set_data_ptr(outbuf.data());
    new_trans.set_data_length(outbuf.size());
    sc_core::sc_time d = sc_core::SC_ZERO_TIME;
    buffer_socket->b_transport(new_trans, d);

    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

// Bind a memory provider so this module can fetch reference blocks
void RecMc::bindMemory(RecMemoryIf &mem) {
    mem_if = &mem;
}

void RecMc::bindMvMemory(RecMvIf &mvmem) {
    mvd_if = &mvmem;
}

bool RecMc::getRefBlock(RecPlane plane, uint32_t x, uint32_t y, uint8_t size4x4, PaddingMode pad, RefBlock &out) {
    if (mem_if) return mem_if->getRefBlock(plane, x, y, size4x4, pad, out);
    return false;
}

bool RecMc::fetchRefAsVector(RecPlane plane,
                             uint32_t x,
                             uint32_t y,
                             uint8_t size4x4,
                             PaddingMode pad,
                             std::vector<uint8_t> &out)
{
    RefBlock blk;
    if (!getRefBlock(plane, x, y, size4x4, pad, blk)) return false;
    out = std::move(blk.data);
    return true;
}
