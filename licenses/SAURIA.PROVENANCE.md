# SAURIA NPU provenance and attribution

## Upstream project

- Project: SAURIA — Systolic Array tensor Unit for aRtificial Intelligence
  Acceleration
- Upstream: <https://github.com/bsc-loca/sauria>
- Revision inspected: `2bb469e4e4ab7413b88c985b4c83a98b9544c827`
- Upstream rights notice: Copyright 2022 Jordi Fornt Mas
- SPDX expression: `Apache-2.0 WITH SHL-2.1`
- License text: `licenses/SAURIA.SHL-2.1`
- Upstream `LICENSE` SHA-256:
  `f725a7d2ff028400f9f43618232222b6fe9a68563eecbce2e9666a9898153a07`

The upstream license states that a recipient may optionally treat an upstream
Work as released under Apache License 2.0. The full Apache License 2.0 text is
the repository root `LICENSE`.

## Optional external SystemC implementation

The public CDC-VP source tree contains an Apache-2.0 TLM adapter and
software-visible ABI, but it does not contain the SystemC implementation of the
SAURIA core. An authorized internal build can supply that implementation
through `SAURIA_NPU_ROOT` and enable it with
`CDC_ENABLE_SAURIA_NPU_V4=ON`.

The external SystemC implementation follows the SAURIA architecture, module
naming, and register addressing, but it is not a verbatim copy of an upstream
SystemC model—the official upstream implementation is primarily SystemVerilog
and Python.

This notice preserves upstream attribution and its license without publishing
private source history. A public CDC-VP source release does not include the
external implementation or an NPU-enabled binary. Before publishing such a
binary, the rights owner of the external SystemC implementation must separately
approve that distribution and confirm the license selected for that
implementation.
