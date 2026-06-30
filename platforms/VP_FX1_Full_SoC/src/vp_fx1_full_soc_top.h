#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::vp_fx1_full_soc {

// VP_FX1 full SoC: the integrated Bremen RV32 virtual SoC assembled from all
// implemented TLM IP at their SoC-spec addresses (docs/peripheral_memory_map.md).
// CLINT/PLIC interrupt subsystem; ISP/VPU/NPU windows are reserved (planned).
class vp_fx1_full_soc_top : public sc_core::sc_module {
public:
    vp_fx1_full_soc_top(sc_core::sc_module_name name, std::string config_path);
    ~vp_fx1_full_soc_top() override;

    void load_firmware(const std::string& path);
    std::string backend_name() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::vp_fx1_full_soc
