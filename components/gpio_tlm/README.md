# gpio_tlm

Minimal memory-mapped TLM-2.0 GPIO block: 32 pins, single port. Added for the
VP_FX1 ROM-code boot flow (boot-mode strap on pin 1); general-purpose enough
for later use.

## Register Map

| Offset | Name | Access | Description |
|---:|---|---|---|
| `0x00` | VALUE | R | Pin levels. Input pins reflect external stimulus (`set_pin()`); output pins read back OUT. |
| `0x04` | OUT | R/W | Output latch, effective on pins whose DIR bit is 1. |
| `0x08` | DIR | R/W | Direction: bit=1 output, bit=0 input. Reset: all inputs. |

All accesses are 32-bit aligned words. No interrupt output in this revision
(the platform PLIC slot stays reserved, same precedent as PWM/CMU).

## External stimulus

Board straps / testbenches / CLI inject input-pin levels through the C++ API:

```cpp
cdc::components::gpio_tlm gpio("gpio0");
gpio.set_pin(1, true);   // boot-mode strap high
```

`set_pin()` only affects pins configured as inputs; outputs read back OUT.

## Usage

```cpp
bus.add_target(0x10160000, 0x1000).bind(gpio.socket);
```

## Boot-strap ABI (VP_FX1)

Pin 1 is the ROM-code boot-mode strap ("Port A pin 1" in the firmware team's
boot-sequence diagram):

- pin1 = LOW  (default): ROM jumps to the application in internal flash.
- pin1 = HIGH: ROM enters the UART/SPI download probe loop.

The VP_FX1 platform exposes this as `--boot-pin high|low`.
