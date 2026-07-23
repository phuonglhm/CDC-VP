/*
 * ISP Metrics Base Class
 * Provides common metrics collection interface for all ISP blocks
 */

#ifndef ISP_METRICS_BASE_H
#define ISP_METRICS_BASE_H

#include <systemc>
#include <cstdint>
#include <string>
#include <map>
#include <iomanip>
#include <sstream>

//=============================================================================
// Technology Parameters (configurable per target)
//=============================================================================
namespace tech_params {
    // Default: Generic 28nm FPGA/ASIC
    constexpr double VDD = 0.85;           // Voltage (V)
    constexpr double FREQ_MHZ = 200.0;     // Clock frequency (MHz)

    // Capacitance estimates (fF per bit)
    constexpr double LUT_CAP_FF = 0.5;
    constexpr double FF_CAP_FF = 0.3;
    constexpr double MUX_CAP_FF = 0.2;
    constexpr double BRAM_CAP_PF = 2.5;    // BRAM per bit (pF)

    // Power estimates per operation (mW/MHz)
    constexpr double DSP_POWER_PER_MULT = 25.0;
    constexpr double BRAM_POWER_PER_ACCESS = 0.1;
    constexpr double LUT_POWER_PER_TOGGLE = 0.001;

    // Leakage (µA per gate)
    constexpr double LEAKAGE_PER_LUT_UA = 10.0;
    constexpr double LEAKAGE_PER_FF_UA = 5.0;
    constexpr double LEAKAGE_PER_BRAM_KBIT_UA = 100.0;
}

//=============================================================================
// Block Metrics Data Structure
//=============================================================================
struct BlockMetrics {
    // Configuration
    std::string block_name;
    unsigned width = 0;
    unsigned height = 0;

    // Timing metrics
    uint64_t total_cycles = 0;
    uint64_t active_cycles = 0;
    uint64_t stall_cycles = 0;
    uint64_t idle_cycles = 0;
    uint64_t pixel_count = 0;
    uint64_t line_count = 0;
    uint64_t frame_count = 0;

    // Operation counts
    uint64_t add_ops = 0;
    uint64_t sub_ops = 0;
    uint64_t mul_ops = 0;
    uint64_t cmp_ops = 0;
    uint64_t mem_reads = 0;
    uint64_t mem_writes = 0;
    uint64_t lut_accesses = 0;
    uint64_t shifts = 0;

    // Resource estimates (RTL synthesis hints)
    unsigned lut4_estimate = 0;     // 4-input LUT count
    unsigned ff_count = 0;           // Flip-flop count
    unsigned bram_kbits = 0;        // BRAM in Kbits
    unsigned dsp_mults = 0;         // DSP multiplier count
    unsigned fifo_depth = 0;         // Max FIFO depth

    // Computed metrics
    double utilization_pct() const {
        if (total_cycles == 0) return 0.0;
        return 100.0 * active_cycles / total_cycles;
    }

    double throughput_fps() const {
        if (total_cycles == 0 || frame_count == 0) return 0.0;
        return (double)frame_count * tech_params::FREQ_MHZ * 1e6 / total_cycles;
    }

    double pixels_per_cycle() const {
        if (total_cycles == 0) return 0.0;
        return (double)pixel_count / total_cycles;
    }

    double ops_per_pixel() const {
        if (pixel_count == 0) return 0.0;
        return (double)(add_ops + sub_ops + mul_ops) / pixel_count;
    }

    // Power estimates
    double dynamic_power_mw() const {
        double cap = 0.0;
        cap += lut4_estimate * tech_params::LUT_CAP_FF * 0.001;      // fF -> pF
        cap += ff_count * tech_params::FF_CAP_FF * 0.001;
        cap += bram_kbits * 1024 * tech_params::BRAM_CAP_PF;          // Kbits -> bits

        // Activity factor estimate (based on utilization)
        double activity = utilization_pct() / 100.0;
        if (activity < 0.1) activity = 0.1;  // Minimum 10% activity

        double p = 0.5 * tech_params::VDD * tech_params::VDD *
                   tech_params::FREQ_MHZ * cap * activity;
        return p;  // mW
    }

    double static_power_mw() const {
        double leak = 0.0;
        leak += lut4_estimate * tech_params::LEAKAGE_PER_LUT_UA;
        leak += ff_count * tech_params::LEAKAGE_PER_FF_UA;
        leak += bram_kbits * tech_params::LEAKAGE_PER_BRAM_KBIT_UA;
        return leak * tech_params::VDD * 0.001;  // mW
    }

    double total_power_mw() const {
        return dynamic_power_mw() + static_power_mw();
    }

    double energy_per_frame_uj() const {
        if (frame_count == 0) return 0.0;
        double total_energy = total_power_mw() * 1e6;  // mW -> µW
        double time_per_frame = (double)total_cycles / (tech_params::FREQ_MHZ * 1e6);  // seconds
        return total_energy * time_per_frame;  // µJ
    }

    // Reset counters
    void reset() {
        total_cycles = 0;
        active_cycles = 0;
        stall_cycles = 0;
        idle_cycles = 0;
        pixel_count = 0;
        line_count = 0;
        frame_count = 0;
        add_ops = 0;
        sub_ops = 0;
        mul_ops = 0;
        cmp_ops = 0;
        mem_reads = 0;
        mem_writes = 0;
        lut_accesses = 0;
        shifts = 0;
    }
};

//=============================================================================
// Metrics Base Class
//=============================================================================
class IspMetricsBase {
public:
    IspMetricsBase(const std::string& name) : m_block_name(name) {}

    virtual ~IspMetricsBase() = default;

    // Get metrics reference
    const BlockMetrics& get_metrics() const { return m_metrics; }
    BlockMetrics& get_metrics() { return m_metrics; }

    // Record operations
    void record_pixel() { m_metrics.pixel_count++; }
    void record_line() { m_metrics.line_count++; }
    void record_frame() { m_metrics.frame_count++; }

    void record_add() { m_metrics.add_ops++; }
    void record_sub() { m_metrics.sub_ops++; }
    void record_mul() { m_metrics.mul_ops++; }
    void record_cmp() { m_metrics.cmp_ops++; }
    void record_mem_read() { m_metrics.mem_reads++; }
    void record_mem_write() { m_metrics.mem_writes++; }
    void record_lut_access() { m_metrics.lut_accesses++; }
    void record_shift() { m_metrics.shifts++; }

    // Record cycles
    void record_active_cycle() { m_metrics.active_cycles++; }
    void record_stall_cycle() { m_metrics.stall_cycles++; }
    void record_idle_cycle() { m_metrics.idle_cycles++; }
    void record_total_cycle() { m_metrics.total_cycles++; }

    // Set configuration
    void set_config(unsigned width, unsigned height) {
        m_metrics.width = width;
        m_metrics.height = height;
    }

    // Reset metrics
    virtual void reset() {
        m_metrics.reset();
        m_metrics.block_name = m_block_name;
    }

    // Print summary
    virtual void print_summary(std::ostream& os = std::cout) const {
        os << "\n" << std::string(60, '=') << "\n";
        os << "  METRICS SUMMARY: " << m_block_name << "\n";
        os << std::string(60, '=') << "\n";

        // Configuration
        os << std::setw(30) << std::left << "Image Size:"
           << m_metrics.width << " x " << m_metrics.height << "\n";

        // Timing
        os << "\n  [Timing]" << "\n";
        os << std::setw(30) << std::left << "  Total Cycles:"
           << m_metrics.total_cycles << "\n";
        os << std::setw(30) << std::left << "  Active Cycles:"
           << m_metrics.active_cycles << "\n";
        os << std::setw(30) << std::left << "  Stall Cycles:"
           << m_metrics.stall_cycles << "\n";
        os << std::setw(30) << std::left << "  Utilization:"
           << std::fixed << std::setprecision(2)
           << m_metrics.utilization_pct() << "%\n";
        os << std::setw(30) << std::left << "  Throughput:"
           << std::fixed << std::setprecision(2)
           << m_metrics.throughput_fps() << " fps\n";
        os << std::setw(30) << std::left << "  Pixels/Cycle:"
           << std::fixed << std::setprecision(4)
           << m_metrics.pixels_per_cycle() << "\n";

        // Operations
        os << "\n  [Operations]" << "\n";
        os << std::setw(30) << std::left << "  Total Pixels Processed:"
           << m_metrics.pixel_count << "\n";
        os << std::setw(30) << std::left << "  Add Operations:"
           << m_metrics.add_ops << "\n";
        os << std::setw(30) << std::left << "  Subtract Operations:"
           << m_metrics.sub_ops << "\n";
        os << std::setw(30) << std::left << "  Multiply Operations:"
           << m_metrics.mul_ops << "\n";
        os << std::setw(30) << std::left << "  Compare Operations:"
           << m_metrics.cmp_ops << "\n";
        os << std::setw(30) << std::left << "  Ops/Pixel:"
           << std::fixed << std::setprecision(2)
           << m_metrics.ops_per_pixel() << "\n";

        // Resources
        os << "\n  [Resource Estimates]" << "\n";
        os << std::setw(30) << std::left << "  LUT4 Estimate:"
           << m_metrics.lut4_estimate << "\n";
        os << std::setw(30) << std::left << "  Flip-Flops:"
           << m_metrics.ff_count << "\n";
        os << std::setw(30) << std::left << "  BRAM (Kbits):"
           << m_metrics.bram_kbits << "\n";
        os << std::setw(30) << std::left << "  DSP Multipliers:"
           << m_metrics.dsp_mults << "\n";

        // Power
        os << "\n  [Power Estimates]" << "\n";
        os << std::setw(30) << std::left << "  Dynamic Power:"
           << std::fixed << std::setprecision(3)
           << m_metrics.dynamic_power_mw() << " mW\n";
        os << std::setw(30) << std::left << "  Static Power:"
           << std::fixed << std::setprecision(3)
           << m_metrics.static_power_mw() << " mW\n";
        os << std::setw(30) << std::left << "  Total Power:"
           << std::fixed << std::setprecision(3)
           << m_metrics.total_power_mw() << " mW\n";
        os << std::setw(30) << std::left << "  Energy/Frame:"
           << std::fixed << std::setprecision(3)
           << m_metrics.energy_per_frame_uj() << " µJ\n";

        os << std::string(60, '=') << "\n";
    }

    // Export to JSON string
    virtual std::string to_json() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(6);

        oss << "{\n";
        oss << "  \"block_name\": \"" << m_block_name << "\",\n";
        oss << "  \"config\": {\n";
        oss << "    \"width\": " << m_metrics.width << ",\n";
        oss << "    \"height\": " << m_metrics.height << "\n";
        oss << "  },\n";

        oss << "  \"timing\": {\n";
        oss << "    \"total_cycles\": " << m_metrics.total_cycles << ",\n";
        oss << "    \"active_cycles\": " << m_metrics.active_cycles << ",\n";
        oss << "    \"stall_cycles\": " << m_metrics.stall_cycles << ",\n";
        oss << "    \"utilization_pct\": " << m_metrics.utilization_pct() << ",\n";
        oss << "    \"throughput_fps\": " << m_metrics.throughput_fps() << ",\n";
        oss << "    \"pixels_per_cycle\": " << m_metrics.pixels_per_cycle() << "\n";
        oss << "  },\n";

        oss << "  \"operations\": {\n";
        oss << "    \"pixel_count\": " << m_metrics.pixel_count << ",\n";
        oss << "    \"add_ops\": " << m_metrics.add_ops << ",\n";
        oss << "    \"sub_ops\": " << m_metrics.sub_ops << ",\n";
        oss << "    \"mul_ops\": " << m_metrics.mul_ops << ",\n";
        oss << "    \"cmp_ops\": " << m_metrics.cmp_ops << ",\n";
        oss << "    \"ops_per_pixel\": " << m_metrics.ops_per_pixel() << "\n";
        oss << "  },\n";

        oss << "  \"resources\": {\n";
        oss << "    \"lut4_estimate\": " << m_metrics.lut4_estimate << ",\n";
        oss << "    \"ff_count\": " << m_metrics.ff_count << ",\n";
        oss << "    \"bram_kbits\": " << m_metrics.bram_kbits << ",\n";
        oss << "    \"dsp_mults\": " << m_metrics.dsp_mults << "\n";
        oss << "  },\n";

        oss << "  \"power\": {\n";
        oss << "    \"dynamic_mw\": " << m_metrics.dynamic_power_mw() << ",\n";
        oss << "    \"static_mw\": " << m_metrics.static_power_mw() << ",\n";
        oss << "    \"total_mw\": " << m_metrics.total_power_mw() << ",\n";
        oss << "    \"energy_per_frame_uj\": " << m_metrics.energy_per_frame_uj() << "\n";
        oss << "  }\n";
        oss << "}";

        return oss.str();
    }

protected:
    std::string m_block_name;
    BlockMetrics m_metrics;
};

#endif // ISP_METRICS_BASE_H
