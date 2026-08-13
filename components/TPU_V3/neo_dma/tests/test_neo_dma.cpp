// SPDX-License-Identifier: Apache-2.0
//
// NEO DMA tests — the Phase 4 gate.
//
// The DMA is the first TPU_V3 component that both reads and writes memory it
// does not own, on two different planes, asynchronously, while firmware pokes
// its registers. Almost every check here is about one of the three ways that
// goes wrong:
//
//   * **accounting that flatters.** `BYTES_DONE` must count destination bytes
//     committed, not source bytes fetched into the staging buffer and not the
//     payload a chunk merely named. A partial transfer that reports its full
//     length is indistinguishable from a successful one.
//   * **state from a job that no longer exists.** Reset and abort advance an
//     epoch; a worker that resumes afterwards must abandon its job rather than
//     publish `DONE` into a state nobody asked for.
//   * **a path that bypasses the plane it is supposed to use.** Every local
//     byte must appear as a `neo_requester::dma` access on the native fabric,
//     and none must appear at the external stub.
//
// Everything that consumes simulated time runs under a bounded `sc_start`,
// because a DMA that never finishes and a DMA that deadlocks look identical
// from outside.

// The scenarios need one process each and their count is not fixed at compile
// time, so the dynamic-process macro has to be defined before <systemc>.
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "tpu_v3/core/neo_local_sram_fabric.h"
#include "tpu_v3/dma/neo_dma.h"

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
namespace dma = cdc::components::tpu_v3::dma;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

using core::local_fabric_timing;
using core::neo_local_sram_fabric;
using dma::error_cause;
using dma::neo_dma;
using sram::neo_requester;

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
constexpr std::uint64_t kSramCapacity = 64 * 1024;
constexpr std::uint64_t kExternalBase = 0x8000'0000ull;
constexpr std::uint64_t kExternalSize = 256 * 1024;
constexpr int kWatchdogMilliseconds = 2;

sram::core_sram_config sram_config()
{
    sram::core_sram_config config;
    config.base_address = am::core_sram_base(kChip, kCore);
    config.window_bytes = am::core_sram_window;
    config.capacity_bytes = kSramCapacity;
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

dma::neo_dma_config dma_config()
{
    dma::neo_dma_config config;
    config.control_base = am::dma_control(kChip, kCore);
    config.control_size = am::dma_control_size;
    config.sram_base = am::core_sram_base(kChip, kCore);
    config.sram_window = am::core_sram_window;
    config.max_burst_bytes = 2048;
    config.chunk_latency = sc_core::sc_time(10, sc_core::SC_NS);
    return config;
}

/// Chip/global memory on the far side of the external port.
///
/// It is a real target rather than a recorder because the DMA's correctness
/// includes what it *wrote*, and it can be told to fail a chosen address so
/// first-error semantics can be exercised on a chunk that is not the first.
class external_memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<external_memory> socket;

    std::vector<unsigned char> storage;
    std::uint64_t reads = 0;
    std::uint64_t writes = 0;
    std::uint64_t bytes_read = 0;
    std::uint64_t bytes_written = 0;

    /// When set, any access touching this address is refused.
    std::uint64_t fail_at = kNoFailure;
    static constexpr std::uint64_t kNoFailure = ~0ull;
    /// Transactions refused because of `fail_at`.
    std::uint64_t refusals = 0;
    /// Transactions that named an address this target does not hold. A local
    /// byte arriving here would show up as one of these.
    std::uint64_t address_errors = 0;
    /// Largest payload seen.
    unsigned int largest_payload = 0;
    /// Payloads longer than the 8-byte/256-beat frame allows *at their own
    /// lane offset*. Checked here rather than from the outside because the
    /// limit depends on each transaction's address: 2048 bytes is reachable
    /// only at bus alignment and shrinks by one byte per byte of offset.
    std::uint64_t frame_violations = 0;
    /// Annotated per transaction, so the DMA has a delay to consume and a
    /// reset has somewhere to land.
    sc_core::sc_time latency = sc_core::sc_time(5, sc_core::SC_NS);

    /// When non-zero, the target *waits* inside `b_transport` for this long
    /// before committing, and does so only for transactions touching
    /// `block_at`.
    ///
    /// A real TPU_V3 target may never do this (`INTERFACE_CONTRACT.md` §3).
    /// This one is a testbench stub standing in for a slow remote target, and
    /// the wait is the point: "reset arrives while an external transaction is
    /// in flight" cannot be exercised at all if every transaction completes in
    /// zero time. The annotated `latency` above only gives the *initiator*
    /// something to consume afterwards, which is a different window.
    sc_core::sc_time block_for = sc_core::SC_ZERO_TIME;
    std::uint64_t block_at = kNoFailure;
    /// Set while the target is inside such a wait, so a driver can reset at
    /// exactly the right moment instead of guessing a delay.
    bool blocking = false;

    explicit external_memory(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage(kExternalSize, 0)
    {
        socket.register_b_transport(this, &external_memory::b_transport);
    }

    unsigned char at(std::uint64_t address) const
    {
        return storage[address - kExternalBase];
    }
    void poke(std::uint64_t address, unsigned char value)
    {
        storage[address - kExternalBase] = value;
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        largest_payload = std::max(largest_payload, length);
        if (length > dma::external_frame_limit(address)) {
            ++frame_violations;
        }

        if (address < kExternalBase
            || address - kExternalBase >= kExternalSize
            || kExternalSize - (address - kExternalBase) < length) {
            // A local byte arriving here is the leak the gate is looking for,
            // and it shows up as an address error rather than as silence.
            ++address_errors;
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (fail_at != kNoFailure && address <= fail_at
            && fail_at < address + length) {
            ++refusals;
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        if (block_for != sc_core::SC_ZERO_TIME && block_at != kNoFailure
            && address <= block_at && block_at < address + length) {
            blocking = true;
            sc_core::wait(block_for);
            blocking = false;
        }

        const std::size_t offset =
            static_cast<std::size_t>(address - kExternalBase);
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(storage.data() + offset, trans.get_data_ptr(), length);
            ++writes;
            bytes_written += length;
        } else {
            std::memcpy(trans.get_data_ptr(), storage.data() + offset, length);
            ++reads;
            bytes_read += length;
        }
        delay += latency;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

/// Firmware, as a set of register accesses.
class control_master : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<control_master> socket;

    explicit control_master(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status raw(tlm::tlm_command command,
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
        CHECK_MSG(!trans.is_dmi_allowed(),
                  "DMI is disabled platform-wide (INTERFACE_CONTRACT.md §9)");
        return trans.get_response_status();
    }

    tlm::tlm_response_status write(std::uint64_t offset, std::uint32_t value)
    {
        return raw(tlm::TLM_WRITE_COMMAND, base + offset,
                   reinterpret_cast<unsigned char*>(&value), 4);
    }

    std::uint32_t read(std::uint64_t offset)
    {
        std::uint32_t value = 0xDEAD'BEEFu;
        const auto status =
            raw(tlm::TLM_READ_COMMAND, base + offset,
                reinterpret_cast<unsigned char*>(&value), 4);
        CHECK(status == tlm::TLM_OK_RESPONSE);
        return value;
    }

    unsigned int debug_read(std::uint64_t offset, std::uint32_t& value)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_READ_COMMAND);
        trans.set_address(base + offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        return socket->transport_dbg(trans);
    }

    unsigned int debug_write(std::uint64_t offset, std::uint32_t value)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(base + offset);
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&value));
        trans.set_data_length(4);
        return socket->transport_dbg(trans);
    }

    std::uint64_t base = 0;
};

/// Keeps a bank busy so a DMA request has to arbitrate for it.
class bank_pressure : public sc_core::sc_module {
public:
    bank_pressure(sc_core::sc_module_name name, neo_local_sram_fabric& fabric,
                  neo_requester requester, std::uint64_t address,
                  sc_core::sc_time until)
        : sc_core::sc_module(name)
        , fabric_(fabric)
        , requester_(requester)
        , address_(address)
        , until_(until)
    {
        sc_core::sc_spawn([this] { run(); });
    }

    unsigned iterations() const noexcept { return iterations_; }
    /// True once this requester has no request in flight and will issue no
    /// more. Conservation can only be checked at a quiescent point: the fabric
    /// publishes a requester's byte total when `b_access` returns, while the
    /// SRAM counts at each beat, so mid-request the two legitimately differ by
    /// the beat currently draining the pipeline.
    bool finished() const noexcept { return finished_; }

private:
    void run()
    {
        std::array<unsigned char, 16> data{};
        while (sc_core::sc_time_stamp() < until_) {
            sram::neo_local_request request;
            request.requester = requester_;
            request.command = sram::neo_command::write;
            request.address = address_;
            request.size = static_cast<std::uint32_t>(data.size());
            request.data = data.data();

            sram::neo_local_response response;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            fabric_.b_access(request, response, delay);
            if (response.status != sram::neo_status::ok) {
                break;
            }
            ++iterations_;
        }
        finished_ = true;
    }

    neo_local_sram_fabric& fabric_;
    neo_requester requester_;
    std::uint64_t address_;
    sc_core::sc_time until_;
    unsigned iterations_ = 0;
    bool finished_ = false;
};

/// Runs a scenario inside the simulation.
///
/// `sc_main` is not a SystemC process, so it cannot wait — and every DMA job
/// takes simulated time by construction. Each scenario therefore gets a
/// spawned thread and a "did it finish" flag; a scenario that never sets it is
/// a hang, which is what the bounded `sc_start` turns into a failure.
class scenario : public sc_core::sc_module {
public:
    scenario(sc_core::sc_module_name name, std::function<void()> body)
        : sc_core::sc_module(name)
        , body_(std::move(body))
    {
        sc_core::sc_spawn([this] {
            body_();
            finished_ = true;
        });
    }

    bool finished() const noexcept { return finished_; }

private:
    std::function<void()> body_;
    bool finished_ = false;
};

} // namespace

int sc_main(int, char*[])
{
    const std::uint64_t sram_base = am::core_sram_base(kChip, kCore);

    // ── the machine under test ───────────────────────────────────────────────
    sram::core_sram store("core_sram", sram_config());
    neo_local_sram_fabric fabric("local_fabric", fabric_config(), store,
                                 {neo_requester::cpu, neo_requester::dma,
                                  neo_requester::sa, neo_requester::transform},
                                 local_fabric_timing::arbitrated,
                                 sc_core::sc_time(1, sc_core::SC_NS));

    neo_dma engine("neo_dma", dma_config());
    external_memory outside("external_memory");
    control_master firmware("firmware");
    firmware.base = am::dma_control(kChip, kCore);

    sc_core::sc_signal<bool> irq_line("irq_line");

    engine.local.bind(fabric.native_port);
    engine.external.bind(outside.socket);
    engine.irq.bind(irq_line);
    firmware.socket.bind(engine.control);

    // ── a second, isolated machine ───────────────────────────────────────────
    //
    // One case needs `fabric.reset()`, which clears the fabric's counters
    // while `core_sram`'s keep counting. Running it on the main instance would
    // break the lifetime SRAM-versus-fabric reconciliation for every check
    // that follows, so it gets its own SRAM, fabric, DMA and target.
    sram::core_sram store_b("core_sram_b", sram_config());
    neo_local_sram_fabric fabric_b("local_fabric_b", fabric_config(), store_b,
                                   {neo_requester::dma, neo_requester::sa},
                                   local_fabric_timing::arbitrated,
                                   sc_core::sc_time(1, sc_core::SC_NS));
    neo_dma engine_b("neo_dma_b", dma_config());
    external_memory outside_b("external_memory_b");
    control_master firmware_b("firmware_b");
    firmware_b.base = am::dma_control(kChip, kCore);
    sc_core::sc_signal<bool> irq_line_b("irq_line_b");

    engine_b.local.bind(fabric_b.native_port);
    engine_b.external.bind(outside_b.socket);
    engine_b.irq.bind(irq_line_b);
    firmware_b.socket.bind(engine_b.control);

    // Three other fabric users, so the DMA has to arbitrate rather than owning
    // the array. The plan asks for CPU, SA and Transform pressure by name:
    // one contender proves the DMA can wait, three prove it is not merely
    // alternating with a single peer.
    //
    // All three sit on bank 0 — +64 bytes at a 16-byte beat and four banks is
    // the same bank — so they contend with each other as well as with the DMA.
    bank_pressure pressure_sa("pressure_sa", fabric, neo_requester::sa,
                              sram_base + 4096,
                              sc_core::sc_time(40, sc_core::SC_US));
    bank_pressure pressure_cpu("pressure_cpu", fabric, neo_requester::cpu,
                               sram_base + 4096 + 64,
                               sc_core::sc_time(40, sc_core::SC_US));
    bank_pressure pressure_transform("pressure_transform", fabric,
                                     neo_requester::transform,
                                     sram_base + 4096 + 128,
                                     sc_core::sc_time(40, sc_core::SC_US));
    // Contention on the second machine too, so its DMA has to queue for a bank
    // and a reset has a request in flight to interrupt.
    // Bank 0, like the DMA's destination at +40960 ((40960/16) % 4 == 0), but
    // a different address: a pressure requester parked on top of the region
    // under test would overwrite the very bytes the check reads back.
    bank_pressure pressure_b("pressure_b", fabric_b, neo_requester::sa,
                             sram_base + 4096,
                             sc_core::sc_time(40, sc_core::SC_US));

    // ── helpers shared by the scenarios ──────────────────────────────────────

    const auto status = [&] { return firmware.read(dma::reg::status); };
    const auto is_busy = [&] {
        return (status() & dma::status_bit::busy) != 0;
    };

    /// Polls until the job leaves BUSY. Bounded: an unbounded poll on a DMA
    /// that never completes is the hang this suite exists to convert into a
    /// failure.
    const auto wait_idle = [&](const std::string& what) {
        for (unsigned i = 0; i < 20000 && is_busy(); ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK_MSG(!is_busy(), what + ": the job never left BUSY");
    };

    const auto program = [&](std::uint64_t source, std::uint64_t destination,
                             std::uint32_t length) {
        CHECK(firmware.write(dma::reg::src_addr_lo,
                             static_cast<std::uint32_t>(source))
              == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.write(dma::reg::src_addr_hi,
                             static_cast<std::uint32_t>(source >> 32))
              == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.write(dma::reg::dst_addr_lo,
                             static_cast<std::uint32_t>(destination))
              == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.write(dma::reg::dst_addr_hi,
                             static_cast<std::uint32_t>(destination >> 32))
              == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.write(dma::reg::length, length)
              == tlm::TLM_OK_RESPONSE);
    };

    /// Clears whatever the previous job left, so each check starts from a
    /// known state without needing a reset.
    const auto acknowledge = [&] {
        CHECK(firmware.write(dma::reg::status, dma::status_bit::w1c_mask)
              == tlm::TLM_OK_RESPONSE);
    };

    const auto run_job = [&](std::uint64_t source, std::uint64_t destination,
                             std::uint32_t length, const std::string& what) {
        acknowledge();
        program(source, destination, length);
        CHECK(firmware.write(dma::reg::control, dma::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        wait_idle(what);
    };

    const auto bytes_done = [&] {
        return static_cast<std::uint64_t>(firmware.read(dma::reg::bytes_done_lo))
            | (static_cast<std::uint64_t>(
                   firmware.read(dma::reg::bytes_done_hi))
               << 32);
    };

    // Seeds a source pattern and checks the destination copy byte for byte.
    //
    // Split into 64-byte pieces because that is the largest a single SRAM
    // access may be — the same 1..64-byte rule the DMA itself has to respect
    // (decision record D7). A helper that ignored it would fail on the long
    // lengths and take the checks that follow down with it.
    const auto fill_sram = [&](std::uint64_t address, std::uint32_t length,
                               unsigned char seed) {
        std::vector<unsigned char> pattern(length);
        for (std::uint32_t i = 0; i < length; ++i) {
            pattern[i] = static_cast<unsigned char>(seed + i);
        }
        for (std::uint32_t done = 0; done < length;) {
            const std::uint32_t piece =
                std::min<std::uint32_t>(length - done,
                                        sram::neo_max_transfer_bytes);
            CHECK(store.debug_write(address + done, piece,
                                    pattern.data() + done, nullptr)
                  == sram::neo_status::ok);
            done += piece;
        }
    };
    const auto check_sram = [&](std::uint64_t address, std::uint32_t length,
                                unsigned char seed, const std::string& what) {
        std::vector<unsigned char> back(length, 0);
        for (std::uint32_t done = 0; done < length;) {
            const std::uint32_t piece =
                std::min<std::uint32_t>(length - done,
                                        sram::neo_max_transfer_bytes);
            CHECK(store.debug_read(address + done, piece, back.data() + done,
                                   nullptr)
                  == sram::neo_status::ok);
            done += piece;
        }
        for (std::uint32_t i = 0; i < length; ++i) {
            CHECK_MSG(back[i] == static_cast<unsigned char>(seed + i),
                      what + ": SRAM byte " + std::to_string(i) + " differs");
        }
    };
    const auto fill_external = [&](std::uint64_t address, std::uint32_t length,
                                   unsigned char seed) {
        for (std::uint32_t i = 0; i < length; ++i) {
            outside.poke(address + i, static_cast<unsigned char>(seed + i));
        }
    };
    const auto check_external = [&](std::uint64_t address,
                                    std::uint32_t length, unsigned char seed,
                                    const std::string& what) {
        for (std::uint32_t i = 0; i < length; ++i) {
            CHECK_MSG(outside.at(address + i)
                          == static_cast<unsigned char>(seed + i),
                      what + ": external byte " + std::to_string(i)
                          + " differs");
        }
    };

    // ── scenario 1: the register file ────────────────────────────────────────
    const auto phase_registers = [&] {
        const std::uint64_t base = firmware.base;

        CHECK(firmware.read(dma::reg::id) == dma::identity);
        CHECK(firmware.read(dma::reg::version) == dma::model_version);
        CHECK(firmware.read(dma::reg::status) == 0);
        CHECK(firmware.read(dma::reg::error_cause)
              == static_cast<std::uint32_t>(error_cause::none));
        CHECK(firmware.read(dma::reg::irq_enable) == 0);
        CHECK_MSG(irq_line.read() == false,
                  "the interrupt line must start low, or 'no interrupt yet' "
                  "and 'interrupt pending' are the same observation");

        // Descriptor registers round-trip while idle, on both halves.
        CHECK(firmware.write(dma::reg::src_addr_lo, 0x1234'5678u)
              == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.write(dma::reg::src_addr_hi, 0x0000'00ABu)
              == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.read(dma::reg::src_addr_lo) == 0x1234'5678u);
        CHECK(firmware.read(dma::reg::src_addr_hi) == 0x0000'00ABu);

        // Read-only registers drop writes rather than erroring.
        CHECK(firmware.write(dma::reg::id, 0) == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.read(dma::reg::id) == dma::identity);
        CHECK(firmware.write(dma::reg::transfer_count, 99)
              == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.read(dma::reg::transfer_count) == 0);

        // W1C leaves untargeted bits alone. Set DONE and ERROR through a real
        // job later; here, clear-on-zero must be a no-op.
        CHECK(firmware.write(dma::reg::status, 0) == tlm::TLM_OK_RESPONSE);

        // IRQ_ENABLE keeps only its defined bit.
        CHECK(firmware.write(dma::reg::irq_enable, 0xFFFF'FFFFu)
              == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.read(dma::reg::irq_enable)
              == dma::irq_enable_bit::completion);
        CHECK(firmware.write(dma::reg::irq_enable, 0) == tlm::TLM_OK_RESPONSE);

        // Reserved offsets are a defined state, and must not alias.
        CHECK(firmware.read(0x100) == 0);
        CHECK(firmware.write(0x100, 0x5A5A'5A5Au) == tlm::TLM_OK_RESPONSE);
        CHECK(firmware.read(0x100) == 0);
        CHECK_MSG(firmware.read(0x1000) == 0,
                  "DMA_CONTROL + 0x1000 answered as offset zero; a reserved "
                  "offset must not alias an implemented register");
        CHECK(firmware.read(0x1000) != dma::identity);

        // ── payload rules ────────────────────────────────────────────────────
        std::array<unsigned char, 64> buffer{};

        CHECK(firmware.raw(tlm::TLM_READ_COMMAND, base, buffer.data(), 1)
              == tlm::TLM_BURST_ERROR_RESPONSE);
        CHECK(firmware.raw(tlm::TLM_READ_COMMAND, base, buffer.data(), 2)
              == tlm::TLM_BURST_ERROR_RESPONSE);
        CHECK(firmware.raw(tlm::TLM_READ_COMMAND, base, buffer.data(), 8)
              == tlm::TLM_BURST_ERROR_RESPONSE);
        CHECK_MSG(firmware.raw(tlm::TLM_WRITE_COMMAND, base, buffer.data(), 64)
                      == tlm::TLM_BURST_ERROR_RESPONSE,
                  "a 64-byte accelerator payload must not be routed through a "
                  "control register file");
        CHECK(firmware.raw(tlm::TLM_READ_COMMAND, base + 2, buffer.data(), 4)
              == tlm::TLM_BURST_ERROR_RESPONSE);
        CHECK(firmware.raw(tlm::TLM_IGNORE_COMMAND, base, buffer.data(), 4)
              == tlm::TLM_COMMAND_ERROR_RESPONSE);

        std::array<unsigned char, 4> partial{0xFF, 0xFF, 0x00, 0x00};
        CHECK(firmware.raw(tlm::TLM_WRITE_COMMAND, base + dma::reg::length,
                           buffer.data(), 4, partial.data(), 4)
              == tlm::TLM_BURST_ERROR_RESPONSE);
        std::array<unsigned char, 4> full{0xFF, 0xFF, 0xFF, 0xFF};
        CHECK(firmware.raw(tlm::TLM_WRITE_COMMAND, base + dma::reg::length,
                           buffer.data(), 4, full.data(), 4)
              == tlm::TLM_OK_RESPONSE);

        CHECK_MSG(firmware.raw(tlm::TLM_READ_COMMAND, base, buffer.data(), 4,
                               nullptr, 0, 2)
                      == tlm::TLM_BURST_ERROR_RESPONSE,
                  "a wrapped streaming transfer must be refused");
        CHECK_MSG(firmware.raw(tlm::TLM_READ_COMMAND, base, nullptr, 4)
                      == tlm::TLM_GENERIC_ERROR_RESPONSE,
                  "a null data pointer must be answered, not thrown: this "
                  "target is reachable from a remote master");

        // 64 KiB bounds, from both sides.
        CHECK(firmware.raw(tlm::TLM_READ_COMMAND, base - 4, buffer.data(), 4)
              == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        CHECK(firmware.raw(tlm::TLM_READ_COMMAND,
                           base + am::dma_control_size, buffer.data(), 4)
              == tlm::TLM_ADDRESS_ERROR_RESPONSE);
        CHECK(firmware.raw(tlm::TLM_READ_COMMAND,
                           base + am::dma_control_size - 4, buffer.data(), 4)
              == tlm::TLM_OK_RESPONSE);

        // ── debug transport ──────────────────────────────────────────────────
        for (std::uint64_t offset :
             {dma::reg::id, dma::reg::version, dma::reg::status,
              dma::reg::src_addr_lo, dma::reg::length, dma::reg::error_cause,
              std::uint64_t{0x1000}}) {
            std::uint32_t debugged = 0xFFFF'FFFFu;
            CHECK(firmware.debug_read(offset, debugged) == 4);
            CHECK_MSG(debugged == firmware.read(offset),
                      "the debug path disagrees with b_transport at offset "
                          + std::to_string(offset));
        }
        // A debug write is side-effect free: a host-side poke must not launch
        // a transfer, and must not change a register either.
        const std::uint32_t before = firmware.read(dma::reg::length);
        CHECK(firmware.debug_write(dma::reg::length, 0x1234) == 0);
        CHECK(firmware.read(dma::reg::length) == before);
        CHECK(firmware.debug_write(dma::reg::control, dma::control_bit::start)
              == 0);
        CHECK_MSG(!is_busy(),
                  "a debug write to CONTROL started a transfer; the debug path "
                  "must be side-effect free");

        // Leave the descriptor clean for the scenarios that follow.
        program(0, 0, 0);
    };

    // ── scenario 2: copies in both directions, at the awkward lengths ────────
    const auto phase_copies = [&] {
        const std::array<std::uint32_t, 12> lengths{
            1, 2, 3, 7, 8, 15, 16, 63, 64, 65, 2048, 5000};

        for (std::uint32_t length : lengths) {
            // local → external, at an odd source and an odd destination so the
            // local split and the external lane offset are both exercised.
            const std::uint64_t source = sram_base + 8192 + 3;
            const std::uint64_t destination = kExternalBase + 4096 + 5;
            const auto seed = static_cast<unsigned char>(length);

            fill_sram(source, length, seed);
            const std::uint64_t local_before = fabric.counters(
                                                         neo_requester::dma)
                                                   .transferred_bytes;
            const std::uint64_t external_before = outside.bytes_written;

            run_job(source, destination, length,
                    "local->external " + std::to_string(length));

            CHECK_MSG((status() & dma::status_bit::done) != 0,
                      "local->external " + std::to_string(length)
                          + " did not report DONE");
            CHECK(bytes_done() == length);
            check_external(destination, length, seed,
                           "local->external " + std::to_string(length));

            // On success both path totals equal LENGTH — the conservation the
            // gate asks for, measured on this job alone.
            CHECK_MSG(fabric.counters(neo_requester::dma).transferred_bytes
                              - local_before
                          == length,
                      "native-plane bytes do not reconcile at length "
                          + std::to_string(length));
            CHECK_MSG(outside.bytes_written - external_before == length,
                      "external-path bytes do not reconcile at length "
                          + std::to_string(length));

            // external → local, back into a different SRAM address.
            const std::uint64_t back_source = kExternalBase + 16384 + 1;
            const std::uint64_t back_destination = sram_base + 32768 + 7;
            const auto back_seed = static_cast<unsigned char>(length + 0x40);

            fill_external(back_source, length, back_seed);
            run_job(back_source, back_destination, length,
                    "external->local " + std::to_string(length));

            CHECK((status() & dma::status_bit::done) != 0);
            CHECK(bytes_done() == length);
            check_sram(back_destination, length, back_seed,
                       "external->local " + std::to_string(length));
        }

        // The frame limit is a real bound, not an aspiration. A 5000-byte
        // transfer cannot be one payload, and at a lane offset the legal frame
        // is shorter than 2048 — which is the part an implementation that
        // assumed 2048 always fits would get wrong.
        CHECK_MSG(outside.frame_violations == 0,
                  "an external payload exceeded the 8-byte/256-beat frame at "
                  "its own lane offset");
        CHECK_MSG(outside.largest_payload > 2048 - 8,
                  "no payload came near the frame limit, so the chunking was "
                  "never actually exercised");

        // Every local byte went through the native plane as the DMA, and none
        // of it leaked to the external stub as a local address.
        CHECK(fabric.counters(neo_requester::dma).request_count > 0);
        CHECK(fabric.counters(neo_requester::dma).error_count == 0);
        CHECK_MSG(fabric.counters(neo_requester::dma).transferred_bytes
                      == engine.local_bytes(),
                  "the fabric and the DMA disagree about how many bytes the "
                  "requester 'dma' moved, so something else is being counted "
                  "as the DMA or the DMA is not being counted");
    };

    // ── scenario 3: descriptors the DMA must refuse ──────────────────────────
    const auto phase_refusals = [&] {
        struct bad_case {
            std::uint64_t source;
            std::uint64_t destination;
            std::uint32_t length;
            error_cause expected;
            const char* what;
        };

        const std::uint64_t sram_top = sram_base + am::core_sram_window;
        const std::array<bad_case, 7> cases{{
            {sram_base, kExternalBase, 0, error_cause::invalid_length,
             "zero length"},
            {sram_base, kExternalBase, 64, error_cause::none, "control case"},
            {sram_base + 64, sram_base + 4096, 64,
             error_cause::unsupported_endpoints, "both endpoints local"},
            {kExternalBase, kExternalBase + 4096, 64,
             error_cause::unsupported_endpoints, "both endpoints external"},
            {sram_top - 8, kExternalBase, 64, error_cause::source_unmapped,
             "source straddles the SRAM boundary"},
            {kExternalBase, sram_top - 8, 64,
             error_cause::destination_unmapped,
             "destination straddles the SRAM boundary"},
            {sram_base, 0xFFFF'FFF0ull, 64, error_cause::address_overflow,
             "destination span ends above the 4 GiB RV32 limit"},
        }};

        for (const auto& entry : cases) {
            run_job(entry.source, entry.destination, entry.length, entry.what);

            if (entry.expected == error_cause::none) {
                CHECK_MSG((status() & dma::status_bit::done) != 0,
                          std::string(entry.what) + " should have succeeded");
                continue;
            }
            CHECK_MSG((status() & dma::status_bit::error) != 0,
                      std::string(entry.what) + " should have set ERROR");
            CHECK_MSG(firmware.read(dma::reg::error_cause)
                          == static_cast<std::uint32_t>(entry.expected),
                      std::string(entry.what) + " latched cause "
                          + dma::to_string(static_cast<error_cause>(
                                firmware.read(dma::reg::error_cause)))
                          + ", expected " + dma::to_string(entry.expected));
            CHECK_MSG(bytes_done() == 0,
                      std::string(entry.what)
                          + " committed bytes despite an invalid descriptor");
        }
        acknowledge();
    };

    // ── scenario 4: first-error stop and committed-byte accounting ───────────
    const auto phase_partial = [&] {
        // A 5 KiB transfer at bus alignment is three frames of 2048, 2048 and
        // 1024. Fail inside the third one, so the failure is demonstrably not
        // the first chunk.
        const std::uint32_t length = 5 * 1024;
        const std::uint64_t source = sram_base + 8192;
        const std::uint64_t destination = kExternalBase + 32768;
        fill_sram(source, length, 0x11);

        outside.fail_at = destination + 4096 + 16;
        const std::uint64_t writes_before = outside.writes;
        const std::uint64_t bytes_before = outside.bytes_written;
        const std::uint64_t attempts_before = engine.external_requests();

        run_job(source, destination, length, "failing third frame");

        CHECK_MSG((status() & dma::status_bit::error) != 0,
                  "a refused destination must set ERROR");
        CHECK(firmware.read(dma::reg::error_cause)
              == static_cast<std::uint32_t>(error_cause::external_write));
        CHECK_MSG(bytes_done() == 4096,
                  "BYTES_DONE must count destination bytes committed — two "
                  "frames here — not the source bytes fetched into the "
                  "staging buffer");

        // The first two frames really did land, and nothing was issued after
        // the failing one.
        check_external(destination, 4096, 0x11, "committed prefix");
        CHECK_MSG(outside.writes - writes_before == 2,
                  "exactly two external writes landed");
        CHECK(outside.bytes_written - bytes_before == 4096);
        CHECK(outside.refusals == 1);
        CHECK_MSG(engine.external_requests() - attempts_before == 3,
                  "exactly three external transactions were attempted: two "
                  "that landed and the one that failed. A fourth means a "
                  "chunk was issued after the error");

        // On error the two path totals need not agree with LENGTH, but each
        // must agree with its own successful transactions. The DMA read all
        // three frames locally before the third write failed.
        CHECK_MSG(engine.external_bytes() >= 4096,
                  "the external byte total must count its successful "
                  "transactions");
        CHECK_MSG(engine.bytes_done() == 4096,
                  "committed destination bytes and path totals are different "
                  "quantities and must not be conflated");

        outside.fail_at = external_memory::kNoFailure;
        acknowledge();

        // A failing *source* on the external side reports its own cause.
        const std::uint64_t read_source = kExternalBase + 65536;
        outside.fail_at = read_source;
        run_job(read_source, sram_base + 40960, 256, "failing external read");
        CHECK((status() & dma::status_bit::error) != 0);
        CHECK(firmware.read(dma::reg::error_cause)
              == static_cast<std::uint32_t>(error_cause::external_read));
        CHECK(bytes_done() == 0);
        outside.fail_at = external_memory::kNoFailure;
        acknowledge();

        // A local destination above the instantiated capacity: the native
        // plane refuses it and the DMA reports the local origin, not a generic
        // failure.
        run_job(kExternalBase, sram_base + kSramCapacity, 64,
                "local write above capacity");
        CHECK((status() & dma::status_bit::error) != 0);
        CHECK(firmware.read(dma::reg::error_cause)
              == static_cast<std::uint32_t>(error_cause::local_write));
        acknowledge();
    };

    // ── scenario 5: interrupts, overrun and busy-time refusals ───────────────
    const auto phase_interrupts = [&] {
        CHECK(firmware.write(dma::reg::irq_enable,
                             dma::irq_enable_bit::completion)
              == tlm::TLM_OK_RESPONSE);

        const std::uint32_t length = 4096;
        const std::uint64_t source = sram_base + 8192;
        fill_sram(source, length, 0x33);
        acknowledge();
        program(source, kExternalBase + 98304, length);
        CHECK(firmware.write(dma::reg::control, dma::control_bit::start)
              == tlm::TLM_OK_RESPONSE);

        // BUSY is visible before the START access returns, and no data has
        // moved yet.
        CHECK_MSG(is_busy(),
                  "BUSY must be set before the START write returns");

        // A second START and a descriptor write are both refused while busy.
        CHECK_MSG(firmware.write(dma::reg::control, dma::control_bit::start)
                      == tlm::TLM_GENERIC_ERROR_RESPONSE,
                  "a second START while busy must be refused");
        CHECK(firmware.read(dma::reg::overrun_count) == 1);
        CHECK_MSG(firmware.write(dma::reg::length, 7)
                      == tlm::TLM_GENERIC_ERROR_RESPONSE,
                  "a descriptor write while busy must be refused, not applied "
                  "to a job that is already running");
        CHECK(firmware.read(dma::reg::length) == length);

        // START and ABORT in one access have no defined ordering, so they are
        // refused together and change nothing.
        CHECK(firmware.write(dma::reg::control,
                             dma::control_bit::start | dma::control_bit::abort)
              == tlm::TLM_GENERIC_ERROR_RESPONSE);
        CHECK(is_busy());

        wait_idle("interrupt job");

        CHECK((status() & dma::status_bit::done) != 0);
        CHECK_MSG(irq_line.read(),
                  "a completed job with the interrupt enabled must raise the "
                  "level line");

        // Level, not a pulse: it stays high across time until acknowledged.
        sc_core::wait(sc_core::sc_time(200, sc_core::SC_NS));
        CHECK_MSG(irq_line.read(),
                  "the interrupt line dropped without an acknowledgement; it "
                  "is level, not edge (ARCHITECTURE.md §5)");

        // W1C on DONE deasserts it.
        CHECK(firmware.write(dma::reg::status, dma::status_bit::done)
              == tlm::TLM_OK_RESPONSE);
        sc_core::wait(sc_core::SC_ZERO_TIME);
        CHECK(!irq_line.read());

        // An errored job wakes firmware exactly as a completed one does.
        run_job(sram_base, sram_base + 4096, 64, "error interrupt");
        CHECK((status() & dma::status_bit::error) != 0);
        CHECK_MSG(irq_line.read(),
                  "an errored job must raise the interrupt too, or a driver "
                  "waiting on completion never learns it failed");
        CHECK(firmware.write(dma::reg::status, dma::status_bit::error)
              == tlm::TLM_OK_RESPONSE);
        sc_core::wait(sc_core::SC_ZERO_TIME);
        CHECK(!irq_line.read());
    };

    // ── scenario 6: abort and reset epochs ───────────────────────────────────
    //
    // Two windows, both of which a looser test leaves open.
    //
    // **Lost start.** `ABORT` and `reset()` clear `BUSY` immediately, so
    // firmware may legitimately start a new job while the old worker is still
    // unwinding. If the worker is parked somewhere it cannot be woken, a start
    // request delivered as a bare event reaches nobody and the new job holds
    // `BUSY` for ever. Waiting before restarting hides it — and so does
    // restarting while the worker happens to be resuming in the same delta,
    // which is what a chunk delay phase-locked to the polling period gives
    // you. The external target's blocking window is used to park the worker
    // deterministically instead of hoping.
    //
    // **Under-reported commit.** `BYTES_DONE` must equal the bytes the
    // destination actually holds. Checking only that the destination holds
    // *at least* that many catches over-reporting and misses under-reporting,
    // which is the direction a publish-at-end-of-chunk implementation fails
    // in. So the byte after the reported count is checked too: it must still
    // be the filler.
    const auto phase_epochs = [&] {
        CHECK(firmware.write(dma::reg::irq_enable,
                             dma::irq_enable_bit::completion)
              == tlm::TLM_OK_RESPONSE);

        constexpr unsigned char kFiller = 0xEE;
        const std::uint32_t length = 16 * 1024;
        const std::uint64_t source = sram_base + 8192;

        const auto fill_destination = [&](std::uint64_t base) {
            for (std::uint32_t i = 0; i < length + 64; ++i) {
                outside.poke(base + i, kFiller);
            }
        };

        // Parks the worker inside an external transaction at a known point.
        // 2048 + 32 is inside the second frame, so at least one frame has
        // already been committed when the window opens.
        const auto start_and_park = [&](std::uint64_t destination,
                                        unsigned char seed) {
            fill_sram(source, 4096, seed);
            fill_destination(destination);
            outside.block_at = destination + 2048 + 32;
            outside.block_for = sc_core::sc_time(500, sc_core::SC_NS);

            acknowledge();
            program(source, destination, length);
            CHECK(firmware.write(dma::reg::control, dma::control_bit::start)
                  == tlm::TLM_OK_RESPONSE);
            CHECK(is_busy());

            for (unsigned i = 0; i < 5000 && !outside.blocking; ++i) {
                sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
            }
            CHECK_MSG(outside.blocking,
                      "the worker never entered the external target's "
                      "blocking window, so it was not parked and the "
                      "lost-start window was never opened");
        };

        const auto disarm = [&] {
            outside.block_at = external_memory::kNoFailure;
            outside.block_for = sc_core::SC_ZERO_TIME;
        };

        // Starts a short job with **no intervening wait** and requires it to
        // complete. This is the lost-start check.
        const auto restart_immediately = [&](std::uint64_t destination,
                                             unsigned char seed,
                                             const std::string& what) {
            fill_sram(source, 256, seed);
            acknowledge();
            program(source, destination, 256);
            CHECK(firmware.write(dma::reg::control, dma::control_bit::start)
                  == tlm::TLM_OK_RESPONSE);
            wait_idle(what);
            CHECK_MSG((status() & dma::status_bit::done) != 0,
                      what + ": the restarted job never completed. A START "
                             "issued while the old worker was parked must not "
                             "be lost");
            CHECK(bytes_done() == 256);
            check_external(destination, 256, seed, what);
        };

        // ── A. abort with the worker parked, then START in the same delta ────
        start_and_park(kExternalBase + 131072, 0x55);

        const std::uint64_t aborts_before = firmware.read(dma::reg::abort_count);
        const std::uint64_t committed_at_abort = engine.bytes_done();
        CHECK_MSG(committed_at_abort > 0,
                  "the abort landed before any byte was committed, so the "
                  "committed-bytes-survive rule was never exercised");

        CHECK(firmware.write(dma::reg::control, dma::control_bit::abort)
              == tlm::TLM_OK_RESPONSE);
        CHECK_MSG(!is_busy(), "an abort must clear BUSY immediately");
        CHECK_MSG((status() & dma::status_bit::aborted) != 0,
                  "an abort must be visible as ABORTED, not as DONE");
        CHECK((status() & dma::status_bit::done) == 0);
        CHECK(firmware.read(dma::reg::abort_count) == aborts_before + 1);
        CHECK_MSG(!irq_line.read(),
                  "firmware asked for the abort, so there is nothing to wake "
                  "it for");

        disarm();
        restart_immediately(kExternalBase + 200704, 0xA1,
                            "START immediately after ABORT");
        sc_core::wait(sc_core::sc_time(2, sc_core::SC_US));
        CHECK_MSG(!is_busy(), "an abandoned job re-entered BUSY");
        acknowledge();

        // ── B. reset while idle ──────────────────────────────────────────────
        engine.reset();
        sc_core::wait(sc_core::SC_ZERO_TIME);
        CHECK(engine.status() == 0);
        CHECK_MSG(engine.bytes_done() == 0,
                  "a reset with no active job initialises the committed count "
                  "to zero");
        CHECK(!irq_line.read());

        // ── C. reset with the worker parked, then START in the same delta ────
        start_and_park(kExternalBase + 163840, 0x77);
        CHECK(engine.bytes_done() > 0);

        engine.reset();
        sc_core::wait(sc_core::SC_ZERO_TIME);
        CHECK_MSG(engine.status() == 0,
                  "reset must clear BUSY, DONE, ERROR and ABORTED together");
        CHECK_MSG(!irq_line.read(),
                  "reset deasserts the interrupt without reporting DONE");

        disarm();
        restart_immediately(kExternalBase + 204800, 0xB2,
                            "START immediately after a reset that interrupted "
                            "an external transaction");
        acknowledge();

        // ── D. reset under native arbitration, then check the count exactly ──
        //
        // No blocking target here: the reset lands while the DMA is queueing
        // behind the three pressure requesters or consuming an annotated
        // delay, which is the ordinary case.
        const std::uint64_t destination = kExternalBase + 208896;
        const unsigned char seed = 0xC3;
        fill_sram(source, 4096, seed);
        fill_destination(destination);
        acknowledge();
        program(source, destination, length);
        CHECK(firmware.write(dma::reg::control, dma::control_bit::start)
              == tlm::TLM_OK_RESPONSE);
        while (engine.bytes_done() == 0 && is_busy()) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        const std::uint64_t committed_at_reset = engine.bytes_done();
        CHECK_MSG(committed_at_reset > 0,
                  "the reset landed before any byte was committed");

        engine.reset();
        sc_core::wait(sc_core::SC_ZERO_TIME);
        CHECK(engine.status() == 0);
        CHECK(!irq_line.read());
        CHECK_MSG(engine.bytes_done() >= committed_at_reset,
                  "bytes committed before a reset stay committed and stay "
                  "reported; nothing is rolled back");

        // Let the interrupted transaction finish at the target — the DMA
        // cannot un-write it — then compare the count against memory.
        sc_core::wait(sc_core::sc_time(2, sc_core::SC_US));
        CHECK_MSG(!engine.busy(),
                  "the abandoned worker re-entered BUSY after the reset");
        const std::uint64_t reported = engine.bytes_done();

        for (std::uint64_t i = 0; i < reported; ++i) {
            CHECK_MSG(outside.at(destination + i)
                          == static_cast<unsigned char>(seed + i),
                      "BYTES_DONE claims byte " + std::to_string(i)
                          + " was committed, but the destination does not "
                            "hold it");
        }
        // ...and nothing beyond it. Eight bytes, because the pattern is
        // strictly incrementing and one of them could coincide with the
        // filler by chance while eight cannot.
        for (std::uint64_t i = reported; i < reported + 8; ++i) {
            CHECK_MSG(outside.at(destination + i) == kFiller,
                      "the destination holds byte " + std::to_string(i)
                          + ", which is past BYTES_DONE ("
                          + std::to_string(reported)
                          + "). A commit the register does not report is as "
                            "wrong as one it invents");
        }

        acknowledge();

        // ── E. reset between the commit and the report ───────────────────────
        //
        // The narrowest window, and the one an implementation that publishes
        // `BYTES_DONE` at the end of a chunk leaves open: the destination
        // transaction has returned — the bytes are in memory — but the DMA has
        // not yet recorded them, because it is consuming the delay the target
        // annotated. A reset landing there freezes a count that is *lower*
        // than what memory holds.
        //
        // D above cannot reach it: with a short annotated delay the report
        // follows the commit within the polling granularity. So the target is
        // told to annotate 300 ns and the write counter says when the window
        // opened. There is no straggler either, because the transaction has
        // already returned — which is what makes an exact comparison legal
        // here and not after case C.
        const std::uint64_t narrow_destination = kExternalBase + 217088;
        const unsigned char narrow_seed = 0xE5;
        const sc_core::sc_time restore_latency = outside.latency;
        outside.latency = sc_core::sc_time(300, sc_core::SC_NS);

        fill_sram(source, 4096, narrow_seed);
        fill_destination(narrow_destination);
        acknowledge();
        program(source, narrow_destination, length);
        const std::uint64_t writes_before = outside.writes;
        CHECK(firmware.write(dma::reg::control, dma::control_bit::start)
              == tlm::TLM_OK_RESPONSE);

        for (unsigned i = 0; i < 5000 && outside.writes == writes_before; ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK_MSG(outside.writes > writes_before,
                  "no destination write completed, so the commit-to-report "
                  "window never opened");

        engine.reset();
        sc_core::wait(sc_core::SC_ZERO_TIME);
        outside.latency = restore_latency;

        const std::uint64_t narrow_reported = engine.bytes_done();
        CHECK_MSG(narrow_reported > 0,
                  "a destination write had already committed when the reset "
                  "landed, so BYTES_DONE cannot be zero");

        sc_core::wait(sc_core::sc_time(2, sc_core::SC_US));
        CHECK(engine.bytes_done() == narrow_reported);

        for (std::uint64_t i = 0; i < narrow_reported; ++i) {
            CHECK_MSG(outside.at(narrow_destination + i)
                          == static_cast<unsigned char>(narrow_seed + i),
                      "BYTES_DONE claims byte " + std::to_string(i)
                          + " was committed, but the destination does not "
                            "hold it");
        }
        for (std::uint64_t i = narrow_reported; i < narrow_reported + 8; ++i) {
            CHECK_MSG(outside.at(narrow_destination + i) == kFiller,
                      "the destination holds byte " + std::to_string(i)
                          + ", past BYTES_DONE ("
                          + std::to_string(narrow_reported)
                          + "). A commit the register does not report is as "
                            "wrong as one it invents");
        }

        acknowledge();
        CHECK(firmware.write(dma::reg::irq_enable, 0) == tlm::TLM_OK_RESPONSE);
    };

    // ── scenario 7: conservation across everything that ran ──────────────────
    // ── scenario 6b: the native destination side of reset ────────────────────
    //
    // Every reset case above copies local→external, so the destination is the
    // external target and the only accounting path exercised is the TLM one.
    // The native path is where a commit can be lost differently: the fabric
    // writes beats into SRAM, waits for arbitration, and only then returns —
    // so a reset can land *after* bytes are in memory and *before* the DMA is
    // told about them.
    const auto phase_native_destination_reset = [&] {
        constexpr unsigned char kFiller = 0x3C;
        const std::uint64_t destination = sram_base + 49152;
        const std::uint64_t source = kExternalBase + 32768;
        const std::uint32_t length = 8192;
        const unsigned char seed = 0x91;

        const auto fill_local_destination = [&] {
            std::vector<unsigned char> filler(64, kFiller);
            for (std::uint32_t i = 0; i < length + 64; i += 64) {
                CHECK(store.debug_write(destination + i, 64, filler.data(),
                                        nullptr)
                      == sram::neo_status::ok);
            }
        };
        const auto local_byte = [&](std::uint64_t address) {
            unsigned char value = 0;
            CHECK(store.debug_read(address, 1, &value, nullptr)
                  == sram::neo_status::ok);
            return value;
        };

        // ── A. DMA reset while a native destination access is in flight ──────
        for (std::uint32_t i = 0; i < length; ++i) {
            outside.poke(source + i, static_cast<unsigned char>(seed + i));
        }
        fill_local_destination();
        acknowledge();
        program(source, destination, length);
        CHECK(firmware.write(dma::reg::control, dma::control_bit::start)
              == tlm::TLM_OK_RESPONSE);

        // Grants are recorded when the arbiter hands over a bank, i.e. from
        // inside `b_access`. Waiting for one puts the reset in the middle of a
        // native request rather than between two of them, which is the whole
        // point.
        const auto dma_grants = [&] {
            std::uint64_t total = 0;
            for (unsigned bank = 0; bank < fabric.config().bank_count; ++bank) {
                total += fabric.bank_grants(bank, neo_requester::dma);
            }
            return total;
        };
        const std::uint64_t grants_before = dma_grants();
        for (unsigned i = 0; i < 20000 && dma_grants() == grants_before; ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK_MSG(dma_grants() > grants_before,
                  "the DMA never got a bank, so no native destination beat "
                  "was committed and the window was never opened");

        engine.reset();
        sc_core::wait(sc_core::SC_ZERO_TIME);
        sc_core::wait(sc_core::sc_time(2, sc_core::SC_US));

        const std::uint64_t reported = engine.bytes_done();
        CHECK_MSG(reported > 0,
                  "a native destination beat had committed when the reset "
                  "landed, so BYTES_DONE cannot be zero. A commit made before "
                  "the reset must be reported (plan §11.5)");
        for (std::uint64_t i = 0; i < reported; ++i) {
            CHECK_MSG(local_byte(destination + i)
                          == static_cast<unsigned char>(seed + i),
                      "BYTES_DONE claims local byte " + std::to_string(i)
                          + " was committed, but the SRAM does not hold it");
        }
        for (std::uint64_t i = reported; i < reported + 8; ++i) {
            CHECK_MSG(local_byte(destination + i) == kFiller,
                      "the SRAM holds byte " + std::to_string(i)
                          + ", past BYTES_DONE (" + std::to_string(reported)
                          + ")");
        }
        acknowledge();
    };

    // ── scenario 6c: the fabric aborts a request that had already moved bytes
    //
    // The other half of the same window, on the isolated machine: here the
    // *fabric* is reset, so the DMA's request comes back with `aborted` and a
    // non-zero byte count. Those bytes are in SRAM and belong in `BYTES_DONE`;
    // the job itself failed, and the two facts are independent.
    const auto phase_fabric_abort = [&] {
        constexpr unsigned char kFiller = 0x6D;
        const std::uint64_t destination = sram_base + 40960;
        const std::uint64_t source = kExternalBase + 65536;
        const std::uint32_t length = 4096;
        const unsigned char seed = 0x4F;

        for (std::uint32_t i = 0; i < length; ++i) {
            outside_b.poke(source + i, static_cast<unsigned char>(seed + i));
        }
        std::vector<unsigned char> filler(64, kFiller);
        for (std::uint32_t i = 0; i < length + 64; i += 64) {
            CHECK(store_b.debug_write(destination + i, 64, filler.data(),
                                      nullptr)
                  == sram::neo_status::ok);
        }

        const auto write_b = [&](std::uint64_t offset, std::uint32_t value) {
            CHECK(firmware_b.write(offset, value) == tlm::TLM_OK_RESPONSE);
        };
        write_b(dma::reg::src_addr_lo, static_cast<std::uint32_t>(source));
        write_b(dma::reg::src_addr_hi,
                static_cast<std::uint32_t>(source >> 32));
        write_b(dma::reg::dst_addr_lo,
                static_cast<std::uint32_t>(destination));
        write_b(dma::reg::dst_addr_hi,
                static_cast<std::uint32_t>(destination >> 32));
        write_b(dma::reg::length, length);
        write_b(dma::reg::control, dma::control_bit::start);

        const auto grants_b = [&] {
            std::uint64_t total = 0;
            for (unsigned bank = 0; bank < fabric_b.config().bank_count;
                 ++bank) {
                total += fabric_b.bank_grants(bank, neo_requester::dma);
            }
            return total;
        };
        for (unsigned i = 0; i < 20000 && grants_b() == 0; ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK_MSG(grants_b() > 0,
                  "the DMA never got a bank on the isolated fabric");

        fabric_b.reset();
        for (unsigned i = 0; i < 20000 && engine_b.busy(); ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK_MSG(!engine_b.busy(), "the job never left BUSY after the fabric "
                                    "abandoned its request");

        CHECK_MSG((engine_b.status() & dma::status_bit::error) != 0,
                  "a native access the fabric abandoned is a failed local "
                  "write, and the job must say so");
        CHECK(engine_b.last_error() == error_cause::local_write);
        CHECK_MSG(engine_b.bytes_done() > 0,
                  "the fabric reported the bytes it had already moved, and "
                  "those bytes are in SRAM; BYTES_DONE must include them even "
                  "though the job failed");

        const std::uint64_t reported = engine_b.bytes_done();
        for (std::uint64_t i = 0; i < reported; ++i) {
            unsigned char value = 0;
            CHECK(store_b.debug_read(destination + i, 1, &value, nullptr)
                  == sram::neo_status::ok);
            CHECK_MSG(value == static_cast<unsigned char>(seed + i),
                      "BYTES_DONE claims local byte " + std::to_string(i)
                          + " was committed, but the SRAM does not hold it");
        }
    };

    // ── scenario 6e: hierarchical reset, the Phase 7 shape ───────────────────
    //
    // Phase 7 resets the DMA and the fabric together from the core's reset
    // path, so this is the arrangement that actually has to hold. Both are
    // reset while a native request is in flight, and the request then returns
    // carrying bytes it had already committed. Three things must be true
    // afterwards, and they pull in different directions:
    //
    //   * `BYTES_DONE` still reports those bytes — they are in SRAM;
    //   * the DMA's path counters read zero, because the returning request
    //     belongs to the measurement window that just closed;
    //   * the DMA and the fabric agree, which they only can if both exclude
    //     the same request.
    //
    // Runs on the isolated machine: `fabric_b.reset()` clears counters that
    // the main instance's lifetime reconciliation depends on.
    const auto phase_hierarchical_reset = [&] {
        constexpr unsigned char kFiller = 0x1B;
        const std::uint64_t destination = sram_base + 24576;
        const std::uint64_t source = kExternalBase + 98304;
        const std::uint32_t length = 4096;
        const unsigned char seed = 0x7A;

        for (std::uint32_t i = 0; i < length; ++i) {
            outside_b.poke(source + i, static_cast<unsigned char>(seed + i));
        }
        std::vector<unsigned char> filler(64, kFiller);
        for (std::uint32_t i = 0; i < length + 64; i += 64) {
            CHECK(store_b.debug_write(destination + i, 64, filler.data(),
                                      nullptr)
                  == sram::neo_status::ok);
        }

        const auto write_b = [&](std::uint64_t offset, std::uint32_t value) {
            CHECK(firmware_b.write(offset, value) == tlm::TLM_OK_RESPONSE);
        };
        const auto local_byte_b = [&](std::uint64_t address) {
            unsigned char value = 0;
            CHECK(store_b.debug_read(address, 1, &value, nullptr)
                  == sram::neo_status::ok);
            return value;
        };

        write_b(dma::reg::status, dma::status_bit::w1c_mask);
        write_b(dma::reg::src_addr_lo, static_cast<std::uint32_t>(source));
        write_b(dma::reg::src_addr_hi,
                static_cast<std::uint32_t>(source >> 32));
        write_b(dma::reg::dst_addr_lo,
                static_cast<std::uint32_t>(destination));
        write_b(dma::reg::dst_addr_hi,
                static_cast<std::uint32_t>(destination >> 32));
        write_b(dma::reg::length, length);
        write_b(dma::reg::control, dma::control_bit::start);

        const auto grants_b = [&] {
            std::uint64_t total = 0;
            for (unsigned bank = 0; bank < fabric_b.config().bank_count;
                 ++bank) {
                total += fabric_b.bank_grants(bank, neo_requester::dma);
            }
            return total;
        };
        const std::uint64_t grants_before = grants_b();
        for (unsigned i = 0; i < 20000 && grants_b() == grants_before; ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK_MSG(grants_b() > grants_before,
                  "no native beat was committed, so the hierarchical reset "
                  "had nothing in flight to interrupt");

        // Both, in the same delta, as the core's reset path would.
        fabric_b.reset();
        engine_b.reset();
        sc_core::wait(sc_core::SC_ZERO_TIME);

        for (unsigned i = 0; i < 20000 && engine_b.busy(); ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK_MSG(!engine_b.busy(), "the job never drained after the reset");
        sc_core::wait(sc_core::sc_time(1, sc_core::SC_US));

        const std::uint64_t reported = engine_b.bytes_done();
        CHECK_MSG(reported > 0,
                  "bytes were committed to SRAM before the reset, so "
                  "BYTES_DONE must still report them");
        for (std::uint64_t i = 0; i < reported; ++i) {
            CHECK_MSG(local_byte_b(destination + i)
                          == static_cast<unsigned char>(seed + i),
                      "BYTES_DONE claims byte " + std::to_string(i)
                          + " was committed, but the SRAM does not hold it");
        }
        for (std::uint64_t i = reported; i < reported + 8; ++i) {
            CHECK_MSG(local_byte_b(destination + i) == kFiller,
                      "the SRAM holds byte " + std::to_string(i)
                          + ", past BYTES_DONE");
        }

        CHECK_MSG(engine_b.local_bytes() == 0 && engine_b.local_requests() == 0
                      && engine_b.external_bytes() == 0
                      && engine_b.external_requests() == 0
                      && engine_b.chunks_completed() == 0,
                  "an old request re-populated the traffic counters of the "
                  "new epoch: local_bytes=" + std::to_string(engine_b.local_bytes())
                      + " local_requests="
                      + std::to_string(engine_b.local_requests())
                      + " external_bytes="
                      + std::to_string(engine_b.external_bytes())
                      + " external_requests="
                      + std::to_string(engine_b.external_requests()));
        CHECK_MSG(fabric_b.counters(neo_requester::dma).transferred_bytes
                      == engine_b.local_bytes(),
                  "the DMA and the fabric disagree about the new epoch, so "
                  "one of them counted a request the other excluded");
        CHECK(fabric_b.counters(neo_requester::dma).request_count
              == engine_b.local_requests());

        // And the new epoch works: a fresh job reconciles on both sides.
        const std::uint64_t fresh_destination = sram_base + 32768;
        const std::uint32_t fresh_length = 512;
        for (std::uint32_t i = 0; i < fresh_length; ++i) {
            outside_b.poke(source + i, static_cast<unsigned char>(0x5C + i));
        }
        write_b(dma::reg::status, dma::status_bit::w1c_mask);
        write_b(dma::reg::dst_addr_lo,
                static_cast<std::uint32_t>(fresh_destination));
        write_b(dma::reg::length, fresh_length);
        write_b(dma::reg::control, dma::control_bit::start);
        for (unsigned i = 0; i < 20000 && engine_b.busy(); ++i) {
            sc_core::wait(sc_core::sc_time(10, sc_core::SC_NS));
        }
        CHECK(!engine_b.busy());
        CHECK((engine_b.status() & dma::status_bit::done) != 0);
        CHECK(engine_b.bytes_done() == fresh_length);
        CHECK(engine_b.local_bytes() == fresh_length);
        CHECK_MSG(fabric_b.counters(neo_requester::dma).transferred_bytes
                      == engine_b.local_bytes(),
                  "the DMA and the fabric disagree in the new epoch");
        CHECK(fabric_b.counters(neo_requester::dma).request_count
              == engine_b.local_requests());
        for (std::uint32_t i = 0; i < fresh_length; ++i) {
            CHECK(local_byte_b(fresh_destination + i)
                  == static_cast<unsigned char>(0x5C + i));
        }
    };

    // ── scenario 6d: ABORT with nothing to abort ─────────────────────────────
    const auto phase_idle_abort = [&] {
        acknowledge();
        CHECK(!is_busy());
        const std::uint64_t aborts_before = firmware.read(dma::reg::abort_count);
        const std::uint32_t status_before = status();

        CHECK_MSG(firmware.write(dma::reg::control, dma::control_bit::abort)
                      == tlm::TLM_OK_RESPONSE,
                  "an abort with no job to abort is accepted and ignored, not "
                  "refused: firmware asking twice is not an error");
        CHECK_MSG(firmware.read(dma::reg::abort_count) == aborts_before,
                  "an idle abort aborted nothing, so it must not count one");
        CHECK_MSG(status() == status_before,
                  "an idle abort must leave the status untouched, and in "
                  "particular must not set ABORTED");
        CHECK((status() & dma::status_bit::aborted) == 0);
    };

    const auto phase_conservation = [&] {
        // Wait for a quiescent point first. Every equality below relates a
        // counter the fabric publishes when a request completes to one the
        // SRAM increments per beat, and mid-request those legitimately differ
        // by the beat still draining the pipeline. Checking anyway would give
        // a test that fails by exactly one beat whenever the timing shifts.
        for (unsigned i = 0; i < 20000; ++i) {
            if (pressure_sa.finished() && pressure_cpu.finished()
                && pressure_transform.finished() && !engine.busy()) {
                break;
            }
            sc_core::wait(sc_core::sc_time(100, sc_core::SC_NS));
        }
        CHECK_MSG(pressure_sa.finished() && pressure_cpu.finished()
                      && pressure_transform.finished(),
                  "a pressure requester was still in flight, so the totals "
                  "below could not be compared at a quiescent point");
        CHECK(!engine.busy());

        // ── lifetime: nothing reached the SRAM except through the fabric ─────
        //
        // Both of these are lifetime totals and neither was reset, so they can
        // be compared directly. `core_sram` hands out no backing pointer, so
        // there is no second path today; this is what would notice if one
        // appeared.
        CHECK_MSG(store.bytes_written() + store.bytes_read()
                      == fabric.total_bytes(),
                  "the SRAM moved bytes the fabric did not carry: sram="
                      + std::to_string(store.bytes_written()
                                       + store.bytes_read())
                      + " fabric=" + std::to_string(fabric.total_bytes()));
        CHECK_MSG(store.write_accesses() + store.read_accesses()
                      == fabric.total_beats(),
                  "the storage sees beats, the fabric sees requests, and the "
                  "two counts must reconcile beat-for-beat (D7)");

        // ── per-epoch: the DMA's path totals against everyone else's ─────────
        //
        // The DMA clears its path counters on reset and the fabric clears its
        // own, so in Phase 7 — where the core's hierarchical reset drives both
        // — these totals are per-epoch quantities. Comparing lifetime sums
        // here would only work as long as nothing was ever reset, which is the
        // opposite of what this component has to survive. So: reset, take a
        // baseline of everything that was *not* reset, run one known job, and
        // reconcile the deltas.
        engine.reset();
        sc_core::wait(sc_core::SC_ZERO_TIME);
        CHECK(engine.local_bytes() == 0);
        CHECK(engine.external_bytes() == 0);
        CHECK(engine.local_requests() == 0);
        CHECK(engine.external_requests() == 0);
        CHECK_MSG(engine.abort_count() > 0,
                  "the abort history is an event counter and must survive the "
                  "reset that produced it");
        const std::uint64_t transfers_before = engine.transfer_count();

        const std::uint64_t fabric_bytes_base =
            fabric.counters(neo_requester::dma).transferred_bytes;
        const std::uint64_t fabric_requests_base =
            fabric.counters(neo_requester::dma).request_count;
        const std::uint64_t stub_bytes_base =
            outside.bytes_read + outside.bytes_written;
        const std::uint64_t stub_transactions_base =
            outside.reads + outside.writes + outside.refusals
            + outside.address_errors;

        const std::uint32_t final_length = 4096;
        const std::uint64_t final_source = sram_base + 8192;
        fill_sram(final_source, final_length, 0x2B);
        run_job(final_source, kExternalBase + 229376, final_length,
                "conservation job");
        CHECK((status() & dma::status_bit::done) != 0);
        check_external(kExternalBase + 229376, final_length, 0x2B,
                       "conservation job");

        CHECK_MSG(engine.transfer_count() == transfers_before + 1,
                  "the accepted-job counter is an event counter and must "
                  "survive reset");

        CHECK_MSG(engine.local_bytes()
                      == fabric.counters(neo_requester::dma).transferred_bytes
                          - fabric_bytes_base,
                  "the DMA's local byte total disagrees with the fabric's "
                  "attribution for requester 'dma' over the same epoch");
        CHECK(engine.local_requests()
              == fabric.counters(neo_requester::dma).request_count
                  - fabric_requests_base);
        CHECK_MSG(engine.external_bytes()
                      == outside.bytes_read + outside.bytes_written
                          - stub_bytes_base,
                  "the DMA's external byte total disagrees with the target's "
                  "over the same epoch");
        CHECK(engine.external_requests()
              == outside.reads + outside.writes + outside.refusals
                  + outside.address_errors - stub_transactions_base);

        // On a successful job both path totals equal LENGTH, which is the
        // conservation the gate asks for stated at its simplest.
        CHECK(engine.local_bytes() == final_length);
        CHECK(engine.external_bytes() == final_length);
        CHECK(engine.bytes_done() == final_length);

        // The DMA contended for banks rather than owning them, against all
        // three named local requesters the plan asks for.
        CHECK_MSG(pressure_sa.iterations() > 0
                      && pressure_cpu.iterations() > 0
                      && pressure_transform.iterations() > 0,
                  "a pressure requester never ran, so it contended with "
                  "nothing");
        CHECK_MSG(fabric.total_bank_conflicts() > 0,
                  "the DMA and the pressure requesters never collided, so "
                  "back-pressure was not exercised");
        for (auto requester : {neo_requester::cpu, neo_requester::sa,
                               neo_requester::transform}) {
            CHECK_MSG(fabric.counters(requester).request_count > 0,
                      std::string("no traffic from requester ")
                          + sram::to_string(requester));
        }

        std::cout << engine.report();
    };

    // One thread, seven phases, in order. They share `firmware` and the
    // descriptor registers, so running them concurrently would not be a
    // stronger test — it would be two drivers interleaving register writes
    // into one descriptor slot, which is a race, not a scenario. The
    // concurrency that *is* wanted comes from `bank_pressure`, which contends
    // for the array underneath all of them.
    scenario phases("phases", [&] {
        phase_registers();
        phase_copies();
        phase_refusals();
        phase_partial();
        phase_interrupts();
        phase_epochs();
        phase_native_destination_reset();
        phase_fabric_abort();
        phase_hierarchical_reset();
        phase_idle_abort();
        phase_conservation();
    });

    sc_core::sc_start(
        sc_core::sc_time(kWatchdogMilliseconds, sc_core::SC_MS));

    CHECK_MSG(phases.finished(),
              "the watchdog expired with a phase still running: a DMA that "
              "never finishes and one that deadlocks look identical from "
              "outside, which is why this bound exists");

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_neo_dma: all checks passed\n";
    return 0;
}
