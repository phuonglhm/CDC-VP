#pragma once
#include "ec_types.h"

namespace h264::ec {
    
struct SyntaxBuilder {
    static void build_slice_header(const EcRequest& req, BitWriter& bw);
};

} // namespace h264::ec