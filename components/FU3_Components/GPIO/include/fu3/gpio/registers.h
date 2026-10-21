#pragma once
#include <cstdint>

namespace fu3::gpio::reg {
constexpr unsigned PADDIR = 0x00, PADIN = 0x04, PADOUT = 0x08, INTEN = 0x0c, INTTYPE0 = 0x10,
                   INTTYPE1 = 0x14, INTSTATUS = 0x18, POWEREVENT = 0x1c, PADCFG0 = 0x20;
constexpr std::uint32_t PADCFG_MASK = 0x3f3f3f3f, PADCFG_RESET = 0x02020202;
} // namespace fu3::gpio::reg
