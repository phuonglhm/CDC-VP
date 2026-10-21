# Register map

The 2026-10-21 PDF pp.13-16 uses local CSR **indices**, not an agreed software byte map.
The VP defines byte offset = index * 4 (AES-R03 resolution for simulation).
Add SoC base `0x40000000` when accessing through the FU3 bus.

| PDF index | VP byte offset | Register | Access | VP reset / meaning |
| --- | --- | --- | --- | --- |
| 0x00 | 0x00 | NAME0 | RO | 0x61657320 (`aes `) |
| 0x01 | 0x04 | NAME1 | RO | 0x20202020 |
| 0x02 | 0x08 | VERSION | RO | 0x302e3630 (`0.60`) |
| 0x08 | 0x20 | CTRL | command/readback | 0; INIT bit 0, NEXT bit 1 |
| 0x09 | 0x24 | STATUS | RO | 1; READY bit 0, VALID bit 1 |
| 0x0a | 0x28 | CONFIG | WO | 0; ENCRYPT bit 0, KEY256 bit 1 |
| 0x10..0x17 | 0x40..0x5c | KEY0..7 | WO | 0; key words, MSW first |
| 0x20..0x23 | 0x80..0x8c | BLOCK0..3 | WO | 0; input words, MSW first |
| 0x30..0x33 | 0xc0..0xcc | RESULT0..3 | RO | 0; output words, MSW first |

CONFIG, KEY and BLOCK read as zero. CTRL bits [3:2] mirror CONFIG [1:0].
CTRL bits [1:0] expose an accepted command until the next rising clock edge.
Only enabled low-byte command bits trigger an operation. CTRL upper bits
are read-only; CONFIG bits [31:2] are ignored.

All other aligned slots below 0x400 read zero and ignore writes. Writes to
RO registers are ignored. Reserved bits read zero. Reset defaults beyond
identification are VP decisions because the PDF does not provide a complete
reset-value table.

INIT snapshots and expands the key, clears VALID and lowers READY until
initialization completes. NEXT requires a successfully initialized schedule,
snapshots the input, clears VALID and lowers READY. Completion atomically
updates all four result words and sets READY and VALID. Reading results
does not clear VALID. Old result words remain visible while a new operation
is running, so software must check VALID.

With READY=0, writes to KEY, BLOCK, CONFIG and nonzero CTRL commands return
`TLM_GENERIC_ERROR_RESPONSE` without mutation. INIT|NEXT together selects
INIT; CTRL=0 is a no-op. NEXT before INIT returns an error. These sequence
rules are VP policies for cases the PDF leaves unspecified.

Byte strobes merge into stored key/input/config words, including WO registers;
they do not merge against zero-valued readback. All-disabled writes are
no-ops. Disabled read lanes retain the caller's bytes.

`ResultReadback::ObservedRtlMux` changes only result readback: RESULT1/2 read
zero, RESULT0 and RESULT3 return the least-significant word. RESULT0 is an
inference from the PDF's assignment of 128-bit `result_reg` to a 32-bit read
path. This option does not emulate the wrapper's address defect or the old
reset defect still listed in Appendix B. Reset follows section 6.2: synchronous
active-low. Reset values/retention remain model choices.

Malformed commands, lengths/streaming widths, addresses and byte enables
return the corresponding TLM error. A null data pointer, reset cancellation
or illegal operation sequence returns a generic error. The socket validates
before state changes, disables DMI, and supports only non-destructive debug
reads; debug writes fail. Debug reads consume no simulated time.
