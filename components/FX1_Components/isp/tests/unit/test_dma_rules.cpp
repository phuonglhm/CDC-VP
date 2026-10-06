// SPDX-License-Identifier: Apache-2.0
// Unit tests of the DMA burst formation (HAS Table 6-92) and start-time checks
// (HAS Table 6-89). Expected values are worked by hand from the tables.

#include <cstdint>
#include <vector>

#include "dma/dma_rules.h"
#include "fx1_check.h"

using namespace cdc::components::fx1_isp::dma;

namespace {

std::vector<std::uint32_t> sizes(const std::vector<burst> &bs) {
   std::vector<std::uint32_t> v;
   for (const burst &b : bs) {
      v.push_back(b.bytes);
   }
   return v;
}

void test_split_line() {
   // HAS §6.25.13: a 4K line of 7680 bytes at 64 beats x 16 bytes is 7 bursts of
   // 1024 and one of 512.
   auto b = split_line(0x10000, 7680, 64, 16);
   FX1_CHECK(sizes(b) == (std::vector<std::uint32_t>{1024, 1024, 1024, 1024, 1024, 1024, 1024, 512}));
   FX1_CHECK_EQ(b[4].addr, 0x11000);
   // 4 KB boundary: 0x0F00 -> 256 bytes up to 0x1000, then the rest.
   b = split_line(0x0F00, 512, 64, 16);
   FX1_CHECK(sizes(b) == (std::vector<std::uint32_t>{256, 256}));
   FX1_CHECK_EQ(b[1].addr, 0x1000);
   // A line length that is not a multiple of the beat (partial final beat).
   b = split_line(0x2000, 2686, 64, 16);
   FX1_CHECK(sizes(b) == (std::vector<std::uint32_t>{1024, 1024, 638}));
   // Maximum burst 256 beats x 16 = 4096, limited by the 4 KB boundary.
   b = split_line(0x3000, 8192, 256, 16);
   FX1_CHECK(sizes(b) == (std::vector<std::uint32_t>{4096, 4096}));
   // One-beat bursts.
   b = split_line(0x4000, 40, 1, 16);
   FX1_CHECK(sizes(b) == (std::vector<std::uint32_t>{16, 16, 8}));
   // 64-bit bus, 32 beats: 256-byte bursts.
   b = split_line(0x5000, 600, 32, 8);
   FX1_CHECK(sizes(b) == (std::vector<std::uint32_t>{256, 256, 88}));
   FX1_CHECK(split_line(0x6000, 0, 64, 16).empty());
}

void test_port_address() {
   FX1_CHECK_EQ(port_address(0xFFFF12345678ABCDull, 40), 0x12345678ABCDull & 0xFFFFFFFFFFull);
   FX1_CHECK_EQ(port_address(0x1FFFFFFFFull, 32), 0xFFFFFFFFull);
   FX1_CHECK_EQ(port_address(0xFFFFFFFFFFFFFFFFull, 64), 0xFFFFFFFFFFFFFFFFull);
}

void test_setup_checks() {
   const idma_setup ok{0x1000, 256, 64, 32};
   FX1_CHECK(idma_setup_ok(ok, 16));
   idma_setup s = ok;
   s.base += 8;
   FX1_CHECK(!idma_setup_ok(s, 16));  // base not a multiple of B
   FX1_CHECK(idma_setup_ok(s, 8));    // ... but fine for a 64-bit bus
   s = ok;
   s.stride = 248;
   FX1_CHECK(!idma_setup_ok(s, 16));  // stride not a multiple of B
   s = ok;
   s.stride = 112;
   FX1_CHECK(!idma_setup_ok(s, 16));  // stride < 2W
   s.stride = 128;
   FX1_CHECK(idma_setup_ok(s, 16));   // stride == 2W is legal
   s = ok;
   s.width = 63;
   FX1_CHECK(!idma_setup_ok(s, 16));
   s = ok;
   s.height = 31;
   FX1_CHECK(!idma_setup_ok(s, 16));
   s = {0, 7680, 3840, 2160};
   FX1_CHECK(idma_setup_ok(s, 16));   // 4K maximum
   s.width = 3842;
   s.stride = 7696;
   FX1_CHECK(!idma_setup_ok(s, 16));
   s = {0, 7680, 3840, 2162};
   FX1_CHECK(!idma_setup_ok(s, 16));
   s = {0, 16, 0, 2};
   FX1_CHECK(!idma_setup_ok(s, 16));  // W = 0 (e.g. ISP_EN trap on the output side)

   const odma_setup o{0x2000, 0x3000, 64, 64, 64, 32};
   FX1_CHECK(odma_setup_ok(o, 16));
   odma_setup t = o;
   t.uv_base += 4;
   FX1_CHECK(!odma_setup_ok(t, 16));
   t = o;
   t.y_stride = 48;
   FX1_CHECK(!odma_setup_ok(t, 16));  // < W
   t = o;
   t.uv_stride = 72;
   FX1_CHECK(!odma_setup_ok(t, 16));  // not a multiple of B
   t = o;
   t.width = 0;
   t.height = 0;
   FX1_CHECK(!odma_setup_ok(t, 16));  // DEC-20
}

}  // namespace

int main() {
   test_split_line();
   test_port_address();
   test_setup_checks();
   return fx1_test::summary("fx1_isp_test_dma_rules");
}
