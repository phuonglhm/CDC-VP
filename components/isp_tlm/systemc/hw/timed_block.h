/**
 * @file timed_block.h
 * @brief Hardware shell base class for timing-aware ISP blocks
 *
 * Provides a base class that separates:
 *   - Functional kernel (pure algorithm)
 *   - Hardware shell (timing, buffering, control)
 *
 * Each hardware-aware block inherits from this and implements:
 *   - Functional processing (unchanged from existing code)
 *   - Hardware parameters (II, latency, ports)
 *   - Timing behavior (wait cycles)
 */

#ifndef TIMED_BLOCK_H
#define TIMED_BLOCK_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <string>
#include <queue>

#include "isp_arch_config.h"
#include "stream_beat.h"
#include "metrics.h"

// ============================================================================
// Cycle Counter Types
// ============================================================================
enum class cycle_type_t : std::uint8_t {
    ACTIVE = 0,       // Processing data
    IDLE = 1,         // Waiting for enable
    STARVED = 2,      // Waiting for input data
    BLOCKED = 3,      // Waiting to write output
    MEMORY_WAIT = 4,  // Waiting for memory
    BYPASS = 5,       // Bypassing processing
    STALL = 6         // Any stall condition
};

// ============================================================================
// Timed Block Base Class
// ============================================================================
class timed_block_base {
public:
    virtual ~timed_block_base() = default;

    // Hardware configuration
    virtual void set_arch_config(const block_arch_config& cfg) = 0;
    virtual block_arch_config get_arch_config() const = 0;

    // Control
    virtual void enable() = 0;
    virtual void disable() = 0;
    virtual void reset() = 0;

    // Metrics
    virtual const block_perf_metrics& metrics() const = 0;
    virtual void reset_metrics() = 0;

    // Name
    virtual std::string name() const = 0;
};

// ============================================================================
// Timed Block Shell (Template)
// ============================================================================
template <typename T>
class timed_block : public timed_block_base {
public:
    struct block_state {
        cycle_type_t current_cycle_type = cycle_type_t::IDLE;
        std::uint64_t cycle_count = 0;
        std::uint64_t input_count = 0;
        std::uint64_t output_count = 0;
        bool processing = false;
        std::uint32_t stall_remaining = 0;  // Cycles remaining in stall
    };

    timed_block(const std::string& name, const block_arch_config& arch_cfg = block_arch_config{})
        : m_name(name)
        , m_arch_cfg(arch_cfg)
        , m_enabled(true)
        , m_reset(false) {
        reset_metrics();
    }

    virtual ~timed_block() = default;

    // timed_block_base interface
    void set_arch_config(const block_arch_config& cfg) override {
        m_arch_cfg = cfg;
    }

    block_arch_config get_arch_config() const override {
        return m_arch_cfg;
    }

    void enable() override { m_enabled = true; }
    void disable() override { m_enabled = false; }

    void reset() override {
        m_state = block_state{};
        m_reset = true;
    }

    const block_perf_metrics& metrics() const override { return m_metrics; }

    void reset_metrics() override {
        m_metrics = block_perf_metrics{};
        m_metrics.block_name = m_name;
        m_state = block_state{};
    }

    std::string name() const override { return m_name; }

    // Cycle accounting
    void count_cycle(cycle_type_t type) {
        m_state.current_cycle_type = type;
        ++m_state.cycle_count;

        switch (type) {
            case cycle_type_t::ACTIVE:
                ++m_metrics.cycles.active_cycles;
                break;
            case cycle_type_t::IDLE:
                ++m_metrics.cycles.idle_cycles;
                break;
            case cycle_type_t::STARVED:
                ++m_metrics.cycles.starved_cycles;
                break;
            case cycle_type_t::BLOCKED:
                ++m_metrics.cycles.blocked_cycles;
                break;
            case cycle_type_t::MEMORY_WAIT:
                ++m_metrics.cycles.memory_wait_cycles;
                break;
            case cycle_type_t::BYPASS:
                ++m_metrics.cycles.bypass_cycles;
                break;
            case cycle_type_t::STALL:
                ++m_metrics.cycles.stall_cycles;
                break;
        }
    }

    // Initiation interval handling
    bool can_output() const {
        if (m_state.cycle_count == 0) return true;
        return (m_state.cycle_count % m_arch_cfg.initiation_interval == 0);
    }

    void wait_pipeline_latency() {
        for (std::uint32_t i = 0; i < m_arch_cfg.pipeline_latency; ++i) {
            count_cycle(cycle_type_t::ACTIVE);
        }
    }

    // Input/output accounting
    void record_input() { ++m_state.input_count; ++m_metrics.input_beats; }
    void record_output() { ++m_state.output_count; ++m_metrics.output_beats; }

    // Utility
    bool is_enabled() const { return m_enabled; }
    bool is_reset() const { return m_reset; }
    void clear_reset() { m_reset = false; }
    block_state state() const { return m_state; }

protected:
    std::string m_name;
    block_arch_config m_arch_cfg;
    block_perf_metrics m_metrics;
    block_state m_state;
    bool m_enabled;
    bool m_reset;
};

// ============================================================================
// Hardware Shell Wrapper
// ============================================================================
template <typename T>
class hw_shell : public sc_module {
public:
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> rst_n;
    sc_in<bool> enable;

    // Data ports
    sc_port<sc_fifo_in_if<T>> data_in;
    sc_port<sc_fifo_out_if<T>> data_out;

    // Bypass control
    sc_in<bool> bypass;

    SC_HAS_PROCESS(hw_shell);

    hw_shell(sc_module_name name,
             const block_arch_config& arch_cfg = block_arch_config{},
             std::uint32_t pipeline_latency = 1,
             std::uint32_t initiation_interval = 1)
        : sc_module(name)
        , m_arch_cfg(arch_cfg)
        , m_pipeline_latency(pipeline_latency)
        , m_initiation_interval(initiation_interval)
        , m_cycle_count(0)
        , m_processing(false)
        , m_output_ready(true) {
        SC_THREAD(shell_process);
        sensitive << clk.pos();

        // Initialize output ready
        sc_assert(pipeline_latency > 0);
    }

    void set_arch_config(const block_arch_config& cfg) {
        m_arch_cfg = cfg;
    }

    const block_arch_config& arch_config() const { return m_arch_cfg; }

    // Metrics accessors
    std::uint64_t active_cycles() const { return m_active_cycles; }
    std::uint64_t starved_cycles() const { return m_starved_cycles; }
    std::uint64_t blocked_cycles() const { return m_blocked_cycles; }
    std::uint64_t total_cycles() const { return m_cycle_count; }
    std::uint64_t input_count() const { return m_input_count; }
    std::uint64_t output_count() const { return m_output_count; }

    double utilization() const {
        return (m_cycle_count > 0) ?
            static_cast<double>(m_active_cycles) / m_cycle_count : 0.0;
    }

    void reset_counters() {
        m_cycle_count = 0;
        m_active_cycles = 0;
        m_starved_cycles = 0;
        m_blocked_cycles = 0;
        m_input_count = 0;
        m_output_count = 0;
    }

private:
    block_arch_config m_arch_cfg;
    std::uint32_t m_pipeline_latency;
    std::uint32_t m_initiation_interval;

    // Counters
    std::uint64_t m_cycle_count;
    std::uint64_t m_active_cycles;
    std::uint64_t m_starved_cycles;
    std::uint64_t m_blocked_cycles;
    std::uint64_t m_input_count;
    std::uint64_t m_output_count;

    // State
    bool m_processing;
    bool m_output_ready;
    std::queue<T> m_pipeline_delay;  // For modeling pipeline latency

    void shell_process() {
        reset_counters();

        while (true) {
            wait();

            if (rst_n.read() == false) {
                reset_counters();
                while (!m_pipeline_delay.empty()) m_pipeline_delay.pop();
                m_processing = false;
                m_output_ready = true;
                continue;
            }

            ++m_cycle_count;

            if (!enable.read()) {
                ++m_active_cycles;  // Clock gating - still "active" in HW sense
                continue;
            }

            // Check input availability
            if (data_in->num_available() > 0) {
                // Input available
                if (bypass.read()) {
                    // Bypass mode - pass through
                    T data = data_in->read();
                    data_out->write(data);
                    ++m_input_count;
                    ++m_output_count;
                    ++m_active_cycles;
                } else {
                    // Normal processing mode
                    if (!m_processing) {
                        // Start new input
                        data_in->read();  // Consume input
                        ++m_input_count;
                        m_processing = true;
                        m_output_ready = false;

                        // Add to pipeline delay queue
                        if (m_pipeline_delay.empty()) {
                            m_pipeline_delay.push(T{});
                        }

                        // Fill pipeline
                        while (m_pipeline_delay.size() < m_pipeline_latency) {
                            m_pipeline_delay.push(T{});
                        }
                    }

                    // Check if we can output (II constraint)
                    if (m_output_ready && !m_pipeline_delay.empty()) {
                        T result = m_pipeline_delay.front();
                        m_pipeline_delay.pop();
                        data_out->write(result);
                        ++m_output_count;
                        m_output_ready = false;
                    }

                    ++m_active_cycles;
                }
            } else {
                // Input starved
                ++m_starved_cycles;
            }

            // Check output backpressure
            if (data_out->num_free() == 0) {
                ++m_blocked_cycles;
            }

            // Advance pipeline
            if (m_processing && m_output_ready && !m_pipeline_delay.empty()) {
                T result = m_pipeline_delay.front();
                m_pipeline_delay.pop();
                data_out->write(result);
                ++m_output_count;
                m_processing = false;
            }

            // Check II constraint for next output
            if (m_cycle_count % m_initiation_interval == 0) {
                m_output_ready = true;
            }
        }
    }
};

#endif  // TIMED_BLOCK_H
