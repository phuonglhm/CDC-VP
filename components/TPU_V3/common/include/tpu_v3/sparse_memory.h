// SPDX-License-Identifier: Apache-2.0
//
// Deterministic sparse page-backed storage (decision record D6).
//
// The problem it solves: `mesh_4x4` describes 1.25 GiB of logical memory —
// sixteen 16 MiB core SRAMs plus a 1 GiB global RAM — and D6 requires that
// configuration to *elaborate* without committing 1.25 GiB of host memory. A
// `std::vector<unsigned char>` per target would commit all of it before a
// single instruction ran, and on a developer laptop the platform would simply
// fail to start.
//
// The rules, all of them from D6:
//
//   * an unallocated page reads as zero and consumes no page backing;
//   * the first write, or writable debug access, allocates only the touched
//     pages;
//   * byte enables and transfers crossing page boundaries keep normal TLM
//     semantics;
//   * reset releases the pages and restores the all-zero state;
//   * an access outside the instantiated capacity is refused, and the caller's
//     buffer is left untouched — an error is never converted to zero data
//     (`INTERFACE_CONTRACT.md` §1);
//   * the backing index is a plain vector of nullable page pointers indexed by
//     page number, not an unordered container: iteration order and therefore
//     host-memory behaviour must not vary between hosts.
//
// Plain C++ with no SystemC, so it is testable as an ordinary program and
// usable by both the core SRAM and the platform's global RAM.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace cdc::components::tpu_v3 {

class sparse_memory {
public:
    /// 4 KiB, as D6 specifies. It is also the natural granularity for the
    /// host: smaller pages multiply the index, larger ones make a single
    /// scattered write commit memory the workload never touches.
    static constexpr std::uint64_t page_size = 4096;

    /// Throws `std::invalid_argument` for a zero capacity.
    ///
    /// The index — one pointer per logical page — *is* eagerly sized, and that
    /// is deliberate: 8 bytes per 4 KiB page is 2 MiB of index for a 1 GiB
    /// window, it makes page lookup a single bounds-checked array read with no
    /// hashing, and it is what makes the allocation behaviour identical on
    /// every host.
    explicit sparse_memory(std::uint64_t capacity_bytes);

    std::uint64_t capacity() const noexcept { return capacity_; }
    std::size_t page_count() const noexcept { return pages_.size(); }

    /// Copy `[offset, offset + length)` into `out`.
    ///
    /// Unallocated pages read as zero and stay unallocated: a read never
    /// commits host memory, so a firmware scan over an untouched 16 MiB SRAM
    /// costs nothing. `strobes`, when non-null, is one byte per data byte and
    /// leaves the masked-out destination bytes untouched.
    ///
    /// Returns false — with `out` completely untouched — when the range is
    /// outside the capacity.
    bool read(std::uint64_t offset, std::uint64_t length, unsigned char* out,
              const unsigned char* strobes = nullptr) const;

    /// Copy `in` into `[offset, offset + length)`, allocating only the pages
    /// that receive at least one enabled byte.
    ///
    /// A page whose bytes are all masked out by `strobes` is not allocated:
    /// nothing was written to it, so committing memory for it would make the
    /// reported backing depend on the shape of the strobe pattern rather than
    /// on what the workload stored.
    ///
    /// Returns false, having written nothing, when the range is outside the
    /// capacity.
    bool write(std::uint64_t offset, std::uint64_t length,
               const unsigned char* in,
               const unsigned char* strobes = nullptr);

    /// True when `[offset, offset + length)` lies inside the capacity. Folded
    /// so a length near 2^64 cannot wrap the comparison into a false "inside".
    bool in_capacity(std::uint64_t offset, std::uint64_t length) const noexcept;

    /// Release every page and restore the all-zero state.
    ///
    /// `peak_allocated_bytes()` survives a reset on purpose: it answers "how
    /// much host memory did this run need", which a reset does not un-ask.
    void reset();

    // ── backing counters ─────────────────────────────────────────────────────
    //
    // Named for what they measure. `capacity()` is logical address space,
    // these are host memory; conflating the two is exactly the confusion D6
    // exists to prevent.

    std::uint64_t allocated_bytes() const noexcept
    {
        return static_cast<std::uint64_t>(allocated_pages_) * page_size;
    }
    std::uint64_t peak_allocated_bytes() const noexcept
    {
        return static_cast<std::uint64_t>(peak_allocated_pages_) * page_size;
    }
    std::size_t allocated_pages() const noexcept { return allocated_pages_; }
    std::size_t peak_allocated_page_count() const noexcept
    {
        return peak_allocated_pages_;
    }

    /// True when page `index` currently has backing. For tests and reports;
    /// no data path needs it.
    bool page_is_allocated(std::size_t index) const noexcept
    {
        return index < pages_.size() && pages_[index] != nullptr;
    }

private:
    unsigned char* allocate_page(std::size_t index);

    std::uint64_t capacity_ = 0;
    /// Indexed by page number. `nullptr` means "reads as zero, costs nothing".
    std::vector<std::unique_ptr<unsigned char[]>> pages_;
    std::size_t allocated_pages_ = 0;
    std::size_t peak_allocated_pages_ = 0;
};

} // namespace cdc::components::tpu_v3
