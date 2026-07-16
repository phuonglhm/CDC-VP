/**
 * @file trace.h
 * @brief VCD tracing utilities for ISP architecture analysis
 *
 * Provides:
 *   - Clock and reset tracing
 *   - Frame marker tracing
 *   - Block state tracing
 *   - Stream occupancy tracing
 *   - Transfer activity tracing
 */

#ifndef ISP_TRACE_H
#define ISP_TRACE_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

// ============================================================================
// Trace Signal Types
// ============================================================================
enum class trace_signal_type {
    BIT,
    INTEGER,
    REAL,
    STRING
};

// ============================================================================
// Trace Signal
// ============================================================================
struct trace_signal {
    std::string name;
    std::string hierarchy;
    trace_signal_type type;
    std::uint32_t width;  // For BIT type

    std::string full_name() const {
        return hierarchy.empty() ? name : hierarchy + "." + name;
    }
};

// ============================================================================
// Trace Manager
// ============================================================================
class isp_trace_manager {
public:
    static isp_trace_manager& instance() {
        static isp_trace_manager inst;
        return inst;
    }

    // Disable copy
    isp_trace_manager(const isp_trace_manager&) = delete;
    isp_trace_manager& operator=(const isp_trace_manager&) = delete;

    // Enable/disable tracing
    void enable(const std::string& filename = "isp_trace.vcd") {
        m_enabled = true;
        m_filename = filename;
        m_vcd_file = sc_trace_file::create_waveform_file(filename.c_str());
    }

    void disable() {
        m_enabled = false;
        if (m_vcd_file) {
            sc_close_vcd_trace_file(m_vcd_file);
            m_vcd_file = nullptr;
        }
    }

    bool is_enabled() const { return m_enabled; }

    // Register signals
    void register_clock(const std::string& name) {
        if (!m_enabled) return;
        m_clocks.push_back(name);
    }

    void register_reset(const std::string& name) {
        if (!m_enabled) return;
        m_resets.push_back(name);
    }

    void register_block(const std::string& name, const std::string& hierarchy = "") {
        if (!m_enabled) return;
        trace_signal sig{name, hierarchy, trace_signal_type::INTEGER, 0};
        m_blocks.push_back(sig);
    }

    void register_stream(const std::string& name, std::uint32_t max_depth,
                        const std::string& hierarchy = "") {
        if (!m_enabled) return;
        trace_signal sig{name, hierarchy, trace_signal_type::INTEGER, 0};
        m_streams[name] = sig;
        m_stream_depths[name] = max_depth;
    }

    // Update stream occupancy
    void update_stream_occupancy(const std::string& name, std::uint32_t occupancy) {
        if (!m_enabled || m_stream_traces.find(name) == m_stream_traces.end()) return;
        // Would trace here
    }

    // Frame markers
    void trace_frame_start(std::uint64_t frame_id) {
        if (!m_enabled) return;
        m_last_frame_start = frame_id;
        // Would emit VCD event
    }

    void trace_frame_end(std::uint64_t frame_id, std::uint64_t frame_cycles) {
        if (!m_enabled) return;
        m_last_frame_end = frame_id;
        // Would emit VCD event
    }

    // Block state
    void trace_block_state(const std::string& block,
                          std::uint8_t state,
                          const std::string& state_name) {
        if (!m_enabled) return;
        m_block_states[block] = state_name;
    }

    // Transfer activity
    void trace_transfer(const std::string& stream,
                       std::uint64_t bytes,
                       bool is_read) {
        if (!m_enabled) return;
        if (is_read) {
            m_total_read_bytes += bytes;
        } else {
            m_total_write_bytes += bytes;
        }
    }

    // Statistics
    std::uint64_t total_read_bytes() const { return m_total_read_bytes; }
    std::uint64_t total_write_bytes() const { return m_total_write_bytes; }
    std::uint64_t last_frame_id() const { return m_last_frame_start; }

    // Dump trace summary
    void dump_summary(const std::string& filename) const;

private:
    isp_trace_manager()
        : m_enabled(false)
        , m_vcd_file(nullptr)
        , m_total_read_bytes(0)
        , m_total_write_bytes(0)
        , m_last_frame_start(0)
        , m_last_frame_end(0) {}

    bool m_enabled;
    std::string m_filename;
    sc_vcd_trace_file* m_vcd_file;

    std::vector<std::string> m_clocks;
    std::vector<std::string> m_resets;
    std::vector<trace_signal> m_blocks;
    std::unordered_map<std::string, trace_signal> m_streams;
    std::unordered_map<std::string, std::uint32_t> m_stream_depths;
    std::unordered_map<std::string, std::string> m_stream_traces;
    std::unordered_map<std::string, std::string> m_block_states;

    std::uint64_t m_total_read_bytes;
    std::uint64_t m_total_write_bytes;
    std::uint64_t m_last_frame_start;
    std::uint64_t m_last_frame_end;
};

// ============================================================================
// Convenience Macros
// ============================================================================
#define ISP_TRACE() isp_trace_manager::instance()

// ============================================================================
// Trace Scope (RAII)
// ============================================================================
class trace_scope {
public:
    trace_scope(const std::string& hierarchy) {
        ISP_TRACE().register_block(hierarchy, "");
    }
    ~trace_scope() = default;
};

#endif  // ISP_TRACE_H
