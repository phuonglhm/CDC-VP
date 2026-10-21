# Follow-up behavioral review (2026-10-21)

Scope is the eight owned component folders, following the Common rebaseline.
No other owner's implementation is modified. Private reproduction sources/logs
remain in the ignored bus build tree.

## Confirmed and corrected

**SPI APB reset during setup could launch a stale TXDR request after reset.**
The model originally captured reset epochs only after its APB setup delay and
mutex admission. A reset pulse entirely inside that delay was missed. A local
probe issued TXDR, reset APB before launch, then observed OK and TX FIFO level 1.
HDS p.16 says APB reset clears reg_req; only an access already running in the
system domain may complete there after APB reset.

The adapter now captures epochs at logical arrival, after incoming annotated
delay and before setup/admission. A changed epoch prevents launch, returns
GENERIC_ERROR and leaves the FIFO unchanged. Existing post-launch behavior is
preserved: commit once, report interrupted response, do not replay automatically.
`reset_phases` now covers the pre-launch reset pulse at APB periods 1, 10 and
13 ns in addition to existing post-commit reset cases; the installed corner
consumer also executes the new case. This changes coverage, not CTest case count.

## Remaining points requiring explicit interpretation

| Topic | Assessment |
|---|---|
| UART 32-entry physical FIFO with extended trigger 56 | HDS allows SIZE_E=5/6 but lists only 16/64-mode trigger tables. Current model and tests preserve literal trigger 56, making RDA/threshold DMA unreachable at depth 32; receive timeout can still request service. Auto-RTS also cannot reach that threshold. This is a documented source/profile ambiguity, not a confirmed arithmetic bug. Firmware should use a reachable trigger until an approved 32-entry profile is defined. |
| Debug-mode metadata | Bus protection relies on a trusted CPU adapter setting Access::instruction/debug. The bus tests exercise the flags directly; the out-of-scope CPU wrapper does not yet establish full debug-mode/PMP integration. Package tests do not prove an executed debug handler or complete boot-security handoff. |
| IRQ_ADAPT ownership | acknowledge() is a functional platform handshake after source service and before PLIC completion. It is not a hardware CSR defined by Common. The current fixture proves one Timer/PLIC path; every integrated pulse source still needs an explicit owner/acknowledgment path. |
| Timer/HyperRAM debug byte enables | Their transport_dbg reads fill the complete word, while normal transport honors read masks. Other models also honor masks on debug reads. This is an inspection-interface consistency question, not a demonstrated CPU/MMIO failure or new APB requirement. |
| Common/IP unresolved rules | PLIC context offsets, WDT0 INTMAP/reset scope, WDT reset pulse/debug pin/snapshot, HyperRAM controller ABI and RNG generation details remain as recorded in COMMON_REBASELINE.md. Passing a larger regression does not turn those choices into approved RTL behavior. |

No additional production defect was established in the reviewed bus permission,
strobe, Timer, WDT reset/lock, I2C and HyperRAM/RNG paths. This is a focused
review, not exhaustive interleaving or full-platform firmware certification.
