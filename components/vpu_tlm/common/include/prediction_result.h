#pragma once

#include <cstdint>
#include <vector>

#include "encoder_defs.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// Data passed between prediction, mode decision, reconstruction and CABAC.
// -----------------------------------------------------------------------------

enum class prediction_mode : std::uint32_t {
    intra = 0,
    inter = 1,
};

enum class partition_mode : std::uint32_t {
    part_2nx2n = 0,
    part_2nxn  = 1,
    part_nx2n  = 2,
    part_split = 3,
};

enum class intra_prediction_mode : std::uint32_t {
    planar = 0,
    dc = 1,

    // HEVC angular modes are 2..34.
    angular_2  = 2,
    angular_10 = 10,
    angular_18 = 18,
    angular_26 = 26,
    angular_34 = 34,
};

// Motion vector is stored in quarter-pel unit.
// Example:
//   mv.x = 4  means +1 integer pixel
//   mv.x = 1  means +1/4 pixel
//   mv.x = -2 means -1/2 pixel
struct motion_vector {
    int x = 0;
    int y = 0;

    motion_vector() = default;

    motion_vector(int x_, int y_)
        : x(x_),
          y(y_)
    {
    }

    int integer_x() const
    {
        return x / 4;
    }

    int integer_y() const
    {
        return y / 4;
    }
};

struct prediction_result {
    bool valid = false;

    prediction_mode mode = prediction_mode::intra;

    std::uint32_t cost = 0;
    std::uint32_t rate = 0;
    std::uint32_t distortion = 0;
    std::uint32_t qp = INIT_QP;

    partition_mode partition = partition_mode::part_2nx2n;
    intra_prediction_mode intra_mode = intra_prediction_mode::dc;

    motion_vector mv {};

    bool skip = false;
    bool merge = false;
    bool i_in_p = false;

    // Predicted pixels for current block, luma only at current level.
    std::vector<std::uint8_t> predicted_luma;

    // Residual = original - predicted.
    std::vector<std::int16_t> residual_luma;

    static prediction_result invalid()
    {
        return prediction_result {};
    }

    static prediction_result make_intra(std::uint32_t cost_,
                                        intra_prediction_mode intra_mode_,
                                        partition_mode partition_,
                                        std::uint32_t qp_)
    {
        prediction_result result;
        result.valid = true;
        result.mode = prediction_mode::intra;
        result.cost = cost_;
        result.intra_mode = intra_mode_;
        result.partition = partition_;
        result.qp = qp_;
        return result;
    }

    static prediction_result make_inter(std::uint32_t cost_,
                                        motion_vector mv_,
                                        partition_mode partition_,
                                        std::uint32_t qp_)
    {
        prediction_result result;
        result.valid = true;
        result.mode = prediction_mode::inter;
        result.cost = cost_;
        result.mv = mv_;
        result.partition = partition_;
        result.qp = qp_;
        return result;
    }
};

} // namespace cdc::components
