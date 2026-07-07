#pragma once

#include <cstdint>
#include <string>

namespace cdc::components {

enum class mem_port_kind {
    single_port,
    simple_dual_port,
    true_dual_port,
    register_file
};

enum class mem_status {
    ok,
    invalid_config,
    out_of_range,
    width_mismatch,
    byte_enable_unsupported,
    byte_enable_size_mismatch,
    instance_not_found
};

struct mem_config {
    std::string name;

    std::uint32_t depth = 0;
    std::uint32_t width_bits = 0;

    mem_port_kind port_kind = mem_port_kind::single_port;

    bool byte_enable = false;

    std::uint32_t read_ports = 1;
    std::uint32_t write_ports = 1;

    bool clear_on_reset = true;
};

const char* to_string(mem_status status);

} // namespace cdc::components
