#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

#include "registers/isp_register_map.h"
#include "tlm/isp_control_target.h"

namespace {

namespace reg = isp_tlm::registers;
namespace control = isp_tlm::tlm_frontend;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

struct TransportResult {
    tlm::tlm_response_status response = tlm::TLM_INCOMPLETE_RESPONSE;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    bool dmi_allowed = true;
};

class ControlTargetTester : public sc_core::sc_module {
public:
    reg::IspRegisterBank bank;
    control::IspControlTarget target;
    tlm_utils::simple_initiator_socket<ControlTargetTester> initiator_socket;

    SC_HAS_PROCESS(ControlTargetTester);
    explicit ControlTargetTester(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , bank()
        , target("target", bank, sc_core::sc_time(5, sc_core::SC_NS))
        , initiator_socket("initiator_socket")
    {
        initiator_socket.bind(target.socket);
        SC_THREAD(run);
    }

private:
    static constexpr unsigned int kDefaultStreamingWidth =
        std::numeric_limits<unsigned int>::max();

    TransportResult transport(
        tlm::tlm_command command,
        std::uint64_t address,
        unsigned char* data,
        unsigned int length,
        unsigned int streaming_width = kDefaultStreamingWidth,
        unsigned char* byte_enables = nullptr,
        unsigned int byte_enable_length = 0u,
        sc_core::sc_time initial_delay = sc_core::SC_ZERO_TIME)
    {
        tlm::tlm_generic_payload transaction;
        transaction.set_command(command);
        transaction.set_address(address);
        transaction.set_data_ptr(data);
        transaction.set_data_length(length);
        transaction.set_streaming_width(
            streaming_width == kDefaultStreamingWidth ? length
                                                       : streaming_width);
        transaction.set_byte_enable_ptr(byte_enables);
        transaction.set_byte_enable_length(byte_enable_length);
        transaction.set_dmi_allowed(true);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        sc_core::sc_time delay = initial_delay;
        initiator_socket->b_transport(transaction, delay);
        return TransportResult {transaction.get_response_status(),
                                delay,
                                transaction.is_dmi_allowed()};
    }

    static std::array<unsigned char, 4> little_endian(std::uint32_t value)
    {
        return {
            static_cast<unsigned char>(value & 0xffu),
            static_cast<unsigned char>((value >> 8u) & 0xffu),
            static_cast<unsigned char>((value >> 16u) & 0xffu),
            static_cast<unsigned char>((value >> 24u) & 0xffu),
        };
    }

    std::uint32_t bank_read(std::uint32_t address, const std::string& label)
    {
        std::uint32_t value = 0xdeadbeefu;
        expect(bank.read(address, value) == reg::AccessResult::Ok,
               label + ": direct pending read succeeds");
        return value;
    }

    std::uint32_t bank_read_active(std::uint32_t address,
                                   const std::string& label)
    {
        std::uint32_t value = 0xdeadbeefu;
        expect(bank.read_active(address, value) == reg::AccessResult::Ok,
               label + ": direct active read succeeds");
        return value;
    }

    void expect_status(const TransportResult& result,
                       tlm::tlm_response_status expected,
                       const std::string& label)
    {
        expect(result.response == expected, label + ": response status");
        expect(!result.dmi_allowed, label + ": DMI is not advertised");
    }

    void test_little_endian_partial_access_and_commit()
    {
        bank.reset();
        target.set_latency(sc_core::sc_time(5, sc_core::SC_NS));

        auto bytes = little_endian(0x11223344u);
        auto result = transport(tlm::TLM_WRITE_COMMAND,
                                reg::kIspSourceAddress,
                                bytes.data(),
                                bytes.size(),
                                kDefaultStreamingWidth,
                                nullptr,
                                0u,
                                sc_core::sc_time(1, sc_core::SC_NS));
        expect_status(result, tlm::TLM_OK_RESPONSE, "full-word write");
        expect(result.delay == sc_core::sc_time(6, sc_core::SC_NS),
               "target adds configured latency to existing annotation");
        expect(bank_read(reg::kIspSourceAddress, "full-word write") ==
                   0x11223344u,
               "four-byte write is assembled little-endian");
        expect(bank_read_active(reg::kIspSourceAddress,
                                "pre-commit source address") == 0u,
               "TLM write changes pending state but not active state");

        unsigned char byte = 0xaau;
        result = transport(tlm::TLM_WRITE_COMMAND,
                           reg::kIspSourceAddress + 1u,
                           &byte,
                           1u);
        expect_status(result, tlm::TLM_OK_RESPONSE, "single-byte write");
        expect(bank_read(reg::kIspSourceAddress, "single-byte write") ==
                   0x1122aa44u,
               "byte address selects the matching little-endian lane");

        std::array<unsigned char, 2> halfword {0xbbu, 0xccu};
        std::array<unsigned char, 2> halfword_enable {
            TLM_BYTE_ENABLED,
            TLM_BYTE_DISABLED,
        };
        result = transport(tlm::TLM_WRITE_COMMAND,
                           reg::kIspSourceAddress + 2u,
                           halfword.data(),
                           halfword.size(),
                           8u,
                           halfword_enable.data(),
                           halfword_enable.size());
        expect_status(result, tlm::TLM_OK_RESPONSE,
                      "byte-enabled halfword write");
        expect(bank_read(reg::kIspSourceAddress,
                         "byte-enabled halfword write") == 0x11bbaa44u,
               "disabled transfer byte preserves its register lane");

        std::array<unsigned char, 4> update {0x10u, 0x20u, 0x30u, 0x40u};
        std::array<unsigned char, 2> alternating_enable {
            TLM_BYTE_DISABLED,
            TLM_BYTE_ENABLED,
        };
        result = transport(tlm::TLM_WRITE_COMMAND,
                           reg::kIspSourceAddress,
                           update.data(),
                           update.size(),
                           update.size(),
                           alternating_enable.data(),
                           alternating_enable.size());
        expect_status(result, tlm::TLM_OK_RESPONSE,
                      "repeating byte-enable write");
        expect(bank_read(reg::kIspSourceAddress,
                         "repeating byte-enable write") == 0x40bb2044u,
               "short byte-enable pattern repeats across the transfer");

        std::array<unsigned char, 2> halfword_read {0u, 0u};
        result = transport(tlm::TLM_READ_COMMAND,
                           reg::kIspSourceAddress + 2u,
                           halfword_read.data(),
                           halfword_read.size(),
                           4u);
        expect_status(result, tlm::TLM_OK_RESPONSE, "halfword read");
        expect(halfword_read == std::array<unsigned char, 2> {0xbbu, 0x40u},
               "halfword read is extracted little-endian");

        std::array<unsigned char, 4> selective_read {
            0xa5u, 0xa5u, 0xa5u, 0xa5u,
        };
        std::array<unsigned char, 2> read_enable {
            TLM_BYTE_ENABLED,
            TLM_BYTE_DISABLED,
        };
        result = transport(tlm::TLM_READ_COMMAND,
                           reg::kIspSourceAddress,
                           selective_read.data(),
                           selective_read.size(),
                           selective_read.size(),
                           read_enable.data(),
                           read_enable.size());
        expect_status(result, tlm::TLM_OK_RESPONSE,
                      "byte-enabled full-word read");
        expect(selective_read ==
                   std::array<unsigned char, 4> {0x44u, 0xa5u, 0xbbu, 0xa5u},
               "disabled read bytes remain untouched");

        bank.commit_frame();
        expect(bank_read_active(reg::kIspSourceAddress,
                                "committed source address") == 0x40bb2044u,
               "direct frame commit publishes the TLM pending value");

        expect(bank.write(reg::kIspSourceAddress, 0xa1b2c3d4u) ==
                   reg::AccessResult::Ok,
               "direct control frontend updates the shared pending bank");
        std::array<unsigned char, 4> shared_read {0u, 0u, 0u, 0u};
        result = transport(tlm::TLM_READ_COMMAND,
                           reg::kIspSourceAddress,
                           shared_read.data(),
                           shared_read.size());
        expect_status(result, tlm::TLM_OK_RESPONSE,
                      "shared-bank TLM read");
        expect(shared_read == little_endian(0xa1b2c3d4u),
               "TLM observes writes made through the shared bank API");
        expect(bank_read_active(reg::kIspSourceAddress,
                                "active state after direct pending write") ==
                   0x40bb2044u,
               "direct pending update also remains frame-shadowed");

        const std::uint32_t errors =
            bank_read(reg::kIspTlmErrorCount, "successful transfers");
        expect(errors == 0u,
               "successful partial and byte-enabled accesses record no errors");
    }

    void expect_rejected(TransportResult result,
                         tlm::tlm_response_status response,
                         std::uint32_t expected_error_count,
                         const std::string& label)
    {
        expect_status(result, response, label);
        expect(result.delay == target.latency(),
               label + ": rejected access receives configured latency");
        expect(bank_read(reg::kIspTlmErrorCount, label) ==
                   expected_error_count,
               label + ": increments the shared TLM error counter once");
    }

    void test_error_mapping()
    {
        bank.reset();
        target.set_latency(sc_core::sc_time(7, sc_core::SC_NS));
        std::uint32_t error_count = 0u;
        auto data = little_endian(0x00001000u);

        expect_rejected(transport(tlm::TLM_WRITE_COMMAND,
                                  reg::kIspSensorWidth,
                                  data.data(),
                                  data.size()),
                        tlm::TLM_COMMAND_ERROR_RESPONSE,
                        ++error_count,
                        "write to read-only identification register");

        expect_rejected(transport(tlm::TLM_READ_COMMAND,
                                  0x20u,
                                  data.data(),
                                  data.size()),
                        tlm::TLM_ADDRESS_ERROR_RESPONSE,
                        ++error_count,
                        "read from reserved word");

        expect_rejected(transport(tlm::TLM_READ_COMMAND,
                                  reg::kDpcThreshold + 1u,
                                  data.data(),
                                  2u),
                        tlm::TLM_ADDRESS_ERROR_RESPONSE,
                        ++error_count,
                        "naturally misaligned halfword");

        expect_rejected(transport(tlm::TLM_WRITE_COMMAND,
                                  reg::kDpcThreshold + 3u,
                                  data.data(),
                                  2u),
                        tlm::TLM_ADDRESS_ERROR_RESPONSE,
                        ++error_count,
                        "transfer crossing a register word");

        expect_rejected(transport(tlm::TLM_READ_COMMAND,
                                  reg::kAddressSpaceBytes,
                                  data.data(),
                                  1u),
                        tlm::TLM_ADDRESS_ERROR_RESPONSE,
                        ++error_count,
                        "address beyond the canonical map");

        expect_rejected(transport(tlm::TLM_READ_COMMAND,
                                  reg::kDpcThreshold,
                                  data.data(),
                                  3u),
                        tlm::TLM_BURST_ERROR_RESPONSE,
                        ++error_count,
                        "unsupported three-byte transfer");

        expect_rejected(transport(tlm::TLM_WRITE_COMMAND,
                                  reg::kDpcThreshold,
                                  data.data(),
                                  data.size(),
                                  2u),
                        tlm::TLM_BURST_ERROR_RESPONSE,
                        ++error_count,
                        "streaming width shorter than data length");

        unsigned char invalid_enable = 0x7fu;
        expect_rejected(transport(tlm::TLM_READ_COMMAND,
                                  reg::kDpcThreshold,
                                  data.data(),
                                  data.size(),
                                  data.size(),
                                  &invalid_enable,
                                  1u),
                        tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                        ++error_count,
                        "invalid byte-enable value");

        unsigned char enabled = TLM_BYTE_ENABLED;
        expect_rejected(transport(tlm::TLM_WRITE_COMMAND,
                                  reg::kDpcThreshold,
                                  data.data(),
                                  data.size(),
                                  data.size(),
                                  &enabled,
                                  0u),
                        tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE,
                        ++error_count,
                        "non-null zero-length byte-enable array");

        expect_rejected(transport(tlm::TLM_IGNORE_COMMAND,
                                  reg::kDpcThreshold,
                                  data.data(),
                                  data.size()),
                        tlm::TLM_COMMAND_ERROR_RESPONSE,
                        ++error_count,
                        "unsupported TLM command");

        expect_rejected(transport(tlm::TLM_READ_COMMAND,
                                  reg::kDpcThreshold,
                                  nullptr,
                                  data.size()),
                        tlm::TLM_GENERIC_ERROR_RESPONSE,
                        ++error_count,
                        "null transaction data pointer");

        expect_rejected(transport(tlm::TLM_WRITE_COMMAND,
                                  reg::kOecfLutIndex,
                                  data.data(),
                                  data.size()),
                        tlm::TLM_GENERIC_ERROR_RESPONSE,
                        ++error_count,
                        "register-bank invalid value");
        expect(bank_read(reg::kOecfLutIndex, "rejected OECF index") == 0u,
               "invalid-value transaction does not update the register");
    }

    void test_start_and_shared_commit()
    {
        bank.reset();
        target.set_latency(sc_core::sc_time(5, sc_core::SC_NS));

        unsigned char command = static_cast<unsigned char>(
            reg::kJobControlSourceMode | reg::kJobControlStart |
            reg::kJobControlDirectRgbInput);
        const auto result = transport(tlm::TLM_WRITE_COMMAND,
                                      reg::kIspJobControl,
                                      &command,
                                      1u);
        expect_status(result, tlm::TLM_OK_RESPONSE, "START byte write");
        expect(bank.consume_start_request(),
               "TLM START write creates one shared-bank request");
        expect(!bank.consume_start_request(),
               "shared-bank START request is consumed exactly once");
        expect(bank_read(reg::kIspJobControl, "pending job control") ==
                   (reg::kJobControlSourceMode |
                    reg::kJobControlDirectRgbInput),
               "START self-clears while persistent job fields remain pending");
        expect(bank_read_active(reg::kIspJobControl,
                                "active job control before commit") == 0u,
               "persistent job fields remain inactive before frame commit");

        bank.commit_frame();
        expect(bank_read_active(reg::kIspJobControl,
                                "active job control after commit") ==
                   (reg::kJobControlSourceMode |
                    reg::kJobControlDirectRgbInput),
               "frame commit activates persistent TLM job fields");
    }

    void run()
    {
        test_little_endian_partial_access_and_commit();
        test_error_mapping();
        test_start_and_shared_commit();
        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char**)
{
    ControlTargetTester tester("tester");
    sc_core::sc_start();
    if (failures == 0) {
        std::cout << "PASS: ISP shared-register-bank TLM control target tests\n";
    }
    return failures == 0 ? 0 : 1;
}
