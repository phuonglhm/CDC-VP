#pragma once
#include "fu3_uart/uart.h"
#include <array>
#include <memory>
#include <string>

namespace fu3_uart {
class UartBank : public sc_core::sc_module {
public:
    inline static constexpr std::array<std::uint64_t, 3> bases{
        FU3_UART0_BASE, FU3_UART1_BASE, FU3_UART2_BASE};
    std::array<std::unique_ptr<Uart>, 3> controllers;
    explicit UartBank(sc_core::sc_module_name name, std::array<Config, 3> configs = {})
        : sc_module(name) {
        for (unsigned i=0; i<controllers.size(); ++i)
            controllers[i] = std::make_unique<Uart>(("UART"+std::to_string(i)).c_str(), configs[i]);
    }
};
} // namespace fu3_uart
