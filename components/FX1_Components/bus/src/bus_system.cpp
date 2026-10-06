#include "bus/bus_system.h"
#include <stdexcept>

namespace bus {
namespace {
BusConfig checked_config(const BusConfig& cfg) {
    cfg.validate();
    return cfg;
}

using Targets = std::vector<const TargetConfig*>;
Targets select(const BusConfig& cfg, TargetPath path) {
    Targets result;
    for (const auto& target : cfg.targets)
        if (target.enabled && target.path == path) result.push_back(&target);
    return result;
}

std::vector<Region> leaves(const Targets& targets) {
    std::vector<Region> result;
    for (unsigned i = 0; i < targets.size(); ++i) {
        const auto& t = *targets[i];
        result.push_back({t.name, t.base, t.base + t.size, i, true});
    }
    return result;
}

void append_link(std::vector<Region>& parent, const std::vector<Region>& child,
                 unsigned port) {
    // Exact leaf windows preserve holes; no large window hides an unrelated IP.
    for (const auto& r : child)
        parent.push_back({r.name, r.begin, r.end, port, false});
}
} // namespace

BusSystem::BusSystem(sc_core::sc_module_name name, bool trace)
    : BusSystem(name, BusConfig::fx1(), trace) {}

BusSystem::BusSystem(sc_core::sc_module_name name, const BusConfig& cfg, bool trace)
    : sc_module(name), config_(checked_config(cfg)),
      initiator_ports_("initiators", config_.initiators.size()), target_ports_("targets") {
    const auto direct1 = select(config_, TargetPath::SysBus1Axi);
    const auto direct0 = select(config_, TargetPath::SysBus0Axi);
    const auto apb1 = select(config_, TargetPath::Peribus1Apb);
    const auto apb0 = select(config_, TargetPath::Peribus0Apb);
    target_ports_.init(direct1.size() + direct0.size() + apb1.size() + apb0.size());

    auto s1 = leaves(direct1);
    auto s0 = leaves(direct0);
    const auto p1 = leaves(apb1);
    const auto p0 = leaves(apb0);
    unsigned outputs0 = static_cast<unsigned>(direct0.size());
    if (!p0.empty()) append_link(s0, p0, outputs0++);
    unsigned outputs1 = static_cast<unsigned>(direct1.size());
    if (!p1.empty()) append_link(s1, p1, outputs1++);
    if (!s0.empty()) append_link(s1, s0, outputs1++);

    const auto latency = config_.router_latency_ns;
    sysbus1_ = std::make_unique<Router>("SYSBUS_1", s1, outputs1, latency, trace);
    if (!s0.empty())
        sysbus0_ = std::make_unique<Router>("SYSBUS_0", s0, outputs0, latency, trace);
    if (!p1.empty()) {
        peribus1_ = std::make_unique<Router>("PERIBUS_1", p1, p1.size(), latency, trace);
        a2p1_ = std::make_unique<ApbBridge>("A2P_PP1", config_.apb_cycle_ns);
    }
    if (!p0.empty()) {
        peribus0_ = std::make_unique<Router>("PERIBUS_0", p0, p0.size(), latency, trace);
        a2p0_ = std::make_unique<ApbBridge>("A2P_PP0", config_.apb_cycle_ns);
    }

    for (std::size_t i = 0; i < config_.initiators.size(); ++i) {
        initiator_index_.emplace(config_.initiators[i].name, i);
        initiator_ports_[i].out.bind(sysbus1_->target);
    }
    std::size_t external = 0;
    auto bind_leaves = [&](Router& router, const Targets& targets) {
        for (const auto* t : targets) {
            target_index_.emplace(t->name, external);
            router.out.bind(target_ports_[external++].input);
        }
    };
    bind_leaves(*sysbus1_, direct1);
    if (sysbus0_) bind_leaves(*sysbus0_, direct0);
    if (peribus1_) {
        sysbus1_->out.bind(a2p1_->target);
        a2p1_->out.bind(peribus1_->target);
        bind_leaves(*peribus1_, apb1);
    }
    if (sysbus0_) sysbus1_->out.bind(sysbus0_->target);
    if (peribus0_) {
        sysbus0_->out.bind(a2p0_->target);
        a2p0_->out.bind(peribus0_->target);
        bind_leaves(*peribus0_, apb0);
    }
}

tlm_utils::simple_target_socket_optional<InitiatorPort>&
BusSystem::initiator(const std::string& name) {
    const auto it = initiator_index_.find(name);
    if (it == initiator_index_.end()) throw std::out_of_range("Unknown initiator: " + name);
    return initiator_ports_[it->second].socket;
}

tlm_utils::simple_initiator_socket<TargetPort>& BusSystem::target(const std::string& name) {
    const auto it = target_index_.find(name);
    if (it == target_index_.end()) throw std::out_of_range("Unknown or disabled target: " + name);
    return target_ports_[it->second].socket;
}
} // namespace bus
