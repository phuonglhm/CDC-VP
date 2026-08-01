// SPDX-License-Identifier: Apache-2.0
//
// Step 10.3: bounded, deterministic-random stress of the unsigned TLM-to-AXI
// integration layer above the RTL-signed FlooNoC blocks.
//
// Three managers start at different sub-cycle offsets and contend for one
// delayed RAM. Each owns a disjoint address slice and maintains its own byte
// scoreboard. The offsets deliberately put two b_transport calls inside the
// network thread's drive-to-sample half-cycle; this is the shape that exposed
// the old injection race.

#include "floo_noc_model/axi_lanes.hpp"
#include "floo_noc_model/noc_interconnect.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

using cdc::components::noc_interconnect;

constexpr unsigned manager_count = 3;
constexpr std::uint64_t ram_base = 0x8000'0000;
constexpr std::size_t ram_size = 0x4000;
constexpr std::size_t owner_stride = 0x800;
constexpr std::size_t error_offset = 0x3F00;
constexpr unsigned phase_target_accesses_per_manager = 18;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct target_record {
    std::uint64_t address{};
    unsigned length{};
    bool is_write{};

    bool operator==(const target_record& rhs) const
    {
        return address == rhs.address && length == rhs.length
            && is_write == rhs.is_write;
    }
};

class delayed_scoreboard_ram : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<delayed_scoreboard_ram> socket;
    std::vector<target_record> records;

    SC_HAS_PROCESS(delayed_scoreboard_ram);

    delayed_scoreboard_ram(
        sc_core::sc_module_name name, sc_core::sc_time latency)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(ram_size, 0)
        , latency_(latency)
    {
        socket.register_b_transport(
            this, &delayed_scoreboard_ram::b_transport);
    }

private:
    std::vector<unsigned char> storage_;
    sc_core::sc_time latency_;

    void b_transport(
        tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const auto address = trans.get_address();
        const unsigned length = trans.get_data_length();
        records.push_back({address, length, trans.is_write()});
        delay += latency_;

        if (address >= error_offset) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (address > storage_.size()
            || length > storage_.size() - static_cast<std::size_t>(address)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        const auto* enables = trans.get_byte_enable_ptr();
        const unsigned enable_length = trans.get_byte_enable_length();
        for (unsigned index = 0; index < length; ++index) {
            const bool enabled = enables == nullptr
                || enables[index % enable_length] == TLM_BYTE_ENABLED;
            if (!enabled) {
                continue;
            }
            auto& byte = storage_[static_cast<std::size_t>(address) + index];
            if (trans.is_write()) {
                byte = trans.get_data_ptr()[index];
            } else {
                trans.get_data_ptr()[index] = byte;
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct stress_control {
    std::array<bool, manager_count> phase_done{};
    bool wake_done = false;
    bool coordinator_done = false;
    sc_core::sc_event wake_manager_zero;
};

class stress_manager : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<stress_manager> socket;

    noc_interconnect* noc = nullptr;
    stress_control* control = nullptr;
    unsigned port = 0;
    sc_core::sc_time start_offset = sc_core::SC_ZERO_TIME;

    std::vector<unsigned> expected_completion_order;
    std::vector<unsigned> completion_order;
    std::vector<target_record> expected_target_records;
    bool finished = false;

    SC_HAS_PROCESS(stress_manager);

    explicit stress_manager(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        SC_THREAD(run);
        SC_THREAD(transaction_watchdog);
    }

private:
    bool transaction_active_ = false;
    sc_core::sc_time transaction_started_ = sc_core::SC_ZERO_TIME;
    std::string active_name_;
    unsigned next_transaction_ = 0;
    std::uint32_t random_state_ = 1;
    std::uint64_t wake_address_ = 0;
    std::vector<unsigned char> wake_expected_;

    std::uint32_t random_word()
    {
        std::uint32_t value = random_state_;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        random_state_ = value;
        return value;
    }

    static target_record expected_record(
        tlm::tlm_command command, std::uint64_t address, unsigned length)
    {
        const auto shape =
            cdc::components::axi_lanes::shape_of(address, length);
        if (command == tlm::TLM_WRITE_COMMAND || shape.size_log2 < 3) {
            return {address - ram_base, length,
                    command == tlm::TLM_WRITE_COMMAND};
        }
        return {shape.beat0_addr - ram_base,
                shape.beats * cdc::components::axi_lanes::bus_bytes, false};
    }

    tlm::tlm_response_status transact(
        tlm::tlm_command command, std::uint64_t address,
        std::vector<unsigned char>& bytes, const std::string& name,
        const std::vector<unsigned char>* enables = nullptr,
        bool reaches_target = true)
    {
        const unsigned transaction = next_transaction_++;
        expected_completion_order.push_back(transaction);
        if (reaches_target && address < ram_base + error_offset) {
            expected_target_records.push_back(
                expected_record(command, address,
                                static_cast<unsigned>(bytes.size())));
        }

        tlm::tlm_generic_payload payload;
        payload.set_command(command);
        payload.set_address(address);
        payload.set_data_ptr(bytes.data());
        payload.set_data_length(static_cast<unsigned>(bytes.size()));
        payload.set_streaming_width(static_cast<unsigned>(bytes.size()));
        if (enables != nullptr) {
            payload.set_byte_enable_ptr(
                const_cast<unsigned char*>(enables->data()));
            payload.set_byte_enable_length(
                static_cast<unsigned>(enables->size()));
        } else {
            payload.set_byte_enable_ptr(nullptr);
            payload.set_byte_enable_length(0);
        }
        payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        const auto before = sc_core::sc_time_stamp();
        active_name_ = name;
        transaction_started_ = before;
        transaction_active_ = true;
        socket->b_transport(payload, delay);
        transaction_active_ = false;
        const auto elapsed = sc_core::sc_time_stamp() - before;

        completion_order.push_back(transaction);
        check(delay == sc_core::SC_ZERO_TIME,
              "stress b_transport must spend time and return zero delay");

        if (reaches_target) {
            const double elapsed_cycles =
                elapsed / sc_core::sc_time(1, sc_core::SC_NS);
            const auto network_cycles = noc->last_latency_cycles(port);
            check(network_cycles > 0,
                  "each stress completion must retain a per-requester latency");
            check(elapsed_cycles - static_cast<double>(network_cycles) >= 3.0,
                  "each requester must receive only its own target hold-off");
        }
        return payload.get_response_status();
    }

    void expect_ok(tlm::tlm_response_status status, const std::string& name)
    {
        check(status == tlm::TLM_OK_RESPONSE,
              name + " must complete with TLM_OK_RESPONSE");
    }

    void run()
    {
        wait(start_offset);
        random_state_ = 0x9E37'79B9u ^ (0x1020'3041u * (port + 1));

        const std::array<unsigned, 7> lengths{{1, 2, 4, 6, 8, 13, 24}};
        const std::uint64_t owner_base =
            ram_base + static_cast<std::uint64_t>(port) * owner_stride;

        for (unsigned item = 0; item < lengths.size(); ++item) {
            const unsigned length = lengths[item];
            const unsigned lane_offset = random_word() % 7;
            const std::uint64_t address =
                owner_base + 0x40 + item * 0x60 + lane_offset;

            std::vector<unsigned char> expected(length);
            for (unsigned index = 0; index < length; ++index) {
                expected[index] = static_cast<unsigned char>(
                    (port << 6) ^ (item << 3) ^ index ^ random_word());
            }
            auto write_bytes = expected;
            expect_ok(
                transact(tlm::TLM_WRITE_COMMAND, address, write_bytes,
                         "random write"),
                "random write");

            std::vector<unsigned char> read_bytes(length, 0);
            expect_ok(
                transact(tlm::TLM_READ_COMMAND, address, read_bytes,
                         "random read"),
                "random read");
            check(read_bytes == expected,
                  "stress read data must match its requester scoreboard");

            if (item == 0) {
                wake_address_ = address;
                wake_expected_ = expected;
            }
        }

        // A partial, multi-beat write. The repeating enable pattern includes
        // the first and final bytes, so the target sees the full 16-byte range
        // with holes carried by TLM byte enables.
        const std::uint64_t partial_address = owner_base + 0x600;
        std::vector<unsigned char> baseline(16);
        std::vector<unsigned char> expected(16);
        for (unsigned index = 0; index < baseline.size(); ++index) {
            baseline[index] =
                static_cast<unsigned char>(0x20 + port * 0x20 + index);
        }
        expected = baseline;
        expect_ok(
            transact(tlm::TLM_WRITE_COMMAND, partial_address, baseline,
                     "partial baseline"),
            "partial baseline");

        std::vector<unsigned char> patch(16);
        for (unsigned index = 0; index < patch.size(); ++index) {
            patch[index] =
                static_cast<unsigned char>(0xD0 + port * 7 + index);
        }
        const std::vector<unsigned char> enables{
            TLM_BYTE_ENABLED, 0, TLM_BYTE_ENABLED};
        for (unsigned index = 0; index < expected.size(); ++index) {
            if (enables[index % enables.size()] == TLM_BYTE_ENABLED) {
                expected[index] = patch[index];
            }
        }
        expect_ok(
            transact(tlm::TLM_WRITE_COMMAND, partial_address, patch,
                     "partial write", &enables),
            "partial write");
        std::vector<unsigned char> partial_read(16, 0);
        expect_ok(
            transact(tlm::TLM_READ_COMMAND, partial_address, partial_read,
                     "partial read"),
            "partial read");
        check(partial_read == expected,
              "partial write holes must match the byte scoreboard");

        std::vector<unsigned char> error_probe(8, 0);
        check(
            transact(tlm::TLM_READ_COMMAND,
                     ram_base + error_offset + port * 8, error_probe,
                     "target error")
                == tlm::TLM_GENERIC_ERROR_RESPONSE,
            "target SLVERR must return to the manager that requested it");

        std::vector<unsigned char> unmapped(8, 0);
        check(
            transact(tlm::TLM_READ_COMMAND,
                     0xDEAD'0000 + static_cast<std::uint64_t>(port) * 8,
                     unmapped, "unmapped", nullptr, false)
                == tlm::TLM_ADDRESS_ERROR_RESPONSE,
            "unmapped DECERR must return to the manager that requested it");

        control->phase_done[port] = true;
        if (port == 0) {
            wait(control->wake_manager_zero);
            std::vector<unsigned char> readback(wake_expected_.size(), 0);
            expect_ok(
                transact(tlm::TLM_READ_COMMAND, wake_address_, readback,
                         "idle wake-up"),
                "idle wake-up");
            check(readback == wake_expected_,
                  "idle wake-up read must preserve scoreboard data");
            control->wake_done = true;
        }
        finished = true;
    }

    void transaction_watchdog()
    {
        while (!finished) {
            wait(sc_core::sc_time(100, sc_core::SC_NS));
            if (transaction_active_
                && sc_core::sc_time_stamp() - transaction_started_
                       > sc_core::sc_time(20, sc_core::SC_US)) {
                check(false,
                      "stress transaction exceeded its per-transaction deadline: "
                          + active_name_);
                sc_core::sc_stop();
                return;
            }
        }
    }
};

class stress_coordinator : public sc_core::sc_module {
public:
    noc_interconnect* noc = nullptr;
    delayed_scoreboard_ram* ram = nullptr;
    stress_control* control = nullptr;
    std::array<stress_manager*, manager_count> managers{};

    SC_HAS_PROCESS(stress_coordinator);

    explicit stress_coordinator(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        SC_THREAD(run);
        SC_THREAD(global_watchdog);
    }

private:
    bool all_phase_done() const
    {
        for (const bool done : control->phase_done) {
            if (!done) {
                return false;
            }
        }
        return true;
    }

    bool wait_until(bool (stress_coordinator::*condition)() const,
                    sc_core::sc_time timeout)
    {
        const auto deadline = sc_core::sc_time_stamp() + timeout;
        while (!(this->*condition)()
               && sc_core::sc_time_stamp() < deadline) {
            wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        return (this->*condition)();
    }

    bool fully_quiescent() const
    {
        return noc->wrapper_idle() && noc->mesh_quiescent();
    }

    bool wake_done() const { return control->wake_done; }

    void run()
    {
        check(wait_until(&stress_coordinator::all_phase_done,
                         sc_core::sc_time(200, sc_core::SC_US)),
              "all three stress managers must finish within the phase deadline");
        if (!all_phase_done()) {
            return;
        }

        for (unsigned port = 0; port < manager_count; ++port) {
            const auto& manager = *managers[port];
            check(manager.completion_order == manager.expected_completion_order,
                  "each requester must complete transactions in issue order");

            std::vector<target_record> observed;
            const std::uint64_t low =
                static_cast<std::uint64_t>(port) * owner_stride;
            const std::uint64_t high = low + owner_stride;
            for (const auto& record : ram->records) {
                if (record.address >= low && record.address < high) {
                    observed.push_back(record);
                }
            }
            check(observed == manager.expected_target_records,
                  "target address/length/order must match each requester");
        }

        check(ram->records.size()
                  == manager_count * phase_target_accesses_per_manager,
              "unmapped requests must not reach the shared RAM target");
        check(noc->completed_transactions() == ram->records.size(),
              "every routed stress request must complete exactly once");
        check(noc->mesh_quiescent_wrapper_busy_cycles() > 0,
              "the directed run must observe a quiescent mesh with wrapper work");
        check(noc->mid_half_cycle_request_arrivals() >= 2,
              "staggered managers must enter during the sample half-cycle");

        check(wait_until(&stress_coordinator::fully_quiescent,
                         sc_core::sc_time(20, sc_core::SC_US)),
              "clock gate must wait for complete mesh quiescence");
        if (!fully_quiescent()) {
            return;
        }

        const auto cycles_before_idle = noc->elapsed_cycles();
        const auto gates_before_idle = noc->clock_gate_transitions();
        wait(sc_core::sc_time(50, sc_core::SC_NS));
        check(noc->elapsed_cycles() == cycles_before_idle,
              "a proven-quiescent network must stop advancing its clock");
        check(noc->clock_gate_transitions() > 0
                  && noc->clock_gate_transitions() >= gates_before_idle,
              "a proven-quiescent transition must enter the clock gate");

        control->wake_manager_zero.notify(sc_core::SC_ZERO_TIME);
        check(wait_until(&stress_coordinator::wake_done,
                         sc_core::sc_time(20, sc_core::SC_US)),
              "idle-to-active wake-up must complete within its deadline");
        check(noc->elapsed_cycles() > cycles_before_idle,
              "idle-to-active wake-up must restart the mesh clock");

        check(wait_until(&stress_coordinator::fully_quiescent,
                         sc_core::sc_time(20, sc_core::SC_US)),
              "the wake-up transaction must drain to full quiescence");
        check(ram->records.size()
                  == manager_count * phase_target_accesses_per_manager + 1,
              "the wake-up read must reach the target exactly once");
        check(noc->completed_transactions() == ram->records.size(),
              "completion count must equal every routed target access");

        control->coordinator_done = true;
        sc_core::sc_stop();
    }

    void global_watchdog()
    {
        wait(sc_core::sc_time(500, sc_core::SC_US));
        if (!control->coordinator_done) {
            check(false, "stress test exceeded its global watchdog");
            sc_core::sc_stop();
        }
    }
};

} // namespace

int sc_main(int, char**)
{
    noc_interconnect noc{"noc", 4, 4, 1, manager_count};
    delayed_scoreboard_ram ram{
        "ram", sc_core::sc_time(3.5, sc_core::SC_NS)};
    stress_control control;
    std::array<stress_manager*, manager_count> managers{};

    noc.place_initiator(0, {0, 0});
    noc.place_initiator(1, {0, 1});
    noc.place_initiator(2, {0, 2});
    noc.add_target(
           ram_base, ram_size, {3, 3}, noc_interconnect::target_kind::memory)
        .bind(ram.socket);

    stress_manager manager0{"manager0"};
    stress_manager manager1{"manager1"};
    stress_manager manager2{"manager2"};
    managers = {{&manager0, &manager1, &manager2}};

    const std::array<sc_core::sc_time, manager_count> starts{{
        sc_core::sc_time(10, sc_core::SC_NS),
        sc_core::sc_time(10, sc_core::SC_NS)
            + sc_core::sc_time(250, sc_core::SC_PS),
        sc_core::sc_time(10, sc_core::SC_NS)
            + sc_core::sc_time(375, sc_core::SC_PS),
    }};

    for (unsigned port = 0; port < manager_count; ++port) {
        auto& manager = *managers[port];
        manager.noc = &noc;
        manager.control = &control;
        manager.port = port;
        manager.start_offset = starts[port];
        manager.socket.bind(noc.cpu_port(port));
    }

    stress_coordinator coordinator{"coordinator"};
    coordinator.noc = &noc;
    coordinator.ram = &ram;
    coordinator.control = &control;
    coordinator.managers = managers;

    sc_core::sc_start();

    if (failures == 0) {
        std::cout << "PASS: Step 10.3 three-manager TLM wrapper stress\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " Step 10.3 stress checks failed\n";
    return 1;
}
