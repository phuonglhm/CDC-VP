// SPDX-License-Identifier: Apache-2.0
//
// The Phase 8 chip-fabric gate.
//
// The fabric's whole job is deciding where an address goes, so this file is
// about decode outcomes and about who was made to wait — not about what the
// things behind it do. Every downstream port is a probe that records what
// arrived, because "the transaction succeeded" is not the same claim as "the
// transaction arrived at the target the map names", and only the second one is
// what a routing component can be wrong about.
//
// Three properties here have no single-core equivalent and are the reason this
// component exists:
//
//   * a core addressing its sibling is answered **inside the chip**. If that
//     ever reached `external`, the mesh would be handed traffic for a node that
//     hosts an upstream port, which `NoLoopback = 1` refuses (D1);
//   * an inbound access naming a foreign address is refused rather than
//     forwarded, so one mis-route cannot become a loop;
//   * with a real arbiter, two initiators contending for one port are served in
//     rotating priority. Fairness is only measurable in that mode: under
//     annotation alone, requests are processed in call order and round-robin is
//     indistinguishable from first-come-first-served.
//
// Exit codes: 0 pass, 1 fail.

#include "tpu_v3/chip/chip_local_fabric.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"

namespace tpu = cdc::components::tpu_v3;
namespace chip = cdc::components::tpu_v3::chip;
namespace am = cdc::components::tpu_v3::address_map;

using chip::chip_aperture_of;
using chip::chip_destination;
using chip::chip_fabric_timing;
using chip::chip_initiator;
using chip::chip_local_fabric;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr tpu::chip_id_t kChip = 1;

/// One downstream port. Records what arrived and answers; the point is the
/// record, not the data.
class probe : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<probe> socket;

    std::uint64_t requests = 0;
    std::uint64_t last_address = 0;
    unsigned last_length = 0;
    /// Held while a transaction is inside `b_transport`, so a test can show
    /// that the fabric never has two of them here at once.
    unsigned in_flight = 0;
    unsigned peak_in_flight = 0;
    /// Non-zero makes the port slow, which is how contention is produced.
    sc_core::sc_time service = sc_core::SC_ZERO_TIME;
    /// Arrival order, read out of the payload. The fairness evidence.
    ///
    /// Deliberately **not** a variable the driver sets before calling: between
    /// that assignment and the arrival here the fabric blocks, so the second
    /// contender overwrites it while the first is still queued and every grant
    /// gets attributed to whoever called most recently. The first version of
    /// this bench did that, and once the arbiter was fixed it still produced a
    /// perfectly alternating sequence — with the two labels swapped. It would
    /// have passed either way, which is the problem.
    std::vector<int> order;

    explicit probe(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(4096, 0)
    {
        socket.register_b_transport(this, &probe::b_transport);
        socket.register_transport_dbg(this, &probe::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        ++requests;
        last_address = trans.get_address();
        last_length = trans.get_data_length();
        // The contenders write their own index into every byte, so the order
        // recorded here is the order the port actually served.
        order.push_back(trans.get_command() == tlm::TLM_WRITE_COMMAND
                                && trans.get_data_length() > 0
                            ? static_cast<int>(trans.get_data_ptr()[0])
                            : -1);

        ++in_flight;
        peak_in_flight = std::max(peak_in_flight, in_flight);
        if (service != sc_core::SC_ZERO_TIME) {
            sc_core::wait(service);
        }
        --in_flight;

        const std::uint64_t offset = trans.get_address() % storage_.size();
        const unsigned length = trans.get_data_length();
        if (offset + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&storage_[offset], trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), &storage_[offset], length);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        const std::uint64_t offset = trans.get_address() % storage_.size();
        const unsigned length = trans.get_data_length();
        if (offset + length > storage_.size()) {
            return 0;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&storage_[offset], trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), &storage_[offset], length);
        }
        return length;
    }

    std::vector<unsigned char> storage_;
};

/// One initiator port on the fabric, plus the payload plumbing every test here
/// needs.
class driver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<driver> socket;

    driver(sc_core::sc_module_name name, int id)
        : sc_core::sc_module(name)
        , socket("socket")
        , id_(id)
    {
    }

    int id() const noexcept { return id_; }

    /// Writes this driver's own index into every byte, so a target can say
    /// which initiator it is serving without the bench having to guess.
    tlm::tlm_response_status write(std::uint64_t address, unsigned length,
                                   sc_core::sc_time& delay)
    {
        std::vector<unsigned char> data(length,
                                        static_cast<unsigned char>(id_));
        return access(tlm::TLM_WRITE_COMMAND, address, data, delay);
    }

    tlm::tlm_response_status read(std::uint64_t address, unsigned length,
                                  sc_core::sc_time& delay)
    {
        std::vector<unsigned char> data(length, 0);
        return access(tlm::TLM_READ_COMMAND, address, data, delay);
    }

    /// A debug transaction with a non-empty length and no data pointer. It is
    /// malformed, and `common_payload_error()` cannot see that — it checks the
    /// command, the streaming width and the byte-enable shape and says nothing
    /// about the pointer. Forwarding it hands a downstream debug target a null
    /// to dereference.
    unsigned int debug_read_null(std::uint64_t address, unsigned length)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(nullptr);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return socket->transport_dbg(trans);
    }

    unsigned int debug_read(std::uint64_t address, unsigned length)
    {
        std::vector<unsigned char> data(length, 0);
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data.data());
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return socket->transport_dbg(trans);
    }

    tlm::tlm_response_status access(tlm::tlm_command command,
                                    std::uint64_t address,
                                    std::vector<unsigned char>& data,
                                    sc_core::sc_time& delay)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data.data());
        trans.set_data_length(static_cast<unsigned>(data.size()));
        trans.set_streaming_width(static_cast<unsigned>(data.size()));
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }

private:
    int id_;
};

/// Fabric, three drivers and five probes, wired the way a chip wires them.
///
/// Every instance takes a name prefix. SystemC renames a duplicate module name
/// with a warning rather than refusing it, and a suite whose modules are all
/// called `fabric` is one where the warning is the only sign that two tests are
/// looking at each other's object.
struct bench {
    chip_local_fabric fabric;
    driver core0;
    driver core1;
    driver inbound;
    probe core0_port;
    probe core1_port;
    probe control;
    probe counters;
    probe outside;

    explicit bench(const std::string& prefix,
                   chip_fabric_timing timing = chip_fabric_timing::annotated,
                   sc_core::sc_time cycle = sc_core::sc_time(10,
                                                             sc_core::SC_NS))
        : fabric((prefix + "_fabric").c_str(), chip_aperture_of(kChip), timing,
                 cycle)
        , core0((prefix + "_drv_core0").c_str(), 0)
        , core1((prefix + "_drv_core1").c_str(), 1)
        , inbound((prefix + "_drv_inbound").c_str(), 2)
        , core0_port((prefix + "_probe_core0").c_str())
        , core1_port((prefix + "_probe_core1").c_str())
        , control((prefix + "_probe_control").c_str())
        , counters((prefix + "_probe_counters").c_str())
        , outside((prefix + "_probe_outside").c_str())
    {
        core0.socket.bind(fabric.from[0]);
        core1.socket.bind(fabric.from[1]);
        inbound.socket.bind(fabric.from[2]);
        fabric.to_core[0].bind(core0_port.socket);
        fabric.to_core[1].bind(core1_port.socket);
        fabric.to_control.bind(control.socket);
        fabric.to_counters.bind(counters.socket);
        fabric.external.bind(outside.socket);
    }
};

// ── decode ───────────────────────────────────────────────────────────────────

void check_decode()
{
    const auto spec = chip_aperture_of(kChip);
    chip_local_fabric fabric("decode_only", spec);

    CHECK(fabric.decode(am::core_sram_base(kChip, 0), 4)
          == chip_destination::core0);
    CHECK(fabric.decode(am::sa_control(kChip, 1), 4)
          == chip_destination::core1);
    CHECK(fabric.decode(am::chip_control(kChip), 4)
          == chip_destination::chip_control);
    CHECK(fabric.decode(am::chip_counters(kChip), 4)
          == chip_destination::chip_counters);
    CHECK(fabric.decode(am::global_ram_base, 4) == chip_destination::outside);
    CHECK(fabric.decode(am::boot_rom_base, 4) == chip_destination::outside);
    CHECK_MSG(fabric.decode(am::chip_base(kChip == 0 ? 1 : 0), 4)
                  == chip_destination::outside,
              "another chip's aperture is outside this one");

    // The last four bytes of core 0's aperture plus the first four of core 1's.
    CHECK_MSG(fabric.decode(am::core_base(kChip, 1) - 4, 8)
                  == chip_destination::none,
              "a transfer spanning both cores decodes to two destinations and "
              "must be refused, not routed by whichever check ran first");

    // The reserved hole above CHIP_COUNTERS, still inside the chip aperture.
    const std::uint64_t hole
        = am::chip_counters(kChip) + am::chip_counters_size;
    CHECK_MSG(fabric.decode(hole, 4) == chip_destination::none,
              "a hole inside the chip aperture must be refused; sending it "
              "outside would hand the mesh this node's own address");

    CHECK_MSG(fabric.decode(am::core_sram_base(kChip, 0), 0)
                  == chip_destination::none,
              "a zero-length transfer names no bytes and decodes to nothing");
}

// ── routing ──────────────────────────────────────────────────────────────────

void check_routing()
{
    bench b("route");
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    // Core 0 reaching its sibling: answered inside the chip.
    CHECK(b.core0.write(am::core_sram_base(kChip, 1), 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.core1_port.requests == 1);
    CHECK(b.core1_port.last_address == am::core_sram_base(kChip, 1));
    CHECK_MSG(b.outside.requests == 0,
              "core-to-core traffic reached the mesh port; with NoLoopback = 1 "
              "that is a deadlock, not a slow path");
    CHECK(b.fabric.local_bypass() == 1);
    CHECK(b.fabric.routed(chip_initiator::core0, chip_destination::core1) == 1);

    // Core 1 the other way.
    CHECK(b.core1.write(am::sa_control(kChip, 0), 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.core0_port.requests == 1);
    CHECK(b.fabric.local_bypass() == 2);

    // Chip registers.
    CHECK(b.core0.read(am::chip_control(kChip), 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.control.requests == 1);
    CHECK(b.core1.read(am::chip_counters(kChip), 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.counters.requests == 1);

    // Outside the chip.
    CHECK(b.core0.read(am::global_ram_base, 8, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.outside.requests == 1);
    CHECK(b.fabric.outbound_requests() == 1);

    // A core naming its own aperture. The core's own bridge refuses this
    // first; the fabric refusing it too is the second line, and this is the
    // only way to prove the second line exists.
    CHECK_MSG(b.core0.write(am::core_sram_base(kChip, 0), 4, delay)
                  == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "the fabric accepted a core's own aperture on its outbound port");
    CHECK(b.fabric.self_refused() == 1);
    CHECK_MSG(b.core0_port.requests == 1,
              "the refused access was forwarded to the core anyway");

    // Inbound may name anything inside the chip.
    CHECK(b.inbound.write(am::core_sram_base(kChip, 0), 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.core0_port.requests == 2);
    CHECK(b.inbound.read(am::chip_control(kChip), 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.control.requests == 2);

    // And nothing outside it.
    CHECK_MSG(b.inbound.read(am::global_ram_base, 4, delay)
                  == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "an inbound access to a foreign address was accepted; forwarding "
              "it back out turns one mis-route into a loop");
    CHECK(b.fabric.inbound_foreign_refused() == 1);
    CHECK(b.outside.requests == 1);

    // A straddling transfer.
    CHECK(b.core0.write(am::core_base(kChip, 1) - 4, 8, delay)
          == tlm::TLM_ADDRESS_ERROR_RESPONSE);
    CHECK(b.fabric.decode_refused() == 1);

    // A command that is neither read nor write.
    std::vector<unsigned char> data(4, 0);
    CHECK(b.core0.access(tlm::TLM_IGNORE_COMMAND, am::global_ram_base, data,
                         delay)
          == tlm::TLM_COMMAND_ERROR_RESPONSE);
    CHECK(b.fabric.protocol_errors() == 1);
}

void check_debug_transport()
{
    bench b("dbg");

    // The debug path decodes and routes by the same rules, and reaches the
    // same targets.
    CHECK(b.core0.debug_read(am::core_sram_base(kChip, 1), 4) == 4);
    CHECK_MSG(b.core0.debug_read(am::core_sram_base(kChip, 0), 4) == 0,
              "a debug access got a relaxation the normal path does not have");
    CHECK(b.inbound.debug_read(am::global_ram_base, 4) == 0);
    CHECK(b.core0.debug_read(am::core_base(kChip, 1) - 4, 8) == 0);
    CHECK(b.inbound.debug_read(am::chip_counters(kChip), 4) == 4);

    CHECK_MSG(b.fabric.requests(chip_initiator::core0) == 0,
              "the debug path moved a counter; it must be free of side "
              "effects (INTERFACE_CONTRACT.md §8)");

    // A malformed payload is refused on the debug path too. §8 relaxes the
    // *timing*, not the rules, and `b_transport` already refuses this one.
    CHECK_MSG(b.core0.debug_read_null(am::core_sram_base(kChip, 1), 4) == 0,
              "a debug transaction with a non-zero length and a null data "
              "pointer was forwarded; the target it reaches has nothing to "
              "read from but a null pointer");
    CHECK_MSG(b.core1_port.requests == 0,
              "the malformed debug transaction reached a target");
}

// ── contention ───────────────────────────────────────────────────────────────

/// Drives one initiator at a chosen time, so two of them can be made to
/// contend for one port deterministically.
class contender : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(contender);

    /// `carry` is the TLM delay each request arrives with. Non-zero models a
    /// temporally decoupled initiator that has not reached the current instant.
    contender(sc_core::sc_module_name name, driver& drv,
              std::uint64_t address, sc_core::sc_time start, unsigned repeats,
              sc_core::sc_time carry = sc_core::SC_ZERO_TIME)
        : sc_core::sc_module(name)
        , drv_(drv)
        , address_(address)
        , start_(start)
        , repeats_(repeats)
        , carry_(carry)
    {
        SC_THREAD(run);
    }

    sc_core::sc_time finished_at = sc_core::SC_ZERO_TIME;
    /// `sc_time_stamp()` when `b_transport` returned, before this process
    /// consumes the delay it came back with. A correctly returned remainder
    /// makes the two differ, and it is the *return* that says whether a reset
    /// was prompt.
    sc_core::sc_time returned_at = sc_core::SC_ZERO_TIME;
    unsigned completed = 0;
    unsigned errors = 0;

    /// Set if this initiator's logical time — `sc_time_stamp() + delay` — ever
    /// decreased across a call. A decoupled initiator sets its quantum keeper
    /// from the returned delay, so a component that hands back less than it
    /// consumed moves the initiator into its own past.
    bool went_backwards = false;
    sc_core::sc_time worst_rollback = sc_core::SC_ZERO_TIME;

private:
    void run()
    {
        if (start_ != sc_core::SC_ZERO_TIME) {
            sc_core::wait(start_);
        }
        for (unsigned i = 0; i < repeats_; ++i) {
            sc_core::sc_time delay = carry_;
            const sc_core::sc_time logical_before
                = sc_core::sc_time_stamp() + delay;

            const auto status = drv_.write(address_, 4, delay);

            returned_at = sc_core::sc_time_stamp();
            const sc_core::sc_time logical_after
                = sc_core::sc_time_stamp() + delay;
            if (logical_after < logical_before) {
                went_backwards = true;
                const auto rollback = logical_before - logical_after;
                if (rollback > worst_rollback) {
                    worst_rollback = rollback;
                }
            }

            if (delay != sc_core::SC_ZERO_TIME) {
                sc_core::wait(delay);
            }
            if (status == tlm::TLM_OK_RESPONSE) {
                ++completed;
            } else {
                ++errors;
            }
        }
        finished_at = sc_core::sc_time_stamp();
    }

    driver& drv_;
    std::uint64_t address_;
    sc_core::sc_time start_;
    unsigned repeats_;
    sc_core::sc_time carry_;
};

/// Calls `reset()` on a fabric at a chosen instant.
class fabric_resetter : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(fabric_resetter);

    fabric_resetter(sc_core::sc_module_name name, chip_local_fabric& fabric,
                    sc_core::sc_time at)
        : sc_core::sc_module(name)
        , fabric_(fabric)
        , at_(at)
    {
        SC_THREAD(run);
    }

    bool fired = false;

private:
    void run()
    {
        sc_core::wait(at_);
        fabric_.reset();
        fired = true;
    }

    chip_local_fabric& fabric_;
    sc_core::sc_time at_;
};

void check_annotated_contention()
{
    bench b("annot", chip_fabric_timing::annotated,
            sc_core::sc_time(10, sc_core::SC_NS));

    // Two initiators arriving at the same instant, both bound for the mesh
    // port. Nothing blocks: the second one is charged for the port the first
    // is holding, and the charge appears in its delay.
    sc_core::sc_time first = sc_core::SC_ZERO_TIME;
    CHECK(b.core0.write(am::global_ram_base, 4, first)
          == tlm::TLM_OK_RESPONSE);
    sc_core::sc_time second = sc_core::SC_ZERO_TIME;
    CHECK(b.core1.write(am::global_ram_base, 4, second)
          == tlm::TLM_OK_RESPONSE);

    CHECK_MSG(second > first,
              "the second initiator was not charged for the port the first "
              "was holding");
    CHECK(b.fabric.port_conflicts(chip_destination::outside) == 1);
    CHECK_MSG(b.outside.peak_in_flight == 1,
              "two transactions were inside the target at once");
}

void check_arbitrated_reset_keeps_the_port_with_its_owner(bench& b,
                                                          contender& holder,
                                                          contender& queued,
                                                          fabric_resetter& reset);

void check_arbitrated_fairness()
{
    // A slow port and two initiators hammering it. In this mode the fabric
    // blocks them, so who goes next is a behaviour rather than an annotation.
    static bench b("arb", chip_fabric_timing::arbitrated,
                   sc_core::sc_time(10, sc_core::SC_NS));
    b.outside.service = sc_core::sc_time(40, sc_core::SC_NS);

    static contender a("contend_core0", b.core0, am::global_ram_base,
                       sc_core::SC_ZERO_TIME, 6);
    static contender c("contend_core1", b.core1, am::global_ram_base,
                       sc_core::SC_ZERO_TIME, 6);

    // The reset-ownership bench, built here because this file has one
    // `sc_start` and SystemC refuses a module created after it.
    static bench r("rstarb", chip_fabric_timing::arbitrated,
                   sc_core::sc_time(10, sc_core::SC_NS));
    r.outside.service = sc_core::sc_time(1, sc_core::SC_US);
    static contender holder("rst_holder", r.core0, am::global_ram_base,
                            sc_core::SC_ZERO_TIME, 1);
    static contender queued("rst_queued", r.core1, am::global_ram_base,
                            sc_core::sc_time(100, sc_core::SC_NS), 2);
    static fabric_resetter reset_at("rst_at", r.fabric,
                                    sc_core::sc_time(300, sc_core::SC_NS));

    // The third reset population, one level up from the bridge: an initiator
    // waiting out its own quantum before it may contend is registered nowhere
    // — not queued on the arbiter, not downstream — and a bare timed wait
    // could not be woken by `reset()`. `q_core0` holds this port for 5 us;
    // `q_core1` arrives carrying 3 us and must be abandoned by the 500 ns
    // reset rather than at 3 us.
    static bench q("rstq", chip_fabric_timing::arbitrated,
                   sc_core::sc_time(10, sc_core::SC_NS));
    q.outside.service = sc_core::sc_time(5, sc_core::SC_US);
    static contender q_holder("rstq_holder", q.core0, am::global_ram_base,
                              sc_core::SC_ZERO_TIME, 1);
    static contender q_late("rstq_late", q.core1, am::global_ram_base,
                            sc_core::SC_ZERO_TIME, 1,
                            sc_core::sc_time(3, sc_core::SC_US));
    static fabric_resetter q_reset("rstq_at", q.fabric,
                                   sc_core::sc_time(500, sc_core::SC_NS));

    sc_core::sc_start(sc_core::sc_time(20, sc_core::SC_US));

    CHECK(a.completed == 6);
    CHECK(c.completed == 6);
    CHECK_MSG(b.outside.peak_in_flight == 1,
              "the arbiter let two transactions into the port at once");
    CHECK_MSG(b.fabric.port_conflicts(chip_destination::outside) > 0,
              "no initiator ever found the port busy, so this run measured no "
              "arbitration at all");

    const std::uint64_t g0
        = b.fabric.port_grants(chip_destination::outside, chip_initiator::core0);
    const std::uint64_t g1
        = b.fabric.port_grants(chip_destination::outside, chip_initiator::core1);
    CHECK(g0 == 6 && g1 == 6);

    // Rotating priority: once both are queued, the port alternates. The first
    // arrival is whoever SystemC ran first, so the evidence is the alternation
    // after that, not the identity of the first.
    unsigned alternations = 0;
    for (std::size_t i = 1; i < b.outside.order.size(); ++i) {
        if (b.outside.order[i] != b.outside.order[i - 1]) {
            ++alternations;
        }
    }
    std::cout << "arbitrated external port: " << alternations
              << " alternations in " << b.outside.order.size() << " grants\n";
    CHECK_MSG(alternations >= 9,
              "the port did not alternate between the two contenders: "
              + std::to_string(alternations) + " changes in "
              + std::to_string(b.outside.order.size()) + " grants, which is "
              "first-come-first-served rather than rotating priority");

    check_arbitrated_reset_keeps_the_port_with_its_owner(r, holder, queued,
                                                         reset_at);

    CHECK_MSG(q_reset.fired, "the quantum-catch-up reset never ran");
    CHECK_MSG(q_late.errors == 1,
              "the initiator waiting out its quantum was not abandoned");
    std::cout << "chip port quantum catch-up abandoned at "
              << q_late.returned_at << " (reset at 500 ns, quantum would have "
                                       "expired at 3 us)\n";
    CHECK_MSG(q_late.returned_at < sc_core::sc_time(1, sc_core::SC_US),
              "the reset did not reach an initiator waiting out its quantum: "
              "it returned at "
                  + q_late.finished_at.to_string()
                  + " instead of promptly after the 500 ns reset, so it slept "
                    "out the whole 3 us");

    // Waking early must not cost the initiator its clock: the unelapsed part of
    // the quantum has to come back in `delay`.
    for (const auto* who : {&q_late, &q_holder, &holder, &queued, &a, &c}) {
        CHECK_MSG(!who->went_backwards,
                  std::string("an initiator's logical time moved backwards by ")
                      + who->worst_rollback.to_string()
                      + ". `sc_time_stamp() + delay` must never decrease across "
                        "a b_transport");
    }
}

/// A reset must not hand a port away underneath the initiator holding it.
///
/// Core 0 is inside a one-microsecond downstream call; core 1 queues behind it
/// and is abandoned by a reset at 300 ns, then issues again immediately — into
/// the window where core 0 still owns the port. `reset()` cannot unwind core
/// 0's blocked call, so it must not release its port either: doing so puts two
/// cores inside one target at the same time, which is the one thing this
/// arbiter exists to prevent.
///
/// Constructed here rather than in its own function because the whole file
/// shares one `sc_start`, and a SystemC module cannot be created after it.
void check_arbitrated_reset_keeps_the_port_with_its_owner(bench& b,
                                                          contender& holder,
                                                          contender& queued,
                                                          fabric_resetter& reset)
{
    CHECK_MSG(reset.fired, "the reset never ran");

    CHECK_MSG(holder.completed == 1,
              "the transaction that was already inside the target when the "
              "reset arrived did not complete. Reset abandons what is queued; "
              "it cannot un-issue what a target has already been handed");

    CHECK_MSG(queued.completed == 1,
              "the request issued after the reset never completed, so the "
              "arbiter did not recover from it");

    CHECK_MSG(b.outside.peak_in_flight == 1,
              "two transactions were inside one downstream target at once "
              "after a reset: the port was released on behalf of an initiator "
              "that was still inside its downstream call");
}

void check_reset()
{
    bench b("rst");
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    CHECK(b.core0.write(am::core_sram_base(kChip, 1), 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.core0.read(am::global_ram_base, 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.core0.write(am::core_sram_base(kChip, 0), 4, delay)
          == tlm::TLM_ADDRESS_ERROR_RESPONSE);
    CHECK_MSG(b.fabric.requests(chip_initiator::core0) == 3,
              "the counters were not moving before the reset, so what follows "
              "would be a statement about a fabric that was never used");

    b.fabric.reset();

    CHECK(b.fabric.requests(chip_initiator::core0) == 0);
    CHECK(b.fabric.local_bypass() == 0);
    CHECK(b.fabric.outbound_requests() == 0);
    CHECK(b.fabric.self_refused() == 0);
    CHECK(b.fabric.port_conflicts(chip_destination::outside) == 0);
    CHECK(b.fabric.port_grants(chip_destination::outside,
                               chip_initiator::core0)
          == 0);

    // And it still works afterwards.
    CHECK(b.core1.write(am::core_sram_base(kChip, 0), 4, delay)
          == tlm::TLM_OK_RESPONSE);
    CHECK(b.fabric.local_bypass() == 1);
}

void check_configuration_refusals()
{
    bool threw = false;
    try {
        chip_aperture_of(static_cast<tpu::chip_id_t>(tpu::max_chips));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "a chip index past the FlooNoC manager-id limit was "
                     "accepted");

    threw = false;
    try {
        auto spec = chip_aperture_of(kChip);
        spec.control_base = spec.core_base[0];
        chip_local_fabric bad("bad_overlap", spec);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "overlapping chip windows were accepted; there is no "
                     "correct answer for which one should answer");

    threw = false;
    try {
        auto spec = chip_aperture_of(kChip);
        spec.counters_base = am::global_ram_base;
        chip_local_fabric bad("bad_outside", spec);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "a window outside the chip aperture was accepted");

    threw = false;
    try {
        chip_local_fabric bad("bad_cycle", chip_aperture_of(kChip),
                              chip_fabric_timing::annotated,
                              sc_core::SC_ZERO_TIME);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "a zero fabric clock period was accepted; it makes every "
                     "port free at every instant");
}

} // namespace

int sc_main(int, char*[])
{
    check_decode();
    check_configuration_refusals();
    check_routing();
    check_debug_transport();
    check_annotated_contention();
    check_reset();

    // Last, because it is the only one that runs the scheduler.
    check_arbitrated_fairness();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_chip_local_fabric: all checks passed\n";
    return 0;
}
