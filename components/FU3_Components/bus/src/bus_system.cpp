#include "fu3_bus/bus_system.h"
#include <stdexcept>

namespace fu3_bus {
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
    : BusSystem(name, BusConfig::fu3(), trace) {}

BusSystem::BusSystem(sc_core::sc_module_name name, const BusConfig& cfg, bool trace)
    : sc_module(name), config_(checked_config(cfg)),
      initiator_ports_("initiators", config_.initiators.size()),
      master_pipelines_("AP_master",config_.initiators.size()), target_ports_("targets") {
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
    if (!s0.empty()) {
        sysbus0_ = std::make_unique<Router>("SYSBUS_0", s0, outputs0, latency, trace);
        ap_sb0_=std::make_unique<TargetPort>("AP_SB0",config_.axi_cycle_ns);
        ap_sb0_->capacity(std::numeric_limits<unsigned>::max());
    }
    if (!p1.empty()) {
        peribus1_ = std::make_unique<Router>("PERIBUS_1", p1, p1.size(), latency, trace);
        a2p1_ = std::make_unique<ApbBridge>("A2P_PP1", config_.apb_cycle_ns);
        ap_pp1_=std::make_unique<TargetPort>("AP_PP1",config_.axi_cycle_ns);
    }
    if (!p0.empty()) {
        peribus0_ = std::make_unique<Router>("PERIBUS_0", p0, p0.size(), latency, trace);
        a2p0_ = std::make_unique<ApbBridge>("A2P_PP0", config_.apb_cycle_ns);
        ap_pp0_=std::make_unique<TargetPort>("AP_PP0",config_.axi_cycle_ns);
    }

    for (std::size_t i = 0; i < config_.initiators.size(); ++i) {
        initiator_index_.emplace(config_.initiators[i].name, i);
        initiator_ports_[i].configure(config_.initiators[i], config_, staging_, accepting_, activity_);
        master_pipelines_[i].cycle(config_.axi_cycle_ns);
        master_pipelines_[i].capacity(std::numeric_limits<unsigned>::max());
        initiator_ports_[i].out.bind(master_pipelines_[i].input);
        master_pipelines_[i].socket.bind(sysbus1_->target);
    }
    std::size_t external = 0;
    auto bind_leaves = [&](Router& router, const Targets& targets) {
        for (const auto* t : targets) {
            target_index_.emplace(t->name, external);
            const bool apb=t->path==TargetPath::Peribus0Apb || t->path==TargetPath::Peribus1Apb;
            target_ports_[external].cycle(apb?config_.apb_cycle_ns:config_.axi_cycle_ns);
            target_ports_[external].capacity(apb?1:t->outstanding);
            router.out.bind(target_ports_[external++].input);
        }
    };
    bind_leaves(*sysbus1_, direct1);
    if (sysbus0_) bind_leaves(*sysbus0_, direct0);
    if (peribus1_) {
        sysbus1_->out.bind(ap_pp1_->input);
        ap_pp1_->socket.bind(a2p1_->target);
        a2p1_->out.bind(peribus1_->target);
        bind_leaves(*peribus1_, apb1);
    }
    if (sysbus0_) {
        sysbus1_->out.bind(ap_sb0_->input);
        ap_sb0_->socket.bind(sysbus0_->target);
    }
    if (peribus0_) {
        sysbus0_->out.bind(ap_pp0_->input);
        ap_pp0_->socket.bind(a2p0_->target);
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

void BusSystem::finish_staging() {
    const auto it=initiator_index_.find("IBEX");
    if(it!=initiator_index_.end() && initiator_ports_[it->second].in_flight())
        throw std::logic_error("Drain IBEX transactions before closing staging/releasing CVA6 reset");
    staging_=false;
}
void BusSystem::set_clock(sc_core::sc_time axi, sc_core::sc_time apb, sc_core::sc_time router) {
    for(auto* r:{sysbus1_.get(),sysbus0_.get(),peribus1_.get(),peribus0_.get()}) if(r) r->latency(router);
    for(auto* b:{a2p0_.get(),a2p1_.get()}) if(b) b->cycle(apb);
    for(auto* p:{ap_sb0_.get(),ap_pp0_.get(),ap_pp1_.get()}) if(p) p->cycle(axi);
    for(auto& p:master_pipelines_) p.cycle(axi);
    for(auto& p:initiator_ports_) p.rejection_latency(router);
    for(const auto& t:config_.targets) {
        const auto it=target_index_.find(t.name);
        if(it==target_index_.end()) continue;
        const bool apb_target=t.path==TargetPath::Peribus0Apb || t.path==TargetPath::Peribus1Apb;
        target_ports_[it->second].cycle(apb_target?apb:axi);
    }
}
void BusSystem::close_staging() {
    if(sc_core::sc_get_current_process_handle().proc_kind()!=sc_core::SC_THREAD_PROC_)
        throw std::logic_error("close_staging must run in SC_THREAD outside a bus transaction");
    staging_=false;
    const auto it=initiator_index_.find("IBEX");
    if(it==initiator_index_.end()) return;
    const auto& ibex=initiator_ports_[it->second];
    const auto mark=ibex.issued();
    while(!ibex.completed_before(mark)) sc_core::wait(activity_);
}
unsigned BusSystem::peak_outstanding(const std::string& name, bool write) const {
    const auto it=initiator_index_.find(name);
    if(it==initiator_index_.end()) throw std::out_of_range("Unknown initiator: "+name);
    return initiator_ports_[it->second].peak(write);
}
void BusSystem::reset() {
    for(const auto& port:initiator_ports_)
        if(port.in_flight()) throw std::logic_error("Quiesce all masters before resetting the FU3 bus");
    staging_=true;
}
void BusSystem::quiesce() {
    if(sc_core::sc_get_current_process_handle().proc_kind()!=sc_core::SC_THREAD_PROC_)
        throw std::logic_error("Bus quiesce must run in SC_THREAD outside a bus transaction");
    accepting_=false;
    for (;;) {
        bool active=false;
        for(const auto& port:initiator_ports_) active=active || port.in_flight()!=0;
        if(!active) return;
        sc_core::wait(activity_);
    }
}
void BusSystem::resume() {
    for(const auto& port:initiator_ports_)
        if(port.in_flight()) throw std::logic_error("Drain admitted work before reopening ingress");
    accepting_=true;
}

tlm_utils::simple_initiator_socket<TargetPort>& BusSystem::target(const std::string& name) {
    const auto it = target_index_.find(name);
    if (it == target_index_.end()) throw std::out_of_range("Unknown or disabled target: " + name);
    return target_ports_[it->second].socket;
}
} // namespace fu3_bus
