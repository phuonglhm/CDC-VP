#include "bus/config.h"
#include "support/test_helpers.h"
#include <limits>
#include <stdexcept>

int sc_main(int, char**) {
    Checks c;
    auto valid=[](const bus::BusConfig& cfg) {
        try { cfg.validate(); return true; } catch(const std::invalid_argument&) { return false; }
    };
    auto cfg=bus::BusConfig::fx1();
    c.expect(valid(cfg),"FX1 default");
    c.expect(cfg.initiators.size()==9,"nine masters (MIPI_DMA: CSI-2 RX DMA writer)");
    c.expect(cfg.axi_data_width==32 && cfg.axi_address_width==32,"default 32-bit widths");
    auto bad=[&](auto mutate,const char* label){auto x=bus::BusConfig::fx1();mutate(x);c.expect(!valid(x),label);};
    bad([](auto& x){x.axi_data_width=64;},"unsupported 64-bit data rejected");
    bad([](auto& x){x.axi_data_width=0;},"zero data width");
    bad([](auto& x){x.axi_address_width=0;},"zero address width");
    bad([](auto& x){x.axi_address_width=65;},"address width above payload capacity");
    bad([](auto& x){x.axi_address_width=31;},"DDR exceeds address width");
    bad([](auto& x){x.initiators.clear();},"no initiators");
    bad([](auto& x){x.initiators.push_back({"CPU1"});},"duplicate initiator");
    bad([](auto& x){x.initiators[0].name.clear();},"empty initiator name");
    bad([](auto& x){x.targets.push_back(x.targets[0]);},"duplicate target");
    bad([](auto& x){x.targets[0].name.clear();},"empty target name");
    bad([](auto& x){x.targets[1].base=0;},"overlap across different paths");
    bad([](auto& x){x.targets[0].size=0;},"zero-sized enabled target");
    // CPU1_IRAM has no address yet (TBD); SRAM (index 5) got one in DDR3 subtask 5.
    bad([](auto& x){x.targets[6].enabled=true;},"enabled TBD");
    {   // The fx1_soc DDR3 integration enables these placeholders (fx1_memory_map.h).
        auto x=bus::BusConfig::fx1();
        for(auto& t:x.targets)
            if(t.name=="SRAM"||t.name=="DDR3_CSR"||t.name=="DDR_SYSCTRL") t.enabled=true;
        c.expect(valid(x),"SRAM, DDR3_CSR and DDR_SYSCTRL placeholders enabled");
    }
    {   // fx1_soc enables I2C0 always and MIPI_CSR with --camera (fx1_memory_map.h).
        auto x=bus::BusConfig::fx1();
        for(auto& t:x.targets)
            if(t.name=="I2C0"||t.name=="MIPI_CSR") t.enabled=true;
        c.expect(valid(x),"I2C0 and MIPI_CSR placeholders enabled");
    }
    bad([](auto& x){for(auto& t:x.targets)t.enabled=false;},"no active targets");
    bad([](auto& x){x.axi_address_width=64;x.targets[0].base=UINT64_MAX-1;x.targets[0].size=4;},"end overflow");
    bad([](auto& x){x.targets[0].base=0xFFFFF000ULL;x.targets[0].size=0x2000;},"range extends beyond address width");
    bad([](auto& x){x.targets[0].path=static_cast<bus::TargetPath>(99);},"unknown target path");
    bad([](auto& x){x.arbitration=static_cast<bus::ArbitrationPolicy>(99);},"unknown arbitration");
    bad([](auto& x){x.targets[1].base+=1;},"APB base alignment");
    cfg.targets[5]={"SRAM",0x30000000,0x1000,bus::TargetPath::SysBus0Axi,true};
    cfg.targets.push_back({"adjacent",0x30001000,0x1000,bus::TargetPath::Peribus1Apb,true});
    c.expect(valid(cfg),"enable TBD and adjacent regions on different paths");
    cfg.targets.push_back({"last32",0xFFFFF000ULL,0x1000,bus::TargetPath::SysBus1Axi,true});
    c.expect(valid(cfg),"32-bit top boundary exclusive 2^32");
    cfg.axi_address_width=64;
    cfg.targets.push_back({"high",0x100000000ULL,0x1000,bus::TargetPath::SysBus0Axi,true});
    c.expect(valid(cfg),"64-bit address metadata with 32-bit data");
    for(unsigned i=0;i<20;++i) {
        cfg.initiators.push_back({"master"+std::to_string(i)});
        cfg.targets.push_back({"extra"+std::to_string(i),0x60000000+i*0x1000ULL,0x1000,bus::TargetPath::SysBus0Axi,true});
    }
    c.expect(valid(cfg),"port count not limited to 5+3");
    std::cout<<"Config: "<<c.checks<<" checks, "<<c.failures<<" failures\n";
    return c.failures?1:0;
}
