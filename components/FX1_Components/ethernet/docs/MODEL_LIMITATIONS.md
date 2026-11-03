# Ethernet MAC VP: model contract and limitations

Reference: FTEL Ethernet MAC MAS v1.0.7 and `FTEL_IP_Ethernet_MAC_CSR_v1.0.xlsx`.
State: plan milestone M1 (2026-11-02). Integration into `platforms/fx1_soc`
(M2 onward) is not covered here.

The model is a functional, loosely timed (LT) TLM-2.0 model. It does not model
RMII di-bits, MDIO pin timing, the CDC block, word-level FIFOs or AXI channels,
IDs and outstanding transactions. It is not a timing or RTL sign-off reference.

## Timing contract

- A CSR access takes effect at the simulation time of its `b_transport` call.
  The target annotates the access latency (2 cycles, 5 for an RMON counter) and
  never calls `wait()`.
- A hardware event (DMA completion, error, MDIO completion, Rx MAC event)
  takes effect at the time stamp of the process that produces it.
- "The same clock edge" is the same simulation time stamp, in any delta order.
  Collisions on one edge resolve as MAS Table 6-47 row 11, Table 6-37 and
  Table 9-3, whatever order the SystemC scheduler runs the two processes in:

| Collision on one edge | Result |
| --- | --- |
| Sticky bit set by hardware + W1C | Bit stays 1 |
| New DMA error while ERR is set + W1C of ERR | ERR stays 1, ERRINFO loads the new error |
| `ETH_DMA_RX_DROP_CNT` increment + WC write | Counter becomes the increment of that edge |
| `ETH_MDIO_DATA` hardware load + software write | Hardware value |
| RMON event + CLEAR_ALL or read-clear | Counter restarts at that edge's increment; the read returns the value before it |
| Plain RMON counter read + event | Value before the event |
| RMON wrap + W1C of its overflow flag | Flag stays 1 |
| Completion + coalescing timer expiry | No pulse on that edge unless the count reaches COAL_CNT; the timer restarts |
| Completion + timer expiry + W1C of an earlier, unacknowledged DONE | DONE = (the completion reaches COAL_CNT) or (old DONE and no W1C on the edge) |

A completion that cancels a timer expiry on the same edge takes back the DONE
the expiry set with `CsrFile::hw_retract()`. The bit returns to its value
before that set, less any W1C software wrote on the edge, before or after the
expiry. The order of the DMA and timer processes inside one time stamp is up to
the SystemC scheduler. `eth_m1_csr-edge` covers every order of set, W1C and
retract; `eth_m1_coal-collision` covers what the public API can reach.

Software that reads DONE on that exact time stamp, between the expiry and the
completion, can see the transient value. It never outlives the time stamp, and
the IRQ output, driven one clock later, does not glitch.

`COAL_TIMER` written as 0 stops a running timer. Writing it nonzero again does
not restart it; the next completion does (MAS Table 9-3).

## CSR decode (MAS Table 6-47)

| Access | Core response |
| --- | --- |
| Offset that misses the decode (a hole) | `TLM_GENERIC_ERROR_RESPONSE` (SLVERR); read data 0; write has no effect, also with no byte strobe |
| Address bits [1:0] not 0 | The word that contains the address; data lanes stay those of the word |
| Address at or above `csr_window` (2^PARA_AXIL_ADDR_WIDTH) | `TLM_ADDRESS_ERROR_RESPONSE`: it never reaches the IP |
| `csr_window` above 64 KiB | Register space repeats every 64 KiB |
| Unicast entry at or above `rx_unicast_num` | Reads 0, writes ignored, OKAY in any RX_EN state |
| Write outside its window, with a strobe | SLVERR, no change |

"Write to a hole with no strobe answers SLVERR" is a model decision: row 2 does
not exempt strobe-less writes, and row 5 is read as applying to decoded
registers only.

On fx1_soc (plan M2) a `fx1::WordAccessGuard` stands in front of the MAC: only
an aligned 32-bit access with every byte enabled reaches it; anything else is
SLVERR, which the CPU takes as an access fault (the APB bridge already refuses
misaligned addresses). So on FX1 the CPU reaches the CSRs only with aligned
32-bit accesses; byte, halfword and misaligned accesses fault, and that policy
is tested (`fx1_smoke_eth_wiring`, `fx1_access_guard`). The core's own
byte-strobe and address-bit handling cannot be reached from the FX1 CPU and is
tested on the core alone.
That wrapper policy belongs to the SoC. The core decode above is what the IP does.

## Parameters

`EthMacParams::validate()` enforces MAS Table 5-1:

- `axi_data_bytes` is 4 or 8.
- `axi_max_burst` is a power of two in 1..16. ETH_DMA_BUILDCFG reports it as a
  log2; MAS allows any value in 1..16, so non-powers of two are refused by
  this model.
- FIFO depths are powers of two, at least 2 × (max_frame_len − 4).
- `max_frame_len` is in 64..65535.
- `rx_unicast_num` is in 1..4 and `mdio_clk_div` in 40..128.
- `poll_cycles` is a power of two in 256..8192.
- `tx_meta_depth` is a power of two above `tx_meta_reserve`.
- `dram_base` is a multiple of 4 KiB.
- `csr_window` is a power of two from 2^16 to 2^32.
- `clk_period` is positive.

Every `i_eth_rst_n` reset loads these from the parameters:
`ETH_RX_MAX_FRAME`, `ETH_TX_MAX_FRAME` (max_frame_len), `ETH_DMA_RX_BUF_SIZE`
(max_frame_len − 4) and `ETH_MDIO_CTRL.CLK_DIV` (mdio_clk_div). The workbook
reset values are those of the default parameter set.

Only the default set with a 32-bit AXI bus is regression-tested end to end. The
64-bit bus configuration is accepted but not verified. FX1 uses 32-bit.

## Resets

- **RX FIFO reset (ETH_CTRL[6] or [13], MAS Table 6-2 row 8).** The reset holds
  only the FIFO. The Rx MAC keeps receiving and counts each frame in RMON. A
  frame loses its FIFO commit, and has no DMA write-back, when:
  - its SOF found the FIFO in reset, or
  - a FIFO reset was asserted at any time before its EOF, even if already
    released.

  No overflow or drop event is counted. Frames stored but not delivered are
  lost. A frame open at the DMA is voided with no write-back.
- **Frame-level transport (review F10).** `send_frame()` hands the peer the
  whole DA..FCS at the frame's start, so a Tx MAC, Tx RMII or TX path reset
  after the start cannot take the frame back from the peer. Local completion and
  RMON are suppressed, but the peer may still hold a complete frame. An isolated
  `ETH_CTRL[8]` or `[10]` reset while TX_BUSY=1 (outside the software
  precondition of Table 6-2 rows 11 and 12) does not reproduce the RTL's
  truncated or tail frames. Lifecycle software must use the valid sequence:
  HALTED, then TX_EN=0, then TX_BUSY=0, then reset. Fault-injection results on
  this path describe the abstraction, not the RTL.

## Not covered by the M1 regressions

- RMON wrap through the MAC and the CSR path (RMON_OVF_STAT, CLEAR_ALL,
  read-clear). `eth_m1_rmon-wrap` checks the same-edge rules on `core::Rmon`,
  where TX_OCTETS wraps after 2,829,423 events of 1518 bytes. Through the MAC
  that would take about 2.8 million frames, beyond the 2-minute test budget.
- Cycle-exact allocation of events beyond the "same time stamp" rule. The model
  has no clock, so two events 1 ns apart are on different edges.
- 64-bit AXI and non-default FIFO, burst or poll parameters beyond reset values
  and validation.
- Delayed DES0 visibility after DONE (review F08) is not a property of the
  model's own timing; it is injected in front of memory: the driver bench
  (`tests/eth_drv_fixture.h`, `SlowWb`) and `fx1_soc --eth-wb-delay-us`.
  Combined with ring wraps: `fx1_eth_manager_wb-wrap` (3 turns, holds across
  the deadline, coalescing off and CNT/TIMER != 0) and
  `fx1_smoke_freertos_cli_eth_wbwrap` (FX1, coalescing 0). A non-zero
  coalescing setting with delayed write-backs is accepted on the host bench
  only, not on FX1/FreeRTOS, where the configuration is 0.

## FX1 integration (plan M2 to M5)

- DMA addresses: descriptors and buffers carry IP offsets; `dram_base =
  FX1_DDR_BASE` is added once, in the MAC. An offset past the 512 MiB aperture
  reaches no target: DECERR (tested with a buffer crossing the end of DDR).
- Every DMA write (payload and DES0) goes through a `WriteGuard` (master 0x103)
  so it cancels LR reservations in its granule and waits for AMO/SC brackets.
  Tested for the Ethernet DMA itself, AMO and SC brackets each against the RX
  payload write, the RX DES0 write-back and the TX DES0 write-back
  (`fx1_smoke_eth_{amo,sc}{,_rxdes0,_txdes0}`, one image per cell, writes held
  by the guard in each; DES0 cells use a free word of the descriptor's
  granule). Negative controls with `--eth-unguarded-dma`. A temporary mutation
  run (review GR01) removed the SC bracket, the AMO bracket, the guard for
  DES0-shaped writes, and the guard for the other Ethernet writes, one at a
  time: each failed exactly the cells of its row or column.
- SLVERR on a DES0 write-back (TX_DESC_WR_ERR) is tested on the driver bench
  (an error injected in front of memory), SLVERR raised by the DDR3 controller
  on an Ethernet write on FX1 (`fx1_smoke_eth_ddr_slverr`, `--ddr3-fault
  T:write-result:1` inside a 10 Mbps burst: RX_DATA_WR_ERR with DECERR 0,
  recovered). That test depends on T falling where only the Ethernet DMA
  writes DDR (calibrated, about 2 ms margin either side): a firmware that
  changes the burst's timing needs T re-chosen.
- Not tested (accepted limitations, Ethernet gap review): DDR reset,
  reinitialization or remap while the Ethernet DMA runs. Nothing coordinates
  the two: software must `eth stop` (both directions HALTED) before it resets
  or remaps DDR, and start again after. The PHY link is fixed for a run
  (`--eth-peer`); there is no runtime link down/up, and the manager does not
  watch the link (BMSR is read only by `eth probe`).
- The PHY's line side is `--eth-peer`: a loopback plug, an echo station or no
  cable. There is no host network (TAP, plan M6, out of scope).
- CPU cost on the VP: every instruction fetch goes to DDR (no cache, about
  45 ns each), and the HAL copies frames byte by byte. One 1 KiB frame with
  the payload check of `eth load` costs about 1 ms of hart 0; the service task
  (priority 3) can hold the CPU for several ms under that load. That exposed a
  false timeout of the I2C driver (fixed, `fw/fx1_soc/drivers/fx1_i2c.c`);
  other code with a budget shorter than such a preemption is equally exposed.
- FAULTED (a stop that cannot reach HALTED): the model itself always halts
  after a second HALT_REQ, but a DMA write held in front of memory keeps it
  from halting. Reached that way on the host bench (`fx1_eth_manager_faulted`)
  and on FX1 (`fx1_smoke_freertos_cli_eth_faulted`, `--eth-wb-delay-us
  100000`).
- Recovery from FAULTED (review N2): the only way out is both directions
  HALTED, which the MAC sets after the drain of every issued transaction, so
  no write of the old session can land in rebuilt rings. The global reset
  (ETH_CTRL[0]) is not used: MAS 9.1 / Table 6-2 row 3 require both
  directions HALTED before software clears it, and "abandoned" transactions
  still complete on the bus (seen on FX1: a DES0 of the old session landed on
  the RX ring rebuilt by a cold init). A device whose DMA never halts stays
  FAULTED; leaving that state needs a reset of the SoC or interconnect with a
  defined contract, outside this driver.
- A halting error hidden behind a class-4 error (review ER01, second round):
  ERRINFO keeps the first error, so it shows only as HALTED, with no new ERR
  or interrupt. The FX1 manager watches a direction after a class-4 clear,
  checks HALTED on every service pass (at least every 100 ms while running)
  and on every transmit submit. Between the halting error and HALTED nothing
  is observable: a frame submitted then can be accepted. What it is counted as
  depends on where it is at the recovery: still queued (OWN 1), it is
  abandoned and counted in `tx_dropped`; one the DMA completed or failed has
  its own completion or descriptor-error accounting; one already in the TX
  FIFO may be flushed (`tx_fifo_flushed`, a possible loss, not a count).
- The TX FIFO occupancy is not observable (no level register; TX_BUSY drops
  during the inter-packet gap). `eth_quiesce()` reports `tx_fifo_flushed`
  when frames written back were not seen leave (the Tx MAC idle on two
  samples 20 us apart, after HALTED, with TX_EN 1); it cannot count them.

## Regression helper note

`scripts/run_regression.sh` used to re-check its markers as
`prog | grep -q MARKER` under `set -o pipefail`. `grep -q` exits at the marker
while the program still prints, so the pipeline could fail on SIGPIPE. One run
aborted silently after all CTests passed. The suspected cause was not
reproduced deterministically.

The helper now captures the whole output first and then looks for the marker.
On a failure it prints the program, its exit status and the last 20 lines of
output, so a future abort says what failed.
