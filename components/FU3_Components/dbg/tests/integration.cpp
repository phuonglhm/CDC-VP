// Cùng bộ kiểm tra qua bus FU3 thật: JTAG2AXI4 -> cổng master DBG -> SYSBUS_1 -> DSRAM, ROM và DM (IBEX_DBG @ 0x1A11_0000).
#define FU3_DBG_WITH_BUS
#include "bench.h"

int sc_main(int, char**) {
    Bench bench("Bench", true);
    sc_core::sc_start();
    std::cout << "DBG integration (FU3 bus): " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
