// SPDX-License-Identifier: Apache-2.0
//
// NEO Local SRAM Fabric tests — the Phase 3 gate for the native local-data
// plane (decision record D15).
//
// The gate asks for same-bank round-robin, different-bank concurrency,
// back-pressure, response ownership and TLM-request-versus-physical-beat
// counters, all under a watchdog. Those five are the second half of this file
// and they run in `arbitrated` mode, because that is the only mode in which
// they are behaviours rather than estimates: with annotation alone every
// request is processed in call order and a round-robin arbiter is
// indistinguishable from a fixed-priority one.
//
// The first half is the functional contract — decode, capacity, sizes,
// strobes, counters, debug — and runs in `annotated` mode with no simulated
// time at all, which is also the proof that that mode never blocks.
//
// The fairness check is deliberately a *starvation* check rather than an
// exact-alternation check. Exact alternation is an artefact of how many beats
// each requester happens to ask for; what a fixed-priority arbiter would do,
// and what must not happen, is that one requester never gets the bank.

// `sc_spawn` is how the contention scenarios get one process per requester
// without hard-coding how many there are. It is a dynamic process, so the
// macro has to be defined before <systemc> is reached.
#define SC_INCLUDE_DYNAMIC_PROCESSES

#include "tpu_v3/core/neo_local_sram_fabric.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <systemc>

namespace tpu = cdc::components::tpu_v3;
namespace core = cdc::components::tpu_v3::core;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

using core::local_fabric_timing;
using core::neo_local_sram_fabric;
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

constexpr std::uint64_t kBase = 0xC000'0000ull;
constexpr std::uint64_t kCapacity = 64 * 1024;

sram::core_sram_config sram_config()
{
    sram::core_sram_config config;
    config.base_address = kBase;
    config.window_bytes = am::core_sram_window;
    config.capacity_bytes = kCapacity;
    return config;
}

/// 128-bit x 4 banks, one pipeline stage. Narrow enough that a 64-byte vector
/// access spans every bank, which is what makes beat splitting and contention
/// visible at all.
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

std::vector<neo_requester> all_requesters()
{
    return {neo_requester::cpu, neo_requester::dma, neo_requester::sa,
            neo_requester::transform, neo_requester::external_inbound};
}

neo_local_response access(neo_local_sram_fabric& fabric,
                          neo_requester requester, neo_command command,
                          std::uint64_t address, std::uint32_t size,
                          unsigned char* data,
                          const unsigned char* strobes = nullptr)
{
    neo_local_request request;
    request.requester = requester;
    request.command = command;
    request.address = address;
    request.size = size;
    request.data = data;
    request.strobes = strobes;

    neo_local_response response;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    fabric.b_access(request, response, delay);
    return response;
}

// ── annotated mode: the functional contract ──────────────────────────────────

void every_size_and_alignment_round_trips(neo_local_sram_fabric& fabric)
{
    std::vector<unsigned char> pattern(sram::neo_max_transfer_bytes);
    std::vector<unsigned char> back(sram::neo_max_transfer_bytes);

    for (std::uint32_t size = 1; size <= sram::neo_max_transfer_bytes; ++size) {
        for (std::uint32_t align = 0; align < 64; ++align) {
            const std::uint64_t address = kBase + 8192 + align * 96;
            for (std::uint32_t i = 0; i < size; ++i) {
                pattern[i] =
                    static_cast<unsigned char>((size * 5 + align * 11 + i) & 0xFF);
            }

            const auto wrote = access(fabric, neo_requester::cpu,
                                      neo_command::write, address, size,
                                      pattern.data());
            CHECK(wrote.status == neo_status::ok);
            CHECK(wrote.bytes == size);
            CHECK_MSG(wrote.beats == fabric.beats_for(address, size),
                      "beat count disagrees with the configured geometry at "
                      "size " + std::to_string(size) + " alignment "
                          + std::to_string(align));

            std::fill(back.begin(), back.end(), 0xEE);
            const auto read = access(fabric, neo_requester::cpu,
                                     neo_command::read, address, size,
                                     back.data());
            CHECK(read.status == neo_status::ok);
            for (std::uint32_t i = 0; i < size; ++i) {
                CHECK_MSG(back[i] == pattern[i],
                          "size " + std::to_string(size) + " alignment "
                              + std::to_string(align) + " byte "
                              + std::to_string(i));
            }
        }
    }
}

void requests_and_beats_are_different_numbers(neo_local_sram_fabric& fabric)
{
    // Decision record D7's rule made mechanical: if the fabric splits a
    // payload it records one request and the actual number of beats, and
    // neither may be presented as the other. At 16 bytes per beat a 64-byte
    // access is one request and four beats.
    fabric.reset();

    std::array<unsigned char, 64> data{};
    const auto response = access(fabric, neo_requester::dma,
                                 neo_command::write, kBase + 4096, 64,
                                 data.data());
    CHECK(response.status == neo_status::ok);
    CHECK(response.beats == 4);
    CHECK(response.bytes == 64);

    const auto& counters = fabric.counters(neo_requester::dma);
    CHECK(counters.request_count == 1);
    CHECK_MSG(counters.physical_beat_count == 4,
              "one 64-byte request is four 16-byte beats, and the two counts "
              "must not be conflated");
    CHECK(counters.transferred_bytes == 64);

    // An unaligned access costs an extra beat, which is the whole reason the
    // count is measured rather than derived from the size.
    const auto split = access(fabric, neo_requester::dma, neo_command::write,
                              kBase + 4096 + 8, 64, data.data());
    CHECK(split.beats == 5);
    CHECK(fabric.counters(neo_requester::dma).request_count == 2);
    CHECK(fabric.counters(neo_requester::dma).physical_beat_count == 9);

    // A 1-byte access is one beat whatever the geometry.
    unsigned char one = 0x5A;
    CHECK(access(fabric, neo_requester::dma, neo_command::write, kBase + 4097,
                 1, &one).beats
          == 1);
}

void the_bank_mapping_is_low_order_interleaved(neo_local_sram_fabric& fabric)
{
    // 16 bytes per beat, four banks: consecutive beats of one sequential burst
    // land on consecutive banks, which is what lets a wide access use the
    // whole array instead of hammering one macro.
    CHECK(fabric.bank_of(kBase + 0) == 0);
    CHECK(fabric.bank_of(kBase + 15) == 0);
    CHECK(fabric.bank_of(kBase + 16) == 1);
    CHECK(fabric.bank_of(kBase + 48) == 3);
    CHECK(fabric.bank_of(kBase + 64) == 0);
    CHECK(fabric.config().stripe_bytes() == 64);
}

void errors_are_classified_and_never_return_data(neo_local_sram_fabric& fabric)
{
    fabric.reset();

    std::array<unsigned char, 8> buffer{};
    buffer.fill(0x3C);

    // Inside the window, above the capacity.
    auto response = access(fabric, neo_requester::sa, neo_command::read,
                           kBase + kCapacity, 8, buffer.data());
    CHECK(response.status == neo_status::capacity_error);
    CHECK(response.bytes == 0);
    CHECK(response.beats == 0);

    // Outside the window.
    response = access(fabric, neo_requester::sa, neo_command::read,
                      kBase + am::core_sram_window, 8, buffer.data());
    CHECK(response.status == neo_status::decode_error);

    // Zero and oversized.
    CHECK(access(fabric, neo_requester::sa, neo_command::read, kBase, 0,
                 buffer.data())
              .status
          == neo_status::size_error);
    std::array<unsigned char, 65> big{};
    CHECK(access(fabric, neo_requester::sa, neo_command::write, kBase, 65,
                 big.data())
              .status
          == neo_status::size_error);

    for (auto byte : buffer) {
        CHECK_MSG(byte == 0x3C,
                  "a refused access must leave the caller's buffer untouched; "
                  "an error is never converted to zero data");
    }
    CHECK(fabric.counters(neo_requester::sa).error_count == 4);
    // Refused requests are still requests. Counting them here as well as in
    // `error_count` is what makes `request_count >= error_count` hold and the
    // failure rate a ratio of two numbers that count the same thing; a counter
    // that silently dropped failures would hide the interesting case.
    CHECK(fabric.counters(neo_requester::sa).request_count == 4);
    CHECK(fabric.counters(neo_requester::sa).transferred_bytes == 0);
}

void transferred_bytes_counts_bytes_moved_not_bytes_named(
    neo_local_sram_fabric& fabric)
{
    // Decision record D7 requires each counter to mean what its name says. A
    // masked write moves fewer bytes than it names, and counting the whole
    // payload would report bandwidth the model never carried.
    fabric.reset();

    std::array<unsigned char, 8> data{1, 2, 3, 4, 5, 6, 7, 8};
    const std::array<unsigned char, 8> strobe{1, 0, 1, 0, 0, 0, 1, 0};

    const auto response = access(fabric, neo_requester::cpu,
                                 neo_command::write, kBase + 4096, 8,
                                 data.data(), strobe.data());
    CHECK(response.status == neo_status::ok);
    CHECK_MSG(response.bytes == 3,
              "three strobes were set, so three bytes were transferred");
    CHECK(fabric.counters(neo_requester::cpu).transferred_bytes == 3);
    // The beat count is unchanged: the access still occupied its bank.
    CHECK(response.beats == 1);

    // A fully masked write moves nothing at all, and must not be reported as
    // if it had moved a payload.
    const std::array<unsigned char, 8> none{};
    const auto masked = access(fabric, neo_requester::cpu, neo_command::write,
                               kBase + 4096, 8, data.data(), none.data());
    CHECK(masked.status == neo_status::ok);
    CHECK(masked.bytes == 0);
    CHECK(fabric.counters(neo_requester::cpu).request_count == 2);
    CHECK(fabric.counters(neo_requester::cpu).transferred_bytes == 3);
}

void every_named_requester_reaches_storage_and_the_error_path(
    neo_local_sram_fabric& fabric)
{
    // The Phase 3 gate names all four internal requesters plus the external
    // inbound one. Transform in particular has no engine behind it until
    // Phase 6, so without this it would be a port that exists and has never
    // carried a transaction — and a port nothing has used is a port nothing
    // has tested.
    fabric.reset();

    const std::array<neo_requester, 5> requesters{
        neo_requester::cpu, neo_requester::dma, neo_requester::sa,
        neo_requester::transform, neo_requester::external_inbound};

    std::uint64_t address = kBase + 16384;
    for (auto requester : requesters) {
        std::array<unsigned char, 8> pattern{};
        pattern.fill(static_cast<unsigned char>(
            0x40 + static_cast<unsigned>(requester)));

        CHECK_MSG(access(fabric, requester, neo_command::write, address, 8,
                         pattern.data())
                          .status
                      == neo_status::ok,
                  std::string("requester ") + sram::to_string(requester)
                      + " could not reach the SRAM");

        std::array<unsigned char, 8> back{};
        CHECK(access(fabric, requester, neo_command::read, address, 8,
                     back.data())
                  .status
              == neo_status::ok);
        CHECK_MSG(back == pattern,
                  std::string("requester ") + sram::to_string(requester)
                      + " read back somebody else's data");

        // ...and the same requester routed to an address nothing backs.
        CHECK(access(fabric, requester, neo_command::read,
                     kBase + am::core_sram_window, 8, back.data())
                  .status
              == neo_status::decode_error);

        CHECK(fabric.counters(requester).request_count == 3);
        CHECK(fabric.counters(requester).error_count == 1);
        address += 64;
    }
}

void strobes_are_honoured_across_a_bank_boundary(neo_local_sram_fabric& fabric)
{
    // Straddling a beat boundary is where a strobe offset gets dropped: the
    // second beat's strobes have to be offset by the bytes the first consumed.
    const std::uint64_t address = kBase + 4096 + 12;

    std::array<unsigned char, 8> initial{1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(access(fabric, neo_requester::cpu, neo_command::write, address, 8,
                 initial.data())
              .status
          == neo_status::ok);

    std::array<unsigned char, 8> update{0xB0, 0xB1, 0xB2, 0xB3,
                                        0xB4, 0xB5, 0xB6, 0xB7};
    const std::array<unsigned char, 8> strobe{0, 1, 0, 1, 1, 0, 1, 0};
    CHECK(access(fabric, neo_requester::cpu, neo_command::write, address, 8,
                 update.data(), strobe.data())
              .status
          == neo_status::ok);

    std::array<unsigned char, 8> back{};
    CHECK(access(fabric, neo_requester::cpu, neo_command::read, address, 8,
                 back.data())
              .status
          == neo_status::ok);
    const std::array<unsigned char, 8> expected{1,    0xB1, 3,    0xB3,
                                                0xB4, 6,    0xB6, 8};
    CHECK_MSG(back == expected,
              "a strobed write straddling a bank boundary lost the offset");
}

void annotated_mode_charges_contention_without_blocking(
    neo_local_sram_fabric& fabric)
{
    fabric.reset();

    // Both requests "arrive" at the same instant, because each carries its own
    // zero delay. The first has the bank to itself; the second finds it busy.
    std::array<unsigned char, 16> data{};

    neo_local_request first;
    first.requester = neo_requester::cpu;
    first.command = neo_command::write;
    first.address = kBase + 4096; // bank 0
    first.size = 16;
    first.data = data.data();
    neo_local_response first_response;
    sc_core::sc_time first_delay = sc_core::SC_ZERO_TIME;
    fabric.b_access(first, first_response, first_delay);

    neo_local_request same_bank = first;
    same_bank.requester = neo_requester::dma;
    same_bank.address = kBase + 4096 + 64; // also bank 0
    neo_local_response same_response;
    sc_core::sc_time same_delay = sc_core::SC_ZERO_TIME;
    fabric.b_access(same_bank, same_response, same_delay);

    neo_local_request other_bank = first;
    other_bank.requester = neo_requester::sa;
    other_bank.address = kBase + 4096 + 16; // bank 1, untouched
    neo_local_response other_response;
    sc_core::sc_time other_delay = sc_core::SC_ZERO_TIME;
    fabric.b_access(other_bank, other_response, other_delay);

    CHECK(first_response.bank_conflicts == 0);
    CHECK_MSG(same_response.bank_conflicts == 1,
              "the second request to a busy bank must be charged for it");
    CHECK_MSG(other_response.bank_conflicts == 0,
              "a different bank must not be charged for someone else's "
              "occupancy");
    CHECK_MSG(same_delay > first_delay,
              "back-pressure must show up as latency even when nothing blocks");
    CHECK(other_delay == first_delay);

    // ...and no simulated time was consumed by any of it, which is the
    // property that makes this mode safe behind a detailed-NoC target.
    CHECK_MSG(sc_core::sc_time_stamp() == sc_core::SC_ZERO_TIME,
              "annotated mode must not advance simulated time");
}

void debug_access_bypasses_the_counters_but_not_the_bounds(
    neo_local_sram_fabric& fabric)
{
    fabric.reset();

    const std::array<unsigned char, 4> value{0xCA, 0xFE, 0xBA, 0xBE};
    neo_local_request request;
    request.requester = neo_requester::cpu;
    request.command = neo_command::write;
    request.address = kBase + 2048;
    request.size = 4;
    request.data = const_cast<unsigned char*>(value.data());
    CHECK(fabric.dbg_access(request) == 4);

    std::array<unsigned char, 4> back{};
    request.command = neo_command::read;
    request.data = back.data();
    CHECK(fabric.dbg_access(request) == 4);
    CHECK(back == value);

    CHECK_MSG(fabric.total_requests() == 0,
              "a loader is not workload traffic and must not be counted");
    CHECK(fabric.total_beats() == 0);

    // Bounds still apply.
    request.address = kBase + kCapacity;
    CHECK(fabric.dbg_access(request) == 0);
    request.address = kBase + am::core_sram_window;
    CHECK(fabric.dbg_access(request) == 0);
}

void an_unattached_requester_is_a_defect(neo_local_sram_fabric& cpu_only)
{
    // Unattributable traffic is worse than refused traffic: it silently
    // corrupts every counter that follows, so it throws rather than returning
    // a status the requester could "handle".
    std::array<unsigned char, 4> data{};
    bool threw = false;
    try {
        (void)access(cpu_only, neo_requester::sa, neo_command::read, kBase, 4,
                     data.data());
    } catch (const std::runtime_error&) {
        threw = true;
    }
    CHECK(threw);
    CHECK(cpu_only.is_attached(neo_requester::cpu));
    CHECK(!cpu_only.is_attached(neo_requester::sa));
}

// ── arbitrated mode: the contention gate ─────────────────────────────────────

struct requester_job {
    neo_requester requester;
    std::uint64_t address = 0;
    std::uint32_t size = 16;
    /// Zero means "keep going until the deadline", which is how starvation is
    /// measured: a fixed-priority arbiter would leave the low-priority
    /// requesters with far fewer grants than the top one.
    unsigned iterations = 0;
};

class contention_bench : public sc_core::sc_module {
public:
    contention_bench(sc_core::sc_module_name name,
                     neo_local_sram_fabric& fabric,
                     std::vector<requester_job> jobs, sc_core::sc_time deadline)
        : sc_core::sc_module(name)
        , fabric_(fabric)
        , jobs_(std::move(jobs))
        , deadline_(deadline)
        , completed_(jobs_.size(), 0)
    {
        for (std::size_t i = 0; i < jobs_.size(); ++i) {
            sc_core::sc_spawn([this, i] { run(i); });
        }
    }

    unsigned completed(std::size_t job) const { return completed_[job]; }
    unsigned mismatches() const noexcept { return mismatches_; }
    sc_core::sc_time last_finish() const noexcept { return last_finish_; }
    bool all_done() const noexcept { return done_ == jobs_.size(); }

private:
    void run(std::size_t index)
    {
        const requester_job job = jobs_[index];
        std::vector<unsigned char> pattern(job.size);
        std::vector<unsigned char> back(job.size);

        for (unsigned iteration = 0;; ++iteration) {
            if (job.iterations != 0) {
                if (iteration >= job.iterations) {
                    break;
                }
            } else if (sc_core::sc_time_stamp() >= deadline_) {
                break;
            }

            for (std::uint32_t i = 0; i < job.size; ++i) {
                pattern[i] = static_cast<unsigned char>(
                    (static_cast<unsigned>(job.requester) * 37 + iteration * 7
                     + i)
                    & 0xFF);
            }

            neo_local_request request;
            request.requester = job.requester;
            request.command = neo_command::write;
            request.address = job.address;
            request.size = job.size;
            request.data = pattern.data();
            neo_local_response response;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            fabric_.b_access(request, response, delay);
            if (response.status != neo_status::ok) {
                ++mismatches_;
                break;
            }

            // Read it straight back. Requests are in order per requester, so
            // this must return exactly what this requester wrote — that is
            // the response-ownership property, and a fabric that returned
            // another requester's data under contention would fail here.
            std::fill(back.begin(), back.end(), 0);
            request.command = neo_command::read;
            request.data = back.data();
            delay = sc_core::SC_ZERO_TIME;
            fabric_.b_access(request, response, delay);
            if (response.status != neo_status::ok || back != pattern) {
                ++mismatches_;
                break;
            }

            ++completed_[index];
        }

        last_finish_ = sc_core::sc_time_stamp();
        ++done_;
    }

    neo_local_sram_fabric& fabric_;
    std::vector<requester_job> jobs_;
    sc_core::sc_time deadline_;
    std::vector<unsigned> completed_;
    unsigned mismatches_ = 0;
    std::size_t done_ = 0;
    sc_core::sc_time last_finish_ = sc_core::SC_ZERO_TIME;
};

/// Two processes driving **one** requester identity.
///
/// Nothing in the real wiring does this, so the test builds it: D15 allows one
/// request in flight per requester, and a blocking call only enforces that
/// while one process owns the identity. The moment the first waits for a bank
/// the second can enter, and from then on their beats interleave under one
/// name — the arbiter sees one contender where there are two and the response
/// ownership the fabric promises is gone.
class shared_identity_bench : public sc_core::sc_module {
public:
    shared_identity_bench(sc_core::sc_module_name name,
                          neo_local_sram_fabric& fabric, std::uint64_t address)
        : sc_core::sc_module(name)
        , fabric_(fabric)
        , address_(address)
    {
        sc_core::sc_spawn([this] { run(); });
        sc_core::sc_spawn([this] { run(); });
    }

    unsigned rejections() const noexcept { return rejections_; }
    unsigned completions() const noexcept { return completions_; }

private:
    void run()
    {
        std::array<unsigned char, 64> data{};
        neo_local_request request;
        request.requester = neo_requester::cpu;
        request.command = neo_command::write;
        request.address = address_;
        request.size = static_cast<std::uint32_t>(data.size());
        request.data = data.data();

        neo_local_response response;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        try {
            fabric_.b_access(request, response, delay);
            ++completions_;
        } catch (const std::runtime_error&) {
            // Caught rather than allowed to escape: an exception out of an
            // SC_THREAD takes the kernel down and the test would report a
            // crash instead of the refusal it is looking for.
            ++rejections_;
        }
    }

    neo_local_sram_fabric& fabric_;
    std::uint64_t address_;
    unsigned rejections_ = 0;
    unsigned completions_ = 0;
};

/// A reset arriving while requesters are blocked on a bank.
///
/// This is the case that used to hang. `reset()` cleared `waiting[]` and
/// `busy` without waking anybody, so a requester blocked in the arbitration
/// loop was left both unselectable and unwoken — waiting for a grant no
/// arbiter could issue. It presents as a deadlock, which is why the watchdog
/// below is the check that matters.
class reset_bench : public sc_core::sc_module {
public:
    reset_bench(sc_core::sc_module_name name, neo_local_sram_fabric& fabric,
                sc_core::sc_time reset_at)
        : sc_core::sc_module(name)
        , fabric_(fabric)
    {
        const std::array<neo_requester, 3> requesters{
            neo_requester::cpu, neo_requester::dma, neo_requester::sa};
        for (std::size_t i = 0; i < requesters.size(); ++i) {
            const neo_requester requester = requesters[i];
            const std::uint64_t address = kBase + 4096 + i * 64;
            sc_core::sc_spawn(
                [this, requester, address] { hammer(requester, address); });
        }
        sc_core::sc_spawn([this, reset_at] {
            sc_core::wait(reset_at);
            fabric_.reset();
            reset_done_ = true;
        });
    }

    unsigned finished() const noexcept { return finished_; }
    unsigned aborted() const noexcept { return aborted_; }
    bool reset_done() const noexcept { return reset_done_; }

private:
    void hammer(neo_requester requester, std::uint64_t address)
    {
        // 64-byte accesses, four beats each, all on the same bank offsets:
        // long enough that a reset lands while somebody is mid-request.
        std::array<unsigned char, 64> data{};
        for (unsigned iteration = 0; iteration < 200; ++iteration) {
            neo_local_request request;
            request.requester = requester;
            request.command = neo_command::write;
            request.address = address;
            request.size = static_cast<std::uint32_t>(data.size());
            request.data = data.data();

            neo_local_response response;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            fabric_.b_access(request, response, delay);
            if (response.status == neo_status::aborted) {
                ++aborted_;
                break;
            }
            if (response.status != neo_status::ok) {
                break;
            }
        }
        ++finished_;
    }

    neo_local_sram_fabric& fabric_;
    unsigned finished_ = 0;
    unsigned aborted_ = 0;
    bool reset_done_ = false;
};

/// A reset during the caller-supplied temporal-decoupling delay.
///
/// This is distinct from a waiter already inside bank arbitration. The first
/// implementation captured the reset generation only after consuming this
/// delay, so the resumed request adopted the new generation and executed as
/// though it had been issued after reset. It also let an old request write an
/// abort into counters that reset had just cleared.
class delayed_reset_bench : public sc_core::sc_module {
public:
    delayed_reset_bench(sc_core::sc_module_name name,
                        neo_local_sram_fabric& fabric,
                        sc_core::sc_time request_delay,
                        sc_core::sc_time reset_at)
        : sc_core::sc_module(name)
        , fabric_(fabric)
        , request_delay_(request_delay)
    {
        sc_core::sc_spawn([this] { access(); });
        sc_core::sc_spawn([this, reset_at] {
            sc_core::wait(reset_at);
            fabric_.reset();
            reset_done_ = true;
        });
    }

    bool finished() const noexcept { return finished_; }
    bool reset_done() const noexcept { return reset_done_; }
    const neo_local_response& response() const noexcept { return response_; }
    sc_core::sc_time remaining_delay() const noexcept { return remaining_delay_; }

private:
    void access()
    {
        std::array<unsigned char, 16> data{};
        data.fill(0xA6);

        neo_local_request request;
        request.requester = neo_requester::cpu;
        request.command = neo_command::write;
        request.address = kBase + 8192;
        request.size = static_cast<std::uint32_t>(data.size());
        request.data = data.data();

        remaining_delay_ = request_delay_;
        fabric_.b_access(request, response_, remaining_delay_);
        finished_ = true;
    }

    neo_local_sram_fabric& fabric_;
    sc_core::sc_time request_delay_;
    sc_core::sc_time remaining_delay_ = sc_core::SC_ZERO_TIME;
    neo_local_response response_{};
    bool finished_ = false;
    bool reset_done_ = false;
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
    const sc_core::sc_time cycle(1, sc_core::SC_NS);
    const sc_core::sc_time deadline(200, sc_core::SC_NS);

    // ── annotated: functional contract, no simulated time ────────────────────
    sram::core_sram sram_annotated("sram_annotated", sram_config());
    neo_local_sram_fabric fabric("fabric_annotated", fabric_config(),
                                 sram_annotated, all_requesters(),
                                 local_fabric_timing::annotated, cycle);

    sram::core_sram sram_cpu_only("sram_cpu_only", sram_config());
    neo_local_sram_fabric cpu_only("fabric_cpu_only", fabric_config(),
                                   sram_cpu_only, {neo_requester::cpu},
                                   local_fabric_timing::annotated, cycle);

    // ── arbitrated: three requesters fighting over one bank ──────────────────
    sram::core_sram sram_same("sram_same_bank", sram_config());
    neo_local_sram_fabric fabric_same("fabric_same_bank", fabric_config(),
                                      sram_same, all_requesters(),
                                      local_fabric_timing::arbitrated, cycle);
    // 16-byte beats and four banks, so +64 bytes is the same bank.
    contention_bench bench_same(
        "bench_same_bank", fabric_same,
        {{neo_requester::cpu, kBase + 4096, 16, 0},
         {neo_requester::dma, kBase + 4096 + 64, 16, 0},
         {neo_requester::sa, kBase + 4096 + 128, 16, 0}},
        deadline);

    // ── arbitrated: three requesters on three different banks ────────────────
    sram::core_sram sram_diff("sram_diff_bank", sram_config());
    neo_local_sram_fabric fabric_diff("fabric_diff_bank", fabric_config(),
                                      sram_diff, all_requesters(),
                                      local_fabric_timing::arbitrated, cycle);
    contention_bench bench_diff(
        "bench_diff_bank", fabric_diff,
        {{neo_requester::cpu, kBase + 4096, 16, 0},
         {neo_requester::dma, kBase + 4096 + 16, 16, 0},
         {neo_requester::sa, kBase + 4096 + 32, 16, 0}},
        deadline);

    // ── arbitrated: two processes sharing one requester identity ─────────────
    sram::core_sram sram_shared("sram_shared_identity", sram_config());
    neo_local_sram_fabric fabric_shared("fabric_shared_identity",
                                        fabric_config(), sram_shared,
                                        all_requesters(),
                                        local_fabric_timing::arbitrated, cycle);
    shared_identity_bench bench_shared("bench_shared_identity", fabric_shared,
                                       kBase + 4096);

    // ── arbitrated: a reset in the middle of contention ──────────────────────
    sram::core_sram sram_reset("sram_reset", sram_config());
    neo_local_sram_fabric fabric_reset("fabric_reset", fabric_config(),
                                       sram_reset, all_requesters(),
                                       local_fabric_timing::arbitrated, cycle);
    reset_bench bench_reset("bench_reset", fabric_reset,
                            sc_core::sc_time(25, sc_core::SC_NS));

    // ── arbitrated: reset while consuming an incoming timing annotation ─────
    sram::core_sram sram_delayed_reset("sram_delayed_reset", sram_config());
    neo_local_sram_fabric fabric_delayed_reset(
        "fabric_delayed_reset", fabric_config(), sram_delayed_reset,
        {neo_requester::cpu}, local_fabric_timing::arbitrated, cycle);
    delayed_reset_bench bench_delayed_reset(
        "bench_delayed_reset", fabric_delayed_reset,
        sc_core::sc_time(50, sc_core::SC_NS),
        sc_core::sc_time(25, sc_core::SC_NS));

    // Everything below runs before `sc_start`, which is itself the check that
    // `annotated` never blocks: none of it could complete otherwise.
    every_size_and_alignment_round_trips(fabric);
    the_bank_mapping_is_low_order_interleaved(fabric);
    requests_and_beats_are_different_numbers(fabric);
    errors_are_classified_and_never_return_data(fabric);
    transferred_bytes_counts_bytes_moved_not_bytes_named(fabric);
    every_named_requester_reaches_storage_and_the_error_path(fabric);
    strobes_are_honoured_across_a_bank_boundary(fabric);
    annotated_mode_charges_contention_without_blocking(fabric);
    debug_access_bypasses_the_counters_but_not_the_bounds(fabric);
    an_unattached_requester_is_a_defect(cpu_only);

    sc_core::sc_start(
        sc_core::sc_time(kWatchdogMilliseconds, sc_core::SC_MS));

    CHECK_MSG(bench_same.all_done() && bench_diff.all_done(),
              "the watchdog expired with work still in flight: the arbiter "
              "deadlocked, which is exactly what a concurrency test without "
              "one would hide");

    // ── one request in flight per requester, enforced ────────────────────────
    CHECK_MSG(bench_shared.rejections() == 1,
              "two processes shared a requester identity and the fabric "
              "accepted both. Revision 1 allows one request in flight per "
              "requester (decision record D15)");
    CHECK(bench_shared.completions() == 1);
    // The one that was let through still ran correctly.
    CHECK(fabric_shared.counters(neo_requester::cpu).request_count == 1);
    CHECK(fabric_shared.counters(neo_requester::cpu).physical_beat_count == 4);

    // ── a reset in the middle of contention releases every waiter ────────────
    CHECK(bench_reset.reset_done());
    CHECK_MSG(bench_reset.finished() == 3,
              "a requester was still blocked when the watchdog expired: reset "
              "left a waiter unselectable and unwoken, which is the hang this "
              "case exists to catch");
    CHECK_MSG(bench_reset.aborted() > 0,
              "no requester reported `aborted`, so the reset either missed "
              "the contention window or silently completed work it was "
              "supposed to abandon (ARCHITECTURE.md §6)");

    // Reset defines a new statistics epoch. A request issued before reset and
    // still consuming its incoming delay must abort without touching storage,
    // and its unwind must not re-populate the freshly cleared counters.
    CHECK(bench_delayed_reset.reset_done());
    CHECK_MSG(bench_delayed_reset.finished(),
              "a request reset during its input delay did not return before "
              "the watchdog expired");
    CHECK(bench_delayed_reset.response().status == neo_status::aborted);
    CHECK(bench_delayed_reset.response().bytes == 0);
    CHECK(bench_delayed_reset.response().beats == 0);
    CHECK(bench_delayed_reset.remaining_delay() == sc_core::SC_ZERO_TIME);
    CHECK(fabric_delayed_reset.total_requests() == 0);
    CHECK(fabric_delayed_reset.total_beats() == 0);
    CHECK(fabric_delayed_reset.total_bytes() == 0);
    const auto& delayed_reset_cpu =
        fabric_delayed_reset.counters(neo_requester::cpu);
    CHECK(delayed_reset_cpu.request_count == 0);
    CHECK(delayed_reset_cpu.error_count == 0);
    CHECK(sram_delayed_reset.write_accesses() == 0);
    CHECK(sram_delayed_reset.bytes_written() == 0);

    // ── same bank: serialised, back-pressured, and nobody starved ────────────
    CHECK(bench_same.mismatches() == 0);
    CHECK_MSG(bench_same.all_done(), "a same-bank requester never finished");
    CHECK_MSG(fabric_same.total_bank_conflicts() > 0,
              "three requesters on one bank must produce back-pressure");

    std::array<neo_requester, 3> contenders{
        neo_requester::cpu, neo_requester::dma, neo_requester::sa};
    std::uint64_t lowest = ~0ull;
    std::uint64_t highest = 0;
    for (auto requester : contenders) {
        const std::uint64_t grants = fabric_same.bank_grants(0, requester);
        lowest = std::min(lowest, grants);
        highest = std::max(highest, grants);
        CHECK_MSG(grants > 0,
                  std::string("requester ") + sram::to_string(requester)
                      + " was starved of bank 0");
    }
    // Rotating priority shares the bank; fixed priority would not. The bound
    // is loose on purpose — exact alternation is an artefact of how many beats
    // each requester happens to ask for, while starvation is the real failure.
    CHECK_MSG(highest <= lowest + lowest / 4 + 2,
              "bank 0 was shared unevenly (" + std::to_string(lowest) + ".."
                  + std::to_string(highest)
                  + " grants); that is fixed priority, not round-robin");

    // Every beat landed on bank 0 and none on the others, so the mapping under
    // contention is the same one the functional test measured.
    for (unsigned bank = 1; bank < fabric_same.config().bank_count; ++bank) {
        for (auto requester : contenders) {
            CHECK(fabric_same.bank_grants(bank, requester) == 0);
        }
    }

    // ── different banks: concurrent, and no conflicts at all ─────────────────
    CHECK(bench_diff.mismatches() == 0);
    CHECK_MSG(bench_diff.all_done(), "a different-bank requester never finished");
    CHECK_MSG(fabric_diff.total_bank_conflicts() == 0,
              "requesters on different banks must not back-pressure each "
              "other");

    std::uint64_t same_total = 0;
    std::uint64_t diff_total = 0;
    for (auto requester : contenders) {
        same_total += fabric_same.counters(requester).request_count;
        diff_total += fabric_diff.counters(requester).request_count;
    }
    CHECK_MSG(diff_total > same_total,
              "in the same wall of simulated time, independent banks must "
              "carry more traffic than one contended bank ("
                  + std::to_string(diff_total) + " vs "
                  + std::to_string(same_total) + ')');

    std::cout << fabric_same.report() << '\n' << fabric_diff.report();

    // The D7 label has to be on the report that carries the numbers, not only
    // in a document nobody quotes alongside them.
    CHECK(fabric_same.report().find("VP++ element-wise granularity")
          != std::string::npos);
    CHECK(fabric_same.report().find("not a hardware bus transaction")
          != std::string::npos);

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "\ntest_neo_local_sram_fabric: all checks passed\n";
    return 0;
}
