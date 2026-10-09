#pragma once
#include "ec_types.h"

namespace h264::ec {
    
struct NalFormatter {
    static void wrap_nal_unit(EcResult& res);
};

} // namespace h264::ec