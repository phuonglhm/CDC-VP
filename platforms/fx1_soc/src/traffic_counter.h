// VP-only traffic counter on one DMA master (fx1_soc --traffic-report FILE;
// plan DIRECT_CSI_ISP D2): counts the reads and writes a master issues, with
// their bytes and lowest / highest address, at the moment the transaction
// enters the counter (before it is forwarded), and separately the ones that
// have returned, with their errors. `outstanding` is issued - returned: a
// transaction still blocked in its target at report time is visible (D2a
// review F3). Every transaction is forwarded unchanged (payload, delay,
// response). It exists only with the option: without it the sockets are bound
// to the fabric as before. Not a model of the SoC.
//
// Address windows (fx1_soc --traffic-window MASTER:LO:HI, D2b review F2): with
// windows set, every issued transaction is checked at entry; one whose bytes
// [address, address + length) do not lie wholly inside one window is counted
// `outside` (a write into the gap between two windows, or a burst running past
// the end of one, is caught; global lo / hi cannot tell those apart). The
// report then lists the windows and the count.
//
// Bandwidth events (fx1_soc --bw-report, plan CSI_ISP_BW): with an event sink
// set, every read / write gets an ID and two events, its issue and its
// response, each at its effective time: issue = sc_time_stamp() + the incoming
// local delay; response = sc_time_stamp() + the delay left when b_transport
// returns (what the target annotated and did not wait out). Command, address,
// length and byte-enable use are snapshot before forwarding. A response may
// therefore carry a time later than events reported after it: consumers order
// by time, not by report order. The counts above keep their meaning.
#pragma once

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <sstream>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::platforms::fx1_soc {

class DmaEventSink {
public:
    virtual ~DmaEventSink() = default;
    struct issue {
        unsigned master;        // as set_event_sink() named it
        std::uint64_t id;       // per counter, 1, 2, ...
        bool write;
        std::uint64_t address;
        std::uint64_t bytes;    // data length
        bool byte_enable;       // a byte-enable array was attached
        sc_core::sc_time when;  // effective issue time
    };
    virtual void dma_issue(const issue& i) = 0;
    virtual void dma_response(unsigned master, std::uint64_t id, const sc_core::sc_time& when, bool ok) = 0;
};

class TrafficCounter : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<TrafficCounter> target{"target"};
    tlm_utils::simple_initiator_socket<TrafficCounter> out{"out"};

    explicit TrafficCounter(sc_core::sc_module_name name) : sc_module(name)
    {
        target.register_b_transport(this, &TrafficCounter::b_transport);
        target.register_transport_dbg(this, &TrafficCounter::transport_dbg);
    }

    struct counts {
        std::uint64_t txns = 0, bytes = 0;    // issued (counted on entry)
        std::uint64_t done = 0, errors = 0;   // returned; errors: response not OK
        std::uint64_t lo = std::numeric_limits<std::uint64_t>::max(), hi = 0;  // issued addresses
    };
    const counts& reads() const { return rd_; }
    const counts& writes() const { return wr_; }
    std::uint64_t outstanding() const { return rd_.txns - rd_.done + wr_.txns - wr_.done; }
    // Inclusive byte range [lo, hi]; set before the simulation starts.
    void add_window(std::uint64_t lo, std::uint64_t hi) { windows_.emplace_back(lo, hi); }
    std::uint64_t outside() const { return outside_; }
    // Bandwidth events (see above); set before the simulation starts.
    void set_event_sink(DmaEventSink* sink, unsigned master)
    {
        sink_ = sink;
        master_ = master;
    }

    // "<master> reads=N read_bytes=N read_done=N read_errors=N [read_lo= read_hi=]
    //  writes=... outstanding=N [windows=LO-HI,... outside=N]" (one line,
    //  key=value; reads / writes = issued).
    std::string report(const std::string& master) const
    {
        std::ostringstream o;
        auto one = [&](const char* k, const counts& c) {
            o << ' ' << k << "s=" << c.txns << ' ' << k << "_bytes=" << c.bytes << ' ' << k << "_done=" << c.done << ' '
              << k << "_errors=" << c.errors;
            if (c.txns) o << std::hex << ' ' << k << "_lo=0x" << c.lo << ' ' << k << "_hi=0x" << c.hi << std::dec;
        };
        o << master;
        one("read", rd_);
        one("write", wr_);
        o << " outstanding=" << outstanding();
        if (!windows_.empty()) {
            o << " windows=" << std::hex;
            for (std::size_t i = 0; i < windows_.size(); ++i)
                o << (i ? "," : "") << "0x" << windows_[i].first << "-0x" << windows_[i].second;
            o << std::dec << " outside=" << outside_;
        }
        return o.str();
    }

private:
    void b_transport(tlm::tlm_generic_payload& t, sc_core::sc_time& d)
    {
        if (!t.is_read() && !t.is_write()) {  // TLM_IGNORE_COMMAND: not traffic
            out->b_transport(t, d);
            return;
        }
        // Snapshot before forwarding: the target may block, and may modify
        // the payload's address or length fields (TLM-2.0 rules allow it).
        counts& c = t.is_write() ? wr_ : rd_;
        const std::uint64_t a = t.get_address();
        const std::uint64_t n = t.get_data_length();
        ++c.txns;
        c.bytes += n;
        if (a < c.lo) c.lo = a;
        if (n && a + n - 1 > c.hi) c.hi = a + n - 1;
        if (!windows_.empty()) {
            bool inside = false;
            for (const auto& w : windows_) inside |= n && a >= w.first && a + n - 1 <= w.second;
            if (!inside) ++outside_;
        }
        std::uint64_t id = 0;
        if (sink_) {
            id = ++ids_;
            sink_->dma_issue({master_, id, t.is_write(), a, n, t.get_byte_enable_ptr() != nullptr,
                              sc_core::sc_time_stamp() + d});
        }
        out->b_transport(t, d);
        ++c.done;
        if (!t.is_response_ok()) ++c.errors;
        if (sink_) sink_->dma_response(master_, id, sc_core::sc_time_stamp() + d, t.is_response_ok());
    }
    unsigned transport_dbg(tlm::tlm_generic_payload& t) { return out->transport_dbg(t); }

    counts rd_, wr_;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> windows_;
    std::uint64_t outside_ = 0;
    DmaEventSink* sink_ = nullptr;
    unsigned master_ = 0;
    std::uint64_t ids_ = 0;
};

}  // namespace cdc::platforms::fx1_soc
