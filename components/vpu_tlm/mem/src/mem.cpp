#include "mem.h"

#include <algorithm>

namespace cdc::components {

const char* to_string(mem_status status)
{
    switch (status) {
    case mem_status::ok:
        return "ok";
    case mem_status::invalid_config:
        return "invalid_config";
    case mem_status::out_of_range:
        return "out_of_range";
    case mem_status::width_mismatch:
        return "width_mismatch";
    case mem_status::byte_enable_unsupported:
        return "byte_enable_unsupported";
    case mem_status::byte_enable_size_mismatch:
        return "byte_enable_size_mismatch";
    case mem_status::instance_not_found:
        return "instance_not_found";
    default:
        return "unknown";
    }
}

bool mem::add_instance(const mem_config& config)
{
    mem_instance instance(config);

    if (!instance.valid()) {
        return false;
    }

    const auto result =
        instances_.emplace(config.name, std::move(instance));

    return result.second;
}

bool mem::has(const std::string& name) const
{
    return instances_.find(name) != instances_.end();
}

mem_instance* mem::get(const std::string& name)
{
    auto it = instances_.find(name);

    if (it == instances_.end()) {
        return nullptr;
    }

    return &it->second;
}

const mem_instance* mem::get(const std::string& name) const
{
    auto it = instances_.find(name);

    if (it == instances_.end()) {
        return nullptr;
    }

    return &it->second;
}

mem_status mem::read(const std::string& name,
                     std::uint32_t addr,
                     std::vector<std::uint8_t>& data) const
{
    const mem_instance* instance = get(name);

    if (instance == nullptr) {
        return mem_status::instance_not_found;
    }

    return instance->read(addr, data);
}

mem_status mem::write(const std::string& name,
                      std::uint32_t addr,
                      const std::vector<std::uint8_t>& data)
{
    mem_instance* instance = get(name);

    if (instance == nullptr) {
        return mem_status::instance_not_found;
    }

    return instance->write(addr, data);
}

mem_status mem::write_be(const std::string& name,
                         std::uint32_t addr,
                         const std::vector<std::uint8_t>& data,
                         const std::vector<bool>& byte_enable)
{
    mem_instance* instance = get(name);

    if (instance == nullptr) {
        return mem_status::instance_not_found;
    }

    return instance->write_be(addr, data, byte_enable);
}

mem_status mem::reset_all()
{
    for (auto& item : instances_) {
        const mem_status status = item.second.reset();

        if (status != mem_status::ok) {
            return status;
        }
    }

    return mem_status::ok;
}

std::vector<std::string> mem::names() const
{
    std::vector<std::string> out;
    out.reserve(instances_.size());

    for (const auto& item : instances_) {
        out.push_back(item.first);
    }

    std::sort(out.begin(), out.end());
    return out;
}

} // namespace cdc::components
