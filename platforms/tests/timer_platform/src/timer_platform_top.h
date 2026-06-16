#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::tests::timer_platform {

// Small Bremen/RISC-V SoC for timer IP verification:
// CPU -> bus_router -> RAM/UART/CLINT/PLIC/timer, with timer external IRQ via PLIC.
class timer_platform_top : public sc_core::sc_module {
public:
   timer_platform_top(sc_core::sc_module_name name, std::string config_path);
   ~timer_platform_top() override;

   void load_firmware(const std::string &path);
   std::string backend_name() const;

private:
   struct impl;
   std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::tests::timer_platform
