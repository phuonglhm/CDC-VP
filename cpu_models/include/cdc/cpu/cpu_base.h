#pragma once

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>

namespace cdc::cpu {

// Static configuration of a CPU model instance.
//
// `hart_id` and `reset_pc` are *static properties*, set here at construction,
// not runtime setters (TPU_V3 decision record D5). A defaulted virtual setter
// that a backend silently ignores is worse than a missing one: a 16-hart
// platform would elaborate cleanly while every core still reported
// `mhartid == 0`, and nothing would fail until firmware tried to tell the cores
// apart.
//
// A backend that cannot honour a requested value must **throw from its
// constructor**, naming the field and what it does support. Ignoring it is
// forbidden.
struct cpu_config {
    unsigned xlen = 32;      // 32 or 64
    unsigned num_irq = 0;    // number of external interrupt lines (0 = none yet)
    bool has_mmu = false;
    bool has_smp = false;

    // Architectural hart id, visible to firmware through `mhartid`. Must be
    // unique across the platform.
    std::uint32_t hart_id = 0;

    // "The platform has no opinion; use whatever reset vector this backend
    // implements."
    //
    // A sentinel is needed rather than D5's literal `reset_pc = 0` because 0 is
    // a *legitimate* reset vector: the TPU_V3 map puts GLOBAL_BOOT_ROM at
    // 0x0000_0000, so `reset_pc == 0` must mean "reset at address zero" and
    // cannot also mean "unset". Existing single-hart platforms that never set
    // the field keep their backend's own default.
    static constexpr std::uint64_t reset_pc_unspecified =
        ~static_cast<std::uint64_t>(0);

    // PC after reset. Firmware-visible, so a backend must either reset there or
    // refuse to construct.
    std::uint64_t reset_pc = reset_pc_unspecified;

    bool reset_pc_specified() const noexcept
    {
        return reset_pc != reset_pc_unspecified;
    }
};

// Backend-agnostic CPU interface. Platforms instantiate a concrete CPU via this
// interface (selected by CMake flag CDC_CPU_BACKEND) and bind its two outgoing
// bus sockets to the bus_router, so platform code never depends on a specific
// CPU model implementation.
//
// The bus sockets are exposed as base tlm_initiator_socket<> references: every
// simple_initiator_socket<T> derives from it, so a concrete wrapper can return
// the underlying model's socket regardless of its owner type.
//
// NOTE (M1 scope): interrupts, quantum-keeper control, cycle counters and halt
// are intentionally omitted for the first bare-metal-hello milestone and will be
// added when needed (Gate 2 / benchmarking).
class cpu_base : public sc_core::sc_module {
public:
    cpu_config cfg;

    cpu_base(sc_core::sc_module_name name, const cpu_config& config)
        : sc_core::sc_module(name)
        , cfg(config)
    {
    }

    ~cpu_base() override = default;

    // Outgoing bus sockets. Bind these to bus_router target sockets.
    virtual tlm::tlm_initiator_socket<>& instr_bus() = 0;
    virtual tlm::tlm_initiator_socket<>& data_bus() = 0;

    // True if instr_bus() and data_bus() are the SAME socket (one combined bus,
    // e.g. Bremen). The platform then binds a single upstream port instead of two.
    virtual bool has_unified_bus() const { return false; }

    // Inject an external interrupt with a RISC-V cause code (default: no-op for
    // backends that don't wire interrupts yet).
    virtual void raise_irq(std::uint32_t /*cause*/) {}

    // Level-triggered interrupt input from an interrupt controller (CLINT/PLIC).
    // cause: 3 = software (MSIP), 7 = timer (MTIP), 11 = external (MEIP).
    // level: true = assert, false = deassert. Default: no-op.
    virtual void set_irq(unsigned /*cause*/, bool /*level*/) {}

    // Load an ELF image into memory (via the data bus, backdoor) and set the
    // reset PC to the ELF entry point. Throws std::runtime_error on failure.
    virtual void load_elf(const std::string& path) = 0;

    virtual void reset_cpu() = 0;
    virtual std::uint64_t get_pc() const = 0;
    virtual std::string backend_name() const = 0;

    // Retired instruction count (for benchmarking). 0 if the backend can't report it.
    virtual std::uint64_t get_instret() const { return 0; }

    // Read-only identity accessors (decision record D5). Non-virtual on
    // purpose: they report the immutable configuration this instance was
    // constructed with, and a backend that could not honour it threw instead of
    // reaching here, so there is nothing for a backend to override.
    std::uint32_t hart_id() const noexcept { return cfg.hart_id; }

    // The requested reset vector, or `cpu_config::reset_pc_unspecified`.
    std::uint64_t configured_reset_pc() const noexcept { return cfg.reset_pc; }
};

} // namespace cdc::cpu
