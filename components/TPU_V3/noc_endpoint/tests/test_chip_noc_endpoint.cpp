// SPDX-License-Identifier: Apache-2.0
//
// The Phase 9 chip-endpoint gate.
//
// The endpoint's whole job is absorbing two limits the interconnect enforces by
// *refusing* rather than by coping, so what this file checks is the splitting
// rule and the containment rule — not that data survives a round trip, which
// any pass-through would also satisfy.
//
// The recorded contract (`INTERFACE_CONTRACT.md` §7) is what each check is
// written against, clause by clause: chunks bus-aligned except possibly the
// first, ascending order, the first failing chunk stops the transfer, bytes
// already moved stay moved and are reported, and every chunk attributed.
//
// **The inbound stub drives aperture-relative addresses**, because that is what
// `noc_interconnect` delivers to a mapped target
// (`floo_noc_model/src/noc_interconnect.cpp:1189`). The first version of this
// file drove absolute addresses, which is the one convention the interconnect
// never uses: every check passed while the endpoint would have refused every
// real inbound access as foreign. A stub that invents its own convention tests
// the stub.
//
// Exit codes: 0 pass, 1 fail.

#include "tpu_v3/noc/chip_noc_endpoint.h"

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
#include "tpu_v3/sram/native_port.h"

namespace tpu = cdc::components::tpu_v3;
namespace am = cdc::components::tpu_v3::address_map;

using cdc::components::tpu_v3::noc::chip_endpoint_config;
using cdc::components::tpu_v3::noc::chip_noc_endpoint;

namespace {

int failures = 0;

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "FAIL: " << (msg) << " @ " << __FILE__ << ':'        \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr tpu::chip_id_t kChip = 1;
/// Small enough to reach the splitting boundary without moving kilobytes, and
/// still a whole number of 8-byte beats.
constexpr std::uint64_t kFrame = 64;
constexpr std::uint32_t kInboundSramLimit = 16;

/// Records every chunk it is given: address, length, and the order they came
/// in. That record is the evidence for every clause of the chunking contract.
class recorder : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<recorder> socket;

    struct access {
        std::uint64_t address;
        unsigned length;
        bool is_write;
    };

    std::vector<access> seen;
    std::uint64_t debug_calls = 0;
    /// Fail the chunk covering this address, once.
    std::uint64_t fail_at = 0;
    bool fail_armed = false;
    /// Non-zero makes every access block, so a reset can land while a chunk is
    /// inside this call — the one window the endpoint cannot unwind.
    sc_core::sc_time service = sc_core::SC_ZERO_TIME;

    /// Enforce the interconnect's burst limit, the way `noc_interconnect`
    /// does: an access whose beat frame exceeds one burst is **refused**, not
    /// served. Set on the mesh-side stub only — it is standing in for the
    /// interconnect, and a stub that accepts what the real thing refuses turns
    /// the endpoint's whole reason for existing into something untested.
    bool enforce_frame_limit = false;

    /// Zero the enabled bytes of a **read** that this stub then refuses, which
    /// is what `noc_interconnect` does: a failed access assigns zeroed
    /// `read_data` (`noc_interconnect.cpp:1149` decode miss, `:1234` target
    /// refusal) and `unpack_read()` copies it into the caller's pointer
    /// unconditionally one line before the status is set — fast/bypass at
    /// `:1369`, routed mesh at `:2204`.
    ///
    /// Set on the **mesh** stub and not the chip one, and the asymmetry is
    /// faithful rather than convenient: `core_sram` and the fabrics below the
    /// chip port already leave a refused access's buffer untouched, which is
    /// the Phase 3 property. The politeness that had to go is the mesh side's.
    ///
    /// Deliberately **not** applied to the frame-limit refusal above: that one
    /// is answered before injection (`noc_interconnect.cpp:2084`) and returns
    /// without ever reaching `unpack_read`, so the buffer really is untouched
    /// there.
    bool zero_fills_failed_reads = false;

    explicit recorder(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(0x4000, 0)
    {
        socket.register_b_transport(this, &recorder::b_transport);
        socket.register_transport_dbg(this, &recorder::transport_dbg);
    }

    std::vector<unsigned char>& storage() { return storage_; }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned length = trans.get_data_length();
        seen.push_back({address, length,
                        trans.get_command() == tlm::TLM_WRITE_COMMAND});

        if (enforce_frame_limit && (address % 8) + length > kFrame) {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }

        if (service != sc_core::SC_ZERO_TIME) {
            sc_core::wait(service);
        }

        if (fail_armed && address <= fail_at && fail_at < address + length) {
            fail_armed = false;
            if (zero_fills_failed_reads
                && trans.get_command() == tlm::TLM_READ_COMMAND) {
                const auto* en = trans.get_byte_enable_ptr();
                for (unsigned i = 0; i < length; ++i) {
                    if (en != nullptr
                        && en[i % trans.get_byte_enable_length()]
                            != TLM_BYTE_ENABLED) {
                        continue;
                    }
                    trans.get_data_ptr()[i] = 0;
                }
            }
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        const std::uint64_t offset = address % storage_.size();
        if (offset + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        const auto* enables = trans.get_byte_enable_ptr();
        for (unsigned i = 0; i < length; ++i) {
            if (enables != nullptr
                && enables[i % trans.get_byte_enable_length()]
                    != TLM_BYTE_ENABLED) {
                continue;
            }
            if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
                storage_[offset + i] = trans.get_data_ptr()[i];
            } else {
                trans.get_data_ptr()[i] = storage_[offset + i];
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(1, sc_core::SC_NS);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        ++debug_calls;
        return trans.get_data_length();
    }

    std::vector<unsigned char> storage_;
};

/// Issues one transfer from its own process, so a reset can land mid-flight.
class master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<master> socket;

    explicit master(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status access(tlm::tlm_command command,
                                    std::uint64_t address,
                                    std::vector<unsigned char>& data)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(data.data());
        trans.set_data_length(static_cast<unsigned>(data.size()));
        trans.set_streaming_width(static_cast<unsigned>(data.size()));
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }

    unsigned int debug(std::uint64_t address, unsigned length)
    {
        std::vector<unsigned char> data(length, 0);
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(data.data());
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        return socket->transport_dbg(trans);
    }
};

class transfer_thread : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(transfer_thread);

    transfer_thread(sc_core::sc_module_name name, master& drv,
                    std::uint64_t address, unsigned length,
                    sc_core::sc_time carry = sc_core::SC_ZERO_TIME)
        : sc_core::sc_module(name)
        , drv_(drv)
        , address_(address)
        , length_(length)
        , carry_(carry)
    {
        SC_THREAD(run);
    }

    tlm::tlm_response_status status = tlm::TLM_INCOMPLETE_RESPONSE;
    bool finished = false;
    sc_core::sc_time returned_at = sc_core::SC_ZERO_TIME;
    /// `sc_time_stamp() + delay` must never decrease across the call.
    bool went_backwards = false;

private:
    void run()
    {
        std::vector<unsigned char> data(length_, 0xEE);
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(address_);
        trans.set_data_ptr(data.data());
        trans.set_data_length(length_);
        trans.set_streaming_width(length_);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = carry_;
        const sc_core::sc_time logical_before
            = sc_core::sc_time_stamp() + delay;
        drv_.socket->b_transport(trans, delay);
        returned_at = sc_core::sc_time_stamp();
        if (sc_core::sc_time_stamp() + delay < logical_before) {
            went_backwards = true;
        }
        status = trans.get_response_status();
        finished = true;
    }

    master& drv_;
    std::uint64_t address_;
    unsigned length_;
    sc_core::sc_time carry_;
};


/// An absolute TPU_V3 address as the mesh presents it to a mapped target:
/// relative to the chip aperture base.
std::uint64_t relative(std::uint64_t absolute)
{
    return absolute - am::chip_base(kChip);
}

chip_endpoint_config endpoint_config()
{
    chip_endpoint_config config;
    config.chip = kChip;
    config.max_frame_bytes = kFrame;
    config.bus_bytes = 8;
    config.max_inbound_sram_bytes = kInboundSramLimit;
    return config;
}

/// The endpoint with a stub on each of its four sockets.
struct bench {
    chip_noc_endpoint endpoint;
    master chip_side{"chip_side"};
    master noc_side{"noc_side"};
    recorder mesh{"mesh"};
    recorder chip{"chip"};

    explicit bench(const std::string& prefix)
        : endpoint((prefix + "_endpoint").c_str(), endpoint_config())
    {
        mesh.enforce_frame_limit = true;
        mesh.zero_fills_failed_reads = true;
        chip_side.socket.bind(endpoint.from_chip);
        noc_side.socket.bind(endpoint.from_noc);
        endpoint.to_noc.bind(mesh.socket);
        endpoint.to_chip.bind(chip.socket);
    }
};

// ── the splitting rule ───────────────────────────────────────────────────────

void check_no_split_below_the_frame_limit()
{
    bench b("nosplit");
    std::vector<unsigned char> data(kFrame, 0x11);

    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::global_ram_base,
                                 data)
                  == tlm::TLM_OK_RESPONSE,
              "an exactly-frame-sized aligned transfer must be accepted");
    CHECK_MSG(b.mesh.seen.size() == 1,
              "a transfer that fits one frame must not be split");
    CHECK_MSG(b.endpoint.outbound_split_transfers() == 0,
              "an unsplit transfer was counted as split");
    CHECK_MSG(b.endpoint.outbound_chunks() == 1
                  && b.endpoint.outbound_transfers() == 1,
              "one transfer of one chunk must be counted as exactly that");
}

void check_aligned_split()
{
    bench b("aligned");
    const std::uint64_t length = kFrame * 3 + 8;
    std::vector<unsigned char> data(length, 0x22);

    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::global_ram_base,
                                 data)
                  == tlm::TLM_OK_RESPONSE,
              "an oversized transfer must be split, not refused");

    CHECK_MSG(b.mesh.seen.size() == 4,
              "expected four chunks, got "
                  + std::to_string(b.mesh.seen.size()));

    std::uint64_t expect_address = am::global_ram_base;
    for (std::size_t i = 0; i < b.mesh.seen.size(); ++i) {
        CHECK_MSG(b.mesh.seen[i].address == expect_address,
                  "chunk " + std::to_string(i)
                      + " is out of ascending address order");
        CHECK_MSG(b.mesh.seen[i].length
                      <= b.endpoint.frame_capacity_at(
                          b.mesh.seen[i].address),
                  "chunk " + std::to_string(i)
                      + " exceeds the frame the interconnect accepts at its "
                        "address");
        CHECK_MSG(b.endpoint.beats_for(b.mesh.seen[i].address,
                                       b.mesh.seen[i].length)
                      <= kFrame / 8,
                  "chunk " + std::to_string(i) + " is more beats than a burst "
                                                 "can describe");
        expect_address += b.mesh.seen[i].length;
    }
    CHECK_MSG(expect_address == am::global_ram_base + length,
              "the chunks do not cover the transfer exactly");
    CHECK_MSG(b.endpoint.outbound_bytes() == length,
              "the byte count does not match what was transferred");
    CHECK_MSG(b.endpoint.outbound_split_transfers() == 1,
              "a split transfer was not counted as split");
}

void check_unaligned_first_chunk()
{
    bench b("unaligned");
    // Start three bytes into a bus word. The contract says chunks are aligned
    // *except possibly the first*, so the first runs to the next boundary and
    // every later one starts aligned — the transfer pays for its offset once.
    const std::uint64_t start = am::global_ram_base + 3;
    const std::uint64_t length = kFrame * 2;
    std::vector<unsigned char> data(length, 0x33);

    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, start, data)
                  == tlm::TLM_OK_RESPONSE,
              "an unaligned oversized transfer must be split, not refused");

    CHECK_MSG(!b.mesh.seen.empty(), "nothing was forwarded");
    CHECK_MSG(b.mesh.seen[0].address == start,
              "the first chunk must start where the caller asked");
    CHECK_MSG((b.mesh.seen[0].address + b.mesh.seen[0].length) % 8 == 0,
              "the first chunk must run to a bus boundary so the rest are "
              "aligned");
    for (std::size_t i = 1; i < b.mesh.seen.size(); ++i) {
        CHECK_MSG(b.mesh.seen[i].address % 8 == 0,
                  "chunk " + std::to_string(i)
                      + " is not bus-aligned; only the first may be");
        CHECK_MSG(b.endpoint.beats_for(b.mesh.seen[i].address,
                                       b.mesh.seen[i].length)
                      <= kFrame / 8,
                  "chunk " + std::to_string(i) + " is too many beats");
    }
}

void check_partial_failure_stops_and_reports()
{
    bench b("partial");
    const std::uint64_t length = kFrame * 4;
    std::vector<unsigned char> data(length, 0x44);

    // Fail the third chunk. Two chunks' worth must already have been moved,
    // and the transfer must stop there rather than pressing on.
    b.mesh.fail_at = am::global_ram_base + kFrame * 2 + 8;
    b.mesh.fail_armed = true;

    const auto status =
        b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::global_ram_base, data);

    CHECK_MSG(status == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "the failing chunk's status must be the transfer's status");
    CHECK_MSG(b.mesh.seen.size() == 3,
              "the transfer must stop at the first failing chunk, not carry "
              "on; saw " + std::to_string(b.mesh.seen.size()) + " chunks");
    CHECK_MSG(b.endpoint.last_partial_bytes() == kFrame * 2,
              "the completion must report the bytes already transferred; "
              "reported " + std::to_string(b.endpoint.last_partial_bytes()));
    CHECK_MSG(b.endpoint.outbound_partial_failures() == 1,
              "a partial failure was not counted");
    CHECK_MSG(b.endpoint.outbound_bytes() == kFrame * 2,
              "bytes already transferred must still be counted as "
              "transferred");
}

// ── containment ──────────────────────────────────────────────────────────────

void check_local_containment()
{
    bench b("contain");
    std::vector<unsigned char> data(8, 0x55);

    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND,
                                 am::core_sram_base(kChip, 0), data)
                  != tlm::TLM_OK_RESPONSE,
              "an address inside this chip must never be handed to the mesh");
    CHECK_MSG(b.mesh.seen.empty(),
              "a chip-local address reached the mesh port. The chip fabric "
              "answers those itself; with D1 in place the bypass would deliver "
              "it back into the chip and the broken decode would go unnoticed");
    CHECK_MSG(b.endpoint.outbound_local_refused() == 1,
              "the refusal was not counted");

    // Overlap, not containment: a transfer that starts outside and runs into
    // the aperture is still local traffic in the mesh for part of its length.
    std::vector<unsigned char> straddle(16, 0x66);
    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND,
                                 am::chip_base(kChip) - 8, straddle)
                  != tlm::TLM_OK_RESPONSE,
              "a transfer overlapping the chip aperture must be refused");
    CHECK_MSG(b.mesh.seen.empty(), "a straddling transfer reached the mesh");
}

void check_inbound_route_and_split()
{
    bench b("inbound");

    // A remote master writing more than a core's inbound SRAM limit. The
    // endpoint splits it; the core's bridge would refuse the whole thing.
    const std::uint64_t length = kInboundSramLimit * 3;
    std::vector<unsigned char> data(length, 0x77);
    CHECK_MSG(b.noc_side.access(tlm::TLM_WRITE_COMMAND,
                                relative(am::core_sram_base(kChip, 1)), data)
                  == tlm::TLM_OK_RESPONSE,
              "an inbound SRAM write longer than the core limit must be "
              "split, not refused");
    CHECK_MSG(b.chip.seen.size() == 3,
              "expected three inbound chunks, got "
                  + std::to_string(b.chip.seen.size()));
    for (const auto& chunk : b.chip.seen) {
        CHECK_MSG(chunk.length <= kInboundSramLimit,
                  "an inbound chunk exceeds what a core's SRAM path accepts");
    }
    CHECK_MSG(b.chip.seen.front().address == am::core_sram_base(kChip, 1),
              "the chip must be handed the **absolute** address; everything "
              "inside a chip uses the one address per resource of "
              "ADDRESS_MAP.md §4, and the aperture-relative form the mesh uses "
              "would land in core 0's SRAM instead");

    // A four-byte register at an odd bus lane. With the limit taken from the
    // transfer's own length this produced a zero-length chunk and an error
    // instead of one access -- half of the naturally aligned 32-bit register
    // positions.
    b.chip.seen.clear();
    std::vector<unsigned char> reg(4, 0xAA);
    CHECK_MSG(b.noc_side.access(tlm::TLM_WRITE_COMMAND,
                                relative(am::sa_control(kChip, 0) + 4), reg)
                  == tlm::TLM_OK_RESPONSE,
              "a 4-byte MMIO write at bus lane 4 must be one whole access");
    CHECK_MSG(b.chip.seen.size() == 1 && b.chip.seen[0].length == 4,
              "a 4-byte MMIO access at an odd bus lane was split or dropped");

    // MMIO is deliberately left whole: AXI4-Lite is four bytes and a wider
    // access is an error the control plane must report, not something to turn
    // into several register writes.
    b.chip.seen.clear();
    std::vector<unsigned char> wide(kInboundSramLimit * 2, 0x88);
    b.noc_side.access(tlm::TLM_WRITE_COMMAND,
                      relative(am::sa_control(kChip, 0)), wide);
    CHECK_MSG(b.chip.seen.size() == 1,
              "an inbound MMIO access was split; that invents a semantics the "
              "control plane never agreed to");

    // And nothing outside this chip may arrive here.
    // Past the end of the aperture, in relative terms: the mesh delivered a
    // packet to the wrong node.
    std::vector<unsigned char> foreign(8, 0x99);
    CHECK_MSG(b.noc_side.access(tlm::TLM_READ_COMMAND,
                                am::chip_aperture_stride, foreign)
                  != tlm::TLM_OK_RESPONSE,
              "an inbound access past the end of the aperture must be "
              "refused");
    CHECK_MSG(b.endpoint.inbound_foreign_refused() == 1,
              "the foreign inbound refusal was not counted");
}

void check_debug_path()
{
    bench b("debug");
    CHECK_MSG(b.chip_side.debug(am::global_ram_base, 8) == 8,
              "a debug access to a remote address must reach the mesh");
    CHECK_MSG(b.chip_side.debug(am::core_sram_base(kChip, 0), 8) == 0,
              "a debug access to this chip's own aperture must not reach the "
              "mesh; §8 relaxes timing, not the routing rules");
    CHECK_MSG(b.noc_side.debug(relative(am::core_sram_base(kChip, 0)), 8) == 8,
              "an inbound debug access must reach the chip");
    CHECK_MSG(b.noc_side.debug(am::chip_aperture_stride, 8) == 0,
              "an inbound debug access past the aperture must be refused");
    CHECK_MSG(b.chip_side.debug(am::global_ram_base, 0) == 0,
              "a zero-length debug payload names no bytes and must not be "
              "forwarded");
    CHECK_MSG(b.endpoint.outbound_transfers() == 0,
              "the debug path moved a counter; it must be free of side "
              "effects");
}

/// A fully masked write moves nothing, and must be counted as moving nothing.
void check_masked_bytes_are_not_counted_as_moved()
{
    bench b("masked");
    const unsigned length = static_cast<unsigned>(kFrame * 2);
    std::vector<unsigned char> data(length, 0xCC);
    std::vector<unsigned char> enables(length, TLM_BYTE_DISABLED);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(am::global_ram_base);
    trans.set_data_ptr(data.data());
    trans.set_data_length(length);
    trans.set_streaming_width(length);
    trans.set_byte_enable_ptr(enables.data());
    trans.set_byte_enable_length(length);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    b.chip_side.socket->b_transport(trans, delay);

    CHECK_MSG(trans.get_response_status() == tlm::TLM_OK_RESPONSE,
              "a fully masked write is legal and must succeed");
    CHECK_MSG(b.endpoint.outbound_bytes() == 0,
              "a fully masked write reported "
                  + std::to_string(b.endpoint.outbound_bytes())
                  + " bytes moved; the target performed no access, so the "
                    "count must be zero. Address progress and bytes moved are "
                    "different quantities");
}

/// A transfer that leaves one architectural region for another is refused
/// **before** any chunk is issued.
void check_multi_region_span_is_refused_before_splitting()
{
    bench b("span");
    // Ends past the top of global RAM and into the first chip aperture.
    const std::uint64_t start =
        am::global_ram_base + am::global_ram_window - 16;
    std::vector<unsigned char> data(kFrame * 2, 0xDD);

    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, start, data)
                  != tlm::TLM_OK_RESPONSE,
              "a transfer spanning two architectural regions must be refused");
    CHECK_MSG(b.mesh.seen.empty(),
              "a multi-region transfer was split and its leading chunks "
              "committed before the failure. The interconnect refuses such a "
              "transfer having touched nothing; chunking first turns a clean "
              "refusal into a partial side effect");
    CHECK_MSG(b.endpoint.outbound_span_refused() == 1,
              "the multi-region refusal was not counted");
}

/// A span that stays inside one chip aperture but crosses a *window* boundary
/// inside it. The coarse version of the span check accepted this.
void check_subregion_boundary_is_refused()
{
    bench b("subregion");
    // Runs off the end of a remote chip's core-0 SRAM into that core's
    // CORE_CONTROL window: one aperture, two targets.
    const std::uint64_t start =
        am::core_sram_base(0, 0) + am::core_sram_window - 16;
    std::vector<unsigned char> data(kFrame * 2, 0x1A);

    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, start, data)
                  != tlm::TLM_OK_RESPONSE,
              "a span crossing CORE_SRAM into CORE_CONTROL must be refused");
    CHECK_MSG(b.mesh.seen.empty(),
              "a span crossing two windows inside one chip aperture was split; "
              "the SRAM chunks would commit before the MMIO chunk failed, "
              "which is the partial side effect the pre-split check exists to "
              "prevent");
    CHECK_MSG(b.endpoint.outbound_span_refused() == 1,
              "the sub-region span refusal was not counted");

    // The reserved hole above a core's counters window decodes nowhere.
    b.mesh.seen.clear();
    std::vector<unsigned char> small(8, 0x1B);
    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND,
                                 am::core_counters(0, 0)
                                     + am::core_counters_size,
                                 small)
                  != tlm::TLM_OK_RESPONSE,
              "an address in the reserved hole inside a core aperture must be "
              "refused");
}

/// An oversized MMIO transfer must reach the interconnect whole, so the
/// interconnect can refuse it — not be split into legal register writes.
void check_oversized_mmio_is_not_split()
{
    bench b("mmio_out");
    const std::uint64_t base = am::sa_control(0, 0);
    std::vector<unsigned char> data(kFrame * 3, 0x2A);

    b.chip_side.access(tlm::TLM_WRITE_COMMAND, base, data);

    CHECK_MSG(b.mesh.seen.size() == 1,
              "an oversized MMIO transfer was split into "
                  + std::to_string(b.mesh.seen.size())
                  + " chunks. Each would be a legal register write, so an "
                    "illegal transaction would have modified the device; the "
                    "interconnect must be given it whole and refuse it");
    CHECK_MSG(b.endpoint.outbound_split_transfers() == 0,
              "an MMIO transfer that must not be split was counted as split");
}

/// The split counter describes what the transfer needed, not what it managed.
void check_split_counted_when_the_first_chunk_fails()
{
    bench b("splitfail");
    const std::uint64_t length = kFrame * 3;
    std::vector<unsigned char> data(length, 0x3A);

    b.mesh.fail_at = am::global_ram_base;   // the very first chunk
    b.mesh.fail_armed = true;

    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::global_ram_base,
                                 data)
                  != tlm::TLM_OK_RESPONSE,
              "the failing first chunk's status must be returned");
    CHECK_MSG(b.endpoint.outbound_split_transfers() == 1,
              "a transfer that needed splitting was not counted as split "
              "because its first chunk failed; the counter must describe the "
              "transfer, not the attempt");
}

/// An inbound failure must not rewrite the outbound completion's number.
void check_partial_metrics_are_direction_specific()
{
    bench b("partmetric");

    // An outbound transfer that fails part-way.
    std::vector<unsigned char> out(kFrame * 3, 0x4A);
    b.mesh.fail_at = am::global_ram_base + kFrame + 8;
    b.mesh.fail_armed = true;
    b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::global_ram_base, out);
    const auto outbound_report = b.endpoint.last_partial_bytes();
    CHECK_MSG(outbound_report == kFrame,
              "the outbound partial report is wrong before any inbound "
              "traffic");

    // Now an inbound transfer that also fails.
    std::vector<unsigned char> in(kInboundSramLimit * 3, 0x4B);
    b.chip.fail_at = am::core_sram_base(kChip, 0) + kInboundSramLimit;
    b.chip.fail_armed = true;
    b.noc_side.access(tlm::TLM_WRITE_COMMAND,
                      relative(am::core_sram_base(kChip, 0)), in);

    CHECK_MSG(b.endpoint.last_partial_bytes() == outbound_report,
              "an inbound failure rewrote the outbound completion's byte "
              "count; under bidirectional traffic that metric would change "
              "without its own transfer doing anything");
    CHECK_MSG(b.endpoint.last_inbound_partial_bytes() == kInboundSramLimit,
              "the inbound partial report is wrong");
}

/// A failure that moved nothing is not a partial completion.
void check_zero_progress_failure_is_not_partial()
{
    bench b("zeroprog");

    // Establish a real partial failure first, so the check below is about the
    // zero-progress case overwriting it rather than about an empty metric.
    std::vector<unsigned char> out(kFrame * 3, 0x5A);
    b.mesh.fail_at = am::global_ram_base + kFrame + 8;
    b.mesh.fail_armed = true;
    b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::global_ram_base, out);
    const auto real_partial = b.endpoint.last_partial_bytes();
    const auto partial_count = b.endpoint.outbound_partial_failures();
    CHECK_MSG(real_partial == kFrame && partial_count == 1,
              "the genuine partial failure was not recorded");

    // An oversized MMIO transfer: forwarded whole, refused whole, nothing
    // moved. This is the common case, not a corner one — it is exactly what
    // leaving MMIO unsplit produces.
    std::vector<unsigned char> mmio(kFrame * 3, 0x5B);
    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::sa_control(0, 0),
                                 mmio)
                  != tlm::TLM_OK_RESPONSE,
              "an oversized MMIO transfer must be refused");

    CHECK_MSG(b.endpoint.outbound_partial_failures() == partial_count,
              "a transfer that committed nothing was counted as a partial "
              "failure");
    CHECK_MSG(b.endpoint.last_partial_bytes() == real_partial,
              "a zero-progress failure overwrote the byte count of a genuine "
              "partial completion; the accessor documents bytes committed "
              "before the transfer stopped, and nothing was committed here");
}

/// **E-2.** A target that refuses having moved nothing must reach a counter,
/// and must be distinguishable from a fully byte-disabled write that
/// legitimately moves nothing and succeeds.
///
/// Both leave the same residue in the published metrics — one chunk, zero
/// bytes — so before this counter existed the two were not merely uncounted
/// but indistinguishable, and a real error was unrecoverable from the report.
void check_zero_progress_target_error_is_counted()
{
    bench b("targeterr");

    // An oversized MMIO transfer: forwarded whole because MMIO is never split,
    // refused whole by the interconnect stub, nothing moved. This is the
    // common case rather than a corner one.
    std::vector<unsigned char> mmio(kFrame * 3, 0x6A);
    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::sa_control(0, 0),
                                 mmio)
                  != tlm::TLM_OK_RESPONSE,
              "an oversized MMIO transfer must be refused by the target");

    CHECK_MSG(b.endpoint.outbound_target_errors() == 1,
              "a target refusal that moved no bytes was counted nowhere. "
              "`outbound_partial_failures()` asks how often a transfer stopped "
              "part-way, which is the one question that cannot see this");
    CHECK_MSG(b.endpoint.outbound_partial_failures() == 0,
              "a transfer that committed nothing was counted as a partial "
              "failure");
    CHECK_MSG(b.endpoint.outbound_chunks() == 1
                  && b.endpoint.outbound_bytes() == 0,
              "the failing transfer should show exactly one chunk and no "
              "bytes");

    // The same residue, from a transfer that succeeded. Everything published
    // about the two agrees except the counter added for exactly this.
    const auto errors_after_refusal = b.endpoint.outbound_target_errors();
    const unsigned length = static_cast<unsigned>(kFrame);
    std::vector<unsigned char> data(length, 0x6B);
    std::vector<unsigned char> enables(length, TLM_BYTE_DISABLED);

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(am::global_ram_base);
    trans.set_data_ptr(data.data());
    trans.set_data_length(length);
    trans.set_streaming_width(length);
    trans.set_byte_enable_ptr(enables.data());
    trans.set_byte_enable_length(length);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    b.chip_side.socket->b_transport(trans, delay);

    CHECK_MSG(trans.get_response_status() == tlm::TLM_OK_RESPONSE,
              "a fully masked write is legal and must succeed");
    CHECK_MSG(b.endpoint.outbound_target_errors() == errors_after_refusal,
              "a successful fully masked write was counted as a target error");
    CHECK_MSG(b.endpoint.outbound_chunks() == 2
                  && b.endpoint.outbound_bytes() == 0,
              "the masked write should add a chunk and no bytes, leaving the "
              "same chunks>0/bytes==0 residue the refusal left -- which is "
              "why the residue cannot be the evidence");
}

/// **E-3.** An inbound transfer that fails after committing bytes must
/// increment a count, not only overwrite a most-recent value.
///
/// Run twice on purpose: with only the value, the second failure erases the
/// evidence of the first and how often it happened is unrecoverable.
void check_inbound_partial_failures_are_counted()
{
    bench b("inpartial");

    const std::uint64_t first_length = kInboundSramLimit * 3;
    std::vector<unsigned char> first(first_length, 0x7A);
    b.chip.fail_at = am::core_sram_base(kChip, 0) + kInboundSramLimit;
    b.chip.fail_armed = true;
    CHECK_MSG(b.noc_side.access(tlm::TLM_WRITE_COMMAND,
                                relative(am::core_sram_base(kChip, 0)), first)
                  != tlm::TLM_OK_RESPONSE,
              "the failing inbound chunk's status must be returned");
    CHECK_MSG(b.endpoint.inbound_partial_failures() == 1,
              "an inbound partial failure was not counted");
    CHECK_MSG(b.endpoint.last_inbound_partial_bytes() == kInboundSramLimit,
              "the inbound partial byte report is wrong");

    const std::uint64_t second_length = kInboundSramLimit * 4;
    std::vector<unsigned char> second(second_length, 0x7B);
    b.chip.fail_at = am::core_sram_base(kChip, 1) + kInboundSramLimit * 2;
    b.chip.fail_armed = true;
    b.noc_side.access(tlm::TLM_WRITE_COMMAND,
                      relative(am::core_sram_base(kChip, 1)), second);

    CHECK_MSG(b.endpoint.inbound_partial_failures() == 2,
              "the second inbound partial failure was not counted; with only "
              "`last_inbound_partial_bytes()` the first one's evidence is "
              "erased and the frequency is unrecoverable");
    CHECK_MSG(b.endpoint.last_inbound_partial_bytes() == kInboundSramLimit * 2,
              "the most-recent inbound partial value did not follow the "
              "second failure");
    CHECK_MSG(b.endpoint.inbound_target_errors() == 2,
              "the inbound target refusals were not counted");
}

/// **E-4.** The defect is that the number never reaches the report, so the
/// assertion has to be on `report()` text.
void check_report_covers_both_directions()
{
    bench b("reporttext");

    // Give it one failure in each direction, so every field being checked has
    // something to say.
    std::vector<unsigned char> out(kFrame * 3, 0x8A);
    b.mesh.fail_at = am::global_ram_base + kFrame + 8;
    b.mesh.fail_armed = true;
    b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::global_ram_base, out);

    std::vector<unsigned char> in(kInboundSramLimit * 3, 0x8B);
    b.chip.fail_at = am::core_sram_base(kChip, 0) + kInboundSramLimit;
    b.chip.fail_armed = true;
    b.noc_side.access(tlm::TLM_WRITE_COMMAND,
                      relative(am::core_sram_base(kChip, 0)), in);

    const std::string text = b.endpoint.report();
    const auto line = [&text](const std::string& prefix) {
        const auto start = text.find(prefix);
        if (start == std::string::npos) {
            return std::string{};
        }
        return text.substr(start, text.find('\n', start) - start);
    };

    const std::string outbound = line("  outbound: ");
    const std::string inbound = line("  inbound : ");

    CHECK_MSG(!outbound.empty() && !inbound.empty(),
              "report() is missing a direction line entirely");
    CHECK_MSG(outbound.find("partial failures") != std::string::npos
                  && outbound.find("target errors") != std::string::npos,
              "the outbound report line does not name its failure counters");
    CHECK_MSG(inbound.find("partial failures") != std::string::npos,
              "the inbound report line prints no partial-failure count. It is "
              "set and reachable only through an accessor, from a test, so an "
              "inbound failure reaches no report at all");
    CHECK_MSG(inbound.find("target errors") != std::string::npos,
              "the inbound report line prints no target-error count");

    const std::string latency = line("  latency : ");
    CHECK_MSG(!latency.empty(),
              "report() prints no latency line; plan §11.10 requires latency "
              "and decision record D24 puts it here");
    CHECK_MSG(latency.find("outbound") != std::string::npos
                  && latency.find("inbound") != std::string::npos,
              "the latency line does not cover both directions");
    CHECK_MSG(text.find("per transaction") != std::string::npos
                  && text.find("outstanding is deliberately not measured")
                      != std::string::npos,
              "report() does not say what its latency figure is not. D24 "
              "forbids quoting it as noc_interconnect's per-transaction "
              "figure, and a reader with both numbers in front of them will "
              "compare them unless the report says otherwise");
}

/// **D24.** Transfer latency is measured here and is measured as *logical*
/// time. Outstanding is deliberately not measured at all.
///
/// The load-bearing part is that this runs against a target that only
/// **annotates**: `sc_time_stamp()` never moves, so an implementation that
/// measured a stamp difference would read zero for every transfer on a
/// loosely timed path — which is the path a full-system run uses, so the
/// counter would be silently empty exactly where it matters.
void check_transfer_latency_is_logical_time()
{
    bench b("latency");
    const std::uint64_t length = kFrame * 4;   // four chunks, four annotations
    std::vector<unsigned char> data(length, 0x9A);

    CHECK_MSG(b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::global_ram_base,
                                 data)
                  == tlm::TLM_OK_RESPONSE,
              "the latency probe transfer did not complete");

    const sc_core::sc_time expected(4, sc_core::SC_NS);   // 1 ns per chunk
    CHECK_MSG(b.endpoint.outbound_chunks() == 4,
              "the latency probe did not produce the four chunks it assumes");
    CHECK_MSG(b.endpoint.outbound_latency_samples() == 1,
              "one forwarded transfer must contribute exactly one latency "
              "sample; saw "
                  + std::to_string(b.endpoint.outbound_latency_samples()));
    CHECK_MSG(b.endpoint.outbound_latency_total() == expected,
              "transfer latency read "
                  + b.endpoint.outbound_latency_total().to_string()
                  + ", expected " + expected.to_string()
                  + ". Latency must be logical time -- sc_time_stamp() + delay "
                    "-- not an sc_time_stamp() difference: an annotating "
                    "target consumes no simulated time, so a stamp difference "
                    "reads zero on every loosely timed path");
    CHECK_MSG(b.endpoint.outbound_latency_max()
                  == b.endpoint.outbound_latency_total(),
              "with one sample the maximum must equal the total");

    // A transfer this endpoint refused describes nothing about the mesh and
    // must not be sampled; including it would drag the mean toward zero in
    // proportion to how many integration defects were present.
    const auto samples_before = b.endpoint.outbound_latency_samples();
    const auto total_before = b.endpoint.outbound_latency_total();
    std::vector<unsigned char> local(8, 0x9B);
    b.chip_side.access(tlm::TLM_WRITE_COMMAND, am::core_sram_base(kChip, 0),
                       local);
    CHECK_MSG(b.endpoint.outbound_latency_samples() == samples_before
                  && b.endpoint.outbound_latency_total() == total_before,
              "a transfer the endpoint refused was counted as a latency "
              "sample");

    // The two directions are accounted separately.
    CHECK_MSG(b.endpoint.inbound_latency_samples() == 0,
              "outbound traffic contributed an inbound latency sample");
    std::vector<unsigned char> in(kInboundSramLimit * 2, 0x9C);
    b.noc_side.access(tlm::TLM_WRITE_COMMAND,
                      relative(am::core_sram_base(kChip, 1)), in);
    CHECK_MSG(b.endpoint.inbound_latency_samples() == 1
                  && b.endpoint.inbound_latency_total()
                      == sc_core::sc_time(2, sc_core::SC_NS),
              "the inbound transfer's latency was not accounted on its own "
              "direction");
    CHECK_MSG(b.endpoint.outbound_latency_samples() == samples_before,
              "inbound traffic contributed an outbound latency sample");
}

/// A pattern for the stub's storage: never zero (so a zero-fill is visible)
/// and never the caller's sentinel (so an untouched byte is visible).
unsigned char pattern_at(std::size_t i)
{
    return static_cast<unsigned char>(0x10 + (i % 0x60));
}

constexpr unsigned char kSentinel = 0xA5;

/// **A failed read leaves the caller's buffer untouched.**
///
/// `INTERFACE_CONTRACT.md` §1: "Errors are never converted to zero data. A
/// failed read leaves the caller's buffer untouched and sets an error status."
/// The endpoint used to hand each chunk a direct pointer into that buffer,
/// which cannot honour the rule, because the downstream path writes the buffer
/// before this component sees the status — and `noc_interconnect` writes
/// zeroes there on every failed read, in both timing modes.
///
/// The gate could not see it until the stub stopped being politer than the
/// component it stands in for. That is the same lesson this file's header
/// already records about inbound addresses, from the other direction: a stub
/// that refuses more gently than the real thing tests the stub.
void check_failed_read_leaves_the_buffer_untouched()
{
    bench b("readfail");
    for (std::size_t i = 0; i < b.mesh.storage().size(); ++i) {
        b.mesh.storage()[i] = pattern_at(i);
    }

    const std::uint64_t length = kFrame * 4;
    std::vector<unsigned char> data(length, kSentinel);

    // Fail the third chunk. Two must land, the third must not be written at
    // all, and the fourth must never be issued.
    b.mesh.fail_at = am::global_ram_base + kFrame * 2 + 8;
    b.mesh.fail_armed = true;

    const auto status =
        b.chip_side.access(tlm::TLM_READ_COMMAND, am::global_ram_base, data);
    CHECK_MSG(status != tlm::TLM_OK_RESPONSE,
              "the failing chunk's status must be the transfer's status");

    bool retained = true;
    for (std::uint64_t i = 0; i < kFrame * 2; ++i) {
        if (data[i] != pattern_at(i)) {
            retained = false;
        }
    }
    CHECK_MSG(retained,
              "bytes read by the chunks that succeeded before the failure were "
              "not retained; the contract says what has already been "
              "transferred stays transferred");

    bool failing_chunk_untouched = true;
    for (std::uint64_t i = kFrame * 2; i < kFrame * 3; ++i) {
        if (data[i] != kSentinel) {
            failing_chunk_untouched = false;
        }
    }
    CHECK_MSG(failing_chunk_untouched,
              "the failing read chunk overwrote the caller's buffer. "
              "INTERFACE_CONTRACT.md §1 requires a failed read to leave it "
              "untouched, and noc_interconnect writes zeroes into whatever "
              "pointer it is given before the status is set -- so a chunk must "
              "be staged and published only after TLM_OK_RESPONSE");

    bool never_issued_untouched = true;
    for (std::uint64_t i = kFrame * 3; i < length; ++i) {
        if (data[i] != kSentinel) {
            never_issued_untouched = false;
        }
    }
    CHECK_MSG(never_issued_untouched,
              "a chunk after the failing one touched the buffer, so the "
              "transfer did not stop at the first failure");
}

/// **A masked read keeps the caller's disabled bytes.**
///
/// The control for the trap inside the fix above rather than for the defect it
/// fixes: staging and then copying the whole chunk back would publish staging
/// content over the disabled positions. `unpack_read()` skips disabled bytes,
/// so the target never wrote them and the caller's own values are what belongs
/// there.
void check_masked_read_preserves_disabled_bytes()
{
    bench b("maskedread");
    for (std::size_t i = 0; i < b.mesh.storage().size(); ++i) {
        b.mesh.storage()[i] = pattern_at(i);
    }

    const unsigned length = static_cast<unsigned>(kFrame);
    std::vector<unsigned char> data(length, kSentinel);
    std::vector<unsigned char> enables(length, TLM_BYTE_DISABLED);
    for (unsigned i = 0; i < length; i += 2) {
        enables[i] = TLM_BYTE_ENABLED;
    }

    tlm::tlm_generic_payload trans;
    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(am::global_ram_base);
    trans.set_data_ptr(data.data());
    trans.set_data_length(length);
    trans.set_streaming_width(length);
    trans.set_byte_enable_ptr(enables.data());
    trans.set_byte_enable_length(length);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    b.chip_side.socket->b_transport(trans, delay);

    CHECK_MSG(trans.get_response_status() == tlm::TLM_OK_RESPONSE,
              "a masked read is legal and must succeed");

    bool enabled_delivered = true;
    bool disabled_preserved = true;
    for (unsigned i = 0; i < length; ++i) {
        if (enables[i] == TLM_BYTE_ENABLED) {
            if (data[i] != pattern_at(i)) {
                enabled_delivered = false;
            }
        } else if (data[i] != kSentinel) {
            disabled_preserved = false;
        }
    }
    CHECK_MSG(enabled_delivered,
              "an enabled byte of a masked read was not delivered");
    CHECK_MSG(disabled_preserved,
              "a masked read overwrote the caller's disabled bytes. The target "
              "never wrote them, so publishing a whole staged chunk back turns "
              "a correct read into a corrupting one");
    CHECK_MSG(b.endpoint.outbound_bytes() == length / 2,
              "a masked read must count only the bytes it moved");
}

void check_configuration_refusals()
{
    bool threw = false;
    try {
        auto config = endpoint_config();
        config.max_frame_bytes = 12;  // not a whole number of 8-byte beats
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "a frame that is not a whole number of beats was "
                     "accepted");

    threw = false;
    try {
        auto config = endpoint_config();
        config.chip = tpu::max_chips;
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "a chip index past the FlooNoC manager-id limit was "
                     "accepted (decision record D2)");

    threw = false;
    try {
        auto config = endpoint_config();
        config.bus_bytes = 6;  // not a power of two
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "a non-power-of-two bus width was accepted");

    // The splitter aligns chunk boundaries to the bus, so a limit that is not
    // a bus multiple makes later chunks start mid-word, and one smaller than a
    // lane offset underflows into a chunk larger than the limit itself.
    threw = false;
    try {
        auto config = endpoint_config();
        config.max_inbound_sram_bytes = 12;
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw,
              "an inbound SRAM limit that is not a whole number of bus words "
              "was accepted");

    // Above the native plane's own maximum. A NEO-CORE's external bridge
    // refuses an inbound SRAM access longer than `neo_max_transfer_bytes`, so
    // such a configuration elaborates and then fails every inbound SRAM
    // transfer -- the endpoint would be splitting into chunks the core cannot
    // take. It is a whole number of bus words, so the check above cannot catch
    // it.
    threw = false;
    try {
        auto config = endpoint_config();
        config.max_inbound_sram_bytes =
            tpu::sram::neo_max_transfer_bytes + 8;
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw,
              "an inbound SRAM limit above sram::neo_max_transfer_bytes was "
              "accepted; the endpoint would emit chunks a core's external "
              "bridge refuses by design, so every inbound SRAM transfer would "
              "fail after a clean elaboration");

    // Exactly at the maximum is the reference configuration and must be
    // accepted, so the bound is a bound rather than an off-by-one.
    threw = false;
    try {
        auto config = endpoint_config();
        config.max_inbound_sram_bytes = tpu::sram::neo_max_transfer_bytes;
        config.validate("test");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(!threw,
              "the native plane's own maximum was refused as an inbound SRAM "
              "limit; it is the reference value, not an overflow");
}

/// Samples the endpoint's counters at a chosen instant, from a process of its
/// own.
///
/// The point is *when*. Reading them after the transfer returns says nothing
/// about whether bytes and chunks agree while one is in flight, and a
/// simulation that ends mid-transfer is the case where disagreeing counters
/// lose the bytes outright.
class counter_probe : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(counter_probe);

    counter_probe(sc_core::sc_module_name name,
                  const chip_noc_endpoint& endpoint, sc_core::sc_time at)
        : sc_core::sc_module(name)
        , endpoint_(endpoint)
        , at_(at)
    {
        SC_THREAD(run);
    }

    bool sampled = false;
    std::uint64_t chunks = 0;
    std::uint64_t bytes = 0;

private:
    void run()
    {
        sc_core::wait(at_);
        chunks = endpoint_.outbound_chunks();
        bytes = endpoint_.outbound_bytes();
        sampled = true;
    }

    const chip_noc_endpoint& endpoint_;
    sc_core::sc_time at_;
};

/// Calls `reset()` at a chosen instant.
class resetter : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(resetter);

    resetter(sc_core::sc_module_name name, chip_noc_endpoint& endpoint,
             sc_core::sc_time at)
        : sc_core::sc_module(name)
        , endpoint_(endpoint)
        , at_(at)
    {
        SC_THREAD(run);
    }

    bool fired = false;

private:
    void run()
    {
        sc_core::wait(at_);
        endpoint_.reset();
        fired = true;
    }

    chip_noc_endpoint& endpoint_;
    sc_core::sc_time at_;
};

} // namespace

int sc_main(int, char*[])
{
    check_configuration_refusals();
    check_no_split_below_the_frame_limit();
    check_aligned_split();
    check_unaligned_first_chunk();
    check_partial_failure_stops_and_reports();
    check_local_containment();
    check_multi_region_span_is_refused_before_splitting();
    check_subregion_boundary_is_refused();
    check_oversized_mmio_is_not_split();
    check_split_counted_when_the_first_chunk_fails();
    check_partial_metrics_are_direction_specific();
    check_zero_progress_failure_is_not_partial();
    check_zero_progress_target_error_is_counted();
    check_inbound_partial_failures_are_counted();
    check_report_covers_both_directions();
    check_transfer_latency_is_logical_time();
    check_failed_read_leaves_the_buffer_untouched();
    check_masked_read_preserves_disabled_bytes();
    check_masked_bytes_are_not_counted_as_moved();
    check_inbound_route_and_split();
    check_debug_path();

    // ── the D23 reset rule ───────────────────────────────────────────────────
    //
    // Two populations the frozen rule names, and neither is visible without a
    // process: a transfer with a chunk **inside** a downstream call, and a
    // transfer **waiting out an incoming quantum** before it reaches one.
    //
    // Built here and run under the single `sc_start` below, because SystemC
    // refuses a module created after it.

    // (a) reset lands while the last chunk is in flight. The old code re-checked
    // the generation only at the top of the loop, so the finished chunk was
    // counted into freshly cleared counters and a reset on the final chunk
    // returned TLM_OK for a transfer it had abandoned.
    bench inflight("inflight");
    inflight.mesh.service = sc_core::sc_time(200, sc_core::SC_NS);
    transfer_thread inflight_xfer{"inflight_xfer", inflight.chip_side,
                                  am::global_ram_base,
                                  static_cast<unsigned>(kFrame * 3)};
    resetter inflight_reset{"inflight_reset", inflight.endpoint,
                            sc_core::sc_time(500, sc_core::SC_NS)};

    // (b) reset lands while the transfer is catching up on its own quantum,
    // before it has reached the interconnect at all. A delay handed downstream
    // is slept inside a call this component cannot interrupt.
    bench quantum("quantum");
    transfer_thread quantum_xfer{"quantum_xfer", quantum.chip_side,
                                 am::global_ram_base, 8,
                                 sc_core::sc_time(3, sc_core::SC_US)};
    resetter quantum_reset{"quantum_reset", quantum.endpoint,
                           sc_core::sc_time(500, sc_core::SC_NS)};

    // (c) **E-1.** Bytes must be published with the chunk that moved them, so
    // that `bytes` and `chunks` agree at every instant a reader can observe —
    // not merely once the transfer has unwound. Sampled from a separate
    // process while the transfer is parked inside a downstream call, which is
    // the only way to see the disagreement at all.
    bench conserve("conserve");
    conserve.mesh.service = sc_core::sc_time(200, sc_core::SC_NS);
    transfer_thread conserve_xfer{"conserve_xfer", conserve.chip_side,
                                  am::global_ram_base,
                                  static_cast<unsigned>(kFrame * 4)};
    // 500 ns: chunks one and two have returned, chunk three is inside the
    // target's wait.
    counter_probe conserve_probe{"conserve_probe", conserve.endpoint,
                                 sc_core::sc_time(500, sc_core::SC_NS)};

    // (d) `downstream_spends_delay = false`, the loosely-timed configuration.
    // The endpoint must leave the caller's annotation alone and never suspend:
    // consuming it would destroy the temporal decoupling the fast backend
    // exists to provide. The flag was added with a paragraph explaining why it
    // was needed and then read by nothing.
    chip_endpoint_config lt = endpoint_config();
    lt.downstream_spends_delay = false;
    chip_noc_endpoint lt_endpoint{"lt_endpoint", lt};
    master lt_chip{"lt_chip"};
    master lt_noc_in{"lt_noc_in"};
    recorder lt_mesh{"lt_mesh"};
    recorder lt_chip_target{"lt_chip_target"};
    lt_chip.socket.bind(lt_endpoint.from_chip);
    lt_noc_in.socket.bind(lt_endpoint.from_noc);
    lt_endpoint.to_noc.bind(lt_mesh.socket);
    lt_endpoint.to_chip.bind(lt_chip_target.socket);
    transfer_thread lt_xfer{"lt_xfer", lt_chip, am::global_ram_base, 8,
                            sc_core::sc_time(3, sc_core::SC_US)};

    sc_core::sc_start(sc_core::sc_time(200, sc_core::SC_US));

    CHECK_MSG(lt_xfer.finished && lt_xfer.status == tlm::TLM_OK_RESPONSE,
              "the loosely-timed transfer did not complete");
    CHECK_MSG(lt_xfer.returned_at == sc_core::SC_ZERO_TIME,
              "with downstream_spends_delay = false the endpoint suspended "
              "anyway: it returned at " + lt_xfer.returned_at.to_string()
                  + " instead of immediately, so the annotation became a real "
                    "wait and temporal decoupling is gone");
    CHECK_MSG(!lt_xfer.went_backwards,
              "the loosely-timed transfer lost part of its annotation");

    CHECK_MSG(conserve_probe.sampled, "the mid-flight counter probe never ran");
    CHECK_MSG(conserve_probe.chunks == 2,
              "the probe was meant to catch the transfer with two chunks "
              "complete and a third in flight; it saw "
                  + std::to_string(conserve_probe.chunks)
                  + " chunks, so the check below would prove nothing");
    CHECK_MSG(conserve_probe.bytes == conserve_probe.chunks * kFrame,
              "mid-flight the endpoint reported "
                  + std::to_string(conserve_probe.chunks) + " chunks and "
                  + std::to_string(conserve_probe.bytes)
                  + " bytes. Bytes must be published with the chunk that moved "
                    "them: publishing them only when the whole transfer "
                    "unwinds makes the two disagree for as long as a transfer "
                    "is suspended, and loses them entirely when a simulation "
                    "ends in flight -- which is endpoint-boundary conservation "
                    "failing");
    CHECK_MSG(conserve_xfer.finished
                  && conserve_xfer.status == tlm::TLM_OK_RESPONSE,
              "the conservation transfer did not complete");
    CHECK_MSG(conserve.endpoint.outbound_bytes() == kFrame * 4
                  && conserve.endpoint.outbound_chunks() == 4,
              "the finished transfer's totals are wrong; per-chunk publication "
              "must not double-count or lose the last chunk");

    CHECK_MSG(inflight_reset.fired && quantum_reset.fired,
              "a reset never ran");

    CHECK_MSG(inflight_xfer.finished,
              "the transfer reset mid-flight never returned");
    CHECK_MSG(inflight_xfer.status != tlm::TLM_OK_RESPONSE,
              "a transfer abandoned by a reset reported success. Reset cannot "
              "unwind a blocked call, so the chunk finishes -- but the "
              "transfer it belongs to was abandoned and must say so (D23)");
    CHECK_MSG(inflight.endpoint.outbound_transfers() == 0
                  && inflight.endpoint.outbound_chunks() == 0
                  && inflight.endpoint.outbound_bytes() == 0,
              "an old-epoch chunk repopulated counters the reset had cleared");
    CHECK_MSG(inflight.endpoint.outbound_latency_samples() == 0
                  && inflight.endpoint.outbound_latency_total()
                      == sc_core::SC_ZERO_TIME,
              "a transfer abandoned by a reset contributed a latency sample "
              "into the new epoch");

    // The blocking path, where simulated time really does advance: four
    // chunks, 200 ns of service and 1 ns of annotation each.
    CHECK_MSG(conserve.endpoint.outbound_latency_samples() == 1,
              "the completed transfer contributed no latency sample");
    CHECK_MSG(conserve.endpoint.outbound_latency_total()
                  == sc_core::sc_time(804, sc_core::SC_NS),
              "a transfer through a blocking target measured "
                  + conserve.endpoint.outbound_latency_total().to_string()
                  + ", expected 804 ns -- 4 x (200 ns waited + 1 ns "
                    "annotated). Logical time has to pick up both");

    CHECK_MSG(quantum_xfer.finished,
              "the transfer waiting out its quantum never returned");
    CHECK_MSG(quantum_xfer.status != tlm::TLM_OK_RESPONSE,
              "a transfer abandoned while catching up on its quantum reported "
              "success");
    CHECK_MSG(quantum_xfer.returned_at < sc_core::sc_time(1, sc_core::SC_US),
              "the reset did not reach a transfer waiting out its quantum: it "
              "returned at " + quantum_xfer.returned_at.to_string()
                  + " rather than promptly after the 500 ns reset, so it slept "
                    "out the whole 3 us and could still have entered the mesh "
                    "afterwards");
    CHECK_MSG(quantum.mesh.seen.empty(),
              "an abandoned transfer entered the mesh after the reset");
    CHECK_MSG(!quantum_xfer.went_backwards && !inflight_xfer.went_backwards,
              "an abandoned transfer came back with less delay than it had "
              "left to spend, moving a decoupled caller into its own past");

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_chip_noc_endpoint: all checks passed\n";
    return 0;
}
