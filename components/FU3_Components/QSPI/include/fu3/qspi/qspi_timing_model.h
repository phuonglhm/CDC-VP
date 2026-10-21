#pragma once
#include <algorithm>
#include <cstdint>
#include <systemc>

namespace fu3::qspi {
class QspiTimingModel {
public:
    std::uint64_t cycles=0,ready_cycle=0,poll_start=0;
    sc_core::sc_time backend_ready=sc_core::SC_ZERO_TIME;
    void tick() { ++cycles; }
    void reset() { ready_cycle=cycles; poll_start=cycles; backend_ready=sc_core::SC_ZERO_TIME; }
    bool poll_expired(std::uint32_t timeout) const { return cycles-poll_start>=timeout; }
    bool backend_due() const { return sc_core::sc_time_stamp()>=backend_ready; }
    static bool legal_divider(std::uint32_t cr,bool ddr) { return ((cr>>24)+2)%(ddr?4:2)==0; }
    static std::uint64_t phase_cycles(unsigned bytes,unsigned lines,bool ddr,std::uint32_t cr) {
        return lines ? ((std::uint64_t(bytes)*8+lines-1)/lines)*((cr>>24)+2)/(ddr?2:1) : 0;
    }
    static std::uint64_t cs_high(std::uint32_t cr,std::uint32_t dcr) { return (((dcr>>8)&7)+1)*((cr>>24)+2); }
    static std::uint64_t poll_gap(std::uint32_t cr,std::uint32_t dcr,std::uint32_t pir) {
        return std::max<std::uint64_t>(cs_high(cr,dcr),pir);
    }
};
}
