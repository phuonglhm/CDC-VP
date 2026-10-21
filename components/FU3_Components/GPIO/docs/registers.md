# GPIO register map

Offsets are local to **each** of GPIOA, GPIOB and GPIOC. Source: GPIO HDS
sections 6.2–6.5 and 8.2, PDF pages 10–14.

| Offset | Name | Access | Reset |
| --- | --- | --- | --- |
| 0x00 | PADDIR | RW | 0 |
| 0x04 | PADIN | RO | 0 |
| 0x08 | PADOUT | RW | 0 |
| 0x0c | INTEN | RW | 0 |
| 0x10 | INTTYPE0 | RW | 0 |
| 0x14 | INTTYPE1 | RW | 0 |
| 0x18 | INTSTATUS | Read-clear | 0 |
| 0x1c | POWEREVENT | RW | 0 |
| 0x20–0x3c, step 4 | PADCFG0–7 | RW | 0x02020202 |

All registers are 32 bits. Writes to PADIN and INTSTATUS are ignored, matching
the absence of APB error reporting in the IP. PADCFGn contains four six-bit fields
at byte offsets 0, 8, 16 and 24. Its writable mask is `0x3f3f3f3f`.
Reserved bits read zero. Pad configuration is exposed as 32 six-bit outputs per bank.

PADDIR and PADOUT drive separate signals. They do not gate input sampling or
each other. Direction polarity and pad feature meanings require the pad wrapper
and selected cell documentation. The subsystem passes PADDIR through as a
requested enable value only; it does not infer physical tri-state behavior.

For each pin, `(INTTYPE1 << 1) | INTTYPE0` selects:

| Mode | Condition, evaluated before each rising edge |
| --- | --- |
| 0 | High level: previous sample is 1 |
| 1 | Low level: previous sample is 0 |
| 2 | Rising edge: synchronized input is 1 and previous sample is 0 |
| 3 | Falling edge: synchronized input is 0 and previous sample is 1 |

INTEN gates each condition. With IRQ low, an event latches the complete enabled
condition vector and asserts IRQ. While IRQ is high, subsequent events do not
change INTSTATUS. A normal TLM read returns the pre-edge INTSTATUS then clears
status and IRQ at the access edge if IRQ was already high. A persistent level
can reassert on the following edge. An edge concurrent with clearing a pending
IRQ is lost. If IRQ was low and an event first arrives at the read edge, that
new event is latched, consistent with the HDS pending-IRQ clear condition.

POWEREVENT selects **raw**, unsynchronized input. When multiple bits are set,
the highest-numbered selected pin determines the result. Zero selection gives
zero output. It is neither an OR reduction nor a latched event.

The target decodes only address bits `[5:2]`: upper bits and `[1:0]` alias, as in
the GPIO RTL described by the HDS. A bus must restrict the peripheral aperture
and enforce alignment if the platform requires it. No address-range error is
invented inside this target.

The existing FU3 bus config supplies the following platform bases, independently
of the two IP specs:

| Bank | Base in `bus/src/config.cpp` | Window |
| --- | --- | --- |
| GPIOA | 0x50030000 | 64 KiB |
| GPIOB | 0x50040000 | 64 KiB |
| GPIOC | 0x440f0000 | 64 KiB |

No IOSEL or WAKE register address is defined by this component.

`Gpio` (TLM) and `ApbGpio` (signal-level APB) share this register behavior.
APB PRDATA follows the decoded word regardless of PSEL/PENABLE; changing its
address to INTSTATUS does not clear anything. Clearing requires a selected
read access edge while IRQ is pending. APB PREADY remains 1 and PSLVERR 0.
