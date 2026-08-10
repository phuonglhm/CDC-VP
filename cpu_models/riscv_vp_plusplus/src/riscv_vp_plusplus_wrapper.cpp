// SPDX-License-Identifier: Apache-2.0

#include "riscv_vp_plusplus_wrapper.h"

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

namespace {

// Fallback stack top, used only when no platform value is available. The
// bare-metal startup sets `sp` itself, so this is a bring-up convenience and
// not an architectural value.
constexpr std::uint32_t kFallbackStackTop = 0x8001'0000u;

/// Multi-hart bus lock for `lr`/`sc` and AMO.
///
/// Upstream's `BusLock` lives in `vp/src/platform/common/bus.h`, which CDC-VP
/// does not build. The interface is four methods, so owning it is cheaper than
/// depending on the platform layer — and it has to be owned anyway, because the
/// cores that share an address space must share **one** lock instance. This
/// per-CPU default is correct only for a single-hart system; TPU_V3 replaces it
/// with a chip-wide lock in Phase 6.
struct local_bus_lock : bus_lock_if {
    bool locked = false;
    unsigned owner = 0;
    sc_core::sc_event released;

    void lock(unsigned hart) override
    {
        if (locked && hart != owner) {
            wait_until_unlocked();
        }
        locked = true;
        owner = hart;
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
        while (locked) {
            sc_core::wait(released);
        }
    }
};

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
    std::shared_ptr<local_bus_lock> bus_lock = std::make_shared<local_bus_lock>();
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
    // `false, false` are `use_dbbcache` and `use_lscache`. They stay off until
    // their TLM transparency is measured; see the header.
    impl_->iss.init(&impl_->mem_if, /*use_dbbcache=*/true,
                    &impl_->mem_if, /*use_lscache=*/true, &impl_->clint,
                    static_cast<std::uint32_t>(entry_pc_), kFallbackStackTop);
}

void riscv_vp_plusplus_cpu::reset_cpu()
{
    if (!initialised_) {
        throw std::runtime_error(
            "riscv_vp_plusplus_cpu: reset_cpu() before start_of_simulation(); "
            "there is no reset vector yet.");
    }
    initialise_iss();
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
