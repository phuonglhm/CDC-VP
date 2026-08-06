// SPDX-License-Identifier: Apache-2.0
//
// Direct contract test for `meta_buffer.hpp`, the request-metadata retention
// of the `MaxUniqueIds == 1` branch of `hw/floo_meta_buffer.sv`.
//
// Why this exists separately from the cross-check. The 221-cycle
// `run_chimney_rsp_crosscheck.sh` / `run_chimney_rsp_timing_crosscheck.sh` pair
// already compares this leaf against the frozen RTL inside the chimney's
// subordinate side, including multi-entry FIFO occupancy. That is the hardware
// evidence and this test does not replace it. What it adds is isolation: when a
// 221-cycle trace diverges, a direct leaf test says whether the metadata FIFO
// itself moved or whether the fault is in the unpacker, the spill register or
// the response arbiter around it.
//
// Two expectations are read off the RTL rather than off this model, and both
// are easy to get wrong in the obvious direction:
//
//   * `assign no_atop_aw_req_id = '1;` — the downstream ID is **all ones** for
//     `OutIdWidth` bits, not zero. A model that reissued under ID 0 would look
//     plausible and be wrong;
//   * the branch retains metadata in a plain in-order `fifo_v3`, one for writes
//     and one for reads, so responses pop oldest-first and the two directions
//     do not share state.

#include "floo_noc_model/meta_buffer.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace floo::model;

int failures = 0;

void check(bool condition, const std::string& what)
{
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

/// Build metadata whose complete payload can be distinguished after a FIFO
/// round trip.
response_meta tagged(
    std::uint64_t axi_id, unsigned x = 0, unsigned y = 0,
    bool rob_req = true, bool atop = false)
{
    response_meta meta{};
    meta.axi_id = axi_id;
    meta.src_id = coordinate{x, y};
    meta.rob_req = rob_req;
    meta.rob_idx = static_cast<unsigned>(axi_id);
    meta.atop = atop;
    return meta;
}

bool same_meta(const response_meta& lhs, const response_meta& rhs)
{
    return lhs.src_id.x == rhs.src_id.x && lhs.src_id.y == rhs.src_id.y
        && lhs.axi_id == rhs.axi_id && lhs.rob_req == rhs.rob_req
        && lhs.rob_idx == rhs.rob_idx && lhs.atop == rhs.atop;
}

/// `'1` for `OutIdWidth` bits. Checked at several widths because an
/// implementation that returned 0, or that computed `1 << width` without the
/// `- 1`, would still pass at exactly one of them.
void test_downstream_id_is_all_ones()
{
    check(meta_buffer(1, 4).downstream_id() == 0x1ull,
          "OutIdWidth=1: downstream ID must be 0x1");
    check(meta_buffer(3, 4).downstream_id() == 0x7ull,
          "OutIdWidth=3: downstream ID must be 0x7");
    check(meta_buffer(4, 4).downstream_id() == 0xFull,
          "OutIdWidth=4: downstream ID must be 0xF");
    check(meta_buffer(8, 4).downstream_id() == 0xFFull,
          "OutIdWidth=8: downstream ID must be 0xFF");
    // The widest accepted width, where a 32-bit intermediate would wrap.
    check(meta_buffer(63, 4).downstream_id() == 0x7FFF'FFFF'FFFF'FFFFull,
          "OutIdWidth=63: downstream ID must not overflow a 64-bit shift");
}

/// The frozen branch is a FIFO, not an ID-keyed queue: responses pop in the
/// order the requests arrived, whatever their AXI IDs are.
void test_fifo_order_with_three_entries()
{
    meta_buffer buffer(3, 8);

    const auto write0 = tagged(5, 1, 2, true, false);
    const auto write1 = tagged(2, 3, 0, false, true);
    const auto write2 = tagged(9, 2, 3, true, true);
    buffer.push_write(write0);
    buffer.push_write(write1);
    buffer.push_write(write2);

    check(same_meta(buffer.pop_write(), write0),
          "writes must return the whole oldest metadata payload");
    check(same_meta(buffer.pop_write(), write1),
          "writes must return whole payloads in arrival order");
    check(same_meta(buffer.pop_write(), write2),
          "the newest whole write payload must pop last");

    // Reads take the same rule. Vary every retained field so a partial copy
    // cannot satisfy the test accidentally.
    const auto read0 = tagged(7, 1, 2, false, true);
    const auto read1 = tagged(1, 3, 0, true, false);
    const auto read2 = tagged(6, 0, 3, true, true);
    buffer.push_read(read0);
    buffer.push_read(read1);
    buffer.push_read(read2);
    check(same_meta(buffer.pop_read(), read0),
          "reads must return the whole oldest metadata payload");
    check(same_meta(buffer.pop_read(), read1),
          "reads must return whole payloads in arrival order");
    check(same_meta(buffer.pop_read(), read2),
          "the newest whole read payload must pop last");
}

/// One FIFO for writes and one for reads. Sharing them would let a B response
/// consume an AR's metadata, which is the defect this separation prevents.
void test_read_and_write_state_are_independent()
{
    meta_buffer buffer(3, 4);

    buffer.push_write(tagged(1));
    check(buffer.outstanding_writes() == 1, "a write must count as a write");
    check(buffer.outstanding_reads() == 0, "a write must not count as a read");

    buffer.push_read(tagged(2));
    check(buffer.outstanding_writes() == 1,
          "a read must not disturb the write count");
    check(buffer.outstanding_reads() == 1, "a read must count as a read");

    check(buffer.pop_write().axi_id == 1,
          "the write FIFO must return the write metadata");
    check(buffer.outstanding_reads() == 1,
          "popping a write must leave the read outstanding");
    check(buffer.pop_read().axi_id == 2,
          "the read FIFO must return the read metadata");

    // Filling one direction must not make the other look full.
    meta_buffer narrow(3, 1);
    narrow.push_write(tagged(0));
    check(narrow.write_full(), "one entry at MaxTxns=1 must fill writes");
    check(!narrow.read_full(), "a full write FIFO must not fill reads");
}

/// Exact transitions, not just the endpoints: the count must move by one on
/// every push and pop, and `*_full()` must flip only at the configured depth.
void test_full_and_outstanding_transitions()
{
    constexpr std::size_t depth = 3;
    meta_buffer buffer(3, depth);

    for (std::size_t index = 0; index < depth; ++index) {
        check(buffer.outstanding_writes() == index,
              "the write count must match the number pushed");
        check(!buffer.write_full(),
              "the write FIFO must not report full below its depth");
        buffer.push_write(tagged(index));
    }
    check(buffer.outstanding_writes() == depth,
          "the write count must reach the configured depth");
    check(buffer.write_full(), "the write FIFO must report full at its depth");

    for (std::size_t index = 0; index < depth; ++index) {
        check(buffer.outstanding_writes() == depth - index,
              "the write count must fall by one per pop");
        buffer.pop_write();
        check(!buffer.write_full(),
              "the write FIFO must leave full as soon as one entry is taken");
    }
    check(buffer.outstanding_writes() == 0, "the write FIFO must drain to zero");

    for (std::size_t index = 0; index < depth; ++index) {
        check(!buffer.read_full(),
              "the read FIFO must not report full below its depth");
        buffer.push_read(tagged(index));
    }
    check(buffer.read_full(), "the read FIFO must report full at its depth");
    check(buffer.outstanding_reads() == depth,
          "the read count must reach the configured depth");
}

/// Overflow is refused, not absorbed. The RTL cannot accept a request when its
/// FIFO is full — the chimney back-pressures instead — so a model that grew
/// past `MaxTxns` would silently model a deeper buffer than the hardware has.
void test_overflow_is_rejected()
{
    meta_buffer buffer(3, 2);
    buffer.push_write(tagged(0));
    buffer.push_write(tagged(1));

    bool threw = false;
    try {
        buffer.push_write(tagged(2));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "a write past MaxTxns must be refused");
    check(buffer.outstanding_writes() == 2,
          "a refused write must not change the count");

    buffer.push_read(tagged(0));
    buffer.push_read(tagged(1));
    threw = false;
    try {
        buffer.push_read(tagged(2));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "a read past MaxTxns must be refused");
    check(buffer.outstanding_reads() == 2,
          "a refused read must not change the count");
}

/// A response with no retained request is a protocol violation upstream, and
/// must surface rather than return default-constructed metadata — which would
/// route the response to node (0,0) under AXI ID 0.
void test_underflow_is_rejected()
{
    meta_buffer buffer(3, 4);

    bool threw = false;
    try {
        buffer.pop_write();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "a B response with no outstanding write must be refused");

    threw = false;
    try {
        buffer.pop_read();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "an R response with no outstanding read must be refused");

    // Draining exactly to empty and then popping once more is the realistic
    // shape of the defect, not popping a never-used buffer.
    buffer.push_read(tagged(4));
    check(buffer.pop_read().axi_id == 4, "the retained read must pop first");
    threw = false;
    try {
        buffer.pop_read();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "popping a drained read FIFO must be refused");
}

/// Configuration that cannot describe real hardware is refused at construction.
void test_invalid_construction()
{
    const auto rejected = [](unsigned width, std::size_t txns,
                             const std::string& what) {
        bool threw = false;
        try {
            meta_buffer buffer(width, txns);
            (void)buffer;
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        check(threw, what);
    };

    rejected(0, 4, "OutIdWidth=0 must be refused");
    rejected(64, 4, "OutIdWidth=64 must be refused: the shift would overflow");
    rejected(65, 4, "OutIdWidth above 64 must be refused");
    rejected(3, 0, "MaxTxns=0 must be refused");

    // The boundary on the accepting side, so the rejection is not simply
    // refusing everything near the limit.
    bool accepted = true;
    try {
        meta_buffer buffer(63, 1);
        (void)buffer;
    } catch (const std::exception&) {
        accepted = false;
    }
    check(accepted, "OutIdWidth=63 with MaxTxns=1 must be accepted");
}

} // namespace

// No kernel runs here: the metadata buffer is plain state with no processes.
// `sc_main` rather than `main` because SystemC owns the real entry point.
int sc_main(int, char**)
{
    test_downstream_id_is_all_ones();
    test_fifo_order_with_three_entries();
    test_read_and_write_state_are_independent();
    test_full_and_outstanding_transitions();
    test_overflow_is_rejected();
    test_underflow_is_rejected();
    test_invalid_construction();

    if (failures != 0) {
        std::cerr << failures << " meta_buffer checks failed\n";
        return 1;
    }
    std::cout << "test_meta_buffer: all checks passed\n";
    return 0;
}
