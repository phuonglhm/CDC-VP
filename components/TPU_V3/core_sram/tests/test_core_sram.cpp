// SPDX-License-Identifier: Apache-2.0
//
// Core SRAM unit tests (plan §11.3 and its Phase 3 gate).
//
// The properties that matter here are the ones a plausible implementation gets
// almost right:
//
//   * the window decodes and the capacity backs, and an access between the two
//     is an error rather than an alias into valid storage (D6);
//   * every payload from 1 to 64 bytes at every alignment works, and none is
//     required (D7);
//   * a refused access transfers nothing and leaves the caller's buffer
//     untouched — an error is never converted to zero data;
//   * a 16 MiB SRAM costs a few pages, not 16 MiB;
//   * the debug path bypasses the counters but not the bounds.

#include "tpu_v3/sram/core_sram.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <systemc>

namespace tpu = cdc::components::tpu_v3;
namespace sram = cdc::components::tpu_v3::sram;
namespace am = cdc::components::tpu_v3::address_map;

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

template <typename Fn>
bool throws_invalid_argument(Fn&& fn)
{
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

constexpr std::uint64_t kBase = 0xC000'0000ull;

sram::core_sram_config small_config(std::uint64_t capacity)
{
    sram::core_sram_config config;
    config.base_address = kBase;
    config.window_bytes = am::core_sram_window;
    config.capacity_bytes = capacity;
    return config;
}

void the_window_decodes_and_the_capacity_backs()
{
    // A bring-up capacity well below the frozen window, which is where the two
    // rules can be told apart at all.
    sram::core_sram sram_("sram_window", small_config(64 * 1024));

    CHECK(sram_.window_bytes() == 16u * 1024 * 1024);
    CHECK(sram_.capacity_bytes() == 64u * 1024);

    // Inside the capacity.
    CHECK(sram_.classify(kBase, 4) == sram::neo_status::ok);
    CHECK(sram_.classify(kBase + 64 * 1024 - 4, 4) == sram::neo_status::ok);

    // Inside the window, above the capacity. This must be its own error and
    // must never alias down: aliasing would turn a firmware pointer bug into
    // silent corruption whose symptom changes with the configured SRAM size.
    CHECK(sram_.classify(kBase + 64 * 1024, 4)
          == sram::neo_status::capacity_error);
    CHECK(sram_.classify(kBase + 16u * 1024 * 1024 - 4, 4)
          == sram::neo_status::capacity_error);
    // Straddling the capacity boundary is not backed either.
    CHECK(sram_.classify(kBase + 64 * 1024 - 2, 4)
          == sram::neo_status::capacity_error);

    // Outside the window entirely.
    CHECK(sram_.classify(kBase + 16u * 1024 * 1024, 4)
          == sram::neo_status::decode_error);
    CHECK(sram_.classify(kBase - 4, 4) == sram::neo_status::decode_error);
    // Starting inside and running past the window end.
    CHECK(sram_.classify(kBase + 16u * 1024 * 1024 - 2, 4)
          == sram::neo_status::decode_error);

    CHECK(sram_.in_window(kBase + 1024 * 1024, 4));
    CHECK(!sram_.is_backed(kBase + 1024 * 1024, 4));
}

void every_size_from_one_to_sixty_four_works_at_every_alignment()
{
    // Decision record D7: a memory target must accept any payload from 1 to
    // 64 bytes — 64 being one RVV register at VLEN=512 — and must never
    // *require* the largest. The synthetic loop is the proof; VP++ is not
    // required to emit a 64-byte payload and its absence from a VP++ trace is
    // not a defect.
    sram::core_sram sram_("sram_sizes", small_config(64 * 1024));

    std::vector<unsigned char> pattern(sram::neo_max_transfer_bytes);
    std::vector<unsigned char> back(sram::neo_max_transfer_bytes);

    for (std::uint32_t size = 1; size <= sram::neo_max_transfer_bytes; ++size) {
        for (std::uint32_t align = 0; align < 64; ++align) {
            const std::uint64_t address = kBase + 4096 + align * 128;
            for (std::uint32_t i = 0; i < size; ++i) {
                pattern[i] = static_cast<unsigned char>((size * 7 + align * 3
                                                         + i)
                                                        & 0xFF);
            }
            CHECK(sram_.write(address, size, pattern.data(), nullptr)
                  == sram::neo_status::ok);

            std::fill(back.begin(), back.end(), 0xEE);
            CHECK(sram_.read(address, size, back.data(), nullptr)
                  == sram::neo_status::ok);
            for (std::uint32_t i = 0; i < size; ++i) {
                CHECK_MSG(back[i] == pattern[i],
                          "size " + std::to_string(size) + " alignment "
                              + std::to_string(align) + " byte "
                              + std::to_string(i));
            }
        }
    }

    // Zero and oversized are size errors, not truncations.
    unsigned char one = 0;
    CHECK(sram_.classify(kBase, 0) == sram::neo_status::size_error);
    CHECK(sram_.read(kBase, 0, &one, nullptr) == sram::neo_status::size_error);
    CHECK(sram_.classify(kBase, 65) == sram::neo_status::size_error);
    std::array<unsigned char, 65> big{};
    CHECK(sram_.write(kBase, 65, big.data(), nullptr)
          == sram::neo_status::size_error);
}

void byte_enables_are_honoured()
{
    sram::core_sram sram_("sram_strobes", small_config(64 * 1024));

    const std::array<unsigned char, 8> initial{1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(sram_.write(kBase, 8, initial.data(), nullptr)
          == sram::neo_status::ok);

    // Non-contiguous, which a memory-like target must accept
    // (`INTERFACE_CONTRACT.md` §2).
    const std::array<unsigned char, 8> strobe{1, 0, 0, 1, 0, 0, 1, 0};
    const std::array<unsigned char, 8> update{0xA0, 0xA1, 0xA2, 0xA3,
                                              0xA4, 0xA5, 0xA6, 0xA7};
    CHECK(sram_.write(kBase, 8, update.data(), strobe.data())
          == sram::neo_status::ok);

    std::array<unsigned char, 8> back{};
    CHECK(sram_.read(kBase, 8, back.data(), nullptr) == sram::neo_status::ok);
    const std::array<unsigned char, 8> expected{0xA0, 2, 3, 0xA3, 5, 6, 0xA6, 8};
    CHECK(back == expected);

    // The counters follow the mask, not the payload size. Decision record D7
    // requires each counter to mean what its name says, and a masked write
    // that reported eight bytes would credit the model with bandwidth it
    // never carried. Eight fully enabled bytes, then three enabled of eight.
    CHECK_MSG(sram_.bytes_written() == 8 + 3,
              "a masked write must count only the bytes it moved");
    CHECK(sram_.write_accesses() == 2);
    CHECK(sram_.bytes_read() == 8);

    // A fully masked write moves nothing, and must not be counted as if it
    // had — nor may it commit a page.
    const std::array<unsigned char, 8> none{};
    const std::array<unsigned char, 8> ignored{9, 9, 9, 9, 9, 9, 9, 9};
    const std::size_t pages_before = sram_.allocated_page_count();
    CHECK(sram_.write(kBase + 2048, 8, ignored.data(), none.data())
          == sram::neo_status::ok);
    CHECK(sram_.bytes_written() == 8 + 3);
    CHECK(sram_.allocated_page_count() == pages_before);
}

void a_refused_access_changes_nothing_and_returns_no_data()
{
    sram::core_sram sram_("sram_refusal", small_config(8 * 1024));

    std::array<unsigned char, 8> buffer{};
    buffer.fill(0x5C);

    CHECK(sram_.read(kBase + 8 * 1024, 8, buffer.data(), nullptr)
          == sram::neo_status::capacity_error);
    CHECK(sram_.read(kBase + 16u * 1024 * 1024, 8, buffer.data(), nullptr)
          == sram::neo_status::decode_error);
    for (auto byte : buffer) {
        CHECK_MSG(byte == 0x5C,
                  "a refused read must leave the caller's buffer untouched; "
                  "an error is never converted to zero data");
    }

    const std::array<unsigned char, 8> data{1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(sram_.write(kBase + 8 * 1024 - 4, 8, data.data(), nullptr)
          == sram::neo_status::capacity_error);
    CHECK_MSG(sram_.allocated_backing_bytes() == 0,
              "a refused write must not have committed a page");

    CHECK(sram_.error_count() == 3);
    CHECK(sram_.bytes_written() == 0);
    CHECK(sram_.bytes_read() == 0);
}

void a_sixteen_mebibyte_sram_costs_a_few_pages()
{
    // The reference capacity (D6). The point of the sparse backing is that
    // sixteen of these plus a 1 GiB global RAM must elaborate on an ordinary
    // host, so an untouched one has to cost nothing but its page index.
    sram::core_sram sram_("sram_reference",
                          small_config(am::core_sram_default_capacity));

    CHECK(sram_.capacity_bytes() == 16u * 1024 * 1024);
    CHECK(sram_.allocated_backing_bytes() == 0);

    // Reading the far end of an untouched SRAM commits nothing.
    std::array<unsigned char, 64> buffer{};
    buffer.fill(0x99);
    CHECK(sram_.read(kBase + 16u * 1024 * 1024 - 64, 64, buffer.data(), nullptr)
          == sram::neo_status::ok);
    for (auto byte : buffer) {
        CHECK(byte == 0);
    }
    CHECK(sram_.allocated_backing_bytes() == 0);

    // Touching both ends commits two pages.
    const unsigned char one = 1;
    CHECK(sram_.write(kBase, 1, &one, nullptr) == sram::neo_status::ok);
    CHECK(sram_.write(kBase + 16u * 1024 * 1024 - 1, 1, &one, nullptr)
          == sram::neo_status::ok);
    CHECK(sram_.allocated_backing_bytes() == 2 * tpu::sparse_memory::page_size);
    CHECK(sram_.allocated_page_count() == 2);

    // A transfer crossing a page boundary is ordinary traffic, not a special
    // case: 64 bytes four bytes before the boundary spans two pages.
    std::array<unsigned char, 64> pattern{};
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        pattern[i] = static_cast<unsigned char>(i);
    }
    const std::uint64_t crossing = kBase + tpu::sparse_memory::page_size * 4 - 4;
    CHECK(sram_.write(crossing, 64, pattern.data(), nullptr)
          == sram::neo_status::ok);
    std::array<unsigned char, 64> back{};
    CHECK(sram_.read(crossing, 64, back.data(), nullptr)
          == sram::neo_status::ok);
    CHECK_MSG(back == pattern, "data was lost across a page boundary");
    CHECK(sram_.allocated_page_count() == 4);
}

void debug_access_bypasses_the_counters_but_not_the_bounds()
{
    sram::core_sram sram_("sram_debug", small_config(64 * 1024));

    const std::array<unsigned char, 4> value{0xDE, 0xAD, 0xBE, 0xEF};
    CHECK(sram_.debug_write(kBase + 32, 4, value.data(), nullptr)
          == sram::neo_status::ok);

    // Storage really changed...
    std::array<unsigned char, 4> back{};
    CHECK(sram_.debug_read(kBase + 32, 4, back.data(), nullptr)
          == sram::neo_status::ok);
    CHECK(back == value);

    // ...but a loader is not workload traffic, so none of it is counted
    // (`INTERFACE_CONTRACT.md` §8). Counting it would corrupt every metric
    // that follows.
    CHECK(sram_.read_accesses() == 0);
    CHECK(sram_.write_accesses() == 0);
    CHECK(sram_.bytes_read() == 0);
    CHECK(sram_.bytes_written() == 0);
    CHECK(sram_.debug_bytes_written() == 4);

    // Bounds are not relaxed for the debug path.
    CHECK(sram_.debug_write(kBase + 64 * 1024, 4, value.data(), nullptr)
          == sram::neo_status::capacity_error);
    CHECK(sram_.debug_read(kBase + 16u * 1024 * 1024, 4, back.data(), nullptr)
          == sram::neo_status::decode_error);
}

void counters_and_reset_behave()
{
    sram::core_sram sram_("sram_counters", small_config(64 * 1024));

    const std::array<unsigned char, 16> data{};
    for (int i = 0; i < 3; ++i) {
        CHECK(sram_.write(kBase + i * 64, 16, data.data(), nullptr)
              == sram::neo_status::ok);
    }
    std::array<unsigned char, 16> back{};
    CHECK(sram_.read(kBase, 16, back.data(), nullptr) == sram::neo_status::ok);

    CHECK(sram_.write_accesses() == 3);
    CHECK(sram_.read_accesses() == 1);
    CHECK(sram_.bytes_written() == 48);
    CHECK(sram_.bytes_read() == 16);
    CHECK(sram_.allocated_page_count() == 1);

    const std::uint64_t peak = sram_.peak_allocated_backing_bytes();
    CHECK(peak == tpu::sparse_memory::page_size);

    sram_.reset();

    CHECK(sram_.write_accesses() == 0);
    CHECK(sram_.bytes_written() == 0);
    CHECK(sram_.error_count() == 0);
    CHECK(sram_.allocated_backing_bytes() == 0);
    CHECK_MSG(sram_.peak_allocated_backing_bytes() == peak,
              "the peak answers how much host memory the run needed, which a "
              "reset does not un-ask");

    std::array<unsigned char, 16> zeros{};
    zeros.fill(0x77);
    CHECK(sram_.read(kBase, 16, zeros.data(), nullptr) == sram::neo_status::ok);
    for (auto byte : zeros) {
        CHECK_MSG(byte == 0, "reset must restore the all-zero state");
    }
}

void the_configuration_is_validated_at_construction()
{
    // Validation happens before anything is allocated, so a bad configuration
    // fails during elaboration rather than on the first transaction
    // (`INTERFACE_CONTRACT.md` §10).
    CHECK(throws_invalid_argument([] {
        auto config = small_config(am::core_sram_window * 2);
        sram::core_sram bad("sram_too_big", config);
    }));
    CHECK(throws_invalid_argument([] {
        auto config = small_config(0x3000); // not a power of two
        sram::core_sram bad("sram_odd_capacity", config);
    }));
    CHECK(throws_invalid_argument([] {
        auto config = small_config(4096);
        config.base_address = kBase + 4; // not naturally aligned
        sram::core_sram bad("sram_misaligned", config);
    }));
    CHECK(throws_invalid_argument([] {
        auto config = small_config(4096);
        config.base_address = 0xFFFF'F000ull; // window ends above 4 GiB
        sram::core_sram bad("sram_above_4gib", config);
    }));

    // And the report names window, capacity and backing together, because
    // read apart the three numbers are routinely mistaken for each other.
    sram::core_sram good("sram_report", small_config(64 * 1024));
    const std::string text = good.report();
    CHECK(text.find("decoded window") != std::string::npos);
    CHECK(text.find("backed capacity") != std::string::npos);
    CHECK(text.find("host backing") != std::string::npos);
    CHECK(text.find("never an alias") != std::string::npos);
}

} // namespace

int sc_main(int, char*[])
{
    the_window_decodes_and_the_capacity_backs();
    every_size_from_one_to_sixty_four_works_at_every_alignment();
    byte_enables_are_honoured();
    a_refused_access_changes_nothing_and_returns_no_data();
    a_sixteen_mebibyte_sram_costs_a_few_pages();
    debug_access_bypasses_the_counters_but_not_the_bounds();
    counters_and_reset_behave();
    the_configuration_is_validated_at_construction();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_core_sram: all checks passed\n";
    return 0;
}
