# HyperRAM model contract — provisional VP ABI v1

The user authorized an implementation with explicit decisions for missing requirements. Evidence: controller spec v0.13, CSR proposal v0.1 (references parent v0.14), S80KS2564 datasheet 002-31341 Rev. C, and FU3 architecture v2.0. Private sources remain excluded from the repository. The source documents' RTL approval gates are not a claim of hardware approval for this VP.

## Established requirements and model boundary

- One 256-Mb native-x16 device, 32-MiB array; AXI address/data 32, ID6, aligned 4-byte beats, INCR only, up to 256 beats, no crossing a 4-KiB boundary.
- Little-endian byte data and arbitrary WSTRB masks, including zero. One accepted burst executes as one memory transaction; no automatic splitting, retry, or rollback.
- Four read and four write admission slots; a FIFO serializes the shared protocol engine. APB maintenance commands use that same engine. Each accepted TLM request consumes its annotated delay once and yields while waiting.
- The bus defaults remain unchanged. `TargetConfig::preserve_burst` is opt-in; `configure_bus` enables it only for HYPERBUS. Without it, the original ingress beat decomposition cannot prove single-burst admission.
- The full request is available when TLM `b_transport` is called. This does not model AW/W arrival timing, WLAST, response-channel backpressure, or ID ordering beyond the existing bus ordering streams. Streaming underflow is represented by an explicit fault extension, not by inventing a W channel.
- The backing array is embedded in the controller implementation for functional verification. There is no electrical/pin-level HyperBus link, separate vendor device simulation, lane CDC, or GF22 PHY.

## Explicit decisions at specification gaps

Firmware owns initialization. It can read device registers using the known POR LC7/fixed bootstrap profile, configure the controller, and enable memory admission. Once active timing exists, device-register reads require matching device/controller LC and mode. A CR0 write has no initial latency requirement and may intentionally make the two configurations incoherent; apply matching controller timing before readback/resume.

Reset controller state is disabled, configuration invalid, shadow registers zero, diagnostics clear. Device POR registers are ID0=0x0E76, ID1=0x0009, CR0=0x8F2F, CR1=0xFFC2 for the default Industrial Plus profile (0xFFC1 for Industrial). Controller reset retains device memory and CR values. Device reset resets both configurations and invalidates memory, represented deterministically by zero fill; real hardware does not guarantee zero-filled RAM. Reset cancels active/queued model operations with error, and writes already completed remain unless the device itself resets.

READY requires enabled admission, valid active timing, matching device LC/mode, released resets, and no pending maintenance command. Disable rejects new requests but drains requests already admitted. QUIESCED means disabled, no admitted memory work, and no pending command. APB polling is independent of the memory-engine queue.

Apply commands snapshot their inputs at acceptance, then execute asynchronously under FIFO engine ownership. A second command while pending sets COMMAND_BUSY; it does not replace the first. Failed validation preserves the previous active configuration. DONE/COMMAND_ERROR describe the latest accepted command and remain until another command/reset. Accepted commands have one pending slot, so polling does not need a request tag in this provisional ABI.

Controller timers encode direct numbers of core periods. The selected device grade bounds tCSM to 1 or 4 us. LC validity follows the datasheet frequency limits for LC3..7. Whole-burst admission conservatively budgets setup + three CA cycles + twice LC + one cycle per 32-bit word + hold, even in variable mode. Recovery is outside the CS-low budget. These are abstract timing bounds, not validated physical timing. Firmware chooses nonzero setup/hold/recovery values; the model does not certify their electrical sufficiency.

CR0 requires reserved bits [11:8]=1, normal power, and a legal LC encoding. CR1 supports only normal full refresh and the static single-ended clock profile; its refresh-grade bits remain read-only. Unsupported low-power/partial-refresh settings are rejected. Virtual delay codes 0..31 per lane can be applied and read back; they do not represent characterized physical taps or successful calibration and do not affect READY.

Errors are sticky, with a level IRQ equal to ERRORS AND IRQ_ENABLE. The first fault while ERRORS is nonzero records local address, completed words, and whether a partial write is possible. Clearing all errors clears that record; clearing errors neither retries nor undoes memory writes. Events are ordered by SystemC process execution; a physical same-cycle CDC set/clear priority is not qualified by this LT model.

## Provisional controller CSR map

All registers are 32-bit little-endian, aligned, full-strobe APB accesses. Narrow/sparse CSR requests receive adapter errors; AXI memory strobes remain independently supported. Reserved addresses read zero and ignore writes. Debug CSR reads are untimed/non-destructive; debug writes and memory DMI are unsupported.

| Offset | Register | Behavior |
|---|---|---|
| 0x00 / 0x04 | ID / VERSION | RO, 0x48525650 (`HRVP`) / 1 |
| 0x08 | CTRL | RW bit0 enables new memory admission |
| 0x0C | STATUS | RO: READY0, PENDING1, BUSY2, QUIESCED3, VALID4, DONE5, COMMAND_ERROR6 |
| 0x10 | LATENCY | Shadow: LC[3:0], fixed mode bit8 |
| 0x14 | TCSM | Shadow 16-bit maximum CS-low cycle count |
| 0x18 / 0x1C / 0x20 | SETUP / HOLD / RECOVERY | Shadow 16-bit direct cycle counts |
| 0x24 | APPLY | Nonzero write snapshots all five timing fields; reads zero |
| 0x28 / 0x2C | ACTIVE_LATENCY / GENERATION | RO active LC/mode and successful apply count |
| 0x30 / 0x34 | DEV_ADDR / DEV_WDATA | Shadow selectors 0=ID0, 1=ID1, 2=CR0, 3=CR1; 16-bit data |
| 0x38 / 0x3C | DEV_CMD / DEV_RDATA | Write command 1=read, 2=write; RO completed result |
| 0x40 / 0x44 / 0x48 | ERRORS / ERROR_CLEAR / IRQ_ENABLE | RO sticky errors, write-one clear mask, RW enable mask |
| 0x4C / 0x50 | FAULT_ADDRESS / FAULT_WORDS | RO first-fault local address / completed word count |
| 0x54 / 0x58 / 0x5C | DELAY0 / DELAY1 / DELAY_APPLY | Shadow virtual codes; nonzero apply command |
| 0x60 | ACTIVE_DELAY | RO lane0[7:0], lane1[15:8] |
| 0x64 / 0x68 | TRANSACTIONS / LAST_WORDS | RO launched transaction count / most recent burst length |
| 0x6C | FAULT_FLAGS | RO bit0: partial write possible |

Error bits 0..7 are DECODE, PROFILE, NOT_READY, TIMING, CONFIG, UNDERFLOW, READ_TIMEOUT, COMMAND_BUSY. They are VP diagnostics, not approved hardware encodings. Generic TLM errors represent controller faults; there is no claim that a TLM payload reproduces every AXI RRESP/BRESP beat. Errors rejected upstream by the bus do not reach or set controller diagnostics.

## Fault injection

`Fault::refresh` requests doubled variable latency. `fail_after_words` terminates a transfer after the specified prefix: writes preserve committed enabled bytes; reads preserve the valid prefix and zero-fill enabled bytes of the failed suffix. The request returns error. No byte-accurate device commit acknowledgement, real elapsed no-progress timeout, or electrical failure detection is inferred.
