#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::tests::uart_platform {

class uart_platform_top : public sc_core::sc_module {
public:
    uart_platform_top(sc_core::sc_module_name name, std::string config_path);
    ~uart_platform_top() override;

    void load_firmware(const std::string& path);
    std::string backend_name() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

}
