// SPDX-License-Identifier: Apache-2.0
// Block-level comparison with the Python reference (tests/data/blocks,
// tools/gen_block_vectors.py) for corner cases the full pipeline cannot
// reach: EE gain order, 2DNR lower median, CNF inclusive thresholds, and
// whole-frame 2DNR / EE / CNF runs through their streaming implementations.
// Usage: fx1_isp_test_block_vectors <blocks dir>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "control/control_unit.h"
#include "fx1_check.h"
#include "pipeline/isp_pipeline.h"
#include "registers/csr_desc.h"

using namespace cdc::components::fx1_isp;
namespace fs = std::filesystem;

namespace {

template <class T>
std::vector<T> read_values(std::istream &is, std::size_t n) {
   std::vector<T> v(n);
   for (T &x : v) {
      std::uint64_t t = 0;
      is >> t;
      x = static_cast<T>(t);
   }
   return v;
}

void test_gain_chain(const fs::path &file) {
   std::ifstream f(file);
   std::uint32_t gl, ga, gc, gr, want;
   std::size_t n = 0, bad = 0;
   while (f >> gl >> ga >> gc >> gr >> want) {
      bad += pipe::ee_gain_chain(gl, ga, gc, gr) != want;
      ++n;
   }
   FX1_CHECK(n > 4000);
   FX1_CHECK_EQ_CTX(bad, 0, "ee_gain_chain mismatches");
}

void test_nr_variance(const fs::path &file) {
   std::ifstream f(file);
   std::string line;
   std::size_t n_cases = 0, bad = 0;
   while (std::getline(f, line)) {
      std::istringstream is(line);
      std::uint64_t n = 0;
      std::uint32_t want = 0, k = 0;
      is >> n >> want >> k;
      std::array<std::uint32_t, 256> hist{};
      for (std::uint32_t i = 0; i < k; ++i) {
         std::uint32_t b = 0, c = 0;
         is >> b >> c;
         hist[b] = c;
      }
      bad += pipe::nr_variance(hist, n) != want;
      ++n_cases;
   }
   FX1_CHECK(n_cases > 70);
   FX1_CHECK_EQ_CTX(bad, 0, "nr_variance mismatches");
}

void test_cnf(const fs::path &file) {
   std::ifstream f(file);
   std::uint32_t w, h, tc, ty;
   std::size_t frames = 0;
   while (f >> w >> h >> tc >> ty) {
      const std::vector<std::uint16_t> in = read_values<std::uint16_t>(f, 3u * w * h);
      const std::vector<std::uint16_t> want = read_values<std::uint16_t>(f, 2u * w * h);
      std::vector<pipe::yuv> img(std::size_t{w} * h);
      for (std::size_t i = 0; i < img.size(); ++i) {
         img[i] = {in[3 * i], in[3 * i + 1], in[3 * i + 2]};
      }
      const auto out = pipe::run_cnf(img, w, h, std::min<std::uint32_t>(tc, 511), ty);
      std::size_t bad = 0;
      for (std::size_t i = 0; i < out.size(); ++i) {
         bad += out[i].u != want[2 * i] || out[i].v != want[2 * i + 1];
      }
      FX1_CHECK_EQ_CTX(bad, 0, "cnf tc=" + std::to_string(tc) + " ty=" + std::to_string(ty));
      ++frames;
   }
   FX1_CHECK(frames == 5);
}

void test_nr2d(const fs::path &file) {
   std::ifstream f(file);
   std::uint32_t w, h, vin, vout;
   std::size_t frames = 0;
   while (f >> w >> h >> vin >> vout) {
      const auto in = read_values<std::uint16_t>(f, std::size_t{w} * h);
      const auto want = read_values<std::uint16_t>(f, std::size_t{w} * h);
      std::uint32_t var = 0;
      const auto out = pipe::run_nr2d(in, w, h, vin, var);
      const std::string ctx = "nr2d " + std::to_string(w) + "x" + std::to_string(h) + " v=" + std::to_string(vin);
      FX1_CHECK_EQ_CTX(out == want ? 1 : 0, 1, ctx);
      FX1_CHECK_EQ_CTX(var, vout, ctx + " variance");
      ++frames;
   }
   FX1_CHECK(frames == 7);
}

void test_ee(const fs::path &file) {
   std::ifstream f(file);
   std::uint32_t w, h, nwrites;
   std::size_t frames = 0;
   while (f >> w >> h >> nwrites) {
      control_unit cu;
      for (std::uint32_t i = 0; i < nwrites; ++i) {
         std::string name;
         std::uint64_t value = 0;
         f >> name >> value;
         for (std::size_t r = 0; r < csr::num_registers; ++r) {
            if (name == csr::registers[r].name) {
               cu.csr_write(csr::registers[r].offset, static_cast<std::uint32_t>(value), 0xFFFFFFFFu, i + 1);
            }
         }
      }
      const auto in = read_values<std::uint16_t>(f, std::size_t{w} * h);
      const auto want = read_values<std::uint16_t>(f, std::size_t{w} * h);
      const auto out = pipe::run_ee(in, w, h, cu);
      std::size_t bad = 0;
      for (std::size_t i = 0; i < out.size(); ++i) {
         bad += out[i] != want[i];
      }
      FX1_CHECK_EQ_CTX(bad, 0, "ee frame " + std::to_string(frames));
      ++frames;
   }
   FX1_CHECK(frames == 4);
}

}  // namespace

int main(int argc, char **argv) {
   if (argc < 2) {
      std::cerr << "usage: " << argv[0] << " <blocks dir>\n";
      return 2;
   }
   const fs::path d = argv[1];
   test_gain_chain(d / "ee_gain_chain.txt");
   test_nr_variance(d / "nr_variance.txt");
   test_cnf(d / "cnf_frames.txt");
   test_nr2d(d / "nr2d_frames.txt");
   test_ee(d / "ee_frames.txt");
   return fx1_test::summary("fx1_isp_test_block_vectors");
}
