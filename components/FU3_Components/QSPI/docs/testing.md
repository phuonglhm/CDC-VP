# Testing against HDS V1.0

The default suite has **57 CTest cases**: 18 controller/helper tests,
30 corner/parameter/HDS cases, eight generic flash tests, and one real FU3
bus integration. Assertions stay enabled in Release. Each test prints its
assertion count and simulation time; CTest sets a 30-second host timeout.

Build commands are in the [README](../README.md#build-and-test). Run from the
repository root in Linux/WSL with SystemC 2.3.4 and C++17:

```sh
ctest --test-dir build/fu3-qspi --output-on-failure
ctest --test-dir build/fu3-qspi -L hds --output-on-failure
ctest --test-dir build/fu3-qspi -L p0 --output-on-failure
ctest --test-dir build/fu3-qspi -L p1 --output-on-failure
ctest --test-dir build/fu3-qspi -L flash --output-on-failure
```

## HDS mapping

All names below carry the `fu3_qspi_` prefix. Source **HDS** is
`FTEL_CDC_IP_QSPI_Specification.pdf` V1.0, 2026-10-20; numbers are physical PDF
pages. [Source hash and scope](spec-decisions.md) identify the exact baseline.

| Test suffixes | Checked behavior | HDS source |
| --- | --- | --- |
| registers, components | Register reset/masks/BUSY locks, IRQ masks, 32-word default capacity, helper timing | 5; 6.4.5; 8 |
| protocol, errors, p0_bus_errors | Word payloads, low address bits, unmapped APB zero/error, mapped rejection, illegal dividers, BUSY config protection, backend errors/recovery | 6.4.6; 7.2-7.3; 8 |
| triggers, p0_triggers | 256 EN/FMODE/ADMODE/DMODE/instruction combinations; one selected CCR/AR/DR trigger | 6.4, Table 6-8 |
| indirect, fifo, p0_fifo, p0_dlr | Packing/tails, streaming, exact 128-byte default RX stall, TX overflow/discard/retry, finite/undefined DLR and 4-GiB boundaries | 6.3-6.4.1; DEC-013 |
| threshold_words, p0_irq_w1c | Word thresholds/levels; W1C, threshold reassert, event flags and enabled level IRQ | 6.4.4-6.4.5 |
| polling, timeout, p1_polling | AND/OR, status lengths 1..4, APMS=0/1, PIR, PTR=0/1/31/32, simultaneous SMF/STOF | 6.4.2; DEC-024 |
| dma, p1_dma | Whole-word bursts, CPU residue, held clear, live enable/disable, abort inhibition and CCR re-arm | 6.4.4; 7.4; 9.3; DEC-029/044 |
| phases, spec_timing | Widths/sizes/rates, instruction SDR, SIOO, minimum CSHT/PIR, divisor+2 | 6.3; 6.5-6.8; 8.2.1 |
| xip, concurrency, p1_domains | FSIZE, mapped reads, ordered callers, domain gating and no APB timeout | 6.1; 6.4.3; 7.2-7.3 |
| reset, p0_active_reset | Async state reset during prefix, starvation, full/unread RX, polling gap, delayed XIP; defensive isolated bus-reset cancellation | 6.2; isolated resets are outside ASM-002 |
| p1_parameters_4/8/16/32/4x32/8x32 | Legal FIFO depths, asymmetric TX/RX, independent clock ratios, bounded transfers, HCLK-only timeout, illegal parameter rejection | 5; Table 10-1 |
| random | 100 seeded indirect transfers and 100 mapped reads | Functional randomized regression |
| hds_cache | NONSEQ vs SEQ, invalidate-on-miss, hit/read during prefetch, cached data with stopped core, clamp at flash end, config/disable flushes, IDLE/BUSY, illegal divider TEF+AHB error | 6.4.3; 6.9; DEC-016/027/030 |
| hds_control | Disabled TX prefill; EN clear during write/read/poll; address-less range bypass; low phase bytes; empty frame; observable persistent ABORT and atomic completion | 6.3-6.5; DEC-013/038/044 |
| hds_dma_clock | Independent DMA clock, stopped-core clear acknowledgement, held-low interval, abort re-arm only at CCR, async reset outputs and released SR=4 | 6.1-6.2; 7.4; DEC-036/044 |
| hds_flags | Same-edge hardware TCF/W1C set priority; APB low bits/unmapped zero; APB pending through core reset | 6.4.5; 7.2; Section 10; DEC-012 |
| hds_timeout | CFG-TMO=64 HCLK, late data discarded/no cache fill, unrelated miss waiting for a prefetch, timeout recovery | 6.4.3; Section 10 |
| hds_frame_matrix | 384 independent IMODE/ADMODE/ABMODE/DMODE/SDR-DDR combinations; each writes and reads 1..9 bytes, checks selected phase bytes and timing; varies sizes 1..4, all DCYC 0..31, CKMODE and SIOO; checks repeated AR-triggered SIOO reads | 6.3; 6.5-6.8; Section 10 |
| hds_dividers | All 256 encoded prescalers in SDR and DDR (512 triggers), legal frame duration and illegal TEF/no frame | 6.6; 8.2.1 |
| hds_parameters | Every cache/prefetch pair 1<=prefetch<=count<=16, legal FIFO/async depths and timeout boundaries, independent illegal fields | 5, Table 5-1 |
| hds_cache_parameters_1/2/3/4/8/16 | Runtime prefetch lengths, every line tag/data hit, invalidate-on-miss, HSEL=0 ignored, DLR ignored; includes non-power-of-two cache count | 5; 6.4.3; 6.9; 7.3 |
| hds_abort | Abort requests across instruction/address/alternate/dummy/data configurations and all four FMODE values; pending XIP error and FIFO/DMA completion state | 6.4.6; Section 10 |
| flash_commands/program/erase/protocol/reset/image/sioo/integrity | Generic NOR support profile, image I/O, descriptor mutation/order/cancel checks | Device support API; not controller HDS requirements |
| bus | FU3 CVA6 burst decomposition, DMA FIXED DR, NOR program/status-poll/XIP/erase/QE/SIOO composition | Existing FU3 bus and controller integration |

The corner fixture uses a sparse 4-GiB target and host-byte packing oracles.
Clock periods differ by FIFO configuration; DMA has an independent 18-ns clock.
The 4/4 case uses one cache line, prefetch 1 and async depths 2 (CFG-MIN).
The depth-8 cases use four cache lines/prefetch 4. Illegal Config values are
validated before storage allocation; direct configuration validation is tested.

The phase matrix covers every independent phase-width/rate combination, with
sizes, dummy counts, CKMODE and SIOO varied across those combinations. It is
not the full Cartesian product of all CSR fields. Every named functional group
in [spec-baseline.json](../spec-baseline.json) must map to registered passing
scenarios; the recorder rejects missing, extra or unmapped tests.

The tests check firmware-visible TLM behavior. There are no pin-level serial
or bus monitors; byte abort timing is a descriptor/timer abstraction and does
not establish OE/CSN/SCLK or bus-waveform compliance. The clocks are varied in
selected fixed configurations, not exhaustively randomized for RTL CDC sign-off.

## UBSan and evidence

```sh
cmake -S components/FU3_Components/QSPI -B build/fu3-qspi-ubsan \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4 -DCMAKE_BUILD_TYPE=Debug \
  -DFU3_QSPI_BUILD_TESTS=ON \
  '-DCMAKE_CXX_FLAGS=-fsanitize=undefined -fno-sanitize-recover=undefined -Wall -Wextra -Wpedantic -Werror'
cmake --build build/fu3-qspi-ubsan -j4
ctest --test-dir build/fu3-qspi-ubsan --output-on-failure
python3 components/FU3_Components/QSPI/scripts/record_tests.py \
  build/fu3-qspi build/fu3-qspi-ubsan
```

The recorder runs both suites, checks the exact registered case set, functional
mapping and UBSan flags, and captures actual counts/assertions, toolchain,
source hashes, pinned spec version/date/PDF hash and per-feature outcomes.
To verify the exact source PDF before executing tests, append:

```sh
--spec-pdf /path/to/FTEL_CDC_IP_QSPI_Specification.pdf
```

A hash mismatch stops the recorder before running tests. With no `--spec-pdf`,
the report explicitly records `pdf_hash_verified: false`. Generic NOR support
is reported separately from the 13 controller/system functional groups.
Evidence goes to `build/fu3-qspi/test-reports/` (ignored build output). CTest also
keeps `Testing/Temporary/LastTest.log` in each build directory. The recorder can
use `--output-dir PATH`; it never treats an older transcript as current success.
