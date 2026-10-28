#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <vector>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "fu3/dma/transfer_extension.h"

// Concurrent blocking-TLM contexts; wrapper adapters and pin-level AXI are separate.
namespace fu3::dma {
struct EngineConfig {
    // Integration times, not invented MMIO registers. Zero disables a timer.
    sc_core::sc_time transaction_timeout = sc_core::SC_ZERO_TIME;
    sc_core::sc_time watchdog_timeout = sc_core::SC_ZERO_TIME;
    sc_core::sc_time scheduler_quantum = sc_core::SC_ZERO_TIME;
    std::array<sc_core::sc_time, 5> phase_timeouts{};
    unsigned memory_read_cache = 0, memory_write_cache = 0;
    unsigned peripheral_read_cache = 0, peripheral_write_cache = 0;
    unsigned memory_read_prot = 0, memory_write_prot = 0;
    unsigned peripheral_read_prot = 0, peripheral_write_prot = 0;
    std::uint32_t design_capability = 1; // One IRQ destination; upper bits integration-defined.
    bool command_chain = true; // User-approved 4-word little-endian layout.
};
class ApDma : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<ApDma> target_socket{"target_socket"};
    tlm_utils::simple_initiator_socket<ApDma> master_socket{"master_socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_out<bool> irq{"irq"};
    sc_core::sc_in<sc_dt::sc_uint<32>> rx_request{"rx_request"};
    sc_core::sc_in<sc_dt::sc_uint<32>> tx_request{"tx_request"};
    sc_core::sc_out<sc_dt::sc_uint<32>> rx_clear{"rx_clear"};
    sc_core::sc_out<sc_dt::sc_uint<32>> tx_clear{"tx_clear"};

    SC_HAS_PROCESS(ApDma);
    explicit ApDma(sc_core::sc_module_name name,
                   sc_core::sc_time peripheral_cycle = sc_core::SC_ZERO_TIME,
                   const EngineConfig& config = EngineConfig{});
    std::uint64_t scheduler_wakeups() const noexcept { return scheduler_wakeups_; }
    // Peripheral clock from the clock controller (platform CRM); zero is ignored.
    void set_peripheral_cycle(sc_core::sc_time cycle) { if (cycle > sc_core::SC_ZERO_TIME) peripheral_cycle_ = cycle; }
    sc_core::sc_time peripheral_cycle() const { return peripheral_cycle_; }
    // Clock gate from the clock controller (platform CRM): while disabled the
    // scheduler neither dispatches nor retires and no new bus transfer starts
    // (one already on the bus completes); state is kept. Enabled by default.
    void set_clock_enabled(bool on) {
        if (on == clock_enabled_) return;
        clock_enabled_ = on;
        clock_gate_changed_.notify(sc_core::SC_ZERO_TIME);
    }
    bool clock_enabled() const { return clock_enabled_; }
private:
    bool clock_enabled_ = true;
    sc_core::sc_event clock_gate_changed_;
    struct Channel {
        std::array<std::uint32_t, 64> words{};
        std::array<unsigned char, 32> fifo{};
        unsigned used = 0, read_reserved = 0, write_reserved = 0;
        std::array<unsigned char, 4> swap_pending{};
        unsigned swap_count = 0;
        std::uint32_t source = 0, destination = 0;
        unsigned read_remaining = 0, write_remaining = 0;
        bool active = false, fault = false, need_command = false, fetching = false;
        unsigned read_pending = 0, write_pending = 0;
        sc_core::sc_time progress = sc_core::SC_ZERO_TIME;
    };
    struct Job {
        bool busy = false, done = false, descriptor = false, timed_out = false;
        bool increment = true;
        unsigned channel = 0, unit = 1, peripheral = 0, cache = 0, prot = 0;
        std::uint32_t error_bits = 0;
        TransferExtension* live_metadata = nullptr;
        std::uint64_t epoch = 0;
        std::uint32_t address = 0;
        std::vector<unsigned char> data;
        sc_core::sc_time issued = sc_core::SC_ZERO_TIME;
        tlm::tlm_response_status response = tlm::TLM_INCOMPLETE_RESPONSE;
    };
    struct Arbiter { unsigned next = 0, current = 8, tokens = 0; };
    EngineConfig config_;
    std::array<Channel, 8> channels_{};
    std::array<Job, 4> reads_{}, writes_{};
    std::array<sc_core::sc_event, 4> read_jobs_, write_jobs_;
    std::deque<unsigned> read_order_, write_order_;
    Arbiter read_arbiter_, write_arbiter_;
    std::uint32_t rx_owned_ = 0, tx_owned_ = 0;
    std::uint64_t scheduler_wakeups_ = 0;
    sc_core::sc_time next_dispatch_ = sc_core::SC_ZERO_TIME;
    std::uint32_t priority_ = 0;
    std::uint32_t software_rx_ = 0, software_tx_ = 0;
    sc_core::sc_event state_changed_, pulse_changed_;
    sc_core::sc_time peripheral_cycle_;
    std::uint32_t rx_blocked_ = 0, tx_blocked_ = 0;
    std::array<sc_core::sc_time, 32> rx_until_{}, tx_until_{};
    std::array<sc_core::sc_time, 32> rx_ready_{}, tx_ready_{};
    void requests_changed();
    bool request_ready(unsigned id, bool read) const;
    void acknowledge(unsigned channel, unsigned id, bool read);
    std::uint64_t reset_epoch_ = 0;
    unsigned pending_limit(unsigned channel, bool read) const;
    bool eligible(unsigned channel, bool read) const;
    unsigned select(bool read);
    bool issue(bool read);
    bool retire(bool read);
    void check_timeout(bool read, Job& job);
    void worker(bool read, unsigned slot);
    void read0(); void read1(); void read2(); void read3();
    void write0(); void write1(); void write2(); void write3();
    void refresh_status(unsigned channel);
    void fail(unsigned channel, std::uint32_t bits);
    void load_command(unsigned channel);
    sc_core::sc_time next_deadline() const;
    bool can_start(unsigned channel) const;
    void start(unsigned channel);
    void engine();

    static unsigned unit_bytes(std::uint32_t address, unsigned limit);
    static unsigned burst_bytes(std::uint32_t address, unsigned remaining,
                                unsigned capacity, std::uint32_t config);
    void finish(unsigned channel, std::uint32_t error = 0);
    void reset_registers();
    void update_outputs();
    std::uint32_t core_status() const;
    std::uint32_t capabilities() const;
    void update_restriction(unsigned channel);
    bool fifo_valid(unsigned channel);
    static std::uint32_t phase_interrupt(BusPhase phase);
    tlm::tlm_response_status access(bool write, std::uint32_t address,
                                    std::uint32_t& value);
    void b_transport(tlm::tlm_generic_payload& transaction, sc_core::sc_time& delay);
};
} // namespace fu3::dma
