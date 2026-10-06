// SPDX-License-Identifier: Apache-2.0
// DMA programming rules that do not depend on SystemC: burst formation
// (HAS Table 6-92) and start-time configuration checks (HAS Table 6-89).
#pragma once

#include <cstdint>
#include <vector>

namespace cdc::components::fx1_isp::dma {

constexpr std::uint32_t max_h_active = 3840;  // PARA_MAX_H_ACTIVE
constexpr std::uint32_t max_v_active = 2160;  // PARA_MAX_V_ACTIVE
constexpr unsigned num_buffers = 4;           // SPEC-08

struct burst {
   std::uint64_t addr;
   std::uint32_t bytes;
};

// Splits one line into INCR bursts: at most `max_beats` beats of
// `beat_bytes`, never crossing a 4 KB boundary, never merged across lines.
std::vector<burst> split_line(std::uint64_t addr, std::uint32_t bytes, std::uint32_t max_beats,
                              std::uint32_t beat_bytes);

// Truncates an address to the master's PARA_AXI_ADDR_WIDTH.
inline std::uint64_t port_address(std::uint64_t addr, unsigned addr_bits) {
   return addr_bits >= 64 ? addr : addr & ((std::uint64_t{1} << addr_bits) - 1u);
}

bool frame_geometry_ok(std::uint32_t width, std::uint32_t height);

// Input working set; `stride` in bytes, one 16-bit container per sample.
struct idma_setup {
   std::uint64_t base;
   std::uint32_t stride;
   std::uint32_t width;
   std::uint32_t height;
};
bool idma_setup_ok(const idma_setup &s, std::uint32_t beat_bytes);

// Output working set: NV12, luma W bytes per line, chroma W bytes per line
// over H/2 lines.
struct odma_setup {
   std::uint64_t y_base;
   std::uint64_t uv_base;
   std::uint32_t y_stride;
   std::uint32_t uv_stride;
   std::uint32_t width;
   std::uint32_t height;
};
bool odma_setup_ok(const odma_setup &s, std::uint32_t beat_bytes);

}  // namespace cdc::components::fx1_isp::dma
