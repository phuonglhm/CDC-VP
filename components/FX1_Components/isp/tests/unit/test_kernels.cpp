// Directed V107-5 kernel checks with expected values derived from the MAS
// v1.07 formulas, independently of the Python reference: literal endpoints
// (black, white, primaries, ties, saturation) and identities the formulas
// imply (blend endpoints, equivalent noise models).

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "fx1_check.h"
#include "pipeline/isp_pipeline.h"

using namespace cdc::components::fx1_isp::pipe;

namespace {

yuv csc(const pipeline_config &c, std::uint16_t r, std::uint16_t g, std::uint16_t b) {
   return csc_pixel(c, rgb{r, g, b});
}

void check_yuv(const yuv &got, unsigned y, unsigned u, unsigned v, const std::string &what) {
   FX1_CHECK_EQ_CTX(got.y, y, what + " Y");
   FX1_CHECK_EQ_CTX(got.u, u, what + " U");
   FX1_CHECK_EQ_CTX(got.v, v, what + " V");
}

// §6.15.3.3, Table 6-92: range and standard select RSY/RSCB/RSCR/OY on the
// 12-bit RGB input; R(n, 2^32) = floor(n / 2^32 + 1/2).
void test_csc_range() {
   struct point {
      const char *name;
      std::uint16_t r, g, b;
      unsigned y[4], u[4], v[4];  // full 601, full 709, limited 601, limited 709
   };
   const point pts[] = {
       {"white", 4095, 4095, 4095, {255, 255, 235, 235}, {128, 128, 128, 128}, {128, 128, 128, 128}},
       {"black", 0, 0, 0, {0, 0, 16, 16}, {128, 128, 128, 128}, {128, 128, 128, 128}},
       {"grey", 2048, 2048, 2048, {128, 128, 126, 126}, {128, 128, 128, 128}, {128, 128, 128, 128}},
       {"blue", 0, 0, 4095, {29, 18, 41, 32}, {255, 255, 240, 240}, {107, 116, 110, 118}},
       {"red", 4095, 0, 0, {76, 54, 81, 63}, {85, 99, 90, 102}, {255, 255, 240, 240}},
       {"green", 0, 4095, 0, {150, 182, 145, 173}, {44, 30, 54, 42}, {21, 12, 34, 26}},
   };
   for (const point &p : pts) {
      for (unsigned i = 0; i < 4; ++i) {
         pipeline_config c;
         c.csc_full_range = i < 2;
         c.csc_std = i & 1u;
         check_yuv(csc(c, p.r, p.g, p.b), p.y[i], p.u[i], p.v[i],
                   std::string(p.name) + (i < 2 ? " full " : " limited ") + (c.csc_std ? "709" : "601"));
      }
   }
   pipeline_config legacy;  // FULL_RANGE resets to 1 and legacy mode uses the reset (V107-D22)
   check_yuv(csc(legacy, 4095, 4095, 4095), 255, 128, 128, "reset config white");
}

// The look (§6.15.3.3): Y1 = sat(P + R(C (Y0 - P), 2^8) + B), U1/V1 = sat(128 +
// R(M (U0 - 128, V0 - 128), 2^14)); P = 0 full range, 16 limited.
void test_csc_look() {
   pipeline_config c;
   c.csc_look = true;
   // Identity look = bypass, in both ranges.
   for (bool full : {true, false}) {
      pipeline_config off;
      off.csc_full_range = c.csc_full_range = full;
      for (const rgb &p : {rgb{0, 0, 0}, rgb{4095, 4095, 4095}, rgb{0, 0, 4095}, rgb{4095, 0, 0}, rgb{48, 0, 0}}) {
         const yuv a = csc_pixel(c, p), b = csc_pixel(off, p);
         FX1_CHECK(a.y == b.y && a.u == b.u && a.v == b.v);
      }
   }
   // Ties go toward +infinity. Full-range 601 RGB (48, 0, 0) gives Y0 = 1,
   // U0 = 127, V0 = 129: 0.5 * -1 -> 0 and 0.5 * +1 -> +1; C = 0.5 on Y0 = 1 -> 1.
   c.csc_full_range = true;
   c.csc_m = {0x2000, 0, 0, 0x2000};
   c.csc_contrast = 0x80;
   check_yuv(csc(c, 48, 0, 0), 1, 128, 129, "look ties");
   // Limited range keeps the pedestal: white Y0 = 235 -> 16 + R(0.5 * 219) = 126.
   c.csc_full_range = false;
   c.csc_m = {0x4000, 0, 0, 0x4000};
   check_yuv(csc(c, 4095, 4095, 4095), 126, 128, 128, "limited pedestal");
   FX1_CHECK_EQ(csc(c, 0, 0, 0).y, 16u);  // black stays on the pedestal
   // Saturation without wrap: Y and both chroma ends.
   c.csc_full_range = true;
   c.csc_contrast = 0xFFF;
   c.csc_brightness = 511;
   FX1_CHECK_EQ(csc(c, 4095, 4095, 4095).y, 255u);
   c.csc_contrast = 0x100;
   c.csc_brightness = -512;
   FX1_CHECK_EQ(csc(c, 4095, 4095, 4095).y, 0u);
   c.csc_brightness = -5;
   FX1_CHECK_EQ(csc(c, 0, 0, 0).y, 0u);
   c.csc_brightness = 0;
   c.csc_m = {0x1FFFF, 0, 0, -0x20000};  // UU = +7.99..., VV = -8 (signed Q4.14 ends)
   const yuv blue = csc(c, 0, 0, 4095);  // U0 = 255, V0 = 107
   FX1_CHECK_EQ(blue.u, 255u);
   FX1_CHECK_EQ(blue.v, 255u);  // -8 * -21 = +168
   c.csc_m = {-0x20000, 0, 0, 0x1FFFF};
   const yuv blue2 = csc(c, 0, 0, 4095);
   FX1_CHECK_EQ(blue2.u, 0u);
   FX1_CHECK_EQ(blue2.v, 0u);
   // Cross terms: U1 = 128 + R((UU dU + UV dV) / 2^14) with dU = 127, dV = -21.
   c.csc_m = {0, 0x4000, 0x4000, 0};  // swap U and V
   check_yuv(csc(c, 0, 0, 4095), 29, 107, 255, "look swap");
}

// §6.10.3.6: Gr and Gb gains are independent; floor, saturate to 12 bits.
void test_wb_gb() {
   pipeline_config c;
   c.wb_en = true;
   c.wb_gain = {0x100, 0x180, 0x200};
   c.wb_gain_gb = 0x080;
   FX1_CHECK_EQ(wb_pixel(c, 1000, 0), 1000u);  // R
   FX1_CHECK_EQ(wb_pixel(c, 1000, 1), 1500u);  // Gr
   FX1_CHECK_EQ(wb_pixel(c, 1000, 2), 500u);   // Gb
   FX1_CHECK_EQ(wb_pixel(c, 1000, 3), 2000u);  // B
   FX1_CHECK_EQ(wb_pixel(c, 3, 2), 1u);        // floor(1.5)
   c.wb_gain_gb = 0xFFF;
   FX1_CHECK_EQ(wb_pixel(c, 4095, 2), 4095u);  // saturates, no wrap
}

std::vector<std::uint16_t> noisy_y(std::uint32_t w, std::uint32_t h) {
   std::vector<std::uint16_t> y(std::size_t{w} * h);
   std::uint32_t s = 12345;
   for (std::size_t i = 0; i < y.size(); ++i) {
      s = s * 1103515245u + 12345u;
      const std::uint32_t x = static_cast<std::uint32_t>(i % w);
      y[i] = static_cast<std::uint16_t>(std::min<std::uint32_t>(255, (x * 3) % 200 + ((s >> 16) % 56)));
   }
   return y;
}

std::vector<std::uint16_t> nr(const pipeline_config &c, const std::vector<std::uint16_t> &y, std::uint32_t w,
                              std::uint32_t h) {
   std::uint32_t var = 0;
   return run_nr2d(c, y, w, h, var);
}

// §6.17.3.10: blend endpoints and the equivalences of the noise models.
void test_nr_models() {
   const std::uint32_t w = 34, h = 18;
   const std::vector<std::uint16_t> y = noisy_y(w, h);
   pipeline_config base;
   base.nr_en = true;
   base.nr_mode = 1;
   base.nr_manual = 300;
   const std::vector<std::uint16_t> ref = nr(base, y, w, h);
   FX1_CHECK(ref != y);  // the filter does something on this input
   // Blend 0 returns the input exactly; 128 is the half-up average of both ends.
   pipeline_config c = base;
   c.nr_blend = 0;
   FX1_CHECK(nr(c, y, w, h) == y);
   c.nr_blend = 128;
   const std::vector<std::uint16_t> half = nr(c, y, w, h);
   std::size_t bad = 0;
   for (std::size_t i = 0; i < y.size(); ++i) {
      bad += half[i] != ((128u * y[i] + 128u * ref[i] + 128u) >> 8);
   }
   FX1_CHECK_EQ(bad, 0u);
   // MODE 0: the estimate when one exists, else SEED; MODE 1: VARIANCE.
   c = base;
   c.nr_mode = 0;
   c.nr_estimate_valid = true;
   c.nr_var = 300;
   c.nr_seed = 9999;
   FX1_CHECK(nr(c, y, w, h) == ref);
   c.nr_estimate_valid = false;
   c.nr_var = 9999;
   c.nr_seed = 300;
   FX1_CHECK(nr(c, y, w, h) == ref);
   // MODE 2 with a constant curve.
   c = base;
   c.nr_mode = 2;
   c.nr_manual = 9999;
   c.nr_curve.fill(300);
   FX1_CHECK(nr(c, y, w, h) == ref);
   // SCALE (UQ8.8): 150 * 2.0 = 300 exactly; limits clamp after the scale.
   c = base;
   c.nr_manual = 150;
   c.nr_scale = 0x200;
   FX1_CHECK(nr(c, y, w, h) == ref);
   c = base;
   c.nr_manual = 5000;
   c.nr_max = 300;
   FX1_CHECK(nr(c, y, w, h) == ref);
   c = base;
   c.nr_manual = 7;
   c.nr_min = 300;
   FX1_CHECK(nr(c, y, w, h) == ref);
   // Band scales of 2.0 on all four groups = doubled variance.
   c = base;
   c.nr_manual = 150;
   c.nr_band_scale = {0x200, 0x200, 0x200, 0x200};
   FX1_CHECK(nr(c, y, w, h) == ref);
   // Each band has its own scale: 0 leaves that band unfiltered (zero
   // variance), which changes the output (at 300 the detail bands are shrunk).
   for (unsigned b = 0; b < 4; ++b) {
      c = base;
      c.nr_band_scale[b] = 0;
      FX1_CHECK_EQ_CTX(nr(c, y, w, h) != ref, true, "band " + std::to_string(b));
   }
   // MEDIAN_EN = 0 on a frame that the median leaves unchanged (a ramp)
   // changes nothing; on the noisy frame it does.
   std::vector<std::uint16_t> ramp(std::size_t{w} * h);
   for (std::size_t i = 0; i < ramp.size(); ++i) {
      ramp[i] = static_cast<std::uint16_t>(4 * (i % w));
   }
   c = base;
   c.nr_median = false;
   FX1_CHECK(nr(c, ramp, w, h) == nr(base, ramp, w, h));
   FX1_CHECK(nr(c, y, w, h) != ref);
}

// §6.19.3.6: the CNF output blend with the aligned original.
void test_cnf_blend() {
   const std::uint32_t w = 20, h = 12;
   std::vector<yuv> img(std::size_t{w} * h);
   std::uint32_t s = 7;
   for (yuv &p : img) {
      s = s * 1103515245u + 12345u;
      p = yuv{static_cast<std::uint16_t>(100 + (s >> 28)), static_cast<std::uint16_t>((s >> 8) & 0xFF),
              static_cast<std::uint16_t>((s >> 16) & 0xFF)};
   }
   pipeline_config c;
   c.cnf_en = true;
   c.cnf_chroma_th = 200;
   c.cnf_luma_th = 30;
   const std::vector<uv_pair> full = run_cnf(c, img, w, h);
   std::size_t changed = 0;
   for (std::size_t i = 0; i < img.size(); ++i) {
      changed += full[i].u != img[i].u || full[i].v != img[i].v;
   }
   FX1_CHECK(changed > 0);
   c.cnf_blend = 0;
   const std::vector<uv_pair> none = run_cnf(c, img, w, h);
   c.cnf_blend = 64;
   const std::vector<uv_pair> quarter = run_cnf(c, img, w, h);
   std::size_t bad = 0;
   for (std::size_t i = 0; i < img.size(); ++i) {
      bad += none[i].u != img[i].u || none[i].v != img[i].v;
      bad += quarter[i].u != ((192u * img[i].u + 64u * full[i].u + 128u) >> 8);
      bad += quarter[i].v != ((192u * img[i].v + 64u * full[i].v + 128u) >> 8);
   }
   FX1_CHECK_EQ(bad, 0u);
}

}  // namespace

// CH2 inverse CSC (MAS v1.07 §6.15.3, Table 6-89): literals from the MAS
// equations with exact integers, R(n, 65536) = floor((n + 32768) / 65536),
// output clamp only (limited-range inputs outside 16..235/240 are not
// pre-clamped). Endpoint codes 0, 16, 235, 240, 255 and saturated colours.
// Worked check, limited BT.601, (128, 16, 240), G: L = 112 * 76309 =
// 8546608, -CbG * D = +2875600, -CrG * E = -5967248, sum 5454960 -> 83.
void test_rgb888() {
   struct c {
      std::uint32_t std;
      bool full;
      std::uint16_t y, u, v;
      std::array<unsigned, 3> rgb;
   };
   static const c cases[] = {
      {0u, true, 0, 128, 128, {0u, 0u, 0u}},
      {0u, true, 255, 128, 128, {255u, 255u, 255u}},
      {0u, true, 16, 128, 128, {16u, 16u, 16u}},
      {0u, true, 235, 128, 128, {235u, 235u, 235u}},
      {0u, true, 128, 16, 240, {255u, 87u, 0u}},
      {0u, true, 128, 240, 16, {0u, 169u, 255u}},
      {0u, true, 76, 85, 255, {254u, 0u, 0u}},
      {0u, true, 16, 16, 16, {0u, 135u, 0u}},
      {0u, true, 255, 255, 255, {255u, 121u, 255u}},
      {0u, true, 81, 90, 240, {238u, 14u, 14u}},
      {0u, true, 200, 0, 0, {21u, 255u, 0u}},
      {1u, true, 0, 128, 128, {0u, 0u, 0u}},
      {1u, true, 255, 128, 128, {255u, 255u, 255u}},
      {1u, true, 16, 128, 128, {16u, 16u, 16u}},
      {1u, true, 235, 128, 128, {235u, 235u, 235u}},
      {1u, true, 128, 16, 240, {255u, 97u, 0u}},
      {1u, true, 128, 240, 16, {0u, 159u, 255u}},
      {1u, true, 76, 85, 255, {255u, 25u, 0u}},
      {1u, true, 16, 16, 16, {0u, 89u, 0u}},
      {1u, true, 255, 255, 255, {255u, 172u, 255u}},
      {1u, true, 81, 90, 240, {255u, 36u, 10u}},
      {1u, true, 200, 0, 0, {0u, 255u, 0u}},
      {0u, false, 0, 128, 128, {0u, 0u, 0u}},
      {0u, false, 255, 128, 128, {255u, 255u, 255u}},
      {0u, false, 16, 128, 128, {0u, 0u, 0u}},
      {0u, false, 235, 128, 128, {255u, 255u, 255u}},
      {0u, false, 128, 16, 240, {255u, 83u, 0u}},
      {0u, false, 128, 240, 16, {0u, 178u, 255u}},
      {0u, false, 76, 85, 255, {255u, 0u, 0u}},
      {0u, false, 16, 16, 16, {0u, 135u, 0u}},
      {0u, false, 255, 255, 255, {255u, 125u, 255u}},
      {0u, false, 81, 90, 240, {254u, 0u, 0u}},
      {0u, false, 200, 0, 0, {10u, 255u, 0u}},
      {1u, false, 0, 128, 128, {0u, 0u, 0u}},
      {1u, false, 255, 128, 128, {255u, 255u, 255u}},
      {1u, false, 16, 128, 128, {0u, 0u, 0u}},
      {1u, false, 235, 128, 128, {255u, 255u, 255u}},
      {1u, false, 128, 16, 240, {255u, 95u, 0u}},
      {1u, false, 128, 240, 16, {0u, 166u, 255u}},
      {1u, false, 76, 85, 255, {255u, 11u, 0u}},
      {1u, false, 16, 16, 16, {0u, 84u, 0u}},
      {1u, false, 255, 255, 255, {255u, 184u, 255u}},
      {1u, false, 81, 90, 240, {255u, 24u, 0u}},
      {1u, false, 200, 0, 0, {0u, 255u, 0u}},
   };
   for (const c &k : cases) {
      const rgb8 o = rgb888_pixel(k.std, k.full, k.y, k.u, k.v);
      const std::string ctx = std::string(k.full ? "full" : "limited") + (k.std ? " 709 " : " 601 ") +
                              std::to_string(k.y) + "," + std::to_string(k.u) + "," + std::to_string(k.v);
      FX1_CHECK_EQ_CTX(o.r, k.rgb[0], ctx + " R");
      FX1_CHECK_EQ_CTX(o.g, k.rgb[1], ctx + " G");
      FX1_CHECK_EQ_CTX(o.b, k.rgb[2], ctx + " B");
   }
}

int main() {
   test_rgb888();
   test_csc_range();
   test_csc_look();
   test_wb_gb();
   test_nr_models();
   test_cnf_blend();
   return fx1_test::summary("fx1_isp_test_kernels");
}
