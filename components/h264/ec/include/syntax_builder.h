#pragma once
#include "ec_types.h"
namespace h264::ec {
struct SequenceSyntax {
    unsigned width=16,height=16,qp=26,frame_num_bits=4,poc_bits=4;
    unsigned crop_bottom=0; // 4:2:0 frame crop units: two luma rows.
};
struct SliceSyntax {
    bool idr=true,p_slice=false;
    unsigned frame_num=0,poc=0,idr_pic_id=0;
    int qp_delta=0;
    unsigned disable_deblocking=1;
    int alpha_offset_div2=0,beta_offset_div2=0;
};
struct MacroblockSyntax {
    bool intra=true;
    std::array<unsigned,16> modes{},predicted_modes{};
    unsigned chroma_mode=0,cbp=0;
    int mvd_x=0,mvd_y=0;
};
struct SyntaxBuilder {
    static void validate(const SequenceSyntax&,const SliceSyntax&);
    static void build_sps(const SequenceSyntax&,BitWriter&);
    static void build_pps(const SequenceSyntax&,BitWriter&);
    static void build_slice_header(const SequenceSyntax&,const SliceSyntax&,BitWriter&);
    static void build_macroblock(const MacroblockSyntax&,bool p_slice,BitWriter&);
    // Legacy single-block diagnostic interface; not a complete coded picture.
    static void build_slice_header(const EcRequest&,BitWriter&);
};
} // namespace h264::ec
