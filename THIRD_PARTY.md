# Third-party components & licenses

CDC-VP's own source code is licensed under **Apache-2.0** (see `LICENSE`).
Known code dependencies listed below use permissive licenses. The upstream
SAURIA design uses Solderpad Hardware License v2.1 with an Apache-2.0 option.
Its license and provenance are kept under `licenses/`. This inventory does not
convert unverified media assets or incomplete copyright notices into cleared
material.

The external CPU core is **not** bundled in this repository. It is fetched into
`third_party/` (gitignored) by `tools/third_party/setup_third_party.sh`.

| Component | Where | Upstream | License | Used by |
|---|---|---|---|---|
| RISC-V VP (Bremen) | `third_party/riscv-vp` | `agra-uni-bremen/riscv-vp` | **MIT** | `riscv_vp` CPU backend |
| SystemC | `/opt/systemc-2.3.4` (host) | Accellera | Apache-2.0 | all |
| RISC-V toolchain | host install | xpack `riscv-none-elf` GCC | GCC runtime exception | firmware (`fw/`) |
| SAURIA NPU v4 | optional external `SAURIA_NPU_ROOT`; not bundled in public CDC-VP | [`bsc-loca/sauria`](https://github.com/bsc-loca/sauria) architecture, private SystemC implementation | `Apache-2.0 WITH SHL-2.1` upstream; private implementation is internal-only | optional `npu_tlm_v4_model`, internal VP binary |
| CMake 3.21.7 installer | `cmake-3.21.7-linux-x86_64.sh` | Kitware official binary release | BSD-3-Clause; see `licenses/CMAKE.*` | optional host build tool; not linked |
| VPU TLM 3.0 subtree | `components/vpu_tlm3.0` | CDC-VP Authors | MIT; see `components/vpu_tlm3.0/LICENSE` and `PROVENANCE.md` | optional VPU model |

The public repository ships only the CDC-VP adapter, register ABI, tests, and
upstream attribution. It does not ship the private SystemC source or an
NPU-enabled binary. See `licenses/SAURIA.PROVENANCE.md` for the upstream
revision and public/private boundary.

The ISP RAW inputs and architecture JPEG are project assets released under
Apache-2.0 by the CDC-VP Authors. Their identity hashes and license record are
in `components/isp_tlm/ASSET_PROVENANCE.md`.

## IP register models

The TLM peripheral models are **functional re-implementations** inspired by published
reference designs (ARM PrimeCell PL011/PL022/PL330/SP805, CryptoCell; lowRISC
OpenTitan clkmgr/pwrmgr/OTP/I2C/PWM — Apache-2.0). They are original SystemC code, not
vendor RTL, and are covered by this repository's Apache-2.0 license. Product/trademark
names are used only to describe the modeled programming interface.
