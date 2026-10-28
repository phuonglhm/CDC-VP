// Plans T03/T04/T07: the fu3_soc top elaborates, binds every enabled bus target
// to a memory model (ROM, ISRAMs, DSRAM), an integrated model (CRM, SYSCSR) or
// a not-integrated responder, puts every target except the CRM behind its clock
// gate, leaves the disabled R2 windows unbound, and passes its own self-test.

#include <iostream>
#include <set>
#include <string>

#include <systemc>

#include "fu3_soc_top.h"
#include "self_test.h"

using namespace cdc::platforms::fu3_soc;

namespace {
unsigned failures = 0;
void check(bool ok, const std::string& what)
{
    if (!ok) {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}
} // namespace

int sc_main(int, char**)
{
    sc_core::sc_report_handler::set_actions("fu3_soc/not_integrated", sc_core::SC_DO_NOTHING);
    fu3_soc_options options;
    options.cpus = false;  // the self-test below takes the IBEX port
    fu3_soc_top top("fu3_soc", options);

    std::set<std::string> enabled;
    for (const auto& t : fu3_bus::BusConfig::fu3().targets)
        if (t.enabled)
            enabled.insert(t.name);
    check(top.instances().size() == enabled.size(), "one instance per enabled bus target");
    check(!top.fabric().configuration().revision.empty(), "the bus map names its spec revision");
    const std::set<std::string> memories = {"ROM", "CVA6_ISRAM", "IBEX_ISRAM", "DSRAM"};
    // Integrated peripheral models so far (T07: CRM, SYSCSR; T08: AP_PLIC; ...).
    const std::set<std::string> models = {"CRM", "SYSCSR", "AP_PLIC", "TIMER0", "TIMER1", "TIMER2", "TIMER3", "WDT0", "WDT1",
                                            "UART0", "UART1", "UART2", "UART3", "UART4", "UART5", "UART6", "UART7",
                                            "SPI0", "SPI1", "SPI2", "SPI3", "I2C", "GPIOA", "GPIOB", "GPIOC", "AP_QSPIC", "QSPI_XIP",
                                            "HYPERBUS", "AP_HBCTRL", "AES", "ADC", "RNG", "MAILBOX_CVA6", "MAILBOX_IBEX", "AP_DMA", "IBEX_DBG"};
    for (const auto& i : top.instances()) {
        check(enabled.count(i.name) == 1, i.name + " is a bus target");
        const bool memory = memories.count(i.name) == 1, model = models.count(i.name) == 1;
        check(i.integrated == (memory || model), i.name + " integration state");
        check((top.memory(i.name) != nullptr) == memory, i.name + " memory model bound iff a memory");
        // Every integrated model has a debug probe except the XIP and HyperRAM data
        // windows and AP_DMA (no debug transport).
        const bool probed = model && i.name != "QSPI_XIP" && i.name != "HYPERBUS" && i.name != "AP_DMA";
        check((top.probe(i.name) != nullptr) == probed, i.name + " model probe iff an integrated model with registers");
        check((top.responder(i.name) != nullptr) == !i.integrated, i.name + " not-integrated responder iff not integrated");
        check((top.gate(i.name) != nullptr) == (i.name != "CRM"), i.name + " behind a clock gate (not the CRM)");
        check(!i.domain.empty(), i.name + " names its clock/reset domain");
    }
    for (const auto* t : top.disabled_targets())
        check(enabled.count(t->name) == 0, t->name + " is disabled and has no instance");
    check(fu3_soc_top::initiator_names().size() == 4, "four initiator ports (CVA6, IBEX, DBG, DMA0)");

    self_test probe("self_test", top, std::cerr);
    sc_core::sc_start();
    check(probe.done(), "self-test finished");
    check(probe.passed(), "self-test passed");
    check(probe.checks() > 3 * top.instances().size(), "self-test covered every instance");

    std::cout << "fu3_soc skeleton: " << (failures ? "FAIL" : "PASS") << '\n';
    return failures ? 1 : 0;
}
