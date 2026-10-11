#pragma once
#include "ec_types.h"
namespace h264::ec {
struct NalFormatter {
    // RBSP must already contain trailing bits. Returns unpadded Annex-B bytes.
    static std::vector<std::uint8_t> format_rbsp(const std::uint8_t* rbsp,std::size_t bytes,
        unsigned nal_ref_idc,unsigned nal_unit_type,std::size_t capacity);
    // Legacy fixed-buffer interface adds zero bytes for DMA word alignment.
    static void wrap_nal_unit(EcResult&,std::uint8_t nal_ref_idc,std::uint8_t nal_unit_type);
};
} // namespace h264::ec
