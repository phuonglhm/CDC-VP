**Model Overview:**
UART TlM for ARM PrimeCell PL011 UART.
Structure: master_tb.h for testing, top.h for future expansion (not important rn), uart.h for behavior

**Run instructions:**
Type 'make main' in terminal. (make clean to clean outputs)

**Note**
- See implemented flags in uart.h, if you don't see a flag (OERIS overrun error, LBE loopback empty, etc.), i did not implement it.
- Things are tested in master_tb.h, using address 0x10000000. It passes all 6 tests in this testbench.