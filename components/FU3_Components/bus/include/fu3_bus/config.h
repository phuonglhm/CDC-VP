#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fu3_bus {
struct Region { std::string name; std::uint64_t begin, end; unsigned port; bool translate; };
enum class TargetPath { SysBus1Axi, SysBus0Axi, Peribus0Apb, Peribus1Apb };
enum class ArbitrationPolicy { Fifo };
// ReadExecute: functional masters may read and fetch, never write (QSPI XIP aperture).
enum class AccessPolicy { Open, BootRom, Cva6Program, DataOnly, IbexProgram, DebugControl, ReadExecute };
// One right of an access profile: `target` (by name), the access kinds and,
// when `offsets` is not empty, the local [begin, end) ranges the whole access
// must lie in.
struct AccessGrant {
    std::string target;
    bool read = false, write = false, execute = false;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> offsets{};
};
struct InitiatorConfig {
    std::string name;
    unsigned outstanding = 1;
    bool separate_read_write = false;
    bool require_access = false;
    unsigned max_beats = 256; // VP transaction bound; DMA overrides to 16.
    bool word_increment_only = false; // DBG interface, Table 7-1.
    // Access profile (platform policy): with grants_only, this initiator reaches
    // only what a grant allows; the target policies still apply on top.
    bool grants_only = false;
    std::vector<AccessGrant> grants{};
    // Targets this initiator may not fetch instructions from.
    std::vector<std::string> no_execute{};
};
struct TargetConfig {
    std::string name;
    std::uint64_t base = 0, size = 0;
    TargetPath path = TargetPath::SysBus1Axi;
    bool enabled = false;
    AccessPolicy access = AccessPolicy::Open;
    unsigned outstanding = 1;
    bool word_interface = false; // Adapt subwords to aligned 32-bit beats with lane enables.
    bool preserve_burst = false; // Opt-in endpoint contract: forward the original Access and payload.
    // Side-effect words that must receive a complete word, never a widened subword.
    // Entries are local aligned offsets; every effective byte enable must be set.
    std::vector<std::uint64_t> strict_word_offsets{};
    bool strict_word_access = false; // Complete endpoint requires aligned full-word beats.
    bool non_executable = false;
    std::uint64_t debug_offset = 0; // Nonzero: remaining upper partition is CPU debug code.
};
struct BusConfig {
    // Spec revision the address map follows; reported by platforms with the map.
    std::string revision;
    // All sockets and both buses use 32-bit data. Reject unsupported widths.
    unsigned axi_data_width = 32, axi_address_width = 32;
    unsigned router_latency_ns = 2, apb_cycle_ns = 10;
    unsigned axi_cycle_ns = 2; // One-stage AP delay; clock values are VP parameters.
    ArbitrationPolicy arbitration = ArbitrationPolicy::Fifo;
    std::vector<InitiatorConfig> initiators;
    std::vector<TargetConfig> targets;
    static BusConfig fu3();
    void validate() const;
};
} // namespace fu3_bus
