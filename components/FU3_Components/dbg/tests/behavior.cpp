// DBG nối thẳng qua router của bench: cầu JTAG2AXI4, truy cập DSRAM, DM 2 hart tại 0x1A11_0000, abstract command, ndmreset.
#include "bench.h"

int sc_main(int, char**) {
    Bench bench("Bench", false);
    sc_core::sc_start();
    std::cout << "DBG behavior: " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
