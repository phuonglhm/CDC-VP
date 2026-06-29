#pragma once

#include <vector>

#include "prediction_result.h"

namespace cdc::components {

class mode_decision {
public:
    prediction_result choose(const std::vector<prediction_result>& candidates) const;
};

} // namespace cdc::components
