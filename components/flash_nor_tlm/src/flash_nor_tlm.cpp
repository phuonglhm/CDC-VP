#include "flash_nor_tlm.h"

#include <algorithm>
#include <cstring>

namespace cdc::components {

namespace {

constexpr std::uint8_t kOpcodeRdid = 0x9f;
constexpr std::uint8_t kOpcodeRdsr = 0x05;
constexpr std::uint8_t kOpcodeRead = 0x03;
constexpr std::uint8_t kOpcodeFastRead = 0x0b;
constexpr std::uint8_t kOpcodeQuadRead = 0x6b;
constexpr std::uint8_t kOpcodeQuadIoRead = 0xeb;

constexpr std::uint8_t kJedecManufacturer = 0xef;
constexpr std::uint8_t kJedecMemoryType = 0x40;
constexpr std::uint8_t kJedecCapacity16MiB = 0x18;

} // namespace

flash_nor_tlm::flash_nor_tlm(sc_core::sc_module_name name, std::size_t size_bytes)
    : sc_core::sc_module(name)
    , from_qspi_socket("from_qspi_socket")
    , mem_(size_bytes, 0xff)
{
    from_qspi_socket.register_b_transport(this, &flash_nor_tlm::b_transport);
}

void flash_nor_tlm::load(const std::uint8_t* data, std::size_t len, std::uint64_t offset)
{
    if (data == nullptr && len != 0) {
        SC_REPORT_ERROR("flash_nor_tlm", "load() null data");
        return;
    }
    if (offset > mem_.size() || len > mem_.size() - offset) {
        SC_REPORT_ERROR("flash_nor_tlm", "load() out of bounds");
        return;
    }
    std::memcpy(mem_.data() + offset, data, len);
}

void flash_nor_tlm::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay)
{
    auto* data = trans.get_data_ptr();
    const auto len = trans.get_data_length();
    if (data == nullptr || len == 0) {
        trans.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
        return;
    }

    const std::uint8_t opcode = data[0];
    const std::uint32_t addr = len >= 4 ? read_addr24(data + 1) : 0;

    std::fill(data, data + len, 0);

    switch (opcode) {
    case kOpcodeRdid:
        if (len > 1) {
            data[1] = kJedecManufacturer;
        }
        if (len > 2) {
            data[2] = kJedecMemoryType;
        }
        if (len > 3) {
            data[3] = kJedecCapacity16MiB;
        }
        break;

    case kOpcodeRdsr:
        if (len > 1) {
            data[1] = 0x00; // WIP=0, idle.
        }
        break;

    case kOpcodeRead:
        if (len >= 4) {
            for (unsigned i = 4; i < len; ++i) {
                data[i] = read_mem(addr + (i - 4));
            }
        }
        break;

    case kOpcodeFastRead:
    case kOpcodeQuadRead:
    case kOpcodeQuadIoRead:
        if (len >= 5) {
            for (unsigned i = 5; i < len; ++i) {
                data[i] = read_mem(addr + (i - 5));
            }
        }
        break;

    default:
        break;
    }

    delay += sc_core::sc_time(20, sc_core::SC_NS);
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

std::uint32_t flash_nor_tlm::read_addr24(const unsigned char* data)
{
    return (static_cast<std::uint32_t>(data[0]) << 16) |
           (static_cast<std::uint32_t>(data[1]) << 8) |
           static_cast<std::uint32_t>(data[2]);
}

std::uint8_t flash_nor_tlm::read_mem(std::uint32_t addr) const
{
    if (addr >= mem_.size()) {
        return 0xff;
    }
    return mem_[addr];
}

} // namespace cdc::components
