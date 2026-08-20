// SPDX-License-Identifier: Apache-2.0
//
// The AXI4-Lite register files behind one NEO-CORE's control plane.
//
// Phase 3 routes control traffic; it implements no block behaviour, and the
// gate says so in as many words: "route synthetic CPU/inbound control accesses
// to each AXI4-Lite target. No block-specific compute is required yet."
//
// So this is one generic 32-bit register file, instantiated five times — core,
// SA, DMA, Transform and counters. It exists to be a real target with real
// MMIO semantics: 4-byte accesses only, natural alignment, full strobes,
// unimplemented offsets defined rather than erroneous. Phases 4 to 6 replace
// the SA, DMA and Transform instances with the real register maps; the core
// and counter windows grow into theirs.
//
// The `implemented` bit in STATUS is the honest part. An SA, DMA or Transform
// window that answered "ready" today would be claiming a block that does not
// exist, and for Transform specifically decision record D14 requires the
// unavailable state to be *reported*, never faked into a success.

#pragma once

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components::tpu_v3::core {

/// Which register file this instance is: the five core-local ones, and the two
/// a chip owns above them (Phase 8).
///
/// The value is part of the block's identity register, so a wrong-window decode
/// is visible in a read instead of being answered plausibly by whichever file
/// happened to be bound. That is why the chip windows get their own values
/// rather than reusing `core` and `counters`: a driver that computed a core
/// base where it meant a chip base would otherwise read exactly what it
/// expected.
enum class register_block {
    core,
    sa,
    dma,
    transform,
    counters,
    /// `CHIP_CONTROL` — the chip aperture's own control window.
    chip,
    /// `CHIP_COUNTERS`.
    chip_counters,
};

const char* to_string(register_block block) noexcept;

class mmio_register_file : public sc_core::sc_module {
public:
    /// Offsets. Four registers is not a design; it is the smallest set that
    /// exercises read-only, read-write and unimplemented behaviour, which is
    /// all Phase 3 has to route.
    static constexpr std::uint64_t reg_id = 0x00;      ///< RO identity
    static constexpr std::uint64_t reg_version = 0x04; ///< RO model revision
    static constexpr std::uint64_t reg_status = 0x08;  ///< RO status bits
    static constexpr std::uint64_t reg_scratch = 0x0C; ///< RW

    /// STATUS bit 0. Zero means the block is routed but not implemented, so
    /// firmware can tell "not built yet" from "built and idle" instead of
    /// inferring it from an all-zero register file.
    static constexpr std::uint32_t status_implemented = 1u << 0;

    /// `0x54503300 | block`, i.e. "TP3" and the block code. A wrong-window
    /// decode is otherwise invisible: every register file would answer, and
    /// the driver would happily program the wrong engine.
    static constexpr std::uint32_t id_magic = 0x5450'3300u;

    static constexpr std::uint32_t model_version = 0x0003'0000u; // Phase 3

    tlm_utils::simple_target_socket<mmio_register_file> socket;

    mmio_register_file(sc_core::sc_module_name name, register_block block,
                       std::uint64_t base, std::uint64_t size);

    register_block block() const noexcept { return block_; }
    std::uint64_t base() const noexcept { return base_; }
    std::uint64_t size() const noexcept { return size_; }

    std::uint32_t identity() const noexcept
    {
        return id_magic | static_cast<std::uint32_t>(block_);
    }

    /// Whatever was last written to SCRATCH. The one piece of state, and the
    /// only way a test can prove a write reached this target rather than
    /// merely being accepted somewhere upstream.
    std::uint32_t scratch() const noexcept { return scratch_; }

    std::uint64_t reads() const noexcept { return reads_; }
    std::uint64_t writes() const noexcept { return writes_; }
    /// Reads of offsets this block does not implement. They return 0 with
    /// `TLM_OK_RESPONSE`: unimplemented is a defined state, not an error, so
    /// a firmware register sweep does not have to know the implementation
    /// status of every offset (`ADDRESS_MAP.md` §6).
    std::uint64_t unimplemented_reads() const noexcept
    {
        return unimplemented_reads_;
    }
    std::uint64_t dropped_writes() const noexcept { return dropped_writes_; }
    /// Accesses refused for violating the MMIO rules.
    std::uint64_t protocol_errors() const noexcept { return protocol_errors_; }

    void reset();

    std::string report() const;

private:
    /// STATUS as both the normal and the debug path report it. One function,
    /// so a debug read cannot come to disagree with `b_transport` about what
    /// the hardware says.
    std::uint32_t status_value() const noexcept;

    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    /// The MMIO rules, checked at the target as well as at the fabric.
    ///
    /// Duplicated on purpose: a target reached directly — by a test harness,
    /// or by a future path that does not pass the control fabric — must refuse
    /// a 64-byte accelerator payload just the same. Defence in depth is cheap
    /// here and the alternative is a rule that holds only as long as everyone
    /// remembers to route through the decoder.
    bool check_mmio_rules(tlm::tlm_generic_payload& trans);

    register_block block_;
    std::uint64_t base_;
    std::uint64_t size_;

    std::uint32_t scratch_ = 0;

    std::uint64_t reads_ = 0;
    std::uint64_t writes_ = 0;
    std::uint64_t unimplemented_reads_ = 0;
    std::uint64_t dropped_writes_ = 0;
    std::uint64_t protocol_errors_ = 0;
};

} // namespace cdc::components::tpu_v3::core
