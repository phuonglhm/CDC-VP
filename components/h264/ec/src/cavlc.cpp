#include "cavlc.h"
#include "cavlc_tables.h"
#include <cstdlib>
namespace h264::ec {
Cavlc::Cavlc(sc_core::sc_module_name name):sc_core::sc_module(name) {}
namespace {
void level_bits(int level,unsigned suffix,bool first_adjust,BitWriter& bw) {
    unsigned code=level>0 ? 2u*level-2 : -2*level-1;
    if(first_adjust) code-=2;
    // Invert the normative level_prefix/level_suffix decoding equations.
    for(unsigned prefix=0;prefix<32;++prefix) {
        unsigned length=(prefix==14 && suffix==0)?4:(prefix>=15?prefix-3:suffix);
        std::uint64_t base=std::uint64_t(std::min(prefix,15u))<<suffix;
        if(prefix>=15 && suffix==0) base+=15;
        if(prefix>=16) base+=(std::uint64_t(1)<<(prefix-3))-4096;
        if(code>=base && std::uint64_t(code)-base<(std::uint64_t(1)<<length)) {
            bw.write_bits(0,prefix);bw.write_bits(1,1);bw.write_bits(code-base,length);return;
        }
    }
    throw std::overflow_error("CAVLC level prefix");
}
}
unsigned Cavlc::encode_block(const std::array<std::int16_t,16>& scan,unsigned max,int nc,BitWriter& dst) {
    if((max!=4 && max!=15 && max!=16) || (max==4?nc!=-1:(nc<0 || nc>16)))
        throw std::invalid_argument("CAVLC block/context");
    for(unsigned i=max;i<16;++i) if(scan[i]) throw std::invalid_argument("CAVLC coefficient beyond scan");
    std::array<std::uint8_t,128> tmp{};std::uint32_t bytes=0;BitWriter bw(tmp,bytes);
    int values[16]{},positions[16]{};unsigned count=0;
    for(unsigned i=0;i<max;++i) if(scan[i]) {values[count]=scan[i];positions[count++]=i;}
    unsigned trailing=0;
    while(trailing<std::min(count,3u) && std::abs(values[count-1-trailing])==1) ++trailing;
    unsigned token=count*4+trailing;
    if(max==4) bw.write_bits(vlc::chroma_dc_coeff_token_bits[token],vlc::chroma_dc_coeff_token_len[token]);
    else {unsigned table=nc<2?0:nc<4?1:nc<8?2:3;bw.write_bits(vlc::coeff_token_bits[table][token],vlc::coeff_token_len[table][token]);}
    if(count) {
        for(unsigned i=0;i<trailing;++i) bw.write_bits(values[count-1-i]<0,1);
        unsigned suffix=count>10 && trailing<3 ? 1:0;
        for(unsigned i=trailing;i<count;++i) {
            int level=values[count-1-i];level_bits(level,suffix,i==trailing && trailing<3,bw);
            if(suffix==0) suffix=1;
            if(suffix<6 && unsigned(std::abs(level))>(3u<<(suffix-1))) ++suffix;
        }
        unsigned zeros=positions[count-1]+1-count;
        if(count<max) {
            if(max==4) bw.write_bits(vlc::chroma_dc_total_zeros_bits[count-1][zeros],vlc::chroma_dc_total_zeros_len[count-1][zeros]);
            else bw.write_bits(vlc::total_zeros_bits[count-1][zeros],vlc::total_zeros_len[count-1][zeros]);
        }
        for(unsigned i=count-1;i>0 && zeros;--i) {
            unsigned run=positions[i]-positions[i-1]-1,row=std::min(zeros,7u)-1;
            bw.write_bits(vlc::run_bits[row][run],vlc::run_len[row][run]);zeros-=run;
        }
    }
    if(dst.bits()+bw.bits()>dst.stream.size()*8) throw std::overflow_error("CAVLC destination full");
    for(unsigned i=0;i<bw.bits();++i) dst.write_bits((tmp[i/8]>>(7-i%8))&1,1);
    return count;
}
void Cavlc::encode(const EcRequest& req,BitWriter& bw) {
    std::array<std::int16_t,16> scan{};
    if(req.max_coeff==4) {for(unsigned i=0;i<4;++i) scan[i]=req.levels[i];}
    else if(req.max_coeff==15 || req.max_coeff==16) {
        unsigned start=req.max_coeff==15?1:0;
        for(unsigned i=0;i<req.max_coeff;++i) scan[i]=req.levels[zigzag4[i+start]];
    } else throw std::invalid_argument("CAVLC scan length");
    encode_block(scan,req.max_coeff,req.nc,bw);
}
} // namespace h264::ec
