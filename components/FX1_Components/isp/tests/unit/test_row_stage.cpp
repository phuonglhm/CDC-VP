// SPDX-License-Identifier: Apache-2.0
// Unit tests of the row-streaming helpers: the streamed vertical window must
// see exactly the rows a whole-frame computation would.

#include <cstdint>
#include <string>
#include <vector>

#include "fx1_check.h"
#include "pipeline/row_stage.h"

using namespace cdc::components::fx1_isp::pipe;

namespace {

void test_border_index() {
   // n = 5: replicate, reflect-101, reflect.
   FX1_CHECK_EQ(border_index(-2, 5, border::replicate), 0);
   FX1_CHECK_EQ(border_index(6, 5, border::replicate), 4);
   FX1_CHECK_EQ(border_index(-1, 5, border::mirror), 1);
   FX1_CHECK_EQ(border_index(-2, 5, border::mirror), 2);
   FX1_CHECK_EQ(border_index(5, 5, border::mirror), 3);
   FX1_CHECK_EQ(border_index(6, 5, border::mirror), 2);
   FX1_CHECK_EQ(border_index(-1, 5, border::mirror_edge), 0);
   FX1_CHECK_EQ(border_index(-2, 5, border::mirror_edge), 1);
   FX1_CHECK_EQ(border_index(5, 5, border::mirror_edge), 4);
   FX1_CHECK_EQ(border_index(6, 5, border::mirror_edge), 3);
   // Degenerate extents and far reflections stay in range.
   FX1_CHECK_EQ(border_index(-3, 1, border::mirror), 0);
   FX1_CHECK_EQ(border_index(7, 2, border::mirror), 1);
   FX1_CHECK_EQ(border_index(-9, 3, border::mirror_edge), 2);
}

void test_window_matches_frame() {
   const border modes[] = {border::replicate, border::mirror, border::mirror_edge};
   for (border b : modes) {
      for (unsigned r = 0; r <= 3; ++r) {
         for (std::uint32_t h = 1; h <= 12; ++h) {
            if (b == border::mirror && h <= r) {
               continue;  // reflect-101 needs more rows than the radius
            }
            row_window<row<int>> w(r, b);
            w.begin(h);
            std::uint32_t emitted = 0;
            bool ok = true;
            auto drain = [&](bool ended) {
               while (w.ready(ended)) {
                  const std::uint32_t y = w.centre();
                  for (int dy = -static_cast<int>(r); dy <= static_cast<int>(r); ++dy) {
                     const auto want = border_index(static_cast<std::int64_t>(y) + dy, h, b);
                     ok = ok && w.at(dy)[0] == static_cast<int>(want);
                  }
                  ok = ok && y == emitted;
                  ++emitted;
                  w.advance();
               }
            };
            for (std::uint32_t y = 0; y < h; ++y) {
               w.add(row<int>{static_cast<int>(y)});
               drain(false);
               // Latency: row y - r is out as soon as row y is in.
               ok = ok && emitted == (y + 1 > r ? y + 1 - r : 0);
            }
            drain(true);
            FX1_CHECK_EQ_CTX(emitted, h, "r=" + std::to_string(r) + " h=" + std::to_string(h));
            FX1_CHECK(ok);
         }
      }
   }
}

}  // namespace

int main() {
   test_border_index();
   test_window_matches_frame();
   return fx1_test::summary("fx1_isp_test_row_stage");
}
