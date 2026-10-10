#pragma once
#include "ec_types.h"

namespace h264::ec {
    
struct NalFormatter {
    static void wrap_nal_unit(EcResult& res, 
                              std::uint8_t nal_ref_idc, 
                              std::uint8_t nal_unit_type);
};

} // namespace h264::ec