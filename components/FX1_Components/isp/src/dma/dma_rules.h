// SPDX-License-Identifier: Apache-2.0
// DMA programming rules that do not depend on SystemC: burst formation
// (HAS Table 6-92) and start-time configuration checks (HAS Table 6-89).
#pragma once

#include <cstdint>
#include <vector>

namespace cdc::components::fx1_isp::dma {

// PARA_MAX_H/V_ACTIVE of MAS v1.07 (§6.25 parameters, Table 6-153): the
// external capacity including Bayer phase overscan. The active image derived
// by the Input Formatter is limited to 3840x2160; the DMA does not check that
// (§6.6.3.5: software validates it, V107-A9).
constexpr std::uint32_t max_h_active = 3842;
constexpr std::uint32_t max_v_active = 2162;
constexpr std::uint32_t max_active_w = 3840;
constexpr std::uint32_t max_active_h = 2160;
constexpr std::uint32_t min_active = 32;
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
// The Input Formatter's active image for an external size and COMMON_BAYER
// pattern: ((W - SH) & ~1, (H - SV) & ~1), MAS v1.07 §6.6.3.4.
struct extent {
   std::uint32_t width, height;
};
extent active_extent(std::uint32_t width, std::uint32_t height, std::uint32_t pattern);
// MAS v1.07 §6.6.3.4-6.6.3.5: the derived active image is 32x32..3840x2160.
// A software precondition (the driver checks it); the hardware does not.
bool active_geometry_legal(std::uint32_t width, std::uint32_t height, std::uint32_t pattern);

// Input working set; `stride` in bytes, one 16-bit container per sample.
struct idma_setup {
   std::uint64_t base;
   std::uint32_t stride;
   std::uint32_t width;
   std::uint32_t height;
   std::uint32_t depth = 0;  // COMMON_INPUT_FORMAT.DEPTH; 2/3 refuse the buffer (V107-A10)
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
   bool rgb = false;  // CH2: one packed RGB888 plane, line length 3W, no chroma plane
};
bool odma_setup_ok(const odma_setup &s, std::uint32_t beat_bytes);

}  // namespace cdc::components::fx1_isp::dma
