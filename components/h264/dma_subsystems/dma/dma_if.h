// dma_if.h — Transaction interfaces shared by the four DMA clients and the
// arbiter.
//
// The legacy core issues word-oriented requests; h264_axi_master maps them to
// AXI4 (§5.1). This VP keeps the *legacy request record* as the transaction
// payload so client-side behaviour (addresses, ordering, completion) is what
// gets modeled and checked.
#ifndef H264_DMA_IF_H
#define H264_DMA_IF_H

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "h264_spec.h"

namespace h264 {

struct DmaRequest {
    ClientId     client  = ClientId::NONE;
    uint64_t     addr    = 0;      // byte address (< 4 GiB baseline, §5.5)
    uint32_t     beats   = 0;      // INTERNAL transfers, each size bytes
    TransferSize size    = TransferSize::B4;
    bool         is_write = false;
    uint32_t     tag     = 0;      // client-private correlation handle
    std::vector<uint8_t> data;     // immutable write payload, copied on enqueue
};

// Completion returned to the client. §5.3: done is only signalled AFTER the
// final B response is accepted and BRESP checked. A failed RRESP/BRESP must
// never be converted into a valid completion (§4.5).
struct DmaResponse {
    bool     ok    = true;
    uint32_t tag   = 0;
    uint32_t beats = 0;
    std::vector<uint8_t> data;     // ordered, addressed read bytes for this owner
};

inline size_t request_bytes(const DmaRequest& r) {
    const auto size = static_cast<uint32_t>(r.size);
    if (size != 1 && size != 2 && size != 4)
        throw std::invalid_argument("dma: invalid internal transfer size");
    return static_cast<size_t>(r.beats) * size;
}

// Byte-addressed backing store contract — the AXI DDR responder in §15.1 is
// modelled as a byte-addressed memory, and the VP follows that so read logs
// and golden comparisons line up with the verification flow.
class MemoryIf {
public:
    virtual ~MemoryIf() = default;
    virtual void    write(uint64_t addr, const uint8_t* data, size_t len) = 0;
    virtual void    read(uint64_t addr, uint8_t* out, size_t len) const = 0;
    virtual uint8_t read_byte(uint64_t addr) const = 0;
};

// A client-visible completion sink. Clients report done here so the testbench
// can assert the completion matrix in Bảng 6-1.
class CompletionSink {
public:
    virtual ~CompletionSink() = default;
    virtual void on_done(ClientId client, const DmaResponse& rsp) = 0;
};

// Deterministic request log. §15.2 lists "accepted AXI read-address logs" as a
// debug dump, and the source CMB tests compare a source-address log — so the
// log is a first-class output of the VP, not just diagnostics.
struct RequestLogEntry {
    ClientId client;
    bool     is_write;
    uint64_t addr;
    uint32_t beats;
    uint32_t size_bytes;
    uint32_t tag;
};

class RequestLog {
public:
    void record(const DmaRequest& r) {
        log_.push_back({r.client, r.is_write, r.addr, r.beats,
                        static_cast<uint32_t>(r.size), r.tag});
    }
    const std::vector<RequestLogEntry>& entries() const { return log_; }
    void clear() { log_.clear(); }
    size_t size() const { return log_.size(); }

private:
    std::vector<RequestLogEntry> log_;
};

}  // namespace h264

#endif  // H264_DMA_IF_H
