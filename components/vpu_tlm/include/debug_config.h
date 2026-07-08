#pragma once

namespace cdc::components {

inline bool& verbose_flag()
{
    static bool enabled = false;
    return enabled;
}

inline bool verbose_enabled()
{
    return verbose_flag();
}

inline void set_verbose_enabled(bool enabled)
{
    verbose_flag() = enabled;
}

} // namespace cdc::components
