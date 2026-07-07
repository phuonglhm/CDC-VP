#pragma once

#include <vector>

#include "mem.h"
#include "mem_types.h"

namespace cdc::components {

std::vector<mem_config> make_xk265_default_mem_configs();

mem make_xk265_mem();

} // namespace cdc::components
