#pragma once

#include "prediction_result.h"

namespace cdc::components {

class fme {
public:
    prediction_result refine(const prediction_result& coarse) const;
};

} // namespace cdc::components
