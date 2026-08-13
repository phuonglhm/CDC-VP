// SPDX-License-Identifier: Apache-2.0
//
// Decision record D6's host-backing policy, proved on the storage class
// itself.
//
// The single most important check here is `a_gibibyte_costs_almost_nothing`:
// the whole reason this class exists is that `mesh_4x4` describes 1.25 GiB of
// logical memory and must elaborate on an ordinary host. Everything else is
// the TLM semantics that a page-backed store is easy to get subtly wrong at —
// transfers crossing page boundaries, byte enables, and the rule that a
// refused access leaves the caller's buffer untouched rather than zeroing it.

#include "tpu_v3/sparse_memory.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using cdc::components::tpu_v3::sparse_memory;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__ << ':' \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

constexpr std::uint64_t page = sparse_memory::page_size;

void an_untouched_store_reads_zero_and_costs_nothing()
{
    sparse_memory memory(64 * page);
    CHECK(memory.capacity() == 64 * page);
    CHECK(memory.page_count() == 64);
    CHECK(memory.allocated_pages() == 0);
    CHECK(memory.allocated_bytes() == 0);

    std::array<unsigned char, 32> buffer{};
    buffer.fill(0xAA);
    CHECK(memory.read(17 * page + 8, buffer.size(), buffer.data()));
    for (auto byte : buffer) {
        CHECK_MSG(byte == 0, "an unallocated page must read as zero");
    }
    CHECK_MSG(memory.allocated_pages() == 0,
              "reading must never commit host memory");
}

void a_write_allocates_only_the_touched_pages()
{
    sparse_memory memory(64 * page);

    const std::array<unsigned char, 4> value{0xDE, 0xAD, 0xBE, 0xEF};
    CHECK(memory.write(9 * page + 100, value.size(), value.data()));

    CHECK(memory.allocated_pages() == 1);
    CHECK(memory.allocated_bytes() == page);
    CHECK(memory.page_is_allocated(9));
    CHECK(!memory.page_is_allocated(8));
    CHECK(!memory.page_is_allocated(10));

    std::array<unsigned char, 4> back{};
    CHECK(memory.read(9 * page + 100, back.size(), back.data()));
    CHECK(back == value);

    // The rest of the newly allocated page must still read as zero: a page
    // that came into existence holding uninitialised bytes would make the
    // first write change what its neighbours read back as.
    std::array<unsigned char, 16> neighbours{};
    neighbours.fill(0x5A);
    CHECK(memory.read(9 * page + 200, neighbours.size(), neighbours.data()));
    for (auto byte : neighbours) {
        CHECK(byte == 0);
    }

    // Rewriting the same page does not allocate a second one.
    CHECK(memory.write(9 * page + 300, value.size(), value.data()));
    CHECK(memory.allocated_pages() == 1);
}

void transfers_cross_page_boundaries()
{
    sparse_memory memory(16 * page);

    // Straddles the 3/4 boundary by two bytes.
    std::vector<unsigned char> pattern(64);
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        pattern[i] = static_cast<unsigned char>(i + 1);
    }
    const std::uint64_t offset = 4 * page - 62;
    CHECK(memory.write(offset, pattern.size(), pattern.data()));
    CHECK_MSG(memory.allocated_pages() == 2,
              "a transfer spanning two pages must allocate exactly two");

    std::vector<unsigned char> back(pattern.size(), 0);
    CHECK(memory.read(offset, back.size(), back.data()));
    CHECK_MSG(back == pattern, "data was lost or misplaced at a page boundary");

    // And the bytes either side of the transfer are untouched.
    std::array<unsigned char, 4> before{};
    before.fill(0x77);
    CHECK(memory.read(offset - 4, before.size(), before.data()));
    for (auto byte : before) {
        CHECK(byte == 0);
    }

    // A transfer longer than one page, starting mid-page: three pages.
    sparse_memory wide(16 * page);
    std::vector<unsigned char> big(2 * page + 8, 0x33);
    CHECK(wide.write(page + 4, big.size(), big.data()));
    CHECK(wide.allocated_pages() == 3);
    std::vector<unsigned char> big_back(big.size(), 0);
    CHECK(wide.read(page + 4, big_back.size(), big_back.data()));
    CHECK(big_back == big);
}

void byte_enables_are_honoured_on_both_directions()
{
    sparse_memory memory(4 * page);

    std::array<unsigned char, 8> initial{1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(memory.write(0, initial.size(), initial.data()));

    // Non-contiguous strobe: only bytes 1, 4 and 7 are written.
    const std::array<unsigned char, 8> strobe{0, 1, 0, 0, 1, 0, 0, 1};
    const std::array<unsigned char, 8> update{0xF0, 0xF1, 0xF2, 0xF3,
                                              0xF4, 0xF5, 0xF6, 0xF7};
    CHECK(memory.write(0, update.size(), update.data(), strobe.data()));

    std::array<unsigned char, 8> back{};
    CHECK(memory.read(0, back.size(), back.data()));
    const std::array<unsigned char, 8> expected{1, 0xF1, 3, 4, 0xF4, 6, 7, 0xF7};
    CHECK_MSG(back == expected, "a strobed write touched a masked-out byte");

    // A masked read leaves the caller's masked-out bytes alone.
    std::array<unsigned char, 8> partial{};
    partial.fill(0x9C);
    CHECK(memory.read(0, partial.size(), partial.data(), strobe.data()));
    for (std::size_t i = 0; i < partial.size(); ++i) {
        if (strobe[i] != 0) {
            CHECK(partial[i] == expected[i]);
        } else {
            CHECK_MSG(partial[i] == 0x9C,
                      "a masked-out byte of a read destination was overwritten");
        }
    }
}

void a_fully_masked_write_allocates_nothing()
{
    // Nothing is stored, so nothing should be committed. Otherwise the
    // reported backing would depend on the shape of the strobe pattern rather
    // than on what the workload actually wrote.
    sparse_memory memory(4 * page);
    const std::array<unsigned char, 8> data{1, 2, 3, 4, 5, 6, 7, 8};
    const std::array<unsigned char, 8> none{};
    CHECK(memory.write(2 * page, data.size(), data.data(), none.data()));
    CHECK(memory.allocated_pages() == 0);

    // The mixed case: a transfer that straddles two pages with every enabled
    // byte in the second one commits the second page only.
    const std::array<unsigned char, 8> second_half{0, 0, 0, 0, 1, 1, 1, 1};
    CHECK(memory.write(page - 4, data.size(), data.data(), second_half.data()));
    CHECK(memory.allocated_pages() == 1);
    CHECK(!memory.page_is_allocated(0));
    CHECK(memory.page_is_allocated(1));
}

void out_of_capacity_is_refused_and_changes_nothing()
{
    sparse_memory memory(2 * page);

    std::array<unsigned char, 8> buffer{};
    buffer.fill(0xC3);

    // Entirely outside.
    CHECK(!memory.read(2 * page, buffer.size(), buffer.data()));
    // Starts inside and runs past the end — the case a naive bounds check on
    // the start address alone would let through.
    CHECK(!memory.read(2 * page - 4, buffer.size(), buffer.data()));
    // A length chosen to wrap the 64-bit arithmetic.
    CHECK(!memory.read(4, 0xFFFF'FFFF'FFFF'FFFFull, buffer.data()));

    for (auto byte : buffer) {
        CHECK_MSG(byte == 0xC3,
                  "a refused read must leave the destination untouched; an "
                  "error is never converted to zero data");
    }

    const std::array<unsigned char, 8> data{1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(!memory.write(2 * page - 4, data.size(), data.data()));
    CHECK_MSG(memory.allocated_pages() == 0,
              "a refused write must not have committed a page");

    // The partially-in-range write really did store nothing.
    std::array<unsigned char, 8> back{};
    back.fill(0x11);
    CHECK(memory.read(2 * page - 8, back.size(), back.data()));
    for (auto byte : back) {
        CHECK(byte == 0);
    }

    // Zero-length accesses are legal no-ops, including at the very end.
    CHECK(memory.read(2 * page, 0, nullptr));
    CHECK(memory.write(2 * page, 0, nullptr));
}

void reset_releases_the_pages_but_not_the_high_water_mark()
{
    sparse_memory memory(64 * page);

    const std::array<unsigned char, 4> value{9, 9, 9, 9};
    for (std::uint64_t p = 0; p < 5; ++p) {
        CHECK(memory.write(p * page, value.size(), value.data()));
    }
    CHECK(memory.allocated_pages() == 5);
    CHECK(memory.peak_allocated_page_count() == 5);

    memory.reset();

    CHECK(memory.allocated_pages() == 0);
    CHECK(memory.allocated_bytes() == 0);
    CHECK_MSG(memory.peak_allocated_page_count() == 5,
              "the peak answers how much host memory the run needed, and a "
              "reset does not un-ask that");
    CHECK(memory.capacity() == 64 * page);

    std::array<unsigned char, 4> back{};
    back.fill(0x42);
    CHECK(memory.read(0, back.size(), back.data()));
    for (auto byte : back) {
        CHECK_MSG(byte == 0, "reset must restore the all-zero state");
    }
}

void a_gibibyte_costs_almost_nothing()
{
    // The `mesh_4x4` property, on one target. The index is eagerly sized —
    // 8 bytes per 4 KiB page, so 2 MiB here — and that is the whole host cost
    // until something is written.
    constexpr std::uint64_t gib = 1024ull * 1024 * 1024;
    sparse_memory memory(gib);

    CHECK(memory.capacity() == gib);
    CHECK(memory.page_count() == gib / page);
    CHECK(memory.allocated_bytes() == 0);

    // Reading the last byte of a 1 GiB window commits nothing.
    unsigned char byte = 0x5F;
    CHECK(memory.read(gib - 1, 1, &byte));
    CHECK(byte == 0);
    CHECK(memory.allocated_bytes() == 0);

    // Touching both ends commits exactly two pages, not the window.
    const unsigned char one = 1;
    CHECK(memory.write(0, 1, &one));
    CHECK(memory.write(gib - 1, 1, &one));
    CHECK_MSG(memory.allocated_bytes() == 2 * page,
              "writing two bytes must not commit more than two pages");

    CHECK(!memory.read(gib, 1, &byte));
}

void construction_rejects_a_zero_capacity()
{
    bool threw = false;
    try {
        sparse_memory memory(0);
        (void)memory;
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

void a_capacity_that_is_not_a_page_multiple_still_bounds_correctly()
{
    // Neither the SRAM nor global RAM can currently be configured this way —
    // both are powers of two at or above 4 KiB — but the class is general, and
    // a partial last page is exactly where an off-by-one in the bounds check
    // would hide.
    sparse_memory memory(page + 100);
    CHECK(memory.page_count() == 2);

    const std::array<unsigned char, 4> value{1, 2, 3, 4};
    CHECK(memory.write(page + 96, value.size(), value.data()));
    CHECK(!memory.write(page + 97, value.size(), value.data()));
    CHECK(memory.allocated_pages() == 1);
    CHECK(memory.page_is_allocated(1));
}

} // namespace

int main()
{
    an_untouched_store_reads_zero_and_costs_nothing();
    a_write_allocates_only_the_touched_pages();
    transfers_cross_page_boundaries();
    byte_enables_are_honoured_on_both_directions();
    a_fully_masked_write_allocates_nothing();
    out_of_capacity_is_refused_and_changes_nothing();
    reset_releases_the_pages_but_not_the_high_water_mark();
    a_gibibyte_costs_almost_nothing();
    construction_rejects_a_zero_capacity();
    a_capacity_that_is_not_a_page_multiple_still_bounds_correctly();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_sparse_memory: all checks passed\n";
    return 0;
}
