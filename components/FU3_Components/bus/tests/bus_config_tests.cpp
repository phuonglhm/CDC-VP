#include "fu3_bus/config.h"
#include "support/test_helpers.h"
#include <stdexcept>

int sc_main(int, char**) {
    Checks c;
    auto good = [](const fu3_bus::BusConfig& x) {
        try { x.validate(); return true; } catch (const std::invalid_argument&) { return false; }
    };
    auto cfg = fu3_bus::BusConfig::fu3();
    c.expect(good(cfg), "FU3 map validates");
    unsigned enabled=0;
    for(const auto& t:cfg.targets) enabled+=t.enabled;
    c.expect(cfg.initiators.size()==4 && cfg.targets.size()==44 && enabled==40,
             "4 masters, 44 v2.1 windows, 40 enabled (EDHOC/PWM0/PWM1/CAN wait for R2)");
    c.expect(cfg.revision=="FU3 Common Spec v2.1", "map names its spec revision");
    auto bad = [&](auto mutate, const char* label) { auto x=cfg; mutate(x); c.expect(!good(x),label); };
    bad([](auto& x){x.targets[1].base=0;}, "overlap");
    bad([](auto& x){x.targets[0].size=0;}, "empty range");
    bad([](auto& x){x.targets[0].base=UINT64_MAX;}, "overflow");
    bad([](auto& x){x.targets[0].base=0xfffff000ULL;x.targets[0].size=0x2000;}, "32-bit overflow");
    bad([](auto& x){x.targets.push_back(x.targets[0]);}, "duplicate target");
    bad([](auto& x){x.initiators.push_back(x.initiators[0]);}, "duplicate master");
    bad([](auto& x){x.initiators.clear();}, "missing masters");
    bad([](auto& x){x.initiators[0].outstanding=0;}, "zero admission capacity");
    bad([](auto& x){x.initiators[0].max_beats=0;}, "zero beat limit");
    bad([](auto& x){x.targets.clear();}, "missing targets");
    bad([](auto& x){x.targets[1].outstanding=0;}, "zero target capacity");
    bad([](auto& x){x.targets[1].size--;}, "unaligned word interface window");
    bad([](auto& x){x.targets[8].base++;}, "APB alignment");
    bad([](auto& x){x.axi_data_width=64;}, "unsupported data width");
    bad([](auto& x){x.axi_address_width=64;}, "FU3 address width");
    bad([](auto& x){x.targets[0].path=static_cast<fu3_bus::TargetPath>(99);}, "invalid path");
    bad([](auto& x){x.targets[0].access=static_cast<fu3_bus::AccessPolicy>(99);}, "invalid policy");
    bad([](auto& x){x.targets[8].preserve_burst=true;}, "APB cannot preserve an AXI burst");
    bad([](auto& x){x.targets[8].strict_word_offsets={1};}, "strict word must align");
    bad([](auto& x){x.targets[8].strict_word_offsets={0x10000};}, "strict word must fit target");
    bad([](auto& x){x.targets[8].strict_word_offsets={0x24,0x24};}, "duplicate strict word");
    cfg.targets.push_back({"TOP",0xfffff000ULL,0x1000,fu3_bus::TargetPath::SysBus1Axi,true});
    c.expect(good(cfg), "last valid 32-bit window");
    std::cout << "Config: " << c.checks << " checks, " << c.failures << " failures\n";
    return c.failures ? 1 : 0;
}
