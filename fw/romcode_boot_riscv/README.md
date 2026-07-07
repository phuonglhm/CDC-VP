# romcode_boot_riscv — E2E firmware for the ROM-code boot flow

Test firmware for `docs/romcode_boot_hw_plan.md` on VP_FX1_Full_SoC:

- `bootrom/` → `bootrom.elf` (entry 0x0, runs from BOOTROM): reads the boot
  strap GPIO0 pin 1; LOW → jump to the app at IFLASH `0x0400_0000`; HIGH →
  the diagram's probe loop (send `'R'` over UART0, bounded 8 attempts; a
  response enters an echo "download" loop; SPI0 probe is Phase 3, stubbed).
- `app/` → `app.bin` (raw binary for `--int-flash`): XIP from the read-only
  IFLASH window, prints on UART0. No writable `.data`/`.bss` (flash is ROM);
  stack in SoC RAM.

Build (xpack riscv-none-elf, rv32imac_zicsr/ilp32; path baked into the
Makefile, override with `TOOLDIR=`):

```sh
make
```

Run matrix (from the CDC-VP repo root):

```sh
VP=./build-soc/platforms/VP_FX1_Full_SoC/vp_fx1_full_soc
FW="--fw fw/romcode_boot_riscv/bootrom.elf --int-flash fw/romcode_boot_riscv/app.bin"

# strap LOW: boot the internal-flash app
$VP $FW --boot-pin low                                   # -> "APP: boot flow PASS"

# strap HIGH + CI file replay: UART download branch
$VP $FW --boot-pin high --uart0-rx-file resp.bin --sim-ms 200

# strap HIGH + live host tool over TCP (sim waits for the client)
$VP $FW --boot-pin high --uart0-socket 5577 --uart0-wait --sim-ms 300 &
python3 host_tool.py 5577   # connect, send a response, read the echo

# strap HIGH, nobody answers: probe loop prints 8x 'R' then exits
$VP $FW --boot-pin high --sim-ms 300
```

Note: the probe timeout is a spin count (~4 ms sim time per attempt), so the
strap-HIGH cases need `--sim-ms` well above the default 5.
