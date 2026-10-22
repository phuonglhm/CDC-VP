// Checking AXI4-Stream sink for the CSI-2 stream output (plan M3: unit stream
// sink): applies a TREADY pattern, checks the protocol rules the master must
// keep and reassembles lines per TID. Usable as the platform sink monitor when
// the ISP stream input is not connected (M5).
//
// Rules checked on every cycle:
//   held    once TVALID is high without TREADY, the next cycle presents the same
//           transfer with TVALID high (IHI0051B 2.2.1)
//   tkeep   contiguous from bit 0; 0xFF on every beat but the TLAST beat; 0 only
//           on an abort beat (TLAST, TUSER[1])
//   tid     changes only after a TLAST beat (line granularity)
//   sof     TUSER[0] only on the first beat of a line
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "fx1_csi2/sys_datapath.h"

namespace fx1::csi2 {

class AxisMonitor : public AxisSink {
public:
    struct Line {
        std::vector<uint8_t> bytes;
        uint8_t tid = 0;
        bool sof = false;      // TUSER[0] on its first beat
        bool err = false;      // TUSER[1] on any of its beats
        bool aborted = false;  // closed by an abort beat (TKEEP = 0)
    };

    // TREADY as a function of the cycle number (default: always ready).
    void set_ready(std::function<bool(uint64_t)> f) { ready_ = std::move(f); }
    bool axis_cycle(bool tvalid, const AxisBeat& beat) override;

    const std::vector<Line>& lines() const { return lines_; }
    void clear_lines() { lines_.clear(); }
    uint64_t transfers() const { return transfers_; }
    uint64_t stall_cycles() const { return stalls_; }  // TVALID && !TREADY
    uint64_t abort_beats() const { return aborts_; }   // null transfers closing a line or an exposure
    const std::vector<std::string>& violations() const { return violations_; }

private:
    void violation(const std::string& what);
    std::function<bool(uint64_t)> ready_;
    uint64_t cycle_ = 0, transfers_ = 0, stalls_ = 0, aborts_ = 0;
    bool prev_stalled_ = false;
    AxisBeat prev_;
    bool line_open_ = false;
    uint8_t line_tid_ = 0;
    Line cur_;
    std::vector<Line> lines_;
    std::vector<std::string> violations_;
};

}  // namespace fx1::csi2
