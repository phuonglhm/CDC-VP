#include "../include/fetch_packet.h"
#include "../include/fetch_wrapper_tlm.h"
#include <systemc>
#include <iostream>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

// Minimal FetchWrapper TLM module (sketch): implement constructor and b_transport

// Simple address encoder for example purposes. Real system will use a
// canonical mapping (plane/stride/etc.). Here we just pack plane/y/x.
static uint64_t encodeAddress(uint8_t plane, uint32_t x, uint32_t y) {
    uint64_t a = 0;
    a |= (static_cast<uint64_t>(plane) & 0xFFULL) << 56;
    a |= (static_cast<uint64_t>(y) & 0xFFFFFFFFULL) << 16;
    a |= (static_cast<uint64_t>(x) & 0xFFFFULL);
    return a;
}

FetchWrapper::FetchWrapper(sc_core::sc_module_name name)
    : sc_core::sc_module(name)
    , start_socket("start_socket")
    , out_socket("out_socket")
    , simple_mem("simple_mem")
{
    start_socket.register_b_transport(this, &FetchWrapper::b_transport);
}

void FetchWrapper::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
    // Handle incoming request (we expect a WRITE with packed FetchPacket)
    if (trans.get_command() == tlm::TLM_WRITE_COMMAND && trans.get_data_length() > 0 && trans.get_data_ptr()) {
        FetchPacket req = unpackFetchPacket(trans);

        if (req.cmd == FetchCmd::LOAD) {
            // Read row-major rectangle from mem_socket and return via out_socket
            std::vector<uint8_t> payload;
            payload.reserve(static_cast<size_t>(req.width) * req.height);

            for (uint32_t r = 0; r < req.height; ++r) {
                std::vector<uint8_t> row(static_cast<size_t>(req.width));

                tlm::tlm_generic_payload mtrans;
                mtrans.set_command(tlm::TLM_READ_COMMAND);
                uint64_t addr = encodeAddress(req.plane, req.x_px, req.y_px + r);
                mtrans.set_address(addr);
                mtrans.set_data_ptr(row.data());
                mtrans.set_data_length(row.size());
                mtrans.set_streaming_width(row.size());
                mtrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

                // Read directly from the embedded SimpleMemory
                std::vector<uint8_t> read = simple_mem.read_region(addr, row.size());
                payload.insert(payload.end(), read.begin(), read.end());
            }

            // pack response packet
            FetchPacket resp;
            resp.cmd = FetchCmd::LOAD_RESP;
            resp.req_id = req.req_id;
            resp.width = req.width;
            resp.height = req.height;
            resp.data = std::move(payload);

            std::vector<uint8_t> outbuf = packFetchPacket(resp);

            tlm::tlm_generic_payload otrans;
            otrans.set_command(tlm::TLM_WRITE_COMMAND);
            otrans.set_address(0);
            otrans.set_data_ptr(outbuf.data());
            otrans.set_data_length(outbuf.size());
            otrans.set_streaming_width(outbuf.size());
            otrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            sc_core::sc_time od = sc_core::SC_ZERO_TIME;
            try {
                out_socket->b_transport(otrans, od);
            } catch (...) {
                std::cerr << "FetchWrapper: out_socket threw in b_transport" << std::endl;
            }

            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }

        if (req.cmd == FetchCmd::WRITE_4x4) {
            // Expect req.data.size()==16
            if (req.data.size() < 16) {
                std::cerr << "FetchWrapper: WRITE_4x4 payload too small\n";
                trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
                return;
            }

            // Map 4x4 coordinate packing: assume caller encoded x_px/y_px as 4x4 indices
            uint32_t x4 = req.x_px & 0xFFFFu;
            uint32_t y4 = req.y_px & 0xFFFFu;
            uint64_t addr = encodeAddress(req.plane, x4, y4);

            tlm::tlm_generic_payload mtrans;
            mtrans.set_command(tlm::TLM_WRITE_COMMAND);
            mtrans.set_address(addr);
            mtrans.set_data_ptr(const_cast<uint8_t*>(req.data.data()));
            mtrans.set_data_length(16);
            mtrans.set_streaming_width(16);
            mtrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            // write into the embedded SimpleMemory
            simple_mem.write_region(addr, reinterpret_cast<const uint8_t*>(req.data.data()), 16);

            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }

        if (req.cmd == FetchCmd::GET_EDGE) {
            // Simplified: read (width+2)x(height+2) block and extract top/left
            const uint32_t ext_w = static_cast<uint32_t>(req.width) + 2u;
            const uint32_t ext_h = static_cast<uint32_t>(req.height) + 2u;
            std::vector<uint8_t> buf;
            buf.reserve(static_cast<size_t>(ext_w) * ext_h);
            for (uint32_t r = 0; r < ext_h; ++r) {
                std::vector<uint8_t> row(ext_w);
                tlm::tlm_generic_payload mtrans;
                mtrans.set_command(tlm::TLM_READ_COMMAND);
                uint64_t addr = encodeAddress(req.plane, req.x_px - 1, req.y_px - 1 + r);
                mtrans.set_address(addr);
                mtrans.set_data_ptr(row.data());
                mtrans.set_data_length(row.size());
                mtrans.set_streaming_width(row.size());
                mtrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
                std::vector<uint8_t> read = simple_mem.read_region(addr, row.size());
                buf.insert(buf.end(), read.begin(), read.end());
            }

            // build response containing top[32], left[32], top_left
            FetchPacket resp;
            resp.cmd = FetchCmd::EDGE_RESP;
            resp.req_id = req.req_id;

            // top: first row (skip first column) up to 32
            for (uint32_t i = 0; i < std::min<uint32_t>(32, req.width); ++i) {
                resp.data.push_back(buf[1 + i]);
            }
            // left: first column of subsequent rows (skip first row) up to 32
            for (uint32_t i = 0; i < std::min<uint32_t>(32, req.height); ++i) {
                resp.data.push_back(buf[(1 + (i+1))*ext_w + 0 + 0]); // left column
            }
            // top_left
            resp.data.push_back(buf[0]);

            std::vector<uint8_t> outbuf = packFetchPacket(resp);
            tlm::tlm_generic_payload otrans;
            otrans.set_command(tlm::TLM_WRITE_COMMAND);
            otrans.set_address(0);
            otrans.set_data_ptr(outbuf.data());
            otrans.set_data_length(outbuf.size());
            otrans.set_streaming_width(outbuf.size());
            otrans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
            sc_core::sc_time od = sc_core::SC_ZERO_TIME;
            try { out_socket->b_transport(otrans, od); } catch (...) {}

            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }

        // Unknown/unsupported command
        std::cerr << "FetchWrapper: unsupported FetchCmd " << static_cast<int>(req.cmd) << std::endl;
        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        return;
    }

    // If no payload, just ack
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}
