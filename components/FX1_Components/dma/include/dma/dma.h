#pragma once
#include <array>
#include <deque>
#include <vector>
#include <systemc>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include "dma/axi_extension.h"

namespace fx1::dma {
struct Config {
    sc_core::sc_time cycle = sc_core::sc_time(1, sc_core::SC_NS);
    // Thresholds are integration parameters, not invented MMIO registers.
    sc_core::sc_time transaction_timeout = sc_core::SC_ZERO_TIME;
    sc_core::sc_time watchdog_timeout = sc_core::SC_ZERO_TIME;
    unsigned memory_read_cache = 0, memory_write_cache = 0;
    unsigned peripheral_read_cache = 0, peripheral_write_cache = 0;
    unsigned memory_read_prot = 0, memory_write_prot = 0;
    std::uint32_t design_capability = 1;
};

class Dma : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<Dma> target_socket{"target_socket"};
    tlm_utils::simple_initiator_socket<Dma> master_socket{"master_socket"};
    sc_core::sc_in<bool> reset_n{"reset_n"};
    sc_core::sc_in<std::uint32_t> rx_request{"rx_request"}, tx_request{"tx_request"};
    sc_core::sc_out<std::uint32_t> rx_clear{"rx_clear"}, tx_clear{"tx_clear"};
    sc_core::sc_out<bool> irq{"irq"};

    SC_HAS_PROCESS(Dma);
    explicit Dma(sc_core::sc_module_name name, const Config& config = Config{});

    // Times the scheduler thread resumed. While nothing can progress it sleeps
    // until a register write, a request-input change, a finished bus
    // transaction or the next time deadline, so this stays small when a
    // channel waits for a peripheral (instrumentation for tests).
    std::uint64_t scheduler_wakeups() const noexcept { return wakeups_; }

private:
    struct Channel {
        std::uint32_t source = 0, destination = 0, size = 0, control = 0;
        std::uint32_t read_config = reg::CONFIG_RESET, write_config = reg::CONFIG_RESET;
        std::uint32_t mode = 0, peripheral = 0, attributes = 0;
        std::uint32_t raw = 0, interrupt_enable = reg::INTERRUPT_MASK, restriction = 0;
        bool enabled = true, active = false, fault = false;
        bool need_command = false, fetching_command = false;
        unsigned read_remaining = 0, write_remaining = 0;
        unsigned read_pending = 0, write_pending = 0, reserved = 0;
        unsigned completed = 0, completion_events = 0;
        std::deque<unsigned char> fifo;
        std::vector<unsigned char> swap_pending;
        sc_core::sc_time progress = sc_core::SC_ZERO_TIME;
        sc_core::sc_time read_ready = sc_core::SC_ZERO_TIME, write_ready = sc_core::SC_ZERO_TIME;
    };
    struct Task {
        bool busy = false, done = false, descriptor = false, timed_out = false;
        bool increment = true, external_credit = false;
        unsigned channel = 0, epoch = 0, unit = 4, peripheral = 0;
        std::uint32_t address = 0;
        std::vector<unsigned char> data;
        tlm::tlm_response_status response = tlm::TLM_INCOMPLETE_RESPONSE;
        sc_core::sc_time issued = sc_core::SC_ZERO_TIME;
        unsigned cache = 0, prot = 0;
    };
    struct Arbiter { unsigned next = 0, current = reg::CHANNEL_COUNT, tokens = 0; };
    struct Plan { unsigned bytes = 0, unit = 1; };

    Config config_;
    std::array<Channel, reg::CHANNEL_COUNT> channels_;
    std::array<Task, 4> reads_, writes_;
    std::array<sc_core::sc_event, 4> read_jobs_, write_jobs_;
    std::deque<unsigned> read_order_, write_order_;
    Arbiter read_arbiter_, write_arbiter_;
    unsigned epoch_ = 0;
    std::uint32_t priority_ = 0, software_rx_ = 0, software_tx_ = 0;
    std::uint32_t consumed_rx_ = 0, consumed_tx_ = 0, owned_rx_ = 0, owned_tx_ = 0;
    std::uint32_t clear_rx_ = 0, clear_tx_ = 0;
    std::array<sc_core::sc_time, 32> rx_clear_until_, tx_clear_until_;
    sc_core::sc_event kick_, outputs_;
    std::uint64_t activity_ = 0;  // bumped by every state change the scheduler causes
    std::uint64_t wakeups_ = 0;

    void b_transport(tlm::tlm_generic_payload&, sc_core::sc_time&);
    unsigned transport_dbg(tlm::tlm_generic_payload&);
    bool register_access(std::uint64_t, bool write, std::uint32_t&);
    bool channel_access(unsigned, std::uint32_t, bool, std::uint32_t&);
    void reset();
    void inputs_changed();
    void drive_outputs();
    void scheduler();
    void worker(bool read, unsigned slot);
    void read0(); void read1(); void read2(); void read3();
    void write0(); void write1(); void write2(); void write3();
    void retire(bool read);
    bool issue(bool read);
    bool eligible(unsigned, bool read) const;
    Plan plan(unsigned, bool read) const;
    unsigned select(bool read);
    unsigned peripheral(const Channel&, bool read) const;
    unsigned pending_limit(const Channel&, bool read) const;
    void start(unsigned);
    void begin_command(unsigned);
    void finish_command(unsigned);
    void fail(unsigned, std::uint32_t);
    void release_peripheral(const Task&, bool read);
    std::uint32_t core_status() const;
    std::uint32_t capabilities() const;
    bool idle() const;
    // Earliest future time at which a time-based condition can change; false if none.
    bool next_deadline(sc_core::sc_time now, sc_core::sc_time& at) const;
};
} // namespace fx1::dma
