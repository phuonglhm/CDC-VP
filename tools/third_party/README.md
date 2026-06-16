# Third-party Dependency Setup

This directory contains developer setup scripts for upstream repositories that
are required to build `cdc-vp` but are not committed into this repo.

The source directories below are intentionally ignored by Git:

```text
third_party/RISC-V-TLM/
third_party/riscv-vp/
```

## One-time Setup After Clone

From the repository root:

```bash
./tools/third_party/setup_third_party.sh
```

Then source the build environment in each new terminal:

```bash
source ./tools/third_party/setup_env.sh
```

## Pinned Dependencies

| Dependency | URL | Commit |
|---|---|---|
| RISC-V-TLM | `https://github.com/mariusmm/RISC-V-TLM.git` | `4b949664194799ac5d3331d0ed408445f81accc8` |
| Bremen riscv-vp | `https://github.com/agra-uni-bremen/riscv-vp.git` | `48b2f5877b2368cc466fb0da155db349e676c0b0` |

## Policy

- Do not commit these upstream clones directly.
- Do not delete local changes inside `third_party/*` automatically.
- If a dependency commit changes, update `setup_third_party.sh` and this README
  in the same commit.
