// SPDX-License-Identifier: Apache-2.0
//
// The decision record D19 gate: a hart reset is a full deterministic
// architectural reset, not a restart.
//
// Audit F8 established that `ISS::init()` clears almost nothing. Phase 7 then
// found that it also could not be called twice — `genOpMap()` refuses to
// overwrite an already-filled op map, so `reset_cpu()` threw on its second
// call. Both are fixed; this file is what keeps them fixed.
//
// **The state is dirtied by executing instructions, not by writing to the
// model.** The wrapper exposes readers and no mutators on purpose, and running
// real code is the stronger test anyway: it produces the state a workload
// actually leaves behind, including the vector configuration, rather than the
// state someone remembered to poke. Each class below is checked non-zero
// *before* the reset, so "it is zero afterwards" is a statement about the
// reset rather than about a register that was never touched — the positive
// statement rule D12's `vstart` check uses.
//
// Exit codes: 0 pass, 1 fail.

#include <algorithm>
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

// ── CSR addresses this gate names explicitly ─────────────────────────────────
//
// Written out rather than pulled from the ISS headers, so the gate states the
// architecture's numbers and would notice if the model's ever disagreed.
constexpr unsigned kMhartid = 0xF14;
constexpr unsigned kMisa = 0x301;
constexpr unsigned kVlenb = 0xC22;
constexpr unsigned kMvendorid = 0xF11;
constexpr unsigned kMarchid = 0xF12;
constexpr unsigned kMimpid = 0xF13;
constexpr unsigned kMstatus = 0x300;
constexpr unsigned kMscratch = 0x340;
constexpr unsigned kMtvec = 0x305;
constexpr unsigned kMie = 0x304;
constexpr unsigned kVtype = 0xC21;
constexpr unsigned kVl = 0xC20;
constexpr unsigned kTime = 0xC01;
constexpr unsigned kTimeH = 0xC81;
constexpr unsigned kMtime = 0xB01;
constexpr unsigned kMtimeH = 0xB81;
constexpr unsigned kCycle = 0xC00;
constexpr unsigned kCycleH = 0xC80;
constexpr unsigned kMcycle = 0xB00;
constexpr unsigned kMcycleH = 0xB80;

constexpr std::uint32_t kVtypeVill = 0x8000'0000u;
constexpr std::uint32_t kStackTop = 0x8001'0000u;
constexpr std::uint64_t kMemorySize = 1024 * 1024;

/// Assembled with the pinned cross toolchain
/// (`riscv-none-elf-as -march=rv32imafdv_zicsr_zvl512b -mabi=ilp32d`) and
/// pasted, so the test needs no cross compiler to run:
///
///   lui   t0, 0x1          ; addi t0, t0, 564     -> t0 = 0x1234
///   csrw  mscratch, t0
///   li    t1, 1536         ; csrw mtvec, t1
///   li    s0, -1           ; li   s1, 2047
///   li    t2, 512          ; csrs mstatus, t2     -> mstatus.VS = Initial
///   vsetivli t3, 4, e32, m1, ta, ma
///   vmv.v.i  v1, 15
///   lui   t4, 0x1          ; addi t4, t4, -2048   -> mie.MEIE
///   csrw  mie, t4
///   wfi
///   loop: j loop
///
/// It dirties one register from every class the contract names: integer
/// registers, machine CSRs, the vector configuration and a vector register.
/// `mstatus.VS` has to be enabled first — the ISS constructs with VS=Off, so a
/// vector instruction before that write would trap instead of executing.
constexpr std::uint32_t kDirtyProgram[] = {
    0x000012B7u, 0x23428293u, 0x34029073u, 0x60000313u, 0x30531073u,
    0xFFF00413u, 0x7FF00493u, 0x20000393u, 0x3003A073u, 0xCD027E57u,
    0x5E07B0D7u, 0x00001EB7u, 0x800E8E93u, 0x304E9073u, 0x10500073u,
    0x0000006Fu,
};

constexpr unsigned kRegS0 = 8;
constexpr unsigned kRegS1 = 9;
constexpr unsigned kRegSp = 2;

/// Flat memory holding the program. Answers, counts, and nothing else.
class memory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<memory> socket;

    explicit memory(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(kMemorySize, 0)
    {
        socket.register_b_transport(this, &memory::b_transport);
        socket.register_transport_dbg(this, &memory::transport_dbg);

        for (unsigned i = 0; i < std::size(kDirtyProgram); ++i) {
            std::memcpy(&storage_[i * 4], &kDirtyProgram[i], 4);
        }
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        const std::uint64_t address = trans.get_address();
        const unsigned int length = trans.get_data_length();
        if (address + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            std::memcpy(&storage_[address], trans.get_data_ptr(), length);
        } else {
            std::memcpy(trans.get_data_ptr(), &storage_[address], length);
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
        delay += sc_core::sc_time(1, sc_core::SC_NS);
    }

    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
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
};

bool is_preserved(unsigned address)
{
    return address == kMhartid || address == kMisa || address == kVlenb
        || address == kMvendorid || address == kMarchid || address == kMimpid;
}

bool is_live_time(unsigned address)
{
    return address == kTime || address == kTimeH || address == kMtime
        || address == kMtimeH;
}

/// `cycle`/`mcycle` share storage with a value recomputed on read, so the
/// backing word is not what the contract is about; `mcycle` gets its own
/// check below instead of being swept up in the storage pass.
bool is_cycle(unsigned address)
{
    return address == kCycle || address == kCycleH || address == kMcycle
        || address == kMcycleH;
}

bool is_specified(unsigned address)
{
    return address == kVtype || address == kVl;
}

class scenario : public sc_core::sc_module {
public:
    scenario(sc_core::sc_module_name name,
             cdc::cpu::riscv_vp_plusplus_cpu& cpu)
        : sc_core::sc_module(name)
        , cpu_(cpu)
    {
        SC_HAS_PROCESS(scenario);
        SC_THREAD(run);
    }

    bool ran() const noexcept { return ran_; }

private:
    void run()
    {
        // Long enough for the program to run to its `wfi`.
        wait(sc_core::sc_time(20, sc_core::SC_US));
        CHECK_MSG(cpu_.read_mcycle() > 0,
                  "mcycle never advanced, so the program did not run and "
                  "nothing below is measuring a reset");

        // Wake the hart out of `wfi` and leave it spinning in `j loop`.
        //
        // This matters for the `mcycle` measurement rather than for the state
        // checks: the leak D19 asks about shows up at the first
        // `commit_cycles()` *after* the reset, so a hart that does not execute
        // afterwards makes the check vacuous. The program enabled `mie.MEIE`,
        // and `mstatus.MIE` is clear, so this wakes it without taking a trap.
        cpu_.set_irq(11, true);
        wait(sc_core::sc_time(5, sc_core::SC_US));
        cpu_.set_irq(11, false);

        const auto dirty = capture_dirty_state();
        const std::uint64_t mcycle_before = cpu_.read_mcycle();
        CHECK_MSG(mcycle_before > 100,
                  "the hart did not resume spinning after its interrupt, so "
                  "the cycle measurement below would not be exercising a "
                  "running hart");

        cpu_.reset_cpu();

        check_preserved();
        check_specified();
        check_zeroed(dirty);
        check_general_registers(dirty);
        check_vector_registers(dirty);
        check_mcycle_does_not_jump(mcycle_before);
        wfi_parked_hart_is_not_resumed();

        ran_ = true;
    }

    /// A recorded limitation, tested so it cannot change silently.
    ///
    /// `reset_cpu()` calls `maybe_interrupt_pending()`, which notifies
    /// `wfi_event` — necessary, and **not sufficient**. VP++ implements the
    /// instruction as `while (!has_local_pending_enabled_interrupts())
    /// wait(wfi_event);`, and the reset has just zeroed `mie`, so the woken
    /// hart re-evaluates the condition, finds it false and sleeps again. It
    /// never fetches from the reset vector.
    ///
    /// Decision record D19 assumed the wake was enough. It is not, for the
    /// same structural reason as the terminated-hart case: the blocking wait
    /// sits inside the ISS's instruction execution and nothing outside can
    /// unwind it. Lifting it needs a downstream patch, and until then a
    /// platform must not reset a hart that is idling in `wfi` and expect it to
    /// restart.
    ///
    /// Asserted rather than only written down, on the D10 precedent: an
    /// unobservable behaviour is recorded as tested so a future backend change
    /// makes this fail loudly instead of leaving the documentation wrong.
    void wfi_parked_hart_is_not_resumed()
    {
        // After the reset above the hart re-ran its program and is parked in
        // `wfi` again, this time with `mie` clear.
        wait(sc_core::sc_time(20, sc_core::SC_US));
        const std::uint64_t before = cpu_.read_mcycle();

        cpu_.reset_cpu();
        wait(sc_core::sc_time(20, sc_core::SC_US));

        CHECK_MSG(cpu_.read_mcycle() <= before,
                  "a hart parked in `wfi` resumed after a reset. That is the "
                  "behaviour we want, but this backend cannot currently "
                  "provide it -- if it now does, the D19 limitation note and "
                  "this check are both out of date and should be removed");
    }

    struct dirty_state {
        std::vector<unsigned> non_zero_csrs;
        std::uint32_t s0 = 0;
        std::uint32_t s1 = 0;
        unsigned non_zero_vector_bytes = 0;
        std::uint32_t vtype = 0;
    };

    /// Records what the program actually left behind, so every "is zero now"
    /// below has a "was not zero then" behind it.
    dirty_state capture_dirty_state()
    {
        dirty_state dirty;
        for (unsigned address : cpu_.mapped_csr_addresses()) {
            if (is_preserved(address) || is_live_time(address)
                || is_cycle(address)) {
                continue;
            }
            if (cpu_.read_csr_storage(address) != 0) {
                dirty.non_zero_csrs.push_back(address);
            }
        }
        dirty.s0 = cpu_.read_gpr(kRegS0);
        dirty.s1 = cpu_.read_gpr(kRegS1);
        dirty.vtype = cpu_.read_csr_storage(kVtype);
        for (unsigned byte = 0; byte < 16; ++byte) {
            if (cpu_.read_vector_byte(1, byte) != 0) {
                ++dirty.non_zero_vector_bytes;
            }
        }

        // The program writes these by name; if they are clean the image did
        // not execute as intended and the reset checks would be vacuous.
        CHECK_MSG(cpu_.read_csr_storage(kMscratch) == 0x1234,
                  "mscratch does not hold the value the dirtying program "
                  "wrote; the image did not run as expected");
        CHECK_MSG(cpu_.read_csr_storage(kMtvec) == 0x600,
                  "mtvec does not hold the value the dirtying program wrote");
        CHECK_MSG(dirty.s0 == 0xFFFF'FFFFu, "s0 was not dirtied");
        CHECK_MSG(dirty.s1 == 0x7FFu, "s1 was not dirtied");
        CHECK_MSG(dirty.non_zero_vector_bytes > 0,
                  "v1 was not dirtied; the vector unit may not have been "
                  "enabled, in which case the vector reset check below proves "
                  "nothing");
        CHECK_MSG((dirty.vtype & kVtypeVill) == 0,
                  "vtype already had vill set before the reset, so 'vill is "
                  "set afterwards' would not be a statement about the reset");
        return dirty;
    }

    // ── Class 1 ──────────────────────────────────────────────────────────────

    void check_preserved()
    {
        CHECK_MSG(cpu_.read_csr_storage(kMhartid) == 3,
                  "mhartid did not survive the reset. Identity is a property "
                  "of the instance (D5); a reset that cleared it would make "
                  "every hart in a multi-hart platform report 0");
        CHECK_MSG((cpu_.read_csr_storage(kMisa) & (1u << ('V' - 'A'))) != 0,
                  "misa.V did not survive the reset, so the hart came back "
                  "without its vector extension");
        CHECK_MSG(cpu_.read_csr_storage(kVlenb) == 64,
                  "vlenb did not survive the reset. It is written once by the "
                  "VExtension constructor and never again, so zeroing it is "
                  "not recoverable");
    }

    // ── Class 2 ──────────────────────────────────────────────────────────────

    void check_specified()
    {
        CHECK_MSG(cpu_.read_csr_storage(kVtype) == kVtypeVill,
                  "vtype is not exactly `vill` set with every other bit zero. "
                  "Asserted as the whole word because upstream's own default "
                  "claims vill in a comment while setting bit 27");
        CHECK(cpu_.read_csr_storage(kVl) == 0);
        CHECK_MSG((cpu_.read_csr_storage(kMstatus) & 0x8u) == 0,
                  "mstatus.MIE is set after reset");
        CHECK_MSG(cpu_.get_pc() == 0,
                  "the hart did not restart at its reset PC");
    }

    // ── Class 4 ──────────────────────────────────────────────────────────────

    void check_zeroed(const dirty_state& dirty)
    {
        CHECK_MSG(!dirty.non_zero_csrs.empty(),
                  "no CSR was dirty before the reset, so this sweep would pass "
                  "against a hart that had never executed anything");

        for (unsigned address : cpu_.mapped_csr_addresses()) {
            if (is_preserved(address) || is_live_time(address)
                || is_cycle(address) || is_specified(address)) {
                continue;
            }
            const std::uint32_t value = cpu_.read_csr_storage(address);
            if (value != 0) {
                std::cerr << "CHECK failed: CSR 0x" << std::hex << address
                          << " holds 0x" << value << std::dec
                          << " after reset; every mapped CSR outside the "
                             "preserved, specified and live-time sets is "
                             "zeroed for reproducibility (D19)\n";
                ++failures;
            }
        }

        // Named checks on the two the program wrote, so a sweep that silently
        // stopped iterating would still be caught.
        CHECK_MSG(cpu_.read_csr_storage(kMscratch) == 0,
                  "mscratch survived the reset");
        CHECK_MSG(cpu_.read_csr_storage(kMtvec) == 0,
                  "mtvec survived the reset");
        CHECK_MSG(cpu_.read_csr_storage(kMie) == 0,
                  "mie survived the reset");
    }

    void check_general_registers(const dirty_state& dirty)
    {
        (void)dirty;
        for (unsigned i = 0; i < 32; ++i) {
            if (i == kRegSp) {
                continue;
            }
            CHECK_MSG(cpu_.read_gpr(i) == 0,
                      "x" + std::to_string(i)
                          + " survived the reset. `RegFile_T::reset_zero()` is "
                            "not a register-file clear -- its whole body is "
                            "`regs[zero] = 0`");
        }
        CHECK_MSG(cpu_.read_gpr(kRegSp) == kStackTop,
                  "sp is not the configured stack top. It is written by "
                  "`ISS::init()` after the register file is cleared, so it is "
                  "its own class rather than part of the zeroed remainder");
    }

    void check_vector_registers(const dirty_state& dirty)
    {
        (void)dirty;
        for (unsigned byte = 0; byte < 64; ++byte) {
            CHECK_MSG(cpu_.read_vector_byte(1, byte) == 0,
                      "v1 byte " + std::to_string(byte)
                          + " survived the reset. The vector register file is "
                            "private with no bulk clear, so it is zeroed "
                            "element-wise through the public writer");
        }
    }

    void check_mcycle_does_not_jump(std::uint64_t before)
    {
        // The measurement decision record D19 asked for rather than assumed.
        //
        // `commit_cycles()` charges `dbbcache.get_cycle_counter_raw() -
        // cycle_counter_raw_last` to both `cycle_counter` and the quantum
        // keeper. `ISS::init()` sets the second to zero; if the first survives
        // a reset, the next commit re-adds the whole pre-reset count — into
        // simulated time, not merely into a counter.
        const std::uint64_t immediately_after = cpu_.read_mcycle();
        CHECK_MSG(immediately_after < before,
                  "mcycle did not drop across the reset: it read "
                      + std::to_string(immediately_after) + " against "
                      + std::to_string(before) + " before");

        // Let the hart run again and confirm the count grows from the new
        // baseline rather than leaping back to the old total. The hart is
        // spinning at reset time, so it does re-execute and a leak would show.
        wait(sc_core::sc_time(5, sc_core::SC_US));
        const std::uint64_t later = cpu_.read_mcycle();
        CHECK_MSG(later < before,
                  "mcycle jumped back to at least its pre-reset value ("
                      + std::to_string(later) + " vs " + std::to_string(before)
                      + "). The dbbcache cycle accumulator survived the reset, "
                        "so the first commit after it re-added the whole "
                        "pre-reset count. This is the case D19 left open for a "
                        "fourth patch under the D8 mechanism");
    }

    cdc::cpu::riscv_vp_plusplus_cpu& cpu_;
    bool ran_ = false;
};

} // namespace

int sc_main(int, char*[])
{
    cdc::cpu::cpu_config config;
    config.xlen = 32;
    // Deliberately not 0: a reset that cleared `mhartid` would be invisible on
    // hart 0, which is exactly the failure D5 exists to prevent.
    config.hart_id = 3;
    config.reset_pc = 0;

    cdc::cpu::riscv_vp_plusplus_cpu cpu("cpu", config);
    memory ram("ram");
    cpu.data_bus().bind(ram.socket);

    scenario checks("checks", cpu);

    sc_core::sc_start(sc_core::sc_time(2, sc_core::SC_MS));

    CHECK_MSG(checks.ran(), "the watchdog expired with checks outstanding");

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_architectural_reset: all checks passed\n";
    return 0;
}
