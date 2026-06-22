#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::tests::dmic_platform {

// Small Bremen/RISC-V SoC for DMIC IP verification:
// CPU -> bus_router -> RAM/UART/CLINT/PLIC/DMIC, with DMIC external IRQ via PLIC.
class dmic_platform_top : public sc_core::sc_module {
public:
   dmic_platform_top(sc_core::sc_module_name name, std::string config_path);
   ~dmic_platform_top() override;

   void load_firmware(const std::string &path);
   std::string backend_name() const;

private:
   struct impl;
   std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::tests::dmic_platform
