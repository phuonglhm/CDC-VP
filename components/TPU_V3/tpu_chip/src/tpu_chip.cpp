// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/chip/tpu_chip.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace cdc::components::tpu_v3::chip {

namespace {

/// One core's configuration with its identity filled in from the chip.
///
/// The chip is the single source of both fields. `tpu_core` derives its
/// addresses and its hart id from them, so writing them here — once, in one
/// place — is what makes "there is no second identity field" true across the
/// composition rather than only inside one core.
core::tpu_core_config core_config_for(const tpu_chip_config& config,
                                      unsigned index)
{
    core::tpu_core_config out = config.core[index];
    out.chip = config.chip;
    out.core = static_cast<core_id_t>(index);
    return out;
}

} // namespace

void tpu_chip_config::validate(const std::string& context) const
{
    if (!address_map::valid_chip(chip)) {
        throw std::invalid_argument(
            context + ": chip " + std::to_string(chip) + " is outside 0.."
            + std::to_string(max_chips - 1)
            + ". The limit is the 3-bit FlooNoC manager id, not a layout "
              "preference (decision record D2)");
    }
    if (cores != cores_per_chip) {
        throw std::invalid_argument(
            context + ": cores = " + std::to_string(cores)
            + ", but plan §4.1 freezes " + std::to_string(cores_per_chip)
            + " NEO-COREs per chip. A chip with a different count is not a "
              "configuration of this model; it is a different architecture, "
              "and the address map, the hart-id mapping and the NoC initiator "
              "budget all depend on the frozen value");
    }
    if (cycle <= sc_core::SC_ZERO_TIME) {
        throw std::invalid_argument(
            context + ": the chip fabric clock period must be positive");
    }
    for (unsigned i = 0; i < cores_per_chip; ++i) {
        if (core[i].chip != 0 || core[i].core != 0) {
            throw std::invalid_argument(
                context + ": core[" + std::to_string(i)
                + "] has its own chip/core identity set. Those two fields are "
                  "assigned by the chip and must be left at zero: two sources "
                  "of the same fact can disagree, and a core that announced "
                  "the wrong index would collide with its sibling's address "
                  "aperture and its sibling's mhartid");
        }
    }
}

tpu_chip::tpu_chip(sc_core::sc_module_name name, tpu_chip_config config)
    : sc_core::sc_module(name)
    , config_((config.validate(std::string("tpu_v3::tpu_chip[")
                               + std::string(name) + "]"),
               std::move(config)))
    , fabric_("chip_fabric", chip_aperture_of(config_.chip), config_.timing,
              config_.cycle)
    , control_regs_("chip_control", core::register_block::chip,
                    address_map::chip_control(config_.chip),
                    address_map::chip_control_size)
    , counter_regs_("chip_counters", core::register_block::chip_counters,
                    address_map::chip_counters(config_.chip),
                    address_map::chip_counters_size)
    , bus_lock_(cdc::cpu::make_shared_bus_lock())
{
    for (unsigned i = 0; i < cores_per_chip; ++i) {
        cores_[i] = std::make_unique<core::tpu_core>(
            sc_core::sc_module_name(("core" + std::to_string(i)).c_str()),
            core_config_for(config_, i));
    }

    // ── the cores onto the chip fabric ───────────────────────────────────────
    //
    // Each core's one outbound port becomes one fabric initiator, and the
    // fabric answers each core on its one inbound port. A core has exactly two
    // external sockets by construction (`tpu_core.h`), which is what makes the
    // "one aggregated NoC manager per chip" rule enforceable here rather than
    // hopeful.
    for (unsigned i = 0; i < cores_per_chip; ++i) {
        cores_[i]->external().bind(fabric_.from[i]);
        fabric_.to_core[i].bind(cores_[i]->inbound());
    }

    // ── the chip's own register windows ──────────────────────────────────────
    fabric_.to_control.bind(control_regs_.socket);
    fabric_.to_counters.bind(counter_regs_.socket);

    // ── one bus lock for both harts ──────────────────────────────────────────
    //
    // Attached during elaboration, which is the only time the backend accepts
    // it: the ISS has to have executed no atomic instruction against the lock
    // it is losing. Without this each hart keeps its own, and two private locks
    // exclude nobody — `test_bus_lock_atomicity`'s unshared control loses
    // exactly half of its updates for that reason (decision record D8).
    for (auto& core : cores_) {
        core->cpu().attach_bus_lock(bus_lock_);
    }

    // `fabric_.external` and `fabric_.from[external_inbound]` stay unbound: the
    // platform or the test attaches the mesh. That is the chip's whole
    // interface to the outside, and it is two sockets wide.
}

hart_id_t tpu_chip::hart_id(unsigned core) const
{
    return this->core(core).hart_id();
}

core::tpu_core& tpu_chip::core(unsigned index)
{
    if (index >= cores_per_chip) {
        throw std::out_of_range(
            std::string("tpu_v3::tpu_chip[") + name() + "]::core: index "
            + std::to_string(index) + " is not 0 or 1");
    }
    return *cores_[index];
}

const core::tpu_core& tpu_chip::core(unsigned index) const
{
    if (index >= cores_per_chip) {
        throw std::out_of_range(
            std::string("tpu_v3::tpu_chip[") + name() + "]::core: index "
            + std::to_string(index) + " is not 0 or 1");
    }
    return *cores_[index];
}

unsigned tpu_chip::bus_lock_sharers() const
{
    return cores_[0]->cpu().bus_lock_sharers();
}

void tpu_chip::reset()
{
    // Cores before the fabric that carries their traffic, for the same reason
    // `tpu_core::reset()` resets its requesters before its local fabric: a
    // requester must not be able to enqueue a fresh transaction into a fabric
    // that has already been cleared. The fabric's generation counter then
    // abandons whatever was in flight.
    //
    // Nothing here yields, so the whole chip is reset atomically. Each core's
    // own `reset()` schedules its hardware reset-line pulse without blocking.
    for (auto& core : cores_) {
        core->reset();
    }

    fabric_.reset();
    control_regs_.reset();
    counter_regs_.reset();

    // The bus lock is deliberately **not** cleared here.
    //
    // Each hart releases what it holds through its own `reset_cpu()`, which
    // calls `release_lr_sc_reservation()` and with it `atomic_unlock()`. That
    // is the correct owner-respecting release, and it is what lets the sibling
    // proceed. Zeroing the lock from outside would also "work", and would
    // release a lock this chip's reset had nothing to do with the moment the
    // scope of the lock ever grows past one chip.
}

std::string tpu_chip::report() const
{
    std::ostringstream out;
    out << "tpu_chip[" << name() << "]\n"
        << "  chip " << config_.chip << " at 0x" << std::hex << chip_base()
        << std::dec << ", " << cores_per_chip << " NEO-COREs, harts "
        << cores_[0]->hart_id() << " and " << cores_[1]->hart_id() << '\n'
        << "  chip fabric " << to_string(config_.timing) << ", cycle "
        << config_.cycle << '\n'
        << "  one aggregated NoC manager (plan §4.4); core-to-core traffic is "
           "answered inside the chip and never offered to the mesh\n"
        << "  LR/SC and AMO bus lock shared by "
        << cores_[0]->cpu().bus_lock_sharers() << " harts (decision record D8)"
        << '\n';
    for (const auto& core : cores_) {
        out << core->report();
    }
    out << fabric_.report();
    return out.str();
}

} // namespace cdc::components::tpu_v3::chip
