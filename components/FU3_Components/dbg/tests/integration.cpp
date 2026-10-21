// Cùng bộ kiểm tra nhưng SBA đi qua cổng master DBG của bus FU3 thật: DBG -> SYSBUS_1 -> DSRAM @ 0x0100_0000.
#define FU3_DBG_WITH_BUS
#include "bench.h"

int sc_main(int, char**) {
    Bench bench("Bench", true);
    sc_core::sc_start();
    std::cout << "DBG integration (FU3 bus): " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
