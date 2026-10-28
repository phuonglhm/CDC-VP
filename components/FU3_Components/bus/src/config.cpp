#include "fu3_bus/config.h"
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace fu3_bus {
BusConfig BusConfig::fu3() {
    BusConfig c;
    // Common Spec v2.1 MemoryMap (revision 18/10/2026), decision Q02-a. UART2-7,
    // PWM and CAN windows are "proposed" in the source and kept as given.
    c.revision = "FU3 Common Spec v2.1";
    c.initiators = {{"CVA6",2,false,true,256}, {"IBEX",2,false,true,256},
                    {"DBG",1,false,true,256,true}, {"DMA0",4,true,true,16}};
    using P = TargetPath;
    c.targets = {
        {"ROM",0x00000000,0x1000,P::SysBus1Axi,true},
        {"CVA6_ISRAM",0x00010000,0x31000,P::SysBus1Axi,true},   // 192 KiB image + 4 KiB debug
        {"IBEX_ISRAM",0x00050000,0x11000,P::SysBus1Axi,true},   // 64 KiB image + 4 KiB debug
        {"DSRAM",0x01000000,0x40000,P::SysBus1Axi,true},
        {"IBEX_DBG",0x1A110000,0x1000,P::SysBus1Axi,true},
        {"QSPI_XIP",0x30000000,0x1000000,P::SysBus0Axi,true},
        {"EDHOC_OSCORE",0x31010000,0x10000,P::SysBus0Axi,true},
        {"AES",0x40000000,0x10000,P::SysBus0Axi,true},
        {"AP_PLIC",0x40100000,0x10000,P::Peribus1Apb,true},
        {"HYPERBUS",0x60000000,0x2000000,P::SysBus1Axi,true}
    };
    const char* p0[] = {"AP_QSPIC","SYSCSR","AP_DMA","RNG","MAILBOX_IBEX","WDT0","TIMER0",
        "TIMER1","WDT1","UART2","UART3","SPI1","SPI0","UART0","UART1","GPIOC","PWM0","CAN"};
    for(unsigned i=0;i<18;++i)
        c.targets.push_back({p0[i],0x44000000ULL+i*0x10000ULL,0x10000,P::Peribus0Apb,true});
    const char* p1[] = {"CRM","MAILBOX_CVA6","GPIOA","GPIOB","UART4","TIMER2","TIMER3","I2C",
        "SPI2","ADC","AP_HBCTRL","SPI3","UART5","UART6","UART7","PWM1"};
    for(unsigned i=0;i<16;++i)
        c.targets.push_back({p1[i],0x50010000ULL+i*0x10000ULL,0x10000,P::Peribus1Apb,true});
    for (auto& t : c.targets) {
        // External IP delivered for R2 (Q08-a): in the map, disabled, so an access
        // is a decode error until the model is integrated.
        if (t.name=="EDHOC_OSCORE" || t.name=="PWM0" || t.name=="PWM1" || t.name=="CAN")
            t.enabled=false;
        if (t.name=="CVA6_ISRAM" || t.name=="IBEX_ISRAM" || t.name=="DSRAM" || t.name=="AES")
            t.outstanding=4;
        t.word_interface=t.name=="ROM" || t.name=="CVA6_ISRAM" || t.name=="IBEX_ISRAM" ||
            t.name=="DSRAM" || t.name=="AES" || t.name=="QSPI_XIP" || t.name=="HYPERBUS" || t.name=="AP_PLIC";
        if (t.name == "ROM") t.access = AccessPolicy::BootRom;
        if (t.name == "CVA6_ISRAM") t.access = AccessPolicy::Cva6Program;
        if (t.name == "IBEX_ISRAM") t.access = AccessPolicy::IbexProgram;
        if (t.name == "IBEX_DBG") t.access = AccessPolicy::DebugControl;
        if (t.name == "DSRAM") t.access = AccessPolicy::DataOnly;
        if (t.name == "QSPI_XIP") t.access = AccessPolicy::ReadExecute; // Q02-h
        if (t.name == "CVA6_ISRAM") t.debug_offset = 0x30000;          // HaltAddress 0x0004_0000
        if (t.name == "IBEX_ISRAM") t.debug_offset = 0x10000;          // DmHaltAddr 0x0006_0000
        t.non_executable = t.name!="ROM" && t.name!="CVA6_ISRAM" &&
            t.name!="IBEX_ISRAM" && t.name!="QSPI_XIP";
        // CLAIM/COMPLETE (VP ABI Q03-a) is the side-effect word; the rest of the
        // PLIC takes byte lanes and reports a partial write itself (Q02-g).
        if (t.name == "AP_PLIC") t.strict_word_offsets = {0x3004};
        if (t.name.rfind("TIMER",0)==0 || t.name.rfind("WDT",0)==0 ||
            t.name=="I2C" || t.name=="RNG" || t.name=="AP_HBCTRL") t.word_interface=true;
    }
    return c;
}
void BusConfig::validate() const {
    if (axi_data_width != 32)
        throw std::invalid_argument("This bus supports 32-bit data only");
    if (arbitration != ArbitrationPolicy::Fifo)
        throw std::invalid_argument("Only FIFO arbitration is supported");
    if (axi_address_width != 32)
        throw std::invalid_argument("FU3 requires a 32-bit address map");
    if (initiators.empty())
        throw std::invalid_argument("At least one initiator is required");

    std::unordered_set<std::string> names;
    for (const auto& initiator : initiators) {
        if (!initiator.outstanding || !initiator.max_beats)
            throw std::invalid_argument("Zero admission limit: " + initiator.name);
        if (initiator.name.empty() || !names.insert(initiator.name).second)
            throw std::invalid_argument("Empty or duplicate initiator: " + initiator.name);
    }
    names.clear();
    std::vector<const TargetConfig*> active;
    // end is exclusive; an interval ending at 2^64 cannot be represented.
    const auto max_address = std::numeric_limits<std::uint64_t>::max();
    const auto limit = axi_address_width == 64 ? max_address
                                               : (std::uint64_t{1} << axi_address_width);
    for (const auto& target : targets) {
        if (target.name.empty() || !names.insert(target.name).second)
            throw std::invalid_argument("Empty or duplicate target: " + target.name);
        switch (target.path) {
        case TargetPath::SysBus1Axi:
        case TargetPath::SysBus0Axi:
        case TargetPath::Peribus0Apb:
        case TargetPath::Peribus1Apb: break;
        default: throw std::invalid_argument("Invalid target path: " + target.name);
        }
        if (!target.outstanding) throw std::invalid_argument("Zero target admission limit: " + target.name);
        if (target.word_interface && (target.base%4 || target.size%4))
            throw std::invalid_argument("Word interface must have aligned boundaries: " + target.name);
        if (!target.enabled) continue;
        if (target.access != AccessPolicy::Open && target.access != AccessPolicy::BootRom &&
            target.access != AccessPolicy::Cva6Program && target.access != AccessPolicy::DataOnly &&
            target.access != AccessPolicy::IbexProgram && target.access != AccessPolicy::DebugControl &&
            target.access != AccessPolicy::ReadExecute)
            throw std::invalid_argument("Invalid access policy: " + target.name);
        if (!target.size || target.base > max_address - target.size)
            throw std::invalid_argument("Invalid target range: " + target.name);
        const auto end = target.base + target.size;
        if (target.debug_offset && (target.debug_offset>=target.size || target.debug_offset%4))
            throw std::invalid_argument("Invalid debug partition: " + target.name);
        const bool apb = target.path == TargetPath::Peribus0Apb ||
                         target.path == TargetPath::Peribus1Apb;
        if (apb && target.preserve_burst)
            throw std::invalid_argument("Preserved bursts require an AXI endpoint: " + target.name);
        std::unordered_set<std::uint64_t> strict_words;
        for (const auto offset : target.strict_word_offsets)
            if (target.base%4 || offset%4 || target.size<4 || offset>target.size-4 ||
                !strict_words.insert(offset).second)
                throw std::invalid_argument("Invalid strict-word offset: " + target.name);
        if (apb && target.base % 4)
            throw std::invalid_argument("APB target base must be word aligned: " + target.name);
        if (axi_address_width < 64 && end > limit)
            throw std::invalid_argument("Target exceeds address width: " + target.name);
        for (const auto* other : active) {
            if (target.base < other->base + other->size && other->base < end)
                throw std::invalid_argument("Overlapping target: " + target.name);
        }
        active.push_back(&target);
    }
    if (active.empty())
        throw std::invalid_argument("At least one enabled target is required");
    auto known = [this](const std::string& name) {
        for (const auto& t : targets)
            if (t.name == name) return true;
        return false;
    };
    for (const auto& initiator : initiators) {
        for (const auto& g : initiator.grants) {
            if (!known(g.target))
                throw std::invalid_argument("Access grant to unknown target: " + initiator.name + " -> " + g.target);
            for (const auto& [begin, end] : g.offsets)
                if (begin >= end)
                    throw std::invalid_argument("Empty access grant range: " + initiator.name + " -> " + g.target);
        }
        for (const auto& t : initiator.no_execute)
            if (!known(t))
                throw std::invalid_argument("No-execute rule for unknown target: " + initiator.name + " -> " + t);
    }
}
} // namespace fu3_bus
