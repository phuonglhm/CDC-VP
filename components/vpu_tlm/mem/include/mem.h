#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "mem_instance.h"
#include "mem_types.h"

namespace cdc::components {

class mem {
public:
    mem() = default;

    bool add_instance(const mem_config& config);

    bool has(const std::string& name) const;

    mem_instance* get(const std::string& name);
    const mem_instance* get(const std::string& name) const;

    mem_status read(const std::string& name,
                    std::uint32_t addr,
                    std::vector<std::uint8_t>& data) const;

    mem_status write(const std::string& name,
                     std::uint32_t addr,
                     const std::vector<std::uint8_t>& data);

    mem_status write_be(const std::string& name,
                        std::uint32_t addr,
                        const std::vector<std::uint8_t>& data,
                        const std::vector<bool>& byte_enable);

    mem_status reset_all();

    std::vector<std::string> names() const;

private:
    std::unordered_map<std::string, mem_instance> instances_;
};

} // namespace cdc::components
