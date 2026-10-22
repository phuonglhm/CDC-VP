#include "fx1_csi2/axis_monitor.h"

#include <cstdio>

namespace fx1::csi2 {

void AxisMonitor::violation(const std::string& what) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "cycle %llu: ", (unsigned long long)cycle_);
    violations_.push_back(buf + what);
}

bool AxisMonitor::axis_cycle(bool tvalid, const AxisBeat& b) {
    const bool ready = ready_ ? ready_(cycle_) : true;
    if (prev_stalled_ && (!tvalid || !(b == prev_))) violation("TVALID dropped or payload changed under stall");
    prev_stalled_ = tvalid && !ready;
    prev_ = b;
    if (tvalid && !ready) ++stalls_;
    if (tvalid && ready) {
        ++transfers_;
        const bool abort = b.tkeep == 0;
        if (abort && !(b.tlast && (b.tuser & 2))) violation("TKEEP = 0 on a beat that is not an abort beat");
        if (!abort) {
            const unsigned k = b.tkeep;
            if ((k & (k + 1)) != 0) violation("TKEEP not contiguous from bit 0");
            if (!b.tlast && k != 0xFF) violation("partial TKEEP before TLAST");
        }
        if (line_open_ && b.tid != line_tid_) violation("TID changed inside a line");
        if (line_open_ && (b.tuser & 1)) violation("TUSER[0] not on the first beat of a line");
        if (!line_open_) {
            cur_ = Line{};
            cur_.tid = b.tid;
            cur_.sof = b.tuser & 1;
        }
        if (abort) ++aborts_;
        for (unsigned i = 0; i < 8; ++i)
            if (b.tkeep >> i & 1) cur_.bytes.push_back(static_cast<uint8_t>(b.tdata >> (8 * i)));
        cur_.err |= (b.tuser & 2) != 0;
        line_open_ = !b.tlast;
        line_tid_ = b.tid;
        if (b.tlast) {
            cur_.aborted = abort;
            // An abort beat with no line open is a zero-byte beat outside a frame.
            if (!(abort && cur_.bytes.empty() && !cur_.sof)) lines_.push_back(cur_);
        }
    }
    ++cycle_;
    return ready;
}

}  // namespace fx1::csi2
