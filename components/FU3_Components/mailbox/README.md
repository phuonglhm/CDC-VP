# FU3 MAILBOX - SystemC/TLM

Functional model of the FU3 inter-CPU mailbox: one block with two APB endpoints (MAILBOX_CVA6, MAILBOX_IBEX), two 32-bit FIFOs of 32 words (CVA6 -> Ibex and Ibex -> CVA6), thresholds, sticky W1C interrupts and PSLVERR on invalid accesses.
Addresses and the Ibex interrupt follow FU3 HAS v2.0. Registers follow the RTL team's IP_Feature_Spec until a mailbox HDS exists.
See [MODEL_CONTRACT.md](docs/MODEL_CONTRACT.md) for exact behavior and the open points.

## Layout

- `include/fu3_mailbox/registers.h`: C-compatible bases, offsets and bits.
- `include/fu3_mailbox/mailbox.h`: the `fu3_mailbox::Mailbox` model.
- `tests/`: direct behavior and actual FU3 bus integration (shared `bench.h`), `run_all.sh`.
- `tests/consumer/`: validation through the installed CMake package.

## Connect to the bus

```cpp
#include <fu3_mailbox/mailbox.h>
fu3_mailbox::Mailbox mailbox{"mailbox"};
bus.target("MAILBOX_CVA6").bind(mailbox.cva6_socket);   // PERIBUS_1, 0x5002_0000, local offsets
bus.target("MAILBOX_IBEX").bind(mailbox.ibex_socket);   // PERIBUS_0, 0x4404_0000, local offsets
mailbox.reset_n(reset_n);
mailbox.irq_ibex(irq_mailbox_ibex);                      // -> intmap.sources[fu3_plic::kSrcMailboxIbex]
mailbox.irq_cva6(irq_mailbox_cva6);                      // no consumer in HAS: CVA6 polls STATUS
```

Firmware: the sender writes FIFO_WR while STATUS.FULL is 0; the receiver reads FIFO_RD while STATUS.EMPTY is 0.
For interrupts, set RD_THRESHOLDS / WR_THRESHOLDS and IRQ_ENABLE, then clear IRQ_STATUS by writing 1s in the handler.

## Build and test

Run everything (static/shared builds, both benches, installed consumers):

```bash
tests/run_all.sh        # SYSTEMC_HOME defaults to /usr/local/systemc-2.3.4
```

Set `BUILD_TESTING=OFF` for a standalone library build without the sibling bus.
Public target: `fu3::mailbox` and installed package: `fu3-mailbox`.

## Verification evidence

See [validation results](docs/VALIDATION.md) and [specification traceability](docs/TRACEABILITY.md).
