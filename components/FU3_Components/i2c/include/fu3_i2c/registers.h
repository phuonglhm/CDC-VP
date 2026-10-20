#pragma once
#include <cstdint>
namespace fu3_i2c {
inline constexpr std::uint64_t base_address=0x50080000;
inline constexpr unsigned plic_id=15, ibex_fast_irq=2;
namespace reg {
enum : unsigned { PRESCALER=0, CTRL=4, RX=8, STATUS=12, TX=16, CMD=20,
 TLOW=24, THIGH=28, THD_STA=32, TSU_STA=36, THD_DAT=40, TSU_STO=44,
 TBUF=48, TSP=52, SLAVE_ADDR=56, SLAVE_CTRL=60, SLAVE_STATUS=64 };
enum : unsigned { EN=128, IEN=64, TIMING_EN=32, STA=128, STO=64,
 RD=32, WR=16, NACK=8, IACK=1, RXACK=128, BUSY=64, AL=32, TIP=2, IF=1 };
enum : unsigned { RX_VALID=128, TX_DONE=64, STOP_DET=32, MASTER_NACK=16,
 ACTIVE=8, DIR=4, ADDR_MATCH=2 };
}
}
