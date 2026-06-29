**Model Overview:**
UART TlM for ARM PrimeCell PL011 UART.
Structure: `include/uart.h` for behavior, `tests/master_tb.h` for testing, and `tests/top.h` for the test harness.

**Run instructions:**
Type `make` in terminal, then run `./test_uart2_tlm`. Use `make clean` to clean outputs.

**Interrupts (PL011-style):**
- `sc_out<bool> irq` is the combined level-sensitive `UARTINTR` to the PLIC,
  high whenever `UARTMIS != 0`.
- Implemented raw-interrupt sources (UARTRIS/UARTMIS/UARTIMSC/UARTICR):
  - `UART_RXRIS` (bit4): RX FIFO reached the programmed trigger level.
  - `UART_TXRIS` (bit5): TX FIFO at/below the programmed trigger level
    (level-sensitive, asserted when the TX FIFO is empty, as on real PL011).
  - `UART_RTRIS` (bit6): RX timeout - RX data sat idle for `rx_timeout`
    (constructor arg, default 1 ms; real hardware uses 32 baud clocks). Cleared
    by draining the RX FIFO or by `UARTICR`.
  - `UART_OERIS` (bit10): overrun - a byte arrived while the RX FIFO was full;
    the byte is discarded. `UART_FERIS`/`UART_PERIS`/`UART_BERIS` bits exist for
    completeness but are not stimulated (no line-error injection path).
- `UARTICR` is write-1-to-clear for any raw interrupt bit.

**Programmable FIFO trigger (UARTIFLS, 0x034):**
- `TXIFLSEL[2:0]` / `RXIFLSEL[5:3]` select 1/8, 1/4, 1/2, 3/4, 7/8 of the 16-deep
  FIFO. Reset value `0x12` (both 1/2). Changing it re-evaluates RX/TX interrupts.

**Receive status / error clear (UARTRSR/UARTECR, 0x004):**
- Read (UARTRSR) returns the error flags (`OE`/`BE`/`PE`/`FE`); only `OE` is
  modeled. Write (UARTECR) clears the error flags.

**Note**
- See implemented flags in uart.h. Modem-status interrupts (RI/CTS/DCD/DSR),
  FE/PE/BE error generation, and `UARTLCR_H.FEN` FIFO-disable mode are not
  modeled.
- Behaviour is tested in `tests/master_tb.cpp` (address 0x10000000): RX trigger,
  FIFO full/overflow + overrun (OE) interrupt and clear, TX FIFO/TX interrupt,
  the IRQ line to the PLIC, the RX-timeout interrupt, and programmable UARTIFLS
  trigger levels. The test fails (non-zero exit) on any mismatch.
