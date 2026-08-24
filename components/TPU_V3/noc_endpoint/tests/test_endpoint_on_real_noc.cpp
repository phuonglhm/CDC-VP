// SPDX-License-Identifier: Apache-2.0
//
// The endpoint against a **real** `noc_interconnect` — decision record D25.
//
// The component's own gate uses TLM stubs, deliberately: the split-and-route
// contract is visible without a mesh. This one exists for the single property
// a stub cannot establish, because the stub is not the thing that enforces it.
//
// `ADDRESS_MAP.md` §7 registers a chip aperture as `target_kind::mmio`, even
// though most of it is core SRAM. `noc_interconnect` then refuses a **read**
// against an `mmio` target whenever the beat frame is full width and *either*
// the address *or* the length is not bus-aligned
// (`noc_interconnect.cpp:2084`), before injection, with the target never
// called.
//
// The length half is what decides the design. `neo_dma`'s Phase 4 contract is
// frozen at lengths 1, 2, 3, 7, 8, 15, 16, 63, 64, 65 and odd addresses, so
// "remote SRAM reads must be bus-aligned" would narrow an already-gated
// contract without a decision. D25 instead shapes the read here:
//
//     naturally-aligned narrow prefix
//   → bus-aligned full-width bulk
//   → naturally-aligned narrow suffix
//
// so every chunk is a shape `shape_of()` keeps narrow, or is bus-aligned at
// both ends. What this file checks is that the shaping is **sufficient** —
// judged by the real interconnect, not by our reading of it — and that the
// bytes reassemble.
//
// Exit codes: 0 pass, 1 fail.

#include "tpu_v3/noc/chip_noc_endpoint.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "floo_noc_model/noc_interconnect.h"
#include "tpu_v3/address_map.h"

namespace am = cdc::components::tpu_v3::address_map;
using cdc::components::noc_interconnect;
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

/// This endpoint is chip 0; it reads chip 1's core SRAM.
constexpr cdc::components::tpu_v3::chip_id_t kLocalChip = 0;
constexpr cdc::components::tpu_v3::chip_id_t kRemoteChip = 1;

/// The interconnect's real frame limit: 256 beats of 8 bytes.
constexpr std::uint64_t kFrame = 2048;

unsigned char pattern_at(std::size_t i)
{
    return static_cast<unsigned char>(0x11 + (i % 0xD7));
}

/// Stands in for a remote chip: byte storage, and a count of how often it was
/// entered, so "the target was never called" is a measurement.
class remote_chip : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<remote_chip> socket;
    std::uint64_t accesses = 0;

    explicit remote_chip(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(0x4000)
    {
        for (std::size_t i = 0; i < storage_.size(); ++i) {
            storage_[i] = pattern_at(i);
        }
        socket.register_b_transport(this, &remote_chip::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        ++accesses;
        // Aperture-relative: the interconnect subtracts the mapped target's
        // base before calling (`noc_interconnect.cpp:1189`).
        const std::uint64_t offset = trans.get_address();
        const unsigned length = trans.get_data_length();
        if (offset + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        for (unsigned i = 0; i < length; ++i) {
            if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                trans.get_data_ptr()[i] = storage_[offset + i];
            } else {
                storage_[offset + i] = trans.get_data_ptr()[i];
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(1, sc_core::SC_NS);
    }

    std::vector<unsigned char> storage_;
};

/// A target that must never be entered, bound where nothing should arrive.
class never_called : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<never_called> socket;
    std::uint64_t accesses = 0;

    explicit never_called(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        socket.register_b_transport(this, &never_called::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        ++accesses;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

struct bench;

/// Drives the whole sweep from one SystemC process: a detailed interconnect
/// spends simulated time, so none of this works before `sc_start`.
class driver : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(driver);
    tlm_utils::simple_initiator_socket<driver> socket;

    driver(sc_core::sc_module_name name, bench& b)
        : sc_core::sc_module(name)
        , socket("socket")
        , bench_(b)
    {
        SC_THREAD(run);
    }

    bool finished = false;

    tlm::tlm_response_status read(std::uint64_t address,
                                  std::vector<unsigned char>& data)
    {
        return access(tlm::TLM_READ_COMMAND, address, data);
    }

private:
    void run();

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

    bench& bench_;
};

chip_endpoint_config endpoint_config()
{
    chip_endpoint_config config;
    config.chip = kLocalChip;
    config.max_frame_bytes = kFrame;
    config.bus_bytes = 8;
    config.max_inbound_sram_bytes = 64;
    return config;
}

struct bench {
    noc_interconnect noc{"noc", 2, 2, /*num_targets=*/3, /*num_initiators=*/1,
                         sc_core::sc_time(1, sc_core::SC_NS),
                         /*max_outstanding=*/4,
                         noc_interconnect::timing_mode::detailed};
    chip_noc_endpoint endpoint{"endpoint", endpoint_config()};
    remote_chip remote{"remote"};
    never_called inbound_sink{"inbound_sink"};
    driver drv{"drv", *this};

    bench()
    {
        noc.place_initiator(0, {0, 0});
        drv.socket.bind(endpoint.from_chip);
        endpoint.to_noc.bind(noc.cpu_port(0));
        endpoint.to_chip.bind(inbound_sink.socket);

        // This chip's own aperture, co-located with its manager port and
        // named through the D1 owner mapping — the wiring a real composition
        // uses. Nothing in this file addresses it, because the endpoint
        // refuses its own aperture before `to_noc` is reached; it is here so
        // the bench is bound the way the platform will be, rather than in a
        // shape that only exists in a test.
        noc.add_target(am::chip_base(kLocalChip), am::chip_aperture_stride,
                       {0, 0}, noc_interconnect::target_kind::mmio,
                       /*local_owner=*/0)
            .bind(endpoint.from_noc);

        // The remote chip's whole aperture, `mmio` exactly as ADDRESS_MAP.md
        // §7 registers it. This is the declaration the whole file is about:
        // declaring it `memory` would make every check here pass for the wrong
        // reason.
        noc.add_target(am::chip_base(kRemoteChip), am::chip_aperture_stride,
                       {1, 1}, noc_interconnect::target_kind::mmio)
            .bind(remote.socket);

        // A third mapped region so the interconnect has somewhere else to
        // decode to; nothing in this file addresses it.
        noc.add_target(am::global_ram_base, am::global_ram_window, {0, 1},
                       noc_interconnect::target_kind::memory)
            .bind(spare_.socket);
    }

private:
    never_called spare_{"spare"};
};

bench* the_bench = nullptr;

void driver::run()
{
    auto& b = bench_;
    const std::uint64_t sram = am::core_sram_base(kRemoteChip, 0);

    // ── the sweep ───────────────────────────────────────────────────────────
    //
    // Every offset across a bus word against every length the frozen DMA
    // contract names, plus the real frame edge. Before D25 the great majority
    // of these were refused by the interconnect before injection.
    const std::uint64_t lengths[] = {1,  2,  3,  7,  8,   15,   16,  63,
                                     64, 65, 127, 128, 2047, 2048, 2049};
    unsigned refused = 0;
    unsigned wrong_data = 0;
    unsigned cases = 0;

    for (std::uint64_t offset = 0; offset < 8; ++offset) {
        for (std::uint64_t length : lengths) {
            ++cases;
            std::vector<unsigned char> data(length, 0);
            const auto status = read(sram + offset, data);
            if (status != tlm::TLM_OK_RESPONSE) {
                if (refused < 4) {
                    std::cerr << "  refused: offset " << offset << " length "
                              << length << '\n';
                }
                ++refused;
                continue;
            }
            for (std::uint64_t i = 0; i < length; ++i) {
                if (data[i] != pattern_at(offset + i)) {
                    if (wrong_data < 4) {
                        std::cerr << "  wrong data: offset " << offset
                                  << " length " << length << " byte " << i
                                  << '\n';
                    }
                    ++wrong_data;
                    break;
                }
            }
        }
    }

    std::cout << "remote SRAM read sweep: " << cases << " cases, " << refused
              << " refused, " << wrong_data << " with wrong data\n";
    CHECK_MSG(refused == 0,
              std::to_string(refused)
                  + " of " + std::to_string(cases)
                  + " remote SRAM reads were refused by the interconnect. A "
                    "chip aperture is registered mmio (ADDRESS_MAP.md §7), so "
                    "a full-width read whose address or length is not "
                    "bus-aligned is refused before injection; D25 requires the "
                    "endpoint to shape reads so that never happens");
    CHECK_MSG(wrong_data == 0,
              std::to_string(wrong_data)
                  + " reads returned wrong bytes. The shaped chunks did not "
                    "reassemble into the transfer the caller asked for");

    // ── remote MMIO is not shaped, and must still be refused ────────────────
    //
    // The shaping is for memory inside an mmio aperture. A control window is
    // mmio in both senses, and turning an illegal wide read of one into
    // several legal register reads would be the same mistake as splitting an
    // oversized MMIO write.
    const auto before = b.remote.accesses;
    std::vector<unsigned char> wide(12, 0);
    const auto mmio_status = read(am::sa_control(kRemoteChip, 0) + 1, wide);
    CHECK_MSG(mmio_status != tlm::TLM_OK_RESPONSE,
              "a misaligned wide read of a remote MMIO window must be refused");
    CHECK_MSG(b.remote.accesses == before,
              "the remote target was entered for a read the interconnect "
              "should have refused before injection");

    // ── nothing arrived on the inbound path ─────────────────────────────────
    CHECK_MSG(b.inbound_sink.accesses == 0,
              "outbound traffic reached the chip-side socket");

    finished = true;
}

} // namespace

int sc_main(int, char*[])
{
    bench b;
    the_bench = &b;

    sc_core::sc_start(sc_core::sc_time(500, sc_core::SC_MS));

    CHECK_MSG(b.drv.finished, "the sweep did not finish");
    std::cout << "endpoint outbound: " << b.endpoint.outbound_transfers()
              << " transfers -> " << b.endpoint.outbound_chunks()
              << " chunks, " << b.endpoint.outbound_bytes() << " bytes\n";

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_endpoint_on_real_noc: all checks passed\n";
    return 0;
}
