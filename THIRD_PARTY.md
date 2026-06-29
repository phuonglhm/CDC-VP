# Third-party components & licenses

CDC-VP's own source code is licensed under **Apache-2.0** (see `LICENSE`). All
dependencies are **permissive** (no copyleft).

The external CPU core is **not** bundled in this repository. It is fetched into
`third_party/` (gitignored) by `tools/third_party/setup_third_party.sh`.

| Component | Where | Upstream | License | Used by |
|---|---|---|---|---|
| RISC-V VP (Bremen) | `third_party/riscv-vp` | `agra-uni-bremen/riscv-vp` | **MIT** | `riscv_vp` CPU backend |
| SystemC | `/opt/systemc-2.3.4` (host) | Accellera | Apache-2.0 | all |
| RISC-V toolchain | host install | xpack `riscv-none-elf` GCC | GCC runtime exception | firmware (`fw/`) |

## IP register models

The TLM peripheral models are **functional re-implementations** inspired by published
reference designs (ARM PrimeCell PL011/PL022/PL330/SP805, CryptoCell; lowRISC
OpenTitan clkmgr/pwrmgr/OTP/I2C/PWM — Apache-2.0). They are original SystemC code, not
vendor RTL, and are covered by this repository's Apache-2.0 license. Product/trademark
names are used only to describe the modeled programming interface.
