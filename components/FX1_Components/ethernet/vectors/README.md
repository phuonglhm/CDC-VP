# Reference vector format

The fixed vectors use seed 1. `core.txt` contains 3,069 records; the replay
program performs 3,073 checks including four additional counter properties.
`scenarios/count.txt` specifies 200 files named `s0000.txt` to `s0199.txt`.
Hexadecimal byte strings use `-` for an empty sequence.

Frame records begin with CRC (CRC and hash), FILT (address acceptance), RX
(receive verdict, stored length and counter deltas), RXCS (receive checksum),
TXCS (transmit checksum), TXW (padding and FCS), or TXRM (transmit counters).
The parser and field order are in `tests/test_core_golden.cpp` under
`components/eth_mac_tlm/`.

Scenario records use RING for ring sizes and buffer offsets; REG for initial
register writes; REGW for writes while running and their expected error;
TX for transmit frames, buffer segments and injected fault; RX for incoming
frames and error flags; WIRE for expected transmitted bytes; TXD and RXD
for expected descriptor status and receive data; RMON for counter values;
and EXPREG for masked register expectations. The parser and field order
are in `components/eth_mac_tlm/tests/test_scenarios.cpp`.

Replay uses only these files and the delivered model and tests.
