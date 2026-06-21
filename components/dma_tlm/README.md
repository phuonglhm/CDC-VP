# dma_tlm

SystemC/TLM-2.0 model of the ARM CoreLink DMA-330 DMA Controller.

The model follows the local `wdt_tlm` component style: a single `sc_module`, explicit register constants in the public header, `tlm_utils` sockets, active-low reset, little-endian 32-bit APB register accesses, and direct functional test coverage through CTest.

## Implemented Scope

- DMA-330 programmer-visible 4KB APB register map:
  - Manager status/control registers at `0x000`.
  - Channel status and PC registers at `0x100`.
  - Channel SAR/DAR/CCR/LC registers at `0x400`.
  - Debug registers at `0xD00`.
  - Configuration and watchdog registers at `0xE00`.
  - ARM peripheral/component ID registers at `0xFE0`.
- Eight DMA channels and 32 event/interrupt resources.
- Debug-register launch flow using `DBGINST0`, `DBGINST1`, and `DBGCMD`.
- Functional DMA-330 channel instruction subset:
  - `DMAMOV SAR|CCR|DAR`
  - `DMALD`, `DMALDS`, `DMALDB`
  - `DMAST`, `DMASTS`, `DMASTB`
  - `DMASTZ`
  - `DMAADDH`, `DMAADNH`
  - `DMALP`, `DMALPEND`
  - `DMARMB`, `DMAWMB`, `DMANOP`
  - `DMASEV`, `DMAWFE`
  - `DMAEND`, `DMAKILL`
- TLM master memory access through `master_socket`.
- Event interrupt reporting through `INT_EVENT_RIS`, `INTMIS`, `INTCLR`, and `irq`.
- Fault reporting through `FSRC`, `FTRn`, and `irq_abort` for unsupported instructions, invalid operands, and failed memory transactions.

This is a programmer's-view LT model, not a cycle-accurate AXI/APB implementation. It does not model both secure and non-secure APB ports separately, peripheral request pins, AXI outstanding transaction timing, cache-line fills, or detailed MFIFO packing behavior.

## Tests

`tests/test_dma_tlm.cpp` contains a self-contained SystemC testbench for the
programmer-visible APB interface and the DMA master memory path.

Testbench setup:

- `ram_tlm` is a small vector-backed RAM target with a
  `tlm_utils::simple_target_socket`. It accepts TLM read and write
  transactions, checks address bounds, and adds 1 ns of access latency.
- `Testbench` owns a `tlm_utils::simple_initiator_socket` that acts as the APB
  register master. Its helper methods issue 32-bit little-endian reads and
  writes to the DMA register map.
- `sc_main` instantiates `dma_tlm`, `ram_tlm`, and `Testbench`. The testbench
  initiator socket is bound to `dma_tlm::target_socket`, and
  `dma_tlm::master_socket` is bound to the RAM target socket. Active-low reset,
  `irq`, and `irq_abort` are connected with SystemC signals. Reset is held low
  for 10 ns, then released before the test thread starts checking behavior.

Test 1: Reset and register check

- Reads `DSR` at `0x000` and verifies the manager status field is
  `STATUS_STOPPED` (`0x0`).
- Reads `CSR0` at `0x100` and verifies channel 0 is also stopped after reset.
- Reads `CCR0` at `0x408` and verifies the reset-visible channel control value
  is `0x00800200`.
- Reads ARM identification registers and verifies the implemented ID values:
  `PERIPH_ID0` at `0xFE0` is `0x30`, `PERIPH_ID1` at `0xFE4` is `0x13`,
  `PERIPH_ID2` at `0xFE8` is `0x34`, and `PCELL_ID0` at `0xFF0` is `0x0D`.

Test 2: Debug launch and memory transfer

- The test initializes 32 bytes of source RAM at `0x00000200` with the pattern
  `0x40, 0x41, ...`, clears 32 bytes of destination RAM at `0x00000300`, and
  writes a DMA-330 channel program into RAM at `0x00000100`.
- The program starts with three `DMAMOV` instructions:
  `DMAMOV CCR, <value>` configures incrementing source and destination
  transfers with 4-byte beats and 4-beat bursts, `DMAMOV SAR, 0x00000200`
  sets the source address, and `DMAMOV DAR, 0x00000300` sets the destination
  address.
- The program then executes `DMALP` with two iterations. The loop body contains
  `DMALD` followed by `DMAST`, so each loop iteration loads one configured
  burst from source RAM and stores it to destination RAM. With 4 beats per
  burst and 4 bytes per beat, two iterations copy 32 bytes total.
- `DMALPEND` branches back over the `DMALD`/`DMAST` body until the loop count
  expires. After the loop, `DMAWMB` models the write memory barrier,
  `DMASEV 3` raises event 3, and `DMAEND` stops the channel thread.
- Before launch, the test enables completion interrupt event 3 by writing
  `INTEN = 0x00000008`. It then uses the debug interface to start channel 0:
  `DBGINST0` is written with the encoded `DMAGO` instruction bytes,
  `DBGINST1` is written with the program counter `0x00000100`, and writing
  `DBGCMD` dispatches the debug instruction.
- After allowing the transfer to run, the test verifies the destination RAM
  exactly matches the 32-byte source pattern, `CSR0` has returned to stopped,
  `SAR0` advanced to `0x00000220`, `DAR0` advanced to `0x00000320`, and
  `FSRC` remains zero. It also checks interrupt behavior: `INT_EVENT_RIS`,
  `INTMIS`, and the `irq` vector all contain bit 3 (`0x00000008`), while
  `irq_abort` stays false. Finally, writing `INTCLR = 0x00000008` clears
  `INTMIS` and deasserts `irq`.

Test 3: Read-only registers

- Attempts to write `0xAAAAAAAA` to `SAR0` and verifies `SAR0` still contains
  the architectural value left by the completed transfer, `0x00000220`.
- Attempts to write zero to `CCR0` and verifies `CCR0` still contains the value
  programmed by the DMA instruction stream. This confirms APB writes to these
  read-only architectural channel registers do not override state updated by
  DMA execution.

## Build and Run

These commands are for the standalone `dma_tlm` SystemC/TLM component testbench.
They follow the same component-level flow used by `components/wdt_tlm/README.md`.

There is no `dma_platform` target and no `fw/dma_*` firmware directory in this
workspace, so no firmware build step is required for this component test.

Step 1: Source environments from the CDC-VP repository root:

```bash
cd /home/hoangquan/workspace/CDC-VP
./tools/third_party/setup_third_party.sh
source ./tools/third_party/setup_env.sh
```

Step 2: Configure CMake with Ninja from the repository root:

```bash
cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_BUILD_TESTS=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
```

Step 3: Build the `dma_tlm` SystemC test target:

```bash
cmake --build build/bremen --target test_dma_tlm
```

Step 4: Execute the compiled SystemC simulation binary:

```bash
./build/bremen/components/dma_tlm/tests/test_dma_tlm
```

The expected final testbench line is:

```text
[TB] Result: PASS (0 error(s))
```
