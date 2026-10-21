// Cùng bộ kiểm tra nhưng đi qua bus FU3 thật: CVA6 -> PERIBUS_1 -> MAILBOX_CVA6 @ 0x5002_0000, IBEX -> PERIBUS_0 -> MAILBOX_IBEX @ 0x4404_0000.
#define FU3_MAILBOX_WITH_BUS
#include "bench.h"

int sc_main(int, char**) {
    Bench bench("Bench", true);
    sc_core::sc_start();
    std::cout << "MAILBOX integration (FU3 bus): " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
