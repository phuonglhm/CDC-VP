// SPDX-License-Identifier: Apache-2.0
//
// `cpu_base` wrapper around the RISC-V VP++ RV32 ISS — one architectural
// RV32GCV hart providing both scalar and RVV 1.0 execution.
//
// This is the TPU_V3 runtime CPU backend (decision record D3). The Bremen
// `cdc::cpu::riscv_vp` backend stays as it is for the platforms that already
// use it; it has no vector support and must not be advertised as RV32GCV.
//
// ## What is deliberately *not* used from upstream
//
// CDC-VP owns the memory map, core SRAM, interrupt controllers, devices and
// the NoC, so this wrapper embeds only the ISS and its memory interface:
//
//  * **no DMI.** `dmi_add()` is never called and `InstrMemoryProxy` is never
//    installed. Both exist upstream to bypass TLM for speed; either would let
//    fetches and loads miss core SRAM, the NoC and every counter (plan §11.2
//    integration rule, `INTERFACE_CONTRACT.md` §9).
//  * **no `dbbcache` / `lscache`.** Whether these ISS-internal caches are
//    TLM-transparent has not been measured, so they stay off until it is.
//  * **no `DirectCoreRunner`.** Upstream's runner calls `sc_stop()` when the
//    core terminates. Stopping the simulation is a platform decision, not a
//    CPU one, especially with sixteen cores in the system.
//  * **no upstream platform, GUI, Qt or VNC.** See
//    `components/TPU_V3/docs/TPU_V3_PHASE2_AUDIT.md` §4.
//
// ## Vector memory traffic is element-wise
//
// VP++ implements every vector load/store as a per-element loop, each element a
// separate access on the ISS data-memory interface. One `vle64.v` at VLEN=512
// is therefore **eight 8-byte TLM transactions**, not one 64-byte transaction;
// `vle8.v` is sixty-four single-byte transactions.
//
// That is a property of the ISS, not of this wrapper, and it happens before the
// wrapper can see it. It is the right functional behaviour — vector traffic
// traverses exactly the same fabric as scalar traffic — but any per-access
// metric downstream counts elements, not vector registers, and must say so.
// See audit finding F2.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <cdc/cpu/cpu_base.h>

// Forward declarations keep the VP++ headers (and Boost) out of the public
// interface: a platform needs `cpu_base` plus this header and nothing else.
namespace rv32 {
class ISS;
}

namespace cdc::cpu {

class riscv_vp_plusplus_cpu : public cpu_base {
public:
    /// `config.hart_id` and `config.reset_pc` are honoured, not ignored
    /// (decision record D5). An unsupported `xlen` is refused here rather than
    /// producing an RV32 core that a platform believes is RV64.
    ///
    /// `config.reset_pc` may be left unspecified, in which case the ELF entry
    /// point is used; see `load_elf`.
    explicit riscv_vp_plusplus_cpu(sc_core::sc_module_name name,
                                   const cpu_config& config = cpu_config{});
    ~riscv_vp_plusplus_cpu() override;

    /// VP++'s `CombinedMemoryInterface` carries one socket for fetch and data,
    /// so both accessors return it and `has_unified_bus()` is true.
    tlm::tlm_initiator_socket<>& instr_bus() override;
    tlm::tlm_initiator_socket<>& data_bus() override;
    bool has_unified_bus() const override { return true; }

    void set_irq(unsigned cause, bool level) override;

    /// Records the image path. The image is written through the bus and the ISS
    /// is initialised in `start_of_simulation()`, once socket binding has
    /// resolved.
    ///
    /// Reset PC precedence: an explicit `cpu_config::reset_pc` wins, because a
    /// platform that states a reset vector means it. With no explicit value the
    /// ELF entry point is used. With neither, construction already failed.
    void load_elf(const std::string& path) override;

    /// **Restart at the reset PC and reinitialise the caches. This is not a
    /// full architectural reset**, and no test or report may treat it as one.
    ///
    /// Upstream marks the ISS program counter protected — "must not modified
    /// directly (would break FastISS)" — so the only sanctioned way to place it
    /// is `ISS::init()`. That call assigns the memory interfaces, `sp`, the PC,
    /// the decode/load-store caches and the cycle baseline. It does **not**
    /// clear the general-purpose registers, the FP or vector register files,
    /// the CSRs, `vstart`, `instret`, pending interrupts, or the privilege
    /// level: a hart put through this keeps almost all of its architectural
    /// state.
    ///
    /// That is enough for "run this image again from the top" and not enough
    /// for the hierarchical platform/chip/core reset `ARCHITECTURE.md` §6
    /// requires. Two harts restarted this way would retain stale register and
    /// CSR state, and a reset test built on it would pass while proving
    /// nothing. Closing that gap is an open decision, due before Phase 5 wires
    /// core reset — see `TPU_V3_PHASE2_AUDIT.md` F8.
    void reset_cpu() override;

    std::uint64_t get_pc() const override;
    std::string backend_name() const override;
    std::uint64_t get_instret() const override;

    // ── RVV configuration, readable for tests and for platform reporting ─────
    //
    // VP++ fixes these at compile time (`vp/src/core/common/v.h`), so they are
    // reported rather than configured, and the constructor refuses a
    // configuration whose frozen RVV parameters they cannot satisfy.

    /// VLEN in bits. 512 for TPU_V3.
    static unsigned vlen_bits() noexcept;
    /// ELEN in bits. 64 for TPU_V3.
    static unsigned elen_bits() noexcept;
    /// Architectural vector register count. 32.
    static unsigned vector_register_count() noexcept;

    /// `vlenb` as the hart reports it through the CSR — the value firmware
    /// reads, not a constant recomputed here.
    std::uint32_t vlenb() const;
    /// True when `misa.V` is set on this hart.
    bool vector_extension_enabled() const;
    /// Raw `misa`, for tests that assert the whole extension set.
    std::uint64_t misa() const;

    /// True once `start_of_simulation()` has initialised the ISS.
    bool initialised() const noexcept { return initialised_; }

    // ── architectural state, read-only ───────────────────────────────────────
    //
    // Introspection for tests and platform reporting. Read-only on purpose:
    // these exist so the D19 reset contract can be *checked*, and a mutator
    // set would let a platform put a hart into a state no instruction could
    // produce.
    //
    // They read the storage firmware reads, not a second debug path. That is
    // the D11 reasoning applied locally: comparing through a private interface
    // would make a difference in the interface indistinguishable from a
    // difference in the model.

    /// Every CSR address that has backing storage, ascending.
    ///
    /// This is what makes "the reset gate cannot silently miss a CSR" true for
    /// the registers that *have* state. Derived views — `sstatus`, `sie`,
    /// `sip`, `fflags`, `frm` — are computed from the registers listed here
    /// and have no storage of their own.
    std::vector<unsigned> mapped_csr_addresses() const;

    /// Raw backing storage of one mapped CSR. Not `get_csr_value()`: this is
    /// the state reset acts on, without the read-time recomputation that
    /// `mcycle` and `time` layer on top.
    std::uint32_t read_csr_storage(unsigned address) const;

    /// One integer register, `0..31`.
    std::uint32_t read_gpr(unsigned index) const;

    /// One byte of one vector register.
    std::uint8_t read_vector_byte(unsigned reg, unsigned byte) const;

    /// `mcycle` as firmware would read it, recomputation included.
    std::uint64_t read_mcycle() const;

    /// True while this hart holds the bus lock used by `lr`/`sc` and AMO.
    bool holds_bus_lock() const;

private:
    void start_of_simulation() override;

    /// Binds the memory interfaces and places the PC through the ISS's own
    /// `init()`. Shared by `start_of_simulation()` and `reset_cpu()`; see the
    /// latter for what `init()` does and does not clear.
    void initialise_iss();

    /// The four state classes of decision record D19: identity and
    /// configuration preserved, specification-defined fields set, `sp` left to
    /// `init()`, and the remainder zeroed for reproducibility. Also releases
    /// the LR/SC reservation and bus lock and flushes the MMU TLB.
    void reset_architectural_state();

    struct impl;  // owns the ISS, MMU, memory interface, bus lock and runner
    std::unique_ptr<impl> impl_;
    std::string elf_path_;
    std::uint64_t entry_pc_ = 0;
    bool initialised_ = false;
};

}  // namespace cdc::cpu
