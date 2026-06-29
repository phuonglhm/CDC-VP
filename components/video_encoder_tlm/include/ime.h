#pragma once

#include "block.h"
#include "frame.h"
#include "prediction_result.h"

namespace cdc::components {

class ime {
public:
    prediction_result run(const frame& input, const block& region) const;
};

} // namespace cdc::components
