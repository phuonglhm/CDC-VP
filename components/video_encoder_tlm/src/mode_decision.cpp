#include "mode_decision.h"

namespace cdc::components {

prediction_result mode_decision::choose(const std::vector<prediction_result>& candidates) const
{
    if (candidates.empty()) {
        return {};
    }

    prediction_result best = candidates.front();
    for (const auto& candidate : candidates) {
        if (candidate.cost < best.cost) {
            best = candidate;
        }
    }
    return best;
}

} // namespace cdc::components
