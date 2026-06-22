#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::tests::dma_platform {

class dma_platform_top : public sc_core::sc_module {
public:
    dma_platform_top(sc_core::sc_module_name name, std::string config_path);
    ~dma_platform_top() override;

    void load_firmware(const std::string& path);
    std::string backend_name() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::tests::dma_platform
