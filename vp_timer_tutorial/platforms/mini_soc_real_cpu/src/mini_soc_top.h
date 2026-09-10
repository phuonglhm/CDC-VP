#pragma once

#include <memory>
#include <string>

#include <systemc>

namespace tutorial {

class mini_soc_top : public sc_core::sc_module {
public:
    mini_soc_top(sc_core::sc_module_name name, std::string config_path);
    ~mini_soc_top() override;

    void load_firmware(const std::string& path);
    // Opens a VCD trace of the platform-level signals.  Must be called before
    // sc_start(); the file is closed by the destructor.
    void enable_tracing(const std::string& path);
    std::string cpu_backend_name() const;
    std::uint64_t retired_instructions() const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace tutorial

