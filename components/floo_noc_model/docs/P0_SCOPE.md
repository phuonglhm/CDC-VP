# P0 — FlooNoC Direction-2 scope and locked decisions

## Frozen source

- FlooNoC repository:
  `/home/duyptt_HW/Documents/work/Study_FlooNoC/FlooNoC`
- FlooNoC revision: `9a6972a`
- FlooGen package version: `0.8.4`
- Integration target: CDC-VP, SystemC 2.3.4, C++17.

The model must not silently track another FlooNoC revision. Changing the frozen
revision requires an RTL cross-check rerun and an update to this document.

## Vertical slice v0

The first end-to-end slice is deliberately constrained to:

| Parameter | Locked value |
|---|---|
| Network type | single AXI |
| Routing | deterministic XY |
| Traffic | unicast |
| Physical channels | `req`, `rsp` |
| Link flow control | ready/valid |
| Virtual channels | disabled |
| Router ports | North, East, South, West, Eject |
| Router input FIFO | enabled; depth fixed per instantiated test |
| Router output FIFO | initially disabled |
| Topology | rectangular 2-D mesh |
| Link latency | one configured cycle per registered boundary |

Deferred features:

- narrow-wide AXI and the `wide` channel;
- YX, source, and table-based routing;
- virtual channels and credit flow control;
- multicast, synchronization, and reduction;
- AXI ATOPs;
- configurable RoB modes and multiple unique downstream IDs;
- CDC links and asynchronous clock domains.

Deferred does not mean unsupported permanently. Each feature is added only
after the lower-level RTL equivalence tests remain green.

## Direction-2 decisions

1. **Authoritative output:** the SystemC datapath/network state is authoritative.
   A golden/reference model is comparison-only and never overwrites output.
2. **Timing target:** develop cycle-approximate first; promote a block to
   RTL-signed cycle accuracy only after per-cycle trace comparison.
3. **Runtime style:** traffic-driven. There is no synthetic accelerator
   `START/DONE` contract.
4. **Clock control:** the future CDC-VP wrapper may gate the clock only when all
   ingress queues, router FIFOs, locks, egress queues, and outstanding responses
   are empty.
5. **Integration position:** FlooNoC replaces or hierarchically composes the
   CDC-VP fabric; it is not attached as one MMIO peripheral.

## Metrics

Directly measured:

- transaction and flit latency;
- flits and payload bytes per cycle, per link/channel;
- injection/ejection stall cycles;
- output arbitration wait cycles;
- FIFO occupancy and high-water mark;
- link utilization;
- hop count;
- outstanding response and RoB occupancy when that phase is implemented.

Analytic metrics, if added, must be reported separately from measured counters.

## Completion criteria for the vertical slice

- Every leaf block has a standalone SystemC test.
- A single router passes directed routing, contention, back-pressure, and
  wormhole-lock tests.
- A small mesh delivers every flit exactly once to the expected endpoint.
- The same directed stimuli are run against FlooNoC RTL.
- Function and handshake cycle traces match within the declared tolerance.
