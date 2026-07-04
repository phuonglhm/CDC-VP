# VP_FX1 Firmware Toolchain / ABI

Fixed by the SoC CPU model (Bremen `riscv_vp`). Do not deviate.

| Item | Value |
|---|---|
| Cross toolchain | xPack `riscv-none-elf-` GCC (RISC-V bare-metal) |
| ISA | `-march=rv32imac` (RV32 I/M/A/C) |
| ABI | `-mabi=ilp32` |
| Link base | `0x80000000` (RAM0 FW window, 16 MiB) |
| Boot | ELF loaded directly to RAM; PC = ELF entry (`_start`) |
| Privilege | M-mode only (no MMU, single hart) |

## Install the toolchain

```bash
# xPack GNU RISC-V Embedded GCC, e.g. extracted under /opt/toolchains
export PATH=/opt/toolchains/riscv-none-elf/bin:$PATH
riscv-none-elf-gcc --version
```

## Notes

- Wrong `-mabi` produces a link error or a silent crash on the VP.
- Registers are 32-bit little-endian; use the `mmio_*` helpers in `hal/mmio.h`.
- The VP is loosely-timed (functional). Validate register semantics, IRQ
  ordering, and data movement - not cycle timing or WCET.
