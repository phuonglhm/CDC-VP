# WDT0/1 V1.2 functional contract

Common Spec v2.0 has precedence over FU3 HAS v2.0, then WDT HDS V1.2
(37 pages). Private specifications are not distributed. See the shared
[decision record](../../bus/docs/COMMON_REBASELINE.md) for all ambiguities.

## Register and reset contract

Local offsets: LOCK 0x1000, LOAD 0x1004, VALUE 0x1008, CONTROL 0x100C,
RAW 0x1010, MASK 0x1014, CLEAR 0x1018, DBG_STAT 0x101C. Other offsets
error; no incomplete Version/capability register is fabricated.

- LOCK resets unlocked/read zero. Full-key 0x1ACCE551 unlocks; other nonempty
  writes lock. LOCK always remains writable; other writes while locked are
  ignored with OK response. Read access remains available. Zero strobes do not
  alter the lock. A partial key locks and cannot accidentally unlock.
- preset_n resets programming state: LOAD=0xFFFFFFFF, CONTROL=RESEN,
  LOCK unlocked and debug halt clear. It aborts pending APB work.
- wdogres_n clears runtime state; VALUE holds 0xFFFFFFFF during reset and reloads
  retained LOAD on release. It preserves LOAD, CONTROL and LOCK. With INTEN retained, counting
  restarts after reset. Cold reset asserts both domains.
- Common Full strobes override the HDS's unsupported-partial-write statement.
  LOAD/CONTROL merge internally. CLEAR with any enabled byte clears the warning
  and reloads VALUE before critical reset; all-disabled writes do nothing.
  RO writes remain ignored. Reads only update enabled payload bytes.

## Runtime behavior

INTEN enables counting/IRQ. A loaded N first expires after N+1 eligible watchdog
edges, sets TIMEOUT_SEEN and reloads LOAD. A second expiry clears TIMEOUT_SEEN;
RESEN additionally requests reset. With RESEN clear, reload and begin a new
two-stage sequence. LOAD/CLEAR service the watchdog before reset is pending.
Writing zero LOAD while enabled reports the first timeout immediately; the
next watchdog edge can trigger the second stage. INTEN clear holds VALUE at
LOAD and clears timeout state before critical reset.

Reset requests remain sticky until watchdog reset. RESDIV is storage only,
following the unsupported-programmable-pulse statement rather than inventing
pulse widths from the contradictory register prose. Software cannot retract
an asserted request. DBGEN pauses counting and publishes optional wdogpause;
the undefined external debug-active pin is not invented.

## Functional timing and integration

Constructor arguments select watchdog tick, APB wait count (default ten), and
independent APB period (default 5 ns). Default watchdog tick is 5 ns. MMIO samples
VALUE at completion; detailed two-FF/FIFO snapshot phases are abstracted. Calls
are serialized, consume incoming delay once and abort across reset. Debug is
untimed, read-only and non-destructive. Both optional compatibility ports may
remain unbound; i_wdt_other_unlock has no V1.2 effect.

WDT1 uses PLIC ID 18. Common leaves WDT0's exact INTMAP/NMI assignment and
reset escalation unresolved; standalone fixtures label any assumed route.
The platform owns reset scope, source conditioning and CPU interrupt delivery.
