# WDT V1.1 requirement traceability

Primary source: `FTEL_CDC_WDT_Hardware_Design_Specification_v1.1.docx.pdf`; references below are physical PDF pages. FU3 HAS v2.0 supplies the address windows and interrupt routes. The behavioral model abstracts physical APB/CDC timing as documented in [MODEL_CONTRACT.md](MODEL_CONTRACT.md).

| Requirement | Evidence | Model behavior / existing scenario |
|---|---|---|
| WDT0/WDT1 map and routes | FU3 HAS v2.0; WDT HDS V1.1 pp.25-26 | Separate instances at `0x44050000`/`0x44080000`; WDT0 NMI, WDT1 PLIC 18. Integration uses fake route sinks. |
| Two-stage down-counter | HDS pp.12-14, 22-24 | LOAD reloads; first timeout sets TIMEOUT_SEEN/IRQ; second timeout requests reset when RESEN. |
| Service and interrupt clear | HDS pp.12-14, 26, 29 | LOAD or INT_CLR clears TIMEOUT_SEEN and reloads. |
| Reset request retention | HDS pp.13, 16, 22, 24 | Request stays asserted and counter freezes until watchdog-side reset. RESDIV is read/write storage only. |
| SYSCSR write protection | HDS pp.10-11, 14-17, 30-32 | `i_wdt_other_unlock` gates valid writes; locked writes return OK without changing state. |
| Counter and debug status | HDS pp.22-24, 27-29 | INTEN enables count; DBGEN halts; reset defaults restore LOAD/CONTROL/VALUE. |
| Register decode/access width | HDS pp.25-30 | Seven aligned 32-bit registers only; reserved/misaligned accesses map to TLM address errors. |
| CDC/APB latency | HDS pp.15-18, 20-21, 25 | Fixed configurable seven-functional-cycle TLM delay; phase-accurate synchronizers are outside this model. |
| SYSCSR lock ABI | WDT HDS pp.14-17; SYSCSR workbook v1 | WDT input is implemented; software-visible SYSCSR lock register/key remains unresolved in the supplied workbook. |

The V1.1 static/shared behavior and integration tests and both installed consumers
passed on 2026-10-20 (6/6 executions). The behavior suite includes independent
first/second timeout boundary checks. See [VALIDATION.md](VALIDATION.md) for
execution records, repaired defects and remaining platform integration limits.
