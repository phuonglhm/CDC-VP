**Model Overview:**
UART TlM for ARM PrimeCell PL011 UART.
Structure: `include/uart.h` for behavior, `tests/master_tb.h` for testing, and `tests/top.h` for the test harness.

**Run instructions:**
Type `make` in terminal, then run `./test_uart2_tlm`. Use `make clean` to clean outputs.

**Note**
- See implemented flags in uart.h, if you don't see a flag (OERIS overrun error, LBE loopback empty, etc.), i did not implement it.
- Things are tested in master_tb.h, using address 0x10000000. It passes all 6 tests in this testbench.
