#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "memory/isp_frame_memory.h"

namespace {

using cdc::components::isp::I420FrameDescriptor;
using cdc::components::isp::IspFrameMemory;
using cdc::components::isp::MemoryError;
using cdc::components::isp::MemoryPlane;
using cdc::components::isp::Raw12RggbLe16Descriptor;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

class PhysicalMemoryStub : public sc_core::sc_module {
public:
    struct Access {
        tlm::tlm_command command = tlm::TLM_IGNORE_COMMAND;
        std::uint64_t address = 0;
        unsigned int length = 0;
    };

    tlm_utils::simple_target_socket<PhysicalMemoryStub> socket;

    PhysicalMemoryStub(sc_core::sc_module_name name,
                       std::uint64_t base,
                       std::size_t size)
        : sc_core::sc_module(name)
        , socket("socket")
        , base_(base)
        , storage_(size, 0)
    {
        socket.register_b_transport(this, &PhysicalMemoryStub::b_transport);
    }

    void clear_accesses() { accesses_.clear(); }
    const std::vector<Access>& accesses() const { return accesses_; }

    void fail_at(std::uint64_t address) { failing_address_ = address; }
    void clear_failure()
    {
        failing_address_ = std::numeric_limits<std::uint64_t>::max();
    }

    void fill(std::uint64_t address, std::size_t length, std::uint8_t value)
    {
        const std::size_t offset = checked_offset(address, length);
        std::fill_n(storage_.begin() + static_cast<std::ptrdiff_t>(offset),
                    length,
                    value);
    }

    void poke_le16(std::uint64_t address, std::uint16_t value)
    {
        const std::size_t offset = checked_offset(address, 2);
        storage_[offset] = static_cast<std::uint8_t>(value & 0xffu);
        storage_[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
    }

    std::uint8_t byte_at(std::uint64_t address) const
    {
        return storage_.at(checked_offset(address, 1));
    }

private:
    std::uint64_t base_;
    std::vector<std::uint8_t> storage_;
    std::vector<Access> accesses_;
    std::uint64_t failing_address_ =
        std::numeric_limits<std::uint64_t>::max();

    std::size_t checked_offset(std::uint64_t address, std::size_t length) const
    {
        if (address < base_) {
            throw std::out_of_range("address below physical-memory window");
        }
        const std::uint64_t offset = address - base_;
        if (offset > storage_.size() || length > storage_.size() - offset) {
            throw std::out_of_range("address above physical-memory window");
        }
        return static_cast<std::size_t>(offset);
    }

    bool contains(std::uint64_t address, unsigned int length) const
    {
        if (address < base_) {
            return false;
        }
        const std::uint64_t offset = address - base_;
        return offset <= storage_.size() &&
               length <= storage_.size() - offset;
    }

    void b_transport(tlm::tlm_generic_payload& transaction,
                     sc_core::sc_time& delay)
    {
        accesses_.push_back(Access {transaction.get_command(),
                                    transaction.get_address(),
                                    transaction.get_data_length()});
        delay += sc_core::sc_time(2, sc_core::SC_NS);

        if (transaction.get_command() != tlm::TLM_READ_COMMAND &&
            transaction.get_command() != tlm::TLM_WRITE_COMMAND) {
            transaction.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }
        if (transaction.get_data_ptr() == nullptr ||
            transaction.get_data_length() == 0) {
            transaction.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        if (transaction.get_byte_enable_ptr() != nullptr) {
            transaction.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return;
        }
        if (transaction.get_streaming_width() !=
            transaction.get_data_length()) {
            transaction.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }
        if (!contains(transaction.get_address(),
                      transaction.get_data_length())) {
            transaction.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (transaction.get_address() == failing_address_) {
            transaction.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        const std::size_t offset = checked_offset(transaction.get_address(),
                                                  transaction.get_data_length());
        if (transaction.is_read()) {
            std::memcpy(transaction.get_data_ptr(),
                        storage_.data() + offset,
                        transaction.get_data_length());
        } else {
            std::memcpy(storage_.data() + offset,
                        transaction.get_data_ptr(),
                        transaction.get_data_length());
        }
        transaction.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

class MemoryHelperTester : public sc_core::sc_module {
public:
    static constexpr std::uint64_t kMemoryBase = 0x81000000ull;

    IspFrameMemory frame_memory;
    PhysicalMemoryStub memory;

    SC_HAS_PROCESS(MemoryHelperTester);
    explicit MemoryHelperTester(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , frame_memory("frame_memory")
        , memory("memory", kMemoryBase, 0x20000)
    {
        frame_memory.memory_socket.bind(memory.socket);
        SC_THREAD(run);
    }

private:
    Raw12RggbLe16Descriptor raw_descriptor() const
    {
        Raw12RggbLe16Descriptor descriptor;
        descriptor.address = kMemoryBase + 0x100;
        descriptor.stride_bytes = 8;
        descriptor.size_bytes = 14;
        descriptor.width = 3;
        descriptor.height = 2;
        return descriptor;
    }

    I420FrameDescriptor i420_descriptor() const
    {
        I420FrameDescriptor descriptor;
        descriptor.width = 3;
        descriptor.height = 3;
        descriptor.y.address = kMemoryBase + 0x1000;
        descriptor.y.stride_bytes = 5;
        descriptor.y.size_bytes = 13;
        descriptor.cb.address = kMemoryBase + 0x1100;
        descriptor.cb.stride_bytes = 3;
        descriptor.cb.size_bytes = 5;
        descriptor.cr.address = kMemoryBase + 0x1200;
        descriptor.cr.stride_bytes = 3;
        descriptor.cr.size_bytes = 5;
        return descriptor;
    }

    void reset_observation()
    {
        frame_memory.reset_counters();
        memory.clear_accesses();
        memory.clear_failure();
    }

    void expect_no_access(const std::string& label)
    {
        expect(memory.accesses().empty(), label + ": no TLM access");
        expect(frame_memory.counters().read_transactions == 0 &&
                   frame_memory.counters().write_transactions == 0,
               label + ": no transaction counted");
        expect(frame_memory.counters().errors == 1,
               label + ": one validation error counted");
    }

    void seed_raw(const Raw12RggbLe16Descriptor& descriptor)
    {
        memory.fill(descriptor.address,
                    static_cast<std::size_t>(descriptor.size_bytes),
                    0xee);
        memory.poke_le16(descriptor.address, 0xf123);
        memory.poke_le16(descriptor.address + 2, 0x0abc);
        memory.poke_le16(descriptor.address + 4, 0xafff);
        memory.poke_le16(descriptor.address + descriptor.stride_bytes, 0x0000);
        memory.poke_le16(descriptor.address + descriptor.stride_bytes + 2, 0x1001);
        memory.poke_le16(descriptor.address + descriptor.stride_bytes + 4, 0xb456);
    }

    void test_raw_success()
    {
        reset_observation();
        const Raw12RggbLe16Descriptor descriptor = raw_descriptor();
        seed_raw(descriptor);

        std::vector<std::uint16_t> pixels {0xdead};
        const sc_core::sc_time start = sc_core::sc_time_stamp();
        const auto result = frame_memory.read_raw12_frame(descriptor, pixels);

        expect(result.ok(), "RAW12 read succeeds");
        expect(pixels == std::vector<std::uint16_t>({0x123, 0xabc, 0xfff,
                                                     0x000, 0x001, 0x456}),
               "RAW12_LE16 is decoded and upper nibble is ignored");
        expect(sc_core::sc_time_stamp() - start ==
                   sc_core::sc_time(4, sc_core::SC_NS),
               "RAW read consumes annotated target delay");

        const auto& counters = frame_memory.counters();
        expect(counters.read_bytes == 12 && counters.read_transactions == 2 &&
                   counters.write_bytes == 0 && counters.errors == 0,
               "RAW functional byte/transaction counters");

        const auto& accesses = memory.accesses();
        expect(accesses.size() == 2,
               "RAW read issues one transaction per active row");
        if (accesses.size() == 2) {
            expect(accesses[0].command == tlm::TLM_READ_COMMAND &&
                       accesses[0].address == descriptor.address &&
                       accesses[0].length == 6,
                   "RAW row zero uses programmed physical address");
            expect(accesses[1].command == tlm::TLM_READ_COMMAND &&
                       accesses[1].address ==
                           descriptor.address + descriptor.stride_bytes &&
                       accesses[1].length == 6,
                   "RAW row one uses programmed physical stride");
        }
    }

    void test_raw_validation()
    {
        std::vector<std::uint16_t> pixels {0xbeef};

        reset_observation();
        auto descriptor = raw_descriptor();
        ++descriptor.address;
        auto result = frame_memory.read_raw12_frame(descriptor, pixels);
        expect(result.error == MemoryError::address_misaligned &&
                   result.plane == MemoryPlane::raw,
               "RAW misaligned address is rejected");
        expect(pixels == std::vector<std::uint16_t>({0xbeef}),
               "RAW validation failure preserves caller output");
        expect_no_access("RAW address alignment");

        reset_observation();
        descriptor = raw_descriptor();
        descriptor.stride_bytes = 7;
        result = frame_memory.read_raw12_frame(descriptor, pixels);
        expect(result.error == MemoryError::stride_misaligned,
               "RAW odd stride is rejected");
        expect_no_access("RAW stride alignment");

        reset_observation();
        descriptor = raw_descriptor();
        descriptor.stride_bytes = 4;
        result = frame_memory.read_raw12_frame(descriptor, pixels);
        expect(result.error == MemoryError::stride_too_small,
               "RAW stride smaller than active row is rejected");
        expect_no_access("RAW stride bound");

        reset_observation();
        descriptor = raw_descriptor();
        descriptor.size_bytes = 13;
        result = frame_memory.read_raw12_frame(descriptor, pixels);
        expect(result.error == MemoryError::size_too_small,
               "RAW allocation smaller than used span is rejected");
        expect_no_access("RAW size bound");

        reset_observation();
        descriptor = raw_descriptor();
        descriptor.address = std::numeric_limits<std::uint64_t>::max() - 3u;
        result = frame_memory.read_raw12_frame(descriptor, pixels);
        expect(result.error == MemoryError::address_overflow,
               "RAW physical range overflow is rejected");
        expect_no_access("RAW address overflow");
    }

    void test_raw_transport_error()
    {
        reset_observation();
        const Raw12RggbLe16Descriptor descriptor = raw_descriptor();
        seed_raw(descriptor);
        memory.fail_at(descriptor.address + descriptor.stride_bytes);

        std::vector<std::uint16_t> pixels {0xcafe};
        const auto result = frame_memory.read_raw12_frame(descriptor, pixels);
        expect(result.error == MemoryError::transport_error &&
                   result.response == tlm::TLM_GENERIC_ERROR_RESPONSE &&
                   result.plane == MemoryPlane::raw && result.row == 1 &&
                   result.address == descriptor.address + descriptor.stride_bytes,
               "RAW target response error identifies failed row/address");
        expect(pixels == std::vector<std::uint16_t>({0xcafe}),
               "failed RAW frame does not expose partial decoded output");
        expect(frame_memory.counters().read_transactions == 2 &&
                   frame_memory.counters().read_bytes == 6 &&
                   frame_memory.counters().errors == 1,
               "RAW error counters include issued and successful work");
    }

    void expect_plane_bytes(std::uint64_t address,
                            std::uint64_t stride,
                            std::uint32_t row_bytes,
                            std::uint32_t rows,
                            const std::vector<std::uint8_t>& expected,
                            const std::string& label)
    {
        std::size_t index = 0;
        bool matches = true;
        for (std::uint32_t row = 0; row < rows; ++row) {
            for (std::uint32_t column = 0; column < row_bytes; ++column) {
                matches = matches &&
                          memory.byte_at(address + row * stride + column) ==
                              expected[index++];
            }
        }
        expect(matches, label);
    }

    void test_i420_success()
    {
        reset_observation();
        const I420FrameDescriptor descriptor = i420_descriptor();
        memory.fill(descriptor.y.address, descriptor.y.size_bytes, 0xee);
        memory.fill(descriptor.cb.address, descriptor.cb.size_bytes, 0xee);
        memory.fill(descriptor.cr.address, descriptor.cr.size_bytes, 0xee);

        const std::vector<std::uint8_t> y {1, 2, 3, 4, 5, 6, 7, 8, 9};
        const std::vector<std::uint8_t> cb {21, 22, 23, 24};
        const std::vector<std::uint8_t> cr {31, 32, 33, 34};
        const sc_core::sc_time start = sc_core::sc_time_stamp();
        const auto result = frame_memory.write_i420_frame(descriptor, y, cb, cr);

        expect(result.ok(), "I420 write succeeds for odd dimensions");
        expect(sc_core::sc_time_stamp() - start ==
                   sc_core::sc_time(14, sc_core::SC_NS),
               "I420 write consumes all annotated row delays");
        expect_plane_bytes(descriptor.y.address, descriptor.y.stride_bytes,
                           3, 3, y, "I420 Y active bytes are written");
        expect_plane_bytes(descriptor.cb.address, descriptor.cb.stride_bytes,
                           2, 2, cb, "I420 Cb uses ceil(width/2) x ceil(height/2)");
        expect_plane_bytes(descriptor.cr.address, descriptor.cr.stride_bytes,
                           2, 2, cr, "I420 Cr uses ceil(width/2) x ceil(height/2)");
        expect(memory.byte_at(descriptor.y.address + 3) == 0xee &&
                   memory.byte_at(descriptor.y.address + 4) == 0xee &&
                   memory.byte_at(descriptor.y.address + 8) == 0xee &&
                   memory.byte_at(descriptor.y.address + 9) == 0xee &&
                   memory.byte_at(descriptor.cb.address + 2) == 0xee &&
                   memory.byte_at(descriptor.cr.address + 2) == 0xee,
               "I420 destination row padding is preserved");

        const auto& counters = frame_memory.counters();
        expect(counters.write_bytes == 17 && counters.write_transactions == 7 &&
                   counters.read_bytes == 0 && counters.errors == 0,
               "I420 functional byte/transaction counters");

        const std::vector<std::uint64_t> expected_addresses {
            descriptor.y.address,
            descriptor.y.address + 5,
            descriptor.y.address + 10,
            descriptor.cb.address,
            descriptor.cb.address + 3,
            descriptor.cr.address,
            descriptor.cr.address + 3,
        };
        bool exact_addresses = memory.accesses().size() == expected_addresses.size();
        for (std::size_t index = 0;
             exact_addresses && index < expected_addresses.size();
             ++index) {
            exact_addresses =
                memory.accesses()[index].command == tlm::TLM_WRITE_COMMAND &&
                memory.accesses()[index].address == expected_addresses[index];
        }
        expect(exact_addresses,
               "I420 writes use exact physical plane addresses and strides");
    }

    void test_i420_validation()
    {
        const std::vector<std::uint8_t> y {1, 2, 3, 4, 5, 6, 7, 8, 9};
        const std::vector<std::uint8_t> cb {21, 22, 23, 24};
        const std::vector<std::uint8_t> cr {31, 32, 33, 34};

        reset_observation();
        auto descriptor = i420_descriptor();
        descriptor.cb.address += 2;
        auto result = frame_memory.write_i420_frame(descriptor, y, cb, cr);
        expect(result.error == MemoryError::address_misaligned &&
                   result.plane == MemoryPlane::cb,
               "I420 misaligned plane address is rejected");
        expect_no_access("I420 address alignment");

        reset_observation();
        descriptor = i420_descriptor();
        descriptor.y.stride_bytes = 2;
        result = frame_memory.write_i420_frame(descriptor, y, cb, cr);
        expect(result.error == MemoryError::stride_too_small &&
                   result.plane == MemoryPlane::y,
               "I420 stride smaller than active row is rejected");
        expect_no_access("I420 stride bound");

        reset_observation();
        descriptor = i420_descriptor();
        descriptor.y.size_bytes = 12;
        result = frame_memory.write_i420_frame(descriptor, y, cb, cr);
        expect(result.error == MemoryError::size_too_small &&
                   result.plane == MemoryPlane::y,
               "I420 allocation smaller than used span is rejected");
        expect_no_access("I420 size bound");

        reset_observation();
        descriptor = i420_descriptor();
        descriptor.cb.address = descriptor.y.address + 4;
        result = frame_memory.write_i420_frame(descriptor, y, cb, cr);
        expect(result.error == MemoryError::plane_overlap,
               "overlapping I420 used plane ranges are rejected");
        expect_no_access("I420 plane overlap");

        reset_observation();
        descriptor = i420_descriptor();
        descriptor.cr.address =
            std::numeric_limits<std::uint64_t>::max() - 3u;
        result = frame_memory.write_i420_frame(descriptor, y, cb, cr);
        expect(result.error == MemoryError::address_overflow &&
                   result.plane == MemoryPlane::cr,
               "I420 physical plane range overflow is rejected");
        expect_no_access("I420 address overflow");

        reset_observation();
        descriptor = i420_descriptor();
        std::vector<std::uint8_t> short_y(y.begin(), y.end() - 1);
        result = frame_memory.write_i420_frame(descriptor, short_y, cb, cr);
        expect(result.error == MemoryError::buffer_size_mismatch &&
                   result.plane == MemoryPlane::y,
               "I420 tightly-packed source size mismatch is rejected");
        expect_no_access("I420 buffer size");
    }

    void test_i420_transport_error()
    {
        reset_observation();
        const I420FrameDescriptor descriptor = i420_descriptor();
        memory.fill(descriptor.y.address, descriptor.y.size_bytes, 0xee);
        memory.fill(descriptor.cb.address, descriptor.cb.size_bytes, 0xee);
        memory.fill(descriptor.cr.address, descriptor.cr.size_bytes, 0xee);
        memory.fail_at(descriptor.cb.address);

        const std::vector<std::uint8_t> y {1, 2, 3, 4, 5, 6, 7, 8, 9};
        const std::vector<std::uint8_t> cb {21, 22, 23, 24};
        const std::vector<std::uint8_t> cr {31, 32, 33, 34};
        const auto result = frame_memory.write_i420_frame(descriptor, y, cb, cr);

        expect(result.error == MemoryError::transport_error &&
                   result.response == tlm::TLM_GENERIC_ERROR_RESPONSE &&
                   result.plane == MemoryPlane::cb && result.row == 0 &&
                   result.address == descriptor.cb.address,
               "I420 target response error identifies failed plane/address");
        expect(frame_memory.counters().write_transactions == 4 &&
                   frame_memory.counters().write_bytes == 9 &&
                   frame_memory.counters().errors == 1,
               "I420 error counters expose partial completed work");
        expect(memory.byte_at(descriptor.cb.address) == 0xee &&
                   memory.byte_at(descriptor.cr.address) == 0xee,
               "I420 write stops after the first target error");
    }

    void run()
    {
        test_raw_success();
        test_raw_validation();
        test_raw_transport_error();
        test_i420_success();
        test_i420_validation();
        test_i420_transport_error();
        sc_core::sc_stop();
    }
};

} // namespace

int sc_main(int, char**)
{
    MemoryHelperTester tester("tester");
    sc_core::sc_start();
    if (failures == 0) {
        std::cout << "PASS: ISP RAW12/I420 physical-memory helper tests\n";
    }
    return failures == 0 ? 0 : 1;
}
