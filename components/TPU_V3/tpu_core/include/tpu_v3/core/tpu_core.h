// SPDX-License-Identifier: Apache-2.0
//
// One NEO-CORE (plan §11.8, decision records D14/D15).
//
// This is the composition Phases 2 through 6 were building towards. Every part
// of it already existed and was gated on its own; what this file adds is the
// wiring, and the wiring is where the interesting mistakes live — a DMA bound
// to a memory instead of the bridge, an engine whose IRQ nothing aggregates, a
// fabric in the wrong timing mode.
//
//   riscv_vp_plusplus ──> neo_hart_port ──┬──> neo_local_sram_fabric ──> core_sram
//                                         ├──> neo_control_fabric ──> registers
//                                         └──> neo_external_bridge ──> chip/NoC
//   neo_dma        ──> control + local + external
//   MXU (sauria)   ──> control + local
//   Transform      ──> control + local
//
// Three wiring rules are load-bearing and each has a reason recorded elsewhere:
//
// **The DMA's external port goes to the bridge, not to a memory** (plan §21).
// The DMA classifies core MMIO as external and relies on the bridge to refuse
// it; binding past the bridge removes that check silently.
//
// **The MXU and the Transform block reach memory only through the native local
// port** (D15). Neither is an external AXI master in Revision 1, so neither
// has a socket that could reach the chip path even by accident.
//
// **The fabric is `annotated`** unless a caller deliberately asks otherwise
// (D16). A hart behind an `arbitrated` fabric synchronises to global time on
// every local load and store, which removes temporal decoupling entirely;
// `arbitrated` belongs to the Phase 11 contention studies, and the core says
// which mode it got in its report.
//
// `reset()` drives the hierarchical component reset and the D19 architectural
// hart reset. Two limitations D19 records apply here too: a hart that has
// executed `sys_exit` cannot be revived and the call throws, and a hart idling
// in `wfi` keeps its reset state but does not restart.

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "cdc/cpu/cpu_base.h"
#include "riscv_vp_plusplus_wrapper.h"

#include "tpu_v3/address_map.h"
#include "tpu_v3/architecture_config.h"
#include "tpu_v3/core/core_registers.h"
#include "tpu_v3/core/neo_control_fabric.h"
#include "tpu_v3/core/neo_external_bridge.h"
#include "tpu_v3/core/neo_hart_port.h"
#include "tpu_v3/core/neo_local_sram_fabric.h"
#include "tpu_v3/dma/neo_dma.h"
#include "tpu_v3/sauria/sauria_geometry.h"
#include "tpu_v3/sauria/sauria_matrix_adapter.h"
#include "tpu_v3/sauria/sa_control.h"
#include "tpu_v3/sram/core_sram.h"
#include "tpu_v3/transform/image_transform.h"

namespace cdc::components::tpu_v3::core {

struct tpu_core_config {
    /// Position in the platform. These two decide every absolute address in
    /// the core and, with them, the hart id — there is no second identity
    /// field, because two sources of the same fact can disagree.
    chip_id_t chip = 0;
    core_id_t core = 0;

    /// Backed storage behind the 16 MiB window. D6's reference value is the
    /// whole window; anything smaller is a labelled bring-up configuration.
    std::uint64_t sram_capacity_bytes = address_map::core_sram_default_capacity;

    /// The physical local-plane values. No defaults anywhere in the schema:
    /// D15 leaves them open pending SRAM-macro, frequency and PD inputs, and a
    /// number nobody chose is exactly what becomes an architectural constant
    /// by accident.
    local_sram_fabric_config fabric;

    /// D16. `annotated` for any full-system run.
    local_fabric_timing timing = local_fabric_timing::annotated;

    /// The core's clock period. One domain per core (`ARCHITECTURE.md` §6);
    /// the MXU's sequencer and the SA control register file run on it.
    sc_core::sc_time cycle{10, sc_core::SC_NS};

    /// Largest external payload the DMA will issue before chunking.
    std::uint64_t dma_max_burst_bytes = 2048;

    /// Where the hart starts. `GLOBAL_BOOT_ROM`, which is outside the core, so
    /// the very first instruction fetch crosses the external bridge — that
    /// path is architecturally required and not an optimisation (D15).
    std::uint64_t reset_pc = address_map::boot_rom_base;

    /// Throws `std::invalid_argument` naming the field, the value and the
    /// accepted range. Called before any socket is bound, so a bad
    /// configuration fails during elaboration rather than on first traffic.
    void validate(const std::string& context) const;
};

/// The MXU as this core instantiates it: the pinned 64x64 INT8/INT32 profile,
/// never the 32x32 template default (plan §16 Phase 5).
using core_matrix_engine = sauria::sauria_matrix_adapter<
    sauria::columns, sauria::rows, sauria::activation_t, sauria::weight_t,
    sauria::accumulator_t, /*SRAMA_CAP=*/4096, /*SRAMB_CAP=*/4096,
    /*SRAMC_CAP=*/4096>;

class tpu_core : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(tpu_core);

    tpu_core(sc_core::sc_module_name name, tpu_core_config config);

    // ── the chip-facing boundary ─────────────────────────────────────────────
    //
    // Exactly two sockets, and that is the point: plan §4.4 says the cores
    // aggregate into one chip endpoint rather than appearing as several
    // unrelated NoC managers. A core that exposed its DMA or its hart directly
    // would make that impossible to enforce later.

    /// Outbound: this core towards the chip fabric, global memory or the mesh.
    tlm_utils::simple_initiator_socket<neo_external_bridge>& external() noexcept
    {
        return bridge_.external;
    }
    /// Inbound: the sibling core, a remote chip or the host loader.
    tlm_utils::simple_target_socket<neo_external_bridge>& inbound() noexcept
    {
        return bridge_.inbound;
    }

    // ── identity ─────────────────────────────────────────────────────────────

    /// `chip_linear_id * 2 + core_id` (`ARCHITECTURE.md` §2). Firmware reads
    /// it from `mhartid`; chip and core index are derived from it, never
    /// supplied separately.
    std::uint32_t hart_id() const noexcept;
    std::uint64_t core_base() const noexcept;

    const tpu_core_config& config() const noexcept { return config_; }

    // ── the parts, for tests and platform reporting ──────────────────────────

    cdc::cpu::riscv_vp_plusplus_cpu& cpu() noexcept { return cpu_; }
    sram::core_sram& sram() noexcept { return sram_; }
    neo_local_sram_fabric& fabric() noexcept { return fabric_; }
    neo_control_fabric& control() noexcept { return control_; }
    neo_external_bridge& bridge() noexcept { return bridge_; }
    neo_hart_port& hart_port() noexcept { return hart_port_; }
    dma::neo_dma& dma() noexcept { return dma_; }
    transform::image_transform& transform() noexcept { return transform_; }
    sauria::sa_control& matrix_control() noexcept { return sa_control_; }
    core_matrix_engine& matrix_engine() noexcept { return matrix_; }

    /// True while any engine is asserting its level interrupt. The hart sees
    /// the same thing as machine external interrupt, cause 11.
    bool irq_pending() const noexcept { return irq_pending_; }

    /// Hierarchical reset: core → component (`ARCHITECTURE.md` §6).
    ///
    /// The local fabric is reset in the same call as the engines that use it.
    /// Resetting only a requester cannot retract a beat the fabric already
    /// accepted, and a queued old beat completing after a reset would be worse
    /// than either alternative (D17).
    ///
    /// Must be called from a SystemC process: it drives interrupt signals.
    void reset();

    std::string report() const;

private:
    void aggregate_irq();
    void drive_hart_irq();

    tpu_core_config config_;

    // Declaration order is construction order, and several of these take
    // references to the ones above them.
    sc_core::sc_clock clock_;
    sc_core::sc_signal<bool> reset_n_;

    sram::core_sram sram_;
    neo_local_sram_fabric fabric_;
    neo_control_fabric control_;
    mmio_register_file core_regs_;
    mmio_register_file counter_regs_;
    neo_external_bridge bridge_;
    neo_hart_port hart_port_;

    dma::neo_dma dma_;
    core_matrix_engine matrix_;
    sauria::sa_control sa_control_;
    transform::image_transform transform_;

    cdc::cpu::riscv_vp_plusplus_cpu cpu_;

    // One signal per engine, aggregated by a single method. The engines each
    // drive their own line from one process; the OR of them is this core's
    // business, not theirs.
    sc_core::sc_signal<bool> dma_irq_{"dma_irq"};
    sc_core::sc_signal<bool> sa_irq_{"sa_irq"};
    sc_core::sc_signal<bool> transform_irq_{"transform_irq"};

    bool irq_pending_ = false;
};

} // namespace cdc::components::tpu_v3::core
