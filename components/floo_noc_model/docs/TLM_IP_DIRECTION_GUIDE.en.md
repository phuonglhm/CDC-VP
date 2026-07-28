# TLM IP Development Direction Guide

> **Machine-readable decision document.**
> Purpose: given an IP (and its spec), decide whether it should be modeled with
> **Direction 1 (functional / PV, untimed)** only, or should **also** get a
> **Direction 2 (cycle-approximate micro-architecture / timing)** model.
>
> This project develops **both directions in parallel**. Direction 1 is the
> **default baseline for every IP**. Direction 2 is an **additional** model built
> **only** for IPs that earn it. So the real per-IP question is:
> **"Does this IP ALSO need a Direction-2 timing model?"** — not "which one".

---

## 0. How an AI agent must use this file

### Input modes
- **PRIMARY — a single IP spec document:** the user pushes in **one IP's spec file**
  (usually a `.md`) and asks for a direction verdict. Read the spec, extract facts
  using Section **4B** (`SPEC EXTRACTION MAP`), run the algorithm in Section 4, and
  output the **single-IP block** defined in Section **6.1**.
- **SECONDARY — a list of IP names:** run the algorithm per IP and output the table
  in Section 6.

### Rules (both modes)
1. Run the algorithm in Section 4 (`DECISION ALGORITHM`) exactly.
2. **Cite evidence.** For every score, quote or reference the spec section it came
   from. If a score is not derivable from the spec, mark it `assumed` or
   `needs-context` — never present a guess as a fact.
3. **Do not invent micro-architecture facts.** If the spec is only a register map,
   Gate G1 fails → emit `NEEDS_INFO` and list what to request from HW. You almost
   never have enough to justify `H2_FULL` from a register-only spec — say so.
4. **Value questions (V1–V3) are usually NOT in a spec.** They are project /
   architecture context (who needs the numbers, is it a bottleneck, which decision
   depends on them). Ask the user for this, or state the assumption explicitly and
   lower the confidence rating.
5. Every IP gets Direction 1 regardless. The verdict only decides whether to **add**
   Direction 2, and at which tier.

---

## 1. The two directions

### Direction 1 — Functional / Programmer's View (PV), untimed
- **What it models:** the **software-visible behavior** — the register map plus the
  functional transform. It answers *"if I write the right inputs, do I get the
  right outputs?"*
- **Code shape:** a plain C++ class exposing `reset()`, `readReg(offset)`,
  `writeReg(offset, data)`, `hasInterrupt()`. Registers are `uint32_t` members.
  The operation runs **atomically and instantly** on the triggering register
  write. **There is no clock and no notion of cycles** (`sc_time` does not advance).
- **LISA wrapper:** a thin PVBus-slave adapter that decodes the offset and forwards
  to `readReg`/`writeReg`. Nothing else.
- **Reference examples:** `tlm_model/ip/aes/`, `tlm_model/ip/crc/`, and their
  wrappers `FastModels_Wrapper/<IP>/Wrapper_<IP>.lisa`.
- **Abstraction level (TLM):** PV / Loosely-Timed (LT).
- **Strengths:** tiny (hundreds of lines), fast to write (~1–2 days), **very fast
  simulation** (the whole reason an FVP exists — boot an OS in seconds, run real
  software), functionally bit-exact, almost no room for its own bugs.
- **Cannot provide:** any timing / cycle / FSM / pipeline / utilization data.

### Direction 2 — Cycle-approximate micro-architecture / timing
- **What it models:** the **actual hardware micro-architecture** — clocked,
  cycle-by-cycle. It answers *"how many cycles, where are the stalls, how
  utilized is the datapath, does the FSM sequence match RTL?"* on top of function.
- **Code shape (three layers):**
  1. **Cycle-level SystemC model** (structural): `sc_module`s with `i_clk`,
     `i_rstn`, ports, submodules wired like the RTL block diagram, `SC_METHOD`s,
     FSM enums, pipeline delay lines, internal SRAM/FIFO, valid/ready handshakes,
     performance counters. Verified independently against RTL.
  2. **Bridge**: a pImpl C-ABI layer that hides SystemC. It owns an `sc_clock` and
     `sc_signal`s, drives reset, drives the pin-level host interface over cycles,
     programs config registers, stages tensors, pulses `start`, polls
     `done`/`deadlock`, and reads results back. Exposes a clean
     `struct RunConfig` + `run()` API.
  3. **LISA component**: a PVBus **slave** (register map = SW view) **plus** a
     PVBus **master** (`TransactionGenerator`) so the model can DMA guest memory.
     On `CONTROL.START` it reads shapes/addresses, DMA-reads inputs, calls the
     bridge, DMA-writes outputs, sets `STATUS.DONE/ERROR` + IRQ.
- **Reference implementation (SAURIA NPU):**
  - Model: `fx1/hw/tlm/MP1_V1.1/v4_model/` (see `npu_top.h`,
    `control/main_controller.h`, `systolic_array/sa_processing_element.h`,
    `sram/sram_top.h`).
  - Bridge: `fx1/hw/tlm/npu_fm/mp1_v1_1/mp1_v1_1_systemc_bridge.{h,cpp}`.
  - LISA: `fx1/hw/tlm/npu_fm/npu_fm.lisa`.
- **Abstraction level (TLM):** Approximately-Timed (AT) / Cycle-Approximate (CA).
- **Strengths:** exposes clocked modules, FSM states, per-stage latency,
  pipeline fill/drain, valid/done handshakes, PE-array utilization, cycle/stall
  counters, and configurable X/Y/K/depth.
- **Costs:** thousands of lines + bridge + heavy LISA; **100–1000× slower**
  simulation; needs RTL/micro-arch spec; high maintenance (tracks RTL); more room
  for the model itself to be wrong.

### Comparison table
| Dimension | Direction 1 (functional/PV) | Direction 2 (cycle micro-arch) |
|---|---|---|
| Models | Register map + function | Micro-architecture + timing |
| Effort / IP | Hundreds of lines, ~1–2 days | Thousands of lines + bridge + LISA, weeks–months |
| **Simulation speed** | **Very fast (MIPS)** | 100–1000× slower |
| Prerequisite | Register spec only | RTL / micro-arch spec, reasonably frozen |
| Answers | "Runs correctly?" | + "How many cycles / stalls / utilization?" |
| Risk of model bugs | Near zero | High (hand-ported from RTL) |
| Maintenance | Light | Heavy (follows RTL) |
| Verifies function | Yes (bit-exact) | Yes (if datapath output is not overridden) |
| Verifies timing | No | Yes |

---

## 2. Critical caveats (do not skip)

1. **FVP core value = speed.** ARM FVP / Fast Models exist to run fast (boot OS,
   run full software stacks) because every peripheral is functional/PV. Converting
   **all** IPs to cycle-level turns the FVP into a slow cycle simulator — losing
   the reason to use an FVP. Keep it **mixed-fidelity**: fast by default, detailed
   only where needed. If truly cycle-accurate whole-SoC is required, FVP is the
   wrong tool (consider gem5 / full SystemC-CA / RTL co-sim).
2. **Register spec ≠ micro-architecture spec.** A programmer's register map is
   enough for Direction 1 but **not** for Direction 2. Direction 2 needs FSM /
   pipeline / datapath / internal-memory / latency detail (or RTL). Confirm which
   one the HW team provides **before** committing to Direction 2.
3. **"More detailed" ≠ "more correct."** Direction 2 has far more room for its own
   bugs, and its cycle numbers are only trustworthy once cross-checked against RTL.
   It is **cycle-approximate**, not signed-off cycle-accurate, until validated.
4. **Correctness is per-reference.** Direction 1 is validated against the **spec**;
   Direction 2 is validated against the **RTL**. They prove different things.
   Direction 2 does not prove *functional* correctness "more" — Direction 1 already
   does that, more cheaply.
5. **Known state of the reference NPU bridge:** its `run()` currently overwrites the
   systolic-array datapath result with an exact golden C reference
   (`writeExactInt8GemmOutput`). So today it validates **timing**, not the datapath
   function. This is fixable (use the SRAM-C readback as output) but note it when
   reasoning about what Direction 2 currently proves.

---

## 3. Intermediate tier (cheaper than full Direction 2)

Between "functional only" and "full cycle model" there is a middle path — prefer it
when timing questions exist but full micro-arch modeling is not justified:

- **Functional + timing annotation (LT + latency/throughput annotation):** keep the
  fast Direction-1 model, add cycle-count / latency estimates. Gives ballpark
  performance without building the datapath.
- **Analytic model:** compute data-movement cost with a formula (e.g. bandwidth
  `bytes/cycle` + setup cycles) instead of simulating an engine cycle-by-cycle. The
  reference NPU's `EvalCounters` already does this for DMA.

Verdict `H2_INTERMEDIATE` means: **do this middle path first**; escalate to full
Direction 2 only if the annotation/analytic model cannot answer the question.

---

## 4. DECISION ALGORITHM (execute per IP)

```
INPUT   : IP name (+ any available spec / architecture facts)
BASELINE: every IP always gets a Direction-1 functional model.
GOAL    : decide whether to ALSO build a Direction-2 timing model, and at what tier.

STEP 0 — Spec type
  if only a register/programmer's spec is available (no micro-arch/RTL):
      if IP is an AUTO-NO peripheral (see Step 1):   verdict = H1_ONLY ; stop
      else:                                          verdict = NEEDS_INFO
                                                     (request micro-arch spec/RTL) ; stop

STEP 1 — Shortcuts
  AUTO-NO  (pure config/control/status, no datapath):  verdict = H1_ONLY ; stop
      e.g. GPIO, timer, RTC, WDT, PAD, sysreg, PMU, CMU, CRM, expreg,
           temsen, RNG, small CRC.
  AUTO-YES (compute or data-movement engine whose performance IS the product):
      mark as H2 candidate ; continue to scoring to pick the tier.
      e.g. NPU, DMA, memory/QSPI/NOR controller, video codec (VPU),
           interconnect/NoC, crypto when throughput is a hot path.

STEP 2 — Gates (ALL must pass to build Direction 2)
  G1  micro-arch spec / RTL available?      else verdict = NEEDS_INFO ; stop
  G2  RTL / architecture reasonably frozen? else verdict = H1_ONLY (DEFER, revisit) ; stop
  G3  has a real datapath (processes/moves data, not just control)?
                                            else verdict = H1_ONLY ; stop

STEP 3 — Score each question 0 / 1 / 2

  Group A — VALUE (is the timing question worth answering?)   [necessary]
    V1  Does anyone (architect / SW-perf / customer) actually NEED
        cycle / throughput / latency / utilization numbers for this IP?
    V2  Is the IP on the critical path / a bottleneck of the target workload?
    V3  Is there a CONCRETE design decision that depends on these numbers?
        (SRAM/FIFO sizing, #PEs, DMA burst, clock choice, ...)  If you cannot
        name a decision -> 0.
    V4  Is timing data-/config-dependent (variable latency, stalls, contention),
        i.e. not predictable by a simple constant/formula?  (constant -> 0)
    A = V1 + V2 + V3 + V4            (max 8)

  Group B — MICRO-ARCH RICHNESS (is there anything to model?)
    M1  Multi-stage pipeline / parallelism / systolic array?
    M2  Multi-state FSM orchestrating the work?
    M3  Internal memory (SRAM/FIFO/double-buffer) whose contention/latency
        affects performance?
    M4  Back-pressure / stall / valid-ready handshakes that vary throughput?
    M5  Contention with other masters on the bus / DMA?
    B = M1 + M2 + M3 + M4 + M5      (max 10)

  Group C — COST / FEASIBILITY (modifiers, not added to score)
    C1  Is there RTL to port directly?            (no  -> effort x2, higher risk)
    C2  Does the RTL change frequently?           (yes -> heavy maintenance)
    C3  Does the IP run continuously during boot / while software runs?
                                                  (yes -> needs swappable fidelity)

STEP 4 — Decision
  if A < 3:                              verdict = H1_ONLY
      # nobody will act on the numbers -> a detailed model is waste,
      # regardless of how rich the micro-architecture is.
  elif (A + B) >= 10 and C1 == yes:      verdict = H2_FULL
  elif (A + B) >= 10 and C1 == no:       verdict = H2_FULL
                                         flags += "high-effort/high-risk (no RTL);
                                                   consider starting at INTERMEDIATE"
  elif 6 <= (A + B) <= 9:                verdict = H2_INTERMEDIATE
  else:                                  verdict = H1_ONLY

  if C3 == yes and verdict startswith H2:
      flags += "swappable fidelity required (keep a fast H1 model for boot/OS runs)"

STEP 5 — Sanity check (must still hold for any H2 verdict)
  Q1  "If I did NOT have cycle numbers for this IP, which decision is blocked?"
      -> cannot name one  => downgrade to H1_ONLY.
  Q2  "Would a simple timing annotation answer the question?"
      -> yes              => downgrade to H2_INTERMEDIATE.
  Q3  "Will these numbers be cross-checked against RTL?"
      -> no plan          => keep verdict but flag "estimate only, not CA-signed-off".

OUTPUT  : one row per IP in the format of Section 6.
```

### Verdict meanings (under parallel development)
| Verdict | Meaning |
|---|---|
| `H1_ONLY` | Build only the Direction-1 functional model. Do **not** build a timing model. |
| `H2_INTERMEDIATE` | Keep the Direction-1 model; add timing annotation / analytic model. Defer full Direction 2. |
| `H2_FULL` | Build the Direction-1 model **and**, in parallel, a full Direction-2 cycle model. |
| `NEEDS_INFO` | Cannot decide the timing verdict. Direction 1 proceeds; list what to request from HW. |

---

## 4B. SPEC EXTRACTION MAP (single-IP mode — read the spec, fill the algorithm)

When given one IP spec document, scan it and map its content onto the algorithm.
First classify the spec type, then gather evidence for each question.

### Spec-type detection (drives Step 0 / Gate G1)
- **Register-only spec** (→ G1 likely FAILS, verdict tends to `NEEDS_INFO`): the doc
  is dominated by a register map — offset/address tables, bit-field tables, access
  type (R/W/RO/W1C), reset values — and little else.
- **Micro-arch spec** (→ G1 can PASS): contains architecture / block diagram /
  datapath / pipeline / FSM / latency-in-cycles / throughput / FIFO / buffering /
  AXI-master-DMA / arbitration / timing diagrams or waveforms.
- **Mixed:** both. Use the micro-arch parts for scoring; note any gaps.

### Evidence map (keyword / section → which item it informs)
| Item | Look for in the spec | Reading |
|---|---|---|
| G3 datapath | data in/out streams, compute (mul/acc/transform/filter/codec), DMA transfer — beyond config registers | present → G3 pass |
| V4 variable timing | "latency depends on size/mode/config", "variable cycles" vs. a fixed cycle count | data-dependent → V4 high |
| M1 pipeline | "pipeline", "stages", "N-cycle latency", stage waveforms | present → M1 high |
| M2 FSM | "state machine", "FSM", state names (IDLE/BUSY/DONE), sequencing | present → M2 high |
| M3 internal memory | "SRAM", "FIFO", "buffer", "double-buffer/ping-pong", "line buffer", "cache" | present → M3 high |
| M4 back-pressure | "valid/ready", "handshake", "stall", "flow control", "almost-full" | present → M4 high |
| M5 contention | "AXI master", "DMA", "shared bus", "arbiter", "multiple masters", "bandwidth" | present → M5 high |
| C1 RTL | spec references RTL / an RTL deliverable exists | usually external knowledge — ask if unsure |
| C2 churn / G2 stable | "draft", "preliminary", "TBD", version history still moving | not frozen → G2 fail / C2 bad |
| C3 boot-time | always-on peripheral vs. on-demand engine | affects the swappable-fidelity flag |
| V1–V3 value | **rarely in the spec** — needs project context | ask user or mark `needs-context` |

### Key rule (avoid a common wrong conclusion)
If micro-arch items (M1–M5) cannot be found **because the spec is register-only**,
do **not** score them 0 and conclude `H1_ONLY` — that is wrong for a real engine.
Instead fail Gate G1 and emit `NEEDS_INFO` requesting the micro-arch / RTL doc —
**unless** the IP is an AUTO-NO peripheral (then `H1_ONLY` is genuinely correct).
Scoring 0 is only valid when the spec is complete and the feature truly is absent.

---

## 5. Calibration examples (score sanity)

| IP | Gate | A | B | Verdict | Note |
|---|---|---|---|---|---|
| NPU (SAURIA) | pass | 8 | 10 | `H2_FULL` | systolic array; perf is the product |
| DMA | pass | 7 | 8 | `H2_FULL` | data-mover; bandwidth/contention matter |
| QSPI / NOR controller | pass? | 5 | 5 | `H2_INTERMEDIATE` | annotate access latency first |
| GMAC (Ethernet) | pass | 6 | 6 | `H2_INTERMEDIATE`→`H2_FULL` if throughput is a target |
| AES | pass | 2–3 | 4 | `H1_ONLY` | unless crypto throughput is a hot path |
| GPIO / timer / RTC / WDT | — | — | — | `H1_ONLY` | AUTO-NO |

---

## 6. OUTPUT CONTRACT (the AI must produce this)

For a given IP list, output **one markdown table**, one row per IP, columns:

`| IP | Verdict | GatePass | A | B | Flags | Rationale | MissingInfo |`

- **IP** — the IP name.
- **Verdict** — one of `H1_ONLY`, `H2_INTERMEDIATE`, `H2_FULL`, `NEEDS_INFO`.
- **GatePass** — `yes` / `no (Gx)` / `n/a (auto-no)`.
- **A** — Group A score (0–8) or `-` if not scored.
- **B** — Group B score (0–10) or `-` if not scored.
- **Flags** — e.g. `swappable-fidelity`, `no-RTL`, `estimate-only`, or `-`.
- **Rationale** — one sentence: the deciding factor.
- **MissingInfo** — what to request from HW if `NEEDS_INFO`, else `-`.

After the table, add:
- A short **"Parallel-development plan"** paragraph: state that all listed IPs get a
  Direction-1 model, and list which subset additionally gets Direction 2
  (`H2_FULL`) vs the intermediate tier (`H2_INTERMEDIATE`), ordered by priority
  (highest A+B first).
- A **"Questions for HW"** bullet list aggregating every `MissingInfo`.

### Example output (for input `NPU, DMA, GPIO, AES, QSPI`)
```
| IP   | Verdict          | GatePass | A | B | Flags              | Rationale | MissingInfo |
|------|------------------|----------|---|---|--------------------|-----------|-------------|
| NPU  | H2_FULL          | yes      | 8 |10 | -                  | Systolic accelerator; perf is the deliverable | - |
| DMA  | H2_FULL          | yes      | 7 | 8 | swappable-fidelity | Data-mover; bandwidth/contention drive perf   | - |
| QSPI | H2_INTERMEDIATE  | yes      | 5 | 5 | -                  | Access latency matters; annotation suffices first | - |
| AES  | H1_ONLY          | n/a      | 2 | 4 | -                  | Function-only need; throughput not a hot path | - |
| GPIO | H1_ONLY          | n/a(auto)| - | - | -                  | Pure control/status peripheral, no datapath   | - |
```

### 6.1 Single-IP output format (PRIMARY mode — spec-driven)

When the input is one IP spec document, output this block **instead of** the table:

```
## Verdict: <IP> → <H1_ONLY | H2_INTERMEDIATE | H2_FULL | NEEDS_INFO>

**Spec type:** register-only | micro-arch | mixed   (evidence: <sections>)
**Gates:** G1 <pass/fail> · G2 <pass/fail> · G3 <pass/fail>

**Scores**
| Q  | Score | Evidence (spec section / quote, or 'assumed' / 'needs-context') |
|----|-------|-----------------------------------------------------------------|
| V1 |  ?    | ...   |
| V2 |  ?    | ...   |
| V3 |  ?    | ...   |
| V4 |  ?    | ...   |
| M1 |  ?    | ...   |
| M2 |  ?    | ...   |
| M3 |  ?    | ...   |
| M4 |  ?    | ...   |
| M5 |  ?    | ...   |
A = <a>/8 · B = <b>/10 · A+B = <s>

**Flags:** <swappable-fidelity | no-RTL | estimate-only | ->
**Rationale:** <1–2 sentences: the deciding factor>
**Confidence:** <high/medium/low> — <how much came from the spec vs assumed>
**Questions for HW / Missing info:** <bullets, or 'none'>
**Recommended next action:** <e.g. "Build H1 now; request micro-arch doc to reassess H2">
```

Confidence rubric: **high** = verdict rests on facts found in the spec; **medium** =
one or two value/context items assumed; **low** = spec register-only or several key
items assumed (usually pairs with `NEEDS_INFO`).

---

## 7. Glossary
- **PV (Programmer's View):** untimed functional model; software sees registers only.
- **LT / AT / CA:** Loosely-Timed / Approximately-Timed / Cycle-Accurate TLM levels.
- **FVP:** Fixed Virtual Platform (ARM Fast Models) — fast, functional-first simulator.
- **LISA / LISA+:** the component/behavior language used to wrap a model onto the
  Fast Models PVBus.
- **PE (Processing Element):** one multiply-accumulate (MAC) cell in a systolic
  array; "PE utilization" = fraction of PEs doing useful MACs vs idle.
- **Bridge:** C-ABI layer that hides a cycle-level SystemC model behind a clean
  buffer-based `run()` API and drives its clock/pins.
- **Datapath:** the part of an IP that processes or moves data (vs. control/status).
- **Golden reference:** an exact functional model used to check correctness.
```
