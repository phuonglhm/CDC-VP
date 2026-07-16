/**
 * @file frame_dma.h
 * @brief Frame DMA model for input/output bandwidth simulation
 *
 * Models DMA transfers with:
 *   - Configurable bus width and burst length
 *   - Read/write latency
 *   - Outstanding transaction modeling
 *   - Bandwidth limiting
 */

#ifndef FRAME_DMA_H
#define FRAME_DMA_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <vector>
#include <string>
#include <queue>

// ============================================================================
// DMA Transaction
// ============================================================================
struct dma_transaction {
    enum type_t { READ, WRITE } type;
    std::uint32_t addr;
    std::uint64_t size;          // bytes
    std::uint64_t timestamp;     // cycle when initiated
    bool complete = false;
};

// ============================================================================
// DMA Statistics
// ============================================================================
struct dma_stats {
    std::uint64_t total_reads = 0;
    std::uint64_t total_writes = 0;
    std::uint64_t total_bytes_read = 0;
    std::uint64_t total_bytes_written = 0;
    std::uint64_t max_outstanding = 0;
    std::uint64_t total_cycles = 0;
    std::uint64_t busy_cycles = 0;
    std::uint64_t idle_cycles = 0;
    std::uint64_t stall_cycles = 0;

    double bandwidth_mbps(float clk_mhz) const {
        if (total_cycles == 0) return 0.0;
        double bytes_per_cycle = static_cast<double>(total_bytes_read + total_bytes_written) / total_cycles;
        double bytes_per_sec = bytes_per_cycle * clk_mhz * 1e6;
        return bytes_per_sec / 1e6;
    }

    double utilization() const {
        return (total_cycles > 0) ?
            static_cast<double>(busy_cycles) / total_cycles : 0.0;
    }
};

// ============================================================================
// Frame DMA Module
// ============================================================================
class frame_dma : public sc_module {
public:
    // Clock and reset
    sc_in<bool> clk;
    sc_in<bool> rst_n;

    // Control
    sc_in<bool> start;
    sc_in<bool> enable;

    // Address and size
    sc_in<std::uint32_t> base_addr;
    sc_in<std::uint32_t> frame_size;    // bytes
    sc_in<std::uint32_t> frame_stride;  // bytes (for multi-frame)

    // Data bus
    sc_out<bool> rd_valid;
    sc_out<bool> wr_ready;
    sc_in<std::uint64_t> rd_data;
    sc_out<std::uint64_t> wr_data;

    // Status
    sc_out<bool> busy;
    sc_out<bool> done;

    // Configuration
    struct dma_config {
        std::string name = "dma";
        std::uint32_t bus_width_bits = 64;
        std::uint32_t burst_length = 16;
        std::uint32_t read_latency = 4;
        std::uint32_t write_latency = 2;
        std::uint32_t max_outstanding = 4;
        float max_bandwidth_mbps = 0.0f;  // 0 = unlimited
    };

    SC_HAS_PROCESS(frame_dma);

    explicit frame_dma(sc_module_name name, const dma_config& cfg)
        : sc_module(name)
        , m_cfg(cfg)
        , m_cycles(0)
        , m_current_state(IDLE) {
        m_cfg.name = name;

        SC_THREAD(dma_thread);
        sensitive << clk.pos();

        // Initialize signals
        rd_valid.initialize(false);
        wr_ready.initialize(true);
        busy.initialize(false);
        done.initialize(false);
    }

    void set_config(const dma_config& cfg) { m_cfg = cfg; }

    // Accessors
    const dma_stats& stats() const { return m_stats; }
    dma_config config() const { return m_cfg; }

    void reset_stats() {
        m_stats = dma_stats{};
    }

    // Check if DMA is available
    bool available() const {
        return m_current_state == IDLE && !busy.read();
    }

private:
    enum state_t { IDLE, READ_HEADER, READ_DATA, WRITE_HEADER, WRITE_DATA, COMPLETE };

    dma_config m_cfg;
    dma_stats m_stats;
    std::uint64_t m_cycles;

    state_t m_current_state;
    std::queue<dma_transaction> m_pending_transactions;
    std::uint32_t m_outstanding_count;

    sc_event m_complete_event;

    void dma_thread() {
        while (true) {
            wait();

            ++m_cycles;
            m_stats.total_cycles++;

            // Update outstanding count
            if (m_outstanding_count > m_stats.max_outstanding) {
                m_stats.max_outstanding = m_outstanding_count;
            }

            // State machine
            switch (m_current_state) {
                case IDLE:
                    m_stats.idle_cycles++;
                    if (start.read()) {
                        m_current_state = READ_HEADER;
                        busy.write(true);
                    }
                    break;

                case READ_HEADER:
                    m_stats.busy_cycles++;
                    m_outstanding_count++;
                    m_stats.total_reads++;
                    m_stats.total_bytes_read += frame_size.read();
                    wait(m_cfg.read_latency, SC_NS);
                    m_outstanding_count--;
                    m_current_state = READ_DATA;
                    rd_valid.write(true);
                    break;

                case READ_DATA:
                    m_stats.busy_cycles++;
                    wait();
                    rd_valid.write(false);
                    if (enable.read()) {
                        m_current_state = COMPLETE;
                    }
                    break;

                case WRITE_HEADER:
                    m_stats.busy_cycles++;
                    m_outstanding_count++;
                    m_stats.total_writes++;
                    wait(m_cfg.write_latency, SC_NS);
                    m_outstanding_count--;
                    m_current_state = WRITE_DATA;
                    wr_ready.write(true);
                    break;

                case WRITE_DATA:
                    m_stats.busy_cycles++;
                    wait();
                    wr_ready.write(false);
                    m_stats.total_bytes_written += frame_size.read();
                    m_current_state = COMPLETE;
                    break;

                case COMPLETE:
                    done.write(true);
                    busy.write(false);
                    m_current_state = IDLE;
                    m_complete_event.notify();
                    break;
            }
        }
    }

public:
    // Wait for DMA completion
    void wait_complete() {
        if (m_current_state != IDLE) {
            wait(m_complete_event);
        }
    }
};

// ============================================================================
// Simple DMA Reader (for input streaming)
// ============================================================================
class dma_reader : public sc_module {
public:
    sc_in<bool> clk;
    sc_in<bool> rst_n;

    sc_port<sc_fifo_out_if<std::uint16_t>> data_out;

    sc_in<bool> enable;
    sc_out<bool> ready;

    SC_HAS_PROCESS(dma_reader);

    dma_reader(sc_module_name name,
               std::uint32_t width = 1920,
               std::uint32_t height = 1080,
               std::uint32_t bus_width = 64,
               std::uint32_t burst_len = 16)
        : sc_module(name)
        , m_width(width)
        , m_height(height)
        , m_bus_width(bus_width)
        , m_burst_len(burst_len)
        , m_pixels_per_burst(burst_len * bus_width / 16)
        , m_pixels_transferred(0)
        , m_cycles(0)
        , m_total_cycles(0)
        , m_busy_cycles(0) {
        SC_THREAD(dma_read_process);
        sensitive << clk.pos();
    }

    void set_frame_size(std::uint32_t w, std::uint32_t h) {
        m_width = w;
        m_height = h;
    }

    void reset() {
        m_pixels_transferred = 0;
        m_cycles = 0;
    }

    // Metrics
    std::uint64_t pixels_transferred() const { return m_pixels_transferred; }
    double bandwidth_mpixels_s(float clk_mhz) const {
        return (m_total_cycles > 0) ?
            static_cast<double>(m_pixels_transferred) / m_total_cycles * clk_mhz * 1e6 : 0.0;
    }
    double utilization() const {
        return (m_total_cycles > 0) ?
            static_cast<double>(m_busy_cycles) / m_total_cycles : 0.0;
    }

private:
    std::uint32_t m_width;
    std::uint32_t m_height;
    std::uint32_t m_bus_width;
    std::uint32_t m_burst_len;
    std::uint32_t m_pixels_per_burst;
    std::uint64_t m_pixels_transferred;
    std::uint64_t m_cycles;
    std::uint64_t m_total_cycles;
    std::uint64_t m_busy_cycles;

    void dma_read_process() {
        while (true) {
            wait();

            if (rst_n.read() == false) {
                m_pixels_transferred = 0;
                m_cycles = 0;
                ready.write(false);
                continue;
            }

            ++m_total_cycles;

            if (enable.read()) {
                ++m_busy_cycles;
                ready.write(true);

                // Simulate burst read
                for (std::uint32_t i = 0; i < m_pixels_per_burst && m_pixels_transferred < m_width * m_height; ++i) {
                    // In real HW, data comes from external memory
                    // Here we just track the transfer
                    ++m_pixels_transferred;
                }
            } else {
                ready.write(false);
            }
        }
    }
};

#endif  // FRAME_DMA_H
