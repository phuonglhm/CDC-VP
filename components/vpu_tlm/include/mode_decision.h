#pragma once

#include "prediction_result.h"

namespace cdc::components {

struct mode_decision_result {
    bool valid = false;

    prediction_result selected;
    prediction_mode selected_mode = prediction_mode::intra;

    prediction_result intra_candidate;
    prediction_result inter_candidate;

    static mode_decision_result invalid()
    {
        return mode_decision_result {};
    }
};

class mode_decision {
public:
    mode_decision() = default;

    mode_decision_result run(const prediction_result& intra,
                             const prediction_result& inter) const;
};

} // namespace cdc::components
