#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace bus {
struct Region { std::string name; std::uint64_t begin, end; unsigned port; bool translate; };
enum class TargetPath { SysBus1Axi, SysBus0Axi, Peribus0Apb, Peribus1Apb };
enum class ArbitrationPolicy { Fifo };
struct InitiatorConfig { std::string name; };
struct TargetConfig {
    std::string name;
    std::uint64_t base = 0, size = 0;
    TargetPath path = TargetPath::SysBus1Axi;
    bool enabled = false;
};
struct BusConfig {
    // All sockets and both buses use 32-bit data. Reject unsupported widths.
    unsigned axi_data_width = 32, axi_address_width = 32;
    unsigned router_latency_ns = 2, apb_cycle_ns = 10;
    ArbitrationPolicy arbitration = ArbitrationPolicy::Fifo;
    std::vector<InitiatorConfig> initiators;
    std::vector<TargetConfig> targets;
    static BusConfig fx1();
    void validate() const;
};
} // namespace bus
