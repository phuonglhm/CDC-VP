// SPDX-License-Identifier: Apache-2.0
// Minimal self-contained check macros for the FX1 ISP tests (no framework).
#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace fx1_test {

inline int &failures() {
   static int n = 0;
   return n;
}

inline int &checks() {
   static int n = 0;
   return n;
}

inline std::string hex(std::uint64_t v) {
   std::ostringstream os;
   os << "0x" << std::hex << std::uppercase << v;
   return os.str();
}

inline void report(bool ok, const char *expr, const char *file, int line, const std::string &detail) {
   ++checks();
   if (!ok) {
      ++failures();
      std::cerr << file << ":" << line << ": CHECK FAILED: " << expr;
      if (!detail.empty()) {
         std::cerr << " (" << detail << ")";
      }
      std::cerr << "\n";
   }
}

inline int summary(const char *suite) {
   std::cout << suite << ": " << checks() << " checks, " << failures() << " failures\n";
   return failures() == 0 ? 0 : 1;
}

}  // namespace fx1_test

#define FX1_CHECK(cond) ::fx1_test::report(static_cast<bool>(cond), #cond, __FILE__, __LINE__, "")

#define FX1_CHECK_EQ(actual, expected)                                                             \
   do {                                                                                            \
      const auto fx1_a_ = static_cast<std::uint64_t>(actual);                                      \
      const auto fx1_e_ = static_cast<std::uint64_t>(expected);                                    \
      ::fx1_test::report(fx1_a_ == fx1_e_, #actual " == " #expected, __FILE__, __LINE__,           \
                         "got " + ::fx1_test::hex(fx1_a_) + ", expected " + ::fx1_test::hex(fx1_e_)); \
   } while (0)

#define FX1_CHECK_EQ_CTX(actual, expected, ctx)                                                    \
   do {                                                                                            \
      const auto fx1_a_ = static_cast<std::uint64_t>(actual);                                      \
      const auto fx1_e_ = static_cast<std::uint64_t>(expected);                                    \
      ::fx1_test::report(fx1_a_ == fx1_e_, #actual " == " #expected, __FILE__, __LINE__,           \
                         std::string(ctx) + ": got " + ::fx1_test::hex(fx1_a_) + ", expected " +   \
                             ::fx1_test::hex(fx1_e_));                                             \
   } while (0)
