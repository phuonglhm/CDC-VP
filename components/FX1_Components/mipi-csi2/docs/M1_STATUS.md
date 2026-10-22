# M1 status — detailed D-PHY verifiable and reusable

Date: 2026-10-19. Scope and decisions: [spec_decisions.md](spec_decisions.md),
[PHY_ADAPTER_CONTRACT.md](PHY_ADAPTER_CONTRACT.md). Not committed.

## Exit gate

| Gate item (plan §4) | Status | Evidence |
|---|---|---|
| `phy_elaborate` | PASS | `fx1_dphy_elaborate`: 272 ports bound, generator refuses unclassified ports |
| `phy_startup` | PASS | `fx1_dphy_startup`: FSM 0→1→2→4→5→6→9→A→7, `enable_lanes` 340.5 µs after release, stop-state on all lanes, no X/Z on PPI |
| `phy_lp_hs_receive_{1,2,4}lane` | PASS | `fx1_dphy_hs_{1,2,4}lane`: exact bytes of a CSI-2 RAW10 long packet per lane |
| `phy_reset_recovery` | PASS | `fx1_dphy_reset`: second startup reproduces the first exactly, test registers re-applied and read back. Reset in LP idle only; reset during an HS burst is not covered yet |
| Configuration actually applied | yes | `config_ok()` checked after startup and reset; negative test `fx1_dphy_config_fault` (write to undecoded tester address 0xFF must be detected) |
| Independent oracle | yes | expected bytes = what the BFM was told to send (packet builder checked against CRC-16/MCRF4XX, MIPI CRC examples, two ECC formulations) |
| Pad corruption changes the result | yes | `fx1_dphy_hs_corrupt` (exact byte), `sot_err1`, `sot_err2` |
| Lane skew / order | yes | `skew`, `lead4`, `lead4_link`, `phase` (clock restart, 8 phases) |
| Deskew / ULPS | not supported, documented | contract §2.1, §5 |
| Benchmark | done | below |

## Tests

All tests carry label `fx1_csi2`, so the platform gates
(`fx1_unit|fx1_smoke`) are unaffected.
`ctest -L fx1_csi2_unit` (6 tests, ~5 s): packet builder ×5, `fx1_dphy_elaborate`.
`ctest -L fx1_phy_slow` (14 tests, ~108 s CPU): startup, reset, config_fault, 11 HS cases.
Full run after review fixes, 2026-10-19: **20/20 passed**, 29.7 s wall with `-j4`.

## Benchmark (AlmaLinux 9, GCC 11.5.0, SystemC 2.3.4, Release, 1 fs)

| Item | Value |
|---|---|
| `fx1_mipi_dphy` build (181 TU, `-O1`, `-j8`) | 47 s |
| Elaboration | 4.9 s wall, 405 MiB RSS |
| Startup (340.5 µs simulated) | ~3 s wall |
| One HS test (startup + 86-byte burst on 1–4 lanes at 800 Mb/s) | 6.5–8.5 s wall, ~410 MiB |

The HS bursts themselves are ~1 µs simulated; wall time is dominated by
elaboration and startup. Frame-size throughput is not measured yet (M3/M5).

## How to run

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
# sanity
$CC -dumpfullversion
$CXX --version | head

cmake -S components/FX1_Components/mipi-csi2 -B /tmp/csi2-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DSYSTEMC_HOME=/opt/systemc-2.3.4
cmake --build /tmp/csi2-build -j8
ctest --test-dir /tmp/csi2-build -L fx1_csi2 -j4 --no-tests=error --output-on-failure
```

In the CDC-VP tree, `-DFX1_BUILD_MIPI_CSI2=ON` (default OFF) adds the
directory with `EXCLUDE_FROM_ALL`: neither `all` nor `fx1_all` builds it (the
`fx1_all` collector in `platforms/fx1_soc/CMakeLists.txt` skips
`EXCLUDE_FROM_ALL` directories; verified: 49 direct inputs with ON and OFF, no
MIPI target). Build it explicitly, then run its label:

```bash
cmake --build <build> --target fx1_mipi_csi2_all -j8
ctest --test-dir <build> -L fx1_csi2 -j4 --no-tests=error --output-on-failure
```

ON registers 20 tests (183 → 203); OFF changes nothing.

`PATH=/usr/bin:/bin:$PATH` is required on the review host: the default PATH
resolves `as` to a Synopsys VP wrapper that recurses on itself, and `python3`
to a Synopsys build that cannot run isolated scripts.

Debug aid of `test_fx1_dphy_startup` (not used by ctest):
`FX1_DPHY_STARTUP_TIMEOUT_US=<us>` shortens the startup timeout (every case must then FAIL).

Debug aids of `test_fx1_dphy_hs` (not used by ctest):
`FX1_DPHY_LIST=<regex>` lists internal channels, `FX1_DPHY_TRACE=<regex>`
prints their changes during the burst, `FX1_DPHY_TESTREGS=09=10,...`
overrides the PHY test-register writes; extra arguments `<hs_zero_ui> <clk_phase_ui>`.

## Findings carried into M2/M3

1. `errsotsynchs` does not stop `rxvalidhs`: the burst must be discarded by the controller.
2. The PHY delivers 6–8 trail/EoT bytes after the payload: packet end comes from WC.
3. One common `rxskewcalhs`; no `ulpsactivenot`, no `init_done` (adapter values, contract §2.1).
4. Lane 0 must be enabled for any byte clock; `basedir` must be 1.
5. PHY test register 0x09 = 0x10 (P07) is required for per-lane SoT reporting; who programs it on silicon is open.
6. The README's "PLL lock" step is not needed for RX; PLL stays off.
7. `hsfreqrange` ↔ rate mapping is inferred, only 0x09 / 800 Mb/s is exercised; >1.5 Gb/s and deskew are untested.

## Files

| Path | Content |
|---|---|
| `CMakeLists.txt` | `fx1_mipi_dphy`, `fx1_mipi_dphy_wrapper`, `fx1_csi2_bfm`, `fx1_csi2_dphy_bfm`, tests |
| `phy/` | `DphyRxWrapper` (port binding, generated `dphy_rx_wrapper_bind.inc`), `DphyStartup` fixture |
| `bfm/` | CSI-2 packet builder; pad-level `DphyTxBfm` |
| `tests/` | `test_csi2_packet.cpp`, `test_phy_startup.cpp`, `test_phy_hs_receive.cpp` |
| `tools/` | `csr_workbook.py` (workbook check / CSR table), `gen_dphy_wrapper_bind.py` |
| `docs/` | M0 contracts and this status |

## Review 2026-10-19 (plan/M0_M1_REVIEW.md) — resolution

| ID | Finding | Resolution |
|---|---|---|
| R1 (P1) | `dma_abort` defined three incompatible ways (M10/M13/M15) | Abort and reset separated. M16: any abort cause (AXI/layout fault, `dma_abort`, `core_enable`/`dma_enable` 1→0) = stop issuing, drain, finalise **once** per `err_frame_policy`; epoch unchanged so the finalisation is always published. M13: resets drain, rewind/clear without event, epoch+1; epoch only blocks stale publication. M15: SW-owned sides never touched except by `full_reset`/`resetn`. Transition table in spec_decisions §2.3; D06 now defines delivered/dropped for every end, including policy-1 aborts. |
| R2 (P1) | `reset_in_progress` missed the workbook's "during a disable" | M17: `reset_in_progress` = `soft_reset` OR PHY reset pulse OR `full_reset` sequence OR disable drain pending; falls with the finalisation. C08: event suppression only for reset causes; RST_TAIL replaced by epoch tagging, stated as a VP deviation. Open item: whether silicon counts `dma_abort` as a disable. |
| R3 (P2) | startup/reset PASS despite failed test-register read-back | `DphyStartup::config_ok()`; checked after startup and after reset; `fx1_dphy_config_fault` negative test; reviewer repro (`FF=A5` injected) now FAILs with exit 1. |
| R4 (P2) | `FX1_BUILD_MIPI_CSI2=ON` pulled the PHY into `fx1_all` | `add_subdirectory(mipi-csi2 EXCLUDE_FROM_ALL)`, collector skips such directories, new `fx1_mipi_csi2_all`; tests moved from `fx1_unit` to `fx1_csi2*` labels so the platform gate does not see unbuilt executables. |

Re-review 2026-10-20 (plan/M0_M1_REREVIEW_20261020.md):

| ID | Finding | Resolution |
|---|---|---|
| F1 (P1) | `reset_in_progress` could read 0 after a short `soft_reset` pulse while old writes were still outstanding | Latched `reset_drain_pending` (M13): set at `soft_reset` 0→1, `full_reset`, `resetn`; cleared only after all earlier writes responded and the side was finalised; part of `reset_in_progress` and `dma_busy` (M17); no claim while set. Synchronised in REGISTER_CONTRACT §2 and the transition table. New planned test `csi2_soft_reset_pulse_delayed_response`. |
| F2 (P2) | `config_fault` PASS despite another failed CHECK | Verdict = `detected && g_fail == 0`, same variable for message and exit code. Verified: normal run PASS; with `FX1_DPHY_STARTUP_TIMEOUT_US=1` (new debug override) both `config_fault` and `startup` FAIL, exit 1. |

The DMA/reset contract (R1/R2/F1) has no implementation yet; its tests are M2/M4 work.
