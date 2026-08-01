// SPDX-License-Identifier: Apache-2.0
//
// Step 10.5: concurrent b_transport calls through one upstream port.
//
// The frozen FlooNoC configuration keeps MaxUniqueIds = 1, so reads and writes
// each complete in FIFO order. That does not mean one transaction in flight:
// this test launches several SystemC processes through the same tagged socket,
// saturates a deliberately-small admission bound, and proves response/data/
// error ownership is retained without allocating another downstream AXI ID.

#include "floo_noc_model/noc_interconnect.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using cdc::components::noc_interconnect;

constexpr std::uint64_t memory_base = 0x8000'0000;
constexpr unsigned transfer_bytes = 8;
constexpr unsigned configured_capacity = 2;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct access_record {
    bool is_write = false;
    std::uint64_t address = 0;
};

class delayed_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<delayed_memory> socket;
    std::vector<access_record> records;

    explicit delayed_memory(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &delayed_memory::b_transport);
        for (unsigned index = 0; index < storage_.size(); ++index) {
            storage_[index] = static_cast<unsigned char>(0x40u + index);
        }
    }

    std::array<unsigned char, transfer_bytes> bytes_at(unsigned address) const
    {
        std::array<unsigned char, transfer_bytes> result{};
        std::copy_n(storage_.begin() + address, transfer_bytes, result.begin());
        return result;
    }

private:
    static constexpr std::uint64_t read_error_address = 0x00;
    static constexpr std::uint64_t write_error_address = 0x28;
    std::array<unsigned char, 0x100> storage_{};

    void b_transport(
        tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const auto address = trans.get_address();
        const auto length = trans.get_data_length();
        records.push_back({trans.is_write(), address});
        delay += sc_core::sc_time(5.5, sc_core::SC_NS);

        if ((trans.is_read() && address == read_error_address)
            || (trans.is_write() && address == write_error_address)) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (length != transfer_bytes || address > storage_.size()
            || length > storage_.size() - static_cast<std::size_t>(address)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (trans.is_write()) {
            std::copy_n(trans.get_data_ptr(), length,
                        storage_.begin() + static_cast<std::size_t>(address));
        } else {
            std::copy_n(storage_.begin() + static_cast<std::size_t>(address),
                        length, trans.get_data_ptr());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class concurrent_manager : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<concurrent_manager> socket;
    noc_interconnect* noc = nullptr;
    delayed_memory* memory = nullptr;
    bool finished = false;

    SC_HAS_PROCESS(concurrent_manager);

    explicit concurrent_manager(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        SC_THREAD(read_zero);
        SC_THREAD(read_one);
        SC_THREAD(read_two);
        SC_THREAD(write_zero);
        SC_THREAD(write_one);
        SC_THREAD(mixed_read);
        SC_THREAD(mixed_write);
        SC_THREAD(coordinator);
        SC_THREAD(watchdog);
    }

private:
    struct result {
        tlm::tlm_response_status status = tlm::TLM_INCOMPLETE_RESPONSE;
        std::array<unsigned char, transfer_bytes> bytes{};
        double elapsed_cycles = 0.0;
        std::uint64_t network_cycles = 0;
        bool done = false;
    };

    std::array<result, 3> reads_{};
    std::array<result, 2> writes_{};
    result mixed_read_{};
    result mixed_write_{};
    std::vector<unsigned> read_issue_order_;
    std::vector<unsigned> read_completion_order_;
    std::vector<unsigned> write_issue_order_;
    std::vector<unsigned> write_completion_order_;
    sc_core::sc_event start_writes_;
    sc_core::sc_event start_mixed_;

    result transact(
        tlm::tlm_command command, std::uint64_t address,
        std::array<unsigned char, transfer_bytes> bytes)
    {
        tlm::tlm_generic_payload payload;
        payload.set_command(command);
        payload.set_address(memory_base + address);
        payload.set_data_ptr(bytes.data());
        payload.set_data_length(bytes.size());
        payload.set_streaming_width(bytes.size());
        payload.set_byte_enable_ptr(nullptr);
        payload.set_byte_enable_length(0);
        payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        const auto before = sc_core::sc_time_stamp();
        socket->b_transport(payload, delay);
        const auto elapsed = sc_core::sc_time_stamp() - before;

        result observed{};
        observed.status = payload.get_response_status();
        observed.bytes = bytes;
        observed.elapsed_cycles =
            elapsed / sc_core::sc_time(1, sc_core::SC_NS);
        observed.network_cycles = noc->last_latency_cycles(0);
        observed.done = true;
        check(delay == sc_core::SC_ZERO_TIME,
              "same-port b_transport must spend time and return zero delay");
        return observed;
    }

    void run_read(unsigned index, std::uint64_t address)
    {
        wait(sc_core::sc_time(10, sc_core::SC_NS));
        read_issue_order_.push_back(index);
        reads_[index] = transact(
            tlm::TLM_READ_COMMAND, address,
            std::array<unsigned char, transfer_bytes>{});
        read_completion_order_.push_back(index);
    }

    void read_zero() { run_read(0, 0x00); }
    void read_one() { run_read(1, 0x08); }
    void read_two() { run_read(2, 0x10); }

    void run_write(unsigned index, std::uint64_t address, unsigned seed)
    {
        wait(start_writes_);
        std::array<unsigned char, transfer_bytes> bytes{};
        for (unsigned byte = 0; byte < bytes.size(); ++byte) {
            bytes[byte] = static_cast<unsigned char>(seed + byte);
        }
        write_issue_order_.push_back(index);
        writes_[index] =
            transact(tlm::TLM_WRITE_COMMAND, address, bytes);
        write_completion_order_.push_back(index);
    }

    void write_zero() { run_write(0, 0x20, 0xA0); }
    void write_one() { run_write(1, 0x28, 0xC0); }

    void mixed_read()
    {
        wait(start_mixed_);
        mixed_read_ = transact(
            tlm::TLM_READ_COMMAND, 0x30,
            std::array<unsigned char, transfer_bytes>{});
    }

    void mixed_write()
    {
        wait(start_mixed_);
        std::array<unsigned char, transfer_bytes> bytes{};
        for (unsigned byte = 0; byte < bytes.size(); ++byte) {
            bytes[byte] = static_cast<unsigned char>(0xE0 + byte);
        }
        mixed_write_ = transact(tlm::TLM_WRITE_COMMAND, 0x38, bytes);
    }

    bool reads_done() const
    {
        return reads_[0].done && reads_[1].done && reads_[2].done;
    }

    bool writes_done() const
    {
        return writes_[0].done && writes_[1].done;
    }

    bool mixed_done() const
    {
        return mixed_read_.done && mixed_write_.done;
    }

    template <typename Predicate>
    bool wait_until(Predicate predicate, sc_core::sc_time timeout)
    {
        const auto deadline = sc_core::sc_time_stamp() + timeout;
        while (!predicate() && sc_core::sc_time_stamp() < deadline) {
            wait(sc_core::sc_time(1, sc_core::SC_NS));
        }
        return predicate();
    }

    void check_target_delay_removed(const result& observed)
    {
        check(observed.elapsed_cycles
                  - static_cast<double>(observed.network_cycles)
                  >= 5.5,
              "each same-port completion must exclude its own target delay");
    }

    void coordinator()
    {
        check(wait_until([this] { return reads_done(); },
                         sc_core::sc_time(20, sc_core::SC_US)),
              "same-port read phase exceeded its watchdog");
        if (!reads_done()) {
            return;
        }

        check(noc->peak_outstanding_transactions(0) == configured_capacity,
              "same-port admission must stop at configured capacity");
        check(noc->outstanding_transactions(0) == 0,
              "all admitted read slots must be released");
        check(read_completion_order_ == read_issue_order_,
              "same-port read completions must preserve FIFO issue order");
        check(reads_[0].status == tlm::TLM_GENERIC_ERROR_RESPONSE
                  && reads_[1].status == tlm::TLM_OK_RESPONSE
                  && reads_[2].status == tlm::TLM_OK_RESPONSE,
              "same-port read response must return to its owning caller");
        check(reads_[1].bytes == memory->bytes_at(0x08)
                  && reads_[2].bytes == memory->bytes_at(0x10),
              "same-port read data must return to its owning caller");
        for (const auto& observed : reads_) {
            check_target_delay_removed(observed);
        }

        start_writes_.notify(sc_core::SC_ZERO_TIME);
        check(wait_until([this] { return writes_done(); },
                         sc_core::sc_time(20, sc_core::SC_US)),
              "same-port write phase exceeded its watchdog");
        if (!writes_done()) {
            return;
        }

        check(write_completion_order_ == write_issue_order_,
              "same-port write completions must preserve FIFO issue order");
        check(writes_[0].status == tlm::TLM_OK_RESPONSE
                  && writes_[1].status == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "same-port write response must return to its owning caller");
        const std::array<unsigned char, transfer_bytes> expected_write{
            0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7};
        check(memory->bytes_at(0x20) == expected_write,
              "successful same-port write must update only its target bytes");
        for (const auto& observed : writes_) {
            check_target_delay_removed(observed);
        }

        // One read and one write are now outstanding together. This is the
        // state that requires B and R completion ownership to remain separate;
        // the preceding phases exercise FIFO depth/order within each channel.
        start_mixed_.notify(sc_core::SC_ZERO_TIME);
        check(wait_until([this] { return mixed_done(); },
                         sc_core::sc_time(20, sc_core::SC_US)),
              "mixed same-port B/R phase exceeded its watchdog");
        if (!mixed_done()) {
            return;
        }
        check(mixed_read_.status == tlm::TLM_OK_RESPONSE
                  && mixed_read_.bytes == memory->bytes_at(0x30),
              "concurrent same-port R must retain its own completion owner");
        check(mixed_write_.status == tlm::TLM_OK_RESPONSE,
              "concurrent same-port B must retain its own completion owner");
        const std::array<unsigned char, transfer_bytes> expected_mixed_write{
            0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7};
        check(memory->bytes_at(0x38) == expected_mixed_write,
              "concurrent same-port B/R traffic must preserve write data");
        check_target_delay_removed(mixed_read_);
        check_target_delay_removed(mixed_write_);

        check(noc->completed_transactions() == 7,
              "every same-port transaction must complete exactly once");
        const auto access_count = [this](bool is_write, std::uint64_t address) {
            return std::count_if(
                memory->records.begin(), memory->records.end(),
                [is_write, address](const access_record& record) {
                    return record.is_write == is_write
                        && record.address == address;
                });
        };
        check(memory->records.size() == 7
                  && access_count(false, 0x00) == 1
                  && access_count(false, 0x08) == 1
                  && access_count(false, 0x10) == 1
                  && access_count(true, 0x20) == 1
                  && access_count(true, 0x28) == 1
                  && access_count(false, 0x30) == 1
                  && access_count(true, 0x38) == 1,
              "each same-port request must access the target exactly once");
        check(wait_until(
                  [this] {
                      return noc->wrapper_idle() && noc->mesh_quiescent();
                  },
                  sc_core::sc_time(20, sc_core::SC_US)),
              "same-port traffic must drain to complete quiescence");

        finished = true;
        sc_core::sc_stop();
    }

    void watchdog()
    {
        wait(sc_core::sc_time(100, sc_core::SC_US));
        if (!finished) {
            check(false, "same-port concurrency test exceeded global watchdog");
            sc_core::sc_stop();
        }
    }
};

} // namespace

int sc_main(int, char**)
{
    noc_interconnect noc{
        "noc", 2, 2, 1, 1, sc_core::sc_time(1, sc_core::SC_NS),
        configured_capacity};
    delayed_memory memory{"memory"};
    concurrent_manager manager{"manager"};

    noc.place_initiator(0, {0, 0});
    noc.add_target(memory_base, 0x100, {1, 1},
                   noc_interconnect::target_kind::memory)
        .bind(memory.socket);
    manager.socket.bind(noc.target_socket);
    manager.noc = &noc;
    manager.memory = &memory;

    sc_core::sc_start();

    if (failures == 0) {
        std::cout << "PASS: same-port NoC concurrency\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures
              << " same-port concurrency checks failed\n";
    return 1;
}
