#pragma once

#include <array>
#include <cstdint>

#include "tq_types.h"
#include "transpose_ram.h"

namespace h264::tq {

class Ftq {
public:
    explicit Ftq(TransposeRam& transpose);

    std::array<std::int32_t, 16>
    transform(const std::array<std::int16_t, 16>& residual, BlockClass block_class);

    std::array<std::int16_t, 16>
    quantize(const std::array<std::int32_t, 16>& coeffs,
             std::uint8_t qp,
             BlockClass block_class) const;

private:
    TransposeRam& transpose_;
};

} // namespace h264::tq