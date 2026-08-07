#ifndef ISP_TLM_TLM_ISP_CONTROL_TARGET_H
#define ISP_TLM_TLM_ISP_CONTROL_TARGET_H

#include "registers/isp_register_bank.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

#include <cstdint>

namespace isp_tlm::tlm_frontend {

// Fast software-facing control path backed by the same register bank used by
// the cycle-level control adapter. Addresses are canonical register-map byte
// offsets; this target does not translate or privately cache register state.
class IspControlTarget : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<IspControlTarget> socket;

    IspControlTarget(sc_core::sc_module_name name,
                     registers::IspRegisterBank& register_bank,
                     sc_core::sc_time latency = sc_core::SC_ZERO_TIME)
        : sc_core::sc_module(name)
        , socket("socket")
        , register_bank_(register_bank)
        , latency_(latency)
    {
        socket.register_b_transport(this, &IspControlTarget::b_transport);
    }

    const sc_core::sc_time& latency() const { return latency_; }
    void set_latency(const sc_core::sc_time& latency) { latency_ = latency; }

private:
    registers::IspRegisterBank& register_bank_;
    sc_core::sc_time latency_;

    static bool supported_length(unsigned int length)
    {
        return length == 1u || length == 2u || length == 4u;
    }

    static bool byte_is_valid(unsigned char value)
    {
        return value == TLM_BYTE_ENABLED ||
               value == TLM_BYTE_DISABLED;
    }

    static tlm::tlm_response_status response_for(
        registers::AccessResult result)
    {
        switch (result) {
        case registers::AccessResult::Ok:
            return tlm::TLM_OK_RESPONSE;
        case registers::AccessResult::Misaligned:
        case registers::AccessResult::OutOfRange:
        case registers::AccessResult::Unmapped:
            return tlm::TLM_ADDRESS_ERROR_RESPONSE;
        case registers::AccessResult::ReadOnly:
            return tlm::TLM_COMMAND_ERROR_RESPONSE;
        case registers::AccessResult::InvalidByteStrobe:
            return tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE;
        case registers::AccessResult::InvalidValue:
            return tlm::TLM_GENERIC_ERROR_RESPONSE;
        }
        return tlm::TLM_GENERIC_ERROR_RESPONSE;
    }

    void reject(tlm::tlm_generic_payload& transaction,
                tlm::tlm_response_status response)
    {
        transaction.set_response_status(response);
        register_bank_.record_tlm_error();
    }

    void b_transport(tlm::tlm_generic_payload& transaction,
                     sc_core::sc_time& delay)
    {
        delay += latency_;
        transaction.set_dmi_allowed(false);
        transaction.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

        const tlm::tlm_command command = transaction.get_command();
        if (command != tlm::TLM_READ_COMMAND &&
            command != tlm::TLM_WRITE_COMMAND) {
            reject(transaction, tlm::TLM_COMMAND_ERROR_RESPONSE);
            return;
        }

        unsigned char* const data = transaction.get_data_ptr();
        if (data == nullptr) {
            reject(transaction, tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }

        const unsigned int length = transaction.get_data_length();
        if (!supported_length(length)) {
            reject(transaction, tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }
        if (transaction.get_streaming_width() < length) {
            reject(transaction, tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }

        unsigned char* const byte_enables =
            transaction.get_byte_enable_ptr();
        const unsigned int byte_enable_length =
            transaction.get_byte_enable_length();
        if (byte_enables != nullptr) {
            if (byte_enable_length == 0u) {
                reject(transaction, tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
                return;
            }
            for (unsigned int index = 0u; index < length; ++index) {
                if (!byte_is_valid(byte_enables[index % byte_enable_length])) {
                    reject(transaction, tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
                    return;
                }
            }
        }

        const std::uint64_t address = transaction.get_address();
        if (address >= registers::kAddressSpaceBytes) {
            reject(transaction, tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        const unsigned int byte_offset =
            static_cast<unsigned int>(address & 0x3u);
        if (byte_offset + length > registers::kRegisterBytes ||
            (address & static_cast<std::uint64_t>(length - 1u)) != 0u) {
            reject(transaction, tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }

        const std::uint32_t word_address =
            static_cast<std::uint32_t>(address & ~std::uint64_t {0x3u});
        registers::AccessResult access = registers::AccessResult::Unmapped;

        if (command == tlm::TLM_READ_COMMAND) {
            std::uint32_t word = 0u;
            access = register_bank_.read(word_address, word);
            if (access == registers::AccessResult::Ok) {
                for (unsigned int index = 0u; index < length; ++index) {
                    const bool enabled =
                        byte_enables == nullptr ||
                        byte_enables[index % byte_enable_length] ==
                            TLM_BYTE_ENABLED;
                    if (enabled) {
                        const unsigned int shift =
                            (byte_offset + index) * 8u;
                        data[index] =
                            static_cast<unsigned char>((word >> shift) & 0xffu);
                    }
                }
            }
        } else {
            std::uint32_t word = 0u;
            std::uint8_t byte_strobe = 0u;
            for (unsigned int index = 0u; index < length; ++index) {
                const bool enabled =
                    byte_enables == nullptr ||
                    byte_enables[index % byte_enable_length] ==
                        TLM_BYTE_ENABLED;
                if (enabled) {
                    const unsigned int lane = byte_offset + index;
                    word |= static_cast<std::uint32_t>(data[index]) <<
                            (lane * 8u);
                    byte_strobe |= static_cast<std::uint8_t>(1u << lane);
                }
            }
            access = register_bank_.write(word_address, word, byte_strobe);
        }

        const tlm::tlm_response_status response = response_for(access);
        if (response != tlm::TLM_OK_RESPONSE) {
            reject(transaction, response);
            return;
        }
        transaction.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

}  // namespace isp_tlm::tlm_frontend

#endif  // ISP_TLM_TLM_ISP_CONTROL_TARGET_H
