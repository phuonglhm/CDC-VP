# FU3 Common Spec v2.0 behavioral baseline

Updated 2026-10-21. Applies to bus/A2P/PERIBUS, Timer0-3, WDT0-1,
UART0-2, SPI0-3, I2C, HyperBus/AP_HBCTRL/HyperRAM and RNG only.
AP_HBCTRL is the controller name used by the Common workbook (sometimes
written AP_HPCTRL in discussions). It is implemented in `hyperram`.

## Authority and sources

The supplied FTEL_FU3_Common_Spec_v2.0 workbook takes precedence, then the
existing FU3 HAS v2.0, then the latest supplied IP detail where higher-level
documents are silent. The private documents and extracts are not distributed.
IP baselines are Timer HDS V1.0, WDT V1.2, UART V1.3, SPI V1.0, I2C V1.1,
existing HyperRAM controller/vendor documents, and RNG V1.0. UART V1.3 retains
the reviewed V1.2 behavior. No new RNG health/test registers are invented.

## Fabric and endpoint changes

- Common MemoryMap and PLIC Connections select PERIBUS_1 directly from
  SYSBUS_1 through A2P_PP1. PERIBUS_0 stays behind SYSBUS_0/A2P_PP0. The
  earlier decision to route both bridges through SYSBUS_0 is superseded.
- CVA6 cannot access Ibex program SRAM or its debug-control aperture. Ibex
  can stage and verify CVA6 SRAM before handoff, then loses access to it.
  Ibex program SRAM becomes read/execute after handoff. MemoryMap row 62
  requires these checks in the bus in addition to CPU PMP.
- The top 4 KiB of each program SRAM is a separate debug-code partition;
  normal firmware images have at most 60 KiB. Normal CPU requests cannot
  access this partition except Ibex trusted staging data accesses. `Access::debug`
  identifies trusted CPU debug-mode requests; it must come from a platform
  adapter, not an untrusted firmware register. DBG loader access is separate.
  CPU debug-mode accesses are read/execute only; debug mode does not grant
  write permission. Trusted staging can read back debug bytes for verification.
  The bus preserves the complete 64-KiB physical windows. Image/ELF size and
  CPU privilege/PMP configuration remain platform/CPU responsibilities.
- MMIO, debug control and HyperRAM reject instruction fetches. QSPI XIP is
  executable according to MemoryMap. SRAM data and fetch permissions differ
  by CPU and boot phase. `reset()` reopens staging, after coordinated drain.
- AP_PLIC requires aligned complete 32-bit beats with every byte enabled,
  across its entire aperture. Narrow/partial requests are rejected before any
  target access. The fixed priority/pending/enable map is taken from Common.
  The read-only peer PLIC currently chooses 0x3000/0x3004 for the still-TBD
  threshold/claim map; integration tests use this provisional peer ABI.
- `IrqAdapter` provides a functional sticky pulse-to-level handshake. The
  platform calls `acknowledge()` after source service and before PLIC complete.
  A held-high source wins acknowledgment; reset clears pending state. This API
  is a VP integration choice, not a new CSR or a claim of physical CDC timing.
  The real Timer1/PLIC test uses it rather than relying on an untimed PLIC
  reacting to an arbitrarily short source pulse.

## Full strobes: decisions for old no-PSTRB HDS profiles

Common BUS INTF Full strobes and the user's full-support clarification override
older endpoint restrictions. The models apply byte enables internally; the
fabric never reads a CSR to synthesize a partial write.

| Endpoint | Selected behavior |
|---|---|
| Timer | Masked counter/control/compare writes merge pre-edge storage. A nonzero compare write clears count; zero strobes cause no write event. Existing edge priority and raw IRQ behavior remain. |
| WDT | Masked LOAD/CONTROL writes; clear occurs only with an enabled lane. WD_LOCK unlock requires a full-word key; nonempty partial keys lock, zero strobes do nothing. |
| UART | One 8-bit register per access. Only its payload low-byte enable qualifies a write/read side effect; other enabled bytes do not address adjacent registers. Disabled low-byte reads do not pop RBR or clear status. |
| SPI | Configuration/W1C masks remain lane-qualified. TXDR is write-only: each nonzero mask pushes one zero-filled masked word, including narrow writes; zero strobes never push or overflow. No previous FIFO word is read or merged. |
| I2C | Storage merges inside the model. CMD and status W1C act only on enabled bits. Zero strobes do not launch/clear commands. Existing address aliases remain. |
| AP_HBCTRL | Shadow/config storage merges internally; command/W1C operands contain only enabled bits. Zero strobes cause no command. Memory WSTRB remains independent. |
| RNG | Existing CONTROL/COMMAND/DELAY byte qualification already matches Full strobes; core behavior is unchanged. |
| AP_PLIC | Full-word-only exception is explicitly required by Common PLIC Connections row 60. |

UART register-byte offsets are preserved. For word-register endpoints, bus
subword adaptation generates the proper little-endian lanes. TLM read enables
are a data-selection contract and are not physical APB PSTRB signals.

## Unresolved source details and selected VP treatment

| Item and source | Treatment |
|---|---|
| PERIBUS_0 says APB without a version | Keep both bridges APB4 under the earlier user decision. Generic APB does not mandate APB2. |
| CPU outstanding is 2 without a per-direction definition | Keep the conservative combined limit; DMA has separate read/write gates. FIFO and missing endpoint defaults remain documented VP choices. |
| PLIC compact context offsets TBD | Follow the current read-only peer's provisional 0x3000/0x3004; do not call them an approved firmware ABI. |
| Common INTMAP/WDT0 mapping TBD | Do not change the out-of-scope INTMAP. Component IRQs stay independent; any existing WDT0-to-NMI fixture is labeled provisional. WDT1 uses PLIC ID 18. |
| WDT0 reset escalation TBD | Emit the IP reset request; platform chooses reset scope. WDT1 recovery goes to CRM. No reset/NMI wiring is invented inside the IP. |
| WDT V1.2 pp.15/17/24 vs init example | Reset LOCK unlocked/read zero, following detailed lock behavior and reset key. Ignore the contradictory initial locked-read example. |
| WDT V1.2 pp.20-21 vs loose CSR reset prose | preset_n owns LOCK/LOAD/CONTROL/debug request; watchdog reset clears runtime state and reloads retained LOAD, preserving programmed settings. |
| WDT V1.2 second timeout and RESEN=0 | Clear TIMEOUT_SEEN on every second timeout per p.22; only request reset when RESEN is set. Without reset, reload and start a fresh two-stage sequence. |
| WDT reset-pulse/RESDIV conflict | Retain a sticky request until watchdog reset; store RESDIV without inventing unsupported pulse widths. |
| WDT debug-active input is not defined | Retain software DBGEN pause selection and expose optional wdogpause. No new physical debug-active pin is guessed. |
| WDT wait-state and snapshot details | Default ten APB periods, separately configurable from watchdog tick; VALUE samples at functional transaction completion. Exact CDC snapshot timing is not claimed. |
| WDT incomplete Version row vs unsupported ID list | No fabricated ID/capability CSR; unlisted offsets remain errors. Offsets 0x1000 onward are local offsets, not double-added. |
| UART IER/IIR, DMA, parity and baud ambiguities | Keep documented functional IER/IIR, combined DMA conditions and parity behavior. Exact baud/oversampling correlation remains deferred; no RS485 mode is invented. |
| QSPI XIP permission vs SRAM-only CVA6 executable list | Permit XIP fetch at fabric per explicit MemoryMap R/X and boot mode. Entry-PC/mode selection belongs to the out-of-scope boot/CPU platform. |
| Boot payload 256 KiB vs 60-KiB slots | Preserve 64-KiB decode/debug partitions; require bounded or segmented platform loading. This work does not introduce a boot loader. |
| HyperRAM controller CSR/READY/recovery omissions | Retain labeled provisional controller ABI and initialization state machine, now with Common strobes and NX. |
| RNG equations/entropy and optional IRQ slot | Retain supplied bit-source interface and documented collection choices. Platform keeps an unsupported/unapproved IRQ slot disabled; this is not a new RNG health-test implementation. |

## Reproducible validation

From the bus directory, for each static/shared configuration:

```bash
cmake -S tests/scoped_suite -B build/common-static -DSYSTEMC_HOME=/path/to/systemc -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build/common-static --parallel 4
ctest --test-dir build/common-static --output-on-failure -j 4
cmake --install build/common-static --prefix "$PWD/build/common-static/install"
cmake -S tests/scoped_consumer -B build/common-static/ip-consumer -DCMAKE_PREFIX_PATH="$PWD/build/common-static/install;/path/to/systemc"
cmake --build build/common-static/ip-consumer --parallel 4
ctest --test-dir build/common-static/ip-consumer --output-on-failure -j 4
```

Repeat with a separate `build/common-shared` directory and BUILD_SHARED_LIBS=ON.
Also run the ordinary installed bus consumers and FX1 coexistence commands in
the README. All generated peer builds stay inside the selected bus build tree;
DMA0, PLIC, CPU, CRM, SYSCSR, DBG and other owners' sources are not modified.
Test success validates these functional contracts, not a full firmware boot.
