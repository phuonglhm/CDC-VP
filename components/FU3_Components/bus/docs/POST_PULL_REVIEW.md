# Post-pull compatibility review (2026-10-21)

Reviewed repository HEAD `dce8e88` against the Common rebaseline in `3a7e33a`.
The eight owned component trees are unchanged between these commits. Changes in
other owners' models are integration inputs, never a replacement for the supplied
Common workbook, HAS and applicable IP specification, in that order.

Re-extraction of Common v2.0 worksheet values and the supplied Timer, WDT V1.2,
UART V1.3, SPI V1.0, I2C V1.1 and RNG HDS text matches the preceding review.
The existing [Common decisions](COMMON_REBASELINE.md) and
[follow-up findings](FOLLOWUP_REVIEW.md) therefore remain applicable. This review
does not approve the provisional HyperRAM controller ABI or other source TBDs.

## Corrected bus compatibility

The pulled GPIO register adapter rejects any non-null TLM byte-enable pointer,
including an entirely enabled word. A2P now represents a four-byte beat whose
enables are all `0xFF` as an unmasked transfer. Both representations select the
same bytes. Caller payload fields are restored after transport. Narrow, sparse
and zero masks retain their original semantics; no CSR read-modify-write occurs.

The new read-only peer tests cover:

- GPIOA/B/C word writes and readback with explicit full enables through both A2P
  paths, including caller metadata restoration.
- Actual QSPI control and a four-beat CVA6 XIP fetch through SYSBUS_0, checked
  against an independent 16-byte image. This is a TLM fetch, not CPU execution.
- Rejection of sparse and zero masks by GPIOA/B/C and AP_QSPIC without changing
  their stored values. This last scenario characterizes a remaining owner gap;
  passing it is **not** proof of Common strobe conformance.

## Remaining incompatibilities outside the authorized implementation scope

Common BUS INTF row 14 (AP_QSPIC) and rows 26-28 (GPIOA/B/C) specify Full
strobes. The user's clarification includes partial/sparse masks and zero-write
suppression. The pulled GPIO and QSPI register adapters still reject partial
and zero masks with BYTE_ENABLE_ERROR. Full-word normalization fixes only the
equivalent unmasked case. These endpoints need owner corrections before complete
end-to-end Full-strobe conformance can be claimed. The bus must not hide this
gap with generic CSR RMW or by enabling disabled bytes.

The QSPI package also describes a newer 63-page HDS in its version record; that
document is not present among the supplied local QSPI PDFs. Its claimed revision
does not override Common. The XIP test verifies the current peer's integration,
not complete QSPI HDS compliance. GPIO, IO_PAD, QSPI and all other out-of-scope
sources remain untouched.

## Fresh verification

| Configuration | Scoped suite | Installed bus | Installed owned IPs |
|---|---:|---:|---:|
| Static | 48/48 | 6/6 | 14/14 |
| Shared | 48/48 | 6/6 | 14/14 |

FX1/FU3 shared coexistence also passes 1/1: 137 passing executions in total.
Counts include repeated configurations and the owner-gap characterization above;
they are not a percentage of specification coverage. Logs are ignored under
`bus/build/post-pull-static-final.log`, `post-pull-shared-final.log` and
`post-pull-coexist.log`. Generated peer libraries stay in the bus build directory.

Use the [rebaseline commands](COMMON_REBASELINE.md#reproducible-validation).
The scoped suite enables `FU3_BUS_BUILD_PULLED_PEER_TESTS` by default; it requires
the repository's QSPI, GPIO and IO_PAD source trees as read-only peers. Disable
that option for the preceding 45-case suite. The installed bus package gains no
dependency on those peer libraries.
