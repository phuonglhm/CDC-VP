// SPDX-License-Identifier: Apache-2.0

#include "riscv_vp_plusplus_wrapper.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <cdc/cpu/elf_loader.h>

#include "core/common/bus_lock_if.h"
#include "core/common/clint_if.h"
#include "core/rv32/iss.h"
#include "core/rv32/mem.h"

namespace cdc::cpu {

/// The LR/SC and AMO bus lock, shared by every hart that shares an address
/// space.
///
/// Upstream's `BusLock` lives in `vp/src/platform/common/bus.h`, which CDC-VP
/// does not build. The interface is five methods, so owning it is cheaper than
/// depending on the platform layer — and it has to be owned anyway, because the
/// lock is what makes an AMO atomic **between** harts, and a per-hart lock
/// cannot exclude anybody.
///
/// The type is opaque to callers on purpose: it derives from a VP++ interface,
/// and this backend's public header deliberately exposes no VP++ type. A
/// platform creates one with `make_shared_bus_lock()` and hands the same
/// `shared_ptr` to every hart on the chip.
///
/// ## Why the waiting loop re-checks
///
/// `released` wakes **every** waiter in the same delta. The loop is what makes
/// that safe: the first waiter SystemC resumes takes the lock and runs on
/// without yielding, so when the next one resumes inside `wait()` the
/// re-evaluated condition sees the lock held again and waits once more. A
/// straight-line "wait once, then take it" would hand the same lock to two
/// harts.
struct shared_bus_lock : bus_lock_if {
    bool locked = false;
    unsigned owner = 0;
    sc_core::sc_event released;

    /// Harts attached to this instance. One for the per-CPU default; two for a
    /// TPU chip. A test that means to prove chip-wide atomicity can assert it
    /// is testing one lock rather than two that never met.
    unsigned sharers = 0;

    /// Times the lock was taken, and times a hart had to wait for it.
    ///
    /// `contentions` is the counter that makes an atomicity result mean
    /// something: a contention test reporting zero here proves the harts never
    /// overlapped, whatever its final value came out to be.
    ///
    /// It is incremented in `wait_until_unlocked()` rather than in `lock()`,
    /// and that is where the waiting actually happens. Upstream's `mem.h` calls
    /// `wait_for_access_rights()` on **every** load, store and instruction
    /// fetch, so a hart that executes no atomic instruction at all still blocks
    /// here while another hart holds the lock. Measured, not assumed: counting
    /// inside `lock()` reported zero contention for a two-hart AMO run that was
    /// demonstrably serialised, because the second hart never got as far as its
    /// own `amoadd.w` -- it was already parked on its next instruction fetch.
    std::uint64_t acquisitions = 0;
    std::uint64_t contentions = 0;

    void lock(unsigned hart) override
    {
        if (locked && hart != owner) {
            wait_until_unlocked();
        }
        locked = true;
        owner = hart;
        ++acquisitions;
    }

    void unlock(unsigned hart) override
    {
        if (locked && owner == hart) {
            locked = false;
            released.notify(sc_core::SC_ZERO_TIME);
        }
    }

    bool is_locked() override { return locked; }
    bool is_locked(unsigned hart) override { return locked && owner == hart; }

    void wait_until_unlocked() override
    {
        if (locked) {
            ++contentions;
        }
        while (locked) {
            sc_core::wait(released);
        }
    }
};

std::shared_ptr<shared_bus_lock> make_shared_bus_lock()
{
    return std::make_shared<shared_bus_lock>();
}

namespace {

// Fallback stack top, used only when no platform value is available. The
// bare-metal startup sets `sp` itself, so this is a bring-up convenience and
// not an architectural value.
constexpr std::uint32_t kFallbackStackTop = 0x8001'0000u;

/// Minimal `clint_if`: reports simulation time as `mtime`.
///
/// A real CLINT arrives with the TPU core (Phase 5). Reporting simulated time
/// keeps `rdtime`-style reads monotonic in the meantime, which is what
/// bring-up firmware needs, and it is honest: no timer interrupt is generated
/// from here.
struct simulation_time_clint : clint_if {
    std::uint64_t update_and_get_mtime() override
    {
        return static_cast<std::uint64_t>(sc_core::sc_time_stamp().value());
    }
};

/// Runs `iss.run()` in its own SC_THREAD.
///
/// Deliberately not upstream's `DirectCoreRunner`, which calls `sc_stop()` when
/// its core terminates. With sixteen cores that would let the first one to
/// finish end everyone else's simulation. Termination is reported through the
/// ISS status and left for the platform to act on.
struct core_runner : sc_core::sc_module {
    SC_HAS_PROCESS(core_runner);

    rv32::ISS& core;

    core_runner(sc_core::sc_module_name name, rv32::ISS& core)
        : sc_core::sc_module(name)
        , core(core)
    {
        SC_THREAD(run);
    }

    void run()
    {
        // `CDC_VPP_TRACE` mirrors the Bremen backend's `CDC_ISS_TRACE`. It is
        // worth keeping: when a core retires zero instructions, the first thing
        // to establish is whether this thread ran at all, and the difference
        // between "no runner" and "runner that immediately terminated" is not
        // visible from the outside.
        const bool trace = std::getenv("CDC_VPP_TRACE") != nullptr;
        if (trace) {
            std::cerr << "[vpp] runner entered for hart " << core.get_hart_id()
                      << '\n';
            core.enable_trace(true);
        }

        core.run();

        if (trace) {
            std::cerr << "[vpp] runner left for hart " << core.get_hart_id()
                      << ", status "
                      << static_cast<int>(core.get_status()) << '\n';
        }
    }
};

}  // namespace

/// The VP++ ISS cycle time, fixed in its constructor.
constexpr int kIssCycleTimeNs = 10;

/// Establishes a valid TLM global quantum before an ISS is constructed.
///
/// The ISS constructor reads the global quantum and enforces
/// `quantum >= cycle_time` and `quantum % cycle_time == 0` with `assert`. In a
/// Release build those asserts vanish, and the result is not a diagnostic but a
/// **silent refusal to execute**: the fast-path granularity is computed as
/// `quantum / cycle_time / 10`, which is zero when the quantum is zero, and the
/// core retires no instructions at all. That failure looks exactly like a
/// broken image or a dead runner thread, so it is worth an explicit check here.
struct quantum_guard {
    quantum_guard()
    {
        auto& quantum = tlm::tlm_global_quantum::instance();
        const sc_core::sc_time cycle(kIssCycleTimeNs, sc_core::SC_NS);

        if (quantum.get() == sc_core::SC_ZERO_TIME) {
            // Nobody has chosen one. 1 us is 100 cycles: large enough that
            // temporal decoupling is worth having, small enough to keep
            // multi-core interleaving reasonable.
            quantum.set(sc_core::sc_time(1, sc_core::SC_US));
            return;
        }

        // A platform that set its own quantum keeps it, but an unusable value
        // is refused rather than silently producing a core that never runs.
        if (quantum.get() < cycle) {
            std::ostringstream message;
            message << "riscv_vp_plusplus_cpu: the TLM global quantum is "
                    << quantum.get() << ", smaller than the ISS cycle time of "
                    << cycle << ". The ISS would retire no instructions.";
            throw std::runtime_error(message.str());
        }
        if (quantum.get() % cycle != sc_core::SC_ZERO_TIME) {
            std::ostringstream message;
            message << "riscv_vp_plusplus_cpu: the TLM global quantum "
                    << quantum.get() << " is not a multiple of the ISS cycle "
                    << "time " << cycle << ". Upstream asserts this, and the "
                    << "assert is compiled out in Release.";
            throw std::runtime_error(message.str());
        }
    }
};

struct riscv_vp_plusplus_cpu::impl {
    // Declared first so it runs before `iss`: member initialisation order is
    // what makes the guard effective.
    quantum_guard quantum_guard_;

    // `RV_ISA_Config`'s default is IMACFDV + NUS, so `misa.V` is set without a
    // configuration change. It is held per instance rather than shared: the ISS
    // keeps the pointer.
    RV_ISA_Config isa_config;
    rv32::ISS iss;
    rv32::MMU mmu;
    rv32::CombinedMemoryInterface mem_if;
    std::shared_ptr<shared_bus_lock> bus_lock = make_shared_bus_lock();
    simulation_time_clint clint;
    std::unique_ptr<core_runner> runner;

    impl(std::uint32_t hart_id, const std::string& instance_name)
        : isa_config()
        , iss(&isa_config, hart_id)
        , mmu(iss)
        , mem_if((instance_name + "_mem").c_str(), iss, &mmu)
    {
        iss.systemc_name = instance_name;
        mem_if.bus_lock = bus_lock;
        bus_lock->sharers = 1;
        runner = std::make_unique<core_runner>(
            sc_core::sc_module_name((instance_name + "_runner").c_str()), iss);
    }
};

riscv_vp_plusplus_cpu::riscv_vp_plusplus_cpu(sc_core::sc_module_name name,
                                             const cpu_config& config)
    : cpu_base(name, config)
{
    // Refuse rather than adapt (decision record D5, plan §19). The ISS is an
    // RV32 type chosen at compile time, so an RV64 request cannot be honoured
    // by this class and must not silently produce an RV32 core.
    if (config.xlen != 32) {
        throw std::invalid_argument(
            "riscv_vp_plusplus_cpu: cpu_config.xlen = "
            + std::to_string(config.xlen)
            + " is not supported; this backend wraps the VP++ RV32 ISS. Use 32.");
    }

    // VLEN/ELEN are compile-time constants in VP++ (`v.h`). They happen to be
    // exactly the frozen TPU_V3 values, but assert it here so a future upstream
    // bump cannot silently change the architecture underneath us.
    if (vlen_bits() != 512 || elen_bits() != 64
        || vector_register_count() != 32) {
        std::ostringstream message;
        message << "riscv_vp_plusplus_cpu: the linked VP++ build reports VLEN="
                << vlen_bits() << ", ELEN=" << elen_bits() << ", "
                << vector_register_count()
                << " vector registers, but TPU_V3 requires VLEN=512, ELEN=64 and "
                   "32 registers (plan §4.2). These are compile-time constants "
                   "in vp/src/core/common/v.h.";
        throw std::runtime_error(message.str());
    }

    impl_ = std::make_unique<impl>(config.hart_id, std::string(name));
}

riscv_vp_plusplus_cpu::~riscv_vp_plusplus_cpu() = default;

tlm::tlm_initiator_socket<>& riscv_vp_plusplus_cpu::instr_bus()
{
    return impl_->mem_if.isock;
}

tlm::tlm_initiator_socket<>& riscv_vp_plusplus_cpu::data_bus()
{
    return impl_->mem_if.isock;
}

void riscv_vp_plusplus_cpu::set_irq(unsigned cause, bool level)
{
    // The upstream software/timer entry points take no level argument, so the
    // level is expressed by choosing trigger or clear. Doing it here keeps
    // `cpu_base`'s level-triggered contract intact for the platform.
    switch (cause) {
    case 3:  // machine software interrupt (MSIP)
        if (level) {
            impl_->iss.trigger_software_interrupt();
        } else {
            impl_->iss.clear_software_interrupt();
        }
        break;
    case 7:  // machine timer interrupt (MTIP)
        if (level) {
            impl_->iss.trigger_timer_interrupt();
        } else {
            impl_->iss.clear_timer_interrupt();
        }
        break;
    case 11:  // machine external interrupt (MEIP)
        if (level) {
            impl_->iss.trigger_external_interrupt(MachineMode);
        } else {
            impl_->iss.clear_external_interrupt(MachineMode);
        }
        break;
    default:
        // Unknown causes are dropped rather than mapped to something plausible:
        // silently redirecting an interrupt is worse than not delivering it.
        break;
    }
}

void riscv_vp_plusplus_cpu::load_elf(const std::string& path)
{
    elf_path_ = path;
}

void riscv_vp_plusplus_cpu::start_of_simulation()
{
    // Runs after binding resolves, so the image can be written through the bus
    // the platform actually connected.
    if (!elf_path_.empty()) {
        entry_pc_ = cdc::cpu::load_elf(impl_->mem_if.isock, elf_path_);
    }

    // An explicit reset vector wins: a platform that states one means it. With
    // no explicit value the ELF entry point is used.
    if (cfg.reset_pc_specified()) {
        entry_pc_ = cfg.reset_pc;
    } else if (elf_path_.empty()) {
        throw std::runtime_error(
            "riscv_vp_plusplus_cpu: neither cpu_config.reset_pc nor an ELF image "
            "was provided, so there is no reset vector. Set one of them.");
    }

    if (entry_pc_ > 0xFFFFFFFFull) {
        std::ostringstream message;
        message << "riscv_vp_plusplus_cpu: reset PC 0x" << std::hex << entry_pc_
                << " does not fit in the RV32 address space.";
        throw std::runtime_error(message.str());
    }

    initialise_iss();
    initialised_ = true;
}

void riscv_vp_plusplus_cpu::initialise_iss()
{
    // Both ISS-internal caches stay off (P2-5). `dbbcache` caches decoded basic
    // blocks and elides the repeated instruction fetches that go with them, so
    // with it on the TLM socket no longer sees every fetch and the "all CPU
    // traffic traverses CDC-VP TLM" rule (plan §11.2, `INTERFACE_CONTRACT.md`
    // §9) is quietly false. `lscache` has no DMI behind it here and is less
    // dangerous, but its transparency is equally unmeasured.
    //
    // These were briefly `true` while F11 was being diagnosed — the F11 defect
    // lives in `dbbcache` — and the comment above them still said `false`,
    // which is why nobody noticed. `dbbcache_and_lscache_stay_disabled` in
    // `test_riscv_vp_plusplus.cpp` now asserts the observable consequence
    // rather than trusting a comment.
    // `ISS::init()` calls `genOpMap()`, which fills `opMap[].labelPtr` and
    // **throws if an entry is already set** — "Multiple implementations for
    // operation N". The ISS constructor is what leaves those null, so the map
    // survives an `init()` and the *second* call always fails.
    //
    // That makes `reset_cpu()` a once-per-instance operation as upstream ships
    // it, which is a stronger limitation than audit F8 recorded: F8 established
    // that `init()` is a restart rather than an architectural reset, on the
    // assumption that it could at least be called. It could not. Phase 7 found
    // it by being the first code to reset a hart during a simulation.
    //
    // Only `labelPtr` is cleared. `opId` and `instr_time` are set in the ISS
    // constructor and `instr_time` carries the per-operation timing model
    // (memory accesses cost more than ALU operations), so zeroing the whole
    // entry would silently flatten that model on the first reset.
    for (auto& entry : impl_->iss.opMap) {
        entry.labelPtr = nullptr;
    }

    impl_->iss.init(&impl_->mem_if, /*use_dbbcache=*/false,
                    &impl_->mem_if, /*use_lscache=*/false, &impl_->clint,
                    static_cast<std::uint32_t>(entry_pc_), kFallbackStackTop);
}

std::vector<unsigned> riscv_vp_plusplus_cpu::mapped_csr_addresses() const
{
    std::vector<unsigned> addresses;
    addresses.reserve(impl_->iss.csrs.register_mapping.size());
    for (const auto& entry : impl_->iss.csrs.register_mapping) {
        addresses.push_back(entry.first);
    }
    std::sort(addresses.begin(), addresses.end());
    return addresses;
}

std::uint32_t riscv_vp_plusplus_cpu::read_csr_storage(unsigned address) const
{
    const auto it = impl_->iss.csrs.register_mapping.find(address);
    if (it == impl_->iss.csrs.register_mapping.end()) {
        throw std::out_of_range("riscv_vp_plusplus_cpu: CSR address "
                                + std::to_string(address)
                                + " has no backing storage");
    }
    return *it->second;
}

std::uint32_t riscv_vp_plusplus_cpu::read_gpr(unsigned index) const
{
    if (index >= rv32::RegFile::NUM_REGS) {
        throw std::out_of_range("riscv_vp_plusplus_cpu: register index "
                                + std::to_string(index) + " is outside 0..31");
    }
    return static_cast<std::uint32_t>(impl_->iss.regs.regs[index]);
}

std::uint8_t riscv_vp_plusplus_cpu::read_vector_byte(unsigned reg,
                                                     unsigned byte) const
{
    if (reg >= NUM_REGS || byte >= VLENB) {
        throw std::out_of_range(
            "riscv_vp_plusplus_cpu: vector register byte out of range");
    }
    return impl_->iss.v_ext.reg_read<std::uint8_t>(reg, byte);
}

std::uint64_t riscv_vp_plusplus_cpu::read_mcycle() const
{
    return impl_->iss.get_csr_value(rv32::csr::MCYCLE_ADDR)
        | (static_cast<std::uint64_t>(
               impl_->iss.get_csr_value(rv32::csr::MCYCLEH_ADDR))
           << 32);
}

bool riscv_vp_plusplus_cpu::holds_bus_lock() const
{
    return impl_->bus_lock->is_locked(
        static_cast<unsigned>(impl_->iss.get_hart_id()));
}

void riscv_vp_plusplus_cpu::attach_bus_lock(
    std::shared_ptr<shared_bus_lock> lock)
{
    if (!lock) {
        throw std::invalid_argument(
            std::string("riscv_vp_plusplus_cpu[") + name()
            + "]::attach_bus_lock: the lock is null. Leaving the per-CPU "
              "default in place is the way to ask for an unshared lock; a null "
              "one would remove the lock every atomic instruction dereferences.");
    }
    if (initialised_) {
        // Refuse rather than adapt (D5). The ISS holds `mem_if` and every
        // atomic instruction it has already executed took the old lock;
        // swapping it mid-run would silently drop an outstanding LR/SC
        // reservation and could leave a waiter parked on an event nobody will
        // notify again.
        throw std::runtime_error(
            std::string("riscv_vp_plusplus_cpu[") + name()
            + "]::attach_bus_lock: the ISS is already running. The lock must be "
              "attached during elaboration, before start_of_simulation().");
    }

    // The hart may hold the lock it is losing -- not while idle, but a caller
    // could attach twice. Release it on the old instance so a waiter there is
    // not parked forever.
    impl_->bus_lock->unlock(static_cast<unsigned>(impl_->iss.get_hart_id()));
    if (impl_->bus_lock->sharers > 0) {
        --impl_->bus_lock->sharers;
    }

    impl_->bus_lock = std::move(lock);
    impl_->mem_if.bus_lock = impl_->bus_lock;
    ++impl_->bus_lock->sharers;
}

unsigned riscv_vp_plusplus_cpu::bus_lock_sharers() const
{
    return impl_->bus_lock->sharers;
}

std::uint64_t riscv_vp_plusplus_cpu::bus_lock_acquisitions() const
{
    return impl_->bus_lock->acquisitions;
}

std::uint64_t riscv_vp_plusplus_cpu::bus_lock_contentions() const
{
    return impl_->bus_lock->contentions;
}

void riscv_vp_plusplus_cpu::reset_architectural_state()
{
    auto& iss = impl_->iss;
    auto& csrs = iss.csrs;

    // ── Class 1: preserved ───────────────────────────────────────────────────
    //
    // Identity and configuration are properties of the instance, not of the
    // run. Read out before the sweep below and put back after it, because the
    // sweep is deliberately indiscriminate: a CSR upstream adds later should
    // be zeroed by default rather than quietly missed, so the exceptions are
    // listed here and nowhere else.
    const std::uint32_t saved_mhartid = csrs.mhartid.reg.val;
    const std::uint32_t saved_misa = csrs.misa.reg.val;
    const std::uint32_t saved_mvendorid = csrs.mvendorid.reg.val;
    const std::uint32_t saved_marchid = csrs.marchid.reg.val;
    const std::uint32_t saved_mimpid = csrs.mimpid.reg.val;
    const std::uint32_t saved_vlenb = csrs.vlenb.reg.val;

    // ── Class 4: zeroed for determinism ──────────────────────────────────────
    //
    // Driven from `csrs.register_mapping`, which enumerates every CSR that has
    // backing storage. Derived views — `sstatus`, `sie`, `sip`, `fflags`,
    // `frm` — have no entry because `get_csr_value()` computes them from
    // `mstatus`, `mip`, `mie` and `fcsr`, so they follow the registers cleared
    // here rather than needing their own pass.
    //
    // The specification does not define reset values for most of this. The
    // model zeroes it so that a run's starting state cannot depend on what ran
    // before it; firmware that comes to rely on a zeroed CSR after reset is
    // relying on this model, not on RISC-V (decision record D19).
    for (const auto& entry : csrs.register_mapping) {
        const unsigned address = entry.first;
        // `time`/`mtime` are not this model's state at all: `get_csr_value()`
        // reloads them from the CLINT, whose wrapper returns
        // `sc_time_stamp()`. Reset does not rewind simulated time, so writing
        // zero here would be erased by the next read and would only look like
        // a check that passes.
        if (address == rv32::csr::TIME_ADDR
            || address == rv32::csr::TIMEH_ADDR
            || address == rv32::csr::MTIME_ADDR
            || address == rv32::csr::MTIMEH_ADDR) {
            continue;
        }
        *entry.second = 0;
    }

    csrs.mhartid.reg.val = saved_mhartid;
    csrs.misa.reg.val = saved_misa;
    csrs.mvendorid.reg.val = saved_mvendorid;
    csrs.marchid.reg.val = saved_marchid;
    csrs.mimpid.reg.val = saved_mimpid;
    csrs.vlenb.reg.val = saved_vlenb;

    // ── Class 2: what the specification asks for ─────────────────────────────
    //
    // `mstatus` is now zero, which is `MIE = 0`, `MPRV = 0` and `VS = Off` —
    // the state the ISS constructor produces, so a reset hart needs its vector
    // unit enabled again exactly as a freshly powered one does.
    iss.prv = MachineMode;

    // `mcause` = 0. The privileged specification wants an indication of the
    // reset cause and leaves the encoding implementation-defined; `reset_cpu()`
    // carries no argument that could distinguish one cause from another, so
    // zero is chosen and written down rather than invented.
    csrs.mcause.reg.val = 0;

    // RVV 1.0 recommends `vill` set with the rest of `vtype` zero, and `vl`
    // zero. The model follows the recommendation because the alternative is a
    // hart advertising SEW=8/LMUL=1 — a *valid* configuration — that no
    // firmware asked for, so a missing `vsetvli` would run instead of trapping.
    //
    // Written from the field rather than as a literal. Upstream's own default
    // is `0x8000000`, one hex digit short of bit 31, so it lands in `reserved`
    // and `fields.vill` reads 0; taking the constant from there would inherit
    // the defect.
    csrs.vtype.reg.val = 0;
    csrs.vtype.reg.fields.vill = 1;
    csrs.vl.reg.val = 0;

    // ── Class 4, the parts with no CSR address ───────────────────────────────
    //
    // `RegFile_T::reset_zero()` is *not* this: its whole body is
    // `regs[zero] = 0`, which re-zeroes `x0`. `sp` is left to `ISS::init()`,
    // which writes the stack top after this function returns (Class 3).
    for (unsigned i = 0; i < rv32::RegFile::NUM_REGS; ++i) {
        iss.regs.regs[i] = 0;
    }
    iss.fp_regs = FpRegs{};

    // The vector register file is private and has no bulk clear, but
    // `reg_write` is public. 32 registers of VLENB bytes is 2 KiB of
    // byte-at-a-time writes, which costs nothing at reset and needs no
    // upstream patch.
    for (unsigned reg = 0; reg < NUM_REGS; ++reg) {
        for (unsigned byte = 0; byte < VLENB; ++byte) {
            iss.v_ext.reg_write<std::uint8_t>(reg, byte, 0);
        }
    }

    // ── State that is neither register nor CSR ───────────────────────────────

    // Clears `lr_sc_counter` and calls `mem->atomic_unlock()`, so one call
    // covers the reservation and the bus lock. A reset between `LR.W` and
    // `SC.W` would otherwise leave both behind — harmless while the lock is
    // per-wrapper, and a hang the moment multi-hart atomicity makes it shared.
    iss.release_lr_sc_reservation();

    // `ISS::init()` reinitialises the decode and load/store caches and never
    // touches the TLB. Unreachable at `satp.MODE = Bare`, which is where
    // TPU_V3 runs, and one `memset` — the same posture D13 took with its own
    // unreachable path.
    impl_->mmu.flush_tlb();

    // The architectural cycle count. `csrs.cycle` is not the state: `MCYCLE`
    // is recomputed on every read from `_compute_and_get_current_cycles()`,
    // making the CSR field a cache of this.
    iss.cycle_counter = sc_core::SC_ZERO_TIME;
}

void riscv_vp_plusplus_cpu::reset_cpu()
{
    if (!initialised_) {
        throw std::runtime_error(
            "riscv_vp_plusplus_cpu: reset_cpu() before start_of_simulation(); "
            "there is no reset vector yet.");
    }

    // A hart that has executed `sys_exit` cannot be revived, and reset says so
    // instead of pretending (decision record D19). `shall_exit` is protected,
    // `exec_steps()` turns any `Runnable` status straight back to `Terminated`
    // while it is set, `rv32::ISS` is `final` in this build, and the runner's
    // `SC_THREAD` has already returned. `set_status(Runnable)` is therefore
    // necessary and not sufficient, and a reset that appeared to succeed would
    // hand the platform a hart that silently never executes again.
    if (impl_->iss.get_status() == CoreExecStatus::Terminated) {
        throw std::runtime_error(
            "riscv_vp_plusplus_cpu[" + std::string(name())
            + "]: reset_cpu() on a hart that has terminated. Revision 1 resets "
              "a running or trapped hart; reviving a terminated one needs "
              "either an upstream patch exposing `shall_exit` or a runner that "
              "loops on a restart event (decision record D19).");
    }

    reset_architectural_state();

    // Last, because it writes Class 3 (`sp`), places the PC and reinitialises
    // the caches and the cycle baseline. Running it after the sweep keeps the
    // F11 baseline consistent with the counters just cleared.
    initialise_iss();

    // A hart parked in `wfi` is blocked in `sc_core::wait(wfi_event)` inside
    // the call stack of `core.run()`; nothing above ends that wait. This
    // notifies the event and forces the slow path **without** injecting an
    // interrupt, so reset wakes the hart rather than fabricating a completion
    // firmware never received.
    impl_->iss.maybe_interrupt_pending();
}

std::uint64_t riscv_vp_plusplus_cpu::get_pc() const
{
    // Upstream spelling, typo included.
    return impl_->iss.get_progam_counter();
}

std::string riscv_vp_plusplus_cpu::backend_name() const
{
    return "riscv_vp_plusplus (VP++ rv32gcv, VLEN=512)";
}

std::uint64_t riscv_vp_plusplus_cpu::get_instret() const
{
    return impl_->iss.csrs.instret.reg.val;
}

unsigned riscv_vp_plusplus_cpu::vlen_bits() noexcept
{
    return VLEN;
}

unsigned riscv_vp_plusplus_cpu::elen_bits() noexcept
{
    return ELEN;
}

unsigned riscv_vp_plusplus_cpu::vector_register_count() noexcept
{
    return NUM_REGS;
}

std::uint32_t riscv_vp_plusplus_cpu::vlenb() const
{
    // Read from the CSR the hart exposes, not recomputed from VLEN: the point
    // is to check what firmware would see.
    return static_cast<std::uint32_t>(impl_->iss.csrs.vlenb.reg.val);
}

bool riscv_vp_plusplus_cpu::vector_extension_enabled() const
{
    return ((impl_->iss.csrs.misa.reg.val >> ('v' - 'a')) & 1u) != 0u;
}

std::uint64_t riscv_vp_plusplus_cpu::misa() const
{
    return impl_->iss.csrs.misa.reg.val;
}

}  // namespace cdc::cpu
