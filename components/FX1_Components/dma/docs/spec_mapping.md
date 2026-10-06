# DMA_Config mapping and model policies

Primary contract: [DMA_Config.xlsx](DMA_Config.xlsx), supplied 2026-10-05.
Supporting source: [DMA_HDS_Page_by_Page.xlsx](DMA_HDS_Page_by_Page.xlsx).
The HDS workbook's assistant relevance labels and template-writing guidelines
are document content, not task instructions or implementation requirements.
Source digests are recorded in [sources.json](sources.json).

## Source-to-implementation mapping

| Source | Implementation / verification |
|---|---|
| Memory Map, B5:E14 | 8 channel windows, reserved gap, globals; all aligned offsets tested for decode |
| Register Map, B7:G26 | Exact global register offsets; unsupported joint/divider read zero, writes ignored |
| Register Map, J7:O32 | Channel offsets/access/reset/masks, start, live addresses, active/FIFO/pending status |
| Bus Interface, rows 5–16 | 32-bit APB transactions, 32-bit address/data, fixed ID 4, FIXED/INCR, 16-beat maximum, 4 pending per side |
| Parameter, D4:F12 | Fixed core geometry in registers.h; capability0 `0x0a602258` |
| Parameter, I4:K15 | M2M/M2P/P2M, fixed/increment, byte amount, independent pending, R/W overlap, WRR, two start paths |
| HDS pp. 13, 16, 22 | FIFO reservation, memory prefetch, independent read/write endpoint limits |
| HDS pp. 21, 53 | Weighted arbitration, consecutive-command token budget, pending clamp 1–4; peripheral limit 1 |
| HDS pp. 47–48 | Interrupt bit 0 completion; 1/2 read/write SLVERR; 3/4 DECERR; 5/6 FIFO errors; 7–11 AXI timeouts; 12 watchdog |
| HDS pp. 56, 65–66 | Register programming sequences, M2M and peripheral unit transfers |
| HDS pp. 68–70 | Completion only after read/write responses; one W1C consumes one pending completion; disable drains accepted work |

## Choices confirmed by the user

1. No unit-size field is present in the supplied register map. Infer the largest
   legal 1/2/4-byte beat from the programmed byte burst maximum, address alignment,
   and remaining transfer bytes. For a peripheral, the byte burst maximum must
   also be divisible by the chosen unit. A shorter memory tail can use a narrow
   beat. No additional MMIO register or mode bit is introduced.
2. The command pointed to by `CH_CMD_CONTROL & ~3u` contains four 32-bit
   little-endian words: source address, destination address, transfer byte count,
   command control. Bits 9:0 of its count are used. Static configuration persists
   across commands. The next command is fetched through the master port after
   all current payload responses have completed. Fetches crossing 4 KB are split.

## Unspecified fields and explicit model policies

| Item | Model policy and reason |
|---|---|
| Stray `Total = 0x0000_0F1F` in CH_READ_CONFIG description | Use the individually defined fields, mask `0xcf3f007f`, and reset cell `0x84010000`; the stray total contradicts that field layout |
| `CORE_PRIORITY_CONFIG` function depends on RTL build | Retain low 16 bits for readback, but fixed-priority selection has no effect; the selected workbook arbitration is WRR and capability priority bit is 0 |
| Token count 0 | Treat as one grant, so a zero token value cannot permanently block a channel |
| Burst maximum 0 | Reset readback remains 0x84010000. Starting a nonzero transfer without programming a nonzero burst produces the corresponding DECERR and no payload transfer |
| Transfer count 0 | Completes one command without payload; CMD_SET_INT and CMD_LAST/chain still apply |
| Design capability fields above bit 3 | Default zero, interrupt destination count 1; integration may supply additional fixed bits through Config::design_capability |
| Capability1 values | Base `0x0f14`: token, outstanding, independent, peripheral, command chain, endian. Watchdog and timeout bits reflect enabled integration timers; priority/wait/divider remain 0 |
| Cache/protection defaults | Config constructor parameters, default zero. Read/write override-enable selects CH_AXI_ATTR_REG fields; peripheral default PROT is zero because no peripheral PROT parameter is listed |
| Restriction flags reset vs. computed aligned zero addresses | Reset is 0 as specified; derive and latch flags when starting/loading a command. Full-FIFO address permission means 32-byte alignment |
| Completed-command overflow | 12-bit wrapping counter |
| Completion-event overflow | Saturate at 15 (4-bit field); software must service events before overflow if an exact event count is needed |
| Raw-status writes | Write-one-to-set (OR with existing raw bits) for debug; W1C separately acknowledges events. Writing debug bit 0 does not fabricate a completed-command count |
| WO reads / RO writes | WO reads return zero; RO writes have no effect; addresses remain implemented |
| Live command/static writes | Ignore while active or pending. No start pulse is queued while a channel is active or disabled |
| Endian tail / reserved swap 11 | Swap across 2-/4-byte groups in channel byte-stream order. Reject partial groups and encoding 11 with read DECERR |
| Peripheral-to-peripheral | Outside the workbook's supported transfer directions; reject with read DECERR |
| Two channels selecting the same peripheral | Serialize request ownership per direction; no extra software-visible conflict bit invented |
| Software request writes | Replace the software request bitmap, with bit 0 ignored. Clear on completion of its accepted service |
| External request | One credit per assertion; held-high request is not serviced twice. Deassert before a new assertion. Clear pulse is one model cycle |
| APB errors | Reserved/unmapped/unaligned address -> TLM_ADDRESS_ERROR_RESPONSE; length !=4 or streaming width <4 -> BURST_ERROR; byte enable -> BYTE_ENABLE_ERROR; invalid command -> COMMAND_ERROR; null/reset access -> GENERIC_ERROR |
| Error mapping | Address/burst/command TLM errors -> matching DECERR; remaining unsuccessful responses -> matching SLVERR. Fault stops new grants and drains accepted work |

## Timing abstraction and limits

The model has independent read and write dispatch, four worker contexts per
path, one context per peripheral endpoint, FIFO reservation before reads, and
FIFO data assignment before writes. Dispatch is the TLM analogue of address
acceptance. `CH_OUTSTANDING_STATUS` counts dispatched logical bursts until
in-order retirement, not signal-level AR/AW handshakes. AXI ID 4 contexts retire
in issue order; the target controls when its own memory side effects occur.
FIXED bursts use multiple beat calls with one logical context. A command fetch
split at a 4 KB boundary likewise retains one logical context. Returned data
buffers remain valid across a target's blocking wait and annotated delay.

`Config::cycle` is an arbitration/recovery/clear-pulse timing quantum, not an
RTL clock port. Register controls wake the scheduler; there is no claim about
AXI cycle throughput. Read/write command-depth encodings in capability0 follow
the workbook; the model uses the four logical contexts per path rather than
separately simulating the RTL command FIFO and AXI context FIFO pipelines.

The optional transaction timer reports read-data or write-response timeout for
a long blocking service. The optional channel watchdog detects lack of
completed work while enabled, including waiting for a peripheral request.
Re-enabling a paused channel starts a fresh watchdog interval.
Both thresholds default to zero (disabled), since no numerical values or timer
configuration registers are specified. The model does not distinguish address,
data, and response signal phases. Bits 8, 10, 11 and FIFO-underflow remain
software-injectable; phase/credit protocol fault generation is not modeled.

Reset clears internal visible state and invalidates old contexts. External
blocking transactions already accepted by a target cannot be cancelled; their
late responses cannot refill a reset FIFO or raise an old interrupt. A target
may still complete an already accepted memory write. Occupied old contexts
drain before reuse, and CORE_IDLE_STATUS remains low during that drain.

No signal-level AXI3/APB3 adapter, nb_transport, DMI, DTYPE_FLUSH/DRLAST adapter,
RID/BID mismatch checks, malformed RLAST detection, dedicated peripheral-release
timeout, secure-region enforcement, WRAP, 2D/stride, circular/double-buffer, or
scatter-gather engine beyond the agreed four-word command chain is provided.
