// SPDX-License-Identifier: Apache-2.0
//
// Contract test for the TLM wrapper.
//
// The network underneath is RTL-signed, so what this test has to establish is
// that the wrapper drives it correctly: that a TLM payload becomes the right
// AXI transaction, that data survives the round trip, that a burst stays one
// packet, that distance costs cycles, and that debug access does not touch the
// network at all.

#include "floo_noc_model/noc_interconnect.h"
#include "floo_noc_model/axi_types.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

namespace {

int failures = 0;

void check(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

/// A byte-addressed memory with an optional access latency, so the wrapper's
/// handling of a target's own delay is exercised.
class memory_target : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<memory_target> socket;

    SC_HAS_PROCESS(memory_target);

    memory_target(
        sc_core::sc_module_name name, std::size_t bytes,
        sc_core::sc_time latency = sc_core::SC_ZERO_TIME)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(bytes, 0)
        , latency_(latency)
    {
        socket.register_b_transport(this, &memory_target::b_transport);
        socket.register_transport_dbg(this, &memory_target::transport_dbg);
    }

    std::vector<unsigned char>& storage() { return storage_; }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        // Only a successful access reports OK. The previous version set OK
        // unconditionally after `access()`, which silently erased the address
        // error the same function had just raised — so an out-of-range access
        // looked like a clean read of zeroes.
        const unsigned served = access(trans);
        delay += latency_;
        if (trans.get_response_status() == tlm::TLM_INCOMPLETE_RESPONSE) {
            trans.set_response_status(served == trans.get_data_length()
                                          ? tlm::TLM_OK_RESPONSE
                                          : tlm::TLM_GENERIC_ERROR_RESPONSE);
        }
    }

    /// Debug access reports what it really moved, so a caller can tell a
    /// partial or refused access from a complete one.
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans)
    {
        return access(trans);
    }

    /// Returns the number of bytes actually transferred.
    ///
    /// Byte enables are honoured, because a wrapper that builds the wrong
    /// `WSTRB` can only be caught by a target that refuses to write the
    /// disabled lanes. TLM byte enables repeat with period
    /// `byte_enable_length` when that is shorter than the payload.
    unsigned int access(tlm::tlm_generic_payload& trans)
    {
        const auto address = static_cast<std::size_t>(trans.get_address());
        const auto length = trans.get_data_length();
        auto* const data = trans.get_data_ptr();

        if (data == nullptr && length != 0) {
            trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return 0;
        }
        if (address + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return 0;
        }
        // A wrapped streaming transfer is not modelled here. The wrapper is
        // supposed to reject it upstream, so seeing one is a wrapper defect and
        // must not be papered over.
        const auto streaming = trans.get_streaming_width();
        if (streaming != 0 && streaming < length) {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return 0;
        }

        const auto* const enables = trans.get_byte_enable_ptr();
        const auto enable_length = trans.get_byte_enable_length();
        if (enables != nullptr && enable_length == 0) {
            trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return 0;
        }

        for (unsigned index = 0; index < length; ++index) {
            if (enables != nullptr
                && enables[index % enable_length] != TLM_BYTE_ENABLED) {
                continue;
            }
            if (trans.is_write()) {
                storage_[address + index] = data[index];
            } else {
                data[index] = storage_[address + index];
            }
        }
        return length;
    }

    std::vector<unsigned char> storage_;
    sc_core::sc_time latency_;
};

constexpr std::uint64_t near_base = 0x8000'0000;
constexpr std::uint64_t far_base = 0x9000'0000;
constexpr std::uint64_t region_size = 0x1000;
/// A region the wrapper maps in full, backed by a target with less storage
/// than that. Reaching past the storage produces a target-side refusal — the
/// only way to distinguish `SLVERR` from `DECERR` at the TLM boundary.
constexpr std::uint64_t oversized_base = 0xA000'0000;
constexpr std::size_t oversized_storage = 0x100;

/// Four targets on one node, differing only in their annotated latency, so a
/// timing comparison between them isolates rounding from distance.
/// An MMIO target that records every downstream access it is given, so a test
/// can assert on what the wrapper actually asked for rather than on what came
/// back. Reading a register it was not asked for is exactly the failure the
/// widened-read policy exists to prevent, and only the target can see it.
constexpr std::uint64_t spy_base = 0xC000'0000;

/// A region whose last byte is `UINT64_MAX`. `base + size` wraps to zero here,
/// so an addition-based decode never matches it and the whole region becomes
/// unreachable. Only an *access* shows that; accepting the mapping does not.
constexpr std::uint64_t top_base = 0xFFFF'FFFF'FFFF'F000ull;

/// A region whose **end is not bus-aligned**, backed by a target with more
/// storage than the region.
///
/// This is the only shape that reaches the beat-frame guard. With an
/// 8-byte-aligned region end, any access whose widened frame runs past the
/// region also has its *requested* last byte outside the region, so the
/// whole-range decode rejects it first and the frame guard is never
/// evaluated — which is exactly how an earlier version of these tests passed
/// with the guard deleted.
///
/// `0x1004` puts the region end four bytes into a bus word. A 6-byte access at
/// `+0x0FFE` has every requested byte inside the region and a beat frame that
/// reaches `+0x1007`, four bytes past it.
constexpr std::uint64_t frame_base = 0xD000'0000;
constexpr std::uint64_t frame_size = 0x1004;
constexpr std::size_t frame_storage = 0x2000;

constexpr std::uint64_t zero_base = 0xB000'0000;
constexpr std::uint64_t sub_base = 0xB001'0000;
constexpr std::uint64_t frac_base = 0xB002'0000;
constexpr std::uint64_t exact_base = 0xB003'0000;

/// A second AXI manager on the mesh, from its own node. The platform puts the
/// DMA on one of these, so the path has to be exercised: two managers issuing
/// concurrently share links and contend for the same target.
class spy_target : public sc_core::sc_module {
public:
    struct access_record {
        std::uint64_t address;
        unsigned length;
        bool is_write;
    };

    tlm_utils::simple_target_socket<spy_target> socket;
    std::vector<access_record> accesses;

    SC_HAS_PROCESS(spy_target);

    /// `storage` is deliberately independent of the region the wrapper maps.
    /// A target backed by *more* memory than its declared region is what makes
    /// an over-reaching beat frame observable: without a frame guard the extra
    /// beat lands inside the backing store and succeeds silently, so the
    /// response code alone would prove nothing.
    explicit spy_target(sc_core::sc_module_name name,
                        std::size_t storage = 0x1000)
        : sc_core::sc_module(name)
        , socket("socket")
        , storage_(storage, 0)
    {
        socket.register_b_transport(this, &spy_target::b_transport);
    }

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
    {
        (void)delay;
        const auto address = static_cast<std::size_t>(trans.get_address());
        const auto length = trans.get_data_length();
        accesses.push_back({trans.get_address(), length, trans.is_write()});

        if (address + length > storage_.size()) {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        for (unsigned index = 0; index < length; ++index) {
            if (trans.is_write()) {
                storage_[address + index] = trans.get_data_ptr()[index];
            } else {
                trans.get_data_ptr()[index] = storage_[address + index];
            }
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }

    std::vector<unsigned char> storage_;
};

class second_manager : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<second_manager> socket;
    bool finished = false;
    unsigned completed = 0;

    SC_HAS_PROCESS(second_manager);

    explicit second_manager(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        SC_THREAD(run);
    }

private:
    void run()
    {
        for (unsigned index = 0; index < 8; ++index) {
            std::uint64_t value = 0xA000 + index;
            unsigned char bytes[8] = {};
            std::memcpy(bytes, &value, sizeof(value));

            tlm::tlm_generic_payload trans;
            trans.set_command(tlm::TLM_WRITE_COMMAND);
            trans.set_address(far_base + 0x200 + index * 8);
            trans.set_data_ptr(bytes);
            trans.set_data_length(sizeof(value));
            trans.set_streaming_width(sizeof(value));
            trans.set_byte_enable_ptr(nullptr);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            socket->b_transport(trans, delay);
            check(trans.is_response_ok(),
                  "the second manager's access must complete");
            ++completed;
        }
        finished = true;
    }
};

/// Fails the run rather than letting it hang.
///
/// Every wait in this file is individually bounded, but a bound that is itself
/// never reached — a `wait` on an event nothing will notify — needs a process
/// outside the driver to notice. This is that process.
class watchdog : public sc_core::sc_module {
public:
    bool driver_finished = false;

    SC_HAS_PROCESS(watchdog);

    explicit watchdog(sc_core::sc_module_name name, sc_core::sc_time limit)
        : sc_core::sc_module(name)
        , limit_(limit)
    {
        SC_THREAD(run);
    }

private:
    void run()
    {
        wait(limit_);
        if (!driver_finished) {
            check(false, "the test did not finish within the global watchdog");
            sc_core::sc_stop();
        }
    }

    sc_core::sc_time limit_;
};

class driver : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<driver> socket;
    cdc::components::noc_interconnect* noc = nullptr;
    memory_target* near_memory = nullptr;
    memory_target* far_memory = nullptr;
    second_manager* other = nullptr;
    watchdog* dog = nullptr;
    spy_target* spy = nullptr;
    spy_target* frame_spy = nullptr;

    SC_HAS_PROCESS(driver);

    explicit driver(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
        SC_THREAD(run);
    }

private:
    /// One blocking access. Returns the simulated time it took, which for this
    /// wrapper is spent rather than annotated.
    sc_core::sc_time access(
        bool write, std::uint64_t address, unsigned char* bytes,
        unsigned length)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(write ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND);
        trans.set_address(address);
        trans.set_data_ptr(bytes);
        trans.set_data_length(length);
        trans.set_streaming_width(length);
        trans.set_byte_enable_ptr(nullptr);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        const auto before = sc_core::sc_time_stamp();
        socket->b_transport(trans, delay);
        const auto elapsed = sc_core::sc_time_stamp() - before;

        check(trans.is_response_ok(),
              "the access must complete with an OK response");
        return elapsed;
    }

    /// A full-control access, for the payload-contract and lane tests.
    /// Returns the response status rather than asserting it, so a test can
    /// require a *specific* failure.
    tlm::tlm_response_status raw_access(
        tlm::tlm_command command, std::uint64_t address, unsigned char* bytes,
        unsigned length, const unsigned char* enables = nullptr,
        unsigned enable_length = 0, unsigned streaming = 0)
    {
        tlm::tlm_generic_payload trans;
        trans.set_command(command);
        trans.set_address(address);
        trans.set_data_ptr(bytes);
        trans.set_data_length(length);
        trans.set_streaming_width(streaming != 0 ? streaming : length);
        trans.set_byte_enable_ptr(const_cast<unsigned char*>(enables));
        trans.set_byte_enable_length(enable_length);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }

    /// Reads target memory directly, bypassing the network. The point of the
    /// lane tests is what actually landed in the target, not what a matching
    /// pair of packing bugs hands back.
    static std::uint64_t peek(
        memory_target& memory, std::size_t offset, unsigned length)
    {
        std::uint64_t value = 0;
        for (unsigned index = 0; index < length; ++index) {
            value |= static_cast<std::uint64_t>(
                         memory.storage()[offset + index])
                  << (8 * index);
        }
        return value;
    }

    // ---- F1: the TLM generic-payload contract ------------------------------
    void test_payload_contract()
    {
        unsigned char buffer[64] = {};

        check(raw_access(tlm::TLM_IGNORE_COMMAND, near_base, buffer, 8)
                  == tlm::TLM_COMMAND_ERROR_RESPONSE,
              "TLM_IGNORE_COMMAND must be refused, not treated as a read");

        check(raw_access(tlm::TLM_READ_COMMAND, near_base, nullptr, 8)
                  == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "a null data pointer with non-zero length must be refused");

        check(raw_access(tlm::TLM_READ_COMMAND, near_base, buffer, 0)
                  == tlm::TLM_BURST_ERROR_RESPONSE,
              "a zero-length payload must be refused");

        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base, buffer, 16,
                         nullptr, 0, /*streaming=*/8)
                  == tlm::TLM_BURST_ERROR_RESPONSE,
              "a wrapped streaming width must be refused, not flattened");

        unsigned char one_enable = TLM_BYTE_ENABLED;
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base, buffer, 8,
                         &one_enable, /*enable_length=*/0)
                  == tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
              "byte enables with zero length must be refused");

        // ---- a partial write must leave the disabled bytes alone ----------
        const std::size_t offset = 0x100;
        for (unsigned index = 0; index < 8; ++index) {
            near_memory->storage()[offset + index] =
                static_cast<unsigned char>(0xC0 + index);
        }
        std::uint64_t pattern = 0x1122'3344'5566'7788ull;
        std::memcpy(buffer, &pattern, sizeof(pattern));

        // Contiguous run: bytes 2..5 enabled.
        unsigned char enables[8] = {0, 0, TLM_BYTE_ENABLED, TLM_BYTE_ENABLED,
                                    TLM_BYTE_ENABLED, TLM_BYTE_ENABLED, 0, 0};
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + offset, buffer, 8,
                         enables, 8)
                  == tlm::TLM_OK_RESPONSE,
              "a contiguous byte-enable write must be accepted");
        check(near_memory->storage()[offset + 0] == 0xC0
                  && near_memory->storage()[offset + 1] == 0xC1
                  && near_memory->storage()[offset + 6] == 0xC6
                  && near_memory->storage()[offset + 7] == 0xC7,
              "a disabled byte must be left unchanged in target memory");
        check(peek(*near_memory, offset + 2, 4) == 0x3344'5566ull,
              "the enabled bytes must land, in the right order");

        // ---- non-contiguous is representable in WSTRB, so it must work ----
        const std::size_t sparse = 0x140;
        for (unsigned index = 0; index < 8; ++index) {
            near_memory->storage()[sparse + index] = 0;
        }
        unsigned char sparse_enables[8] = {
            TLM_BYTE_ENABLED, 0, TLM_BYTE_ENABLED, 0,
            TLM_BYTE_ENABLED, 0, TLM_BYTE_ENABLED, 0};
        std::uint64_t sparse_pattern = 0xAABB'CCDD'EEFF'0011ull;
        std::memcpy(buffer, &sparse_pattern, sizeof(sparse_pattern));
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + sparse, buffer, 8,
                         sparse_enables, 8)
                  == tlm::TLM_OK_RESPONSE,
              "a non-contiguous byte-enable write must be accepted");
        check(near_memory->storage()[sparse + 0] == 0x11
                  && near_memory->storage()[sparse + 1] == 0x00
                  && near_memory->storage()[sparse + 2] == 0xFF
                  && near_memory->storage()[sparse + 3] == 0x00
                  && near_memory->storage()[sparse + 4] == 0xDD
                  && near_memory->storage()[sparse + 6] == 0xBB,
              "every enabled lane, and only those, must be written");
    }

    // ---- F3: byte-lane placement -------------------------------------------
    void test_lane_placement()
    {
        unsigned char buffer[64] = {};

        // A 32-bit access at +4 lives in the upper half of the bus. Placing it
        // in lanes 0..3 was the defect; a direct memory check is the only way
        // to see it, because a matching unpack bug hides it on readback.
        const std::size_t at4 = 0x200;
        std::uint32_t word = 0x89AB'CDEFu;
        std::memcpy(buffer, &word, sizeof(word));
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + at4 + 4, buffer, 4)
                  == tlm::TLM_OK_RESPONSE,
              "a 32-bit write at +4 must be accepted");
        check(peek(*near_memory, at4 + 4, 4) == 0x89AB'CDEFull,
              "a 32-bit write at +4 must land at +4, not at +0");
        check(peek(*near_memory, at4, 4) == 0,
              "it must not disturb the lower half of the bus word");

        // 16-bit at +6: the last two lanes.
        const std::size_t at6 = 0x210;
        std::uint16_t half = 0xBEEF;
        std::memcpy(buffer, &half, sizeof(half));
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + at6 + 6, buffer, 2)
                  == tlm::TLM_OK_RESPONSE,
              "a 16-bit write at +6 must be accepted");
        check(peek(*near_memory, at6 + 6, 2) == 0xBEEFull,
              "a 16-bit write at +6 must land at +6");
        check(peek(*near_memory, at6, 6) == 0,
              "it must not disturb the lanes below it");

        // Every aligned natural width.
        const std::size_t sizes_at = 0x220;
        for (unsigned width : {1u, 2u, 4u, 8u}) {
            std::uint64_t value = 0;
            for (unsigned index = 0; index < width; ++index) {
                value |= static_cast<std::uint64_t>(0x40 + index) << (8 * index);
            }
            std::memcpy(buffer, &value, width);
            const std::size_t here = sizes_at + width * 8;
            check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + here, buffer,
                             width)
                      == tlm::TLM_OK_RESPONSE,
                  "an aligned natural-width write must be accepted");
            check(peek(*near_memory, here, width) == value,
                  "an aligned natural-width write must land exactly");
        }

        // An odd length, and one that straddles a beat boundary.
        const std::size_t odd_at = 0x280;
        unsigned char six[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
        std::memcpy(buffer, six, sizeof(six));
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + odd_at + 5, buffer,
                         6)
                  == tlm::TLM_OK_RESPONSE,
              "a 6-byte write straddling a beat boundary must be accepted");
        for (unsigned index = 0; index < 6; ++index) {
            check(near_memory->storage()[odd_at + 5 + index] == six[index],
                  "every byte of a straddling write must land at its address");
        }
        check(near_memory->storage()[odd_at + 4] == 0
                  && near_memory->storage()[odd_at + 11] == 0,
              "a straddling write must not spill outside its range");

        // Read it back through the network: the lanes have to be undone with
        // the same offset they were built with.
        std::memset(buffer, 0, sizeof(buffer));
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + odd_at + 5, buffer,
                         6)
                  == tlm::TLM_OK_RESPONSE,
              "reading back a straddling range must be accepted");
        check(std::memcmp(buffer, six, sizeof(six)) == 0,
              "a straddling read must return the bytes it wrote");

        // Running off the end of a mapped region must be refused, not replayed
        // against the first target it decoded.
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + region_size - 4,
                         buffer, 8)
                  == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "a transfer crossing a region boundary must be refused");
    }

    // ---- F2/F6: response classification ------------------------------------
    void test_response_classification()
    {
        unsigned char buffer[64] = {};

        check(raw_access(tlm::TLM_READ_COMMAND, 0x1234'0000, buffer, 8)
                  == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "an unmapped address must decode to an address error (DECERR)");

        // A mapped address the target itself refuses. The near memory is
        // `region_size` bytes, and the wrapper maps exactly that, so reach the
        // target through a region whose declared size exceeds its storage.
        check(raw_access(tlm::TLM_READ_COMMAND, oversized_base, buffer, 8)
                  == tlm::TLM_OK_RESPONSE,
              "inside the target's storage the access must succeed");
        // A mapped target that refuses. Both directions, so B/SLVERR and
        // R/SLVERR are each exercised — one of them passing says nothing about
        // the other, since they travel different response paths.
        check(raw_access(tlm::TLM_READ_COMMAND,
                         oversized_base + oversized_storage + 0x40, buffer, 8)
                  == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "a read a mapped target refuses must surface as SLVERR "
              "(R path), distinct from the DECERR of an unmapped address");
        check(raw_access(tlm::TLM_WRITE_COMMAND,
                         oversized_base + oversized_storage + 0x40, buffer, 8)
                  == tlm::TLM_GENERIC_ERROR_RESPONSE,
              "a write a mapped target refuses must surface as SLVERR "
              "(B path)");

        // Said plainly because the earlier version of this test implied
        // otherwise: the unmapped case above is rejected at the TLM boundary
        // *before* injection. It proves the address decode, not that an AXI
        // DECERR flit ever traversed the network. The B/R response codes
        // themselves are exercised in `test_axi_endpoint`, where a flit can be
        // crafted with each of the four values.
    }

    // ---- F4: delay semantics ------------------------------------------------
    void test_delay_contract()
    {
        unsigned char buffer[8] = {};

        // An incoming annotated delay is time the caller has not yet spent.
        // It must be waited out, not dropped, and must not come back.
        {
            tlm::tlm_generic_payload trans;
            trans.set_command(tlm::TLM_READ_COMMAND);
            trans.set_address(near_base);
            trans.set_data_ptr(buffer);
            trans.set_data_length(8);
            trans.set_streaming_width(8);
            trans.set_byte_enable_ptr(nullptr);
            trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

            sc_core::sc_time delay(37, sc_core::SC_NS);
            const auto before = sc_core::sc_time_stamp();
            socket->b_transport(trans, delay);
            const auto elapsed = sc_core::sc_time_stamp() - before;

            check(trans.is_response_ok(), "the delayed access must complete");
            check(delay == sc_core::SC_ZERO_TIME,
                  "delay must be cleared: this wrapper spends time");
            check(elapsed >= sc_core::sc_time(37, sc_core::SC_NS),
                  "an incoming delay must be spent, not silently discarded");
        }

        // Target latency, rounded up to whole network cycles.
        //
        // All four of these targets sit on the *same* mesh node, so hop count
        // is identical and the only difference between the measurements is the
        // target's own annotated delay. Comparing against a target on another
        // node would measure distance, not rounding.
        const auto zero = timed_access(zero_base);      // 0 ns
        const auto sub = timed_access(sub_base);        // 0.4 ns -> 1 cycle
        const auto frac = timed_access(frac_base);      // 1.5 ns -> 2 cycles
        const auto exact = timed_access(exact_base);    // 2.0 ns -> 2 cycles

        check(sub > zero,
              "a sub-cycle target latency must cost one cycle, not zero");
        check(frac == exact,
              "1.5 and 2.0 cycles must both round up to 2: that is the ceiling");
        check(frac > sub, "2 cycles must cost more than 1");
    }

    /// One 8-byte read, returning the simulated time it took.
    sc_core::sc_time timed_access(std::uint64_t base)
    {
        unsigned char buffer[8] = {};
        const auto before = sc_core::sc_time_stamp();
        const auto status = raw_access(tlm::TLM_READ_COMMAND, base, buffer, 8);
        check(status == tlm::TLM_OK_RESPONSE,
              "a timed access must complete before its time is compared");
        return sc_core::sc_time_stamp() - before;
    }

    // ---- V1: reset-time submission and idle-to-active wake-up --------------
    void test_reset_and_wakeup()
    {
        unsigned char buffer[8] = {};

        // This runs first, at simulated time zero, while the mesh is still in
        // reset. The wrapper must hold the transaction until reset is released
        // rather than handing flits to a mesh that will swallow them — that was
        // a real hang once.
        check(sc_core::sc_time_stamp() == sc_core::SC_ZERO_TIME,
              "the reset-time case must actually be issued at time zero");
        std::uint64_t value = 0x0BAD'F00Dull;
        std::memcpy(buffer, &value, sizeof(value));
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + 0x300, buffer, 8)
                  == tlm::TLM_OK_RESPONSE,
              "a transaction submitted during reset must still complete");
        check(sc_core::sc_time_stamp() > sc_core::SC_ZERO_TIME,
              "it must have waited for reset to be released");

        std::memset(buffer, 0, sizeof(buffer));
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + 0x300, buffer, 8)
                  == tlm::TLM_OK_RESPONSE,
              "and its data must be readable afterwards");
        std::uint64_t seen = 0;
        std::memcpy(&seen, buffer, sizeof(seen));
        check(seen == value,
              "a reset-time write must land, not be swallowed by the mesh");

        // Idle-to-active wake-up. The network clock stops when nothing is in
        // flight; a new transaction has to restart it. Waiting long enough for
        // the mesh to go quiet and then issuing again exercises that path — if
        // the `work` event were not notified, this would hang and the watchdog
        // would fire.
        wait(sc_core::sc_time(2, sc_core::SC_US));
        const auto before_idle = noc->elapsed_cycles();
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + 0x300, buffer, 8)
                  == tlm::TLM_OK_RESPONSE,
              "a transaction after a long idle period must complete");
        check(noc->elapsed_cycles() > before_idle,
              "waking the network must advance its cycle count");
    }

    // ---- R2-F2: sparse multi-beat writes ----------------------------------
    //
    // Every case here has at least one *fully disabled beat*. That is the shape
    // that broke: the wrapper moved its address to the lowest enabled byte and
    // then re-derived beat numbers from the moved address, so a write whose
    // first beat was entirely disabled had its data placed one beat too early —
    // dropped, while still returning success.
    //
    // These go through `noc_interconnect`, the endpoints, the mesh and the
    // downstream replay. A `pack_write()` unit test cannot see the defect,
    // because the defect is in what happens to the beats afterwards.
    void test_sparse_multibeat_writes()
    {
        auto fill = [&](std::size_t offset, unsigned length,
                        unsigned char value) {
            for (unsigned index = 0; index < length; ++index) {
                near_memory->storage()[offset + index] = value;
            }
        };

        // Case 1: two beats, first fully disabled, one byte enabled in beat 1.
        {
            const std::size_t at = 0x300;
            fill(at, 16, 0x5A);
            unsigned char data[16];
            for (unsigned index = 0; index < 16; ++index) {
                data[index] = static_cast<unsigned char>(0xA0 + index);
            }
            unsigned char enables[16] = {};
            enables[8] = TLM_BYTE_ENABLED;   // lane 0 of beat 1

            check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + at, data, 16,
                             enables, 16) == tlm::TLM_OK_RESPONSE,
                  "a write with a fully disabled first beat must be accepted");
            check(near_memory->storage()[at + 8] == 0xA8,
                  "the enabled byte in beat 1 must land at its own address, "
                  "not one beat earlier");
            for (unsigned index = 0; index < 16; ++index) {
                if (index == 8) {
                    continue;
                }
                check(near_memory->storage()[at + index] == 0x5A,
                      "every disabled byte must be untouched");
            }
        }

        // Case 2: two beats, second fully disabled.
        {
            const std::size_t at = 0x320;
            fill(at, 16, 0x33);
            unsigned char data[16];
            for (unsigned index = 0; index < 16; ++index) {
                data[index] = static_cast<unsigned char>(0xB0 + index);
            }
            unsigned char enables[16] = {};
            enables[2] = TLM_BYTE_ENABLED;
            enables[3] = TLM_BYTE_ENABLED;

            check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + at, data, 16,
                             enables, 16) == tlm::TLM_OK_RESPONSE,
                  "a write with a fully disabled second beat must be accepted");
            check(near_memory->storage()[at + 2] == 0xB2
                      && near_memory->storage()[at + 3] == 0xB3,
                  "the enabled bytes must land");
            check(near_memory->storage()[at + 8] == 0x33
                      && near_memory->storage()[at + 15] == 0x33,
                  "the disabled second beat must not be written");
        }

        // Case 3: three beats, only the middle one enabled.
        {
            const std::size_t at = 0x340;
            fill(at, 24, 0x77);
            unsigned char data[24];
            for (unsigned index = 0; index < 24; ++index) {
                data[index] = static_cast<unsigned char>(0xC0 + index);
            }
            unsigned char enables[24] = {};
            enables[9] = TLM_BYTE_ENABLED;
            enables[13] = TLM_BYTE_ENABLED;  // sparse lanes, later beat

            check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + at, data, 24,
                             enables, 24) == tlm::TLM_OK_RESPONSE,
                  "a write with only the middle beat enabled must be accepted");
            check(near_memory->storage()[at + 9] == 0xC9
                      && near_memory->storage()[at + 13] == 0xCD,
                  "sparse lanes in a later beat must land at their addresses");
            check(near_memory->storage()[at + 8] == 0x77
                      && near_memory->storage()[at + 10] == 0x77
                      && near_memory->storage()[at + 23] == 0x77,
                  "nothing outside the enabled addresses may change");
        }

        // Case 4: a short repeating byte-enable array spanning several beats.
        {
            const std::size_t at = 0x380;
            fill(at, 16, 0x11);
            unsigned char data[16];
            for (unsigned index = 0; index < 16; ++index) {
                data[index] = static_cast<unsigned char>(0xD0 + index);
            }
            // Period 4: enable one byte in every four, across both beats.
            unsigned char enables[4] = {TLM_BYTE_ENABLED, 0, 0, 0};

            check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + at, data, 16,
                             enables, 4) == tlm::TLM_OK_RESPONSE,
                  "a repeating byte-enable array must be accepted");
            for (unsigned index = 0; index < 16; ++index) {
                const bool enabled = (index % 4) == 0;
                const unsigned char expect =
                    enabled ? static_cast<unsigned char>(0xD0 + index) : 0x11;
                check(near_memory->storage()[at + index] == expect,
                      "a repeating enable pattern must apply in every beat, "
                      "at the right addresses");
            }
        }

        // Case 5: every lane disabled. Legal AXI, and it must touch nothing.
        {
            const std::size_t at = 0x3A0;
            fill(at, 16, 0x99);
            unsigned char data[16];
            std::memset(data, 0xEE, sizeof(data));
            unsigned char enables[16] = {};

            check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + at, data, 16,
                             enables, 16) == tlm::TLM_OK_RESPONSE,
                  "an all-disabled write must succeed");
            for (unsigned index = 0; index < 16; ++index) {
                check(near_memory->storage()[at + index] == 0x99,
                      "an all-disabled write must change nothing");
            }
        }
    }

    // ---- R2-F3: widened reads never reach an MMIO target -------------------
    void test_widened_read_policy()
    {
        unsigned char buffer[32] = {};
        spy->accesses.clear();

        // Aligned natural widths are never widened, so they must all be
        // allowed and must arrive at exactly the address and length asked for.
        for (unsigned width : {1u, 2u, 4u, 8u}) {
            spy->accesses.clear();
            const std::uint64_t at = spy_base + width * 8;
            check(raw_access(tlm::TLM_READ_COMMAND, at, buffer, width)
                      == tlm::TLM_OK_RESPONSE,
                  "an aligned natural-width read of an MMIO target must work");
            check(spy->accesses.size() == 1,
                  "it must produce exactly one downstream access");
            check(spy->accesses[0].address == at - spy_base
                      && spy->accesses[0].length == width,
                  "and that access must be the one the caller asked for, "
                  "neither widened nor moved");
        }

        // An aligned multi-beat read is a whole number of beats, so it is not
        // widened either.
        spy->accesses.clear();
        check(raw_access(tlm::TLM_READ_COMMAND, spy_base + 0x40, buffer, 16)
                  == tlm::TLM_OK_RESPONSE,
              "an aligned two-beat read of an MMIO target must work");
        check(spy->accesses.size() == 1 && spy->accesses[0].length == 16
                  && spy->accesses[0].address == 0x40,
              "an aligned multi-beat read must not be widened");

        // These cannot be represented without fetching bytes outside the
        // request, so they must be refused *and the target must never see
        // them*. Checking only the response would not prove the second half.
        const struct { std::uint64_t offset; unsigned length; } widened[] = {
            {0x85, 6},   // unaligned start and odd length
            {0x82, 4},   // unaligned start
            {0x80, 6},   // aligned start, partial beat
            {0x80, 12},  // aligned start, one and a half beats
        };
        for (const auto& item : widened) {
            spy->accesses.clear();
            check(raw_access(tlm::TLM_READ_COMMAND, spy_base + item.offset,
                             buffer, item.length)
                      == tlm::TLM_BURST_ERROR_RESPONSE,
                  "a widened read of an MMIO target must be refused");
            check(spy->accesses.empty(),
                  "a refused widened read must never reach the target");
        }

        // The same shapes against a memory-like target are allowed, because a
        // platform has declared that widening is safe there.
        std::memset(buffer, 0, sizeof(buffer));
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + 0x405, buffer, 6)
                  == tlm::TLM_OK_RESPONSE,
              "the same shape must be allowed on a memory-like target");

        // Writes are not affected: they carry byte enables, so they touch only
        // the addresses they name, on any kind of target.
        spy->accesses.clear();
        unsigned char one = 0x5A;
        check(raw_access(tlm::TLM_WRITE_COMMAND, spy_base + 0x91, &one, 1)
                  == tlm::TLM_OK_RESPONSE,
              "an unaligned single-byte write to MMIO must still work");
    }

    // ---- R3-F1: the wrapper refuses a payload needing more than 256 beats --
    void test_burst_length_limit()
    {
        std::vector<unsigned char> buffer(2100, 0);
        const unsigned bus = 8;
        const unsigned max_beats = 256;

        // Aligned: 2048 bytes is exactly 256 beats, 2049 needs 257.
        spy->accesses.clear();
        check(raw_access(tlm::TLM_READ_COMMAND, near_base, buffer.data(),
                         max_beats * bus) == tlm::TLM_OK_RESPONSE,
              "an aligned 256-beat read must be accepted");
        check(raw_access(tlm::TLM_READ_COMMAND, near_base, buffer.data(),
                         max_beats * bus + 1) == tlm::TLM_BURST_ERROR_RESPONSE,
              "an aligned 257-beat read must be refused");
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base, buffer.data(),
                         max_beats * bus) == tlm::TLM_OK_RESPONSE,
              "an aligned 256-beat write must be accepted");
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base, buffer.data(),
                         max_beats * bus + 1) == tlm::TLM_BURST_ERROR_RESPONSE,
              "an aligned 257-beat write must be refused");

        // At a non-zero lane offset the boundary moves, but not by a whole
        // beat. The beat count is `ceil((lane_offset + length) / bus)`, so at
        // `+1`:
        //
        //     ceil((1 + 2047) / 8) = 256 beats   accepted
        //     ceil((1 + 2048) / 8) = 257 beats   refused
        //
        // 2047 is the last accepted length, not 2040. An earlier version of
        // this test asserted 2040 and 2048 and called the pair a boundary — but
        // 2040 is seven bytes short of the edge, so an off-by-one in the beat
        // count would have gone unnoticed on the accepting side. Both sides are
        // now pinned to adjacent lengths.
        const unsigned last_ok_at_plus_one = max_beats * bus - 1;
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + 1, buffer.data(),
                         last_ok_at_plus_one) == tlm::TLM_OK_RESPONSE,
              "at +1, the longest 256-beat transfer must be accepted");
        check(raw_access(tlm::TLM_WRITE_COMMAND, near_base + 1, buffer.data(),
                         last_ok_at_plus_one + 1) == tlm::TLM_BURST_ERROR_RESPONSE,
              "at +1, one byte more needs 257 beats and must be refused");

        // The same pair on the read path, and at the widest lane offset, where
        // seven bytes of the first beat are already spent.
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + 7, buffer.data(),
                         max_beats * bus - 7) == tlm::TLM_OK_RESPONSE,
              "at +7, the longest 256-beat read must be accepted");
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + 7, buffer.data(),
                         max_beats * bus - 6) == tlm::TLM_BURST_ERROR_RESPONSE,
              "at +7, one byte more needs 257 beats and must be refused");

        // And the refusal must happen before anything downstream is called.
        spy->accesses.clear();
        check(raw_access(tlm::TLM_WRITE_COMMAND, spy_base, buffer.data(),
                         max_beats * bus + 1) == tlm::TLM_BURST_ERROR_RESPONSE,
              "an over-long write to MMIO must be refused too");
        check(spy->accesses.empty(),
              "a refused over-long transfer must never reach the target");
    }

    // ---- R3-F2: transactions near the address-space end --------------------
    void test_address_space_end()
    {
        unsigned char buffer[16] = {};

        // Two bytes starting at UINT64_MAX: the last byte does not exist. The
        // check must be a subtraction, not `address + length - 1`, which wraps
        // to 0 and would decode as some low-address region.
        check(raw_access(tlm::TLM_READ_COMMAND, UINT64_MAX, buffer, 2)
                  == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "a payload running off the end of the address space must be "
              "refused");

        // An access reaching past the region end in the *requested* bytes is
        // rejected by the whole-range decode, before the beat frame is even
        // computed. This is not the frame guard; `test_beat_frame_guard`
        // is.
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + region_size - 4,
                         buffer, 8) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "a request running past the region end must be refused");

        // A region ending exactly at UINT64_MAX must be reachable, including
        // its final byte. This is the case an addition-based decode loses.
        std::uint64_t marker = 0x1122'3344'5566'7788ull;
        std::memcpy(buffer, &marker, sizeof(marker));
        check(raw_access(tlm::TLM_WRITE_COMMAND, top_base, buffer, 8)
                  == tlm::TLM_OK_RESPONSE,
              "a region ending at UINT64_MAX must be writable");
        std::memset(buffer, 0, sizeof(buffer));
        check(raw_access(tlm::TLM_READ_COMMAND, top_base, buffer, 8)
                  == tlm::TLM_OK_RESPONSE,
              "a region ending at UINT64_MAX must be readable");
        std::uint64_t seen = 0;
        std::memcpy(&seen, buffer, sizeof(seen));
        check(seen == marker,
              "and must return what was written to it");

        // The byte at UINT64_MAX itself, written *and* read back. Accepting
        // the write proves only that the decode matched; it takes the read to
        // prove the byte that came back is the one that went in, at the very
        // address where every wrapping arithmetic error lands.
        unsigned char last = 0xA5;
        check(raw_access(tlm::TLM_WRITE_COMMAND, UINT64_MAX, &last, 1)
                  == tlm::TLM_OK_RESPONSE,
              "the byte at UINT64_MAX itself must be addressable");
        last = 0;
        check(raw_access(tlm::TLM_READ_COMMAND, UINT64_MAX, &last, 1)
                  == tlm::TLM_OK_RESPONSE,
              "the byte at UINT64_MAX itself must be readable");
        check(last == 0xA5,
              "and must read back what was written to it");

        // The last legal byte of a region is still reachable.
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + region_size - 1,
                         buffer, 1) == tlm::TLM_OK_RESPONSE,
              "the final byte of a region must be readable");
        check(raw_access(tlm::TLM_READ_COMMAND, near_base + region_size,
                         buffer, 1) == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "one byte past a region must not be");
    }

    // ---- The beat-frame guard, on a region the guard is the only thing to --
    // ---- reject ------------------------------------------------------------
    //
    // A full-width transfer is replayed over its whole beat frame, which starts
    // at the bus-aligned address below the request and can end above it. The
    // frame therefore reaches addresses the caller never named, and those must
    // still belong to the same target.
    //
    // Reaching this guard needs a region whose end is *not* bus-aligned. With
    // an aligned end, any frame that overruns the region also has requested
    // bytes outside it, so the whole-range decode rejects the access several
    // checks earlier and the frame guard is dead code as far as the test is
    // concerned. That is what an earlier version of this file did, and deleting
    // the guard entirely left it green.
    //
    // The target is backed by more memory than the region it is mapped with, so
    // an unguarded overrun does not fail on its own: it lands inside the
    // backing store and reports success. Both halves are asserted — the
    // response code and the fact that the target was never called at all.
    //
    // What the two commands actually lose without the guard is **not** the
    // same, and an earlier version of this comment got the write wrong.
    //
    //  * **Read** — the replay takes the whole beat frame, because that is what
    //    AXI fetches: `length = beats * 2**ARSIZE` from the bus-aligned base of
    //    beat 0. So an unguarded read really does pull bytes from outside the
    //    declared region — four of them here — out of a neighbouring mapping or,
    //    as arranged below, out of backing store the platform never mapped.
    //  * **Write** — the replay is strobe-exact: `absorb_request` reduces the
    //    burst to the span of the addresses its `WSTRB` bits actually named, so
    //    the downstream access stays inside the requested bytes and therefore
    //    inside the region. Nothing is corrupted today.
    //
    // The write is still refused, and deliberately. The frame is a property of
    // the AXI burst, not of this wrapper's replay: `AWADDR` remains the
    // possibly unaligned transaction start, and `AWSIZE` and `AWLEN` define a
    // beat sequence whose final beat crosses the region boundary. So the burst
    // the wrapper accepted describes a transfer leaving the region. Today a
    // strobe-exact TLM replay hides that; from Step A-3 the timed chimney puts
    // that burst on the wire, where a real subordinate decoder sees the frame
    // and not the strobes. Refusing it now keeps the two paths agreeing.
    void test_beat_frame_guard()
    {
        unsigned char buffer[16] = {};
        // Six bytes ending exactly on the region's last byte. `+0x0FFE` sits at
        // lane 6, so this is a full-width two-beat transfer whose frame runs
        // from `+0x0FF8` to `+0x1007` — four bytes past a region that ends at
        // `+0x1003`.
        const std::uint64_t straddling = frame_base + frame_size - 6;

        frame_spy->accesses.clear();
        check(raw_access(tlm::TLM_WRITE_COMMAND, straddling, buffer, 6)
                  == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "a write whose beat frame leaves the region must be refused");
        check(frame_spy->accesses.empty(),
              "and must never reach the target");

        // The read path has its own earlier gate — the widened-read policy —
        // but this target is `memory`, so that gate lets the access through and
        // the frame guard is what has to stop it.
        frame_spy->accesses.clear();
        check(raw_access(tlm::TLM_READ_COMMAND, straddling, buffer, 6)
                  == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "a read whose beat frame leaves the region must be refused");
        check(frame_spy->accesses.empty(),
              "a refused frame must not read the target either");

        // The identical shape well inside the region is accepted, so the guard
        // is rejecting the position and not the shape.
        frame_spy->accesses.clear();
        check(raw_access(tlm::TLM_WRITE_COMMAND, frame_base + 0x106, buffer, 6)
                  == tlm::TLM_OK_RESPONSE,
              "the same shape away from the region end must be accepted");
        check(!frame_spy->accesses.empty(),
              "and must actually reach the target");

        // The last byte of the unaligned region is still reachable on its own:
        // a single narrow beat has a frame one byte wide and cannot overrun.
        check(raw_access(tlm::TLM_WRITE_COMMAND, frame_base + frame_size - 1,
                         buffer, 1) == tlm::TLM_OK_RESPONSE,
              "a single-byte access to the region's last byte must work");
    }

    void run()
    {
        test_reset_and_wakeup();
        test_address_space_end();
        test_beat_frame_guard();
        test_burst_length_limit();
        test_widened_read_policy();
        test_sparse_multibeat_writes();
        test_delay_contract();
        test_payload_contract();
        test_lane_placement();
        test_response_classification();

        // ---- A single-beat write, read back through the network -----------
        std::uint64_t written = 0xDEAD'BEEF'CAFE'F00Dull;
        unsigned char buffer[64] = {};
        std::memcpy(buffer, &written, sizeof(written));
        const auto write_time =
            access(true, near_base + 0x40, buffer, sizeof(written));
        check(write_time > sc_core::SC_ZERO_TIME,
              "a cycle-accurate interconnect must consume simulated time");

        std::memset(buffer, 0, sizeof(buffer));
        access(false, near_base + 0x40, buffer, sizeof(written));
        std::uint64_t read_back = 0;
        std::memcpy(&read_back, buffer, sizeof(read_back));
        check(read_back == written,
              "the value read back must be the value written");

        // The write really reached the peripheral, not just the model.
        std::uint64_t in_memory = 0;
        std::memcpy(&in_memory, near_memory->storage().data() + 0x40,
                    sizeof(in_memory));
        check(in_memory == written,
              "the write must land in the target's own storage");

        // ---- A burst: four beats in one packet ----------------------------
        std::uint64_t burst[4] = {0x1111'1111, 0x2222'2222, 0x3333'3333,
                                  0x4444'4444};
        std::memcpy(buffer, burst, sizeof(burst));
        access(true, near_base + 0x80, buffer, sizeof(burst));

        std::memset(buffer, 0, sizeof(buffer));
        access(false, near_base + 0x80, buffer, sizeof(burst));
        std::uint64_t burst_back[4] = {};
        std::memcpy(burst_back, buffer, sizeof(burst_back));
        for (unsigned beat = 0; beat < 4; ++beat) {
            check(burst_back[beat] == burst[beat],
                  "every beat of the burst must survive the round trip");
        }

        // ---- Distance costs cycles ----------------------------------------
        //
        // This is a no-contention calibration point, not a congestion sample.
        // The second manager has already exercised concurrent traffic above;
        // drain it before pinning exact hop-count latency.
        const auto calibration_deadline =
            sc_core::sc_time_stamp() + sc_core::sc_time(50, sc_core::SC_US);
        while (!other->finished
               && sc_core::sc_time_stamp() < calibration_deadline) {
            wait(sc_core::sc_time(20, sc_core::SC_NS));
        }
        check(other->finished,
              "the second manager must drain before latency calibration");

        const auto near_time =
            access(false, near_base, buffer, sizeof(std::uint64_t));
        const auto near_network_cycles = noc->last_latency_cycles();
        const auto far_time =
            access(false, far_base, buffer, sizeof(std::uint64_t));
        const auto far_network_cycles = noc->last_latency_cycles();
        check(far_time > near_time,
              "a farther target must cost more than a nearer one");
        check(far_network_cycles > near_network_cycles,
              "the measured network cycles must grow with hop count");
        // A-3 calibration baseline through the complete signal-driven path.
        // The far target annotates 5 ns, which is deliberately absent from
        // `last_latency_cycles()`: these numbers are the NoC only.
        check(near_network_cycles == 10,
              "the one-hop signal-driven baseline must stay at 10 cycles");
        check(far_network_cycles == 30,
              "the six-hop signal-driven baseline must stay at 30 cycles");
        std::cout << "near " << near_time << " (" << near_network_cycles
                  << " network cycles), far " << far_time << " ("
                  << far_network_cycles << " network cycles)\n";

        // ---- Debug access bypasses the network ----------------------------
        const auto before = sc_core::sc_time_stamp();
        tlm::tlm_generic_payload dbg;
        std::uint64_t probe = 0;
        dbg.set_command(tlm::TLM_READ_COMMAND);
        dbg.set_address(near_base + 0x40);
        dbg.set_data_ptr(reinterpret_cast<unsigned char*>(&probe));
        dbg.set_data_length(sizeof(probe));
        const auto served = socket->transport_dbg(dbg);
        check(served == sizeof(probe), "debug access must serve every byte");
        check(probe == written, "debug access must see the stored value");
        check(sc_core::sc_time_stamp() == before,
              "debug access must not consume simulated time");

        // ---- An unmapped address is an address error ----------------------
        tlm::tlm_generic_payload bad;
        bad.set_command(tlm::TLM_READ_COMMAND);
        bad.set_address(0xDEAD'0000);
        bad.set_data_ptr(buffer);
        bad.set_data_length(sizeof(std::uint64_t));
        bad.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        socket->b_transport(bad, delay);
        check(bad.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE,
              "an unmapped address must be reported, not routed");

        // ---- Two managers on the mesh --------------------------------------
        //
        // The second manager has been writing to the far memory concurrently.
        // Wait for it, then check every one of its writes landed: two managers
        // sharing links must not corrupt or lose each other's traffic.
        // Bounded. An unbounded `while (!other->finished) wait(...)` turns any
        // lost transaction into a hung suite with no message, which is exactly
        // the failure mode a NoC test must not have.
        const auto other_deadline =
            sc_core::sc_time_stamp() + sc_core::sc_time(50, sc_core::SC_US);
        while (!other->finished && sc_core::sc_time_stamp() < other_deadline) {
            wait(sc_core::sc_time(20, sc_core::SC_NS));
        }
        check(other->finished,
              "the second manager must finish within its deadline");
        check(other->completed == 8, "every second-manager write must complete");
        for (unsigned index = 0; index < 8; ++index) {
            std::uint64_t stored = 0;
            std::memcpy(&stored,
                        far_memory->storage().data() + 0x200 + index * 8,
                        sizeof(stored));
            check(stored == 0xA000 + index,
                  "each second-manager write must land at its own address");
        }

        // ---- scoreboard --------------------------------------------------
        //
        // Every byte either manager wrote must be readable back, from the
        // other manager's traffic as well as this one's. Two managers sharing
        // links must not corrupt, drop, or cross-deliver each other's data.
        for (unsigned index = 0; index < 8; ++index) {
            unsigned char probe[8] = {};
            const auto status = raw_access(
                tlm::TLM_READ_COMMAND, far_base + 0x200 + index * 8, probe, 8);
            check(status == tlm::TLM_OK_RESPONSE,
                  "the scoreboard read must complete");
            std::uint64_t seen = 0;
            std::memcpy(&seen, probe, sizeof(seen));
            check(seen == 0xA000 + index,
                  "a read through the network must return what the other "
                  "manager wrote at that address");
        }

        // ---- last_latency_cycles() excludes only the requester's hold-off --
        //
        // The far memory annotates 5 ns of its own latency. That is the
        // target's time, not the network's, so the reported network latency for
        // an access to it must not be inflated by it: it should stay close to
        // the same-distance access to a target with no latency.
        {
            unsigned char probe[8] = {};
            raw_access(tlm::TLM_READ_COMMAND, zero_base, probe, 8);
            const auto without = noc->last_latency_cycles();
            raw_access(tlm::TLM_READ_COMMAND, far_base, probe, 8);
            const auto with_delay = noc->last_latency_cycles();
            check(with_delay <= without + 1,
                  "a target's own latency must not be counted as network "
                  "latency for the transaction that caused it");
        }

        check(noc->completed_transactions() >= 6,
              "the wrapper must count the transactions it completed");
        check(noc->total_latency_cycles() > 0,
              "the wrapper must accumulate measured network cycles");

        if (dog != nullptr) {
            dog->driver_finished = true;
        }
        sc_core::sc_stop();
    }
};

} // namespace

/// R3-V2: the production AXI-to-TLM mapping, every code, exactly.
///
/// This calls the same function `b_transport` calls, so the two cannot drift.
/// Driving a real failure end to end reaches only two of the four codes.
void check_response_mapping()
{
    using floo::model::axi_pkg::axi_resp;
    using floo::model::axi_pkg::to_bits;
    using cdc::components::noc_interconnect;

    check(noc_interconnect::tlm_status_for(to_bits(axi_resp::okay))
              == tlm::TLM_OK_RESPONSE,
          "OKAY must map to TLM_OK_RESPONSE");
    check(noc_interconnect::tlm_status_for(to_bits(axi_resp::exokay))
              == tlm::TLM_OK_RESPONSE,
          "EXOKAY must map to TLM_OK_RESPONSE: it is a success code");
    check(noc_interconnect::tlm_status_for(to_bits(axi_resp::slverr))
              == tlm::TLM_GENERIC_ERROR_RESPONSE,
          "SLVERR must map to TLM_GENERIC_ERROR_RESPONSE");
    check(noc_interconnect::tlm_status_for(to_bits(axi_resp::decerr))
              == tlm::TLM_ADDRESS_ERROR_RESPONSE,
          "DECERR must map to TLM_ADDRESS_ERROR_RESPONSE");
    check(noc_interconnect::tlm_status_for(to_bits(axi_resp::slverr))
              != noc_interconnect::tlm_status_for(to_bits(axi_resp::decerr)),
          "SLVERR and DECERR must not collapse to the same TLM status");
}

int sc_main(int, char**)
{
    check_response_mapping();

    // A 4x4 mesh: initiator at (0,0), a near memory at (1,0) and a far one at
    // (3,3), so the A-3 integrated TLM path is measured at one and six hops.
    // Two upstream ports: the driver at (0,0) and a second manager at (0,1),
    // matching how the platform places its CPU and DMA on separate nodes.
    cdc::components::noc_interconnect noc{"noc", 4, 4, 10, 2};
    memory_target near_memory{"near_memory", region_size};
    memory_target far_memory{"far_memory", region_size,
                             sc_core::sc_time(5, sc_core::SC_NS)};
    memory_target small_memory{"small_memory", oversized_storage};
    memory_target zero_memory{"zero_memory", region_size};
    memory_target top_memory{"top_memory", region_size};
    memory_target sub_memory{"sub_memory", region_size,
                             sc_core::sc_time(0.4, sc_core::SC_NS)};
    memory_target frac_memory{"frac_memory", region_size,
                              sc_core::sc_time(1.5, sc_core::SC_NS)};
    memory_target exact_memory{"exact_memory", region_size,
                               sc_core::sc_time(2.0, sc_core::SC_NS)};
    spy_target spy{"spy"};
    spy_target frame_spy{"frame_spy", frame_storage};
    driver cpu{"cpu"};
    second_manager dma{"dma"};
    watchdog dog{"watchdog", sc_core::sc_time(200, sc_core::SC_US)};

    noc.place_initiator(0, {0, 0});
    noc.place_initiator(1, {0, 1});
    cpu.socket.bind(noc.target_socket);
    dma.socket.bind(noc.cpu_port(1));
    noc.add_target(near_base, region_size, {1, 0},
                   cdc::components::noc_interconnect::target_kind::memory)
        .bind(near_memory.socket);
    noc.add_target(far_base, region_size, {3, 3}).bind(far_memory.socket);
    noc.add_target(oversized_base, region_size, {3, 3})
        .bind(small_memory.socket);
    noc.add_target(zero_base, region_size, {3, 3}).bind(zero_memory.socket);
    noc.add_target(top_base, region_size, {3, 3},
                   cdc::components::noc_interconnect::target_kind::memory)
        .bind(top_memory.socket);
    noc.add_target(sub_base, region_size, {3, 3}).bind(sub_memory.socket);
    noc.add_target(frac_base, region_size, {3, 3}).bind(frac_memory.socket);
    noc.add_target(exact_base, region_size, {3, 3}).bind(exact_memory.socket);
    // Default kind: `mmio`. That is the point of the widened-read tests.
    noc.add_target(spy_base, region_size, {3, 3}).bind(spy.socket);
    // `memory`, so the widened-read policy lets a read through and the beat
    // frame guard is the check under test. Mapped `frame_size`, backed by
    // `frame_storage` — the overrun must be refused by the wrapper, not by the
    // target running out of memory.
    noc.add_target(frame_base, frame_size, {3, 3},
                   cdc::components::noc_interconnect::target_kind::memory)
        .bind(frame_spy.socket);

    cpu.noc = &noc;
    cpu.near_memory = &near_memory;
    cpu.far_memory = &far_memory;
    cpu.other = &dma;
    cpu.dog = &dog;
    cpu.spy = &spy;
    cpu.frame_spy = &frame_spy;

    sc_core::sc_start();

    if (failures == 0) {
        std::cout << "PASS: NoC TLM interconnect\n";
        return 0;
    }
    std::cerr << "FAIL: " << failures << " interconnect checks failed\n";
    return 1;
}
