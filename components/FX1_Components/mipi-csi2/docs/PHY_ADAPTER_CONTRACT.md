# CSI-2 RX VP — PHY adapter contract (mipirxphy model)

Status: **draft for review, 2026-10-19.** Everything about the PHY below is
**model-derived, not confirmed by the PHY vendor**: values were read from the
SystemC model in `src/` (translated from the PHY Verilog) and, where marked
*measured*, observed by `tests/test_phy_startup.cpp`. Items the code does not
decide are **UNRESOLVED**; nothing here is filled from a generic D-PHY flow.

Paths are relative to `src/`. Abbreviations: TOP `top/mipirxphy.cpp`,
NS `rtl/mipi_4_rx_dphy_ns.cpp`, IF `rtl/mipi_4_rx_dphy_ns_if.cpp`,
PWR `rtl/mipi_4_rx_dphy_ns_dphyifpowerctrlrx.cpp`, TST
`rtl/mipi_4_rx_dphy_ns_dphy4rxtester.cpp`, SYNC `rtl/mipi_4_rx_dphy_ns_gen_sync_rst.cpp`,
BIST `rtl/mipi_4_rx_dphy_ns_bist_machine_rx.cpp`, PLLS `rtl/mipi_4_rx_dphy_ns_mpll_startup.cpp`,
CB `rx_core/mipi_4_rx_dphy_ns_DPHY3AFECBRX.cpp`, ATB `rx_core/mipi_4_rx_dphy_ns_DPHYATB.cpp`,
CLKL `rx_core/mipi_4_rx_dphy_ns_DPHYHSRX_clk_lane.cpp`, LPRX `manual/mipi_4_rx_dphy_ns_DPHYLPRX.cpp`.
Confidence: **[C]** proven by code, **[I]** inferred from code, **[M]** measured
in simulation, **[U]** unresolved.

## 1. The macro is not the one the MAS describes

MAS v1.3 §6.1 maps the controller PPI to AMD PG202 v4.3 (`core_rst`,
`core_clk`, `init_done`, per-lane `dl<n>_rxskewcalhs`). The model is a
different macro (`mipi_4_rx_dphy_ns`, test-register interface, `hsfreqrange`,
`cfgclkfreqrange`). The adapter therefore defines the mapping below; the
controller side keeps the MAS names.

## 2. Signal mapping

### 2.1 Controller PPI ← model

| MAS PPI signal | Model port | Notes |
|---|---|---|
| `cl_enable` | `sysc2mipirxphy_enableclk` | IF:2250 `enable_clklane = enableclk & enable_lanes` [C] |
| `dl_enable_x` | `mipirxphy_enable[x]` | IF:2327 [C] |
| `forcerxmode_x` | `sysc2mipirxphy_forcerxmode_x` | straight to core netlist; behaviour [U] |
| `cl_stopstate` | `stopstateclk` | [M] 1 in LP-11 after startup |
| `stopstate_x` | `stopstatedata_x` | lane 0 registered/synchronised (IF:3911-3916), lanes 1–3 direct (IF:2529) [C]; [M] 1 in LP-11 |
| `cl_ulpsactivenot` | **not available** | inner `ulpsactivenotclk` is left unconnected at TOP:437; `phy_ulpsactivenotclk_in` tied 0 at TOP:233. Adapter drives the controller input with 1 (never ULPS) and documents it; ULPS entry is not supported by this adapter [C] |
| `ulpsactivenot_x` | **not available** | TOP:433-436 unconnected; MAS: declared, not consumed |
| `rxbyteclkhs` | `rxbyteclkhs` | lane 0 word clock (IF:2263, core_part1:2877); lane 0 must be enabled for any byte clock [C/I] |
| `cl_rxclkactivehs` | `rxclkactivehs` | |
| `cl_ulpsclknot` | `rxulpsclknot` | not a substitute for `cl_ulpsactivenot` (different deassert time) |
| `rxdatahs_x[7:0]`, `rxvalidhs_x` | `rxdatahs_x`, `rxvalidhs_x` | 8-bit PPI [C] |
| `rxactivehs_x`, `rxsynchs_x` | `mipirxphy2sysc_rxactivehs_x`, `mipirxphy2sysc_rxsynchs_x` | |
| `rxskewcalhs_x` | `rxskewcalhs` (**one** signal) | adapter fans it out to all lanes; LANE_STATUS.laneX_skewcalhs therefore cannot identify a lane [C] |
| `errsoths_x`, `errsotsynchs_x` | `mipirxphy_errsoths_x`, `mipirxphy_errsotsynchs_x` | |
| `erresc_x`, `errsyncesc_x`, `errcontrol_x` | `mipirxphy_erresc_x`, `mipirxphy2sysc_errsyncesc_x`, `mipirxphy2sysc_errcontrol_x` | declared, not consumed (MAS) |
| `rxclkesc_x`, `rxlpdtesc_x`, `rxulpsesc_x`, `rxtriggeresc_x`, `rxdataesc_x`, `rxvalidesc_x` | same names (`mipirxphy2sysc_` prefix for some) | declared, not consumed |
| `init_done` | **not available** | adapter derives `init_done = (state_main == 7) && enable_lanes_int` from the model's internal signals for observability only (P05). Never used as proof of calibration. |
| `phy_core_rst` (active-high) | `dphy_reset_n = !phy_core_rst` | see §3 for the full sequence the adapter must run |
| `core_clk` | — | the model has no such clock; see `cfg_clk` |

### 2.2 Clocks

| Model input | Role | VP value |
|---|---|---|
| `mipi_pclk` → `cfg_clk` | clocks the power-control FSM, calibration timers and tester (IF, PWR) [C] | **25 MHz** (40 ns) |
| `sysc2mipirxphy_cfgclkfreqrange[7:0]` | `rx_timebase_if = 0x44 + cfgclkfreqrange` (IF:2772-2773) is a 4 µs time base in cfg_clk cycles ("4us timebase", TST:6310; `del_datalane = timebase[7:3]` "target 500n", IF:3662-3666) → **code = 4·f_cfg[MHz] − 68**, valid 17–63.75 MHz [I] | **0x20** (25 MHz) |
| `mipi_clk` → `refclk`, PLL `clkin/clkext` | only feeds the PLL and `pll_refclk = ~scan_mode & refclk` (IF:3525-3526) [C]; PLL off in RX mode (§3.3) | **0, not toggled** [M: startup completes without it] |

### 2.3 HS rate configuration

`sysc2mipirxphy_hsfreqrange[6:0]` selects seven per-code tables (IF:3999-5404):
`pll_m`, `vco_cntrl`, data-lane Ths-settle count, compatibility mode, gmode,
clock-lane DDR divider, DDL oscillator target [C].

- The data-lane Ths-settle counter counts the received clock-lane DDR clock
  after the `ddr_clk_div` divider, not cfg_clk (core netlist,
  `hsrxtimer_reg`, core_part3:4327-4455) [C]. The setting must therefore match
  the lane rate actually transmitted.
- **Rate association is UNRESOLVED at gate level.** The only rate statement in
  the code is the PLL comment at IF:3993-3998 ("refclk at 24MHz", "HS clock =
  datarate/2"). Reading it as `rate = 4·M / 2^vco[5:4]` Mb/s gives round,
  continuous rates (80 … 2500) and a Ths-settle of ~90–100 ns for every code,
  which is self-consistent [I].
- VP choice: **0x09** (implied 800 Mb/s, compatibility mode 1, ddr_clk_div 1,
  settle count 19 → 19·4 UI = 95 ns at 800 Mb/s) [I]. To be confirmed by the
  HS receive tests: a wrong rate association shows as a settle window that
  swallows or misses the sync byte.
- Codes with compatibility mode 1 (≤ 0x2c) run the internal skew-backward step
  at startup (state 0xA) and need no TX deskew burst; codes ≥ 0x3d skip it.
  Whether those need a TX deskew burst is [U]; not used in M1.

## 3. Startup sequence

### 3.1 Static tie-offs (functional mode)

Complete per-port list: `tools/gen_dphy_wrapper_bind.py`, which refuses to
generate if a port is unclassified.

| Group | Value | Evidence |
|---|---|---|
| `MIPIRX_VDD`, `VP`, `VPH`, `VPL` | 1 (`VDD` driven by `vl::tie`, inout) | cells output X unless VDD=1, VSS=0 (`rx_core/rx_cells.h:2521`); per-block supply checks (CB:274, LPRX:99-124) [C] |
| `MIPIRX_VSS`, `MIPIRX_AGND` | 0 (`vl::tie`) | same [C] |
| `MIPIRX_REXT`, `*_ATB*`, `PLL_VBG` | Z | REXT reaches only the continuity buffer and an ATB input whose mux is commented out; termination calibration compares against a fixed reference (ATB:56-58, 95-107) [C/I]; VBG only in commented PLL code [I] |
| `MIPIRX_RX_ATB_LINE`, `RX_HSDDR_CLK_M/P`, `RX_OSC_CLK_M/P` | undriven resolved nets | analog test outputs [I] |
| `MIPIRX_DATAP/DATAN0..3` (PLL IO bumps) | Z | bump model has an empty evaluator (`top/top_cells.h`) [C] |
| `scan_mode` | 0 | selects every reset/clock scan mux (IF:775, 887, 2888, 2892) [C] |
| `scan_set_rst_n` | 1 | only used through `scan_mode` muxes [C] |
| other `scan*` inputs | 0 | netlist / unused when `scan_mode`=0 [I] |
| `mipirxphy_bisten` | 0 | turns the BIST machine and the PLL on (IF:3650, BIST:453-465) [C] |
| `cont_en`, `forcetxstopmode_0`, `turndisable_0`, `turnrequest_0` | 0 | straight to the netlist [I] |
| `mipi_reset_n` | 1 | only the PLL scan reset (TOP:88) [C]; PLL unused |
| `sysc2mipirxphy_*` PLL shadow inputs | 0 | PLL powered down (§3.3) [I] |
| `basedir` | **1** | gives `direction`=1 [M]; mapping inside the netlist [U] |
| test interface `testclk/testen/testdin` | 0 | no register write is required [I]; `testdout` readable |

### 3.2 Sequence

Implemented by `DphyAdapter` (`phy/include/fx1_csi2_phy/dphy_adapter.h`),
the only writer of the PHY control inputs. In the IP it is driven by
PHY_CONFIG and the PHY reset sequencer of the controller (P08–P10); the M1
fixture `DphyStartup` is the same adapter switched on at t = 0.

| # | Action | Wait / exit condition | Evidence | Conf. |
|---|---|---|---|---|
| 1 | Supplies and tie-offs of §3.1; `cfg_clk` running | — | | C |
| 2 | `cfgclkfreqrange`, `hsfreqrange`, `enableclk`=1, `enable`=lane mask (lane 0 always), `basedir`=1 stable | before step 5 | IF:2773, 4600+ | I |
| 3 | `shutdownz`=0, `dphy_reset_n`=0, lanes driven LP-11 (pull strength) | ≥ 2 cfg_clk | `resetz = rstz & shutdownz` (IF:2872) | C |
| 4 | `testclr` 1 for 4 cfg_clk, then 0, then 2 cfg_clk | tester registers power up X; only testclr loads defaults (TST:1653, 13658+; `vl_logic.h:27-31`) | C; width: no minimum in model |
| 5 | `shutdownz`=1, `dphy_reset_n`=1 | reset synchroniser releases after 4 cfg_clk edges (SYNC:29-43) | C |
| 6 | — | `state_main`: 0→1→2→4→5→6→9→A→7, then `enable_lanes_int`=1 | PWR:99-216 | M |
| 7 | — | `stopstateclk`=1 and `stopstatedata_x`=1 for enabled lanes; `direction`=1 | | M |

Measured at cfg_clk 25 MHz, `hsfreqrange`=0x09, 4 lanes, lanes in LP-11
(`fx1_dphy_startup`, 2026-10-19), times from reset release:

| State | Meaning | Entered at (µs) |
|---|---|---|
| 1 | bias / bandgap timer | 0.18 |
| 2 → 4 | termination calibration | 11.14 / 11.18 |
| 5 | clock-lane offset calibration | 113.02 |
| 6 | data-lane offset calibration | 176.02 |
| 9 | DDL tuning | 204.18 |
| A | skew-backward (compatibility mode) | 305.02 |
| 7 | active; `enable_lanes`=1 304 cfg_clk later | 328.38 / 340.50 |

Startup of 340.5 µs simulated takes ~3 s wall on the review host; reset
recovery (`fx1_dphy_reset`) repeats the identical trace.

### 3.3 PLL

The PLL is **not** part of RX startup: `onpll` comes from the BIST machine
(IF:3756-3757, BIST:453-466) or test-register overrides that reset to 0
(TST:13828, 13838), so `pll_pwron`=0, `pll_reset`=1 (PLLS:229, 244) [C].
PLL clocks reach only the loopback serializers (CLKL:137-143, 305-345) [C/I].
The README mentions "PLL lock" in its verified 300 µs startup; that run must
have enabled the PLL through BIST or test registers [U]. The VP startup above
completes without the PLL [M].

Simulation cost (M5a, decision P13): with the PLL off, the PLL core model still
toggles its VCO (`pllcoremodel.cpp`: `half_period` = 1.8 when `on` = 0;
`proc_initial_10952` toggles whenever `half_period` > 0) [C], about 10^7 delta
cycles per simulated ms [M]. `DphyRxWrapper` suspends the PLL core's processes
while its `on` input (`c_I01PLLCOREMODEL_on`, the AND of `clk_stable` and
`supply_ok_vco`, mpll_ana.cpp:438) is not 1 and resumes them with a warning
when it rises. Its outputs stay at their time-0 values while suspended; no
RX path uses them (above) [C], and the whole regression passes with gating [M].

### 3.4 Reset pulse (P04)

The model has no minimum low width for `rstz`/`shutdownz`; release is
synchronised over 4 cfg_clk edges and cfg_clk must run [C]. The adapter
pulse is `max(200 ns, 4 cfg_clk)` = 200 ns at 25 MHz, followed by the full
calibration sequence (§3.2, ~340 µs). `reset_in_progress` covers the pulse
only; the controller observes PHY readiness through stop-state, not through a
timer.

## 4. HS receive

Implemented by `bfm/include/fx1_csi2_bfm/dphy_tx_bfm.h`; verified by
`tests/test_phy_hs_receive.cpp` (labels `fx1_phy_slow`). The HS framing logic
(LP state machine, settle timer, sync detector, lane aligner) is in the core
gate netlist; the facts below were traced in the netlist and then confirmed by
simulation [M].

### 4.1 Pad stimulus

| Item | Contract | Evidence |
|---|---|---|
| LP vs HS | decided by drive strength only: ≥ `ST_STRONG` reads LP-0 on both lines; LP levels are written at `ST_PULL` | LPRX `comp()` cpp:129-133 [C] |
| LP glitch filter | two 13 ns inertial stages: an LP state must last > 13 ns, latency 26 ns | LPRX h:43-46, cpp:85-97 [C] |
| HS levels | strictly complementary strong P/N written by one process at the same timestamp (P=1,N=0 is bit 1); P==N gives X on the CMOS path used for codes ≤ 0x2c | `cml2cmos_core.cpp:107-136` [C] |
| LP-00 | strong HS-0 is read as LP-00; pull 0/0 is not needed | [C] |
| Clock lane entry | LP-11 → LP-01 (50 ns) → HS-0 (300 ns) → DDR clock; `rxclkactivehs` rose ~350 ns after the clock lane's internal `hsactiverx` (one trace); the tests wait 1 µs | [M] |
| Data lane entry | LP-11 → LP-01 (50 ns) → HS-0 128 UI → sync 0xB8 → payload | [M] |
| Bit order | each byte LSB first on the wire; `rxdatahs[0]` = first bit | [M] |
| Clock phase | any launch phase 0…7/8 UI received correctly (zero-delay model) | `fx1_dphy_hs_phase` [M] |
| End of burst | trail = inverted last bit, 56 UI by default (D-PHY THS-TRAIL ≥ 60 ns + 4 UI = 52 UI at 800 Mb/s) then LP-11; the clock lane must keep toggling until the lane closes | [M] |
| Unused pads | LP-11 at pull strength | [M] |
| `basedir` | must be 1, otherwise lane 0 is a transmitter and its LP-TX collides with the BFM | IF:3206, 3210 [C] |
| Lane 0 | must be enabled whenever any lane is used: `rxbyteclkhs` is lane 0's word clock | core_part1:2877 [C] |

### 4.2 PPI behaviour observed

| Case | Result at the PPI | Test |
|---|---|---|
| Normal burst, 1/2/4 lanes | one `rxsynchs` pulse per lane, then `rxvalidhs`=1 with the exact payload bytes | `fx1_dphy_hs_{1,2,4}lane` |
| Lanes 1–3 lag lane 0 by 3/8/5 UI | every lane exact | `fx1_dphy_hs_skew` |
| Two bursts, clock running between | both exact | `fx1_dphy_hs_multi` |
| 1-bit leader error | `rxsynchs` and `errsoths`, data aligned | `fx1_dphy_hs_sot_err1` |
| ≥ 2-bit leader error (no offset within 1 bit of 0xB8) | `errsotsynchs`, no `rxsynchs`; **`rxvalidhs` still asserts with misaligned data** — the controller must discard the burst | `fx1_dphy_hs_sot_err2` |
| 1 payload bit inverted on the wire | exactly that byte differs | `fx1_dphy_hs_corrupt` |
| PPI timing at burst start | `rxactivehs` and `rxsynchs` rise on the **same** `rxbyteclkhs` edge; the first payload byte follows one cycle later with `rxvalidhs` (MAS/PG202: `rxactivehs` first, first byte with `rxsynchs`). Handled by Lane Management (spec_decisions C18) | M1 trace, `fx1_csi2_rxphy_frame_*` |
| Trailing bytes | the PHY does not strip the trail: 6–8 bytes (trail + LP-11 detection) follow the payload with `rxvalidhs`=1 at 800 Mb/s; the controller ignores them through the packet word count | all HS tests |
| Leader search | a corrupted leader can lock on a later bit offset that is within 1 bit of 0xB8 (reported as `errsoths`, data misaligned) | found while writing `sot_err2` |

### 4.3 Multi-lane alignment and test register 0x09 (VP decision – pending IP confirmation)

The PHY contains its own lane aligner. With the reset value of test register
0x09 (`noalign_error`=0, "all errsotsynchs_* behave as a single link",
TST:6473-6475), **any lane whose sync is detected earlier than lane 0's — by
as little as 1 UI — makes every lane report `errsotsynchs` and no
`rxsynchs`**, although each lane's data is correctly aligned
(`fx1_dphy_hs_lead4_link`). Lanes lagging lane 0 are accepted. The same
failure occurred without any wire skew after the clock lane had been stopped
and restarted, because the per-lane word-clock dividers restart with
different phases (traced: `align_done` of lane 3 one byte clock before lanes
0–2). `aligner_dk_cnf` (0x09[6:5]) values 0–3 made no difference [M].

With 0x09 = 0x10 (`noalign_error`=1) each lane reports only its own
alignment: leads of 1 and 4 UI (the values measured) and all clock-restart
cases pass
(`fx1_dphy_hs_lead4`, `fx1_dphy_hs_phase`). The VP startup writes 0x09 = 0x10
through the test interface after `testclr` and checks it by read-back. The
controller (MAS Lane Management) deskews lanes itself using each lane's
`rxsynchs`, so per-lane reporting is what it needs. Who programs this register
on silicon (boot code, PHY wrapper, controller) is an open SoC/IP item.

Test interface protocol used [C, read-back verified]: address on the falling
edge of `testclk` with `testen`=1 (TST:13642-13648), data on the rising edge
with `testen`=0 (TST:14155+), `testdout` returns the selected register
combinationally.

## 5. Known limitations

- `cl_ulpsactivenot` / `ulpsactivenot_x` not exported (TOP:233, 433-437): ULPS not supported through the adapter.
- One common `rxskewcalhs`.
- `MIPIRXPHY_CONT_DATn_oen` constant (README: `IODIR.v` missing); outputs sunk.
- No timing checks, zero specify delays (README).
- `hsfreqrange` ↔ rate association unresolved at gate level (§2.3); 0x09 receives 800 Mb/s correctly with a 128 UI leader [M], which is consistent with, not a proof of, the inferred table.
- Escape mode, ULPS, deskew bursts (`rxskewcalhs`) and the `hsfreqrange` ≥ 0x3d CML path are not exercised.
- Tclk-miss (stopped clock lane ending a burst) threshold not characterised; the BFM always keeps the clock running until data lanes are in LP-11.
