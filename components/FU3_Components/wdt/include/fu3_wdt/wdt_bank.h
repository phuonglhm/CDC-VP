#pragma once
#include "fu3_wdt/wdt.h"
#include <array>
#include <memory>
namespace fu3_wdt {
class WatchdogBank : public sc_core::sc_module {
public:
    inline static constexpr std::array<std::uint64_t,2> bases{FU3_WDT0_BASE,FU3_WDT1_BASE};
    std::array<std::unique_ptr<Watchdog>,2> watchdogs;
    explicit WatchdogBank(sc_core::sc_module_name name,
        std::array<sc_core::sc_time,2> ticks={sc_core::sc_time(5,sc_core::SC_NS),sc_core::sc_time(5,sc_core::SC_NS)},
        unsigned apb_wait_cycles=7)
        :sc_module(name) {
        for(unsigned i=0;i<2;++i)
            watchdogs[i]=std::make_unique<Watchdog>(("WDT"+std::to_string(i)).c_str(),ticks[i],apb_wait_cycles);
    }
};
}
