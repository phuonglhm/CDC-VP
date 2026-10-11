#pragma once
#include "syntax_builder.h"
#include "cavlc.h"
#include <vector>
namespace h264::ec {
using Coefficients=std::array<std::int16_t,16>;
struct CodedMacroblock {
    bool intra=true;
    std::array<unsigned,16> intra_modes{2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2};
    unsigned chroma_mode=0;
    int mvd_x=0,mvd_y=0;
    std::array<Coefficients,16> luma{}; // Natural order coefficients, AVC block scan.
    std::array<std::array<Coefficients,4>,2> chroma_ac{}; // Natural order, DC must be zero.
    std::array<std::array<std::int16_t,4>,2> chroma_dc{}; // Already transformed/quantized 2x2 scan order.
};
// Bounded transaction-level slice builder. One complete frame per slice, one L0
// reference, I4x4/P16x16, 8-bit 4:2:0. Context is reset for every call/slice.
// Caller drains the returned bytes through NAL DMA; no DMA-word padding inside NALs.
class SliceEncoder {
public:
    static std::vector<std::uint8_t> parameter_sets(const SequenceSyntax&);
    static std::vector<std::uint8_t> picture(const SequenceSyntax&,const SliceSyntax&,
        const std::vector<CodedMacroblock>&,std::size_t capacity=1024*1024);
};
} // namespace h264::ec
