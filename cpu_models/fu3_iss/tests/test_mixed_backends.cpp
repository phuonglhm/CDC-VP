// A binary may link the FU3 ISS fork (through cva6_wrapper) together with the
// shared upstream riscv_vp backend: every fork symbol lives in fu3_rv32, so the
// link must succeed and both cores must elaborate (T04/T05 review R07).
#include <iostream>

#include <systemc>
#include <tlm_utils/tlm_quantumkeeper.h>

#include "cva6_wrapper.h"
#include "riscv_vp_wrapper.h"

int sc_main(int, char**)
{
    tlm::tlm_global_quantum::instance().set(sc_core::sc_time(1, sc_core::SC_US));
    cdc::cpu::cva6_wrapper fu3("fu3");
    cdc::cpu::riscv_vp_cpu shared("shared");
    std::cout << "fu3_iss + riscv_vp in one binary: " << fu3.backend_name() << ", " << shared.backend_name()
              << " -> PASS\n";
    return 0;
}
