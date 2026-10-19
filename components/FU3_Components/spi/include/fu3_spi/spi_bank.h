#pragma once
#include "fu3_spi/spi.h"
#include <memory>
namespace fu3_spi {
class SpiBank:public sc_core::sc_module {
public:
    inline static constexpr std::array<std::uint64_t,4> bases{FU3_SPI0_BASE,FU3_SPI1_BASE,FU3_SPI2_BASE,FU3_SPI3_BASE};
    std::array<std::unique_ptr<Spi>,4> controllers;
    explicit SpiBank(sc_core::sc_module_name name,unsigned depth=16,
        std::array<sc_core::sc_time,4> periods={sc_core::sc_time(10,sc_core::SC_NS),sc_core::sc_time(10,sc_core::SC_NS),sc_core::sc_time(10,sc_core::SC_NS),sc_core::sc_time(10,sc_core::SC_NS)})
        :sc_module(name){for(unsigned i=0;i<4;++i)controllers[i]=std::make_unique<Spi>(("SPI"+std::to_string(i)).c_str(),depth,periods[i]);}
};
}
