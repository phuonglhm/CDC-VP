#include "mode_decision.h"

namespace cdc::components {

mode_decision_result mode_decision::run(const prediction_result& intra,
                                        const prediction_result& inter) const
{
    mode_decision_result result;
    result.intra_candidate = intra;
    result.inter_candidate = inter;

    if (!intra.valid && !inter.valid) {
        return mode_decision_result::invalid();
    }

    if (intra.valid && !inter.valid) {
        result.valid = true;
        result.selected = intra;
        result.selected_mode = prediction_mode::intra;
        return result;
    }

    if (!intra.valid && inter.valid) {
        result.valid = true;
        result.selected = inter;
        result.selected_mode = prediction_mode::inter;
        return result;
    }

    if (inter.cost < intra.cost) {
        result.valid = true;
        result.selected = inter;
        result.selected_mode = prediction_mode::inter;
        return result;
    }

    if (intra.cost < inter.cost) {
        result.valid = true;
        result.selected = intra;
        result.selected_mode = prediction_mode::intra;
        return result;
    }

    // Stable tie-break rules for deterministic behavior:
    // 1. Prefer inter when skip is available at the same cost.
    // 2. Otherwise prefer intra.
    if (inter.skip && !intra.skip) {
        result.valid = true;
        result.selected = inter;
        result.selected_mode = prediction_mode::inter;
        return result;
    }

    result.valid = true;
    result.selected = intra;
    result.selected_mode = prediction_mode::intra;
    return result;
}

} // namespace cdc::components
