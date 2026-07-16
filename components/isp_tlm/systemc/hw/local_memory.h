/**
 * @file local_memory.h
 * @brief Local memory model for ISP pipeline blocks
 *
 * Models line buffers and scratchpad memories with:
 *   - Configurable banks for ping-pong operation
 *   - Read/write port modeling
 *   - Memory latency
 *   - Bank conflict detection
 */

#ifndef LOCAL_MEMORY_H
#define LOCAL_MEMORY_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <vector>
#include <string>
#include <array>

// ============================================================================
// Memory Access Types
// ============================================================================
enum class mem_access_type {
    READ,
    WRITE,
    READ_WRITE  // For dual-port memories
};

struct mem_access {
    mem_access_type type;
    std::uint32_t addr;
    std::uint64_t data;        // For writes
    std::uint64_t read_data;   // For reads (filled by memory)
    std::uint32_t bank;
    std::uint32_t cycle;        // When access was initiated
    bool pending = false;
};

// ============================================================================
// Memory Configuration
// ============================================================================
struct local_mem_config {
    std::string name = "unnamed_mem";
    std::uint32_t depth = 1024;           // Number of entries
    std::uint32_t width_bits = 16;        // Bits per entry
    std::uint32_t num_banks = 1;          // Banking factor
    std::uint32_t read_ports = 1;         // Concurrent read ports
    std::uint32_t write_ports = 1;        // Concurrent write ports
    std::uint32_t latency_cycles = 1;     // Memory access latency
    bool collision_modeling = true;        // Model bank conflicts
};

// ============================================================================
// Local Memory Model
// ============================================================================
class local_memory : public sc_module {
public:
    SC_HAS_PROCESS(local_memory);

    explicit local_memory(sc_module_name name, const local_mem_config& cfg)
        : sc_module(name)
        , m_cfg(cfg) {
        // Allocate storage
        m_storage.resize(cfg.depth * cfg.num_banks, 0);

        SC_THREAD(mem_process);
    }

    // Blocking read (waits for latency)
    std::uint64_t read(std::uint32_t addr, std::uint32_t bank = 0) {
        wait(m_cfg.latency_cycles, SC_NS);
        return read_no_wait(addr, bank);
    }

    // Non-blocking read (for pipelined access)
    void read_async(std::uint32_t addr, std::uint32_t bank = 0) {
        std::uint64_t data = read_no_wait(addr, bank);
        m_pending_read.data = data;
        m_pending_read.pending = true;
    }

    // Blocking write
    void write(std::uint32_t addr, std::uint64_t data, std::uint32_t bank = 0) {
        std::size_t idx = bank * m_cfg.depth + addr;
        if (idx < m_storage.size()) {
            m_storage[idx] = data;
        }
        wait(m_cfg.latency_cycles, SC_NS);
    }

    // Non-blocking write
    void write_no_wait(std::uint32_t addr, std::uint64_t data, std::uint32_t bank = 0) {
        std::size_t idx = bank * m_cfg.depth + addr;
        if (idx < m_storage.size()) {
            m_storage[idx] = data;
        }
    }

    // Check if read is ready
    bool is_read_ready() const { return m_pending_read.pending; }

    // Get pending read data
    std::uint64_t get_pending_read() {
        m_pending_read.pending = false;
        return m_pending_read.data;
    }

    // Configuration access
    const local_mem_config& config() const { return m_cfg; }

    // Metrics
    std::uint64_t read_count() const { return m_read_count; }
    std::uint64_t write_count() const { return m_write_count; }
    std::uint64_t bank_conflicts() const { return m_bank_conflicts; }

    void reset_metrics() {
        m_read_count = 0;
        m_write_count = 0;
        m_bank_conflicts = 0;
        m_storage.assign(m_storage.size(), 0);
    }

private:
    local_mem_config m_cfg;
    std::vector<std::uint64_t> m_storage;

    // Metrics
    std::uint64_t m_read_count = 0;
    std::uint64_t m_write_count = 0;
    std::uint64_t m_bank_conflicts = 0;

    // Pending read for async operations
    struct { std::uint64_t data; bool pending; } m_pending_read = {0, false};

    // Active accesses for port modeling
    std::vector<std::uint32_t> m_active_read_ports;
    std::vector<std::uint32_t> m_active_write_ports;

    std::uint64_t read_no_wait(std::uint32_t addr, std::uint32_t bank) {
        ++m_read_count;
        std::size_t idx = bank * m_cfg.depth + addr;
        if (idx < m_storage.size()) {
            return m_storage[idx];
        }
        return 0;
    }

    void mem_process() {
        while (true) {
            // Memory model would handle complex arbitration here
            wait();
        }
    }
};

// ============================================================================
// Line Buffer Model
// ============================================================================
template <typename T, std::size_t MaxCols = 4096>
class line_buffer : public sc_module {
public:
    static constexpr std::size_t MAX_COLS = MaxCols;

    sc_in<bool> clk;
    sc_in<bool> rst_n;

    SC_HAS_PROCESS(line_buffer);

    line_buffer(sc_module_name name,
                std::size_t num_rows = 3,
                std::size_t width = 1920)
        : sc_module(name)
        , m_num_rows(num_rows)
        , m_width(width)
        , m_row_idx(0)
        , m_col_idx(0) {
        // Allocate line storage (num_rows x width)
        m_lines.resize(num_rows * width);

        SC_METHOD(update_indices);
        sensitive << clk.pos();
        dont_initialize();
    }

    // Write pixel to current position
    void write_pixel(T pixel) {
        std::size_t idx = m_row_idx * m_width + m_col_idx;
        if (idx < m_lines.size()) {
            m_lines[idx] = pixel;
        }
    }

    // Read pixel at (row, col) - for window access
    T read_pixel(std::size_t row, std::size_t col) const {
        if (row >= m_num_rows || col >= m_width) {
            return T{};
        }
        std::size_t idx = row * m_width + col;
        return m_lines[idx];
    }

    // Get current row for writing
    std::size_t current_row() const { return m_row_idx; }
    std::size_t current_col() const { return m_col_idx; }

    // Get window (for convolution kernels)
    void get_window(std::size_t center_row, std::size_t center_col,
                    std::array<T, 9>& window) const {
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 3; ++c) {
                std::size_t row = (center_row + r + m_num_rows) % m_num_rows;
                std::size_t col = (center_col + c + m_width) % m_width;
                window[r * 3 + c] = read_pixel(row, col);
            }
        }
    }

    void reset() {
        m_row_idx = 0;
        m_col_idx = 0;
    }

    // Metrics
    std::size_t num_rows() const { return m_num_rows; }
    std::size_t width() const { return m_width; }

private:
    std::size_t m_num_rows;
    std::size_t m_width;
    std::vector<T> m_lines;
    std::size_t m_row_idx;
    std::size_t m_col_idx;

    void update_indices() {
        if (rst_n.read() == false) {
            m_row_idx = 0;
            m_col_idx = 0;
            return;
        }

        ++m_col_idx;
        if (m_col_idx >= m_width) {
            m_col_idx = 0;
            m_row_idx = (m_row_idx + 1) % m_num_rows;
        }
    }
};

#endif  // LOCAL_MEMORY_H
