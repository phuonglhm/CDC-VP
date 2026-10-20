// MAILBOX nối thẳng (không qua bus): reset, FIFO hai chiều, đầy/rỗng, threshold, IRQ, truy cập lỗi, debug, reset.
#include "bench.h"

int sc_main(int, char**) {
    Bench bench("Bench", false);
    sc_core::sc_start();
    std::cout << "MAILBOX behavior: " << bench.checks << " checks, " << bench.errors << " failures\n";
    return bench.errors ? 1 : 0;
}
