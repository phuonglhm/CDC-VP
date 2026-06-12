#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace cdc::platforms::mini_tlm {

class mini_tlm_top : public sc_core::sc_module {
public:
    mini_tlm_top(sc_core::sc_module_name name, std::string config_path);
    ~mini_tlm_top() override;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace cdc::platforms::mini_tlm
