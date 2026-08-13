// SPDX-License-Identifier: Apache-2.0
//
// NEO external bridge tests — the inbound/outbound half of the Phase 3 gate.
//
// Two gate lines are proved here.
//
// "Inbound remote SRAM/MMIO traffic traverses the adapters and normal
// arbitration": inbound SRAM accesses appear in the fabric's
// `external_inbound` counters, contend for banks against a local requester,
// and — the part that makes it a proof rather than an observation — the bytes
// the SRAM recorded reconcile exactly with the bytes the fabric carried. A
// path that reached storage another way would show up as SRAM traffic with no
// fabric traffic behind it. `core_sram` exposes no pointer into its backing
// store, so there is no such path to find; the conservation check is what
// keeps that true as the tree grows.
//
// "An attempted bypass is covered by a negative test": an outbound access
// naming an address inside this core is refused and counted, and the external
// stub never sees it. Local containment is a decode consequence, not a routing
// decision (plan §9.2), and it is also what makes the FlooNoC NoLoopback
// constraint survivable — traffic that leaked into the mesh would either
// deadlock or, worse, work.

// `sc_spawn` is how the contention scenarios get one process per requester
// without hard-coding how many there are. It is a dynamic process, so the
// macro has to be defined before <systemc> is reached.
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "tpu_v3/core/core_registers.h"
#include "tpu_v3/core/neo_control_fabric.h"
#include "tpu_v3/core/neo_external_bridge.h"
#include "tpu_v3/core/neo_local_sram_fabric.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "tpu_v3/address_map.h"

namespace tpu = cdc::components::tpu_v3;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

using core::control_initiator;
using core::local_fabric_timing;
using core::mmio_register_file;
using core::neo_control_fabric;
using core::neo_external_bridge;
using core::neo_local_sram_fabric;
using core::register_block;
using sram::neo_command;
using sram::neo_local_request;
using sram::neo_local_response;
using sram::neo_requester;
using sram::neo_status;

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
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__ << ':' \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr tpu::chip_id_t kChip = 0;
constexpr tpu::core_id_t kCore = 0;
constexpr std::uint64_t kCapacity = 64 * 1024;

sram::core_sram_config sram_config()
{
    sram::core_sram_config config;
    config.base_address = am::core_sram_base(kChip, kCore);
    config.window_bytes = am::core_sram_window;
    config.capacity_bytes = kCapacity;
    return config;
}

tpu::local_sram_fabric_config fabric_config()
{
    tpu::local_sram_fabric_config config;
    config.data_width_bits = 128;
    config.bank_count = 4;
    config.mapping = tpu::bank_mapping::low_order_interleaved;
    config.pipeline_stages = 1;
    config.max_outstanding_per_requester = 1;
    config.arbitration = tpu::arbitration_policy::round_robin;
    return config;
}

/// Stands in for the chip-local fabric or NoC endpoint on the far side of the
/// bridge. It records rather than models: what matters is whether a
/// transaction arrived at all.
class external_stub : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<external_stub> socket;
    std::uint64_t seen = 0;
    std::uint64_t debug_seen = 0;
    std::uint64_t last_address = 0;

    explicit external_stub(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &external_stub::b_transport);
        socket.register_transport_dbg(this, &external_stub::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        ++seen;
        last_address = trans.get_address();
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        ++debug_seen;
        last_address = trans.get_address();
        return trans.get_data_length();
    }
};

class tlm_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<tlm_master> socket;

    explicit tlm_master(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status access(tlm::tlm_command command,
                                    std::uint64_t address, unsigned char* data,
                                    unsigned int length,
                                    unsigned char* byte_enable = nullptr,
                                    unsigned int byte_enable_length = 0,
                                    unsigned int streaming_width = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_byte_enable_ptr(byte_enable);
        trans.set_byte_enable_length(byte_enable_length);
        trans.set_streaming_width(streaming_width == 0 ? length
                                                       : streaming_width);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        CHECK_MSG(trans.get_response_status() != tlm::TLM_INCOMPLETE_RESPONSE,
                  "a path returned without setting a response status");
        return trans.get_response_status();
    }

    unsigned int debug(tlm::tlm_command command, std::uint64_t address,
                       unsigned char* data, unsigned int length,
                       unsigned char* byte_enable = nullptr,
                       unsigned int byte_enable_length = 0,
                       unsigned int streaming_width = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data);
        trans.set_data_length(length);
        trans.set_byte_enable_ptr(byte_enable);
        trans.set_byte_enable_length(byte_enable_length);
        trans.set_streaming_width(streaming_width == 0 ? length
                                                       : streaming_width);
        return socket->transport_dbg(trans);
    }
};

/// Drives the two halves of the contention scenario: one local requester
/// straight onto the native port, one remote master through the bridge, both
/// aimed at the same bank.
class contention_driver : public sc_core::sc_module {
public:
    contention_driver(sc_core::sc_module_name name,
                      neo_local_sram_fabric& fabric, tlm_master& remote,
                      std::uint64_t local_address,
                      std::uint64_t remote_address, unsigned iterations)
        : sc_core::sc_module(name)
        , fabric_(fabric)
        , remote_(remote)
        , local_address_(local_address)
        , remote_address_(remote_address)
        , iterations_(iterations)
    {
        sc_core::sc_spawn([this] { local(); });
        sc_core::sc_spawn([this] { inbound(); });
    }

    unsigned local_done() const noexcept { return local_done_; }
    unsigned inbound_done() const noexcept { return inbound_done_; }
    unsigned errors() const noexcept { return errors_; }

private:
    void local()
    {
        std::array<unsigned char, 16> pattern{};
        for (unsigned i = 0; i < iterations_; ++i) {
            pattern.fill(static_cast<unsigned char>(0x10 + (i & 0x0F)));

            neo_local_request request;
            request.requester = neo_requester::cpu;
            request.command = neo_command::write;
            request.address = local_address_;
            request.size = static_cast<std::uint32_t>(pattern.size());
            request.data = pattern.data();
            neo_local_response response;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            fabric_.b_access(request, response, delay);
            if (response.status != neo_status::ok) {
                ++errors_;
                return;
            }
            ++local_done_;
        }
    }

    void inbound()
    {
        std::array<unsigned char, 16> pattern{};
        for (unsigned i = 0; i < iterations_; ++i) {
            pattern.fill(static_cast<unsigned char>(0xA0 + (i & 0x0F)));
            if (remote_.access(tlm::TLM_WRITE_COMMAND, remote_address_,
                               pattern.data(),
                               static_cast<unsigned int>(pattern.size()))
                != tlm::TLM_OK_RESPONSE) {
                ++errors_;
                return;
            }
            ++inbound_done_;
        }
    }

    neo_local_sram_fabric& fabric_;
    tlm_master& remote_;
    std::uint64_t local_address_;
    std::uint64_t remote_address_;
    unsigned iterations_;
    unsigned local_done_ = 0;
    unsigned inbound_done_ = 0;
    unsigned errors_ = 0;
};

/// Runs the sequential half of the test *inside* the simulation.
///
/// `sc_main` is not a SystemC process, so it cannot call anything that waits —
/// and an `arbitrated` fabric waits, which is the whole point of using one
/// here. The sequential checks therefore run in a spawned thread that first
/// waits for the contention scenario to finish, so the counters it inspects
/// are the contention scenario's and not a mixture.
class deferred_checks : public sc_core::sc_module {
public:
    deferred_checks(sc_core::sc_module_name name, std::function<bool()> ready,
                    std::function<void()> body)
        : sc_core::sc_module(name)
        , ready_(std::move(ready))
        , body_(std::move(body))
    {
        sc_core::sc_spawn([this] { run(); });
    }

    bool ran() const noexcept { return ran_; }

private:
    void run()
    {
        while (!ready_()) {
            sc_core::wait(10, sc_core::SC_NS);
        }
        body_();
        ran_ = true;
    }

    std::function<bool()> ready_;
    std::function<void()> body_;
    bool ran_ = false;
};

/// The watchdog. A concurrency test without one is not a test: a deadlock must
/// fail, not hang the suite (`INTERFACE_CONTRACT.md` §5).
///
/// It is a bounded `sc_start` rather than a process that fires an alarm. A
/// process would have to schedule an event at the limit, which keeps the
/// simulation alive to the limit and makes the alarm fire on every run,
/// including the successful ones — the first version of this file did exactly
/// that. Stopping the kernel at the limit and then asking each bench whether
/// it finished distinguishes "done early" from "still stuck" without adding an
/// event of its own.
constexpr int kWatchdogMilliseconds = 1;

} // namespace

int sc_main(int, char*[])
{
    const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

    sram::core_sram store("core_sram", sram_config());
    neo_local_sram_fabric fabric("local_fabric", fabric_config(), store,
                                 {neo_requester::cpu,
                                  neo_requester::external_inbound},
                                 local_fabric_timing::arbitrated,
                                 sc_core::sc_time(1, sc_core::SC_NS));

    std::vector<core::control_target_spec> specs{
        {am::core_control(kChip, kCore), am::core_control_size, "core"},
        {am::sa_control(kChip, kCore), am::sa_control_size, "sa"},
        {am::dma_control(kChip, kCore), am::dma_control_size, "dma"},
        {am::transform_control(kChip, kCore), am::transform_control_size,
         "transform"},
        {am::core_counters(kChip, kCore), am::core_counters_size, "counters"},
    };
    neo_control_fabric control("control_fabric", specs);

    mmio_register_file core_regs("core_regs", register_block::core,
                                 specs[0].base, specs[0].size);
    mmio_register_file sa_regs("sa_regs", register_block::sa, specs[1].base,
                               specs[1].size);
    mmio_register_file dma_regs("dma_regs", register_block::dma, specs[2].base,
                                specs[2].size);
    mmio_register_file transform_regs("transform_regs",
                                      register_block::transform, specs[3].base,
                                      specs[3].size);
    mmio_register_file counter_regs("counter_regs", register_block::counters,
                                    specs[4].base, specs[4].size);
    control.to[0].bind(core_regs.socket);
    control.to[1].bind(sa_regs.socket);
    control.to[2].bind(dma_regs.socket);
    control.to[3].bind(transform_regs.socket);
    control.to[4].bind(counter_regs.socket);

    core::core_aperture_spec aperture;
    aperture.core_base = am::core_base(kChip, kCore);
    aperture.core_size = am::core_aperture_stride;
    aperture.sram_base = sram_base;
    aperture.sram_window = am::core_sram_window;

    neo_external_bridge bridge("external_bridge", aperture, fabric);
    bridge.inbound_control.bind(
        control.from[static_cast<unsigned>(control_initiator::external_inbound)]);

    external_stub outside("outside");
    bridge.external.bind(outside.socket);

    // The control fabric's local port needs an initiator too. SystemC refuses
    // to elaborate an unbound port, and a local master is what the core's own
    // register accesses will be from Phase 7 anyway.
    tlm_master core_master("core_master");
    core_master.socket.bind(
        control.from[static_cast<unsigned>(control_initiator::cpu)]);

    tlm_master remote("remote");
    remote.socket.bind(bridge.inbound);
    tlm_master local("local");
    local.socket.bind(bridge.local_outbound);

    contention_driver driver("driver", fabric, remote, sram_base + 4096,
                             sram_base + 4096 + 64, 40);

    deferred_checks checks(
        "checks",
        [&] {
            return driver.local_done() == 40 && driver.inbound_done() == 40;
        },
        [&] {
        // ── inbound SRAM traffic really went through the fabric ──────────────────
        CHECK(driver.errors() == 0);
        CHECK(driver.local_done() == 40);
        CHECK(driver.inbound_done() == 40);
        CHECK(bridge.inbound_sram_requests() == 40);
        CHECK(bridge.inbound_sram_bytes() == 40 * 16);

        const auto& inbound_counters = fabric.counters(neo_requester::external_inbound);
        CHECK_MSG(inbound_counters.request_count == 40,
                  "every inbound SRAM access must appear as an 'external_inbound' "
                  "request on the local fabric");
        CHECK(inbound_counters.transferred_bytes == 40 * 16);

        // ...and was arbitrated against the local requester rather than slipping
        // past it. Both aim at bank 0, so somebody had to wait.
        CHECK_MSG(fabric.bank_grants(0, neo_requester::external_inbound) == 40,
                  "inbound beats must be granted by the bank arbiter");
        CHECK_MSG(fabric.total_bank_conflicts() > 0,
                  "a local requester and inbound remote traffic on the same bank "
                  "must contend; zero conflicts would mean one of them bypassed "
                  "arbitration");

        // ── conservation: nothing reached storage except through the fabric ──────
        //
        // The negative control for the bypass prohibition. `core_sram` hands out
        // no backing pointer, so there is no second path today; this is what would
        // notice if one appeared.
        CHECK_MSG(store.bytes_written() + store.bytes_read() == fabric.total_bytes(),
                  "the SRAM moved bytes the fabric did not carry, which means "
                  "something reached the backing store without being arbitrated "
                  "or attributed");
        CHECK_MSG(store.write_accesses() + store.read_accesses()
                      == fabric.total_beats(),
                  "the storage sees beats, the fabric sees requests, and the two "
                  "counts must reconcile beat-for-beat (decision record D7)");
        CHECK(fabric.total_requests() == 80);
        CHECK_MSG(fabric.total_beats() == 80,
                  "16-byte accesses on a 16-byte beat are one beat each");

        // ── inbound MMIO reaches the control plane ───────────────────────────────
        std::uint32_t token = 0xFEEDFACEu;
        CHECK(remote.access(tlm::TLM_WRITE_COMMAND,
                            specs[2].base + mmio_register_file::reg_scratch,
                            reinterpret_cast<unsigned char*>(&token), 4)
              == tlm::TLM_OK_RESPONSE);
        CHECK_MSG(dma_regs.scratch() == 0xFEEDFACEu,
                  "inbound MMIO must land in the addressed register file");
        CHECK(bridge.inbound_mmio_requests() == 1);
        CHECK(control.requests(control_initiator::external_inbound) == 1);

        // A remote master gets exactly the refusals the local core would: the
        // bridge does not pre-judge the AXI4-Lite rules on its behalf.
        std::array<unsigned char, 4> four{};
        CHECK(remote.access(tlm::TLM_WRITE_COMMAND,
                            specs[2].base + mmio_register_file::reg_scratch + 1,
                            four.data(), 4)
              == tlm::TLM_BURST_ERROR_RESPONSE);

        // ── inbound refusals ─────────────────────────────────────────────────────
        std::array<unsigned char, 64> buffer{};
        buffer.fill(0x77);

        // Inside the SRAM window, above the instantiated capacity. The address
        // decoded and the target refused it, which is a generic error and not an
        // address error: the map is right and the capacity is too small.
        CHECK(remote.access(tlm::TLM_READ_COMMAND, sram_base + kCapacity,
                            buffer.data(), 4)
              == tlm::TLM_GENERIC_ERROR_RESPONSE);
        for (auto byte : buffer) {
            CHECK_MSG(byte == 0x77,
                      "a refused inbound read must leave the caller's buffer "
                      "untouched");
        }

        // Bigger than any single access can need. The chip endpoint chunks
        // oversized transfers before they reach a core; one arriving here means
        // that did not happen, so it is refused rather than silently split.
        CHECK(remote.access(tlm::TLM_WRITE_COMMAND, sram_base, buffer.data(), 65)
              == tlm::TLM_BURST_ERROR_RESPONSE);

        // Neither SRAM nor MMIO in this core.
        CHECK(remote.access(tlm::TLM_READ_COMMAND, 0x8000'0000ull, buffer.data(), 4)
              == tlm::TLM_ADDRESS_ERROR_RESPONSE);

        // A 64-byte inbound SRAM access is legal, unlike the same payload on the
        // control plane: this is the memory-like target class.
        CHECK(remote.access(tlm::TLM_WRITE_COMMAND, sram_base + 8192,
                            buffer.data(), 64)
              == tlm::TLM_OK_RESPONSE);

        // ── TLM's repeating byte-enable pattern ──────────────────────────────────
        //
        // `byte_enable_length` may be shorter than `data_length`, in which
        // case the pattern repeats. The native plane has no such rule, so the
        // bridge has to expand it; forwarding the raw pointer reads past the
        // end of the caller's array, which is both a wrong result and an
        // overread. Two lengths and two payload sizes, because the bug only
        // shows when `data_length > byte_enable_length`.
        for (unsigned int pattern_length : {1u, 2u}) {
            for (unsigned int payload : {8u, 64u}) {
                const std::uint64_t base = sram_base + 16384;

                // Start from a known state through a fully enabled write.
                std::vector<unsigned char> zeros(payload, 0x00);
                CHECK(remote.access(tlm::TLM_WRITE_COMMAND, base, zeros.data(),
                                    payload)
                      == tlm::TLM_OK_RESPONSE);

                std::vector<unsigned char> payload_bytes(payload);
                for (unsigned int i = 0; i < payload; ++i) {
                    payload_bytes[i] = static_cast<unsigned char>(0x80 + i);
                }
                // `{0xff}` enables everything; `{0xff, 0x00}` enables every
                // other byte.
                std::vector<unsigned char> pattern(pattern_length, 0xFF);
                if (pattern_length == 2) {
                    pattern[1] = 0x00;
                }
                CHECK(remote.access(tlm::TLM_WRITE_COMMAND, base,
                                    payload_bytes.data(), payload,
                                    pattern.data(), pattern_length)
                      == tlm::TLM_OK_RESPONSE);

                std::vector<unsigned char> back(payload, 0xEE);
                CHECK(remote.access(tlm::TLM_READ_COMMAND, base, back.data(),
                                    payload)
                      == tlm::TLM_OK_RESPONSE);
                for (unsigned int i = 0; i < payload; ++i) {
                    const bool enabled = pattern[i % pattern_length] != 0;
                    const unsigned char want =
                        enabled ? payload_bytes[i] : 0x00;
                    CHECK_MSG(back[i] == want,
                              "repeating byte-enable pattern of length "
                                  + std::to_string(pattern_length)
                                  + " on a " + std::to_string(payload)
                                  + "-byte payload got byte "
                                  + std::to_string(i) + " wrong");
                }
            }
        }

        // The same expansion on the debug path, which had the identical bug.
        {
            const std::uint64_t base = sram_base + 20480;
            std::vector<unsigned char> payload_bytes(64, 0x5A);
            std::vector<unsigned char> pattern{0xFF, 0x00};
            CHECK(remote.debug(tlm::TLM_WRITE_COMMAND, base,
                               payload_bytes.data(), 64, pattern.data(), 2)
                  == 64);
            std::vector<unsigned char> back(64, 0xEE);
            CHECK(remote.debug(tlm::TLM_READ_COMMAND, base, back.data(), 64)
                  == 64);
            for (unsigned int i = 0; i < 64; ++i) {
                CHECK(back[i] == ((i % 2 == 0) ? 0x5A : 0x00));
            }
            // A command that is neither read nor write has no debug meaning
            // and must not be served as a read that overwrites the buffer.
            std::vector<unsigned char> guard_bytes(4, 0xC7);
            CHECK(remote.debug(tlm::TLM_IGNORE_COMMAND, base,
                               guard_bytes.data(), 4)
                  == 0);
            for (auto byte : guard_bytes) {
                CHECK(byte == 0xC7);
            }

            // Debug bypasses timing, not protocol validation. Neither a
            // wrapped stream nor a malformed byte-enable pattern may reach
            // the native SRAM path or mutate storage.
            std::vector<unsigned char> before(8, 0x31);
            CHECK(remote.debug(tlm::TLM_WRITE_COMMAND, base + 128,
                               before.data(), 8)
                  == 8);
            std::vector<unsigned char> rejected(8, 0xA5);
            CHECK(remote.debug(tlm::TLM_WRITE_COMMAND, base + 128,
                               rejected.data(), 8, nullptr, 0, 4)
                  == 0);
            std::array<unsigned char, 1> malformed_enable{0xFF};
            CHECK(remote.debug(tlm::TLM_WRITE_COMMAND, base + 128,
                               rejected.data(), 8,
                               malformed_enable.data(), 0)
                  == 0);
            std::vector<unsigned char> after(8, 0);
            CHECK(remote.debug(tlm::TLM_READ_COMMAND, base + 128,
                               after.data(), 8)
                  == 8);
            CHECK(after == before);
        }

        // A fully masked write reaches the SRAM, changes nothing, and is
        // reported as having transferred nothing.
        {
            const std::uint64_t base = sram_base + 24576;
            std::vector<unsigned char> ones(8, 0xFF);
            CHECK(remote.access(tlm::TLM_WRITE_COMMAND, base, ones.data(), 8)
                  == tlm::TLM_OK_RESPONSE);

            const std::uint64_t bytes_before = bridge.inbound_sram_bytes();
            std::vector<unsigned char> data(8, 0x11);
            std::vector<unsigned char> none(8, 0x00);
            CHECK(remote.access(tlm::TLM_WRITE_COMMAND, base, data.data(), 8,
                                none.data(), 8)
                  == tlm::TLM_OK_RESPONSE);
            CHECK_MSG(bridge.inbound_sram_bytes() == bytes_before,
                      "a fully masked write transferred no bytes and must not "
                      "be counted as if it had");

            std::vector<unsigned char> back(8, 0);
            CHECK(remote.access(tlm::TLM_READ_COMMAND, base, back.data(), 8)
                  == tlm::TLM_OK_RESPONSE);
            for (auto byte : back) {
                CHECK_MSG(byte == 0xFF,
                          "a fully masked write modified storage");
            }
        }

        // A byte-enable pointer with a zero-length pattern describes no bytes
        // and cannot be repeated into anything.
        {
            std::array<unsigned char, 4> data{};
            std::array<unsigned char, 4> pattern{0xFF, 0xFF, 0xFF, 0xFF};
            CHECK(remote.access(tlm::TLM_WRITE_COMMAND, sram_base + 28672,
                                data.data(), 4, pattern.data(), 0)
                  == tlm::TLM_BURST_ERROR_RESPONSE);
        }

        // ── a wrapped streaming transfer is refused, not ignored ─────────────
        {
            std::array<unsigned char, 16> data{};
            CHECK_MSG(remote.access(tlm::TLM_WRITE_COMMAND, sram_base + 28672,
                                    data.data(), 16, nullptr, 0, 4)
                          == tlm::TLM_BURST_ERROR_RESPONSE,
                      "a streaming width narrower than the payload repeats it "
                      "over a window nothing here implements; it must be "
                      "refused rather than served as if the field were absent");
            // A streaming width equal to or wider than the payload is the
            // ordinary non-streaming case and stays legal.
            CHECK(remote.access(tlm::TLM_WRITE_COMMAND, sram_base + 28672,
                                data.data(), 16, nullptr, 0, 16)
                  == tlm::TLM_OK_RESPONSE);
            CHECK(remote.access(tlm::TLM_WRITE_COMMAND, sram_base + 28672,
                                data.data(), 16, nullptr, 0, 64)
                  == tlm::TLM_OK_RESPONSE);
        }

        // ── a transfer straddling two regions decodes to neither ─────────────
        //
        // There is no correct answer for which target should answer, so it is
        // refused rather than routed by whichever range test ran first.
        {
            std::array<unsigned char, 8> data{};
            const std::uint64_t sram_top =
                sram_base + am::core_sram_window;
            CHECK_MSG(remote.access(tlm::TLM_READ_COMMAND, sram_top - 4,
                                    data.data(), 8)
                          == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                      "a transfer straddling the SRAM/MMIO boundary must be "
                      "refused");
            CHECK(remote.debug(tlm::TLM_READ_COMMAND, sram_top - 4,
                               data.data(), 8)
                  == 0);
        }

        // ── outbound: local addresses must never enter the mesh ──────────────────
        const std::uint64_t forwarded_before = outside.seen;
        CHECK(local.access(tlm::TLM_WRITE_COMMAND, 0x8000'1000ull, four.data(), 4)
              == tlm::TLM_OK_RESPONSE);
        CHECK(outside.seen == forwarded_before + 1);
        CHECK(outside.last_address == 0x8000'1000ull);
        CHECK(bridge.outbound_requests() == 1);

        // Every outbound entry point enforces the bridge's payload contract
        // before forwarding. Refusal must be local: the far-side target and
        // the forwarded-request counter see none of these malformed payloads.
        const std::uint64_t seen_before_protocol_errors = outside.seen;
        const std::uint64_t requests_before_protocol_errors =
            bridge.outbound_requests();
        std::array<unsigned char, 8> protocol_data{};
        std::array<unsigned char, 1> malformed_enable{0xFF};
        CHECK(local.access(tlm::TLM_IGNORE_COMMAND, 0x8000'1100ull,
                           protocol_data.data(), 8)
              == tlm::TLM_COMMAND_ERROR_RESPONSE);
        CHECK(local.access(tlm::TLM_WRITE_COMMAND, 0x8000'1100ull,
                           protocol_data.data(), 8, nullptr, 0, 4)
              == tlm::TLM_BURST_ERROR_RESPONSE);
        CHECK(local.access(tlm::TLM_WRITE_COMMAND, 0x8000'1100ull,
                           protocol_data.data(), 8,
                           malformed_enable.data(), 0)
              == tlm::TLM_BURST_ERROR_RESPONSE);
        CHECK(outside.seen == seen_before_protocol_errors);
        CHECK(bridge.outbound_requests() == requests_before_protocol_errors);

        const std::uint64_t debug_before_protocol_errors = outside.debug_seen;
        CHECK(local.debug(tlm::TLM_IGNORE_COMMAND, 0x8000'1200ull,
                          protocol_data.data(), 8)
              == 0);
        CHECK(local.debug(tlm::TLM_WRITE_COMMAND, 0x8000'1200ull,
                          protocol_data.data(), 8, nullptr, 0, 4)
              == 0);
        CHECK(local.debug(tlm::TLM_WRITE_COMMAND, 0x8000'1200ull,
                          protocol_data.data(), 8,
                          malformed_enable.data(), 0)
              == 0);
        CHECK(outside.debug_seen == debug_before_protocol_errors);

        // Positive control for the debug forwarding path: the three zeroes
        // above mean validation stopped them, not that debug was never bound.
        CHECK(local.debug(tlm::TLM_READ_COMMAND, 0x8000'1200ull,
                          protocol_data.data(), 8)
              == 8);
        CHECK(outside.debug_seen == debug_before_protocol_errors + 1);

        // The negative control. An address inside this core arriving on the
        // outbound port means the core-local decoder upstream is wrong; handing it
        // to the interconnect would either deadlock against NoLoopback or work and
        // hide the bug.
        const std::uint64_t before_leak = outside.seen;
        CHECK_MSG(local.access(tlm::TLM_WRITE_COMMAND, sram_base + 16, four.data(),
                               4)
                      == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  "an outbound access naming this core's own SRAM must be "
                  "refused");
        CHECK(local.access(tlm::TLM_READ_COMMAND, specs[1].base, four.data(), 4)
              == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        CHECK_MSG(outside.seen == before_leak,
                  "local traffic reached the external port; local containment is "
                  "a decode consequence, not a routing decision (plan §9.2)");
        CHECK(bridge.outbound_local_refused() == 2);

        // Both edges of the aperture, where containment and overlap disagree.
        // A transfer that begins inside this core and runs past its aperture
        // is not *contained* by it, and the first version of the check
        // forwarded exactly that — one byte of local traffic in the mesh is
        // still local traffic in the mesh.
        const std::uint64_t core_base = am::core_base(kChip, kCore);
        const std::uint64_t core_top = core_base + am::core_aperture_stride;
        std::array<unsigned char, 8> straddle{};
        const std::uint64_t seen_before_edges = outside.seen;

        CHECK_MSG(local.access(tlm::TLM_WRITE_COMMAND, core_top - 4,
                               straddle.data(), 8)
                      == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  "a transfer starting inside this core and ending outside it "
                  "must not be forwarded");
        CHECK_MSG(local.access(tlm::TLM_WRITE_COMMAND, core_base - 4,
                               straddle.data(), 8)
                      == tlm::TLM_ADDRESS_ERROR_RESPONSE,
                  "a transfer starting outside this core and overlapping into "
                  "it must not be forwarded either");
        CHECK(local.debug(tlm::TLM_WRITE_COMMAND, core_top - 4,
                          straddle.data(), 8)
              == 0);
        CHECK_MSG(outside.seen == seen_before_edges,
                  "a straddling outbound transfer reached the external port");
        CHECK(bridge.outbound_local_refused() == 4);

        // The byte immediately past the aperture is genuinely outside and
        // still goes out, so the rule refuses overlap rather than everything
        // nearby.
        CHECK(local.access(tlm::TLM_WRITE_COMMAND, core_top, straddle.data(), 8)
              == tlm::TLM_OK_RESPONSE);
        CHECK(outside.seen == seen_before_edges + 1);
        });

    // ── construction refuses a bridge that could not attribute its traffic ───
    sram::core_sram lonely_store("lonely_sram", sram_config());
    neo_local_sram_fabric cpu_only("cpu_only_fabric", fabric_config(),
                                   lonely_store, {neo_requester::cpu},
                                   local_fabric_timing::annotated,
                                   sc_core::sc_time(1, sc_core::SC_NS));
    bool threw = false;
    try {
        neo_external_bridge orphan("orphan_bridge", aperture, cpu_only);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw,
              "a bridge whose fabric has no 'external_inbound' requester would "
              "die on the first remote access; it must fail during "
              "elaboration instead");
    sc_core::sc_start(
        sc_core::sc_time(kWatchdogMilliseconds, sc_core::SC_MS));

    CHECK_MSG(checks.ran(),
              "the watchdog expired with work still in flight: inbound "
              "traffic and a local requester deadlocked against each other");

    std::cout << bridge.report() << '\n' << store.report();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_neo_external_bridge: all checks passed\n";
    return 0;
}
