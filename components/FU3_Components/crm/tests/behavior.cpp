// CRM nối thẳng (không qua bus): reset value/mask, PRSTN, nhả CPU, PLL/clock, gate/reset output, reset mềm, IRQ, truy cập.
#include "bench.h"

int sc_main(int, char**) {
    Bench bench("Bench", false);
    sc_core::sc_start();
    std::cout << "CRM behavior: " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
