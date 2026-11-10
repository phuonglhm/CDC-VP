// SPDX-License-Identifier: Apache-2.0
// Test helper (ISP-UPD-008): the canonical write stream of a profile as the
// model's reader (tests/common/vector_io.h) resolves it, one "REG 0xVALUE"
// line per write; an error goes to stderr with exit status 1. Compared with
// tools/csrw_codec.py by tests/tools/test_csrw_codec.py. --frames: a
// frames.csrw ("<frame> <profile line>"), one "FRAME REG 0xVALUE" per write.
#include <cstdio>
#include <exception>
#include <iostream>
#include <string>

#include "vector_io.h"

int main(int argc, char **argv) {
   const bool frames = argc == 3 && std::string(argv[1]) == "--frames";
   if (argc != 2 && !frames) {
      std::cerr << "usage: csrw_dump [--frames] FILE.csrw\n";
      return 2;
   }
   try {
      if (frames) {
         for (const fx1_test::csr_line &l : fx1_test::read_frame_writes(argv[2])) {
            std::printf("%u %s 0x%08X\n", l.frame, l.reg.c_str(), l.value);
         }
         return 0;
      }
      for (const fx1_test::csr_line &l : fx1_test::read_profile(argv[1])) {
         std::printf("%s 0x%08X\n", l.reg.c_str(), l.value);
      }
   } catch (const std::exception &e) {
      std::cerr << e.what() << "\n";
      return 1;
   }
   return 0;
}
