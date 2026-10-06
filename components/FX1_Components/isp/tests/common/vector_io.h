// SPDX-License-Identifier: Apache-2.0
// Readers for the committed vector files (tools/gen_vectors.py): CSR write
// lists, between-frame writes and the expected-read script.
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fx1_test {

struct csr_line {
   std::uint32_t frame = 0;  // frames.csrw: written before the SOF of this frame
   bool set = false;         // expected_stats.txt: a write, not an expected read
   std::string reg;
   std::uint32_t value = 0;
};

// profile.csrw: "<REG> <value>".
inline std::vector<csr_line> read_profile(const std::filesystem::path &p) {
   std::vector<csr_line> out;
   std::ifstream f(p);
   std::string line;
   while (std::getline(f, line)) {
      std::istringstream is(line.substr(0, line.find('#')));
      csr_line l;
      std::string v;
      if (is >> l.reg >> v) {
         l.value = static_cast<std::uint32_t>(std::stoul(v, nullptr, 0));
         out.push_back(l);
      }
   }
   return out;
}

// frames.csrw (optional): "<frame> <REG> <value>".
inline std::vector<csr_line> read_frame_writes(const std::filesystem::path &p) {
   std::vector<csr_line> out;
   std::ifstream f(p);
   std::string line;
   while (std::getline(f, line)) {
      std::istringstream is(line.substr(0, line.find('#')));
      csr_line l;
      std::string v;
      if (is >> l.frame >> l.reg >> v) {
         l.value = static_cast<std::uint32_t>(std::stoul(v, nullptr, 0));
         out.push_back(l);
      }
   }
   return out;
}

// expected_stats.txt: "<REG> <value>" reads and "SET <REG> <value>" writes.
inline std::vector<csr_line> read_expected(const std::filesystem::path &p) {
   std::vector<csr_line> out;
   std::ifstream f(p);
   std::string line;
   while (std::getline(f, line)) {
      std::istringstream is(line);
      csr_line l;
      std::string first, v;
      if (!(is >> first >> v)) {
         continue;
      }
      if (first == "SET") {
         l.set = true;
         l.reg = v;
         is >> v;
      } else {
         l.reg = first;
      }
      l.value = static_cast<std::uint32_t>(std::stoull(v, nullptr, 0));
      out.push_back(l);
   }
   return out;
}

}  // namespace fx1_test
