# ROM-Code Boot Flow — Hardware Change Plan & Status (VP_FX1_Full_SoC)

> AI/engineer context file. Read this before touching anything related to the
> VP_FX1 boot flow. It records WHY each change exists, the frozen ABI
> decisions, exact status of the work, and what remains.
>
> Status as of 2026-07-07: **Phase 1 COMPLETE — built, unit-tested, and E2E
> boot flow verified on the VP (both strap values).** Nothing has been
> committed, packed, or delivered to the firmware workspace yet.

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
| VP CLI | `--fw <bootrom.elf>` (ROM code, entry 0x0), `--int-flash <app.bin>` (raw binary → IFLASH), `--boot-pin high|low` (default low) | |
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

### Phase 2 — UART0 host-tool input path
- uart2_tlm RX side already modeled (FIFO, RXRIS/RTRIS). Missing: VP-process
  backend feeding bytes into uart0 RX. Plan: TCP socket option
  (`--uart0-socket <port>`) + file replay (`--uart0-rx-file <f>`) for CI.
- Without this, the diagram's UART download branch cannot be exercised.

### Phase 3 — NOR flash behind SPI0 (hardest)
- `spi_tlm` has `to_peri_socket` (word-at-a-time MOSI/MISO initiator) but NO
  chip-select modeling; `flash_nor_tlm` only speaks qspi_tlm transactions.
- Plan: add CS output to spi_tlm (from SSP register or explicit), add a
  `from_spi_socket` byte-stream face to flash_nor_tlm (state machine:
  CMD 0x03 + 3 addr bytes → data out; reset on CS deassert). Separate flash
  instance + image file (`--spi-flash <bin>`), independent from QSPI0 flash0.
- Until then the diagram's SPI probe can never answer (spi0 is bound to a
  dummy sink in the top).

### Packaging / delivery (after phases build & pass)
1. Update `docs/peripheral_memory_map.md` (+BOOTROM, IFLASH, GPIO0 rows) and
   the firmware-facing header blocks in it.
2. Update `platforms/VP_FX1_Full_SoC/configs/default.yaml` memory_map section.
3. Regenerate / hand-update fx1 BSP headers (`soc_memory_map.h`); run
   `tools/check_regs_drift.sh`.
4. Add `tools/fx1_sdk_template` regref for gpio_tlm (fx1 side:
   `sw/bsp/VP_FX1_SOC/regref/gpio_tlm/README.md`).
5. `tools/pack_fx1_sdk.sh --build --fx1 ../fx1` → delivers new VP + BSP to the
   firmware workspace (`vp/VERSION` records the CDC-VP sha).

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
