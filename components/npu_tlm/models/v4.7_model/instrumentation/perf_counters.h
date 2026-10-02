#ifndef FX1_PERF_COUNTERS_H
#define FX1_PERF_COUNTERS_H

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <algorithm>

namespace fx1
{
  struct PerfCounters
  {
    // =========================================================================
    // Base & Hardware Hooks / Measured Counters
    // =========================================================================

    // Legacy Systolic Array & PE counters
    uint64_t active_pe_cycles{0}; // Sum over cycles of (#PEs doing a real MAC)
    uint64_t total_pe_cycles{0};  // Sum over cycles of (X*Y) while pipeline on
    uint64_t mac_ops{0};          // Count of non-gated MACs actually performed
    uint64_t sa_cycles{0};        // Cycles Systolic Array (SA) is actively computing
    uint64_t obp_cycles{0};       // Cycles Output Boundary Pipeline (OBP) is active

    // Top-Level Timing
    uint64_t total_cycles{0};     // End-to-end execution cycles from start to done
    uint64_t exec_cycles{0};      // Cycles with pipeline_en high (compute)
    uint64_t stall_cycles{0};     // Cycles stalled (feeder empty / fifo full)

    // Measured Engine Cycles
    uint64_t processing_cycles{0};        // Pure compute cycles (VP: Sum of real SystemC array-active exec cycles per tile)
    uint64_t transfer_cycles{0};          // Cycles spent moving data
    uint64_t mac_engine_cycles{0};        // MAC/matrix engine active cycles (VP: = processing_cycles)
    uint64_t dma_engine_cycles{0};        // DMA engine active cycles (VP: = transfer_cycles)
    uint64_t activation_engine_cycles{0}; // Activation/vector op engine cycles
    uint64_t pooling_engine_cycles{0};    // Pooling engine cycles
    uint64_t reshape_engine_cycles{0};    // Reshape/shape engine cycles
    uint64_t reduction_engine_cycles{0};  // Reduction/softmax-like cycles

    // DMA Measured & Helper Data
    uint64_t ddr_read_bytes{0};        // Measured read bytes from DDR
    uint64_t ddr_write_bytes{0};       // Measured write bytes to DDR
    uint64_t dma_read_cycles_raw{0};   // Raw measured DMA read active cycles
    uint64_t dma_write_cycles_raw{0};  // Raw measured DMA write active cycles
    uint64_t dma_stall_cycles{0};      // Double-buffer exposed stall cycles (VP: Sum max(0, store(t-1)+load(t) - compute(t)))
    uint64_t dma_setup_cycles{0};      // Per-tile DMA setup overhead cycles

    // Memory Wait & Pipeline Stall Specific Member Fields
    uint64_t sram_conflict_cycles{0};       // SRAM bank conflict cycles
    uint64_t memory_arbitration_cycles{0};  // NoC/bus arbitration cycles
    uint64_t wait_input_cycles{0};          // Compute waits for input data
    uint64_t wait_output_cycles{0};         // Compute waits for output buffer/writeback
    uint64_t pipeline_bubble_override{0};   // Pipeline empty/bubble cycle override
    uint64_t synchronization_cycles{0};     // Synchronization overhead
    uint64_t dependency_stall_cycles{0};   // Producer-consumer dependency stall
    uint64_t n_regs{0};                     // Config registers count (config_bytes / 4)
    uint64_t instruction_stall_override{0}; // Instruction stall cycle override

    // Memory Traffic, Footprint & Architectural Member Fields
    double freq_ghz{0.8};                   // NPU clock frequency in GHz (default 0.8 GHz = 800 MHz)
    uint64_t elem_bytes{1};                 // Default element size in bytes (1 for INT8, 2 for FP16)
    uint64_t A_bytes{1};                    // Activation element size in bytes
    uint64_t B_bytes{1};                    // Weight element size in bytes
    uint64_t C_bytes{4};                    // Accumulator / output element size in bytes (4 for INT32/FP32)
    uint64_t Y_used{0};                     // Active Y rows used (defaults to Y)
    uint64_t ncontexts{1};                  // Context count
    uint64_t mvm_k{0};                      // MVM K dimension
    uint64_t weight_bytes_override{0};      // Direct override for weight_bytes
    uint64_t l2_to_l1_bytes_override{0};    // Direct override for l2_to_l1_bytes
    uint64_t l1_to_l2_bytes_override{0};    // Direct override for l1_to_l2_bytes
    uint64_t max_l2_taken_size_override{0}; // Peak L2 occupancy override
    double peak_external_bw_gbps_val{16.0}; // Peak external DDR bandwidth (16.0 GB/s default)

    // Geometry & Architectural Placeholders
    int X{0}, Y{0};               // Systolic array dimensions (X x Y)
    double dma_bw_bpc{32.0};      // DMA bandwidth placeholder in bytes per cycle (default: 32 B/cycle)
    uint64_t M{0}, K{0}, N{0};    // Problem dimensions for ideal lower-bound theory_min_cycles
    uint64_t reps{1};             // Repetition count for theory_min_cycles
    uint64_t theory_min_override{0}; // Direct override for theory_min_cycles

    // =========================================================================
    // Category 1: Cycle Counter Accessors & Logic
    // =========================================================================

    // 1. total_cycles: End-to-end execution cycles
    uint64_t get_total_cycles() const
    {
      return total_cycles;
    }

    // 2. processing_cycles: Pure compute cycles
    uint64_t get_processing_cycles() const
    {
      if (processing_cycles > 0) return processing_cycles;
      if (sa_cycles > 0) return sa_cycles;
      return exec_cycles;
    }

    // 3. transfer_cycles: Cycles spent moving data
    uint64_t get_transfer_cycles() const
    {
      if (transfer_cycles > 0) return transfer_cycles;
      if (dma_engine_cycles > 0) return dma_engine_cycles;
      return get_dma_read_cycles() + get_dma_write_cycles() + dma_setup_cycles;
    }

    // 4. transfer_overhead_cycles: Transfer overhead excluding useful compute (VP: total_cycles - processing_cycles)
    uint64_t get_transfer_overhead_cycles() const
    {
      uint64_t proc = get_processing_cycles();
      return (total_cycles > proc) ? (total_cycles - proc) : 0;
    }

    // 5. transfer_overhead_percent: Percent of total cycles used by overhead (VP: 100*transfer_overhead_cycles/total_cycles)
    double get_transfer_overhead_percent() const
    {
      return total_cycles ? (100.0 * static_cast<double>(get_transfer_overhead_cycles()) / static_cast<double>(total_cycles)) : 0.0;
    }

    // 6. theory_min_cycles: Ideal lower-bound cycles (VP: ceil(M*K*N/PEs)*reps)
    uint64_t get_theory_min_cycles() const
    {
      if (theory_min_override > 0) return theory_min_override;
      uint64_t pes = (X > 0 && Y > 0) ? (static_cast<uint64_t>(X) * static_cast<uint64_t>(Y)) : 1;
      if (M > 0 && K > 0 && N > 0)
      {
        double total_macs = static_cast<double>(M) * static_cast<double>(K) * static_cast<double>(N);
        uint64_t ideal_tile = static_cast<uint64_t>(std::ceil(total_macs / static_cast<double>(pes)));
        return ideal_tile * (reps ? reps : 1);
      }
      return 0;
    }

    // 7. active_cycles: Cycles with useful engine activity (VP: = processing_cycles)
    uint64_t get_active_cycles() const
    {
      return get_processing_cycles();
    }

    // 8. idle_cycles: Cycles without useful work (VP: total_cycles - processing_cycles)
    uint64_t get_idle_cycles() const
    {
      uint64_t proc = get_processing_cycles();
      return (total_cycles > proc) ? (total_cycles - proc) : 0;
    }

    // =========================================================================
    // Category 2: Engine Cycle Accessors & Logic
    // =========================================================================

    // 9. mac_engine_cycles: MAC/matrix engine active cycles (VP: = processing_cycles)
    uint64_t get_mac_engine_cycles() const
    {
      if (mac_engine_cycles > 0) return mac_engine_cycles;
      return get_processing_cycles();
    }

    // 10. dma_engine_cycles: DMA engine active cycles (VP: = transfer_cycles)
    uint64_t get_dma_engine_cycles() const
    {
      if (dma_engine_cycles > 0) return dma_engine_cycles;
      return get_transfer_cycles();
    }

    // 11. activation_engine_cycles: Activation/vector op engine cycles
    uint64_t get_activation_engine_cycles() const
    {
      if (activation_engine_cycles > 0) return activation_engine_cycles;
      return obp_cycles;
    }

    // 12. pooling_engine_cycles: Pooling engine cycles
    uint64_t get_pooling_engine_cycles() const
    {
      return pooling_engine_cycles;
    }

    // 13. reshape_engine_cycles: Reshape/shape engine cycles
    uint64_t get_reshape_engine_cycles() const
    {
      return reshape_engine_cycles;
    }

    // 14. reduction_engine_cycles: Reduction/softmax-like cycles
    uint64_t get_reduction_engine_cycles() const
    {
      return reduction_engine_cycles;
    }

    // =========================================================================
    // Category 3: Engine Utilization Accessors & Logic
    // =========================================================================

    // 15. engine_utilization: Per-engine utilization percentage (VP: 100*mac_engine_cycles/total_cycles)
    double get_engine_utilization() const
    {
      return total_cycles ? (100.0 * static_cast<double>(get_mac_engine_cycles()) / static_cast<double>(total_cycles)) : 0.0;
    }

    // =========================================================================
    // Category 4: DMA Cycle Accessors & Logic
    // =========================================================================

    // 16. dma_read_cycles: DMA read service cycles (VP: ceil(ddr_read_bytes/BW_bpc))
    uint64_t get_dma_read_cycles() const
    {
      if (dma_read_cycles_raw > 0) return dma_read_cycles_raw;
      if (dma_bw_bpc > 0.0 && ddr_read_bytes > 0)
      {
        return static_cast<uint64_t>(std::ceil(static_cast<double>(ddr_read_bytes) / dma_bw_bpc));
      }
      return 0;
    }

    // 17. dma_write_cycles: DMA write service cycles (VP: ceil(ddr_write_bytes/BW_bpc))
    uint64_t get_dma_write_cycles() const
    {
      if (dma_write_cycles_raw > 0) return dma_write_cycles_raw;
      if (dma_bw_bpc > 0.0 && ddr_write_bytes > 0)
      {
        return static_cast<uint64_t>(std::ceil(static_cast<double>(ddr_write_bytes) / dma_bw_bpc));
      }
      return 0;
    }

    // 18. dma_busy_cycles: DMA busy cycles (VP: = transfer_cycles)
    uint64_t get_dma_busy_cycles() const
    {
      return get_transfer_cycles();
    }

    // 19. dma_idle_cycles: DMA idle cycles (VP: total_cycles - transfer_cycles)
    uint64_t get_dma_idle_cycles() const
    {
      uint64_t xfer = get_transfer_cycles();
      return (total_cycles > xfer) ? (total_cycles - xfer) : 0;
    }

    // 20. dma_stall_cycles: Cycles DMA is blocked/stalled
    uint64_t get_dma_stall_cycles() const
    {
      return dma_stall_cycles;
    }

    // 21. dma_wait_cycles: Cycles waiting for DMA completion (VP: = dma_stall_cycles)
    uint64_t get_dma_wait_cycles() const
    {
      return get_dma_stall_cycles();
    }

    // =========================================================================
    // Category 5: Memory Wait Accessors & Logic
    // =========================================================================

    // 22. ddr_wait_cycles: Waiting for DDR response (VP: = dma_stall_cycles*reps)
    uint64_t get_ddr_wait_cycles() const
    {
      return get_dma_stall_cycles() * (reps ? reps : 1);
    }

    // 23. l2_wait_cycles: Waiting for L2/shared SRAM (VP: = wait_output_cycles*reps)
    uint64_t get_l2_wait_cycles() const
    {
      return get_wait_output_cycles() * (reps ? reps : 1);
    }

    // 24. l1_wait_cycles: Waiting for L1/local SRAM (VP: = wait_input_cycles*reps)
    uint64_t get_l1_wait_cycles() const
    {
      return get_wait_input_cycles() * (reps ? reps : 1);
    }

    // 25. sram_conflict_cycles: SRAM bank conflict cycles
    uint64_t get_sram_conflict_cycles() const
    {
      return sram_conflict_cycles;
    }

    // 26. memory_arbitration_cycles: NoC/bus arbitration cycles
    uint64_t get_memory_arbitration_cycles() const
    {
      return memory_arbitration_cycles;
    }

    // =========================================================================
    // Category 6: Pipeline Stall Accessors & Logic
    // =========================================================================

    // 27. wait_input_cycles: Compute waits for input data
    uint64_t get_wait_input_cycles() const
    {
      if (wait_input_cycles > 0) return wait_input_cycles;
      return stall_cycles;
    }

    // 28. wait_output_cycles: Compute waits for output buffer/writeback
    uint64_t get_wait_output_cycles() const
    {
      return wait_output_cycles;
    }

    // 29. pipeline_bubble_cycles: Pipeline empty/bubble cycles (VP: (X+Y)*tiles fill+drain)
    uint64_t get_pipeline_bubble_cycles() const
    {
      if (pipeline_bubble_override > 0) return pipeline_bubble_override;
      if (X > 0 && Y > 0)
      {
        return static_cast<uint64_t>(X + Y) * (reps ? reps : 1);
      }
      return 0;
    }

    // 30. synchronization_cycles: Synchronization overhead
    uint64_t get_synchronization_cycles() const
    {
      return synchronization_cycles;
    }

    // 31. dependency_stall_cycles: Producer-consumer dependency stall
    uint64_t get_dependency_stall_cycles() const
    {
      return dependency_stall_cycles;
    }

    // 32. instruction_stall_cycles: Instruction issue/config stall (VP: N_REGS*reps)
    uint64_t get_instruction_stall_cycles() const
    {
      if (instruction_stall_override > 0) return instruction_stall_override;
      return n_regs * (reps ? reps : 1);
    }

    // =========================================================================
    // Category 7: Utilization Counter Accessors & Logic
    // =========================================================================

    // 33. mac_active_cycles: MAC active cycles (VP: = processing_cycles)
    uint64_t get_mac_active_cycles() const
    {
      return get_processing_cycles();
    }

    // 34. mac_idle_cycles: MAC idle cycles (VP: total_cycles - mac_active_cycles)
    uint64_t get_mac_idle_cycles() const
    {
      uint64_t mac_act = get_mac_active_cycles();
      return (total_cycles > mac_act) ? (total_cycles - mac_act) : 0;
    }

    // 35. pe_active_cycles: PE active cycles (VP: = processing_cycles)
    uint64_t get_pe_active_cycles() const
    {
      return get_processing_cycles();
    }

    // 36. pe_idle_cycles: PE idle cycles (VP: total_cycles - pe_active_cycles)
    uint64_t get_pe_idle_cycles() const
    {
      uint64_t pe_act = get_pe_active_cycles();
      return (total_cycles > pe_act) ? (total_cycles - pe_act) : 0;
    }

    // 37. engine_active_cycles: Any engine active cycles (VP: max(mac_engine_cycles, dma_engine_cycles))
    uint64_t get_engine_active_cycles() const
    {
      return std::max(get_mac_engine_cycles(), get_dma_engine_cycles());
    }

    // 38. engine_idle_cycles: Any engine idle cycles (VP: total_cycles - engine_active_cycles)
    uint64_t get_engine_idle_cycles() const
    {
      uint64_t eng_act = get_engine_active_cycles();
      return (total_cycles > eng_act) ? (total_cycles - eng_act) : 0;
    }

    // =========================================================================
    // Category 8: Memory Traffic Accessors & Logic
    // =========================================================================

    // 39. ddr_read_bytes: External DDR read traffic
    uint64_t get_ddr_read_bytes() const
    {
      return ddr_read_bytes;
    }

    // 40. ddr_write_bytes: External DDR write traffic
    uint64_t get_ddr_write_bytes() const
    {
      return ddr_write_bytes;
    }

    // 41. weight_bytes: Weight read traffic (VP: Sum B(weight) tile bytes reloaded when not kept)
    uint64_t get_weight_bytes() const
    {
      if (weight_bytes_override > 0) return weight_bytes_override;
      if (K > 0 && N > 0)
      {
        return K * N * B_bytes * (reps ? reps : 1);
      }
      return ddr_read_bytes;
    }

    // 42. bias_bytes: Bias/control read traffic (VP: N*4*reps)
    uint64_t get_bias_bytes() const
    {
      if (N > 0) return N * 4 * (reps ? reps : 1);
      return 0;
    }

    // 43. l2_to_l1_bytes: L2 to L1 internal traffic (VP: ncontexts*mvm_k*(Yused*A_bytes + X*B_bytes))
    uint64_t get_l2_to_l1_bytes() const
    {
      if (l2_to_l1_bytes_override > 0) return l2_to_l1_bytes_override;
      uint64_t y_eff = (Y_used > 0) ? Y_used : static_cast<uint64_t>(Y > 0 ? Y : 1);
      uint64_t k_eff = (mvm_k > 0) ? mvm_k : (K > 0 ? K : 1);
      uint64_t x_eff = static_cast<uint64_t>(X > 0 ? X : 1);
      uint64_t ctx = (ncontexts > 0) ? ncontexts : 1;
      return ctx * k_eff * (y_eff * A_bytes + x_eff * B_bytes);
    }

    // 44. l1_to_l2_bytes: L1 to L2 internal traffic (VP: Sum output-drain bytes = per-tile store bytes)
    uint64_t get_l1_to_l2_bytes() const
    {
      if (l1_to_l2_bytes_override > 0) return l1_to_l2_bytes_override;
      if (ddr_write_bytes > 0) return ddr_write_bytes;
      uint64_t y_eff = (Y_used > 0) ? Y_used : static_cast<uint64_t>(Y > 0 ? Y : 1);
      uint64_t x_eff = static_cast<uint64_t>(X > 0 ? X : 1);
      return y_eff * x_eff * C_bytes * (reps ? reps : 1);
    }

    // 45. l1_read_bytes: L1 read traffic (VP: = l2_to_l1_bytes)
    uint64_t get_l1_read_bytes() const
    {
      return get_l2_to_l1_bytes();
    }

    // 46. l1_write_bytes: L1 write traffic (VP: = l1_to_l2_bytes)
    uint64_t get_l1_write_bytes() const
    {
      return get_l1_to_l2_bytes();
    }

    // 47. l2_read_bytes: L2 read traffic (VP: = ddr_read_bytes)
    uint64_t get_l2_read_bytes() const
    {
      return get_ddr_read_bytes();
    }

    // 48. l2_write_bytes: L2 write traffic (VP: = ddr_write_bytes)
    uint64_t get_l2_write_bytes() const
    {
      return get_ddr_write_bytes();
    }

    // =========================================================================
    // Category 9: Footprint Accessors & Logic
    // =========================================================================

    // 49. l2_footprint_bytes: Layer working set in L2 (VP: Yused*K*A_bytes + X*K*B_bytes + Yused*X*C_bytes)
    uint64_t get_l2_footprint_bytes() const
    {
      uint64_t y_eff = (Y_used > 0) ? Y_used : static_cast<uint64_t>(Y > 0 ? Y : 1);
      uint64_t k_eff = (K > 0) ? K : 1;
      uint64_t x_eff = static_cast<uint64_t>(X > 0 ? X : 1);
      return (y_eff * k_eff * A_bytes) + (x_eff * k_eff * B_bytes) + (y_eff * x_eff * C_bytes);
    }

    // 50. max_l2_taken_size_bytes: Peak L2 taken size (VP: max over layers of l2_footprint_bytes)
    uint64_t get_max_l2_taken_size_bytes() const
    {
      if (max_l2_taken_size_override > 0) return max_l2_taken_size_override;
      return get_l2_footprint_bytes();
    }

    // 51. ddr_footprint_data_bytes: DDR data footprint (VP: M*K*elem_bytes*reps)
    uint64_t get_ddr_footprint_data_bytes() const
    {
      if (M > 0 && K > 0)
      {
        return M * K * elem_bytes * (reps ? reps : 1);
      }
      return ddr_read_bytes;
    }

    // 52. ddr_footprint_weight_bytes: DDR weight footprint (VP: K*N*elem_bytes*reps)
    uint64_t get_ddr_footprint_weight_bytes() const
    {
      if (K > 0 && N > 0)
      {
        return K * N * elem_bytes * (reps ? reps : 1);
      }
      return get_weight_bytes();
    }

    // 53. ddr_footprint_control_bytes: DDR control/config footprint (VP: N_REGS*4)
    uint64_t get_ddr_footprint_control_bytes() const
    {
      return n_regs * 4;
    }

    // =========================================================================
    // Category 10: Bandwidth Accessors & Logic
    // =========================================================================

    // 54. internal_bw_gbps: Internal L1-L2 bandwidth in GB/s (VP: (l2_to_l1+l1_to_l2)*freq_GHz/total_cycles)
    double get_internal_bw_gbps() const
    {
      if (total_cycles == 0) return 0.0;
      double total_bytes = static_cast<double>(get_l2_to_l1_bytes() + get_l1_to_l2_bytes());
      return (total_bytes * freq_ghz) / static_cast<double>(total_cycles);
    }

    // 55. external_bw_gbps: External DDR bandwidth in GB/s (VP: (ddr_read+ddr_write)*freq_GHz/total_cycles)
    double get_external_bw_gbps() const
    {
      if (total_cycles == 0) return 0.0;
      double total_bytes = static_cast<double>(get_ddr_read_bytes() + get_ddr_write_bytes());
      return (total_bytes * freq_ghz) / static_cast<double>(total_cycles);
    }

    // 56. peak_internal_bw_gbps: Peak internal memory bandwidth in GB/s (VP: (X+Y)*elem_bytes*freq_GHz)
    double get_peak_internal_bw_gbps() const
    {
      return static_cast<double>(X + Y) * static_cast<double>(elem_bytes) * freq_ghz;
    }

    // 57. peak_external_bw_gbps: Peak external DDR bandwidth in GB/s (VP: DMA DDR bandwidth placeholder 16.0 GB/s)
    double get_peak_external_bw_gbps() const
    {
      return peak_external_bw_gbps_val;
    }

    // =========================================================================
    // Category 11: Throughput Accessors & Logic
    // =========================================================================

    // Latency helper in seconds
    double get_latency_s() const
    {
      if (freq_ghz <= 0.0 || total_cycles == 0) return 0.0;
      return static_cast<double>(total_cycles) / (freq_ghz * 1e9);
    }

    // 58. ips: Inferences per second (VP: 1 / latency_s)
    double get_ips() const
    {
      double lat_s = get_latency_s();
      return (lat_s > 0.0) ? (1.0 / lat_s) : 0.0;
    }

    // 59. ips_bw_limited: Bandwidth-limited IPS estimate (VP: 1 / (Sum ddr_bytes / peak_DDR_BW))
    double get_ips_bw_limited() const
    {
      double total_ddr = static_cast<double>(get_ddr_read_bytes() + get_ddr_write_bytes());
      double peak_bw_bytes_sec = get_peak_external_bw_gbps() * 1e9;
      if (total_ddr > 0.0 && peak_bw_bytes_sec > 0.0)
      {
        double bw_time_s = total_ddr / peak_bw_bytes_sec;
        return (bw_time_s > 0.0) ? (1.0 / bw_time_s) : 0.0;
      }
      return 0.0;
    }

    // 60. algorithmic_tops: Algorithmic TOPS (VP: 2*M*K*N / latency_s / 1e12)
    double get_algorithmic_tops() const
    {
      double lat_s = get_latency_s();
      if (lat_s <= 0.0) return 0.0;
      double total_mac_ops = 2.0 * static_cast<double>(M > 0 ? M : 1) *
                                  static_cast<double>(K > 0 ? K : 1) *
                                  static_cast<double>(N > 0 ? N : 1) *
                                  static_cast<double>(reps ? reps : 1);
      return (total_mac_ops / lat_s) / 1e12;
    }

    // =========================================================================
    // Legacy Utilities
    // =========================================================================

    double pe_utilization() const
    {
      return total_pe_cycles ? (static_cast<double>(active_pe_cycles) / static_cast<double>(total_pe_cycles)) : 0.0;
    }

    double stall_fraction() const
    {
      return total_cycles ? (static_cast<double>(stall_cycles) / static_cast<double>(total_cycles)) : 0.0;
    }

    void reset()
    {
      active_pe_cycles = total_pe_cycles = mac_ops = 0;
      sa_cycles = obp_cycles = 0;
      exec_cycles = stall_cycles = total_cycles = 0;
      processing_cycles = transfer_cycles = 0;
      mac_engine_cycles = dma_engine_cycles = activation_engine_cycles = 0;
      pooling_engine_cycles = reshape_engine_cycles = reduction_engine_cycles = 0;
      ddr_read_bytes = ddr_write_bytes = dma_read_cycles_raw = dma_write_cycles_raw = 0;
      dma_stall_cycles = dma_setup_cycles = 0;
      sram_conflict_cycles = memory_arbitration_cycles = 0;
      wait_input_cycles = wait_output_cycles = pipeline_bubble_override = 0;
      synchronization_cycles = dependency_stall_cycles = n_regs = instruction_stall_override = 0;
      freq_ghz = 0.8;
      elem_bytes = A_bytes = B_bytes = 1;
      C_bytes = 4;
      Y_used = ncontexts = mvm_k = 0;
      weight_bytes_override = l2_to_l1_bytes_override = l1_to_l2_bytes_override = max_l2_taken_size_override = 0;
      peak_external_bw_gbps_val = 16.0;
      M = K = N = theory_min_override = 0;
      reps = 1;
    }

    void report(const std::string &tag = "") const
    {
      printf("\n=====================================================================================================\n");
      printf("                           SAURIA NPU PERFORMANCE COUNTER REPORT %s\n", tag.empty() ? "" : ("[" + tag + "]").c_str());
      if (X > 0 && Y > 0)
      {
        printf(" Architecture Geometry : %dx%d PE Array | Frequency: %.2f GHz\n", X, Y, freq_ghz);
      }
      printf("-----------------------------------------------------------------------------------------------------\n");
      printf(" %-19s | %-26s | %-13s | %-6s | %s\n", "Category", "Metric", "Value", "Unit", "Description");
      printf("-----------------------------------------------------------------------------------------------------\n");

      // Category 1: Cycle Counter
      printf(" %-19s | %-26s | %-13llu | %-6s | End-to-end execution cycles.\n",
             "Cycle Counter", "total_cycles", (unsigned long long)get_total_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Pure compute cycles.\n",
             "Cycle Counter", "processing_cycles", (unsigned long long)get_processing_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Cycles spent moving data.\n",
             "Cycle Counter", "transfer_cycles", (unsigned long long)get_transfer_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Transfer overhead excluding useful compute.\n",
             "Cycle Counter", "transfer_overhead_cycles", (unsigned long long)get_transfer_overhead_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13.2f | %-6s | Percent of total cycles used by overhead.\n",
             "Cycle Counter", "transfer_overhead_percent", get_transfer_overhead_percent(), "%");
      printf(" %-19s | %-26s | %-13llu | %-6s | Ideal lower-bound cycles.\n",
             "Cycle Counter", "theory_min_cycles", (unsigned long long)get_theory_min_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Cycles with useful engine activity.\n",
             "Cycle Counter", "active_cycles", (unsigned long long)get_active_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Cycles without useful work.\n",
             "Cycle Counter", "idle_cycles", (unsigned long long)get_idle_cycles(), "cycles");

      // Category 2: Engine Cycle
      printf(" %-19s | %-26s | %-13llu | %-6s | MAC/matrix engine active cycles.\n",
             "Engine Cycle", "mac_engine_cycles", (unsigned long long)get_mac_engine_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | DMA engine active cycles.\n",
             "Engine Cycle", "dma_engine_cycles", (unsigned long long)get_dma_engine_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Activation/vector op engine cycles.\n",
             "Engine Cycle", "activation_engine_cycles", (unsigned long long)get_activation_engine_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Pooling engine cycles.\n",
             "Engine Cycle", "pooling_engine_cycles", (unsigned long long)get_pooling_engine_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Reshape/shape engine cycles.\n",
             "Engine Cycle", "reshape_engine_cycles", (unsigned long long)get_reshape_engine_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Reduction/softmax-like cycles.\n",
             "Engine Cycle", "reduction_engine_cycles", (unsigned long long)get_reduction_engine_cycles(), "cycles");

      // Category 3: Engine Utilization
      printf(" %-19s | %-26s | %-13.2f | %-6s | Per-engine utilization percentage.\n",
             "Engine Utilization", "engine_utilization", get_engine_utilization(), "%");

      // Category 4: DMA Cycle
      printf(" %-19s | %-26s | %-13llu | %-6s | DMA read service cycles.\n",
             "DMA Cycle", "dma_read_cycles", (unsigned long long)get_dma_read_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | DMA write service cycles.\n",
             "DMA Cycle", "dma_write_cycles", (unsigned long long)get_dma_write_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | DMA busy cycles.\n",
             "DMA Cycle", "dma_busy_cycles", (unsigned long long)get_dma_busy_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | DMA idle cycles.\n",
             "DMA Cycle", "dma_idle_cycles", (unsigned long long)get_dma_idle_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Cycles DMA is blocked/stalled.\n",
             "DMA Cycle", "dma_stall_cycles", (unsigned long long)get_dma_stall_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Cycles waiting for DMA completion.\n",
             "DMA Cycle", "dma_wait_cycles", (unsigned long long)get_dma_wait_cycles(), "cycles");

      // Category 5: Memory Wait
      printf(" %-19s | %-26s | %-13llu | %-6s | Waiting for DDR response.\n",
             "Memory Wait", "ddr_wait_cycles", (unsigned long long)get_ddr_wait_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Waiting for L2/shared SRAM.\n",
             "Memory Wait", "l2_wait_cycles", (unsigned long long)get_l2_wait_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Waiting for L1/local SRAM.\n",
             "Memory Wait", "l1_wait_cycles", (unsigned long long)get_l1_wait_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | SRAM bank conflict cycles.\n",
             "Memory Wait", "sram_conflict_cycles", (unsigned long long)get_sram_conflict_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | NoC/bus arbitration cycles.\n",
             "Memory Wait", "memory_arbitration_cycles", (unsigned long long)get_memory_arbitration_cycles(), "cycles");

      // Category 6: Pipeline Stall
      printf(" %-19s | %-26s | %-13llu | %-6s | Compute waits for input data.\n",
             "Pipeline Stall", "wait_input_cycles", (unsigned long long)get_wait_input_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Compute waits for output buffer/writeback.\n",
             "Pipeline Stall", "wait_output_cycles", (unsigned long long)get_wait_output_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Pipeline empty/bubble cycles.\n",
             "Pipeline Stall", "pipeline_bubble_cycles", (unsigned long long)get_pipeline_bubble_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Synchronization overhead.\n",
             "Pipeline Stall", "synchronization_cycles", (unsigned long long)get_synchronization_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Producer-consumer dependency stall.\n",
             "Pipeline Stall", "dependency_stall_cycles", (unsigned long long)get_dependency_stall_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Instruction issue/config stall.\n",
             "Pipeline Stall", "instruction_stall_cycles", (unsigned long long)get_instruction_stall_cycles(), "cycles");

      // Category 7: Utilization Counter
      printf(" %-19s | %-26s | %-13llu | %-6s | MAC active cycles.\n",
             "Utilization Counter", "mac_active_cycles", (unsigned long long)get_mac_active_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | MAC idle cycles.\n",
             "Utilization Counter", "mac_idle_cycles", (unsigned long long)get_mac_idle_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | PE active cycles.\n",
             "Utilization Counter", "pe_active_cycles", (unsigned long long)get_pe_active_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | PE idle cycles.\n",
             "Utilization Counter", "pe_idle_cycles", (unsigned long long)get_pe_idle_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Any engine active cycles.\n",
             "Utilization Counter", "engine_active_cycles", (unsigned long long)get_engine_active_cycles(), "cycles");
      printf(" %-19s | %-26s | %-13llu | %-6s | Any engine idle cycles.\n",
             "Utilization Counter", "engine_idle_cycles", (unsigned long long)get_engine_idle_cycles(), "cycles");

      // Category 8: Memory Traffic
      printf(" %-19s | %-26s | %-13llu | %-6s | External DDR read traffic.\n",
             "Memory Traffic", "ddr_read_bytes", (unsigned long long)get_ddr_read_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | External DDR write traffic.\n",
             "Memory Traffic", "ddr_write_bytes", (unsigned long long)get_ddr_write_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | Weight read traffic.\n",
             "Memory Traffic", "weight_bytes", (unsigned long long)get_weight_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | Bias/control read traffic.\n",
             "Memory Traffic", "bias_bytes", (unsigned long long)get_bias_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | L2 to L1 internal traffic.\n",
             "Memory Traffic", "l2_to_l1_bytes", (unsigned long long)get_l2_to_l1_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | L1 to L2 internal traffic.\n",
             "Memory Traffic", "l1_to_l2_bytes", (unsigned long long)get_l1_to_l2_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | L1 read traffic.\n",
             "Memory Traffic", "l1_read_bytes", (unsigned long long)get_l1_read_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | L1 write traffic.\n",
             "Memory Traffic", "l1_write_bytes", (unsigned long long)get_l1_write_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | L2 read traffic.\n",
             "Memory Traffic", "l2_read_bytes", (unsigned long long)get_l2_read_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | L2 write traffic.\n",
             "Memory Traffic", "l2_write_bytes", (unsigned long long)get_l2_write_bytes(), "bytes");

      // Category 9: Footprint
      printf(" %-19s | %-26s | %-13llu | %-6s | Layer working set in L2.\n",
             "Footprint", "l2_footprint_bytes", (unsigned long long)get_l2_footprint_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | Peak L2 taken size.\n",
             "Footprint", "max_l2_taken_size_bytes", (unsigned long long)get_max_l2_taken_size_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | DDR data footprint.\n",
             "Footprint", "ddr_footprint_data_bytes", (unsigned long long)get_ddr_footprint_data_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | DDR weight footprint.\n",
             "Footprint", "ddr_footprint_weight_bytes", (unsigned long long)get_ddr_footprint_weight_bytes(), "bytes");
      printf(" %-19s | %-26s | %-13llu | %-6s | DDR control/config footprint.\n",
             "Footprint", "ddr_footprint_control_bytes", (unsigned long long)get_ddr_footprint_control_bytes(), "bytes");

      // Category 10: Bandwidth
      printf(" %-19s | %-26s | %-13.2f | %-6s | Internal L1-L2 bandwidth.\n",
             "Bandwidth", "internal_bw_gbps", get_internal_bw_gbps(), "GB/s");
      printf(" %-19s | %-26s | %-13.2f | %-6s | External DDR bandwidth.\n",
             "Bandwidth", "external_bw_gbps", get_external_bw_gbps(), "GB/s");
      printf(" %-19s | %-26s | %-13.2f | %-6s | Peak internal memory bandwidth.\n",
             "Bandwidth", "peak_internal_bw_gbps", get_peak_internal_bw_gbps(), "GB/s");
      printf(" %-19s | %-26s | %-13.2f | %-6s | Peak external DDR bandwidth.\n",
             "Bandwidth", "peak_external_bw_gbps", get_peak_external_bw_gbps(), "GB/s");

      // Category 11: Throughput
      printf(" %-19s | %-26s | %-13.2f | %-6s | Inferences per second.\n",
             "Throughput", "ips", get_ips(), "inf/s");
      printf(" %-19s | %-26s | %-13.2f | %-6s | Bandwidth-limited IPS estimate.\n",
             "Throughput", "ips_bw_limited", get_ips_bw_limited(), "inf/s");
      printf(" %-19s | %-26s | %-13.4f | %-6s | Algorithmic TOPS.\n",
             "Throughput", "algorithmic_tops", get_algorithmic_tops(), "TOPS");

      printf("-----------------------------------------------------------------------------------------------------\n");
      printf(" Legacy / PE Details    : MAC ops = %llu, Active PE Cycles = %llu, PE Util = %.2f%%\n",
             (unsigned long long)mac_ops, (unsigned long long)active_pe_cycles, pe_utilization() * 100.0);
      printf("=====================================================================================================\n\n");
    }
  };
} // namespace fx1

#endif // FX1_PERF_COUNTERS_H