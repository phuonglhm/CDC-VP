// SPDX-License-Identifier: Apache-2.0
//
// Phase 9 composition: one dual-core chip, its NoC endpoint, a **real**
// `noc_interconnect`, boot ROM and global RAM.
//
// Everything below this point has been proved separately. `tpu_chip` boots two
// harts (Phase 8), `chip_noc_endpoint` splits and contains (Phase 9 component
// gate), `noc_interconnect` carries flits and is RTL cross-checked. What none
// of those gates can show is what happens when they are wired together, and
// the Phase 7 and Phase 8 audits both record that composition is where the
// interesting defects live.
//
// Plan §16's Phase 9 gate asks four things of this composition, and each check
// below names the one it answers:
//
//   * local SRAM/MMIO traffic injects **zero** NoC flits, remote traffic routes
//     normally;
//   * transfers larger than the NoC frame limit are chunked with defined
//     partial-failure semantics;
//   * a missing or wrong local-owner mapping fails at elaboration (that one is
//     `test_noc_interconnect_bad_config`'s, and is named here so the division
//     is visible rather than assumed);
//   * concurrent traffic completes without deadlock or response
//     misattribution.
//
// **The chip's own aperture is a target on the chip's own node.** That is the
// configuration `NoLoopback = 1` refuses outright, and it is legal here only
// through D1's owner-aware bypass: the aperture is registered naming port 0,
// the port the chip's endpoint injects from. It is also why the harts can boot
// at all — their reset PC is in `GLOBAL_BOOT_ROM`, which is now a mesh hop
// away rather than a socket away.
//
// Usage: test_chip_on_mesh [fast|detailed]

#include "tpu_v3/chip/tpu_chip.h"
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

#include "floo_noc_model/noc_interconnect.h"
#include "tpu_v3/address_map.h"
#include "tpu_v3/dma/dma_registers.h"

namespace tpu = cdc::components::tpu_v3;
namespace chip = cdc::components::tpu_v3::chip;
namespace core = cdc::components::tpu_v3::core;
namespace am = cdc::components::tpu_v3::address_map;

using cdc::components::noc_interconnect;
using chip::tpu_chip;
using chip::tpu_chip_config;
using core::local_fabric_timing;
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

/// Chip 1, so `mhartid` is 2 and 3 — never 0, which is what an uninitialised
/// CSR reads as.
constexpr tpu::chip_id_t kChip = 1;
constexpr std::uint64_t kCapacity = 64 * 1024;

/// The same boot image the Phase 8 chip gate uses: each hart writes its own
/// `mhartid` into its own SRAM and into its sibling's, then parks in `wfi`.
/// Both of those writes are **chip-local**, which is the point here — they must
/// produce no flit at all.
constexpr std::uint32_t kBootProgram[] = {
    0xF14022F3u, 0x0012F313u, 0xC80003B7u, 0x01931E13u, 0x01C38E33u,
    0x005E2023u, 0x00134E93u, 0x019E9F13u, 0x01E38F33u, 0x105F2023u,
    0x00001FB7u, 0x800F8F93u, 0x304F9073u, 0x10500073u, 0x0000006Fu,
};

constexpr std::uint64_t kOwnMark = 0x0;
constexpr std::uint64_t kSiblingMark = 0x100;

/// Bytes the chip's own DMA moves in each direction across the mesh.
constexpr std::uint32_t kMove = 512;

tpu_chip_config chip_config()
{
    tpu_chip_config config;
    config.chip = kChip;
    config.cycle = sc_core::sc_time(10, sc_core::SC_NS);
    // `annotated`, and it is not a preference. Plan §21 item 10: an
    // `arbitrated` fabric on the inbound path would stall the one process that
    // advances the mesh clock (D16).
    config.timing = chip::chip_fabric_timing::annotated;
    for (auto& c : config.core) {
        c.sram_capacity_bytes = kCapacity;
        c.fabric.data_width_bits = 128;
        c.fabric.bank_count = 4;
        c.fabric.mapping = tpu::bank_mapping::low_order_interleaved;
        c.fabric.pipeline_stages = 1;
        c.fabric.max_outstanding_per_requester = 1;
        c.fabric.arbitration = tpu::arbitration_policy::round_robin;
        c.timing = local_fabric_timing::annotated;
        c.cycle = sc_core::sc_time(10, sc_core::SC_NS);
        c.reset_pc = am::boot_rom_base;
    }
    return config;
}

chip_endpoint_config endpoint_config()
{
    chip_endpoint_config config;
    config.chip = kChip;
    config.max_frame_bytes = 2048;
    config.bus_bytes = 8;
    config.max_inbound_sram_bytes = 64;
    return config;
}

/// Boot ROM on its own mesh node. Every instruction the harts execute crosses
/// the network to get here.
class rom_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<rom_target> socket;
    std::uint64_t fetches = 0;

    explicit rom_target(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(am::boot_rom_size, 0)
    {
        for (unsigned i = 0; i < std::size(kBootProgram); ++i) {
            std::memcpy(&storage_[i * 4], &kBootProgram[i], 4);
        }
        socket.register_b_transport(this, &rom_target::b_transport);
        socket.register_transport_dbg(this, &rom_target::transport_dbg);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time&)
    {
        ++fetches;
        serve(trans);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        serve(trans);
        return trans.get_data_length();
    }

    void serve(tlm::tlm_generic_payload& trans)
    {
        const std::uint64_t offset = trans.get_address();
        const unsigned length = trans.get_data_length();
        if (offset + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(trans.get_data_ptr(), &storage_[offset], length);
        } else {
            // The ROM refuses ordinary writes; the loader uses debug.
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    std::vector<unsigned char> storage_;
};

/// Global RAM on a third node, with a recognisable pattern.
class ram_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<ram_target> socket;
    std::uint64_t accesses = 0;
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;

    explicit ram_target(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(128 * 1024)
    {
        for (std::size_t i = 0; i < storage_.size(); ++i) {
            storage_[i] = static_cast<unsigned char>(0x40 + (i % 0x50));
        }
        socket.register_b_transport(this, &ram_target::b_transport);
    }

    unsigned char at(std::size_t i) const { return storage_[i]; }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        ++accesses;
        const std::uint64_t offset = trans.get_address();
        const unsigned length = trans.get_data_length();
        if (offset + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_READ_COMMAND) {
            std::memcpy(trans.get_data_ptr(), &storage_[offset], length);
            ++reads;
            read_bytes += length;
        } else {
            std::memcpy(&storage_[offset], trans.get_data_ptr(), length);
            ++writes;
            write_bytes += length;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(2, sc_core::SC_NS);
    }

    std::vector<unsigned char> storage_;
};

/// A remote master on its own upstream port. It reaches the chip the only way
/// anything outside can: through the mesh, ejecting at the chip's node into the
/// endpoint's inbound socket.
class remote_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<remote_master> socket;

    explicit remote_master(sc_core::sc_module_name name)
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
};

/// Every flit either mesh accepted. The witness for "nothing local entered the
/// network" has to be the network's own passive counters, not anything the
/// endpoint maintains: a counter the endpoint increments could be wrong in
/// exactly the way the endpoint is wrong.
std::uint64_t total_accepted_flits(const noc_interconnect& noc)
{
    const auto snapshot = noc.detailed_counter_snapshot();
    std::uint64_t total = 0;
    for (const auto* mesh : {&snapshot.request, &snapshot.response}) {
        for (const auto& router : mesh->routers) {
            for (const auto& in : router.inputs) {
                total += in.accepted_flits;
            }
            for (const auto& out : router.outputs) {
                total += out.accepted_flits;
            }
        }
    }
    return total;
}

/// Drives everything that has to happen while the kernel is running.
///
/// The mesh consumes simulated time, so none of this works from `sc_main`
/// after `sc_start` returns: a read issued there would never be answered.
class scenario : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(scenario);

    scenario(sc_core::sc_module_name name, remote_master& remote,
             chip_noc_endpoint& endpoint, const ram_target& ram)
        : sc_core::sc_module(name)
        , remote_(remote)
        , endpoint_(endpoint)
        , ram_(ram)
    {
        SC_THREAD(run);
    }

    bool ran() const { return ran_; }

private:
    std::uint32_t read32(std::uint64_t address)
    {
        std::vector<unsigned char> bytes(4, 0);
        const auto status =
            remote_.access(tlm::TLM_READ_COMMAND, address, bytes);
        CHECK_MSG(status == tlm::TLM_OK_RESPONSE,
                  "a remote 4-byte read was refused; it is naturally aligned "
                  "and must survive the mmio target kind a chip aperture "
                  "carries (ADDRESS_MAP.md §7)");
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data(), 4);
        return value;
    }

    void run()
    {
        // Long enough for both harts to reach `wfi`; every one of their
        // fetches is a mesh round trip.
        sc_core::wait(400, sc_core::SC_US);

        // ── the harts booted, read back the only way anything outside can ──
        //
        // This is the inbound path end to end: remote master, mesh, eject at
        // the chip's node, endpoint rebase from aperture-relative to absolute,
        // chip fabric, core SRAM.
        for (tpu::core_id_t c = 0; c < tpu::cores_per_chip; ++c) {
            const std::uint32_t expect = kChip * 2 + c;
            CHECK_MSG(read32(am::core_sram_base(kChip, c) + kOwnMark) == expect,
                      "core " + std::to_string(c)
                          + " did not write its own mhartid into its own "
                            "SRAM; either it never booted, or the inbound "
                            "route delivered the read to the wrong core");
            const std::uint32_t sibling = kChip * 2 + (c == 0 ? 1 : 0);
            CHECK_MSG(read32(am::core_sram_base(kChip, c) + kSiblingMark)
                          == sibling,
                      "core " + std::to_string(c)
                          + "'s SRAM does not carry its sibling's mark, so "
                            "core-to-core traffic inside the chip did not "
                            "land where it was addressed");
        }

        // ── inbound chunking, against the core's real native limit ──────────
        //
        // A NEO-CORE's external bridge refuses an inbound SRAM access longer
        // than `neo_max_transfer_bytes`. This is 4x that, so it only succeeds
        // if the endpoint split it — and the bytes have to come back.
        const std::uint64_t big = am::core_sram_base(kChip, 0) + 0x400;
        std::vector<unsigned char> payload(256);
        for (std::size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<unsigned char>(0xC0 + (i % 0x30));
        }
        const auto chunks_before = endpoint_.inbound_chunks();
        CHECK_MSG(remote_.access(tlm::TLM_WRITE_COMMAND, big, payload)
                      == tlm::TLM_OK_RESPONSE,
                  "a 256-byte inbound SRAM write was refused; the endpoint "
                  "must split it to what a core's native plane accepts");
        CHECK_MSG(endpoint_.inbound_chunks() - chunks_before >= 4,
                  "a 256-byte inbound write produced fewer than four chunks, "
                  "so it was not split at the 64-byte native limit");

        std::vector<unsigned char> back(payload.size(), 0);
        CHECK_MSG(remote_.access(tlm::TLM_READ_COMMAND, big, back)
                      == tlm::TLM_OK_RESPONSE,
                  "reading the inbound-written block back was refused");
        CHECK_MSG(back == payload,
                  "the block read back does not match what was written, so "
                  "the inbound split did not reassemble");

        // ── chip-originated traffic to global RAM ───────────────────────
        //
        // Everything above this point touches only the boot ROM and core SRAM,
        // so global RAM was a mapped address nothing exercised — the test
        // would have passed with its node or its route wrong. The chip's own
        // DMA moves bytes both ways across the endpoint and the mesh.
        const std::uint64_t ram_src = am::global_ram_base + 0x800;
        const std::uint64_t sram_dst = am::core_sram_base(kChip, 0) + 0x2000;

        CHECK_MSG(run_dma(ram_src, sram_dst, kMove),
                  "the DMA did not complete a global RAM -> core SRAM "
                  "transfer; the chip could not reach global RAM through its "
                  "endpoint and the mesh");

        std::vector<unsigned char> moved(kMove, 0);
        CHECK_MSG(remote_.access(tlm::TLM_READ_COMMAND, sram_dst, moved)
                      == tlm::TLM_OK_RESPONSE,
                  "reading back what the DMA fetched was refused");
        bool matched = true;
        for (std::uint32_t i = 0; i < kMove; ++i) {
            if (moved[i] != ram_.at(0x800 + i)) {
                matched = false;
                break;
            }
        }
        CHECK_MSG(matched,
                  "the bytes the DMA fetched from global RAM do not match "
                  "what global RAM holds, so the transfer went somewhere else "
                  "or was reassembled wrong");

        // And the other direction, so the RAM is written as well as read.
        const std::uint64_t ram_dst = am::global_ram_base + 0x4000;
        CHECK_MSG(run_dma(sram_dst, ram_dst, kMove),
                  "the DMA did not complete a core SRAM -> global RAM "
                  "transfer");
        std::vector<unsigned char> written(kMove, 0);
        CHECK_MSG(remote_.access(tlm::TLM_READ_COMMAND, ram_dst, written)
                      == tlm::TLM_OK_RESPONSE,
                  "reading global RAM back through the mesh was refused");
        CHECK_MSG(written == moved,
                  "what the DMA wrote into global RAM is not what it had in "
                  "core SRAM");

        ran_ = true;
    }

    /// Programs core 0's DMA through its MMIO window — reached, like
    /// everything else here, across the mesh — and waits for it to finish.
    ///
    /// This is what makes global RAM more than a mapped address. Until the
    /// chip itself moves bytes to and from it, the RAM's placement and route
    /// are unexercised and the test would pass with either one wrong.
    bool run_dma(std::uint64_t src, std::uint64_t dst, std::uint32_t length)
    {
        namespace reg = tpu::dma::reg;
        namespace ctl = tpu::dma::control_bit;
        namespace st = tpu::dma::status_bit;
        const std::uint64_t base = am::dma_control(kChip, 0);

        write32(base + reg::src_addr_lo, static_cast<std::uint32_t>(src));
        write32(base + reg::src_addr_hi,
                static_cast<std::uint32_t>(src >> 32));
        write32(base + reg::dst_addr_lo, static_cast<std::uint32_t>(dst));
        write32(base + reg::dst_addr_hi,
                static_cast<std::uint32_t>(dst >> 32));
        write32(base + reg::length, length);
        write32(base + reg::control, ctl::start);

        // Bounded: a DMA that never completes must fail the check rather than
        // spin this thread until the outer watchdog kills the run.
        for (unsigned poll = 0; poll < 4000; ++poll) {
            const std::uint32_t status = read32(base + reg::status);
            if ((status & st::done) != 0) {
                write32(base + reg::status, st::done);
                return true;
            }
            if ((status & st::error) != 0) {
                std::cerr << "  DMA error cause "
                          << read32(base + reg::error_cause) << '\n';
                return false;
            }
            sc_core::wait(1, sc_core::SC_US);
        }
        return false;
    }

    void write32(std::uint64_t address, std::uint32_t value)
    {
        std::vector<unsigned char> bytes(4);
        std::memcpy(bytes.data(), &value, 4);
        const auto status =
            remote_.access(tlm::TLM_WRITE_COMMAND, address, bytes);
        CHECK_MSG(status == tlm::TLM_OK_RESPONSE,
                  "a remote 4-byte MMIO write was refused");
    }

    remote_master& remote_;
    chip_noc_endpoint& endpoint_;
    const ram_target& ram_;
    bool ran_ = false;
};

} // namespace

int sc_main(int argc, char* argv[])
{
    auto mode = noc_interconnect::timing_mode::fast;
    std::string mode_name = "fast";
    if (argc > 1) {
        mode_name = argv[1];
        if (mode_name == "detailed") {
            mode = noc_interconnect::timing_mode::detailed;
        } else if (mode_name != "fast") {
            std::cerr << "usage: test_chip_on_mesh [fast|detailed]\n";
            return 1;
        }
    }

    noc_interconnect noc("noc", 2, 2, /*num_targets=*/3, /*num_initiators=*/2,
                         sc_core::sc_time(1, sc_core::SC_NS),
                         /*max_outstanding_per_port=*/32, mode);
    tpu_chip chip_under_test("chip", chip_config());
    chip_noc_endpoint endpoint("endpoint", endpoint_config());
    rom_target rom("rom");
    ram_target ram("ram");
    remote_master remote("remote");

    // The chip's endpoint is port 0 and sits at (0,0); the remote master is
    // port 1 at (1,0).
    noc.place_initiator(0, {0, 0});
    noc.place_initiator(1, {1, 0});

    chip_under_test.external().bind(endpoint.from_chip);
    endpoint.to_noc.bind(noc.cpu_port(0));
    endpoint.to_chip.bind(chip_under_test.inbound());
    remote.socket.bind(noc.cpu_port(1));

    // **The co-located target.** This chip's own aperture lives on the chip's
    // own node, which `NoLoopback = 1` refuses unless the mapping names the
    // port that owns it (D1). `mmio` is the kind `ADDRESS_MAP.md` §7 fixes for
    // a chip aperture, and D25 is why a remote read of a core's SRAM survives
    // that.
    noc.add_target(am::chip_base(kChip), am::chip_aperture_stride, {0, 0},
                   noc_interconnect::target_kind::mmio, /*local_owner=*/0)
        .bind(endpoint.from_noc);
    noc.add_target(am::boot_rom_base, am::boot_rom_size, {0, 1},
                   noc_interconnect::target_kind::memory)
        .bind(rom.socket);
    noc.add_target(am::global_ram_base, am::global_ram_window, {1, 1},
                   noc_interconnect::target_kind::memory)
        .bind(ram.socket);

    scenario runtime("runtime", remote, endpoint, ram);

    std::cout << "chip-on-mesh, NoC timing " << mode_name << '\n';

    sc_core::sc_start(sc_core::sc_time(8, sc_core::SC_MS));

    CHECK_MSG(runtime.ran(),
              "the watchdog expired with the scenario outstanding");

    // Router counters exist only in detailed mode; fast never builds a mesh to
    // count. Asking for them there throws, which is the right answer and not
    // one to work around by pretending the number is zero.
    const bool detailed = mode == noc_interconnect::timing_mode::detailed;
    const auto flits = detailed ? total_accepted_flits(noc) : 0;
    std::cout << "  boot ROM fetches " << rom.fetches
              << ", global RAM accesses " << ram.accesses
              << ", accepted flits "
              << (detailed ? std::to_string(flits) : std::string("n/a (fast)"))
              << '\n'
              << "  endpoint outbound " << endpoint.outbound_transfers()
              << " transfers -> " << endpoint.outbound_chunks() << " chunks, "
              << endpoint.outbound_bytes() << " bytes; local refused "
              << endpoint.outbound_local_refused() << '\n'
              << "  endpoint inbound  " << endpoint.inbound_transfers()
              << " transfers -> " << endpoint.inbound_chunks() << " chunks, "
              << endpoint.inbound_bytes() << " bytes; foreign refused "
              << endpoint.inbound_foreign_refused() << '\n';

    // ── the harts booted, and they booted *through the mesh* ────────────────
    CHECK_MSG(rom.fetches > 0,
              "no instruction fetch reached the boot ROM. The harts' reset PC "
              "is in GLOBAL_BOOT_ROM, which is a mesh hop away here, so a "
              "chip that boots without touching it is not booting");
    CHECK_MSG(endpoint.outbound_transfers() > 0,
              "nothing left the chip through its endpoint");

    if (detailed) {
        CHECK_MSG(flits > 0,
                  "remote traffic created no flits in detailed mode, so "
                  "nothing actually traversed the network");
    }

    // ── local containment, the gate's first line ────────────────────────────
    //
    // Both harts write their own SRAM and their sibling's. Every one of those
    // is chip-local, and the chip fabric answers them without ever offering
    // them to `external()`.
    //
    // **This assertion is defence in depth and cannot fail here**, which is
    // stated rather than left for a reader to assume from its presence. The
    // enforcing layer is `chip_local_fabric`, one level down, and it has two
    // of its own: a window that decodes locally, and — for an address inside
    // the aperture matching no window — an explicit refusal rather than
    // `outside`. Both were broken by hand to look for a control:
    //
    //   * disabling the endpoint's own containment check changes nothing,
    //     because no chip-local address ever arrives to be checked;
    //   * deleting core 1's window from the fabric's decode makes the run fail
    //     loudly — a runaway of 210541 outbound transfers and a hart that
    //     never writes its mark — but `outbound_local_refused()` still reads
    //     zero, because the second fabric layer catches it.
    //
    // So containment is gated by `tpu_v3_chip_fabric`, and what this
    // composition adds is the positive half below: local traffic demonstrably
    // happened, and none of it reached the endpoint or the mesh.
    CHECK_MSG(endpoint.outbound_local_refused() == 0,
              "a chip-local address reached the NoC endpoint. The chip fabric "
              "answers those itself; one arriving here means its decoder is "
              "wrong, and with D1 in place the bypass would quietly deliver it "
              "back into the chip");
    CHECK_MSG(chip_under_test.fabric().self_refused() == 0,
              "a core addressed its own aperture through the chip fabric");

    // The positive half of containment. `local_refused == 0` alone is also
    // what a chip that generated no local traffic at all would report, so on
    // its own it is a check that cannot fail. Both harts write their own SRAM
    // and their sibling's, so the fabric must have answered core-to-core
    // traffic itself — and none of it appears in the endpoint's totals.
    CHECK_MSG(chip_under_test.fabric().local_bypass() > 0,
              "the chip fabric answered no core-to-core access, so the "
              "containment check above had nothing to contain and proves "
              "nothing");

    // ── global RAM was actually used ────────────────────────────────────────
    //
    // Not decoration. Until the chip itself moved bytes to and from global
    // RAM, the RAM was a mapped address nothing exercised and this test would
    // have passed with its mesh node or its route wrong.
    // Asserted **exactly**, and the inequality that used to stand here is the
    // reason. `read_bytes >= kMove` passes just as happily if the network
    // duplicated a read — which is precisely the failure this document claims
    // conservation rules out, so the loose form let the claim outrun the
    // check. None of these five depends on scheduling: the scenario issues a
    // fixed set of accesses, and the only thing that could change the counts
    // is traffic nobody asked for.
    //
    //   DMA leg 1  read  512 from global RAM
    //   DMA leg 2  write 512 into global RAM
    //   remote master reads those 512 back, straight through the mesh
    CHECK_MSG(ram.accesses == 3,
              "global RAM saw " + std::to_string(ram.accesses)
                  + " accesses, expected exactly 3 — two DMA legs and the "
                    "remote read-back. Any other number is traffic nobody in "
                    "this scenario asked for");
    CHECK_MSG(ram.reads == 2,
              "global RAM served " + std::to_string(ram.reads)
                  + " reads, expected exactly 2");
    CHECK_MSG(ram.writes == 1,
              "global RAM took " + std::to_string(ram.writes)
                  + " writes, expected exactly 1");
    CHECK_MSG(ram.read_bytes == 2ull * kMove,
              "global RAM served " + std::to_string(ram.read_bytes)
                  + " bytes to reads, expected "
                  + std::to_string(2ull * kMove)
                  + "; a duplicated or split read would show up here and "
                    "nowhere else");
    CHECK_MSG(ram.write_bytes == kMove,
              "the DMA wrote " + std::to_string(ram.write_bytes)
                  + " bytes into global RAM, not the "
                  + std::to_string(kMove) + " it was told to move");

    // ── conservation across the endpoint boundary (§6 evidence item) ────────
    //
    // Bytes, not transfers: the transfer count cannot reconcile here, and the
    // reason is worth keeping. The remote master reads global RAM directly
    // through the mesh at the end of the scenario, which never crosses this
    // endpoint — so the targets legitimately see one access more than the
    // endpoint forwarded. An earlier version equated the two and failed for
    // exactly that reason.
    //
    // What the endpoint carried outbound is the harts' fetches plus the two
    // DMA legs, and those are counted on the far side of the mesh by targets
    // that know nothing about the endpoint.
    CHECK_MSG(endpoint.outbound_bytes() == rom.fetches * 4 + 2ull * kMove,
              "outbound bytes read "
                  + std::to_string(endpoint.outbound_bytes())
                  + ", expected " + std::to_string(rom.fetches * 4 + 2ull * kMove)
                  + " — the harts' fetches plus both DMA legs. Bytes were lost "
                    "or duplicated crossing the endpoint boundary");
    CHECK_MSG(endpoint.outbound_transfers() == rom.fetches + 2,
              "the endpoint forwarded "
                  + std::to_string(endpoint.outbound_transfers())
                  + " transfers, expected one per fetch plus one per DMA leg");

    // ── inbound accounting ─────────────────────────────────────────────────
    CHECK_MSG(endpoint.inbound_foreign_refused() == 0,
              "an inbound access named an address this chip does not own, so "
              "the mesh delivered a packet to the wrong node");
    CHECK_MSG(endpoint.inbound_bytes() >= 4 * 4 + 256 + 256 + kMove,
              "inbound bytes are below what the scenario demonstrably sent. "
              "The exact total is deliberately not asserted: it also carries "
              "however many times the DMA status register was polled, and "
              "pinning a poll count would make this a test of scheduling");

    // What this composition deliberately does **not** re-prove: outbound
    // chunking at the frame limit. `test_endpoint_on_real_noc` already drives
    // 2047/2048/2049-byte transfers through a real `noc_interconnect` and
    // compares the bytes. Repeating it here would need the chip's DMA
    // programmed by firmware, and would be a second copy of a rule that
    // already has one test.

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_chip_on_mesh: all checks passed\n";
    return 0;
}
