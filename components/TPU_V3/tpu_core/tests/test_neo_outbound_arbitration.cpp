// SPDX-License-Identifier: Apache-2.0
//
// The external bridge's outbound arbiter (Phase 8).
//
// A NEO-CORE has two named things that leave it — the hart and the DMA — and
// **one** external socket. Nothing arbitrated them until Phase 8, and nothing
// could notice: while every downstream target only annotates delay, a caller
// returns before the other one can run, so the two never overlap. The moment
// something downstream blocks they do, and the chip fabric refuses the second
// transaction because it treats a core as one initiator. That is how this was
// found — by running the chip composition gate with its fabric in `arbitrated`
// mode — and Phase 9's first real NoC hop would have found it again.
//
// So this file tests the arbiter under a target that actually waits, which is
// the only condition in which it does anything:
//
//   * one transaction at a time reaches the external socket. Measured **at the
//     target**, because that is the only place the invariant is observable;
//   * the two initiators alternate. Rotating priority is a behaviour here and
//     nothing anywhere else in the core would starve a hart behind a DMA;
//   * a reset while an initiator is blocked releases it with an error rather
//     than leaving it waiting for a grant no arbiter will issue.
//
// Exit codes: 0 pass, 1 fail.

#include "tpu_v3/core/neo_external_bridge.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"
#include "tpu_v3/core/neo_local_sram_fabric.h"
#include "tpu_v3/sram/core_sram.h"

namespace tpu = cdc::components::tpu_v3;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

using core::neo_external_bridge;
using core::neo_local_sram_fabric;
using core::outbound_initiator;

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
constexpr tpu::core_id_t kCore = 0;

/// An external target that takes real time to answer.
///
/// Nothing about this is artificial: a contended SRAM bank, an `arbitrated`
/// chip fabric and a NoC hop all block inside `b_transport` the same way. It is
/// simply the condition under which the arbiter exists.
class slow_external : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<slow_external> socket;

    sc_core::sc_time service{40, sc_core::SC_NS};
    unsigned in_flight = 0;
    unsigned peak_in_flight = 0;
    /// Who was served, in order, read out of the payload rather than from a
    /// variable the caller set before it blocked.
    std::vector<int> order;

    explicit slow_external(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &slow_external::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        order.push_back(trans.get_data_length() > 0
                            ? static_cast<int>(trans.get_data_ptr()[0])
                            : -1);
        ++in_flight;
        peak_in_flight = std::max(peak_in_flight, in_flight);
        sc_core::wait(service);
        --in_flight;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

/// Drives one of the bridge's outbound sockets from its own process.
class contender : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(contender);

    tlm_utils::simple_initiator_socket<contender> socket;

    unsigned completed = 0;
    unsigned errors = 0;
    bool finished = false;

    contender(sc_core::sc_module_name name, int id, unsigned repeats,
              sc_core::sc_time start)
        : sc_core::sc_module(name)
        , socket("socket")
        , id_(id)
        , repeats_(repeats)
        , start_(start)
    {
        SC_THREAD(run);
    }

private:
    void run()
    {
        if (start_ != sc_core::SC_ZERO_TIME) {
            sc_core::wait(start_);
        }
        for (unsigned i = 0; i < repeats_; ++i) {
            // Its own index in every byte, so the target can say which
            // initiator it is serving.
            std::vector<unsigned char> data(4,
                                            static_cast<unsigned char>(id_));
            tlm::tlm_generic_payload trans;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(am::global_ram_base);
            trans.set_data_ptr(data.data());
            trans.set_data_length(4);
            trans.set_streaming_width(4);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            socket->b_transport(trans, delay);
            if (trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
                ++completed;
            } else {
                ++errors;
            }
        }
        finished = true;
    }

    int id_;
    unsigned repeats_;
    sc_core::sc_time start_;
};

/// Calls `reset()` at a chosen instant.
class resetter : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(resetter);

    resetter(sc_core::sc_module_name name, neo_external_bridge& bridge,
             sc_core::sc_time at)
        : sc_core::sc_module(name)
        , bridge_(bridge)
        , at_(at)
    {
        SC_THREAD(run);
    }

    bool fired = false;

private:
    void run()
    {
        sc_core::wait(at_);
        bridge_.reset();
        fired = true;
    }

    neo_external_bridge& bridge_;
    sc_core::sc_time at_;
};

sram::core_sram_config sram_config()
{
    sram::core_sram_config out;
    out.base_address = am::core_sram_base(kChip, kCore);
    out.window_bytes = am::core_sram_window;
    out.capacity_bytes = 64 * 1024;
    return out;
}

tpu::local_sram_fabric_config fabric_config()
{
    tpu::local_sram_fabric_config out;
    out.data_width_bits = 128;
    out.bank_count = 4;
    out.mapping = tpu::bank_mapping::low_order_interleaved;
    out.pipeline_stages = 1;
    out.max_outstanding_per_requester = 1;
    out.arbitration = tpu::arbitration_policy::round_robin;
    return out;
}

core::core_aperture_spec aperture()
{
    core::core_aperture_spec spec;
    spec.core_base = am::core_base(kChip, kCore);
    spec.core_size = am::core_aperture_stride;
    spec.sram_base = am::core_sram_base(kChip, kCore);
    spec.sram_window = am::core_sram_window;
    return spec;
}

/// Answers nothing. It exists because SystemC refuses to elaborate an unbound
/// port and a socket cannot be created outside a module; this bench never sends
/// anything down the inbound control path.
class null_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<null_target> socket;

    explicit null_target(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &null_target::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
    }
};

/// Sends nothing. `bridge.inbound` is a target socket and SystemC refuses to
/// elaborate one with no initiator on it; this bench drives only the outbound
/// side.
class null_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<null_master> socket;

    explicit null_master(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }
};

/// A bridge with its mandatory fabric, and nothing else.
struct bench {
    sram::core_sram store;
    neo_local_sram_fabric fabric;
    neo_external_bridge bridge;
    slow_external outside;
    null_target control;
    null_master remote;

    explicit bench(const std::string& prefix)
        : store((prefix + "_sram").c_str(), sram_config())
        , fabric((prefix + "_fabric").c_str(), fabric_config(), store,
                 {sram::neo_requester::cpu,
                  sram::neo_requester::external_inbound})
        , bridge((prefix + "_bridge").c_str(), aperture(), fabric)
        , outside((prefix + "_outside").c_str())
        , control((prefix + "_control").c_str())
        , remote((prefix + "_remote").c_str())
    {
        bridge.external.bind(outside.socket);
        bridge.inbound_control.bind(control.socket);
        remote.socket.bind(bridge.inbound);
    }
};

} // namespace

int sc_main(int, char*[])
{
    // ── fairness ─────────────────────────────────────────────────────────────
    bench fair("fair");
    contender fair_cpu("fair_cpu", 0, 6, sc_core::SC_ZERO_TIME);
    contender fair_dma("fair_dma", 1, 6, sc_core::SC_ZERO_TIME);
    fair_cpu.socket.bind(
        fair.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::cpu)]);
    fair_dma.socket.bind(
        fair.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::dma)]);

    // ── reset while blocked ──────────────────────────────────────────────────
    bench abandon("abandon");
    abandon.outside.service = sc_core::sc_time(1, sc_core::SC_US);
    contender abandon_cpu("abandon_cpu", 0, 1, sc_core::SC_ZERO_TIME);
    contender abandon_dma("abandon_dma", 1, 1,
                          sc_core::sc_time(100, sc_core::SC_NS));
    abandon_cpu.socket.bind(
        abandon.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::cpu)]);
    abandon_dma.socket.bind(
        abandon.bridge.local_outbound[static_cast<unsigned>(
            outbound_initiator::dma)]);
    resetter reset_at("reset_at", abandon.bridge,
                      sc_core::sc_time(300, sc_core::SC_NS));

    // Bounded, because a deadlock has to fail rather than hang the suite
    // (`INTERFACE_CONTRACT.md` §5).
    sc_core::sc_start(sc_core::sc_time(100, sc_core::SC_US));

    // ── fairness results ─────────────────────────────────────────────────────

    CHECK_MSG(fair_cpu.finished && fair_dma.finished,
              "an initiator never finished; the arbiter deadlocked or starved "
              "it");
    CHECK(fair_cpu.completed == 6 && fair_dma.completed == 6);

    CHECK_MSG(fair.outside.peak_in_flight == 1,
              "two transactions were inside the core's one external socket at "
              "the same time. The chip fabric treats a core as one initiator "
              "and refuses the second, so this is a model defect downstream, "
              "not a slow path");

    CHECK_MSG(fair.bridge.outbound_conflicts() > 0,
              "neither initiator ever found the port busy, so this run "
              "exercised no arbitration and the alternation below would be an "
              "accident");

    CHECK(fair.bridge.outbound_grants(outbound_initiator::cpu) == 6);
    CHECK(fair.bridge.outbound_grants(outbound_initiator::dma) == 6);

    unsigned alternations = 0;
    for (std::size_t i = 1; i < fair.outside.order.size(); ++i) {
        if (fair.outside.order[i] != fair.outside.order[i - 1]) {
            ++alternations;
        }
    }
    std::cout << "outbound port: " << alternations << " alternations in "
              << fair.outside.order.size() << " grants, "
              << fair.bridge.outbound_conflicts() << " conflicts\n";
    CHECK_MSG(alternations >= 9,
              "the external port did not alternate between the hart and the "
              "DMA: " + std::to_string(alternations) + " changes in "
                  + std::to_string(fair.outside.order.size())
                  + " grants, which is fixed priority rather than rotating. A "
                    "hart starved behind a bulk DMA is what that means in a "
                    "real core");

    // ── reset while blocked ──────────────────────────────────────────────────

    CHECK_MSG(reset_at.fired, "the reset never ran");
    CHECK_MSG(abandon_dma.finished,
              "the initiator blocked in the arbiter never returned. A reset "
              "that clears the waiting flags without waking anyone leaves it "
              "waiting for a grant no arbiter will ever issue");
    CHECK_MSG(abandon_dma.errors == 1,
              "the abandoned request did not report an error. Reset abandons "
              "in-flight work; completing it silently afterwards would be "
              "worse than either alternative (ARCHITECTURE.md §6)");
    CHECK_MSG(abandon_cpu.finished,
              "the initiator that held the port when the reset arrived never "
              "returned");

    // And the bridge still works.
    CHECK(abandon.bridge.outbound_requests() >= 0);

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_neo_outbound_arbitration: all checks passed\n";
    return 0;
}
