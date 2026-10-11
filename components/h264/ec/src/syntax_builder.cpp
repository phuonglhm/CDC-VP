#include "syntax_builder.h"
#include "exp_golomb.h"
namespace h264::ec {
void SyntaxBuilder::validate(const SequenceSyntax& s,const SliceSyntax& p) {
    if(!s.width || !s.height || s.width%16 || s.height%16 || s.width>1920 || s.height>1088 ||
       s.crop_bottom>=s.height/2 || s.qp>51 || s.frame_num_bits<4 || s.frame_num_bits>16 || s.poc_bits<4 || s.poc_bits>16 ||
       p.frame_num>=(1u<<s.frame_num_bits) || p.poc>=(1u<<s.poc_bits) ||
       (p.idr && (p.p_slice || p.frame_num!=0)) || p.idr_pic_id>65535 || p.qp_delta < -51 || p.qp_delta > 51 ||
       int(s.qp)+p.qp_delta<0 || int(s.qp)+p.qp_delta>51 || p.disable_deblocking>2 ||
       p.alpha_offset_div2<-6 || p.alpha_offset_div2>6 || p.beta_offset_div2<-6 || p.beta_offset_div2>6)
        throw std::invalid_argument("unsupported Baseline frame/slice configuration");
}
void SyntaxBuilder::build_sps(const SequenceSyntax& s,BitWriter& b) {
    validate(s,{});
    b.write_bits(66,8);b.write_bits(0,8);b.write_bits(40,8); // Baseline, level 4.0.
    ExpGolomb::encode_ue(0,b);ExpGolomb::encode_ue(s.frame_num_bits-4,b);
    ExpGolomb::encode_ue(0,b);ExpGolomb::encode_ue(s.poc_bits-4,b);
    ExpGolomb::encode_ue(1,b);b.write_bits(0,1); // One reference; no frame_num gaps.
    ExpGolomb::encode_ue(s.width/16-1,b);ExpGolomb::encode_ue(s.height/16-1,b);
    b.write_bits(1,1);b.write_bits(1,1);b.write_bits(s.crop_bottom!=0,1);
    if(s.crop_bottom) {ExpGolomb::encode_ue(0,b);ExpGolomb::encode_ue(0,b);ExpGolomb::encode_ue(0,b);ExpGolomb::encode_ue(s.crop_bottom,b);}
    b.write_bits(0,1); // No VUI.
}
void SyntaxBuilder::build_pps(const SequenceSyntax& s,BitWriter& b) {
    validate(s,{});
    ExpGolomb::encode_ue(0,b);ExpGolomb::encode_ue(0,b);
    b.write_bits(0,1);b.write_bits(0,1);ExpGolomb::encode_ue(0,b);
    ExpGolomb::encode_ue(0,b);ExpGolomb::encode_ue(0,b);
    b.write_bits(0,1);b.write_bits(0,2);
    ExpGolomb::encode_se(int(s.qp)-26,b);ExpGolomb::encode_se(0,b);ExpGolomb::encode_se(0,b);
    b.write_bits(1,1);b.write_bits(0,1);b.write_bits(0,1); // Deblocking control, unconstrained intra, no redundant count.
}
void SyntaxBuilder::build_slice_header(const SequenceSyntax& s,const SliceSyntax& p,BitWriter& b) {
    validate(s,p);
    ExpGolomb::encode_ue(0,b);ExpGolomb::encode_ue(p.p_slice?0:2,b);ExpGolomb::encode_ue(0,b);
    b.write_bits(p.frame_num,s.frame_num_bits);
    if(p.idr) ExpGolomb::encode_ue(p.idr_pic_id,b);
    b.write_bits(p.poc,s.poc_bits);
    if(p.p_slice) {b.write_bits(0,1);b.write_bits(0,1);} // Default ref count, no list modification.
    if(p.idr) {b.write_bits(0,1);b.write_bits(0,1);} else b.write_bits(0,1);
    ExpGolomb::encode_se(p.qp_delta,b);ExpGolomb::encode_ue(p.disable_deblocking,b);
    if(p.disable_deblocking!=1) {ExpGolomb::encode_se(p.alpha_offset_div2,b);ExpGolomb::encode_se(p.beta_offset_div2,b);}
}
void SyntaxBuilder::build_macroblock(const MacroblockSyntax& m,bool p,BitWriter& b) {
    if(m.cbp>47 || m.chroma_mode>3 || (!p && !m.intra) ||
       m.mvd_x<-32768 || m.mvd_x>32767 || m.mvd_y<-32768 || m.mvd_y>32767)
        throw std::invalid_argument("macroblock syntax");
    for(unsigned i=0;i<16;++i) if(m.intra && (m.modes[i]>8 || m.predicted_modes[i]>8)) throw std::invalid_argument("intra mode");
    if(p) ExpGolomb::encode_ue(0,b); // No skipped MBs; each MB explicitly coded.
    ExpGolomb::encode_ue(m.intra?(p?5:0):0,b); // I_NxN or P_L0_16x16.
    if(m.intra) {
        for(unsigned i=0;i<16;++i) {
            unsigned mode=m.modes[i],pred=m.predicted_modes[i];b.write_bits(mode==pred,1);
            if(mode!=pred) b.write_bits(mode<pred?mode:mode-1,3);
        }
        ExpGolomb::encode_ue(m.chroma_mode,b);
    } else {ExpGolomb::encode_se(m.mvd_x,b);ExpGolomb::encode_se(m.mvd_y,b);}
    // H.264 Table 9-4, indexed by coded_block_pattern codeNum.
    static constexpr unsigned intra[48]={47,31,15,0,23,27,29,30,7,11,13,14,39,43,45,46,16,3,5,10,12,19,21,26,28,35,37,42,44,1,2,4,8,17,18,20,24,6,9,22,25,32,33,34,36,40,38,41};
    static constexpr unsigned inter[48]={0,16,1,2,4,8,32,3,5,10,12,15,47,7,11,13,14,6,9,31,35,37,42,44,33,34,36,40,39,43,45,46,17,18,20,24,19,21,26,28,23,27,29,30,22,25,38,41};
    const auto* table=m.intra?intra:inter;unsigned code=0;while(table[code]!=m.cbp) ++code;
    ExpGolomb::encode_ue(code,b);
    if(m.cbp) ExpGolomb::encode_se(0,b); // Constant QP within slice.
}
void SyntaxBuilder::build_slice_header(const EcRequest& r,BitWriter& b) {
    SequenceSyntax s;s.qp=r.qp;build_slice_header(s,{},b);
}
} // namespace h264::ec
