// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/sparse_memory.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace cdc::components::tpu_v3 {

namespace {

/// Bytes of `[offset, offset+length)` that fall in the page containing
/// `offset`. Every loop below advances by this, so a transfer crossing any
/// number of page boundaries is handled by the same code as one that does not
/// — which is the point: the crossing case is the one that gets special-cased
/// wrongly.
std::uint64_t bytes_in_first_page(std::uint64_t offset, std::uint64_t length)
{
    const std::uint64_t within = offset % sparse_memory::page_size;
    return std::min(length, sparse_memory::page_size - within);
}

} // namespace

sparse_memory::sparse_memory(std::uint64_t capacity_bytes)
    : capacity_(capacity_bytes)
{
    if (capacity_bytes == 0) {
        throw std::invalid_argument(
            "tpu_v3::sparse_memory: capacity must be greater than zero");
    }

    const std::uint64_t pages =
        (capacity_bytes + page_size - 1) / page_size;
    if (pages > static_cast<std::uint64_t>(pages_.max_size())) {
        std::ostringstream message;
        message << "tpu_v3::sparse_memory: a capacity of " << capacity_bytes
                << " bytes needs " << pages
                << " page slots, more than this host can index";
        throw std::invalid_argument(message.str());
    }
    pages_.resize(static_cast<std::size_t>(pages));
}

bool sparse_memory::in_capacity(std::uint64_t offset,
                                std::uint64_t length) const noexcept
{
    if (offset >= capacity_) {
        return false;
    }
    return length <= capacity_ - offset;
}

unsigned char* sparse_memory::allocate_page(std::size_t index)
{
    auto& page = pages_[index];
    if (page == nullptr) {
        // Value-initialised: an allocated page must be indistinguishable from
        // the zeros an unallocated one reads as, or the first write to a page
        // would change what its *other* bytes read back as.
        page = std::make_unique<unsigned char[]>(page_size);
        ++allocated_pages_;
        peak_allocated_pages_ = std::max(peak_allocated_pages_, allocated_pages_);
    }
    return page.get();
}

bool sparse_memory::read(std::uint64_t offset, std::uint64_t length,
                         unsigned char* out,
                         const unsigned char* strobes) const
{
    // A zero-length access touches nothing, so there is nothing for it to be
    // outside of. It is answered before the bounds check rather than after,
    // so the answer does not depend on the offset it names. The TLM contract
    // forbids `data_length == 0` anyway; refusing it is the target's job, not
    // the storage's.
    if (length == 0) {
        return true;
    }
    // Bounds next, and nothing written before the check passes: a refused
    // read must leave the caller's buffer exactly as it was.
    if (!in_capacity(offset, length)) {
        return false;
    }
    if (out == nullptr) {
        throw std::invalid_argument(
            "tpu_v3::sparse_memory::read: null destination for a non-empty "
            "transfer");
    }

    std::uint64_t done = 0;
    while (done < length) {
        const std::uint64_t here = bytes_in_first_page(offset + done,
                                                       length - done);
        const std::size_t page_index =
            static_cast<std::size_t>((offset + done) / page_size);
        const std::size_t within =
            static_cast<std::size_t>((offset + done) % page_size);
        const unsigned char* page = pages_[page_index].get();

        if (strobes == nullptr) {
            if (page != nullptr) {
                std::memcpy(out + done, page + within,
                            static_cast<std::size_t>(here));
            } else {
                std::memset(out + done, 0, static_cast<std::size_t>(here));
            }
        } else {
            for (std::uint64_t i = 0; i < here; ++i) {
                if (strobes[done + i] == 0) {
                    continue;
                }
                out[done + i] = page != nullptr ? page[within + i] : 0;
            }
        }
        done += here;
    }
    return true;
}

bool sparse_memory::write(std::uint64_t offset, std::uint64_t length,
                          const unsigned char* in,
                          const unsigned char* strobes)
{
    if (length == 0) {
        return true;
    }
    if (!in_capacity(offset, length)) {
        return false;
    }
    if (in == nullptr) {
        throw std::invalid_argument(
            "tpu_v3::sparse_memory::write: null source for a non-empty "
            "transfer");
    }

    std::uint64_t done = 0;
    while (done < length) {
        const std::uint64_t here = bytes_in_first_page(offset + done,
                                                       length - done);
        const std::size_t page_index =
            static_cast<std::size_t>((offset + done) / page_size);
        const std::size_t within =
            static_cast<std::size_t>((offset + done) % page_size);

        if (strobes == nullptr) {
            unsigned char* page = allocate_page(page_index);
            std::memcpy(page + within, in + done,
                        static_cast<std::size_t>(here));
            done += here;
            continue;
        }

        // Count the enabled bytes before allocating. A page whose slice is
        // entirely masked out receives nothing, so it must stay unbacked;
        // otherwise the reported backing would depend on the shape of the
        // strobe pattern rather than on what was stored.
        bool any_enabled = false;
        for (std::uint64_t i = 0; i < here && !any_enabled; ++i) {
            any_enabled = strobes[done + i] != 0;
        }
        if (any_enabled) {
            unsigned char* page = allocate_page(page_index);
            for (std::uint64_t i = 0; i < here; ++i) {
                if (strobes[done + i] == 0) {
                    continue;
                }
                page[within + i] = in[done + i];
            }
        }
        done += here;
    }
    return true;
}

void sparse_memory::reset()
{
    for (auto& page : pages_) {
        page.reset();
    }
    allocated_pages_ = 0;
    // peak_allocated_pages_ deliberately survives: it answers how much host
    // memory the run needed, and a reset does not un-ask that.
}

} // namespace cdc::components::tpu_v3
