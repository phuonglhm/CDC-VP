#pragma once
#include <cstdint>
namespace h264::reg {
constexpr uint32_t SCON=0x00, STAT=0x04, FMSIZE=0x08, DFCON=0x0c;
constexpr uint32_t SPARA0=0x10, SPARA1=0x14, SPARA2=0x18;
constexpr uint32_t REFM=0x1c, NAL=0x20, CMB=0x24, STM_LEN=0x28;
constexpr uint32_t ENABLE=1, GIE=2, BUSY=1u<<16, ERROR=1u<<17, NORMAL=1u<<18;
}
