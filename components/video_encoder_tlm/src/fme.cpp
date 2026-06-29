#include "fme.h"

namespace cdc::components {

prediction_result fme::refine(const prediction_result& coarse) const
{
    prediction_result refined = coarse;
    if (refined.cost > 0) {
        refined.cost -= 1;
    }
    return refined;
}

} // namespace cdc::components
