#ifndef REC_MEM_TARGET_H
#define REC_MEM_TARGET_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "rec_memory.h"
#include <vector>
#include <algorithm>

// Combined in-repo memory target and bridge helper.
// - `MemBridge` implements `RecMemoryIf` and forwards requests to an external
//   TLM memory using an initiator socket.
// - `RecMemory` is a small local storage implementing `RecMemoryIf` for tests.

class MemBridge : public sc_core::sc_module, public RecMemoryIf {
public:
    tlm_utils::simple_initiator_socket<MemBridge> socket; // initiator to external memory
    tlm_utils::simple_target_socket<MemBridge> t_socket;   // target for modules like Cabac

    MemBridge(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket"), t_socket("t_socket") {
        t_socket.register_b_transport(this, &MemBridge::b_transport);
    }

    // Forwarding target entry: forward incoming TLM transactions to the
    // external memory via the initiator `socket`.
    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        try {
            socket->b_transport(trans, delay);
        } catch (...) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        }
    }

    bool getRefBlock(RecPlane plane,
                     uint32_t x,
                     uint32_t y,
                     uint8_t size4x4,
                     PaddingMode pad,
                     RefBlock &out) override
    {
        const uint32_t edge = recSizeToPixels(size4x4);
        // return an (N+2)x(N+2) window anchored at (x,y) so callers
        // can access top/left plus the top-right and bottom-left extras
        const uint32_t ext = edge + 2;
        uint64_t addr = encodeAddress(plane, x, y, size4x4, pad);

        out.width = ext;
        out.height = ext;
        out.stride = ext;
        out.data.assign(static_cast<size_t>(ext) * ext, 0);

        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(addr);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(out.data.data()));
        trans.set_data_length(out.data.size());
        trans.set_streaming_width(out.data.size());
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        try {
            socket->b_transport(trans, delay);
        } catch (...) { return false; }
        return trans.get_response_status() == tlm::TLM_OK_RESPONSE;
    }

    void pushRefBlock(RecPlane plane,
                      uint32_t x,
                      uint32_t y,
                      uint8_t size4x4,
                      const RefBlock &block,
                      uint64_t version) override
    {
        (void)plane; (void)x; (void)y; (void)size4x4; (void)block; (void)version;
    }

    uint64_t regionVersion(RecPlane plane,
                           uint32_t x,
                           uint32_t y,
                           uint8_t size4x4) const override
    {
        (void)plane; (void)x; (void)y; (void)size4x4; return 0;
    }

private:
    static uint64_t encodeAddress(RecPlane plane,
                                  uint32_t x,
                                  uint32_t y,
                                  uint8_t size4x4,
                                  PaddingMode pad)
    {
        uint64_t p = static_cast<uint64_t>(plane) & 0x3;
        uint64_t pd = static_cast<uint64_t>(pad) & 0x3;
        uint64_t s = static_cast<uint64_t>(size4x4) & 0x3;
        uint64_t xx = static_cast<uint64_t>(x) & 0x1FF;
        uint64_t yy = static_cast<uint64_t>(y) & 0x1FF;
        uint64_t a = (p << (2 + 2 + 9)) | (pd << (2 + 9)) | (s << 9) | (xx << 9) | yy;
        return a;
    }
};

class RecMemory : public sc_core::sc_module, public RecMemoryIf {
public:
    RecMemory(sc_core::sc_module_name name, uint32_t width = 256, uint32_t height = 256, bool use_dummy = false, uint8_t dummy_value = 128);

    bool getRefBlock(RecPlane plane,
                     uint32_t x,
                     uint32_t y,
                     uint8_t size4x4,
                     PaddingMode pad,
                     RefBlock &out) override;

private:
    uint32_t width_;
    uint32_t height_;
    std::vector<uint8_t> buf_y_;
    std::vector<uint8_t> buf_u_;
    std::vector<uint8_t> buf_v_;
    bool use_dummy_{false};
    uint8_t dummy_value_{128};
};

#endif
