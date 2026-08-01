// SPDX-License-Identifier: Apache-2.0
//
// Step 11: calibrate the fast LT backend against the detailed signal-driven
// backend. This is model-to-model calibration, not RTL equivalence.

#include "floo_noc_model/noc_interconnect.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using cdc::components::noc_interconnect;

constexpr std::uint64_t base0 = 0x9000'0000;
constexpr std::uint64_t region_size = 0x1000;
constexpr unsigned target_count = 6;
constexpr std::uint64_t calibration_tolerance_cycles = 1;

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class calibration_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<calibration_memory> socket;
    std::array<unsigned char, 0x200> storage{};
    unsigned accesses = 0;

    explicit calibration_memory(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &calibration_memory::b_transport);
        for (unsigned index = 0; index < storage.size(); ++index) {
            storage[index] = static_cast<unsigned char>(
                0x30u + (index * 13u) % 0xC0u);
        }
    }

private:
    void b_transport(
        tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        ++accesses;
        delay += sc_core::sc_time(1.5, sc_core::SC_NS);

        const auto address = trans.get_address();
        const auto length = trans.get_data_length();
        if (address == 0xE8) {
            throw std::runtime_error("injected fast-target exception");
        }
        if (address == 0xF0) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (address > storage.size()
            || length > storage.size() - static_cast<std::size_t>(address)) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        const auto* enables = trans.get_byte_enable_ptr();
        const auto enable_length = trans.get_byte_enable_length();
        for (unsigned index = 0; index < length; ++index) {
            const bool enabled = enables == nullptr
                || enables[index % enable_length] == TLM_BYTE_ENABLED;
            if (!enabled) {
                continue;
            }
            if (trans.is_write()) {
                storage[static_cast<std::size_t>(address) + index] =
                    trans.get_data_ptr()[index];
            } else {
                trans.get_data_ptr()[index] =
                    storage[static_cast<std::size_t>(address) + index];
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class calibration_driver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<calibration_driver> detailed_socket;
    tlm_utils::simple_initiator_socket<calibration_driver> fast_socket;
    noc_interconnect* detailed = nullptr;
    noc_interconnect* fast = nullptr;
    std::array<calibration_memory*, target_count> detailed_memories{};
    std::array<calibration_memory*, target_count> fast_memories{};

    SC_HAS_PROCESS(calibration_driver);

    explicit calibration_driver(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , detailed_socket("detailed_socket")
        , fast_socket("fast_socket")
    {
        SC_THREAD(run);
        SC_THREAD(watchdog);
    }

private:
    struct result {
        tlm::tlm_response_status status = tlm::TLM_INCOMPLETE_RESPONSE;
        std::vector<unsigned char> bytes;
        sc_core::sc_time elapsed = sc_core::SC_ZERO_TIME;
        sc_core::sc_time returned_delay = sc_core::SC_ZERO_TIME;
        std::uint64_t network_cycles = 0;
    };

    result transact(
        tlm_utils::simple_initiator_socket<calibration_driver>& socket,
        noc_interconnect& noc, tlm::tlm_command command,
        std::uint64_t address, std::vector<unsigned char> bytes,
        sc_core::sc_time incoming = sc_core::SC_ZERO_TIME,
        const std::vector<unsigned char>* enables = nullptr)
    {
        tlm::tlm_generic_payload payload;
        payload.set_command(command);
        payload.set_address(address);
        payload.set_data_ptr(bytes.data());
        payload.set_data_length(static_cast<unsigned>(bytes.size()));
        payload.set_streaming_width(static_cast<unsigned>(bytes.size()));
        payload.set_byte_enable_ptr(
            enables == nullptr
                ? nullptr
                : const_cast<unsigned char*>(enables->data()));
        payload.set_byte_enable_length(
            enables == nullptr ? 0u : static_cast<unsigned>(enables->size()));
        payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        auto delay = incoming;
        const auto before = sc_core::sc_time_stamp();
        socket->b_transport(payload, delay);

        result observed{};
        observed.status = payload.get_response_status();
        observed.bytes = std::move(bytes);
        observed.elapsed = sc_core::sc_time_stamp() - before;
        observed.returned_delay = delay;
        observed.network_cycles = noc.last_latency_cycles(0);
        return observed;
    }

    static std::uint64_t expected_fast_cycles(
        unsigned hops, bool is_write, unsigned length)
    {
        const unsigned beats = (length + 7u) / 8u;
        return 4u * hops + 6u
            + (is_write ? beats : beats - 1u);
    }

    void compare_one(
        unsigned target, bool is_write, unsigned length,
        std::uint64_t offset)
    {
        std::vector<unsigned char> input(length, 0);
        for (unsigned index = 0; index < length; ++index) {
            input[index] = static_cast<unsigned char>(
                0x80u + target * 7u + index);
        }
        const auto command =
            is_write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND;
        const auto address = base0 + target * region_size + offset;

        const auto detailed_result = transact(
            detailed_socket, *detailed, command, address, input);
        const auto fast_before = sc_core::sc_time_stamp();
        const auto fast_result = transact(
            fast_socket, *fast, command, address, input);

        check(detailed_result.status == fast_result.status,
              "fast and detailed response status must match");
        check(fast_result.elapsed == sc_core::SC_ZERO_TIME
                  && sc_core::sc_time_stamp() == fast_before,
              "fast b_transport must annotate without advancing simulation");
        check(fast_result.returned_delay
                  == sc_core::sc_time(
                      static_cast<double>(
                          expected_fast_cycles(target + 1, is_write, length)
                          + 2),
                      sc_core::SC_NS),
              "fast delay must include calibrated network and rounded target");

        const auto detailed_cycles =
            static_cast<long long>(detailed_result.network_cycles);
        const auto fast_cycles =
            static_cast<long long>(fast_result.network_cycles);
        check(std::llabs(detailed_cycles - fast_cycles)
                  <= static_cast<long long>(calibration_tolerance_cycles),
              "fast network estimate must stay within the one-cycle "
              "calibration tolerance");
        check(fast_result.network_cycles
                  == expected_fast_cycles(target + 1, is_write, length),
              "fast estimate must retain hop and burst serialization terms");

        if (is_write) {
            check(detailed_memories[target]->storage
                      == fast_memories[target]->storage,
                  "fast and detailed writes must have identical target effects");
        } else {
            check(detailed_result.bytes == fast_result.bytes,
                  "fast and detailed reads must return identical bytes");
        }

        // The LT initiator owns synchronization. Waiting here makes the next
        // sample start after the annotated transaction without changing the
        // fact that fast b_transport itself consumed no time.
        wait(fast_result.returned_delay);
    }

    void run()
    {
        check(detailed->selected_timing_mode()
                  == noc_interconnect::timing_mode::detailed,
              "the reference instance must select detailed timing");
        check(fast->selected_timing_mode()
                  == noc_interconnect::timing_mode::fast,
              "the LT instance must select fast timing");

        // Warm the detailed reset/gating path before taking calibration data.
        compare_one(0, false, 8, 0x40);

        constexpr std::array<unsigned, 5> lengths{1, 2, 4, 8, 32};
        for (unsigned target = 0; target < target_count; ++target) {
            for (const auto length : lengths) {
                compare_one(target, false, length, 0x40);
                compare_one(target, true, length, 0x80);
            }
        }

        check(detailed->last_latency_cycles(0) >= 10,
              "the detailed backend must report measured network cycles");
        check(fast->elapsed_cycles() == 0,
              "fast mode must never tick the cycle-stepped mesh");
        check(fast->mesh_quiescent(),
              "a bypassed fast-mode mesh is quiescent by construction");

        // Incoming local time is preserved and never spent by the fast target.
        const auto incoming = sc_core::sc_time(37, sc_core::SC_NS);
        const auto detailed_incoming = transact(
            detailed_socket, *detailed, tlm::TLM_READ_COMMAND, base0 + 0x48,
            std::vector<unsigned char>(8, 0), incoming);
        check(detailed_incoming.returned_delay == sc_core::SC_ZERO_TIME
                  && detailed_incoming.elapsed
                      == incoming + sc_core::sc_time(12, sc_core::SC_NS),
              "detailed mode must spend incoming plus calibrated access time");

        const auto before = sc_core::sc_time_stamp();
        auto incoming_result = transact(
            fast_socket, *fast, tlm::TLM_READ_COMMAND, base0 + 0x48,
            std::vector<unsigned char>(8, 0), incoming);
        check(sc_core::sc_time_stamp() == before
                  && incoming_result.elapsed == sc_core::SC_ZERO_TIME,
              "fast mode must not wait out incoming local time");
        check(incoming_result.returned_delay
                  == incoming + sc_core::sc_time(12, sc_core::SC_NS),
              "fast mode must preserve incoming delay and add its own latency");

        // An exception from target replay must release the fast-mode admission
        // slot. Otherwise a later access can block forever once the bound is
        // reached, even though the failed call has already unwound.
        bool target_exception_seen = false;
        try {
            (void)transact(
                fast_socket, *fast, tlm::TLM_READ_COMMAND, base0 + 0xE8,
                std::vector<unsigned char>(8, 0));
        } catch (const std::runtime_error& error) {
            target_exception_seen =
                std::string(error.what()) == "injected fast-target exception";
        }
        check(target_exception_seen,
              "fast mode must propagate a downstream target exception");
        check(fast->outstanding_transactions(0) == 0 && fast->wrapper_idle(),
              "fast target exception must release its slot and in-flight state");

        // Error mapping goes through the same downstream replay.
        const auto detailed_error = transact(
            detailed_socket, *detailed, tlm::TLM_READ_COMMAND, base0 + 0xF0,
            std::vector<unsigned char>(8, 0));
        const auto fast_error = transact(
            fast_socket, *fast, tlm::TLM_READ_COMMAND, base0 + 0xF0,
            std::vector<unsigned char>(8, 0));
        check(detailed_error.status == tlm::TLM_GENERIC_ERROR_RESPONSE
                  && fast_error.status == detailed_error.status,
              "fast mode must preserve subordinate error classification");

        // A sparse, unaligned multi-beat write exercises the shared lane and
        // WSTRB replay instead of only comparing naturally aligned accesses.
        const std::vector<unsigned char> enables{
            TLM_BYTE_ENABLED, TLM_BYTE_DISABLED, TLM_BYTE_ENABLED};
        std::vector<unsigned char> sparse(13, 0);
        for (unsigned index = 0; index < sparse.size(); ++index) {
            sparse[index] = static_cast<unsigned char>(0xD0u + index);
        }
        const auto sparse_address = base0 + 3 * region_size + 3;
        const auto sparse_detailed = transact(
            detailed_socket, *detailed, tlm::TLM_WRITE_COMMAND,
            sparse_address, sparse, sc_core::SC_ZERO_TIME, &enables);
        const auto sparse_fast = transact(
            fast_socket, *fast, tlm::TLM_WRITE_COMMAND,
            sparse_address, sparse, sc_core::SC_ZERO_TIME, &enables);
        check(sparse_detailed.status == tlm::TLM_OK_RESPONSE
                  && sparse_fast.status == sparse_detailed.status
                  && detailed_memories[3]->storage
                      == fast_memories[3]->storage,
              "fast mode must share sparse unaligned write semantics");

        check(fast->completed_transactions()
                  == detailed->completed_transactions(),
              "both calibration backends must complete the same access count");
        check(fast->wrapper_idle() && detailed->wrapper_idle(),
              "both backends must drain before calibration completes");

        sc_core::sc_stop();
    }

    void watchdog()
    {
        wait(sc_core::sc_time(200, sc_core::SC_US));
        check(false, "fast-mode calibration exceeded its watchdog");
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    noc_interconnect detailed{
        "detailed", 4, 4, target_count, 1,
        sc_core::sc_time(1, sc_core::SC_NS),
        noc_interconnect::default_max_outstanding_per_port,
        noc_interconnect::timing_mode::detailed};
    noc_interconnect fast{
        "fast", 4, 4, target_count, 1,
        sc_core::sc_time(1, sc_core::SC_NS),
        noc_interconnect::default_max_outstanding_per_port,
        noc_interconnect::timing_mode::fast};
    calibration_driver driver{"driver"};
    std::array<std::unique_ptr<calibration_memory>, target_count>
        detailed_memories;
    std::array<std::unique_ptr<calibration_memory>, target_count> fast_memories;

    detailed.place_initiator(0, {0, 0});
    fast.place_initiator(0, {0, 0});
    driver.detailed_socket.bind(detailed.target_socket);
    driver.fast_socket.bind(fast.target_socket);
    driver.detailed = &detailed;
    driver.fast = &fast;

    constexpr std::array<noc_interconnect::node, target_count> nodes{{
        {1, 0}, {2, 0}, {3, 0}, {3, 1}, {3, 2}, {3, 3}}};
    for (unsigned target = 0; target < target_count; ++target) {
        detailed_memories[target] = std::make_unique<calibration_memory>(
            sc_core::sc_gen_unique_name("detailed_memory"));
        fast_memories[target] = std::make_unique<calibration_memory>(
            sc_core::sc_gen_unique_name("fast_memory"));
        detailed.add_target(
            base0 + target * region_size, region_size, nodes[target],
            noc_interconnect::target_kind::memory)
            .bind(detailed_memories[target]->socket);
        fast.add_target(
            base0 + target * region_size, region_size, nodes[target],
            noc_interconnect::target_kind::memory)
            .bind(fast_memories[target]->socket);
        driver.detailed_memories[target] = detailed_memories[target].get();
        driver.fast_memories[target] = fast_memories[target].get();
    }

    sc_core::sc_start();

    if (failures == 0) {
        std::cout << "PASS: fast NoC calibration\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " fast-mode checks failed\n";
    return 1;
}
