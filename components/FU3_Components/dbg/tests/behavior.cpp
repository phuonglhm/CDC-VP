// DBG nối thẳng (SBA vào bộ nhớ của bench): JTAG DTM, DM 2 hart, abstract command, ndmreset, SBA.
#include "bench.h"

int sc_main(int, char**) {
    Bench bench("Bench", false);
    sc_core::sc_start();
    std::cout << "DBG behavior: " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
