#pragma once
#include <cstdint>
namespace fu3_rng {
inline constexpr std::uint64_t base_address=0x44030000;
inline constexpr unsigned plic_id=21, ibex_fast_irq=10;
namespace reg {
enum : unsigned { CONTROL=0, COMMAND=4, DELAY=8, STATUS=12, DATA=16 };
enum : unsigned { ENABLE=1, NEXT=1, RESTART=2, VALID=1 };
}
}
