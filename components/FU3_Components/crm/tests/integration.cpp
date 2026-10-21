// Cùng bộ kiểm tra nhưng đi qua bus FU3 thật: IBEX -> SYSBUS_1 -> PERIBUS_1 -> A2P -> CRM @ 0x5001_0000.
#define FU3_CRM_WITH_BUS
#include "bench.h"

int sc_main(int, char**) {
    Bench bench("Bench", true);
    sc_core::sc_start();
    std::cout << "CRM integration (FU3 bus): " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
