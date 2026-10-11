#include "slice_encoder.h"
#include "nal_formatter.h"
#include <algorithm>
namespace h264::ec {
namespace {
std::vector<std::uint8_t> nal(const std::vector<std::uint8_t>& rbsp,unsigned bytes,unsigned header,std::size_t capacity) {
    return NalFormatter::format_rbsp(rbsp.data(),bytes,header>>5,header&31,capacity);
}
struct Context {
    unsigned width;std::vector<int> data;
    Context(unsigned w,unsigned h):width(w),data(w*h,-1) {}
    int nc(unsigned x,unsigned y) const {int a=x?data[y*width+x-1]:-1,b=y?data[(y-1)*width+x]:-1;return a<0?(b<0?0:b):b<0?a:(a+b+1)/2;}
    int mode(unsigned x,unsigned y) const {int a=x?data[y*width+x-1]:-1,b=y?data[(y-1)*width+x]:-1;return a<0 || b<0 ? 2:std::min(a,b);}
    void set(unsigned x,unsigned y,int n) {data[y*width+x]=n;}
};
Coefficients scan(const Coefficients& natural,bool ac) {Coefficients out{};for(unsigned i=0;i<(ac?15u:16u);++i)out[i]=natural[zigzag4[i+(ac?1:0)]];return out;}
bool nonzero(const Coefficients& c){return std::any_of(c.begin(),c.end(),[](auto n){return n!=0;});}
}
std::vector<std::uint8_t> SliceEncoder::parameter_sets(const SequenceSyntax& s) {
    std::vector<std::uint8_t> buffer(128);std::uint32_t count=0;BitWriter bits(buffer,count);
    SyntaxBuilder::build_sps(s,bits);bits.rbsp_trailing_bits();auto out=nal(buffer,count,0x67,256);
    BitWriter pps(buffer,count);SyntaxBuilder::build_pps(s,pps);pps.rbsp_trailing_bits();auto p=nal(buffer,count,0x68,256);
    out.insert(out.end(),p.begin(),p.end());return out;
}
std::vector<std::uint8_t> SliceEncoder::picture(const SequenceSyntax& s,const SliceSyntax& picture,const std::vector<CodedMacroblock>& mbs,std::size_t capacity) {
    SyntaxBuilder::validate(s,picture);
    if(mbs.size()!=std::size_t(s.width/16)*(s.height/16) || capacity<16 || capacity>64*1024*1024)
        throw std::invalid_argument("slice geometry/capacity");
    std::vector<std::uint8_t> buffer(capacity);std::uint32_t bytes=0;BitWriter bits(buffer,bytes);
    SyntaxBuilder::build_slice_header(s,picture,bits);
    Context luma(s.width/4,s.height/4),modes(s.width/4,s.height/4),u(s.width/8,s.height/8),v(s.width/8,s.height/8);
    for(unsigned address=0;address<mbs.size();++address) {
        const auto& mb=mbs[address];unsigned mx=address%(s.width/16),my=address/(s.width/16);
        MacroblockSyntax syntax;syntax.intra=mb.intra;syntax.modes=mb.intra_modes;syntax.chroma_mode=mb.chroma_mode;syntax.mvd_x=mb.mvd_x;syntax.mvd_y=mb.mvd_y;
        for(unsigned i=0;i<16;++i) {
            unsigned x=mx*4+((i>>2)&1)*2+(i&1),y=my*4+(i>>3)*2+((i>>1)&1);
            syntax.predicted_modes[i]=modes.mode(x,y);modes.set(x,y,mb.intra?mb.intra_modes[i]:2);
            if(nonzero(mb.luma[i])) syntax.cbp|=1u<<(i/4);
        }
        unsigned chroma=0;
        for(unsigned p=0;p<2;++p) {
            for(auto c:mb.chroma_dc[p]) if(c) chroma=std::max(chroma,1u);
            for(const auto& block:mb.chroma_ac[p]) {if(block[0]) throw std::invalid_argument("chroma AC contains DC");if(nonzero(block)) chroma=2;}
        }
        syntax.cbp|=chroma<<4;SyntaxBuilder::build_macroblock(syntax,picture.p_slice,bits);
        for(unsigned i=0;i<16;++i) {
            unsigned x=mx*4+((i>>2)&1)*2+(i&1),y=my*4+(i>>3)*2+((i>>1)&1),n=0;
            if(syntax.cbp&(1u<<(i/4))) n=Cavlc::encode_block(scan(mb.luma[i],false),16,luma.nc(x,y),bits);
            luma.set(x,y,n);
        }
        if(chroma) for(unsigned p=0;p<2;++p) {Coefficients dc{};std::copy(mb.chroma_dc[p].begin(),mb.chroma_dc[p].end(),dc.begin());Cavlc::encode_block(dc,4,-1,bits);}
        for(unsigned p=0;p<2;++p) for(unsigned i=0;i<4;++i) {
            auto& ctx=p?v:u;unsigned x=mx*2+i%2,y=my*2+i/2,n=0;
            if(chroma==2) n=Cavlc::encode_block(scan(mb.chroma_ac[p][i],true),15,ctx.nc(x,y),bits);
            ctx.set(x,y,n);
        }
    }
    bits.rbsp_trailing_bits();return nal(buffer,bytes,picture.idr?0x65:0x41,capacity);
}
} // namespace h264::ec
