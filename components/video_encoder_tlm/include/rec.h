#pragma once

#include "block.h"
#include "frame.h"

namespace cdc::components {

class rec {
public:
    frame reconstruct(const frame& input, const block& region) const;
};

} // namespace cdc::components
