#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace fx1 {
// Byte-addressable backing store that allocates 4 KiB pages on first write.
// Unwritten bytes read as `fill`. Independent of TLM so a memory-controller
// model (e.g. the DDR3 controller) can reuse it behind its own front end.
class SparseStorage {
public:
    static constexpr std::size_t kPageBytes = 4096;

    explicit SparseStorage(std::uint64_t size, std::uint8_t fill = 0);

    std::uint64_t size() const noexcept { return size_; }
    std::uint8_t fill() const noexcept { return fill_; }

    // True when [offset, offset + length) lies inside the storage.
    bool contains(std::uint64_t offset, std::uint64_t length) const noexcept {
        return offset <= size_ && length <= size_ - offset;
    }

    // Throw std::out_of_range unless contains(offset, length).
    void read(std::uint64_t offset, std::uint8_t* data, std::size_t length) const;
    void write(std::uint64_t offset, const std::uint8_t* data, std::size_t length);
    // TLM byte-enable semantics: byte i is written only if
    // enables[i % enable_length] == 0xFF. A null mask writes every byte.
    void write_masked(std::uint64_t offset, const std::uint8_t* data, std::size_t length,
                      const std::uint8_t* enables, std::size_t enable_length);
    // Read with the same mask: disabled destination bytes are left untouched.
    void read_masked(std::uint64_t offset, std::uint8_t* data, std::size_t length,
                     const std::uint8_t* enables, std::size_t enable_length) const;

    // Pages that hold data; host memory is about resident_pages() * 4 KiB.
    std::size_t resident_pages() const noexcept { return pages_.size(); }
    // Drop every page, so all bytes read as fill() again.
    void clear() noexcept { pages_.clear(); }

private:
    using Page = std::array<std::uint8_t, kPageBytes>;
    void check(std::uint64_t offset, std::uint64_t length) const;
    const Page* find(std::uint64_t page) const;
    Page& touch(std::uint64_t page);

    std::uint64_t size_;
    std::uint8_t fill_;
    std::unordered_map<std::uint64_t, std::unique_ptr<Page>> pages_;
};
} // namespace fx1
