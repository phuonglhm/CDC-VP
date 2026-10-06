#pragma once
#include <cstdint>

namespace fx1::dma::reg {
// DMA_Config.xlsx: Memory Map, Register Map, Bus Interface, Parameter.
inline constexpr unsigned CHANNEL_COUNT = 8;
inline constexpr unsigned CHANNEL_STRIDE = 0x100;
inline constexpr unsigned FIFO_BYTES = 32;
inline constexpr unsigned MAX_READ_PENDING = 4;
inline constexpr unsigned MAX_WRITE_PENDING = 4;
inline constexpr unsigned MAX_BURST_BEATS = 16;
inline constexpr unsigned AXI_DATA_WIDTH = 32;
inline constexpr unsigned AXI_ADDR_WIDTH = 32;
inline constexpr unsigned AXI_ID_WIDTH = 4;
inline constexpr unsigned AXI_MASTER_ID = 4;
inline constexpr unsigned TRANSFER_SIZE_WIDTH = 10;
inline constexpr unsigned REGISTER_BYTES = 0x1100;

inline constexpr std::uint32_t CH_CMD_READ_ADDR = 0x00;
inline constexpr std::uint32_t CH_CMD_WRITE_ADDR = 0x04;
inline constexpr std::uint32_t CH_CMD_TRANSFER_SIZE = 0x08;
inline constexpr std::uint32_t CH_CMD_CONTROL = 0x0c;
inline constexpr std::uint32_t CH_READ_CONFIG = 0x10;
inline constexpr std::uint32_t CH_WRITE_CONFIG = 0x14;
inline constexpr std::uint32_t CH_MODE_CONFIG = 0x18;
inline constexpr std::uint32_t CH_SCHEDULE_CONFIG = 0x1c;
inline constexpr std::uint32_t CH_PERIPHERAL_CONFIG = 0x20;
inline constexpr std::uint32_t CH_AXI_ATTR_REG = 0x24;
inline constexpr std::uint32_t CH_RESTRICTION_STATUS = 0x2c;
inline constexpr std::uint32_t CH_READ_OFFSET_STATUS = 0x30;
inline constexpr std::uint32_t CH_WRITE_OFFSET_STATUS = 0x34;
inline constexpr std::uint32_t CH_FIFO_STATUS = 0x38;
inline constexpr std::uint32_t CH_OUTSTANDING_STATUS = 0x3c;
inline constexpr std::uint32_t CH_ENABLE = 0x40;
inline constexpr std::uint32_t CH_START = 0x44;
inline constexpr std::uint32_t CH_ACTIVE_STATUS = 0x48;
inline constexpr std::uint32_t CH_TRANSFER_COUNT = 0x50;
inline constexpr std::uint32_t CH_INTERRUPT_RAW_STATUS = 0xa0;
inline constexpr std::uint32_t CH_INTERRUPT_CLEAR = 0xa4;
inline constexpr std::uint32_t CH_INTERRUPT_ENABLE = 0xa8;
inline constexpr std::uint32_t CH_INTERRUPT_STATUS = 0xac;

inline constexpr std::uint32_t CORE_STATUS = 0x1000;
inline constexpr std::uint32_t CORE_JOINT_CONFIG = 0x1030;
inline constexpr std::uint32_t CORE_PRIORITY_CONFIG = 0x1038;
inline constexpr std::uint32_t CORE_CLOCK_DIVIDER = 0x1040;
inline constexpr std::uint32_t CORE_CHANNEL_START = 0x1048;
inline constexpr std::uint32_t PERIPHERAL_RX_REQUEST = 0x1050;
inline constexpr std::uint32_t PERIPHERAL_TX_REQUEST = 0x1054;
inline constexpr std::uint32_t CORE_IDLE_STATUS = 0x10d0;
inline constexpr std::uint32_t DESIGN_CAPABILITY_STATUS = 0x10e0;
inline constexpr std::uint32_t CORE_CAPABILITY_STATUS0 = 0x10f0;
inline constexpr std::uint32_t CORE_CAPABILITY_STATUS1 = 0x10f4;

inline constexpr std::uint32_t CONFIG_RESET = 0x84010000;
inline constexpr std::uint32_t CONFIG_MASK = 0xcf3f007f;
inline constexpr std::uint32_t PERIPHERAL_MASK = 0x071f071f;
inline constexpr std::uint32_t ATTR_MASK = 0x0000bfbf;
inline constexpr std::uint32_t MODE_MASK = 0x30000000;
inline constexpr std::uint32_t TRANSFER_SIZE_MASK = 0x3ff;
inline constexpr std::uint32_t INTERRUPT_MASK = 0x1fff;
inline constexpr std::uint32_t CAPABILITY0 = 0x0a602258;
inline constexpr std::uint32_t CMD_SET_INT = 1;
inline constexpr std::uint32_t CMD_LAST = 2;
inline constexpr std::uint32_t ADDRESS_INCREMENT = 1u << 31;
inline constexpr std::uint32_t OUTSTANDING_ENABLE = 1u << 30;

enum Interrupt : std::uint32_t {
    COMMAND_COMPLETE = 1u << 0,
    READ_SLVERR = 1u << 1,
    WRITE_SLVERR = 1u << 2,
    READ_DECERR = 1u << 3,
    WRITE_DECERR = 1u << 4,
    FIFO_OVERFLOW = 1u << 5,
    FIFO_UNDERFLOW = 1u << 6,
    READ_DATA_TIMEOUT = 1u << 7,
    READ_ADDRESS_TIMEOUT = 1u << 8,
    WRITE_RESPONSE_TIMEOUT = 1u << 9,
    WRITE_DATA_TIMEOUT = 1u << 10,
    WRITE_ADDRESS_TIMEOUT = 1u << 11,
    WATCHDOG_TIMEOUT = 1u << 12
};
constexpr std::uint32_t channel(unsigned n, std::uint32_t offset) {
    return CHANNEL_STRIDE * n + offset;
}
} // namespace fx1::dma::reg
