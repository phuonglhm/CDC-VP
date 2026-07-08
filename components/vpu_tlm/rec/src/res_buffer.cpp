#include "res_buffer.h"
#include "../../include/block_coord_codec.h"
#include <algorithm>

#include "debug_config.h"

ResBuffer::ResBuffer(sc_core::sc_module_name name) : 
    sc_module(name), intra_socket("intra_socket"), mc_socket("mc_socket") {
    intra_socket.register_b_transport(this, &ResBuffer::intra_transport);
    mc_socket.register_b_transport(this, &ResBuffer::mc_transport); 
    SC_THREAD(forward_thread);
}

void ResBuffer::bindMemory(RecMemoryIf &mem) {
    mem_if = &mem;
}

void ResBuffer::intra_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    RecPacket pkt;
    auto len = trans.get_data_length();
    if (len > 0 && trans.get_data_ptr()) {
        const uint8_t *ptr = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
        pkt = unpackRecPacket(ptr, len);
    }
    fifo_buffer.write(std::move(pkt));
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void ResBuffer::mc_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    RecPacket pkt;
    auto len = trans.get_data_length();
    if (len > 0 && trans.get_data_ptr()) {
        const uint8_t *ptr = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
        pkt = unpackRecPacket(ptr, len);
    }
    fifo_buffer.write(std::move(pkt));
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void ResBuffer::forward_thread() {
    while (true) {
        RecPacket pkt = fifo_buffer.read();
        if (pkt.cmd == RecCmd::PRE && mem_if) {
            const cdc::components::block_coord_4x4 block_coord =
                cdc::components::decode_block_coord_4x4(pkt.block_idx, pkt.x, pkt.y);
            uint32_t px = block_coord.x * 4u;
            uint32_t py = block_coord.y * 4u;
            uint32_t ax = (px > 0) ? (px - 1u) : 0u;
            uint32_t ay = (py > 0) ? (py - 1u) : 0u;
            RecPlane plane = (pkt.sel <= 2) ? static_cast<RecPlane>(pkt.sel) : RecPlane::Y;
            RefBlock win;
            if (mem_if->getRefBlock(plane, ax, ay, pkt.size, PaddingMode::EDGE, win)) {
                int N = static_cast<int>(recSizeToPixels(pkt.size));
                uint32_t blocks = static_cast<uint32_t>(N / 4);
                uint32_t subX = (blocks > 1) ? (pkt.i4x4_x & (blocks - 1)) : 0u;
                uint32_t subY = (blocks > 1) ? (pkt.i4x4_y & (blocks - 1)) : 0u;
                uint32_t startX = subX * 4;
                uint32_t startY = subY * 4;
                auto idx = [&](int r, int c) -> size_t { return static_cast<size_t>(r) * win.stride + static_cast<size_t>(c); };

                std::vector<uint8_t> orig(16, 0);
                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        size_t pos = idx(1 + static_cast<int>(startY + r), 1 + static_cast<int>(startX + c));
                        uint8_t v = 0;
                        if (pos < win.data.size()) v = win.data[pos];
                        orig[static_cast<size_t>(r) * 4 + c] = v;
                    }
                }

                size_t dlen = std::min(orig.size(), pkt.data.size());
                std::vector<uint8_t> res(dlen);
                for (size_t i = 0; i < dlen; ++i) {
                    int32_t diff = static_cast<int32_t>(orig[i]) - static_cast<int32_t>(pkt.data[i]);
                    // Bias by 128 to store signed residuals in uint8_t
                    int32_t biased = diff + 128;
                    biased = std::clamp(biased, 0, 255);
                    res[i] = static_cast<uint8_t>(biased);
                }

                RecPacket outpkt = pkt;
                outpkt.cmd = RecCmd::RESIDUAL;
                outpkt.data = std::move(res);

                std::vector<uint8_t> buf = packRecPacket(outpkt);

                if (cdc::components::verbose_enabled()) {
                    std::cout << "----------------Residual Packet (internal)---------------" << std::endl;
                    std::cout << outpkt;
                }
                tlm::tlm_generic_payload trans;
                trans.set_command(tlm::TLM_WRITE_COMMAND);
                trans.set_address(0);
                trans.set_data_ptr(buf.data());
                trans.set_data_length(buf.size());
                sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
                buffer_socket->b_transport(trans, delay);
                continue;
            }
        }

        // Default behaviour
        std::vector<uint8_t> buf = packRecPacket(pkt);
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0);
        trans.set_data_ptr(buf.data());
        trans.set_data_length(buf.size());
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        buffer_socket->b_transport(trans, delay);
    }
}
