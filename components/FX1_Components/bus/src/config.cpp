#include "bus/config.h"
#include <stdexcept>

namespace bus {
namespace {
void validate_map(const std::vector<Region>& map, unsigned ports, const char* name) {
    if (!ports) throw std::invalid_argument(std::string(name)+": at least one port required");
    for (std::size_t i=0; i<map.size(); ++i) {
        const auto& r=map[i];
        if (r.begin>=r.end || r.port>=ports)
            throw std::invalid_argument(std::string(name)+": invalid region "+r.name);
        for (std::size_t j=0;j<i;++j)
            if (r.begin<map[j].end && map[j].begin<r.end)
                throw std::invalid_argument(std::string(name)+": overlapping regions");
    }
}
void validate_link(const std::vector<Region>& parent, unsigned port,
                   const std::vector<Region>& child, const char* name) {
    for (const auto& r:parent)
        if (r.port==port && r.translate)
            throw std::invalid_argument(std::string(name)+": inter-bus regions must keep global addresses");
    for (const auto& r:child) {
        // Require a single upstream rule to contain the child rule. The router
        // rejects transfers spanning rules, so adjacent windows are not equivalent.
        bool reachable=false;
        for (const auto& p:parent)
            if (p.port==port && p.begin<=r.begin && p.end>=r.end) reachable=true;
        if (!reachable)
            throw std::invalid_argument(std::string(name)+": unreachable child region "+r.name);
    }
}
}
void BusConfig::validate() const {
    validate_map(sysbus1,5,"SYSBUS_1");
    validate_map(sysbus0,3,"SYSBUS_0");
    validate_map(peribus1,pp1_ports,"PERIBUS_1");
    validate_map(peribus0,pp0_ports,"PERIBUS_0");
    validate_link(sysbus1,3,peribus1,"PP1");
    validate_link(sysbus1,4,sysbus0,"SB0");
    validate_link(sysbus0,2,peribus0,"PP0");
}
} // namespace bus
