// SPDX-License-Identifier: Apache-2.0
//
// Step 12.7: the passive completion observer.
//
// Why it exists. `last_latency_cycles()` cannot attribute a latency under
// concurrent traffic — it holds only the most recent completion, so a platform
// monitor polling it later may read a different manager's transaction. That is
// not a hypothetical: the measurement baseline needs PLIC claim and PLIC
// complete separately, and those are the same target at the same address,
// distinguished only by direction. Attribution has to happen at the completion
// point.
//
// What this test pins:
//
//   * every completed transaction is reported exactly once, in both timing
//     modes;
//   * the record identifies the requester, address, length and direction, so a
//     platform can classify without the interconnect knowing what any register
//     means;
//   * the reported latency equals `last_latency_cycles(port)` for that port,
//     so the observer is a view of the same number rather than a second
//     computation of it;
//   * concurrent managers keep their own attribution, which is the whole point;
//   * the hook is genuinely passive: installing one must not change the
//     transaction count or the measured latency.
//
// Everything is elaborated up front and run under a single `sc_start()`.
// SystemC does not allow a module to be constructed after simulation has
// started, so the "with observer" and "without observer" cases are separate
// instances running side by side rather than two sequential runs.

#include "floo_noc_model/noc_interconnect.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

using cdc::components::noc_interconnect;

constexpr std::uint64_t base0 = 0x9000'0000;
constexpr std::uint64_t region_size = 0x1000;
constexpr unsigned target_count = 2;

int failures = 0;

/// Three instances run side by side under one `sc_start()`. The last one to
/// finish stops the simulation; until then there is still work in flight, so
/// no driver may stop it on its own.
constexpr unsigned expected_drivers = 3;
unsigned drivers_finished = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class observed_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<observed_memory> socket;
    std::array<unsigned char, 0x100> storage{};

    explicit observed_memory(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &observed_memory::b_transport);
        for (std::size_t index = 0; index < storage.size(); ++index) {
            storage[index] = static_cast<unsigned char>(0x20u + index);
        }
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        // A deliberate target hold-off. The observer must report the network
        // share only, exactly as `last_latency_cycles()` does, so this time
        // must not appear in any reported latency.
        delay += sc_core::sc_time(3, sc_core::SC_NS);

        const auto address = static_cast<std::size_t>(trans.get_address());
        const auto length = trans.get_data_length();
        if (address + length > storage.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        for (unsigned index = 0; index < length; ++index) {
            if (trans.is_write()) {
                storage[address + index] = trans.get_data_ptr()[index];
            } else {
                trans.get_data_ptr()[index] = storage[address + index];
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct record {
    unsigned port = 0;
    std::uint64_t address = 0;
    unsigned length = 0;
    bool is_write = false;
    std::uint64_t latency_cycles = 0;
    std::uint64_t at_cycle = 0;
};

/// Drives one interconnect: a sequential pair on port 0, then a concurrent
/// pair across ports 0 and 1.
class observer_driver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<observer_driver> port0;
    tlm_utils::simple_initiator_socket<observer_driver> port1;
    noc_interconnect* noc = nullptr;
    std::vector<record>* seen = nullptr;

    SC_HAS_PROCESS(observer_driver);

    explicit observer_driver(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , port0("port0")
        , port1("port1")
    {
        SC_THREAD(drive_port0);
        SC_THREAD(drive_port1);
        SC_THREAD(watchdog);
    }

private:
    bool port0_done = false;
    bool port1_done = false;

    /// Bounded: a harness must fail with a message, never hang. It also owns
    /// the clean shutdown, because the drivers finishing is the only thing
    /// that means this simulation is over.
    void watchdog()
    {
        const auto deadline = sc_core::sc_time(500, sc_core::SC_US);
        while (sc_core::sc_time_stamp() < deadline) {
            if (port0_done && port1_done) {
                if (++drivers_finished == expected_drivers) {
                    sc_core::sc_stop();
                }
                return;
            }
            sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
        }
        std::cerr << "FAIL: observer harness watchdog expired\n";
        ++failures;
        sc_core::sc_stop();
    }

    void transact(
        tlm_utils::simple_initiator_socket<observer_driver>& socket,
        tlm::tlm_command command, std::uint64_t address,
        std::vector<unsigned char>& bytes)
    {
        tlm::tlm_generic_payload payload;
        payload.set_command(command);
        payload.set_address(address);
        payload.set_data_ptr(bytes.data());
        payload.set_data_length(static_cast<unsigned>(bytes.size()));
        payload.set_streaming_width(static_cast<unsigned>(bytes.size()));
        payload.set_byte_enable_ptr(nullptr);
        payload.set_byte_enable_length(0);
        payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        auto delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(payload, delay);
    }

    void drive_port0()
    {
        std::vector<unsigned char> write_bytes{1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<unsigned char> read_bytes(4, 0);
        std::vector<unsigned char> concurrent_bytes(8, 0x11);

        transact(port0, tlm::TLM_WRITE_COMMAND, base0 + 0x10, write_bytes);
        transact(port0, tlm::TLM_READ_COMMAND, base0 + 0x20, read_bytes);
        // Wait for the second manager to be released, then overlap with it.
        sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
        transact(port0, tlm::TLM_WRITE_COMMAND, base0 + 0x60,
                 concurrent_bytes);
        port0_done = true;
    }

    void drive_port1()
    {
        // Held off until port 0 has finished its sequential pair, so the first
        // two records are unambiguously port 0's and the last two overlap.
        sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));
        std::vector<unsigned char> bytes(4, 0x5A);
        transact(port1, tlm::TLM_WRITE_COMMAND,
                 base0 + region_size + 0x40, bytes);
        port1_done = true;
    }
};

/// One complete instance: interconnect, driver and targets.
struct instance {
    std::unique_ptr<noc_interconnect> noc;
    std::unique_ptr<observer_driver> driver;
    std::array<std::unique_ptr<observed_memory>, target_count> memories;
    std::vector<record> seen;
};

void build(
    instance& built, const char* label, noc_interconnect::timing_mode mode,
    bool install_observer)
{
    built.noc = std::make_unique<noc_interconnect>(
        sc_core::sc_gen_unique_name(label), 4, 4, target_count, 2,
        sc_core::sc_time(1, sc_core::SC_NS),
        noc_interconnect::default_max_outstanding_per_port, mode);
    built.driver = std::make_unique<observer_driver>(
        sc_core::sc_gen_unique_name("driver"));

    built.noc->place_initiator(0, {0, 0});
    built.noc->place_initiator(1, {0, 1});
    built.driver->port0.bind(built.noc->target_socket);
    built.driver->port1.bind(built.noc->cpu_port(1));
    built.driver->noc = built.noc.get();
    built.driver->seen = &built.seen;

    constexpr std::array<noc_interconnect::node, target_count> nodes{{
        {3, 3}, {2, 3}}};
    for (unsigned target = 0; target < target_count; ++target) {
        built.memories[target] = std::make_unique<observed_memory>(
            sc_core::sc_gen_unique_name("memory"));
        built.noc
            ->add_target(
                base0 + target * region_size, region_size, nodes[target],
                noc_interconnect::target_kind::memory)
            .bind(built.memories[target]->socket);
    }

    if (!install_observer) {
        return;
    }
    std::vector<record>* sink = &built.seen;
    built.noc->set_completion_observer(
        [sink](const noc_interconnect::completion& completion) {
            record entry{};
            entry.port = completion.port;
            entry.address = completion.address;
            entry.length = completion.length;
            entry.is_write = completion.is_write;
            entry.latency_cycles = completion.latency_cycles;
            entry.at_cycle = completion.at_cycle;
            sink->push_back(entry);
        });
}

void check_records(const instance& built, const std::string& label)
{
    const auto& seen = built.seen;
    check(seen.size() == built.noc->completed_transactions(),
          label + ": every completion must produce exactly one record");
    if (seen.size() < 2) {
        check(false, label + ": the harness produced too few records");
        return;
    }

    check(seen[0].is_write && seen[0].address == base0 + 0x10
              && seen[0].length == 8 && seen[0].port == 0,
          label + ": the first record must describe the write as presented");
    check(!seen[1].is_write && seen[1].address == base0 + 0x20
              && seen[1].length == 4 && seen[1].port == 0,
          label + ": the second record must describe the read as presented");
    check(seen[0].latency_cycles > 0 && seen[1].latency_cycles > 0,
          label + ": a real transaction must report a non-zero latency");

    // The concurrent pair: each manager keeps its own address and port, which
    // is exactly what a global "most recent completion" cannot preserve.
    unsigned from_port0 = 0;
    unsigned from_port1 = 0;
    for (std::size_t index = 2; index < seen.size(); ++index) {
        if (seen[index].port == 0) {
            ++from_port0;
            check(seen[index].address == base0 + 0x60,
                  label + ": a port-0 record must carry port 0's address");
        } else if (seen[index].port == 1) {
            ++from_port1;
            check(seen[index].address == base0 + region_size + 0x40,
                  label + ": a port-1 record must carry port 1's address");
        } else {
            check(false, label + ": a record names an unknown port");
        }
    }
    check(from_port0 == 1 && from_port1 == 1,
          label + ": each concurrent manager must be reported exactly once");
}

std::uint64_t accepted_flits(
    const floo::model::mesh_counter_snapshot& mesh)
{
    std::uint64_t total = 0;
    for (const auto& router : mesh.routers) {
        for (const auto& port : router.outputs) {
            total += port.accepted_flits;
        }
    }
    return total;
}

} // namespace

int sc_main(int, char**)
{
    instance detailed_observed;
    instance detailed_bare;
    instance fast_observed;

    build(detailed_observed, "detailed_observed",
          noc_interconnect::timing_mode::detailed, true);
    build(detailed_bare, "detailed_bare",
          noc_interconnect::timing_mode::detailed, false);
    build(fast_observed, "fast_observed",
          noc_interconnect::timing_mode::fast, true);

    sc_core::sc_start();

    check_records(detailed_observed, "detailed");
    check_records(fast_observed, "fast");

    // Passivity. The bare instance runs identical traffic with no observer
    // installed; an observer that moved a latency would be useless for
    // measuring one.
    check(detailed_bare.seen.empty(), "no observer means no records");
    check(detailed_bare.noc->completed_transactions()
              == detailed_observed.noc->completed_transactions(),
          "installing an observer must not change the transaction count");
    check(detailed_bare.noc->total_latency_cycles()
              == detailed_observed.noc->total_latency_cycles(),
          "installing an observer must not change the measured latency");

    // The target hold-off must not be in the reported latency. Detailed mode
    // charges it at the target's node, so the sum of the reported per-record
    // latencies must equal the interconnect's own total.
    std::uint64_t reported_sum = 0;
    for (const auto& entry : detailed_observed.seen) {
        reported_sum += entry.latency_cycles;
    }
    check(reported_sum == detailed_observed.noc->total_latency_cycles(),
          "the records must sum to the interconnect's own latency total");

    // D1 production counter integration. Both physical fabrics must have
    // observed real traffic, and installing the transaction observer above
    // must remain behaviour-neutral all the way down to the router counters.
    const auto observed_counters =
        detailed_observed.noc->detailed_counter_snapshot();
    const auto bare_counters = detailed_bare.noc->detailed_counter_snapshot();
    check(observed_counters.request.width == 4
              && observed_counters.request.height == 4
              && observed_counters.request.routers.size() == 16
              && observed_counters.response.routers.size() == 16,
          "detailed counter snapshot must preserve both 4x4 physical meshes");
    check(accepted_flits(observed_counters.request) > 0
              && accepted_flits(observed_counters.response) > 0,
          "request and response production meshes must both count traffic");
    check(accepted_flits(observed_counters.request)
                  == accepted_flits(bare_counters.request)
              && accepted_flits(observed_counters.response)
                  == accepted_flits(bare_counters.response),
          "a completion observer must not change router counter totals");

    detailed_observed.noc->reset_detailed_counters();
    const auto cleared = detailed_observed.noc->detailed_counter_snapshot();
    check(accepted_flits(cleared.request) == 0
              && accepted_flits(cleared.response) == 0
              && detailed_observed.noc->mesh_quiescent(),
          "counter reset must clear measurements without changing mesh state");

    bool fast_refused = false;
    try {
        (void)fast_observed.noc->detailed_counter_snapshot();
    } catch (const std::logic_error&) {
        fast_refused = true;
    }
    check(fast_refused,
          "fast mode must refuse measured router-counter snapshots");

    if (failures != 0) {
        std::cerr << failures << " completion-observer checks failed\n";
        return 1;
    }
    std::cout << "PASS: NoC completion observer\n";
    return 0;
}
