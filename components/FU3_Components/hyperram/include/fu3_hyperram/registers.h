#pragma once
#include <cstdint>
namespace fu3_hyperram {
// PROVISIONAL VP ABI v1. These controller offsets are not a published hardware CSR map.
inline constexpr std::uint64_t memory_base=0x60000000, csr_base=0x500b0000;
inline constexpr unsigned memory_size=32*1024*1024;
namespace reg {
enum : unsigned { ID=0, VERSION=4, CTRL=8, STATUS=12, LATENCY=16, TCSM=20,
 SETUP=24, HOLD=28, RECOVERY=32, APPLY=36, ACTIVE_LATENCY=40, GENERATION=44,
 DEV_ADDR=48, DEV_WDATA=52, DEV_CMD=56, DEV_RDATA=60,
 ERRORS=64, ERROR_CLEAR=68, IRQ_ENABLE=72, FAULT_ADDRESS=76, FAULT_WORDS=80,
 DELAY0=84, DELAY1=88, DELAY_APPLY=92, ACTIVE_DELAY=96,
 TRANSACTIONS=100, LAST_WORDS=104, FAULT_FLAGS=108 };
enum : unsigned { READY=1, PENDING=2, BUSY=4, QUIESCED=8, VALID=16, DONE=32, COMMAND_ERROR=64 };
enum : unsigned { DECODE=1, PROFILE=2, NOT_READY=4, TIMING=8, CONFIG=16,
 UNDERFLOW=32, READ_TIMEOUT=64, COMMAND_BUSY=128 };
}
}
