#include "bus/config.h"
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace bus {
BusConfig BusConfig::fx1() {
    BusConfig c;
    c.initiators = {{"CPU1"},{"CPU2"},{"SYS_DMA"},{"ISP_IDMA"},{"ISP_ODMA"},
                    {"NPU_DMA"},{"H264_H265_DMA"},{"ETH_DMA"}};
    c.targets = {
        {"BootROM",0x00000000ULL,2ULL<<20,TargetPath::SysBus1Axi,true},
        {"CLINT",0x02000000ULL,64ULL<<10,TargetPath::Peribus0Apb,true},
        {"PLIC",0x0C000000ULL,16ULL<<20,TargetPath::Peribus0Apb,true},
        {"UART",0x10000000ULL,64ULL<<10,TargetPath::Peribus0Apb,true},
        {"MEMCTL_DDR",0x80000000ULL,512ULL<<20,TargetPath::SysBus1Axi,true},
        {"SRAM",0,0,TargetPath::SysBus1Axi,false},
        {"CPU1_IRAM",0,0,TargetPath::SysBus1Axi,false},
        {"CPU2_IRAM",0,0,TargetPath::SysBus1Axi,false},
        {"ISP_CSR",0,0,TargetPath::Peribus1Apb,false},
        {"NPU_CSR",0,0,TargetPath::Peribus1Apb,false},
        {"H264_H265_CSR",0,0,TargetPath::Peribus1Apb,false},
        {"ETH_CSR",0,0,TargetPath::Peribus1Apb,false},
        {"MIPI_CSR",0,0,TargetPath::Peribus1Apb,false},
        {"SYS_DMA_CSR",0,0,TargetPath::Peribus0Apb,false}
    };
    return c;
}
void BusConfig::validate() const {
    if (axi_data_width != 32)
        throw std::invalid_argument("This bus supports 32-bit data only");
    if (arbitration != ArbitrationPolicy::Fifo)
        throw std::invalid_argument("Only FIFO arbitration is supported");
    if (!axi_address_width || axi_address_width > 64)
        throw std::invalid_argument("AXI address width must be in [1,64]");
    if (initiators.empty())
        throw std::invalid_argument("At least one initiator is required");

    std::unordered_set<std::string> names;
    for (const auto& initiator : initiators) {
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
        if (!target.enabled) continue;
        if (!target.size || target.base > max_address - target.size)
            throw std::invalid_argument("Invalid target range: " + target.name);
        const auto end = target.base + target.size;
        const bool apb = target.path == TargetPath::Peribus0Apb ||
                         target.path == TargetPath::Peribus1Apb;
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
}
} // namespace bus
