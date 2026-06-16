#pragma once

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>

namespace cdc::cpu {

// Static configuration of a CPU model instance.
struct cpu_config {
    unsigned xlen = 32;      // 32 or 64
    unsigned num_irq = 0;    // number of external interrupt lines (0 = none yet)
    bool has_mmu = false;
    bool has_smp = false;
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
};

} // namespace cdc::cpu
