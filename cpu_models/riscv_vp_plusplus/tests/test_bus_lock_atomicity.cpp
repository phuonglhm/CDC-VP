// SPDX-License-Identifier: Apache-2.0
//
// The decision record D8 multi-hart atomicity gate, and the Phase 8
// prerequisite it names.
//
// D8 deferred upstream `52d376d4` ("reworked AMO handling and fixed an
// atomicity violation (lost bus lock) bug") on one condition: it "needs a
// multi-hart AMO contention test before Phase 8; a single-threaded differential
// run against Spike cannot demonstrate atomicity between harts". This is that
// test, and Phase 8 is where a chip first has two harts sharing one address
// space.
//
// ## What makes it a test rather than a hope
//
// **The interleaving is forced, not waited for.** The shared counter's target
// performs the access *after* a `wait()`, so an AMO's load and its store are
// separated by a real SystemC yield. That is the exact window in which the
// other hart runs, and it is not artificial: an `arbitrated` local fabric, a
// contended bank and a NoC hop all block inside `b_transport` the same way.
//
// **The negative control has to fail.** Each mode runs twice: once with one
// lock shared by both harts, once with the per-CPU default. Two harts each
// holding their own lock exclude nobody, so the unshared run must lose updates.
// If it does not, this file reports that fact as a failure rather than as a
// pass -- a negative control that passes proves the positive one was measuring
// something else.
//
// **Contention is measured, not assumed.** `bus_lock_contentions()` counts the
// times a hart actually had to wait for the other. A shared run that ends with
// zero would mean the harts never overlapped, and its correct total would say
// nothing about atomicity.
//
// Both atomic mechanisms are covered, because they fail differently: `amoadd.w`
// takes the lock inside one instruction, while `lr.w`/`sc.w` holds it across
// two and D19 already records that a reset in between must release it.
//
// A third mode, `forward-progress`, answers a question decision record D19
// raised and could not settle with one hart. D19 predicted that a reservation
// left behind by a reset "becomes a hang the moment multi-hart atomicity makes
// it chip-shared" -- one hart holding the lock and never releasing it, the
// other stuck. Measured, that does not happen, and the reason is in the ISS
// rather than in this wrapper: upstream implements the RISC-V forward-progress
// property by arming a 17-instruction counter at `lr.w` and calling
// `release_lr_sc_reservation()` when it expires
// (`rv32/iss_ctemplate.cpp:301`), which releases the bus lock too. A hart
// therefore *cannot* strand the lock by taking a reservation and wandering off.
//
// The mode pins that behaviour: a hart takes the lock with `lr.w` and then
// spins forever without an `sc.w`, and the sibling is expected to be blocked
// for a while and then to finish on its own. If upstream ever drops the bound,
// this test hangs into its timeout rather than the fact going unnoticed.
//
// Usage: test_bus_lock_atomicity <amo|lrsc|forward-progress> [shared|unshared]
// Exit codes: 0 pass, 1 fail.

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cdc/cpu/cpu_base.h>

#include "riscv_vp_plusplus_wrapper.h"

namespace {

int failures = 0;

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr std::uint64_t kMemorySize = 64 * 1024;
/// The one contended word. `lui t0, 0x4` puts it there.
constexpr std::uint64_t kCounterAddress = 0x4000;
/// Iterations per hart, and therefore `2 * kIterations` if every update lands.
constexpr std::uint32_t kIterations = 64;
constexpr unsigned kHartCount = 2;

/// How long the contended word takes to answer.
///
/// Any non-zero value works; what matters is that it is a `wait()` and not an
/// annotated delay, because only a `wait()` lets the other hart run between an
/// AMO's load and its store.
const sc_core::sc_time kContendedAccess(20, sc_core::SC_NS);

/// `amoadd.w zero, t1, (t0)` in a loop. Assembled with the pinned cross
/// toolchain (`riscv-none-elf-as -march=rv32imafdv_zicsr_zvl512b
/// -mabi=ilp32d`) and pasted, so this gate needs no cross compiler:
///
///   lui   t0, 0x4          ; li t1, 1 ; li t2, 64
///   loop: amoadd.w zero, t1, (t0)
///         addi t2, t2, -1  ; bnez t2, loop
///   halt: j halt
constexpr std::uint32_t kAmoProgram[] = {
    0x000042B7u, 0x00100313u, 0x04000393u, 0x0062A02Fu,
    0xFFF38393u, 0xFE039CE3u, 0x0000006Fu,
};
constexpr std::uint64_t kAmoHaltPc = 0x18;

/// The same increment written as an LR/SC critical section:
///
///   lui   t0, 0x4          ; li t1, 1 ; li t2, 64
///   outer: lr.w t3, (t0)   ; add t3, t3, t1 ; sc.w t4, t3, (t0)
///          bnez t4, outer  ; addi t2, t2, -1 ; bnez t2, outer
///   halt: j halt
///
/// The retry on a failed `sc.w` is required by the architecture and is also
/// what keeps the unshared run from deadlocking: a hart that loses its
/// reservation goes round again instead of waiting for something.
constexpr std::uint32_t kLrScProgram[] = {
    0x000042B7u, 0x00100313u, 0x04000393u, 0x1002AE2Fu, 0x006E0E33u,
    0x19C2AEAFu, 0xFE0E9AE3u, 0xFFF38393u, 0xFE0396E3u, 0x0000006Fu,
};
constexpr std::uint64_t kLrScHaltPc = 0x24;

/// One hart takes the lock and walks away; the other tries to work.
///
///   csrr t0, mhartid ; lui t1, 0x4 ; bnez t0, worker
///   holder: lw t5, 4(t1) ; bnez t5, spin      # only ever take it once
///           li t5, 1 ; sw t5, 4(t1) ; lr.w t2, (t1)
///   spin:   j spin
///   worker: li t3, 1 ; li t4, 8
///   wloop:  amoadd.w zero, t3, (t1) ; addi t4, t4, -1 ; bnez t4, wloop
///   whalt:  j whalt
///
/// The flag at `0x4004` makes the holder take the lock exactly once, so a
/// restarted hart cannot take it again and hide the release under a second
/// acquisition.
constexpr std::uint32_t kHoldProgram[] = {
    0xF14022F3u, 0x00004337u, 0x00029E63u, 0x00432F03u, 0x000F1863u,
    0x00100F13u, 0x01E32223u, 0x100323AFu, 0x0000006Fu, 0x00100E13u,
    0x00800E93u, 0x01C3202Fu, 0xFFFE8E93u, 0xFE0E9CE3u, 0x0000006Fu,
};
constexpr std::uint64_t kHoldHaltPc = 0x38;
constexpr std::uint32_t kHoldIterations = 8;

/// Flat memory for both harts, with one deliberately slow word.
class shared_memory : public sc_core::sc_module {
public:
    sc_core::sc_vector<tlm_utils::simple_target_socket_tagged<shared_memory>>
        socket;

    shared_memory(sc_core::sc_module_name name, const std::uint32_t* program,
                  std::size_t words)
        : sc_core::sc_module(name)
        , socket("socket", kHartCount)
        , storage_(kMemorySize, 0)
    {
        for (unsigned i = 0; i < kHartCount; ++i) {
            socket[i].register_b_transport(this, &shared_memory::b_transport,
                                           static_cast<int>(i));
            socket[i].register_transport_dbg(
                this, &shared_memory::transport_dbg, static_cast<int>(i));
        }
        for (std::size_t i = 0; i < words; ++i) {
            std::memcpy(&storage_[i * 4], &program[i], 4);
        }
    }

    std::uint32_t counter() const
    {
        std::uint32_t value = 0;
        std::memcpy(&value, &storage_[kCounterAddress], 4);
        return value;
    }

    /// Accesses that took the slow path. Both harts must appear here, or the
    /// contended word was never the contended word.
    std::uint64_t contended_accesses(unsigned hart) const
    {
        return contended_[hart];
    }

private:
    bool is_counter(std::uint64_t address, unsigned length) const
    {
        return address < kCounterAddress + 4 && address + length > kCounterAddress;
    }

    void b_transport(int id, tlm::tlm_generic_payload& trans,
                     sc_core::sc_time& delay)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        if (address + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        if (is_counter(address, length)) {
            // **The access happens after the wait, not before it.**
            //
            // That ordering is the whole mechanism. A target that copied the
            // bytes first and then waited would make every read-modify-write
            // look atomic no matter what the lock did, because the load would
            // have taken its value before anything else could run. Waiting
            // first models a real slow target -- the data is sampled when the
            // access completes -- and leaves a window the other hart can use.
            ++contended_[id];
            sc_core::wait(kContendedAccess);
        } else {
            delay += sc_core::sc_time(1, sc_core::SC_NS);
        }

        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&storage_[address], trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), &storage_[address], length);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    unsigned int transport_dbg(int, tlm::tlm_generic_payload& trans)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        if (address + length > storage_.size()) {
            return 0;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&storage_[address], trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), &storage_[address], length);
        }
        return length;
    }

    std::vector<unsigned char> storage_;
    std::uint64_t contended_[kHartCount] = {};
};

void usage()
{
    std::cerr << "usage: test_bus_lock_atomicity <amo|lrsc> "
                 "<shared|unshared>\n"
                 "       test_bus_lock_atomicity forward-progress\n";
}

/// Builds the two harts on one shared lock, wired to `ram`.
std::vector<std::unique_ptr<cdc::cpu::riscv_vp_plusplus_cpu>> build_harts(
    shared_memory& ram, bool share,
    std::shared_ptr<cdc::cpu::shared_bus_lock>& lock_out)
{
    std::vector<std::unique_ptr<cdc::cpu::riscv_vp_plusplus_cpu>> cpus;
    for (unsigned i = 0; i < kHartCount; ++i) {
        cdc::cpu::cpu_config config;
        config.xlen = 32;
        config.hart_id = i;
        config.reset_pc = 0;
        cpus.push_back(std::make_unique<cdc::cpu::riscv_vp_plusplus_cpu>(
            sc_core::sc_module_name(("cpu" + std::to_string(i)).c_str()),
            config));
        cpus.back()->data_bus().bind(ram.socket[i]);
    }
    if (share) {
        // What Phase 8 adds. One lock, created once, handed to both harts --
        // the chip is what owns it in the real composition (plan §11.9).
        lock_out = cdc::cpu::make_shared_bus_lock();
        for (auto& cpu : cpus) {
            cpu->attach_bus_lock(lock_out);
        }
    }
    return cpus;
}

/// D19's chip-shared-lock question, answered by measurement.
///
/// The premise this started from was that a hart holding the lock and never
/// releasing it would strand its sibling until something reset it. It does not,
/// and the reason is worth pinning: the ISS releases the reservation -- and
/// with it the bus lock -- 16 instructions after the `lr.w`, because the RISC-V
/// specification lets an implementation fail any LR/SC sequence that does not
/// maintain forward progress, and upstream bounds it rather than trusting the
/// guest.
int run_forward_progress()
{
    tlm::tlm_global_quantum::instance().set(
        sc_core::sc_time(100, sc_core::SC_NS));

    shared_memory ram("ram", kHoldProgram, std::size(kHoldProgram));
    std::shared_ptr<cdc::cpu::shared_bus_lock> lock;
    auto cpus = build_harts(ram, /*share=*/true, lock);

    // Generous: the sibling needs the holder's reservation to lapse first, and
    // then eight contended read-modify-writes of its own.
    sc_core::sc_start(sc_core::sc_time(400, sc_core::SC_US));

    std::cout << "mode=forward-progress counter=" << ram.counter()
              << " expected=" << kHoldIterations
              << " holder_still_holds=" << cpus[0]->holds_bus_lock()
              << " acquisitions=" << cpus[0]->bus_lock_acquisitions()
              << " contentions=" << cpus[0]->bus_lock_contentions() << '\n';

    // The holder really did take it: one acquisition for its `lr.w` and one per
    // sibling `amoadd.w`. Without this the rest would also be satisfied by a
    // run in which the `lr.w` never executed.
    CHECK_MSG(cpus[0]->bus_lock_acquisitions() >= kHoldIterations + 1,
              "the lock was taken fewer times than the two programs must take "
              "it, so the holder's `lr.w` never ran");

    CHECK_MSG(cpus[0]->bus_lock_contentions() > 0,
              "the sibling never waited for the lock, so the holder's "
              "reservation was not observable and this run measures nothing");

    CHECK_MSG(!cpus[0]->holds_bus_lock(),
              "the holder still owns the lock. Upstream's forward-progress "
              "bound (rv32/iss_ctemplate.cpp:301) is what releases it, and if "
              "that is gone then a guest can strand every other hart on the "
              "chip by executing one `lr.w` and branching away");

    CHECK_MSG(ram.counter() == kHoldIterations,
              "the sibling did not finish: the counter is "
                  + std::to_string(ram.counter()) + ", expected "
                  + std::to_string(kHoldIterations)
                  + ". A hart that holds the lock and walks away must not be "
                    "able to stop the other one permanently");

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_bus_lock_atomicity: all checks passed\n";
    return 0;
}

} // namespace

int sc_main(int argc, char* argv[])
{
    if (argc == 2 && std::string(argv[1]) == "forward-progress") {
        return run_forward_progress();
    }
    if (argc != 3) {
        usage();
        return 1;
    }
    const std::string mode = argv[1];
    const std::string locking = argv[2];
    if ((mode != "amo" && mode != "lrsc")
        || (locking != "shared" && locking != "unshared")) {
        usage();
        return 1;
    }

    const bool amo = mode == "amo";
    const bool share = locking == "shared";
    const std::uint32_t* program = amo ? kAmoProgram : kLrScProgram;
    const std::size_t words = amo ? std::size(kAmoProgram)
                                  : std::size(kLrScProgram);
    const std::uint64_t halt_pc = amo ? kAmoHaltPc : kLrScHaltPc;

    // Force the harts to interleave, the way `test_fp_concurrency` does.
    //
    // The wrapper's own default is 1 us -- 100 ISS cycles -- which lets a hart
    // run its whole loop between yields, and a run in which the harts barely
    // overlap tests almost nothing. Ten cycles is small enough to interleave
    // and still legal (the ISS asserts a non-zero multiple of its 10 ns cycle
    // time), and it has to be set *before* the first CPU is constructed,
    // because the ISS reads the global quantum in its constructor.
    tlm::tlm_global_quantum::instance().set(
        sc_core::sc_time(100, sc_core::SC_NS));

    shared_memory ram("ram", program, words);
    std::shared_ptr<cdc::cpu::shared_bus_lock> lock;
    auto cpus = build_harts(ram, share, lock);

    // Run in slices and stop as soon as both harts are spinning at `halt`,
    // rather than always burning the full watchdog.
    const sc_core::sc_time slice(50, sc_core::SC_US);
    const sc_core::sc_time watchdog(4, sc_core::SC_MS);
    bool finished = false;
    while (sc_core::sc_time_stamp() < watchdog) {
        sc_core::sc_start(slice);
        if (cpus[0]->get_pc() == halt_pc && cpus[1]->get_pc() == halt_pc) {
            finished = true;
            break;
        }
    }

    const std::uint32_t counter = ram.counter();
    const std::uint32_t expected = kHartCount * kIterations;

    std::cout << "mode=" << mode << " locking=" << locking
              << " counter=" << counter << " expected=" << expected
              << " sharers=" << cpus[0]->bus_lock_sharers()
              << " acquisitions=" << cpus[0]->bus_lock_acquisitions()
              << " contentions=" << cpus[0]->bus_lock_contentions()
              << " contended_accesses=" << ram.contended_accesses(0) << '/'
              << ram.contended_accesses(1) << '\n';

    CHECK_MSG(finished,
              "the watchdog expired before both harts reached their halt "
              "loop; nothing below describes a completed run");

    // Both harts must have reached the contended word. A run in which one hart
    // never touched it would produce a plausible total for the wrong reason.
    CHECK_MSG(ram.contended_accesses(0) > 0 && ram.contended_accesses(1) > 0,
              "one of the harts never accessed the contended word");

    if (share) {
        CHECK_MSG(cpus[0]->bus_lock_sharers() == kHartCount
                      && cpus[1]->bus_lock_sharers() == kHartCount,
                  "attach_bus_lock() did not leave both harts on one lock");
        CHECK_MSG(cpus[0]->bus_lock_contentions() > 0,
                  "no hart ever waited for the other, so the harts never "
                  "overlapped and this run measures nothing about atomicity");
        CHECK_MSG(counter == expected,
                  "updates were lost with one shared lock: the counter is "
                      + std::to_string(counter) + ", expected "
                      + std::to_string(expected));
    } else {
        CHECK_MSG(cpus[0]->bus_lock_sharers() == 1
                      && cpus[1]->bus_lock_sharers() == 1,
                  "the unshared control was not actually unshared");
        CHECK_MSG(cpus[0]->bus_lock_contentions() == 0
                      && cpus[1]->bus_lock_contentions() == 0,
                  "a per-CPU lock reported contention, which it cannot have: "
                  "no other hart can take it");
        CHECK_MSG(counter < expected,
                  "the negative control passed: two harts with private locks "
                  "produced the correct total "
                      + std::to_string(counter)
                      + ", so the shared-lock run above is not evidence that "
                        "the lock is what makes these updates atomic. Either "
                        "the harts no longer interleave or the contended "
                        "target stopped yielding");
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_bus_lock_atomicity: all checks passed\n";
    return 0;
}
