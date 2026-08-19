// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/core/tpu_core.h"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cdc::components::tpu_v3::core {

namespace {

/// Machine external interrupt. `ARCHITECTURE.md` §5: every engine's level line
/// is aggregated in the core and reaches the hart as cause 11 through
/// `cpu_base::set_irq`. Named here rather than written as a bare 11 at the two
/// call sites, and not taken from the CPU backend, because it is the
/// architecture's choice of line rather than that backend's encoding.
constexpr unsigned kMachineExternalInterrupt = 11;

sram::core_sram_config make_sram_config(const tpu_core_config& config)
{
    sram::core_sram_config out;
    out.base_address = address_map::core_sram_base(config.chip, config.core);
    out.window_bytes = address_map::core_sram_window;
    out.capacity_bytes = config.sram_capacity_bytes;
    return out;
}

std::vector<control_target_spec> make_control_targets(
    const tpu_core_config& config)
{
    // Order matters only in that `to[i]` binds the target named at `i`. The
    // addresses come from `address_map.h`; no component in TPU_V3 carries a
    // second copy of a base address.
    return {
        {address_map::core_control(config.chip, config.core),
         address_map::core_control_size, "core"},
        {address_map::sa_control(config.chip, config.core),
         address_map::sa_control_size, "sa"},
        {address_map::dma_control(config.chip, config.core),
         address_map::dma_control_size, "dma"},
        {address_map::transform_control(config.chip, config.core),
         address_map::transform_control_size, "transform"},
        {address_map::core_counters(config.chip, config.core),
         address_map::core_counters_size, "counters"},
    };
}

core_aperture_spec make_aperture(const tpu_core_config& config)
{
    core_aperture_spec spec;
    spec.core_base = address_map::core_base(config.chip, config.core);
    spec.core_size = address_map::core_aperture_stride;
    spec.sram_base = address_map::core_sram_base(config.chip, config.core);
    spec.sram_window = address_map::core_sram_window;
    return spec;
}

hart_port_spec make_hart_port_spec(const tpu_core_config& config)
{
    const auto aperture = make_aperture(config);
    hart_port_spec spec;
    spec.core_base = aperture.core_base;
    spec.core_size = aperture.core_size;
    spec.sram_base = aperture.sram_base;
    spec.sram_window = aperture.sram_window;
    return spec;
}

dma::neo_dma_config make_dma_config(const tpu_core_config& config)
{
    dma::neo_dma_config out;
    out.control_base = address_map::dma_control(config.chip, config.core);
    out.control_size = address_map::dma_control_size;
    out.sram_base = address_map::core_sram_base(config.chip, config.core);
    out.sram_window = address_map::core_sram_window;
    out.max_burst_bytes = config.dma_max_burst_bytes;
    return out;
}

sauria::adapter_config make_matrix_config(const tpu_core_config& config)
{
    sauria::adapter_config out;
    out.sram_base = address_map::core_sram_base(config.chip, config.core);
    out.sram_window = address_map::core_sram_window;
    out.cycle = config.cycle;
    return out;
}

sauria::sa_control_config make_sa_control_config(const tpu_core_config& config)
{
    sauria::sa_control_config out;
    out.control_base = address_map::sa_control(config.chip, config.core);
    out.control_size = address_map::sa_control_size;
    return out;
}

transform::image_transform_config make_transform_config(
    const tpu_core_config& config)
{
    transform::image_transform_config out;
    out.control_base = address_map::transform_control(config.chip, config.core);
    out.control_size = address_map::transform_control_size;
    out.sram_base = address_map::core_sram_base(config.chip, config.core);
    out.sram_window = address_map::core_sram_window;
    return out;
}

cdc::cpu::cpu_config make_cpu_config(const tpu_core_config& config)
{
    cdc::cpu::cpu_config out;
    out.xlen = 32;
    // D5: static construction-time properties, not runtime setters. A backend
    // that cannot honour them refuses to construct rather than reporting
    // `mhartid = 0` from every hart in a 16-hart platform.
    out.hart_id = static_cast<std::uint32_t>(config.chip) * 2u
        + static_cast<std::uint32_t>(config.core);
    out.reset_pc = config.reset_pc;
    return out;
}

} // namespace

void tpu_core_config::validate(const std::string& context) const
{
    if (!address_map::valid_chip(chip)) {
        throw std::invalid_argument(
            context + ": chip " + std::to_string(chip)
            + " is outside 0.." + std::to_string(max_chips - 1)
            + ". The limit is the 3-bit FlooNoC manager id, not a layout "
              "preference (decision record D2)");
    }
    if (!address_map::valid_core(core)) {
        throw std::invalid_argument(
            context + ": core " + std::to_string(core)
            + " is not 0 or 1; plan §4.1 freezes two cores per chip");
    }
    if (sram_capacity_bytes < address_map::core_sram_min_capacity
        || sram_capacity_bytes > address_map::core_sram_max_capacity) {
        throw std::invalid_argument(
            context + ": core SRAM capacity "
            + std::to_string(sram_capacity_bytes) + " is outside "
            + std::to_string(address_map::core_sram_min_capacity) + ".."
            + std::to_string(address_map::core_sram_max_capacity));
    }
    if (cycle <= sc_core::SC_ZERO_TIME) {
        throw std::invalid_argument(
            context + ": the core clock period must be positive; a zero period "
                      "would make every engine that counts clock edges run "
                      "forever inside one delta");
    }
    if (dma_max_burst_bytes == 0) {
        throw std::invalid_argument(context
                                    + ": dma_max_burst_bytes must be non-zero");
    }
    // The fabric's own validation covers the physical values, and it is the
    // authority on them; calling it here means a bad core configuration fails
    // in one place with one message.
    fabric.validate(context + ".fabric");
}

tpu_core::tpu_core(sc_core::sc_module_name name, tpu_core_config config)
    : sc_core::sc_module(name)
    , config_((config.validate(std::string("tpu_v3::tpu_core[")
                               + std::string(name) + "]"),
               std::move(config)))
    , clock_("clock", config_.cycle)
    , reset_n_("reset_n")
    , sram_("core_sram", make_sram_config(config_))
    , fabric_("local_fabric", config_.fabric, sram_,
              // The closed requester list. Every one of these is a port that
              // exists on this instance; an access from anything else is an
              // integration defect and the fabric throws rather than counting
              // it as somebody.
              {sram::neo_requester::cpu, sram::neo_requester::dma,
               sram::neo_requester::sa, sram::neo_requester::transform,
               sram::neo_requester::external_inbound},
              config_.timing, config_.cycle)
    , control_("control_fabric", make_control_targets(config_))
    , core_regs_("core_regs", register_block::core,
                 address_map::core_control(config_.chip, config_.core),
                 address_map::core_control_size)
    , counter_regs_("counter_regs", register_block::counters,
                    address_map::core_counters(config_.chip, config_.core),
                    address_map::core_counters_size)
    , bridge_("external_bridge", make_aperture(config_), fabric_)
    , hart_port_("hart_port", make_hart_port_spec(config_), fabric_)
    , dma_("dma", make_dma_config(config_))
    , matrix_("matrix_engine", make_matrix_config(config_))
    , sa_control_("sa_control", make_sa_control_config(config_), matrix_)
    , transform_("transform", make_transform_config(config_))
    , cpu_("cpu", make_cpu_config(config_))
{
    // ── the hart's three legs ────────────────────────────────────────────────
    //
    // One socket in, three planes out. VP++ has a combined instruction/data
    // interface, so this single bind carries fetch, scalar data, vector data
    // and MMIO alike.
    cpu_.data_bus().bind(hart_port_.from_hart);
    hart_port_.to_control.bind(
        control_.from[static_cast<unsigned>(control_initiator::cpu)]);
    hart_port_.to_external.bind(
        bridge_.local_outbound[static_cast<unsigned>(outbound_initiator::cpu)]);

    // ── the control plane ────────────────────────────────────────────────────
    //
    // Two register files this core owns, and three that belong to engines.
    // The engines' own targets are bound here rather than shadowed by a
    // register file of ours: a second copy of an engine's registers would
    // answer reads the engine never saw.
    control_.to[0].bind(core_regs_.socket);
    control_.to[1].bind(sa_control_.control);
    control_.to[2].bind(dma_.control);
    control_.to[3].bind(transform_.control);
    control_.to[4].bind(counter_regs_.socket);
    bridge_.inbound_control.bind(
        control_.from[static_cast<unsigned>(control_initiator::external_inbound)]);

    // ── the local data plane ─────────────────────────────────────────────────
    //
    // Three requesters, one export. Requester identity travels in the request,
    // so every byte any of them moves is attributable by construction.
    dma_.local.bind(fabric_.native_port);
    matrix_.local_port.bind(fabric_.native_port);
    transform_.local.bind(fabric_.native_port);

    // ── the external plane ───────────────────────────────────────────────────
    //
    // The DMA is the bulk mover and the only engine with an external port.
    // Binding it to the bridge rather than straight at a memory is what keeps
    // the bridge's containment check in the path (plan §21).
    dma_.external.bind(
        bridge_.local_outbound[static_cast<unsigned>(outbound_initiator::dma)]);

    // ── clock and reset for the parts that are clocked ───────────────────────
    matrix_.i_clk(clock_);
    matrix_.i_rstn(reset_n_);
    sa_control_.i_clk(clock_);

    // ── interrupts ───────────────────────────────────────────────────────────
    //
    // Level, aggregated here, delivered as machine external interrupt (cause
    // 11). Level rather than edge is deliberate: a completion lost during a
    // reset window is unrecoverable if it was an edge, and produces a hang
    // that looks like a modelling bug (`ARCHITECTURE.md` §5).
    dma_.irq(dma_irq_);
    sa_control_.irq(sa_irq_);
    transform_.irq(transform_irq_);

    SC_METHOD(aggregate_irq);
    sensitive << dma_irq_ << sa_irq_ << transform_irq_;
    dont_initialize();

    SC_METHOD(drive_hart_irq);
    sensitive << clock_.posedge_event();
    dont_initialize();

    reset_n_.write(true);
}

std::uint32_t tpu_core::hart_id() const noexcept
{
    return static_cast<std::uint32_t>(config_.chip) * 2u
        + static_cast<std::uint32_t>(config_.core);
}

std::uint64_t tpu_core::core_base() const noexcept
{
    return address_map::core_base(config_.chip, config_.core);
}

void tpu_core::aggregate_irq()
{
    irq_pending_
        = dma_irq_.read() || sa_irq_.read() || transform_irq_.read();
}

void tpu_core::drive_hart_irq()
{
    // Sampled on a clock edge rather than driven straight from the aggregation
    // method. `set_irq` reaches into the ISS, and the engines' signals settle
    // over deltas; edge-sampling means the hart sees one settled level per
    // cycle instead of every intermediate combination.
    cpu_.set_irq(kMachineExternalInterrupt, irq_pending_);
}

void tpu_core::reset()
{
    // Order is not arbitrary. The requesters are reset before the fabric they
    // share, so a requester cannot enqueue a fresh beat into a fabric that has
    // already been cleared; the fabric's own generation counter then abandons
    // whatever was in flight and returns `aborted` to whoever was blocked.
    dma_.reset();
    matrix_.reset();
    sa_control_.reset();
    transform_.reset();

    fabric_.reset();
    control_.reset();
    bridge_.reset();
    hart_port_.reset();
    sram_.reset();

    core_regs_.reset();
    counter_regs_.reset();

    // The D19 four-class architectural reset. It throws if this hart has
    // executed `sys_exit`, which is a case Revision 1 does not support and
    // refuses loudly rather than leaving a hart that silently never runs
    // again.
    cpu_.reset_cpu();

    irq_pending_ = false;
    cpu_.set_irq(kMachineExternalInterrupt, false);
}

std::string tpu_core::report() const
{
    std::ostringstream out;
    out << "tpu_core[" << name() << "]\n"
        << "  chip " << static_cast<unsigned>(config_.chip) << ", core "
        << static_cast<unsigned>(config_.core) << ", hart_id " << hart_id()
        << '\n'
        << "  clock " << config_.cycle << ", local fabric "
        << to_string(config_.timing);
    if (config_.timing == local_fabric_timing::arbitrated) {
        out << "  (blocks its requesters; not a full-system configuration -- "
               "D16)";
    }
    out << '\n'
        << "  MXU " << matrix_.identity().rows << 'x'
        << matrix_.identity().columns
        << " from the pinned Sauria v4.2 source; Transform exposes Im2Col "
           "only, Col2Im unavailable (D18)\n"
        << "  hart reset implements the D19 four-class architectural contract; "
           "a hart idling in `wfi` keeps its reset state but does not restart\n";
    return out.str();
}

} // namespace cdc::components::tpu_v3::core
