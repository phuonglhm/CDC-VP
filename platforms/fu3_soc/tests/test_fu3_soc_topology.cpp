// Plan T21: connection audit of the assembled platform, from the SystemC
// object tree after elaboration (CPUs and JTAG included).
//   - every port, export and TLM socket is bound, except the optional ports
//     listed below with their reason (a new unbound one fails the test);
//   - no signal has two writer ports;
//   - every PLIC source (Common v2.1 Interrupt Mapping) has exactly one writer
//     port, except the sources listed below as tie-offs (constant 0) or as
//     written by a platform process (no port), and the
//     PLIC and INTMAP read those nets; the Ibex-only INTMAP inputs and the
//     CPU interrupt nets have their writer;
//   - the interrupt/DMA outputs left unconnected (no PLIC ID, no DMA channel
//     in Common v2.1) are exactly the documented ones, driven and unread;
//   - the four bus initiators (CVA6, IBEX, DBG, DMA0) are bound.
// Mandatory ports SystemC already checks at elaboration; this test covers the
// optional ones and the nets, which elaboration does not.

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <systemc>

#include <fu3_plic/registers.h>

#include "fu3_soc_top.h"

using namespace cdc::platforms::fu3_soc;

namespace {

unsigned checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

// Optional ports left unbound on purpose.
const std::map<std::string, const char*> kUnbound = {
    {"fu3_soc.peripherals.WDT0.wdogpause", "status output of the software DBGEN pause; no consumer in Common v2.1"},
    {"fu3_soc.peripherals.WDT1.wdogpause", "status output of the software DBGEN pause; no consumer in Common v2.1"},
    {"fu3_soc.peripherals.WDT0.i_wdt_other_unlock", "WDT HDS V1.1 input, no effect in V1.2 (WDT MODEL_CONTRACT)"},
    {"fu3_soc.peripherals.WDT1.i_wdt_other_unlock", "WDT HDS V1.1 input, no effect in V1.2 (WDT MODEL_CONTRACT)"},
};

// PLIC sources without a driver in R1 (tied low).
const std::map<unsigned, const char*> kTiedSources = {
    {FU3_PLIC_ID_AES, "AES model has no interrupt port; assert/clear rule open (Q03-f, T08_T18_Van_De)"},
    {FU3_PLIC_ID_ADC, "ADC model has no interrupt port; assert/clear rule open (Q03-f, T08_T18_Van_De)"},
    {FU3_PLIC_ID_EDHOC_OSCORE, "EDHOC/OSCORE: R2 IP, not received (T24/T25)"},
    {FU3_PLIC_ID_PWM0, "PWM0: R2 IP, not received (T24/T26)"},
    {FU3_PLIC_ID_PWM1, "PWM1: R2 IP, not received (T24/T26)"},
    {FU3_PLIC_ID_CAN, "CAN: R2 IP, not received (T24/T27)"},
};

// Interrupt and DMA request outputs the platform leaves unconnected: a writer
// port, no reader port (and no platform process reads them).
const std::map<std::string, const char*> kDangling = {
    {"fu3_soc.peripherals.mailbox_cva6_irq_unrouted", "MAILBOX_CVA6 interrupt: no PLIC ID in Common v2.1 (Q03-g)"},
    {"fu3_soc.peripherals.hbctrl_irq_unrouted", "HyperRAM controller interrupt: no PLIC ID in Common v2.1"},
    {"fu3_soc.peripherals.qspi_dma_rx_req", "QSPI DMA request: no channel in Common v2.1 DMA Mapping"},
    {"fu3_soc.peripherals.qspi_dma_tx_req", "QSPI DMA request: no channel in Common v2.1 DMA Mapping"},
};

// PLIC sources written by a platform process instead of a model port.
const std::map<unsigned, const char*> kProcessSources = {
    {FU3_PLIC_ID_GPIO0_WAKE, "OR of the GPIOA/B/C power_event levels (fu3_peripherals, provisional: Q06-c); "
                             "behaviour checked by fu3_soc_peripherals"},
};

struct audit : sc_core::sc_module {
    fu3_soc_top& top;
    std::vector<std::string> unbound;
    std::map<const sc_core::sc_interface*, std::vector<std::string>> writers, readers;
    unsigned ports = 0, exports = 0;
    bool finished = false;

    SC_HAS_PROCESS(audit);
    audit(sc_core::sc_module_name n, fu3_soc_top& t) : sc_module(n), top(t) { SC_THREAD(run); }

    std::map<std::string, const sc_core::sc_interface*> nets;
    void visit(sc_core::sc_object* o)
    {
        if (std::string(o->kind()) == "sc_signal")
            nets[o->name()] = dynamic_cast<const sc_core::sc_interface*>(o);
        if (auto* p = dynamic_cast<sc_core::sc_port_base*>(o)) {
            ++ports;
            if (!p->get_interface()) {
                unbound.push_back(o->name());
            } else {
                // sc_out/sc_inout and out-interface ports write the channel.
                const std::string type = p->get_interface_type().name();
                const bool writes = type.find("out_if") != std::string::npos;
                (writes ? writers : readers)[p->get_interface()].push_back(o->name());
            }
        } else if (auto* e = dynamic_cast<sc_core::sc_export_base*>(o)) {
            ++exports;
            if (!e->get_interface())
                unbound.push_back(o->name());
        }
        for (auto* child : o->get_child_objects())
            visit(child);
    }
    const sc_core::sc_interface* net(sc_core::sc_signal<bool>& s) { return &s; }

    void run()
    {
        for (auto* o : sc_core::sc_get_top_level_objects())
            if (o != this)
                visit(o);
        std::cout << "audit: " << ports << " ports, " << exports << " exports\n";

        // Unbound ports and sockets: exactly the documented optional ones.
        std::set<std::string> seen;
        for (const auto& name : unbound) {
            seen.insert(name);
            const auto it = kUnbound.find(name);
            check(it != kUnbound.end(), "unbound port or socket without a documented reason: " + name);
            if (it != kUnbound.end())
                std::cout << "  unbound " << name << ": " << it->second << '\n';
        }
        for (const auto& [name, why] : kUnbound)
            check(seen.count(name) == 1, "documented unbound port is bound or gone (update the list): " + name);

        // One writer port per signal at most.
        for (const auto& [channel, names] : writers)
            check(names.size() == 1, "signal written by " + std::to_string(names.size()) + " ports: " + names[0] +
                                         (names.size() > 1 ? ", " + names[1] : ""));

        // PLIC sources: driven by one model, read by the PLIC (and the INTMAP for shared ones).
        auto& p = top.peripherals();
        for (unsigned id = 1; id <= fu3_plic::kSources; ++id) {
            const auto* channel = net(p.plic_source[id - 1]);
            const bool tied = kTiedSources.count(id) == 1, process = kProcessSources.count(id) == 1;
            const auto w = writers.count(channel) ? writers[channel].size() : 0;
            check(tied || process ? w == 0 : w == 1,
                  "PLIC ID " + std::to_string(id) + (tied ? " tied low" : process ? " process-driven" : " has one driver port"));
            if (process)
                std::cout << "  PLIC ID " << id << " process-driven: " << kProcessSources.at(id) << '\n';
            check(readers.count(channel) && !readers[channel].empty(), "PLIC ID " + std::to_string(id) + " read by the PLIC");
            if (tied)
                std::cout << "  PLIC ID " << id << " tied low: " << kTiedSources.at(id) << '\n';
            check(!tied || !p.plic_source[id - 1].read(), "tied PLIC ID " + std::to_string(id) + " is low");
        }
        for (auto* s : {&p.wdt0_warning, &p.mailbox_ibex_irq, &p.ibex_software, &p.cva6_external, &p.cva6_timer,
                        &p.cva6_software})
            check(writers.count(net(*s)) == 1, std::string("interrupt net has its driver: ") + s->name());
        for (unsigned o = 0; o < fu3_plic::kIntmapOutputs; ++o)
            check(writers.count(net(p.ibex_irq[o])) == 1, "INTMAP output " + std::to_string(o) + " has its driver");

        // Documented dangling interrupt/DMA outputs: driven, read by no port.
        for (const auto& [name, why] : kDangling) {
            const auto it = nets.find(name);
            check(it != nets.end() && writers.count(it->second) == 1 && !readers.count(it->second),
                  "documented unconnected output: " + name);
            std::cout << "  unconnected " << name << ": " << why << '\n';
        }

        // Bus initiators: every port of the FU3 map has its master.
        for (const auto& name : fu3_soc_top::initiator_names())
            check(top.fabric().initiator(name).get_base_port().get_interface() != nullptr,
                  "bus initiator " + name + " bound to its master");
        finished = true;
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    fu3_soc_top top("fu3_soc", fu3_soc_options{});
    audit a("audit", top);
    sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_US));
    check(a.finished, "audit ran");
    std::cout << "fu3_soc topology: " << checks << " checks, " << failures << " failures -> "
              << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? 1 : 0;
}
