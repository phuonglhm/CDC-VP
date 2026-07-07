# ROM-Code Boot Flow — Hardware Change Plan & Status (VP_FX1_Full_SoC)

> AI/engineer context file. Read this before touching anything related to the
> VP_FX1 boot flow. It records WHY each change exists, the frozen ABI
> decisions, exact status of the work, and what remains.
>
> Status as of 2026-07-07: **ALL PHASES + PACKAGING COMPLETE.** Phases 1–3
> committed; packaging (docs, yaml, BSP headers, regref, AI_CONTEXT) staged
> into fx1 via `tools/pack_fx1_sdk.sh --build --fx1 ../fx1` and smoke-tested
> there (uart_hello + full boot-flow matrix on the delivered binary).
> NOTE: the first pack ran from a dirty tree (`vp/VERSION` = 661367a predates
> the packaging edits) — re-run the pack after committing so VERSION matches.

## 0. Repo topology — two projects, one direction of flow

| | **CDC-VP** (this repo) | **fx1** |
|---|---|---|
| Path | `~/Desktop/VP_INTER/upgit/CDC-VP` | `~/Desktop/VP_INTER/upgit/fx1` |
| Role | VP **development**: SystemC/TLM sources of all IP models (`components/`), platform tops (`platforms/`), CPU wrappers (`cpu_models/`), unit tests, SDK packaging | SDK **consumer / firmware workspace**: receives a prebuilt VP binary + docs + BSP; firmware team writes drivers & bootloader against it |
| Hardware changes | **All made here** (source of truth) | **Never** — `vp/` and `sw/bsp/` are delivered artifacts; hand-edits get overwritten by the next SDK pack |
| Build | `cmake --build build-soc --target vp_fx1_full_soc` | no VP build; only firmware `make` (xpack riscv-none-elf) |
| Firmware-owned dirs | `fw/` (VP-side test firmware only) | `sw/drivers/`, `sw/bootloader/` |

Delivery flow (one-way):

```
CDC-VP  ──(tools/pack_fx1_sdk.sh --build --fx1 ../fx1)──►  fx1
   sources, models, platform      vp/bin (binary+libsystemc), vp/configs,
   build-soc/, tests              vp/doc, sw/bsp (headers+regref+startup)
                                  vp/VERSION = CDC-VP commit sha
```

Rule of thumb: if a change touches an address, a register, or an IRQ, it
starts in CDC-VP and reaches fx1 only through the pack script. If fx1's
headers and the VP binary ever disagree, the binary (i.e. CDC-VP source at
`vp/VERSION`) wins.

## 1. Background / decision

The firmware team specified a ROM-code boot sequence (diagram:
`fx1 workspace: vp/doc/VP_FX1_SOC/ROMCode Boot Sequence.png`) and the project
decision is: **the boot sequence is the standard; the VP hardware is changed
to match it** (not the other way around).

The sequence, in words:

1. Reset → System Init (clock, WDT, "SysTick" ⇒ on this RISC-V SoC: CLINT
   MTIME, microseconds).
2. Read boot-mode strap "Port A pin 1".
   - strap **LOW** → jump to the application in **internal flash**.
   - strap **HIGH** → probe loop:
     a. send request over **USART0** (19200 8N1) to a PC host tool; if a
        response arrives → UART bootloader loop (download image).
     b. else send NOR READ (CMD 0x03) over **SPI0 master**; if a response
        arrives → SPI bootloader loop.
     c. else repeat.

## 2. Gap analysis (platform before this work)

| Diagram requirement | VP_FX1_Full_SoC before | Resolution |
|---|---|---|
| GPIO boot strap pin | No GPIO IP at all (PLIC 24-31 "reserved for GPIO") | NEW `gpio_tlm` component, pin 1 = strap |
| Internal flash + jump to app | No internal flash; only NOR behind QSPI0; no XIP | NEW ROM window "IFLASH" 4 MiB @ `0x0400_0000` |
| ROM code at reset | No memory at 0x0; CPU kResetPc=0x8000_0000, `--fw` ELF entry wins | NEW ROM "BOOTROM" 64 KiB @ `0x0000_0000`; ROM-code ELF built with entry 0x0 and loaded via `--fw` |
| SPI0 master reads NOR flash (CMD 0x03) | SPI0 `to_peri_socket` bound to a dummy sink; flash only behind QSPI0 | Phase 3: attach a NOR model behind SPI0 (needs CS + byte-stream protocol) |
| PC host tool over USART0 | uart2_tlm has RX FIFO+IRQs but the VP process has NO host input path (stdout print only) | Phase 2: add RX backend (TCP socket + file replay) |
| WDT, timer init | wdt_tlm + CLINT already exist | no HW change |
| "Disable unused IPs" | clkmgr has no real gating | no HW change; cosmetic step |

## 3. Frozen ABI decisions (do not change silently)

| Item | Value | Rationale |
|---|---|---|
| BOOTROM | base `0x0000_0000`, 64 KiB, read-only | doc already said "optional first-stage boot ROM" at 0x0 |
| IFLASH (internal code flash) | base `0x0400_0000`, 4 MiB, read-only, executable | free window below PLIC; do NOT reuse `0x2000_0000` (reserved for future QSPI XIP) |
| GPIO0 | base `0x1016_0000`, size 0x1000 | next free instance-1 slot after ADC0 |
| GPIO0 registers | `0x00 VALUE` RO, `0x04 OUT` RW, `0x08 DIR` RW (1=out, reset all-in) | minimal; 32-bit word access only |
| GPIO0 IRQ | none in this revision | same precedent as PWM0/CMU0; PLIC 24 stays reserved |
| Boot strap | GPIO0 **pin 1**; LOW (default) = boot internal flash app; HIGH = probe UART/SPI download | matches diagram "A1 != HIGH → Jump to App" |
| VP CLI | `--fw <bootrom.elf>` (ROM code, entry 0x0), `--int-flash <app.bin>` (raw binary → IFLASH), `--boot-pin high|low` (default low), `--uart0-socket <port>` (TCP host-tool bridge on 127.0.0.1, bidirectional), `--uart0-wait` (block sim at t=0 until the client connects), `--uart0-rx-file <f>` (deterministic RX replay for CI), `--spi-flash <bin>` (image for the NOR behind SPI0, independent from QSPI0's flash0) | |
| SPI0 chip-select | vendor register `SSPCSR` @ SPI0+`0x28`, bit0: 1 = assert (line low), reset 0 = deasserted; optional `cs_n` port on spi_tlm | PL022 has no SW CS; NOR command framing needs one (command ends on CS deassert) |
| SPI0 NOR protocol | 8-bit frames; only CMD `0x03` + 3 addr bytes → sequential data out; other opcodes dead (0xFF) until CS deassert | minimal set required by the boot diagram |
| ROM loading mechanism | ELF loader writes through bus `transport_dbg`; memory_tlm debug writes now bypass `read_only` (backdoor). Functional writes to ROM still fail. | standard TLM debug-transport convention |
| PLIC sources | unchanged (1..23 assigned, 24-31 reserved) | |

Firmware-facing consequence (for the fx1 SDK headers, not yet regenerated):

```c
#define CDC_BOOTROM_BASE   0x00000000u  /* 64 KiB ROM, ROM-code entry     */
#define CDC_IFLASH_BASE    0x04000000u  /* 4 MiB internal code flash      */
#define CDC_GPIO0_BASE     0x10160000u
#define CDC_GPIO_VALUE     0x00u        /* RO pin levels                  */
#define CDC_GPIO_OUT       0x04u
#define CDC_GPIO_DIR       0x08u        /* 1=output, reset: all inputs    */
#define CDC_GPIO_BOOT_PIN  1u           /* boot strap: LOW=app, HIGH=download */
```

## 4. Changes already written (Phase 1 — built & verified 2026-07-07)

All paths relative to CDC-VP repo root.

| File | Change |
|---|---|
| `components/memory_tlm/src/memory_tlm.cpp` | `transport_dbg` write no longer blocked by `read_only_` (backdoor for image loaders; comment explains) |
| `components/memory_tlm/include/memory_tlm.h` | header comment documents the backdoor semantics |
| `components/gpio_tlm/**` | NEW component: `include/gpio_tlm.h`, `src/gpio_tlm.cpp`, `tests/test_gpio_tlm.cpp`, `tests/CMakeLists.txt`, `CMakeLists.txt`, `README.md`. Pure register block, `set_pin()` C++ API for external strap stimulus |
| `components/CMakeLists.txt` | `add_subdirectory(gpio_tlm)` after adc_tlm |
| `platforms/VP_FX1_Full_SoC/src/vp_fx1_full_soc_top.cpp` | +`bootrom` (memory_tlm ROM 64 KiB @0x0), +`iflash` (memory_tlm ROM 4 MiB @0x0400_0000), +`gpio0` @0x1016_0000; bus `num_targets` 22→25; banner updated; new methods `load_int_flash()` (reads raw .bin, backdoor `load()`, errors if >4 MiB) and `set_boot_pin()` (gpio0 pin 1) |
| `platforms/VP_FX1_Full_SoC/src/vp_fx1_full_soc_top.h` | declares `load_int_flash()`, `set_boot_pin()` |
| `platforms/VP_FX1_Full_SoC/src/main.cpp` | parses `--int-flash`, `--boot-pin` |
| `platforms/VP_FX1_Full_SoC/CMakeLists.txt` | links `cdc::components::gpio_tlm` |

## 5. Remaining work

### Phase 1 finish — DONE 2026-07-07
1. ✅ Built `vp_fx1_full_soc` in `build-soc/` (env: `export CC=/usr/bin/gcc
   CXX=/usr/bin/g++ PATH=/usr/bin:/bin:$PATH` first — default PATH g++ is a
   broken Synopsys wrapper). `build-soc` was reconfigured with
   `-DCDC_BUILD_TESTS=ON` (was OFF).
2. ✅ `build-soc/components/gpio_tlm/tests/test_gpio_tlm` → PASS.
3. ✅ E2E firmware added at `fw/romcode_boot_riscv/` (Makefile builds
   `bootrom.elf` entry 0x0 + `app.bin` for IFLASH, xpack toolchain path
   baked in). Verified:
   ```
   ./build-soc/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc \
       --fw fw/romcode_boot_riscv/bootrom.elf \
       --int-flash fw/romcode_boot_riscv/app.bin --boot-pin low
   ```
   `--boot-pin low`  → "BOOTROM: strap LOW → jump" + "APP: boot flow PASS"
   (app executes XIP from read-only IFLASH).
   `--boot-pin high` → "BOOTROM: strap HIGH → probe UART0/SPI0 download
   (not modeled yet)". Both paths print via UART0.

### Phase 2 — UART0 host-tool input path — DONE 2026-07-07
- NEW component `components/uart_host_tlm/` (`cdc::components::uart_host_bridge`,
  + unit test `test_uart_host_bridge`): no bus presence, binds on the UART's
  pin side (`rx_out` → `UartTLM::rx` sc_buffer, `tx_in` ← the uart TX signal).
  Backends selected before `sc_start()`:
  - `listen_on(port, wait_for_client)` — TCP server on 127.0.0.1; client bytes
    → RX FIFO, firmware TX forwarded back (bidirectional, non-blocking,
    reconnect allowed). `wait_for_client` blocks the sim at t=0 (sim time
    races wall clock, so interactive tools should use it or a long --sim-ms).
  - `replay_file(path, start_delay)` — deterministic CI replay.
  No line-rate modeling: 10 µs/byte injection pacing, 100 µs socket poll.
- Platform: `uart0_host` instance in the top, always bound, idles unless
  configured. CLI: `--uart0-socket <port>`, `--uart0-wait`, `--uart0-rx-file`.
- E2E verified with `fw/romcode_boot_riscv` (bootrom probe branch now real:
  strap HIGH → send 'R' over UART0, bounded 8-attempt probe loop, response →
  echo download loop):
  - file replay: `--boot-pin high --uart0-rx-file resp.bin --sim-ms 200` →
    "UART response -> download mode, echo: …" + "download done".
  - TCP: `--uart0-socket 5577 --uart0-wait` + mock host tool (python) →
    host saw the 'R' probe + all TX, ROM echoed the host's bytes back.
  - no backend: probe loop exits after 8 attempts; strap LOW unchanged.

### Phase 3 — NOR flash behind SPI0 — DONE 2026-07-07
- `spi_tlm`: vendor register `SSPCSR` @0x28 (bit0 = CS assert) + optional
  active-low `cs_n` port (`SC_ZERO_OR_MORE_BOUND`, existing bindings
  unaffected). Register readable; reset deasserts.
- `flash_nor_tlm`: new `from_spi_socket` byte-stream face (spi_tlm frame
  protocol: TLM write, 2-byte payload, low byte MOSI/MISO, 8-bit frames) +
  `spi_cs(bool)` C++ chip-select. State machine idle→addr(3B)→data; unknown
  opcode → dead (0xFF) until CS deassert. DESELECTED = frame left untouched
  (MISO tri-state → master sees its own bytes): deliberately preserves the
  legacy loopback-dummy behavior so `fw/spi_test_riscv` (which never asserts
  CS) still passes — verified PHASE 1-3 PASS on the platform. BOTH sockets
  are now `simple_target_socket_optional` so an instance can serve either
  face alone. Unit test extended (deselected echo + no decode, read,
  CS-abort/restart, unknown opcode).
  (Found while testing, unrelated: `fw/spi_test_riscv/linker.ld` is broken at
  HEAD — `//Author` comment lines from commit 146a4c4 are invalid ld syntax.)
- Top: `spi_flash0` (16 MiB, erased 0xFF unless `--spi-flash <bin>`) replaces
  the spi0 dummy sink; `spi0_cs_n` signal (SC_MANY_WRITERS: written from
  spi0's reset method AND from register writes in the CPU process) bridged to
  `spi_flash0.spi_cs()`. spi1 keeps the dummy.
- Bootrom E2E: SPI probe implemented (CR0 8-bit, CPSR=2, SSE, CS assert,
  CMD 0x03 + addr 0 + 8 dummy frames, CS deassert; "present" = any first byte
  not 0xFF/0x00 — real ROM must validate an image header instead, plan §6).
  Verified: `--boot-pin high --spi-flash img.bin` → dumps the image's first
  8 bytes ("SPI download mode"); UART present + SPI present → UART wins
  (diagram order a before b); nothing attached → 8 probe rounds then exit;
  strap LOW unchanged. Regression: test_spi_tlm, test_qspi_tlm,
  test_flash_nor_tlm, test_gpio_tlm, test_uart_host_bridge all PASS.

### Packaging / delivery — DONE 2026-07-07
1. ✅ `docs/peripheral_memory_map.md`: BOOTROM row made real + IFLASH + GPIO0
   rows, SPI0 SSPCSR/NOR note, ROM-code boot flow in the boot-assumptions
   table, integration-notes rows (GPIO0, BOOTROM/IFLASH backdoor, SPI_FLASH0),
   C-define block (+GPIO0/IFLASH/GPIO regs/boot pin/SPI_CSR).
2. ✅ `configs/default.yaml`: +bootrom, +iflash, +gpio0 (and the previously
   missing adc0) memory_map rows.
3. ✅ `fw/common/include/soc/soc_memory_map.h` (BSP ABI source): BOOTROM_SIZE,
   IFLASH_BASE/SIZE, GPIO0_BASE, GPIO reg offsets + BOOT_PIN, SPI_CSR.
   `tools/check_regs_drift.{sh,cpp}` extended to static_assert the GPIO
   offsets against gpio_tlm — PASS.
4. ✅ regref: automatic — the pack script copies every component README;
   fx1 now has `regref/gpio_tlm/` and `regref/uart_host_tlm/`.
   Boot-flow docs added to `tools/fx1_sdk_template/AI_CONTEXT.md` (new
   "ROM-code boot flow" section + map rows + gotchas), template `README.md`,
   and `run_vp.sh` usage examples.
5. ✅ `tools/pack_fx1_sdk.sh --build --fx1 ../fx1` ran clean (drift check
   PASS). Verified in fx1 on the DELIVERED binary: uart_hello smoke PASS,
   strap LOW → IFLASH app PASS, strap HIGH + `--spi-flash` → SPI download
   PASS. Re-run the pack after committing these packaging edits so
   `vp/VERSION` matches the shipped headers/docs.

## 6. Firmware-side gaps the hardware will NOT fix (relay to firmware team)

1. The probe loop (UART→SPI→repeat) has **no timeout/retry limit**; if System
   Init really enables the WDT, the loop will trip it. Needs an exit path.
2. **No image validation** before "Jump to App" (magic/checksum in an image
   header). Recommend a header: magic + size + entry + checksum, both for
   IFLASH app and downloaded images.
3. Baud 19200/8N1 on the VP is cosmetic (no line-rate modeling): the host
   protocol must be framed/verified at the protocol level, not by timing.

## 7. Related workspaces / files

- Firmware/SDK workspace ("fx1"): `~/Desktop/VP_INTER/upgit/fx1`
  - boot diagram: `vp/doc/VP_FX1_SOC/ROMCode Boot Sequence.png`
  - VP runner: `sw/bootloader/test/VP_FX1_SOC/run_vp.sh`
  - driver lib pattern to reuse in ROM code: `sw/drivers/sources/VP_FX1_SOC/lib/`
    (single trap handler + `irq_register()`; ADC driver as example)
- This repo: platform `platforms/VP_FX1_Full_SoC/`, components `components/`,
  pack script `tools/pack_fx1_sdk.sh` (builds via `build-soc/`).
