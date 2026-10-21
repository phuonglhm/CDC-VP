# FU3 MAILBOX - SystemC/TLM

Functional model of the FU3 inter-CPU mailbox: one block with two APB endpoints, port 0 for CPU0 (CVA6, MAILBOX_CVA6) and port 1 for CPU1 (Ibex, MAILBOX_IBEX), two 32-bit FIFOs of 32 words, thresholds, sticky W1C interrupts, PSLVERR on invalid accesses and one reset per port.
Addresses and the Ibex interrupt follow FU3 HAS v2.0 and Common Spec v2.0. Registers and behavior follow the Mailbox HDS V1.
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
mailbox.presetn0(rst_mailbox_cva6);                      // CRM SW_RST_N_1.mailbox_cva6 & system reset
mailbox.presetn1(rst_mailbox_ibex);                      // CRM SW_RST_N_1.mailbox_ibex & system reset
mailbox.irq_ibex(irq_mailbox_ibex);                      // -> intmap.sources[fu3_plic::kSrcMailboxIbex]
mailbox.irq_cva6(irq_mailbox_cva6);                      // no consumer in HAS v2.0: CVA6 polls STATUS
```

Firmware: the sender writes FIFO_WR while STATUS.FULL is 0 and the receiver reads FIFO_RD while STATUS.EMPTY is 0.
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
