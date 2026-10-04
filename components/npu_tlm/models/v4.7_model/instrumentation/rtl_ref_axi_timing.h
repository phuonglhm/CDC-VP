// rtl_ref_axi_timing.h -- verbatim port of sauria_model's instrumentation/axi_timing.h.
//
// Only three things differ from the original; no logic or formula is changed:
//   1. header guard FX1_*_H -> SAURIA_RTL_*_H (instrumentation/perf_counters.h already uses FX1_PERF_COUNTERS_H;
//      keeping the guard would load only one of the two files).
//   2. namespace fx1 -> sauria_rtl (this tree has its own fx1::PerfCounters with a different field set: no
//      ctrl_state_cycles / act|wei_feed|stall_cycles / act|wei_l1_read_words / srama|b|c_*; one name would clash).
//   3. internal include paths -> instrumentation/rtl_ref_*.h.
//
// Kept verbatim because the port is accepted only when its run.log / CSV numbers match sauria_model exactly on
// the same job; any formula change here would break that. Behaviour changes belong in sauria_model first.
// The core's rtl_ref_* files only change includes and type names to point here.
#ifndef SAURIA_RTL_AXI_TIMING_H
#define SAURIA_RTL_AXI_TIMING_H

#include <cstdint>
#include <cstdlib>
#include <vector>

namespace sauria_rtl
{

// Timing of the AXI path instantiated by test/verilator/sauria_tester.sv for
// int8_32x32. Values are in clk_sys cycles; clk_sys runs at 2x clk_sauria.
struct AxiTimingParams
{
    uint32_t dram_latency_sys{100};
    uint32_t bandwidth_up_beats{10};
    uint32_t bandwidth_down_sys{54};
    uint32_t burst_turn_sys{2};
    uint32_t command_turn_sys{16};
    uint32_t sys_per_accel{2};

    static AxiTimingParams sauria_verilator()
    {
        return AxiTimingParams{};
    }

    // -- allows a bandwidth SWEEP. The default keeps the Verilator testbench
    // configuration; override with environment variables to sweep.
    static AxiTimingParams from_env()
    {
        AxiTimingParams p;
        if (const char *s = std::getenv("NPU_AXI_BW_UP"))
            p.bandwidth_up_beats = (uint32_t)std::atoi(s);
        if (const char *s = std::getenv("NPU_AXI_BW_DOWN"))
            p.bandwidth_down_sys = (uint32_t)std::atoi(s);
        if (const char *s = std::getenv("NPU_AXI_DRAM_LAT"))
            p.dram_latency_sys = (uint32_t)std::atoi(s);
        if (const char *s = std::getenv("NPU_AXI_BURST_TURN"))
            p.burst_turn_sys = (uint32_t)std::atoi(s);
        if (const char *s = std::getenv("NPU_AXI_CMD_TURN"))
            p.command_turn_sys = (uint32_t)std::atoi(s);
        return p;
    }

    // Effective bandwidth allowed by the throttle, at core frequency `freq_ghz`.
    //   each group of `up` beats (128 B) takes (up + down) sys cycles;
    //   f_sys = sys_per_accel * f_accel.
    // No throttle (up = 0) => one sys cycle per beat.
    double effective_gbps(double freq_ghz, uint32_t beat_bytes = 128) const
    {
        const double f_sys = freq_ghz * 1e9 * (sys_per_accel ? sys_per_accel : 1);
        if (bandwidth_up_beats == 0)
            return beat_bytes * f_sys / 1e9;
        const double per_group_sys = (double)bandwidth_up_beats + (double)bandwidth_down_sys;
        return (double)bandwidth_up_beats * beat_bytes * f_sys / per_group_sys / 1e9;
    }
};

struct AxiTimingBreakdown
{
    uint64_t commands{0};
    uint64_t bursts{0};
    uint64_t beats{0};
    uint64_t latency_sys{0};
    uint64_t payload_sys{0};
    uint64_t throttle_wait_sys{0};
    uint64_t burst_turn_sys{0};
    uint64_t command_turn_sys{0};

    uint64_t total_sys() const
    {
        return latency_sys + payload_sys + throttle_wait_sys +
               burst_turn_sys + command_turn_sys;
    }

    uint64_t accel_cycles(uint32_t sys_per_accel) const
    {
        const uint64_t ratio = sys_per_accel == 0 ? 1 : sys_per_accel;
        return (total_sys() + ratio - 1) / ratio;
    }
};

inline AxiTimingBreakdown axi_transfer_timing(
    uint64_t commands,
    const std::vector<uint16_t> &burst_beats,
    const AxiTimingParams &p = AxiTimingParams::sauria_verilator())
{
    AxiTimingBreakdown out;
    out.commands = commands;
    out.bursts = burst_beats.size();
    out.command_turn_sys = commands * p.command_turn_sys;

    for (uint16_t beats : burst_beats)
    {
        if (beats == 0)
            continue;

        out.beats += beats;
        out.latency_sys += p.dram_latency_sys;
        out.payload_sys += beats;
        out.burst_turn_sys += p.burst_turn_sys;

        // axi_delayer returns READ/WRITE_BANDWIDTH_UP beats, then inserts
        // BANDWIDTH_DOWN idle clocks. A final full group does not pay a wait.
        if (p.bandwidth_up_beats != 0)
            out.throttle_wait_sys +=
                ((static_cast<uint64_t>(beats) - 1) / p.bandwidth_up_beats) *
                p.bandwidth_down_sys;
    }
    return out;
}

} // namespace sauria_rtl

#endif // SAURIA_RTL_AXI_TIMING_H
