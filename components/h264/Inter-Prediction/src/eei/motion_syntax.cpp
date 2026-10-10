#include <h264/inter/inter_core.h>

namespace h264::inter {
MotionSyntax motion_syntax(const Mode& mode) {
    validate_partition(mode.partition); validate_mv(mode.winner.candidate.mv); validate_mv(mode.predicted_mv);
    const auto mv = mode.winner.candidate.mv;
    return {mode.winner.candidate.reference, mode.partition, mv,
            {mv.x - mode.predicted_mv.x, mv.y - mode.predicted_mv.y}};
}
}
