#ifndef FX1_PERF_COUNTERS_H
#define FX1_PERF_COUNTERS_H

#include <cstdint>
#include <cstdio>
#include <string>

namespace fx1
{
  struct PerfCounters
  {
    // From the Systolia array (per-cycle active PE accounting)
    uint64_t active_pe_cycles{0}; // Sum over cycles of (#PEs doing a real MAC)
    uint64_t total_pe_cycles{0};  // Sum over cycles of (X*Y) while pipeline on
    uint64_t mac_ops{0};          // count of non-gated MACs actually performed

    // From the NPU top / control (timing)
    uint64_t exec_cycles{0};  // cycles with pipeline_en high (compute)
    uint64_t stall_cycles{0}; // cycles stalled (feeder empty / fifo full)
    uint64_t total_cycles{0}; // all clcoked cycles from start to done

    int X{0}, Y{0};

    // Derived metrics
    double pe_utilization() const
    {
      return total_pe_cycles ? (double)active_pe_cycles / total_pe_cycles : 0.0;
    }

    double stall_fraction() const
    {
      return total_cycles ? (double)stall_cycles / total_cycles : 0.0;
    }

    void reset()
    {
      active_pe_cycles = total_pe_cycles = mac_ops = 0;
      exec_cycles = stall_cycles = total_cycles = 0;
    }

    void report(const std::string &tag = "") const
    {
      printf("\n==== [PERF] %s (array %dx%d) ====\n", tag.c_str(), X, Y);
      printf("  total cycles      : %llu\n", (unsigned long long)total_cycles);
      printf("  exec cycles       : %llu\n", (unsigned long long)exec_cycles);
      printf("  stall cycles      : %llu  (%.1f%%)\n",
             (unsigned long long)stall_cycles, stall_fraction() * 100.0);
      printf("  MAC ops performed : %llu\n", (unsigned long long)mac_ops);
      printf("  PE utilization    : %.1f%%\n", pe_utilization() * 100.0);
      printf("=========================================\n");
    }
  };
} // namespace fx1

#endif // FX1_PERF_COUNTERS_H