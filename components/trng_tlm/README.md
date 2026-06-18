# trng_tlm

Memory-mapped TLM-2.0 TRNG model based on the specification of ARM TrustZone TRNG True Random Number Generator.

## Register Map

| Offset | Name | Access | Description |
|---:|---|---|---|
| `0x00` | CONTROL | R/W | bit0 START, bit1 ADC_EN. START self-clears after conversion. |
| `0x04` | STATUS | R/W1C | bit0 EOC. Cleared by DATA read or W1C. |
| `0x08` | DATA | R | 12-bit conversion sample. Reading clears EOC. |
| `0x0C` | INTR_ENABLE | R/W | bit0 enables EOC interrupt. |

`irq_out` is asserted when `STATUS.EOC && INTR_ENABLE.EOC`.

## Usage

```cpp
#include <adc_tlm.h>

cdc::components::adc_tlm adc("adc");
sc_core::sc_signal<bool> adc_irq;

bus.add_target(0x10060000, 0x1000).bind(adc.socket);
adc.irq_out(adc_irq);
plic.irq_in[source_id - 1](adc_irq);
```

## Note
- TRNG's scan mode is not supported for this tlm, so there is no scan signal input.
- This TLM uses rand() for simple modelling since simulating real inverters is slow, and thus
defeats the point of  using TLM in the first place.
