# adc_tlm

Memory-mapped TLM-2.0 ADC model ported from the FU2 ADC register model.

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
