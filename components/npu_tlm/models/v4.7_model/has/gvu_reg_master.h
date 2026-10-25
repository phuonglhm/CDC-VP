// gvu_reg_master.h -- the register write port of the data flow controller towards the vector unit (gvu_mst of
// df_controller_top.sv, AXI4-Lite master into gvu_core.sv). Pure C++17, no SystemC: it counts clock edges that the caller gives it.
//
// A sequence of register writes is issued one write after the other (no overlap, AXI-Lite): write k of a sequence that was requested
// at edge E reaches the register file at edge E + k * L + kApply, and the sequence is over (the requester may go on) at edge E + n * L,
// with L = latency of one write seen by the requester (AW + W handshake, axi_lite_2ram, B response). L is a parameter
// (HAS_GVU_REG_LAT, default 4): an ESTIMATE until the hardware team gives the number. kApply = 2: AW / W accepted at the edge
// after the request, axi_lite_2ram drives the register write one edge later (gvu_core.sv: axi_lite_2ram, READ_LATENCY 1).
// The request edge itself counts for nothing, whatever the order of the processes of that edge: the caller passes the time of the
// edge, a request made "at" edge E is counted from the first edge strictly after E.
#ifndef HAS_GVU_REG_MASTER_H
#define HAS_GVU_REG_MASTER_H

#include <cstdint>
#include <vector>

namespace has
{
    class GvuRegMaster
    {
    public:
        struct W { uint32_t byte_addr{0}; uint32_t data{0}; unsigned strb{0xF}; };
        static constexpr unsigned kApply = 2;

        explicit GvuRegMaster(unsigned lat = 4) { set_lat(lat); }
        void set_lat(unsigned lat) { lat_ = lat < kApply ? kApply : lat; }
        unsigned lat() const { return lat_; }

        bool busy() const { return active_; }
        uint64_t writes() const { return writes_; }
        uint64_t sequences() const { return sequences_; }
        uint64_t busy_edges() const { return busy_edges_; }

        // Requests a sequence at time `now` (the time of the current edge). Only one sequence at a time.
        bool request(const std::vector<W> &seq, uint64_t now_ps)
        {
            if (active_ || seq.empty()) return false;
            seq_ = seq; req_ = now_ps; ctr_ = 0; idx_ = 0; active_ = true; sequences_++;
            return true;
        }

        struct Step { bool apply{false}; W w{}; bool drained{false}; };

        // One clock edge at time `now_ps`. apply: the register file gets `w` at this edge; drained: the sequence is over at this edge.
        Step edge(uint64_t now_ps)
        {
            Step s;
            if (!active_) return s;
            if (ctr_ == 0 && now_ps <= req_) return s;    // the edge of the request
            ctr_++;
            busy_edges_++;
            const unsigned n = static_cast<unsigned>(seq_.size());
            if (idx_ < n && ctr_ == idx_ * lat_ + kApply) { s.apply = true; s.w = seq_[idx_]; idx_++; writes_++; }
            if (ctr_ == n * lat_) { s.drained = true; active_ = false; }
            return s;
        }

    private:
        unsigned lat_{4};
        bool active_{false};
        uint64_t req_{0};
        unsigned ctr_{0}, idx_{0};
        std::vector<W> seq_;
        uint64_t writes_{0}, sequences_{0}, busy_edges_{0};
    };
}

#endif // HAS_GVU_REG_MASTER_H
