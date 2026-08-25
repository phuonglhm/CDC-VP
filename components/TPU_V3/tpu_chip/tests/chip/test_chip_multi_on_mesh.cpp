// SPDX-License-Identifier: Apache-2.0
//
// Phase 9 multi-chip contention: **two** dual-core chips, two NoC endpoints, a
// real `noc_interconnect`, a shared boot ROM and shared global RAM.
//
// `TPU_V3_PHASE9_NOC_REBASELINE.md` §6 asks for "2x2 concurrent inter-chip
// traffic completing without deadlock or response misattribution". The
// single-chip gate next door (`test_chip_on_mesh`) cannot answer either half:
// with one manager there is nothing to contend with, and with one destination
// a misattributed response is indistinguishable from a correct one.
//
// ## What makes misattribution *visible* here
//
// A response goes to the wrong caller only when there is another caller to
// confuse it with, and it is only detectable when the two would have received
// **different bytes**. Both properties are built in rather than hoped for:
//
//   * four harts, four distinct `mhartid` values, each written by the hart
//     itself into its own SRAM and its sibling's. Chip 0 holds 0 and 1, chip 1
//     holds 2 and 3 — so a swap between chips is a wrong number, not a missing
//     one;
//   * the two chips DMA **different regions** of global RAM, concurrently, into
//     their own SRAMs. The RAM pattern has period 0x50, and the two source
//     offsets are chosen so the regions do not alias.
//
// A test that moved the same bytes to both chips would pass with the responses
// swapped, which is the failure it exists to catch.
//
// ## Concurrency is measured, not assumed — and the threshold is not "> 0"
//
// The two DMAs run in separate processes and `contention_watch` samples the
// interconnect while both are live. The run fails unless it caught them
// overlapping for at least 20 samples of 100 ns, and that floor is the part
// worth explaining: an earlier draft required only a single sample and passed
// on **three** — 300 ns of overlap, which is not a contention measurement. It
// would swing to zero on any timing change and could not tell a real
// regression to sequential traffic from noise.
//
// What produces the overlap is the **shared RAM latency**, not the payload
// size and not the synchronised start. Both of those were tried; the numbers
// and what each one was worth are recorded at `ram_target` and at
// `dma_driver::run`, because two of the three things that look like they
// should matter here do not.
//
// ## The boot image is chip-agnostic, and it has to be
//
// `test_chip_on_mesh` boots an image with chip 1's SRAM base compiled in
// (`lui 0xC8000`). Two chips share one boot ROM, so the image here derives its
// addresses from `mhartid`:
//
//     mhartid = chip * 2 + core                        (tpu_core.cpp:117)
//     core SRAM = 0xC000_0000 + chip<<27 + core<<25    (address_map.h)
//
// Assembled with the D4 toolchain and checked against `am::core_sram_base()`
// for all four harts before being pasted in; the source is in the comment above
// `kBootProgram`.
//
// Usage: test_chip_multi_on_mesh [fast|detailed]

#include "tpu_v3/chip/tpu_chip.h"
#include "tpu_v3/noc/chip_noc_endpoint.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
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

/// Chips 0 and 1. Both are real; neither is a stand-in for the other.
constexpr tpu::chip_id_t kChipA = 0;
constexpr tpu::chip_id_t kChipB = 1;
constexpr std::uint64_t kCapacity = 64 * 1024;

/// Chip-agnostic boot image. Source, assembled with
/// `riscv-none-elf-as -march=rv32i_zicsr`:
///
/// ```asm
///     csrr  t0, mhartid
///     srli  t1, t0, 1            # chip
///     andi  t2, t0, 1            # core
///     lui   t3, 0xC0000
///     slli  t4, t1, 27           # chip * 0x0800_0000
///     add   t3, t3, t4
///     slli  t4, t2, 25           # core * 0x0200_0000
///     add   t3, t3, t4           # own core SRAM base
///     sw    t0, 0(t3)
///     xori  t5, t2, 1            # sibling core
///     lui   t4, 0xC0000
///     slli  t6, t1, 27
///     add   t4, t4, t6
///     slli  t6, t5, 25
///     add   t4, t4, t6           # sibling core SRAM base
///     sw    t0, 0x100(t4)
///     lui   t6, 0x1
///     addi  t6, t6, -2048
///     csrw  mie, t6
///     wfi
/// 1:  j 1b
/// ```
///
/// Both stores are **chip-local**: they cross the chip fabric, never the mesh.
/// That is what makes the flit count below a containment measurement.
constexpr std::uint32_t kBootProgram[] = {
    0xF14022F3u, 0x0012D313u, 0x0012F393u, 0xC0000E37u, 0x01B31E93u,
    0x01DE0E33u, 0x01939E93u, 0x01DE0E33u, 0x005E2023u, 0x0013CF13u,
    0xC0000EB7u, 0x01B31F93u, 0x01FE8EB3u, 0x019F1F93u, 0x01FE8EB3u,
    0x105EA023u, 0x00001FB7u, 0x800F8F93u, 0x304F9073u, 0x10500073u,
    0x0000006Fu,
};

constexpr std::uint64_t kOwnMark = 0x0;
constexpr std::uint64_t kSiblingMark = 0x100;

/// Where each chip's concurrent DMA lands, clear of the boot marks above.
constexpr std::uint64_t kDmaDst = 0x1000;
/// **4 KiB, not 512.** Each transfer is then several frames rather than one,
/// so it occupies the mesh long enough for the overlap witness to have margin.
/// At 512 bytes the two chips overlapped for about 300 ns out of a 2 ms
/// window -- true, but three samples wide, which is a measurement that would
/// swing to zero on any timing change and could not tell a real regression to
/// sequential traffic from noise.
constexpr std::uint32_t kMove = 4096;

/// Source offsets in global RAM, one per chip.
///
/// **Not adjacent, and not a multiple of the pattern period.** The RAM pattern
/// repeats every 0x50 bytes, so two sources 0x50 apart would put identical
/// bytes in both chips and a swapped response would read as correct. 0x4000 is
/// 0x40 into the period, so the two windows share no byte position.
constexpr std::uint64_t kSrcA = 0x0000;
constexpr std::uint64_t kSrcB = 0x4000;

/// Round 2: the two chips read **each other's** core SRAM, at the same time,
/// in opposite directions. Both sources are unaligned and both lengths odd —
/// the shape D25's `mmio` aperture will not have widened, so the endpoint has
/// to reshape rather than forward.
constexpr std::uint64_t kInterDst = 0x3000;
constexpr std::uint64_t kInterSrcA = kDmaDst + 3;   ///< in chip B's SRAM
constexpr std::uint64_t kInterSrcB = kDmaDst + 7;   ///< in chip A's SRAM
/// Odd, and **past the 2048-byte frame** once the unaligned start is counted,
/// so the inter-chip path exercises chunking as well as reshaping — and so the
/// two transfers last long enough to be seen overlapping. At 37 and 53 bytes
/// they were over before the sampler's second tick: round 2 measured 1 sample.
constexpr std::uint32_t kInterLenA = 3001;
constexpr std::uint32_t kInterLenB = 2003;

tpu_chip_config chip_config(tpu::chip_id_t id)
{
    tpu_chip_config config;
    config.chip = id;
    config.cycle = sc_core::sc_time(10, sc_core::SC_NS);
    // `annotated`: an `arbitrated` fabric on the inbound path would stall the
    // one process that advances the mesh clock (D16, plan §21 item 10).
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

chip_endpoint_config endpoint_config(tpu::chip_id_t id,
                                     noc_interconnect::timing_mode mode)
{
    chip_endpoint_config config;
    config.chip = id;
    config.max_frame_bytes = 2048;
    config.bus_bytes = 8;
    config.max_inbound_sram_bytes = 64;
    config.downstream_spends_delay =
        mode == noc_interconnect::timing_mode::detailed;
    return config;
}

/// Boot ROM on its own mesh node, shared by both chips.
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
        if (trans.get_command() != tlm::TLM_READ_COMMAND) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        std::memcpy(trans.get_data_ptr(), &storage_[offset], length);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    std::vector<unsigned char> storage_;
};

/// Global RAM, shared. The pattern is what tells the two chips' DMA payloads
/// apart when they are read back.
class ram_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<ram_target> socket;
    std::uint64_t accesses = 0;
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;

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
        } else {
            std::memcpy(&storage_[offset], trans.get_data_ptr(), length);
            ++writes;
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        // **2 us per access, and this is the knob that makes contention
        // measurable at all.**
        //
        // A DMA here reads global RAM in whole frames and writes its own core
        // SRAM, which never touches the mesh — so the mesh occupancy of one
        // transfer is a couple of RAM reads and nothing else. Measured while
        // tuning this: at 2 ns the two chips overlapped for ~300 ns, and
        // raising the payload eightfold only took that to ~600 ns, because the
        // payload was never what the transfers were waiting on.
        //
        // Making the *shared* resource slow is what puts both chips in the
        // mesh at once, and it is the honest model besides: a global DRAM two
        // chips queue behind is exactly the situation §6 asks about. Nothing
        // about correctness depends on this number -- only whether the run has
        // a contention window wide enough to measure.
        delay += sc_core::sc_time(2, sc_core::SC_US);
    }

    std::vector<unsigned char> storage_;
};

/// The host: one upstream port, reaching both chips the only way anything
/// outside can — through the mesh, ejecting at each chip's node.
class host_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<host_master> socket;

    explicit host_master(sc_core::sc_module_name name)
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

    /// Reads `length` bytes from `address`, whatever their alignment.
    ///
    /// **Three constraints, and the first one is the one that bites.** A chip
    /// aperture is `mmio`, and the interconnect refuses a read it would have
    /// to widen: an 8-byte-shaped read whose address or length is not a
    /// multiple of 8 comes back refused (`noc_interconnect.cpp`, the D25
    /// rule). The host is a plain master with no endpoint to reshape for it,
    /// so it must ask only for spans the mesh will carry — this reads the
    /// enclosing 8-aligned span and trims. A first draft asked for 56 bytes at
    /// a 4-aligned address and was refused.
    ///
    /// Second, a single request may not exceed the NoC frame limit, so the
    /// span is issued in 1 KiB pieces.
    ///
    /// Third, one request per kilobyte rather than one per word: the payload
    /// checks used to walk 8 KiB in 2048 separate four-byte mesh round trips,
    /// which buried the run's own flit count under verification traffic.
    std::vector<unsigned char> read_bytes(std::uint64_t address,
                                          unsigned length)
    {
        const std::uint64_t start = address & ~std::uint64_t{7};
        const std::uint64_t end = (address + length + 7) & ~std::uint64_t{7};
        std::vector<unsigned char> raw;
        raw.reserve(static_cast<std::size_t>(end - start));
        for (std::uint64_t a = start; a < end;) {
            const unsigned n =
                static_cast<unsigned>(std::min<std::uint64_t>(1024, end - a));
            std::vector<unsigned char> chunk(n, 0);
            const auto status = access(tlm::TLM_READ_COMMAND, a, chunk);
            CHECK_MSG(status == tlm::TLM_OK_RESPONSE,
                      "a host block read of " + std::to_string(n)
                          + " bytes was refused");
            raw.insert(raw.end(), chunk.begin(), chunk.end());
            a += n;
        }
        const unsigned skew = static_cast<unsigned>(address - start);
        return {raw.begin() + skew, raw.begin() + skew + length};
    }

    std::uint32_t read32(std::uint64_t address)
    {
        std::vector<unsigned char> bytes(4, 0);
        const auto status = access(tlm::TLM_READ_COMMAND, address, bytes);
        CHECK_MSG(status == tlm::TLM_OK_RESPONSE,
                  "a host 4-byte read was refused; it is naturally aligned and "
                  "must survive the mmio target kind a chip aperture carries "
                  "(D25, ADDRESS_MAP.md §7)");
        std::uint32_t value = 0;
        std::memcpy(&value, bytes.data(), 4);
        return value;
    }

    void write32(std::uint64_t address, std::uint32_t value)
    {
        std::vector<unsigned char> bytes(4);
        std::memcpy(bytes.data(), &value, 4);
        const auto status = access(tlm::TLM_WRITE_COMMAND, address, bytes);
        CHECK_MSG(status == tlm::TLM_OK_RESPONSE,
                  "a host 4-byte MMIO write was refused");
    }
};

/// Every flit either mesh accepted, on every router port. The witness for
/// "nothing local entered the network" has to be the network's own passive
/// counters — a counter the endpoint maintains could be wrong in exactly the
/// way the endpoint is wrong.
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

/// Programs one chip's DMA across the mesh and waits for it.
///
/// One instance per chip, both released from the same event, so the two chips'
/// transfers are in the mesh at the same time. That is the scenario §6 names,
/// and a sequential driver cannot produce it.
class dma_driver : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(dma_driver);

    dma_driver(sc_core::sc_module_name name, host_master& host,
               tpu::chip_id_t chip, std::uint64_t src, std::uint64_t dst,
               std::uint32_t length, sc_core::sc_event& go,
               sc_core::sc_event& fire)
        : sc_core::sc_module(name)
        , host_(host)
        , chip_(chip)
        , src_(src)
        , dst_(dst)
        , length_(length)
        , go_(go)
        , fire_(fire)
    {
        SC_THREAD(run);
    }

    bool started = false;
    bool armed = false;
    bool finished = false;
    bool ok = false;
    sc_core::sc_time began{sc_core::SC_ZERO_TIME};
    sc_core::sc_time ended{sc_core::SC_ZERO_TIME};

private:
    void run()
    {
        // **Two phases, and round 2 depends on it.** Both drivers share the
        // host's single upstream port, so six register writes each are six
        // serialised mesh round trips; programming both first and releasing
        // the two `START` writes together cuts the stagger to one write.
        //
        // Whether that matters was measured twice, and the answer changed:
        //
        //   round 1 (chip <-> global RAM)   53 samples with, 49 without
        //   round 2 (chip <-> chip)         25 samples with, 18 without
        //
        // Round 1 does not need it — the shared 2 us RAM latency dominates
        // everything, and an earlier version of this comment generalised that
        // into "a tightening, not the mechanism". Round 2 has no slow shared
        // resource, so its transfers are short enough that the programming
        // stagger is a large fraction of them, and removing this phase takes
        // it below the floor. The claim is now stated per round, because it is
        // true of one and not the other.
        sc_core::wait(go_);
        began = sc_core::sc_time_stamp();
        started = true;
        program();
        armed = true;
        sc_core::wait(fire_);
        ok = fire_and_wait();
        ended = sc_core::sc_time_stamp();
        finished = true;
    }

    void program()
    {
        namespace reg = tpu::dma::reg;
        const std::uint64_t base = am::dma_control(chip_, 0);
        host_.write32(base + reg::src_addr_lo,
                      static_cast<std::uint32_t>(src_));
        host_.write32(base + reg::src_addr_hi,
                      static_cast<std::uint32_t>(src_ >> 32));
        host_.write32(base + reg::dst_addr_lo,
                      static_cast<std::uint32_t>(dst_));
        host_.write32(base + reg::dst_addr_hi,
                      static_cast<std::uint32_t>(dst_ >> 32));
        host_.write32(base + reg::length, length_);
    }

    bool fire_and_wait()
    {
        namespace reg = tpu::dma::reg;
        namespace ctl = tpu::dma::control_bit;
        namespace st = tpu::dma::status_bit;
        const std::uint64_t base = am::dma_control(chip_, 0);
        host_.write32(base + reg::control, ctl::start);

        // Bounded: a DMA that never completes has to fail its own check rather
        // than spin until the outer watchdog kills the run, or "no deadlock"
        // becomes a claim about the watchdog.
        for (unsigned poll = 0; poll < 4000; ++poll) {
            const std::uint32_t status = host_.read32(base + reg::status);
            if ((status & st::done) != 0) {
                host_.write32(base + reg::status, st::done);
                return true;
            }
            if ((status & st::error) != 0) {
                std::cerr << "  chip " << unsigned{chip_} << " DMA error cause "
                          << host_.read32(base + reg::error_cause) << '\n';
                return false;
            }
            sc_core::wait(1, sc_core::SC_US);
        }
        return false;
    }

    host_master& host_;
    tpu::chip_id_t chip_;
    std::uint64_t src_;
    std::uint64_t dst_;
    std::uint32_t length_;
    sc_core::sc_event& go_;
    sc_core::sc_event& fire_;
};

/// Samples the interconnect while both DMAs are meant to be running.
///
/// **"Concurrent" is a claim about the run, so it is measured.** Without this
/// the two drivers could serialise — one draining before the other is admitted
/// — and every check below would still pass, on a scenario that never put two
/// chips' traffic in the mesh together.
struct contention_sample {
    bool taken = false;
    unsigned outstanding_a = 0;
    unsigned outstanding_b = 0;
    unsigned peak_a = 0;
    unsigned peak_b = 0;
    std::uint64_t both_busy_samples = 0;
    std::uint64_t both_in_endpoint_samples = 0;
};

class contention_watch : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(contention_watch);

    contention_watch(sc_core::sc_module_name name, const noc_interconnect& noc,
                     const chip_noc_endpoint& a, const chip_noc_endpoint& b,
                     sc_core::sc_event& go, contention_sample& out)
        : sc_core::sc_module(name)
        , noc_(noc)
        , a_(a)
        , b_(b)
        , go_(go)
        , out_(out)
    {
        SC_THREAD(run);
    }

private:
    void run()
    {
        sc_core::wait(go_);
        // 100 ns, and the period is load-bearing: a detailed-mode transfer of
        // this size lives for a few microseconds, so a 1 us sampler can step
        // straight over the window in which both chips are inside the mesh.
        for (unsigned i = 0; i < 20000; ++i) {
            sc_core::wait(100, sc_core::SC_NS);
            // **Both managers admitted to the mesh, and nothing weaker.**
            //
            // This used to read `oa > 0 || a_.outbound_in_flight() > 0`, and
            // that OR accepted the exact case the check exists to exclude: a
            // caller blocked in the admission gate has already incremented the
            // endpoint's in-flight count, so a serialisation regression that
            // parks port B *before* admission would still have shown both
            // managers "busy" while `ob == 0`. The predicate was broader than
            // the claim it supported.
            //
            // `outstanding_transactions(port)` is the right witness because
            // D26 makes a routed call own its admission slot from admission
            // until `b_transport` returns — so it is non-zero exactly while
            // that manager is in the network.
            const auto oa = noc_.outstanding_transactions(0);
            const auto ob = noc_.outstanding_transactions(1);
            if (oa > 0 && ob > 0) {
                ++out_.both_busy_samples;
            }
            // Kept as a separate figure rather than folded into the predicate:
            // a caller inside the endpoint but not yet admitted is a real
            // state, and one worth being able to see when the numbers above
            // disagree with expectation.
            if (a_.outbound_in_flight() > 0 && b_.outbound_in_flight() > 0) {
                ++out_.both_in_endpoint_samples;
            }
            out_.outstanding_a = std::max(out_.outstanding_a, oa);
            out_.outstanding_b = std::max(out_.outstanding_b, ob);
            out_.taken = true;
        }
        out_.peak_a = noc_.peak_outstanding_transactions(0);
        out_.peak_b = noc_.peak_outstanding_transactions(1);
    }

    const noc_interconnect& noc_;
    const chip_noc_endpoint& a_;
    const chip_noc_endpoint& b_;
    sc_core::sc_event& go_;
    contention_sample& out_;
};
/// Everything that has to touch the mesh, in a process.
///
/// **Not in `sc_main` after `sc_start`.** The detailed backend spends simulated
/// time inside `b_transport`, so a read issued from `sc_main` calls `wait()`
/// outside any process and the kernel refuses it (E519) — and `fast` hides
/// that, because nothing there waits. The single-chip gate records the same
/// constraint; this file was written against it and still had to learn it
/// twice.
class verifier : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(verifier);

    verifier(sc_core::sc_module_name name, host_master& host,
             const ram_target& ram, dma_driver& a, dma_driver& b,
             dma_driver& ia, dma_driver& ib, sc_core::sc_event& go,
             sc_core::sc_event& fire, sc_core::sc_event& go2,
             sc_core::sc_event& fire2, bool detailed)
        : sc_core::sc_module(name)
        , host_(host)
        , ram_(ram)
        , a_(a)
        , b_(b)
        , ia_(ia)
        , ib_(ib)
        , go_(go)
        , fire_(fire)
        , go2_(go2)
        , fire2_(fire2)
        , detailed_(detailed)
    {
        SC_THREAD(run);
    }

    bool done() const { return done_; }

private:
    void run()
    {
        // Both chips boot first: every instruction fetch is a mesh round trip,
        // and starting the transfers underneath that traffic would make the
        // accounting below unattributable.
        sc_core::wait(500, sc_core::SC_US);
        check_boot_marks();

        run_round(go_, fire_, a_, b_, "global-RAM");
        check_dma_payloads();

        // ── round 2: traffic **between** the chips, concurrently ───────────
        //
        // Round 1 put both chips on the mesh at once, but both were reading
        // global RAM — multi-chip traffic to a shared target, not inter-chip
        // traffic. §6 asks for the second, and running the one chip-to-chip
        // transfer alone after everything else had drained (which the first
        // version of this file did) answers neither the deadlock nor the
        // misattribution question for that path.
        //
        // So both chips now read *each other's* core SRAM at the same time,
        // in opposite directions, each at an unaligned source and odd length.
        run_round(go2_, fire2_, ia_, ib_, "inter-chip");
        check_inter_chip_payloads();
        done_ = true;
        // **The simulation's tail is what this run costs, not the readback.**
        // Everything above finishes inside a couple of milliseconds; the
        // `sc_start` bound exists only so a hang fails instead of stalling the
        // suite. Left to run, four harts step 60 ms of simulated time for
        // nothing -- measured at ~113 s per case, unchanged by cutting the
        // verification traffic by more than half.
        sc_core::sc_stop();
    }

    /// Releases two drivers together and waits for both, bounded.
    ///
    /// Programming first and firing second keeps the six register writes each
    /// driver needs — six serialised mesh round trips on the host's single
    /// port — out of the middle of the other one's transfer.
    void run_round(sc_core::sc_event& go, sc_core::sc_event& fire,
                   dma_driver& x, dma_driver& y, const char* what)
    {
        go.notify(sc_core::SC_ZERO_TIME);
        for (unsigned i = 0; i < 20000 && !(x.armed && y.armed); ++i) {
            sc_core::wait(1, sc_core::SC_US);
        }
        CHECK_MSG(x.armed && y.armed,
                  std::string("a ") + what
                      + " DMA driver never finished programming its registers");
        fire.notify(sc_core::SC_ZERO_TIME);
        // Each driver has its own bounded poll, so a hang here is a driver
        // that never returned rather than a mesh that never answered.
        for (unsigned i = 0; i < 40000 && !(x.finished && y.finished); ++i) {
            sc_core::wait(1, sc_core::SC_US);
        }
        CHECK_MSG(x.finished && y.finished,
                  std::string("a ") + what + " DMA driver never finished");
        CHECK_MSG(x.ok && y.ok,
                  std::string("a ") + what
                      + " DMA reported an error or timed out");
    }

    /// Four harts, four distinct `mhartid` values, read back across the mesh.
    /// A response delivered to the wrong chip is a wrong number here, not a
    /// missing one.
    void check_boot_marks()
    {
        struct expectation {
            tpu::chip_id_t chip;
            tpu::core_id_t core;
            std::uint32_t own;
            std::uint32_t sibling;
        };
        const std::array<expectation, 4> expected{{
            {kChipA, 0, 0, 1},
            {kChipA, 1, 1, 0},
            {kChipB, 0, 2, 3},
            {kChipB, 1, 3, 2},
        }};
        for (const auto& e : expected) {
            const std::uint64_t base = am::core_sram_base(e.chip, e.core);
            const auto own = host_.read32(base + kOwnMark);
            const auto sib = host_.read32(base + kSiblingMark);
            CHECK_MSG(own == e.own,
                      "chip " + std::to_string(unsigned{e.chip}) + " core "
                          + std::to_string(unsigned{e.core})
                          + " holds mhartid " + std::to_string(own) + " where "
                          + std::to_string(e.own)
                          + " was written. Either that hart did not boot, or a "
                            "response was delivered to the wrong chip");
            CHECK_MSG(sib == e.sibling,
                      "chip " + std::to_string(unsigned{e.chip}) + " core "
                          + std::to_string(unsigned{e.core})
                          + " holds sibling mark " + std::to_string(sib)
                          + " where " + std::to_string(e.sibling)
                          + " was written by the sibling hart on that chip");
        }
    }

    /// The load-bearing check for response misattribution on the data path.
    /// The two sources do not alias, so a swap is visible byte for byte — and
    /// it is asserted in both directions: each chip has its own window, and
    /// does **not** have the other's.
    void check_dma_payloads()
    {
        const std::array<std::pair<tpu::chip_id_t, std::uint64_t>, 2> work{
            {{kChipA, kSrcA}, {kChipB, kSrcB}}};
        for (const auto& [chip, src] : work) {
            const std::uint64_t sram = am::core_sram_base(chip, 0) + kDmaDst;
            const std::uint64_t other = src == kSrcA ? kSrcB : kSrcA;
            bool matched = true;
            bool matched_other = true;
            const auto got = host_.read_bytes(sram, kMove);
            for (unsigned i = 0; i < kMove; ++i) {
                if (got[i] != ram_.at(src + i)) {
                    matched = false;
                }
                if (got[i] != ram_.at(other + i)) {
                    matched_other = false;
                }
            }
            CHECK_MSG(matched,
                      "chip " + std::to_string(unsigned{chip})
                          + " did not receive the global RAM window its own "
                            "DMA was programmed with");
            CHECK_MSG(!matched_other,
                      "chip " + std::to_string(unsigned{chip})
                          + " received the *other* chip's window. That is a "
                            "response delivered to the wrong manager, which is "
                            "exactly what concurrent traffic on one mesh has "
                            "to rule out");
        }
    }

    /// Both inter-chip transfers, checked byte for byte.
    ///
    /// Each source is deliberately **not** naturally aligned and each length is
    /// odd, because that is the shape a chip aperture's `mmio` kind refuses to
    /// have widened (D25) and the one the endpoint has to reshape rather than
    /// forward. Control MC-1 confirms the path: disabling the shaping branch
    /// turns these into `error_cause 8`.
    ///
    /// **Each destination is that DMA's own core SRAM.** Revision 1 implements
    /// exactly `local -> external` and `external -> local`; core 1's SRAM is on
    /// the same chip but reached through the chip fabric, so it is external
    /// too, and an earlier draft aimed there and was refused with
    /// `error_cause 3`. "Same chip" and "local to this DMA" differ.
    ///
    /// **Compared against global RAM, not against what the other chip holds.**
    /// Comparing with the source chip would let two errors cancel: it fetches
    /// the wrong window, this one faithfully copies it, and the check passes on
    /// data wrong at both ends.
    void check_inter_chip_payloads()
    {
        struct leg {
            const char* name;
            tpu::chip_id_t dst_chip;
            std::uint64_t dst;      ///< absolute, in the reading chip's SRAM
            std::uint64_t ram_src;  ///< where those bytes came from originally
            unsigned length;
        };
        const std::array<leg, 2> legs{{
            {"chip A <- chip B", kChipA,
             am::core_sram_base(kChipA, 0) + kInterDst, kSrcB + 3, kInterLenA},
            {"chip B <- chip A", kChipB,
             am::core_sram_base(kChipB, 0) + kInterDst + 4, kSrcA + 7,
             kInterLenB},
        }};
        for (const auto& l : legs) {
            const auto got = host_.read_bytes(l.dst, l.length);
            bool ok = true;
            unsigned first_bad = l.length;
            for (unsigned i = 0; i < l.length; ++i) {
                if (got[i] != ram_.at(l.ram_src + i)) {
                    ok = false;
                    if (first_bad == l.length) {
                        first_bad = i;
                    }
                }
            }
            CHECK_MSG(ok,
                      std::string(l.name)
                          + " delivered the wrong bytes, first at offset "
                          + std::to_string(first_bad) + " of "
                          + std::to_string(l.length)
                          + ". An unaligned source of odd length is reshaped "
                            "by the endpoint before it crosses the mesh, and a "
                            "lane error there survives a `done` status");
        }
    }

    host_master& host_;
    const ram_target& ram_;
    dma_driver& a_;
    dma_driver& b_;
    dma_driver& ia_;
    dma_driver& ib_;
    sc_core::sc_event& go_;
    sc_core::sc_event& fire_;
    sc_core::sc_event& go2_;
    sc_core::sc_event& fire2_;
    bool detailed_;
    bool done_ = false;
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
            std::cerr << "usage: test_chip_multi_on_mesh [fast|detailed]\n";
            return 1;
        }
    }
    const bool detailed = mode == noc_interconnect::timing_mode::detailed;

    // Four targets, three managers, four nodes:
    //
    //   (0,0)  chip A endpoint  + chip A aperture (owner 0)
    //   (1,0)  chip B endpoint  + chip B aperture (owner 1)
    //   (0,1)  host             + boot ROM        (owner 2)
    //   (1,1)  global RAM
    //
    // **The host shares the ROM's node deliberately.** Every node is taken, and
    // pairing the host with the ROM is the one combination that costs nothing:
    // the host never fetches instructions, so the bypass it gains is unused,
    // while every contended path — both chips to the ROM, both chips to RAM,
    // the host to both apertures — still routes.
    noc_interconnect noc("noc", 2, 2, /*num_targets=*/4, /*num_initiators=*/3,
                         sc_core::sc_time(1, sc_core::SC_NS),
                         /*max_outstanding_per_port=*/32, mode);

    tpu_chip chip_a("chip_a", chip_config(kChipA));
    tpu_chip chip_b("chip_b", chip_config(kChipB));
    chip_noc_endpoint endpoint_a("endpoint_a", endpoint_config(kChipA, mode));
    chip_noc_endpoint endpoint_b("endpoint_b", endpoint_config(kChipB, mode));
    rom_target rom("rom");
    ram_target ram("ram");
    host_master host("host");

    noc.place_initiator(0, {0, 0});
    noc.place_initiator(1, {1, 0});
    noc.place_initiator(2, {0, 1});

    chip_a.external().bind(endpoint_a.from_chip);
    endpoint_a.to_noc.bind(noc.cpu_port(0));
    endpoint_a.to_chip.bind(chip_a.inbound());

    chip_b.external().bind(endpoint_b.from_chip);
    endpoint_b.to_noc.bind(noc.cpu_port(1));
    endpoint_b.to_chip.bind(chip_b.inbound());

    host.socket.bind(noc.cpu_port(2));

    noc.add_target(am::chip_base(kChipA), am::chip_aperture_stride, {0, 0},
                   noc_interconnect::target_kind::mmio, /*local_owner=*/0)
        .bind(endpoint_a.from_noc);
    noc.add_target(am::chip_base(kChipB), am::chip_aperture_stride, {1, 0},
                   noc_interconnect::target_kind::mmio, /*local_owner=*/1)
        .bind(endpoint_b.from_noc);
    noc.add_target(am::boot_rom_base, am::boot_rom_size, {0, 1},
                   noc_interconnect::target_kind::memory, /*local_owner=*/2)
        .bind(rom.socket);
    noc.add_target(am::global_ram_base, am::global_ram_window, {1, 1},
                   noc_interconnect::target_kind::memory)
        .bind(ram.socket);

    sc_core::sc_event go;
    sc_core::sc_event fire;
    sc_core::sc_event go2;
    sc_core::sc_event fire2;
    contention_sample watch{};
    contention_sample watch2{};

    // Round 1: both chips read global RAM. Contention on a shared target.
    dma_driver dma_a("dma_a", host, kChipA, am::global_ram_base + kSrcA,
                     am::core_sram_base(kChipA, 0) + kDmaDst, kMove, go, fire);
    dma_driver dma_b("dma_b", host, kChipB, am::global_ram_base + kSrcB,
                     am::core_sram_base(kChipB, 0) + kDmaDst, kMove, go, fire);
    // Round 2: each chip reads the other's core SRAM. Traffic **between** the
    // chips, in both directions at once — which is what §6 names and what
    // round 1 does not provide.
    dma_driver inter_a("inter_a", host, kChipA,
                       am::core_sram_base(kChipB, 0) + kInterSrcA,
                       am::core_sram_base(kChipA, 0) + kInterDst, kInterLenA,
                       go2, fire2);
    dma_driver inter_b("inter_b", host, kChipB,
                       am::core_sram_base(kChipA, 0) + kInterSrcB,
                       am::core_sram_base(kChipB, 0) + kInterDst + 4,
                       kInterLenB, go2, fire2);
    contention_watch watcher("watcher", noc, endpoint_a, endpoint_b, fire,
                             watch);
    contention_watch watcher2("watcher2", noc, endpoint_a, endpoint_b, fire2,
                              watch2);
    verifier runtime("runtime", host, ram, dma_a, dma_b, inter_a, inter_b, go,
                     fire, go2, fire2, detailed);

    std::cout << "multi-chip-on-mesh, NoC timing " << mode_name << '\n';

    sc_core::sc_start(sc_core::sc_time(60, sc_core::SC_MS));

    CHECK_MSG(runtime.done(),
              "the watchdog expired with the scenario outstanding");

    // ── the two DMAs really overlapped ──────────────────────────────────────
    CHECK_MSG(dma_a.started && dma_b.started, "a DMA driver never started");
    CHECK_MSG(dma_a.finished && dma_b.finished,
              "a DMA driver never finished: with a bounded poll of its own, "
              "that is a transfer the mesh never completed");
    CHECK_MSG(dma_a.ok && dma_b.ok,
              "a concurrent DMA reported an error or timed out");
    CHECK_MSG(watch.taken, "the contention watch never sampled");
    // **Overlap is a detailed-mode property, and requiring it in both would be
    // asserting something false.** `fast` annotates and never blocks, so no two
    // calls are ever inside the interconnect at one instant — there is no queue
    // to contend for. Demanding overlap there would fail a backend behaving
    // exactly as a loosely timed backend must, and the same split is why the
    // single-chip gate asserts in-flight peak 2 detailed / 1 fast.
    if (detailed) {
        CHECK_MSG(watch.both_busy_samples >= 20,
                  "the two chips' transfers overlapped in the mesh for fewer "
                  "than 20 samples of 100 ns. A handful of samples is not a "
                  "contention measurement: it swings to zero on any timing "
                  "change, and it cannot tell a real regression to sequential "
                  "traffic from noise. Saw "
                      + std::to_string(watch.both_busy_samples));
    } else {
        CHECK_MSG(watch.both_busy_samples == 0,
                  "the fast backend showed two managers inside the "
                  "interconnect at once. Nothing there blocks, so an overlap "
                  "means something suspended where it should have annotated");
    }
    std::cout << "  round 1 (chip <-> global RAM): overlap "
              << watch.both_busy_samples << " samples, endpoint-busy "
              << watch.both_in_endpoint_samples << ", max outstanding port0 "
              << watch.outstanding_a << " port1 " << watch.outstanding_b
              << '\n'
              << "  round 2 (chip <-> chip):      overlap "
              << watch2.both_busy_samples << " samples, endpoint-busy "
              << watch2.both_in_endpoint_samples << ", max outstanding port0 "
              << watch2.outstanding_a << " port1 " << watch2.outstanding_b
              << '\n';

    // **Round 2 is the one §6 actually names.** Round 1 is two chips against a
    // shared target; this is traffic between the chip apertures themselves, in
    // both directions at once, and it is where a deadlock or a response
    // delivered to the wrong aperture would show.
    CHECK_MSG(inter_a.finished && inter_b.finished && inter_a.ok && inter_b.ok,
              "an inter-chip DMA did not complete. Two chips reading each "
              "other's SRAM at once is the case §6 names, and it is also the "
              "one where a routing or ordering deadlock would appear");
    if (detailed) {
        CHECK_MSG(watch2.both_busy_samples >= 20,
                  "the two inter-chip transfers overlapped in the mesh for "
                  "fewer than 20 samples of 100 ns, so this run measured them "
                  "one after the other. Saw "
                      + std::to_string(watch2.both_busy_samples));
    }

    std::cout << "  boot ROM fetches " << rom.fetches
              << ", global RAM accesses " << ram.accesses
              << ", accepted flits "
              << (detailed ? std::to_string(total_accepted_flits(noc))
                           : std::string("n/a (fast)"))
              << '\n'
              << "  endpoint A outbound " << endpoint_a.outbound_transfers()
              << " transfers / " << endpoint_a.outbound_bytes() << " bytes;"
              << " inbound " << endpoint_a.inbound_transfers() << " / "
              << endpoint_a.inbound_bytes() << " bytes\n"
              << "  endpoint B outbound " << endpoint_b.outbound_transfers()
              << " transfers / " << endpoint_b.outbound_bytes() << " bytes;"
              << " inbound " << endpoint_b.inbound_transfers() << " / "
              << endpoint_b.inbound_bytes() << " bytes\n";

    // ── both endpoints carried traffic in both directions ───────────────────
    //
    // A zero on either side would mean one chip sat out the whole run while the
    // other did the work, and every check above would still pass.
    for (const auto* e : {&endpoint_a, &endpoint_b}) {
        CHECK_MSG(e->outbound_transfers() > 0,
                  "an endpoint presented no outbound transfer at all");
        CHECK_MSG(e->inbound_transfers() > 0,
                  "an endpoint received no inbound transfer at all");
        CHECK_MSG(e->outbound_local_refused() == 0,
                  "a chip-local address reached a NoC endpoint. The chip "
                  "fabric answers those itself, and with D1 in place the "
                  "bypass would quietly deliver it back into the chip");
        CHECK_MSG(e->protocol_errors() == 0,
                  "an endpoint counted a protocol error");
    }

    CHECK_MSG(chip_a.fabric().self_refused() == 0
                  && chip_b.fabric().self_refused() == 0,
              "a core addressed its own aperture through a chip fabric");

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_chip_multi_on_mesh: all checks passed\n";
    return 0;
}
